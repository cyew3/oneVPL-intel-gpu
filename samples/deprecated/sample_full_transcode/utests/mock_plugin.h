//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfxplugin++.h"
#include <gmock/gmock.h>


class MockMFXPlugin : public MFXPlugin {
    MOCK_METHOD1(Init, mfxStatus (mfxVideoParam *));
    MOCK_METHOD2(SetAuxParams, mfxStatus (void *, int));
    MOCK_METHOD2(QueryIOSurf, mfxStatus (mfxVideoParam *,  mfxFrameAllocRequest *));
};

class MockMFXCodecPlugin : public MFXCodecPlugin {
    MOCK_METHOD2(Query, mfxStatus (mfxVideoParam *, mfxVideoParam *));
    MOCK_METHOD1(Reset, mfxStatus (mfxVideoParam *));
    MOCK_METHOD1(GetVideoParam, mfxStatus (mfxVideoParam *));
};

class MockMFXDecoderPlugin : public MFXDecoderPlugin, MockMFXCodecPlugin {
    MOCK_METHOD2(DecodeHeader, mfxStatus (mfxBitstream *, mfxVideoParam *));
    MOCK_METHOD2(GetPayload, mfxStatus (mfxU64 *, mfxPayload *));
    MOCK_METHOD4(DecodeFrameSubmit, mfxStatus (mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 **,  mfxThreadTask *));
};

class MockMFXEncoderPlugin : public MFXEncoderPlugin {
    MOCK_METHOD4(DecodeFrameSubmit, mfxStatus (mfxEncodeCtrl *, mfxFrameSurface1 *, mfxBitstream *, mfxThreadTask *));
};
