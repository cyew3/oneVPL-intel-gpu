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

#ifndef __UMC_H264_WIDEVINE_DECRYPTER_H
#define __UMC_H264_WIDEVINE_DECRYPTER_H

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#include "umc_h264_headers.h"
#include "huc_based_drm_common.h"

namespace UMC
{
class DecryptParametersWrapper : public DECRYPT_QUERY_STATUS_PARAMS_AVC
{
public:
    DecryptParametersWrapper(void);
    DecryptParametersWrapper(const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters);
    virtual ~DecryptParametersWrapper(void);

    DecryptParametersWrapper & operator = (const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters);
    DecryptParametersWrapper & operator = (const DecryptParametersWrapper & pDecryptParametersWrapper);

    Status GetSequenceParamSet(UMC_H264_DECODER::H264SeqParamSet *sps);
    Status GetPictureParamSetPart1(UMC_H264_DECODER::H264PicParamSet *pps);
    Status GetPictureParamSetPart2(UMC_H264_DECODER::H264PicParamSet *pps);

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

    void ParseSEIBufferingPeriod(const Headers & headers, UMC_H264_DECODER::H264SEIPayLoad *spl);
    void ParseSEIPicTiming(const Headers & headers, UMC_H264_DECODER::H264SEIPayLoad *spl);
    void ParseSEIRecoveryPoint(UMC_H264_DECODER::H264SEIPayLoad *spl);

    double GetTime() const {return m_pts;}
    void SetTime(double pts) {m_pts = pts;}

private:
    Status GetVUIParam(UMC_H264_DECODER::H264SeqParamSet *sps, UMC_H264_DECODER::H264VUI *vui);
    Status GetHRDParam(UMC_H264_DECODER::H264SeqParamSet *sps, h264_hrd_param_set_t *hrd, UMC_H264_DECODER::H264VUI *vui);

    void CopyDecryptParams(const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters);

private:
    double m_pts;
};


class WidevineDecrypter
{
public:

    WidevineDecrypter()
        : m_va(0)
        , m_bitstreamSubmitted(false)
        , m_PESPacketCounter(0)
    {
#ifdef UMC_VA_DXVA
        m_pDummySurface = NULL;
#endif
    }

    ~WidevineDecrypter()
    {
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

    Status DecryptFrame(MediaData *pSource, DecryptParametersWrapper* pDecryptParams);

    void ReleaseForNewBitstream()
    {
        m_bitstreamSubmitted = false;
    }

protected:
    VideoAccelerator *m_va;
    bool m_bitstreamSubmitted;
    uint16_t m_PESPacketCounter;

#ifdef UMC_VA_DXVA
    IDirect3DSurface9* m_pDummySurface;
#endif
};

} // namespace UMC

#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // __UMC_H264_WIDEVINE_DECRYPTER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
