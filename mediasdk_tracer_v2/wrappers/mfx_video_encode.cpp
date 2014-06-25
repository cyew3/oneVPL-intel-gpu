#include <exception>
#include <iostream>

#include "../loggers/timer.h"
#include "../tracer/functions_table.h"
#include "mfx_structures.h"

// ENCODE interface functions
mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam *in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump/*_mfxVideoParam*/("in", in));
        Log::WriteLog(dump("out", out));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Query) proc) (session, in, out);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Query called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump/*_mfxVideoParam*/("in", in));
        Log::WriteLog(dump("out", out));
        Log::WriteLog("function: MFXVideoENCODE_Query(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog(dump("request", request));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_QueryIOSurf) proc) (session, par, request);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_QueryIOSurf called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog(dump("request", request));
        Log::WriteLog("function: MFXVideoENCODE_QueryIOSurf(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Init) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Init called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog("function: MFXVideoENCODE_Init(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");

        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Reset) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Reset called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog("function: MFXVideoENCODE_Reset(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_Close(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_Close) proc) (session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_Close called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog("function: MFXVideoENCODE_Close(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_GetVideoParam(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetVideoParam];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_GetVideoParam) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_GetVideoParam called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("par", par));
        Log::WriteLog("function: MFXVideoENCODE_GetVideoParam(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat)
{
    try{
        Log::WriteLog("function: MFXVideoENCODE_GetEncodeStat(mfxSession session=" + ToString(session) + ", mfxEncodeStat *stat=" + ToString(stat) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetEncodeStat];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("stat", stat));

        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_GetEncodeStat) proc) (session, stat);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoENCODE_GetEncodeStat called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("stat", stat));
        Log::WriteLog("function: MFXVideoENCODE_GetEncodeStat(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}

mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    try{
        TracerSyncPoint * sp = new TracerSyncPoint();
        sp->syncPoint = (*syncp);
        sp->component = ENCODE;

        Log::WriteLog("function: MFXVideoENCODE_EncodeFrameAsync(mfxSession session=" + ToString(session) + ", mfxEncodeCtrl *ctrl=" + ToString(ctrl) + ", mfxFrameSurface1 *surface=" + ToString(surface) + ", mfxBitstream *bs=" + ToString(bs) + ", mfxSyncPoint *syncp=" + ToString(syncp) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_EncodeFrameAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("ctrl", ctrl));
        Log::WriteLog(dump("surface", surface));
        Log::WriteLog(dump("bs", bs));
        Log::WriteLog(dump("syncp", syncp));

        sp->timer.Restart();
        Timer t;
        mfxStatus status = (*(fMFXVideoENCODE_EncodeFrameAsync) proc) (session, ctrl, surface, bs, &sp->syncPoint);
        std::string elapsed = TimeToString(t.GetTime());

        *syncp = (mfxSyncPoint)sp;

        Log::WriteLog(">> MFXVideoENCODE_EncodeFrameAsync called");
        Log::WriteLog(dump("session", session));
        Log::WriteLog(dump("ctrl", ctrl));
        Log::WriteLog(dump("surface", surface));
        Log::WriteLog(dump("bs", bs));
        Log::WriteLog(dump("syncp", &sp->syncPoint));
        Log::WriteLog("function: MFXVideoENCODE_EncodeFrameAsync(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
        return MFX_ERR_ABORTED;
    }
}
