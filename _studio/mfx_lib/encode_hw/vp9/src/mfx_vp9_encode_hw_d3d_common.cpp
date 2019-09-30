// Copyright(c) 2016 - 2019 Intel Corporation
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
// SOFTWARE

#include "mfx_common.h"

#include "mfx_interface_scheduler.h"
#include  "mfxvideo++int.h"
#include "libmfx_core_interface.h"
#include "mfx_session.h"
#include <mfx_trace.h>

#if defined (MFX_VA_WIN)

#include "mfx_vp9_encode_hw_d3d_common.h"
#include "mfx_vp9_encode_hw_utils.h"

namespace MfxHwVP9Encode
{

#define DEFAULT_TIMEOUT_VP9_HW 60000
#define DEFAULT_TIMEOUT_VP9_HW_SIM 3600000

    D3DXCommonEncoder::D3DXCommonEncoder()
        :m_bIsBlockingTaskSyncEnabled(false)
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        ,pScheduler(NULL)
        ,m_bSingleThreadMode(false)
        ,m_TaskSyncTimeOutMs(DEFAULT_TIMEOUT_VP9_HW)
#endif
    {
    }

    D3DXCommonEncoder::~D3DXCommonEncoder()
    {
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        if (m_bIsBlockingTaskSyncEnabled)
            (void)DestroyBlockingSync();
#endif
    }

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    // Init
    mfxStatus D3DXCommonEncoder::InitBlockingSync(VideoCORE *pCore)
    {
        MFX_CHECK_NULL_PTR1(pCore);

        pScheduler = (MFXIScheduler2 *)pCore->GetSession()->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);

        if (pScheduler == NULL)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        MFX_SCHEDULER_PARAM schedule_Param;
        mfxStatus paramsts = pScheduler->GetParam(&schedule_Param);
        if (paramsts == MFX_ERR_NONE && schedule_Param.flags == MFX_SINGLE_THREAD)
        {
            m_bSingleThreadMode = true;
        }

        MFX_CHECK_STS(SetGPUSyncEventEnable(pCore));

        if (m_bIsBlockingTaskSyncEnabled)
        {
            m_EventCache.reset(new EventCache());
            m_EventCache->SetGlobalHwEvent(pScheduler->GetHwEvent());
        }

        eMFXHWType platform = pCore->GetHWType();
        if (platform >= MFX_HW_JSL)
            m_TaskSyncTimeOutMs = DEFAULT_TIMEOUT_VP9_HW_SIM;

        return MFX_ERR_NONE;
    }

    // Destroy
    mfxStatus D3DXCommonEncoder::DestroyBlockingSync()
    {
        if (pScheduler != NULL)
        {
            m_bSingleThreadMode = false;
            pScheduler->Release();
            pScheduler = NULL;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus D3DXCommonEncoder::WaitTaskSync(
        Task & task,
        mfxU32 timeOutMs)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::WaitTaskSync");
        mfxStatus sts = MFX_ERR_NONE;
        HRESULT waitRes = WaitForSingleObject(task.m_GpuEvent.gpuSyncEvent, timeOutMs);
        if (WAIT_OBJECT_0 != waitRes)
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject", "(WAIT_OBJECT_0 != waitRes) => sts = MFX_WRN_DEVICE_BUSY", task.m_GpuEvent.gpuSyncEvent);
            sts = MFX_WRN_DEVICE_BUSY;
        }
        else
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject", "Event %p", task.m_GpuEvent.gpuSyncEvent);
        }

        return sts;
    }
#endif

    mfxStatus D3DXCommonEncoder::QueryStatus(
        Task & task)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::QueryStatus");
        mfxStatus sts = MFX_ERR_NONE;

        // use GPUTaskSync call to wait task completion.
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
        if (m_bIsBlockingTaskSyncEnabled)
        {
            mfxU32 timeOutMs = m_TaskSyncTimeOutMs;

            if (m_bSingleThreadMode)
            {
                sts = pScheduler->GetTimeout(timeOutMs);
                MFX_CHECK_STS(sts);
            }

            sts = WaitTaskSync(task, timeOutMs);
            if (sts == MFX_WRN_DEVICE_BUSY)
            {
                // Need to add a check for TDRHang
                return m_bSingleThreadMode ? MFX_WRN_DEVICE_BUSY : MFX_ERR_GPU_HANG;
            }
            else if (sts != MFX_ERR_NONE)
                return MFX_ERR_DEVICE_FAILED;

            m_EventCache->ReturnEvent(task.m_GpuEvent.gpuSyncEvent);

            sts = QueryStatusAsync(task);
            if (sts == MFX_WRN_DEVICE_BUSY)
            {
                MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "ERROR !!! QueryStatus", "(ReportNumber==%d) => sts == MFX_WRN_DEVICE_BUSY", task.m_taskIdForDriver);
                sts = MFX_ERR_DEVICE_FAILED;
            }
        }
        else
            sts = QueryStatusAsync(task);
#else
        sts = QueryStatusAsync(task);
#endif
        return sts;
    }

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    mfxStatus D3DXCommonEncoder::SetGPUSyncEventEnable(VideoCORE *pCore)
    {
        MFX_CHECK_NULL_PTR1(pCore);
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
        bool *eventsEnabled = (bool *)pCore->QueryCoreInterface(MFXBlockingTaskSyncEnabled_GUID);
        if (eventsEnabled)
            m_bIsBlockingTaskSyncEnabled = *eventsEnabled;
#endif
        return MFX_ERR_NONE;
    }
#endif
}// namespace MfxHwVP9Encode

#endif // #if defined (MFX_VA_WIN)
/* EOF */
