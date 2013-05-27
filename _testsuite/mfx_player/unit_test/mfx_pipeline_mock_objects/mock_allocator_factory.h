/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iallocator_factory.h"
#include "test_method.h"

class MockAllocatorFactory
    : public IAllocatorFactory
{
public:

    DECLARE_TEST_METHOD0(MFXFrameAllocator*, CreateD3DAllocator)
    {
        TEST_METHOD_TYPE(CreateD3DAllocator) params;

        if (!_CreateD3DAllocator.GetReturn(params))
        {
            return NULL;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD0(MFXFrameAllocator*, CreateD3D11Allocator)
    {
        TEST_METHOD_TYPE(CreateD3D11Allocator) params;

        if (!_CreateD3D11Allocator.GetReturn(params))
        {
            return NULL;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD0(MFXFrameAllocator*, CreateVAAPIAllocator)
    {
        TEST_METHOD_TYPE(CreateVAAPIAllocator) params;

        if (!_CreateVAAPIAllocator.GetReturn(params))
        {
            return NULL;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD0(MFXFrameAllocator*, CreateSysMemAllocator)
    {
        TEST_METHOD_TYPE(CreateSysMemAllocator) params;

        if (!_CreateSysMemAllocator.GetReturn(params))
        {
            return NULL;
        }
        return params.ret_val;
    }
};
 
