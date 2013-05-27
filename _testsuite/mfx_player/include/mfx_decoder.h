/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_DECODER_H
#define __MFX_DECODER_H

#include "mfx_iyuv_source.h"
#include "mfx_frame_allocator_rw.h"


// this is wrapper over MFXVideoDECODE (+ MFXSession to implement SyncOperation)
class MFXDecoder : public IYUVSource
{
public:
    MFXDecoder(mfxSession session) : m_session(session) {}

    virtual ~MFXDecoder() { Close(); }
    virtual mfxStatus Init(mfxVideoParam *par) { return MFXVideoDECODE_Init(m_session, par); }
    virtual mfxStatus Close() { return MFXVideoDECODE_Close(m_session); }
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { return MFXVideoDECODE_Query(m_session, in, out); }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request){return MFXVideoDECODE_QueryIOSurf(m_session, par, request);}
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat) { return MFXVideoDECODE_GetDecodeStat(m_session, stat); }
    virtual mfxStatus DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        return MFXVideoDECODE_DecodeFrameAsync(m_session, bs.isNull ? NULL : &bs, surface_work, surface_out, syncp);
    }
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) { return MFXVideoCORE_SyncOperation(m_session, syncp, wait); }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) {return MFXVideoDECODE_DecodeHeader(m_session, bs, par);}
    virtual mfxStatus Reset(mfxVideoParam *par) {return MFXVideoDECODE_Reset(m_session, par);}
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) {return MFXVideoDECODE_GetVideoParam(m_session, par);}
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) {return MFXVideoDECODE_SetSkipMode(m_session, mode);}

protected:
    mfxSession m_session;
};

#endif//__MFX_DECODER_H
