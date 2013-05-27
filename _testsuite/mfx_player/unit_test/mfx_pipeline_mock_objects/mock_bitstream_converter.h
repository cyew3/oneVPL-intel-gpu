/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "test_method.h"
#include "mfx_ibitstream_converter.h"

class MockBitstreamConverter
    : public IBitstreamConverter
{
    IMPLEMENT_CLONE(MockBitstreamConverter);
public:
    typedef std::pair<mfxU32, mfxU32> ret_type1;

    DECLARE_TEST_METHOD1(ret_type1, GetTransformType, int )
    {
        UNREFERENCED_PARAMETER(_0);
        TEST_METHOD_TYPE(GetTransformType) params_holder;
        _GetTransformType.GetReturn(params_holder);

        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, Transform, mfxBitstream * , mfxFrameSurface1 *)
    {
        UNREFERENCED_PARAMETER(_0);
        UNREFERENCED_PARAMETER(_1);
        
        return MFX_ERR_NONE;
    }
};
