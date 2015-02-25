#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

mfxStatus MFXVideoENC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENC_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam* in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Query_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (in) Log::WriteLog(context.dump("in", *in));
        if (out) Log::WriteLog(context.dump("out", *out));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Query) proc)(session, in, out);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Query called");

        Log::WriteLog(context.dump("session", session));
        if (in) Log::WriteLog(context.dump("in", *in));
        if (out) Log::WriteLog(context.dump("out", *out));

        Log::WriteLog("function: MFXVideoENC_Query(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENC_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_QueryIOSurf_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));
        if (request) Log::WriteLog(context.dump("request", *request));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_QueryIOSurf) proc)(session, par, request);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_QueryIOSurf called");

        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));
        if (request) Log::WriteLog(context.dump("request", *request));

        Log::WriteLog("function: MFXVideoENC_QueryIOSurf(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_Init(mfxSession session, mfxVideoParam *par)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENC_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Init_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Init) proc)(session, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Init called");

        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Log::WriteLog("function: MFXVideoENC_Init(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_Reset(mfxSession session, mfxVideoParam *par)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENC_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam* par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Reset_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Reset) proc)(session, par);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Reset called");

        Log::WriteLog(context.dump("session", session));
        if (par) Log::WriteLog(context.dump("par", *par));

        Log::WriteLog("function: MFXVideoENC_Reset(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_Close(mfxSession session)
{
    try {
        DumpContext context;
        context.context = DUMPCONTEXT_MFX;
        Log::WriteLog("function: MFXVideoENC_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENC_Close_tracer];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(context.dump("session", session));

        Timer t;
        mfxStatus status = (*(fMFXVideoENC_Close) proc)(session);
        std::string elapsed = TimeToString(t.GetTime());

        Log::WriteLog(">> MFXVideoENC_Close called");

        Log::WriteLog(context.dump("session", session));

        Log::WriteLog("function: MFXVideoENC_Close(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp)
{
    try {
        if (Log::GetLogLevel() >= LOG_LEVEL_FULL) // call with logging
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;
            Log::WriteLog("function: MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp) +");
            mfxLoader *loader = (mfxLoader*) session;

            if (!loader) return MFX_ERR_INVALID_HANDLE;

            mfxFunctionPointer proc = loader->table[eMFXVideoENC_ProcessFrameAsync_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            Log::WriteLog(context.dump("session", session));
            if (in) Log::WriteLog(context.dump("in", *in));
            if (out) Log::WriteLog(context.dump("out", *out));
            Log::WriteLog(context.dump("syncp", *syncp));
        
            Timer t;
            mfxStatus status = (*(fMFXVideoENC_ProcessFrameAsync) proc)(session, in, out, syncp);
            std::string elapsed = TimeToString(t.GetTime());
        
            Log::WriteLog(">> MFXVideoENC_ProcessFrameAsync called");

            Log::WriteLog(context.dump("session", session));
            if (in) Log::WriteLog(context.dump("in", *in));
            if (out) Log::WriteLog(context.dump("out", *out));
            Log::WriteLog(context.dump("syncp", *syncp));

            Log::WriteLog("function: MFXVideoENC_ProcessFrameAsync(" + elapsed + ", " + context.dump_mfxStatus("status", status) + ") - \n\n");
          
            return status;
        }
        else // call without logging
        {
            DumpContext context;
            context.context = DUMPCONTEXT_MFX;
            
            mfxLoader *loader = (mfxLoader*) session;

            if (!loader) return MFX_ERR_INVALID_HANDLE;

            mfxFunctionPointer proc = loader->table[eMFXVideoENC_ProcessFrameAsync_tracer];
            if (!proc) return MFX_ERR_INVALID_HANDLE;

            session = loader->session;
            
            mfxStatus status = (*(fMFXVideoENC_ProcessFrameAsync) proc)(session, in, out, syncp);
            
            return status;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
