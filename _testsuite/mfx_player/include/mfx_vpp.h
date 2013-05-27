/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_ivpp.h"

//default implementation of vpp interface
class MFXVideoVPPImpl
    : public IMFXVideoVPP
{
public:

    MFXVideoVPPImpl(mfxSession session) { m_session = session; }
    virtual ~MFXVideoVPPImpl(void) { Close(); }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { return MFXVideoVPP_Query(m_session, in, out); }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest request[2]) { return MFXVideoVPP_QueryIOSurf(m_session, par, request); }
    virtual mfxStatus Init(mfxVideoParam *par) { return MFXVideoVPP_Init(m_session, par); }
    virtual mfxStatus Reset(mfxVideoParam *par) { return MFXVideoVPP_Reset(m_session, par); }
    virtual mfxStatus Close(void) { return MFXVideoVPP_Close(m_session); }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) { return MFXVideoVPP_GetVideoParam(m_session, par); }
    virtual mfxStatus GetVPPStat(mfxVPPStat *stat) { return MFXVideoVPP_GetVPPStat(m_session, stat); }
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp) { return MFXVideoVPP_RunFrameVPPAsync(m_session, in, out, aux, syncp); }
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) { return MFXVideoCORE_SyncOperation(m_session, syncp, wait);}

protected:

    mfxSession m_session;  // (mfxSession) handle to the owning session
};
