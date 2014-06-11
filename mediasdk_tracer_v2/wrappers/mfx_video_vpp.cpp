#include "mfx_video_vpp.h"
#include "mfx_structures.h"
#include "../loggers/timer.h"
#include <exception>
#include <iostream>

// VPP interface functions
mfxStatus MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Query(mfxSession session=" + ToString(session) + ", mfxVideoParam *in=" + ToString(in) + ", mfxVideoParam *out=" + ToString(out) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("in", in));
        Log::WriteLog(dump_mfxVideoParam("out", out));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *in, mfxVideoParam *out)) proc) (session, in, out);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoVPP_Query called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("in", in));
        Log::WriteLog(dump_mfxVideoParam("out", out));
        Log::WriteLog("function: MFXVideoVPP_Query(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_QueryIOSurf(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ", mfxFrameAllocRequest *request=" + ToString(request) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog(dump_mfxFrameAllocRequest("request", request));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)) proc) (session, par, request);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoVPP_QueryIOSurf called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog(dump_mfxFrameAllocRequest("request", request));
        Log::WriteLog("function: MFXVideoVPP_QueryIOSurf(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Init(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoVPP_Init called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoVPP_Init(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Reset(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoVPP_Reset called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoVPP_Reset(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_Close(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Close(mfxSession session=" + ToString(session) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoVPP_Close called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog("function: MFXVideoVPP_Close(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_GetVideoParam(mfxSession session=" + ToString(session) + ", mfxVideoParam *par=" + ToString(par) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_GetVideoParam];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoVPP_GetVideoParam called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoVPP_GetVideoParam(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_GetVPPStat(mfxSession session=" + ToString(session) + ", mfxVPPStat *stat=" + ToString(stat) + ") +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_GetVPPStat];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVPPStat("stat", stat));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVPPStat *stat)) proc) (session, stat);
        std::string elapsed = TimeToString(t.GetTime());
        Log::WriteLog(">> MFXVideoVPP_GetVPPStat called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxVPPStat("stat", stat));
        Log::WriteLog("function: MFXVideoVPP_GetVPPStat(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
{
    try{
        TracerSyncPoint * sp = new TracerSyncPoint();
        sp->syncPoint = (*syncp);
        sp->component = VPP;

        Log::WriteLog("function: MFXVideoVPP_RunFrameVPPAsync(mfxSession session=" + ToString(session) + ", mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_RunFrameVPPAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxFrameSurface1("in", in));
        Log::WriteLog(dump_mfxFrameSurface1("out", out));
        Log::WriteLog(dump_mfxExtVppAuxData("aux", aux));
        Log::WriteLog(dump_mfxSyncPoint("syncp", syncp));

        sp->timer.Restart();
        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)) proc) (session, in, out, aux, &sp->syncPoint);
        std::string elapsed = TimeToString(t.GetTime());

        *syncp = (mfxSyncPoint)sp;

        Log::WriteLog(">> MFXVideoVPP_RunFrameVPPAsync called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog(dump_mfxFrameSurface1("in", in));
        Log::WriteLog(dump_mfxFrameSurface1("out", out));
        Log::WriteLog(dump_mfxExtVppAuxData("aux", aux));
        Log::WriteLog(dump_mfxSyncPoint("syncp", &sp->syncPoint));
        Log::WriteLog("function: MFXVideoVPP_RunFrameVPPAsync(" + elapsed + ", " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}
