//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once
#include <map>
#include "mfxsplmux++.h"
#include "sample_utils.h"
#include "cmdline_parser.h"
#include "cmdline_options.h"
#include "session_info.h"
#include "track_profile.h"
#include "stream_params_ext.h"


class IPipelineProfile {
public:
    virtual ~IPipelineProfile () {}
    virtual bool isAudioDecoderExist (int nTrack) const  = 0;
    virtual bool isAudioEncoderExist (int nTrack) const  = 0;
    virtual bool isDecoderExist () const  = 0;
    virtual bool isDecoderPluginExist () const  = 0;
    virtual bool isEncoderPluginExist () const  = 0;
    virtual bool isGenericPluginExist () const  = 0;
    virtual bool isVppExist () const  = 0;
    virtual bool isEncoderExist ()const  = 0;
    virtual bool isMultiplexerExist () const  = 0;
    virtual VideoTrackInfoExt GetVideoTrackInfo()const  = 0;
    virtual AudioTrackInfoExt GetAudioTrackInfo(size_t nTrack)const  = 0;
    virtual MFXStreamParamsExt GetMuxerInfo() const = 0;
    //virtual LogLevel & GetLoggingLevel() const = 0;
};

class PipelineProfile : public IPipelineProfile, private no_copy {
    MFXStreamParams m_splInfo;
    CmdLineParser  &m_parser;
    bool m_bIsMuxer;
    std::vector<AudioTrackProfile> m_audios;
    std::vector<VideoTrackProfile> m_videos;

    std::vector<msdk_string> m_generic_plugins;
    std::vector<msdk_string> m_dec_plugins;
    std::vector<msdk_string> m_enc_plugins;

    MFXStreamParamsExt m_muxerInfo;
public:
    PipelineProfile(const MFXStreamParams &splInfo, CmdLineParser & parser, OutputFormat &, OutputCodec & ) ;
    virtual bool isAudioDecoderExist (int nTrack) const {
        return m_audios.size() > (size_t)nTrack;    
    }
    virtual bool isAudioEncoderExist (int nTrack) const {
        if (!isAudioDecoderExist(nTrack)) {
            return false;
        }
        //if acodec copy no audio transcoding
        if (m_parser.IsPresent(OPTION_ACODEC) && m_parser[OPTION_ACODEC].as<msdk_string>() == ARG_COPY) {
            return false;
        }
        return true;    
    }
    virtual bool isDecoderExist () const {
        return !m_videos.empty() ;    
    }
    virtual bool isDecoderPluginExist () const {
        return !m_dec_plugins.empty();
    }
    virtual bool isEncoderPluginExist () const {
        return !m_enc_plugins.empty();
    }
    virtual bool isGenericPluginExist () const {
        return !m_generic_plugins.empty();
    }
    virtual bool isVppExist () const {
        return m_parser.IsPresent(OPTION_W) || m_parser.IsPresent(OPTION_H) ;
    }
    virtual bool isEncoderExist ()const {
        if (!isDecoderExist()) {
            return false;
        }
        //if acodec copy no audio transcoding
        if (m_parser.IsPresent(OPTION_VCODEC) && m_parser[OPTION_VCODEC].as<msdk_string>() == ARG_COPY) {
            return false;
        }
        return true;    
    }
    virtual bool isMultiplexerExist() const {
        return m_bIsMuxer;
    }

    virtual VideoTrackInfoExt GetVideoTrackInfo()const {
        return m_videos[0].GetTrackInfo();
    }
    virtual AudioTrackInfoExt GetAudioTrackInfo(size_t nTrack)const {
        return m_audios[nTrack].GetTrackInfo();
    }
    virtual MFXStreamParamsExt GetMuxerInfo() const{
        return m_muxerInfo;
    }
protected:
    void ParseLogLevel();
    void ParseStreamInfo();
    void ParseSessionInfo();
    void ParsePlugins();
    void ParseAEncodeParams( size_t nTrack, mfxAudioParam &aParam, OutputFormat & extensions);
    void ParseVEncodeParams( size_t nTrack, mfxVideoParam &vParam, OutputFormat & extensions);
    void CheckOutputExtension();
    void BuildAudioVideoInfo();
    void BuildMuxerInfo();
    void ParseTrackSummary();

    MFXSessionInfo m_vSesInfo;
    MFXSessionInfo m_aSesInfo;

    unsigned int m_nAudioTracks;
    unsigned int m_nVideoTracks;
    
    bool m_bAudioTracksEnabled;
    bool m_bVideoTracksEnabled;

    OutputFormat m_extensions;
    OutputCodec m_codecs;

    mfxTrackType ConvertCodecIdToTrackType(mfxU32 CodecId);
};
