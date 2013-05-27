/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#pragma once

#include "test_method.h"
#include "mfx_irandom.h"

class MockRandom : public IRandom
{
public:

    virtual void srand(mfxU32 ){}
    DECLARE_TEST_METHOD0(mfxU32, rand)
    {
        TEST_METHOD_TYPE(rand) params_holder(0);
        _rand.GetReturn(params_holder);
        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD0(mfxU32, rand_max)
    {
        TEST_METHOD_TYPE(rand_max) params_holder(0);
        _rand_max.GetReturn(params_holder);
        return params_holder.ret_val;
    }
};

//count number of random calls
class MockRandom2 : public IRandom
{
    int nCalls;
public:
    MockRandom2()
        : nCalls()
    {
    }
    int& Calls(){return nCalls;}
    mfxU32 rand()
    {
        nCalls++;
        return DefaultRandom::Instance().rand();
    }
    mfxU32 rand_max()
    {
        return DefaultRandom::Instance().rand_max();
    }
    void srand(mfxU32 seed)
    {
        DefaultRandom::Instance().srand(seed);
    }
};