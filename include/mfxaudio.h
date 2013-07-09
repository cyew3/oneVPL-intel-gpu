/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfxaudio.h

\* ****************************************************************************** */

#ifndef __MFXAUDIO_H__
#define __MFXAUDIO_H__
#include "mfxsession.h"
#include "mfxastructures.h"

#define MFX_AUDIO_VERSION_MAJOR 1
#define MFX_AUDIO_VERSION_MINOR 8

/* AudioENCODE */
mfxStatus MFX_CDECL MFXAudioENCODE_Query(mfxSession session, mfxAudioParam *in, mfxAudioParam *out);
mfxStatus MFX_CDECL MFXAudioENCODE_QueryIOSize(mfxSession session, mfxAudioParam *par, mfxAudioAllocRequest *request);
mfxStatus MFX_CDECL MFXAudioENCODE_Init(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioENCODE_Reset(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioENCODE_Close(mfxSession session);
mfxStatus MFX_CDECL MFXAudioENCODE_GetAudioParam(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioENCODE_EncodeFrameAsync(mfxSession session, mfxBitstream *in, mfxBitstream *out, mfxSyncPoint *syncp);

/* AudioDECODE */
mfxStatus MFX_CDECL MFXAudioDECODE_Query(mfxSession session, mfxAudioParam *in, mfxAudioParam *out);
mfxStatus MFX_CDECL MFXAudioDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxAudioParam* par);
mfxStatus MFX_CDECL MFXAudioDECODE_Init(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioDECODE_Reset(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioDECODE_Close(mfxSession session);
mfxStatus MFX_CDECL MFXAudioDECODE_QueryIOSize(mfxSession session, mfxAudioParam *par, mfxAudioAllocRequest *request);
mfxStatus MFX_CDECL MFXAudioDECODE_GetAudioParam(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *in, mfxBitstream *out, mfxSyncPoint *syncp);

#endif
