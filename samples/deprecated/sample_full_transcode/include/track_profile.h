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
    T Encode;
    MFXSessionInfo Session;

    TrackInfoExt(mfxU32 sid = 0,
        T decodeParams = T(),
        T encodeParams = T(),
        MFXSessionInfo sessionInfo = MFXSessionInfo())
        : SID(sid)
        , Decode(decodeParams)
        , Encode(encodeParams)
        , Session(sessionInfo) {
    }
};

template <class T>
inline TrackInfoExt<T> CreateTrackInfo(mfxU32 sid, T decodeParams, T encodeParams, MFXSessionInfo sessionInfo) {
    TrackInfoExt<T> info(sid, decodeParams, encodeParams, sessionInfo);
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
            , m_trackInfo((mfxU32)nTrack, T(), T(), sesInfo) {
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
    virtual void ParseEncodeParams();
    virtual void ParseDecodeParams();
    virtual void OnNoExtension();
    virtual void OnUnsupportedExtension(const msdk_string &extension);
    virtual void OnUnsupportedCodec(const msdk_string &);
};

typedef TrackProfile<mfxAudioParam> AudioTrackProfile ;
typedef TrackProfile<mfxVideoParam> VideoTrackProfile ;
