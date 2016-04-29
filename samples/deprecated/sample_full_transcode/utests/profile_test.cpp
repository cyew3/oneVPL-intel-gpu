//kWillRepeatedly
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

#include "pipeline_profile.h"
#include "exceptions.h"

struct PipelineProfileTest : public ::testing::Test {
    std::auto_ptr<IPipelineProfile> profile;
    MockCmdLineParser mock_parser;
    MFXStreamParams splParams;
    mfxStreamParams & sp;
    mfxTrackInfo  track_video ;
    mfxTrackInfo  track_audio ;
    mfxTrackInfo  track_vbi ;
    mfxTrackInfo  *tracks[10];
    MockHandler   hdl;
    MockHandler   hdlo;

    PipelineProfileTest()
         : sp(splParams)
         , track_video()
         , track_audio()
         , track_vbi() {
          track_video.Type = MFX_TRACK_H264;
          track_audio.Type = MFX_TRACK_AAC;
          track_vbi.Type = MFX_TRACK_UNKNOWN;
          sp.TrackInfo = (mfxTrackInfo**)&tracks;
          sp.NumTracks = 0;
          DECL_NO_OPTION_IN_PARSER(OPTION_TRACELEVEL);
     }

    void InitMFXImpl(int bSw, int bAudioSW, int bD3d11) {
        if (bSw >=0 )
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_HW))).WillRepeatedly(Return(1==bSw));
        if (bAudioSW >=0 ) {
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ASW))).WillRepeatedly(Return(1==bAudioSW));
        }
        else{
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ASW))).WillRepeatedly(Return(1!=bAudioSW));
        }
        if (bD3d11 >= 0)
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_D3D11))).WillRepeatedly(Return(1==bD3d11));
    }

    void InitEncoders(int hasAudioSetting, int HasVideoSettings) {
        if (HasVideoSettings>=0) {
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VB))).WillRepeatedly(Return(HasVideoSettings>0));
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VCODEC))).WillRepeatedly(Return(HasVideoSettings>0));
        }

        if (hasAudioSetting >=0) {
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_AB))).WillRepeatedly(Return(hasAudioSetting>0));
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC))).WillRepeatedly(Return(hasAudioSetting>0));
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(hasAudioSetting>0));
        }

    }
    void InitProfile(bool bParsePlugins = false
                    , bool bParseMFXImpl = false
                    , int optionOPresent = 1
                    , bool bParseVppOptions = true
                    , msdk_string filename = MSDK_STRING("file111111.mp4")
                    , int optionOIsPresentCalled = 1) {

        if (!bParsePlugins) {
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_PLG))).WillOnce(Return(false));
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VDECPLG))).WillOnce(Return(false));
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VENCPLG))).WillOnce(Return(false));
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VPPPLG_GUID))).WillOnce(Return(false));
        }
        if (!bParseMFXImpl) {
            InitMFXImpl(0,0,0);
        }
        if (optionOPresent >=0 ) {
            if (!optionOPresent ) {
                EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_O))).WillRepeatedly(Return(false));
            } else {
                if (optionOIsPresentCalled) {
                    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_O))).WillRepeatedly(Return(true));
                }
                EXPECT_CALL(mock_parser, at(msdk_string(OPTION_O))).WillRepeatedly(ReturnRef(hdlo));
                EXPECT_CALL(hdlo, Exist()).WillRepeatedly(Return(true));
                EXPECT_CALL(hdlo, GetValue(0)).WillRepeatedly(Return(filename.c_str()));
            }
        }
        if (bParseVppOptions) {
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_W))).WillOnce(Return(false));
            EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_H))).WillOnce(Return(false));
        }

        OutputFormat f;
        OutputCodec c;
        profile.reset( new PipelineProfile(splParams, mock_parser, f, c));
    }
};

TEST_F(PipelineProfileTest, no_output_option_specified) {
    EXPECT_THROW(InitProfile(0,1, false, false), PipelineProfileNoOuputError);
}

TEST_F(PipelineProfileTest, transcode_only_video_if_no_videos_in_input) {

    DECL_OPTION_IN_PARSER(OPTION_ACODEC, "copy", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);

    tracks[0] = &track_audio;
    sp.NumTracks = 1;

    EXPECT_THROW(InitProfile(0, 0,  true, false, MSDK_STRING("a.h264")), PipelineProfileOnlyVideoTranscodeError);
}

TEST_F(PipelineProfileTest, transcode_only_audio_if_no_audios_in_input) {

    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "copy", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);

    tracks[0] = &track_video;
    sp.NumTracks = 1;

    EXPECT_THROW(InitProfile(0, 0,  true, false, MSDK_STRING("a.mp3")), PipelineProfileOnlyAudioTranscodeError);
}

TEST_F(PipelineProfileTest, vcodec_copy_no_encoder) {

    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "copy", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    track_video.VideoParam.CodecId = MFX_CODEC_AVC;


    InitProfile();

    EXPECT_EQ(false, profile->isEncoderExist());
    EXPECT_EQ(MFX_CODEC_AVC, profile->GetVideoTrackInfo().Encode.mfx.CodecId);
}

TEST_F(PipelineProfileTest, DISABLED_vcodec_mpeg2_original_was_avc) {

    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "mpeg2", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    track_video.VideoParam.CodecId = MFX_CODEC_AVC;

    InitProfile();

    EXPECT_EQ(true, profile->isEncoderExist());
    EXPECT_EQ(MFX_CODEC_MPEG2, profile->GetVideoTrackInfo().Encode.mfx.CodecId);
}


TEST_F(PipelineProfileTest, vcodec_invalid_codec) {

    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "unknown", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    EXPECT_THROW(InitProfile(false,false,1,false), UnsupportedVideoCodecError);
}

TEST_F(PipelineProfileTest, vcodec_h264_and_no_extension) {
    //InitEncoders(-1, 0);

    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "h264", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    //
    EXPECT_NO_THROW(InitProfile(0,0,1,true,MSDK_CHAR("12345")));
    EXPECT_EQ(true, profile->isDecoderExist());
}

TEST_F(PipelineProfileTest, vcodec_not_found_and_no_extension) {
    InitEncoders(-1, 0);

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    //
    EXPECT_THROW(InitProfile(0,0,1,false,MSDK_CHAR("12345")), NothingToTranscode);
}

TEST_F(PipelineProfileTest, vcodec_not_found_mp3_extension) {
    InitEncoders(-1, 0);

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    EXPECT_THROW(InitProfile(0,0,1,false,MSDK_CHAR("12345.mp3")), PipelineProfileOnlyAudioTranscodeError);
}

TEST_F(PipelineProfileTest, no_plugins) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);

    InitProfile();
    EXPECT_EQ(false, profile->isDecoderPluginExist());
    EXPECT_EQ(false, profile->isEncoderPluginExist());
    EXPECT_EQ(false, profile->isGenericPluginExist());
    EXPECT_EQ(false, profile->isVPPPluginExist());
}

TEST_F(PipelineProfileTest, dec_plugin) {
    InitEncoders(-1, 0);
    tracks[0] = &track_video;
    sp.NumTracks = 1;

    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VENCPLG))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_PLG))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VDECPLG))).WillOnce(Return(true));

    EXPECT_CALL(hdl, Exist()).WillOnce(Return(true));
    EXPECT_CALL(hdl, GetValue(0)).WillOnce(Return(MSDK_CHAR("decPlugin1")));
    EXPECT_CALL(hdl, GetValue(1)).WillOnce(Return(MSDK_CHAR("")));
    EXPECT_CALL(mock_parser, at(msdk_string(OPTION_VDECPLG))).WillOnce(ReturnRef(hdl));

    InitProfile(1);
    EXPECT_EQ(true, profile->isDecoderPluginExist());
}

TEST_F(PipelineProfileTest, enc_plugin) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);

    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_PLG))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VDECPLG))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VENCPLG))).WillOnce(Return(true));

    EXPECT_CALL(hdl, Exist()).WillOnce(Return(true));
    EXPECT_CALL(hdl, GetValue(0)).WillOnce(Return(MSDK_CHAR("decPlugin1")));
    EXPECT_CALL(hdl, GetValue(1)).WillOnce(Return(MSDK_CHAR("")));
    EXPECT_CALL(mock_parser, at(msdk_string(OPTION_VENCPLG))).WillOnce(ReturnRef(hdl));

    InitProfile(1);
    EXPECT_EQ(true, profile->isEncoderPluginExist());
}

TEST_F(PipelineProfileTest, vpp_plugin) {
    InitEncoders(-1, 0);
    tracks[0] = &track_video;
    sp.NumTracks = 1;

    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VDECPLG))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_VENCPLG))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_PLG))).WillOnce(Return(true));

    EXPECT_CALL(hdl, GetValue(0)).WillOnce(Return(MSDK_CHAR("plg1")));
    EXPECT_CALL(hdl, GetValue(1)).WillOnce(Return(MSDK_CHAR("")));

    EXPECT_CALL(hdl, Exist()).WillOnce(Return(true));
    EXPECT_CALL(mock_parser, at(msdk_string(OPTION_PLG))).WillRepeatedly(ReturnRef(hdl));

    InitProfile(1);
    EXPECT_EQ(true, profile->isGenericPluginExist());
}


TEST_F(PipelineProfileTest, no_adec_dueto_h264_extension) {
    tracks[0] = &track_video;
    tracks[1] = &track_audio;
    sp.NumTracks = 2;

    InitEncoders(0, 0);
    InitProfile(0,0,1,true,MSDK_STRING("1.h264"));

    EXPECT_EQ(false, profile->isAudioDecoderExist(0));
}

TEST_F(PipelineProfileTest, no_vdec_dueto_mp3_extension) {
    tracks[0] = &track_video;
    tracks[1] = &track_audio;
    sp.NumTracks = 2;
    InitEncoders(0, 0);
    InitProfile(0,0,1,true,MSDK_STRING("1.mp3"));

    EXPECT_EQ(false, profile->isDecoderExist());
}


TEST_F(PipelineProfileTest, no_vdec_since_no_video) {
    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    InitEncoders(0, 0);
    InitProfile();
    EXPECT_EQ(false, profile->isDecoderExist());
}

TEST_F(PipelineProfileTest, vdec_since_1_video_track) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);
    InitProfile();

    EXPECT_EQ(true, profile->isDecoderExist());
}

TEST_F(PipelineProfileTest, no_adec_since_no_audio) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(0, 0);
    InitProfile();
    EXPECT_EQ(false, profile->isAudioDecoderExist(0));
}

TEST_F(PipelineProfileTest, adec_since_1_audio_track) {
    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    InitEncoders(0, -1);
    InitProfile();

    EXPECT_EQ(true, profile->isAudioDecoderExist(0));
    EXPECT_EQ(false, profile->isAudioDecoderExist(1));
}

TEST_F(PipelineProfileTest, 2adecs_since_2_audio_tracks) {
    tracks[0] = &track_audio;
    tracks[1] = &track_audio;
    sp.NumTracks = 2;
    InitEncoders(0, -1);
    InitProfile();

    EXPECT_EQ(true, profile->isAudioDecoderExist(0));
    EXPECT_EQ(true, profile->isAudioDecoderExist(1));
    EXPECT_EQ(false, profile->isAudioDecoderExist(2));
}

TEST_F(PipelineProfileTest, v_enc_enabled) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;

    InitEncoders(-1, 0);
    InitProfile();

    EXPECT_EQ(true, profile->isEncoderExist());
}

TEST_F(PipelineProfileTest, acodec_not_found_and_no_extension) {
    InitEncoders(0, -1);

    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    track_audio.AudioParam.CodecId = MFX_CODEC_MP3;
    EXPECT_THROW(InitProfile(0,0,1,true,MSDK_CHAR("12345")), NothingToTranscode);
}

TEST_F(PipelineProfileTest, vcodec_not_found_h264_extension) {
    InitEncoders(0, -1);

    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    track_audio.AudioParam.CodecId = MFX_CODEC_MP3;
    EXPECT_THROW(InitProfile(0,0,1,true,MSDK_CHAR("12345.h264")), PipelineProfileOnlyVideoTranscodeError);
}

TEST_F(PipelineProfileTest, acodec_copy_no_encoder) {
    EXPECT_CALL(hdl, GetValue(0)).WillRepeatedly(Return(MSDK_CHAR("copy")));
    EXPECT_CALL(hdl, Exist()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_parser, at(msdk_string(OPTION_ACODEC))).WillRepeatedly(ReturnRef(hdl));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC))).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_AB))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));
    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    track_audio.AudioParam.CodecId = MFX_CODEC_MP3;
    InitProfile();
    mfxU32 u = profile->GetAudioTrackInfo(0).Encode.mfx.CodecId;
    EXPECT_EQ(false, profile->isAudioEncoderExist(0));
    EXPECT_EQ(MFX_CODEC_AAC, profile->GetAudioTrackInfo(0).Encode.mfx.CodecId);
}

TEST_F(PipelineProfileTest, acodec_aac_was_mp3) {
    EXPECT_CALL(hdl, GetValue(0)).WillRepeatedly(Return(MSDK_CHAR("aac")));
    EXPECT_CALL(hdl, Exist()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_parser, at(msdk_string(OPTION_ACODEC))).WillRepeatedly(ReturnRef(hdl));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC))).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_AB))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));

    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    track_audio.AudioParam.CodecId = MFX_CODEC_MP3;
    InitProfile();

    EXPECT_EQ(MFX_CODEC_AAC, profile->GetAudioTrackInfo(0).Encode.mfx.CodecId);
}

TEST_F(PipelineProfileTest, acodec_invalid_codec) {
    EXPECT_CALL(hdl, GetValue(0)).WillRepeatedly(Return(MSDK_CHAR("h264")));
    EXPECT_CALL(hdl, Exist()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_parser, at(msdk_string(OPTION_ACODEC))).WillRepeatedly(ReturnRef(hdl));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC))).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_AB))).WillOnce(Return(false));

    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    EXPECT_THROW(InitProfile(false,false), UnsupportedAudioCodecError);
}

TEST_F(PipelineProfileTest, a_enc_enabled) {

    tracks[0] = &track_audio;
    sp.NumTracks = 1;
    InitEncoders(0, 1);
    InitProfile();

    EXPECT_EQ(true, profile->isAudioEncoderExist(0));
}


TEST_F(PipelineProfileTest, no_mux_dueto_extension) {
    //EXPECT_CALL(hdl, GetValue(0)).WillOnce(Return(MSDK_CHAR("filename.h264")));
    //EXPECT_CALL(hdl, Exist()).WillOnce(Return(true));
    //EXPECT_CALL(mock_parser, at(msdk_string(OPTION_O))).WillOnce(ReturnRef(hdl));
    //EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_O))).WillOnce(Return(true));

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);
    InitProfile(0,0,true,true,MSDK_CHAR("filename.h264"));

    EXPECT_EQ(false, profile->isMultiplexerExist());
}

TEST_F(PipelineProfileTest, mp4_mux_selected) {
    //EXPECT_CALL(hdl, GetValue(0)).WillOnce(Return(MSDK_CHAR("filename.mp4")));
    //EXPECT_CALL(hdl, Exist()).WillOnce(Return(true));
    //EXPECT_CALL(mock_parser, at(msdk_string(OPTION_O))).WillOnce(ReturnRef(hdl));
    //EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_O))).WillOnce(Return(true));

    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);
    InitProfile(0,0,true,true,MSDK_CHAR("filename.mp4"));

    EXPECT_EQ(true, profile->isMultiplexerExist());
}

TEST_F(PipelineProfileTest, no_vpp) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);

    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_W))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_H))).WillOnce(Return(false));

    InitProfile();

    EXPECT_EQ(false, profile->isVppExist());
}

TEST_F(PipelineProfileTest, vpp_scale_w) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);

    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_W))).WillOnce(Return(true));

    InitProfile();

    EXPECT_EQ(true, profile->isVppExist());
}

TEST_F(PipelineProfileTest, vpp_scale_h) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;
    InitEncoders(-1, 0);

    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_W))).WillOnce(Return(false));
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_H))).WillOnce(Return(true));

    InitProfile();

    EXPECT_EQ(true, profile->isVppExist());
}

TEST_F(PipelineProfileTest, sw_and_d3d11_throws_an_error) {
    InitMFXImpl(1, -1, 1);
    //EXPECT_THROW(InitProfile(0, 1), CommandlineConsistencyError);
    EXPECT_ANY_THROW(InitProfile(0, 1));
}

#if 0
TEST_F(PipelineProfileTest, video_hw_d3d11) {

    tracks[0] = &track_video;
    sp.NumTracks = 1;

    InitEncoders(-1, 0);
    InitMFXImpl(0,0,1);
    InitProfile(0, 1);

    EXPECT_EQ( MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11, profile->GetVideoTrackInfo().Session.IMPL());
}
#endif

TEST_F (PipelineProfileTest, video_hw) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;

    InitEncoders(-1, 0);
    InitMFXImpl(1,0,0);
    InitProfile(0, 1);
    EXPECT_EQ( (mfxIMPL)MFX_IMPL_HARDWARE, (int)profile->GetVideoTrackInfo().Session.IMPL());
}

#if 0
TEST_F (PipelineProfileTest, video_auto) {
    tracks[0] = &track_video;
    sp.NumTracks = 1;

    InitEncoders(-1, 0);
    InitMFXImpl(0,0,0);
    InitProfile(0, 1);

    EXPECT_EQ( (mfxIMPL)MFX_IMPL_AUTO, profile->GetVideoTrackInfo().Session.IMPL());
}
#endif

TEST_F (PipelineProfileTest, DISABLED_audio_default) {
    tracks[0] = &track_audio;
    sp.NumTracks = 1;

    InitEncoders(0, -1);
    InitMFXImpl(0,0,0);
    InitProfile(0, 1);

    EXPECT_EQ( (mfxIMPL)MFX_IMPL_SOFTWARE | MFX_IMPL_AUDIO, profile->GetAudioTrackInfo(0).Session.IMPL());
}

TEST_F (PipelineProfileTest, audio_sw) {
    tracks[0] = &track_audio;
    sp.NumTracks = 1;

    InitEncoders(0, -1);
    InitMFXImpl(0,1,0);
    InitProfile(0, 1);

    EXPECT_EQ( (mfxIMPL)MFX_IMPL_SOFTWARE | MFX_IMPL_AUDIO, profile->GetAudioTrackInfo(0).Session.IMPL());
}

TEST_F (PipelineProfileTest, audio_version_min_1_8) {
    tracks[0] = &track_audio;
    sp.NumTracks = 1;

    InitEncoders(0, -1);
    InitMFXImpl(0,1,0);
    InitProfile(0, 1);

    //audio version is 1.8 minimum
    EXPECT_EQ( 8u, profile->GetAudioTrackInfo(0).Session.Version().Minor);
    EXPECT_EQ( 1u, profile->GetAudioTrackInfo(0).Session.Version().Major);
}

TEST_F (PipelineProfileTest, splitter_init_with_unsupported_tracks_nothing_to_transcode) {
    tracks[0] = &track_vbi;
    sp.NumTracks = 1;
    InitEncoders(0, 0);
    InitMFXImpl(0,0,0);
    EXPECT_THROW(InitProfile(0, 1), NothingToTranscode);
}

TEST_F (PipelineProfileTest, splitter_init_with_unsupported_tracks_no_exception) {
    tracks[0] = &track_vbi;
    tracks[1] = &track_audio;
    sp.NumTracks = 2;
    InitMFXImpl(0,0,0);
    InitEncoders(0, 0);
    EXPECT_NO_THROW(InitProfile(0, 1));
    EXPECT_TRUE(profile->isAudioDecoderExist(0));
}

TEST_F (PipelineProfileTest, multi_tracks_mapping) {
    tracks[0] = &track_vbi;
    tracks[1] = &track_vbi;
    tracks[2] = &track_audio;
    tracks[3] = &track_video;
    sp.NumTracks = 4;
    InitMFXImpl(0,0,0);
    InitEncoders(0, 0);
    EXPECT_NO_THROW(InitProfile(0, 1));
    MFXStreamParamsExt extparam = profile->GetMuxerInfo();
    EXPECT_EQ(0, extparam.TrackMapping()[2]);
    EXPECT_EQ(1, extparam.TrackMapping()[3]);
}

TEST_F (PipelineProfileTest, tracks_mapping_only_video_output) {
    tracks[0] = &track_vbi;
    tracks[1] = &track_vbi;
    tracks[2] = &track_audio;
    tracks[3] = &track_video;
    sp.NumTracks = 4;
    InitMFXImpl(0,0,0);
    InitEncoders(0, 0);
    EXPECT_NO_THROW(InitProfile(0, 1, 1, true, MSDK_STRING("1.h264")));
    MFXStreamParamsExt extparam = profile->GetMuxerInfo();
    EXPECT_EQ(0, extparam.TrackMapping()[3]);
}

TEST_F (PipelineProfileTest, tracks_mapping_only_audio_output) {
    tracks[0] = &track_vbi;
    tracks[1] = &track_vbi;
    tracks[2] = &track_audio;
    tracks[3] = &track_video;
    sp.NumTracks = 4;
    InitMFXImpl(0,0,0);
    InitEncoders(0, 0);
    EXPECT_NO_THROW(InitProfile(0, 1, 1, true, MSDK_STRING("1.aac")));
    MFXStreamParamsExt extparam = profile->GetMuxerInfo();
    EXPECT_EQ(0, extparam.TrackMapping()[2]);
}

TEST_F (PipelineProfileTest, tracks_mapping_only_video_output_format_not_defined_from_extension) {
    tracks[0] = &track_vbi;
    tracks[1] = &track_vbi;
    tracks[2] = &track_video;
    tracks[3] = &track_video;

    sp.NumTracks = 4;
    InitMFXImpl(0,0,0);

    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "copy", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);

    EXPECT_NO_THROW(InitProfile(0, 1, 1, true, MSDK_STRING("whataformat")));
    MFXStreamParamsExt extparam = profile->GetMuxerInfo();
    EXPECT_EQ(0, extparam.TrackMapping()[3]);
}

TEST_F (PipelineProfileTest, tracks_mapping_only_audio_output_format_not_defined_from_extension) {
    tracks[0] = &track_vbi;
    tracks[1] = &track_vbi;
    tracks[2] = &track_audio;
    tracks[3] = &track_audio;
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(OPTION_ACODEC_COPY))).WillRepeatedly(Return(false));
    sp.NumTracks = 4;
    InitMFXImpl(0,0,0);
    DECL_OPTION_IN_PARSER(OPTION_ACODEC, "copy", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);

    EXPECT_NO_THROW(InitProfile(0, 1, 1, true, MSDK_STRING("whataformat")));
    MFXStreamParamsExt extparam = profile->GetMuxerInfo();
    EXPECT_EQ(0, extparam.TrackMapping()[2]);
}
