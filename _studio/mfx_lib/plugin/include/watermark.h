//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

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
