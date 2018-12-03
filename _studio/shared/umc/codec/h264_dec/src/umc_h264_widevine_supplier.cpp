// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#ifndef MFX_ENABLE_CPLIB

#include "umc_h264_notify.h"
#include "umc_h264_widevine_decrypter.h"
#include "umc_h264_widevine_supplier.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

#ifdef UMC_VA_LINUX
#include "umc_va_linux_protected.h"
#endif

#include "mfx_common_int.h"

namespace UMC
{

void POCDecoderWidevine::DecodePictureOrderCount(const H264Slice *slice, int32_t frame_num)
{
    const UMC_H264_DECODER::H264SliceHeader* sliceHeader = slice->GetSliceHeader();
    const UMC_H264_DECODER::H264SeqParamSet* sps = slice->GetSeqParam();

    int32_t uMaxFrameNum = (1<<sps->log2_max_frame_num);

    if (sps->pic_order_cnt_type == 0)
    {
        m_PicOrderCnt = sliceHeader->pic_order_cnt_lsb;
        m_TopFieldPOC = sliceHeader->pic_order_cnt_lsb;
        m_BottomFieldPOC = sliceHeader->pic_order_cnt_lsb + sliceHeader->delta_pic_order_cnt_bottom;
    }
    else if (sps->pic_order_cnt_type == 1)
    {
        m_PicOrderCnt = sliceHeader->pic_order_cnt_lsb;
        m_TopFieldPOC = sliceHeader->pic_order_cnt_lsb;
        m_BottomFieldPOC = sliceHeader->pic_order_cnt_lsb + sliceHeader->delta_pic_order_cnt_bottom;

        if ((sliceHeader->delta_pic_order_cnt[0] == 0))
        {
            uint32_t i;
            uint32_t uAbsFrameNum;    // frame # relative to last IDR pic
            uint32_t uPicOrderCycleCnt = 0;
            uint32_t uFrameNuminPicOrderCntCycle = 0;
            int32_t ExpectedPicOrderCnt = 0;
            int32_t ExpectedDeltaPerPicOrderCntCycle;
            uint32_t uNumRefFramesinPicOrderCntCycle = sps->num_ref_frames_in_pic_order_cnt_cycle;

            if (frame_num < m_FrameNum)
                m_FrameNumOffset += uMaxFrameNum;

            if (uNumRefFramesinPicOrderCntCycle != 0)
                uAbsFrameNum = m_FrameNumOffset + frame_num;
            else
                uAbsFrameNum = 0;

            if ((sliceHeader->nal_ref_idc == false)  && (uAbsFrameNum > 0))
                uAbsFrameNum--;

            if (uAbsFrameNum)
            {
                uPicOrderCycleCnt = (uAbsFrameNum - 1) /
                        uNumRefFramesinPicOrderCntCycle;
                uFrameNuminPicOrderCntCycle = (uAbsFrameNum - 1) %
                        uNumRefFramesinPicOrderCntCycle;
            }

            ExpectedDeltaPerPicOrderCntCycle = 0;
            for (i=0; i < uNumRefFramesinPicOrderCntCycle; i++)
            {
                ExpectedDeltaPerPicOrderCntCycle +=
                    sps->poffset_for_ref_frame[i];
            }

            if (uAbsFrameNum)
            {
                ExpectedPicOrderCnt = uPicOrderCycleCnt * ExpectedDeltaPerPicOrderCntCycle;
                for (i=0; i<=uFrameNuminPicOrderCntCycle; i++)
                {
                    ExpectedPicOrderCnt +=
                        sps->poffset_for_ref_frame[i];
                }
            }
            else
                ExpectedPicOrderCnt = 0;

            if (sliceHeader->nal_ref_idc == false)
                ExpectedPicOrderCnt += sps->offset_for_non_ref_pic;
            m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[0];
            if (sliceHeader->field_pic_flag==0)
            {
                m_TopFieldPOC = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
                m_BottomFieldPOC = m_TopFieldPOC +
                    sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 1 ];
            }
            else if (! sliceHeader->bottom_field_flag)
                m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
            else
                m_PicOrderCnt  = ExpectedPicOrderCnt + sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 0 ];
        }
    }
    else if (sps->pic_order_cnt_type == 2)
    {
        int32_t iMaxFrameNum = (1<<sps->log2_max_frame_num);
        uint32_t uAbsFrameNum;    // frame # relative to last IDR pic

        if (frame_num < m_FrameNum)
            m_FrameNumOffset += iMaxFrameNum;
        uAbsFrameNum = frame_num + m_FrameNumOffset;
        m_PicOrderCnt = uAbsFrameNum*2;
        if (sliceHeader->nal_ref_idc == false)
            m_PicOrderCnt--;
        m_TopFieldPOC = m_PicOrderCnt;
        m_BottomFieldPOC = m_PicOrderCnt;
    }

    if (sliceHeader->nal_ref_idc)
    {
        m_PrevFrameRefNum = frame_num;
    }

    m_FrameNum = frame_num;
}

WidevineTaskSupplier::WidevineTaskSupplier()
    : VATaskSupplier()
    , m_pWidevineDecrypter(new WidevineDecrypter{})
{}

WidevineTaskSupplier::~WidevineTaskSupplier()
{}

Status WidevineTaskSupplier::Init(VideoDecoderParams *pInit)
{
    Status umsRes = VATaskSupplier::Init(pInit);
    if (umsRes != UMC_OK)
        return umsRes;

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->Init();
        m_pWidevineDecrypter->SetVideoHardwareAccelerator(((UMC::VideoDecoderParams*)pInit)->pVideoAccelerator);

        for (int32_t i = 0; i < MVC_Extension::GetViewCount(); i++)
        {
            ViewItem &view = GetViewByNumber(i);
            for (uint32_t j = 0; j < MAX_NUM_LAYERS; j++)
            {
                view.pPOCDec[j].reset(new POCDecoderWidevine());
            }
        }
    }

#if defined(ANDROID)
    m_DPBSizeEx += 2; // Fix for Netflix freeze issue
#endif

    return UMC_OK;
}

void WidevineTaskSupplier::Reset()
{
    VATaskSupplier::Reset();

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->Reset();
    }
}

static
bool IsNeedSPSInvalidate(const UMC_H264_DECODER::H264SeqParamSet *old_sps, const UMC_H264_DECODER::H264SeqParamSet *new_sps)
{
    if (!old_sps || !new_sps)
        return false;

    //if (new_sps->no_output_of_prior_pics_flag)
      //  return true;

    if (old_sps->frame_width_in_mbs != new_sps->frame_width_in_mbs)
        return true;

    if (old_sps->frame_height_in_mbs != new_sps->frame_height_in_mbs)
        return true;

    if (old_sps->vui.max_dec_frame_buffering < new_sps->vui.max_dec_frame_buffering)
        return true;

    if (old_sps->chroma_format_idc != new_sps->chroma_format_idc)
        return true;

    if (old_sps->profile_idc != new_sps->profile_idc)
        return true;

    if (old_sps->bit_depth_luma != new_sps->bit_depth_luma)
        return true;

    if (old_sps->bit_depth_chroma != new_sps->bit_depth_chroma)
        return true;

    return false;
}

Status WidevineTaskSupplier::ParseWidevineSPSPPS(DecryptParametersWrapper* pDecryptParams)
{
    ViewItem *view = GetViewCount() ? &GetViewByNumber(BASE_VIEW) : 0;
    Status umcRes = UMC_OK;

    H264HeadersBitstream bitStream;

    try
    {
        // SPS
        {
            UMC_H264_DECODER::H264SeqParamSet sps;
            sps.seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS;
            umcRes = pDecryptParams->GetSequenceParamSet(&sps);
            if (umcRes != UMC_OK)
            {
                UMC_H264_DECODER::H264SeqParamSet * old_sps = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                if (old_sps)
                    old_sps->errorFlags = 1;
                return UMC_ERR_INVALID_STREAM;
            }

            uint8_t newDPBsize = (uint8_t)CalculateDPBSize(sps.level_idc,
                                        sps.frame_width_in_mbs * 16,
                                        sps.frame_height_in_mbs * 16,
                                        sps.num_ref_frames);

            bool isNeedClean = sps.vui.max_dec_frame_buffering == 0;
            sps.vui.max_dec_frame_buffering = sps.vui.max_dec_frame_buffering ? sps.vui.max_dec_frame_buffering : newDPBsize;
            if (sps.vui.max_dec_frame_buffering > newDPBsize)
                sps.vui.max_dec_frame_buffering = newDPBsize;

            if (sps.num_ref_frames > newDPBsize)
                sps.num_ref_frames = newDPBsize;

            const UMC_H264_DECODER::H264SeqParamSet * old_sps = m_Headers.m_SeqParams.GetCurrentHeader();
            bool newResolution = false;
            if (IsNeedSPSInvalidate(old_sps, &sps))
            {
                newResolution = true;
            }

            if (isNeedClean)
                sps.vui.max_dec_frame_buffering = 0;

            UMC_H264_DECODER::H264SeqParamSet * temp = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
            m_Headers.m_SeqParams.AddHeader(&sps);

            // Validate the incoming bitstream's image dimensions.
            temp = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
            if (!temp)
            {
                return UMC_ERR_NULL_PTR;
            }
            if (view)
            {
                uint8_t newDPBsizeL = (uint8_t)CalculateDPBSize(m_level_idc ? m_level_idc : temp->level_idc,
                                                                sps.frame_width_in_mbs * 16,
                                                                sps.frame_height_in_mbs * 16,
                                                                sps.num_ref_frames);

                temp->vui.max_dec_frame_buffering = temp->vui.max_dec_frame_buffering ? temp->vui.max_dec_frame_buffering : newDPBsizeL;
                if (temp->vui.max_dec_frame_buffering > newDPBsizeL)
                    temp->vui.max_dec_frame_buffering = newDPBsizeL;
            }

            if (!temp->vui.timing_info_present_flag || m_use_external_framerate)
            {
                temp->vui.num_units_in_tick = 90000;
                temp->vui.time_scale = (uint32_t)(2*90000 / m_local_delta_frame_time);
            }

            m_local_delta_frame_time = 1 / ((0.5 * temp->vui.time_scale) / temp->vui.num_units_in_tick);

            ErrorStatus::isSPSError = 0;

            if (newResolution)
                return UMC_NTF_NEW_RESOLUTION;
        }

        // PPS
        {
            UMC_H264_DECODER::H264PicParamSet pps;
            // set illegal id
            pps.pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS;

            // Get id
            umcRes = pDecryptParams->GetPictureParamSetPart1(&pps);
            if (UMC_OK != umcRes)
            {
                UMC_H264_DECODER::H264PicParamSet * old_pps = m_Headers.m_PicParams.GetHeader(pps.pic_parameter_set_id);
                if (old_pps)
                    old_pps->errorFlags = 1;
                return UMC_ERR_INVALID_STREAM;
            }

            UMC_H264_DECODER::H264SeqParamSet *refSps = m_Headers.m_SeqParams.GetHeader(pps.seq_parameter_set_id);
            uint32_t prevActivePPS = m_Headers.m_PicParams.GetCurrentID();

            if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
            {
                refSps = m_Headers.m_SeqParamsMvcExt.GetHeader(pps.seq_parameter_set_id);
                if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                {
                    refSps = m_Headers.m_SeqParamsSvcExt.GetHeader(pps.seq_parameter_set_id);
                    if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                        return UMC_ERR_INVALID_STREAM;
                }
            }

            // Get rest of pic param set
            umcRes = pDecryptParams->GetPictureParamSetPart2(&pps);
            if (UMC_OK != umcRes)
            {
                UMC_H264_DECODER::H264PicParamSet * old_pps = m_Headers.m_PicParams.GetHeader(pps.pic_parameter_set_id);
                if (old_pps)
                    old_pps->errorFlags = 1;
                return UMC_ERR_INVALID_STREAM;
            }

            m_Headers.m_PicParams.AddHeader(&pps);

            // in case of MVC restore previous active PPS
            if ((H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH == refSps->profile_idc) ||
                (H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH == refSps->profile_idc))
            {
                m_Headers.m_PicParams.SetCurrentID(prevActivePPS);
            }

            ErrorStatus::isPPSError = 0;
        }
    }
    catch(const h264_exception & ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    {
        UMC_H264_DECODER::H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

        if (currSPS)
        {
            if (currSPS->chroma_format_idc > 2)
                throw h264_exception(UMC_ERR_UNSUPPORTED);

            switch (currSPS->profile_idc)
            {
            case H264VideoDecoderParams::H264_PROFILE_UNKNOWN:
            case H264VideoDecoderParams::H264_PROFILE_BASELINE:
            case H264VideoDecoderParams::H264_PROFILE_MAIN:
            case H264VideoDecoderParams::H264_PROFILE_EXTENDED:
            case H264VideoDecoderParams::H264_PROFILE_HIGH:
            case H264VideoDecoderParams::H264_PROFILE_HIGH10:
            case H264VideoDecoderParams::H264_PROFILE_HIGH422:

            case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
            case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:

            case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
            case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
                break;
            default:
                throw h264_exception(UMC_ERR_UNSUPPORTED);
            }

            DPBOutput::OnNewSps(currSPS);
        }
    }

    if (m_firstVideoParams.mfx.FrameInfo.Width)
    {
        UMC_H264_DECODER::H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

        if (currSPS)
        {
            if (m_firstVideoParams.mfx.FrameInfo.Width < (currSPS->frame_width_in_mbs * 16) ||
                m_firstVideoParams.mfx.FrameInfo.Height < (currSPS->frame_height_in_mbs * 16) ||
                (currSPS->level_idc && m_firstVideoParams.mfx.CodecLevel && m_firstVideoParams.mfx.CodecLevel < currSPS->level_idc))
            {
                return UMC_NTF_NEW_RESOLUTION;
            }
        }
    }

    return UMC_OK;
}

H264Slice *WidevineTaskSupplier::ParseWidevineSliceHeader(DecryptParametersWrapper* pDecryptParams)
{
    if ((0 > m_Headers.m_SeqParams.GetCurrentID()) ||
        (0 > m_Headers.m_PicParams.GetCurrentID()))
    {
        return 0;
    }

    if (m_Headers.m_nalExtension.extension_present)
    {
        if (SVC_Extension::IsShouldSkipSlice(&m_Headers.m_nalExtension))
        {
            m_Headers.m_nalExtension.extension_present = 0;
            return 0;
        }
    }

    H264Slice * pSlice = CreateSlice();
    if (!pSlice)
    {
        return 0;
    }
    pSlice->SetHeap(&m_ObjHeap);
    pSlice->IncrementReference();

    notifier0<H264Slice> memory_leak_preventing_slice(pSlice, &H264Slice::DecrementReference);

    int32_t pps_pid = pDecryptParams->PicParams.pic_parameter_set_id;
    if (pps_pid == -1)
    {
        ErrorStatus::isPPSError = 1;
        return 0;
    }

    UMC_H264_DECODER::H264SEIPayLoad * spl = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);

    pSlice->m_pPicParamSet = m_Headers.m_PicParams.GetHeader(pps_pid);
    if (!pSlice->m_pPicParamSet)
    {
        return 0;
    }

    int32_t seq_parameter_set_id = pSlice->m_pPicParamSet->seq_parameter_set_id;

    pSlice->m_pSeqParamSetSvcEx = m_Headers.m_SeqParamsSvcExt.GetCurrentHeader();
    pSlice->m_pSeqParamSetMvcEx = m_Headers.m_SeqParamsMvcExt.GetCurrentHeader();
    pSlice->m_pSeqParamSet = m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id);

    if (!pSlice->m_pSeqParamSet)
    {
        ErrorStatus::isSPSError = 0;
        return 0;
    }

    m_Headers.m_SeqParams.SetCurrentID(pSlice->m_pPicParamSet->seq_parameter_set_id);
    m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);

    if (pSlice->m_pSeqParamSet->errorFlags)
        ErrorStatus::isSPSError = 1;

    if (pSlice->m_pPicParamSet->errorFlags)
        ErrorStatus::isPPSError = 1;

    Status sts = InitializePictureParamSet(m_Headers.m_PicParams.GetHeader(pps_pid), pSlice->m_pSeqParamSet, NAL_UT_CODED_SLICE_EXTENSION == pSlice->GetSliceHeader()->nal_unit_type);
    if (sts != UMC_OK)
    {
        return 0;
    }

    pSlice->m_pSeqParamSetEx = m_Headers.m_SeqExParams.GetHeader(seq_parameter_set_id);
    pSlice->m_pCurrentFrame = 0;

    pSlice->m_dTime = pDecryptParams->GetTime();

    int32_t iMBInFrame;
    int32_t iFieldIndex;

    // decode slice header
    {
        Status umcRes = UMC_OK;
        int32_t iSQUANT;

        try
        {
            memset(&pSlice->m_SliceHeader, 0, sizeof(pSlice->m_SliceHeader));

            // decode first part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart1(&pSlice->m_SliceHeader);
            if (UMC_OK != umcRes)
                return 0;


            // decode second part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart2(&pSlice->m_SliceHeader,
                                                         pSlice->m_pPicParamSet,
                                                         pSlice->m_pSeqParamSet);
            if (UMC_OK != umcRes)
                return 0;

            // decode second part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart3(&pSlice->m_SliceHeader,
                                                         pSlice->m_PredWeight[0],
                                                         pSlice->m_PredWeight[1],
                                                         &pSlice->ReorderInfoL0,
                                                         &pSlice->ReorderInfoL1,
                                                         &pSlice->m_AdaptiveMarkingInfo,
                                                         &pSlice->m_BaseAdaptiveMarkingInfo,
                                                         pSlice->m_pPicParamSet,
                                                         pSlice->m_pSeqParamSet,
                                                         pSlice->m_pSeqParamSetSvcEx);
            if (UMC_OK != umcRes)
                return 0;

            //7.4.3 Slice header semantics
            //If the current picture is an IDR picture, frame_num shall be equal to 0
            //If this restrictions is violated, clear LT flag to avoid ref. pic. marking process corruption
            if (pSlice->m_SliceHeader.IdrPicFlag && pSlice->m_SliceHeader.frame_num != 0)
                pSlice->m_SliceHeader.long_term_reference_flag = 0;

            // decode 4 part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart4(&pSlice->m_SliceHeader, pSlice->m_pSeqParamSetSvcEx);
            if (UMC_OK != umcRes)
                return 0;

            pSlice->m_iMBWidth = pSlice->m_pSeqParamSet->frame_width_in_mbs;
            pSlice->m_iMBHeight = pSlice->m_pSeqParamSet->frame_height_in_mbs;

            pSlice->m_WidevineStatusReportNumber = pDecryptParams->usStatusReportFeedbackNumber;

            // redundant slice, discard
            if (pSlice->m_SliceHeader.redundant_pic_cnt)
                return 0;

            // Set next MB.
            if (pSlice->m_SliceHeader.first_mb_in_slice >= (int32_t) (pSlice->m_iMBWidth * pSlice->m_iMBHeight))
            {
                return 0;
            }

            int32_t bit_depth_luma = pSlice->GetSeqParam()->bit_depth_luma;

            iSQUANT = pSlice->m_pPicParamSet->pic_init_qp +
                      pSlice->m_SliceHeader.slice_qp_delta;
            if (iSQUANT < QP_MIN - 6*(bit_depth_luma - 8)
                || iSQUANT > QP_MAX)
            {
                return 0;
            }

            if (pSlice->m_pPicParamSet->entropy_coding_mode)
                pSlice->m_BitStream.AlignPointerRight();
        }
        catch(const h264_exception & )
        {
            return 0;
        }
        catch(...)
        {
            return 0;
        }
    }

    pSlice->m_iMBWidth  = pSlice->GetSeqParam()->frame_width_in_mbs;
    pSlice->m_iMBHeight = pSlice->GetSeqParam()->frame_height_in_mbs;

    iMBInFrame = (pSlice->m_iMBWidth * pSlice->m_iMBHeight) / ((pSlice->m_SliceHeader.field_pic_flag) ? (2) : (1));
    iFieldIndex = (pSlice->m_SliceHeader.field_pic_flag && pSlice->m_SliceHeader.bottom_field_flag) ? (1) : (0);

    // set slice variables
    pSlice->m_iFirstMB = pSlice->m_SliceHeader.first_mb_in_slice;
    pSlice->m_iMaxMB = iMBInFrame;

    pSlice->m_iFirstMBFld = pSlice->m_SliceHeader.first_mb_in_slice + iMBInFrame * iFieldIndex;

    pSlice->m_iAvailableMB = iMBInFrame;

    if (pSlice->m_iFirstMB >= pSlice->m_iAvailableMB)
        return 0;

    // reset all internal variables
    pSlice->m_bFirstDebThreadedCall = true;
    pSlice->m_bError = false;

    // set conditional flags
    pSlice->m_bDecoded = false;
    pSlice->m_bPrevDeblocked = false;
    pSlice->m_bDeblocked = (pSlice->m_SliceHeader.disable_deblocking_filter_idc == DEBLOCK_FILTER_OFF);

    // frame is not associated yet
    pSlice->m_pCurrentFrame = NULL;

    pSlice->m_bInited = true;
    pSlice->m_pSeqParamSet->IncrementReference();
    pSlice->m_pPicParamSet->IncrementReference();
    if (pSlice->m_pSeqParamSetEx)
        pSlice->m_pSeqParamSetEx->IncrementReference();
    if (pSlice->m_pSeqParamSetMvcEx)
        pSlice->m_pSeqParamSetMvcEx->IncrementReference();
    if (pSlice->m_pSeqParamSetSvcEx)
        pSlice->m_pSeqParamSetSvcEx->IncrementReference();

    if (IsShouldSkipSlice(pSlice))
        return 0;

    if (!IsExtension())
    {
        memset(&pSlice->GetSliceHeader()->nal_ext, 0, sizeof(UMC_H264_DECODER::H264NalExtension));
    }

    if (spl)
    {
        if (m_WaitForIDR)
        {
            AllocateAndInitializeView(pSlice);
            ViewItem &view = GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
            uint32_t recoveryFrameNum = (spl->SEI_messages.recovery_point.recovery_frame_cnt + pSlice->GetSliceHeader()->frame_num) % (1 << pSlice->m_pSeqParamSet->log2_max_frame_num);
            view.GetDPBList(0)->SetRecoveryFrameCnt(recoveryFrameNum);
        }

        if ((pSlice->GetSliceHeader()->slice_type != INTRASLICE))
        {
            UMC_H264_DECODER::H264SEIPayLoad * splToRemove = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);
            m_Headers.m_SEIParams.RemoveHeader(splToRemove);
        }
    }

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    return pSlice;
}

Status WidevineTaskSupplier::ParseWidevineSEI(DecryptParametersWrapper* pDecryptParams)
{
    try
    {
        {
            UMC_H264_DECODER::H264SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIBufferingPeriod(m_Headers, &m_SEIPayLoads);
            if (m_SEIPayLoads.isValid)
            {
                UMC_H264_DECODER::H264SEIPayLoad* payload = m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
                m_accessUnit.m_payloads.AddPayload(payload);
            }
        }

        {
            UMC_H264_DECODER::H264SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIPicTiming(m_Headers, &m_SEIPayLoads);
            if (m_SEIPayLoads.isValid)
            {
                UMC_H264_DECODER::H264SEIPayLoad* payload = m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
                m_accessUnit.m_payloads.AddPayload(payload);
            }
        }

        {
            UMC_H264_DECODER::H264SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIRecoveryPoint(&m_SEIPayLoads);
            if (m_SEIPayLoads.isValid)
            {
                UMC_H264_DECODER::H264SEIPayLoad* payload = m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
                m_accessUnit.m_payloads.AddPayload(payload);
            }
        }

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC_OK;
}

Status WidevineTaskSupplier::AddOneFrame(MediaData* src)
{
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return UMC_ERR_FAILED;
    }

    Status umsRes = UMC_OK;

    if (m_pLastSlice)
    {
        umsRes = AddSlice(m_pLastSlice, !src);
        if (umsRes == UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC_OK || umsRes == UMC_ERR_ALLOC)
            return umsRes;
    }

    uint32_t const flags = src ? src->GetFlags() : 0;
    if (flags & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
        return UMC_ERR_FAILED;

    bool decrypted = false;
    int32_t size = src ? (int32_t)src->GetDataSize() : 0;
    DecryptParametersWrapper decryptParams{};

    auto aux = src ? src->GetAuxInfo(MFX_EXTBUFF_DECRYPTED_PARAM) : nullptr;
    auto dp_ext = aux ? reinterpret_cast<mfxExtDecryptedParam*>(aux->ptr) : nullptr;

    if (dp_ext)
    {
        if (!dp_ext->Data       ||
             dp_ext->DataLength != sizeof (DECRYPT_QUERY_STATUS_PARAMS_AVC))
            return UMC_ERR_FAILED;

        decryptParams = *(reinterpret_cast<DECRYPT_QUERY_STATUS_PARAMS_AVC*>(dp_ext->Data));
        decrypted = true;
    }
    else
    {
        void* bsDataPointer = src ? src->GetDataPointer() : 0;

        umsRes = m_pWidevineDecrypter->DecryptFrame(src, &decryptParams);
        if (umsRes != UMC_OK)
            return umsRes;

        decrypted = src ? (bsDataPointer != src->GetDataPointer()) : false;
    }

    decryptParams.SetTime(src ? src->GetTime() : 0);

    if (decrypted)
    {
        umsRes = ParseWidevineSPSPPS(&decryptParams);
        if (umsRes != UMC_OK)
        {
            if (!dp_ext && (umsRes == UMC_NTF_NEW_RESOLUTION))
                src->MoveDataPointer(-size);

            return umsRes;
        }

        umsRes = ParseWidevineSEI(&decryptParams);
        if (umsRes != UMC_OK)
            return umsRes;

        H264Slice * pSlice = ParseWidevineSliceHeader(&decryptParams);
        if (pSlice)
        {
            umsRes = AddSlice(pSlice, !src);
            if (umsRes == UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC_OK || umsRes == UMC_ERR_ALLOC)
                return umsRes;
        }
    }

    return AddSlice(0, !src);
}

Status WidevineTaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, int32_t field)
{
    Status sts = VATaskSupplier::CompleteFrame(pFrame, field);

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->ReleaseForNewBitstream();
    }

    return sts;
}

} // namespace UMC

#endif // #ifndef MFX_ENABLE_CPLIB
#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // UMC_ENABLE_H264_VIDEO_DECODER
