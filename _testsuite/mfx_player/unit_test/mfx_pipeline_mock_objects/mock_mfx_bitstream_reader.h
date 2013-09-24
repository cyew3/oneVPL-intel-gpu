/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "test_method.h"
#include "mfx_ibitstream_reader.h"

class MockBitstreamReader : public IBitstreamReader
{
public:
    
    virtual void      Close(){}
    virtual mfxStatus Init(const vm_char *strFileName) { strFileName;return MFX_ERR_NONE;}
    DECLARE_TEST_METHOD1(mfxStatus, ReadNextFrame, MAKE_REF_BY_VALUE_TRAIT(mfxBitstream2)) 
    {
        _ReadNextFrame.RegisterEvent(TEST_METHOD_TYPE(ReadNextFrame)(MFX_ERR_NONE, _0));
        
        TEST_METHOD_TYPE(ReadNextFrame) ret_params;
        if (!_ReadNextFrame.GetReturn(ret_params))
        {
            return MFX_ERR_NONE;
        }
        _0 = ret_params.value0;
        
        return ret_params.ret_val;
    }
    virtual bool isFrameModeEnabled() {return true;}
    virtual mfxStatus GetStreamInfo(sStreamInfo * pParams) {pParams;return MFX_ERR_NONE;}
    virtual mfxStatus SeekTime(mfxF64 fSeekTo) {fSeekTo;return MFX_ERR_NONE;}
    virtual mfxStatus SeekPercent(mfxF64 fSeekTo) {fSeekTo;return MFX_ERR_NONE;}
    //reposition to specific frames number offset in  input stream
    virtual mfxStatus SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &in_info) {nFrameOffset; in_info; return MFX_ERR_NONE;}
};

