/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfxsplmux.h

\* ****************************************************************************** */
#ifndef __MFXSPLMUX_H__
#define __MFXSPLMUX_H__

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif // #if defined(_WIN32) || defined(_WIN64)

#include "mfxsmstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* DataReaderWriter */
typedef struct {
    mfxU32         reserved1[4];

    mfxHDL         pthis;
    mfxI32         (*Read) (mfxHDL pthis, mfxBitstream *bs);
    mfxI32         (*Write) (mfxHDL pthis, mfxBitstream *bs);
    mfxI64         (*Seek) (mfxHDL pthis, mfxI64 offset, mfxSeekOrigin origin); /* returns position in the file after seek, may be used to determine file size, unsupported in muxers */
    mfxHDL        reserved2[4];
} mfxDataIO;

typedef struct _mfxSplitter *mfxSplitter;
mfxStatus  MFX_CDECL MFXSplitter_Init(mfxDataIO *data_io, mfxSplitter *spl);
mfxStatus  MFX_CDECL MFXSplitter_Close(mfxSplitter spl);
mfxStatus  MFX_CDECL MFXSplitter_GetInfo(mfxSplitter spl, mfxStreamParams *par);
mfxStatus  MFX_CDECL MFXSplitter_GetBitstream(mfxSplitter spl, mfxU32 *track_num, mfxBitstream *bs);
mfxStatus  MFX_CDECL MFXSplitter_ReleaseBitstream(mfxSplitter spl, mfxBitstream *bs);
mfxStatus  MFX_CDECL MFXSplitter_Seek(mfxSplitter spl, mfxU64 timestamp);

typedef struct _mfxMuxer *mfxMuxer;
mfxStatus  MFX_CDECL MFXMuxer_Init(mfxStreamParams* par, mfxDataIO *data_io, mfxMuxer *mux);
mfxStatus  MFX_CDECL MFXMuxer_Close(mfxMuxer mux);
mfxStatus  MFX_CDECL MFXMuxer_PutBitstream(mfxMuxer mux, mfxU32 track_num, mfxBitstream *bs, mfxU64 duration);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__MFXSPLMUX_H__
