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

#include "transform_venc.h"
#include "exceptions.h"
#include "pipeline_factory.h"
#include "sample_defs.h"
#include "sample_metadata.h"

#include <iostream>

Transform <MFXVideoENCODE>::Transform( PipelineFactory& factory, MFXVideoSessionExt & session, int TimeToWait ) :
      m_session(session)
    , m_factory(factory)
    , m_dTimeToWait(TimeToWait)
    , m_bInited(false)
    , m_bCreateSPS(true)
    , m_bDrainSampleSent(false)
    , m_bEOS(false)
    , m_bDrainComplete()
    , m_nTrackId(0) {
    m_pENC.reset(m_factory.CreateVideoEncoder(m_session));
    m_BitstreamPool.reset(m_factory.CreateSamplePool(TimeToWait));
}

void Transform <MFXVideoENCODE>::Configure(MFXAVParams& param, ITransform *) {
    m_initVideoParam = param;
    m_initVideoParam.mfx.GopPicSize = 30;
    m_initVideoParam.mfx.GopRefDist = 3;
    m_initVideoParam.mfx.IdrInterval = 0;
}

void Transform <MFXVideoENCODE>::MakeSPSPPS( std::vector<char>& spspps) {
    if (!m_bCreateSPS)
        return;
    m_Sps.resize(128,0);
    m_Pps.resize(128,0);

    mfxExtCodingOptionSPSPPS extSPSPPS;
    MSDK_ZERO_MEMORY(extSPSPPS);
    extSPSPPS.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
    extSPSPPS.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
    extSPSPPS.PPSBufSize = (mfxU16)m_Sps.size();
    extSPSPPS.SPSBufSize = (mfxU16)m_Pps.size();
    extSPSPPS.PPSBuffer = &m_Sps.front();
    extSPSPPS.SPSBuffer = &m_Pps.front();

    mfxExtBuffer* encExtParams[1];
    mfxVideoParam par={};
    encExtParams[0] = (mfxExtBuffer *)&extSPSPPS;
    par.ExtParam = &encExtParams[0];
    par.NumExtParam = 1;
    for (;;) {
        mfxStatus sts = m_pENC->GetVideoParam(&par);
        if (sts == MFX_ERR_NONE)
            break;
        if (sts == MFX_ERR_NOT_ENOUGH_BUFFER) {
            extSPSPPS.PPSBufSize = (mfxU16)m_Pps.size()*2;
            extSPSPPS.SPSBufSize = (mfxU16)m_Sps.size()*2;
            m_Sps.resize(extSPSPPS.SPSBufSize);
            m_Pps.resize(extSPSPPS.PPSBufSize);
            continue;
        } else {
            MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::GetVideoParam with mfxExtCodingOptionSPSPPS, sts=") << sts);
            throw EncodeGetVideoParamError();
        }
    }

    spspps.resize(extSPSPPS.SPSBufSize + extSPSPPS.PPSBufSize, 0);
    MSDK_MEMCPY(&spspps.front(), extSPSPPS.SPSBuffer, extSPSPPS.SPSBufSize);
    MSDK_MEMCPY(&spspps.front() + extSPSPPS.SPSBufSize, extSPSPPS.PPSBuffer, extSPSPPS.PPSBufSize);

    m_bCreateSPS = false;
}

bool Transform <MFXVideoENCODE>::GetSample( SamplePtr& sample) {
    if (!m_bInited) {
        return false;
    }

    if (m_ExtBitstreamQueue.empty()) {
        if (m_bDrainSampleSent)
            return false;

        m_bDrainSampleSent = true;
        //TODO: remove call to findfree surface
        sample.reset(new MetaSample(META_EOS, 0, 0, m_nTrackId));
        return true;
    }

    //submit if we have a room
    if (m_bEOS && !m_bDrainComplete && m_ExtBitstreamQueue.size() < m_initVideoParam.AsyncDepth) {
        SubmitEncodingWorkload(0);
    }

    mfxStatus sts = MFX_ERR_NONE;

    //wait for timeout in case of eos or not enough bitstreams
    int nWait = m_bEOS || (m_ExtBitstreamQueue.size() == m_initVideoParam.AsyncDepth) ? m_dTimeToWait : 0;

    sts = m_session.SyncOperation(m_ExtBitstreamQueue.front().second, nWait);

    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::SyncOperation(), sts=") << sts);
        throw EncodeSyncOperationError();
    }

    if (MFX_WRN_IN_EXECUTION == sts) {
        if (nWait) {
            MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::SyncOperation(), sts=MFX_WRN_IN_EXECUTION"));
            throw EncodeSyncOperationTimeout();
        }
        return false;
    }
    if (sts > 0) {
        MSDK_TRACE_DEBUG(MSDK_STRING("MFXVideoENCODE::SyncOperation(), sts=")<<sts);
    }

    MSDK_TRACE_DEBUG(MSDK_STRING("MFXVideoENCODE::SyncOperation(), pts=")
        << m_ExtBitstreamQueue.front().first->GetBitstream().TimeStamp << MSDK_STRING(", dts=")
        << m_ExtBitstreamQueue.front().first->GetBitstream().DecodeTimeStamp );


    sample.reset(m_ExtBitstreamQueue.front().first);

    if (m_bCreateSPS) {
        MakeSPSPPS(m_SpsPps);
        sample.reset(new AddMetadata(sample, META_SPSPPS, m_SpsPps));
        sample.reset(new AddMetadata(sample, META_VIDEOPARAM, (char*)&m_initVideoParam, sizeof(m_initVideoParam)));
    }

    m_ExtBitstreamQueue.pop();
    return true;
}

void Transform <MFXVideoENCODE>::PutSample(SamplePtr& sample) {
    m_bEOS |= sample->HasMetaData(META_EOS);

    InitEncode(sample);

    return SubmitEncodingWorkload(m_bEOS? 0: &sample->GetSurface());
}

void Transform <MFXVideoENCODE>::InitEncode(SamplePtr& sample) {
    if (m_bInited)
        return;

    m_nTrackId = sample->GetTrackID();

    mfxFrameInfo &sInfo = sample->GetSurface().Info;
    mfxFrameInfo &iInfo = m_initVideoParam.mfx.FrameInfo;

    //frame info taken from input surface
    iInfo = sInfo;

    //TODO: add query call
    mfxStatus sts = m_pENC->Query(&m_initVideoParam, &m_initVideoParam);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::Query, sts=") << sts);
        throw EncodeQueryError();
    }

    sts = m_pENC->Init(&m_initVideoParam);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::Init, sts=") << sts);
        throw EncodeInitError();
    }
    //TODO: replace by internal method
    MFXAVAllocRequest<mfxFrameAllocRequest> request;
    MFXAVParams initVideoParam(m_initVideoParam);
    GetNumSurfaces(initVideoParam, request);

    mfxVideoParam vparam = {};
    sts = m_pENC->GetVideoParam(&vparam);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::GetVideoParam, sts=") << sts);
        throw EncodeGetVideoParamError();
    }
    //TODO: get asyncdepth from queryiosurface call
    m_initVideoParam.AsyncDepth = request.Video().NumFrameSuggested;

    if (!vparam.mfx.BufferSizeInKB) {
        MSDK_TRACE_ERROR(MSDK_STRING("BufferSizeInKB=0"));
        throw NullPointerError();
    }
    //TODO: [business critical] add overflow u16 test
    mfxU32 bits_MaxLength = vparam.mfx.BufferSizeInKB * 1000 *
        (0 == vparam.mfx.BRCParamMultiplier ? 1 : vparam.mfx.BRCParamMultiplier);

    // add one bitstream to process buffered frames correctly within existing architecture
    for (int i = 0; i < m_initVideoParam.AsyncDepth + 1; i++) {
        SamplePtr sampleOut(new SampleBitstream(bits_MaxLength, sample->GetTrackID()));
        m_BitstreamPool->RegisterSample(sampleOut);
    }
    m_bInited = true;
}

void Transform <MFXVideoENCODE>::SubmitEncodingWorkload(mfxFrameSurface1 * surface)
{
    SamplePtr sampleBitstream;
    mfxSyncPoint syncp = 0;
    for (int i = 0, waitPortion = 10 ; i <= m_dTimeToWait; ) {
        if (!sampleBitstream.get()) {
            sampleBitstream.reset(m_BitstreamPool->LockSample(m_BitstreamPool->FindFreeSample()));
        }
        sampleBitstream->GetBitstream().DataLength = 0;
        sampleBitstream->GetBitstream().DataOffset = 0;

        if (surface) {
            MSDK_TRACE_DEBUG(MSDK_STRING("MFXVideoENCODE::EncodeFrameAsync(), pts=") << surface->Data.TimeStamp);
        }

        mfxStatus sts = m_pENC->EncodeFrameAsync(0, surface, &sampleBitstream->GetBitstream(), &syncp);
        if (sts > 0 && sts != MFX_WRN_DEVICE_BUSY) {
            MSDK_TRACE_DEBUG(MSDK_STRING("MFXVideoENCODE::EncodeFrameAsync(), sts=") << sts);
        }
        if (sts > 0 && syncp)
            sts = MFX_ERR_NONE;

        switch(sts) {
        case MFX_ERR_MORE_BITSTREAM:
            m_ExtBitstreamQueue.push(std::make_pair(sampleBitstream.release(), syncp));
            break;
        case MFX_ERR_NONE:
            m_ExtBitstreamQueue.push(std::make_pair(sampleBitstream.release(), syncp));
            return;
        case MFX_ERR_MORE_DATA:
            m_bDrainComplete |= 0 == surface ;
            return;
        case MFX_WRN_DEVICE_BUSY:
            MSDK_SLEEP(waitPortion);
            i += waitPortion;
            continue;
        default:
            MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::EncodeFrameAsync(), sts=") << sts);
            throw EncodeFrameAsyncError();
        }
        if(MFX_ERR_NONE == sts) {
            return;
        }
    }
    MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE device busy"));
    throw EncodeTimeoutError();
}

void Transform <MFXVideoENCODE>::GetNumSurfaces(MFXAVParams& param, IAllocRequest& request) {
    mfxVideoParam Param = param;
    Param.mfx.CodecId = m_initVideoParam.mfx.CodecId;
    Param.mfx.CodecProfile = m_initVideoParam.mfx.CodecProfile;
    Param.mfx.CodecLevel = m_initVideoParam.mfx.CodecLevel;
    Param.IOPattern = Param.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        ? (mfxU16)MFX_IOPATTERN_IN_SYSTEM_MEMORY
        : (mfxU16)MFX_IOPATTERN_IN_VIDEO_MEMORY;
    mfxStatus sts = m_pENC->QueryIOSurf(&Param, &request.Video());
    if ((sts < 0) || (request.Video().NumFrameSuggested < Param.AsyncDepth)) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoENCODE::QueryIOSurf, sts=") << sts);
        throw EncodeQueryIOSurfError();
    }
}

