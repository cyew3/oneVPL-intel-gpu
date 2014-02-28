//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "splitter_wrapper.h"
#include "fileio.h"
#include "mock_splitter.h"
#include "gmock\gmock.h"
#include "test_utils.h"


class MockSource : public ISplitterWrapper
{
public:
    MOCK_METHOD1(GetSample, bool (SamplePtr &));
    MOCK_METHOD1(GetInfo, void(MFXStreamParams &));
};
