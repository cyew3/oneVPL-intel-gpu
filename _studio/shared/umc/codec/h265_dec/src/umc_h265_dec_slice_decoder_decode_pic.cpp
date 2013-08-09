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

/*  DEBUG: this file is requred to changing name
    to umc_h264_dec_slice_decode_pic */

#include "umc_h265_slice_decoding.h"
#include "umc_h265_dec.h"
#include "vm_debug.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_task_supplier.h"

namespace UMC_HEVC_DECODER
{

UMC::Status H265Slice::UpdateReferenceList(H265DBPList *pDecoderFrameList)
{
    UMC::Status ps = UMC::UMC_OK;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->m_refPicList;
    H265DecoderRefPicList::ReferenceInformation* pRefPicList1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->m_refPicList;

    if (getSliceType() == I_SLICE)
    {
        m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->Reset();
        m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->Reset();

        for (Ipp32s number = 0; number < 3; number++)
            m_SliceHeader.m_numRefIdx[number] = 0;

        return UMC::UMC_OK;
    }

    H265DecoderFrame *pFrm = NULL;
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
            Ipp32s poc = getPOC() + getRPS()->getDeltaPOC(i);

            pFrm = pDecoderFrameList->findShortRefPic(poc);
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
            Ipp32s poc = getPOC() + getRPS()->getDeltaPOC(i);

            pFrm = pDecoderFrameList->findShortRefPic(poc);
            m_pCurrentFrame->AddReference(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(false);
            RefPicSetStCurr1[NumPocStCurr1] = pFrm;
            NumPocStCurr1++;
            // pcRefPic->setCheckLTMSBPresent(false);
        }
    }

    for(Ipp32s i = getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures() + getRPS()->getNumberOfLongtermPictures() - 1;
        i > getRPS()->getNumberOfNegativePictures() + getRPS()->getNumberOfPositivePictures() - 1 ; i--)
    {
        if(getRPS()->getUsed(i))
        {
            Ipp32s poc = getRPS()->getPOC(i);

            pFrm = pDecoderFrameList->findLongTermRefPic(m_pCurrentFrame, poc, getSPS()->log2_max_pic_order_cnt_lsb, !getRPS()->getCheckLTMSBPresent(i));
            m_pCurrentFrame->AddReference(pFrm);

            if (pFrm)
                pFrm->SetisLongTermRef(true);
            RefPicSetLtCurr[NumPocLtCurr] = pFrm;
            NumPocLtCurr++;
        }
        // pFrm->setCheckLTMSBPresent(getRPS()->getCheckLTMSBPresent(i));
    }

    // ref_pic_list_init
    Ipp32s cIdx = 0;
    H265DecoderFrame *refPicListTemp0[MAX_NUM_REF_PICS + 1];
    H265DecoderFrame *refPicListTemp1[MAX_NUM_REF_PICS + 1];
    Ipp32s numPocTotalCurr = NumPocStCurr0 + NumPocStCurr1 + NumPocLtCurr;

    cIdx = 0;
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

    if (getSliceType() == B_SLICE)
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

    for (cIdx = 0; cIdx <= m_SliceHeader.m_numRefIdx[0] - 1; cIdx ++)
    {
        bool isLong = getRefPicListModification()->getRefPicListModificationFlagL0() ?
            (getRefPicListModification()->getRefPicSetIdxL0(cIdx) >= (NumPocStCurr0 + NumPocStCurr1))
            : ((Ipp32u)(cIdx % numPocTotalCurr) >= (NumPocStCurr0 + NumPocStCurr1));

        pRefPicList0[cIdx].refFrame = getRefPicListModification()->getRefPicListModificationFlagL0() ? refPicListTemp0[getRefPicListModification()->getRefPicSetIdxL0(cIdx)] : refPicListTemp0[cIdx % numPocTotalCurr];

        pRefPicList0[cIdx].isLongReference = isLong;
    }

    if (getSliceType() == P_SLICE)
    {
        m_SliceHeader.m_numRefIdx[1] = 0;
        m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->Reset();
    }
    else
    {
        for (cIdx = 0; cIdx <= m_SliceHeader.m_numRefIdx[1] - 1; cIdx ++)
        {
            bool isLong = getRefPicListModification()->getRefPicListModificationFlagL1() ?
                (getRefPicListModification()->getRefPicSetIdxL1(cIdx) >= (NumPocStCurr0 + NumPocStCurr1))
                : ((Ipp32u)(cIdx % numPocTotalCurr) >= (NumPocStCurr0 + NumPocStCurr1));

            pRefPicList1[cIdx].refFrame = getRefPicListModification()->getRefPicListModificationFlagL1() ? refPicListTemp1[getRefPicListModification()->getRefPicSetIdxL1(cIdx)] : refPicListTemp1[cIdx % numPocTotalCurr];
            pRefPicList1[cIdx].isLongReference = isLong;
        }
    }

    // For generalized B
    // note: maybe not existed case (always L0 is copied to L1 if L1 is empty)
    if (isInterB() && getNumRefIdx(REF_PIC_LIST_1) == 0)
    {
        Ipp32s iNumRefIdx = getNumRefIdx(REF_PIC_LIST_0);
        setNumRefIdx(REF_PIC_LIST_1, iNumRefIdx);

        for (Ipp32s iRefIdx = 0; iRefIdx < iNumRefIdx; iRefIdx++)
        {
            pRefPicList1[iRefIdx] = pRefPicList0[iRefIdx];
        }
    }

    if (!isIntra())
    {
        bool bLowDelay = true;
        Ipp32s iCurrPOC = getPOC();
        Ipp32s iRefIdx = 0;

        for (iRefIdx = 0; iRefIdx < getNumRefIdx(REF_PIC_LIST_0) && bLowDelay; iRefIdx++)
        {
            if (pRefPicList0[iRefIdx].refFrame->PicOrderCnt() > iCurrPOC)
            {
                bLowDelay = false;
            }
        }

        if (isInterB())
        {
            for (iRefIdx = 0; iRefIdx < getNumRefIdx(REF_PIC_LIST_1) && bLowDelay; iRefIdx++)
            {
                if (pRefPicList1[iRefIdx].refFrame->PicOrderCnt() > iCurrPOC)
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
