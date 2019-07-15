/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_decoder.h"
#include "mfx_iproxy.h"

typedef struct
{
    mfxFrameSurface1 *surface_out;
    mfxSyncPoint     syncp;
} sDecodedFrame;


class MFXAdvanceDecoder
    : public InterfaceProxy<IYUVSource>
{
public:
    MFXAdvanceDecoder( int nOutputBuffering, std::unique_ptr<IYUVSource> &&pTarget)
        : InterfaceProxy<IYUVSource>(std::move(pTarget))
        , m_OutputBuffering(nOutputBuffering)
    {}

    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

protected:
    size_t                   m_OutputBuffering;
    std::list<sDecodedFrame> m_OutputBuffer;
};
