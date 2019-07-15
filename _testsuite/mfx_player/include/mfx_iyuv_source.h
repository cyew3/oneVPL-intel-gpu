/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxvideo++.h"
#include "mfx_iproxy.h"
#include "mfx_bitstream2.h"

// ABStraction for MFXVideoDECODE and YUV file reader
class IYUVSource : public EnableProxyForThis<IYUVSource>
{
public:
    virtual ~IYUVSource() {}

    virtual mfxStatus Init(mfxVideoParam *par) = 0;
    virtual mfxStatus Close() = 0;

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) = 0;
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat) = 0;

    virtual mfxStatus DecodeFrameAsync(
        mfxBitstream2 &bs,
        mfxFrameSurface1 *surface_work,
        mfxFrameSurface1 **surface_out,
        mfxSyncPoint *syncp) = 0;

    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) = 0;
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) = 0;
    virtual mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) = 0;
    // don't need these for now
    // virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) = 0;
};

//wrappers always necessary
template <>
class InterfaceProxy<IYUVSource>
    : public InterfaceProxyBase<IYUVSource>
{
public:
    InterfaceProxy(std::unique_ptr<IYUVSource> &&pTarget)
        : InterfaceProxyBase<IYUVSource>(std::move(pTarget))
    {
    }

    virtual mfxStatus Init(mfxVideoParam *par) { return m_pTarget->Init(par); }
    virtual mfxStatus Close() { return m_pTarget->Close(); }
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { return m_pTarget->Query(in, out); }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request){return m_pTarget->QueryIOSurf(par, request);}
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat) { return m_pTarget->GetDecodeStat(stat); }
    virtual mfxStatus DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        return m_pTarget->DecodeFrameAsync(bs, surface_work, surface_out, syncp);
    }
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) { return m_pTarget->SyncOperation(syncp, wait); }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) {return m_pTarget->DecodeHeader(bs, par);}
    virtual mfxStatus Reset(mfxVideoParam *par) {return m_pTarget->Reset(par);}
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) {return m_pTarget->GetVideoParam(par);}
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) {return m_pTarget->SetSkipMode(mode);}
};
