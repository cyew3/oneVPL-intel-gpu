#include "mfx_core.h"
#include "../loggers/timer.h"
#include <exception>
#include <iostream>

mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl)
{
    try{
        Log::WriteLog("function: MFXQueryIMPL(mfxSession session, mfxIMPL *impl) +");
        Log::WriteLog(dump_mfxSession("session", session));
        if(impl) Log::WriteLog(dump_mfxIMPL("impl", impl));
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXQueryIMPL];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(impl) Log::WriteLog(dump_mfxIMPL("impl", impl));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxIMPL *impl)) proc) (session, impl);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXQueryIMPL called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(impl) Log::WriteLog(dump_mfxIMPL("impl", impl));
        Log::WriteLog("function: MFXQueryIMPL(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }

}

mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *version)
{
    try{
        Log::WriteLog("function: MFXQueryVersion(mfxSession session, mfxVersion *version) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXQueryVersion];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;

        Log::WriteLog(dump_mfxSession("session", session));
        if(version) Log::WriteLog(dump_mfxVersion("version", *version));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVersion *version)) proc) (session, version);
        std::string elapsed = ToString(t.GetTime());

        Log::WriteLog("MFXQueryVersion called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(version) Log::WriteLog(dump_mfxVersion("version", *version));
        Log::WriteLog("function: MFXQueryVersion(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");

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
        Log::WriteLog("function: MFXJoinSession(mfxSession session, mfxSession child_session) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXJoinSession];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSession("child_session", child_session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSession child_session)) proc) (session, child_session);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXJoinSession called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSession("child_session", child_session));
        Log::WriteLog("function: MFXJoinSession(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone)
{
    try{
        Log::WriteLog("function: MFXCloneSession(mfxSession session, mfxSession *clone) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXCloneSession];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(clone) Log::WriteLog(dump_mfxSession("clone", (*clone)));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSession *clone)) proc) (session, clone);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXCloneSession called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(clone) Log::WriteLog(dump_mfxSession("clone", (*clone)));
        Log::WriteLog("function: MFXCloneSession(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXDisjoinSession(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXDisjoinSession(mfxSession session) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXCloneSession];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXDisjoinSession called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog("function: MFXDisjoinSession(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXSetPriority(mfxSession session, mfxPriority priority)
{
    try{
        Log::WriteLog("function: MFXSetPriority(mfxSession session, mfxPriority priority) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXSetPriority];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxPriority("priority", priority));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxPriority priority)) proc) (session, priority);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXSetPriority called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxPriority("priority", priority));
        Log::WriteLog("function: MFXSetPriority(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXGetPriority(mfxSession session, mfxPriority *priority)
{
    try{
        Log::WriteLog("function: MFXGetPriority(mfxSession session, mfxPriority *priority) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXGetPriority];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(priority) Log::WriteLog(dump_mfxPriority("priority", *priority));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxPriority *priority)) proc) (session, priority);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXGetPriority called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(priority) Log::WriteLog(dump_mfxPriority("priority", *priority));
        Log::WriteLog("function: MFXGetPriority(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}
