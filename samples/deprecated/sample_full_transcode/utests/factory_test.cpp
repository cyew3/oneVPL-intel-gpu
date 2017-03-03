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
