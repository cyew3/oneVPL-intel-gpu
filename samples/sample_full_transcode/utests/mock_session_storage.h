//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "session_storage.h"
#include "gmock\gmock-generated-function-mockers.h"

class MockSessionStorage : public SessionStorage {
public:
    MockSessionStorage ():
        SessionStorage(*(PipelineFactory*)0, MFXSessionInfo(), MFXSessionInfo()) {}
    
    MOCK_METHOD1(GetAudioSessionForID, MFXAudioSession * (int id));
    MOCK_METHOD1(GetVideoSessionForID, MFXVideoSessionExt * (int id));
};