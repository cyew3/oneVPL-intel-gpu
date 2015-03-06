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

#define ADTS_HEADER_WITH_CRC_LENGTH 9

typedef struct _adtsMuxer *adtsMuxer;

typedef struct
{
    mfxDataIO                *dataIO;
    AVIOContext              *ioContext;
    AVFormatContext          *formatContext;
    int                       dataLen;
    AVStream                 *adtsStream;
} _adtsMuxer;

mfxStatus InitAdtsMuxer(AVCodecContext *inputCodecContext, adtsMuxer *mux);
mfxStatus CloseAdtsMuxer(adtsMuxer mux);
mfxStatus SetAdtsInfo(AVCodecContext *inputCodecContext, adtsMuxer mux);
mfxStatus GetAdtsBitstream(adtsMuxer mux, AVPacket *pkt, mfxBitstream *bs);
mfxI32    WriteADTS(void* pthis, mfxBitstream *bs);
