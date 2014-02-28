//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfxsplmux++.h"
#include "gmock\gmock.h"
#include "splitter_wrapper.h"

class MockMFXSplitter : public MFXSplitter {
public:
    MOCK_METHOD1(Init, mfxStatus (MFXDataIO &));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD1(GetInfo, mfxStatus (MFXStreamParams &));
    MOCK_METHOD2(GetBitstream, mfxStatus (mfxU32* , mfxBitstream *));
    MOCK_METHOD1(ReleaseBitstream, mfxStatus (mfxBitstream* ));
};

class MockSplitterWrapper : public ISplitterWrapper {
public:
    MOCK_METHOD1( GetSample, bool (SamplePtr & sample));
    MOCK_METHOD1(GetInfo, void (MFXStreamParams & info));
};