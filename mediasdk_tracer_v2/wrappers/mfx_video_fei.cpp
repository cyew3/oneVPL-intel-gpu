#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"


mfxStatus MFXVideoPAK_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoPAK_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam* in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*)session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoPAK_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (in) Log::WriteLog(context.dump("in", *in));
        if (out) Log::WriteLog(context.dump("out", *out));

        Timer t;
        mfxStatus status = (*(fMFXVideoPAK_Query)proc)(session, in, out);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoPAK_Query called");

        Log::WriteLog(context.dump("session", session));
        if (in) Log::WriteLog(context.dump("in", *in));
        if (out) Log::WriteLog(context.dump("out", *out));

        Log::WriteLog("function: MFXVideoPAK_Query(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoPAK_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoPAK_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*)session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoPAK_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));
        if (request) Log::WriteLog(context.dump("request", *request));

        Timer t;
        mfxStatus status = (*(fMFXVideoPAK_QueryIOSurf)proc)(session, par, request);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoPAK_QueryIOSurf called");

        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));
        if (request) Log::WriteLog(context.dump("request", *request));

        Log::WriteLog("function: MFXVideoPAK_QueryIOSurf(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoPAK_Init(mfxSession session, mfxVideoParam *par)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoPAK_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*)session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoPAK_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoPAK_Init)proc)(session, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoPAK_Init called");

        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Log::WriteLog("function: MFXVideoPAK_Init(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoPAK_Reset(mfxSession session, mfxVideoParam *par)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoPAK_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*)session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoPAK_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoPAK_Reset)proc)(session, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoPAK_Reset called");

        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Log::WriteLog("function: MFXVideoPAK_Reset(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoPAK_Close(mfxSession session)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoPAK_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*)session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoPAK_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));

        Timer t;
        mfxStatus status = (*(fMFXVideoPAK_Close)proc)(session);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoPAK_Close called");

        Log::WriteLog(context.dump("session", session));

        Log::WriteLog("function: MFXVideoPAK_Close(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoPAK_ProcessFrameAsync(mfxSession session, mfxPAKInput *in, mfxPAKOutput *out, mfxSyncPoint *syncp)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoPAK_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp) +");
        mfxLoader *loader = (mfxLoader*)session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoPAK_ProcessFrameAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (in) Log::WriteLog(context.dump("in", *in));
        if (out) Log::WriteLog(context.dump("out", *out));
        Log::WriteLog(context.dump("syncp", *syncp));

        Timer t;
        mfxStatus status = (*(fMFXVideoPAK_ProcessFrameAsync)proc)(session, in, out, syncp);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoPAK_ProcessFrameAsync called");

        Log::WriteLog(context.dump("session", session));
        if (in) Log::WriteLog(context.dump("in", *in));
        if (out) Log::WriteLog(context.dump("out", *out));
        Log::WriteLog(context.dump("syncp", *syncp));

        Log::WriteLog("function: MFXVideoPAK_ProcessFrameAsync(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
