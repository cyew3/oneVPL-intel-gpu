#include "mfx_video_vpp.h"
#include "mfx_structures.h"
#include "../loggers/timer.h"
#include <exception>
#include <iostream>

// VPP interface functions
mfxStatus MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Query];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(in) Log::WriteLog(dump_mfxVideoParam("in", in));
        if(out) Log::WriteLog(dump_mfxVideoParam("out", out));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *in, mfxVideoParam *out)) proc) (session, in, out);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoVPP_Query called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(in) Log::WriteLog(dump_mfxVideoParam("in", in));
        if(out) Log::WriteLog(dump_mfxVideoParam("out", out));
        Log::WriteLog("function: MFXVideoVPP_Query(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_QueryIOSurf];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        if(request) Log::WriteLog(dump_mfxFrameAllocRequest("request", request));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)) proc) (session, par, request);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoVPP_QueryIOSurf called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        if(request) Log::WriteLog(dump_mfxFrameAllocRequest("request", request));
        Log::WriteLog("function: MFXVideoVPP_QueryIOSurf(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Init];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoVPP_Init called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoVPP_Init(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Reset];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoVPP_Reset called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoVPP_Reset(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_Close(mfxSession session)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_Close(mfxSession session) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Close];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoVPP_Close called");
        Log::WriteLog(dump_mfxSession("session", session));
        Log::WriteLog("function: MFXVideoVPP_Close(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_GetVideoParam];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoVPP_GetVideoParam called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(par) Log::WriteLog(dump_mfxVideoParam("par", par));
        Log::WriteLog("function: MFXVideoVPP_GetVideoParam(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}

mfxStatus MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat)
{
    try{
        Log::WriteLog("function: MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_GetVPPStat];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(stat) Log::WriteLog(dump_mfxVPPStat("stat", stat));

        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVPPStat *stat)) proc) (session, stat);
        std::string elapsed = ToString(t.GetTime());
        Log::WriteLog("MFXVideoVPP_GetVPPStat called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(stat) Log::WriteLog(dump_mfxVPPStat("stat", stat));
        Log::WriteLog("function: MFXVideoVPP_GetVPPStat(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
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

        Log::WriteLog("function: MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp) +");
        mfxLoader *loader = (mfxLoader*) session;

        if (!loader) return MFX_ERR_INVALID_HANDLE;

        mfxFunctionPointer proc = loader->table[eMFXVideoVPP_RunFrameVPPAsync];
        if (!proc) return MFX_ERR_INVALID_HANDLE;

        session = loader->session;
        Log::WriteLog(dump_mfxSession("session", session));
        if(in) Log::WriteLog(dump_mfxFrameSurface1("in", in));
        if(out) Log::WriteLog(dump_mfxFrameSurface1("out", out));
        if(aux) Log::WriteLog(dump_mfxExtVppAuxData("aux", aux));
        if(syncp) Log::WriteLog(dump_mfxSyncPoint("syncp", syncp));

        sp->timer.Restart();
        Timer t;
        mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)) proc) (session, in, out, aux, &sp->syncPoint);
        std::string elapsed = ToString(t.GetTime());

        *syncp = (mfxSyncPoint)sp;

        Log::WriteLog("MFXVideoVPP_RunFrameVPPAsync called");
        Log::WriteLog(dump_mfxSession("session", session));
        if(in) Log::WriteLog(dump_mfxFrameSurface1("in", in));
        if(out) Log::WriteLog(dump_mfxFrameSurface1("out", out));
        if(aux) Log::WriteLog(dump_mfxExtVppAuxData("aux", aux));
        if(&sp->syncPoint) Log::WriteLog(dump_mfxSyncPoint("syncp", &sp->syncPoint));
        Log::WriteLog("function: MFXVideoVPP_RunFrameVPPAsync(" + elapsed + " sec, " + dump_mfxStatus("status", status) + ") - \n\n");
        return status;
    }
    catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << '\n';
    }
}
