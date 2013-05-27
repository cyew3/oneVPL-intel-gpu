/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivpp.h"
#include "test_method.h"

class MockVpp: public IMFXVideoVPP
{
public:
    virtual mfxStatus Query(mfxVideoParam *  /*in*/, mfxVideoParam * /*out*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest /*request*/[2])
    {
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD1(mfxStatus , Init, mfxVideoParam *)
    {
        TEST_METHOD_TYPE(Init) params_holder;
        if (NULL != _0)
        {
            params_holder.value0 = _0;
        }
        _Init.RegisterEvent(params_holder);

        if (!_Init.GetReturn(params_holder))
        {
            //no strategy
            return MFX_ERR_NONE;
        }
        return params_holder.ret_val;
    }
    virtual mfxStatus Reset(mfxVideoParam * /*par*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Close(void)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam * /*par*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus GetVPPStat(mfxVPPStat * /*stat*/)
    {
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD4(mfxStatus, RunFrameVPPAsync, 
        MAKE_STATIC_TRAIT(mfxFrameSurface1, mfxFrameSurface1*), //in
        MAKE_STATIC_TRAIT(mfxFrameSurface1, mfxFrameSurface1*), //out
        MAKE_STATIC_TRAIT(mfxExtVppAuxData, mfxExtVppAuxData*), //aux
        mfxSyncPoint * /*syncp*/)
    {
        TEST_METHOD_TYPE(RunFrameVPPAsync) params_holder;
        if (NULL != _0)
        {
            params_holder.value0 = *_0;
        }
        if (NULL != _1)
        {
            params_holder.value1 = *_1;
        }
        if (NULL != _2)
        {
            params_holder.value2 = *_2;
        }
        _3;
        _RunFrameVPPAsync.RegisterEvent(params_holder);

        if (!_RunFrameVPPAsync.GetReturn(params_holder))
        {
            //no strategy
            return MFX_ERR_NONE;
        }
        return params_holder.ret_val;
    }
    virtual mfxStatus SyncOperation(mfxSyncPoint /*syncp*/, mfxU32 /*wait*/)
    {
        return MFX_ERR_NONE;
    }
};