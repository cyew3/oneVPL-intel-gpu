#include "mfx_video_core.h"

// CORE interface functions
mfxStatus MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator)
{
    Log::WriteLog("MFXVideoCORE_SetBufferAllocator");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetBufferAllocator];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxBufferAllocator *allocator)) proc) (session, allocator);

    dump_mfxSession("session", session);
    if(allocator) dump_mfxBufferAllocator("allocator", allocator);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoCORE_SetBufferAllocator End \n\n");
    return status;
}

mfxStatus MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator)
{
    Log::WriteLog("MFXVideoCORE_SetFrameAllocator");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetFrameAllocator];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxFrameAllocator *allocator)) proc) (session, allocator);

    dump_mfxSession("session", session);
    if(allocator) dump_mfxFrameAllocator("allocator", allocator);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoCORE_SetFrameAllocator End \n\n");
    return status;
}

mfxStatus MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl)
{
    Log::WriteLog("MFXVideoCORE_SetHandle");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetHandle];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxHandleType type, mfxHDL hdl)) proc) (session, type, hdl);

    dump_mfxSession("session", session);
    dump_mfxHandleType("type", type);
    dump_mfxHDL("hdl", &hdl);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoCORE_SetHandle End \n\n");
    return status;
}

mfxStatus MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl)
{
    Log::WriteLog("MFXVideoCORE_GetHandle");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoCORE_GetHandle];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxHandleType type, mfxHDL *hdl)) proc) (session, type, hdl);

    dump_mfxSession("session", session);
    dump_mfxHandleType("type", type);
    if(hdl) dump_mfxHDL("hdl", hdl);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoCORE_GetHandle End \n\n");
    return status;
}

mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait)
{
    Log::WriteLog("MFXVideoCORE_SyncOperation");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SyncOperation];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSyncPoint syncp, mfxU32 wait)) proc) (session, syncp, wait);

    dump_mfxSession("session", session);
    dump_mfxSyncPoint("syncp", &syncp);
    dump_mfxU32("wait", wait);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoCORE_SyncOperation End \n\n");
    return status;
}
