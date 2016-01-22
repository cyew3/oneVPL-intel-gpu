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

#include "umc_h265_widevine_slice_decoding.h"
#include "umc_h265_frame_list.h"

namespace UMC_HEVC_DECODER
{


H265WidevineSlice::H265WidevineSlice():
    H265Slice()
{
    Reset();
}


H265WidevineSlice::~H265WidevineSlice()
{
    Release();
}


// Initialize slice structure to default values
void H265WidevineSlice::Reset()
{
    H265Slice::Reset();

    DECRYPT_QUERY_STATUS_PARAMS_HEVC decryptParams;
    memset(&decryptParams, 0, sizeof(DECRYPT_QUERY_STATUS_PARAMS_HEVC));
    m_DecryptParams = decryptParams;
}


// Release resources
void H265WidevineSlice::Release()
{
    Reset();
}


void H265WidevineSlice::SetDecryptParameters(DecryptParametersWrapper* pDecryptParameters)
{
    m_DecryptParams = *pDecryptParameters;
}


// Parse beginning of slice header to get PPS ID
Ipp32s H265WidevineSlice::RetrievePicParamSetNumber()
{
    memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));

    UMC::Status umcRes = UMC::UMC_OK;

    try
    {
        m_SliceHeader.nal_unit_type = (NalUnitType)m_DecryptParams.ui8NalUnitType;
        m_SliceHeader.nuh_temporal_id = m_DecryptParams.ui8NuhTemporalId;

        // decode first part of slice header
        umcRes = m_DecryptParams.GetSliceHeaderPart1(&m_SliceHeader);
        if (UMC::UMC_OK != umcRes)
            return -1;
    } catch (...)
    {
        return -1;
    }

    return m_SliceHeader.slice_pic_parameter_set_id;

} // Ipp32s H265WidevineSlice::RetrievePicParamSetNumber()


// Decode slice header and initializ slice structure with parsed values
bool H265WidevineSlice::Reset(PocDecoding * pocDecoding)
{
    //m_BitStream.Reset((Ipp8u *) m_source.GetPointer(), (Ipp32u) m_source.GetDataSize());

    // decode slice header
    if (false == DecodeSliceHeader(pocDecoding))
        return false;

    //m_SliceHeader.m_HeaderBitstreamOffset = (Ipp32u)m_BitStream.BytesDecoded();

    m_SliceHeader.m_SeqParamSet = m_pSeqParamSet;
    m_SliceHeader.m_PicParamSet = m_pPicParamSet;

    //Ipp32s iMBInFrame = (GetSeqParam()->WidthInCU * GetSeqParam()->HeightInCU);

    // set slice variables
    //m_iFirstMB = m_SliceHeader.slice_segment_address;
    //m_iFirstMB = m_iFirstMB > iMBInFrame ? iMBInFrame : m_iFirstMB;
    //m_iFirstMB = m_pPicParamSet->m_CtbAddrRStoTS[m_iFirstMB];
    //m_iMaxMB = iMBInFrame;

    //processInfo.Initialize(m_iFirstMB, GetSeqParam()->WidthInCU);

    m_bError = false;

    // frame is not associated yet
    m_pCurrentFrame = NULL;
    return true;

} // H265WidevineSlice::Reset(PocDecoding * pocDecoding)


bool H265WidevineSlice::DecodeSliceHeader(PocDecoding * pocDecoding)
{
    UMC::Status umcRes = UMC::UMC_OK;
    // Locals for additional slice data to be read into, the data
    // was read and saved from the first slice header of the picture,
    // is not supposed to change within the picture, so can be
    // discarded when read again here.
    try
    {
        memset(&m_SliceHeader, 0, sizeof(m_SliceHeader));

        m_SliceHeader.nal_unit_type = (NalUnitType)m_DecryptParams.ui8NalUnitType;
        m_SliceHeader.nuh_temporal_id = m_DecryptParams.ui8NuhTemporalId;
        //umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
        //                                    m_SliceHeader.nuh_temporal_id);
        //if (UMC::UMC_OK != umcRes)
        //    return false;

        umcRes = m_DecryptParams.GetSliceHeaderFull(this, m_pSeqParamSet, m_pPicParamSet);

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

                //const H265SeqParamSet * sps = m_pSeqParamSet;
                H265SliceHeader * sliceHdr = &m_SliceHeader;

                ReferencePictureSet *rps = getRPS();
                Ipp32u offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();
                for(Ipp32s index = offset; index < offset + rps->getNumberOfLongtermPictures(); index++)
                {
                    rps->m_POC[index] = sliceHdr->slice_pic_order_cnt_lsb + rps->m_DeltaPOC[index];
                }

                //if (GetSeqParam()->long_term_ref_pics_present_flag)
                //{
                //    ReferencePictureSet *rps = getRPS();
                //    Ipp32u offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();

                //    Ipp32s prevDeltaMSB = 0;
                //    Ipp32s maxPicOrderCntLSB = 1 << sps->log2_max_pic_order_cnt_lsb;
                //    Ipp32s DeltaPocMsbCycleLt = 0;
                //    for(Ipp32u j = offset, k = 0; k < rps->getNumberOfLongtermPictures(); j++, k++)
                //    {
                //        int pocLsbLt = rps->poc_lbs_lt[j];
                //        if (rps->delta_poc_msb_present_flag[j])
                //        {
                //            bool deltaFlag = false;

                //            if( (j == offset) || (j == (offset + rps->num_long_term_sps)))
                //                deltaFlag = true;

                //            if(deltaFlag)
                //                DeltaPocMsbCycleLt = rps->delta_poc_msb_cycle_lt[j];
                //            else
                //                DeltaPocMsbCycleLt = rps->delta_poc_msb_cycle_lt[j] + prevDeltaMSB;

                //            Ipp32s pocLTCurr = sliceHdr->slice_pic_order_cnt_lsb - DeltaPocMsbCycleLt * maxPicOrderCntLSB - slice_pic_order_cnt_lsb + pocLsbLt;
                //            rps->setPOC(j, pocLTCurr);
                //            rps->setDeltaPOC(j, - sliceHdr->slice_pic_order_cnt_lsb + pocLTCurr);
                //        }
                //        else
                //        {
                //            rps->setPOC     (j, pocLsbLt);
                //            rps->setDeltaPOC(j, - sliceHdr->slice_pic_order_cnt_lsb + pocLsbLt);
                //            if (j == offset + rps->num_long_term_sps)
                //                DeltaPocMsbCycleLt = 0;
                //        }

                //        prevDeltaMSB = DeltaPocMsbCycleLt;
                //    }

                //    offset += rps->getNumberOfLongtermPictures();
                //    rps->num_pics = offset;
                //}

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

} // bool H265WidevineSlice::DecodeSliceHeader(PocDecoding * pocDecoding)

// Build reference lists from slice reference pic set. HEVC spec 8.3.2
UMC::Status H265WidevineSlice::UpdateReferenceList(H265DBPList *pDecoderFrameList)
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

    //RefPicListModification &refPicListModification = GetSliceHeader()->m_RefPicListModification;

    //for (cIdx = 0; cIdx <= GetSliceHeader()->m_numRefIdx[0] - 1; cIdx ++)
    //{
    //    bool isLong = refPicListModification.ref_pic_list_modification_flag_l0 ?
    //        (refPicListModification.list_entry_l0[cIdx] >= (NumPocStCurr0 + NumPocStCurr1))
    //        : ((Ipp32u)(cIdx % numPocTotalCurr) >= (NumPocStCurr0 + NumPocStCurr1));

    //    pRefPicList0[cIdx].refFrame = refPicListModification.ref_pic_list_modification_flag_l0 ? refPicListTemp0[refPicListModification.list_entry_l0[cIdx]] : refPicListTemp0[cIdx % numPocTotalCurr];
    //    pRefPicList0[cIdx].isLongReference = isLong;
    //}

    //if (GetSliceHeader()->slice_type == P_SLICE)
    //{
    //    GetSliceHeader()->m_numRefIdx[1] = 0;
    //}
    //else
    //{
    //    for (cIdx = 0; cIdx <= GetSliceHeader()->m_numRefIdx[1] - 1; cIdx ++)
    //    {
    //        bool isLong = refPicListModification.ref_pic_list_modification_flag_l1 ?
    //            (refPicListModification.list_entry_l1[cIdx] >= (NumPocStCurr0 + NumPocStCurr1))
    //            : ((Ipp32u)(cIdx % numPocTotalCurr) >= (NumPocStCurr0 + NumPocStCurr1));

    //        pRefPicList1[cIdx].refFrame = refPicListModification.ref_pic_list_modification_flag_l1 ? refPicListTemp1[refPicListModification.list_entry_l1[cIdx]] : refPicListTemp1[cIdx % numPocTotalCurr];
    //        pRefPicList1[cIdx].isLongReference = isLong;
    //    }
    //

    for (cIdx = 0; cIdx < MAX_NUM_REF_PICS; cIdx ++)
    {
        pRefPicList0[cIdx].refFrame = refPicListTemp0[m_DecryptParams.RefFrames.ref_list_idx[0][cIdx]];
        pRefPicList0[cIdx].isLongReference = m_DecryptParams.RefFrames.ref_list_idx[0][cIdx] >= (NumPocStCurr0 + NumPocStCurr1);
    }

    if (GetSliceHeader()->slice_type == P_SLICE)
    {
        GetSliceHeader()->m_numRefIdx[1] = 0;
    }
    else
    {
        for (cIdx = 0; cIdx < MAX_NUM_REF_PICS; cIdx ++)
        {
            pRefPicList1[cIdx].refFrame = refPicListTemp1[m_DecryptParams.RefFrames.ref_list_idx[1][cIdx]];
            pRefPicList1[cIdx].isLongReference = m_DecryptParams.RefFrames.ref_list_idx[1][cIdx] >= (NumPocStCurr0 + NumPocStCurr1);
        }
    }

    // Not sure about this code
/*
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
*/
    return ps;
} // Status H265WidevineSlice::UpdateRefPicList(H265DBPList *pDecoderFrameList)

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
