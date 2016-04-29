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

#include "fileio.h"

struct MuxerWrapperTest  : public ::testing::Test {
    std::auto_ptr<MuxerWrapper> muxer;
    MockSample &mockSample;
    MockMFXMuxer &mockmuxer;
    mfxBitstream bitsteam;
    MFXStreamParamsExt sParams;
    MockDataIO &mockio;
    //mfxTrackInfo *info[20];
    mfxTrackInfo  videoTrack;
    mfxTrackInfo  audioTrack;
    mfxU8 data[100];
    MuxerWrapperTest()
        : mockmuxer(*new MockMFXMuxer())
        , mockio(*new MockDataIO())
        , mockSample(*new MockSample())
        , bitsteam() {
            bitsteam.Data = data;
            bitsteam.DataLength = 1;
            videoTrack.Type = MFX_TRACK_H264;
            audioTrack.Type = MFX_TRACK_AAC;
            //sParams.TrackInfo = info;
    }
    void CreateMuxer() {
        EXPECT_CALL(mockmuxer, Init(_, _)).WillOnce(Return(MFX_ERR_NONE));
        muxer.reset(new MuxerWrapper(instant_auto_ptr2<MFXMuxer>(&mockmuxer), sParams, instant_auto_ptr2<MFXDataIO>(&mockio)));
    }
    bool GetMetaDataSPS(int type, MetaData& out) const  {
        out.resize(1);
        return true;
    }
    bool GetMetaDataVideoParams(int type, MetaData& out) const  {
        out.resize(1);
        return true;
    }
};


TEST_F(MuxerWrapperTest, initFailureResultinException) {
    EXPECT_CALL(mockmuxer, Init(_, _)).WillOnce(Return(MFX_ERR_UNKNOWN));
    EXPECT_CALL(mockSample, HasMetaData(META_EOS)).WillOnce(Return(false));
    std::auto_ptr<MuxerWrapper> wrp (new MuxerWrapper(instant_auto_ptr2<MFXMuxer>(&mockmuxer), sParams, instant_auto_ptr2<MFXDataIO>(&mockio)));
    EXPECT_THROW(wrp->PutSample(instant_auto_ptr2<ISample>(&mockSample)), MuxerInitError);
}


TEST_F(MuxerWrapperTest, putSample_fail) {
    EXPECT_CALL(mockmuxer, PutBitstream(_, _, _)).WillOnce(Return(MFX_ERR_UNDEFINED_BEHAVIOR));
    EXPECT_CALL(mockSample, GetBitstream()).WillOnce(ReturnRef(bitsteam));
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(2277));
    EXPECT_CALL(mockSample, HasMetaData(META_EOS)).WillOnce(Return(false));

    CreateMuxer();
    EXPECT_THROW(muxer->PutSample(instant_auto_ptr2<ISample>(&mockSample)), MuxerPutSampleError);
}

TEST_F(MuxerWrapperTest, putSample_mock_sample) {
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(2277));
    EXPECT_CALL(mockSample, GetBitstream()).WillOnce(ReturnRef(bitsteam));
    EXPECT_CALL(mockSample, HasMetaData(META_EOS)).WillOnce(Return(false));
    //due to trackmapping id not saved
    EXPECT_CALL(mockmuxer, PutBitstream(0, _, _)).WillOnce(Return(MFX_ERR_NONE));


    CreateMuxer();
    EXPECT_NO_THROW(muxer->PutSample(instant_auto_ptr2<ISample>(&mockSample)));
}

TEST_F(MuxerWrapperTest, NoInitIfNotEnoughVideoData) {
    EXPECT_CALL(mockSample, HasMetaData(META_EOS)).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetMetaData(META_VIDEOPARAM, _)).Times(1);
    EXPECT_CALL(mockSample, GetMetaData(META_SPSPPS, _)).Times(1);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(0));
    EXPECT_CALL(mockSample, GetBitstream()).WillOnce(ReturnRef(bitsteam));

    sParams.Tracks().push_back(videoTrack);
    sParams.Tracks().push_back(audioTrack);

    std::auto_ptr<MuxerWrapper> wrp (new MuxerWrapper(instant_auto_ptr2<MFXMuxer>(&mockmuxer), sParams, instant_auto_ptr2<MFXDataIO>(&mockio)));
    EXPECT_NO_THROW(wrp->PutSample(instant_auto_ptr2<ISample>(&mockSample)));
}

TEST_F(MuxerWrapperTest, DefferedAudioSampleOrder) {
    mfxBitstream abs1 = {};
    mfxBitstream abs2 = {};
    mfxBitstream abs3 = {};
    mfxBitstream vbs1 = {};
    mfxU8 buf[1];
    abs1.TimeStamp = 1;
    abs1.DataLength = 1;
    abs1.MaxLength = 1;
    abs1.Data = buf;

    abs2.TimeStamp = 2;
    abs2.DataLength = 1;
    abs2.MaxLength = 1;
    abs2.Data = buf;


    abs3.TimeStamp = 3;
    abs3.DataLength = 1;
    abs3.MaxLength = 1;
    abs3.Data = buf;

    vbs1.TimeStamp = 99;
    vbs1.DataLength = 1;
    vbs1.MaxLength = 1;
    vbs1.Data = buf;

    Sequence s1;
    Sequence s2;
    Sequence s3;
    Sequence s4;

    EXPECT_CALL(mockSample, GetMetaData(META_VIDEOPARAM, _)).InSequence(s2).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetMetaData(META_VIDEOPARAM, _)).InSequence(s2).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetMetaData(META_VIDEOPARAM, _)).InSequence(s2).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetMetaData(META_VIDEOPARAM, _)).InSequence(s2).WillOnce(Invoke(this, &MuxerWrapperTest::GetMetaDataVideoParams));

    EXPECT_CALL(mockSample, GetMetaData(META_SPSPPS, _)).InSequence(s3).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetMetaData(META_SPSPPS, _)).InSequence(s3).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetMetaData(META_SPSPPS, _)).InSequence(s3).WillOnce(Return(false));
    EXPECT_CALL(mockSample, GetMetaData(META_SPSPPS, _)).InSequence(s3).WillOnce(Invoke(this, &MuxerWrapperTest::GetMetaDataSPS));

    EXPECT_CALL(mockSample, GetTrackID()).InSequence(s4).WillOnce(Return(0));
    EXPECT_CALL(mockSample, GetTrackID()).InSequence(s4).WillOnce(Return(0));
    EXPECT_CALL(mockSample, GetTrackID()).InSequence(s4).WillOnce(Return(0));
    EXPECT_CALL(mockSample, GetTrackID()).InSequence(s4).WillOnce(Return(1));

    EXPECT_CALL(mockSample, GetBitstream()).InSequence(s1).WillOnce(ReturnRef(abs1));
    EXPECT_CALL(mockSample, GetBitstream()).InSequence(s1).WillOnce(ReturnRef(abs2));
    EXPECT_CALL(mockSample, GetBitstream()).InSequence(s1).WillOnce(ReturnRef(abs3));
    EXPECT_CALL(mockSample, GetBitstream()).InSequence(s1).WillOnce(ReturnRef(vbs1));

    detail::SamplePoolForMuxer sp(2);
    mfxTrackInfo tInfo;
    SamplePtr mock_sample_ptr(&mockSample);

    //1st audio
    EXPECT_FALSE(sp.GetVideoInfo(tInfo));
    sp.PutSample(mock_sample_ptr);

    //2nd audio
    EXPECT_FALSE(sp.GetVideoInfo(tInfo));
    sp.PutSample(mock_sample_ptr);

    //3rd audio
    EXPECT_FALSE(sp.GetVideoInfo(tInfo));
    sp.PutSample(mock_sample_ptr);

    //1 video
    EXPECT_FALSE(sp.GetVideoInfo(tInfo));
    sp.PutSample(mock_sample_ptr);

    EXPECT_TRUE(sp.GetVideoInfo(tInfo));

    SamplePtr p;
    EXPECT_TRUE(sp.GetSample(p));
    EXPECT_EQ(1, p->GetBitstream().TimeStamp);
    EXPECT_TRUE(sp.GetSample(p));
    EXPECT_EQ(2, p->GetBitstream().TimeStamp);
    EXPECT_TRUE(sp.GetSample(p));
    EXPECT_EQ(3, p->GetBitstream().TimeStamp);
    EXPECT_TRUE(sp.GetSample(p));
    EXPECT_EQ(99, p->GetBitstream().TimeStamp);
    EXPECT_FALSE(sp.GetSample(p));
}