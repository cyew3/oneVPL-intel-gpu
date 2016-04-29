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

#include "transform_vpp.h"
#include "pipeline_factory.h"
#include "mfxstructures.h"

Transform <MFXVideoVPP>::Transform( PipelineFactory& factory, MFXVideoSessionExt & session, int TimeToWait ) :
    m_session(session)
    ,m_factory(factory)
    ,m_dTimeToWait(TimeToWait)
    ,m_pVPP(m_factory.CreateVideoVPP(m_session))
    ,m_pInputSurface(0)
    ,m_pSamplesSurfPool(factory.CreateSamplePool(TimeToWait)) {
        m_bInited = false;
        m_nFramesForNextTransform = 0;

}

void Transform <MFXVideoVPP>::Configure(MFXAVParams& param, ITransform * nextTransform) {
    m_initVideoParam = param;
    MFXAVAllocRequest<mfxFrameAllocRequest> request;
    nextTransform->GetNumSurfaces(param, request);
    m_nFramesForNextTransform = request.Video().NumFrameSuggested;
}

bool Transform <MFXVideoVPP>::GetSample( SamplePtr& sample) {
    if (!m_bInited || !m_pInputSurface) {
        return false;
    }

    mfxFrameSurface1 *outSurf = &m_pSamplesSurfPool->FindFreeSample().GetSurface();

    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    for (int i = 0, WaitPortion = 10 ; i <= m_dTimeToWait; ) {
        mfxSyncPoint syncp;

        sts = m_pVPP->RunFrameVPPAsync(m_pInputSurface, outSurf, NULL, &syncp);
        if (sts > 0 && syncp)
            sts = MFX_ERR_NONE;

        switch(sts) {
        case MFX_ERR_NONE:
            m_pInputSurface = 0;
        case MFX_ERR_MORE_SURFACE:
            sample.reset(m_pSamplesSurfPool->LockSample(SampleSurface1(*outSurf, 0)));
            return true;
        case MFX_WRN_DEVICE_BUSY:
            MSDK_SLEEP(WaitPortion);
            i += WaitPortion;
            continue;
        default:
            MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::RunFrameVPPAsync, sts=") << sts);
            throw VPPRunFrameVPPAsyncError();
        }
    }
    MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::RunFrameVPPAsync device busy") << sts);
    throw VPPTimeoutError();
}

void Transform <MFXVideoVPP>::PutSample(SamplePtr& sample) {
    InitVPP();
    m_pInputSurface = &sample->GetSurface();
    sample.reset();
}

void Transform <MFXVideoVPP>::InitVPP() {
    if (m_bInited)
        return;

    mfxStatus sts = m_pVPP->Init(&m_initVideoParam);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::Init, sts=") << sts);
        throw VPPInitError();
    }
    AllocFrames();
    m_bInited = 1;
}

void Transform <MFXVideoVPP>::AllocFrames() {
    MFXAVAllocRequest<mfxFrameAllocRequest> allocReq;
    mfxFrameAllocResponse allocResp = {};
    MFXAVParams tmpVppParam(m_initVideoParam);
    GetNumSurfaces(tmpVppParam, allocReq);

    //alloc frames with an eye to next transform
    // If surfaces are shared by 2 components, c1 and c2. NumSurf = c1_out + c2_in - AsyncDepth + 1
    // WA: added 1 extra surface for SFT only
    allocReq.Video().NumFrameSuggested = allocReq.Video().NumFrameSuggested + m_nFramesForNextTransform - m_initVideoParam.AsyncDepth + 2;
    allocReq.Video().NumFrameMin = allocReq.Video().NumFrameSuggested;
    mfxFrameAllocator *allocator = 0;

    mfxStatus sts = m_session.GetFrameAllocator(allocator);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::GetFrameAllocator, sts=") << sts);
        throw VPPGetFrameAllocatorError();
    }

    sts = allocator->Alloc(allocator->pthis, &allocReq.Video(), &allocResp);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::Alloc, sts=") << sts);
        throw VPPAllocError();
    }
    mfxFrameSurface1 surf;
    memset((void*)&surf, 0, sizeof(mfxFrameSurface1));

    m_SurfArray.resize(allocResp.NumFrameActual, surf);

    for (int i = 0; i < allocResp.NumFrameActual; i++) {
        m_SurfArray[i].Data.MemId = allocResp.mids[i];
    /*    SamplePtr sample(new DataSample(m_SurfArray[i]));
        m_pSamplesSurfPool->RegisterSample(sample);*/
    }
}

void Transform <MFXVideoVPP>::GetNumSurfaces(MFXAVParams& param, IAllocRequest& request)
{
    mfxStatus sts = m_pVPP->QueryIOSurf(&param.GetVideoParam(), &request.Video());
    if ((sts < 0) || (request.Video().NumFrameSuggested < param.GetVideoParam().AsyncDepth)) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::QueryIOSurf, sts=") << sts);
        throw VPPQueryIOSurfError();
    }
}
