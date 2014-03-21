/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

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

