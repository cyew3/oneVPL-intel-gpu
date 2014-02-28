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
class Transform<MFXAudioENCODE> : public ITransform, private no_copy {
public:
    Transform (PipelineFactory&, MFXAudioSession & ,int /*timeout*/ );
    virtual void Configure(MFXAVParams& , ITransform *pNext);
    virtual void PutSample(SamplePtr&);
    virtual bool GetSample(SamplePtr&);
    virtual void GetNumSurfaces(MFXAVParams& param, IAllocRequest& request){
        param.GetAudioParam().mfx.CodecId = MFX_CODEC_AAC;
        mfxStatus sts = m_pENC->QueryIOSize(&param.GetAudioParam(), &request.Audio());
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioENCODE::QueryIOSize, sts=") << sts);
            throw EncodeQueryIOSizeError();
        }
    }
private:
    PipelineFactory& m_factory;
    MFXAudioSession& m_session;
    int m_dTimeToWait;
    bool m_bEOS;
    bool m_bNeedNewBitstream;
    mfxAudioAllocRequest m_allocRequest;

    SamplePtr m_pInput;
    std::auto_ptr<SamplePool> m_outSamples;
    std::auto_ptr<MFXAudioENCODE> m_pENC;
    mfxAudioParam m_encParam;
    SamplePtr m_pCurrentBitstream;

    void InitEncode();

protected:
    bool m_bInited;

};