/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include"transform_adec.h"
#include"pipeline_factory.h"
#include <algorithm>


Transform <MFXAudioDECODE>::Transform(PipelineFactory& factory, MFXAudioSession & session, int TimeToWait)
    : m_factory(factory)
    , m_session(session)
    , m_dTimeToWait(TimeToWait)
    , m_outSamples(factory.CreateSamplePool(TimeToWait))
    , m_pDEC(factory.CreateAudioDecoder(session))
    , m_bEOS(false)
    , m_bDrainSamplefSent(false) {
        m_bInited = false;
}

void Transform <MFXAudioDECODE>::Configure(MFXAVParams& param, ITransform * transform) {
    m_decParam = param;
    m_pNextTransform = transform;
}

void Transform <MFXAudioDECODE>::PutSample( SamplePtr& sample) {
    m_pInput = sample;
    m_bEOS = m_pInput->HasMetaData(META_EOS);
    InitDecode();
}

bool Transform <MFXAudioDECODE>::GetSample( SamplePtr& sample) {
    if (!m_bInited) {
        return false;
    }
    if (!m_pInput.get()) {
        return false;
    }
    if (m_pInput->HasMetaData(META_EOS_RESET)) {
        sample = m_pInput;
        return true;
    }
    mfxBitstream * pIn = m_bEOS ? NULL : &m_pInput->GetBitstream();

    for (int i = 0, WaitPortion = 10 ; i <= m_dTimeToWait; ) {
        mfxSyncPoint syncp;
        ISample &pOut = m_outSamples->FindFreeSample();

        pOut.GetAudioFrame().DataLength = 0;

        mfxStatus sts = m_pDEC->DecodeFrameAsync(pIn, &pOut.GetAudioFrame() , &syncp);
        if (sts > 0 && syncp) {
            MSDK_TRACE_INFO(MSDK_STRING("MFXAudioDECODE::DecodeFrameAsync, sts=") << sts);
            sts = MFX_ERR_NONE;
        }
        switch(sts) {
            case MFX_ERR_NONE:
                sample.reset(m_outSamples->LockSample(pOut));
                m_pInput.reset(0);
                return true;
            case MFX_ERR_MORE_DATA:
                if (!pIn & !m_bDrainSamplefSent) {
                    sample.reset(new MetaSample(META_EOS, 0, 0, m_pInput->GetTrackID()));
                    m_bDrainSamplefSent = true;
                    return true;
                }
                return false;
            case MFX_WRN_DEVICE_BUSY:
                MSDK_SLEEP(WaitPortion);
                i += WaitPortion;
                continue;
            default: {
                MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioDECODE::DecodeFrameAsync(), sts=") << sts);
                throw DecodeFrameAsyncError();
            }
        }
    }
    MSDK_TRACE_ERROR(MSDK_STRING("DecodeFrameAsync Timeout Error"));
    throw DecodeTimeoutError();
}

void Transform <MFXAudioDECODE>::InitDecode() {
    if (m_bInited)
        return;

    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pDEC->DecodeHeader(&m_pInput->GetBitstream(), &m_decParam);
    if (sts < 0){
        MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioDECODE::DecodeHeader, sts=") << sts);
        throw DecodeHeaderError();
    }

    sts = m_pDEC->Query(&m_decParam, &m_decParam);
    if (sts < 0){
        MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioDECODE::Query, sts=") << sts);
        throw DecodeQueryError();
    }

    mfxAudioAllocRequest allocRequest = {};
    sts = m_pDEC->QueryIOSize(&m_decParam, &allocRequest);
    if (sts < 0){
        MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioDECODE::QueryIOsize, sts=") << sts);
        throw DecodeQueryIOSizeError();
    }

    MFXAVAllocRequest<mfxAudioAllocRequest> aRequest;
    MFXAVParams decParam(m_decParam);
    m_pNextTransform->GetNumSurfaces(decParam, aRequest);
    int nFramesToAlloc = std::max(aRequest.Audio().SuggestedInputSize, m_pInput->GetBitstream().DataLength) /
        std::min(aRequest.Audio().SuggestedInputSize, m_pInput->GetBitstream().DataLength);
    for (int i = 0; i < nFramesToAlloc; i++) {
        std::auto_ptr<ISample> outSample(new SampleAudioFrameWithData(allocRequest.SuggestedOutputSize, m_pInput->GetTrackID()));
        m_outSamples->RegisterSample(outSample);
    }

    sts = m_pDEC->Init(&m_decParam);

    if (sts < 0){
        MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioDECODE::Init, sts=") << sts);
        throw DecodeInitError();
    }

    m_bInited = true;
}

