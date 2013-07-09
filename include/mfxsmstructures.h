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
    MFX_TRACK_MPEG1V                = 0x00000001,
    MFX_TRACK_MPEG2V                = 0x00000002,
    MFX_TRACK_MPEG4V                = 0x00000004,
    MFX_TRACK_H261                  = 0x00000008,
    MFX_TRACK_H263                  = 0x00000010,
    MFX_TRACK_H264                  = 0x00000020,
    MFX_TRACK_DVSD                  = 0x00000040,
    MFX_TRACK_DV50                  = 0x00000080,
    MFX_TRACK_DVHD                  = 0x00000100,
    MFX_TRACK_DVSL                  = 0x00000200,
    MFX_TRACK_VC1                   = 0x00000400,
    MFX_TRACK_WMV                   = 0x00000800,
    MFX_TRACK_MJPEG                 = 0x00001000,
    MFX_TRACK_YUV                   = 0x00002000,
    MFX_TRACK_AVS                   = 0x00004000,
    MFX_TRACK_VP8                   = 0x00004000,
    MFX_TRACK_ANY_VIDEO             = 0x0000FFFF,

    /* audio types TRACK 0x0XXX0000 */
    MFX_TRACK_PCM                   = 0x00010000,
    MFX_TRACK_LPCM                  = 0x00020000,
    MFX_TRACK_AC3                   = 0x00040000,
    MFX_TRACK_AAC                   = 0x00080000,
    MFX_TRACK_MPEGA                 = 0x00100000,
    MFX_TRACK_TWINVQ                = 0x00200000,
    MFX_TRACK_DTS                   = 0x00400000,
    MFX_TRACK_VORBIS                = 0x00800000,
    MFX_TRACK_AMR                   = 0x01000000,
    MFX_TRACK_WMA                   = 0x02000000,
    MFX_TRACK_ANY_AUDIO             = 0x0FFF0000,

    MFX_TRACK_SUB_PIC               = 0x10000000,
    MFX_TRACK_DVD_NAV               = 0x20000000,
    MFX_TRACK_ANY_DVD               = 0x30000000,

    MFX_TRACK_VBI_TXT               = 0x40000000,
    MFX_TRACK_VBI_SPEC              = 0x80000000,
    MFX_TRACK_ANY_VBI               = 0xC0000000,

    MFX_TRACK_ANY_SPECIAL           = 0xF0000000,

    MFX_TRACK_UNKNOWN               = 0x00000000
} mfxTrackType;

typedef enum
{
    MFX_UNDEF_STREAM            = 0x00000000,
    MFX_AVI_STREAM              = 0x00000001, 
    MFX_MP4_ATOM_STREAM         = 0x00000010,
    MFX_ASF_STREAM              = 0x00000100,

    MFX_H26x_PURE_VIDEO_STREAM  = 0x00100000,
    MFX_H261_PURE_VIDEO_STREAM  = MFX_H26x_PURE_VIDEO_STREAM + 1,
    MFX_H263_PURE_VIDEO_STREAM  = MFX_H26x_PURE_VIDEO_STREAM + 2,
    MFX_H264_PURE_VIDEO_STREAM  = MFX_H26x_PURE_VIDEO_STREAM + 3,

    MFX_MPEGx_SYSTEM_STREAM     = 0x00001000,

    MFX_MPEG1_SYSTEM_STREAM     = MFX_MPEGx_SYSTEM_STREAM|0x00000100,
    MFX_MPEG2_SYSTEM_STREAM     = MFX_MPEGx_SYSTEM_STREAM|0x00000200,
    MFX_MPEG4_SYSTEM_STREAM     = MFX_MPEGx_SYSTEM_STREAM|0x00000400,

    MFX_MPEGx_PURE_VIDEO_STREAM = MFX_MPEGx_SYSTEM_STREAM|0x00000010,
    MFX_MPEGx_PURE_AUDIO_STREAM = MFX_MPEGx_SYSTEM_STREAM|0x00000020,
    MFX_MPEGx_PES_PACKETS_STREAM= MFX_MPEGx_SYSTEM_STREAM|0x00000040,
    MFX_MPEGx_PROGRAMM_STREAM   = MFX_MPEGx_SYSTEM_STREAM|0x00000080,
    MFX_MPEGx_TRANSPORT_STREAM  = MFX_MPEGx_SYSTEM_STREAM|0x000000c0,


    MFX_MPEG1_PURE_VIDEO_STREAM = MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PURE_VIDEO_STREAM,
    MFX_MPEG1_PURE_AUDIO_STREAM = MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PURE_AUDIO_STREAM,
    MFX_MPEG1_PES_PACKETS_STREAM= MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PES_PACKETS_STREAM,
    MFX_MPEG1_PROGRAMM_STREAM   = MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PROGRAMM_STREAM,

    MFX_MPEG2_PURE_VIDEO_STREAM = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PURE_VIDEO_STREAM,
    MFX_MPEG2_PURE_AUDIO_STREAM = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PURE_AUDIO_STREAM,
    MFX_MPEG2_PES_PACKETS_STREAM= MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PES_PACKETS_STREAM,
    MFX_MPEG2_PROGRAMM_STREAM   = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PROGRAMM_STREAM,
    MFX_MPEG2_TRANSPORT_STREAM  = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_TRANSPORT_STREAM,
    MFX_MPEG2_TRANSPORT_STREAM_TTS  = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_TRANSPORT_STREAM | 1,
    MFX_MPEG2_TRANSPORT_STREAM_TTS0 = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_TRANSPORT_STREAM | 2,

    MFX_MPEG4_PURE_VIDEO_STREAM = MFX_MPEG4_SYSTEM_STREAM|MFX_MPEGx_PURE_VIDEO_STREAM,

    MFX_WEB_CAM_STREAM          = 0x00100000,
    MFX_ADIF_STREAM             = 0x00200000,
    MFX_ADTS_STREAM             = 0x00400000,
    MFX_STILL_IMAGE             = 0x00800000,
    MFX_VC1_PURE_VIDEO_STREAM   = 0x01000000,
    MFX_WAVE_STREAM             = 0x02000000,
    MFX_AVS_PURE_VIDEO_STREAM   = 0x04000000,
    MFX_FLV_STREAM              = 0x08000000,
    MFX_IVF_STREAM              = 0x10000000,
    MFX_OGG_STREAM              = 0x10100000,
    MFX_MJPEG_STREAM            = 0x20000000
} mfxSystemStreamType;

typedef struct {
    mfxTrackType       Type;
    mfxU32             PID;
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
    mfxU64               Duration;          /* may be zero if undefined */
    mfxU16               NumTrack;          /* number of tracks detected */
    mfxU16               NumTrackAllocated; /* number of allocated elements in TrackInfo */
    mfxTrackInfo**       TrackInfo;
} mfxStreamParams;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


