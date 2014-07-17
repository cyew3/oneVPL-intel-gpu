//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "session_storage.h"
#include "pipeline_factory.h"
#include "mfxvideo++.h"
#include "mfxaudio++.h"


struct SessionStorageTest  : public ::testing::Test {
    std::auto_ptr<SessionStorage> storage;
    MockPipelineFactory factory;
    std::auto_ptr<MockMFXVideoSession> vSession;
    std::auto_ptr<MockMFXVideoSession> vSession2;
    std::auto_ptr<MockMFXAudioSession> aSession;
    mfxVersion verv;
    mfxVersion vera;
    SessionStorageTest()
        : vSession ( new MockMFXVideoSession(factory))
        , vSession2 ( new MockMFXVideoSession(factory))
        , aSession ( new MockMFXAudioSession()) {
        verv.Major = 1;
        verv.Minor = 4;
        vera.Major = 1;
        vera.Minor = 8;
        storage.reset(new SessionStorage(factory
            , MFXSessionInfo(MFX_IMPL_HARDWARE_ANY, verv)
            , MFXSessionInfo(MFX_IMPL_SOFTWARE | MFX_IMPL_AUDIO, vera)));
    }
    ~SessionStorageTest() {
        //delete vSession;
        //delete aSession;
    }
};

TEST_F(SessionStorageTest, add_video_session) {
    EXPECT_CALL(*vSession, Init(MFX_IMPL_HARDWARE_ANY, _)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(factory, CreateVideoSession()).WillOnce(Return(vSession.get()));
    EXPECT_CALL(*vSession, QueryVersion(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_EQ(vSession.get(), storage->GetVideoSessionForID(5));
    vSession.release();
};

TEST_F(SessionStorageTest, add_2video_sessions_same_pid) {
    EXPECT_CALL(factory, CreateVideoSession()).WillOnce(Return(vSession.get()));
    EXPECT_CALL(*vSession, Init(MFX_IMPL_HARDWARE_ANY, _)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(*vSession, QueryVersion(_)).WillRepeatedly(Return(MFX_ERR_NONE));
    EXPECT_EQ(vSession.get(), storage->GetVideoSessionForID(5));
    EXPECT_EQ(vSession.get(), storage->GetVideoSessionForID(5));

    vSession.release();
};

TEST_F(SessionStorageTest, add_2video_sessions_diff_pid) {
    Sequence s;
    EXPECT_CALL(factory, CreateVideoSession()).InSequence(s).WillOnce(Return(vSession.get()));
    EXPECT_CALL(factory, CreateVideoSession()).InSequence(s).WillOnce(Return(vSession2.get()));

    EXPECT_CALL(*vSession, Init(MFX_IMPL_HARDWARE_ANY, _)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(*vSession2, Init(MFX_IMPL_HARDWARE_ANY, _)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(*vSession, QueryVersion(_)).WillOnce(Return(MFX_ERR_NONE));
    EXPECT_CALL(*vSession2, QueryVersion(_)).WillOnce(Return(MFX_ERR_NONE));

    EXPECT_EQ(vSession.get(), storage->GetVideoSessionForID(5));
    EXPECT_EQ(vSession2.get(), storage->GetVideoSessionForID(6));

    vSession.release();
    vSession2.release();
};


TEST_F(SessionStorageTest, add_audio_session) {
    EXPECT_CALL(factory, CreateAudioSession()).WillOnce(Return(aSession.get()));
    EXPECT_CALL(*aSession, Init(MFX_IMPL_SOFTWARE | MFX_IMPL_AUDIO, _)).WillOnce(Return(MFX_ERR_NONE));

    EXPECT_EQ(aSession.get(), storage->GetAudioSessionForID(5));
    aSession.release();
};

TEST_F(SessionStorageTest, video_factory_init_error) {
    EXPECT_CALL(*vSession.get(), Init(_, _)).WillOnce(Return(MFX_ERR_UNSUPPORTED));
    EXPECT_CALL(factory, CreateVideoSession()).WillOnce(Return(vSession.get()));

    EXPECT_THROW(storage->GetVideoSessionForID(5), MFXVideoSessionInitError);
    vSession.release();
};

TEST_F(SessionStorageTest, audio_factory_init_error) {
    EXPECT_CALL(*aSession.get(), Init(_, _)).WillOnce(Return(MFX_ERR_UNSUPPORTED));
    EXPECT_CALL(factory, CreateAudioSession()).WillOnce(Return(aSession.get()));

    EXPECT_THROW(storage->GetAudioSessionForID(5), MFXAudioSessionInitError);
    aSession.release();
};


