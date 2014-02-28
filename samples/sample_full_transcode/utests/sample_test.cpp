//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "samples_pool.h"

struct SampleTest  : public ::testing::Test {
    SamplePtr sample;
public:

    mfxBitstream bitstream;
    mfxFrameSurface1 surface;
};

TEST_F(SampleTest, SampleBitstream_NullDatalen_Error) {    
    EXPECT_THROW(SampleBitstream((mfxU32)0, 0), SampleBitstreamNullDatalenError);
}

TEST_F(SampleTest, SampleBitstream_NullDatalen_Error2) {    
    bitstream.DataLength = 0;
    EXPECT_THROW(SampleBitstream(&bitstream, 0), SampleBitstreamNullDatalenError);
}