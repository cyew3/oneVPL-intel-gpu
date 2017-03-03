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

    //EXPECT_THROW(storage->GetAudioSessionForID(5), MFXAudioSessionInitError);
    EXPECT_EQ(0, storage->GetAudioSessionForID(5));
    aSession.release();
};


