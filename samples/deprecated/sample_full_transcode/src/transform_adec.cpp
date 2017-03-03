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

