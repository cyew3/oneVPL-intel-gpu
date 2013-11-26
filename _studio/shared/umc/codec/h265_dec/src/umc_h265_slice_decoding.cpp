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
        if (m_pSeqParamSet)
            ((H265SeqParamSet*)m_pSeqParamSet)->DecrementReference();
        if (m_pPicParamSet)
            ((H265PicParamSet*)m_pPicParamSet)->DecrementReference();
        m_pSeqParamSet = 0;
        m_pPicParamSet = 0;
    }

    m_pCurrentFrame = 0;

    m_SliceHeader.nuh_temporal_id = 0;
    m_SliceHeader.m_CheckLDC = false;
    m_SliceHeader.slice_deblocking_filter_disabled_flag = false;
    m_SliceHeader.num_entry_point_offsets = 0;
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
        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.nuh_temporal_id);
        if (UMC::UMC_OK != umcRes)
            return false;

        // decode first part of slice header
        umcRes = m_BitStream.GetSliceHeaderPart1(&m_SliceHeader);
        if (UMC::UMC_OK != umcRes)
            return -1;
    } catch (...)
    {
        return -1;
    }

    return m_SliceHeader.slice_pic_parameter_set_id;
}

bool H265Slice::Reset(void *pSource, size_t nSourceSize, PocDecoding * pocDecoding)
{
    m_BitStream.Reset((Ipp8u *) pSource, (Ipp32u) nSourceSize);

    // decode slice header
    if (nSourceSize && false == DecodeSliceHeader(pocDecoding))
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

    for (int i = 0; i < LAST_PROCESS_ID; i++)
    {
        m_curMBToProcess[i] = m_iFirstMB;
        m_processVacant[i] = 1;
    }
    m_curTileDec = 0;
    m_curTileRec = 0;

    m_bInProcess = false;
    m_bError = false;

    // set conditional flags
    m_bDecoded = false;
    m_bPrevDeblocked = false;
    m_bDeblocked = GetSliceHeader()->slice_deblocking_filter_disabled_flag != 0;
    m_bSAOed = !(GetSliceHeader()->slice_sao_luma_flag || GetSliceHeader()->slice_sao_chroma_flag);

    if (m_bDeblocked)
    {
        m_processVacant[DEB_PROCESS_ID] = 0;
        m_curMBToProcess[DEB_PROCESS_ID] = m_iMaxMB;
    }

    if (m_bSAOed)
    {
        m_processVacant[SAO_PROCESS_ID] = 0;
        m_curMBToProcess[SAO_PROCESS_ID] = m_iMaxMB;
    }

    // frame is not associated yet
    m_pCurrentFrame = NULL;
    return true;

} // bool H265Slice::Reset(void *pSource, size_t nSourceSize, Ipp32s iNumber)

void H265Slice::SetSliceNumber(Ipp32s iSliceNumber)
{
    m_iNumber = iSliceNumber;

} // void H265Slice::SetSliceNumber(Ipp32s iSliceNumber)

bool H265Slice::DecodeSliceHeader(PocDecoding * pocDecoding)
{
    UMC::Status umcRes = UMC::UMC_OK;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
    try
    {
        memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));

        umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                                            m_SliceHeader.nuh_temporal_id);
        if (UMC::UMC_OK != umcRes)
            return false;

        umcRes = m_BitStream.GetSliceHeaderFull(this, m_pSeqParamSet, m_pPicParamSet);

        if (!GetSliceHeader()->dependent_slice_segment_flag)
        {
            if (!GetSliceHeader()->IdrPicFlag)
            {
                Ipp32s PicOrderCntMsb;
                Ipp32s slice_pic_order_cnt_lsb = m_SliceHeader.slice_pic_order_cnt_lsb;
                Ipp32s MaxPicOrderCntLsb = 1<< GetSeqParam()->log2_max_pic_order_cnt_lsb;
                Ipp32s prevPicOrderCntLsb = pocDecoding->prevPocPicOrderCntLsb;
                Ipp32s prevPicOrderCntMsb = pocDecoding->prevPicOrderCntMsb;

                if ( (slice_pic_order_cnt_lsb  <  prevPicOrderCntLsb) && ( (prevPicOrderCntLsb - slice_pic_order_cnt_lsb)  >=  (MaxPicOrderCntLsb / 2) ) )
                {
                    PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
                }
                else if ( (slice_pic_order_cnt_lsb  >  prevPicOrderCntLsb) && ( (slice_pic_order_cnt_lsb - prevPicOrderCntLsb)  >  (MaxPicOrderCntLsb / 2) ) )
                {
                    PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
                }
                else
                {
                    PicOrderCntMsb = prevPicOrderCntMsb;
                }

                if (m_SliceHeader.nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP || m_SliceHeader.nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL ||
                    m_SliceHeader.nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP)
                { // For BLA picture types, POCmsb is set to 0.
  
                    PicOrderCntMsb = 0;
                }

                m_SliceHeader.slice_pic_order_cnt_lsb = PicOrderCntMsb + slice_pic_order_cnt_lsb;

                ReferencePictureSet *rps = getRPS();

                const H265SeqParamSet * sps = m_pSeqParamSet;
                H265SliceHeader * sliceHdr = &m_SliceHeader;

                if (GetSeqParam()->long_term_ref_pics_present_flag)
                {
                    int offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();

                    Ipp32s prevDeltaMSB = 0;
                    Ipp32s maxPicOrderCntLSB = 1 << sps->log2_max_pic_order_cnt_lsb;
                    Ipp32s DeltaPocMsbCycleLt = 0;
                    for(Ipp32s j = offset + rps->getNumberOfLongtermPictures() - 1, k = 0; k < rps->getNumberOfLongtermPictures(); j--, k++)
                    {
                        int pocLsbLt = rps->poc_lbs_lt[j];
                        if (rps->delta_poc_msb_present_flag[j])
                        {
                            bool deltaFlag = false;
                       
                            if( (j == offset+rps->getNumberOfLongtermPictures() - 1) || (j == (offset + rps->num_long_term_pics - 1)))
                                deltaFlag = true;

                            if(deltaFlag)
                                DeltaPocMsbCycleLt = rps->delta_poc_msb_cycle_lt[j];
                            else
                                DeltaPocMsbCycleLt = rps->delta_poc_msb_cycle_lt[j] + prevDeltaMSB;

                            Ipp32s pocLTCurr = sliceHdr->slice_pic_order_cnt_lsb - DeltaPocMsbCycleLt * maxPicOrderCntLSB - slice_pic_order_cnt_lsb + pocLsbLt;
                            rps->setPOC(j, pocLTCurr);
                            rps->setDeltaPOC(j, - sliceHdr->slice_pic_order_cnt_lsb + pocLTCurr);
                        }
                        else
                        {
                            rps->setPOC     (j, pocLsbLt);
                            rps->setDeltaPOC(j, - sliceHdr->slice_pic_order_cnt_lsb + pocLsbLt);
                            if (j == offset + rps->num_long_term_pics - 1)
                                DeltaPocMsbCycleLt = 0;
                        }

                        prevDeltaMSB = DeltaPocMsbCycleLt;
                    }

                    offset += rps->getNumberOfLongtermPictures();
                    rps->num_pics = offset;
                }

                if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
                    || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
                    || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP )
                {
                    rps = getLocalRPS();
                    rps->num_negative_pics = 0;
                    rps->num_positive_pics = 0;
                    rps->setNumberOfLongtermPictures(0);
                    rps->num_pics = 0;
                    setRPS(rps);
                }

                if (GetSliceHeader()->nuh_temporal_id == 0)
                {
                    pocDecoding->prevPicOrderCntMsb = PicOrderCntMsb;
                    pocDecoding->prevPocPicOrderCntLsb = slice_pic_order_cnt_lsb;
                }
            }
            else
            {
                if (GetSliceHeader()->nuh_temporal_id == 0)
                {
                    pocDecoding->prevPicOrderCntMsb = 0;
                    pocDecoding->prevPocPicOrderCntLsb = 0;
                }
            }
        }

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
        }
    }
}

void H265Slice::InitializeContexts()
{
    SliceType slice_type = m_SliceHeader.slice_type;

    if (m_pPicParamSet->cabac_init_present_flag && m_SliceHeader.cabac_init_flag)
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

  if (GetSliceHeader()->slice_type != I_SLICE)
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

void H265Slice::allocSubstreamSizes(unsigned uiNumSubstreams)
{
    if (NULL != m_SliceHeader.m_SubstreamSizes)
        delete[] m_SliceHeader.m_SubstreamSizes;
    m_SliceHeader.m_SubstreamSizes = new unsigned[uiNumSubstreams > 0 ? uiNumSubstreams - 1 : 0];
}

void H265Slice::CopyFromBaseSlice(const H265Slice * s)
{
    if (!s || !m_SliceHeader.dependent_slice_segment_flag)
        return;

    VM_ASSERT(s);
    m_iNumber = s->m_iNumber;

    const H265SliceHeader * slice = s->GetSliceHeader();

    m_SliceHeader.IdrPicFlag = slice->IdrPicFlag;
    m_SliceHeader.slice_pic_order_cnt_lsb = slice->slice_pic_order_cnt_lsb;
    m_SliceHeader.nal_unit_type = slice->nal_unit_type;
    m_SliceHeader.SliceQP = slice->SliceQP;

    m_SliceHeader.slice_deblocking_filter_disabled_flag   = slice->slice_deblocking_filter_disabled_flag;
    m_SliceHeader.deblocking_filter_override_flag = slice->deblocking_filter_override_flag;
    m_SliceHeader.slice_beta_offset = slice->slice_beta_offset;
    m_SliceHeader.slice_tc_offset = slice->slice_tc_offset;

    for (Ipp32s i = 0; i < 3; i++)
    {
        m_SliceHeader.m_numRefIdx[i]     = slice->m_numRefIdx[i];
    }

    m_SliceHeader.m_CheckLDC            = slice->m_CheckLDC;
    m_SliceHeader.slice_type            = slice->slice_type;
    m_SliceHeader.slice_qp_delta        = slice->slice_qp_delta;
    m_SliceHeader.slice_cb_qp_offset    = slice->slice_cb_qp_offset;
    m_SliceHeader.slice_cr_qp_offset    = slice->slice_cr_qp_offset;

    for (Ipp32s i = 0; i < 2; i++)
    {
        for (Ipp32s j = 0; j < MAX_NUM_REF_PICS; j++)
        {
            m_SliceHeader.RefPOCList[i][j] = slice->RefPOCList[i][j];
        }
    }


    m_SliceHeader.m_pRPS                    = slice->m_pRPS;
    m_SliceHeader.collocated_from_l0_flag   = slice->collocated_from_l0_flag;
    m_SliceHeader.collocated_ref_idx        = slice->collocated_ref_idx;
    m_SliceHeader.nuh_temporal_id           = slice->nuh_temporal_id;

    for ( Ipp32s e = 0; e < 2; e++ )
    {
        for ( Ipp32s n = 0; n < MAX_NUM_REF_PICS; n++ )
        {
            MFX_INTERNAL_CPY(m_SliceHeader.pred_weight_table[e][n], slice->pred_weight_table[e][n], sizeof(wpScalingParam)*3);
        }
    }
    m_SliceHeader.slice_sao_luma_flag = slice->slice_sao_luma_flag;
    m_SliceHeader.slice_sao_chroma_flag = slice->slice_sao_chroma_flag;
    m_SliceHeader.cabac_init_flag        = slice->cabac_init_flag;
    m_SliceHeader.num_entry_point_offsets = slice->num_entry_point_offsets;

    m_SliceHeader.mvd_l1_zero_flag = slice->mvd_l1_zero_flag;
    m_SliceHeader.slice_loop_filter_across_slices_enabled_flag  = slice->slice_loop_filter_across_slices_enabled_flag;
    m_SliceHeader.slice_enable_temporal_mvp_flag                = slice->slice_enable_temporal_mvp_flag;
    m_SliceHeader.max_num_merge_cand               = slice->max_num_merge_cand;

    // Set the start of real slice, not slice segment
    m_SliceHeader.SliceCurStartCUAddr = slice->SliceCurStartCUAddr;

    m_SliceHeader.m_RefPicListModification = slice->m_RefPicListModification;

    m_bDeblocked = GetSliceHeader()->slice_deblocking_filter_disabled_flag != 0;
    m_bSAOed = !(GetSliceHeader()->slice_sao_luma_flag || GetSliceHeader()->slice_sao_chroma_flag);

    if (m_bDeblocked)
    {
        m_processVacant[DEB_PROCESS_ID] = 0;
        m_curMBToProcess[DEB_PROCESS_ID] = m_iMaxMB;
    }

    if (m_bSAOed)
    {
        m_processVacant[SAO_PROCESS_ID] = 0;
        m_curMBToProcess[SAO_PROCESS_ID] = m_iMaxMB;
    }
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
