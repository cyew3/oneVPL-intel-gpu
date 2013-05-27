/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_itime.h"
#include "test_method.h"

class MockTime : public ITime
{
public:
    DECLARE_TEST_METHOD0( mfxU64, GetTick)
    {
        TEST_METHOD_TYPE(GetTick) ret_params;
        if (!_GetTick.GetReturn(ret_params))
        {
            return 0;
        }

        return ret_params.ret_val;
    }
    DECLARE_TEST_METHOD0( mfxU64, GetFreq)
    {
        TEST_METHOD_TYPE(GetFreq) ret_params;
        if (!_GetFreq.GetReturn(ret_params))
        {
            return 1;
        }
        return ret_params.ret_val;
    }

    DECLARE_TEST_METHOD0( mfxF64,  Current) 
    {
        TEST_METHOD_TYPE(Current) ret_params;
        if (!_Current.GetReturn(ret_params))
        {
            return 0;
        }
        return ret_params.ret_val;
    }
    DECLARE_TEST_METHOD1(void, Wait, mfxU32)
    {
        TEST_METHOD_TYPE(Wait) ret_params;
        ret_params.value0 = _0;

        _Wait.RegisterEvent(ret_params);
    }

};

