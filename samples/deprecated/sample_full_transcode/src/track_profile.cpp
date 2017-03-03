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

#include "track_profile.h"
#include "sample_utils.h"
#include "sample_defs.h"
#include <iostream>
#include "cmdline_options.h"
#include <limits>

using namespace detail;

TrackProfileBase::TrackProfileBase( const MFXStreamParams &splInfo, CmdLineParser & parser, MFXSessionInfo sesInfo, size_t nTrack ) :
    m_splInfo(splInfo),
    m_track(nTrack),
    m_sessionInfo(sesInfo),
    m_parser(&parser) {
}

mfxU32 TrackProfileBase::ConvertStrToMFXCodecId( const msdk_string & str ) {
    if (!m_codecs.Has(str)) {
        OnUnsupportedCodec(str);
    }
    return m_codecs[str];
}

mfxU32 TrackProfileBase::GuessCodecFromExtension( const msdk_string &extension ) {
    if (m_extensions.find(extension) == m_extensions.end()) {
        OnUnsupportedExtension(extension);
    }
    return m_extensions[extension];
}

msdk_string TrackProfileBase::GetOutputExtension() {
    msdk_string filename = (*m_parser)[OPTION_O].as<msdk_string>();
    size_t pointPos = filename.find_last_of(MSDK_CHAR('.'));
    if (pointPos == msdk_string::npos) {
        OnNoExtension();
    }
    return filename.substr(pointPos + 1);
}

TrackProfile<mfxVideoParam>::TrackProfile (const MFXStreamParams &splInfo, CmdLineParser & parser, OutputFormat & extensions, OutputCodec &codecs, MFXSessionInfo sesInfo, size_t nTrack )
    : TrackProfileBaseTmpl<mfxVideoParam>(splInfo, parser, sesInfo, nTrack)
{
    OutputFormat::Extensions mext ( extensions.MuxerExtensions());
    OutputFormat::Extensions vext ( extensions.RawVideoExtensions());


    for (OutputFormat::Extensions::iterator i = mext.begin(); i != mext.end(); i++) {
        m_extensions[i->first] = i->second.vcodec;
    }
    for (OutputFormat::Extensions::iterator i = vext.begin(); i != vext.end(); i++) {
        m_extensions[i->first] = i->second.vcodec;
    }

    m_codecs = codecs.VCodec();

    ParseEncodeParams();
    ParseDecodeParams();
}

void TrackProfile<mfxVideoParam>::OnUnsupportedCodec( const msdk_string & str)
{
    MSDK_TRACE_ERROR(MSDK_STRING("Unsupported video codec_id: ") << str);
    msdk_cout<<MSDK_STRING("ERROR: Video codec is unsupported (it is configured incorrectly or cannot be autodetected).\n       Please define it correctly using -vcodec option\n");
    throw UnsupportedVideoCodecError();
}

void TrackProfile<mfxVideoParam>::OnUnsupportedExtension( const msdk_string &extension )
{
    MSDK_TRACE_ERROR(MSDK_STRING("Canot guess video codec from extension: ") << extension);
    msdk_cout<<MSDK_STRING("ERROR: Video codec cannot be autodetected.\n       Please define it correctly using -vcodec option\n");
    throw UnsupportedVideoCodecFromExtension();
}

void TrackProfile<mfxVideoParam>::OnNoExtension()
{
    MSDK_TRACE_ERROR(MSDK_STRING("Cannot guess video codec based on extension, use option: ")<<OPTION_VCODEC);
    throw CannotGetExtensionError();
}

void TrackProfile<mfxVideoParam>::ParseEncodeParams()
{
    mfxTrackInfo & vInfo = *((mfxStreamParams&)m_splInfo).TrackInfo[m_track];
    mfxInfoMFX &vInfoMFX = m_trackInfo.Encode.mfx;

    if (m_parser->IsPresent(OPTION_VB)) {
        mfxU32 bitrate = (*m_parser)[OPTION_VB].as<mfxU32>();
        if (bitrate > (std::numeric_limits<mfxU16>::max)()) {
            vInfoMFX.BRCParamMultiplier = 1 + (mfxU16)( bitrate / (std::numeric_limits<mfxU16>::max)());
        } else {
            vInfoMFX.BRCParamMultiplier = 1;
        }
        vInfoMFX.TargetKbps = (mfxU16) (bitrate / vInfoMFX.BRCParamMultiplier);
    }
    else
    {
        msdk_cout<<MSDK_STRING("Output video bitrate is not found, default value of 2000Kbps will be used\n");
        vInfoMFX.BRCParamMultiplier = 1;
        vInfoMFX.TargetKbps = (mfxU16)2000;
    }


    m_trackInfo.Encode.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    if (m_parser->IsPresent(OPTION_HW) || !m_parser->IsPresent(OPTION_SW)) {
        m_trackInfo.Encode.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    if (m_parser->IsPresent(OPTION_ADEPTH)) {
        m_trackInfo.Encode.AsyncDepth = (*m_parser)[OPTION_ADEPTH].as<mfxU16>();
    }

    try
    {
        parse_codec_option(vInfoMFX, vInfo.VideoParam, OPTION_VCODEC);
    }
    catch(KnownException e)
    {

    }
}

void TrackProfile<mfxVideoParam>::ParseDecodeParams()
{
    mfxTrackInfo & vInfo = *((mfxStreamParams&)m_splInfo).TrackInfo[m_track];

    m_trackInfo.Decode.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    if (m_parser->IsPresent(OPTION_HW)) {
        m_trackInfo.Decode.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    if (m_parser->IsPresent(OPTION_ADEPTH)) {
        m_trackInfo.Decode.AsyncDepth = (*m_parser)[OPTION_ADEPTH].as<mfxU16>();
    }
    m_trackInfo.Decode.mfx.CodecId = vInfo.VideoParam.CodecId;
}

TrackProfile <mfxAudioParam>::TrackProfile( const MFXStreamParams &splInfo
                                           , CmdLineParser & parser
                                           , OutputFormat & extensions
                                           , OutputCodec & codecs
                                           , MFXSessionInfo sesInfo, size_t nTrack)
    : TrackProfileBaseTmpl<mfxAudioParam>(splInfo, parser, sesInfo,  nTrack)
{
    OutputFormat::Extensions mext ( extensions.MuxerExtensions());
    OutputFormat::Extensions aext ( extensions.RawAudioExtensions());

    for (OutputFormat::Extensions::iterator i = mext.begin(); i != mext.end(); i++) {
        m_extensions[i->first] = i->second.acodec;
    }
    for (OutputFormat::Extensions::iterator i = aext.begin(); i != aext.end(); i++) {
        m_extensions[i->first] = i->second.acodec;
    }
    m_codecs = codecs.ACodec();

    ParseEncodeParams();
    ParseDecodeParams();
}

void TrackProfile <mfxAudioParam>::ParseDecodeParams() {
    mfxTrackInfo & aInfo = *((mfxStreamParams&)m_splInfo).TrackInfo[m_track];

    m_trackInfo.Decode.mfx.CodecId = aInfo.AudioParam.CodecId;
}

void TrackProfile <mfxAudioParam>::ParseEncodeParams( )
{
    mfxTrackInfo & aInfo = *((mfxStreamParams&)m_splInfo).TrackInfo[m_track];
    mfxAudioInfoMFX &aInfoMFX = m_trackInfo.Encode.mfx;

    //take all audio encoder setting from splitter
    aInfoMFX.Bitrate = aInfo.AudioParam.Bitrate;
    aInfoMFX.SampleFrequency = aInfo.AudioParam.SampleFrequency;
    aInfoMFX.NumChannel = aInfo.AudioParam.NumChannel;
    aInfoMFX.BitPerSample = aInfo.AudioParam.BitPerSample;
    //aInfoMFX.StereoMode = aInfo.AudioParam.StereoMode;

    if (m_parser->IsPresent(OPTION_AB)) {
        aInfoMFX.Bitrate = (*m_parser)[OPTION_AB].as<mfxU32>();
        if ( (std::numeric_limits<mfxU32>::max)() / 1000 < aInfoMFX.Bitrate) {
            MSDK_TRACE_ERROR(MSDK_STRING("Audio bitrate too big : ") << aInfoMFX.Bitrate<<MSDK_STRING("MAX is: ") <<
                (std::numeric_limits<mfxU32>::max)() / 1000);
            throw AudioBitrateError();
        }
        aInfoMFX.Bitrate *= 1000;
    }

    try
    {
        parse_codec_option(aInfoMFX, aInfo.AudioParam, OPTION_ACODEC);
    }
    catch(KnownException e)
    {
        msdk_cout<<MSDK_STRING("ERROR: Audio codec is unsupported (it is configured incorrectly or cannot be autodetected).\n       Using default audio codec type: aac\n");
        aInfoMFX.CodecId = MFX_CODEC_AAC;
        aInfoMFX.OutputFormat = MFX_AUDIO_AAC_ADTS;
        aInfoMFX.CodecProfile = MFX_PROFILE_AAC_LC;
    }

    if (m_parser->IsPresent(OPTION_ACODEC_COPY)) {
        aInfoMFX = aInfo.AudioParam;
    } else {
        // Audio library encoder supports only aac format
        aInfoMFX.CodecId = MFX_CODEC_AAC;
        aInfoMFX.OutputFormat = MFX_AUDIO_AAC_ADTS;
        aInfoMFX.CodecProfile = MFX_PROFILE_AAC_LC;
    }

}

void TrackProfile <mfxAudioParam>::OnNoExtension()
{
    MSDK_TRACE_ERROR(MSDK_STRING("Cannot guess video codec based on extension, use option: ") << OPTION_ACODEC);
    throw CannotGetExtensionError();
}

void TrackProfile <mfxAudioParam>::OnUnsupportedExtension( const msdk_string &extension )
{
    MSDK_TRACE_ERROR(MSDK_STRING("Canot guess audio codec from extension: ") << extension);
    //msdk_cout<<MSDK_STRING("ERROR: Audio codec is unsupported (it is configured incorrectly or cannot be autodetected).\n       Please define it correctly using -acodec option\n");
    throw UnsupportedAudioCodecFromExtension();
}

void TrackProfile <mfxAudioParam>::OnUnsupportedCodec( const msdk_string & str )
{
    MSDK_TRACE_ERROR(MSDK_STRING("Unsupported audio codec_id: ") << str);
    //msdk_cout<<MSDK_STRING("ERROR: Audio codec cannot be autodetected.\n       Please define it correctly using -acodec option\n");
    throw UnsupportedAudioCodecError();
}

