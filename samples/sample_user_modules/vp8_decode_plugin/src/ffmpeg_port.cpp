/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "ffmpeg_port.h"
#include <iostream>
#include <limits>
#include "sample_defs.h"

namespace {
    enum {
        FF_CODEC_H263 = MFX_MAKEFOURCC('h','2','6','3'),
        FF_CODEC_VP8 = MFX_MAKEFOURCC('V','P','8',' ')
    };
    const double MFX_TIME_STAMP_FREQUENCY = 90000.0;
    const mfxU64 MFX_TIME_STAMP_INVALID   = (mfxU64)-1;
}

MSDKMutex  FFPortable::m_mutex;

FFPortable::FFPortable()
{
    av_lockmgr_register(&OnLockOperation);
    av_log_set_callback(AvLogCallback);

    av_register_all();
}

FFPortable::~FFPortable()
{
    av_lockmgr_register(NULL);
}

void FFPortable::AvLogCallback(void* pData, int lvl, const char* fmt, va_list lst)
{
    char av_log_msg[4096] = {0};
    int prefix = 1;
    av_log_format_line(pData, lvl, fmt, lst, av_log_msg, sizeof(av_log_msg), &prefix);

    //cutting end of line
    size_t len = strlen(av_log_msg);
    if (len)
        av_log_msg[len - 1] = 0;

    MSDK_TRACE_INFO(av_log_msg);
}

int FFPortable::OnLockOperation(void** pMutex, enum AVLockOp operation)
{
    if (!pMutex)
    {
        return -1;
    }

    switch (operation)
    {
    case AV_LOCK_CREATE:
        *pMutex = &m_mutex;
        break;

    case AV_LOCK_OBTAIN:
        m_mutex.Lock();
        break;

    case AV_LOCK_RELEASE:
        m_mutex.Unlock();
        break;

    case AV_LOCK_DESTROY:
        *pMutex = NULL;
    }

    return 0;
}

mfxStatus FFPortable::MFXCodecIDToAVCodecID(mfxU32 mfxCodecId, AVCodecID & av_codec_id)
{
    switch(mfxCodecId)
    {
    case MFX_CODEC_AVC :
        av_codec_id = CODEC_ID_H264;
        break;
    case FF_CODEC_H263 :
        av_codec_id = CODEC_ID_H263;
        break;
    case FF_CODEC_VP8 :
        av_codec_id = CODEC_ID_VP8;
        break;

    default:
        MSDK_TRACE_ERROR("Unknown MediaSDK codecID provided" << mfxCodecId);
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxStatus FFPortable::AVCodecIDToMFXCodecID( AVCodecID av_codec_id, mfxU32 &mfxCodecId)
{
    switch(av_codec_id)
    {
    case CODEC_ID_H264 :
        mfxCodecId = MFX_CODEC_AVC ;
        break;
    case CODEC_ID_H263 :
        mfxCodecId = FF_CODEC_H263 ;
        break;
    case CODEC_ID_VP8 :
        mfxCodecId = FF_CODEC_VP8 ;
        break;

    default :
        MSDK_TRACE_ERROR("Unknown AVCodecID provided" << av_codec_id);
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}
mfxStatus FFPortable::PixelFormatToChromaFormat(AVPixelFormat fmt, mfxU16 &chromaFormat)
{
    switch(fmt)
    {
    case  PIX_FMT_NV12 :
        chromaFormat = MFX_CHROMAFORMAT_YUV420;
        break;
    case PIX_FMT_YUV420P :
        chromaFormat = MFX_CHROMAFORMAT_YUV420;
        break;
    case  PIX_FMT_YUYV422:
        chromaFormat = MFX_CHROMAFORMAT_YUV422;
        break;
    case  PIX_FMT_BGR24:
        chromaFormat = MFX_CHROMAFORMAT_YUV444;
        break;
    case  PIX_FMT_ARGB:
        chromaFormat = MFX_CHROMAFORMAT_YUV444;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus FFPortable::PixelFormatToFourcc(AVPixelFormat fmt, mfxU32 &fourcc)
{
    switch(fmt)
    {
    case  PIX_FMT_NV12 :
        fourcc = MFX_FOURCC_NV12;
        break;
    case PIX_FMT_YUV420P :
        fourcc = MFX_FOURCC_YV12;
        break;
    case  PIX_FMT_YUYV422:
        fourcc = MFX_FOURCC_YUY2;
        break;
    case  PIX_FMT_BGR24:
        fourcc = MFX_FOURCC_RGB3;
        break;
    case  PIX_FMT_ARGB:
        fourcc = MFX_FOURCC_RGB4;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus FFPortable::FourccToPixelFormat(mfxU32 mfxFourcc, AVPixelFormat & fmt)
{
    switch(mfxFourcc)
    {
    case MFX_FOURCC_NV12 :
        fmt = PIX_FMT_NV12;
        break;
    case MFX_FOURCC_YV12 :
        fmt = PIX_FMT_YUV420P;
        break;
    case MFX_FOURCC_YUY2 :
        fmt = PIX_FMT_YUYV422;
        break;
    case MFX_FOURCC_RGB3 :
        fmt = PIX_FMT_BGR24;
        break;
    case MFX_FOURCC_RGB4 :
        fmt = PIX_FMT_ARGB;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

namespace {

    template <class TFrom, class TTo>
    bool static_cast_range_check(TTo & lhs, TFrom &rhs) {
        if (rhs > (std::numeric_limits<TTo>::max)() || rhs < (std::numeric_limits<TTo>::min)()) {
            return false;
        }
        lhs = static_cast<TTo>(rhs);
        return true;
    }

    #define MSDK_SET_VALUE(msdk_variable, ffvariable)\
    if (!static_cast_range_check(msdk_variable, ffvariable)) {\
        MSDK_TRACE_ERROR(#ffvariable << "("<< ffvariable <<") not in range limit for MediaSDK variable "\
            << #msdk_variable<<"(" << typeid(msdk_variable).name() <<")");\
    }\

}

mfxStatus FFPortable::AVFrameTomfxFrameSurface(const AVFrame &frame, const AVRational &stream_time_base, mfxFrameSurface1 & surface)
{
    memset(&surface, 0, sizeof(surface));

    if (MFX_ERR_NONE != PixelFormatToFourcc((AVPixelFormat)frame.format, surface.Info.FourCC)) {
        MSDK_TRACE_ERROR( "AVFrameTomfxFrameSurface::unsupported pixel format :" << frame.format);
        return MFX_ERR_UNKNOWN;
    }

    if (MFX_ERR_NONE != PixelFormatToChromaFormat((AVPixelFormat)frame.format, surface.Info.ChromaFormat)) {
        MSDK_TRACE_ERROR( "AVFrameTomfxFrameSurface::unsupported pixel format during chroma conversion:" << frame.format);
        return MFX_ERR_UNKNOWN;
    }

    MSDK_SET_VALUE(surface.Info.Width, frame.width);

    MSDK_SET_VALUE(surface.Info.Height, frame.height);
    MSDK_SET_VALUE(surface.Info.CropW, frame.width);
    MSDK_SET_VALUE(surface.Info.CropH, frame.height);
    surface.Info.PicStruct = 0;
    MSDK_SET_VALUE(surface.Info.AspectRatioW, frame.sample_aspect_ratio.num);
    MSDK_SET_VALUE(surface.Info.AspectRatioH, frame.sample_aspect_ratio.den);


    surface.Data.Y = frame.data[0];
    surface.Data.U = frame.data[1];
    surface.Data.V = frame.data[2];
    surface.Data.MemId = 0;
    MSDK_SET_VALUE(surface.Data.Pitch, frame.linesize[0]);
    surface.Data.TimeStamp = (mfxU64)(av_q2d(stream_time_base) * frame.pts * (double)MFX_TIME_STAMP_FREQUENCY);
    surface.Data.FrameOrder = frame.coded_picture_number;
    surface.Data.Locked = 0;
    surface.Data.Corrupted = 0;
    surface.Data.DataFlag = 0;

    return MFX_ERR_NONE;
}

mfxStatus FFPortable::AllocFrame(FFCodec &codec)
{
    //alloc frame for decoder output/encode input
    if (NULL == (codec.pFrame = avcodec_alloc_frame()))
    {
        MSDK_TRACE_ERROR("NULL == avcodec_alloc_frame()");
        return MFX_ERR_MEMORY_ALLOC;
    }
    return MFX_ERR_NONE;
}

mfxStatus FFPortable::InitCodec(FFCodec &codec, mfxU32 mfxCodecId)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (MFX_ERR_NONE != (sts = AllocFrame(codec)))
    {
        return sts;
    }

    sts = MFXCodecIDToAVCodecID(mfxCodecId, codec.cid);

    switch (sts)
    {
    case MFX_ERR_NONE :
        break;

    default:
        return sts;
    }

    if (NULL == (codec.pCodec = codec.find_ptr(codec.cid)))
    {
        MSDK_TRACE_ERROR( "NULL = codec.avcodec_find_...(" << codec.cid << ")");
        return MFX_ERR_UNSUPPORTED;
    }

    if (NULL == (codec.pCtx = avcodec_alloc_context3(codec.pCodec)))
    {
        MSDK_TRACE_ERROR( "NULL = avcodec_alloc_context3()");
        return MFX_ERR_MEMORY_ALLOC;
    }

    return MFX_ERR_NONE;
}

mfxStatus FFPortable::OpenCodec(FFCodec &codec)
{
    int result = avcodec_open2(codec.pCtx, codec.pCodec, NULL);
    if (0 != result)
    {
        MSDK_TRACE_ERROR( "avcodec_open2() returned " << result);
        return MFX_ERR_UNSUPPORTED;
    }
    codec.bCodecOpened = true;

    return MFX_ERR_NONE;
}

mfxStatus FFPortable::DecodeFrame(FFCodec  &codec, mfxBitstream &bs, mfxFrameSurface1 & surface) {

    AVPacket pkt = {0};

    //following ffmpeg req:
    //* @note Codecs which have the CODEC_CAP_DELAY capability set have a delay
    //* between input and output, these need to be fed with avpkt->data=NULL,
    //* avpkt->size=0 at the end to return the remaining frames.

    if (bs.DataLength) {
        if (bs.DataLength + bs.DataOffset + FF_INPUT_BUFFER_PADDING_SIZE > bs.MaxLength ) {
            MSDK_TRACE_ERROR( "avcodec_decode_video2() requred to suply FF_INPUT_BUFFER_PADDING_SIZE("
                << FF_INPUT_BUFFER_PADDING_SIZE <<" aligned input buffer");
            return MFX_ERR_UNSUPPORTED;
        }
        av_init_packet(&pkt);

        pkt.data = bs.Data + bs.DataOffset;
        pkt.size = bs.DataLength;

        memset(pkt.data + pkt.size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

        pkt.pts = (MFX_TIME_STAMP_INVALID == bs.TimeStamp) ? AV_NOPTS_VALUE :
            (int64_t)((double)bs.TimeStamp / MFX_TIME_STAMP_FREQUENCY / av_q2d(codec.pCtx->time_base));
        pkt.dts = AV_NOPTS_VALUE;
    }

    int got_picture = 0;
    int result = avcodec_decode_video2(codec.pCtx, codec.pFrame, &got_picture, &pkt);

    if (result < 0) {
        MSDK_TRACE_ERROR( "avcodec_decode_video2() returned " << result);
        return MFX_ERR_UNKNOWN;
    }

    bs.DataOffset += result;
    bs.DataLength -= result;

    if (got_picture) {
        if (MFX_ERR_NONE != FFPortable::AVFrameTomfxFrameSurface(*codec.pFrame, codec.pCtx->time_base, surface)) {
            return MFX_ERR_UNKNOWN;
        }
        return MFX_ERR_NONE;
    }

    return MFX_ERR_MORE_DATA;
}

void FFPortable::ReleaseCodec(FFCodec &codec)
{
    if (NULL != codec.pCtx)
    {
        if (codec.bCodecOpened)
        {
            avcodec_close(codec.pCtx);
        }
        av_free(codec.pCtx);
        codec.pCtx = NULL;
        codec.bCodecOpened = false;
    }

    if (NULL != codec.pFrame)
    {
        av_free(codec.pFrame);
        codec.pFrame = NULL;
    }

    codec.pCodec = NULL;
}

mfxStatus FFPortable::OpenParser(FFParser  & parser, mfxU32 mfxCodecId)
{
    mfxStatus sts = MFX_ERR_NONE;

    AVCodecID cid;

    sts = MFXCodecIDToAVCodecID(mfxCodecId, cid);

    switch (sts)
    {
    case MFX_ERR_NONE :
        break;
    default:
        return sts;
    }

    parser.pCtx = av_parser_init(cid);

    if (MFX_ERR_NONE != (sts = InitCodec(parser.dec, mfxCodecId)))
    {
        ReleaseCodec(parser.dec);
        return sts;
    }
    if (MFX_ERR_NONE != (sts = OpenCodec(parser.dec)))
    {
        return sts;
    }

    if (parser.pCtx)
    {
        parser.bInitialized = true;
    }

    return parser.pCtx ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
}

void   FFPortable::ReleaseParser(FFParser &parser)
{
    if (parser.bInitialized)
    {
        av_parser_close(parser.pCtx);
        parser.pCtx = NULL;
        parser.bInitialized = false;
        ReleaseCodec(parser.dec);
    }
}

mfxStatus FFPortable::ConstructFrame(FFParser  & parser, const mfxBitstream & inBs, mfxBitstream & outBs)
{
    mfxBitstream input = inBs;

    while(input.DataLength) {
       int len = av_parser_parse2(parser.pCtx
           , parser.dec.pCtx
           , &outBs.Data
           , (int*)&outBs.DataLength
           , inBs.Data + inBs.DataOffset
           , inBs.DataLength
           , 0
           , 0
           , 0);

       input.DataOffset += len;
       input.DataLength -= len;

       if(outBs.DataLength) {
           outBs.MaxLength = outBs.DataLength;
           return MFX_ERR_NONE;
       }
    }

    return MFX_ERR_MORE_DATA;
}

mfxStatus FFPortable::ConvertSurface(FFScaler &scl, const mfxFrameSurface1& surfaceIn, mfxFrameSurface1 & surfaceOut)
{
    //data pointers check
    if (surfaceIn.Data.Y == NULL)
    {
        MSDK_TRACE_ERROR( "0 == pSurfaceIn->Data.MemId");
        return MFX_ERR_UNSUPPORTED;
    }
    if (surfaceOut.Data.Y == NULL)
    {
        MSDK_TRACE_ERROR( "0 == pSurfaceOut->Data.MemId");
        return MFX_ERR_UNSUPPORTED;
    }

    AVPixelFormat inFmt;
    AVPixelFormat outFmt = AV_PIX_FMT_NONE;
    mfxStatus sts = FourccToPixelFormat(surfaceIn.Info.FourCC, inFmt);
    if (MFX_ERR_NONE != sts) {
        MSDK_TRACE_ERROR( "input surface FourccToPixelFormat() returned" << sts);
        return MFX_ERR_UNSUPPORTED;
    }
    sts = FourccToPixelFormat(surfaceOut.Info.FourCC, outFmt);
    if (MFX_ERR_NONE != sts) {
        MSDK_TRACE_ERROR( "input surface FourccToPixelFormat() returned" << sts);
        return MFX_ERR_UNSUPPORTED;
    }

    scl.pCtx = sws_getCachedContext(scl.pCtx
        , surfaceIn.Info.CropW
        , surfaceIn.Info.CropH
        , inFmt
        , surfaceOut.Info.CropW
        , surfaceOut.Info.CropH
        , outFmt
        , SWS_BILINEAR
        , NULL, NULL, NULL);

    if (NULL == scl.pCtx) {
        MSDK_TRACE_ERROR( "sws_getCachedContext() returned NULL");
        return MFX_ERR_UNSUPPORTED;
    }

    //setting up chroma shifts
    uint32_t chroma_subsampling_in[AV_NUM_DATA_POINTERS]  = {0};
    uint32_t chroma_subsampling_out[AV_NUM_DATA_POINTERS]  = {0};
    {
        int v_shift, h_shift;
        avcodec_get_chroma_sub_sample(inFmt, &h_shift, &v_shift );
        for (size_t i = 1; i < AV_NUM_DATA_POINTERS; i++) {
            chroma_subsampling_in[i] = h_shift;
        }
        avcodec_get_chroma_sub_sample(outFmt, &h_shift, &v_shift );
        for (size_t i = 1; i < AV_NUM_DATA_POINTERS; i++) {
            chroma_subsampling_out[i] = h_shift;
        }
    }
    //all mediasdk planes possible only 4
    uint8_t *srcSlice[] = {surfaceIn.Data.Y, surfaceIn.Data.UV, surfaceIn.Data.Cr, surfaceIn.Data.A};
    uint8_t *dstSlice[] = {surfaceOut.Data.Y, surfaceOut.Data.UV, surfaceOut.Data.Cr, surfaceOut.Data.A};

    int srcStride[AV_NUM_DATA_POINTERS] = {};
    int dstStride[AV_NUM_DATA_POINTERS] = {};

    for(int j = 0; j < _countof(srcStride); j++) {
        srcStride[j] = surfaceIn.Data.Pitch >> chroma_subsampling_in[j];
        dstStride[j] = surfaceOut.Data.Pitch >> chroma_subsampling_out[j];
    }

    int h = sws_scale(scl.pCtx
        , srcSlice
        , srcStride
        , 0
        , surfaceIn.Info.CropH
        , dstSlice
        , dstStride);
    if (h != surfaceOut.Info.CropH ) {
        MSDK_TRACE_ERROR( "sws_scale() returned " << h<<", while output slice height=" << surfaceOut.Info.CropH);
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}