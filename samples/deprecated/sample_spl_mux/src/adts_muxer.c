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

#include "adts_muxer.h"
#include "ffmpeg_reader_writer.h"

mfxStatus InitAdtsMuxer(AVCodecContext *inputCodecContext, adtsMuxer *mux)
{
    mfxStatus        sts;
    _adtsMuxer      *outMux;
    AVOutputFormat  *outputFormat;
    AVFormatContext *formatContext;
    mfxU8           *contextBuffer;
    uint32_t         bufferSize;
    const char      *formatName;
    mfxHDL           hdl;

    if (mux == NULL)
        return MFX_ERR_NULL_PTR;

    if(inputCodecContext == NULL)
        return MFX_ERR_NULL_PTR;

    outMux         = NULL;
    outputFormat  = NULL;
    formatContext = NULL;
    contextBuffer = NULL;
    formatName    = "ADTS";
    sts            = MFX_ERR_NONE;

    outMux = (_adtsMuxer*) av_malloc(sizeof(_adtsMuxer));

    if(!outMux)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    memset(outMux,0,sizeof(_adtsMuxer));

    bufferSize = 4*1024;
    contextBuffer = (mfxU8*) av_malloc(bufferSize);
    if(contextBuffer == NULL)
        sts = MFX_ERR_MEMORY_ALLOC;

    if (sts == MFX_ERR_NONE)
    {
        outMux->dataIO = (mfxDataIO*) av_malloc(sizeof(mfxDataIO));
        outMux->dataIO->pthis = &outMux->dataLen;
        outMux->dataIO->Write = WriteADTS;
        hdl = outMux->dataIO;
        outMux->ioContext = avio_alloc_context(contextBuffer, bufferSize, AVIO_FLAG_WRITE, hdl, NULL, ffmpeg_write, NULL);
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
        formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
        outMux->formatContext = formatContext;
    }
    outMux->adtsStream = avformat_new_stream(outMux->formatContext, NULL);
    if (outMux->adtsStream == NULL)
        sts = MFX_ERR_UNKNOWN;

    if (sts == MFX_ERR_NONE)
    {
        *mux = (adtsMuxer) outMux;
        sts = SetAdtsInfo(inputCodecContext, (adtsMuxer) outMux);
    }

    if (sts == MFX_ERR_NONE)
    {
        if(avformat_write_header(outMux->formatContext, NULL) < 0)
        {
            sts = MFX_ERR_UNKNOWN;
        }
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

mfxStatus CloseAdtsMuxer(adtsMuxer mux)
{
    mfxStatus   sts;
    _adtsMuxer *inMux;

    sts = MFX_ERR_NONE;

    inMux = (_adtsMuxer*)mux;
    if(inMux == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    if (sts == MFX_ERR_NONE )
    {
        if (av_write_trailer(inMux->formatContext) < 0)
            sts = MFX_ERR_UNKNOWN;
    }

    if (inMux->adtsStream)
    {
        if (inMux->adtsStream->codec != NULL)
        {
            if (inMux->adtsStream->codec->extradata_size > 0)
            {
                av_free(inMux->adtsStream->codec->extradata);
            }
            if(avcodec_close(inMux->adtsStream->codec) < 0)
            {
                sts = MFX_ERR_UNKNOWN;
            }
        }
    }

    if (inMux->formatContext->streams[0] != NULL)
    {
        if (inMux->formatContext->streams[0]->codec != NULL)
            av_freep(&inMux->formatContext->streams[0]->codec);
        av_freep(&inMux->formatContext->streams[0]);
    }

    if (inMux->ioContext->buffer != NULL)
    {
        av_free(inMux->ioContext->buffer);
    }
    if (inMux->formatContext->pb != NULL)
        av_free(inMux->formatContext->pb);

    av_free(inMux);

    return sts;
}

mfxStatus SetAdtsInfo(AVCodecContext *inputCodecContext, adtsMuxer mux)
{
    _adtsMuxer      *outMux;
    AVCodecContext  *codecContext;
    mfxI32           ret;

    outMux = (_adtsMuxer*)mux;
    if(outMux == NULL)
        return MFX_ERR_NULL_PTR;

    if (inputCodecContext == NULL)
        return MFX_ERR_NULL_PTR;

    codecContext = outMux->adtsStream->codec = outMux->formatContext->streams[0]->codec;
    ret = avcodec_copy_context(codecContext, inputCodecContext);
    if (ret < 0)
        return MFX_ERR_UNKNOWN;

    return MFX_ERR_NONE;
}

mfxStatus GetAdtsBitstream(adtsMuxer mux, AVPacket *pkt, mfxBitstream *bs)
{
    mfxStatus sts;
    _adtsMuxer *inMux;
    int oldPktIndex;
    int64_t oldPts, oldDts;
    oldPktIndex = pkt->stream_index;
    oldPts = pkt->pts;
    oldDts = pkt->dts;

    inMux = (_adtsMuxer*)mux;
    if(inMux == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }
    if(inMux->formatContext == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    sts = MFX_ERR_NONE;
    pkt->stream_index = 0;
    pkt->pts = AV_NOPTS_VALUE;
    pkt->dts = AV_NOPTS_VALUE;

    if (pkt->size + ADTS_HEADER_WITH_CRC_LENGTH > inMux->ioContext->buffer_size)
    {
        //grow buffer
        if (inMux->ioContext->buffer != NULL)
        {
            av_free(inMux->ioContext->buffer);
            inMux->ioContext->buffer = NULL;
        }
        inMux->ioContext->buffer = (mfxU8*) av_malloc(pkt->size + ADTS_HEADER_WITH_CRC_LENGTH);
        if(inMux->ioContext->buffer == NULL)
            sts = MFX_ERR_MEMORY_ALLOC;
        inMux->ioContext->buffer_size = pkt->size + ADTS_HEADER_WITH_CRC_LENGTH;
    }

    if (sts == MFX_ERR_NONE)
    {
        if (av_interleaved_write_frame(inMux->formatContext, pkt))
        {
            sts = MFX_ERR_UNKNOWN;
        }
        pkt->stream_index = oldPktIndex;
        pkt->pts = oldPts;
        pkt->dts = oldDts;
    }

    if (sts == MFX_ERR_NONE)
    {
        bs->Data = inMux->ioContext->buffer;
        bs->MaxLength = inMux->dataLen;
        bs->DataLength = inMux->dataLen;
        bs->DataOffset = 0;
    }

    return sts;
}

mfxI32 WriteADTS(void* pthis, mfxBitstream *bs)
{
    int *dataLen = (int*) pthis;
    *dataLen = bs->DataLength;
    return bs->DataLength;
}
