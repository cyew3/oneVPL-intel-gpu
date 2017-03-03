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
#include "mfxsplmux++.h"
#include "sample_utils.h"
#include "sample_defs.h"
#include "exceptions.h"
#include <iostream>
#include <memory>

enum {
    SAMPLE_SURFACE,
    SAMPLE_BITSTREAM,
    SAMPLE_AUDIO_FRAME,
};

enum {
    META_SPSPPS,
    META_VIDEOPARAM,
    META_EOS,
    META_EOS_RESET
};

typedef std::vector<char> MetaData;

class ISample
{
public:
    virtual ~ISample() {}
    virtual mfxFrameSurface1 & GetSurface() = 0;
    virtual mfxAudioFrame & GetAudioFrame() = 0;
    virtual mfxBitstream & GetBitstream() = 0;
    virtual int  GetSampleType() const = 0;
    virtual int  GetTrackID() const = 0;
    virtual bool GetMetaData(int type, MetaData& /*out*/) const = 0;
    virtual bool HasMetaData(int type) const = 0;
};

typedef std::auto_ptr<ISample> SamplePtr;

enum {
    SAMPLES_REFERENCES_DIFFERENT = 0,
    SAMPLES_REFERENCES_SAME,
    SAMPLES_HAVE_INCOPARABLE_TYPES
};

//compares references inside sample
inline int CompareSamples (const ISample &lhs, const ISample &rhs) {
    if (lhs.GetSampleType() != rhs.GetSampleType())
        return SAMPLES_HAVE_INCOPARABLE_TYPES;

    ISample &nonc_lhs = const_cast<ISample&>(lhs);
    ISample &nonc_rhs = const_cast<ISample&>(rhs);

    switch (lhs.GetSampleType())
    {
        case SAMPLE_BITSTREAM :
            return &nonc_lhs.GetBitstream() == &nonc_rhs.GetBitstream();
        case SAMPLE_SURFACE:
            return &nonc_lhs.GetSurface() == &nonc_rhs.GetSurface();
        case SAMPLE_AUDIO_FRAME:
            return &nonc_lhs.GetAudioFrame() == &nonc_rhs.GetAudioFrame();
        default: {
            MSDK_TRACE_DEBUG("Unknown Sample type used in comparision: "<< lhs.GetSampleType());
            return SAMPLES_HAVE_INCOPARABLE_TYPES;
        }
    }
}

//lightweight wrapper for MediaSDK containers
template <class T, int SampleType>
class SampleImplTmpl : public ISample {
protected:
    T  m_Data;
    int m_nTrackId;

    SampleImplTmpl (int TrackId) :
        m_Data(),
        m_nTrackId(TrackId) {
    }

    SampleImplTmpl(T data, int TrackId)
        : m_Data(data)
        , m_nTrackId(TrackId) {
    }

public:
    virtual mfxFrameSurface1 & GetSurface() {
        MSDK_TRACE_ERROR(MSDK_STRING("GetSurface(): Incompatible sample type error"));
        throw IncompatibleSampleTypeError();
    }
    virtual mfxBitstream & GetBitstream() {
        MSDK_TRACE_ERROR(MSDK_STRING("GetBitstream(): Incompatible sample type error"));
        throw IncompatibleSampleTypeError();
    }
    virtual mfxAudioFrame & GetAudioFrame() {
        MSDK_TRACE_ERROR(MSDK_STRING("GetAudioFrame(): Incompatible sample type error"));
        throw IncompatibleSampleTypeError();
    }
    virtual int GetTrackID()const{
        return m_nTrackId;
    }
    virtual bool HasMetaData(int /*type*/) const  {
        return false;
    }
    virtual bool GetMetaData(int /*type*/, MetaData& /*out*/) const  {
        //pure Data Sample doesn't contain any metadata
        return false;
    }
    virtual int GetSampleType()const{
        return SampleType;
    }
};

class SampleSurface1 : public SampleImplTmpl<mfxFrameSurface1*, SAMPLE_SURFACE>
{
    typedef SampleImplTmpl<mfxFrameSurface1*, SAMPLE_SURFACE> base;
public:
    SampleSurface1(mfxFrameSurface1& surf, int TrackId) :
        base(&surf, TrackId) {
    }
    //SampleSurface(SampleSurface& sample) {
    //    sample;
    //}
    virtual mfxFrameSurface1 & GetSurface() {
        return *m_Data;
    }
};

class SampleBitstream1 : public SampleImplTmpl<mfxBitstream*, SAMPLE_BITSTREAM>
{
    typedef SampleImplTmpl<mfxBitstream*, SAMPLE_BITSTREAM> base;
public:
    SampleBitstream1(mfxBitstream& bs, int TrackId) :
        base(&bs, TrackId) {
    }
    virtual mfxBitstream & GetBitstream() {
        return *m_Data;
    }
};

class SampleAudioFrame : public SampleImplTmpl<mfxAudioFrame*, SAMPLE_AUDIO_FRAME>
{
    typedef SampleImplTmpl<mfxAudioFrame*, SAMPLE_AUDIO_FRAME> base;

public:
    SampleAudioFrame(mfxAudioFrame& frame, int TrackId) :
        base(&frame, TrackId) {
    }
    virtual mfxAudioFrame & GetAudioFrame() {
        return *m_Data;
    }
};


//bitstream with data
class SampleBitstream : public SampleImplTmpl<mfxBitstream, SAMPLE_BITSTREAM>
{
    typedef SampleImplTmpl<mfxBitstream, SAMPLE_BITSTREAM> base;
    std::vector<mfxU8> m_bits;
public:
    SampleBitstream(mfxU32 dataLen, int TrackId )
        : base(TrackId)
        , m_bits(dataLen) {
        if (!dataLen) {
            MSDK_TRACE_ERROR(MSDK_STRING("Sample bitstream DataLen=0"));
            throw SampleBitstreamNullDatalenError();
        }
        m_Data.Data = &m_bits.front();
        m_Data.MaxLength = dataLen;
    }

    SampleBitstream(mfxBitstream* bits, int TrackId)
        : base(TrackId)
        , m_bits(bits->DataLength) {
        if (!bits->DataLength) {
            MSDK_TRACE_ERROR(MSDK_STRING("Sample bitstream DataLen=0"));
            throw SampleBitstreamNullDatalenError();
        }
        MSDK_MEMCPY(&m_bits.front(), bits->Data, bits->DataLength);
        m_Data = *bits;
        m_Data.Data = &m_bits.front();
    }

    virtual mfxBitstream & GetBitstream(){
        return m_Data;
    }
};

class SampleAudioFrameWithData : public SampleImplTmpl<mfxAudioFrame, SAMPLE_AUDIO_FRAME> {
    typedef SampleImplTmpl<mfxAudioFrame, SAMPLE_AUDIO_FRAME> base;
    std::vector<mfxU8> m_bits;
public:
    SampleAudioFrameWithData(mfxU32 dataLen, int TrackId )
        : base(TrackId)
        , m_bits(dataLen) {
        if (!dataLen) {
            MSDK_TRACE_ERROR(MSDK_STRING("Sample audio frame DataLen=0"));
            throw SampleAudioFrameNullDatalenError();
        }
        m_Data.Data = &m_bits.front();
        m_Data.MaxLength = dataLen;
    }

    SampleAudioFrameWithData(mfxAudioFrame* bits, int TrackId)
        : base(TrackId)
        , m_bits(bits->DataLength) {
        if (!bits->DataLength) {
            MSDK_TRACE_ERROR(MSDK_STRING("Sample audio frame DataLen=0"));
            throw SampleBitstreamNullDatalenError();
        }
        MSDK_MEMCPY(&m_bits.front(), bits->Data, bits->DataLength);
        m_Data.Data = &m_bits.front();
        m_Data.DataLength = bits->DataLength;
    }
    virtual mfxAudioFrame & GetAudioFrame(){
        return m_Data;
    }
};

//surface with data
class SampleSurfaceWithData : public SampleImplTmpl<mfxFrameSurface1, SAMPLE_SURFACE>
{
    typedef SampleImplTmpl<mfxFrameSurface1, SAMPLE_SURFACE> base;
public:
    SampleSurfaceWithData(const mfxFrameSurface1& surf, int TrackId) :
        base(surf, TrackId) {
    }
    //SampleSurface(SampleSurface& sample) {
    //    sample;
    //}
    virtual mfxFrameSurface1 & GetSurface() {
        return m_Data;
    }
};
