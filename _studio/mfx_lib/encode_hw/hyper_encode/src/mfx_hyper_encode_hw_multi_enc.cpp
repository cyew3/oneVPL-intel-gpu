// Copyright (c) 2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_hyper_encode_hw_multi_enc.h"

#ifdef MFX_ENABLE_VIDEO_HYPER_ENCODE_HW

#include "mfx_session.h"
#include "libmfx_core_interface.h" // for MFXIEXTERNALLOC_GUID

mfxStatus HyperEncodeBase::GetVideoParam(mfxVideoParam* par)
{
    auto encoder = std::find_if(m_singleGpuEncoders.begin(), m_singleGpuEncoders.end(),
        [this](const std::unique_ptr<SingleGpuEncode>& it) {
            return it->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType;
        })->get();

    MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);
    return encoder->GetVideoParam(par);
}

mfxStatus HyperEncodeBase::Reset(mfxVideoParam* par)
{
    m_surfaceNum = 0;

    mfxStatus sts = InitEncodeParams(par);
    MFX_CHECK_STS(sts);

    mfxU16 IOPattern = m_mfxEncParams.IOPattern;

    for (auto& encoder : m_singleGpuEncoders) {
        MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);
        m_mfxEncParams.IOPattern = (m_devMngr->m_memType != SYSTEM_MEMORY && encoder->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) ?
            MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        sts = encoder->Reset(&m_mfxEncParams);
        MFX_CHECK_STS(sts);
    }

    m_mfxEncParams.IOPattern = IOPattern;

    return m_paramsChanged ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

mfxStatus HyperEncodeBase::EncodeFrameAsync(
    mfxEncodeCtrl* ctrl,
    mfxFrameSurface1* surface,
    mfxBitstream* appBst,
    mfxSyncPoint* appSyncp)
{
    MFX_CHECK_NULL_PTR1(appBst);
    MFX_CHECK_NULL_PTR1(appSyncp);

    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint syncp = nullptr;

    *appSyncp = nullptr;

    const auto adapterType = GetAdapterTypeByFrame(m_surfaceNum, m_gopSize);
    auto encoder = std::find_if(m_singleGpuEncoders.begin(), m_singleGpuEncoders.end(),
        [adapterType](const std::unique_ptr<SingleGpuEncode>& it) {
            return it->m_adapterType == adapterType;
        })->get();

    mfxBitstreamWrapperWithLock* bst = nullptr;

    if (surface) {
        auto orderInGop = m_surfaceNum % m_gopSize;
        bool bFirstFrameInGOP = !orderInGop;
        bool bLastFrameInGOP = (orderInGop + 1) / m_gopSize;

        mfxFrameSurface1* surfaceToEncode = nullptr;
        if (m_devMngr->m_appSessionPlatform.MediaAdapterType == encoder->m_adapterType)
            surfaceToEncode = surface;
        else
            sts = CopySurface(surface, &surfaceToEncode);
        MFX_CHECK_STS(sts);

        if (bFirstFrameInGOP) {
            mfxU16 IOPattern = m_mfxEncParams.IOPattern;

            m_mfxEncParams.IOPattern = (m_devMngr->m_memType != SYSTEM_MEMORY && encoder->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) ?
                MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            sts = encoder->Reset(&m_mfxEncParams);
            MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_UNKNOWN);

            m_mfxEncParams.IOPattern = IOPattern;
        }

        bst = GetFreeBitstream(adapterType);
        bst->Extend(appBst->MaxLength);

        sts = encoder->EncodeFrameAsync(ctrl, surfaceToEncode, bst, &syncp);

        MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA, sts);

        m_surfaceNum++;

        if (syncp) {
            std::unique_lock<std::mutex> lock(m_mutex);
            bst->Locked = true;
            m_submittedTasks.push({ syncp, bst, nullptr, encoder->m_session });
        }

        if (bLastFrameInGOP)
            do {
                bst = GetFreeBitstream(adapterType);
                bst->Extend(appBst->MaxLength);

                do {
                    sts = encoder->EncodeFrameAsync(ctrl, nullptr, bst, &syncp);
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        Sleep(1);
                } while (MFX_WRN_DEVICE_BUSY == sts);

                // If something (which we cannot handle here) went wrong, then return immediately
                MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA, sts);

                if (syncp) {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    bst->Locked = true;
                    m_submittedTasks.push({ syncp, bst, nullptr, encoder->m_session });
                }
            } while (sts != MFX_ERR_MORE_DATA);
    } else {
        // Drain
        do {
            bst = GetFreeBitstream(adapterType);
            bst->Extend(appBst->MaxLength);

            sts = encoder->EncodeFrameAsync(ctrl, nullptr, bst, &syncp);

            MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA, sts);

            if (syncp) {
                std::unique_lock<std::mutex> lock(m_mutex);
                bst->Locked = true;
                m_submittedTasks.push({ syncp, bst, nullptr, encoder->m_session });
            }
        } while (sts != MFX_ERR_MORE_DATA);
    }

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if ((!surface && m_submittedTasks.size()) ||
            ((m_submittedTasks.size() >= m_gopSize))) {
            mfxSyncPoint fakeSyncp = (mfxSyncPoint)malloc(0);

            auto& outTask = m_waitingSyncOpTasks[fakeSyncp];
            outTask = m_submittedTasks.front();
            m_submittedTasks.pop();
            outTask.appBst = appBst;

            *appSyncp = fakeSyncp;
            sts = MFX_ERR_NONE;
        }
        else if (!surface && !m_submittedTasks.size()) {
            sts = MFX_ERR_MORE_DATA;
        }
        else {
            sts = MFX_ERR_MORE_DATA;
        }
    }

    return sts;
}

mfxStatus HyperEncodeBase::Synchronize(
    mfxSession,
    mfxSyncPoint syncp,
    mfxU32 wait)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_waitingSyncOpTasks.find(syncp);
    if (it == m_waitingSyncOpTasks.end())
        return MFX_ERR_ABORTED;

    const EncodingTasks task = it->second;

    lock.unlock();
    mfxStatus sts = task.session->m_pScheduler->Synchronize(task.syncp, wait);
    lock.lock();

    if (MFX_ERR_NONE == sts) {
        m_waitingSyncOpTasks.erase(syncp);
        free(syncp);

        if ((task.appBst->MaxLength - (task.appBst->DataOffset + task.appBst->DataLength)) < task.internalBst->DataLength)
            return MFX_ERR_NOT_ENOUGH_BUFFER;

        memcpy(task.appBst->Data + task.appBst->DataOffset + task.appBst->DataLength, task.internalBst->Data + task.internalBst->DataOffset, task.internalBst->DataLength);
        task.appBst->DataLength += task.internalBst->DataLength;
        task.appBst->DecodeTimeStamp = task.internalBst->DecodeTimeStamp;
        task.appBst->TimeStamp = task.internalBst->TimeStamp;
        task.appBst->PicStruct = task.internalBst->PicStruct;
        task.appBst->FrameType = task.internalBst->FrameType;
        task.appBst->DataFlag = task.internalBst->DataFlag;
        task.internalBst->Locked = false;
    }

    return sts;
}

mfxStatus HyperEncodeBase::Close()
{
    for (auto& encoder : m_singleGpuEncoders)
    {
        MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);
        mfxStatus sts = encoder->Close();
        MFX_CHECK_STS(sts);
        sts = MFXClose(encoder->m_session);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

mfxStatus HyperEncodeBase::InitSession(mfxSession* appSession, mfxSession* internalSession, mfxHandleType, mfxHDL, mfxAccelerationMode accelMode, mfxU16, mfxU32 adapterNum)
{
    mfxInitializationParam param;
    param.AccelerationMode = accelMode;
    param.VendorImplID = adapterNum;
    param.NumExtParam = 0;
    param.ExtParam = nullptr;

    mfxStatus sts = MFXInitialize(param, internalSession);
    MFX_CHECK_STS(sts);

    return MFXJoinSession(*appSession, *internalSession);
}

mfxStatus HyperEncodeBase::CreateEncoders()
{
    mfxHDL integratedDeviceHdl = nullptr, discreteDeviceHdl = nullptr;
    mfxHandleType integratedDeviceHdlType, discreteDeviceHdlType;
    mfxAccelerationMode integratedDeviceSessionAccelMode, discreteDeviceSessionAccelMode;
    mfxU32 integratedDeviceAdapterNum, discreteDeviceAdapterNum;

    mfxStatus sts = m_devMngr->GetHandle(MFX_MEDIA_INTEGRATED, &integratedDeviceHdl, &integratedDeviceHdlType);
    MFX_CHECK_STS(sts);

    sts = m_devMngr->GetHandle(MFX_MEDIA_DISCRETE, &discreteDeviceHdl, &discreteDeviceHdlType);
    MFX_CHECK_STS(sts);

    sts = m_devMngr->GetIMPL(
#ifdef SINGLE_GPU_DEBUG
        m_devMngr->m_appSessionPlatform.MediaAdapterType
#else
        MFX_MEDIA_INTEGRATED
#endif        
        , &integratedDeviceSessionAccelMode
        , &integratedDeviceAdapterNum);
    MFX_CHECK_STS(sts);

    sts = m_devMngr->GetIMPL(
#ifdef SINGLE_GPU_DEBUG
        m_devMngr->m_appSessionPlatform.MediaAdapterType
#else
        MFX_MEDIA_DISCRETE
#endif
        , &discreteDeviceSessionAccelMode
        , &discreteDeviceAdapterNum);
    MFX_CHECK_STS(sts);

    // initialize session & encoder on iGfx
    mfxSession integratedSession;
    sts = InitSession(&m_appSession, &integratedSession, integratedDeviceHdlType, integratedDeviceHdl, integratedDeviceSessionAccelMode, MFX_MEDIA_INTEGRATED, integratedDeviceAdapterNum);
    MFX_CHECK_STS(sts);

    std::unique_ptr<SingleGpuEncode> integratedEncoder{ new SingleGpuEncode(integratedSession->m_pCORE.get(), MFX_MEDIA_INTEGRATED) };
    m_singleGpuEncoders.push_back(std::move(integratedEncoder));

    // initialize session & encoder on dGfx
    mfxSession discreteSession;
    sts = InitSession(&m_appSession, &discreteSession, discreteDeviceHdlType, discreteDeviceHdl, discreteDeviceSessionAccelMode, MFX_MEDIA_DISCRETE, discreteDeviceAdapterNum);
    MFX_CHECK_STS(sts);

    std::unique_ptr<SingleGpuEncode> discreteEncoder{ new SingleGpuEncode(discreteSession->m_pCORE.get(), MFX_MEDIA_DISCRETE) };
    m_singleGpuEncoders.push_back(std::move(discreteEncoder));

    for (auto& encoder : m_singleGpuEncoders)
        if (encoder->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) {
            m_appPlatformInternalSession = encoder->m_session;
            MFX_CHECK_NULL_PTR1(m_appPlatformInternalSession);
            break;
        }

    return sts;
}

mfxU16 HyperEncodeBase::GetAdapterTypeByFrame(mfxU32 frameNum, mfxU16 gopSize)
{
    mfxU16 gopNum = (mfxU16)(frameNum / gopSize);
    return gopNum % 2;
}

mfxStatus HyperEncodeBase::InitEncodeParams(mfxVideoParam* par)
{
    m_paramsChanged = false;
    m_mfxEncParams = *par;
    auto mfxEncExtCodingOpton = m_mfxEncParams.GetExtendedBuffer<mfxExtCodingOption>(MFX_EXTBUFF_CODING_OPTION);

    if (mfxEncExtCodingOpton) {
        if (!IsOff(mfxEncExtCodingOpton->NalHrdConformance) || !IsOff(mfxEncExtCodingOpton->VuiNalHrdParameters)) {
            mfxEncExtCodingOpton->NalHrdConformance = MFX_CODINGOPTION_OFF;
            mfxEncExtCodingOpton->VuiNalHrdParameters = MFX_CODINGOPTION_OFF;

            m_paramsChanged = true;
        }
    } else {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxBitstreamWrapperWithLock* HyperEncodeBase::GetFreeBitstream(mfxU16 adapterType)
{
    auto encoder = std::find_if(m_singleGpuEncoders.begin(), m_singleGpuEncoders.end(),
        [adapterType](const std::unique_ptr<SingleGpuEncode>& it) {
            return it->m_adapterType == adapterType;
        })->get();

    auto& bitsteamPool = m_bitstreams[encoder];
    auto it = std::find_if(bitsteamPool.begin(), bitsteamPool.end(),
        [](mfxBitstreamWrapperWithLock* bst) {
            return bst->Locked == 0;
        });
    mfxBitstreamWrapperWithLock* bst = nullptr;
    if (it != bitsteamPool.end()) {
        bst = (*it);
    } else {
        bst = new mfxBitstreamWrapperWithLock();
        bitsteamPool.push_back(bst);
    }

    bst->DataOffset = 0;
    bst->DataLength = 0;

    return bst;
}

mfxStatus HyperEncodeSys::Init()
{
    for (auto& encoder : m_singleGpuEncoders) {
        MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);
        mfxStatus sts = encoder->Init(&m_mfxEncParams);
        MFX_CHECK_STS(sts);
    }

    return m_paramsChanged ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

mfxStatus HyperEncodeSys::CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode)
{
    MFX_CHECK(appSurface, MFX_ERR_NOT_INITIALIZED);

    *surfaceToEncode = appSurface;

    return MFX_ERR_NONE;
}

mfxStatus HyperEncodeVideo::AllocateSurfacePool()
{
    mfxFrameAllocRequest singleEncRequest = {};
    mfxFrameAllocRequest vppRequest[2] = {};

    // get session initialized on 2nd adapter
    auto session = std::find_if(m_singleGpuEncoders.begin(), m_singleGpuEncoders.end(),
        [this](const std::unique_ptr<SingleGpuEncode>& it) {
            return it->m_adapterType != m_devMngr->m_appSessionPlatform.MediaAdapterType;
        })->get()->m_session;

    // call QueryIOSurf for session on 2nd adapter
    mfxStatus sts = SingleGpuEncode::QueryIOSurf(session->m_pCORE.get(), &m_mfxEncParams, &singleEncRequest);
    MFX_CHECK_STS(sts);

    vppRequest[0].NumFrameMin = vppRequest[0].NumFrameSuggested;
    vppRequest[0].Info = m_mfxVppParams.vpp.In;

    sts = MFXVideoVPP_QueryIOSurf(m_appPlatformInternalSession, &m_mfxVppParams, vppRequest);
    MFX_CHECK_STS(sts);

    // calculate number of frames for allocate
    singleEncRequest.NumFrameSuggested = singleEncRequest.NumFrameMin =
        singleEncRequest.NumFrameSuggested + vppRequest[0].NumFrameSuggested - m_mfxEncParams.AsyncDepth + 1;

    singleEncRequest.Type = MFX_MEMTYPE_SYSTEM_MEMORY;
    singleEncRequest.Info = m_mfxEncParams.mfx.FrameInfo;

    // allocate required surfaces for 2nd adapter encoder
    sts = m_pFrameAllocator->Alloc(m_pFrameAllocator->pthis, &singleEncRequest, &m_singleEncResponse);
    MFX_CHECK_STS(sts);

    for (int i = 0; i < m_singleEncResponse.NumFrameActual; i++) {
        std::unique_ptr<mfxFrameSurface1> surface{ new mfxFrameSurface1 };
        memset(surface.get(), 0, sizeof(mfxFrameSurface1));
        surface.get()->Info = m_mfxEncParams.mfx.FrameInfo;
        surface.get()->Data.MemId = m_singleEncResponse.mids[i];
        sts = m_pFrameAllocator->Lock(m_pFrameAllocator->pthis, surface.get()->Data.MemId, &surface.get()->Data);
        MFX_CHECK_STS(sts);
        m_pMfxSurfaces.push_back(std::move(surface));
    }

    return sts;
}

mfxStatus HyperEncodeVideo::Init()
{
    mfxStatus sts;
    mfxU16 IOPattern = m_mfxEncParams.IOPattern;

    for (auto& encoder : m_singleGpuEncoders) {
        MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);

        m_mfxEncParams.IOPattern = mfxU16((encoder->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) ?
            MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);

        sts = encoder->Init(&m_mfxEncParams);
        MFX_CHECK_STS(sts);
    }

    m_mfxEncParams.IOPattern = IOPattern;

    sts = m_appPlatformInternalSession->m_pVPP->Init(&m_mfxVppParams);
    MFX_CHECK_STS(sts);

    return m_paramsChanged ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

mfxStatus HyperEncodeVideo::Reset(mfxVideoParam* par)
{
    mfxStatus sts = m_appPlatformInternalSession->m_pScheduler->WaitForAllTasksCompletion(m_appPlatformInternalSession->m_pVPP.get());
    MFX_CHECK_STS(sts);

    mfxStatus stsReset = HyperEncodeBase::Reset(par);
    MFX_CHECK(stsReset >= MFX_ERR_NONE, stsReset);

    m_mfxVppParams.vpp.In = m_mfxEncParams.mfx.FrameInfo;
    m_mfxVppParams.vpp.Out = m_mfxVppParams.vpp.In;
    m_mfxVppParams.AsyncDepth = m_mfxEncParams.AsyncDepth;

    sts = m_appPlatformInternalSession->m_pVPP->Reset(&m_mfxVppParams);
    MFX_CHECK_STS(sts);

    return stsReset;
}

mfxStatus HyperEncodeVideo::Close()
{
    mfxStatus sts = HyperEncodeBase::Close();
    MFX_CHECK_STS(sts);

    MFX_CHECK(m_pFrameAllocator, MFX_ERR_UNDEFINED_BEHAVIOR);
    return m_pFrameAllocator->Free(m_pFrameAllocator->pthis, &m_singleEncResponse);
}

mfxStatus HyperEncodeVideo::CreateVPP()
{
    m_appPlatformInternalSession->m_pVPP.reset(m_appPlatformInternalSession->Create<VideoVPP>(m_mfxVppParams));
    MFX_CHECK(m_appPlatformInternalSession->m_pVPP.get(), MFX_ERR_INVALID_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus HyperEncodeVideo::InitVPPparams()
{
    m_mfxVppParams.vpp.In = m_mfxEncParams.mfx.FrameInfo;
    m_mfxVppParams.vpp.Out = m_mfxVppParams.vpp.In;
    m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_mfxVppParams.AsyncDepth = m_mfxEncParams.AsyncDepth;

    // configure and attach external parameters
    m_vppDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    m_vppDoNotUse.Header.BufferSz = sizeof(mfxExtVPPDoNotUse);
    m_vppDoNotUse.NumAlg = 4;

    m_algList.reset(new mfxU32[m_vppDoNotUse.NumAlg]);
    m_vppDoNotUse.AlgList = m_algList.get();
    m_vppDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;        // turn off denoising (on by default)
    m_vppDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    m_vppDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL;         // turn off detail enhancement (on by default)
    m_vppDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP;        // turn off processing amplified (on by default)

    m_vppExtParams.push_back((mfxExtBuffer*)&m_vppDoNotUse);
    m_mfxVppParams.ExtParam = m_vppExtParams.data();
    m_mfxVppParams.NumExtParam = (mfxU16)m_vppExtParams.size();

    // Querying VPP
    mfxStatus sts = MFXVideoVPP_Query(m_appPlatformInternalSession, &m_mfxVppParams, &m_mfxVppParams);
    MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, sts);

    return MFX_ERR_NONE;
}

mfxStatus HyperEncodeVideo::InitSession(
    mfxSession* appSession, mfxSession* internalSession,
    mfxHandleType type, mfxHDL hdl, mfxAccelerationMode accelMode, mfxU16 mediaAdapterType, mfxU32 adapterNum)
{
    // prepare encode session
    mfxStatus sts = HyperEncodeBase::InitSession(appSession, internalSession, type, hdl, accelMode, mediaAdapterType, adapterNum);
    MFX_CHECK_STS(sts);

    sts = (*internalSession)->m_pCORE->SetHandle(type, hdl);
    MFX_CHECK_STS(sts);

    mfxFrameAllocator* allocator = (mediaAdapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) ?
        (mfxFrameAllocator*)m_appSession->m_pCORE->QueryCoreInterface(MFXIEXTERNALLOC_GUID) :
        m_pFrameAllocator;

    return (*internalSession)->m_pCORE->SetFrameAllocator(allocator);
}

mfxStatus HyperEncodeVideo::CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode)
{
    MFX_CHECK(appSurface, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint VppSyncp = nullptr;

    mfxFrameSurface1* unlockedSurface = nullptr;
    for (auto& surface : m_pMfxSurfaces)
        if (0 == surface.get()->Data.Locked)
        {
            unlockedSurface = surface.get();
            break;
        }
    MFX_CHECK(unlockedSurface, MFX_WRN_DEVICE_BUSY);

    for (;;) {
        sts = MFXVideoVPP_RunFrameVPPAsync(m_appPlatformInternalSession, appSurface, unlockedSurface, nullptr, &VppSyncp);

        if (MFX_ERR_NONE > sts) {
            return sts;
        } else if (MFX_ERR_NONE < sts && !VppSyncp) {
            if (MFX_WRN_DEVICE_BUSY == sts)
                Sleep(1);
        } else if (MFX_ERR_NONE < sts && VppSyncp) {
            sts = MFX_ERR_NONE;
            break;
        } else {
            break;
        }
    }

    unlockedSurface->Info.CropX = appSurface->Info.CropX;
    unlockedSurface->Info.CropY = appSurface->Info.CropY;
    unlockedSurface->Info.CropW = appSurface->Info.CropW;
    unlockedSurface->Info.CropH = appSurface->Info.CropH;
    unlockedSurface->Info.FrameRateExtN = appSurface->Info.FrameRateExtN;
    unlockedSurface->Info.FrameRateExtD = appSurface->Info.FrameRateExtD;
    unlockedSurface->Info.AspectRatioW = appSurface->Info.AspectRatioW;
    unlockedSurface->Info.AspectRatioH = appSurface->Info.AspectRatioH;
    unlockedSurface->Info.PicStruct = appSurface->Info.PicStruct;
    unlockedSurface->Data.TimeStamp = appSurface->Data.TimeStamp;
    unlockedSurface->Data.FrameOrder = appSurface->Data.FrameOrder;
    unlockedSurface->Data.Corrupted = appSurface->Data.Corrupted;
    unlockedSurface->Data.DataFlag = appSurface->Data.DataFlag;

    *surfaceToEncode = unlockedSurface;

    return sts;
}

#endif // MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
