//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#include "transform_vdec.h"
#include "exceptions.h"
#include "pipeline_factory.h"
#include "sample_defs.h"
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

void Transform <MFXVideoDECODE>::CreateAllocatorAndDevice(AllocatorImpl impl)
{
    mfxStatus sts = MFX_ERR_NONE;
    m_pAllocator.reset(m_factory.CreateFrameAllocator(impl));
    if (impl != ALLOC_IMPL_SYSTEM_MEMORY) {
        m_pDevice.reset(m_factory.CreateHardwareDevice(impl));
        sts = m_pDevice->Init(0, 1, 0);
        if (sts < 0) {
            MSDK_TRACE_ERROR(MSDK_STRING("CreateHardwareDevice failed, sts=") << sts);
            throw DecodeHWDeviceInitError();
        }
    }
    std::auto_ptr<mfxAllocatorParams> alloc_params(m_factory.CreateAllocatorParam(m_pDevice.get(), impl));
    sts = m_pAllocator->Init(alloc_params.release());
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXFrameAllocator::Init failed, sts=") << sts);
        throw DecodeAllocatorInitError();
    }
}

void Transform <MFXVideoDECODE>::CreateAllocatorAndSetHandle() {
    mfxIMPL impl;
    mfxStatus sts = m_session.QueryIMPL(&impl);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoSession::QueryIMPL, sts=") << sts);
        throw DecodeQueryIMPLError();
    }

    //cut implementation basetype
    impl ^= impl & 0xff;
    mfxHDL hdl;
    if (impl == MFX_IMPL_VIA_D3D11) {
        CreateAllocatorAndDevice(ALLOC_IMPL_D3D11_MEMORY);
        m_pDevice->GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl);
        sts = m_session.SetHandle(MFX_HANDLE_D3D11_DEVICE, hdl);
    } else if ((impl == MFX_IMPL_VIA_D3D9) || (impl == MFX_IMPL_VIA_VAAPI)) {
        CreateAllocatorAndDevice(ALLOC_IMPL_D3D9_MEMORY);
#if defined(_WIN32) || defined(_WIN64)
        m_pDevice->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, &hdl);
        sts = m_session.SetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, hdl);
#else
        m_pDevice->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        sts = m_session.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
#endif
    } else {
        CreateAllocatorAndDevice(ALLOC_IMPL_SYSTEM_MEMORY);
    }

    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoSession::SetHandle, sts=") << sts);
        throw DecodeSetHandleError();
    }

    sts = m_session.SetFrameAllocator(m_pAllocator.get());
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoSession::SetAllocator, sts=") << sts);
        throw DecodeSetAllocatorError();
    }
}

void Transform <MFXVideoDECODE>::AllocFrames(SamplePtr& sample) {
    CreateAllocatorAndSetHandle();

    mfxFrameAllocRequest allocReq;
    mfxFrameAllocResponse allocResp;
    m_decParam.AsyncDepth = m_decParam.AsyncDepth;

    mfxStatus sts = m_pDEC->QueryIOSurf(&m_decParam, &allocReq);
    if (sts < 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("MFXVideoDECODE::QueryIOSurf, sts=") << sts);
        throw DecodeQueryIOSurfaceError();
    }

    //
    //alloc frames with an eye to the next transform
    MFXAVAllocRequest<mfxFrameAllocRequest> nextTransformRequest;
    MFXAVParams tmpDecParam(m_decParam);
    m_pNextTransform->GetNumSurfaces(tmpDecParam, nextTransformRequest);
    allocReq.NumFrameMin = allocReq.NumFrameMin + nextTransformRequest.Video().NumFrameMin;
    allocReq.NumFrameSuggested = allocReq.NumFrameSuggested + nextTransformRequest.Video().NumFrameSuggested;
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
