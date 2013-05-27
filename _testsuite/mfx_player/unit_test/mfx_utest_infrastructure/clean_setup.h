/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once;

#include "test_registry.h"

class CleanTestEnvironment
{
public :
    CleanTestEnvironment()
    {
        TestParamsRegistry :: Instance().clear();
    }
};