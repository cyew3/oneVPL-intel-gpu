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

#ifndef _MFX_H264_PAK_PRED_H_
#define _MFX_H264_PAK_PRED_H_

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_PAK)

namespace H264Pak
{
    class MbDescriptor;
    union CoeffData;

    void PredictMb(
        mfxFrameParamAVC& fp,
        mfxExtAvcRefSliceInfo& sliceInfo,
        mfxFrameSurface& fs,
        MbDescriptor& neighbours,
        mfxI16 scaleMatrix8x8[2][6][64],
        mfxI16 scaleMatrix8x8Inv[2][6][64],
        CoeffData& coeffData);
};

#endif // MFX_ENABLE_H264_VIDEO_PAK
#endif // _MFX_H264_PAK_PRED_H_
