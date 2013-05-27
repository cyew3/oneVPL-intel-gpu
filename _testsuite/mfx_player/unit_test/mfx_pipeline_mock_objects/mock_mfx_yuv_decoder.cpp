/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"
#include "mock_mfx_yuv_decoder.h"
#include "mfx_test_utils.h"


mfxStatus MockYUVSource::DecodeFrameAsync(
                                   mfxBitstream2 &bs,
                                   mfxFrameSurface1 *surface_work,
                                   mfxFrameSurface1 **surface_out,
                                   mfxSyncPoint *syncp)
{
    TEST_METHOD_TYPE(DecodeFrameAsync) ret_params;

    if (!_DecodeFrameAsync.GetReturn(ret_params))
    {
        //strategy missed
        return MFX_ERR_NONE;
    }

    //copying structures
    if (ret_params.value0) bs = *ret_params.value0;
    if (surface_work && ret_params.value1) *surface_work = *ret_params.value1;
    if (surface_out && ret_params.value2) *surface_out = *ret_params.value2;
    if (syncp && ret_params.value3) *syncp = *ret_params.value3;

    return ret_params.ret_val;
}


mfxStatus MockYUVSource::DecodeHeader(mfxBitstream * pBs, mfxVideoParam * pVparam)
{
    //TODO: do we need to remove return from this initialization list
    //TODO: memory gets lost here
    mfxBitstream *newBs = pBs;
    mfxVideoParam *newVparam= pVparam;
    if (pBs)
    {
        newBs = new mfxBitstream();
        *newBs = *pBs;
    }

    if (pVparam)
    {
        newVparam = new mfxVideoParam();
        TestUtils::CopyVideoParams(newVparam, pVparam, true);
    }

    _DecodeHeader.RegisterEvent(TEST_METHOD_TYPE(DecodeHeader)(MFX_ERR_NONE, newBs, newVparam));

    TEST_METHOD_TYPE(DecodeHeader) ret_params;

    if (!_DecodeHeader.GetReturn(ret_params))
    {
        //strategy missed
        return MFX_ERR_NONE;
    }

    //copying structures
    if (pBs && ret_params.value0) *pBs = *ret_params.value0;

    TestUtils::CopyVideoParams(pVparam, ret_params.value1, false);

    return ret_params.ret_val;
}

mfxStatus MockYUVSource::Init(mfxVideoParam * in)
{
    mfxVideoParam *newVparam= in;
    if (in)
    {
        newVparam = new mfxVideoParam();
        TestUtils::CopyVideoParams(newVparam, in, true);
    }

    _Init.RegisterEvent(TEST_METHOD_TYPE(Init)(MFX_ERR_NONE, newVparam));

    return MFX_ERR_NONE;
}

mfxStatus MockYUVSource::Close()
{
    _Close.RegisterEvent(TEST_METHOD_TYPE(Close)(MFX_ERR_NONE));
    return MFX_ERR_NONE;
}

mfxStatus MockYUVSource::SyncOperation(mfxSyncPoint /*syncp*/, mfxU32 /*wait*/)
{
    TEST_METHOD_TYPE(SyncOperation) ret_params;

    if (!_SyncOperation.GetReturn(ret_params))
    {
        //strategy missed
        return MFX_ERR_NONE;
    }

    //other parameters are input only
    return ret_params.ret_val;
}
