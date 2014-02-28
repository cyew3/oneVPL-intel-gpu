//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <memory>
#include <algorithm>
#include <functional>
#include "isample.h"
#include "exceptions.h"

class SamplePool
{
protected:
    class DataSampleWithRef : private no_copy {
        unsigned int            m_lock;
        SamplePtr  m_psample;
    public:
        DataSampleWithRef(SamplePtr& sample) : 
            m_lock(0),
            m_psample ( sample) {
        }
        virtual ~DataSampleWithRef(){};
        virtual ISample * Get(){
            return m_psample.get();
        }
        virtual void AddRef(){
            m_lock++;
        }
        virtual void Release(){
            m_lock--;
        }
        virtual bool isUnlocked() {
            switch(m_psample->GetSampleType()) {
                case SAMPLE_BITSTREAM: 
                    return !m_lock;
                case SAMPLE_SURFACE: 
                    return !m_lock && !m_psample->GetSurface().Data.Locked;
                case SAMPLE_AUDIO_FRAME: 
                    return !m_lock && !m_psample->GetAudioFrame().Locked;
                default:
                    return false;
            }
        }
    };

    class DataSampleToReturn : public ISample
    {
        DataSampleWithRef *  m_psample;
    public:
        DataSampleToReturn(DataSampleWithRef *sample) : 
            m_psample(sample) {
            m_psample->AddRef();
        }
        ~DataSampleToReturn() {
            m_psample->Release();
        }
        virtual mfxAudioFrame& GetAudioFrame(){
            return m_psample->Get()->GetAudioFrame();
        }
        virtual mfxBitstream& GetBitstream(){
            return m_psample->Get()->GetBitstream();
        }
        virtual mfxFrameSurface1& GetSurface(){
            return m_psample->Get()->GetSurface(); 
        }        
        virtual int GetSampleType() const {
            return m_psample->Get()->GetSampleType();
        }
        virtual int GetTrackID() const {
            return m_psample->Get()->GetTrackID();
        }
        virtual bool GetMetaData(int type, MetaData& data) const  {
            return m_psample->Get()->GetMetaData(type, data);
        }
        virtual bool HasMetaData(int type) const  {
            return m_psample->Get()->HasMetaData(type);
        }
    };

    int m_nTimeout;
    std::vector<DataSampleWithRef* > m_pool;
public:
    SamplePool(int FindFreeTimeout) : m_nTimeout(FindFreeTimeout){
    }

    virtual ~SamplePool() {     
        std::transform(m_pool.begin(), m_pool.end(), m_pool.begin(), DeletePtr());
    }
    //returns locked sample
    virtual ISample * LockSample(const ISample& sample) {
        
        for (std::vector<DataSampleWithRef* > ::iterator i = m_pool.begin(); i != m_pool.end(); i++) {
            switch (CompareSamples(*(*i)->Get(), sample)) {
                case SAMPLES_HAVE_INCOPARABLE_TYPES :
                    MSDK_TRACE_ERROR(MSDK_STRING("Incompatible sample type"));
                    throw IncompatibleSampleTypeError();
                case SAMPLES_REFERENCES_DIFFERENT:
                    continue;
                default:
                    break;
            }
            return new DataSampleToReturn(*i);
        }
        MSDK_TRACE_ERROR(MSDK_STRING("Element not found in SamplePool"));
        throw SamplePoolLockInvalidSample();
    }

    virtual ISample & FindFreeSample() {
        for (int overalTimeout = 0, WaitPortion = 10; m_nTimeout >= overalTimeout; overalTimeout += WaitPortion) {
           std::vector<DataSampleWithRef* > ::iterator i = 
               std::find_if(m_pool.begin(), m_pool.end(), std::mem_fun(&DataSampleWithRef::isUnlocked));
         
           if (i != m_pool.end()) {
               return *(*i)->Get();
           }
           MSDK_SLEEP(WaitPortion);
       }       
       MSDK_TRACE_ERROR(MSDK_STRING("No free samples in SamplePool"));
       throw NoFreeSampleError();      
    };

    virtual bool HasFreeSample() {
        std::vector<DataSampleWithRef* > ::iterator i = 
               std::find_if(m_pool.begin(), m_pool.end(), std::mem_fun(&DataSampleWithRef::isUnlocked));
        if (i != m_pool.end()) {
               return true;
        }
        return false;
    }

    //takes sample ownership
    virtual void RegisterSample(SamplePtr& sample){
        m_pool.push_back(new DataSampleWithRef(sample));
    };
};

