/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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