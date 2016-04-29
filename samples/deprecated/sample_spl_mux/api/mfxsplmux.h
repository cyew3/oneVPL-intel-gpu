/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/
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
