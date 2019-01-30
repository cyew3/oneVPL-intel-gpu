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
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#ifndef __UMC_H265_CENC_DECRYPTER_H
#define __UMC_H265_CENC_DECRYPTER_H

#include "umc_va_base.h"
#if defined (UMC_VA_LINUX)
#if defined (MFX_ENABLE_CPLIB)

#include "umc_h265_headers.h"
#include "va_cp_private.h"

#if !defined(OPEN_SOURCE)
#include "va/va_backend.h"
#endif

namespace UMC_HEVC_DECODER
{
class H265CENCSlice;
class CENCParametersWrapper : public VACencSliceParameterBufferHEVC
{
public:
    CENCParametersWrapper(void);
    virtual ~CENCParametersWrapper(void);

    CENCParametersWrapper & operator = (const VACencSliceParameterBufferHEVC & pDecryptParameters);

    UMC::Status GetSliceHeaderPart1(H265SliceHeader * sliceHdr);
    UMC::Status GetSliceHeaderFull(H265CENCSlice *, const H265SeqParamSet *, const H265PicParamSet *);

    uint32_t GetStatusReportNumber() const {return m_CENCStatusReportNumber;}
    void SetStatusReportNumber(uint32_t statusReportNumber) {m_CENCStatusReportNumber = statusReportNumber;}

    double GetTime() const {return m_pts;}
    void SetTime(double pts) {m_pts = pts;}

private:
    // Parse RPS part in slice header
    void parseRefPicSet(ReferencePictureSet* pRPS);
    // Parse remaining of slice header after GetSliceHeaderPart1
    void decodeSlice(H265CENCSlice *, const H265SeqParamSet *, const H265PicParamSet *);

    void CopyDecryptParams(const VACencSliceParameterBufferHEVC & pDecryptParameters);

private:
    double m_pts;
    uint32_t m_CENCStatusReportNumber;
};

#if !defined(OPEN_SOURCE)
class CENCDecrypter
{
public:

    CENCDecrypter()
        : m_va(0)
        , m_bitstreamSubmitted(false)
        , m_PESPacketCounter(0)
        , m_dpy(nullptr)
        , m_vaEndCenc(nullptr)
        , m_vaQueryCenc(nullptr)
    {
        memset(&m_paramsSet, 0, sizeof(VACencStatusBuf));
    }

    ~CENCDecrypter()
    {
        if (m_paramsSet.buf)
        {
            free(m_paramsSet.buf);
            m_paramsSet.buf = nullptr;
        }
        if (m_paramsSet.slice_buf)
        {
            free(m_paramsSet.slice_buf);
            m_paramsSet.slice_buf = nullptr;
        }
    }

    void Init()
    {
        m_bitstreamSubmitted = false;
        m_PESPacketCounter = 0;
        if (!m_va)
            return;
    }

    void Reset()
    {
        m_bitstreamSubmitted = false;
        m_PESPacketCounter = 0;
    }

    void SetVideoHardwareAccelerator(UMC::VideoAccelerator * va)
    {
        if (va)
            m_va = (UMC::VideoAccelerator*)va;
    }

    UMC::Status DecryptFrame(UMC::MediaData *pSource, VACencStatusBuf* pCencStatus);

    void ReleaseForNewBitstream()
    {
        m_bitstreamSubmitted = false;
    }

protected:
    UMC::VideoAccelerator *m_va;
    bool m_bitstreamSubmitted;
    uint32_t m_PESPacketCounter;

    VADisplay m_dpy;
    VACencStatusBuf m_paramsSet;

    VAStatus (*m_vaEndCenc) (VADriverContextP ctx, VAContextID context);
    VAStatus (*m_vaQueryCenc) (VADriverContextP ctx, VABufferID buf_id, uint32_t size, VACencStatusBuf *info);
};
#endif // #if !defined(OPEN_SOURCE)

} // namespace UMC_HEVC_DECODER

#endif // #if defined (MFX_ENABLE_CPLIB)
#endif // #if defined (UMC_VA_LINUX)
#endif // __UMC_H265_CENC_DECRYPTER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
