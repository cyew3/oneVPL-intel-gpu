//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//

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