/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_encode_query_mod4.h"

SUITE(QueryMode4Encode) {

    struct QueryMode4EncodeTest {
        TEST_METHOD_TYPE(MockVideoEncode::Init) params;
        MockVideoEncode mocker;
        QueryMode4Encode tested_object;
        mfxVideoParam par;
        mfxExtBuffer   bcap; 
        mfxExtBuffer   bother;  
        QueryMode4EncodeTest()
            : tested_object(make_instant_auto_ptr(MakeUndeletable(mocker)))
            , par() {
                mfxExtBuffer b1 = {MFX_EXTBUFF_ENCODER_CAPABILITY, sizeof(mfxExtBuffer)};
                bcap = b1;
                mfxExtBuffer   b2 = {MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtBuffer)};
                bother = b2;
        }
    };
    
    TEST_FIXTURE(QueryMode4EncodeTest, nothing_to_detach) {
        
        tested_object.Init(&par);
        CHECK(mocker._Init.WasCalled(&params));
    }

    TEST_FIXTURE(QueryMode4EncodeTest, detach_capability_and_remain_zero) {
        
        mfxExtBuffer * b1[1] = {&bcap};
        par.NumExtParam = 1;
        par.ExtParam = b1;
        tested_object.Init(&par);
        CHECK(mocker._Init.WasCalled(&params));
        CHECK_EQUAL(0, params.value0.NumExtParam);
        CHECK_EQUAL((mfxExtBuffer**)0, params.value0.ExtParam);
    }

    TEST_FIXTURE(QueryMode4EncodeTest, detach_capability_and_remain_others) {

        mfxExtBuffer * b1[2] = {&bcap, &bother};
        par.NumExtParam = 2;
        par.ExtParam = b1;
        tested_object.Init(&par);
        CHECK(mocker._Init.WasCalled(&params));
        CHECK_EQUAL(1, params.value0.NumExtParam);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_CODING_OPTION2, params.value0.ExtParam[0]->BufferId);
    }

    TEST_FIXTURE(QueryMode4EncodeTest, ON_QUERY_should_call_2times) {
        mfxVideoParam parout = {};
        mfxExtBuffer * b1[2] = {&bcap, &bother};
        par.NumExtParam = 2;
        par.ExtParam = b1;
        parout = par;
        
        tested_object.Query(&par, &parout);
        //mode2
        CHECK(mocker._Query.WasCalled(&params));
        CHECK_EQUAL(1, params.value0.NumExtParam);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_CODING_OPTION2, params.value0.ExtParam[0]->BufferId);

        //mode4
        CHECK(mocker._Query.WasCalled(&params));
        CHECK_EQUAL(2, params.value0.NumExtParam);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_ENCODER_CAPABILITY, params.value0.ExtParam[0]->BufferId);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_CODING_OPTION2, params.value0.ExtParam[1]->BufferId);

    }
}
