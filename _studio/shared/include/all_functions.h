//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2009 Intel Corporation. All Rights Reserved.
//

// Library Common

FUNCTION(mfxStatus, MFXQueryIMPL, (mfxSession session, mfxIMPL *impl), (session, impl))
FUNCTION(mfxStatus, MFXQueryVersion, (mfxSession session, mfxVersion *version), (session, version))

// CORE interface functions
FUNCTION(mfxStatus, MFXVideoCORE_SetBufferAllocator, (mfxSession session, mfxBufferAllocator *allocator), (session, allocator))
FUNCTION(mfxStatus, MFXVideoCORE_SetFrameAllocator, (mfxSession session, mfxFrameAllocator *allocator), (session, allocator))
FUNCTION(mfxStatus, MFXVideoCORE_SetHandle, (mfxSession session, mfxHandleType type, mfxHDL hdl), (session, type, hdl))
FUNCTION(mfxStatus, MFXVideoCORE_GetHandle, (mfxSession session, mfxHandleType type, mfxHDL *hdl), (session, type, hdl))

FUNCTION(mfxStatus, MFXVideoCORE_SyncOperation, (mfxSession session, mfxSyncPoint syncp, mfxU32 wait), (session, syncp, wait))

// ENCODE interface functions
FUNCTION(mfxStatus, MFXVideoENCODE_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoENCODE_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoENCODE_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoENCODE_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENCODE_GetEncodeStat, (mfxSession session, mfxEncodeStat *stat), (session, stat))
FUNCTION(mfxStatus, MFXVideoENCODE_EncodeFrameAsync, (mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp), (session, ctrl, surface, bs, syncp))

// DECODE interface functions
FUNCTION(mfxStatus, MFXVideoDECODE_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoDECODE_DecodeHeader, (mfxSession session, mfxBitstream *bs, mfxVideoParam *par), (session, bs, par))
FUNCTION(mfxStatus, MFXVideoDECODE_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoDECODE_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoDECODE_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoDECODE_GetDecodeStat, (mfxSession session, mfxDecodeStat *stat), (session, stat))
FUNCTION(mfxStatus, MFXVideoDECODE_GetPayload, (mfxSession session, mfxU64 *ts, mfxPayload *payload), (session, ts, payload))
FUNCTION(mfxStatus, MFXVideoDECODE_DecodeFrameAsync, (mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp), (session, bs, surface_work, surface_out, syncp))

// VPP interface functions
FUNCTION(mfxStatus, MFXVideoVPP_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoVPP_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoVPP_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoVPP_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoVPP_GetVPPStat, (mfxSession session, mfxVPPStat *stat), (session, stat))
FUNCTION(mfxStatus, MFXVideoVPP_RunFrameVPPAsync, (mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp), (session, in, out, aux, syncp))

// ENC interface function
FUNCTION(mfxStatus, MFXVideoENC_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoENC_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoENC_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENC_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENC_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoENC_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
//FUNCTION(mfxStatus, MFXVideoENC_GetFrameParam, (mfxSession session, mfxFrameParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoENC_RunFrameVmeENCAsync, (mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp), (session, cuc, syncp))

// PAK interface functions
FUNCTION(mfxStatus, MFXVideoPAK_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoPAK_QueryIOSurf, (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request), (session, par, request))
FUNCTION(mfxStatus, MFXVideoPAK_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoPAK_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoPAK_Close, (mfxSession session), (session))

FUNCTION(mfxStatus, MFXVideoPAK_GetVideoParam, (mfxSession session, mfxVideoParam *par), (session, par))
//FUNCTION(mfxStatus, MFXVideoPAK_GetFrameParam, (mfxSession session, mfxFrameParam *par), (session, par))
//FUNCTION(mfxStatus, MFXVideoPAK_RunSeqHeader, (mfxSession session, mfxFrameCUC *cuc), (session, cuc))
FUNCTION(mfxStatus, MFXVideoPAK_RunFramePAKAsync, (mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp), (session, cuc, syncp))

// BRC interface functions
FUNCTION(mfxStatus, MFXVideoBRC_Query, (mfxSession session, mfxVideoParam *in, mfxVideoParam *out), (session, in, out))
FUNCTION(mfxStatus, MFXVideoBRC_Init, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoBRC_Reset, (mfxSession session, mfxVideoParam *par), (session, par))
FUNCTION(mfxStatus, MFXVideoBRC_Close, (mfxSession session), (session))

//FUNCTION(mfxStatus, MFXVideoBRC_FrameENCUpdate, (mfxSession session, mfxFrameCUC *cuc), (session, cuc))
//FUNCTION(mfxStatus, MFXVideoBRC_FramePAKRefine, (mfxSession session, mfxFrameCUC *cuc), (session, cuc))
//FUNCTION(mfxStatus, MFXVideoBRC_FramePAKRecord, (mfxSession session, mfxFrameCUC *cuc), (session, cuc))
