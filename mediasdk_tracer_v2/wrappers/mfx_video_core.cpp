#include "mfx_video_core.h"
#include "mfx_structures.h"
#include "../loggers/timer.h"
#include <exception>
#include <iostream>

// CORE interface functions
mfxStatus MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetBufferAllocator];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(allocator) Log::WriteLog(dump_mfxBufferAllocator("allocator", allocator));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxBufferAllocator *allocator)) proc) (session, allocator);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoCORE_SetBufferAllocator called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(allocator) Log::WriteLog(dump_mfxBufferAllocator("allocator", allocator));
        Log::WriteLog("function: MFXVideoCORE_SetBufferAllocator(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetFrameAllocator];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(allocator) Log::WriteLog(dump_mfxFrameAllocator("allocator", allocator));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxFrameAllocator *allocator)) proc) (session, allocator);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoCORE_SetFrameAllocator called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(allocator) Log::WriteLog(dump_mfxFrameAllocator("allocator", allocator));
        Log::WriteLog("function: MFXVideoCORE_SetFrameAllocator(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SetHandle];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxHandleType("type", type));
        Log::WriteLog(dump_mfxHDL("hdl", &hdl));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxHandleType type, mfxHDL hdl)) proc) (session, type, hdl);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoCORE_SetHandle called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxHandleType("type", type));
        Log::WriteLog(dump_mfxHDL("hdl", &hdl));
        Log::WriteLog("function: MFXVideoCORE_SetHandle(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl)
{
    try{
        Log::WriteLog("function: MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_GetHandle];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxHandleType("type", type));
        if(hdl) Log::WriteLog(dump_mfxHDL("hdl", hdl));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxHandleType type, mfxHDL *hdl)) proc) (session, type, hdl);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoCORE_GetHandle called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxHandleType("type", type));
        if(hdl) Log::WriteLog(dump_mfxHDL("hdl", hdl));
        Log::WriteLog("function: MFXVideoCORE_GetHandle(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait)
{
    try{
        TracerSyncPoint *sp = (TracerSyncPoint*)syncp;
        Log::WriteLog("function: MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait) +");
        if(sp) sp->component == ENCODE ? Log::WriteLog("SyncOperation(ENCODE, " + ToString(sp->timer.GetTime()) + " sec)") : (sp->component == DECODE ? Log::WriteLog("SyncOperation(DECODE, " + ToString(sp->timer.GetTime()) + " sec)") : Log::WriteLog("SyncOperation(VPP, "  + ToString(sp->timer.GetTime()) + " sec)"));

        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoCORE_SyncOperation];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSyncPoint("syncp", &syncp));
        Log::WriteLog(dump_mfxU32("wait", wait));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSyncPoint syncp, mfxU32 wait)) proc) (session, sp->syncPoint, wait);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoCORE_SyncOperation called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSyncPoint("syncp", &sp->syncPoint));
        Log::WriteLog(dump_mfxU32("wait", wait));
        Log::WriteLog("function: MFXVideoCORE_SyncOperation(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}
