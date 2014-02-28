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

class MockMFXMuxer : public MFXMuxer {
public:
    MOCK_METHOD2(Init, mfxStatus (MFXStreamParams &, MFXDataIO &));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD3(PutBitstream, mfxStatus (mfxU32 , mfxBitstream* , mfxU64 ));
};
