/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_encode.h"
#include "test_method.h"
#include "mfx_test_utils.h"

class MockVideoEncode : public IVideoEncode
{
public:
    DECLARE_TEST_METHOD2(mfxStatus, Query, mfxVideoParam * , mfxVideoParam * );
    
    virtual mfxStatus QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest * /*request*/)
    {
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD1(mfxStatus, Init, MAKE_DYNAMIC_TRAIT(mfxVideoParam, mfxVideoParam *))
    {
        mfxVideoParam *newIn = _0;
        if (_0) {
            newIn = new mfxVideoParam();
            TestUtils::CopyVideoParams(newIn, _0, true);
        }

        _Init.RegisterEvent(TEST_METHOD_TYPE(Init)(MFX_ERR_NONE, *newIn));

        TEST_METHOD_TYPE(Init) result_val;
        if (!_Init.GetReturn(result_val))
        {
            return MFX_ERR_NONE;
        }
        return result_val.ret_val;
    }
    virtual mfxStatus Reset(mfxVideoParam * /*par*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Close(void)
    {
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD1(mfxStatus, GetVideoParam, mfxVideoParam *)
    {
        TEST_METHOD_TYPE(GetVideoParam) result_val;
        if (!_GetVideoParam.GetReturn(result_val))
        {
            return MFX_ERR_NONE;
        }
        
        TestUtils::CopyVideoParams(_0, result_val.value0, false);
        return result_val.ret_val;
    }
    virtual mfxStatus GetEncodeStat(mfxEncodeStat * /*stat*/)
    {
        return MFX_ERR_NONE;
    }
    
    DECLARE_TEST_METHOD4(mfxStatus, EncodeFrameAsync, mfxEncodeCtrl *, mfxFrameSurface1 *, mfxBitstream *, mfxSyncPoint *);
    DECLARE_TEST_METHOD2(mfxStatus, SyncOperation, mfxSyncPoint , mfxU32 );
};