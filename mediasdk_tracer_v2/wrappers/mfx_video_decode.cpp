#include "mfx_video_decode.h"

// DECODE interface functions
mfxStatus MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    Log::WriteLog("MFXVideoDECODE_Query");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Query];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *in, mfxVideoParam *out)) proc) (session, in, out);

    dump_mfxSession("session", session);
    if(in) dump_mfxVideoParam("in", in);
    if(out) dump_mfxVideoParam("out", out);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_Query End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoDECODE_DecodeHeader");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_DecodeHeader];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxBitstream *bs, mfxVideoParam *par)) proc) (session, bs, par);

    dump_mfxSession("session", session);
    if(bs) dump_mfxBitstream("bs", bs);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_DecodeHeader End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    Log::WriteLog("MFXVideoDECODE_QueryIOSurf");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_QueryIOSurf];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)) proc) (session, par, request);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    if(request) dump_mfxFrameAllocRequest("request", request);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_QueryIOSurf End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoDECODE_Init");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Init];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_Init End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoDECODE_Reset");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Reset];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_Reset End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_Close(mfxSession session)
{
    Log::WriteLog("MFXVideoDECODE_Close");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_Close];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session)) proc) (session);

    dump_mfxSession("session", session);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_Close End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    Log::WriteLog("MFXVideoDECODE_GetVideoParam");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetVideoParam];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxVideoParam *par)) proc) (session, par);

    dump_mfxSession("session", session);
    if(par) dump_mfxVideoParam("par", par);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_GetVideoParam End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat)
{
    Log::WriteLog("MFXVideoDECODE_GetDecodeStat");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetDecodeStat];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxDecodeStat *stat)) proc) (session, stat);

    dump_mfxSession("session", session);
    if(stat) dump_mfxDecodeStat("stat", stat);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_GetDecodeStat End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode)
{
    Log::WriteLog("MFXVideoDECODE_SetSkipMode");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_SetSkipMode];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxSkipMode mode)) proc) (session, mode);

    dump_mfxSession("session", session);
    dump_mfxSkipMode("mode", mode);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_SetSkipMode End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload)
{
    Log::WriteLog("MFXVideoDECODE_GetPayload");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_GetPayload];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxU64 *ts, mfxPayload *payload)) proc) (session, ts, payload);

    dump_mfxSession("session", session);
    if(ts) dump_mfxU64("ts", (*ts));
    if(payload) dump_mfxPayload("payload", payload);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_GetPayload End \n\n");
    return status;
}

mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    Log::WriteLog("MFXVideoDECODE_DecodeFrameAsync");
    mfxLoader *loader = (mfxLoader*) session;

    if (!loader) return MFX_ERR_INVALID_HANDLE;

    mfxFunctionPointer proc = loader->table[eMFXVideoDECODE_DecodeFrameAsync];
    if (!proc) return MFX_ERR_INVALID_HANDLE;

    session = loader->session;

    mfxStatus status = (*(mfxStatus (MFX_CDECL*) (mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)) proc) (session, bs, surface_work, surface_out, syncp);

    dump_mfxSession("session", session);
    if(bs) dump_mfxBitstream("bs", bs);
    if(surface_work)dump_mfxFrameSurface1("surface_work", surface_work);
    if(surface_out && (*surface_out))
        dump_mfxFrameSurface1("surface_out", (*surface_out));
    if(syncp) dump_mfxSyncPoint("syncp", syncp);
    dump_mfxStatus("status ", status);
    Log::WriteLog("MFXVideoDECODE_DecodeFrameAsync End \n\n");
    return status;
}
