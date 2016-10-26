//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "ipps.h"
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_frame.h"
#include "umc_h264_slice_decoding.h"
#include "umc_h264_heap.h"

namespace UMC
{

H264Slice::H264Slice(MemoryAllocator *pMemoryAllocator)
    : m_pSeqParamSet(0)
    , m_bInited(false)
    , m_pMemoryAllocator(pMemoryAllocator)
{
    Reset();
} // H264Slice::H264Slice()

H264Slice::~H264Slice()
{
    Release();

} // H264Slice::~H264Slice(void)

void H264Slice::Reset()
{
    m_pSource.Release();

    if (m_bInited && m_pSeqParamSet)
    {
        if (m_pSeqParamSet)
            ((H264SeqParamSet*)m_pSeqParamSet)->DecrementReference();
        if (m_pPicParamSet)
            ((H264PicParamSet*)m_pPicParamSet)->DecrementReference();
        m_pSeqParamSet = 0;
        m_pPicParamSet = 0;

        if (m_pSeqParamSetEx)
        {
            ((H264SeqParamSetExtension*)m_pSeqParamSetEx)->DecrementReference();
            m_pSeqParamSetEx = 0;
        }

        if (m_pSeqParamSetMvcEx)
        {
            ((H264SeqParamSetMVCExtension*)m_pSeqParamSetMvcEx)->DecrementReference();
            m_pSeqParamSetMvcEx = 0;
        }

        if (m_pSeqParamSetSvcEx)
        {
            ((H264SeqParamSetSVCExtension*)m_pSeqParamSetSvcEx)->DecrementReference();
            m_pSeqParamSetSvcEx = 0;
        }
    }

    ZeroedMembers();
    FreeResources();
}

void H264Slice::ZeroedMembers()
{
    m_pPicParamSet = 0;
    m_pSeqParamSet = 0;
    m_pSeqParamSetEx = 0;
    m_pSeqParamSetMvcEx = 0;
    m_pSeqParamSetSvcEx = 0;

    m_iMBWidth = -1;
    m_iMBHeight = -1;

    m_pCurrentFrame = 0;

    memset(&m_AdaptiveMarkingInfo, 0, sizeof(m_AdaptiveMarkingInfo));

    m_bInited = false;
    m_isInitialized = false;
}

void H264Slice::Release()
{
    Reset();
}

Ipp32s H264Slice::RetrievePicParamSetNumber()
{
    if (!m_pSource.GetDataSize())
        return -1;

    memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));
    m_BitStream.Reset((Ipp8u *) m_pSource.GetPointer(), (Ipp32u) m_pSource.GetDataSize());

    Status umcRes = UMC_OK;

    try
    {
        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.nal_ref_idc);
        if (UMC_OK != umcRes)
            return false;

        // decode first part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart1(&m_SliceHeader);
        if (UMC_OK != umcRes)
            return -1;
    } catch (...)
    {
        return -1;
    }

    return m_SliceHeader.pic_parameter_set_id;
}

void H264Slice::FreeResources()
{
}

bool H264Slice::Reset(H264NalExtension *pNalExt)
{
    Ipp32s iMBInFrame;
    Ipp32s iFieldIndex;

    m_BitStream.Reset((Ipp8u *) m_pSource.GetPointer(), (Ipp32u) m_pSource.GetDataSize());

    // decode slice header
    if (m_pSource.GetDataSize() && false == DecodeSliceHeader(pNalExt))
    {
        ZeroedMembers();
        return false;
    }

    m_iMBWidth  = GetSeqParam()->frame_width_in_mbs;
    m_iMBHeight = GetSeqParam()->frame_height_in_mbs;

    iMBInFrame = (m_iMBWidth * m_iMBHeight) / ((m_SliceHeader.field_pic_flag) ? (2) : (1));
    iFieldIndex = (m_SliceHeader.field_pic_flag && m_SliceHeader.bottom_field_flag) ? (1) : (0);

    // set slice variables
    m_iFirstMB = m_SliceHeader.first_mb_in_slice;
    m_iMaxMB = iMBInFrame;

    m_iFirstMBFld = m_SliceHeader.first_mb_in_slice + iMBInFrame * iFieldIndex;

    m_iAvailableMB = iMBInFrame;

    if (m_iFirstMB >= m_iAvailableMB)
        return false;

    // reset all internal variables
    m_bFirstDebThreadedCall = true;
    m_bError = false;

    // set conditional flags
    m_bDecoded = false;
    m_bPrevDeblocked = false;
    m_bDeblocked = (m_SliceHeader.disable_deblocking_filter_idc == DEBLOCK_FILTER_OFF);

    // frame is not associated yet
    m_pCurrentFrame = NULL;

    m_bInited = true;
    m_pSeqParamSet->IncrementReference();
    m_pPicParamSet->IncrementReference();
    if (m_pSeqParamSetEx)
        m_pSeqParamSetEx->IncrementReference();
    if (m_pSeqParamSetMvcEx)
        m_pSeqParamSetMvcEx->IncrementReference();
    if (m_pSeqParamSetSvcEx)
        m_pSeqParamSetSvcEx->IncrementReference();

    return true;

} // bool H264Slice::Reset(void *pSource, size_t nSourceSize, Ipp32s iNumber)

void H264Slice::SetSeqMVCParam(const H264SeqParamSetMVCExtension * sps)
{
    const H264SeqParamSetMVCExtension * temp = m_pSeqParamSetMvcEx;
    m_pSeqParamSetMvcEx = sps;
    if (m_pSeqParamSetMvcEx)
        m_pSeqParamSetMvcEx->IncrementReference();

    if (temp)
        ((H264SeqParamSetMVCExtension*)temp)->DecrementReference();
}

void H264Slice::SetSeqSVCParam(const H264SeqParamSetSVCExtension * sps)
{
    const H264SeqParamSetSVCExtension * temp = m_pSeqParamSetSvcEx;
    m_pSeqParamSetSvcEx = sps;
    if (m_pSeqParamSetSvcEx)
        m_pSeqParamSetSvcEx->IncrementReference();

    if (temp)
        ((H264SeqParamSetSVCExtension*)temp)->DecrementReference();
}

void H264Slice::SetSliceNumber(Ipp32s iSliceNumber)
{
    m_iNumber = iSliceNumber;
}

AdaptiveMarkingInfo * H264Slice::GetAdaptiveMarkingInfo()
{
    return &m_AdaptiveMarkingInfo;
}

bool H264Slice::DecodeSliceHeader(H264NalExtension *pNalExt)
{
    Status umcRes = UMC_OK;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
    Ipp32s iSQUANT;

    try
    {
        memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));

        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.nal_ref_idc);
        if (UMC_OK != umcRes)
            return false;

        if (pNalExt && (NAL_UT_CODED_SLICE_EXTENSION != m_SliceHeader.nal_unit_type))
        {
            if (pNalExt->extension_present)
                m_SliceHeader.nal_ext = *pNalExt;
            pNalExt->extension_present = 0;
        }

        // adjust slice type for auxilary slice
        if (NAL_UT_AUXILIARY == m_SliceHeader.nal_unit_type)
        {
            if (!m_pCurrentFrame || !GetSeqParamEx())
                return false;

            m_SliceHeader.nal_unit_type = m_pCurrentFrame->m_bIDRFlag ?
                                          NAL_UT_IDR_SLICE :
                                          NAL_UT_SLICE;
            m_SliceHeader.is_auxiliary = true;
        }

        // decode first part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart1(&m_SliceHeader);
        if (UMC_OK != umcRes)
            return false;


        // decode second part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart2(&m_SliceHeader,
                                                 m_pPicParamSet,
                                                 m_pSeqParamSet);
        if (UMC_OK != umcRes)
            return false;

        // decode second part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart3(&m_SliceHeader,
                                                 m_PredWeight[0],
                                                 m_PredWeight[1],
                                                 &ReorderInfoL0,
                                                 &ReorderInfoL1,
                                                 &m_AdaptiveMarkingInfo,
                                                 &m_BaseAdaptiveMarkingInfo,
                                                 m_pPicParamSet,
                                                 m_pSeqParamSet,
                                                 m_pSeqParamSetSvcEx);
        if (UMC_OK != umcRes)
            return false;

        // decode 4 part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart4(&m_SliceHeader, m_pSeqParamSetSvcEx);
        if (UMC_OK != umcRes)
            return false;

        m_iMBWidth = m_pSeqParamSet->frame_width_in_mbs;
        m_iMBHeight = m_pSeqParamSet->frame_height_in_mbs;

        // redundant slice, discard
        if (m_SliceHeader.redundant_pic_cnt)
            return false;

        // Set next MB.
        if (m_SliceHeader.first_mb_in_slice >= (Ipp32s) (m_iMBWidth * m_iMBHeight))
        {
            return false;
        }

        Ipp32s bit_depth_luma = m_SliceHeader.is_auxiliary ?
            GetSeqParamEx()->bit_depth_aux : GetSeqParam()->bit_depth_luma;

        iSQUANT = m_pPicParamSet->pic_init_qp +
                  m_SliceHeader.slice_qp_delta;
        if (iSQUANT < QP_MIN - 6*(bit_depth_luma - 8)
            || iSQUANT > QP_MAX)
        {
            return false;
        }

        m_SliceHeader.hw_wa_redundant_elimination_bits[2] = (Ipp32u)m_BitStream.BitsDecoded();

        if (m_pPicParamSet->entropy_coding_mode)
            m_BitStream.AlignPointerRight();
    }
    catch(const h264_exception & )
    {
        return false;
    }
    catch(...)
    {
        return false;
    }

    return (UMC_OK == umcRes);

}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
