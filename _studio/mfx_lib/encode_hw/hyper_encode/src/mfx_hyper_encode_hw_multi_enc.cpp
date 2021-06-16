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
    std::map<mfxEncoderNum, mfxVideoParam> encParams;

    for (auto& encoder : m_singleGpuEncoders) {
        encParams[encoder->m_encoderNum] = {};
        mfxStatus sts = encoder->GetVideoParam(&encParams[encoder->m_encoderNum]);
        MFX_CHECK_STS(sts);
    }

    MFX_CHECK(MFX_ENCODERS_COUNT == 2, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(encParams[MFX_ENCODER_NUM1].mfx.GopRefDist == encParams[MFX_ENCODER_NUM2].mfx.GopRefDist, MFX_ERR_UNDEFINED_BEHAVIOR);

    return m_singleGpuEncoders[0]->GetVideoParam(par);
}

mfxStatus HyperEncodeBase::Reset(mfxVideoParam* par)
{
    mfxStatus sts = MFX_ERR_NONE;

    m_surfaceNum = 0;
    m_mfxEncParams = par;

    mfxU16 IOPattern = m_mfxEncParams->IOPattern;

    for (auto& encoder : m_singleGpuEncoders) {
        MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);
        m_mfxEncParams->IOPattern = mfxU16((m_devMngr->m_memType != SYSTEM_MEMORY && encoder->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) ?
            MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);
        sts = encoder->Reset(m_mfxEncParams);
        MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, sts);
    }

    m_mfxEncParams->IOPattern = IOPattern;

    return sts;
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

    const mfxEncoderNum encoderNum = (mfxEncoderNum)((m_surfaceNum / m_mfxEncParams->mfx.GopPicSize) % MFX_ENCODERS_COUNT);
    if (!m_surfaceNum || m_surfaceNum == m_mfxEncParams->mfx.GopPicSize)
        m_isFirstFrameOfAnyEncoder = true;

    auto encoder = std::find_if(m_singleGpuEncoders.begin(), m_singleGpuEncoders.end(),
        [encoderNum](const std::unique_ptr<SingleGpuEncode>& it) {
            return it->m_encoderNum == encoderNum;
        })->get();

    mfxBitstreamWrapperWithLock* bst = nullptr;

    if (surface) {
        auto orderInGop = m_surfaceNum % m_mfxEncParams->mfx.GopPicSize;
        bool bFirstFrameInGOP = !orderInGop;
        bool bLastFrameInGOP = (orderInGop + 1) / m_mfxEncParams->mfx.GopPicSize;

        mfxFrameSurface1* surfaceToEncode = nullptr;
        if (m_devMngr->m_appSessionPlatform.MediaAdapterType == encoder->m_adapterType)
            surfaceToEncode = surface;
        else
            sts = CopySurface(surface, &surfaceToEncode);
        MFX_CHECK_STS(sts);

        if (bFirstFrameInGOP) {
            mfxU16 IOPattern = m_mfxEncParams->IOPattern;

            m_mfxEncParams->IOPattern = mfxU16((m_devMngr->m_memType != SYSTEM_MEMORY && encoder->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) ?
                MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);
            sts = encoder->Reset(m_mfxEncParams);
            MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_UNKNOWN);

            m_mfxEncParams->IOPattern = IOPattern;
        }

        bst = GetFreeBitstream(encoderNum);
        bst->Extend(appBst->MaxLength);

        sts = encoder->EncodeFrameAsync(ctrl, surfaceToEncode, bst, &syncp);

        MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA, sts);

        m_surfaceNum++;

        if (syncp) {
            std::unique_lock<std::mutex> lock(m_mutex);
            bst->Locked = true;
            m_submittedTasks.push({ syncp, bst, nullptr, encoder->m_session, m_isFirstFrameOfAnyEncoder });
            if (m_isFirstFrameOfAnyEncoder)
                m_isFirstFrameOfAnyEncoder = false;
        }

        if (bLastFrameInGOP)
            do {
                bst = GetFreeBitstream(encoderNum);
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
                    m_submittedTasks.push({ syncp, bst, nullptr, encoder->m_session, m_isFirstFrameOfAnyEncoder });
                }
            } while (sts != MFX_ERR_MORE_DATA);
    } else {
        // Drain
        do {
            bst = GetFreeBitstream(encoderNum);
            bst->Extend(appBst->MaxLength);

            sts = encoder->EncodeFrameAsync(ctrl, nullptr, bst, &syncp);

            MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA, sts);

            if (syncp) {
                std::unique_lock<std::mutex> lock(m_mutex);
                bst->Locked = true;
                m_submittedTasks.push({ syncp, bst, nullptr, encoder->m_session, m_isFirstFrameOfAnyEncoder });
            }
        } while (sts != MFX_ERR_MORE_DATA);
    }

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if ((!surface && m_submittedTasks.size()) ||
            ((m_submittedTasks.size() >= m_mfxEncParams->mfx.GopPicSize))) {
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

        CorrectFrameIfNeeded(&task);

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

void HyperEncodeBase::CorrectFrameIfNeeded(const EncodingTasks* task)
{
    // need to remove IVF sequence header (first 32 bytes) from the first frame of second AV1 encoder
    // otherwise the stream will be displayed incorrectly
    if (task->isFirstFrameOfAnyEncoder) {
        // bytes 0-3 of IVF sequence header are signature (DKIF)
        char sign[5] = "";
        memcpy(sign, task->internalBst->Data + task->internalBst->DataOffset, 4);
        sign[4] = '\0';

        // if signature is found and this is the second frame with it, shift the frame by 32 bytes
        if (strstr(sign, "DKIF")) {
            if (m_isIVFSeqHeaderFound) {
                task->internalBst->DataOffset += 32;
                task->internalBst->DataLength -= 32;
            }
            m_isIVFSeqHeaderFound = true;
        }
    }

    // IVF frame header (12 bytes) contains the presentation timestamp (bytes 4-11). In the first frame of the
    // second encoder, this timestamp starts from 0 by default, and this leads to issues when viewing the stream
    if (m_isIVFSeqHeaderFound) {
        if (!m_timeStamps.size()) {
            m_timeStamps.push_back(0);
        }
        else if (m_timeStamps.size() < m_mfxEncParams->mfx.GopPicSize) {
            mfxU64 timeStamp;
            memcpy(&timeStamp, task->internalBst->Data + task->internalBst->DataOffset + 4, 8);
            m_timeStamps.push_back(timeStamp);
        }
        else {
            mfxU32 index = m_lastProcessedFrame % m_mfxEncParams->mfx.GopPicSize;
            m_timeStamps[index] += (mfxU64)m_mfxEncParams->mfx.GopPicSize * 2;
            memcpy(task->internalBst->Data + task->internalBst->DataOffset + 4, &m_timeStamps[index], 8);
        }
        m_lastProcessedFrame++;
    }
}

mfxStatus HyperEncodeBase::InitSession(mfxSession* appSession, mfxSession* internalSession, mfxHandleType, mfxHDL, mfxAccelerationMode accelMode, mfxMediaAdapterType, mfxU32 adapterNum)
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

mfxStatus HyperEncodeBase::CreateEncoder(mfxMediaAdapterType adapterType, mfxEncoderNum encoderNum)
{
    mfxHDL deviceHdl = nullptr;
    mfxHandleType deviceHdlType;
    mfxAccelerationMode deviceSessionAccelMode;
    mfxU32 deviceAdapterNum;

    mfxStatus sts = m_devMngr->GetHandle(adapterType, &deviceHdl, &deviceHdlType);
    MFX_CHECK_STS(sts);

    sts = m_devMngr->GetIMPL(
#ifdef SINGLE_GPU_DEBUG
        m_devMngr->m_appSessionPlatform.MediaAdapterType
#else
        adapterType
#endif
        , &deviceSessionAccelMode
        , &deviceAdapterNum);
    MFX_CHECK_STS(sts);

    // initialize session & encoder
    mfxSession session;
    sts = InitSession(&m_appSession, &session, deviceHdlType, deviceHdl, deviceSessionAccelMode, adapterType, deviceAdapterNum);
    MFX_CHECK_STS(sts);

    std::unique_ptr<SingleGpuEncode> encoder{ new SingleGpuEncode(session->m_pCORE.get(), adapterType, encoderNum) };
    m_singleGpuEncoders.push_back(std::move(encoder));

    return MFX_ERR_NONE;
}

mfxStatus HyperEncodeBase::ConfigureEncodersPool()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_isEncSupportedOnIntegrated && m_isEncSupportedOnDiscrete) {
        sts = CreateEncoder(MFX_MEDIA_INTEGRATED, MFX_ENCODER_NUM1);
        MFX_CHECK_STS(sts);
        sts = CreateEncoder(MFX_MEDIA_DISCRETE, MFX_ENCODER_NUM2);
        MFX_CHECK_STS(sts);
    }
    else if (m_isEncSupportedOnDiscrete) {
        sts = CreateEncoder(MFX_MEDIA_DISCRETE, MFX_ENCODER_NUM1);
        MFX_CHECK_STS(sts);
        sts = CreateEncoder(MFX_MEDIA_DISCRETE, MFX_ENCODER_NUM2);
        MFX_CHECK_STS(sts);
    }
    else if (m_isEncSupportedOnIntegrated) {
        sts = CreateEncoder(MFX_MEDIA_INTEGRATED, MFX_ENCODER_NUM1);
        MFX_CHECK_STS(sts);
        sts = CreateEncoder(MFX_MEDIA_INTEGRATED, MFX_ENCODER_NUM2);
        MFX_CHECK_STS(sts);
    }
    else {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    mfxMediaAdapterType appSessionPlatformAdapterType = (mfxMediaAdapterType)m_devMngr->m_appSessionPlatform.MediaAdapterType;

    for (auto& encoder : m_singleGpuEncoders)
        if (encoder->m_adapterType == appSessionPlatformAdapterType) {
            m_appPlatformInternalSession = encoder->m_session;
            MFX_CHECK_NULL_PTR1(m_appPlatformInternalSession);
        } else {
            m_areAllEncodersOnAppPlatform = false;
        }

    if (!m_appPlatformInternalSession) {
        mfxHDL deviceHdl = nullptr;
        mfxHandleType deviceHdlType;
        mfxAccelerationMode deviceSessionAccelMode;
        mfxU32 deviceAdapterNum;

        sts = m_devMngr->GetHandle(appSessionPlatformAdapterType, &deviceHdl, &deviceHdlType);
        MFX_CHECK_STS(sts);

        sts = m_devMngr->GetIMPL(appSessionPlatformAdapterType, &deviceSessionAccelMode, &deviceAdapterNum);
        MFX_CHECK_STS(sts);

        sts = InitSession(&m_appSession, &m_appPlatformInternalSession, deviceHdlType, deviceHdl, deviceSessionAccelMode, appSessionPlatformAdapterType, deviceAdapterNum);
        MFX_CHECK_STS(sts);
    }

    return sts;
}

mfxBitstreamWrapperWithLock* HyperEncodeBase::GetFreeBitstream(mfxEncoderNum encoderNum)
{
    auto encoder = std::find_if(m_singleGpuEncoders.begin(), m_singleGpuEncoders.end(),
        [encoderNum](const std::unique_ptr<SingleGpuEncode>& it) {
            return it->m_encoderNum == encoderNum;
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
    mfxStatus sts = MFX_ERR_NONE;

    for (auto& encoder : m_singleGpuEncoders) {
        MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);
        sts = encoder->Init(m_mfxEncParams);
        MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, sts);
    }

    return sts;
}

mfxStatus HyperEncodeSys::CopySurface(mfxFrameSurface1* appSurface, mfxFrameSurface1** surfaceToEncode)
{
    MFX_CHECK(appSurface, MFX_ERR_NOT_INITIALIZED);

    *surfaceToEncode = appSurface;

    return MFX_ERR_NONE;
}

mfxStatus HyperEncodeVideo::AllocateSurfacePool()
{
    // if all encoders on app's adapter - we don't need copy surfaces
    if (!m_appPlatformInternalSession->m_pVPP.get())
        return MFX_ERR_NONE;

    mfxFrameAllocRequest singleEncRequest = {};
    mfxFrameAllocRequest vppRequest[2] = {};

    // get session initialized on 2nd adapter
    auto session = std::find_if(m_singleGpuEncoders.begin(), m_singleGpuEncoders.end(),
        [this](const std::unique_ptr<SingleGpuEncode>& it) {
            return it->m_adapterType != m_devMngr->m_appSessionPlatform.MediaAdapterType;
        })->get()->m_session;

    // call QueryIOSurf for session on 2nd adapter
    mfxStatus sts = SingleGpuEncode::QueryIOSurf(session->m_pCORE.get(), m_mfxEncParams, &singleEncRequest);
    MFX_CHECK_STS(sts);

    vppRequest[0].NumFrameMin = vppRequest[0].NumFrameSuggested;
    vppRequest[0].Info = m_mfxVppParams.vpp.In;

    sts = MFXVideoVPP_QueryIOSurf(m_appPlatformInternalSession, &m_mfxVppParams, vppRequest);
    MFX_CHECK_STS(sts);

    // calculate number of frames for allocate
    singleEncRequest.NumFrameSuggested = singleEncRequest.NumFrameMin =
        singleEncRequest.NumFrameSuggested + vppRequest[0].NumFrameSuggested - m_mfxEncParams->AsyncDepth + 1;

    singleEncRequest.Type = MFX_MEMTYPE_SYSTEM_MEMORY;
    singleEncRequest.Info = m_mfxEncParams->mfx.FrameInfo;

    // allocate required surfaces for 2nd adapter encoder
    sts = m_pFrameAllocator->Alloc(m_pFrameAllocator->pthis, &singleEncRequest, &m_singleEncResponse);
    MFX_CHECK_STS(sts);

    for (int i = 0; i < m_singleEncResponse.NumFrameActual; i++) {
        std::unique_ptr<mfxFrameSurface1> surface{ new mfxFrameSurface1 };
        memset(surface.get(), 0, sizeof(mfxFrameSurface1));
        surface.get()->Info = m_mfxEncParams->mfx.FrameInfo;
        surface.get()->Data.MemId = m_singleEncResponse.mids[i];
        sts = m_pFrameAllocator->Lock(m_pFrameAllocator->pthis, surface.get()->Data.MemId, &surface.get()->Data);
        MFX_CHECK_STS(sts);
        m_pMfxSurfaces.push_back(std::move(surface));
    }

    return sts;
}

mfxStatus HyperEncodeVideo::Init()
{
    mfxStatus stsEnc = MFX_ERR_NONE;

    mfxU16 IOPattern = m_mfxEncParams->IOPattern;

    for (auto& encoder : m_singleGpuEncoders) {
        MFX_CHECK(encoder, MFX_ERR_UNDEFINED_BEHAVIOR);

        m_mfxEncParams->IOPattern = mfxU16((encoder->m_adapterType == m_devMngr->m_appSessionPlatform.MediaAdapterType) ?
            MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);

        stsEnc = encoder->Init(m_mfxEncParams);
        MFX_CHECK(stsEnc == MFX_ERR_NONE || stsEnc == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, stsEnc);
    }

    m_mfxEncParams->IOPattern = IOPattern;

    if (m_appPlatformInternalSession->m_pVPP.get()) {
        mfxStatus sts = m_appPlatformInternalSession->m_pVPP->Init(&m_mfxVppParams);
        MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, sts);
    }

    return stsEnc;
}

mfxStatus HyperEncodeVideo::Reset(mfxVideoParam* par)
{
    if (m_appPlatformInternalSession->m_pVPP.get()) {
        mfxStatus sts = m_appPlatformInternalSession->m_pScheduler->WaitForAllTasksCompletion(m_appPlatformInternalSession->m_pVPP.get());
        MFX_CHECK_STS(sts);
    }

    mfxStatus stsReset = HyperEncodeBase::Reset(par);
    MFX_CHECK(stsReset == MFX_ERR_NONE || stsReset == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, stsReset);

    if (m_appPlatformInternalSession->m_pVPP.get()) {
        m_mfxVppParams.vpp.In = m_mfxEncParams->mfx.FrameInfo;
        m_mfxVppParams.vpp.Out = m_mfxVppParams.vpp.In;
        m_mfxVppParams.AsyncDepth = m_mfxEncParams->AsyncDepth;

        mfxStatus sts = m_appPlatformInternalSession->m_pVPP->Reset(&m_mfxVppParams);
        MFX_CHECK_STS(sts);
    }

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
    m_mfxVppParams.vpp.In = m_mfxEncParams->mfx.FrameInfo;
    m_mfxVppParams.vpp.Out = m_mfxVppParams.vpp.In;
    m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_mfxVppParams.AsyncDepth = m_mfxEncParams->AsyncDepth;

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
    mfxHandleType type, mfxHDL hdl, mfxAccelerationMode accelMode, mfxMediaAdapterType mediaAdapterType, mfxU32 adapterNum)
{
    // create session
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
    MFX_CHECK(m_appPlatformInternalSession->m_pVPP.get(), MFX_ERR_NOT_INITIALIZED);

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
