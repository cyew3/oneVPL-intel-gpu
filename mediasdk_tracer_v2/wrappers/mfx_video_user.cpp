#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

mfxStatus MFXVideoUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par)
{
    try {
        Log::WriteLog("function: MFXVideoUSER_Register(mfxSession session=" + ToString(session) + ", mfxU32 type=" + ToString(type) + ", mfxPlugin *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoUSER_Register];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxU32("type", type));
        if (par) Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoUSER_Register) proc)(session, type, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoUSER_Register called");

        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxU32("type", type));
        if (par) Log::WriteLog(dump("par", par));

        Log::WriteLog("function: MFXVideoUSER_Register(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoUSER_Unregister(mfxSession session, mfxU32 type)
{
    try {
        Log::WriteLog("function: MFXVideoUSER_Unregister(mfxSession session=" + ToString(session) + ", mfxU32 type=" + ToString(type) + ", mfxPlugin *par=" + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoUSER_Unregister];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxU32("type", type));

        Timer t;
        mfxStatus status = (*(fMFXVideoUSER_Unregister) proc) (session, type);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoUSER_Unregister called");

        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxU32("type", type));

        Log::WriteLog("function: MFXVideoUSER_Unregister(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoUSER_ProcessFrameAsync(mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp)
{
    try {
        Log::WriteLog("function: MFXVideoUSER_ProcessFrameAsync(mfxSession session=, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoUSER_ProcessFrameAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxHDL("in", in));
        Log::WriteLog(dump_mfxU32("in_num", in_num));
        Log::WriteLog(dump_mfxHDL("out", out));
        Log::WriteLog(dump_mfxU32("out_num", out_num));
        Log::WriteLog(dump("syncp", syncp));

        Timer t;
        mfxStatus status = (*(fMFXVideoUSER_ProcessFrameAsync) proc) (session, in, in_num, out, out_num, syncp);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoUSER_ProcessFrameAsync called");

        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump_mfxHDL("in", in));
        Log::WriteLog(dump_mfxU32("in_num", in_num));
        Log::WriteLog(dump_mfxHDL("out", out));
        Log::WriteLog(dump_mfxU32("out_num", out_num));
        Log::WriteLog(dump("syncp", syncp));

        Log::WriteLog("function: MFXVideoUSER_ProcessFrameAsync(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
