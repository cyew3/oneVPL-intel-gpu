//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#ifndef MFX_MUX_FFMPEG_IMPL
#define MFX_MUX_FFMPEG_IMPL

#pragma warning( push )

#pragma warning( disable : 4244 )
#pragma warning( disable : 4204 )

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"

#pragma warning( pop )

#include "mfxsplmux.h"

#include "vm/strings_defs.h"

typedef struct
{
    mfxU32                    reserved[12];
    mfxDataIO                *dataIO;
    mfxStreamParams           streamParams;
    AVIOContext              *ioContext;
    AVFormatContext          *formatContext;
    AVBitStreamFilterContext *bsfContext;
    AVStream                **streams;
    //WA to handle framerate based dts prediction
    int64_t                   lastDts[10];
    int64_t                   ptsToDtsOffset;

} _mfxMuxerFFMPEG;

mfxStatus           SetStreamInfo(mfxHDL mux, mfxU16 trackNum);
mfxSystemStreamType GetSystemStreamTypeByExtension(const char* extension);
enum AVCodecID      GetAVCodecIDFromMFXTrackType(mfxTrackType trackType);
int                 GetAVCodecProfileFromMFXCodecProfile(mfxTrackType trackType, mfxU16 codecProfile);

#endif // MFX_MUX_FFMPEG_IMPL
