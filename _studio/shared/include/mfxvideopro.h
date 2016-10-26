//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __MFXVIDEOPRO_H__
#define __MFXVIDEOPRO_H__
#include "mfxstructurespro.h"
#include "mfxvideo.h"

/* This is the external include file for the Media SDK product */

#ifdef __cplusplus
extern "C"
{
#endif

/* VideoCORE: */
mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

/* VideoENC: */
mfxStatus MFXVideoENC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFXVideoENC_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFXVideoENC_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoENC_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoENC_Close(mfxSession session);

mfxStatus MFXVideoENC_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoENC_GetFrameParam(mfxSession session, mfxFrameParam *par);
mfxStatus MFXVideoENC_RunFrameVmeENCAsync(mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp);

/* VideoPAK: */
mfxStatus MFXVideoPAK_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFXVideoPAK_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFXVideoPAK_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoPAK_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoPAK_Close(mfxSession session);

mfxStatus MFXVideoPAK_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoPAK_GetFrameParam(mfxSession session, mfxFrameParam *par);
mfxStatus MFXVideoPAK_RunSeqHeader(mfxSession session, mfxFrameCUC *cuc);
mfxStatus MFXVideoPAK_RunFramePAKAsync(mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp);

/* VideoBRC */
mfxStatus MFXVideoBRC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFXVideoBRC_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoBRC_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoBRC_Close(mfxSession session);

mfxStatus MFXVideoBRC_FrameENCUpdate(mfxSession session, mfxFrameCUC *cuc);
mfxStatus MFXVideoBRC_FramePAKRefine(mfxSession session, mfxFrameCUC *cuc);
mfxStatus MFXVideoBRC_FramePAKRecord(mfxSession session, mfxFrameCUC *cuc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
