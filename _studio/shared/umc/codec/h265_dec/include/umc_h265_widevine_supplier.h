//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#ifndef __UMC_H265_WIDEVINE_SUPPLIER_H
#define __UMC_H265_WIDEVINE_SUPPLIER_H

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#include "umc_h265_va_supplier.h"

namespace UMC_HEVC_DECODER
{
    class WidevineDecrypter;
    class DecryptParametersWrapper;

    class WidevineTaskSupplier
        : public VATaskSupplier
    {

    public:

        WidevineTaskSupplier();
        ~WidevineTaskSupplier();

        UMC::Status Init(UMC::VideoDecoderParams*) override;
        void Reset() override;

    protected:

        UMC::Status ParseWidevineVPSSPSPPS(DecryptParametersWrapper*);
        H265Slice*  ParseWidevineSliceHeader(DecryptParametersWrapper*);
        UMC::Status ParseWidevineSEI(DecryptParametersWrapper*);

        UMC::Status AddOneFrame(UMC::MediaData*) override;

        void CompleteFrame(H265DecoderFrame*) override;

    private:

        WidevineTaskSupplier& operator=(WidevineTaskSupplier const&) = delete;

    private:

        std::unique_ptr<WidevineDecrypter> m_pWidevineDecrypter;
    };

} // namespace UMC_HEVC_DECODER

#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // __UMC_H265_WIDEVINE_SUPPLIER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
