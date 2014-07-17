//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "itransform.h"
#include <gmock/gmock.h>

class MockTransform  : public ITransform {
public:
    MOCK_METHOD2(Configure, void(MFXAVParams&, ITransform*));
    MOCK_METHOD1(PutSample, void(SamplePtr&));
    MOCK_METHOD1(GetSample, bool(SamplePtr&));
    MOCK_METHOD2(GetNumSurfaces, void(MFXAVParams &, IAllocRequest &));
};

