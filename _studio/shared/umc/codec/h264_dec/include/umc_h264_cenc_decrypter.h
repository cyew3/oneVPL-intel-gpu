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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_CENC_DECRYPTER_H
#define __UMC_H264_CENC_DECRYPTER_H

#include "umc_va_base.h"
#if defined (UMC_VA_LINUX)
#if defined (MFX_ENABLE_CPLIB)

#include "umc_h264_headers.h"
#include "va_cp_private.h"

#if !defined(OPEN_SOURCE)
#include "va/va_backend.h"
#endif

namespace UMC
{
class CENCParametersWrapper : public VACencSliceParameterBufferH264
{
public:
    CENCParametersWrapper(void);
    virtual ~CENCParametersWrapper(void);

    CENCParametersWrapper & operator = (const VACencSliceParameterBufferH264 & pDecryptParameters);

    Status GetSliceHeaderPart1(UMC_H264_DECODER::H264SliceHeader *pSliceHeader);
    Status GetSliceHeaderPart2(UMC_H264_DECODER::H264SliceHeader *pSliceHeader,
                               const UMC_H264_DECODER::H264PicParamSet *pps,
                               const UMC_H264_DECODER::H264SeqParamSet *sps);
    Status GetSliceHeaderPart3(UMC_H264_DECODER::H264SliceHeader *pSliceHeader,
                               UMC_H264_DECODER::PredWeightTable *pPredWeight_L0,
                               UMC_H264_DECODER::PredWeightTable *pPredWeight_L1,
                               UMC_H264_DECODER::RefPicListReorderInfo *pReorderInfo_L0,
                               UMC_H264_DECODER::RefPicListReorderInfo *pReorderInfo_L1,
                               UMC_H264_DECODER::AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
                               UMC_H264_DECODER::AdaptiveMarkingInfo *pBaseAdaptiveMarkingInfo,
                               const UMC_H264_DECODER::H264PicParamSet *pps,
                               const UMC_H264_DECODER::H264SeqParamSet *sps,
                               const UMC_H264_DECODER::H264SeqParamSetSVCExtension *spsSvcExt);
    Status GetSliceHeaderPart4(UMC_H264_DECODER::H264SliceHeader *hdr,
                                const UMC_H264_DECODER::H264SeqParamSetSVCExtension *spsSvcExt);

    uint16_t GetStatusReportNumber() const {return m_CENCStatusReportNumber;}
    void SetStatusReportNumber(uint16_t statusReportNumber) {m_CENCStatusReportNumber = statusReportNumber;}

    double GetTime() const {return m_pts;}
    void SetTime(double pts) {m_pts = pts;}

private:
    void CopyDecryptParams(const VACencSliceParameterBufferH264 & pDecryptParameters);

private:
    double m_pts;
    uint16_t m_CENCStatusReportNumber;
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

    void SetVideoHardwareAccelerator(VideoAccelerator * va)
    {
        if (va)
            m_va = (VideoAccelerator*)va;
    }

    Status DecryptFrame(MediaData *pSource, VACencStatusBuf* pCencStatus);

    void ReleaseForNewBitstream()
    {
        m_bitstreamSubmitted = false;
    }

protected:
    VideoAccelerator *m_va;
    bool m_bitstreamSubmitted;
    uint16_t m_PESPacketCounter;

    VADisplay m_dpy;
    VACencStatusBuf m_paramsSet;

    VAStatus (*m_vaEndCenc) (VADriverContextP ctx, VAContextID context);
    VAStatus (*m_vaQueryCenc) (VADriverContextP ctx, VABufferID buf_id, uint32_t size, VACencStatusBuf *info);
};
#endif // #if !defined(OPEN_SOURCE)

} // namespace UMC

#endif // #if defined (MFX_ENABLE_CPLIB)
#endif // #if defined (UMC_VA_LINUX)
#endif // __UMC_H264_CENC_DECRYPTER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
