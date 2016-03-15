/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "ipps.h"
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_slice_decoding.h"
#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_heap.h"

namespace UMC
{


#if 0
H264ThreadedDeblockingTools::H264ThreadedDeblockingTools(void)
{

} // H264ThreadedDeblockingTools::H264ThreadedDeblockingTools(void)

H264ThreadedDeblockingTools::~H264ThreadedDeblockingTools(void)
{
    Release();

} // H264ThreadedDeblockingTools::~H264ThreadedDeblockingTools(void)

void H264ThreadedDeblockingTools::Release(void)
{
    // there is nothing to do

} // void H264ThreadedDeblockingTools::Release(void)

bool H264ThreadedDeblockingTools::Init(Ipp32s iConsumerNumber)
{
    // release object before initialization
    Release();

    // save variables
    m_iConsumerNumber = IPP_MIN(NUMBER_OF_DEBLOCKERS, iConsumerNumber) * 2;

    // allocate working flags
    if (false == m_bThreadWorking.Init(m_iConsumerNumber))
        return false;

    // allocate array for current macroblock number
    if (false == m_iCurMBToDeb.Init(m_iConsumerNumber))
        return false;

    return true;

} // bool H264ThreadedDeblockingTools::Init(Ipp32s iConsumerNumber)

static void GetDeblockingRange(Ipp32s &iLeft,
                        Ipp32s &iRight,
                        Ipp32s iMBWidth,
                        Ipp32s iThreadNumber,
                        Ipp32s iConsumerNumber,
                        Ipp32s iDebUnit)
{
    // calculate left value
    if (0 == iThreadNumber)
        iLeft = 0;
    else
        iLeft = align_value<Ipp32s> (iMBWidth * iThreadNumber / iConsumerNumber,
                                     iDebUnit);

    // calculate right value
    if (iConsumerNumber - 1 == iThreadNumber)
        iRight = iMBWidth - 1;
    else
        // calculate right as left_for_next_thread - deb_unit
        iRight = align_value<Ipp32s> ((iMBWidth * (iThreadNumber + 1)) / iConsumerNumber,
                                      iDebUnit) - 1;

} // void GetDeblockingRange(Ipp32s &iLeft,

void H264ThreadedDeblockingTools::Reset(Ipp32s iFirstMB, Ipp32s iMaxMB, Ipp32s iDebUnit, Ipp32s iMBWidth)
{
    Ipp32s i;
    Ipp32s iCurrMB = iFirstMB;
    Ipp32s iCurrMBInRow = iCurrMB % iMBWidth;

    // save variables
    m_iMaxMB = iMaxMB;
    m_iDebUnit = iDebUnit;
    m_iMBWidth = iMBWidth;

    // reset event(s) & current position(s)
    for (i = 0; i < m_iConsumerNumber; i += 1)
    {
        Ipp32s iLeft, iRight;

        // get working range for this thread
        GetDeblockingRange(iLeft, iRight, iMBWidth, i, m_iConsumerNumber, iDebUnit);

        // reset current MB to deblock
        if (iCurrMBInRow < iLeft)
            m_iCurMBToDeb[i] = iCurrMB - iCurrMBInRow + iLeft;
        else if (iCurrMBInRow > iRight)
            m_iCurMBToDeb[i] = iCurrMB - iCurrMBInRow + iMBWidth + iLeft;
        else
            m_iCurMBToDeb[i] = iCurrMB;

        // set current thread working status
        m_bThreadWorking[i] = false;
    }

} // void H264ThreadedDeblockingTools::Reset(Ipp32s iFirstMB, Ipp32s iMaxMB, Ipp32s iDebUnit, Ipp32s iMBWidth)

bool H264ThreadedDeblockingTools::GetMBToProcess(H264Task *pTask)
{
    Ipp32s iThreadNumber;
    Ipp32s iFirstMB, iMBToProcess;

    for (iThreadNumber = 0; iThreadNumber < m_iConsumerNumber; iThreadNumber += 1)
    {
        if (GetMB(iThreadNumber, iFirstMB, iMBToProcess))
        {
            // prepare task
            pTask->m_bDone = false;
            pTask->m_bError = false;
            pTask->m_iFirstMB = iFirstMB;
            pTask->m_iMaxMB = m_iMaxMB;
            pTask->m_iMBToProcess = iMBToProcess;
            pTask->m_iTaskID = TASK_DEB_THREADED;
            pTask->m_pSlice = NULL;
            pTask->pFunction = &H264SegmentDecoderMultiThreaded::DeblockSegmentTask;

#ifdef ECHO_DEB
            DEBUG_PRINT(VM_STRING("(%d) dbt - % 4d to % 4d\n"), pTask->m_iThreadNumber,
                pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

            return true;
        }
    }

    return false;

} // bool H264ThreadedDeblockingTools::GetMBToProcess(H264Task *pTask)

bool H264ThreadedDeblockingTools::GetMB(Ipp32s iThreadNumber,
                                        Ipp32s &iFirstMB,
                                        Ipp32s &iMBToProcess)
{
    Ipp32s iCur;
    Ipp32s iNumber = iThreadNumber;
    Ipp32s iLeft, iRight;

    // do we have anything to do ?
    iCur = m_iCurMBToDeb[iNumber];
    if ((iCur >= m_iMaxMB) ||
        (m_bThreadWorking[iThreadNumber]))
        return false;

    // get working range for this thread
    GetDeblockingRange(iLeft, iRight, m_iMBWidth, iNumber, m_iConsumerNumber, m_iDebUnit);

    // the left column
    if (0 == iNumber)
    {
        if (m_iCurMBToDeb[iNumber + 1] <= iCur - (iCur % m_iMBWidth) - m_iMBWidth + iRight + m_iDebUnit)
            return false;

        iFirstMB = m_iCurMBToDeb[iNumber];
        iMBToProcess = iRight - IPP_MAX(iLeft, iCur % m_iMBWidth) + 1;
        m_bThreadWorking[iThreadNumber] = true;

        return true;
    }

    // a middle column
    if (m_iConsumerNumber - 1 != iNumber)
    {
        if ((m_iCurMBToDeb[iNumber - 1] < iCur) ||
            (m_iCurMBToDeb[iNumber + 1] <= iCur - (iCur % m_iMBWidth) - m_iMBWidth + iRight + m_iDebUnit))
            return false;

        iFirstMB = m_iCurMBToDeb[iNumber];
        iMBToProcess = iRight - IPP_MAX(iLeft, iCur % m_iMBWidth) + 1;
        m_bThreadWorking[iThreadNumber] = true;

        return true;
    }

    // the right column
    if (m_iConsumerNumber - 1 == iNumber)
    {
        if (m_iCurMBToDeb[iNumber - 1] < iCur)
            return false;

        iFirstMB = m_iCurMBToDeb[iNumber];
        iMBToProcess = iRight - IPP_MAX(iLeft, iCur % m_iMBWidth) + 1;
        m_bThreadWorking[iThreadNumber] = true;

        return true;
    }

    return false;

} // bool H264ThreadedDeblockingTools::GetMB(Ipp32s iThreadNumber,

void H264ThreadedDeblockingTools::SetProcessedMB(H264Task *pTask)
{
    Ipp32s iThreadNumber;

    for (iThreadNumber = 0; iThreadNumber < m_iConsumerNumber; iThreadNumber += 1)
    {
        if (m_iCurMBToDeb[iThreadNumber] == pTask->m_iFirstMB)
        {
            SetMB(iThreadNumber, m_iCurMBToDeb[iThreadNumber], pTask->m_iMBToProcess);
            break;
        }
    }

} // void H264ThreadedDeblockingTools::SetProcessedMB(H264Task *pTask)

void H264ThreadedDeblockingTools::SetMB(Ipp32s iThreadNumber,
                                        Ipp32s iFirstMB,
                                        Ipp32s iMBToProcess)
{
    Ipp32s iLeft, iRight, iCur;
    Ipp32s iNumber;

    // fill variables
    iNumber = iThreadNumber;
    iCur = iFirstMB;

    // get working range for this thread
    GetDeblockingRange(iLeft, iRight, m_iMBWidth, iNumber, m_iConsumerNumber, m_iDebUnit);

    VM_ASSERT(iCur == m_iCurMBToDeb[iNumber]);

    // increment current working MB index
    iCur += iMBToProcess;

    // correct current macroblock index to working range
    {
        Ipp32s iRest = iCur % m_iMBWidth;

        iRest = (iRest) ? (iRest) : (m_iMBWidth);
        if (iRest > iRight)
            iCur = iCur - iRest + m_iMBWidth + iLeft;

        // save current working MB index
        m_iCurMBToDeb[iNumber] = iCur;
    }

    m_bThreadWorking[iNumber] = false;

} // void H264ThreadedDeblockingTools::SetMB(Ipp32s iThreadNumber,

bool H264ThreadedDeblockingTools::IsDeblockingDone(void)
{
    bool bDeblocked = false;
    Ipp32s i;

    // test whole slice deblocking condition
    for (i = 0; i < m_iConsumerNumber; i += 1)
    {
        if (m_iCurMBToDeb[i] < m_iMaxMB)
            break;
    }
    if (m_iConsumerNumber == i)
        bDeblocked = true;

    return bDeblocked;

} // bool H264ThreadedDeblockingTools::IsDeblockingDone(void)
#endif

H264Slice::H264Slice(MemoryAllocator *pMemoryAllocator)
    : m_pSeqParamSet(0)
    , m_coeffsBuffers(0)
    , m_pMemoryAllocator(pMemoryAllocator)
    , m_bInited(false)
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
    m_mbinfo = 0;

    m_iMBWidth = -1;
    m_iMBHeight = -1;

    m_pCurrentFrame = 0;

    m_DistScaleFactorAFF = 0;
    m_DistScaleFactorMVAFF = 0;
    memset(&m_AdaptiveMarkingInfo, 0, sizeof(m_AdaptiveMarkingInfo));

    m_bInited = false;
    m_isInitialized = false;
}

void H264Slice::Release()
{
    if (GetCoeffsBuffers())
        m_coeffsBuffers->Reset();

    m_pObjHeap->FreeObject(m_DistScaleFactorAFF);
    m_pObjHeap->FreeObject(m_DistScaleFactorMVAFF);

    Reset();

} // void H264Slice::Release(void)

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

H264CoeffsBuffer *H264Slice::GetCoeffsBuffers() const
{
    return m_coeffsBuffers;
}

void H264Slice::FreeResources()
{
    if (!GetCoeffsBuffers())
        return;

    m_coeffsBuffers->DecrementReference();
    m_coeffsBuffers = 0;
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
    m_iCurMBToDec = m_iFirstMB;
    m_iCurMBToRec = m_iFirstMB;
    m_iCurMBToDeb = m_iFirstMB;

    m_bInProcess = false;
    m_bDecVacant = 1;
    m_bRecVacant = 1;
    m_bDebVacant = 1;
    m_bFirstDebThreadedCall = true;
    m_bError = false;

    m_MVsDistortion = 0;

    // reset through-decoding variables
    m_nMBSkipCount = 0;
    m_nQuantPrev = m_pPicParamSet->pic_init_qp +
                   m_SliceHeader.slice_qp_delta;
    m_prev_dquant = 0;
    m_field_index = iFieldIndex;

    if (IsSliceGroups())
        m_bNeedToCheckMBSliceEdges = true;
    else
        m_bNeedToCheckMBSliceEdges = (0 == m_SliceHeader.first_mb_in_slice) ? (false) : (true);

    // set conditional flags
    m_bDecoded = false;
    m_bPrevDeblocked = false;
    m_bDeblocked = (m_SliceHeader.disable_deblocking_filter_idc == DEBLOCK_FILTER_OFF);

    if (m_bDeblocked)
    {
        m_bDebVacant = 0;
        m_iCurMBToDeb = m_iMaxMB;
    }

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

} // void H264Slice::SetSliceNumber(Ipp32s iSliceNumber)

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

} // bool H264Slice::DecodeSliceHeader(bool bFullInitialization)

void H264Slice::InitializeContexts()
{
    if (!m_isInitialized && m_pPicParamSet->entropy_coding_mode)
    {
        // reset CABAC engine
        m_BitStream.InitializeDecodingEngine_CABAC();
        if (INTRASLICE == m_SliceHeader.slice_type)
        {
            m_BitStream.InitializeContextVariablesIntra_CABAC(m_pPicParamSet->pic_init_qp +
                                                              m_SliceHeader.slice_qp_delta);
        }
        else
        {
            m_BitStream.InitializeContextVariablesInter_CABAC(m_pPicParamSet->pic_init_qp +
                                                              GetSliceHeader()->slice_qp_delta,
                                                              GetSliceHeader()->cabac_init_idc);
        }
    }

    m_isInitialized = true;
}

bool H264Slice::GetDeblockingCondition(void) const
{
    // there is no deblocking
    if (DEBLOCK_FILTER_OFF == GetSliceHeader()->disable_deblocking_filter_idc)
        return false;

    // no filtering edges of this slice
    if (DEBLOCK_FILTER_ON_NO_SLICE_EDGES == GetSliceHeader()->disable_deblocking_filter_idc || m_bPrevDeblocked)
    {
        if (false == IsSliceGroups())
            return true;
    }

    return false;

} // bool H264Slice::GetDeblockingCondition(void)

void H264Slice::CompleteDecoding()
{
    //if (m_bDecoded)//  && m_bDeblocked) - we do not need to wait deblocking because FreeResources frees coeff buffer
        FreeResources();
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
