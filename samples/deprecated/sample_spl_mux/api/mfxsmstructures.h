/*******************************************************************************

Copyright (C) 2013 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxsmstructures.h

*******************************************************************************/
#ifndef __MFXSMSTRUCTURES_H__
#define __MFXSMSTRUCTURES_H__
#include "mfxvstructures.h"
#include "mfxastructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define MFX_TRACK_HEADER_MAX_SIZE 1024

typedef enum
{
    /* video types 0x0000XXXX */
    MFX_TRACK_MPEG2V                = 0x00000002,
    MFX_TRACK_H264                  = 0x00000020,
    MFX_TRACK_VC1                   = 0x00000400,
    MFX_TRACK_VP8                   = 0x00008000,
    MFX_TRACK_ANY_VIDEO             = 0x0000FFFF,

    /* audio types TRACK 0x0XXX0000 */
    MFX_TRACK_AAC                   = 0x00080000,
    MFX_TRACK_MPEGA                 = 0x00100000,
    MFX_TRACK_ANY_AUDIO             = 0x0FFF0000,

    MFX_TRACK_UNKNOWN               = 0x00000000
} mfxTrackType;

typedef enum
{
    MFX_UNDEF_STREAM            = 0x00000000,
    MFX_MPEG2_TRANSPORT_STREAM  = 0x000012c0,
    MFX_MPEG4_SYSTEM_STREAM     = 0x00001400,
    MFX_IVF_STREAM              = 0x10000000,
    MFX_ASF_STREAM              = 0x00000100
} mfxSystemStreamType;

typedef struct {
    mfxTrackType       Type;
    mfxU32             SID;
    mfxU16             Enable;
    mfxU16             HeaderLength;
    mfxU8              Header[MFX_TRACK_HEADER_MAX_SIZE]; /* header with codec specific data */
    mfxU16             reserved[16];

    union {
        mfxAudioInfoMFX AudioParam;
        mfxInfoMFX      VideoParam;
    };
} mfxTrackInfo;

typedef struct mfxStreamParams {
    mfxU16               reserved[22];
    mfxSystemStreamType  SystemType;
    mfxU32               Flags;
    mfxU64               Duration;           /* may be zero if undefined */
    mfxU16               NumTracks;          /* number of tracks detected */
    mfxU16               NumTracksAllocated; /* number of allocated elements in TrackInfo */
    mfxTrackInfo**       TrackInfo;
} mfxStreamParams;

typedef enum {
    MFX_SEEK_ORIGIN_BEGIN=1,
    MFX_SEEK_ORIGIN_CURRENT=2,
    MFX_SEEK_ORIGIN_END=3
} mfxSeekOrigin;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


