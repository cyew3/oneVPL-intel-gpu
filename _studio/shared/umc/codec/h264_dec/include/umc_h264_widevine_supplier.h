// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_WIDEVINE_SUPPLIER_H
#define __UMC_H264_WIDEVINE_SUPPLIER_H

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#ifndef MFX_ENABLE_CPLIB

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

#endif // #ifndef MFX_ENABLE_CPLIB
#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // __UMC_H264_WIDEVINE_SUPPLIER_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
