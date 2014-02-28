//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

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
