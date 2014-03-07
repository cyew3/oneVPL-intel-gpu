//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include "vm/thread_defs.h"
#include "mfxstructures.h"

#ifndef INT64_C
    #define INT64_C(c) (c ## LL)
    #define UINT64_C(c) (c ## ULL)
#endif

#pragma warning(push)
#pragma warning(disable: 4244)

extern "C" {
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersink.h"
    #include "libavfilter/avcodec.h"
    #include "libavfilter/avfiltergraph.h"
}


#pragma warning(pop)


//decoder and encoder wrapper
struct FFCodec
{
    bool            bEncoder; //release and create activities are different
    bool            bCodecOpened;
    AVCodecID       cid;
    AVCodec        *pCodec;
    AVCodecContext *pCtx;
    AVFrame        *pFrame;
    AVPacket        packet;

    AVCodec * (*find_ptr)(AVCodecID id);

    FFCodec(bool isEncoder)
        : bEncoder(isEncoder)
        , bCodecOpened()
        , cid()
        , pCodec()
        , pCtx()
        , pFrame()
        , packet()
    {
        if (!isEncoder)
        {
            find_ptr = &avcodec_find_decoder;
        }
        else
        {
            find_ptr = &avcodec_find_encoder;
        }
    }
};

struct FFScaler
{
    bool bInitialized;
    SwsContext * pCtx;
    FFScaler()
        : bInitialized()
        , pCtx()
    {

    }
};


struct FFParser
{
    bool bInitialized;
    AVCodecParserContext * pCtx;
    FFCodec dec;

    FFParser()
        : bInitialized()
        , pCtx()
        , dec(false)
    {
    }
};

/*
* portable version of ffmpeg codec utils
*/
class FFPortable
{
public:

    FFPortable();
    ~FFPortable();

    static mfxStatus MFXCodecIDToAVCodecID(mfxU32 fourcc, AVCodecID & av_codec_id);
    static mfxStatus AVCodecIDToMFXCodecID(AVCodecID av_codec_id, mfxU32 &fourcc);
    static mfxStatus FourccToPixelFormat(mfxU32 fourcc, AVPixelFormat & fmt);
    static mfxStatus PixelFormatToFourcc(AVPixelFormat fmt, mfxU32 &fourcc);
    static mfxStatus PixelFormatToChromaFormat(AVPixelFormat fmt, mfxU16 &chromaFormat);
    static mfxStatus AVFrameTomfxFrameSurface(const AVFrame &, const AVRational &stream_time_base, mfxFrameSurface1 &);

    mfxStatus AllocFrame(FFCodec &codec);
    mfxStatus InitCodec(FFCodec &codec, mfxU32 fourcc);
    mfxStatus OpenCodec(FFCodec &codec);
    mfxStatus OpenParser(FFParser  &, mfxU32 fourcc);
    //used to convert internal ffmpeg surface to external provided by allocator->Lock()
    mfxStatus ConvertSurface(FFScaler &scl, const mfxFrameSurface1& pSurfaceIn, mfxFrameSurface1 & pSurfaceOut);

    void      ReleaseCodec(FFCodec &codec);
    void      ReleaseParser(FFParser &);


    mfxStatus ConstructFrame(FFParser  &, const mfxBitstream & inBs, mfxBitstream & outBs);

    // contract:
    // bitstream offset pointer is moved
    // psurface pointers are set up
    // return MFX_ERR_NONE on sucess
    mfxStatus DecodeFrame(FFCodec  &, mfxBitstream &, mfxFrameSurface1 &);

private:
    static void AvLogCallback(void*, int, const char*, va_list);
    static int OnLockOperation(void** pMutex, enum AVLockOp operation);
    static MSDKMutex m_mutex;
};