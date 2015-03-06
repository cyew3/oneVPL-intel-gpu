//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <memory>
#include "isink.h"

class MockSink : public ISink
{
public:
    MOCK_METHOD1(PutSample, void (SamplePtr & sample));
};

