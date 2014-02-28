//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
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
    allocReq.Video().NumFrameMin = allocReq.Video().NumFrameMin + m_nFramesForNextTransform;
    allocReq.Video().NumFrameSuggested = allocReq.Video().NumFrameSuggested + m_nFramesForNextTransform;
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
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::QueryIOSurf, sts=") << sts);
        throw VPPQueryIOSurfError();
    }
}
