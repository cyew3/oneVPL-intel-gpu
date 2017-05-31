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

#ifndef __UMC_H265_VA_PACKER_DXVA_H
#define __UMC_H265_VA_PACKER_DXVA_H

#include "umc_h265_va_packer.h"

#ifndef UMC_RESTRICTED_CODE_VA

#ifdef UMC_VA_DXVA

#include "umc_h265_task_supplier.h"

namespace UMC_HEVC_DECODER
{
    inline int
    LengthInMinCb(int length, int cbsize)
    {
        return length/(1 << cbsize);
    }

    class PackerDXVA2
        : public Packer
    {

    public:

        PackerDXVA2(UMC::VideoAccelerator * va);

        virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size);
        virtual UMC::Status SyncTask(Ipp32s /*index*/, void * /*error*/) { return UMC::UMC_ERR_UNSUPPORTED; }

        virtual void BeginFrame(H265DecoderFrame*);
        virtual void EndFrame();

        void PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier);

        using Packer::PackQmatrix;
        virtual void PackSubsets(const H265DecoderFrame *pCurrentFrame);

    protected:

        template <typename T>
        void PackQmatrix(H265Slice const*, T* pQmatrix);

    protected:

        Ipp32u              m_statusReportFeedbackCounter;
    };
}

#endif // UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA

#endif /* __UMC_H265_VA_PACKER_DXVA_H */
#endif // UMC_ENABLE_H265_VIDEO_DECODER