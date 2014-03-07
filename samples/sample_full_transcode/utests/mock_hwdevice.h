//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "gmock\gmock.h"
#include "hw_device.h"

class MockCHWDevice : public CHWDevice
{
public:
    MOCK_METHOD3(Init, mfxStatus (mfxHDL hWindow, mfxU16 nViews, mfxU32 nAdapterNum));
    MOCK_METHOD2(GetHandle, mfxStatus (mfxHandleType type, mfxHDL *pHdl));
    MOCK_METHOD2(SetHandle, mfxStatus (mfxHandleType type, mfxHDL pHdl));
    MOCK_METHOD0(Reset, mfxStatus ());
    MOCK_METHOD2(RenderFrame, mfxStatus (mfxFrameSurface1 * pSurface, mfxFrameAllocator * pmfxAlloc));
    MOCK_METHOD0(Close, void ());
};

