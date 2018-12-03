// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_H265_CENC_SUPPLIER_H
#define __UMC_H265_CENC_SUPPLIER_H

#include "umc_va_base.h"
#if defined (UMC_VA_LINUX)
#if defined (MFX_ENABLE_CPLIB)

#include "umc_h265_va_supplier.h"

namespace UMC_HEVC_DECODER
{
    class CENCDecrypter;
    class CENCParametersWrapper;

    class CENCTaskSupplier
        : public VATaskSupplier
    {

    public:

        CENCTaskSupplier();
        ~CENCTaskSupplier();

    #if !defined(OPEN_SOURCE)
        UMC::Status Init(UMC::VideoDecoderParams*) override;
        void Reset() override;
    #endif

    protected:

        H265Slice * ParseCENCSliceHeader(CENCParametersWrapper*);

        UMC::Status AddOneFrame(UMC::MediaData*) override;

    #if !defined(OPEN_SOURCE)
        void CompleteFrame(H265DecoderFrame*) override;
    #endif

    private:

        CENCTaskSupplier & operator = (CENCTaskSupplier &) = delete;

    private:

        mfxU32 m_status_report_index_feedback;
        UMC::MediaData m_cencData;

    #if !defined(OPEN_SOURCE)
        std::unique_ptr<CENCDecrypter> m_pCENCDecrypter;
    #endif
    };

} // namespace UMC_HEVC_DECODER

#endif // #if defined (MFX_ENABLE_CPLIB)
#endif // #if defined (UMC_VA_LINUX)
#endif // __UMC_H265_CENC_SUPPLIER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
