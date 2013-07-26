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
int H265Slice::m_prevPOC;

H265Slice::H265Slice(UMC::MemoryAllocator *pMemoryAllocator)
    : m_pSeqParamSet(0)
    , m_pMemoryAllocator(pMemoryAllocator)
    , m_pHeap(0)
    , m_context(0)
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

    m_pCurrentFrame = 0;

    m_SliceHeader.m_nuh_temporal_id = 0;
    m_SliceHeader.m_CheckLDC = false;
    m_SliceHeader.m_deblockingFilterDisable = false;
    m_SliceHeader.m_numEntryPointOffsets = 0;
    m_SliceHeader.m_TileCount = 0;

    delete[] m_SliceHeader.m_SubstreamSizes;
    m_SliceHeader.m_SubstreamSizes = NULL;

    delete[] m_SliceHeader.m_TileByteLocation;
    m_SliceHeader.m_TileByteLocation = NULL;
}

void H265Slice::Release()
{
    Reset();
} // void H265Slice::Release(void)

bool H265Slice::Init(Ipp32s )
{
    Release();
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

bool H265Slice::Reset(void *pSource, size_t nSourceSize, Ipp32s )
{
    m_BitStream.Reset((Ipp8u *) pSource, (Ipp32u) nSourceSize);

    // decode slice header
    if (nSourceSize && false == DecodeSliceHeader())
        return false;

    m_SliceHeader.m_HeaderBitstreamOffset = m_BitStream.BytesDecoded();

    m_SliceHeader.m_SeqParamSet = m_pSeqParamSet;
    m_SliceHeader.m_PicParamSet = m_pPicParamSet;

    Ipp32s iMBInFrame = (GetSeqParam()->WidthInCU * GetSeqParam()->HeightInCU);

    // set slice variables
    m_iFirstMB = m_SliceHeader.slice_segment_address;
    m_iFirstMB = m_iFirstMB > iMBInFrame ? iMBInFrame : m_iFirstMB;
    m_iFirstMB = m_pPicParamSet->m_CtbAddrRStoTS[m_iFirstMB];
    m_iMaxMB = iMBInFrame;

    m_iAvailableMB = iMBInFrame;

    if (m_iFirstMB >= m_iAvailableMB)
        return false;

    // reset all internal variables
    m_mvsDistortion = 0;
    m_iCurMBToDec = m_iFirstMB;
    m_iCurMBToRec = m_iFirstMB;
    m_iCurMBToDeb = m_iFirstMB;
    m_curTileDec = 0;
    m_curTileRec = 0;

    m_bInProcess = false;
    m_bDecVacant = 1;
    m_bRecVacant = 1;
    m_bDebVacant = 1;
    m_bError = false;

    // set conditional flags
    m_bDecoded = false;
    m_bPrevDeblocked = false;
    m_bDeblocked = getDeblockingFilterDisable();
    m_bSAOed = !(GetSliceHeader()->slice_sao_luma_flag || GetSliceHeader()->slice_sao_chroma_flag);

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

bool H265Slice::DecodeSliceHeader()
{
    UMC::Status umcRes = UMC::UMC_OK;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
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
    m_SliceHeader.slice_cb_qp_offset    = slice->slice_cb_qp_offset;
    m_SliceHeader.slice_cr_qp_offset    = slice->slice_cr_qp_offset;

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
    m_SliceHeader.slice_sao_luma_flag = slice->slice_sao_luma_flag;
    m_SliceHeader.slice_sao_chroma_flag = slice->slice_sao_chroma_flag;
    m_SliceHeader.m_CabacInitFlag        = slice->m_CabacInitFlag;
    m_SliceHeader.m_numEntryPointOffsets  = slice->m_numEntryPointOffsets;

    m_SliceHeader.m_MvdL1Zero = slice->m_MvdL1Zero;
    m_SliceHeader.m_LFCrossSliceBoundaryFlag = slice->m_LFCrossSliceBoundaryFlag;
    m_SliceHeader.m_enableTMVPFlag                = slice->m_enableTMVPFlag;
    m_SliceHeader.m_MaxNumMergeCand               = slice->m_MaxNumMergeCand;

    // Set the start of real slice, not slice segment
    m_SliceHeader.SliceCurStartCUAddr = slice->SliceCurStartCUAddr;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
