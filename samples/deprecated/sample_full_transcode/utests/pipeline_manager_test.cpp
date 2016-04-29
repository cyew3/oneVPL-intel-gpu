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

#include "pipeline_manager.h"


class PipelineManagerRunTest : public PipelineManager, public ::testing::Test{
    PipelineFactory fac1;
public:
    MockSource &mockSource;
    MockTransformStorage &mockStorage ;
    MockSink &mockSink;

    std::vector<ITransform*> transvec;

    PipelineManagerRunTest()
        : PipelineManager(fac1)
        , mockSink(*new MockSink())
        , mockStorage (*new MockTransformStorage())
        , mockSource(*new MockSource())

    {
            //v1.push_back(new MockTransform());

            Transforms().reset(&mockStorage);
            Source().reset(&mockSource);
            Sink().reset(&mockSink);

    }
    std::auto_ptr<TransformStorage>& Transforms() {
        return m_transforms;
    }
    std::auto_ptr<ISink>& Sink() {
        return m_pSink;
    }
    std::auto_ptr<ISplitterWrapper>& Source() {
        return m_pSource;
    }

    void FPutSample(SamplePtr& sample) {
        sample.reset(0);
    }
};

class MockGetSample {
public:
    std::auto_ptr<MockSample> mockedSampleToReturn;
    MockGetSample (MockSample *smpl) : mockedSampleToReturn(smpl){

    }
    bool GetSample(SamplePtr& sample) {
        sample.reset(mockedSampleToReturn.release());
        return true;
    }

};

TEST_F (PipelineManagerRunTest, no_samples_in_source) {
    EXPECT_CALL(mockSource, GetSample(_)).WillOnce(Return(false));

    //check that mock sink and mock source components are used according to expect_calls
    PipelineManager::Run();
}

TEST_F (PipelineManagerRunTest, no_Transform_1sample_in_source) {
    //only this one sample with trackid = 0
    MockSample &smpl = *new MockSample();
    MockGetSample mock_get(&smpl);

    EXPECT_CALL(smpl, GetTrackID()).WillRepeatedly(Return(0));
    EXPECT_CALL(mockStorage, empty(0)).WillOnce(Return(true));

    Sequence s1;
    EXPECT_CALL(mockSource, GetSample(_)).InSequence(s1).WillOnce(Invoke(&mock_get, &MockGetSample::GetSample));
    EXPECT_CALL(mockSource, GetSample(_)).InSequence(s1).WillOnce(Return(false));

    //check that mock sink and mock source components are used according to expect_calls
    PipelineManager::Run();
}

TEST_F (PipelineManagerRunTest, transform_plus_1_sample) {
    std::auto_ptr<MockTransform> mockTrans (new MockTransform());
    transvec.push_back(mockTrans.get());

    EXPECT_CALL(mockStorage, begin(0)).WillOnce(Return(transvec.begin()));
    EXPECT_CALL(mockStorage, end(0)).WillOnce(Return(transvec.end()));
    EXPECT_CALL(mockStorage, empty(0)).WillRepeatedly(Return(false));

    //this one source sample
    MockSample &smpl_source = *new MockSample();
    MockGetSample mock_get_source(&smpl_source);
    EXPECT_CALL(smpl_source, GetTrackID()).WillRepeatedly(Return(0));

    //this one trans sample
    MockSample &smpl_trans = *new MockSample();
    MockGetSample mock_get_trans(&smpl_trans);
    EXPECT_CALL(smpl_trans, GetTrackID()).WillRepeatedly(Return(0));

    //
    Sequence s1;
    EXPECT_CALL(mockSource, GetSample(_)).InSequence(s1).WillOnce(Invoke(&mock_get_source, &MockGetSample::GetSample));
    EXPECT_CALL(mockSource, GetSample(_)).InSequence(s1).WillOnce(Return(false));

    //transform
    EXPECT_CALL(*mockTrans, PutSample(_)).WillOnce(Invoke(this, &PipelineManagerRunTest::FPutSample));

    Sequence s2;
    EXPECT_CALL(*mockTrans, GetSample(_)).InSequence(s2).WillOnce(Invoke(&mock_get_trans, &MockGetSample::GetSample));
    EXPECT_CALL(*mockTrans, GetSample(_)).InSequence(s2).WillOnce(Return(false));


    //sink
    EXPECT_CALL(mockSink, PutSample(_)).WillOnce(Invoke(this, &PipelineManagerRunTest::FPutSample));

    //check that mock sink and mock source components are used according to expect_calls
    PipelineManager::Run();
}
//todo: add multitracks test


class PipelineManager_BuildTest: public ::testing::Test{

public:
    enum {
        evideo = 0,
        eaudio = 1
    };
    class RegisterTransformCallback{
        std::auto_ptr<ITransform > next;
        TransformConfigDesc desc;
    public:
        RegisterTransformCallback ()
            : desc(0, MFXAVParams(*new mfxAudioParam()) ) {
        }
        ITransform * GetTransform() {
            return next.get();
        }
        TransformConfigDesc & Description() {
            return desc;
        }
        virtual void RegisterTransform(const TransformConfigDesc &desc, std::auto_ptr<ITransform> & transform) {
            this->desc = desc;
            next = transform;
        }
    };

    RegisterTransformCallback adec, vdec, aenc, venc, vpp;

    MockPipelineFactory fac;
    PipelineManager mgr;
    MockSessionStorage &mock_storage;
    MockDataIO &mock_file_io;
    MockDataIO &mock_file_io_output;
    MockMFXVideoDECODE &mock_decode;
    MockTransform &mock_vtransform;
    MockTransform &mock_venctransform;
    MockTransform &mock_atransform;
    MockTransform &mock_aenctransform;
    MockTransform &mock_vpptransform;

    MockTransformStorage &mock_transform_storage;
    MockPipelineProfile &mock_profile;
    MockSplitterWrapper &mock_splitter_wrapper;
    MockCmdLineParser mock_parser;
    MockHandler mock_handler;
    Sequence svideo;
    Sequence saudio;
    Sequence scommon;

    mfxTrackInfo trackInfo0;
    mfxTrackInfo trackInfo1;

    mfxTrackInfo *trackInfos [1];
    MFXStreamParams sp;

    MFXSessionInfo vSession;
    MFXSessionInfo aSession;

    PipelineManager_BuildTest ()
        : mgr(fac)
        , mock_storage(*new MockSessionStorage())
        , mock_file_io(*new MockDataIO())
        , mock_file_io_output(*new MockDataIO())
        , mock_decode(*new MockMFXVideoDECODE())
        , mock_vtransform(*new MockTransform())
        , mock_atransform(*new MockTransform())
        , mock_transform_storage(*new MockTransformStorage())
        , mock_profile(*new MockPipelineProfile())
        , mock_splitter_wrapper(*new MockSplitterWrapper())
        , mock_venctransform(*new MockTransform())
        , mock_aenctransform(*new MockTransform())
        , mock_vpptransform(*new MockTransform())
        , trackInfo0()
        , trackInfo1() {
        trackInfos[0] = &trackInfo0;
        trackInfos[1] = &trackInfo1;
        sp.NumTracks = 0;
        sp.TrackInfo = (mfxTrackInfo**)trackInfos;
    }
    void InitMFXImpl(int bSw, int bAudioSW, int bD3d11) {
        if (bSw >=0 )
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_HW))).WillOnce(Return(1==bSw));
        if (bAudioSW >=0 )
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ASW))).WillOnce(Return(1==bAudioSW));
        if (bD3d11 >= 0)
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_D3D11))).WillOnce(Return(1==bD3d11));
    }
    void InitSplitter() {
        EXPECT_CALL(mock_handler, Exist()).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_handler, GetValue(0)).WillRepeatedly(Return(msdk_string(MSDK_STRING("filename.avi"))));
        EXPECT_CALL(mock_parser, at(msdk_string(OPTION_I))).WillRepeatedly(ReturnRef(mock_handler));
        //mocking file_io
        EXPECT_CALL(fac, CreateFileIO(_, _)).InSequence(scommon).WillOnce(Return(&mock_file_io));
        //mocking splitter
        EXPECT_CALL(fac, CreateSplitterWrapper(_)).WillOnce(Return(&mock_splitter_wrapper));
        EXPECT_CALL(mock_splitter_wrapper, GetInfo(_)).Times(1);
    }

    void InitSession() {
        EXPECT_CALL(fac, CreateSessionStorage(_, _)).WillOnce(DoAll(Invoke(this, &PipelineManager_BuildTest::operator ()), Return(&mock_storage)));
        EXPECT_CALL(fac, CreateTransformStorage()).WillOnce(Return(&mock_transform_storage) );
    }

    void operator () ( const MFXSessionInfo & v, const MFXSessionInfo &a) {
        vSession = v;
        aSession = a;
    }

    void InitDecoders(int audio,int atrack=0/*int bSw, int bAudioSW, int bD3d11*/) {
        if (audio) {
            EXPECT_CALL(mock_storage, GetAudioSessionForID(trackInfo0.SID)).WillRepeatedly(Return((MFXAudioSession*)NULL));
            //EXPECT_CALL(fac, CreateAudioDecoderTransform(_, _)).WillOnce(Return(&mock_atransform));
            EXPECT_CALL(mock_transform_storage, RegisterTransform(_,_)).InSequence(saudio).WillRepeatedly(Invoke(&adec, &RegisterTransformCallback::RegisterTransform));
            EXPECT_CALL(mock_profile, isAudioDecoderExist(atrack)).WillRepeatedly(Return(true));

        } else {
            EXPECT_CALL(mock_storage, GetVideoSessionForID(trackInfo0.SID)).WillRepeatedly(Return((MFXVideoSessionExt*)NULL));
            EXPECT_CALL(fac, CreateVideoDecoderTransform(_,_)).WillOnce(Return(&mock_vtransform));
            EXPECT_CALL(mock_transform_storage, RegisterTransform(_,_)).InSequence(svideo).WillOnce(Invoke(&vdec, &RegisterTransformCallback::RegisterTransform));;
            EXPECT_CALL(mock_profile, isDecoderExist()).WillRepeatedly(Return(true));
        }
    }

    void InitEncoders(int audio, int atrack=0/*int bSw, int bAudioSW, int bD3d11*/) {
        if (audio) {
            EXPECT_CALL(mock_profile, isAudioEncoderExist(atrack)).WillRepeatedly(Return(true));
            //EXPECT_CALL(fac, CreateAudioEncoderTransform(_,_)).WillOnce(Return(&mock_aenctransform));
            EXPECT_CALL(mock_transform_storage, RegisterTransform(_,_)).InSequence(saudio).WillRepeatedly(Invoke(&aenc, &RegisterTransformCallback::RegisterTransform));;

        } else {
            EXPECT_CALL(mock_profile, isEncoderExist()).WillRepeatedly(Return(true));
            EXPECT_CALL(fac, CreateVideoEncoderTransform(_,_)).WillOnce(Return(&mock_venctransform));
            EXPECT_CALL(mock_transform_storage, RegisterTransform(_,_)).InSequence(svideo).WillOnce(Invoke(&venc, &RegisterTransformCallback::RegisterTransform));;
        }
    }
    void InitVpp() {
        EXPECT_CALL(mock_profile, isVppExist()).WillRepeatedly(Return(true));
        EXPECT_CALL(fac, CreateVideoVPPTransform(_,_,_)).WillOnce(Return(&mock_vpptransform));
        EXPECT_CALL(mock_transform_storage, RegisterTransform(_,_)).InSequence(svideo).WillOnce(Invoke(&vpp, &RegisterTransformCallback::RegisterTransform));
    }

    void DisableAudio(int track) {
        EXPECT_CALL(mock_profile, isAudioDecoderExist(track)).WillRepeatedly(Return(false));
    }

    void DisableVideo() {
        EXPECT_CALL(mock_profile, isDecoderExist()).WillRepeatedly(Return(false));
    }

    void InitOutput() {
        EXPECT_CALL(mock_parser, at(msdk_string(OPTION_O))).WillRepeatedly(ReturnRef(mock_handler));
        //mocking file_io for output
        EXPECT_CALL(fac, CreateFileIO(_, _)).InSequence(scommon).WillOnce(Return(&mock_file_io_output));
    }

    MFXSessionInfo GetSessionForVideo() {
        return vSession;
    }
    MFXSessionInfo GetSessionForAudio() {
        return aSession;
    }
};

TEST_F (PipelineManager_BuildTest, decode_only_profile_reports_1_video_no_audio) {

    InitSplitter();
    InitDecoders(evideo);
    InitSession();
    DisableAudio(0);

    EXPECT_CALL(fac, CreatePipelineProfile(_, _)).WillOnce(Return((IPipelineProfile*)&mock_profile));

    //only vdecoder
    EXPECT_CALL(mock_profile, isVppExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isGenericPluginExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isEncoderExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isMultiplexerExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_LOOP))).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));

    InitOutput();

    VideoTrackInfoExt vTrackInfo;
    EXPECT_CALL(mock_profile, GetVideoTrackInfo()).WillRepeatedly(Return(vTrackInfo));

    EXPECT_NO_THROW(mgr.Build(mock_parser));
}

TEST_F (PipelineManager_BuildTest, decode_only_profile_reports_1_audio_no_video) {
    InitSplitter();
    InitDecoders(eaudio);
    InitSession();
    DisableVideo();

    EXPECT_CALL(fac, CreatePipelineProfile(_, _)).WillOnce(Return((IPipelineProfile*)&mock_profile));

    //only adecoder
    EXPECT_CALL(mock_profile, isDecoderExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isAudioEncoderExist(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isAudioDecoderExist(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isMultiplexerExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_LOOP))).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));
    InitOutput();

    AudioTrackInfoExt aTrackInfo;
    EXPECT_CALL(mock_profile, GetAudioTrackInfo(0)).WillRepeatedly(Return(aTrackInfo));

    EXPECT_NO_THROW(mgr.Build(mock_parser));
}

TEST_F (PipelineManager_BuildTest, decode_encode_video_only) {
    InitSplitter();
    InitDecoders(evideo);
    InitEncoders(evideo);
    InitSession();
    DisableAudio(0);

    EXPECT_CALL(fac, CreatePipelineProfile(_, _)).WillOnce(Return((IPipelineProfile*)&mock_profile));
    EXPECT_CALL(mock_profile, isMultiplexerExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isVppExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isGenericPluginExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_LOOP))).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));
    InitOutput();

    VideoTrackInfoExt vTrackInfo;
    EXPECT_CALL(mock_profile, GetVideoTrackInfo()).WillRepeatedly(Return(vTrackInfo));

    EXPECT_NO_THROW(mgr.Build(mock_parser));
}

TEST_F (PipelineManager_BuildTest, decode_vpp_encode_video_only) {
    InitSplitter();
    InitDecoders(evideo);
    InitVpp();
    InitEncoders(evideo);
    InitSession();
    DisableAudio(0);

    EXPECT_CALL(fac, CreatePipelineProfile(_, _)).WillOnce(Return((IPipelineProfile*)&mock_profile));
    EXPECT_CALL(mock_profile, isMultiplexerExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isGenericPluginExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_profile, isVPPPluginExist()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_LOOP))).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));

    InitOutput();

    VideoTrackInfoExt vTrackInfo;
    EXPECT_CALL(mock_profile, GetVideoTrackInfo()).WillRepeatedly(Return(vTrackInfo));

    EXPECT_NO_THROW(mgr.Build(mock_parser));
}

TEST_F (PipelineManager_BuildTest, decode_encode_1_audio_no_video) {
    InitSplitter();
    InitDecoders(eaudio);
    InitEncoders(eaudio);
    InitSession();
    DisableVideo();
    DisableAudio(1);

    EXPECT_CALL(fac, CreatePipelineProfile(_, _)).WillOnce(Return((IPipelineProfile*)&mock_profile));

    //only adecoder
    EXPECT_CALL(mock_profile, isMultiplexerExist()).WillRepeatedly(Return(false));

    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_LOOP))).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));

    InitOutput();

    AudioTrackInfoExt aTrackInfo;
    EXPECT_CALL(mock_profile, GetAudioTrackInfo(0)).WillRepeatedly(Return(aTrackInfo));

    EXPECT_NO_THROW(mgr.Build(mock_parser));
}
