#include "mfx_video_vpp.h"

// VPP interface functions
mfxStatus MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    Log::WriteLog("MFXVideoVPP_Query");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Query];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *in, mfxVideoParam *out)) proc) (session, in, out);

    dump_mfxSession("session", session);
    if(in) dump_mfxVideoParam("in", in);
    if(out) dump_mfxVideoParam("out", out);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_Query End \n\n");
    return status;
}

mfxStatus MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    Log::WriteLog("MFXVideoVPP_QueryIOSurf");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_QueryIOSurf];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)) proc) (session, par, request);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    if(request) dump_mfxFrameAllocRequest("request", request);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_QueryIOSurf End \n\n");
    return status;
}

mfxStatus MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoVPP_Init");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Init];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_Init End \n\n");
    return status;
}

mfxStatus MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoVPP_Reset");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Reset];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_Reset End \n\n");
    return status;
}

mfxStatus MFXVideoVPP_Close(mfxSession session)
{
    Log::WriteLog("MFXVideoVPP_Close");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_Close];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);

    dump_mfxSession("session", session);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_Close End \n\n");
    return status;
}

mfxStatus MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoVPP_GetVideoParam");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_GetVideoParam];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_GetVideoParam End \n\n");
    return status;
}

mfxStatus MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat)
{
    Log::WriteLog("MFXVideoVPP_GetVPPStat");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_GetVPPStat];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVPPStat *stat)) proc) (session, stat);

    dump_mfxSession("session", session);
    if(stat) dump_mfxVPPStat("stat", stat);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_GetVPPStat End \n\n");
    return status;
}

mfxStatus MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
{
    Log::WriteLog("MFXVideoVPP_RunFrameVPPAsync");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoVPP_RunFrameVPPAsync];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)) proc) (session, in, out, aux, syncp);

    dump_mfxSession("session", session);
    if(in) dump_mfxFrameSurface1("in", in);
    if(out) dump_mfxFrameSurface1("out", out);
    if(aux) dump_mfxExtVppAuxData("aux", aux);
    if(syncp) dump_mfxSyncPoint("syncp", syncp);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoVPP_RunFrameVPPAsync End \n\n");
    return status;
}
