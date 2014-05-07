//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

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