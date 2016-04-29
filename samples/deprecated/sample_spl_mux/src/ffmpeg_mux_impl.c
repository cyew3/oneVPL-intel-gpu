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

#include "mfx_samples_config.h"

#define inline __inline

#include "mfxdefs.h"
#include "mfxsplmux.h"

#include "ffmpeg_mux_impl.h"
#include "ffmpeg_reader_writer.h"

static AVRational q = {1, 90000};

enum AVCodecID GetAVCodecIDFromMFXTrackType(mfxTrackType trackType)
{
    switch(trackType)
    {
    case MFX_TRACK_H264:
        return AV_CODEC_ID_H264;
    case MFX_TRACK_MPEG2V:
        return AV_CODEC_ID_MPEG2VIDEO;
    case MFX_TRACK_AAC:
        return AV_CODEC_ID_AAC;
    case MFX_TRACK_MPEGA:
        return AV_CODEC_ID_MP3;
    default:
        return AV_CODEC_ID_NONE;
    }
}

int GetAVCodecProfileFromMFXCodecProfile(mfxTrackType trackType, mfxU16 codecProfile)
{
    switch(trackType)
    {
    case MFX_TRACK_H264:
        switch(codecProfile)
        {
        case MFX_PROFILE_AVC_BASELINE:
            return FF_PROFILE_H264_BASELINE;
        case MFX_PROFILE_AVC_MAIN:
            return FF_PROFILE_H264_MAIN;
        case MFX_PROFILE_AVC_EXTENDED:
            return FF_PROFILE_H264_EXTENDED;
        case MFX_PROFILE_AVC_HIGH:
            return FF_PROFILE_H264_HIGH;
        case MFX_PROFILE_AVC_CONSTRAINED_BASELINE:
            return FF_PROFILE_H264_CONSTRAINED_BASELINE;
        default:
            break;
        }
        break;
    case MFX_TRACK_MPEG2V:
        switch(codecProfile)
        {
        case MFX_PROFILE_MPEG2_SIMPLE:
            return FF_PROFILE_MPEG2_SIMPLE;
        case MFX_PROFILE_MPEG2_MAIN:
            return FF_PROFILE_MPEG2_MAIN;
        case MFX_PROFILE_MPEG2_HIGH:
            return FF_PROFILE_MPEG2_HIGH;
        default:
            break;
        }
        break;
    case MFX_TRACK_AAC:
        switch(codecProfile)
        {
        case MFX_PROFILE_AAC_LC:
            return FF_PROFILE_AAC_LOW;
        case MFX_PROFILE_AAC_LTP:
            return FF_PROFILE_AAC_LTP;
        case MFX_PROFILE_AAC_MAIN:
            return FF_PROFILE_AAC_MAIN;
        case MFX_PROFILE_AAC_SSR:
            return FF_PROFILE_AAC_SSR;
        case MFX_PROFILE_AAC_HE:
            return FF_PROFILE_AAC_HE;
        case MFX_PROFILE_AAC_PS:
            return FF_PROFILE_AAC_HE_V2;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return FF_PROFILE_UNKNOWN;
}

mfxStatus SetStreamInfo(mfxHDL mux, mfxU16 trackNum)
{
    mfxStatus        sts;
    _mfxMuxerFFMPEG *inMux;
    AVCodecContext  *codecContext;

    sts            = MFX_ERR_NONE;

    inMux = (_mfxMuxerFFMPEG*)mux;
    if(inMux == NULL)
        return MFX_ERR_NULL_PTR;

    //TODO: convert and set duration
    codecContext = inMux->streams[trackNum]->codec;
    if (inMux->streamParams.TrackInfo[trackNum]->Type & MFX_TRACK_ANY_VIDEO)
    {
        codecContext->codec_type     = AVMEDIA_TYPE_VIDEO;
        codecContext->profile        = GetAVCodecProfileFromMFXCodecProfile(inMux->streamParams.TrackInfo[trackNum]->Type, inMux->streamParams.TrackInfo[trackNum]->VideoParam.CodecProfile);
        codecContext->codec_id       = GetAVCodecIDFromMFXTrackType(inMux->streamParams.TrackInfo[trackNum]->Type);
        codecContext->width          = inMux->streamParams.TrackInfo[trackNum]->VideoParam.FrameInfo.Width;
        codecContext->height         = inMux->streamParams.TrackInfo[trackNum]->VideoParam.FrameInfo.Height;
        codecContext->time_base.num  = inMux->streamParams.TrackInfo[trackNum]->VideoParam.FrameInfo.FrameRateExtD;
        codecContext->time_base.den  = inMux->streamParams.TrackInfo[trackNum]->VideoParam.FrameInfo.FrameRateExtN;
        codecContext->pix_fmt        = AV_PIX_FMT_YUV420P;
        codecContext->gop_size       = inMux->streamParams.TrackInfo[trackNum]->VideoParam.GopPicSize;
    }
    else if (inMux->streamParams.TrackInfo[trackNum]->Type & MFX_TRACK_ANY_AUDIO)
    {
        codecContext->codec_type  = AVMEDIA_TYPE_AUDIO;
        codecContext->profile     = GetAVCodecProfileFromMFXCodecProfile(inMux->streamParams.TrackInfo[trackNum]->Type, inMux->streamParams.TrackInfo[trackNum]->AudioParam.CodecProfile);
        codecContext->codec_id = GetAVCodecIDFromMFXTrackType(inMux->streamParams.TrackInfo[trackNum]->Type);
        codecContext->bits_per_coded_sample = inMux->streamParams.TrackInfo[trackNum]->AudioParam.BitPerSample;
        codecContext->bit_rate = inMux->streamParams.TrackInfo[trackNum]->AudioParam.Bitrate;
        codecContext->channels = inMux->streamParams.TrackInfo[trackNum]->AudioParam.NumChannel;
        codecContext->sample_rate = inMux->streamParams.TrackInfo[trackNum]->AudioParam.SampleFrequency;
        codecContext->time_base.num = 1;
        codecContext->time_base.den = codecContext->sample_rate;
        codecContext->sample_fmt = AV_SAMPLE_FMT_S16;

        if (inMux->streamParams.TrackInfo[trackNum]->Type == MFX_TRACK_AAC)
            codecContext->frame_size = 1024;
        else if (inMux->streamParams.TrackInfo[trackNum]->Type == MFX_TRACK_MPEGA)
            codecContext->frame_size = 1152; // actually, 384, 576, or 1152 samples in depends on MPEG version and layer
    }
    if (inMux->streamParams.TrackInfo[trackNum]->HeaderLength > 0)
    {
        codecContext->extradata_size = inMux->streamParams.TrackInfo[trackNum]->HeaderLength;
        codecContext->extradata = (uint8_t*) av_malloc (codecContext->extradata_size);
        if (!codecContext->extradata)
            return MFX_ERR_MEMORY_ALLOC;

        MSDK_MEMCPY(codecContext->extradata, inMux->streamParams.TrackInfo[trackNum]->Header,
                 (inMux->streamParams.TrackInfo[trackNum]->HeaderLength < codecContext->extradata_size ? inMux->streamParams.TrackInfo[trackNum]->HeaderLength : codecContext->extradata_size) );

    }
    if (sts == MFX_ERR_NONE)
    {
        if(inMux->formatContext->oformat->flags & AVFMT_GLOBALHEADER)
            codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    return sts;
}

mfxStatus MFXMuxer_Init(mfxStreamParams* par, mfxDataIO *data_io, mfxMuxer *mux)
{
    mfxStatus        sts;
    _mfxMuxerFFMPEG *outMux;
    AVOutputFormat  *outputFormat;
    AVFormatContext *formatContext;
    mfxU8           *contextBuffer;
    mfxU32           bufferSize;
    const char      *formatName;
    mfxHDL           hdl;
    mfxU16           i;

    sts            = MFX_ERR_NONE;
    outMux        = NULL;
    outputFormat  = NULL;
    formatContext = NULL;
    contextBuffer = NULL;
    formatName    = "";

    av_register_all();

    if (mux == NULL)
        return MFX_ERR_NULL_PTR;

    if (data_io == NULL)
        return MFX_ERR_NULL_PTR;

    if (data_io->pthis == NULL || data_io->Write == NULL) //TODO: add Seek
        return MFX_ERR_NULL_PTR;

    if (par->SystemType == MFX_MPEG4_SYSTEM_STREAM)
    {
        formatName = "mp4";
    }
    else if (par->SystemType == MFX_MPEG2_TRANSPORT_STREAM)
    {
        formatName = "mpegts";
    }
    else
        return MFX_ERR_UNSUPPORTED;

    if (par->NumTracks == 0)
        return MFX_ERR_UNKNOWN;

    if (par->TrackInfo == NULL)
        return MFX_ERR_NULL_PTR;

    for (i = 0; i < par->NumTracks; i++)
    {
        if (GetAVCodecIDFromMFXTrackType(par->TrackInfo[i]->Type) == AV_CODEC_ID_NONE)
            return MFX_ERR_UNSUPPORTED;
    }

    outMux = (_mfxMuxerFFMPEG*) av_malloc(sizeof(_mfxMuxerFFMPEG));
    if(!outMux)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    memset((void*)outMux, 0, sizeof(_mfxMuxerFFMPEG));

    bufferSize = 4*1024;
    contextBuffer = (mfxU8*) av_malloc(bufferSize);
    if(contextBuffer == NULL)
        sts = MFX_ERR_MEMORY_ALLOC;

    if (sts == MFX_ERR_NONE)
    {
        outMux->dataIO = data_io;
        hdl = outMux->dataIO;
        outMux->ioContext = avio_alloc_context(contextBuffer, bufferSize, AVIO_FLAG_WRITE, hdl, NULL, ffmpeg_write, ffmpeg_seek);
        if(!outMux->ioContext)
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        outputFormat = av_guess_format(formatName, NULL, NULL);
        if(outputFormat == NULL)
            sts = MFX_ERR_UNKNOWN;
    }
    if (sts == MFX_ERR_NONE)
    {
        if(avformat_alloc_output_context2(&formatContext, NULL, formatName, NULL) < 0)
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        formatContext->oformat = outputFormat;
        formatContext->pb = outMux->ioContext;
        formatContext->flags = AVFMT_FLAG_CUSTOM_IO;
        outMux->formatContext = formatContext;
    }
    if (sts == MFX_ERR_NONE)
    {
        outMux->streams = (AVStream**) malloc(par->NumTracks*sizeof(AVStream*));
        if (!outMux->streams)
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        outMux->streamParams.NumTracks = outMux->streamParams.NumTracksAllocated = par->NumTracks;
        outMux->streamParams.TrackInfo = (mfxTrackInfo **) malloc(sizeof(mfxTrackInfo*)*outMux->streamParams.NumTracks);
        if (!outMux->streamParams.TrackInfo)
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        for (i=0; i < outMux->streamParams.NumTracks; i++)
        {
            outMux->streamParams.TrackInfo[i] = (mfxTrackInfo *) malloc(sizeof(mfxTrackInfo));
            if (!outMux->streamParams.TrackInfo[i])
            {
                sts = MFX_ERR_MEMORY_ALLOC;
                break;
            }
            memset((void*)outMux->streamParams.TrackInfo[i], 0, sizeof(mfxTrackInfo));
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        for (i = 0; i < par->NumTracks; i++)
        {
            outMux->streamParams.Duration = par->Duration;
            outMux->streamParams.SystemType = par->SystemType;
            outMux->streamParams.TrackInfo[i]->Type = par->TrackInfo[i]->Type;
            outMux->streamParams.TrackInfo[i]->HeaderLength = par->TrackInfo[i]->HeaderLength;
            MSDK_MEMCPY(outMux->streamParams.TrackInfo[i]->Header, par->TrackInfo[i]->Header,
                        (par->TrackInfo[i]->HeaderLength < outMux->streamParams.TrackInfo[i]->HeaderLength ? par->TrackInfo[i]->HeaderLength : outMux->streamParams.TrackInfo[i]->HeaderLength));
            if (outMux->streamParams.TrackInfo[i]->Type & MFX_TRACK_ANY_AUDIO)
            {
                outMux->streamParams.TrackInfo[i]->AudioParam.CodecId = par->TrackInfo[i]->AudioParam.CodecId;
                outMux->streamParams.TrackInfo[i]->AudioParam.NumChannel = par->TrackInfo[i]->AudioParam.NumChannel;
                outMux->streamParams.TrackInfo[i]->AudioParam.SampleFrequency = par->TrackInfo[i]->AudioParam.SampleFrequency;
                outMux->streamParams.TrackInfo[i]->AudioParam.Bitrate = par->TrackInfo[i]->AudioParam.Bitrate;
                outMux->streamParams.TrackInfo[i]->AudioParam.BitPerSample = par->TrackInfo[i]->AudioParam.BitPerSample;

                if (outMux->streamParams.TrackInfo[i]->Type == MFX_TRACK_AAC && outMux->streamParams.SystemType == MFX_MPEG4_SYSTEM_STREAM)
                {
                    outMux->bsfContext = av_bitstream_filter_init("aac_adtstoasc");
                }
            }
            else if (outMux->streamParams.TrackInfo[i]->Type & MFX_TRACK_ANY_VIDEO)
            {
                outMux->streamParams.TrackInfo[i]->VideoParam.CodecId = par->TrackInfo[i]->VideoParam.CodecId;
                outMux->streamParams.TrackInfo[i]->VideoParam.FrameInfo.Width = par->TrackInfo[i]->VideoParam.FrameInfo.Width;
                outMux->streamParams.TrackInfo[i]->VideoParam.FrameInfo.Height = par->TrackInfo[i]->VideoParam.FrameInfo.Height;
                outMux->streamParams.TrackInfo[i]->VideoParam.FrameInfo.FrameRateExtN = par->TrackInfo[i]->VideoParam.FrameInfo.FrameRateExtN;
                outMux->streamParams.TrackInfo[i]->VideoParam.FrameInfo.FrameRateExtD = par->TrackInfo[i]->VideoParam.FrameInfo.FrameRateExtD;
                outMux->streamParams.TrackInfo[i]->VideoParam.FrameInfo.ChromaFormat = par->TrackInfo[i]->VideoParam.FrameInfo.ChromaFormat;
                outMux->streamParams.TrackInfo[i]->VideoParam.GopPicSize = par->TrackInfo[i]->VideoParam.GopPicSize;
            }
            outMux->streams[i] = avformat_new_stream(outMux->formatContext, NULL);
            if (outMux->streams[i] == NULL)
                sts = MFX_ERR_UNKNOWN;
            sts = SetStreamInfo(outMux, i);
            outMux->streams[i]->nb_frames = 0;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        if(avformat_write_header(outMux->formatContext, NULL) < 0)
        {
            sts = MFX_ERR_UNKNOWN;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        *mux = (mfxMuxer) outMux;
    }
    else
    {
        if (outMux->ioContext != NULL)
            av_free(outMux->ioContext);
        if (contextBuffer != NULL)
            av_free(contextBuffer);
        if (outMux != NULL)
            av_free(outMux);
    }
    return sts;
}

mfxStatus MFXMuxer_Close(mfxMuxer mux)
{
    mfxStatus        sts;
    _mfxMuxerFFMPEG *inMux;
    mfxU16           i;

    sts = MFX_ERR_NONE;

    inMux = (_mfxMuxerFFMPEG*)mux;
    if(inMux == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    if(inMux->bsfContext)
        av_bitstream_filter_close(inMux->bsfContext);

    if (sts == MFX_ERR_NONE )
    {
        if (av_write_trailer(inMux->formatContext) < 0)
            sts = MFX_ERR_UNKNOWN;
    }

    /* Close each codec. */
    if (inMux->streams != NULL)
    {
        for (i = 0; i < inMux->streamParams.NumTracks; i++)
        {
            if (inMux->streams[i]->codec != NULL)
            {
                if (inMux->streams[i]->codec->extradata_size > 0)
                {
                    av_free(inMux->streams[i]->codec->extradata);
                }
                if(avcodec_close(inMux->streams[i]->codec) < 0)
                {
                    sts = MFX_ERR_UNKNOWN;
                }
            }
        }
        free(inMux->streams);
    }

    /* Free the streams. */
    for (i = 0; i < inMux->formatContext->nb_streams; i++)
    {
        if (inMux->formatContext->streams[i] != NULL)
        {
            if (inMux->formatContext->streams[i]->codec != NULL)
                av_freep(&inMux->formatContext->streams[i]->codec);
            av_freep(&inMux->formatContext->streams[i]);
        }
    }
    if (inMux->ioContext->buffer != NULL)
    {
        av_free(inMux->ioContext->buffer);
    }
    /* free the stream */
    if (inMux->formatContext->pb != NULL)
        av_free(inMux->formatContext->pb);
    //if (inMux->avContext)
    //    avformat_free_context(inMux->avContext);

    if (inMux->streamParams.TrackInfo != NULL)
    {
        for (i = 0; i < inMux->streamParams.NumTracksAllocated; i++)
        {
            if (inMux->streamParams.TrackInfo[i] != NULL)
                free(inMux->streamParams.TrackInfo[i]);
        }
        free(inMux->streamParams.TrackInfo);
    }

    av_free(inMux);

    return sts;
}

mfxStatus MFXMuxer_PutBitstream(mfxMuxer mux, mfxU32 track_num, mfxBitstream *bs, mfxU64 duration)
{
    mfxStatus        sts;
    AVPacket         packet;
    _mfxMuxerFFMPEG *inMux;
    mfxU8           *outBuffer;
    mfxI32           outBufferSize;

    inMux           = NULL;
    sts             = MFX_ERR_NONE;

    if (bs == NULL)
        return MFX_ERR_NULL_PTR;

    inMux = (_mfxMuxerFFMPEG*)mux;
    if (inMux == NULL)
        return MFX_ERR_NULL_PTR;

    if (track_num >= inMux->streamParams.NumTracks)
        return MFX_ERR_UNKNOWN;

    av_init_packet(&packet);

    if (inMux->streams[track_num] == NULL)
    {
        sts = MFX_ERR_NULL_PTR;
    }
    if (sts == MFX_ERR_NONE)
    {
        if (inMux->streams[track_num]->codec == NULL)
        {
            sts = MFX_ERR_NULL_PTR;
        }
    }

    if (sts == MFX_ERR_NONE)
    {
        if (duration > 0)
        {
            packet.duration = (int) av_rescale_q(duration, q, inMux->streams[track_num]->time_base);
        }

        if (bs->TimeStamp == MFX_TIMESTAMP_UNKNOWN)
        {
            packet.pts = AV_NOPTS_VALUE;
        }
        else
        {
            packet.pts = av_rescale_q(bs->TimeStamp, q, inMux->streams[track_num]->time_base);
        }

        if (bs->DecodeTimeStamp == MFX_TIMESTAMP_UNKNOWN)
        {
            packet.dts = AV_NOPTS_VALUE;
        }
        else
        {
            packet.dts = av_rescale_q(bs->DecodeTimeStamp, q, inMux->streams[track_num]->time_base);
        }
        if ( (track_num < sizeof(inMux->lastDts) / sizeof(*inMux->lastDts)) && (packet.dts != AV_NOPTS_VALUE) )
        {
            if ( (inMux->streams[track_num]->nb_frames != 0) && (packet.dts <= inMux->lastDts[track_num]) ) {
                packet.dts  = inMux->lastDts[track_num] + 1;
            }
        }

        if ( (packet.pts != AV_NOPTS_VALUE) && (packet.dts != AV_NOPTS_VALUE) )
        {
            if (packet.pts + inMux->ptsToDtsOffset < packet.dts) {
                inMux->ptsToDtsOffset = packet.dts - packet.pts;
            }
            packet.pts += inMux->ptsToDtsOffset;
        }

        if (inMux->streamParams.TrackInfo[track_num]->Type & MFX_TRACK_ANY_VIDEO)
        {
            packet.stream_index = inMux->streams[track_num]->index;
            packet.data = bs->Data + bs->DataOffset;
            packet.size = bs->DataLength;
            if(bs->FrameType & MFX_FRAMETYPE_I)
                packet.flags = AV_PKT_FLAG_KEY;
            else
                packet.flags = 0;
        }
        else if (inMux->streamParams.TrackInfo[track_num]->Type & MFX_TRACK_ANY_AUDIO)
        {
            if (inMux->streamParams.TrackInfo[track_num]->Type == MFX_TRACK_AAC && inMux->streamParams.SystemType == MFX_MPEG4_SYSTEM_STREAM)
            {
                av_bitstream_filter_filter(inMux->bsfContext, inMux->formatContext->streams[track_num]->codec,
                                           NULL, &outBuffer, &outBufferSize, bs->Data + bs->DataOffset, bs->DataLength, AV_PKT_FLAG_KEY);
            }
            else
            {
                outBuffer = bs->Data + bs->DataOffset;
                outBufferSize = bs->DataLength;
            }
            packet.stream_index = inMux->streams[track_num]->index;
            packet.data = outBuffer;
            packet.size = outBufferSize;
            packet.flags |= AV_PKT_FLAG_KEY;
        }
        if (av_interleaved_write_frame(inMux->formatContext, &packet))
        {
            sts = MFX_ERR_UNKNOWN;
        }
        inMux->streams[track_num]->nb_frames++;
        if ( (track_num < sizeof(inMux->lastDts) / sizeof(*inMux->lastDts)) && (packet.dts != AV_NOPTS_VALUE) )
        {
            inMux->lastDts[track_num] = packet.dts;
        }
    }

    return sts;
}
