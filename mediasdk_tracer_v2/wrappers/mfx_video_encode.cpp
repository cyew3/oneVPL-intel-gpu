#include "mfx_video_encode.h"

// ENCODE interface functions
mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    Log::WriteLog("MFXVideoENCODE_Query");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Query];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *in, mfxVideoParam *out)) proc) (session, in, out);

    dump_mfxSession("session", session);
    if(in) dump_mfxVideoParam("in", in);
    if(out) dump_mfxVideoParam("out", out);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_Query End \n\n");
    return status;
}

mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    Log::WriteLog("MFXVideoENCODE_QueryIOSurf");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_QueryIOSurf];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)) proc) (session, par, request);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    if(request) dump_mfxFrameAllocRequest("request", request);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_QueryIOSurf End \n\n");
    return status;
}

mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoENCODE_Init");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Init];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_Init End \n\n");
    return status;
}

mfxStatus MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoENCODE_Reset");

    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Reset];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_Reset End \n\n");
    return status;
}

mfxStatus MFXVideoENCODE_Close(mfxSession session)
{
    Log::WriteLog("MFXVideoENCODE_Close");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_Close];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);

    dump_mfxSession("session", session);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_Close End \n\n");
    return status;
}

mfxStatus MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoENCODE_GetVideoParam");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetVideoParam];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_GetVideoParam End \n\n");
    return status;
}

mfxStatus MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat)
{
    Log::WriteLog("MFXVideoENCODE_GetEncodeStat");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_GetEncodeStat];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxEncodeStat *stat)) proc) (session, stat);

    dump_mfxSession("session", session);
    if(stat) dump_mfxEncodeStat("stat", stat);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_GetEncodeStat End \n\n");
    return status;
}

mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    Log::WriteLog("MFXVideoENCODE_EncodeFrameAsync");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoENCODE_EncodeFrameAsync];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)) proc) (session, ctrl, surface, bs, syncp);

    dump_mfxSession("session", session);
    if(ctrl) dump_mfxEncodeCtrl("ctrl", ctrl);
    if(surface) dump_mfxFrameSurface1("surface", surface);
    if(bs) dump_mfxBitstream("bs", bs);
    if(syncp) dump_mfxSyncPoint("syncp", syncp);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoENCODE_EncodeFrameAsync End \n\n");
    return status;
}
