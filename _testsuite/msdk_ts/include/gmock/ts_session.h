#pragma once

#include "ts_alloc.h"

class tsSession 
{
public:
    bool                m_initialized;
    mfxSession          m_session;
    mfxIMPL             m_impl;
    mfxVersion          m_version;
    mfxSyncPoint        m_syncpoint;
    tsBufferAllocator   m_buffer_allocator;
    mfxSession*         m_pSession;
    mfxVersion*         m_pVersion;
    frame_allocator*    m_pFrameAllocator;

    tsSession(mfxIMPL impl = g_tsImpl, mfxVersion version = g_tsVersion);
    ~tsSession();
    
    mfxStatus MFXInit();
    mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxSession *session);
    
    mfxStatus MFXClose();
    mfxStatus MFXClose(mfxSession session);

    mfxStatus SyncOperation();
    mfxStatus SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

    mfxStatus SetFrameAllocator();
    mfxStatus SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator);

    mfxStatus MFX_CDECL MFXQueryIMPL(mfxSession session, mfxIMPL *impl);

    mfxStatus Load(mfxSession session, const mfxPluginUID *uid, mfxU32 version);
    mfxStatus UnLoad(mfxSession session, const mfxPluginUID *uid);
    mfxStatus SetHandle(mfxSession session, mfxHandleType type, mfxHDL handle);
};
