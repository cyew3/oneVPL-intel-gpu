#pragma once

#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"

class tsVideoVPP : public tsSession
{
public:
    bool                        m_default;
    bool                        m_initialized;
    tsExtBufType<mfxVideoParam> m_par;
    mfxFrameAllocRequest        m_request[2];
    mfxVideoParam*              m_pPar;
    mfxVideoParam*              m_pParOut;
    mfxFrameAllocRequest*       m_pRequest;
    mfxSyncPoint*               m_pSyncPoint;
    mfxFrameSurface1*           m_pSurfIn;
    mfxFrameSurface1*           m_pSurfOut;
    mfxFrameSurface1*           m_pSurfWork;
    tsSurfacePool*              m_pSurfPoolIn;
    tsSurfacePool*              m_pSurfPoolOut;
    tsSurfaceProcessor*         m_surf_in_processor;
    tsSurfaceProcessor*         m_surf_out_processor;

    std::map<mfxSyncPoint, mfxFrameSurface1*> m_surf_out;
    tsSurfacePool               m_spool_in;
    tsSurfacePool               m_spool_out;
    
    tsVideoVPP(bool useDefaults = true);
    ~tsVideoVPP();

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
     
    mfxStatus RunFrameVPPAsync();
    mfxStatus RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp);

    mfxStatus RunFrameVPPAsyncEx();
    mfxStatus RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);
    
    mfxStatus SyncOperation();
    mfxStatus SyncOperation(mfxSyncPoint syncp);
    mfxStatus SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);
    
    mfxStatus ProcessFrames(mfxU32 n);
    mfxStatus ProcessFramesEx(mfxU32 n);
};