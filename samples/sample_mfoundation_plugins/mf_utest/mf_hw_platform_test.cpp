/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/


#include "mf_hw_platform.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"



TEST(HW_PLATFORM, expectBuildOnIntelHW) {
    HWPlatform::HWType  hwType = HWPlatform::Type(HWPlatform::ADAPTER_ANY);
    //expect that code compiled on HW platform
    EXPECT_NE(HWPlatform::UNKNOWN, hwType);
}

TEST(HW_PLATFORM, expectDefaultHWisIntel) {
    HWPlatform::HWType  hwType = HWPlatform::Type(HWPlatform::ADAPTER_DEFAULT);
    //cannot make any expectation regarding target platform but used in algorithm testing at least
    SUCCEED();
}

TEST(HW_PLATFORM, unexpectHW) {
    HWPlatform::HWType  hwType = HWPlatform::Type(1);
    //cannot make any expectation regarding target platform but used in algorithm testing at least
    SUCCEED();
}