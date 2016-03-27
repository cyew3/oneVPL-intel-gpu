//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2016 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "gmock/gmock.h"

#include "mfxvideo++int.h"
#include "mfxplugin++.h"

namespace ApiTestCommon {
    struct ParamSet;

    class MockMFXCoreInterface : public MFXCoreInterface1 {
    public:

        MOCK_METHOD1(GetCoreParam, mfxStatus (mfxCoreParam *par));
        MOCK_METHOD2(GetHandle, mfxStatus (mfxHandleType type, mfxHDL *handle));
        MOCK_METHOD1(IncreaseReference, mfxStatus (mfxFrameData *fd));
        MOCK_METHOD1(DecreaseReference, mfxStatus (mfxFrameData *fd));
        MOCK_METHOD2(CopyFrame, mfxStatus (mfxFrameSurface1 *dst, mfxFrameSurface1 *src));
        MOCK_METHOD3(CopyBuffer, mfxStatus (mfxU8 *dst, mfxU32 size, mfxFrameSurface1 *src));
        MOCK_METHOD3(MapOpaqueSurface, mfxStatus (mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf));
        MOCK_METHOD3(UnmapOpaqueSurface, mfxStatus (mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf));
        MOCK_METHOD2(GetRealSurface, mfxStatus (mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf));
        MOCK_METHOD2(GetOpaqueSurface, mfxStatus (mfxFrameSurface1 *surf, mfxFrameSurface1 **op_surf));
        MOCK_METHOD2(CreateAccelerationDevice, mfxStatus (mfxHandleType type, mfxHDL *handle));
//        MOCK_METHOD0(FrameAllocator, mfxFrameAllocator & ());

        MockMFXCoreInterface();
        bool IsCoreSet() { return 1; };
        void SetParamSet(const ParamSet &param);

        const ParamSet *paramset;
//        mfxMemId memIds[10];
    };
};
