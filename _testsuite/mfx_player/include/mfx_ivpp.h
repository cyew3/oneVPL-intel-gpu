/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfxstructures.h"
#include "mfx_iproxy.h"

//Generic vpp abstraction
class  IMFXVideoVPP 
    : public EnableProxyForThis<IMFXVideoVPP>
{
public:
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest request[2])   = 0;
    virtual mfxStatus Init(mfxVideoParam *par)   = 0;
    virtual mfxStatus Reset(mfxVideoParam *par)   = 0;
    virtual mfxStatus Close(void)   = 0;
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)   = 0;
    virtual mfxStatus GetVPPStat(mfxVPPStat *stat)   = 0;
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)   = 0;
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) = 0;
};


//proxy definition
template <>
class InterfaceProxy<IMFXVideoVPP>
    : public InterfaceProxyBase<IMFXVideoVPP>
{
public:
    InterfaceProxy(IMFXVideoVPP* pTarget)
        : InterfaceProxyBase<IMFXVideoVPP>(pTarget)
    {
    }
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        return m_pTarget->Query(in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest request[2])
    {
        return m_pTarget->QueryIOSurf(par, request);
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        return m_pTarget->Init(par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return m_pTarget->Reset(par);
    }
    virtual mfxStatus Close(void)
    {
        return m_pTarget->Close();
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return m_pTarget->GetVideoParam(par);
    }
    virtual mfxStatus GetVPPStat(mfxVPPStat *stat)
    {
        return m_pTarget->GetVPPStat(stat);
    }
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
    {
        return m_pTarget->RunFrameVPPAsync(in, out, aux, syncp);
    }
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait)
    {
        return Target().SyncOperation(syncp, wait);
    }
};

