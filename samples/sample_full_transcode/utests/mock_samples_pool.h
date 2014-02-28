//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "samples_pool.h"
#include "gmock\gmock.h"

class MockSamplePool : public SamplePool, private no_copy {
public:
    MockSamplePool() : SamplePool(0){}
    MOCK_METHOD0(FindFreeSample, ISample & ());
    MOCK_METHOD1(RegisterSample, void(SamplePtr&));
    //MOCK_METHOD1(LockSample, ISample * (const ISample& ));
    MOCK_METHOD1(LockSample, ISample * (const ISample& ));
};