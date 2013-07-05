/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_detach_buffer_after_query_encoder.h"

SUITE(DetachExtBufferEncode) {
    
    TEST(nothing_to_detach) {
        TEST_METHOD_TYPE(MockVideoEncode::Init) params;
        MockVideoEncode mocker;
        DetachExtBufferEncode<mfxExtCodingOption> tested_object(make_instant_auto_ptr(MakeUndeletable(mocker)));
        mfxVideoParam par = {};
        tested_object.Init(&par);
        CHECK(mocker._Init.WasCalled(&params));
    }

    TEST(detach_target_and_remain_zero) {
        TEST_METHOD_TYPE(MockVideoEncode::Init) params;
        MockVideoEncode mocker;
        DetachExtBufferEncode<mfxExtCodingOption> tested_object(make_instant_auto_ptr(MakeUndeletable(mocker)));
        mfxVideoParam par = {};
        mfxExtBuffer   b2 = {MFX_EXTBUFF_CODING_OPTION};
        mfxExtBuffer * b1[1] = {&b2};
        par.NumExtParam = 1;
        par.ExtParam = b1;
        tested_object.Init(&par);
        CHECK(mocker._Init.WasCalled(&params));
        CHECK_EQUAL(0, params.value0.NumExtParam);
        CHECK_EQUAL((mfxExtBuffer**)0, params.value0.ExtParam);
    }

    TEST(detach_target_and_remain_others) {
        TEST_METHOD_TYPE(MockVideoEncode::Init) params;
        MockVideoEncode mocker;
        DetachExtBufferEncode<mfxExtCodingOption> tested_object(make_instant_auto_ptr(MakeUndeletable(mocker)));
        mfxVideoParam par = {};
        mfxExtBuffer   b2 = {MFX_EXTBUFF_CODING_OPTION, sizeof(mfxExtBuffer)};
        mfxExtBuffer   b3 = {MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtBuffer)};
        mfxExtBuffer * b1[2] = {&b2, &b3};
        par.NumExtParam = 2;
        par.ExtParam = b1;
        tested_object.Init(&par);
        CHECK(mocker._Init.WasCalled(&params));
        CHECK_EQUAL(1, params.value0.NumExtParam);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_CODING_OPTION2, params.value0.ExtParam[0]->BufferId);
    }
}
