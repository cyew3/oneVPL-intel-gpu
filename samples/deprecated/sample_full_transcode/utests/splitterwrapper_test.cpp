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
