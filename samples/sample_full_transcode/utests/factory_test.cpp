//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"


#include "pipeline_factory.h"
#include "cmdline_options.h"

#include "fileio.h"
#include "stdio.h"
struct PipelineFactoryTest  : public ::testing::Test {
    PipelineFactory factory;

    template <class T> void VerifyCreation(T * obj) {
        EXPECT_NE((T*)0, obj);
        delete obj;
    }
};

TEST_F(PipelineFactoryTest, audio_session) {
    VerifyCreation(factory.CreateAudioSession());
}

TEST_F(PipelineFactoryTest, video_session) {
    VerifyCreation(factory.CreateVideoSession());
}


TEST_F(PipelineFactoryTest, video_base) {
    std::auto_ptr<MFXVideoSessionExt> session (factory.CreateVideoSession());

    VerifyCreation(factory.CreateVideoDecoder(*session));
    VerifyCreation(factory.CreateVideoEncoder(*session));
    VerifyCreation(factory.CreateVideoVPP(*session));
}

TEST_F(PipelineFactoryTest, audio_decoder) {
    std::auto_ptr<MFXAudioSession> session(factory.CreateAudioSession());
    VerifyCreation(factory.CreateAudioDecoder(*session));
}

TEST_F(PipelineFactoryTest, audio_encoder) {
        std::auto_ptr<MFXAudioSession> session( factory.CreateAudioSession());
    VerifyCreation(factory.CreateAudioEncoder(*session));
}


TEST_F(PipelineFactoryTest, allocator_create) {
    VerifyCreation(factory.CreateFrameAllocator(ALLOC_IMPL_SYSTEM_MEMORY));
#if defined(_WIN32) || defined(_WIN64)
    VerifyCreation(factory.CreateFrameAllocator(ALLOC_IMPL_D3D9_MEMORY));
    VerifyCreation(factory.CreateFrameAllocator(ALLOC_IMPL_D3D11_MEMORY));
#endif
}

TEST_F(PipelineFactoryTest, splitter_create) {
    VerifyCreation(factory.CreateSplitter());
}

TEST_F(PipelineFactoryTest, muxer_create) {
    VerifyCreation(factory.CreateMuxer());
}

TEST_F(PipelineFactoryTest, fileio_create) {
    VerifyCreation(factory.CreateFileIO(MSDK_STRING("FileIO.tmp"),  MSDK_STRING("wb")));
    remove(MSDK_STRING("FileIO.tmp"));
}

TEST_F(PipelineFactoryTest, session_storage) {
    VerifyCreation(factory.CreateSessionStorage(MFXSessionInfo(), MFXSessionInfo()));
}

#if 0
TEST_F(PipelineFactoryTest, cmdlineparser_create) {
    PipelineFactory factory;
    std::auto_ptr<CmdLineParser> parser (factory.CreateCmdLineParser());
    EXPECT_EQ(true, parser->Parse(MSDK_STRING("-i c:\\my cool videos\\my video.avi -o c:\\heap\\my cool video.mp4 -async 4 -h 1080 -w 1920 -sw -d3d11 -acodec mp3 -vcodec jpeg")));

    EXPECT_EQ(1920, parser->at(OPTION_W).as<int>());
    EXPECT_EQ(1080, parser->at(OPTION_H).as<int>());
    EXPECT_EQ(4, parser->at(OPTION_ADEPTH).as<int>());
    EXPECT_STREQ("c:\\my cool videos\\my video.avi", utest_cvt_msdk2string(parser->at(OPTION_I).as<msdk_string>()).c_str());
    EXPECT_STREQ("c:\\heap\\my cool video.mp4", utest_cvt_msdk2string(parser->at(OPTION_O).as<msdk_string>()).c_str());
    EXPECT_STREQ("jpeg", utest_cvt_msdk2string(parser->at(OPTION_VCODEC).as<msdk_string>()).c_str());
    EXPECT_STREQ("mp3", utest_cvt_msdk2string(parser->at(OPTION_ACODEC).as<msdk_string>()).c_str());
    EXPECT_EQ(true, parser->at(OPTION_SW).Exist());

    EXPECT_EQ(true, (*parser)[OPTION_D3D11].Exist());
}
#endif
