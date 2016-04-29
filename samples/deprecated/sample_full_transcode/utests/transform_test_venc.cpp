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

struct TransformTestVEnc  : public ::testing::Test {
    std::auto_ptr<ITransform> transform_ve;
    MockSample& sample;
    MockSample& sampleToReturn;
    MockSample& sampleToLock;
    MockMFXVideoENCODE &encode;
    mfxIMPL impl;
    MockPipelineFactory factory;
    MockSamplePool& pool;
    MockMFXVideoSession vSession;
    mfxFrameSurface1 surface;
    mfxVideoParam vParam;
    mfxBitstream bitstream;
    bool bWasSPSBuffer;
    std::vector<mfxU8> sps;
    std::vector<mfxU8> pps;
public:

    TransformTestVEnc()
        : impl(MFX_IMPL_HARDWARE_ANY), bWasSPSBuffer()
        , sample(*new MockSample())
        , pool(*new MockSamplePool())
        , encode(*new MockMFXVideoENCODE())
        , sampleToReturn(*new MockSample())
        , sampleToLock(*new MockSample())
        , vSession(factory)
    {
        EXPECT_CALL(factory, CreateVideoEncoder(_)).WillRepeatedly(Return(&encode));
        EXPECT_CALL(factory, CreateSamplePool(_)).WillRepeatedly(Return(&pool));
        transform_ve.reset(new Transform<MFXVideoENCODE>(factory, vSession, 10));
        bitstream.Data = new mfxU8[1024];
        bitstream.MaxLength = 1024;
        memset(&vParam, 0, sizeof(vParam));
        vParam.AsyncDepth = 2;
        vParam.mfx.BufferSizeInKB = 1000;
        transform_ve->Configure(*new MFXAVParams(vParam), NULL);

        EXPECT_CALL(sample, GetSurface()).WillRepeatedly(ReturnRef(surface));
    }

    ~TransformTestVEnc() {

    }

    void PutSampleNoThrow(int nTimesGetParam = 1) {
        EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));
        EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
        EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
        EXPECT_CALL(encode, GetVideoParam(_)).Times(nTimesGetParam).WillRepeatedly(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
        EXPECT_CALL(encode, EncodeFrameAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_ERR_NONE)));

        EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
        EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
        EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
        EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

        LetQueryReturns(MFX_ERR_NONE, 1);
        LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);
        EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));

        EXPECT_NO_THROW(transform_ve->PutSample(instant_auto_ptr((ISample*)&sample)));
    }

    MOCK_METHOD0(WhatGetParamShouldReturn, mfxStatus());
    MOCK_METHOD4(OnSpsPPsBufferSize, void(mfxU16 &sps, mfxU8* spsptr, mfxU16 & pps, mfxU8* ppsptr));

    void WriteSPsPPsBuffers(mfxU16 &spssize, mfxU8* spsptr, mfxU16 & ppssize, mfxU8* ppsptr) {
        spssize = (mfxU16)sps.size();
        ppssize = (mfxU16)pps.size();
        MSDK_MEMCPY(spsptr, (void*)&sps.front(), sps.size());
        MSDK_MEMCPY(ppsptr, (void*)&pps.front(), pps.size());
    }

    mfxStatus SaveWetherSPSPPSBuffer(mfxVideoParam *par) {
        if (par->ExtParam[0]->BufferId == MFX_EXTBUFF_CODING_OPTION_SPSPPS) {
            bWasSPSBuffer = true;
            mfxExtCodingOptionSPSPPS *p = (mfxExtCodingOptionSPSPPS *)par->ExtParam[0];
            OnSpsPPsBufferSize(p->SPSBufSize, p->SPSBuffer, p->PPSBufSize, p->PPSBuffer);
        }
        return WhatGetParamShouldReturn();
    }

    void LetQueryReturns(mfxStatus sts, int nTimes) {
        EXPECT_CALL(encode, Query(_, _)).Times(nTimes).WillRepeatedly(Return(sts));
    }

    void LetQueryIOSurfReturns(mfxStatus sts,int asyncDEpth, int nTimes) {
        mfxFrameAllocRequest request = {};
        request.NumFrameSuggested = request.NumFrameMin = asyncDEpth;
        EXPECT_CALL(encode, QueryIOSurf(_, _)).Times(nTimes).WillRepeatedly(DoAll(SetArgumentPointee<1>(request), Return(sts)));
    }

    void InitAll() {
        EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
        EXPECT_CALL(encode, GetVideoParam(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
        EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
        EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
        EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
        EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
        EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    }
};


TEST_F(TransformTestVEnc, PutSample_Device_Busy) {
    InitAll();
    LetQueryReturns(MFX_ERR_NONE, 1);
    LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);
    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));
    EXPECT_CALL(encode, EncodeFrameAsync(_, _, _, _)).Times(2).WillRepeatedly(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_WRN_DEVICE_BUSY)));

    EXPECT_THROW(transform_ve->PutSample(instant_auto_ptr((ISample*)&sample)), EncodeTimeoutError);

}

TEST_F(TransformTestVEnc, PutSample_EncodeInitError) {
    EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_UNSUPPORTED)));
    EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
    EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    LetQueryReturns(MFX_ERR_NONE, 1);
    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));
    EXPECT_THROW(transform_ve->PutSample(instant_auto_ptr((ISample*)&sample)), EncodeInitError);

    delete &sampleToReturn;
}

TEST_F(TransformTestVEnc, PutSample_EncodeGetVideoParamError) {
    EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).WillOnce(Return(MFX_ERR_UNSUPPORTED));

    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
    EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());

    LetQueryReturns(MFX_ERR_NONE, 1);
    LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);
    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));
    EXPECT_THROW(transform_ve->PutSample(instant_auto_ptr((ISample*)&sample)), EncodeGetVideoParamError);

    delete &sampleToReturn;
}

TEST_F(TransformTestVEnc, MakeSPSPPS_attaches_extBuffer) {
    Sequence s1;
    EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).InSequence(s1).WillOnce(DoAll(SetArgumentPointee<0>(vParam), Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).InSequence(s1).WillOnce(Invoke(this, &TransformTestVEnc::SaveWetherSPSPPSBuffer));
    EXPECT_CALL(*this, WhatGetParamShouldReturn()).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(*this, OnSpsPPsBufferSize(_, _,_,_)).Times(1);
    EXPECT_CALL(vSession, SyncOperation(_, _)).WillRepeatedly(Return(MFX_ERR_NONE));
    EXPECT_CALL(encode, EncodeFrameAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_ERR_NONE)));
    EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
    EXPECT_CALL(sample, GetMetaData(META_SPSPPS, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(sample, GetMetaData(META_VIDEOPARAM, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(sample, GetMetaData(META_EOS, _)).WillRepeatedly(Return(false));

    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
    EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    LetQueryReturns(MFX_ERR_NONE, 1);
    LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);
    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(true));

    transform_ve->PutSample(instant_auto_ptr((ISample*)&sample));
    SamplePtr out_sample;
    transform_ve->GetSample(out_sample);

    EXPECT_EQ(true, bWasSPSBuffer);
}

TEST_F(TransformTestVEnc, PutSample_EOS) {
    EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam), Return(MFX_ERR_NONE)));
    EXPECT_CALL(vSession, SyncOperation(_, _)).WillRepeatedly(Return(MFX_ERR_NONE));
    EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
    EXPECT_CALL(sample, GetMetaData(META_SPSPPS, _)).WillRepeatedly(Return(false));
    EXPECT_CALL(sample, GetMetaData(META_VIDEOPARAM, _)).WillRepeatedly(Return(false));

    LetQueryReturns(MFX_ERR_NONE, 1);
    LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);
    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(true));

    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
    EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    EXPECT_CALL(encode, EncodeFrameAsync(0, 0, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_ERR_MORE_DATA)));

    EXPECT_NO_THROW(transform_ve->PutSample(instant_auto_ptr((ISample*)&sample)));
    SamplePtr sample_out;
    EXPECT_TRUE(transform_ve->GetSample(sample_out));
    EXPECT_TRUE(sample_out->HasMetaData(META_EOS));

    EXPECT_FALSE(transform_ve->GetSample(sample_out));
}

TEST_F(TransformTestVEnc, MakeSPSPPS_verify_sps_pps_joining) {
    Sequence ss1;
    EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).InSequence(ss1).WillOnce(DoAll(SetArgumentPointee<0>(vParam), Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).InSequence(ss1).WillOnce(Invoke(this, &TransformTestVEnc::SaveWetherSPSPPSBuffer));

    EXPECT_CALL(*this, WhatGetParamShouldReturn()).WillOnce(Return(MFX_ERR_NONE));
    sps.push_back(1);
    sps.push_back(2);
    pps.push_back(3);
    pps.push_back(4);

    EXPECT_CALL(*this, OnSpsPPsBufferSize(_, _, _, _)).WillOnce(Invoke(this, &TransformTestVEnc::WriteSPsPPsBuffers));
    EXPECT_CALL(encode, EncodeFrameAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_ERR_NONE)));

    EXPECT_CALL(vSession, SyncOperation(_, _)).WillRepeatedly(Return(MFX_ERR_NONE));
    EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
    EXPECT_CALL(sample, GetMetaData(META_SPSPPS, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(sample, GetMetaData(META_VIDEOPARAM, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));
    //EXPECT_CALL(sample, GetMetaData(META_EOS, _)).WillRepeatedly(Return(false));

    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
    EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));


    LetQueryReturns(MFX_ERR_NONE, 1);
    LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);

    transform_ve->PutSample(instant_auto_ptr((ISample*)&sample));
    instant_auto_ptr((ISample*)0);
    SamplePtr out_sample;
    transform_ve->GetSample(out_sample);
    MetaData mData;
    EXPECT_TRUE(out_sample->GetMetaData(META_SPSPPS, mData));
    EXPECT_EQ(1, mData[0]);
    EXPECT_EQ(2, mData[1]);
    EXPECT_EQ(3, mData[2]);
    EXPECT_EQ(4, mData[3]);
    EXPECT_EQ(4, mData.size());
}

TEST_F(TransformTestVEnc, MakeSPSPPS_GetVideoParamReturns_More_Buffer) {
    Sequence ss1;
    Sequence ss2;
    Sequence ss3;
    EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).InSequence(ss1).WillOnce(DoAll(SetArgumentPointee<0>(vParam), Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).InSequence(ss1).WillOnce(Invoke(this, &TransformTestVEnc::SaveWetherSPSPPSBuffer));
    EXPECT_CALL(encode, GetVideoParam(_)).InSequence(ss1).WillOnce(Invoke(this, &TransformTestVEnc::SaveWetherSPSPPSBuffer));

    EXPECT_CALL(*this, WhatGetParamShouldReturn()).InSequence(ss2).WillOnce(Return(MFX_ERR_NOT_ENOUGH_BUFFER));
    EXPECT_CALL(*this, WhatGetParamShouldReturn()).InSequence(ss2).WillOnce(Return(MFX_ERR_NONE));
    mfxU16 sps1,pps1;
    mfxU16 sps2,pps2;
    EXPECT_CALL(*this, OnSpsPPsBufferSize(_, _, _, _)).InSequence(ss3).WillOnce(DoAll(SaveArg<0>(&sps1), SaveArg<2>(&pps1)));
    EXPECT_CALL(*this, OnSpsPPsBufferSize(_, _, _, _)).InSequence(ss3).WillOnce(DoAll(SaveArg<0>(&sps2), SaveArg<2>(&pps2)));

    LetQueryReturns(MFX_ERR_NONE, 1);
    LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);

    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());
    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
    EXPECT_CALL(pool, LockSample(_)).WillRepeatedly(Return((&sampleToReturn)));
    EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    EXPECT_CALL(encode, EncodeFrameAsync(_, _, _, _)).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_ERR_NONE)));
    EXPECT_CALL(vSession, SyncOperation(_, _)).WillRepeatedly(Return(MFX_ERR_NONE));

    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));
    EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));


    transform_ve->PutSample(instant_auto_ptr2<ISample>(&sample));
    SamplePtr sample_out;
    transform_ve->GetSample(sample_out);

    EXPECT_EQ(sps2 > sps1 && pps2 > pps1, bWasSPSBuffer);
}


TEST_F(TransformTestVEnc, PutSample_NoThrow) {
    PutSampleNoThrow();
    delete &sampleToReturn;
}

TEST_F(TransformTestVEnc, GetSample_SyncOperationWrnInExecution) {
    PutSampleNoThrow(1);

    EXPECT_CALL(vSession, SyncOperation(_, _)).WillOnce(Return(MFX_WRN_IN_EXECUTION));
    SamplePtr outSample;
    //EXPECT_FALSE(transform_ve->GetSample(outSample));
    EXPECT_ANY_THROW(transform_ve->GetSample(outSample));
    delete &sampleToReturn;
}

TEST_F(TransformTestVEnc, GetSample_SyncOperationSuccess) {
    PutSampleNoThrow(2);

    EXPECT_CALL(vSession, SyncOperation(_, _)).WillOnce(Return(MFX_ERR_NONE));
    SamplePtr outSample;
    EXPECT_TRUE(transform_ve->GetSample(outSample));
}



TEST_F(TransformTestVEnc, GetSample_SyncOperationError) {

    PutSampleNoThrow(1);

    EXPECT_CALL(vSession, SyncOperation(_, _)).WillOnce(Return(MFX_ERR_UNDEFINED_BEHAVIOR));
    SamplePtr outSample;
    EXPECT_THROW(transform_ve->GetSample(outSample), EncodeSyncOperationError);
    delete &sampleToReturn;
}

TEST_F(TransformTestVEnc, PutSample_ErrMoreBitstream) {
    EXPECT_CALL(encode, Init(_)).WillOnce(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
    EXPECT_CALL(encode, GetVideoParam(_)).WillRepeatedly(DoAll(SetArgumentPointee<0>(vParam) ,Return(MFX_ERR_NONE)));
    Sequence s1;
    Sequence s2;
    EXPECT_CALL(encode, EncodeFrameAsync(_, _, _, _)).InSequence(s1).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_ERR_MORE_BITSTREAM)));
    EXPECT_CALL(encode, EncodeFrameAsync(_, _, _, _)).InSequence(s1).WillOnce(DoAll(SetArgPointee<3>((mfxSyncPoint)0),Return(MFX_ERR_NONE)));
    EXPECT_CALL(vSession, SyncOperation(_, _)).Times(2).WillRepeatedly(Return(MFX_ERR_NONE));

    LetQueryReturns(MFX_ERR_NONE, 1);
    LetQueryIOSurfReturns(MFX_ERR_NONE, 1, 1);

    EXPECT_CALL(sample, HasMetaData(META_EOS)).WillRepeatedly(Return(false));
    EXPECT_CALL(sample, GetTrackID()).WillRepeatedly(Return(0));
    EXPECT_CALL(pool, RegisterSample(_)).WillRepeatedly(Return());

    EXPECT_CALL(pool, FindFreeSample()).WillRepeatedly(ReturnRef(sample));
    EXPECT_CALL(pool, LockSample(_)).InSequence(s2).WillOnce(Return((&sampleToReturn)));
    EXPECT_CALL(pool, LockSample(_)).InSequence(s2).WillOnce(Return((&sampleToLock)));
    EXPECT_CALL(sampleToReturn, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));
    EXPECT_CALL(sampleToLock, GetBitstream()).WillRepeatedly(ReturnRef(bitstream));

    transform_ve->PutSample(instant_auto_ptr2<ISample>(&sample));

    //drain sps
    SamplePtr out_sample;
    transform_ve->GetSample(out_sample);
    //two more samples are ready
    EXPECT_EQ(true, transform_ve->GetSample(out_sample));
    EXPECT_EQ(true, transform_ve->GetSample(out_sample));
    EXPECT_EQ(false, transform_ve->GetSample(out_sample));
}

