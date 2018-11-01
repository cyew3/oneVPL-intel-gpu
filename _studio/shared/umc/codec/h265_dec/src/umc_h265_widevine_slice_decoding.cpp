//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

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

    m_WidevineStatusReportNumber = 0;
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
int32_t H265WidevineSlice::RetrievePicParamSetNumber()
{
    m_SliceHeader = H265SliceHeader{};

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

} // int32_t H265WidevineSlice::RetrievePicParamSetNumber()

// Decode slice header and initializ slice structure with parsed values
bool H265WidevineSlice::Reset(PocDecoding * pocDecoding)
{
    // decode slice header
    if (false == DecodeSliceHeader(pocDecoding))
        return false;

    m_SliceHeader.m_SeqParamSet = m_pSeqParamSet;
    m_SliceHeader.m_PicParamSet = m_pPicParamSet;

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
        m_SliceHeader = H265SliceHeader{};

        m_SliceHeader.nal_unit_type = (NalUnitType)m_DecryptParams.ui8NalUnitType;
        m_SliceHeader.nuh_temporal_id = m_DecryptParams.ui8NuhTemporalId;

        umcRes = m_DecryptParams.GetSliceHeaderFull(this, m_pSeqParamSet, m_pPicParamSet);

        if (!GetSliceHeader()->dependent_slice_segment_flag)
        {
            if (!GetSliceHeader()->IdrPicFlag)
            {
                int32_t PicOrderCntMsb;
                int32_t slice_pic_order_cnt_lsb = m_SliceHeader.slice_pic_order_cnt_lsb;
                int32_t MaxPicOrderCntLsb = 1<< GetSeqParam()->log2_max_pic_order_cnt_lsb;
                int32_t prevPicOrderCntLsb = pocDecoding->prevPocPicOrderCntLsb;
                int32_t prevPicOrderCntMsb = pocDecoding->prevPicOrderCntMsb;

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

                H265SliceHeader * sliceHdr = &m_SliceHeader;

                {
                    ReferencePictureSet *rps = getRPS();
                    uint32_t offset = rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures();
                    for (uint32_t index = offset; index < offset + rps->getNumberOfLongtermPictures(); index++)
                    {
                        rps->m_POC[index] = sliceHdr->slice_pic_order_cnt_lsb + rps->m_DeltaPOC[index];
                    }
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

} // bool H265WidevineSlice::DecodeSliceHeader(PocDecoding * pocDecoding)

// Build reference lists from slice reference pic set. HEVC spec 8.3.2
UMC::Status H265WidevineSlice::UpdateReferenceList(H265DBPList *pDecoderFrameList)
{
    UMC::Status ps = UMC::UMC_OK;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList0 = NULL;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList1 = NULL;
    const H265DecoderRefPicList* pPicList0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0);
    const H265DecoderRefPicList* pPicList1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1);
    if(pPicList0 && pPicList1)
    {
        pRefPicList0 = pPicList0->m_refPicList;
        pRefPicList1 = pPicList1->m_refPicList;
    }
    else
    {
        return UMC::UMC_ERR_NULL_PTR;
    }

    H265SliceHeader* header = GetSliceHeader();
    VM_ASSERT(header);

    if (header->slice_type == I_SLICE)
    {
        for (int32_t number = 0; number < 3; number++)
            header->m_numRefIdx[number] = 0;

        return UMC::UMC_OK;
    }

    H265DecoderFrame *RefPicSetStCurr0[16] = {};
    H265DecoderFrame *RefPicSetStCurr1[16] = {};
    H265DecoderFrame *RefPicSetLtCurr[16] = {};
    uint32_t NumPicStCurr0 = 0;
    uint32_t NumPicStCurr1 = 0;
    uint32_t NumPicLtCurr = 0;
    uint32_t i;

    for(i = 0; i < getRPS()->getNumberOfNegativePictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            int32_t poc = header->slice_pic_order_cnt_lsb + getRPS()->getDeltaPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findShortRefPic(poc);
            m_pCurrentFrame->AddReferenceFrame(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(false);
            RefPicSetStCurr0[NumPicStCurr0] = pFrm;
            NumPicStCurr0++;
            if (!pFrm)
            {
                // Reporting about missed reference
                m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                // And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too
                m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
            }
        }
    }

    for(; i < getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            int32_t poc = header->slice_pic_order_cnt_lsb + getRPS()->getDeltaPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findShortRefPic(poc);
            m_pCurrentFrame->AddReferenceFrame(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(false);
            RefPicSetStCurr1[NumPicStCurr1] = pFrm;
            NumPicStCurr1++;
            if (!pFrm)
            {
                // Reporting about missed reference
                m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                // And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too
                m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
            }
        }
    }

    for(i = getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures();
        i < getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures() + getRPS()->getNumberOfLongtermPictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            int32_t poc = getRPS()->getPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findLongTermRefPic(m_pCurrentFrame, poc, GetSeqParam()->log2_max_pic_order_cnt_lsb, !getRPS()->getCheckLTMSBPresent(i));

            if (!pFrm)
                continue;

            m_pCurrentFrame->AddReferenceFrame(pFrm);

            pFrm->SetisLongTermRef(true);
            RefPicSetLtCurr[NumPicLtCurr] = pFrm;
            NumPicLtCurr++;
            if (!pFrm)
            {
                // Reporting about missed reference
                m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                // And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too
                m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
            }
        }
    }

    H265PicParamSet const* pps = GetPicParam();
    VM_ASSERT(pps);

    int32_t numPicTotalCurr = NumPicStCurr0 + NumPicStCurr1 + NumPicLtCurr;
    if (pps->pps_curr_pic_ref_enabled_flag)
        numPicTotalCurr++;

    // ref_pic_list_init
    H265DecoderFrame *refPicListTemp0[MAX_NUM_REF_PICS + 1] = {};
    H265DecoderFrame *refPicListTemp1[MAX_NUM_REF_PICS + 1] = {};

    if (header->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP &&
        header->nal_unit_type <= NAL_UT_CODED_SLICE_CRA &&
        numPicTotalCurr != pps->pps_curr_pic_ref_enabled_flag)
    {
        //8.3.2: If the current picture is a BLA or CRA picture, the value of NumPicTotalCurr shall be equal to pps_curr_pic_ref_enabled_flag ...
        m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
        return UMC::UMC_OK;
    }
    else if (!numPicTotalCurr)
    {
        //8.3.2: ... Otherwise, when the current picture contains a P or B slice, the value of NumPicTotalCurr shall not be equal to 0
        m_pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
        return UMC::UMC_OK;
    }

    int32_t cIdx = 0;
    for (i = 0; i < NumPicStCurr0; cIdx++, i++)
    {
        refPicListTemp0[cIdx] = RefPicSetStCurr0[i];
    }
    for (i = 0; i < NumPicStCurr1; cIdx++, i++)
    {
        refPicListTemp0[cIdx] = RefPicSetStCurr1[i];
    }
    for (i = 0; i < NumPicLtCurr; cIdx++, i++)
    {
        refPicListTemp0[cIdx] = RefPicSetLtCurr[i];
    }

    if (header->slice_type == B_SLICE)
    {
        cIdx = 0;
        for (i = 0; i < NumPicStCurr1; cIdx++, i++)
        {
            refPicListTemp1[cIdx] = RefPicSetStCurr1[i];
        }
        for (i = 0; i < NumPicStCurr0; cIdx++, i++)
        {
            refPicListTemp1[cIdx] = RefPicSetStCurr0[i];
        }
        for (i = 0; i < NumPicLtCurr; cIdx++, i++)
        {
            refPicListTemp1[cIdx] = RefPicSetLtCurr[i];
        }
    }

    for (cIdx = 0; cIdx < MAX_NUM_REF_PICS; cIdx ++)
    {
        pRefPicList0[cIdx].refFrame = refPicListTemp0[m_DecryptParams.RefFrames.ref_list_idx[0][cIdx]];
        pRefPicList0[cIdx].isLongReference = m_DecryptParams.RefFrames.ref_list_idx[0][cIdx] >= (NumPicStCurr0 + NumPicStCurr1);
    }

    if (header->slice_type == P_SLICE)
    {
        header->m_numRefIdx[1] = 0;
    }
    else
    {
        for (cIdx = 0; cIdx < MAX_NUM_REF_PICS; cIdx ++)
        {
            if (refPicListTemp1[m_DecryptParams.RefFrames.ref_list_idx[1][cIdx]] != NULL)
            {
                pRefPicList1[cIdx].refFrame = refPicListTemp1[m_DecryptParams.RefFrames.ref_list_idx[1][cIdx]];
                pRefPicList1[cIdx].isLongReference = m_DecryptParams.RefFrames.ref_list_idx[1][cIdx] >= (NumPicStCurr0 + NumPicStCurr1);
            }
        }
    }

    return ps;
} // Status H265WidevineSlice::UpdateRefPicList(H265DBPList *pDecoderFrameList)

} // namespace UMC_HEVC_DECODER

#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // UMC_ENABLE_H265_VIDEO_DECODER
