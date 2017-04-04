/////////////////////////////////////////////////////////////////////////////
////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
////     This software is supplied under the terms of a license agreement or
////     nondisclosure agreement with Intel Corporation and may not be copied
////     or disclosed except in accordance with the terms of that agreement.
////          Copyright(c) 2017 Intel Corporation. All Rights Reserved.
////

#pragma once

#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"

class tsVideoEncoder : virtual public tsSession, virtual public tsSurfacePool
{
public:
    bool                        m_default;
    bool                        m_initialized;
    bool                        m_loaded;
    tsExtBufType<mfxVideoParam> m_par;
    tsExtBufType<mfxBitstream>  m_bitstream;
    tsExtBufType<mfxEncodeCtrl> m_ctrl;
    mfxFrameAllocRequest        m_request;
    mfxVideoParam*              m_pPar;
    mfxVideoParam*              m_pParOut;
    mfxBitstream*               m_pBitstream;
    mfxFrameAllocRequest*       m_pRequest;
    mfxSyncPoint*               m_pSyncPoint;
    mfxFrameSurface1*           m_pSurf;
    mfxEncodeCtrl*              m_pCtrl;
    tsSurfaceProcessor*         m_filler;
    tsBitstreamProcessor*       m_bs_processor;
    mfxU32                      m_frames_buffered;
    mfxPluginUID*               m_uid;
    bool                        m_single_field_processing;
    mfxU16                      m_field_processed;

    tsVideoEncoder(mfxU32 CodecId = 0, bool useDefaults = true);
    tsVideoEncoder(mfxFeiFunction func, mfxU32 CodecId = 0, bool useDefaults = true);
    ~tsVideoEncoder();
    
    mfxStatus Init();
    mfxStatus Init(mfxSession session, mfxVideoParam *par);

    mfxStatus Close();
    mfxStatus Close(mfxSession session);

    mfxStatus Query();
    mfxStatus Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);

    mfxStatus QueryIOSurf();
    mfxStatus QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
    
    mfxStatus Reset();
    mfxStatus Reset(mfxSession session, mfxVideoParam *par);

    mfxStatus GetVideoParam();
    mfxStatus GetVideoParam(mfxSession session, mfxVideoParam *par);

    mfxStatus EncodeFrameAsync();
    mfxStatus EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp);

    mfxStatus AllocBitstream(mfxU32 size = 0);
    mfxStatus AllocSurfaces();

    
    mfxStatus SyncOperation();
    mfxStatus SyncOperation(mfxSyncPoint syncp);
    mfxStatus SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

    mfxStatus EncodeFrames(mfxU32 n, bool check=false);

    mfxStatus InitAndSetAllocator();

    mfxStatus DrainEncodedBitstream();

    mfxStatus Load() { m_loaded = (0 == tsSession::Load(m_session, m_uid, 1)); return g_tsStatus.get(); }
};

class tsAutoFlush : tsAutoFlushFiller
{
public:
    tsAutoFlush(tsVideoEncoder& enc, mfxU32 n_frames)
        : tsAutoFlushFiller(n_frames)
        , m_enc(enc)
    {
        m_enc.m_filler = this;
    }

    ~tsAutoFlush()
    {
        m_enc.m_filler = 0;
    }

private:
    tsVideoEncoder& m_enc;
};
