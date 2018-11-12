// Copyright (c) 2013-2018 Intel Corporation
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

#pragma once

#include "mfx_common.h"

#ifdef MFX_ENABLE_WATERMARK

class Watermark
{
public:
    Watermark()
    {
        m_watermarkData = NULL;
        m_yuvBuffer[0] = NULL;
        m_rgbaBuffer = NULL;
        m_bitmapBuffer = NULL;
    }

    ~Watermark()
    {
        Release();
    }

    static Watermark *CreateFromResource(void);
    void Apply(mfxU8 *srcY, mfxU8 *srcUV, mfxI32 stride, mfxI32 width, mfxI32 height);
    void Release(void);
private:
    mfxI32 m_watermarkWidth, m_watermarkHeight;
    mfxU8 *m_watermarkData;
    Ipp8u *m_yuvBuffer[3];
    Ipp8u *m_rgbaBuffer, *m_bitmapBuffer;
    Ipp32s m_yuvSize[3];
    IppiSize m_roi;

    void InitBuffers(mfxI32 size);
    void ReleaseBuffers(void);

    // noncopyable
    Watermark(const Watermark &);
    void operator=(const Watermark &);
};

#endif // MFX_ENABLE_WATERMARK
