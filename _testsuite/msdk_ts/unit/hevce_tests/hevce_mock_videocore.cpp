//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2016 Intel Corporation. All Rights Reserved.
//

#include "stdexcept"

#include "gtest/gtest.h"

#include "hevce_common_api.h"
#include "hevce_mock_videocore.h"

using namespace ApiTestCommon;

MockMFXCoreInterface::MockMFXCoreInterface()
    : paramset()
{
//    Zero(memIds);
}

void MockMFXCoreInterface::SetParamSet(const ParamSet &param)
{ 
    paramset = &param;
}
