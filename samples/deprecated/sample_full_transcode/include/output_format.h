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

#include <vector>
#include "sample_utils.h"
#include "mfxastructures.h"
#include "../../sample_spl_mux/api/mfxsmstructures.h"
#include <map>


class OutputFormat {
public:
    struct Extension {
        mfxU32 acodec;
        mfxU32 vcodec;
        mfxSystemStreamType system;
        Extension (mfxU32 vcodec = 0,
                   mfxU32 acodec = 0,
                   mfxSystemStreamType system = MFX_UNDEF_STREAM)
        : acodec(acodec)
        , vcodec(vcodec)
        , system(system) {
        }
    };
    typedef std::map<msdk_string, Extension> Extensions;

    OutputFormat() {
        mux_extensions[MSDK_CHAR("mp4")] = Extension(MFX_CODEC_AVC, MFX_CODEC_AAC, MFX_MPEG4_SYSTEM_STREAM);
        mux_extensions[MSDK_CHAR("mpg")] = Extension( MFX_CODEC_MPEG2, MFX_CODEC_MP3, MFX_MPEG2_TRANSPORT_STREAM);
        mux_extensions[MSDK_CHAR("mpeg")] = Extension( MFX_CODEC_MPEG2, MFX_CODEC_MP3, MFX_MPEG2_TRANSPORT_STREAM);
        mux_extensions[MSDK_CHAR("m2ts")] = Extension( MFX_CODEC_MPEG2, MFX_CODEC_MP3, MFX_MPEG2_TRANSPORT_STREAM);

        //in case of undef streams we will use not a multiplexer but file writer, so it is OK
        raw_extensions_video[MSDK_CHAR("h264")] = Extension(MFX_CODEC_AVC, 0, MFX_UNDEF_STREAM);
        raw_extensions_video[MSDK_CHAR("m2v")] = Extension(MFX_CODEC_MPEG2, 0, MFX_UNDEF_STREAM);

        raw_extensions_audio[MSDK_CHAR("mp3")] = Extension( 0, MFX_CODEC_MP3, MFX_UNDEF_STREAM);
        raw_extensions_audio[MSDK_CHAR("aac")] = Extension( 0, MFX_CODEC_AAC, MFX_UNDEF_STREAM);
    }

    virtual const Extensions& MuxerExtensions() const {
        return mux_extensions;
    }
    virtual const Extensions & RawVideoExtensions()const {
        return raw_extensions_video;
    }
    virtual const Extensions & RawAudioExtensions() const{
        return raw_extensions_audio;
    }

protected:
    Extensions mux_extensions;
    Extensions raw_extensions_video;
    Extensions raw_extensions_audio;
};

//parametrized output for -acodec -vcodec options
class OutputCodec {
public:
    class StringToCodecMap {
        std::map<msdk_string, mfxU32> m_map;
    public:
        mfxU32& operator [] (const msdk_string &id) {
            return m_map[id];
        }
        mfxU32 operator [] (const msdk_string &id) const {
            return m_map.find(id)->second;
        }
        void ToStream(msdk_ostream & os) const {
            std::map<msdk_string, mfxU32> :: const_iterator i;
            for (i = m_map.begin(); i!= m_map.end(); i++) {
                os << i->first <<" ";
            }
        }
        bool Has(const msdk_string &id) const {
            return m_map.find(id) != m_map.end();
        }
    };
protected:
    StringToCodecMap acodecs;
    StringToCodecMap vcodecs;
public:
    OutputCodec () {
       // acodecs[MSDK_CHAR("mp3")] = MFX_CODEC_MP3;
        acodecs[MSDK_CHAR("aac")] = MFX_CODEC_AAC;
        vcodecs[MSDK_CHAR("h264")] = MFX_CODEC_AVC;
        //vcodecs[MSDK_CHAR("mpeg2")] = MFX_CODEC_MPEG2;
    }
    virtual const StringToCodecMap & ACodec () const {
        return acodecs;
    }
    virtual const StringToCodecMap & VCodec () const {
        return vcodecs;
    }
};

inline msdk_ostream & operator << (msdk_ostream & os, const OutputCodec::StringToCodecMap &smap) {
    smap.ToStream(os);
    return os;
}