//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "samples_pool.h"

struct SamplesPoolTest  : public ::testing::Test {
    SamplePtr sample;
public:
    SamplePool pool;
    mfxFrameSurface1 surfBusy;
    mfxFrameSurface1 surfBusy2;
    mfxFrameSurface1 surfFree;
    std::auto_ptr<MockSample> pBusySample;
    std::auto_ptr<MockSample> pBusySample2;
    std::auto_ptr<MockSample> pFreeSample;

    mfxBitstream bitstream;
    SamplePtr freeSample ;
    SamplesPoolTest() :
        pool(SamplePool(10)) {
        surfBusy.Data.Locked = 1;
        surfBusy2.Data.Locked = 1;
        surfFree.Data.Locked = 0;
        pBusySample.reset(new MockSample());
        pBusySample2.reset(new MockSample());
        pFreeSample.reset(new MockSample());
    }
    void register_busy(int n) {
        if (n == -1) {
            EXPECT_CALL(*pBusySample, GetSurface()).WillRepeatedly(ReturnRef(surfBusy));
            EXPECT_CALL(*pBusySample, GetSampleType()).WillRepeatedly(Return(SAMPLE_SURFACE));
        } else  if (1 == n) {
            EXPECT_CALL(*pBusySample, GetSurface()).WillOnce(ReturnRef(surfBusy));
            EXPECT_CALL(*pBusySample, GetSampleType()).WillOnce(Return(SAMPLE_SURFACE));
        } else if (2 == n) {
            Sequence s1, s2;
            EXPECT_CALL(*pBusySample, GetSurface()).InSequence(s1).WillOnce(ReturnRef(surfBusy));
            EXPECT_CALL(*pBusySample, GetSampleType()).InSequence(s2).WillOnce(Return(SAMPLE_SURFACE));
            EXPECT_CALL(*pBusySample, GetSurface()).InSequence(s1).WillRepeatedly(ReturnRef(surfBusy));
            EXPECT_CALL(*pBusySample, GetSampleType()).InSequence(s2).WillOnce(Return(SAMPLE_SURFACE));
        }
        pool.RegisterSample(instant_auto_ptr((ISample*)pBusySample.release()));
    }
    void register_busy2() {
        EXPECT_CALL(*pBusySample2, GetSurface()).WillRepeatedly(ReturnRef(surfBusy2));
        EXPECT_CALL(*pBusySample2, GetSampleType()).WillRepeatedly(Return(SAMPLE_SURFACE));
        pool.RegisterSample(instant_auto_ptr((ISample*)pBusySample2.release()));
    }
    void register_free() {
        EXPECT_CALL(*pFreeSample, GetSurface()).WillRepeatedly(ReturnRef(surfFree));
        EXPECT_CALL(*pFreeSample, GetSampleType()).WillRepeatedly(Return(SAMPLE_SURFACE));
        pool.RegisterSample(instant_auto_ptr((ISample*)pFreeSample.release()));
    }

    void register_bitstream() {
        EXPECT_CALL(*pBusySample, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
        EXPECT_CALL(*pBusySample, GetSampleType()).WillRepeatedly(Return(SAMPLE_BITSTREAM));
        pool.RegisterSample(instant_auto_ptr((ISample*)pBusySample.release()));
    }
};

TEST_F(SamplesPoolTest, FindFreeSample_Surface_found) {
    register_free();
    register_busy(-1);

    SamplePtr pFreeSample(&pool.FindFreeSample());
    EXPECT_EQ(&surfFree, &pFreeSample.release()->GetSurface());
}

TEST_F(SamplesPoolTest, FindFreeSample_Surface_Not_found) {
    register_busy(-1);
    register_busy2();
    EXPECT_THROW(pool.FindFreeSample(), NoFreeSampleError);
}

TEST_F(SamplesPoolTest, FindFreeSample_Surface_Not_found2) {
    register_free();
    register_busy(-1);
    pool.LockSample(pool.FindFreeSample());
    EXPECT_THROW(pool.FindFreeSample(), NoFreeSampleError);
}

TEST_F(SamplesPoolTest, FindFreeSample_Sleep2Times) {
    register_busy(2);
    EXPECT_THROW(pool.FindFreeSample(), NoFreeSampleError);
}

TEST_F(SamplesPoolTest, FindFreeSample_Bitstream_found) {
    register_bitstream();
    freeSample.reset(&pool.FindFreeSample());
    EXPECT_EQ(&bitstream, &freeSample.release()->GetBitstream());
}

TEST_F(SamplesPoolTest, FindFreeSample_Bitstream_Not_found) {
    register_bitstream();
    pool.LockSample(pool.FindFreeSample());
    EXPECT_THROW(pool.FindFreeSample(), NoFreeSampleError);
}