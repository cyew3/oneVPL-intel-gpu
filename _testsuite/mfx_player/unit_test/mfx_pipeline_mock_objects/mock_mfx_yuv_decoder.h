/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include <memory>
#include "mfx_iyuv_source.h"
#include "test_method.h"

class MockYUVSource : public IYUVSource
{
public:
    DECLARE_TEST_METHOD1(mfxStatus, Init, mfxVideoParam *);
    
    DECLARE_TEST_METHOD0( mfxStatus, Close);

    virtual mfxStatus Query(mfxVideoParam * /*in*/, mfxVideoParam  * /*out*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest * /*request*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus GetDecodeStat(mfxDecodeStat * /*stat*/)
    {
        return MFX_ERR_NONE;
    }

    DECLARE_TEST_METHOD4(mfxStatus, DecodeFrameAsync, 
                         mfxBitstream2 &,
                         mfxFrameSurface1 *,
                         mfxFrameSurface1 **,
                         mfxSyncPoint *);

    DECLARE_TEST_METHOD2(mfxStatus, SyncOperation, mfxSyncPoint /*syncp*/, mfxU32 /*wait*/);
    DECLARE_TEST_METHOD2(mfxStatus, DecodeHeader, mfxBitstream *, mfxVideoParam *);

    virtual mfxStatus Reset(mfxVideoParam * /*par*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam * /*par*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus SetSkipMode(mfxSkipMode /*mode*/)
    {
        return MFX_ERR_NONE;
    }
};
