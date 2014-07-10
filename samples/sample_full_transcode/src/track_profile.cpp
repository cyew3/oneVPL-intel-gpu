/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

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
    ParseVPPParams();
}

void TrackProfile<mfxVideoParam>::OnUnsupportedCodec( const msdk_string & str)
{
    MSDK_TRACE_ERROR(MSDK_STRING("Unsupported video codec_id: ") << str);
    throw UnsupportedVideoCodecError();
}

void TrackProfile<mfxVideoParam>::OnUnsupportedExtension( const msdk_string &extension )
{
    MSDK_TRACE_ERROR(MSDK_STRING("Canot gues video codec from extension: ") << extension);
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
    m_trackInfo.Encode.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    if (m_parser->IsPresent(OPTION_HW)) {
        m_trackInfo.Encode.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }

    parse_codec_option(vInfoMFX, vInfo.VideoParam, OPTION_VCODEC);
}

void TrackProfile<mfxVideoParam>::ParseDecodeParams()
{
    mfxTrackInfo & vInfo = *((mfxStreamParams&)m_splInfo).TrackInfo[m_track];

    m_trackInfo.Decode.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    if (m_parser->IsPresent(OPTION_HW)) {
        m_trackInfo.Decode.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    m_trackInfo.Decode.mfx.CodecId = vInfo.VideoParam.CodecId;
}

void TrackProfile<mfxVideoParam>::ParseVPPParams()
{
    mfxTrackInfo & vInfo = *((mfxStreamParams&)m_splInfo).TrackInfo[m_track];
    m_trackInfo.VPP.vpp.Out.Width = m_trackInfo.VPP.vpp.Out.CropW = vInfo.VideoParam.FrameInfo.Width;
    m_trackInfo.VPP.vpp.Out.Height = m_trackInfo.VPP.vpp.Out.CropH = vInfo.VideoParam.FrameInfo.Height;
    if (m_parser->IsPresent(OPTION_W)) {
        mfxU16 width = (*m_parser)[OPTION_W].as<mfxU16>();
        m_trackInfo.VPP.vpp.Out.Width = m_trackInfo.VPP.vpp.Out.CropW = width;
    }
    if (m_parser->IsPresent(OPTION_H)) {
        mfxU16 height = (*m_parser)[OPTION_H].as<mfxU16>();
        m_trackInfo.VPP.vpp.Out.Height = m_trackInfo.VPP.vpp.Out.CropH = height;
    }
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
    parse_codec_option(aInfoMFX, aInfo.AudioParam, OPTION_ACODEC);

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
    throw UnsupportedAudioCodecFromExtension();
}

void TrackProfile <mfxAudioParam>::OnUnsupportedCodec( const msdk_string & str )
{
    MSDK_TRACE_ERROR(MSDK_STRING("Unsupported audio codec_id: ") << str);
    throw UnsupportedAudioCodecError();
}

