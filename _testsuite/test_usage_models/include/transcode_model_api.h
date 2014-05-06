//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "test_usage_models_utils.h"

class TranscodeModel
{
public:
    // Destructor
    virtual ~TranscodeModel(void) {}

    virtual mfxStatus Init( AppParam& params ) = 0;
    virtual mfxStatus Run(void)   = 0;
    virtual mfxStatus Close(void) = 0;

    virtual int GetProcessedFramesCount(void) = 0;
};
/* EOF */