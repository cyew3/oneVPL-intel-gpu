#include "mfx_core.h"

mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl)
{
    Log::WriteLog("MFXQueryIMPL Start");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXQueryIMPL];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;
    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxIMPL *impl)) proc) (session, impl);

    dump_mfxSession("session", session);
    if(impl) dump_mfxIMPL("impl", impl);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXQueryIMPL End \n\n");
    return status;
}

mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *version)
{
    Log::WriteLog("MFXQueryVersion");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXQueryVersion];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;
    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVersion *version)) proc) (session, version);

    dump_mfxSession("session", session);
    if(version) dump_mfxVersion("version", *version);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXQueryVersion End \n\n");

    return status;
}

/*
 * API version 1.1 functions
 */
mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session)
{
    Log::WriteLog("MFXJoinSession");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXJoinSession];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSession child_session)) proc) (session, child_session);

    dump_mfxSession("session", session);
    dump_mfxSession("child_session", child_session);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXJoinSession End \n\n");
    return status;
}

mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone)
{
    Log::WriteLog("MFXCloneSession");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXCloneSession];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSession *clone)) proc) (session, clone);

    dump_mfxSession("session", session);
    if(clone) dump_mfxSession("clone", (*clone));
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXCloneSession End \n\n");
    return status;
}

mfxStatus MFXDisjoinSession(mfxSession session)
{
    Log::WriteLog("MFXDisjoinSession");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXCloneSession];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);

    dump_mfxSession("session", session);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXDisjoinSession End \n\n");
    return status;
}

mfxStatus MFXSetPriority(mfxSession session, mfxPriority priority)
{
    Log::WriteLog("MFXSetPriority");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXSetPriority];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxPriority priority)) proc) (session, priority);

    dump_mfxSession("session", session);
    dump_mfxPriority("priority", priority);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXSetPriority End \n\n");
    return status;
}

mfxStatus MFXGetPriority(mfxSession session, mfxPriority *priority)
{
    Log::WriteLog("MFXGetPriority");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXGetPriority];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxPriority *priority)) proc) (session, priority);

    dump_mfxSession("session", session);
    if(priority) dump_mfxPriority("priority", *priority);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXGetPriority End \n\n");
    return status;
}
