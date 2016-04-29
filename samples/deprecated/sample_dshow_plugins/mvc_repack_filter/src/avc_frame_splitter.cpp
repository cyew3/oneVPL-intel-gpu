/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include <exception>
#include <stdio.h>

#include "avc_frame_splitter.h"

#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

AVCFrameInfo::AVCFrameInfo()
{
    Reset();
}

void AVCFrameInfo::Reset()
{
    m_slice = 0;
    m_index = 0;
}

AVCFrameSplitter::AVCFrameSplitter()
    : m_WaitForIDR(true)
    , m_currentInfo(0)
    , m_pLastSlice(0)
    , m_lastNalUnit(0)
{
    Init();
}

AVCFrameSplitter::~AVCFrameSplitter()
{
    Close();
}

mfxStatus AVCFrameSplitter::Init()
{
    Close();

    m_pNALSplitter.reset(new NALUnitSplitter());
    m_pNALSplitter->Init();

    m_WaitForIDR = true;

    m_AUInfo.reset(new AVCFrameInfo());

    m_currentFrame.resize(BUFFER_SIZE);
    //m_currentEncryptedFrame.resize(BUFFER_SIZE);

    m_pLastSlice = 0;
    m_lastNalUnit = 0;
    m_currentInfo = 0;

    m_slices.resize(128);
    memset(&m_frame, 0, sizeof(m_frame));
    m_frame.Data = &m_currentFrame[0];
    m_frame.Slice = &m_slices[0];

    return MFX_ERR_NONE;
}

void AVCFrameSplitter::Close()
{
    m_pLastSlice = 0;
    m_lastNalUnit = 0;
    m_currentInfo = 0;
}

mfxU8 * AVCFrameSplitter::GetMemoryForSwapping(mfxU32 size)
{
    if (m_swappingMemory.size() <= size + 8)
        m_swappingMemory.resize(size + 8);

    return &(m_swappingMemory[0]);
}

mfxStatus AVCFrameSplitter::DecodeHeader(mfxBitstream * nalUnit)
{
    mfxStatus umcRes = MFX_ERR_NONE;

    AVCHeadersBitstream bitStream;

    try
    {
        mfxU32 swappingSize = nalUnit->DataLength;
        mfxU8 * swappingMemory = GetMemoryForSwapping(swappingSize);

        BytesSwapper::SwapMemory(swappingMemory, swappingSize, nalUnit->Data + nalUnit->DataOffset, nalUnit->DataLength);

        bitStream.Reset(swappingMemory, swappingSize);

        NAL_Unit_Type uNALUnitType;
        mfxU8 uNALStorageIDC;

        bitStream.GetNALUnitType(uNALUnitType, uNALStorageIDC);

        switch(uNALUnitType)
        {
        // sequence parameter set
        case NAL_UT_SPS:
            {
                AVCSeqParamSet sps;
                umcRes = bitStream.GetSequenceParamSet(&sps);
                if (umcRes == MFX_ERR_NONE)
                {
                    /*const AVCSeqParamSet * old_sps = GetCurrentSequence();
                    if (IsNeedSPSInvalidate(old_sps, &sps))
                    {
                        return UMC_NTF_NEW_RESOLUTION;
                    }*/

                    AVCSeqParamSet * temp = m_headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                    m_headers.m_SeqParams.AddHeader(&sps);

                    // Validate the incoming bitstream's image dimensions.
                    temp = m_headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);

                    //m_maxDecFrameBuffering = temp->max_dec_frame_buffering ?
                      //  temp->max_dec_frame_buffering : m_dpbSize;

                    m_pNALSplitter->SetSuggestedSize(CalculateSuggestedSize(&sps));

                    if (umcRes != MFX_ERR_NONE)
                        return umcRes;
                }
                else
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            break;

        case NAL_UT_SPS_EX:
            {
                AVCSeqParamSetExtension sps_ex;
                umcRes = bitStream.GetSequenceParamSetExtension(&sps_ex);

                if (umcRes == MFX_ERR_NONE)
                {
                    m_headers.m_SeqExParams.AddHeader(&sps_ex);
                }
                else
                    return umcRes;
            }
            break;

            // picture parameter set
        case NAL_UT_PPS:
            {
                AVCPicParamSet pps;
                // set illegal id
                pps.pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS;

                // Get id
                umcRes = bitStream.GetPictureParamSetPart1(&pps);
                if (MFX_ERR_NONE == umcRes)
                {
                    AVCSeqParamSet *pRefsps = m_headers.m_SeqParams.GetHeader(pps.seq_parameter_set_id);
                    //Ipp32u prevActivePPS = m_headers.m_PicParams.GetCurrentID();

                    if (!pRefsps || pRefsps ->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                    {
               //         pRefsps = m_headers.m_SeqParamsMvcExt.GetHeader(pps.seq_parameter_set_id);
               //         if (!pRefsps || pRefsps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                        {
                            return MFX_ERR_UNDEFINED_BEHAVIOR;
                        }
                    }

                    // Get rest of pic param set
                    umcRes = bitStream.GetPictureParamSetPart2(&pps, pRefsps);
                    if (MFX_ERR_NONE == umcRes)
                    {
                        m_headers.m_PicParams.AddHeader(&pps);

                        pps.SliceGroupInfo.t3.pSliceGroupIDMap = 0; // avoid twice deleting
                    }

                    m_headers.m_SeqParams.SetCurrrentID(pps.seq_parameter_set_id);
                }
            }
            break;

        // subset sequence parameter set
        case NAL_UNIT_SUBSET_SPS:
            {
                AVCSeqParamSet sps;
                umcRes = bitStream.GetSequenceParamSet(&sps);
                if (umcRes == MFX_ERR_NONE)
                {
                    /*const AVCSeqParamSet * old_sps = GetCurrentSequence();
                    if (IsNeedSPSInvalidate(old_sps, &sps))
                    {
                        return UMC_NTF_NEW_RESOLUTION;
                    }*/

                    AVCSeqParamSet * temp = m_headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                    m_headers.m_SeqParams.AddHeader(&sps);

                    // Validate the incoming bitstream's image dimensions.
                    temp = m_headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);

                    //m_maxDecFrameBuffering = temp->max_dec_frame_buffering ?
                      //  temp->max_dec_frame_buffering : m_dpbSize;

                    m_pNALSplitter->SetSuggestedSize(CalculateSuggestedSize(&sps));

                    if (umcRes != MFX_ERR_NONE)
                        return umcRes;
                }
                else
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            break;
            // decode a prefix nal unit
        case NAL_UNIT_PREFIX:
            {
                umcRes = bitStream.GetNalUnitPrefix(&m_headers.m_nalExtension);
                if (MFX_ERR_NONE != umcRes)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
            break;
        default:
            break;
        }
    }
    catch(const AVC_exception & )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    catch(...)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus AVCFrameSplitter::DecodeSEI(mfxBitstream * nalUnit)
{
    if (m_headers.m_SeqParams.GetCurrrentID() == -1)
        return MFX_ERR_NONE;

    AVCHeadersBitstream bitStream;

    try
    {
        mfxU32 swappingSize = nalUnit->DataLength;
        mfxU8* swappingMemory = GetMemoryForSwapping(swappingSize);
        NAL_Unit_Type uNALUnitType;
        mfxU8 uNALStorageIDC;

        BytesSwapper::SwapMemory(swappingMemory, swappingSize, nalUnit->Data + nalUnit->DataOffset, nalUnit->DataLength);

        bitStream.Reset(swappingMemory, swappingSize);
        bitStream.GetNALUnitType(uNALUnitType, uNALStorageIDC);

        do
        {
            AVCSEIPayLoad    m_SEIPayLoads;

            bitStream.GetSEI(m_headers.m_SeqParams, m_headers.m_SeqParams.GetCurrrentID(), &m_SEIPayLoads);

            if (m_SEIPayLoads.payLoadType == SEI_RESERVED)
                continue;

            m_headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
        } while (bitStream.More_RBSP_Data());

    } catch(...)
    {
        // nothing to do just catch it
    }

    return MFX_ERR_NONE;
}

AVCSlice * AVCFrameSplitter::DecodeSliceHeader(mfxBitstream * nalUnit)
{
    m_slicesStorage.push_back(AVCSlice());
    AVCSlice *pSlice = &m_slicesStorage.back();

    mfxU32 swappingSize = nalUnit->DataLength;
    mfxU8 * swappingMemory = GetMemoryForSwapping(swappingSize);

    BytesSwapper::SwapMemory(swappingMemory, swappingSize, nalUnit->Data + nalUnit->DataOffset, nalUnit->DataLength);

    mfxI32 pps_pid = pSlice->RetrievePicParamSetNumber(swappingMemory, swappingSize);
    if (pps_pid == -1)
    {
        return 0;
    }

    AVCSEIPayLoad * spl = m_headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);

    //if (m_WaitForIDR && !(0== pSlice->GetSliceHeader()->slice_type))
    //{
    //    if (pSlice->GetSliceHeader()->slice_type != INTRASLICE && !spl)
    //    {
    //        return 0;
    //    }
    //}

    pSlice->m_picParamSet = m_headers.m_PicParams.GetHeader(pps_pid);
    if (!pSlice->m_picParamSet)
    {
        return 0;
    }

    mfxI32 seq_parameter_set_id = pSlice->m_picParamSet->seq_parameter_set_id;

    pSlice->m_seqParamSet = m_headers.m_SeqParams.GetHeader(seq_parameter_set_id);
    if (!pSlice->m_seqParamSet)
    {
        return 0;
    }

    pSlice->m_seqParamSetEx = m_headers.m_SeqExParams.GetHeader(seq_parameter_set_id);
    if (NAL_UNIT_SLICE_SCALABLE != pSlice->GetSliceHeader()->nal_unit_type)
    {
        m_headers.m_SeqParams.SetCurrrentID(pSlice->m_picParamSet->seq_parameter_set_id);
    }
    pSlice->m_dTime = nalUnit->TimeStamp;

    //if (!pSlice->DecodeHeader(swappingMemory, swappingSize))
    //{
    //    return 0;
    //}

    if (spl && (pSlice->GetSliceHeader()->slice_type != INTRASLICE))
    {
        m_headers.m_SEIParams.RemoveHeader(spl->GetID());
    }

    m_WaitForIDR = false;

    return pSlice;
}


AVCFrameInfo * AVCFrameSplitter::GetFreeFrame()
{
    return m_AUInfo.get();
}

void AVCFrameSplitter::ResetCurrentState()
{
    m_frame.DataLength = 0;
    m_frame.SliceNum = 0;
    m_frame.FirstFieldSliceNum = 0;
    m_AUInfo->Reset();
    if (m_slicesStorage.size() > 1)
    {
        m_slicesStorage.erase(m_slicesStorage.begin(), --m_slicesStorage.end());
    }
}

bool AVCFrameSplitter::IsFieldOfOneFrame(AVCFrameInfo * frame, const AVCSliceHeader * slice1, const AVCSliceHeader *slice2)
{
    if (frame && frame->m_index)
        return false;

    if ((slice1->nal_ref_idc && !slice2->nal_ref_idc) ||
        (!slice1->nal_ref_idc && slice2->nal_ref_idc))
        return false;

    if (slice1->field_pic_flag != slice2->field_pic_flag)
        return false;

    if (slice1->bottom_field_flag == slice2->bottom_field_flag)
        return false;

    return true;
}

inline bool IsSlicesOfOneAU(const AVCSliceHeader *pOne, const AVCSliceHeader *pTwo)
{
    if (!pOne || !pTwo)
        return true;

    // this function checks two slices are from same picture or not
    // 7.4.1.2.4 part of AVC standart

    if ((pOne->frame_num != pTwo->frame_num) ||
        (pOne->first_mb_in_slice == pTwo->first_mb_in_slice) ||
        (pOne->pic_parameter_set_id != pTwo->pic_parameter_set_id) ||
        (pOne->field_pic_flag != pTwo->field_pic_flag) ||
        (pOne->bottom_field_flag != pTwo->bottom_field_flag))
        return false;

    if ((pOne->nal_ref_idc != pTwo->nal_ref_idc) &&
        (0 == MIN(pOne->nal_ref_idc, pTwo->nal_ref_idc)))
        return false;

    //if (0 == pSliceOne->GetSeqParam()->pic_order_cnt_type)
    {
        if ((pOne->pic_order_cnt_lsb != pTwo->pic_order_cnt_lsb) ||
            (pOne->delta_pic_order_cnt_bottom != pTwo->delta_pic_order_cnt_bottom))
            return false;
    }
    //else
    {
        if ((pOne->delta_pic_order_cnt[0] != pTwo->delta_pic_order_cnt[0]) ||
            (pOne->delta_pic_order_cnt[1] != pTwo->delta_pic_order_cnt[1]))
            return false;
    }

    if (pOne->nal_unit_type != pTwo->nal_unit_type)
    {
        if ((NAL_UT_IDR_SLICE == pOne->nal_unit_type) ||
            (NAL_UT_IDR_SLICE == pTwo->nal_unit_type))
            return false;
    }
    else if (NAL_UT_IDR_SLICE == pOne->nal_unit_type)
    {
        if (pOne->idr_pic_id != pTwo->idr_pic_id)
            return false;
    }

    return true;
}


mfxStatus AVCFrameSplitter::AddSlice(AVCSlice * pSlice)
{
    m_pLastSlice = 0;

    if (!pSlice)
    {
        // complete current frame info
        return MFX_ERR_NONE;
    }

    if (m_currentInfo)
    {
        AVCSlice * pFirstFrameSlice = m_currentInfo->m_slice;

        if (pFirstFrameSlice && (false == IsSlicesOfOneAU(pFirstFrameSlice->GetSliceHeader(), pSlice->GetSliceHeader())))
        {
            //bool isField = pFirstFrameSlice->IsField();

            // complete frame
            if (pSlice->IsField())
            {
                if (IsFieldOfOneFrame(m_currentInfo, pFirstFrameSlice->GetSliceHeader(), pSlice->GetSliceHeader()))
                {
                    m_currentInfo->m_index = 1;
                    m_currentInfo->m_slice = pSlice;
                    // init new au
                }
                else
                {
                    m_pLastSlice = pSlice;
                    return MFX_ERR_NONE;
                }
            }
            else
            {
                m_pLastSlice = pSlice;
                return MFX_ERR_NONE;
            }
        }
    }
    else
    {
        m_currentInfo = GetFreeFrame();
        if (!m_currentInfo)
        {
            m_pLastSlice = pSlice;
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }
    }

    if (!m_currentInfo->m_slice)
        m_currentInfo->m_slice = pSlice;

    //m_currentInfo->m_au[au_index]->AddSlice(pSlice);
    return MFX_ERR_MORE_DATA;
}

mfxStatus AVCFrameSplitter::AddNalUnit(mfxBitstream * nalUnit)
{
    static mfxU8 start_code_prefix[] = {0, 0, 1};

    /*{
    mfxU8 *ptr = (mfxU8*)nalUnit->GetDataPointer();
    mfxU32 size = (mfxU32)nalUnit->GetDataSize();

    for (mfxU32 i = 0; i < size; i++)
    {
        if (ptr[i] == 0 && ptr[i + 1] == 0 && ptr[i + 2] == 3)
        {
            //memmove(ptr + i + 2, ptr + i + 3, size - i);
            size--;
        }
    }
    nalUnit->SetDataSize(size);
    }*/

    if (m_frame.DataLength + nalUnit->DataLength + sizeof(start_code_prefix) >= BUFFER_SIZE)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    MSDK_MEMCPY(m_frame.Data + m_frame.DataLength, start_code_prefix, sizeof(start_code_prefix));
    MSDK_MEMCPY(m_frame.Data + m_frame.DataLength + sizeof(start_code_prefix), nalUnit->Data + nalUnit->DataOffset, nalUnit->DataLength);

    m_frame.DataLength += (mfxU32)(nalUnit->DataLength + sizeof(start_code_prefix));

    return MFX_ERR_NONE;
}

mfxStatus AVCFrameSplitter::AddSliceNalUnit(mfxBitstream * nalUnit, AVCSlice * slice)
{
    static mfxU8 start_code_prefix[] = {0, 0, 1};

    mfxU32 sliceLength = (mfxU32)(nalUnit->DataLength + sizeof(start_code_prefix));

    if (m_frame.DataLength + sliceLength >= BUFFER_SIZE)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    MSDK_MEMCPY(m_frame.Data + m_frame.DataLength, start_code_prefix, sizeof(start_code_prefix));
    MSDK_MEMCPY(m_frame.Data + m_frame.DataLength + sizeof(start_code_prefix), nalUnit->Data + nalUnit->DataOffset, nalUnit->DataLength);

    if (!m_frame.SliceNum)
    {
        m_frame.TimeStamp = nalUnit->TimeStamp;
    }

    m_frame.SliceNum++;

    if (m_slices.size() <= m_frame.SliceNum)
    {
        m_slices.resize(m_frame.SliceNum + 10);
        m_frame.Slice = &m_slices[0];
    }

    SliceSplitterInfo & newSlice = m_slices[m_frame.SliceNum - 1];

    AVCHeadersBitstream * bs = slice->GetBitStream();

    newSlice.HeaderLength = (mfxU32)bs->BytesDecoded();
    newSlice.HeaderLength += sizeof(start_code_prefix) + 1; // with start code and nal unit type; DEBUG to eliminate it ?
    newSlice.DataLength = sliceLength;
    newSlice.DataOffset = m_frame.DataLength;

    m_frame.DataLength += sliceLength;

    if (!m_currentInfo->m_index)
        m_frame.FirstFieldSliceNum++;

    return MFX_ERR_NONE;
}

mfxStatus AVCFrameSplitter::ProcessNalUnit(mfxI32 nalType, mfxBitstream * nalUnit)
{
    if (!nalUnit)
        return MFX_ERR_MORE_DATA;

    switch (nalType)
    {
    case NAL_UT_IDR_SLICE:
    case NAL_UT_SLICE:
    case NAL_UT_AUXILIARY:
    case NAL_UNIT_SLICE_SCALABLE:
        {
            AVCSlice * pSlice = DecodeSliceHeader(nalUnit);
            if (pSlice)
            {
                mfxStatus sts = AddSlice(pSlice);
                if (sts == MFX_ERR_NOT_ENOUGH_BUFFER)
                {
                    return sts;
                }

                if (!m_pLastSlice)
                {
                    AddSliceNalUnit(nalUnit, pSlice);
                }
                else
                {
                    m_lastNalUnit = nalUnit;
                }

                if (sts == MFX_ERR_NONE)
                {
                    return sts;
                }
            }
        }
        break;

    case NAL_UT_SPS:
    case NAL_UT_PPS:
    case NAL_UT_SPS_EX:
    case NAL_UNIT_SUBSET_SPS:
    case NAL_UNIT_PREFIX:
        DecodeHeader(nalUnit);
        AddNalUnit(nalUnit);
        break;

    case NAL_UT_SEI:
        DecodeSEI(nalUnit);
        AddNalUnit(nalUnit);
        break;
    case NAL_UT_AUD:
    case NAL_UT_DPA:
    case NAL_UT_DPB:
    case NAL_UT_DPC:
    case NAL_UT_FD:
    case NAL_UT_UNSPECIFIED:
        break;

    case NAL_END_OF_STREAM:
    case NAL_END_OF_SEQ:
        {
            AddNalUnit(nalUnit);
        }
        break;

    default:
        break;
    };

    return MFX_ERR_MORE_DATA;
}

mfxStatus AVCFrameSplitter::GetFrame(mfxBitstream * bs_in, AVCFrameSplitterInfo ** frame)
{
    *frame = 0;
    do
    {
        if (m_pLastSlice)
        {
            AVCSlice * pSlice = m_pLastSlice;
            mfxStatus sts = AddSlice(pSlice);
            AddSliceNalUnit(m_lastNalUnit, pSlice);
            m_lastNalUnit = 0;
            if (sts == MFX_ERR_NONE)
                return MFX_ERR_NONE;
        }

        mfxBitstream * destination;
        mfxI32 nalType = m_pNALSplitter->GetNalUnits(bs_in, destination);

        mfxStatus sts = ProcessNalUnit(nalType, destination);

        if (sts == MFX_ERR_NONE || (!bs_in && m_frame.SliceNum))
        {
            m_currentInfo = 0;
            *frame = &m_frame;
            return MFX_ERR_NONE;
        }

    } while (bs_in && bs_in->DataLength > MINIMAL_DATA_SIZE);

    return MFX_ERR_MORE_DATA;
}

AVCSlice::AVCSlice()
{
}

AVCSliceHeader * AVCSlice::GetSliceHeader()
{
    return &m_sliceHeader;
}

mfxI32 AVCSlice::RetrievePicParamSetNumber(mfxU8 *pSource, mfxU32 nSourceSize)
{
    if (!nSourceSize)
        return -1;

    m_BitStream.Reset(pSource, nSourceSize);

    mfxStatus umcRes = MFX_ERR_NONE;

    try
    {
        NAL_Unit_Type NALUType;
        mfxU8 nal_ref_idc;

        umcRes = m_BitStream.GetNALUnitType(NALUType, nal_ref_idc);
        if (MFX_ERR_NONE != umcRes)
            return false;

        m_sliceHeader.nal_unit_type = NALUType;

        // decode first part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart1(&m_sliceHeader);
        if (MFX_ERR_NONE != umcRes)
            return -1;
    } catch (...)
    {
        return -1;
    }

    return m_sliceHeader.pic_parameter_set_id;
}

bool AVCSlice::DecodeHeader(mfxU8 *pSource, mfxU32 nSourceSize)
{
    m_BitStream.Reset(pSource, nSourceSize);

    if (!nSourceSize)
        return false;

    mfxStatus umcRes = MFX_ERR_NONE;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
    bool bIsIDRPic = false;
    NAL_Unit_Type NALUType;
    mfxU8 nal_ref_idc;

    try
    {
        memset(&m_sliceHeader, 0, sizeof(m_sliceHeader));

        umcRes = m_BitStream.GetNALUnitType(NALUType, nal_ref_idc);
        if (MFX_ERR_NONE != umcRes)
            return false;

        if ((NALUType != NAL_UT_SLICE) && (NALUType != NAL_UT_IDR_SLICE) &&
            (NALUType != NAL_UT_AUXILIARY))
        {
            SAMPLE_ASSERT((NALUType == NAL_UT_SLICE) || (NALUType == NAL_UT_IDR_SLICE) || (NALUType == NAL_UT_AUXILIARY));
            return false;
        }

        m_sliceHeader.nal_unit_type = NALUType;
        bIsIDRPic = (NALUType == NAL_UT_IDR_SLICE);

        // decode first part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart1(&m_sliceHeader);
        if (MFX_ERR_NONE != umcRes)
            return false;

        // decode second part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart2(&m_sliceHeader,
                                                 m_picParamSet,
                                                 bIsIDRPic,
                                                 m_seqParamSet,
                                                 nal_ref_idc);
        if (MFX_ERR_NONE != umcRes)
            return false;

        PredWeightTable m_PredWeight[2][MAX_NUM_REF_FRAMES];
        RefPicListReorderInfo ReorderInfoL0;
        RefPicListReorderInfo ReorderInfoL1;
        AdaptiveMarkingInfo     m_AdaptiveMarkingInfo;

        // decode second part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart3(&m_sliceHeader,
                                                 m_PredWeight[0],
                                                 m_PredWeight[1],
                                                 &ReorderInfoL0,
                                                 &ReorderInfoL1,
                                                 &m_AdaptiveMarkingInfo,
                                                 m_picParamSet,
                                                 m_seqParamSet,
                                                 nal_ref_idc);
        if (MFX_ERR_NONE != umcRes)
            return false;

        if (m_picParamSet->entropy_coding_mode)
            m_BitStream.AlignPointerRight();
    }
    catch(const AVC_exception & )
    {
        return false;
    }
    catch(...)
    {
        return false;
    }

    return (MFX_ERR_NONE == umcRes);
}
