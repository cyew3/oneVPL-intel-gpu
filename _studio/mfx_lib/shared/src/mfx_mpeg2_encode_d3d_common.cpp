//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2019 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include  "mfxvideo++int.h"
#include "libmfx_core_interface.h"
#include "mfx_session.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
#include "mfx_mpeg2_encode_d3d_common.h"

using namespace MfxHwMpeg2Encode;

#define DEFAULT_TIMEOUT_MPEG2_HW 60000

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

    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::WaitTaskSync(mfxU32 timeOutMs, GPU_SYNC_EVENT_HANDLE *pEvent)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::WaitTaskSync");

    HRESULT waitRes = WaitForSingleObject(pEvent->gpuSyncEvent, timeOutMs);
    if (WAIT_OBJECT_0 != waitRes)
        return MFX_ERR_GPU_HANG;

    m_EventCache->ReturnEvent(pEvent->gpuSyncEvent);

    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt, GPU_SYNC_EVENT_HANDLE *pEvent)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::FillBSBuffer");

    mfxStatus sts = MFX_ERR_NONE;
    if (m_bIsBlockingTaskSyncEnabled)
    {
        sts = WaitTaskSync(DEFAULT_TIMEOUT_MPEG2_HW, pEvent);
        MFX_CHECK_STS(sts);
    }

    sts = FillBSBuffer(nFeedback, nBitstream, pBitstream, pEncrypt);
    if (m_bIsBlockingTaskSyncEnabled && sts == MFX_WRN_DEVICE_BUSY)
        sts = MFX_ERR_GPU_HANG;

    return sts;
}

#endif // MFX_ENABLE_HW_BLOCKING_TASK_SYNC
#endif // #if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE) && defined (MFX_VA_WIN)
/* EOF */
