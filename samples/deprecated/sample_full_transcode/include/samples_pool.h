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

