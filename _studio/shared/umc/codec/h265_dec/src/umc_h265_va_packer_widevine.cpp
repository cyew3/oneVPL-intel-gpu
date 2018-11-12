// Copyright (c) 2013-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_va_base.h"
#if defined (UMC_VA_LINUX) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#include "umc_h265_va_packer.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_widevine_slice_decoding.h"

#include "huc_based_drm_common.h"

using namespace UMC;

namespace UMC_HEVC_DECODER
{

    class PackerVA_Widevine
        : public PackerVA
    {

    public:

        PackerVA_Widevine(VideoAccelerator * va)
            : PackerVA(va)
        {}

    private:

        void PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier) override;
        void PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier) override;
    };

    Packer* CreatePackerWidevine(UMC::VideoAccelerator* va)
    { return new PackerVA_Widevine(va); }

    void PackerVA_Widevine::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
    {
        if(!frame) { throw h265_exception(UMC_ERR_NULL_PTR); }
        H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
        if(!sliceInfo) { throw h265_exception(UMC_ERR_NULL_PTR); }
        int sliceCount = sliceInfo->GetSliceCount();

        if (!sliceCount)
            return;

        H265Slice *pSlice = sliceInfo->GetSlice(0);
        if(!pSlice) { throw h265_exception(UMC_ERR_NULL_PTR); }
        H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

        PackPicParams(pCurrentFrame, sliceInfo, supplier);

        Status s = m_va->Execute();
        if(s != UMC_OK)
            throw h265_exception(s);
    }

    void PackerVA_Widevine::PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier)
    {
        if(!pCurrentFrame || !pSliceInfo || !supplier) { throw h265_exception(UMC_ERR_NULL_PTR); }
        auto pSlice = static_cast<H265WidevineSlice*>(pSliceInfo->GetSlice(0));
        const H265SeqParamSet* pSeqParamSet = pSlice->GetSeqParam();

        UMCVACompBuffer *picParamBuf;
        VAPictureParameterBufferHEVC* picParam = (VAPictureParameterBufferHEVC*)m_va->GetCompBuffer(VAPictureParameterBufferType, &picParamBuf, sizeof(VAPictureParameterBufferHEVC));
        if (!picParam)
            throw h265_exception(UMC_ERR_FAILED);

        picParamBuf->SetDataSize(sizeof(VAPictureParameterBufferHEVC));
        memset(picParam, 0, sizeof(VAPictureParameterBufferHEVC));

        picParam->CurrPic.picture_id = m_va->GetSurfaceID(pCurrentFrame->GetFrameMID());
        picParam->CurrPic.pic_order_cnt = pCurrentFrame->m_PicOrderCnt;
        picParam->CurrPic.flags = 0;

        size_t count = 0;
        H265DBPList *dpb = supplier->GetDPBList();
        for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(picParam->ReferenceFrames)/sizeof(picParam->ReferenceFrames[0]) ; frame = frame->future())
        {
            if (frame == pCurrentFrame)
                continue;

            int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

            if (refType != NO_REFERENCE)
            {
                picParam->ReferenceFrames[count].pic_order_cnt = frame->m_PicOrderCnt;
                picParam->ReferenceFrames[count].picture_id = m_va->GetSurfaceID(frame->GetFrameMID());;
                picParam->ReferenceFrames[count].flags = refType == LONG_TERM_REFERENCE ? VA_PICTURE_HEVC_LONG_TERM_REFERENCE : 0;
                count++;
            }
        }

        for (size_t n = count;n < sizeof(picParam->ReferenceFrames)/sizeof(picParam->ReferenceFrames[0]); n++)
        {
            picParam->ReferenceFrames[n].picture_id = VA_INVALID_SURFACE;
            picParam->ReferenceFrames[n].flags = VA_PICTURE_HEVC_INVALID;
        }

        ReferencePictureSet *rps = pSlice->getRPS();
        uint32_t index;
        int32_t pocList[3*8];
        int32_t numRefPicSetStCurrBefore = 0,
            numRefPicSetStCurrAfter  = 0,
            numRefPicSetLtCurr       = 0;
        for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
                pocList[numRefPicSetStCurrBefore++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);

        for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);

        for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures(); index++)
        {
            int32_t poc = rps->getPOC(index);
            H265DecoderFrame *pFrm = supplier->GetDPBList()->findLongTermRefPic(pCurrentFrame, poc, pSeqParamSet->log2_max_pic_order_cnt_lsb, !rps->getCheckLTMSBPresent(index));

            if (pFrm)
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pFrm->PicOrderCnt();
            }
            else
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);
            }
        }

        int32_t i = 0;
        VAPictureHEVC sortedReferenceFrames[15];
        memset(sortedReferenceFrames, 0, sizeof(sortedReferenceFrames));

        for(int32_t n=0 ; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr ; n++)
        {
            if (!rps->getUsed(n))
                continue;

            for(size_t k=0;k < count;k++)
            {
                if(pocList[n] == picParam->ReferenceFrames[k].pic_order_cnt)
                {
                    if(n < numRefPicSetStCurrBefore)
                    {
                        picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE;
                        sortedReferenceFrames[i++] = picParam->ReferenceFrames[k];
                    }
                    else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                    {
                        picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_ST_CURR_AFTER;
                        sortedReferenceFrames[i++] = picParam->ReferenceFrames[k];
                    }
                    else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                    {
                        picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_LT_CURR;
                        sortedReferenceFrames[i++] = picParam->ReferenceFrames[k];
                    }
                }
            }
        }

        for (uint32_t n = count;n < sizeof(sortedReferenceFrames)/sizeof(sortedReferenceFrames[0]); n++)
        {
            sortedReferenceFrames[n].picture_id = VA_INVALID_SURFACE;
            sortedReferenceFrames[n].flags = VA_PICTURE_HEVC_INVALID;
        }

        MFX_INTERNAL_CPY(picParam->ReferenceFrames, sortedReferenceFrames, sizeof(sortedReferenceFrames));

        UMCVACompBuffer *pParamBuf;
        unsigned int* pSliceID = (unsigned int*)m_va->GetCompBuffer(VAStatusParameterBufferType, &pParamBuf, sizeof(unsigned int));
        if (!pSliceID)
            throw h265_exception(UMC_ERR_FAILED);

        *pSliceID = pSlice->GetWidevineStatusReportNumber();

        pParamBuf->SetDataSize(sizeof(unsigned int));
    }
} // namespace UMC_HEVC_DECODER

#endif // #if defined (UMC_VA_LINUX) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // UMC_ENABLE_H265_VIDEO_DECODER