//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "fileio_wrapper.h"

class DataSinkTest : public Test{
public:
    MockDataIO &mockDataIo;
    MockSample &mockSample;
    mfxBitstream bs;
    DataSinkTest()
    : mockDataIo(*new MockDataIO)
    , mockSample(*new MockSample) {
    }
};

TEST_F(DataSinkTest, write_returned0_result_in_exception) {
    DataSink sink(instant_auto_ptr2<MFXDataIO>(&mockDataIo));
    EXPECT_CALL(mockSample, HasMetaData(META_EOS)).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetBitstream()).WillOnce(ReturnRef(bs));
    EXPECT_CALL(mockDataIo, Write(&bs)).WillOnce(Return(0));

    EXPECT_THROW(sink.PutSample(instant_auto_ptr2<ISample>(&mockSample)), MuxerPutSampleError);
}

TEST_F(DataSinkTest, handleEOS) {
    DataSink sink(instant_auto_ptr2<MFXDataIO>(&mockDataIo));
    EXPECT_CALL(mockSample, HasMetaData(META_EOS)).WillOnce(Return(true));
    EXPECT_NO_THROW(sink.PutSample(instant_auto_ptr2<ISample>(&mockSample)));
}
