/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_base_allocator.h"
#include "test_method.h"

class MockMFXFrameAllocator : public MFXFrameAllocator
{
    
public:

    DECLARE_TEST_METHOD1(mfxStatus, Init, mfxAllocatorParams *)
    {
        _0;
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD0(mfxStatus, Close)
    {
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD2(mfxStatus, AllocFrames, mfxFrameAllocRequest *, mfxFrameAllocResponse *)
    {
        _0;_1;
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD2(mfxStatus, LockFrame, mfxMemId, mfxFrameData *)
    {
        TEST_METHOD_TYPE(LockFrame) result;

        result.value0 = _0;
        result.value1 = _1;
        _LockFrame.RegisterEvent(result);


        if (!_LockFrame.GetReturn(result))
            return MFX_ERR_NONE;


        return result.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, UnlockFrame, mfxMemId, mfxFrameData *)
    {
        _0;_1;
        TEST_METHOD_TYPE(UnlockFrame) result;
        if (!_UnlockFrame.GetReturn(result))
            return MFX_ERR_NONE;
        return result.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, GetFrameHDL, mfxMemId , mfxHDL *)
    {
        _0;_1;
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD1(mfxStatus, FreeFrames, mfxFrameAllocResponse *)
    {
        _0;
        return MFX_ERR_NONE;
    }
};
