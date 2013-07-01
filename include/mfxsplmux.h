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

#include "mfxstructures.h"
#include "mfxaudio.h"

#ifdef _WIN32
    #define MFX_CDECL __cdecl
    #define MFX_STDCALL __stdcall
#else
    #define MFX_CDECL
    #define MFX_STDCALL
#endif /* _WIN32 */

#ifdef __cplusplus
extern "C" //XXX
{
#endif

/* StreamInfo */

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
    MFX_UNDEF_STREAM            = 0x00000000, //unsupported stream type
    MFX_AVI_STREAM              = 0x00000001, //AVI RIFF
    MFX_MP4_ATOM_STREAM         = 0x00000010, //ISO/IEC 14496-14 stream
    MFX_ASF_STREAM              = 0x00000100, //ASF stream                     ???

    MFX_H26x_PURE_VIDEO_STREAM  = 0x00100000,
    MFX_H261_PURE_VIDEO_STREAM  = MFX_H26x_PURE_VIDEO_STREAM + 1,
    MFX_H263_PURE_VIDEO_STREAM  = MFX_H26x_PURE_VIDEO_STREAM + 2,
    MFX_H264_PURE_VIDEO_STREAM  = MFX_H26x_PURE_VIDEO_STREAM + 3,

    MFX_MPEGx_SYSTEM_STREAM     = 0x00001000,                    //MPEG 1,2,4 - like system

    MFX_MPEG1_SYSTEM_STREAM     = MFX_MPEGx_SYSTEM_STREAM|0x00000100,//MPEG 1 system
    MFX_MPEG2_SYSTEM_STREAM     = MFX_MPEGx_SYSTEM_STREAM|0x00000200,//MPEG 2 system
    MFX_MPEG4_SYSTEM_STREAM     = MFX_MPEGx_SYSTEM_STREAM|0x00000400,//MPEG 4 system

    MFX_MPEGx_PURE_VIDEO_STREAM = MFX_MPEGx_SYSTEM_STREAM|0x00000010,//MPEG 1,2 - like pure video data
    MFX_MPEGx_PURE_AUDIO_STREAM = MFX_MPEGx_SYSTEM_STREAM|0x00000020,//MPEG 1,2 - like pure audio data
    MFX_MPEGx_PES_PACKETS_STREAM= MFX_MPEGx_SYSTEM_STREAM|0x00000040,//MPEG 1,2 - like pes packets system
    MFX_MPEGx_PROGRAMM_STREAM   = MFX_MPEGx_SYSTEM_STREAM|0x00000080,//MPEG 1,2 - like program system
    MFX_MPEGx_TRANSPORT_STREAM  = MFX_MPEGx_SYSTEM_STREAM|0x000000c0,//MPEG 1,2 - like transport system


    MFX_MPEG1_PURE_VIDEO_STREAM = MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PURE_VIDEO_STREAM, //MPEG1 pure video stream
    MFX_MPEG1_PURE_AUDIO_STREAM = MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PURE_AUDIO_STREAM, //MPEG1 pure video stream
    MFX_MPEG1_PES_PACKETS_STREAM= MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PES_PACKETS_STREAM,//MPEG1 pes packets stream
    MFX_MPEG1_PROGRAMM_STREAM   = MFX_MPEG1_SYSTEM_STREAM|MFX_MPEGx_PROGRAMM_STREAM,   //MPEG1 program stream

    MFX_MPEG2_PURE_VIDEO_STREAM = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PURE_VIDEO_STREAM,//MPEG2 pure video stream
    MFX_MPEG2_PURE_AUDIO_STREAM = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PURE_AUDIO_STREAM,//MPEG2 pure audio stream
    MFX_MPEG2_PES_PACKETS_STREAM= MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PES_PACKETS_STREAM,//MPEG2 pes packets stream
    MFX_MPEG2_PROGRAMM_STREAM   = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_PROGRAMM_STREAM,   //MPEG2 program stream
    MFX_MPEG2_TRANSPORT_STREAM  = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_TRANSPORT_STREAM,  //MPEG2 transport stream
    MFX_MPEG2_TRANSPORT_STREAM_TTS  = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_TRANSPORT_STREAM | 1,  //MPEG2 transport stream with valid packet time stamps
    MFX_MPEG2_TRANSPORT_STREAM_TTS0 = MFX_MPEG2_SYSTEM_STREAM|MFX_MPEGx_TRANSPORT_STREAM | 2,  //MPEG2 transport stream with zero packet time stamps

    MFX_MPEG4_PURE_VIDEO_STREAM = MFX_MPEG4_SYSTEM_STREAM|MFX_MPEGx_PURE_VIDEO_STREAM,//MPEG4 pure video stream

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
    mfxU8   Data[MFX_TRACK_HEADER_MAX_SIZE];
    mfxU32  HeaderSize;
} mfxTrackHeader;

typedef struct {
    mfxTrackType       Type;           // common type (all audio/video/other in one enum) //
    mfxU32             PID;
    mfxU8              Enable;
    mfxU8              Header[MFX_TRACK_HEADER_MAX_SIZE]; // Header with codec specific data
    mfxU64             HeaderLength;
    union {
        mfxAudioStreamInfo audioParam;
        mfxVideoParam videoParam;
    };
    mfxU32             reserved[8];
} mfxTrackInfo;

/* DataReaderWriter */
typedef struct {
    mfxHDL          p; // pointer on external reader struct
    mfxI32         (*Read) (mfxHDL p, mfxBitstream *outBitStream);
    mfxI32         (*Write) (mfxHDL p, mfxBitstream *outBitStream);
    mfxI64         (*Seek) (mfxHDL p, mfxI64 offset, mfxI32 origin);  // Used only for read from file(mp4) or repositioning. Offset from begin of file
    mfxU64          DataSize; // May be set zero if unknown

} mfxDataIO;

/* Splitter */

typedef struct mfxStreamParams {
    mfxU32               reserved[12];
    mfxSystemStreamType  SystemType;        // system type (MPEG4, MPEG2, AVI, pure)
    mfxU64               Duration;          // May be zero if undefined
    mfxU32               NumTrack;          // number of tracks detected
    mfxU32               NumTrackAllocated; // Number of allocated elements in TrackInfo
    mfxTrackInfo**       TrackInfo;         // array of pointers to TrackInfo(s)
    mfxU32               Flags;
} mfxStreamParams;

typedef struct _mfxSplitter *mfxSplitter;

mfxStatus  MFX_CDECL MFXSplitter_Init(mfxDataIO dataIO, mfxSplitter *spl);
mfxStatus  MFX_CDECL MFXSplitter_Close(mfxSplitter spl);
mfxStatus  MFX_CDECL MFXSplitter_GetInfo(mfxSplitter spl, mfxStreamParams *par);

mfxStatus  MFX_CDECL MFXSplitter_GetBitstream(mfxSplitter spl,  mfxU32 iTrack, mfxBitstream *Bitstream);
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
