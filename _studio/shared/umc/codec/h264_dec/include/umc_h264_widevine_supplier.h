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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_WIDEVINE_SUPPLIER_H
#define __UMC_H264_WIDEVINE_SUPPLIER_H

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#include "umc_h264_va_supplier.h"

namespace UMC
{
    class POCDecoderWidevine
        : public POCDecoder
    {

    public:

        void DecodePictureOrderCount(const H264Slice*, int32_t) override;
    };

    class WidevineDecrypter;
    class DecryptParametersWrapper;

    class WidevineTaskSupplier
        : public VATaskSupplier
    {

    public:

        WidevineTaskSupplier();
        ~WidevineTaskSupplier();

        Status Init(VideoDecoderParams*) override;
        void Reset() override;

    protected:

        Status ParseWidevineSPSPPS(DecryptParametersWrapper*);
        Status ParseWidevineSEI(DecryptParametersWrapper*);
        H264Slice* ParseWidevineSliceHeader(DecryptParametersWrapper*);

        Status AddOneFrame(MediaData*) override;

        Status CompleteFrame(H264DecoderFrame*, int32_t) override;

    private:

        WidevineTaskSupplier& operator=(WidevineTaskSupplier const&) = delete;

    private:

        std::unique_ptr<WidevineDecrypter> m_pWidevineDecrypter;
    };

} // namespace UMC

#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // __UMC_H264_WIDEVINE_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
