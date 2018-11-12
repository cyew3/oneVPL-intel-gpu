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

#ifndef _MFX_H264_PAK_H_
#define _MFX_H264_PAK_H_

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_PAK

#include "mfxvideo++int.h"
#include "mfx_h264_pak_utils.h"
#include "mfx_h264_pak_pack.h"

class MFXVideoPAKH264 : public VideoPAK {
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoPAKH264(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoPAKH264();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus RunFramePAK(mfxFrameCUC *cuc);

private:
    VideoCORE* m_core;
    mfxVideoParam m_videoParam;
    mfxFrameParam m_frameParam;

    H264Pak::SeqScalingMatrices8x8 m_scaling8x8;
    H264Pak::PredThreadPool m_threads;
    H264Pak::H264PakBitstream m_bitstream;
    H264Pak::PakResources m_res;
};

#endif // MFX_ENABLE_H264_VIDEO_PAK
#endif // _MFX_H264_PAK_H_
