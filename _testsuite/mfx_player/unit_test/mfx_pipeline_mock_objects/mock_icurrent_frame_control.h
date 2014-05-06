#include "mfx_icurrent_frame_ctrl.h"
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_icurrent_frame_ctrl.h"
#include "test_method.h"
#include "mfx_test_utils.h"


class MockCurrentFrameControl : public ICurrentFrameControl
{
public:
    DECLARE_TEST_METHOD1(void, AddExtBuffer, MAKE_DYNAMIC_TRAIT(mfxExtBuffer, mfxExtBuffer&)) {
        _AddExtBuffer.RegisterEvent(TEST_METHOD_TYPE(AddExtBuffer)(MFX_ERR_NONE, _0));
    }
    DECLARE_TEST_METHOD1(void,  RemoveExtBuffer, mfxU32 ) {
        _RemoveExtBuffer.RegisterEvent(TEST_METHOD_TYPE(RemoveExtBuffer)(MFX_ERR_NONE, _0));
    }
};