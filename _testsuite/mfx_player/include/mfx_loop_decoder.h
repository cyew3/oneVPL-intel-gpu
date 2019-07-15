/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once


#include "mfx_yuv_decoder.h"
//#include "mfx_io_utils.h"
#include "mfx_iproxy.h"

//decodes several frames and then delivers them in a loop fashion, will work endless, should be used only with -n option
class MFXLoopDecoder
    : public InterfaceProxy<IYUVSource>
{
    typedef InterfaceProxy<IYUVSource> base;
public:
    MFXLoopDecoder( mfxI32 nNumFramesInLoop, std::unique_ptr<IYUVSource> &&pTarget);

    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    //virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus DecodeFrameAsync(
        mfxBitstream2     &bs2,
        mfxFrameSurface1  *surface_work,
        mfxFrameSurface1 **surface_out,
        mfxSyncPoint      *syncp);

    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait);

protected:

    //buffered surfaces pool
    std::vector<mfxFrameSurface1*>  m_Surfaces;
    mfxU16                  m_CurrSurfaceIndex;
    mfxSyncPoint            m_syncPoint;
    IVideoSession* m_session;
};
