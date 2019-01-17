// Copyright (c) 2013-2019 Intel Corporation
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

#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_h265_va_packer_dxva.h"

using namespace UMC;

namespace UMC_HEVC_DECODER
{
    inline
    H265ScalingList* GetDefaultScalingList()
    {
        static H265ScalingList list{};
        if (!list.is_initialized())
            list.initFromDefaultScalingList();

        return &list;
    }

    PackerDXVA2::PackerDXVA2(VideoAccelerator * va)
        : Packer(va)
        , m_statusReportFeedbackCounter(1)
        , m_refFrameListCacheSize(0)
    {
    }

    Status PackerDXVA2::GetStatusReport(void * pStatusReport, size_t size)
    {
        return m_va->ExecuteStatusReportBuffer(pStatusReport, (uint32_t)size);
    }

    Status PackerDXVA2::SyncTask(int32_t index, void * error)
    {
        return m_va->SyncTask(index, error);
    }

    bool PackerDXVA2::IsGPUSyncEventEnable()
    {
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H265D
        return m_va->IsGPUSyncEventEnable();
#else
        return false;
#endif
    }

    void PackerDXVA2::BeginFrame(H265DecoderFrame*)
    {
        m_statusReportFeedbackCounter++;
        // WA: DXVA_Intel_Status_HEVC::StatusReportFeedbackNumber - USHORT, can't be 0 - reported status will be ignored
        if (m_statusReportFeedbackCounter > USHRT_MAX)
            m_statusReportFeedbackCounter = 1;
    }

    void PackerDXVA2::EndFrame()
    {
    }

    void PackerDXVA2::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
    {
        if (!frame || !supplier)
            throw h265_exception(UMC_ERR_NULL_PTR);

        H265DecoderFrameInfo const* pSliceInfo = frame->GetAU();
        if (!pSliceInfo)
            throw h265_exception(UMC_ERR_FAILED);

        auto pSlice = pSliceInfo->GetSlice(0);
        if (!pSlice)
            throw h265_exception(UMC_ERR_FAILED);

        H265SeqParamSet const* pSeqParamSet = pSlice->GetSeqParam();
        H265PicParamSet const* pPicParamSet = pSlice->GetPicParam();
        if (!pSeqParamSet || !pPicParamSet)
            throw h265_exception(UMC_ERR_FAILED);

        int32_t first_slice = 0;
        for (;;)
        {
            bool notchopping = true;
            PackPicParams(frame, supplier);
            if (pSeqParamSet->scaling_list_enabled_flag)
            {
                PackQmatrix(pSlice);
            }

            if (pPicParamSet->tiles_enabled_flag)
            {
                PackSubsets(frame);
            }

            uint32_t sliceNum = 0;
            int32_t sliceCount = pSliceInfo->GetSliceCount();
            for (int32_t n = first_slice; n < sliceCount; n++)
            {
                notchopping = PackSliceParams(pSliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
                if (!notchopping)
                {
                    //dependent slices should be with first independent slice
                    for (int32_t i = n; i >= first_slice; i--)
                    {
                        if (!pSliceInfo->GetSlice(i)->GetSliceHeader()->dependent_slice_segment_flag)
                            break;

                        UMCVACompBuffer *headVABffr = 0;

                        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
                        int32_t headerSize = m_va->IsLongSliceControl() ? sizeof(DXVA_Intel_Slice_HEVC_Long) : sizeof(DXVA_Slice_HEVC_Short);
                        headVABffr->SetDataSize(headVABffr->GetDataSize() - headerSize); //remove one slice

                        n--;
                    }

                    if (n <= first_slice) // avoid splitting of slice
                    {
                        m_va->Execute(); //for free picParam and Quant buffers
                        return;
                    }

                    first_slice = n;
                    break;
                }

                sliceNum++;
            }

            Status s = m_va->Execute();
            if(s != UMC_OK)
                throw h265_exception(s);

            if (!notchopping)
                continue;

            break;
        }
    }

    void PackerDXVA2::PackSubsets(const H265DecoderFrame *)
    { /* Nothing to do here, derived classes could extend behavior */ }

    void PackerDXVA2::PackQmatrix(H265Slice const* pSlice)
    {
        UMCVACompBuffer* buffer = nullptr;
        auto qmatrix = reinterpret_cast<DXVA_Qmatrix_HEVC*>(m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &buffer));
        if (!qmatrix)
            throw h265_exception(UMC_ERR_FAILED);

        buffer->SetDataSize(sizeof(DXVA_Qmatrix_HEVC));
        *qmatrix = {};

        const H265ScalingList *scalingList = 0;
        if (pSlice->GetPicParam()->pps_scaling_list_data_present_flag)
        {
            scalingList = pSlice->GetPicParam()->getScalingList();
        }
        else if (pSlice->GetSeqParam()->sps_scaling_list_data_present_flag)
        {
            scalingList = pSlice->GetSeqParam()->getScalingList();
        }
        else
        {
            scalingList = GetDefaultScalingList();
        }

        //new driver want list to be in raster scan, but we made it flatten during [H265HeadersBitstream::xDecodeScalingList]
        //so we should restore it now
        bool force_upright_scan =
            m_va->ScalingListScanOrder() == 0;

        initQMatrix<16>(scalingList, SCALING_LIST_4x4, qmatrix->ucScalingLists0, force_upright_scan);    // 4x4
        initQMatrix<64>(scalingList, SCALING_LIST_8x8, qmatrix->ucScalingLists1, force_upright_scan);    // 8x8
        initQMatrix<64>(scalingList, SCALING_LIST_16x16, qmatrix->ucScalingLists2, force_upright_scan);    // 16x16
        initQMatrix(scalingList, SCALING_LIST_32x32, qmatrix->ucScalingLists3, force_upright_scan);    // 32x32

        for (uint32_t sizeId = SCALING_LIST_16x16; sizeId <= SCALING_LIST_32x32; sizeId++)
        {
            for (uint32_t listId = 0; listId < g_scalingListNum[sizeId]; listId++)
            {
                if (sizeId == SCALING_LIST_16x16)
                    qmatrix->ucScalingListDCCoefSizeID2[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
                else if (sizeId == SCALING_LIST_32x32)
                    qmatrix->ucScalingListDCCoefSizeID3[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
            }
        }
    }
}

#endif //UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER