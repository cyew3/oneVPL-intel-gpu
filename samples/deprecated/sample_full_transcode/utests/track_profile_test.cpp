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

using namespace ::testing;

class TrackProfileTest : public Test{
public:
     MFXStreamParams splInfo;
     mfxStreamParams & sp;
     MockCmdLineParser mock_parser;
     MockHandler   hdl;
     MockHandler   hdl1;
     MockHandler   hdl2;
     OutputFormat  extensions;
     OutputCodec codecs;
     mfxTrackInfo  *tracks[3];
     mfxTrackInfo  track_video ;
     mfxTrackInfo  track_audio ;
     mfxTrackInfo  track_vbi ;
     MFXSessionInfo s;
     TrackProfileTest() : sp(splInfo) {
         track_video.Type = MFX_TRACK_H264;
         track_audio.Type = MFX_TRACK_AAC;
         track_vbi.Type = MFX_TRACK_UNKNOWN;
         sp.TrackInfo = (mfxTrackInfo**)&tracks;
         sp.NumTracks = 0;
         track_audio.AudioParam.CodecId = MFX_CODEC_AAC;
         track_video.VideoParam.CodecId = MFX_CODEC_AVC;
     }
};
class AudioTrackProfileTest : public TrackProfileTest
{
public:
    AudioTrackProfileTest() {
        tracks[0] = &track_audio;
        sp.NumTracks = 1;
    }

};
class VideoTrackProfileTest: public TrackProfileTest{
public:
    VideoTrackProfileTest() {
        tracks[0] = &track_video;
        sp.NumTracks = 1;
    }
};

TEST_F(AudioTrackProfileTest, take_codec_id_from_spl_params) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp4", hdl1);

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AAC, profile.GetTrackInfo().Decode.mfx.CodecId);
}

TEST_F(AudioTrackProfileTest, take_encode_sample_rate_from_spl_params) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp4", hdl1);
    track_audio.AudioParam.SampleFrequency = 48000;

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(track_audio.AudioParam.SampleFrequency, profile.GetTrackInfo().Encode.mfx.SampleFrequency);
}

TEST_F(AudioTrackProfileTest, set_header_type_to_adts_for_AAC) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp4", hdl1);

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs,s, 0);
    EXPECT_EQ((mfxU16)MFX_AUDIO_AAC_ADTS, profile.GetTrackInfo().Encode.mfx.OutputFormat);
}

TEST_F(AudioTrackProfileTest, set_LC_profile_to_hw_for_AAC) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp4", hdl1);

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs,s, 0);
    EXPECT_EQ((mfxU16)MFX_PROFILE_AAC_LC, profile.GetTrackInfo().Encode.mfx.CodecProfile);
}

TEST_F(AudioTrackProfileTest, leave_header_type_default_for_mp3) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp4", hdl1);

    track_audio.Type = MFX_TRACK_ANY_AUDIO;
    track_audio.AudioParam.CodecId = MFX_CODEC_MP3;

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs,s, 0);
    EXPECT_EQ((mfxU16)MFX_AUDIO_AAC_ADTS, profile.GetTrackInfo().Encode.mfx.OutputFormat);
}


TEST_F(AudioTrackProfileTest, parse_bitrate) {
    DECL_OPTION_IN_PARSER(OPTION_AB, "100", hdl);
    DECL_OPTION_IN_PARSER(OPTION_ACODEC, "copy", hdl1);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs,s, 0);

    EXPECT_EQ(100*1000, profile.GetTrackInfo().Encode.mfx.Bitrate);
}

TEST_F(AudioTrackProfileTest, audio_ACC_for_mp4_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp4", hdl1);

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AAC, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(AudioTrackProfileTest, audio_mp3_for_mp3_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp3", hdl1);

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AAC, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(AudioTrackProfileTest, audio_ACC_for_aac_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.aac", hdl1);

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AAC, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(AudioTrackProfileTest, audio_mp3_for_mpg_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.m2ts", hdl1);

    AudioTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AAC, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(VideoTrackProfileTest, unsupported_audio_codec_exception_for_h264_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_AB);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.h264", hdl1);

    EXPECT_THROW(AudioTrackProfile (splInfo, mock_parser, extensions, codecs, s, 0), UnsupportedAudioCodecFromExtension);
}

TEST_F(VideoTrackProfileTest, audio_bitrate_big) {
    DECL_OPTION_IN_PARSER(OPTION_AB, "100000000", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);

    DECL_OPTION_IN_PARSER(OPTION_O, "file.aac", hdl1);

    EXPECT_THROW(AudioTrackProfile (splInfo, mock_parser, extensions, codecs, s, 0), AudioBitrateError);
}

TEST_F(VideoTrackProfileTest, audio_bitrate_fine) {
    DECL_OPTION_IN_PARSER(OPTION_AB, "1000000", hdl);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.aac", hdl1);

    EXPECT_NO_THROW(AudioTrackProfile (splInfo, mock_parser, extensions, codecs, s, 0));
}

//////////////////////////////////////////////////////////////////////////

TEST_F(VideoTrackProfileTest, take_codec_id_from_spl_params) {
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);
    DECL_NO_OPTION_IN_PARSER(OPTION_VCODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_D3D11);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);

    DECL_OPTION_IN_PARSER(OPTION_O, "file.h264", hdl1);

    VideoTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AVC, profile.GetTrackInfo().Decode.mfx.CodecId);
}

TEST_F(VideoTrackProfileTest, parse_bitrate) {
    DECL_OPTION_IN_PARSER(OPTION_VB, "1001", hdl);
    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "copy", hdl1);
    DECL_OPTION_IN_PARSER(OPTION_D3D11, "", hdl2);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);

    VideoTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(1001, profile.GetTrackInfo().Encode.mfx.TargetKbps);
}

TEST_F(VideoTrackProfileTest, parse_bitrate_to_BRCParamMultiplier) {
    DECL_OPTION_IN_PARSER(OPTION_VB, "100000", hdl);
    DECL_OPTION_IN_PARSER(OPTION_VCODEC, "copy", hdl1);
    DECL_OPTION_IN_PARSER(OPTION_D3D11, "", hdl2);
    DECL_NO_OPTION_IN_PARSER(OPTION_ACODEC_COPY);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);
    VideoTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(50000, profile.GetTrackInfo().Encode.mfx.TargetKbps);
    EXPECT_EQ(2, profile.GetTrackInfo().Encode.mfx.BRCParamMultiplier);
}

TEST_F(VideoTrackProfileTest, mpeg2_for_mpg_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);
    DECL_NO_OPTION_IN_PARSER(OPTION_VCODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_D3D11);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.m2ts", hdl1);

    VideoTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_MPEG2, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(VideoTrackProfileTest, avc_for_mp4_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);
    DECL_NO_OPTION_IN_PARSER(OPTION_VCODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_D3D11);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp4", hdl1);

    VideoTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AVC, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(VideoTrackProfileTest, mpeg2_for_m2v_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);
    DECL_NO_OPTION_IN_PARSER(OPTION_VCODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_D3D11);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.m2v", hdl1);

    VideoTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_MPEG2, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(VideoTrackProfileTest, avc_for_h264_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);
    DECL_NO_OPTION_IN_PARSER(OPTION_VCODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_D3D11);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.h264", hdl1);

    VideoTrackProfile profile(splInfo, mock_parser, extensions, codecs, s, 0);

    EXPECT_EQ(MFX_CODEC_AVC, profile.GetTrackInfo().Encode.mfx.CodecId);
}

TEST_F(VideoTrackProfileTest, unsupported_video_codec_exception_for_mp3_extension) {
    DECL_NO_OPTION_IN_PARSER(OPTION_VB);
    DECL_NO_OPTION_IN_PARSER(OPTION_VCODEC);
    DECL_NO_OPTION_IN_PARSER(OPTION_D3D11);
    DECL_NO_OPTION_IN_PARSER(OPTION_HW);
    DECL_OPTION_IN_PARSER(OPTION_O, "file.mp3", hdl1);

    EXPECT_THROW(VideoTrackProfile (splInfo, mock_parser, extensions, codecs, s, 0), UnsupportedVideoCodecFromExtension);
}