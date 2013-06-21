/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_slice_decoding.h"
#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_heap.h"
#include "umc_h265_frame_info.h"

namespace UMC_HEVC_DECODER
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
    m_iConsumerNumber = IPP_MIN(NUMBER_OF_DEBLOCKERS_H265, iConsumerNumber) * 2;

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
        iLeft = ALIGN_VALUE_H265<Ipp32s> (iMBWidth * iThreadNumber / iConsumerNumber,
                                     iDebUnit);

    // calculate right value
    if (iConsumerNumber - 1 == iThreadNumber)
        iRight = iMBWidth - 1;
    else
        // calculate right as left_for_next_thread - deb_unit
        iRight = ALIGN_VALUE_H265<Ipp32s> ((iMBWidth * (iThreadNumber + 1)) / iConsumerNumber,
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

bool H264ThreadedDeblockingTools::GetMBToProcess(H265Task *pTask)
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
            pTask->m_iTaskID = TASK_DEB_THREADED_H265;
            pTask->m_pSlice = NULL;
            pTask->pFunction = &H265SegmentDecoderMultiThreaded::DeblockSegmentTask;

#ifdef ECHO_DEB
            DEBUG_PRINT(VM_STRING("(%d) dbt - % 4d to % 4d\n"), pTask->m_iThreadNumber,
                pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

            return true;
        }
    }

    return false;

} // bool H264ThreadedDeblockingTools::GetMBToProcess(H265Task *pTask)

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

void H264ThreadedDeblockingTools::SetProcessedMB(H265Task *pTask)
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

} // void H264ThreadedDeblockingTools::SetProcessedMB(H265Task *pTask)

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

int H265Slice::m_prevPOC;

H265Slice::H265Slice(UMC::MemoryAllocator *pMemoryAllocator)
    : m_pSeqParamSet(0)
    , m_pMemoryAllocator(pMemoryAllocator)
    , m_pHeap(0)
{
    m_SliceHeader.m_SubstreamSizes = NULL;
    m_SliceHeader.m_TileByteLocation = NULL;
    Reset();
} // H265Slice::H265Slice()

H265Slice::~H265Slice()
{
    Release();

} // H265Slice::~H265Slice(void)

void H265Slice::Reset()
{
    if (m_pHeap)
    {
        m_pHeap->Free(m_pSource);
    }

    m_pSource = 0;

    if (m_pSeqParamSet)
    {
        ((H265VideoParamSet*)m_pVideoParamSet)->DecrementReference();
        ((H265SeqParamSet*)m_pSeqParamSet)->DecrementReference();
        ((H265PicParamSet*)m_pPicParamSet)->DecrementReference();
        m_pVideoParamSet = 0;
        m_pSeqParamSet = 0;
        m_pPicParamSet = 0;
    }

    m_iMBWidth = -1;
    m_iMBHeight = -1;
    m_CurrentPicParamSet = -1;
    m_CurrentSeqParamSet = -1;
    m_CurrentVideoParamSet = -1;

    m_iAllocatedMB = 0;

    m_pCurrentFrame = 0;

    m_SliceHeader.m_nuh_temporal_id = 0;
    m_SliceHeader.m_CheckLDC = false;
    m_SliceHeader.m_deblockingFilterDisable = false;
    m_SliceHeader.m_numEntryPointOffsets = 0;
    m_SliceHeader.m_TileCount = 0;

    if (m_SliceHeader.m_SubstreamSizes)
    {
        delete[] m_SliceHeader.m_SubstreamSizes;
        m_SliceHeader.m_SubstreamSizes = NULL;
    }
    if (m_SliceHeader.m_TileByteLocation)
    {
        delete[] m_SliceHeader.m_TileByteLocation;
        m_SliceHeader.m_TileByteLocation = NULL;
    }
}

void H265Slice::Release()
{
    m_CoeffsBuffers.Reset();

    Reset();

} // void H265Slice::Release(void)

bool H265Slice::Init(Ipp32s )
{
    // release object before initialization
    Release();

#if 0
    // initialize threading tools
    m_DebTools.Init(iConsumerNumber);
#endif

    return true;

} // bool H265Slice::Init(Ipp32s iConsumerNumber)

Ipp32s H265Slice::RetrievePicParamSetNumber(void *pSource, size_t nSourceSize)
{
    if (!nSourceSize)
        return -1;

    memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));
    m_BitStream.Reset((Ipp8u *) pSource, (Ipp32u) nSourceSize);

    UMC::Status umcRes = UMC::UMC_OK;

    try
    {
        Ipp32u reserved_zero_6bits;
        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.m_nuh_temporal_id,
                                            reserved_zero_6bits);
        VM_ASSERT(0 == reserved_zero_6bits);
        if (UMC::UMC_OK != umcRes)
            return false;

        // decode first part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart1(this);
        if (UMC::UMC_OK != umcRes)
            return -1;
    } catch (...)
    {
        return -1;
    }

    return m_SliceHeader.pic_parameter_set_id;
}

bool H265Slice::Reset(void *pSource, size_t nSourceSize, Ipp32s iConsumerNumber)
{
    Ipp32s iMBInFrame;
    //Ipp32s iCUInFrame;
    Ipp32s iFieldIndex;

    m_BitStream.Reset((Ipp8u *) pSource, (Ipp32u) nSourceSize);

    // decode slice header
    if (nSourceSize && false == DecodeSliceHeader())
        return false;

    m_SliceHeader.m_HeaderBitstreamOffset = m_BitStream.BytesDecoded();

    m_SliceHeader.m_SeqParamSet = m_pSeqParamSet;
    m_SliceHeader.m_PicParamSet = m_pPicParamSet;

    m_iMBWidth  = GetSeqParam()->WidthInCU;
    m_iMBHeight = GetSeqParam()->HeightInCU;

    iMBInFrame = (m_iMBWidth * m_iMBHeight);
    //iCUInFrame = m_iCUWidth * m_iCUHeight;
    iFieldIndex = 0;

    // set slice variables
    m_iFirstMB = m_SliceHeader.m_sliceAddr;
    m_iMaxMB = iMBInFrame;

    m_iAvailableMB = iMBInFrame;

    if (m_iFirstMB >= m_iAvailableMB)
        return false;

    // reset all internal variables
    m_iCurMBToDec = 0;
    m_iCurMBToRec = 0;
    m_iCurMBToDeb = m_iFirstMB;

    m_bInProcess = false;
    m_bDecVacant = 1;
    m_bRecVacant = 1;
    m_bDebVacant = 1;
    m_bFirstDebThreadedCall = true;
    m_bError = false;

    m_MVsDistortion = 0;

    // reallocate internal buffer
    if (iConsumerNumber > 1)
    {
        Ipp32s iMBRowSize = GetMBRowWidth();
        Ipp32s iMBRowBuffers;
        Ipp32s bit_depth_luma, bit_depth_chroma;
        bit_depth_luma = GetSeqParam()->bit_depth_luma;
        bit_depth_chroma = GetSeqParam()->bit_depth_chroma;

        Ipp32s isU16Mode = (bit_depth_luma > 8 || bit_depth_chroma > 8) ? 2 : 1;

        // decide number of buffers
        iMBRowBuffers = IPP_MAX(MINIMUM_NUMBER_OF_ROWS_H265, MB_BUFFER_SIZE_H265 / iMBRowSize);
        iMBRowBuffers = IPP_MIN(MAXIMUM_NUMBER_OF_ROWS_H265, iMBRowBuffers);

        m_CoeffsBuffers.Init(iMBRowBuffers, (Ipp32s)sizeof(Ipp16s) * isU16Mode * (iMBRowSize * COEFFICIENTS_BUFFER_SIZE_H265 + UMC::DEFAULT_ALIGN_VALUE));
    }

    m_CoeffsBuffers.Reset();

    // reset through-decoding variables
    m_nMBSkipCount = 0;
    //m_nQuantPrev = m_pPicParamSet->pic_init_qp +
    //               m_SliceHeader.slice_qp_delta;
    m_nQuantPrev = m_SliceHeader.SliceQP + m_SliceHeader.slice_qp_delta;
    m_prev_dquant = 0;

    m_bNeedToCheckMBSliceEdges = (0 == m_SliceHeader.m_sliceAddr) ? (false) : (true);

    // set conditional flags
    m_bDecoded = false;
    m_bPrevDeblocked = false;
    m_bDeblocked = false;
    m_bSAOed = false;

    if (m_bDeblocked)
    {
        m_bDebVacant = 0;
        m_iCurMBToDeb = m_iMaxMB;
    }

    // frame is not associated yet
    m_pCurrentFrame = NULL;

    m_pVideoParamSet->IncrementReference();
    m_pSeqParamSet->IncrementReference();
    m_pPicParamSet->IncrementReference();

    return true;

} // bool H265Slice::Reset(void *pSource, size_t nSourceSize, Ipp32s iNumber)

void H265Slice::SetSliceNumber(Ipp32s iSliceNumber)
{
    m_iNumber = iSliceNumber;

} // void H265Slice::SetSliceNumber(Ipp32s iSliceNumber)

bool H265Slice::DecodeSliceHeader(bool bFullInitialization)
{
    UMC::Status umcRes = UMC::UMC_OK;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
    Ipp32s iSQUANT;
    try
    {
        memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));

        Ipp32u reserved_zero_6bits;
        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.m_nuh_temporal_id,
                                            reserved_zero_6bits);
        VM_ASSERT(0 == reserved_zero_6bits);
        if (UMC::UMC_OK != umcRes)
            return false;

        umcRes = m_BitStream.GetSliceHeaderFull(this, m_pSeqParamSet, m_pPicParamSet);

        m_CurrentPicParamSet = m_SliceHeader.pic_parameter_set_id;
        m_CurrentSeqParamSet = m_pPicParamSet->seq_parameter_set_id;
        m_CurrentVideoParamSet = m_pSeqParamSet->sps_video_parameter_set_id;

        // when we require only slice header
        if (false == bFullInitialization)
            return true;

        Ipp32s bit_depth_luma = GetSeqParam()->bit_depth_luma;

        iSQUANT = m_SliceHeader.SliceQP + m_SliceHeader.slice_qp_delta;
        if (iSQUANT < QP_MIN - 6*(bit_depth_luma - 8)
            || iSQUANT > QP_MAX)
        {
            return false;
        }
    }
    catch(const h265_exception & )
    {
        return false;
    }
    catch(...)
    {
        return false;
    }

    return (UMC::UMC_OK == umcRes);

} // bool H265Slice::DecodeSliceHeader(bool bFullInitialization)

void H265Slice::setRefPOCList()
{
    for (Ipp32s iDir = 0; iDir < 2; iDir++)
    {
        H265DecoderRefPicList::ReferenceInformation *refInfo = m_pCurrentFrame->GetRefPicList(m_iNumber, iDir)->m_refPicList;

        for (Ipp32s NumRefIdx = 0; NumRefIdx < m_SliceHeader.m_numRefIdx[iDir]; NumRefIdx++)
        {
            m_SliceHeader.RefPOCList[iDir][NumRefIdx] = refInfo[NumRefIdx].refFrame->m_PicOrderCnt;
            m_SliceHeader.RefLTList[iDir][NumRefIdx] = refInfo[NumRefIdx].isLongReference;
        }
    }
}

void H265Slice::InitializeContexts()
{
    SliceType slice_type = m_SliceHeader.slice_type;

    if (m_pPicParamSet->cabac_init_present_flag && m_SliceHeader.m_CabacInitFlag)
    {
        switch (slice_type)
        {
            case P_SLICE:
                slice_type = B_SLICE;
                break;
            case B_SLICE:
                slice_type = P_SLICE;
                break;
            default:
                VM_ASSERT(0);
        }
    }

    Ipp32s InitializationType;
    if (I_SLICE == slice_type)
    {
        InitializationType = 2;
    }
    else if (P_SLICE == slice_type)
    {
        InitializationType = 1;
    }
    else
    {
        InitializationType = 0;
    }

    m_BitStream.InitializeContextVariablesHEVC_CABAC(InitializationType,
        m_SliceHeader.SliceQP + m_SliceHeader.slice_qp_delta);
}

bool H265Slice::GetDeblockingCondition(void) const
{
    // there is no deblocking
    if (getDeblockingFilterDisable())
        return false;

    // no filtering edges of this slice
    if (!getLFCrossSliceBoundaryFlag() || (m_bPrevDeblocked))
    {
        return true;
    }

    return false;

} // bool H265Slice::GetDeblockingCondition(void)

int H265Slice::getNumRpsCurrTempList() const
{
  int numRpsCurrTempList = 0;

  if (!isIntra())
  {
      ReferencePictureSet *rps = getRPS();

      for(int i=0;i < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures();i++)
      {
        if(rps->getUsed(i))
        {
          numRpsCurrTempList++;
        }
      }
  }

  return numRpsCurrTempList;
}

void  H265Slice::initWpScaling(wpScalingParam  wp[2][MAX_NUM_REF][3])
{
  for ( int e=0 ; e<2 ; e++ )
  {
    for ( int i=0 ; i<MAX_NUM_REF ; i++ )
    {
      for ( int yuv=0 ; yuv<3 ; yuv++ )
      {
        wpScalingParam  *pwp = &(wp[e][i][yuv]);
        if ( !pwp->bPresentFlag ) {
          // Inferring values not present :
          pwp->iWeight = (1 << pwp->uiLog2WeightDenom);
          pwp->iOffset = 0;
        }

        pwp->w      = pwp->iWeight;
        int bitDepth = yuv ? g_bitDepthC : g_bitDepthY;
        pwp->o      = pwp->iOffset * (1 << (bitDepth-8));
        pwp->shift  = pwp->uiLog2WeightDenom;
        pwp->round  = (pwp->uiLog2WeightDenom>=1) ? (1 << (pwp->uiLog2WeightDenom-1)) : (0);
      }
    }
  }
}

void H265Slice::allocSubstreamSizes(unsigned uiNumSubstreams)
{
    if (NULL != m_SliceHeader.m_SubstreamSizes)
        delete[] m_SliceHeader.m_SubstreamSizes;
    m_SliceHeader.m_SubstreamSizes = new unsigned[uiNumSubstreams > 0 ? uiNumSubstreams - 1 : 0];
}

void H265Slice::CopyFromBaseSlice(const H265Slice * s)
{
    if (!s || !m_SliceHeader.m_DependentSliceSegmentFlag)
        return;

    VM_ASSERT(s);
    m_iNumber = s->m_iNumber;

    const H265SliceHeader * slice = s->GetSliceHeader();

    m_SliceHeader.IdrPicFlag = slice->IdrPicFlag;
    m_SliceHeader.pic_order_cnt_lsb = slice->pic_order_cnt_lsb;
    m_SliceHeader.nal_unit_type = slice->nal_unit_type;
    m_SliceHeader.SliceQP = slice->SliceQP;

    m_SliceHeader.m_deblockingFilterDisable   = slice->m_deblockingFilterDisable;
    m_SliceHeader.m_deblockingFilterOverrideFlag = slice->m_deblockingFilterOverrideFlag;
    m_SliceHeader.m_deblockingFilterBetaOffsetDiv2 = slice->m_deblockingFilterBetaOffsetDiv2;
    m_SliceHeader.m_deblockingFilterTcOffsetDiv2 = slice->m_deblockingFilterTcOffsetDiv2;

    for (Ipp32s i = 0; i < 3; i++)
    {
        m_SliceHeader.m_numRefIdx[i]     = slice->m_numRefIdx[i];
    }

    /*for (i = 0; i < MAX_NUM_REF; i++)
    {
        m_SliceHeader.m_list1IdxToList0Idx[i] = slice->m_list1IdxToList0Idx[i];
    } */

    m_SliceHeader.m_CheckLDC            = slice->m_CheckLDC;
    m_SliceHeader.slice_type            = slice->slice_type;
    m_SliceHeader.slice_qp_delta        = slice->slice_qp_delta;
    m_SliceHeader.m_slice_qp_delta_cb   = slice->m_slice_qp_delta_cb;
    m_SliceHeader.m_slice_qp_delta_cr   = slice->m_slice_qp_delta_cr;

    for (Ipp32s i = 0; i < 2; i++)
    {
        for (Ipp32s j = 0; j < MAX_NUM_REF; j++)
        {
            //m_SliceHeader.m_apcRefPicList[i][j]  = slice->m_apcRefPicList[i][j];
            m_SliceHeader.RefPOCList[i][j] = slice->RefPOCList[i][j];
            m_SliceHeader.RefLTList[i][j] = slice->RefLTList[i][j];
        }
    }

    for (Ipp32s i = 0; i < 2; i++)
    {
        for (Ipp32s j = 0; j < MAX_NUM_REF + 1; j++)
        {
            //m_bIsUsedAsLongTerm[i][j] = slice->m_bIsUsedAsLongTerm[i][j];
        }
    }

    /*m_SliceHeader.m_iDepth               = slice->m_iDepth;

    // access channel
    m_SliceHeader.m_pcSPS                = slice->m_pcSPS;
    m_SliceHeader.m_pcPPS                = slice->m_pcPPS;
    m_SliceHeader.m_iLastIDR             = slice->m_iLastIDR;

    m_SliceHeader.m_pcPic                = slice->m_pcPic;*/

    m_SliceHeader.m_pRPS                    = slice->m_pRPS;

    m_SliceHeader.collocated_from_l0_flag   = slice->collocated_from_l0_flag;
    m_SliceHeader.m_ColRefIdx               = slice->m_ColRefIdx;

    m_SliceHeader.m_nuh_temporal_id                      = slice->m_nuh_temporal_id;

    for ( Ipp32s  e=0 ; e<2 ; e++ )
    {
        for ( Ipp32s  n=0 ; n<MAX_NUM_REF ; n++ )
        {
            memcpy(m_SliceHeader.m_weightPredTable[e][n], slice->m_weightPredTable[e][n], sizeof(wpScalingParam)*3 );
        }
    }
    m_SliceHeader.m_SaoEnabledFlag = slice->m_SaoEnabledFlag;
    m_SliceHeader.m_SaoEnabledFlagChroma = slice->m_SaoEnabledFlagChroma;
    m_SliceHeader.m_CabacInitFlag        = slice->m_CabacInitFlag;
    m_SliceHeader.m_numEntryPointOffsets  = slice->m_numEntryPointOffsets;

    m_SliceHeader.m_MvdL1Zero = slice->m_MvdL1Zero;
    m_SliceHeader.m_LFCrossSliceBoundaryFlag = slice->m_LFCrossSliceBoundaryFlag;
    m_SliceHeader.m_enableTMVPFlag                = slice->m_enableTMVPFlag;
    m_SliceHeader.m_MaxNumMergeCand               = slice->m_MaxNumMergeCand;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
