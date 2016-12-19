/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "gtest/gtest.h"

#include "mfx_trace.h"

class SchedulerTestsEnvironment: public ::testing::Environment
{
public:
    virtual ~SchedulerTestsEnvironment() {}
    virtual void SetUp() {
        MFX_TRACE_INIT();
    }
    virtual void TearDown() {
        MFX_TRACE_CLOSE();
    }
};

::testing::Environment* const scheduler_tests_env = ::testing::AddGlobalTestEnvironment(new SchedulerTestsEnvironment);
