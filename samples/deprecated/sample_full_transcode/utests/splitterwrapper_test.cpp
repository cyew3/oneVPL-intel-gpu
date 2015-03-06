//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "splitter_wrapper.h"
#include "fileio.h"


struct SplitterWrapperTest  : public ::testing::Test, private no_copy {
    std::auto_ptr<ISplitterWrapper> splitter;
    SamplePtr sample;

    MockMFXSplitter &mocksplitter;
    MockDataIO *pMockio;
    mfxBitstream bitstream;
    MFXStreamParams si;
    MFXStreamParams params;
    SplitterWrapperTest()
        : mocksplitter(*new MockMFXSplitter())
    {
        params.NumTracks = 5;
        memset(&bitstream, 0, sizeof(bitstream));
        bitstream.Data = new mfxU8[1024];
        sample.reset(new MockSample());
    }

    void CreateSplitter() {

        splitter.reset(new SplitterWrapper(instant_auto_ptr2<MFXSplitter>(&mocksplitter),
                                           instant_auto_ptr2<MFXDataIO>(new MockDataIO())));
    }
    void getInfo() {
        EXPECT_CALL(mocksplitter, GetInfo(_)).WillRepeatedly(DoAll(SetArgReferee<0>(params),Return(MFX_ERR_NONE)));
    }
};


TEST_F(SplitterWrapperTest, getSample_success) {
    getInfo();
    EXPECT_CALL(mocksplitter, Init(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, Close()).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, GetBitstream(_, _)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, ReleaseBitstream(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(*((MockSample*)sample.get()), GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    CreateSplitter();

    EXPECT_EQ(true, splitter->GetSample(sample));
    EXPECT_EQ(SAMPLE_BITSTREAM, sample->GetSampleType());
}

TEST_F(SplitterWrapperTest, getSample_fail) {
    getInfo();
    EXPECT_CALL(mocksplitter, Init(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, Close()).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, GetBitstream(_, _)).WillRepeatedly(Return(MFX_ERR_UNDEFINED_BEHAVIOR));
    EXPECT_CALL(*((MockSample*)sample.get()), GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    CreateSplitter();
    EXPECT_THROW(splitter->GetSample(sample), SplitterGetBitstreamError);
}

TEST_F(SplitterWrapperTest, getInfo_StreamParams) {
    params.NumTracks = 5;
    params.SystemType = MFX_IVF_STREAM;

    EXPECT_CALL(mocksplitter, Init(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, Close()).WillOnce(Return(MFX_ERR_NONE));

    EXPECT_CALL(mocksplitter, GetInfo(_)).WillRepeatedly(DoAll(SetArgReferee<0>(params), Return(MFX_ERR_NONE)));

    CreateSplitter();
    splitter->GetInfo(si);
    EXPECT_EQ(5, si.NumTracks);
    EXPECT_EQ(MFX_IVF_STREAM, si.SystemType);
}

TEST_F(SplitterWrapperTest, splitterInitFailed) {
    params.NumTracks = 5;
    EXPECT_CALL(mocksplitter, Init(_)).WillOnce(Return(MFX_ERR_UNKNOWN));

    EXPECT_THROW(CreateSplitter(), SplitterInitError);
}

TEST_F(SplitterWrapperTest, splitterGetInfoFailed) {
    params.NumTracks = 5;
    EXPECT_CALL(mocksplitter, Init(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, Close()).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, GetInfo(_)).WillOnce(Return(MFX_ERR_NONE));
    CreateSplitter();
    EXPECT_CALL(mocksplitter, GetInfo(_)).WillOnce(Return(MFX_ERR_UNSUPPORTED));

    EXPECT_THROW(splitter->GetInfo(si), SplitterGetInfoError);
}

TEST_F(SplitterWrapperTest, getSample_EOF) {
    params.NumTracks = 3;
    getInfo();
    EXPECT_CALL(mocksplitter, Init(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, Close()).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(mocksplitter, GetBitstream(_, _)).WillRepeatedly(Return(MFX_ERR_MORE_DATA));
    EXPECT_CALL(*((MockSample*)sample.get()), GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    CreateSplitter();
    EXPECT_CALL(mocksplitter, ReleaseBitstream(_)).WillRepeatedly(Return(MFX_ERR_NONE));
    EXPECT_EQ(true, splitter->GetSample(sample));
    EXPECT_EQ(0, sample->GetTrackID());
    EXPECT_EQ(true, splitter->GetSample(sample));
    EXPECT_EQ(1, sample->GetTrackID());
    EXPECT_EQ(true, splitter->GetSample(sample));
    EXPECT_EQ(2, sample->GetTrackID());
    EXPECT_EQ(false, splitter->GetSample(sample));
}
