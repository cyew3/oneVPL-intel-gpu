/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ifile.h"
#include "test_method.h"

class MockFile : public IFile
{
    IMPLEMENT_CLONE(MockFile);
public:
    DECLARE_TEST_METHOD0(mfxStatus, GetLastError)
    {
        TEST_METHOD_TYPE(GetInfo) params_holder;
        if (!_GetLastError.GetReturn(params_holder))
        {
            //no strategy
            return MFX_ERR_NONE;
        }
        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, GetInfo, const tstring &, MAKE_STATIC_TRAIT(mfxU64, mfxU64&))
    {
        _0;
        TEST_METHOD_TYPE(GetInfo) params_holder;
        if (!_GetInfo.GetReturn(params_holder))
        {
            //no strategy
            return MFX_ERR_NONE;
        }
        _1 = params_holder.value1;
        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, Open, const tstring &, const tstring&);
    DECLARE_TEST_METHOD0(bool, isOpen);
    DECLARE_TEST_METHOD0(bool, isEOF)
    {
        TEST_METHOD_TYPE(isEOF) params_holder;
        if (!_isEOF.GetReturn(params_holder))
        {
            //no strategy
            return true;
        }
        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD0(mfxStatus, Close);
    DECLARE_TEST_METHOD0(mfxStatus, Reopen);
    DECLARE_TEST_METHOD2(mfxStatus, Read, mfxU8*, mfxU32 &);
    DECLARE_TEST_METHOD2(mfxStatus, ReadLine, vm_char*, const mfxU32 &)
    {
        _1;
        TEST_METHOD_TYPE(ReadLine) params_holder;
        if (!_ReadLine.GetReturn(params_holder))
        {
            //no strategy
            return MFX_ERR_NONE;
        }
        vm_string_strcpy(_0, params_holder.value0);
        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD2(mfxStatus, Write, mfxU8*, mfxU32 );
    
    mfxStatus Write(mfxBitstream *);
};
