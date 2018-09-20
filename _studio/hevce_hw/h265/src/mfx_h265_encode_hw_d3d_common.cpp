//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_interface_scheduler.h"
#include  "mfxvideo++int.h"
#include "libmfx_core_interface.h"
#include "mfx_session.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "mfx_h265_encode_hw_d3d_common.h"
#include "mfx_h265_encode_hw_utils.h"

using namespace MfxHwH265Encode;

#define DEFAULT_TIMEOUT_H265_HW 60000

D3DXCommonEncoder::D3DXCommonEncoder()
    :pSheduler(nullptr)
    ,m_bSingleThreadMode(false)
    ,m_bIsBlockingTaskSyncEnabled(false)
{
}

D3DXCommonEncoder::~D3DXCommonEncoder()
{
    (void)Destroy();
}

// Init
mfxStatus D3DXCommonEncoder::Init(VideoCORE *pCore)
{
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    MFX_CHECK_NULL_PTR1(pCore);
    pSheduler = (MFXIScheduler2 *)pCore->GetSession()->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);
    if (pSheduler == NULL)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_SCHEDULER_PARAM schedule_Param;
    mfxStatus paramsts = pSheduler->GetParam(&schedule_Param);
    if (paramsts == MFX_ERR_NONE && schedule_Param.flags == MFX_SINGLE_THREAD)
        m_bSingleThreadMode = true;

    bool *eventsEnabled = (bool *)pCore->QueryCoreInterface(MFXBlockingTaskSyncEnabled_GUID);
    if (eventsEnabled)
        m_bIsBlockingTaskSyncEnabled = *eventsEnabled;

#else
    pCore;
#endif
    return MFX_ERR_NONE;
}

// Destroy
mfxStatus D3DXCommonEncoder::Destroy()
{
    if (pSheduler != NULL)
    {
        m_bSingleThreadMode = false;
        m_bIsBlockingTaskSyncEnabled = false;
        pSheduler->Release();
        pSheduler = NULL;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::WaitTaskSync(
    Task & task,
    mfxU32 timeOutMs)
{
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::WaitTaskSync");
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT waitRes = WaitForSingleObject(task.m_GpuEvent.gpuSyncEvent, timeOutMs);
    if (WAIT_OBJECT_0 != waitRes)
    {
        MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject", "(WAIT_OBJECT_0 != waitRes) => sts = MFX_WRN_DEVICE_BUSY", task.m_statusReportNumber);
        sts = MFX_WRN_DEVICE_BUSY;
    }
    else
    {
        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject Event %p", "ReportNumber = %d", task.m_GpuEvent.gpuSyncEvent, task.m_statusReportNumber);
    }

    return sts;
#else
    (void)task;
    (void)timeOutMs;
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxStatus D3DXCommonEncoder::Execute(
    Task const & task, mfxHDLPair surface)
{
    mfxStatus sts = MFX_ERR_NONE;

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    // Put dummy event in the task. Real event will be attached if the current task is not to be skipped
    Task & task1 = const_cast<Task&>(task);
    task1.m_GpuEvent.gpuSyncEvent = INVALID_HANDLE_VALUE;
#endif

    sts = ExecuteImpl(task, surface);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::QueryStatus(
    Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::QueryStatus");
    mfxStatus sts=MFX_ERR_NONE;

#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    // If the task was submitted to the driver
    if (task.m_GpuEvent.gpuSyncEvent != INVALID_HANDLE_VALUE)
    {
        mfxU32 timeOutMs = DEFAULT_TIMEOUT_H265_HW;

        if (m_bSingleThreadMode)
        {
            sts = pSheduler->GetTimeout(timeOutMs);
            MFX_CHECK_STS(sts);
        }

        sts = WaitTaskSync(task, timeOutMs);
        if (sts == MFX_WRN_DEVICE_BUSY && m_bSingleThreadMode)
        {
            // Need to add a check for TDRHang
            return MFX_WRN_DEVICE_BUSY;
        }
        else if (sts == MFX_WRN_DEVICE_BUSY && !m_bSingleThreadMode)
            return MFX_ERR_GPU_HANG;

        // Return event to the EventCache
        m_EventCache->ReturnEvent(task.m_GpuEvent.gpuSyncEvent);
        task.m_GpuEvent.gpuSyncEvent = INVALID_HANDLE_VALUE;
    }
    // Get the current task status
    sts = QueryStatusAsync(task);

#else
    sts = QueryStatusAsync(task);
#endif
    return sts;
}

#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE_HW) && defined (MFX_VA_WIN)
/* EOF */
