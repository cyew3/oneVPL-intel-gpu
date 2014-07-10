//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfxstructures.h"
#include "mfxastructures.h"
#include "av_param.h"
#include "session_info.h"
#include <map>
#include "mfxsplmux++.h"
#include "cmdline_parser.h"
#include "cmdline_options.h"
#include "output_format.h"

template <class T>
struct TrackInfoExt {
    mfxU32 SID;//stream id
    T Decode;
    T VPP;
    T Encode;
    MFXSessionInfo Session;

    TrackInfoExt(mfxU32 sid = 0,
        T decodeParams = T(),
        T vppParams = T(),
        T encodeParams = T(),
        MFXSessionInfo sessionInfo = MFXSessionInfo())
        : SID(sid)
        , Decode(decodeParams)
        , VPP(vppParams)
        , Encode(encodeParams)
        , Session(sessionInfo) {
    }
};

template <class T>
inline TrackInfoExt<T> CreateTrackInfo(mfxU32 sid, T decodeParams, T vppParams, T encodeParams, MFXSessionInfo sessionInfo) {
    TrackInfoExt<T> info(sid, decodeParams, vppParams, encodeParams, sessionInfo);
    return info;
}

typedef TrackInfoExt<mfxAudioParam> AudioTrackInfoExt;
typedef TrackInfoExt<mfxVideoParam> VideoTrackInfoExt;

namespace detail {
    //rationale - pipeline profile decomposition into separate tracks profiles
    class TrackProfileBase {
    protected:
        MFXStreamParams m_splInfo;
        size_t m_track;
        MFXSessionInfo m_sessionInfo;
        CmdLineParser *m_parser;
        //used in ctor only
        OutputCodec::StringToCodecMap m_codecs;

        std::map<msdk_string, mfxU32> m_extensions;

        TrackProfileBase (const MFXStreamParams &splInfo, CmdLineParser & parser, MFXSessionInfo sesInfo, size_t nTrack);
        virtual ~TrackProfileBase (){}

        virtual mfxU32 ConvertStrToMFXCodecId(const msdk_string & str);
        virtual mfxU32 GuessCodecFromExtension(const msdk_string &extension);

        virtual void OnUnsupportedCodec(const msdk_string &str) = 0;
        virtual void OnUnsupportedExtension(const msdk_string &extension)  = 0;
        virtual void OnNoExtension()  = 0;

        virtual msdk_string GetOutputExtension();

        //dedicated handler for -o option
        template <class T, class T2>
        void parse_codec_option(T & param, T2 & srcParam, const msdk_string & option) {
            if (m_parser->IsPresent(option)) {
                msdk_string codec_str = (*m_parser)[option].as<msdk_string>();
                if (codec_str == ARG_COPY) {
                    param.CodecId = srcParam.CodecId;
                } else {
                    param.CodecId = ConvertStrToMFXCodecId(codec_str);
                }
            } else {
                param.CodecId = GuessCodecFromExtension(GetOutputExtension());
            }
        }
    };

    template <class T>
    class TrackProfileBaseTmpl : public TrackProfileBase {
    protected:
        TrackInfoExt<T> m_trackInfo;
    public:
        TrackProfileBaseTmpl (const MFXStreamParams &splInfo, CmdLineParser & parser, MFXSessionInfo sesInfo, size_t nTrack)
            : TrackProfileBase (splInfo, parser, sesInfo, nTrack)
            , m_trackInfo((mfxU32)nTrack, T(), T(), T(), sesInfo) {
        }
        virtual TrackInfoExt<T> GetTrackInfo() const {
            return m_trackInfo;
        }
    };
}

template <class T>
class TrackProfile {};

template <>
class TrackProfile <mfxAudioParam> : public detail::TrackProfileBaseTmpl<mfxAudioParam> {
public:
    TrackProfile(const MFXStreamParams &splInfo, CmdLineParser & parser, OutputFormat & extensions, OutputCodec &codecs, MFXSessionInfo sesInfo, size_t nTrack);

protected:
    virtual void ParseDecodeParams();
    virtual void ParseEncodeParams();
    virtual void OnNoExtension();
    virtual void OnUnsupportedExtension(const msdk_string &extension);
    virtual void OnUnsupportedCodec(const msdk_string & str);
};

template <>
class TrackProfile <mfxVideoParam>: public detail::TrackProfileBaseTmpl<mfxVideoParam> {

public:
    TrackProfile (const MFXStreamParams &splInfo, CmdLineParser & parser, OutputFormat & extensions, OutputCodec &codecs, MFXSessionInfo sesInfo, size_t nTrack);

protected:
    virtual void ParseDecodeParams();
    virtual void ParseVPPParams();
    virtual void ParseEncodeParams();
    virtual void OnNoExtension();
    virtual void OnUnsupportedExtension(const msdk_string &extension);
    virtual void OnUnsupportedCodec(const msdk_string &);
};

typedef TrackProfile<mfxAudioParam> AudioTrackProfile ;
typedef TrackProfile<mfxVideoParam> VideoTrackProfile ;
