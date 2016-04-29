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

#pragma once

#include "pipeline_factory.h"
#include <gmock/gmock.h>

class MockPipelineFactory : public PipelineFactory {
public:

    MOCK_METHOD0(CreateSplitter, MFXSplitter *());
    MOCK_METHOD1(CreateSplitterWrapper, ISplitterWrapper *(std::auto_ptr<MFXDataIO>&io));

    MOCK_METHOD2(CreateSessionStorage, SessionStorage * ( const MFXSessionInfo &, const MFXSessionInfo &));
    MOCK_METHOD2(CreateFileIO, MFXDataIO * (const msdk_string&, const msdk_string&));

    MOCK_METHOD0(CreateVideoSession, MFXVideoSessionExt* ());
    MOCK_METHOD1(CreateVideoDecoder, MFXVideoDECODE* (MFXVideoSessionExt&));
    MOCK_METHOD2(CreateVideoDecoderTransform, ITransform* (MFXVideoSessionExt&, int));


    MOCK_METHOD1(CreateVideoEncoder, MFXVideoENCODE* (MFXVideoSessionExt&));
    MOCK_METHOD2(CreateVideoEncoderTransform, ITransform* (MFXVideoSessionExt&, int));

    MOCK_METHOD1(CreateVideoVPP, MFXVideoVPP* (MFXVideoSessionExt&));
    MOCK_METHOD3(CreateVideoVPPTransform, ITransform *(MFXVideoSessionExt& , int, const mfxPluginUID& ));

    MOCK_METHOD0(CreateAudioSession, MFXAudioSession* ());
    MOCK_METHOD1(CreateAudioDecoder, MFXAudioDECODE* (MFXAudioSession&));
    MOCK_METHOD2(CreateAudioDecoderTransform, ITransform* (MFXAudioSession&, int));
    MOCK_METHOD2(CreateAudioEncoderTransform, ITransform* (MFXAudioSession&, int));

    MOCK_METHOD1(CreateAudioEncoder, MFXAudioENCODE* (MFXAudioSession&));
    MOCK_METHOD1(CreateFrameAllocator, BaseFrameAllocator* (AllocatorImpl));
    MOCK_METHOD1(CreateSink, ISink* (const msdk_string&));
    MOCK_METHOD0(CreateCmdLineParser, CmdLineParser* ());
    MOCK_METHOD1(CreateSamplePool, SamplePool* (int));
    MOCK_METHOD0(CreateTransformStorage, TransformStorage* ());
    MOCK_METHOD2(CreatePipelineProfile, IPipelineProfile* (const MFXStreamParams &splInfo, CmdLineParser & parser));
    MOCK_METHOD2(CreateAllocatorParam, mfxAllocatorParams*(CHWDevice*, AllocatorImpl));
    MOCK_METHOD1(CreateHardwareDevice, CHWDevice* (AllocatorImpl ));
};