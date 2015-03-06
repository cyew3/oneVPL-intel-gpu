//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfxsplmux++.h"
#include <gmock/gmock.h>
#include "fileio.h"


class MockDataIO : public MFXDataIO
{
public:
    MOCK_METHOD1(Read,  mfxI32 (mfxBitstream *));
    MOCK_METHOD1(Write, mfxI32 (mfxBitstream *));
    MOCK_METHOD2(Seek, mfxI64 (mfxI64 , mfxSeekOrigin ));
};