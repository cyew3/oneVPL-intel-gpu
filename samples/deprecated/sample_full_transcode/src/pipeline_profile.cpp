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

#include "mfx_samples_config.h"

#include "pipeline_profile.h"
#include <iostream>
#include "sample_defs.h"
#include "exceptions.h"
#include "trace_levels.h"

PipelineProfile::PipelineProfile(const MFXStreamParams &splInfo, CmdLineParser & parser, OutputFormat & extensions, OutputCodec & codecs)
    : m_splInfo(splInfo)
    , m_parser(parser)
    , m_bIsMuxer()
    , m_nAudioTracks()
    , m_nVideoTracks()
    , m_bAudioTracksEnabled()
    , m_bVideoTracksEnabled()
    , m_extensions (extensions)
    , m_codecs (codecs)
{
    ParseLogLevel();
    ParsePlugins();

    if (!m_parser.IsPresent(OPTION_O)) {
        MSDK_TRACE_ERROR(MSDK_STRING("Output file not specified"));
        throw PipelineProfileNoOuputError();
    }
    //lightweight track summary
    ParseTrackSummary();
    ParseSessionInfo();

    //handling -o option and filenames
    CheckOutputExtension();
    BuildAudioVideoInfo();
    BuildMuxerInfo();
}
void PipelineProfile::ParseLogLevel() {
    if (m_parser.IsPresent(OPTION_TRACELEVEL)) {
        msdk_trace_set_level(TraceLevel::FromString(m_parser[OPTION_TRACELEVEL].as<msdk_string>()).ToTraceLevel());
    }
}
void PipelineProfile::ParsePlugins() {
    if (m_parser.IsPresent(OPTION_PLG)) {
        m_generic_plugins = m_parser[OPTION_PLG].as<std::vector<msdk_string> >();
    }
    if (m_parser.IsPresent(OPTION_VDECPLG)) {
        m_dec_plugins = m_parser[OPTION_VDECPLG].as<std::vector<msdk_string> >();
    }
    if (m_parser.IsPresent(OPTION_VENCPLG)) {
        m_enc_plugins = m_parser[OPTION_VENCPLG].as<std::vector<msdk_string> >();
    }
}

void PipelineProfile::ParseSessionInfo() {
    //define MFXInit parameters : impl and version
    APIChangeFeatures featuresa = {};
    APIChangeFeatures featuresv = {};

    // default implementation
    m_vSesInfo.IMPL() = MFX_IMPL_HARDWARE;

    if (m_parser.IsPresent(OPTION_SW)) {
        m_vSesInfo.IMPL() = MFX_IMPL_SOFTWARE;
    }

    if (m_parser.IsPresent(OPTION_D3D11)) {
        if (m_vSesInfo.IMPL() == MFX_IMPL_SOFTWARE) {
            MSDK_TRACE_ERROR(MSDK_STRING("Option -d3d11 can be used only with -hw option"));
            throw CommandlineConsistencyError();
        }
        m_vSesInfo.IMPL() = MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11;
    }
    if (m_parser.IsPresent(OPTION_ASW)) {
        m_aSesInfo.IMPL() = MFX_IMPL_SOFTWARE | MFX_IMPL_AUDIO;
    } else {
        m_aSesInfo.IMPL() = m_vSesInfo.IMPL() | MFX_IMPL_AUDIO;
    }

    featuresa.AudioDecode = true;

    //version could be codec - sensitive
    m_aSesInfo.Version() = getMinimalRequiredVersion(featuresa);
    m_vSesInfo.Version() = getMinimalRequiredVersion(featuresv);
}

void PipelineProfile::BuildAudioVideoInfo() {

    mfxStreamParams& sp = (mfxStreamParams&)m_splInfo;

      //if format not defined try first audio and video tracks but not both
    bool bFormatDefined =  m_bAudioTracksEnabled || m_bVideoTracksEnabled;

    if (!bFormatDefined) {
        for (size_t i = 0; i < sp.NumTracks; i++) {
            if (MFX_TRACK_ANY_AUDIO & sp.TrackInfo[i]->Type ) {
                if (m_bAudioTracksEnabled) {
                    MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Audio ") << MSDK_STRING("disabled"));
                    continue;
                }
                try
                {
                    AudioTrackProfile ap(m_splInfo, m_parser, m_extensions, m_codecs, m_aSesInfo, i);
                    if (m_bVideoTracksEnabled) {
                        MSDK_TRACE_ERROR(MSDK_STRING("Track ")<<i<<("   : Audio enabled, 2 tracks not supported without container"));
                        throw PipelineProfileOutputFormatError();
                    }
                    m_bAudioTracksEnabled = true;
                    MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Audio  enabled"));

                    m_muxerInfo.TrackMapping()[(int)i] = 0;
                    m_audios.push_back(ap);
                }
                catch (KnownException &)
                {
                    MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Audio  disabled"));
                }
            } else if (MFX_TRACK_ANY_VIDEO & sp.TrackInfo[(int)i]->Type ) {

                if (m_bVideoTracksEnabled) {
                    MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Video ") << MSDK_STRING("disabled"));
                    continue;
                }

                try
                {
                    VideoTrackProfile vp(m_splInfo, m_parser, m_extensions, m_codecs, m_vSesInfo, i);

                    if (m_bAudioTracksEnabled) {
                        MSDK_TRACE_ERROR(MSDK_STRING("Track ")<<i<<("   : Video enabled, 2 tracks not supported without container"));
                        throw PipelineProfileOutputFormatError();
                    }
                    m_bVideoTracksEnabled = true;

                    m_muxerInfo.TrackMapping()[(int)i] = 0;
                    m_videos.push_back(vp);
                    MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Video ") << MSDK_STRING("enabled"));
                }
                catch (KnownException &)
                {
                    MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Video  disabled"));
                }

            } else {
                MSDK_TRACE_INFO(MSDK_STRING("Track ") << i << MSDK_STRING("   : Unsupported track type : ")<<sp.TrackInfo[i]->Type);
            }
        }
        bFormatDefined =  m_bAudioTracksEnabled || m_bVideoTracksEnabled;
        if (!bFormatDefined) {
            MSDK_TRACE_ERROR(MSDK_STRING("Nothing to transcode"));
            throw NothingToTranscode();
        }
    } else {
        for (size_t i = 0; i < sp.NumTracks; i++) {
            if (MFX_TRACK_ANY_AUDIO & sp.TrackInfo[i]->Type ) {
                MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Audio ") << (m_bAudioTracksEnabled ? MSDK_STRING("enabled") : MSDK_STRING("disabled")));
                if (!m_bAudioTracksEnabled) continue;
                m_muxerInfo.TrackMapping()[(int)i] = (mfxU32)m_audios.size();
                m_audios.push_back(AudioTrackProfile (m_splInfo, m_parser, m_extensions, m_codecs, m_aSesInfo, i));
            } else if (MFX_TRACK_ANY_VIDEO & sp.TrackInfo[i]->Type ) {
                MSDK_TRACE_INFO(MSDK_STRING("Track ")<<i<<("   : Video ") << (m_bVideoTracksEnabled ? MSDK_STRING("enabled") : MSDK_STRING("disabled")));
                if (!m_bVideoTracksEnabled) continue;
                //audio tracks are first
                m_muxerInfo.TrackMapping()[(int)i] = (mfxU32)(m_bAudioTracksEnabled ? m_nAudioTracks : 0 + m_videos.size());
                m_videos.push_back(VideoTrackProfile(m_splInfo, m_parser, m_extensions, m_codecs, m_vSesInfo, i));
            } else {
                MSDK_TRACE_INFO(MSDK_STRING("Track ") << i << MSDK_STRING("   : Unsupported track type : ")<<sp.TrackInfo[i]->Type);
            }
        }
    }
}

void PipelineProfile::BuildMuxerInfo() {
    if(!m_bIsMuxer) {
        return;
    }

    for (size_t i = 0; i < m_audios.size(); i++) {
        mfxTrackInfo tInfo;
        MSDK_ZERO_MEMORY(tInfo);
        tInfo.SID = (mfxU32)m_muxerInfo.Tracks().size();
        tInfo.Enable = 1;
        tInfo.AudioParam = m_audios[i].GetTrackInfo().Encode.mfx;
        tInfo.Type  = ConvertCodecIdToTrackType(m_audios[i].GetTrackInfo().Encode.mfx.CodecId);
        m_muxerInfo.Tracks().push_back(tInfo);
    }

    for (size_t i = 0; i < m_videos.size(); i++) {
        mfxTrackInfo tInfo;
        MSDK_ZERO_MEMORY(tInfo);
        tInfo.SID = (mfxU32)m_muxerInfo.Tracks().size();
        tInfo.Enable = 1;
        tInfo.VideoParam = m_videos[i].GetTrackInfo().Encode.mfx;
        tInfo.Type  = ConvertCodecIdToTrackType(m_videos[i].GetTrackInfo().Encode.mfx.CodecId);
        m_muxerInfo.Tracks().push_back(tInfo);
    }
}

void PipelineProfile::CheckOutputExtension()
{
    const OutputFormat::Extensions & mux_extensions(m_extensions.MuxerExtensions());
    const OutputFormat::Extensions & raw_extensions_video(m_extensions.RawVideoExtensions());
    const OutputFormat::Extensions & raw_extensions_audio(m_extensions.RawAudioExtensions());

    //in case or extension mp4, and m2ts, mpg, mpeg, will create a multiplexer, otherwise no
    msdk_string filename = m_parser[OPTION_O].as<msdk_string>();
    size_t pointPos = filename.find_last_of(MSDK_CHAR('.'));
    if (pointPos == msdk_string::npos) {
        return ;
    }
    msdk_string e = filename.substr(pointPos + 1);

    OutputFormat::Extensions::const_iterator i = mux_extensions.find(e);

    if(mux_extensions.end() != i) {
        if (m_nVideoTracks) {
            m_bVideoTracksEnabled = true;
        }
        if (m_nAudioTracks) {
            m_bAudioTracksEnabled = true;
        }
        m_muxerInfo.SystemType() = i->second.system;
        m_bIsMuxer = true;
        return;
    }

    if(raw_extensions_video.end() != raw_extensions_video.find(e)) {
        if (!m_nVideoTracks) {
            MSDK_TRACE_ERROR(MSDK_STRING("Cannot transcode only video, since input contains no video"));
            throw PipelineProfileOnlyVideoTranscodeError();
        }
        //TODO: add description
        MSDK_TRACE_INFO(MSDK_STRING("Transcoding only video track: "));
        m_bAudioTracksEnabled = false;
        m_bVideoTracksEnabled = true;
        m_bIsMuxer = false;
        return ;
    }

    if(raw_extensions_audio.end() != raw_extensions_audio.find(e)) {
        if (!m_nAudioTracks) {
            MSDK_TRACE_ERROR(MSDK_STRING("Cannot transcode only audio, since input contains no audio"));
            throw PipelineProfileOnlyAudioTranscodeError();
        }
        //TODO: add description
        MSDK_TRACE_INFO(MSDK_STRING("Transcoding only first audio track: "));
        m_bIsMuxer = false;
        m_bVideoTracksEnabled = false;
        m_bAudioTracksEnabled = true;
        return;

    }

    MSDK_TRACE_ERROR(MSDK_STRING("Unsupported extension: ")<<e);
    throw PipelineProfileOutputFormatError();

}

void PipelineProfile::ParseTrackSummary() {
    mfxStreamParams& sp = (mfxStreamParams&)m_splInfo;

    for (int i = 0; i < sp.NumTracks; i++) {
        if (sp.TrackInfo[i]->Type & MFX_TRACK_ANY_AUDIO) {
            m_nAudioTracks ++;
        }
        if (sp.TrackInfo[i]->Type & MFX_TRACK_ANY_VIDEO) {
            m_nVideoTracks ++;
        }
    }
}

mfxTrackType PipelineProfile::ConvertCodecIdToTrackType(mfxU32 CodecId) {
    switch (CodecId) {
        case MFX_CODEC_AAC :
            return MFX_TRACK_AAC;
        case MFX_CODEC_MP3:
            return MFX_TRACK_MPEGA;
        case MFX_CODEC_MPEG2:
            return MFX_TRACK_MPEG2V;
        case MFX_CODEC_AVC:
            return MFX_TRACK_H264;
        case MFX_CODEC_HEVC:
            return MFX_TRACK_H264;
        case MFX_CODEC_VC1 :
            return MFX_TRACK_MPEGA;
        default:
            MSDK_TRACE_ERROR(MSDK_STRING("unknown codec id") << CodecId);
            return MFX_TRACK_UNKNOWN;
    }
}