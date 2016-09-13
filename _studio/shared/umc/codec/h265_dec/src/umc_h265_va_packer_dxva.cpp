/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_h265_va_packer_dxva.h"

using namespace UMC;

namespace UMC_HEVC_DECODER
{
    PackerDXVA2::PackerDXVA2(VideoAccelerator * va)
        : Packer(va)
        , m_statusReportFeedbackCounter(1)
    {
    }

    Status PackerDXVA2::GetStatusReport(void * pStatusReport, size_t size)
    {
        return m_va->ExecuteStatusReportBuffer(pStatusReport, (Ipp32u)size);
    }

    void PackerDXVA2::BeginFrame(H265DecoderFrame*)
    {
        m_statusReportFeedbackCounter++;
    }

    void PackerDXVA2::EndFrame()
    {
    }

    void PackerDXVA2::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
    {
        H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
        int sliceCount = sliceInfo->GetSliceCount();

        if (!sliceCount)
            return;

        H265Slice *pSlice = sliceInfo->GetSlice(0);
        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
        H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

        Ipp32s first_slice = 0;

        for (;;)
        {
            bool notchopping = true;
            PackPicParams(pCurrentFrame, sliceInfo, supplier);
            if (pSeqParamSet->scaling_list_enabled_flag)
            {
                PackQmatrix(pSlice);
            }

            Ipp32u sliceNum = 0;
            for (Ipp32s n = first_slice; n < sliceCount; n++)
            {
                notchopping = PackSliceParams(sliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
                if (!notchopping)
                {
                    //dependent slices should be with first independent slice
                    for (Ipp32s i = n; i >= first_slice; i--)
                    {
                        if (!sliceInfo->GetSlice(i)->GetSliceHeader()->dependent_slice_segment_flag)
                            break;

                        UMCVACompBuffer *headVABffr = 0;

                        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
                        size_t headerSize = m_va->IsLongSliceControl() ? sizeof(DXVA_Intel_Slice_HEVC_Long) : sizeof(DXVA_Slice_HEVC_Short);
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
}

#endif //UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER