#include "mfx_video_decode.h"
#include "mfx_structures.h"
#include "../loggers/timer.h"
#include <exception>
#include <iostream>

// DECODE interface functions
mfxStatus MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(in) Log::WriteLog(dump_mfxVideoParam("in", in));
        if(out) Log::WriteLog(dump_mfxVideoParam("out", out));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *in, mfxVideoParam *out)) proc) (session, in, out);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_Query called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(in) Log::WriteLog(dump_mfxVideoParam("in", in));
        if(out) Log::WriteLog(dump_mfxVideoParam("out", out));
        Log::WriteLog("function: MFXVideoDECODE_Query(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_DecodeHeader];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(bs) Log::WriteLog(dump_mfxBitstream("bs", bs));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxBitstream *bs, mfxVideoParam *par)) proc) (session, bs, par);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_DecodeHeader called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(bs) Log::WriteLog(dump_mfxBitstream("bs", bs));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoDECODE_DecodeHeader(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        if(request) Log::WriteLog(dump_mfxFrameAllocRequest("request", request));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)) proc) (session, par, request);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_QueryIOSurf called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        if(request) Log::WriteLog(dump_mfxFrameAllocRequest("request", request));
        Log::WriteLog("function: MFXVideoDECODE_QueryIOSurf(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_Init called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoDECODE_Init(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_Reset called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoDECODE_Reset(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_Close(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_Close(mfxSession session) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_Close called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog("function: MFXVideoDECODE_Close(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetVideoParam];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_GetVideoParam called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoDECODE_GetVideoParam(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetDecodeStat];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(stat) Log::WriteLog(dump_mfxDecodeStat("stat", stat));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxDecodeStat *stat)) proc) (session, stat);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_GetDecodeStat called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(stat) Log::WriteLog(dump_mfxDecodeStat("stat", stat));
        Log::WriteLog("function: MFXVideoDECODE_GetDecodeStat(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_SetSkipMode];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSkipMode("mode", mode));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSkipMode mode)) proc) (session, mode);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_SetSkipMode called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxSkipMode("mode", mode));
        Log::WriteLog("function: MFXVideoDECODE_SetSkipMode(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload)
{
    try{
        Log::WriteLog("function: MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetPayload];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(ts) Log::WriteLog(dump_mfxU64("ts", (*ts)));
        if(payload) Log::WriteLog(dump_mfxPayload("payload", payload));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxU64 *ts, mfxPayload *payload)) proc) (session, ts, payload);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoDECODE_GetPayload called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(ts) Log::WriteLog(dump_mfxU64("ts", (*ts)));
        if(payload) Log::WriteLog(dump_mfxPayload("payload", payload));
        Log::WriteLog("function: MFXVideoDECODE_GetPayload(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    try{
        TracerSyncPoint * sp = new TracerSyncPoint();
        sp->syncPoint = (*syncp);
        sp->component = DECODE;

        Log::WriteLog("function: MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_DecodeFrameAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(bs) Log::WriteLog(dump_mfxBitstream("bs", bs));
        if(surface_work) Log::WriteLog(dump_mfxFrameSurface1("surface_work", surface_work));
        if(surface_out && (*surface_out))
            Log::WriteLog(dump_mfxFrameSurface1("surface_out", (*surface_out)));
        if(syncp) Log::WriteLog(dump_mfxSyncPoint("syncp", syncp));

        sp->timer.Restart();
        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)) proc) (session, bs, surface_work, surface_out, &sp->syncPoint);
        std::string elapsed = ToString(t.GetTime());

        *syncp = (mfxSyncPoint)sp;

        Log::WriteLog("MFXVideoDECODE_DecodeFrameAsync called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(bs) Log::WriteLog(dump_mfxBitstream("bs", bs));
        if(surface_work) Log::WriteLog(dump_mfxFrameSurface1("surface_work", surface_work));
        if(surface_out && (*surface_out))
            Log::WriteLog(dump_mfxFrameSurface1("surface_out", (*surface_out)));
        if(&sp->syncPoint) Log::WriteLog(dump_mfxSyncPoint("syncp", &sp->syncPoint));
        Log::WriteLog("function: MFXVideoDECODE_DecodeFrameAsync(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}
