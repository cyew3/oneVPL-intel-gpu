#pragma once

#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"
#include <map>

class tsVideoDecoder : public tsSession, public tsSurfacePool
{
public:
    bool                        m_default;
    bool                        m_initialized;
    bool                        m_loaded;
    bool                        m_par_set;
    bool                        m_use_memid;
    bool                        m_update_bs;
    bool                        m_sw_fallback;
    tsExtBufType<mfxVideoParam> m_par;
    tsExtBufType<mfxBitstream>  m_bitstream;
    mfxFrameAllocRequest        m_request;
    mfxVideoParam*              m_pPar;
    mfxVideoParam*              m_pParOut;
    mfxBitstream*               m_pBitstream;
    mfxFrameAllocRequest*       m_pRequest;
    mfxSyncPoint*               m_pSyncPoint;
    mfxFrameSurface1*           m_pSurf;
    mfxFrameSurface1*           m_pSurfOut;
    tsSurfaceProcessor*         m_surf_processor;
    tsBitstreamProcessor*       m_bs_processor;
    mfxPluginUID*               m_uid;
    std::map<mfxSyncPoint, mfxFrameSurface1*> m_surf_out;

    tsVideoDecoder(mfxU32 CodecId = 0, bool useDefaults = true, mfxU32 id = 0);
    ~tsVideoDecoder();
    
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

    mfxStatus AllocSurfaces();
    
    mfxStatus DecodeHeader();
    mfxStatus DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par);

    mfxStatus DecodeFrameAsync();
    mfxStatus DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);
    
    mfxStatus SyncOperation();
    mfxStatus SyncOperation(mfxSyncPoint syncp);
    mfxStatus SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

    mfxStatus GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload);

    mfxStatus DecodeFrames(mfxU32 n, bool check=false);

    mfxStatus Load();
    mfxStatus UnLoad();

    void SetPar4_DecodeFrameAsync();
};
