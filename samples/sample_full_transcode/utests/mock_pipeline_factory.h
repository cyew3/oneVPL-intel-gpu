//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

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
    MOCK_METHOD2(CreateVideoVPPTransform, ITransform *(MFXVideoSessionExt& , int ));

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