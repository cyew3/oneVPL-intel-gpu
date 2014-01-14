/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

/*  DEBUG: this file is requred to changing name
    to umc_h264_dec_slice_decode_pic */

#include "umc_h265_slice_decoding.h"
#include "umc_h265_frame_list.h"
#include "umc_h265_task_supplier.h"

namespace UMC_HEVC_DECODER
{

UMC::Status H265Slice::UpdateReferenceList(H265DBPList *pDecoderFrameList)
{
    UMC::Status ps = UMC::UMC_OK;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->m_refPicList;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->m_refPicList;

    if (GetSliceHeader()->slice_type == I_SLICE)
    {
        m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->Reset();
        m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->Reset();

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
    Ipp32s i;

    for(i = 0; i < getRPS()->getNumberOfNegativePictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            Ipp32s poc = GetSliceHeader()->slice_pic_order_cnt_lsb + getRPS()->getDeltaPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findShortRefPic(poc);
            m_pCurrentFrame->AddReference(pFrm);

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
            m_pCurrentFrame->AddReference(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(false);
            RefPicSetStCurr1[NumPocStCurr1] = pFrm;
            NumPocStCurr1++;
            // pcRefPic->setCheckLTMSBPresent(false);
        }
    }

    for(Ipp32s i = getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures();
        i < getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures() + getRPS()->getNumberOfLongtermPictures(); i++)
    {
        if(getRPS()->getUsed(i))
        {
            Ipp32s poc = getRPS()->getPOC(i);

            H265DecoderFrame *pFrm = pDecoderFrameList->findLongTermRefPic(m_pCurrentFrame, poc, GetSeqParam()->log2_max_pic_order_cnt_lsb, !getRPS()->getCheckLTMSBPresent(i));
            m_pCurrentFrame->AddReference(pFrm);

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
        m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->Reset();
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
} // Status H265Slice::UpdateRefPicList(H265DecoderFrameList *pDecoderFrameList)

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
