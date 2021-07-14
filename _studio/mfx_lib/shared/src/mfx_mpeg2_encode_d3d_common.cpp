//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2020 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include  "mfxvideo++int.h"
#include "libmfx_core_interface.h"
#include "mfx_session.h"

#if defined (MFX_VA_WIN)
#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
#include "mfx_mpeg2_encode_d3d_common.h"

using namespace MfxHwMpeg2Encode;

D3DXCommonEncoder::D3DXCommonEncoder()
    :m_bIsBlockingTaskSyncEnabled(false)
{
}

D3DXCommonEncoder::~D3DXCommonEncoder()
{
}

mfxStatus D3DXCommonEncoder::InitCommonEnc(VideoCORE *pCore)
{
    MFX_CHECK_NULL_PTR1(pCore);

    bool *eventsEnabled = (bool *)pCore->QueryCoreInterface(MFXBlockingTaskSyncEnabled_GUID);
    if (eventsEnabled)
        m_bIsBlockingTaskSyncEnabled = *eventsEnabled;

    auto pScheduler = (MFXIScheduler2 *)pCore->GetSession()->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);
    if (pScheduler == NULL)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_bIsBlockingTaskSyncEnabled)
    {
        m_EventCache.reset(new EventCache());
        m_EventCache->SetGlobalHwEvent(pScheduler->GetHwEvent());
    }

    pScheduler->Release();

    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::WaitTaskSync(mfxU32 timeOutMs, GPU_SYNC_EVENT_HANDLE *pEvent)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MPEG2 encode DDIWaitTaskSync");

    HRESULT waitRes = WaitForSingleObject(pEvent->gpuSyncEvent, timeOutMs);
    if (WAIT_OBJECT_0 != waitRes)
    {
        MFX_CHECK(timeOutMs < DEFAULT_WAIT_HW_TIMEOUT_MS, MFX_ERR_GPU_HANG);
        return MFX_WRN_DEVICE_BUSY;
    }

    m_EventCache->ReturnEvent(pEvent->gpuSyncEvent);

    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt, GPU_SYNC_EVENT_HANDLE *pEvent)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (m_bIsBlockingTaskSyncEnabled)
    {
        sts = WaitTaskSync(DEFAULT_WAIT_HW_TIMEOUT_MS, pEvent);
        MFX_CHECK_STS(sts);
    }

    sts = FillBSBuffer(nFeedback, nBitstream, pBitstream, pEncrypt);
    if (m_bIsBlockingTaskSyncEnabled && sts == MFX_WRN_DEVICE_BUSY)
        sts = MFX_ERR_GPU_HANG;

    return sts;
}

#endif // MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
#endif // #if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
#endif //#if defined (MFX_VA_WIN)
/* EOF */
