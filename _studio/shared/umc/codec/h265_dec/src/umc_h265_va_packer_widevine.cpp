//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_hevc_ddi.h"
#include "umc_h265_va_packer_dxva.h"

#include "umc_va_dxva2_protected.h"

using namespace UMC;

namespace UMC_HEVC_DECODER
{

    class PackerDXVA2_Widevine
        : public PackerDXVA2
    {

    public:

        PackerDXVA2_Widevine(VideoAccelerator * va)
            : PackerDXVA2(va)
        {}

    private:

        void PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier);
        void PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier);
        bool PackSliceParams(H265Slice*, Ipp32u& /*sliceNum*/, bool /*isLastSlice*/)
        { return true; }
        void PackQmatrix(const H265Slice* /*pSlice*/) {}
    };

    Packer* CreatePackerWidevine(UMC::VideoAccelerator* va)
    { return new PackerDXVA2_Widevine(va); }

    void PackerDXVA2_Widevine::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
    {
        if (!m_va || !frame || !supplier)
            return;

        H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
        if (!sliceInfo)
            return;

        int sliceCount = sliceInfo->GetSliceCount();
        H265Slice *pSlice = sliceInfo->GetSlice(0);
        if (!pSlice || !sliceCount)
            return;

        H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

        PackPicParams(pCurrentFrame, sliceInfo, supplier);

            Status s = m_va->Execute();
            if(s != UMC_OK)
                throw h265_exception(s);

        if (m_va->GetProtectedVA())
        {
            mfxBitstream * bs = m_va->GetProtectedVA()->GetBitstream();

            if (bs && bs->EncryptedData)
            {
                Ipp32s count = sliceInfo->GetSliceCount();
                m_va->GetProtectedVA()->MoveBSCurrentEncrypt(count);
            }
        }
    }

    void PackerDXVA2_Widevine::PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier)
    {
        UMCVACompBuffer *compBuf;
        DXVA_Intel_PicParams_HEVC *pPicParam = (DXVA_Intel_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
        compBuf->SetDataSize(sizeof(DXVA_Intel_PicParams_HEVC));

        memset(pPicParam, 0, sizeof(DXVA_Intel_PicParams_HEVC));

        H265Slice *pSlice = pSliceInfo ? pSliceInfo->GetSlice(0) : NULL;
        if (!pSlice)
            return;

        H265SliceHeader * sliceHeader = pSlice->GetSliceHeader();
        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();

        //
        //
        pPicParam->CurrPic.Index7bits   = pCurrentFrame->m_index;    // ?
        pPicParam->CurrPicOrderCntVal   = sliceHeader->slice_pic_order_cnt_lsb;

        int count = 0;
        int cntRefPicSetStCurrBefore = 0,
            cntRefPicSetStCurrAfter  = 0,
            cntRefPicSetLtCurr = 0;
        H265DBPList *dpb = supplier->GetDPBList();
        ReferencePictureSet *rps = pSlice->getRPS();
        for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]) ; frame = frame->future())
        {
            if(frame != pCurrentFrame)
            {
                int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

                if(refType != NO_REFERENCE)
                {
                    pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                    pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                    pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);

                    count++;
                }
            }
        }

        Ipp32u index;
        int pocList[3*8];
        int numRefPicSetStCurrBefore = 0,
            numRefPicSetStCurrAfter  = 0,
            numRefPicSetLtCurr       = 0;
        for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
                pocList[numRefPicSetStCurrBefore++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures(); index++)
        {
            Ipp32s poc = rps->getPOC(index);
            H265DecoderFrame *pFrm = supplier->GetDPBList()->findLongTermRefPic(pCurrentFrame, poc, pSeqParamSet->log2_max_pic_order_cnt_lsb, !rps->getCheckLTMSBPresent(index));

            if (pFrm)
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pFrm->PicOrderCnt();
            }
            else
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
            }
        }

        for(int n=0 ; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr ; n++)
        {
            if (!rps->getUsed(n))
                continue;

            for(int k=0;k < count;k++)
            {
                if(pocList[n] == pPicParam->PicOrderCntValList[k])
                {
                    if(n < numRefPicSetStCurrBefore)
                        pPicParam->RefPicSetStCurrBefore[cntRefPicSetStCurrBefore++] = (UCHAR)k;
                    else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                        pPicParam->RefPicSetStCurrAfter[cntRefPicSetStCurrAfter++] = (UCHAR)k;
                    else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                        pPicParam->RefPicSetLtCurr[cntRefPicSetLtCurr++] = (UCHAR)k;
                }
            }
        }

        for(int n=count;n < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]);n++)
        {
            pPicParam->RefFrameList[n].bPicEntry = (UCHAR)0xff;
            pPicParam->PicOrderCntValList[n] = -1;
        }

        for(int i=0;i < 8;i++)
        {
            if(i >= cntRefPicSetStCurrBefore)
                pPicParam->RefPicSetStCurrBefore[i] = 0xff;
            if(i >= cntRefPicSetStCurrAfter)
                pPicParam->RefPicSetStCurrAfter[i] = 0xff;
            if(i >= cntRefPicSetLtCurr)
                pPicParam->RefPicSetLtCurr[i] = 0xff;
        }

        pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
        pPicParam->TotalNumEntryPointOffsets = pSlice->m_WidevineStatusReportNumber;
    }
}

#endif //UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER