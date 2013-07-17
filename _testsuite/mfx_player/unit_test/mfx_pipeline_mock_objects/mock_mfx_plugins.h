/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iallocator_factory.h"
#include "mfxplugin++.h"

class MockPlugin
    : public MFXGenericPlugin, public MFXDecoderPlugin, public MFXEncoderPlugin
{
public:
    DECLARE_TEST_METHOD1(mfxStatus, Init, mfxVideoParam*)
    {
        TEST_METHOD_TYPE(Init) params;

        params.value0 = _0;
        _Init.RegisterEvent(params);
        if (!_Init.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD1(mfxStatus, PluginInit, mfxCoreInterface *)
    {
        TEST_METHOD_TYPE(PluginInit) params;

        params.value0 = _0;
        _PluginInit.RegisterEvent(params);
        if (!_PluginInit.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD0(mfxStatus, PluginClose)
    {
        TEST_METHOD_TYPE(PluginClose) params;

        _PluginClose.RegisterEvent(params);
        if (!_PluginClose.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD1(mfxStatus, GetPluginParam, mfxPluginParam*)
    {
        TEST_METHOD_TYPE(GetPluginParam) params;

        params.value0 = _0;
        _GetPluginParam.RegisterEvent(params);
        if (!_GetPluginParam.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD3(mfxStatus, Execute, mfxThreadTask , mfxU32 , mfxU32 )
    {
        TEST_METHOD_TYPE(Execute) params;

        params.value0 = _0;
        params.value1 = _1;
        params.value2 = _2;
        _Execute.RegisterEvent(params);
        if (!_Execute.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, FreeResources, mfxThreadTask , mfxStatus )
    {
        TEST_METHOD_TYPE(FreeResources) params;
        params.value0 = _0;
        params.value1 = _1;
        _FreeResources.RegisterEvent(params);
        if (!_FreeResources.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, QueryIOSurf, mfxVideoParam *, mfxFrameAllocRequest * )
    {
        TEST_METHOD_TYPE(QueryIOSurf) params;
        params.value0 = _0;
        params.value1 = _1;
        _QueryIOSurf.RegisterEvent(params);
        if (!_QueryIOSurf.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD0(void, Release)
    {
        TEST_METHOD_TYPE(Release) params;
        _Release.RegisterEvent(params);
    }
    DECLARE_TEST_METHOD0(mfxStatus, Close)
    {
        TEST_METHOD_TYPE(Close) params;

        _Close.RegisterEvent(params);
        if (!_Close.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, SetAuxParams, void* , int )
    {
        TEST_METHOD_TYPE(SetAuxParams) params;
        params.value0 = _0;
        params.value1 = _1;
        _SetAuxParams.RegisterEvent(params);
        if (!_SetAuxParams.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }

    DECLARE_TEST_METHOD5(mfxStatus, Submit, const mfxHDL *, mfxU32 , const mfxHDL *, mfxU32 , mfxThreadTask *) 
    {
        TEST_METHOD_TYPE(Submit) params;
        params.value0 = _0;
        params.value1 = _1;
        params.value2 = _2;
        params.value3 = _3;
        params.value4 = _4;
        _Submit.RegisterEvent(params);
        if (!_Submit.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, Query, mfxVideoParam *, mfxVideoParam *)
    {
        TEST_METHOD_TYPE(Query) params;
        params.value0 = _0;
        params.value1 = _1;
        TEST_METHOD_NAME(Query).RegisterEvent(params);
        if (!TEST_METHOD_NAME(Query).GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }
    DECLARE_TEST_METHOD1(mfxStatus, Reset, mfxVideoParam *)
    {
        TEST_METHOD_TYPE(Reset) params;
        params.value0 = _0;
        TEST_METHOD_NAME(Reset).RegisterEvent(params);
        if (!TEST_METHOD_NAME(Reset).GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }        
    DECLARE_TEST_METHOD1(mfxStatus, GetVideoParam, mfxVideoParam * )
    {
        TEST_METHOD_TYPE(GetVideoParam) params;
        params.value0 = _0;
        TEST_METHOD_NAME(GetVideoParam).RegisterEvent(params);
        if (!TEST_METHOD_NAME(GetVideoParam).GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }        
    DECLARE_TEST_METHOD2(mfxStatus, DecodeHeader, mfxBitstream *, mfxVideoParam *)
    {
        TEST_METHOD_TYPE(DecodeHeader) params;
        params.value0 = _0;
        params.value1 = _1;
        TEST_METHOD_NAME(DecodeHeader).RegisterEvent(params);
        if (!TEST_METHOD_NAME(DecodeHeader).GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }        
    DECLARE_TEST_METHOD2(mfxStatus, GetPayload, mfxU64 *, mfxPayload *)
    {
        TEST_METHOD_TYPE(GetPayload) params;
        params.value0 = _0;
        params.value1 = _1;
        TEST_METHOD_NAME(GetPayload).RegisterEvent(params);
        if (!TEST_METHOD_NAME(GetPayload).GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }        
    DECLARE_TEST_METHOD4(mfxStatus, DecodeFrameSubmit, mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 **,  mfxThreadTask *)
    {
        TEST_METHOD_TYPE(DecodeFrameSubmit) params;
        params.value0 = _0;
        params.value1 = _1;
        params.value2 = _2;
        params.value3 = _3;
        TEST_METHOD_NAME(DecodeFrameSubmit).RegisterEvent(params);
        if (!TEST_METHOD_NAME(DecodeFrameSubmit).GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }        
    DECLARE_TEST_METHOD4(mfxStatus, EncodeFrameSubmit, mfxEncodeCtrl *, mfxFrameSurface1 *, mfxBitstream *, mfxThreadTask *)
    {
        TEST_METHOD_TYPE(EncodeFrameSubmit) params;
        params.value0 = _0;
        params.value1 = _1;
        params.value2 = _2;
        params.value3 = _3;
        TEST_METHOD_NAME(EncodeFrameSubmit).RegisterEvent(params);
        if (!TEST_METHOD_NAME(EncodeFrameSubmit).GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        return params.ret_val;
    }        
};
