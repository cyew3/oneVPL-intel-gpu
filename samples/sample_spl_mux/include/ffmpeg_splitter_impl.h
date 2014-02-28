//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma warning( push )

#pragma warning( disable : 4244 )
#pragma warning( disable : 4204 )

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"

#pragma warning( pop )

#include "mfxsplmux.h"
#include "adts_muxer.h"

#include "vm/strings_defs.h"
#include "vm/time_defs.h"

#define QUEUE_MAX_SIZE 20

typedef struct
{
    mfxU32                    reserved[8];
    mfxDataIO                *dataIO;
    AVIOContext              *ioContext;
    AVFormatContext          *formatContext;
    AVBitStreamFilterContext *bsfContext;
    AVPacket                  inUsePackets[QUEUE_MAX_SIZE];
    mfxU8                    *outBufs[QUEUE_MAX_SIZE];
    int                       isStreamInfoFound;
    adtsMuxer                *adtsMuxer;
} _mfxSplitter;

mfxU16 GetMFXCodecProfileFromAVCodecTag(mfxU32 codecTag);
mfxU16 GetMFXCodecProfileFromAVCodecProfile(enum AVCodecID codecID, int codecProfile);
mfxSystemStreamType GetSystemStreamTypeByExtension(const char* extension);
mfxTrackType GetTrackTypeByCodecID(enum AVCodecID codecID);
mfxU32 GetMFXCodecIDFromAVCodecID(enum AVCodecID codecID);
