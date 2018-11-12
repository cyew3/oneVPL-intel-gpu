// Copyright (c) 2008-2018 Intel Corporation
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

#include "mfx_common.h"

#ifdef MFX_ENABLE_H264_VIDEO_ENC_HW

#ifndef _MFX_H264_ENC_HW_H_
#define _MFX_H264_ENC_HW_H_

#include "mfxvideo++int.h"
#include "mfx_h264_enc_common_hw.h"

class MFXHWVideoENCH264 : public VideoENC
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXHWVideoENCH264(VideoCORE *core, mfxStatus *status);
    virtual ~MFXHWVideoENCH264();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus RunFrameVmeENC(mfxFrameCUC *cuc);

private:
    VideoCORE *m_core;
    MfxHwH264Encode::MfxVideoParam m_videoParam;
    mfxFrameParam m_frameParam;

    bool m_reconRegistered;
    MfxHwH264Encode::DdiEncoder m_ddi;
    MfxHwH264Encode::ExecuteBuffers m_ddiData;

    MfxHwH264Encode::Initialized<mfxFrameAllocResponse> m_mb;
    MfxHwH264Encode::Initialized<mfxFrameAllocResponse> m_mv;
    MfxHwH264Encode::Initialized<mfxFrameAllocResponse> m_recons;
    MfxHwH264Encode::Initialized<ENCODE_MBDATA_LAYOUT> m_layout;
};

MFXHWVideoENCH264* CreateMFXHWVideoENCH264(VideoCORE *core, mfxStatus *status);

#endif //_MFX_H264_ENC_HW_H_

#endif //MFX_ENABLE_H264_VIDEO_ENC_HW
