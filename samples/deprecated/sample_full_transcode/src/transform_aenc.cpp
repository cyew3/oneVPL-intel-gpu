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

#include"transform_aenc.h"
#include"pipeline_factory.h"


Transform <MFXAudioENCODE>::Transform(PipelineFactory& factory, MFXAudioSession & session, int TimeToWait)
    : m_factory(factory)
    , m_session(session)
    , m_dTimeToWait(TimeToWait)
    , m_bEOS()
    , m_bNeedNewBitstream(true)
    , m_allocRequest(mfxAudioAllocRequest()) {

    m_pENC.reset(m_factory.CreateAudioEncoder(session));
    m_outSamples.reset(m_factory.CreateSamplePool(TimeToWait));
    m_bInited = false;
}

void Transform <MFXAudioENCODE>::Configure(MFXAVParams& param, ITransform *) {
    m_encParam = param;
}

void Transform <MFXAudioENCODE>::PutSample( SamplePtr& sample) {
    m_pInput = sample;
    m_bEOS |=  m_pInput->HasMetaData(META_EOS);
    InitEncode();
}

bool Transform <MFXAudioENCODE>::GetSample( SamplePtr& sample) {
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

    mfxAudioFrame * pIn = m_bEOS ? NULL : &m_pInput->GetAudioFrame();

    for (int i = 0, WaitPortion = 10 ; i <= m_dTimeToWait; ) {
        mfxSyncPoint syncp;
        if (m_bNeedNewBitstream) {
            if (!m_outSamples->HasFreeSample()) {
                SamplePtr sample_ptr(new SampleBitstream(m_allocRequest.SuggestedOutputSize+8, m_pInput->GetTrackID()));
                m_outSamples->RegisterSample(sample_ptr);
            }

            m_pCurrentBitstream.reset(m_outSamples->LockSample(m_outSamples->FindFreeSample()));
            m_pCurrentBitstream->GetBitstream().DataLength = 0;
            m_pCurrentBitstream->GetBitstream().DataOffset = 0;
        }
        mfxStatus sts = m_pENC->EncodeFrameAsync(pIn, &m_pCurrentBitstream->GetBitstream() , &syncp);
        if (sts > 0 && syncp) {
            MSDK_TRACE_INFO(MSDK_STRING("MFXAudioENCODE::EncodeFrameAsync, sts=") << sts);
            sts = MFX_ERR_NONE;
        }
        switch(sts) {
            case MFX_ERR_MORE_BITSTREAM:
            case MFX_ERR_NONE: {
                if (!sts)
                    m_pInput.reset(0);
                sts = m_session.SyncOperation(syncp, m_dTimeToWait);
                if (sts < 0) {
                    MSDK_TRACE_ERROR(MSDK_STRING("MFXSession::SyncOperation(audio_encode), sts=") << sts);
                    throw AudioEncodeSyncOperationError();
                }

                sample.reset(m_pCurrentBitstream.release());
                m_bNeedNewBitstream = true;
                return true;
            }
            case MFX_ERR_MORE_DATA:
                //input no longer needed
                m_pInput.reset(0);
                m_bNeedNewBitstream = false;
                return false;
            case MFX_WRN_DEVICE_BUSY:
                MSDK_SLEEP(WaitPortion);
                i += WaitPortion;
                continue;
            default: {
                MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioENCODE::EncodeFrameAsync(), sts=") << sts);
                throw EncodeFrameAsyncError();
            }
        }
    }
    MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioENCODE::EncodeFrameAsync device busy"));
    throw EncodeTimeoutError();
}

void Transform <MFXAudioENCODE>::InitEncode() {
    if (m_bInited)
        return;

    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pENC->Query(&m_encParam, &m_encParam);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioENCODE::Query, sts=")<<sts);
        throw EncodeQueryError();
    }

    sts = m_pENC->QueryIOSize(&m_encParam, &m_allocRequest);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioENCODE::QueryIOSize, sts=")<<sts);
        throw EncodeQueryIOSizeError();
    }

    sts = m_pENC->Init(&m_encParam);

    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXAudioENCODE::Init, sts=")<<sts);
        throw EncodeInitError();
    }
    m_bInited = true;
}

