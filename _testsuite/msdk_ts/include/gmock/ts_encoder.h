#pragma once

#include "ts_session.h"

class tsVideoEncoder : public tsSession, public tsSurfacePool
{
public:
    bool                    m_default;
    bool                    m_initialized;
    mfxVideoParam           m_par;
    mfxBitstream            m_bitstream;
    mfxFrameAllocRequest    m_request;
    mfxVideoParam*          m_pPar;
    mfxVideoParam*          m_pParOut;
    mfxBitstream*           m_pBitstream;
    mfxFrameAllocRequest*   m_pRequest;
    mfxSyncPoint*           m_pSyncPoint;
    mfxFrameSurface1*       m_pSurf;

    tsVideoEncoder(mfxU32 CodecId = 0, bool useDefaults = true);
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
};