/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include "transform_vdec.h"
#include "exceptions.h"
#include "pipeline_factory.h"
#include "sample_defs.h"
#include "d3d11_device.h"
#include "d3d_device.h"
#include <iostream>

Transform <MFXVideoDECODE>::Transform(PipelineFactory& factory, MFXVideoSessionExt & session, int TimeToWait)
        : m_factory(factory)
        , m_session(session)
        , m_pNextTransform(0)
        , m_dTimeToWait(TimeToWait)
        , m_bDrainSamplefSent(false)
        , m_bEOS(false)
        , mBshouldLoad(true){
    m_pDEC.reset(m_factory.CreateVideoDecoder(session));
    m_pSamplesSurfPool.reset(m_factory.CreateSamplePool(TimeToWait));
    m_bInited = false;
    m_nFramesForNextTransform = 0;
}

void Transform <MFXVideoDECODE>::Configure(MFXAVParams& param, ITransform * nextTransform) {
    m_decParam = param;
    m_pNextTransform = nextTransform;
}

void Transform <MFXVideoDECODE>::PutSample( SamplePtr& sample) {
    m_pInput = sample;
    InitDecode(m_pInput);
}

bool Transform <MFXVideoDECODE>::GetSample( SamplePtr& sample) {
    if (!m_bInited) {
        return false;
    }
    mfxFrameSurface1* outSurf = 0;
    mfxFrameSurface1* workSurf = &m_pSamplesSurfPool->FindFreeSample().GetSurface();
    mfxBitstream *pBs = m_pInput->HasMetaData(META_EOS) ? 0 : &m_pInput->GetBitstream();
#ifdef SPL_NON_COMPLETE_FRAME_WA
    if (pBs && mBshouldLoad){
        m_dataVec.insert(m_dataVec.end(), pBs->Data + pBs->DataOffset, pBs->Data + pBs->DataOffset + pBs->DataLength );
        if (!m_dataVec.empty()) {
            pBs->Data = &m_dataVec.front();
            pBs->DataOffset = 0;
            pBs->DataLength = (mfxU32)m_dataVec.size();
            pBs->MaxLength = pBs->DataLength;
        }
        mBshouldLoad = false;
    }
#endif
    for (int i = 0, WaitPortion = 10 ; i <= m_dTimeToWait; ) {
        mfxSyncPoint syncp;
        mfxStatus sts =  m_pDEC->DecodeFrameAsync(pBs, workSurf, &outSurf, &syncp);
        if (sts > 0 && syncp) {
            MSDK_TRACE_INFO(MSDK_STRING("m_pDEC->DecodeFrameAsync, sts=") << sts);
            sts = MFX_ERR_NONE;
        }
#ifdef SPL_NON_COMPLETE_FRAME_WA
        if (pBs) {
            m_dataVec.erase(m_dataVec.begin(), m_dataVec.begin() + pBs->DataOffset);
            if (!m_dataVec.empty()) {
                pBs->Data = &m_dataVec.front();
                pBs->DataLength = (mfxU32)m_dataVec.size();
            }
            pBs->DataOffset = 0;
        }
#endif
        switch(sts) {
        case MFX_ERR_NONE:
            sample.reset(m_pSamplesSurfPool->LockSample(SampleSurface1(*outSurf, m_pInput->GetTrackID())));
            return true;
        case MFX_ERR_MORE_SURFACE:
            workSurf = &m_pSamplesSurfPool->FindFreeSample().GetSurface();
            break;
        case MFX_ERR_MORE_DATA:
            if (!pBs & !m_bDrainSamplefSent) {
                sample.reset(new MetaSample(META_EOS, 0, 0, m_pInput->GetTrackID()));
                m_bDrainSamplefSent = true;
                return true;
            }
            mBshouldLoad = true;
            return false;
        case MFX_WRN_DEVICE_BUSY:
            MSDK_SLEEP(WaitPortion);
            i += WaitPortion;
            continue;
        case MFX_WRN_VIDEO_PARAM_CHANGED:
            continue;
        default:
            MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoDECODE::DecodeFrameAsync sts=") << sts);
            throw DecodeFrameAsyncError();
        }
    }
    MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoDECODE::DecodeFrameAsync device busy"));
    throw DecodeTimeoutError();
}

void Transform <MFXVideoDECODE>::InitDecode(SamplePtr& sample) {
    if (m_bInited)
        return;

    mfxStatus sts = MFX_ERR_NONE;
    m_decParam.mfx.CodecId = m_decParam.mfx.CodecId;

    sts = m_pDEC->DecodeHeader(&m_pInput->GetBitstream(), &m_decParam);
    if (sts == MFX_ERR_MORE_DATA)
        return;
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoDECODE::DecodeHeader, sts=") << sts);
        if (sts == MFX_ERR_MORE_DATA)
            return;
        throw DecodeHeaderError();
    }

    AllocFrames(sample);

    sts = m_pDEC->Init(&m_decParam);

    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoDECODE::Init, sts=") << sts);
        throw DecodeInitError();
    }

    m_bInited = true;
}

void Transform <MFXVideoDECODE>::AllocFrames(SamplePtr& sample) {
    std::auto_ptr<BaseFrameAllocator>& m_pAllocator = m_session.GetAllocator();
    mfxFrameAllocRequest allocReq;
    mfxFrameAllocResponse allocResp;

    mfxStatus sts = m_pDEC->QueryIOSurf(&m_decParam, &allocReq);
    if ((sts < 0) || (allocReq.NumFrameSuggested < m_decParam.AsyncDepth)) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoDECODE::QueryIOSurf, sts=") << sts);
        throw DecodeQueryIOSurfaceError();
    }

    //
    //alloc frames with an eye to the next transform
    MFXAVAllocRequest<mfxFrameAllocRequest> nextTransformRequest;
    MFXAVParams tmpDecParam(m_decParam);
    m_pNextTransform->GetNumSurfaces(tmpDecParam, nextTransformRequest);
    // If surfaces are shared by 2 components, c1 and c2. NumSurf = c1_out + c2_in - AsyncDepth + 1
    allocReq.NumFrameSuggested = allocReq.NumFrameSuggested + nextTransformRequest.Video().NumFrameSuggested - m_decParam.AsyncDepth + 1;
    allocReq.NumFrameMin = allocReq.NumFrameSuggested;
    allocReq.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
    sts = m_pAllocator->AllocFrames(&allocReq, &allocResp);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXFrameAllocator::AllocFrames, sts=") << sts);
        throw DecodeAllocError();
    }

    mfxFrameSurface1 surf;
    memset((void*)&surf, 0, sizeof(mfxFrameSurface1));

    surf.Info = m_decParam.mfx.FrameInfo;
    surf.Info.FourCC = MFX_FOURCC_NV12;

    for (int i = 0; i < allocResp.NumFrameActual; i++) {
        surf.Data.MemId = allocResp.mids[i];
        SamplePtr sampleToBuf(new SampleSurfaceWithData(surf, sample->GetTrackID()));
        m_pSamplesSurfPool->RegisterSample(sampleToBuf);
    }
}
