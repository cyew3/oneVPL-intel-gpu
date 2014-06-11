#include "mfx_core.h"
#include "../loggers/timer.h"
#include <exception>
#include <iostream>

mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl)
{
    try{
        Log::WriteLog("function: MFXQueryIMPL(mfxSession session=" + ToString(session) + ", mfxIMPL *impl" + ToString(impl) + ") +");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxIMPL("impl", impl));
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXQueryIMPL];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxIMPL("impl", impl));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxIMPL *impl)) proc) (session, impl);


        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXQueryIMPL called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxIMPL("impl", impl));
        Log::WriteLog("function: MFXQueryIMPL(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }

}

mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *version)
{
    try{
        Log::WriteLog("function: MFXQueryVersion(mfxSession session=" + ToString(session) + ", mfxVersion *version=" + ToString(version) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXQueryVersion];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;

        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVersion("version", version));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVersion *version)) proc) (session, version);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXQueryVersion called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVersion("version", version));
        Log::WriteLog("function: MFXQueryVersion(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");

        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

/*
 * API version 1.1 functions
 */
mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session)
{
    try{
        Log::WriteLog("function: MFXJoinSession(mfxSession session=" + ToString(session) + ", mfxSession child_session=" + ToString(child_session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXJoinSession];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSession("child_session", child_session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSession child_session)) proc) (session, child_session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXJoinSession called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSession("child_session", child_session));
        Log::WriteLog("function: MFXJoinSession(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone)
{
    try{
        Log::WriteLog("function: MFXCloneSession(mfxSession session=" + ToString(session) + ", mfxSession *clone=" + ToString(clone) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXCloneSession];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSession("clone", (*clone)));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSession *clone)) proc) (session, clone);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXCloneSession called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSession("clone", (*clone)));
        Log::WriteLog("function: MFXCloneSession(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXDisjoinSession(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXDisjoinSession(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXCloneSession];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXDisjoinSession called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog("function: MFXDisjoinSession(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXSetPriority(mfxSession session, mfxPriority priority)
{
    try{
        Log::WriteLog("function: MFXSetPriority(mfxSession session=" + ToString(session) + ", mfxPriority priority=" + ToString(priority) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXSetPriority];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxPriority("priority", priority));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxPriority priority)) proc) (session, priority);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXSetPriority called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxPriority("priority", priority));
        Log::WriteLog("function: MFXSetPriority(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXGetPriority(mfxSession session, mfxPriority *priority)
{
    try{
        Log::WriteLog("function: MFXGetPriority(mfxSession session=" + ToString(session) + ", mfxPriority *priority=" + ToString(priority) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXGetPriority];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxPriority("priority", *priority));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxPriority *priority)) proc) (session, priority);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXGetPriority called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxPriority("priority", *priority));
        Log::WriteLog("function: MFXGetPriority(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}
