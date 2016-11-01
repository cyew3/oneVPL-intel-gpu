/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"

class tsVideoVPP : virtual public tsSession
{
public:
    bool                        m_default;
    bool                        m_initialized;
    bool                        m_loaded;
    bool                        m_use_memid;
    tsExtBufType<mfxVideoParam> m_par;
    mfxFrameAllocRequest        m_request[2];
    mfxVPPStat                  m_stat;
    mfxVideoParam*              m_pPar;
    mfxVideoParam*              m_pParOut;
    mfxFrameAllocRequest*       m_pRequest;
    mfxSyncPoint*               m_pSyncPoint;
    mfxFrameSurface1*           m_pSurfIn;
    mfxFrameSurface1*           m_pSurfOut;
    mfxFrameSurface1*           m_pSurfWork;
    mfxVPPStat*                 m_pStat;
    tsSurfacePool*              m_pSurfPoolIn;
    tsSurfacePool*              m_pSurfPoolOut;
    tsSurfaceProcessor*         m_surf_in_processor;
    tsSurfaceProcessor*         m_surf_out_processor;
    mfxPluginUID*               m_uid;

    std::map<mfxSyncPoint, mfxFrameSurface1*> m_surf_out;
    tsSurfacePool               m_spool_in;
    tsSurfacePool               m_spool_out;

    tsVideoVPP(bool useDefaults = true, mfxU32 id = 0);
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

    mfxStatus GetVPPStat();
    mfxStatus GetVPPStat(mfxSession session, mfxVPPStat *stat);

    mfxStatus AllocSurfaces();

    mfxStatus CreateAllocators();
    mfxStatus SetFrameAllocator();
    mfxStatus SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator);

    mfxStatus RunFrameVPPAsync();
    virtual mfxStatus RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp);

    mfxStatus RunFrameVPPAsyncEx();
    virtual mfxStatus RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);
    
    mfxStatus SyncOperation();
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp);
    mfxStatus SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);
    
    mfxStatus ProcessFrames(mfxU32 n);
    mfxStatus ProcessFramesEx(mfxU32 n);

    mfxStatus Load();

    mfxStatus SetHandle();
    mfxStatus SetHandle(mfxSession session, mfxHandleType type, mfxHDL handle);
};
