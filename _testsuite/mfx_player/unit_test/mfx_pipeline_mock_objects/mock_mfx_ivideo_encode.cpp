/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mock_mfx_ivideo_encode.h"
#include "mfx_test_utils.h"

mfxStatus MockVideoEncode::EncodeFrameAsync( mfxEncodeCtrl *pCtrl
                                           , mfxFrameSurface1 *pSurface
                                           , mfxBitstream *pBs
                                           , mfxSyncPoint *pSync)
{
    mfxBitstream bsTo = {};
    TestUtils::CopyExtParamsEnabledStructures(&bsTo, pBs, true);
    _EncodeFrameAsync.RegisterEvent(TEST_METHOD_TYPE(EncodeFrameAsync)(MFX_ERR_NONE, pCtrl, pSurface, bsTo, pSync));

    TEST_METHOD_TYPE(EncodeFrameAsync) return_args;

    if (!_EncodeFrameAsync.GetReturn(return_args))
    {
        //strategy not defined
        return MFX_ERR_NONE;
    }

    if (NULL != pCtrl && NULL != return_args.value0)
        *pCtrl = *return_args.value0;
    if (NULL != pSurface && NULL != return_args.value1)
        *pSurface = *return_args.value1;
    if (NULL != pBs)
        *pBs = return_args.value2;

    if (NULL != pSync && NULL != return_args.value3)
        *pSync = *return_args.value3;

    return return_args.ret_val;
}

mfxStatus MockVideoEncode::SyncOperation(mfxSyncPoint sp, mfxU32 wait)
{
    _SyncOperation.RegisterEvent(TEST_METHOD_TYPE(SyncOperation)(MFX_ERR_NONE, sp, wait));

    TEST_METHOD_TYPE(SyncOperation) return_args;

    if (!_SyncOperation.GetReturn(return_args))
    {
        return MFX_ERR_NONE;
    }

    return return_args.ret_val;
}

mfxStatus MockVideoEncode::Query(mfxVideoParam * in, mfxVideoParam * out)
{
    mfxVideoParam *newIn = in;
    mfxVideoParam *newOut = out;
    if (in)
    {
        newIn = new mfxVideoParam();
        TestUtils::CopyExtParamsEnabledStructures(newIn, in, true);
    }
    if (out)
    {
        out = new mfxVideoParam();
        TestUtils::CopyExtParamsEnabledStructures(newOut, out, true);
    }    
    _Query.RegisterEvent(TEST_METHOD_TYPE(Query)(MFX_ERR_NONE, newIn, newOut));

    TEST_METHOD_TYPE(Query) return_args;

    if (!_Query.GetReturn(return_args))
    {
        return MFX_ERR_NONE;
    }
    return return_args.ret_val;
}