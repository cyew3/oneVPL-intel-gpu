//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "pipeline_profile.h"
#include "gmock/gmock.h"

class MockPipelineProfile : public IPipelineProfile {
public:
    MOCK_CONST_METHOD1(isAudioDecoderExist, bool(int));
    MOCK_CONST_METHOD1(isAudioEncoderExist, bool(int));
    MOCK_CONST_METHOD0(isDecoderExist, bool());
    MOCK_CONST_METHOD0(isPluginExist, bool());
    MOCK_CONST_METHOD0(isVppExist, bool());
    MOCK_CONST_METHOD0(isEncoderExist, bool());
    MOCK_CONST_METHOD0(isMultiplexerExist, bool());
    MOCK_CONST_METHOD0(GetVideoTrackInfo, VideoTrackInfoExt());
    MOCK_CONST_METHOD1(GetAudioTrackInfo, AudioTrackInfoExt(size_t));
    MOCK_CONST_METHOD0(GetMuxerInfo, MFXStreamParamsExt ());
    MOCK_CONST_METHOD0(isDecoderPluginExist, bool());
    MOCK_CONST_METHOD0(isEncoderPluginExist, bool ());
    MOCK_CONST_METHOD0(isGenericPluginExist, bool ());

};
