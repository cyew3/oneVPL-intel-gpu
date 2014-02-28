//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "isample.h"
#include "gmock\gmock.h"

class MockSample : public ISample 
{
public:
    MOCK_METHOD0(GetSurface, mfxFrameSurface1 & ());
    MOCK_METHOD0(GetBitstream, mfxBitstream &());
    MOCK_METHOD0(GetAudioFrame, mfxAudioFrame &());
    MOCK_CONST_METHOD0(GetSampleType, int());
    MOCK_CONST_METHOD0(GetTrackID, int());
    MOCK_CONST_METHOD2(GetMetaData, bool(int , MetaData& ));
    MOCK_CONST_METHOD1(HasMetaData, bool(int));
};