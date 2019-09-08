// Copyright (c) 2011-2019 Intel Corporation
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

#include "mfx_common.h"

#include "mfx_interface_scheduler.h"
#include  "mfxvideo++int.h"
#include "libmfx_core_interface.h"
#include "mfx_session.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "mfx_h265_encode_hw_d3d_common.h"
#include "mfx_h265_encode_hw_utils.h"

using namespace MfxHwH265Encode;

D3DXCommonEncoder::D3DXCommonEncoder()
    :pSheduler(nullptr)
    ,m_bSingleThreadMode(false)
    ,m_bIsBlockingTaskSyncEnabled(false)
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    ,m_TaskSyncTimeOutMs(DEFAULT_H265_TIMEOUT_MS)
#endif
{
}

D3DXCommonEncoder::~D3DXCommonEncoder()
{
    (void)Destroy();
}

// Init
mfxStatus D3DXCommonEncoder::Init(VideoCORE *pCore, GUID guid)
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

    //  This is a _temporal_ workaround. Must be removed in a short-term.
    if ((DXVA2_Intel_MFE == guid) && (pCore->GetHWType() >= MFX_HW_ATS))
    {
        m_bIsBlockingTaskSyncEnabled = false;
    }
    else
    {
        bool *eventsEnabled = (bool *)pCore->QueryCoreInterface(MFXBlockingTaskSyncEnabled_GUID);
        if (eventsEnabled)
            m_bIsBlockingTaskSyncEnabled = *eventsEnabled;
    }

    if (m_bIsBlockingTaskSyncEnabled)
    {
        m_EventCache.reset(new EventCache());
        m_EventCache->SetGlobalHwEvent(pSheduler->GetHwEvent());
    }

    eMFXHWType platform = pCore->GetHWType();
    if (platform >= MFX_HW_JSL)
        m_TaskSyncTimeOutMs = DEFAULT_H265_TIMEOUT_MS_SIM;

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
    HANDLE gpuSyncEvent,
    mfxU32 statusReportNumber,
    mfxU32 timeOutMs)
{
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::WaitTaskSync");
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT waitRes = WaitForSingleObject(gpuSyncEvent, timeOutMs);
    if (WAIT_OBJECT_0 != waitRes)
    {
        MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject", "(WAIT_OBJECT_0 != waitRes) => sts = MFX_WRN_DEVICE_BUSY", statusReportNumber);
        sts = MFX_WRN_DEVICE_BUSY;
    }
    else
    {
        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject Event %p", "ReportNumber = %d", gpuSyncEvent, statusReportNumber);
    }

    return sts;
#else
    (void)gpuSyncEvent;
    (void)statusReportNumber;
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
#if defined(MFX_ENABLE_MFE)
    task1.m_mfeGpuEvent.gpuSyncEvent = INVALID_HANDLE_VALUE;
#endif
#endif

    sts = ExecuteImpl(task, surface);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::QueryStatus(
    Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::QueryStatus");
    mfxStatus sts = MFX_ERR_NONE;

#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    // If the task was submitted to the driver
#if defined(MFX_ENABLE_MFE)
    HANDLE & gpuSyncEvent = (task.m_mfeGpuEvent.gpuSyncEvent != INVALID_HANDLE_VALUE) ?
                                task.m_mfeGpuEvent.gpuSyncEvent :
                                task.m_GpuEvent.gpuSyncEvent;
#else
    HANDLE & gpuSyncEvent = task.m_GpuEvent.gpuSyncEvent;
#endif

    if (gpuSyncEvent != INVALID_HANDLE_VALUE)
    {
        mfxU32 timeOutMs = m_TaskSyncTimeOutMs;

        if (m_bSingleThreadMode)
        {
            sts = pSheduler->GetTimeout(timeOutMs);
            MFX_CHECK_STS(sts);
        }

        sts = WaitTaskSync(gpuSyncEvent, task.m_statusReportNumber, timeOutMs);
        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            // Need to add a check for TDRHang
            return m_bSingleThreadMode ? MFX_WRN_DEVICE_BUSY : MFX_ERR_GPU_HANG;
        }
        else if (sts != MFX_ERR_NONE)
            return MFX_ERR_DEVICE_FAILED;

        // Return event to the EventCache
        m_EventCache->ReturnEvent(gpuSyncEvent);
        gpuSyncEvent = INVALID_HANDLE_VALUE;
    }
    // Get the current task status
    sts = QueryStatusAsync(task);

#else
    sts = QueryStatusAsync(task);
#endif
    return sts;
}

#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_WIN)
/* EOF */
