/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_slice_decoding.h"
#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_heap.h"
#include "umc_h265_frame_info.h"
#include "umc_h265_frame_list.h"

namespace UMC_HEVC_DECODER
{
H265Slice::H265Slice()
    : m_pSeqParamSet(0)
    , m_context(0)
    , m_tileByteLocation (0)
{
    Reset();
} // H265Slice::H265Slice()

H265Slice::~H265Slice()
{
    Release();

} // H265Slice::~H265Slice(void)

// Initialize slice structure to default values
void H265Slice::Reset()
{
    m_source.Release();

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

    m_tileCount = 0;
    delete[] m_tileByteLocation;
    m_tileByteLocation = 0;
}

// Release resources
void H265Slice::Release()
{
    Reset();
} // void H265Slice::Release(void)

// Parse beginning of slice header to get PPS ID
Ipp32s H265Slice::RetrievePicParamSetNumber()
{
    if (!m_source.GetDataSize())
        return -1;

    memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));
    m_BitStream.Reset((Ipp8u *) m_source.GetPointer(), (Ipp32u) m_source.GetDataSize());

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

// Decode slice header and initializ slice structure with parsed values
bool H265Slice::Reset(PocDecoding * pocDecoding)
{
    m_BitStream.Reset((Ipp8u *) m_source.GetPointer(), (Ipp32u) m_source.GetDataSize());

    // decode slice header
    if (m_source.GetDataSize() && false == DecodeSliceHeader(pocDecoding))
        return false;

    m_SliceHeader.m_HeaderBitstreamOffset = (Ipp32u)m_BitStream.BytesDecoded();

    m_SliceHeader.m_SeqParamSet = m_pSeqParamSet;
    m_SliceHeader.m_PicParamSet = m_pPicParamSet;

    Ipp32s iMBInFrame = (GetSeqParam()->WidthInCU * GetSeqParam()->HeightInCU);

    // set slice variables
    m_iFirstMB = m_SliceHeader.slice_segment_address;
    m_iFirstMB = m_iFirstMB > iMBInFrame ? iMBInFrame : m_iFirstMB;
    m_iFirstMB = m_pPicParamSet->m_CtbAddrRStoTS[m_iFirstMB];
    m_iMaxMB = iMBInFrame;

    processInfo.Initialize(m_iFirstMB, GetSeqParam()->WidthInCU);

    m_bError = false;

    // frame is not associated yet
    m_pCurrentFrame = NULL;
    return true;

} // bool H265Slice::Reset(void *pSource, size_t nSourceSize, Ipp32s iNumber)

// Set current slice number
void H265Slice::SetSliceNumber(Ipp32s iSliceNumber)
{
    m_iNumber = iSliceNumber;

} // void H265Slice::SetSliceNumber(Ipp32s iSliceNumber)

// Decoder slice header and calculate POC
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

                const H265SeqParamSet * sps = m_pSeqParamSet;
                H265SliceHeader * sliceHdr = &m_SliceHeader;

                if (GetSeqParam()->long_term_ref_pics_present_flag)
                {
                    ReferencePictureSet *rps = getRPS();
                    Ipp32u offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();

                    Ipp32s prevDeltaMSB = 0;
                    Ipp32s maxPicOrderCntLSB = 1 << sps->log2_max_pic_order_cnt_lsb;
                    Ipp32s DeltaPocMsbCycleLt = 0;
                    for(Ipp32u j = offset, k = 0; k < rps->getNumberOfLongtermPictures(); j++, k++)
                    {
                        int pocLsbLt = rps->poc_lbs_lt[j];
                        if (rps->delta_poc_msb_present_flag[j])
                        {
                            bool deltaFlag = false;

                            if( (j == offset) || (j == (offset + rps->num_long_term_sps)))
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
                            if (j == offset + rps->num_long_term_sps)
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
                    ReferencePictureSet *rps = getRPS();
                    rps->num_negative_pics = 0;
                    rps->num_positive_pics = 0;
                    rps->setNumberOfLongtermPictures(0);
                    rps->num_pics = 0;
                }

                if (GetSliceHeader()->nuh_temporal_id == 0 && sliceHdr->nal_unit_type != NAL_UT_CODED_SLICE_RADL_R &&
                    sliceHdr->nal_unit_type != NAL_UT_CODED_SLICE_RASL_R && !IsSubLayerNonReference(sliceHdr->nal_unit_type))
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

// Initialize CABAC context depending on slice type
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

// Returns number of used references in RPS
int H265Slice::getNumRpsCurrTempList() const
{
  int numRpsCurrTempList = 0;

  if (GetSliceHeader()->slice_type != I_SLICE)
  {
      const ReferencePictureSet *rps = getRPS();

      for(Ipp32u i=0;i < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures();i++)
      {
        if(rps->getUsed(i))
        {
          numRpsCurrTempList++;
        }
      }
  }

  return numRpsCurrTempList;
}

// For dependent slice copy data from another slice
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

    m_SliceHeader.m_rps                     = slice->m_rps;
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

    m_SliceHeader.luma_log2_weight_denom = slice->luma_log2_weight_denom;
    m_SliceHeader.chroma_log2_weight_denom = slice->chroma_log2_weight_denom;
    m_SliceHeader.slice_sao_luma_flag = slice->slice_sao_luma_flag;
    m_SliceHeader.slice_sao_chroma_flag = slice->slice_sao_chroma_flag;
    m_SliceHeader.cabac_init_flag        = slice->cabac_init_flag;
    m_SliceHeader.num_entry_point_offsets = slice->num_entry_point_offsets;

    m_SliceHeader.mvd_l1_zero_flag = slice->mvd_l1_zero_flag;
    m_SliceHeader.slice_loop_filter_across_slices_enabled_flag  = slice->slice_loop_filter_across_slices_enabled_flag;
    m_SliceHeader.slice_temporal_mvp_enabled_flag                = slice->slice_temporal_mvp_enabled_flag;
    m_SliceHeader.max_num_merge_cand               = slice->max_num_merge_cand;

    // Set the start of real slice, not slice segment
    m_SliceHeader.SliceCurStartCUAddr = slice->SliceCurStartCUAddr;

    m_SliceHeader.m_RefPicListModification = slice->m_RefPicListModification;
}

// Build reference lists from slice reference pic set. HEVC spec 8.3.2
UMC::Status H265Slice::UpdateReferenceList(H265DBPList *pDecoderFrameList)
{
    UMC::Status ps = UMC::UMC_OK;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->m_refPicList;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->m_refPicList;

    if (GetSliceHeader()->slice_type == I_SLICE)
    {
        for (Ipp32s number = 0; number < 3; number++)
            GetSliceHeader()->m_numRefIdx[number] = 0;

        return UMC::UMC_OK;
    }

    H265DecoderFrame *RefPicSetStCurr0[16];
    H265DecoderFrame *RefPicSetStCurr1[16];
    H265DecoderFrame *RefPicSetLtCurr[16];
    Ipp32u NumPocStCurr0 = 0;
    Ipp32u NumPocStCurr1 = 0;
    Ipp32u NumPocLtCurr = 0;
    Ipp32u i;

    for(i = 0; i < getRPS()->getNumberOfNegativePictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            Ipp32s poc = GetSliceHeader()->slice_pic_order_cnt_lsb + getRPS()->getDeltaPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findShortRefPic(poc);
            m_pCurrentFrame->AddReferenceFrame(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(false);
            RefPicSetStCurr0[NumPocStCurr0] = pFrm;
            NumPocStCurr0++;
            // pcRefPic->setCheckLTMSBPresent(false);
        }
    }

    for(; i < getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            Ipp32s poc = GetSliceHeader()->slice_pic_order_cnt_lsb + getRPS()->getDeltaPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findShortRefPic(poc);
            m_pCurrentFrame->AddReferenceFrame(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(false);
            RefPicSetStCurr1[NumPocStCurr1] = pFrm;
            NumPocStCurr1++;
            // pcRefPic->setCheckLTMSBPresent(false);
        }
    }

    for(Ipp32u i = getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures();
        i < getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures() + getRPS()->getNumberOfLongtermPictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            Ipp32s poc = getRPS()->getPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findLongTermRefPic(m_pCurrentFrame, poc, GetSeqParam()->log2_max_pic_order_cnt_lsb, !getRPS()->getCheckLTMSBPresent(i));
            m_pCurrentFrame->AddReferenceFrame(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(true);
            RefPicSetLtCurr[NumPocLtCurr] = pFrm;
            NumPocLtCurr++;
        }
        // pFrm->setCheckLTMSBPresent(getRPS()->getCheckLTMSBPresent(i));
    }

    // ref_pic_list_init
    H265DecoderFrame *refPicListTemp0[MAX_NUM_REF_PICS + 1];
    H265DecoderFrame *refPicListTemp1[MAX_NUM_REF_PICS + 1];
    Ipp32s numPocTotalCurr = NumPocStCurr0 + NumPocStCurr1 + NumPocLtCurr;

    if (!numPocTotalCurr) // this is error
    {
        m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_DPB);
        return UMC::UMC_OK;
    }

    Ipp32s cIdx = 0;
    for (Ipp32u i = 0; i < NumPocStCurr0; cIdx++, i++)
    {
        refPicListTemp0[cIdx] = RefPicSetStCurr0[i];
    }
    for (Ipp32u i = 0; i < NumPocStCurr1; cIdx++, i++)
    {
        refPicListTemp0[cIdx] = RefPicSetStCurr1[i];
    }
    for (Ipp32u i = 0; i < NumPocLtCurr; cIdx++, i++)
    {
        refPicListTemp0[cIdx] = RefPicSetLtCurr[i];
    }

    if (GetSliceHeader()->slice_type == B_SLICE)
    {
        cIdx = 0;
        for (Ipp32u i = 0; i < NumPocStCurr1; cIdx++, i++)
        {
            refPicListTemp1[cIdx] = RefPicSetStCurr1[i];
        }
        for (Ipp32u i = 0; i < NumPocStCurr0; cIdx++, i++)
        {
            refPicListTemp1[cIdx] = RefPicSetStCurr0[i];
        }
        for (Ipp32u i = 0; i < NumPocLtCurr; cIdx++, i++)
        {
            refPicListTemp1[cIdx] = RefPicSetLtCurr[i];
        }
    }

    RefPicListModification &refPicListModification = GetSliceHeader()->m_RefPicListModification;

    for (cIdx = 0; cIdx <= GetSliceHeader()->m_numRefIdx[0] - 1; cIdx ++)
    {
        bool isLong = refPicListModification.ref_pic_list_modification_flag_l0 ?
            (refPicListModification.list_entry_l0[cIdx] >= (NumPocStCurr0 + NumPocStCurr1))
            : ((Ipp32u)(cIdx % numPocTotalCurr) >= (NumPocStCurr0 + NumPocStCurr1));

        pRefPicList0[cIdx].refFrame = refPicListModification.ref_pic_list_modification_flag_l0 ? refPicListTemp0[refPicListModification.list_entry_l0[cIdx]] : refPicListTemp0[cIdx % numPocTotalCurr];
        pRefPicList0[cIdx].isLongReference = isLong;
    }

    if (GetSliceHeader()->slice_type == P_SLICE)
    {
        GetSliceHeader()->m_numRefIdx[1] = 0;
    }
    else
    {
        for (cIdx = 0; cIdx <= GetSliceHeader()->m_numRefIdx[1] - 1; cIdx ++)
        {
            bool isLong = refPicListModification.ref_pic_list_modification_flag_l1 ?
                (refPicListModification.list_entry_l1[cIdx] >= (NumPocStCurr0 + NumPocStCurr1))
                : ((Ipp32u)(cIdx % numPocTotalCurr) >= (NumPocStCurr0 + NumPocStCurr1));

            pRefPicList1[cIdx].refFrame = refPicListModification.ref_pic_list_modification_flag_l1 ? refPicListTemp1[refPicListModification.list_entry_l1[cIdx]] : refPicListTemp1[cIdx % numPocTotalCurr];
            pRefPicList1[cIdx].isLongReference = isLong;
        }
    }

    if (GetSliceHeader()->slice_type == B_SLICE && getNumRefIdx(REF_PIC_LIST_1) == 0)
    {
        Ipp32s iNumRefIdx = getNumRefIdx(REF_PIC_LIST_0);
        GetSliceHeader()->m_numRefIdx[REF_PIC_LIST_1] = iNumRefIdx;

        for (Ipp32s iRefIdx = 0; iRefIdx < iNumRefIdx; iRefIdx++)
        {
            pRefPicList1[iRefIdx] = pRefPicList0[iRefIdx];
        }
    }

    if (GetSliceHeader()->slice_type != I_SLICE)
    {
        bool bLowDelay = true;
        Ipp32s currPOC = GetSliceHeader()->slice_pic_order_cnt_lsb;

        H265DecoderFrame *missedReference = 0;

        for (Ipp32s i = 0; !missedReference && i < numPocTotalCurr; i++)
        {
            missedReference = refPicListTemp0[i];
        }

        for (Ipp32s k = 0; k < getNumRefIdx(REF_PIC_LIST_0) && bLowDelay; k++)
        {
            if (!pRefPicList0[k].refFrame)
            {
                pRefPicList0[k].refFrame = missedReference;
            }
        }

        for (Ipp32s k = 0; k < getNumRefIdx(REF_PIC_LIST_0) && bLowDelay; k++)
        {
            if (pRefPicList0[k].refFrame && pRefPicList0[k].refFrame->PicOrderCnt() > currPOC)
            {
                bLowDelay = false;
            }
        }

        if (GetSliceHeader()->slice_type == B_SLICE)
        {
            for (Ipp32s k = 0; k < getNumRefIdx(REF_PIC_LIST_1); k++)
            {
                if (!pRefPicList1[k].refFrame)
                {
                    pRefPicList1[k].refFrame = missedReference;
                }
            }

            for (Ipp32s k = 0; k < getNumRefIdx(REF_PIC_LIST_1) && bLowDelay; k++)
            {
                if (pRefPicList1[k].refFrame && pRefPicList1[k].refFrame->PicOrderCnt() > currPOC)
                {
                    bLowDelay = false;
                }
            }
        }

        m_SliceHeader.m_CheckLDC = bLowDelay;
    }

    return ps;
} // Status H265Slice::UpdateRefPicList(H265DBPList *pDecoderFrameList)

// RPS data structure constructor
ReferencePictureSet::ReferencePictureSet()
{
  ::memset(this, 0, sizeof(*this));
}

// Bubble sort RPS by delta POCs placing negative values first, positive values second, increasing POS deltas
// Negative values are stored with biggest absolute value first. See HEVC spec 8.3.2.
void ReferencePictureSet::sortDeltaPOC()
{
    for (Ipp32s j = 1; j < num_pics; j++)
    {
        Ipp32s deltaPOC = m_DeltaPOC[j];
        Ipp8u Used = used_by_curr_pic_flag[j];
        for (Ipp32s k = j - 1; k >= 0; k--)
        {
            Ipp32s temp = m_DeltaPOC[k];
            if (deltaPOC < temp)
            {
                m_DeltaPOC[k + 1] = temp;
                used_by_curr_pic_flag[k + 1] = used_by_curr_pic_flag[k];
                m_DeltaPOC[k] = deltaPOC;
                used_by_curr_pic_flag[k] = Used;
            }
        }
    }
    Ipp32s NumNegPics = (Ipp32s) num_negative_pics;
    for (Ipp32s j = 0, k = NumNegPics - 1; j < NumNegPics >> 1; j++, k--)
    {
        Ipp32s deltaPOC = m_DeltaPOC[j];
        Ipp8u Used = used_by_curr_pic_flag[j];
        m_DeltaPOC[j] = m_DeltaPOC[k];
        used_by_curr_pic_flag[j] = used_by_curr_pic_flag[k];
        m_DeltaPOC[k] = deltaPOC;
        used_by_curr_pic_flag[k] = Used;
    }
}

// RPS list data structure constructor
ReferencePictureSetList::ReferencePictureSetList()
    : m_NumberOfReferencePictureSets(0)
{
}

// Allocate RPS list data structure with a new number of RPS
void ReferencePictureSetList::allocate(unsigned NumberOfReferencePictureSets)
{
    if (m_NumberOfReferencePictureSets == NumberOfReferencePictureSets)
        return;

    m_NumberOfReferencePictureSets = NumberOfReferencePictureSets;
    referencePictureSet.resize(NumberOfReferencePictureSets);
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
