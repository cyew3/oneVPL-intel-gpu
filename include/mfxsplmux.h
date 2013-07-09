/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2013 Intel Corporation. All Rights Reserved.

File Name: mfxsplmux.h

\* ****************************************************************************** */
#ifndef __MFXSPLMUX_H__
#define __MFXSPLMUX_H__
#include "mfxsmstructures.h"


#ifdef __cplusplus
extern "C" //XXX
{
#endif


/* DataReaderWriter */
typedef struct {
    mfxHDL          p; // pointer on external reader struct
    mfxI32         (*Read) (mfxHDL p, mfxBitstream *outBitStream);
    mfxI32         (*Write) (mfxHDL p, mfxBitstream *outBitStream);
    mfxI64         (*Seek) (mfxHDL p, mfxI64 offset, mfxI32 origin);  // Used only for read from file(mp4) or repositioning. Offset from begin of file
    mfxU64          DataSize; // May be set zero if unknown

} mfxDataIO;

typedef struct _mfxSplitter *mfxSplitter;

mfxStatus  MFX_CDECL MFXSplitter_Init(mfxDataIO dataIO, mfxSplitter *spl);
mfxStatus  MFX_CDECL MFXSplitter_Close(mfxSplitter spl);
mfxStatus  MFX_CDECL MFXSplitter_GetInfo(mfxSplitter spl, mfxStreamParams *par);

mfxStatus  MFX_CDECL MFXSplitter_GetBitstream(mfxSplitter spl, mfxU32 iInputTrack, mfxU32* iOutputTrack, mfxBitstream *Bitstream);
mfxStatus  MFX_CDECL MFXSplitter_ReleaseBitstream(mfxSplitter spl,  mfxU32 iTrack, mfxBitstream *Bitstream);

/* Muxer */

typedef struct _mfxMuxer *mfxMuxer;

mfxStatus  MFX_CDECL MFXMuxer_Init(mfxStreamParams* par, mfxDataIO dataIO, mfxMuxer *mux);
mfxStatus  MFX_CDECL MFXMuxer_Close(mfxMuxer mux);

mfxStatus  MFX_CDECL MFXMuxer_PutData(mfxMuxer mux, mfxU32 iTrack, mfxBitstream *Bitstream);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__MFXSPLMUX_H__
