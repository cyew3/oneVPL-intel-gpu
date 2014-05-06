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
    : public MFXGenericPlugin, public MFXDecoderPlugin, public MFXEncoderPlugin, public MFXVPPPlugin
{
public:
    DECLARE_MOCK_METHOD1(mfxStatus, Init, mfxVideoParam*);
    DECLARE_MOCK_METHOD1(mfxStatus, PluginInit, mfxCoreInterface *);
    DECLARE_MOCK_METHOD0(mfxStatus, PluginClose);
    DECLARE_TEST_METHOD1(mfxStatus, GetPluginParam, MAKE_DYNAMIC_TRAIT(mfxPluginParam, mfxPluginParam*))
    {
        TEST_METHOD_TYPE(GetPluginParam) params;

        if (_0) {
            params.value0 = *_0;
        }
        _GetPluginParam.RegisterEvent(params);
        if (!_GetPluginParam.GetReturn(params))
        {
            return MFX_ERR_NONE;
        }
        if (_0) {
            *_0 = params.value0;
        }
        return params.ret_val;
    }
    DECLARE_MOCK_METHOD3(mfxStatus, Execute, mfxThreadTask , mfxU32 , mfxU32 );
    DECLARE_MOCK_METHOD2(mfxStatus, FreeResources, mfxThreadTask , mfxStatus );
    DECLARE_MOCK_METHOD3(mfxStatus, QueryIOSurf, mfxVideoParam *, mfxFrameAllocRequest *, mfxFrameAllocRequest * );
    DECLARE_MOCK_METHOD0(void,      Release)
    DECLARE_MOCK_METHOD0(mfxStatus, Close);
    DECLARE_MOCK_METHOD2(mfxStatus, SetAuxParams, void* , int );
    DECLARE_MOCK_METHOD5(mfxStatus, Submit, const mfxHDL *, mfxU32 , const mfxHDL *, mfxU32 , mfxThreadTask *);
    DECLARE_MOCK_METHOD2(mfxStatus, Query, mfxVideoParam *, mfxVideoParam *);
    DECLARE_MOCK_METHOD1(mfxStatus, Reset, mfxVideoParam *);
    DECLARE_MOCK_METHOD1(mfxStatus, GetVideoParam, mfxVideoParam * );
    DECLARE_MOCK_METHOD2(mfxStatus, DecodeHeader, mfxBitstream *, mfxVideoParam *);
    DECLARE_MOCK_METHOD2(mfxStatus, GetPayload, mfxU64 *, mfxPayload *);
    DECLARE_MOCK_METHOD4(mfxStatus, DecodeFrameSubmit, mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 **,  mfxThreadTask *);
    DECLARE_MOCK_METHOD4(mfxStatus, EncodeFrameSubmit, mfxEncodeCtrl *, mfxFrameSurface1 *, mfxBitstream *, mfxThreadTask *);
    DECLARE_MOCK_METHOD4(mfxStatus, VPPFrameSubmit, mfxFrameSurface1 *, mfxFrameSurface1 *, mfxExtVppAuxData *, mfxThreadTask *);
};
