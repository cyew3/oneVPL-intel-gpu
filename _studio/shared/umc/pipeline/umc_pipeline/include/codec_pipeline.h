// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __CODEC_PIPELINE_H__
#define __CODEC_PIPELINE_H__

#include "umc_base_codecs.h"
#include "umc_media_data_ex.h"
#include "umc_source_types.h"

enum
{
    DEF_DATA_READER = 0,
    FILE_DATA_READER,
    VOB_DATA_READER,
    SOCKET_DATA_READER,
    HTTP_DATA_READER
};

class CodecPipeline
{
public:
    static UMC::Status SelectDataReader(UMC::ModuleContext& rContext,
                                        UMC::DataReader*&   rpDataReader,
                                        Ipp32u ulPrefferedReader);

    static UMC::Status SelectAudioRender(UMC::AudioRenderParams& rAudioRenderInit,
                                         UMC::AudioRender*& rpRender,
                                         Ipp32u ulPrefferedRender = UMC::DEF_AUDIO_RENDER);

    static UMC::Status SelectAudioRender(UMC::sAudioStreamInfo& rAudioInfo,
                                         UMC::ModuleContext& rContext,
                                         UMC::AudioRender*& rpAudioRender,
                                         Ipp32u ulPrefferedRender = UMC::DEF_AUDIO_RENDER,
                                         vm_char*  szOutPutAudio=0);

    static UMC::Status SelectAudioDecoder(UMC::sAudioStreamInfo& rAudioInfo,
                                          UMC::BaseCodec*& rpAudioDecoder);

    static UMC::Status SelectDTAudioDecoder(UMC::sAudioStreamInfo& rAudioInfo,
                                            UMC::BaseCodec*& rpAudioDecoder,
                                            UMC::DualThreadedCodec*& rpDSAudioCodec,
                                            UMC::MediaBuffer*& rpMediaBuffer,
                                            UMC::MediaData*);

    // select splitter for various media source(s)
    static UMC::Status SelectSplitter(UMC::SourceInfo *lpSourceInfo,
                                      Ipp32u uiSplitterFlags,
                                      UMC::Splitter*& rpSplitter,
                                      Ipp32u uiSelectedVideoPID = UMC::SELECT_ANY_VIDEO_PID,
                                      Ipp32u uiSelectedAudioPID = UMC::SELECT_ANY_AUDIO_PID);

    // select splitter for media file
    static UMC::Status SelectSplitter(UMC::DataReader* pDataReader,
                                      Ipp32u uiSplitterFlags,
                                      UMC::Splitter*& rpSplitter,
                                      Ipp32u uiSelectedVideoPID = UMC::SELECT_ANY_VIDEO_PID,
                                      Ipp32u uiSelectedAudioPID = UMC::SELECT_ANY_AUDIO_PID);

    static UMC::Status SelectVideoDecoder(UMC::sVideoStreamInfo& rVideoInfo,
                                          UMC::MediaData*        pDecSpecInfo,
                                          Ipp32u                 lInterpolation,
                                          Ipp32u                 lDeinterlacing,
                                          Ipp32u                 numThreads,
                                          Ipp32u                 ulVideoDecoderFlags,
                                          UMC::BaseCodec&        rColorConverter,
                                          UMC::VideoDecoder*&    rpVideoDecoder);

    static UMC::Status SelectVideoRender(UMC::ModuleContext& rContext,
                                         UMC::sClipInfo ClipInfo,
                                         UMC::sRECT rectDisp,
                                         UMC::sRECT rectRange,
                                         UMC::ColorFormat RenderCFormat,
                                         Ipp32u ulVideoRenderFlags,
                                         UMC::VideoRender*& rpVideoRender,
                                         Ipp32u ulPrefferedRender = UMC::DEF_VIDEO_RENDER,
                                         vm_char * szOutputVideo=0);

    static UMC::Status SelectVideoRender(UMC::VideoRenderParams& rVideoRenderParams,
                                         UMC::ModuleContext& rContext,
                                         UMC::VideoRender*& rpRender,
                                         Ipp32u ulPrefferedRender = UMC::DEF_VIDEO_RENDER);
};

#endif // __CODEC_PIPELINE_H__
