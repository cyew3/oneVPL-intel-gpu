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

#include "isample.h"

class AddMetadata : public ISample {
    SamplePtr m_sample;
    MetaData m_metadata;
    int m_nMetadataType;
public:
    AddMetadata(SamplePtr& sample, int type, const MetaData &mdata)
        : m_sample(sample)
        , m_metadata(mdata)
        , m_nMetadataType (type) {
    }
    AddMetadata(SamplePtr& sample, int type, const char* pData, int nData)
        : m_sample(sample)
        , m_metadata(pData, pData + nData)
        , m_nMetadataType (type) {
    }

    virtual mfxFrameSurface1 & GetSurface() {
        return m_sample->GetSurface();
    };
    virtual mfxAudioFrame & GetAudioFrame() {
        return m_sample->GetAudioFrame();
    }
    virtual mfxBitstream & GetBitstream() {
        return m_sample->GetBitstream();
    }
    virtual int GetSampleType() const {
        return m_sample->GetSampleType();
    }
    virtual int GetTrackID() const {
        return m_sample->GetTrackID();
    }
    virtual bool GetMetaData(int type, MetaData& data) const {
        if (HasMetaData(type)) {
            data = m_metadata;
            return true;
        }
        return m_sample->GetMetaData(type, data);
    }
    virtual bool HasMetaData( int type ) const{
        if (type == m_nMetadataType) {
            return true;
        }
        return false;
    }
protected:
    AddMetadata(int type, const MetaData &mdata)
        : m_sample(0)
        , m_metadata(mdata)
        , m_nMetadataType (type) {
    }
    AddMetadata(int type, const char* pData, int nData)
        : m_sample(0)
        , m_metadata(pData, pData + nData)
        , m_nMetadataType (type) {
    }
};

//rationale : sample without data
class MetaSample : public AddMetadata {
    int m_nTrackId;
public:
    MetaSample(int type, const MetaData &mdata, int nTrackId) :
        AddMetadata (type, mdata)
        , m_nTrackId(nTrackId) {
    }
    MetaSample(int type, const char* pData, int nData, int nTrackId) :
        AddMetadata (type, pData, nData)
        , m_nTrackId(nTrackId) {
    }

    virtual mfxFrameSurface1 & GetSurface() {
        MSDK_TRACE_ERROR(MSDK_STRING("MetaSample: GetSurface Unsupported"));
        throw GetSurfaceUnsupported();
    };
    virtual mfxAudioFrame & GetAudioFrame() {
        MSDK_TRACE_ERROR(MSDK_STRING("MetaSample: GetAudioFrame Unsupported"));
        throw GetBitstreamUnsupported();
    }
    virtual mfxBitstream & GetBitstream() {
        MSDK_TRACE_ERROR(MSDK_STRING("MetaSample: GetBitstream Unsupported"));
        throw GetBitstreamUnsupported();
    }
    virtual int GetSampleType() const {
        MSDK_TRACE_ERROR(MSDK_STRING("MetaSample: GetSampleType Unsupported"));
        throw GetSampleTypeUnsupported();
    }
    virtual int GetTrackID() const {
        return m_nTrackId;
    }
};
