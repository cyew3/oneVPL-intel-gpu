//******************************************************************************\
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

#pragma once

#include "sample_utils.h"
#include "mfxvideo++.h"
#include "mfxaudio++.h"
#include "base_allocator.h"
#include "cmdline_parser.h"
#include "transform_storage.h"
#include "transform_vdec.h"
#include "transform_venc.h"
#include "transform_adec.h"
#include "samples_pool.h"
#include "pipeline_profile.h"
#include "mfxsplmux++.h"
#include "mfxplugin++.h"
#include "session_storage.h"
#include "isink.h"
#include "splitter_wrapper.h"
#include "circular_splitter.h"
#include "muxer_wrapper.h"
#include "hw_device.h"

class PipelineManager;

class PipelineFactory
{
public:
    PipelineFactory(void);
    virtual ~PipelineFactory(void);

    virtual MFXVideoSessionExt*      CreateVideoSession();
    virtual MFXAudioSession*         CreateAudioSession();

    //MFX native objects to be wrapped into transforms
    //TODO: deprecate this
    virtual MFXVideoDECODE*          CreateVideoDecoder(MFXVideoSessionExt&);
    virtual ITransform*              CreateVideoDecoderTransform(MFXVideoSessionExt&, int timeout);

    //TODO: deprecate this
    virtual MFXVideoENCODE*          CreateVideoEncoder(MFXVideoSessionExt&);
    virtual ITransform*              CreateVideoEncoderTransform(MFXVideoSessionExt&, int timeout);

    virtual MFXVideoVPP*             CreateVideoVPP(MFXVideoSessionExt&);
    virtual ITransform*              CreateVideoVPPTransform(MFXVideoSessionExt&, int timeout);

    //TODO: deprecate this
    virtual MFXAudioDECODE*          CreateAudioDecoder(MFXAudioSession&);
    virtual ITransform*              CreateAudioDecoderTransform(MFXAudioSession&, int timeout);
    virtual ITransform*              CreateAudioDecoderNullTransform(MFXAudioSession&, int timeout);

    virtual MFXVideoUSER*            CreateVideoUserModule(MFXVideoSessionExt&);
    //virtual MFXPlugin*               CreatePlugin(mfxPluginType , MFXVideoUSER *, const msdk_string & pluginFullPath);
    virtual ITransform*              CreatePluginTransform(MFXVideoSessionExt&, mfxPluginType , const msdk_string & pluginFullPath, int);

    virtual MFXMuxer*                CreateMuxer();
    virtual MuxerWrapper*            CreateMuxerWrapper(MFXStreamParamsExt &, std::auto_ptr<MFXDataIO>& );

    virtual MFXAudioENCODE*          CreateAudioEncoder(MFXAudioSession&);
    virtual ITransform*              CreateAudioEncoderTransform(MFXAudioSession&, int timeout);
    virtual ITransform*              CreateAudioEncoderNullTransform(MFXAudioSession&, int timeout);

    //TODO: deprecate this
    virtual MFXSplitter*             CreateSplitter();
    virtual ISplitterWrapper*        CreateSplitterWrapper(std::auto_ptr<MFXDataIO>& );
    virtual ISplitterWrapper*        CreateCircularSplitterWrapper(std::auto_ptr<MFXDataIO>& , mfxU64 nLimit);

    virtual BaseFrameAllocator*      CreateFrameAllocator(AllocatorImpl );

    //TODO: deprecate this
    virtual MFXDataIO*               CreateFileIO(const msdk_string&, const msdk_string&);
    virtual ISink*                   CreateFileIOWrapper(const msdk_string&, const msdk_string&);

    virtual CmdLineParser*           CreateCmdLineParser();
    virtual SamplePool*              CreateSamplePool(int);
    virtual TransformStorage*        CreateTransformStorage();

    virtual IPipelineProfile*        CreatePipelineProfile(const MFXStreamParams &splInfo, CmdLineParser & parser);
    virtual SessionStorage *         CreateSessionStorage(const MFXSessionInfo &vSession, const MFXSessionInfo &aSession);

    virtual CHWDevice*               CreateHardwareDevice(AllocatorImpl );
    virtual mfxAllocatorParams*      CreateAllocatorParam(CHWDevice*, AllocatorImpl);
};
