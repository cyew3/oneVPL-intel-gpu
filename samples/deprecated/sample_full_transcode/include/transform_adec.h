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

#include "itransform.h"
#include "samples_pool.h"

class PipelineFactory;

template <>
class Transform<MFXAudioDECODE> : public ITransform, private no_copy {
public:
    Transform (PipelineFactory&, MFXAudioSession & ,int /*timeout*/ );
    //TODO: resolve workaround
    virtual void Configure(MFXAVParams& , ITransform *pNext);
    virtual void PutSample(SamplePtr&);
    virtual bool GetSample(SamplePtr&);
    virtual void GetNumSurfaces(MFXAVParams& , IAllocRequest& ){}
private:
    PipelineFactory& m_factory;
    MFXAudioSession& m_session;
    int m_dTimeToWait;
    std::auto_ptr<SamplePool> m_outSamples;
    std::auto_ptr<MFXAudioDECODE> m_pDEC;
    bool m_bEOS;
    bool m_bDrainSamplefSent;

    ITransform* m_pNextTransform;
    SamplePtr m_pInput;
    mfxAudioParam m_decParam;
    void InitDecode();
protected:
    bool m_bInited;

};

template <>
class NullTransform<MFXAudioDECODE> : public ITransform, private no_copy {
public:
    NullTransform (PipelineFactory&, MFXAudioSession & ,int  ){};
    virtual void Configure(MFXAVParams& , ITransform *){};
    virtual void PutSample(SamplePtr& sample) {
        m_sample = sample;
    }
    virtual bool GetSample(SamplePtr& sample) {
        if (!m_sample.get())
            return false;
        sample = m_sample;
        return true;
    }
    virtual void GetNumSurfaces(MFXAVParams& , IAllocRequest& ){}
protected:
    SamplePtr m_sample;
};