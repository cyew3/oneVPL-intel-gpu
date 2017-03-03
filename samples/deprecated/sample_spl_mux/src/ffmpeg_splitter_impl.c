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

#include "ffmpeg_splitter_impl.h"
#include "ffmpeg_reader_writer.h"

static AVRational q = {1, 90000};

mfxStatus MFXSplitter_Init(mfxDataIO *data_io, mfxSplitter *spl)
{
    mfxStatus sts;
    int res;
    _mfxSplitter *outSpl;
    mfxU8 *data;
    mfxHDL hdl;
    mfxU32 readBytes;
    mfxU32 extraBufferSize, bufferSize;
    AVProbeData probeData;
    AVPacket *pkt;
    mfxU16 i;

    sts = MFX_ERR_NONE;
    hdl = NULL;

    av_register_all();

    if (spl == NULL)
        return MFX_ERR_NULL_PTR;

    if (data_io == NULL)
        return MFX_ERR_NULL_PTR;

    if (data_io->pthis == NULL || data_io->Read == NULL) //TODO: add Seek
        return MFX_ERR_NULL_PTR;

    outSpl = (_mfxSplitter*) malloc(sizeof(_mfxSplitter));
    if(!outSpl)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    outSpl->dataIO = data_io;
    hdl = outSpl->dataIO;
    outSpl->isStreamInfoFound = 0;
    outSpl->adtsMuxer = NULL;

    outSpl->bsfContext = NULL;
    for (i = 0; i < QUEUE_MAX_SIZE; i++)
    {
        outSpl->outBufs[i] = NULL;
        pkt = &(outSpl->inUsePackets[i]);
        pkt->data = NULL;
        pkt->size = 0;
    }
    bufferSize = 1024*4;
    extraBufferSize = bufferSize + AVPROBE_PADDING_SIZE;
    data = (mfxU8*) av_malloc(extraBufferSize);
    if(data == NULL)
        sts = MFX_ERR_MEMORY_ALLOC;

    if (sts == MFX_ERR_NONE)
    {
        memset(data, 0, extraBufferSize);

        outSpl->ioContext = avio_alloc_context(data, bufferSize, 0, hdl, ffmpeg_read, NULL, ffmpeg_seek);
        if (!outSpl->ioContext)
            sts = MFX_ERR_MEMORY_ALLOC;
    }

    if (sts == MFX_ERR_NONE)
    {
        outSpl->formatContext = avformat_alloc_context();
        if (!outSpl->formatContext)
            sts = MFX_ERR_MEMORY_ALLOC;
    }
    if (sts == MFX_ERR_NONE)
    {
        outSpl->formatContext->pb = outSpl->ioContext;

        readBytes = ffmpeg_read(hdl, data, bufferSize);
        ffmpeg_seek(hdl, 0, SEEK_SET);
        // Now we set the ProbeData-structure for av_probe_input_format:

        memset(&probeData,0,sizeof(probeData));
        probeData.buf = data;
        probeData.buf_size = bufferSize;
        probeData.filename = "";

        // Determine the input-format:
        outSpl->formatContext->iformat = av_probe_input_format(&probeData, AVFMT_NOFILE);
        if (outSpl->formatContext->iformat == NULL)
        {
            outSpl->formatContext->pb = NULL;
            sts = MFX_ERR_UNKNOWN;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        outSpl->formatContext->flags = AVFMT_FLAG_CUSTOM_IO;

        res = avformat_open_input(&outSpl->formatContext, "", NULL, NULL);
        if(res < 0)
        {
            sts = MFX_ERR_UNKNOWN;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        outSpl->adtsMuxer = (adtsMuxer*) malloc(outSpl->formatContext->nb_streams*sizeof(adtsMuxer));
        if(!outSpl->adtsMuxer)
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        for (i = 0; i < outSpl->formatContext->nb_streams; i++)
        {
            outSpl->adtsMuxer[i] = NULL;
        }
    }
    if (sts == MFX_ERR_NONE)
    {
        if (strncmp(outSpl->formatContext->iformat->name,"mov,mp4",7) == 0)
            outSpl->bsfContext = av_bitstream_filter_init("h264_mp4toannexb");
        *spl = (mfxSplitter) outSpl;
    }
    else
    {
        MFXSplitter_Close((mfxSplitter)outSpl);
    }

    return sts;
}

mfxStatus MFXSplitter_Close(mfxSplitter spl)
{
    mfxU16 i;
    _mfxSplitter *inSpl;

    inSpl = (_mfxSplitter*)spl;
    if(inSpl == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    if (inSpl->adtsMuxer)
    {
        for (i = 0; inSpl->formatContext && i < inSpl->formatContext->nb_streams; i++)
        {
            if (inSpl->formatContext->streams[i] &&
                inSpl->formatContext->streams[i]->codec &&
                inSpl->formatContext->streams[i]->codec->codec_id == AV_CODEC_ID_AAC)
            {
                CloseAdtsMuxer(inSpl->adtsMuxer[i]);
            }
        }
        free(inSpl->adtsMuxer);
        inSpl->adtsMuxer = NULL;
    }

    if(inSpl->bsfContext != NULL)
        av_bitstream_filter_close(inSpl->bsfContext);

    if (inSpl->ioContext != NULL)
    {
        if (inSpl->ioContext->buffer != NULL)
            av_free(inSpl->ioContext->buffer);
        av_free(inSpl->ioContext);
    }
    for (i = 0; inSpl->formatContext && i < inSpl->formatContext->nb_streams; i++)
    {
        if (inSpl->formatContext->streams[i] &&
            inSpl->formatContext->streams[i]->codec)
            avcodec_close(inSpl->formatContext->streams[i]->codec);
    }

    if(inSpl->formatContext != NULL)
        avformat_close_input(&inSpl->formatContext);

    free(inSpl);

    return MFX_ERR_NONE;
}

//TODO: add Reset with mfxStreamParams
mfxStatus MFXSplitter_GetBitstream(mfxSplitter spl, mfxU32 *track_num, mfxBitstream *bs)
{
    mfxStatus sts;
    mfxI32 ret;
    _mfxSplitter *inSpl;
    AVPacket *pkt;
    mfxI32 outBufferSize;
    mfxU16 i;
    int isNoPkt;
    mfxU8** outBuffer;

    isNoPkt = 1;
    sts = MFX_ERR_NONE;

    inSpl = (_mfxSplitter*)spl;
    if (inSpl == NULL)
        return MFX_ERR_NULL_PTR;

    if (track_num == NULL)
        return MFX_ERR_NULL_PTR;

    if(bs == NULL)
        return MFX_ERR_NULL_PTR;

    outBuffer = NULL;
    pkt = NULL;

    for (i = 0; i < QUEUE_MAX_SIZE; i++)
    {
        pkt = &(inSpl->inUsePackets[i]);
        if (pkt->data == NULL)
        {
            break;
        }
        //All packets from the queue are in use
        if (i == (QUEUE_MAX_SIZE - 1))
            return MFX_ERR_NOT_ENOUGH_BUFFER;
    }
    for (i = 0; i < QUEUE_MAX_SIZE; i++)
    {
        if (inSpl->outBufs[i] == NULL)
        {
            outBuffer = &inSpl->outBufs[i];
            break;
        }
        if (i == (QUEUE_MAX_SIZE - 1))
            return MFX_ERR_UNKNOWN;
    }

    av_init_packet(pkt);
    if (!pkt)
        return MFX_ERR_UNKNOWN;

    while (isNoPkt)
    {
        ret = av_read_frame(inSpl->formatContext, pkt);
        if (ret < 0)
        {
            if(ret == -AVERROR(AVERROR_EOF))
            {
                bs->Data = NULL;
                bs->DataFlag = MFX_BITSTREAM_EOS;
                bs->MaxLength = 0;
                bs->DataLength = 0;
                return MFX_ERR_MORE_DATA;
            }
            else if(ret == AVERROR(EAGAIN))
            {
                MSDK_SLEEP(1);
                isNoPkt = 1;
            }
            else
                return MFX_ERR_UNKNOWN;
        }
        else
        {
            if (!inSpl->formatContext->streams[pkt->stream_index]->nb_frames && !(pkt->flags & AV_PKT_FLAG_KEY)) //first frame should be the keyframe
            {
                isNoPkt = 1;
            }
            else
                isNoPkt = 0;
        }
    }

    inSpl->formatContext->streams[pkt->stream_index]->nb_frames++;

    bs->Data = (mfxU8*)pkt->data;
    bs->MaxLength = pkt->size;
    bs->DataLength = pkt->size;
    bs->DataOffset = 0;

    if(inSpl->formatContext->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_H264
        && strncmp(inSpl->formatContext->iformat->name,"mov,mp4",7) == 0)
    {
        ret = av_bitstream_filter_filter(inSpl->bsfContext, inSpl->formatContext->streams[pkt->stream_index]->codec,
              NULL, outBuffer, &outBufferSize, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
        if (ret > 0)
        {
            bs->Data = *outBuffer;
            bs->MaxLength = outBufferSize;
            bs->DataLength = outBufferSize;
            bs->DataOffset = 0;

            av_free_packet(pkt);
            pkt->data = NULL;
        }
        else
        {
            inSpl->outBufs[i] = NULL; // no need to free, outBuffer was not allocated
        }
    }
    else if(inSpl->formatContext->streams[pkt->stream_index]->codec->codec_id == AV_CODEC_ID_AAC)
    {
        mfxBitstream adtsBs;
        adtsBs.Data = NULL;
        adtsBs.MaxLength = 0;
        adtsBs.DataLength = 0;

        if (!inSpl->adtsMuxer || !inSpl->adtsMuxer[pkt->stream_index])
            sts = MFX_ERR_UNKNOWN;
        if (sts == MFX_ERR_NONE)
        {
            sts = GetAdtsBitstream(inSpl->adtsMuxer[pkt->stream_index], pkt, &adtsBs);
        }
        if (sts == MFX_ERR_NONE)
        {
            int streamIndex;
            int64_t pts;

            streamIndex = pkt->stream_index;
            pts = pkt->pts;
            av_free_packet(pkt);
            pkt->data = NULL;

            bs->Data = adtsBs.Data;
            bs->MaxLength = adtsBs.MaxLength;
            bs->DataLength = adtsBs.DataLength;
            bs->DataOffset = 0;
            pkt->pts = pts;
            pkt->stream_index = streamIndex;
        }
    }
    if (pkt->pts == AV_NOPTS_VALUE)
        bs->TimeStamp = (mfxU64) MFX_TIMESTAMP_UNKNOWN;
    else
        bs->TimeStamp = av_rescale_q(pkt->pts, inSpl->formatContext->streams[pkt->stream_index]->time_base, q);
    if (pkt->dts == AV_NOPTS_VALUE)
        bs->DecodeTimeStamp = MFX_TIMESTAMP_UNKNOWN;
    else
        bs->DecodeTimeStamp = av_rescale_q(pkt->dts, inSpl->formatContext->streams[pkt->stream_index]->time_base, q);

    *track_num = pkt->stream_index;
    return sts;
}

mfxStatus MFXSplitter_ReleaseBitstream(mfxSplitter spl, mfxBitstream *bs)
{
    _mfxSplitter *inSpl = (_mfxSplitter*)spl;
    AVPacket *pkt;
    mfxU16 i;

    if (inSpl == NULL)
        return MFX_ERR_NULL_PTR;

    for (i = 0; i < QUEUE_MAX_SIZE; i++)
    {
        pkt = &(inSpl->inUsePackets[i]);
        if (pkt->data && pkt->data == bs->Data)
        {
            av_free_packet(pkt);
            break;
        }
    }
    for (i = 0; i < QUEUE_MAX_SIZE; i++)
    {
        if (inSpl->outBufs[i] && inSpl->outBufs[i] == bs->Data)
        {
            av_free(inSpl->outBufs[i]);
            inSpl->outBufs[i] = NULL;
            break;
        }
    }
    bs->Data = NULL;
    bs->DataLength = 0;

    return MFX_ERR_NONE;
}

mfxStatus MFXSplitter_GetInfo(mfxSplitter spl, mfxStreamParams *par)
{
    mfxStatus sts;
    _mfxSplitter *inSpl;
    mfxI32 ret;
    mfxU32 numTrack;
    AVRational timeBase = {1, AV_TIME_BASE};

    sts = MFX_ERR_NONE;

    if (spl == NULL || par == NULL)
        return MFX_ERR_NULL_PTR;

    inSpl = (_mfxSplitter*)spl;

    if (inSpl->isStreamInfoFound == 0)
    {
        ret = avformat_find_stream_info(inSpl->formatContext, NULL);
        if(ret < 0)
        {
            return MFX_ERR_UNKNOWN;
        }
        par->NumTracks = (mfxU16) inSpl->formatContext->nb_streams;
        inSpl->isStreamInfoFound = 1;
        return MFX_ERR_NONE;
    }
    if (par->TrackInfo == NULL || par->NumTracks == 0)
    {
        par->NumTracks = (mfxU16) inSpl->formatContext->nb_streams;
        return MFX_ERR_NONE;
    }
    par->Duration = av_rescale_q(inSpl->formatContext->duration, timeBase, q);
    par->SystemType = GetSystemStreamTypeByExtension(inSpl->formatContext->iformat->name);
    if (par->NumTracksAllocated >= inSpl->formatContext->nb_streams)
    {
        for (numTrack = 0; numTrack < inSpl->formatContext->nb_streams; numTrack++ )
        {
            par->TrackInfo[numTrack]->Type = GetTrackTypeByCodecID(inSpl->formatContext->streams[numTrack]->codec->codec_id);
            par->TrackInfo[numTrack]->SID = inSpl->formatContext->streams[numTrack]->id;
            par->TrackInfo[numTrack]->Enable = 1;
            inSpl->formatContext->streams[numTrack]->nb_frames = 0;
            if (par->TrackInfo[numTrack]->Type & MFX_TRACK_ANY_AUDIO)
            {
                memset(&par->TrackInfo[numTrack]->AudioParam, 0, sizeof(par->TrackInfo[numTrack]->AudioParam));
                par->TrackInfo[numTrack]->AudioParam.NumChannel = (mfxU16) inSpl->formatContext->streams[numTrack]->codec->channels;
                par->TrackInfo[numTrack]->AudioParam.SampleFrequency = inSpl->formatContext->streams[numTrack]->codec->sample_rate;
                par->TrackInfo[numTrack]->AudioParam.Bitrate = inSpl->formatContext->streams[numTrack]->codec->bit_rate;
                par->TrackInfo[numTrack]->AudioParam.BitPerSample = (mfxU16) inSpl->formatContext->streams[numTrack]->codec->bits_per_coded_sample;
                par->TrackInfo[numTrack]->AudioParam.CodecId = GetMFXCodecIDFromAVCodecID(inSpl->formatContext->streams[numTrack]->codec->codec_id);
                par->TrackInfo[numTrack]->AudioParam.CodecProfile = GetMFXCodecProfileFromAVCodecProfile(inSpl->formatContext->streams[numTrack]->codec->codec_id, inSpl->formatContext->streams[numTrack]->codec->profile);
                if (inSpl->formatContext->streams[numTrack]->codec->codec_id == AV_CODEC_ID_AAC)
                {
                    sts = InitAdtsMuxer(inSpl->formatContext->streams[numTrack]->codec, &(inSpl->adtsMuxer[numTrack]));
                }
            }
            else if (par->TrackInfo[numTrack]->Type & MFX_TRACK_ANY_VIDEO)
            {
                memset(&par->TrackInfo[numTrack]->VideoParam, 0, sizeof(par->TrackInfo[numTrack]->VideoParam));
                par->TrackInfo[numTrack]->VideoParam.FrameInfo.Width = (mfxU16) inSpl->formatContext->streams[numTrack]->codec->width;
                par->TrackInfo[numTrack]->VideoParam.FrameInfo.Height = (mfxU16) inSpl->formatContext->streams[numTrack]->codec->height;
                par->TrackInfo[numTrack]->VideoParam.CodecId = GetMFXCodecIDFromAVCodecID(inSpl->formatContext->streams[numTrack]->codec->codec_id);
                if (inSpl->formatContext->streams[numTrack]->codec->profile == FF_PROFILE_UNKNOWN)
                    par->TrackInfo[numTrack]->VideoParam.CodecProfile = GetMFXCodecProfileFromAVCodecTag(inSpl->formatContext->streams[numTrack]->codec->codec_tag);
                else
                    par->TrackInfo[numTrack]->VideoParam.CodecProfile = GetMFXCodecProfileFromAVCodecProfile(inSpl->formatContext->streams[numTrack]->codec->codec_id, inSpl->formatContext->streams[numTrack]->codec->profile);
                if (inSpl->formatContext->streams[numTrack]->avg_frame_rate.den && inSpl->formatContext->streams[numTrack]->avg_frame_rate.num)
                {
                    par->TrackInfo[numTrack]->VideoParam.FrameInfo.FrameRateExtD = inSpl->formatContext->streams[numTrack]->avg_frame_rate.den;
                    par->TrackInfo[numTrack]->VideoParam.FrameInfo.FrameRateExtN = inSpl->formatContext->streams[numTrack]->avg_frame_rate.num;
                }
                else // try to take guessed framerate
                {
                    AVRational rFrameRate = av_stream_get_r_frame_rate(inSpl->formatContext->streams[numTrack]);
                    par->TrackInfo[numTrack]->VideoParam.FrameInfo.FrameRateExtD = rFrameRate.den;
                    par->TrackInfo[numTrack]->VideoParam.FrameInfo.FrameRateExtN = rFrameRate.num;
                }
                par->TrackInfo[numTrack]->VideoParam.GopPicSize = (mfxU16) inSpl->formatContext->streams[numTrack]->codec->gop_size;
            }
            par->TrackInfo[numTrack]->HeaderLength = 0;
            if(inSpl->formatContext->streams[numTrack]->codec->extradata_size > 0)
            {
                par->TrackInfo[numTrack]->HeaderLength = (mfxU16) inSpl->formatContext->streams[numTrack]->codec->extradata_size;
                MSDK_MEMCPY(par->TrackInfo[numTrack]->Header, inSpl->formatContext->streams[numTrack]->codec->extradata,
                           (inSpl->formatContext->streams[numTrack]->codec->extradata_size < MFX_TRACK_HEADER_MAX_SIZE ? inSpl->formatContext->streams[numTrack]->codec->extradata_size : MFX_TRACK_HEADER_MAX_SIZE) );
            }
        }
    }
    else
    {
        return MFX_ERR_MORE_DATA;
    }

    return sts;
}

mfxStatus MFXSplitter_Seek(mfxSplitter spl, mfxU64 timestamp)
{
    _mfxSplitter *inSpl;
    mfxU32 numTrack;
    mfxU64 rescaledTimestamp;

    inSpl = (_mfxSplitter*)spl;
    if (inSpl == NULL)
        return MFX_ERR_NULL_PTR;

    for (numTrack = 0; numTrack < inSpl->formatContext->nb_streams; numTrack++ )
    {
        rescaledTimestamp = av_rescale_q(timestamp, q, inSpl->formatContext->streams[numTrack]->time_base);
        if (inSpl->formatContext->streams[numTrack]->duration < 0)
            return MFX_ERR_UNKNOWN;
        if (rescaledTimestamp > (mfxU64) inSpl->formatContext->streams[numTrack]->duration)
            return MFX_ERR_UNKNOWN;
        if (av_seek_frame(inSpl->formatContext, numTrack, rescaledTimestamp, 0) < 0)
        {
            return MFX_ERR_UNKNOWN;
        }
        if (inSpl->formatContext->streams[numTrack]->codec && inSpl->formatContext->streams[numTrack]->codec->codec)
            avcodec_flush_buffers(inSpl->formatContext->streams[numTrack]->codec);
    }

    return MFX_ERR_NONE;
}

mfxU16 GetMFXCodecProfileFromAVCodecTag(mfxU32 codecTag)
{
    switch(codecTag)
    {
    case 0x33564D57:    // "WMV3"
        return MFX_PROFILE_VC1_MAIN;
    case 0x31435657:    // "WVC1"
        return MFX_PROFILE_VC1_ADVANCED;
    default:
        return MFX_PROFILE_UNKNOWN;
    }
}

mfxU16 GetMFXCodecProfileFromAVCodecProfile(enum AVCodecID codecID, int codecProfile)
{
    switch(codecID)
    {
    case AV_CODEC_ID_H264:
        switch(codecProfile)
        {
        case FF_PROFILE_H264_BASELINE:
            return MFX_PROFILE_AVC_BASELINE;
        case FF_PROFILE_H264_MAIN:
            return MFX_PROFILE_AVC_MAIN;
        case FF_PROFILE_H264_EXTENDED:
            return MFX_PROFILE_AVC_EXTENDED;
        case FF_PROFILE_H264_HIGH:
            return MFX_PROFILE_AVC_HIGH;
        case FF_PROFILE_H264_CONSTRAINED_BASELINE:
            return MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
        default:
            break;
        }
        break;
    case AV_CODEC_ID_MPEG2VIDEO:
        switch(codecProfile)
        {
        case FF_PROFILE_MPEG2_SIMPLE:
            return MFX_PROFILE_MPEG2_SIMPLE;
        case FF_PROFILE_MPEG2_MAIN:
            return MFX_PROFILE_MPEG2_MAIN;
        case FF_PROFILE_MPEG2_HIGH:
            return MFX_PROFILE_MPEG2_HIGH;
        default:
            break;
        }
        break;
    case AV_CODEC_ID_AAC:
        switch(codecProfile)
        {
        case FF_PROFILE_AAC_LOW:
            return MFX_PROFILE_AAC_LC;
        case FF_PROFILE_AAC_LTP:
            return MFX_PROFILE_AAC_LTP;
        case FF_PROFILE_AAC_MAIN:
            return MFX_PROFILE_AAC_MAIN;
        case FF_PROFILE_AAC_SSR:
            return MFX_PROFILE_AAC_SSR;
        case FF_PROFILE_AAC_HE:
            return MFX_PROFILE_AAC_HE;
        case FF_PROFILE_AAC_HE_V2:
            return MFX_PROFILE_AAC_PS;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return MFX_PROFILE_UNKNOWN;
}

mfxSystemStreamType GetSystemStreamTypeByExtension(const char* extension)
{
    if(strcmp(extension,"mpegts") == 0) return MFX_MPEG2_TRANSPORT_STREAM;
    if(strncmp(extension,"mov,mp4",7) == 0) return MFX_MPEG4_SYSTEM_STREAM;
    if(strcmp(extension,"ivf") == 0) return MFX_IVF_STREAM;
#ifdef ALL_FORMATS
    if(strcmp(extension,"asf") == 0) return MFX_ASF_STREAM;
    if (strlen(extension) != 0) return (mfxSystemStreamType) !MFX_UNDEF_STREAM;
#endif
    return MFX_UNDEF_STREAM;
}

mfxU32 GetMFXCodecIDFromAVCodecID(enum AVCodecID codecID)
{
    switch(codecID)
    {
    case AV_CODEC_ID_H264:
        return MFX_CODEC_AVC;
    case AV_CODEC_ID_MPEG2VIDEO:
        return MFX_CODEC_MPEG2;
    case AV_CODEC_ID_MP2:
        return MFX_CODEC_MP3;
    case AV_CODEC_ID_AAC:
        return MFX_CODEC_AAC;
    case AV_CODEC_ID_MP3:
        return MFX_CODEC_MP3;
#ifdef ALL_FORMATS
    case AV_CODEC_ID_VC1:
        return MFX_CODEC_VC1;
#endif
    default:
        return 0;
    }
}

mfxTrackType GetTrackTypeByCodecID(enum AVCodecID codecID)
{
    switch(codecID)
    {
    case AV_CODEC_ID_H264:
        return MFX_TRACK_H264;
    case AV_CODEC_ID_MPEG2VIDEO:
        return MFX_TRACK_MPEG2V;
    case AV_CODEC_ID_MP2:
        return MFX_TRACK_MPEGA;
    case AV_CODEC_ID_VP8:
        return MFX_TRACK_VP8;
    case AV_CODEC_ID_AAC:
        return MFX_TRACK_AAC;
    case AV_CODEC_ID_MP3:
        return MFX_TRACK_MPEGA;
#ifdef ALL_FORMATS
    case AV_CODEC_ID_VC1:
        return MFX_TRACK_VC1;
#endif
    default:
#ifdef ALL_FORMATS
        if (codecID > AV_CODEC_ID_NONE && codecID < AV_CODEC_ID_FIRST_AUDIO)
            return MFX_TRACK_ANY_VIDEO;
        else if (codecID >= AV_CODEC_ID_FIRST_AUDIO)
            return MFX_TRACK_ANY_AUDIO;
        else
            return MFX_TRACK_UNKNOWN;
#else
        return MFX_TRACK_UNKNOWN;
#endif
    }
}
