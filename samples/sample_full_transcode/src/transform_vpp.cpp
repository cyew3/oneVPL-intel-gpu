/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "transform_vpp.h"
#include "pipeline_factory.h"
#include "mfxstructures.h"

Transform <MFXVideoVPP>::Transform( PipelineFactory& factory, MFXVideoSessionExt & session, int TimeToWait, const mfxPluginUID & uid) :
         m_session(session)
        ,m_factory(factory)
        ,m_dTimeToWait(TimeToWait)
        ,m_pVPP(m_factory.CreateVideoVPP(m_session))
        ,m_pInputSurface(0)
        ,m_pSamplesSurfPool(factory.CreateSamplePool(TimeToWait))
        ,m_bEOS(false)
        ,m_nTrackId(0)
{
    m_bInited = false;
    m_nFramesForNextTransform = 0;
    MSDK_MEMCPY(&m_uid, &uid, sizeof(mfxPluginUID));
    if (!AreGuidsEqual(m_uid, MSDK_PLUGINGUID_NULL))
    {
        mfxStatus sts = MFXVideoUSER_Load(m_session, &uid, 1);
        if (MFX_ERR_NONE != sts)
        {
            MSDK_TRACE_INFO(MSDK_STRING("MFXVideoUSER_Load(session=0x") << m_session << MSDK_STRING("), sts=") << sts);
        }
    }
}

Transform <MFXVideoVPP>::~Transform()
{
    if (!AreGuidsEqual(m_uid, MSDK_PLUGINGUID_NULL) && m_session)
    {
        mfxStatus sts = MFXVideoUSER_UnLoad(m_session, &m_uid);
        if (sts != MFX_ERR_NONE)
        {
            MSDK_TRACE_INFO(MSDK_STRING("MFXVideoUSER_UnLoad(session=0x") << m_session << MSDK_STRING("), sts=") << sts);
        }
    }
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
    m_bEOS |= sample->HasMetaData(META_EOS);
    InitVPP(sample);
    m_pInputSurface = m_bEOS? 0: &sample->GetSurface();
    sample.reset();
}

void Transform <MFXVideoVPP>::InitVPP(SamplePtr& sample) {
    if (m_bInited)
        return;

    m_nTrackId = sample->GetTrackID();

    mfxU16 oldWidth = 0, oldHeight = 0;
    oldWidth = m_initVideoParam.vpp.Out.Width;
    oldHeight = m_initVideoParam.vpp.Out.Height;
    m_initVideoParam.vpp.In = m_initVideoParam.vpp.Out = sample->GetSurface().Info;

    m_initVideoParam.vpp.Out.Width = m_initVideoParam.vpp.Out.CropW = oldWidth;
    m_initVideoParam.vpp.Out.Height = m_initVideoParam.vpp.Out.CropH = oldHeight;
    m_initVideoParam.IOPattern = (mfxU16)MFX_IOPATTERN_IN_SYSTEM_MEMORY | (mfxU16)MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

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

    surf.Info = m_initVideoParam.vpp.Out;
    surf.Info.FourCC = MFX_FOURCC_NV12;

    for (int i = 0; i < allocResp.NumFrameActual; i++) {
        surf.Data.MemId = allocResp.mids[i];
        SamplePtr sampleToBuf(new SampleSurfaceWithData(surf, m_nTrackId));
        m_pSamplesSurfPool->RegisterSample(sampleToBuf);
    }
}

void Transform <MFXVideoVPP>::GetNumSurfaces(MFXAVParams& param, IAllocRequest& request)
{
    mfxVideoParam *srcParams = &param.GetVideoParam();

    mfxVideoParam tmpVPPParams;
    memset(&tmpVPPParams, 0, sizeof(mfxVideoParam));
    memcpy(&tmpVPPParams.vpp.In, &srcParams->mfx.FrameInfo, sizeof(mfxFrameInfo));
    memcpy(&tmpVPPParams.vpp.Out, &srcParams->mfx.FrameInfo, sizeof(mfxFrameInfo));

    if (tmpVPPParams.vpp.In.FrameRateExtN == 0 || tmpVPPParams.vpp.In.FrameRateExtD == 0)
    {
        tmpVPPParams.vpp.In.FrameRateExtN = tmpVPPParams.vpp.Out.FrameRateExtN = 30;
        tmpVPPParams.vpp.In.FrameRateExtD = tmpVPPParams.vpp.Out.FrameRateExtD = 1;
    }
    tmpVPPParams.vpp.Out.Width = tmpVPPParams.vpp.Out.CropW = m_initVideoParam.vpp.Out.Width;
    tmpVPPParams.vpp.Out.Height = tmpVPPParams.vpp.Out.CropH = m_initVideoParam.vpp.Out.Height;
    tmpVPPParams.vpp.Out.FourCC = MFX_FOURCC_NV12;
    tmpVPPParams.IOPattern = (mfxU16)MFX_IOPATTERN_IN_SYSTEM_MEMORY | (mfxU16)MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    mfxStatus sts = m_pVPP->QueryIOSurf(&tmpVPPParams, &request.Video());
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoVPP::QueryIOSurf, sts=") << sts);
        throw VPPQueryIOSurfError();
    }
}
