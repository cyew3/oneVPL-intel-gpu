/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.


File Name: mfx_encode.h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_encode.h"

class MFXVideoEncode : public IVideoEncode
{

public:
    MFXVideoEncode(mfxSession session) { m_session = session; }
    virtual ~MFXVideoEncode(void) { Close(); }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { return MFXVideoENCODE_Query(m_session, in, out); }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) { return MFXVideoENCODE_QueryIOSurf(m_session, par, request); }
    virtual mfxStatus Init(mfxVideoParam *par) { return MFXVideoENCODE_Init(m_session, par); }
    virtual mfxStatus Reset(mfxVideoParam *par) { return MFXVideoENCODE_Reset(m_session, par); }
    virtual mfxStatus Close(void) { return MFXVideoENCODE_Close(m_session); }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) { return MFXVideoENCODE_GetVideoParam(m_session, par); }
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat) { return MFXVideoENCODE_GetEncodeStat(m_session, stat); }

    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp) 
    { return MFXVideoENCODE_EncodeFrameAsync(m_session, ctrl, surface, bs, syncp); }

    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait){return MFXVideoCORE_SyncOperation(m_session, syncp, wait);}

protected:
    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};