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
#include "itransform.h"
#include "transform_aenc.h"
#include "exceptions.h"
#include "mock_mfxaudio++.h"

class TransformTestAENC: public ::testing::Test{
public:

    MockPipelineFactory mockFactory;
    MockSamplePool &mockPool;
    MockMFXAudioSession mockSession;
    mfxAudioFrame aFrame;
    MockSample& mockSample;
    MockSample& mockSample2;
    MockSample& mockSampleEOS;
    MockSample& mockOutSample;
    MockMFXAudioENCODE &mockEncoder;
    mfxBitstream bitstream;
    mfxAudioAllocRequest allocRequest;
    Sequence sEFA;

    std::auto_ptr<ITransform> pTransform;

    TransformTestAENC()
        : allocRequest()
        , mockSample (*new MockSample)
        , mockSample2 (*new MockSample)
        , mockOutSample (*new MockSample)
        , mockSampleEOS (*new MockSample)
        , mockEncoder(*new MockMFXAudioENCODE)
        , mockPool(*new MockSamplePool)
    {

        EXPECT_CALL(mockFactory, CreateAudioEncoder(_)).WillOnce(Return(&mockEncoder));
        EXPECT_CALL(mockFactory, CreateSamplePool(_)).WillOnce(Return(&mockPool));
        pTransform.reset(new Transform<MFXAudioENCODE>(mockFactory, mockSession, 10000));
    }

    void LetEncoderInitReturn(mfxStatus e){
        EXPECT_CALL(mockEncoder, Init(_)).WillOnce(Return(e));
    }
    void LetQueryIOsizeReturn(mfxStatus e, int size){
        allocRequest.SuggestedOutputSize = size;
        EXPECT_CALL(mockEncoder, QueryIOSize(_,_)).WillOnce(DoAll(SetArgPointee<1>(allocRequest), Return(e)));
    }
    void LetQueryReturn(mfxStatus e){
        EXPECT_CALL(mockEncoder, Query(_, _)).WillOnce(Return(e));
    }
    void LetInitReturn(mfxStatus e){
        EXPECT_CALL(mockEncoder, Init(_)).WillOnce(Return(e));
    }
    void LetEncodeFrameAsyncReturn(mfxStatus e){
        EXPECT_CALL(mockEncoder, EncodeFrameAsync(_,_,_)).InSequence(sEFA).WillOnce(Return(e));
        EXPECT_CALL(mockSample, GetAudioFrame()).WillRepeatedly(ReturnRef(aFrame));
        EXPECT_CALL(mockOutSample, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    }
    void SetupMetaData(MockSample & smpl, bool sts) {
        EXPECT_CALL(smpl, HasMetaData(META_EOS)).WillOnce(Return(sts));
    }
};
#if 0
TEST_F(TransformTestAENC, GetSample_returns_false_after_construction){
    SamplePtr pSample;
    EXPECT_FALSE(pTransform->GetSample(pSample));
}

TEST_F(TransformTestAENC, Configure_with_video_param_throw_exception){
    mfxVideoParam vParam = {0};
    EXPECT_THROW(pTransform->Configure(vParam, NULL), IncompatibleParamTypeError);
}

TEST_F(TransformTestAENC, Query_fail_throw_exception){
    LetQueryReturn(MFX_ERR_ABORTED);
    SetupMetaData(mockSample, false);

    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSample)), EncodeQueryError);
}

TEST_F(TransformTestAENC, QueryIOsize_fail_throw_exception){
    LetQueryReturn(MFX_ERR_NONE);
    LetQueryIOsizeReturn(MFX_ERR_ABORTED, 100);
    SetupMetaData(mockSample, false);

    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSample)), EncodeQueryIOSizeError);
}

TEST_F(TransformTestAENC, EncodeInit_fail_throw_exception){
    LetQueryReturn(MFX_ERR_NONE);
    LetQueryIOsizeReturn(MFX_ERR_NONE, 100);
    LetInitReturn(MFX_ERR_NOT_INITIALIZED);
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(1));
    SetupMetaData(mockSample, false);

    EXPECT_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSample)), EncodeInitError);
}

TEST_F(TransformTestAENC, normal_operation_and_bitstream_alloc_check){
    LetQueryReturn(MFX_ERR_NONE);
    LetQueryIOsizeReturn(MFX_ERR_NONE, 100);

    mfxAudioParam aParam = {0};
    SamplePtr outSample;
    EXPECT_NO_THROW(pTransform->Configure(aParam, NULL));
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    LetInitReturn(MFX_ERR_NONE);
    SetupMetaData(mockSample, false);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(1));
    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSample)));
    LetEncodeFrameAsyncReturn(MFX_ERR_NONE);

    EXPECT_CALL(mockOutSample, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    EXPECT_CALL(mockPool, FindFreeSample()).WillOnce(ReturnRef(mockOutSample));
    EXPECT_CALL(mockPool, LockSample(_)).WillOnce(Return(&mockOutSample));
    EXPECT_CALL(mockSession, SyncOperation(_, _)).WillOnce(Return((mfxStatus)MFX_ERR_NONE));

    ASSERT_TRUE(pTransform->GetSample(outSample));
    ASSERT_TRUE(outSample.get() != NULL);
}

TEST_F(TransformTestAENC, encodeFrameAsyncReturnedError_resultInException){
    LetQueryReturn(MFX_ERR_NONE);
    LetQueryIOsizeReturn(MFX_ERR_NONE, 100);

    mfxAudioParam aParam = {0};
    SamplePtr outSample;
    EXPECT_NO_THROW(pTransform->Configure(aParam, NULL));
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    LetInitReturn(MFX_ERR_NONE);
    LetEncodeFrameAsyncReturn(MFX_ERR_NOT_ENOUGH_BUFFER);
    SetupMetaData(mockSample, false);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(1));

    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSample)));

    EXPECT_CALL(mockPool, FindFreeSample()).WillOnce(ReturnRef(mockOutSample));
    EXPECT_THROW(pTransform->GetSample(outSample), EncodeFrameAsyncError);
    delete &mockOutSample;
}


TEST_F(TransformTestAENC, syncOperation_returned_error_result_in_exception){
    LetQueryReturn(MFX_ERR_NONE);
    LetQueryIOsizeReturn(MFX_ERR_NONE, 100);

    mfxAudioParam aParam = {0};
    SamplePtr outSample;
    EXPECT_NO_THROW(pTransform->Configure(aParam, NULL));
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    LetInitReturn(MFX_ERR_NONE);
    SetupMetaData(mockSample, false);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(1));

    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSample)));
    LetEncodeFrameAsyncReturn(MFX_ERR_NONE);

    EXPECT_CALL(mockPool, FindFreeSample()).WillOnce(ReturnRef(mockOutSample));
    EXPECT_CALL(mockSession, SyncOperation(_, _)).WillOnce(Return((mfxStatus)MFX_ERR_ABORTED));

    EXPECT_THROW(pTransform->GetSample(outSample), AudioEncodeSyncOperationError);
    delete &mockOutSample;
}

TEST_F(TransformTestAENC, EOS_RECEIVED){
    LetQueryReturn(MFX_ERR_NONE);
    LetQueryIOsizeReturn(MFX_ERR_NONE, 100);

    mfxAudioParam aParam = {0};
    SamplePtr outSample;
    Sequence s1;

    EXPECT_NO_THROW(pTransform->Configure(aParam, NULL));
    EXPECT_CALL(mockPool, RegisterSample(_)).Times(1);
    LetInitReturn(MFX_ERR_NONE);
    SetupMetaData(mockSample, false);
    EXPECT_CALL(mockSample, GetTrackID()).WillOnce(Return(1));
    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSample)));

    LetEncodeFrameAsyncReturn(MFX_ERR_NONE);
    EXPECT_CALL(mockOutSample, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    EXPECT_CALL(mockPool, FindFreeSample()).InSequence(s1).WillOnce(ReturnRef(mockOutSample));
    EXPECT_CALL(mockPool, LockSample(_)).WillOnce(Return(&mockOutSample));

    EXPECT_CALL(mockSession, SyncOperation(_, _)).WillOnce(Return((mfxStatus)MFX_ERR_NONE));

    ASSERT_TRUE(pTransform->GetSample(outSample));
    ASSERT_FALSE(pTransform->GetSample(outSample));

    //now attaching EOS to sample
    SetupMetaData(mockSampleEOS, true);
    EXPECT_NO_THROW(pTransform->PutSample(instant_auto_ptr2<ISample>(&mockSampleEOS)));

    EXPECT_CALL(mockSample2, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    EXPECT_CALL(mockPool, FindFreeSample()).InSequence(s1).WillOnce(ReturnRef(mockSample2));
    EXPECT_CALL(mockEncoder, EncodeFrameAsync(0,_,_)).InSequence(sEFA).WillOnce(Return(MFX_ERR_MORE_DATA));


    ASSERT_FALSE(pTransform->GetSample(outSample));
    delete &mockSample2;
}
#endif