// Copyright (c) 2011-2020 Intel Corporation
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

#if defined (MFX_VA_WIN)
#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)

#include "mfx_h264_encode_hw_utils.h"

#include "mfx_h264_encode_d3d_common.h"

using namespace MfxHwH264Encode;

#define DEFAULT_TIMEOUT_AVCE_HW_SIM 600000

D3DXCommonEncoder::D3DXCommonEncoder()
    :pSheduler(NULL)
    ,m_bSingleThreadMode(false)
    ,m_timeoutSync(0)
    ,m_timeoutForTDR(0)
{
}

D3DXCommonEncoder::~D3DXCommonEncoder()
{
    (void)Destroy();
}

// Init
mfxStatus D3DXCommonEncoder::Init(VideoCORE *core)
{
    if (!core->GetSession())
        return MFX_ERR_NONE; //init with incomplete core, just to query caps

    pSheduler = (MFXIScheduler2 *)core->GetSession()->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);
    if (pSheduler == NULL)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_SCHEDULER_PARAM schedule_Param;
    mfxStatus paramsts = pSheduler->GetParam(&schedule_Param);
    eMFXHWType platform = core->GetHWType();

    if (paramsts == MFX_ERR_NONE && schedule_Param.flags == MFX_SINGLE_THREAD)
    {
        m_bSingleThreadMode = true;

        m_timeoutForTDR = IsPreSiPlatform(platform) ? MFX_H264ENC_HW_TASK_TIMEOUT_SIM : MFX_H264ENC_HW_TASK_TIMEOUT;
    }

    m_timeoutSync = IsPreSiPlatform(platform) ? DEFAULT_TIMEOUT_AVCE_HW_SIM : DEFAULT_WAIT_HW_TIMEOUT_MS;

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    m_EventCache.reset(new EventCache());
    m_EventCache->SetGlobalHwEvent(pSheduler->GetHwEvent());
#endif

    return MFX_ERR_NONE;
}

// Destroy
mfxStatus D3DXCommonEncoder::Destroy()
{
    if (pSheduler != NULL)
    {
        m_timeoutForTDR = 0;
        m_bSingleThreadMode = false;
        pSheduler->Release();
        pSheduler = NULL;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3DXCommonEncoder::Execute(mfxHDLPair pair, DdiTask const & task, mfxU32 fieldId, PreAllocatedVector const & sei)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "H264 encode DDISubmitTask");

    ETW_NEW_EVENT(MFX_TRACE_HOTSPOT_DDI_EXECUTE_D3DX_TASK, 0, make_event_data(this), [&](){ return make_event_data(sts);});

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    // Put dummy event in the task. Real event will be attached if the current task is not to be skipped
    DdiTask & task1 = const_cast<DdiTask&>(task);
    task1.m_GpuEvent[fieldId].gpuSyncEvent = INVALID_HANDLE_VALUE;
#endif

    sts = ExecuteImpl(pair, task, fieldId, sei);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

// sync call
mfxStatus D3DXCommonEncoder::WaitTaskSync(
    DdiTask & task,
    mfxU32    fieldId,
    mfxU32    timeOutMs)
{
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE)
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "H264 encode DDIWaitTaskSync");
    mfxStatus sts = MFX_ERR_NONE;

    HRESULT waitRes = WaitForSingleObject(task.m_GpuEvent[fieldId].gpuSyncEvent, timeOutMs);

    if (WAIT_OBJECT_0 != waitRes)
    {
        MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject", "(WAIT_OBJECT_0 != waitRes) => sts = MFX_WRN_DEVICE_BUSY", task.m_statusReportNumber[fieldId]);
        sts = MFX_WRN_DEVICE_BUSY;
    }
    else
    {
        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "WaitForSingleObject Event %p", "ReportNumber = %d", task.m_GpuEvent->gpuSyncEvent, task.m_statusReportNumber[fieldId]);
    }

    (void)fieldId;
    return sts;
#else
    (void)task;
    (void)fieldId;
    (void)timeOutMs;
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxStatus D3DXCommonEncoder::QueryStatus(DdiTask & task, mfxU32 fieldId, bool useEvent)
{
    mfxStatus sts = MFX_ERR_NONE;
    ETW_NEW_EVENT(MFX_TRACE_HOTSPOT_DDI_QUERY_D3DX_TASK, 0, make_event_data(this), [&](){ return make_event_data(sts);});

    // use GPUTaskSync call to wait task completion.
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE)
    if(useEvent) {
        // If the task was submitted to the driver
        if(task.m_GpuEvent[fieldId].gpuSyncEvent != INVALID_HANDLE_VALUE)
        {
            mfxU32 timeOutMs = m_timeoutSync;

            if(m_bSingleThreadMode)
            {
                sts = pSheduler->GetTimeout(timeOutMs);
                MFX_CHECK_STS(sts);
            }

            sts = WaitTaskSync(task, fieldId, timeOutMs);

            // for single thread mode the timeOutMs can be too small, need to use an extra check
            mfxU32 curTime = vm_time_get_current_time();
            if(sts == MFX_WRN_DEVICE_BUSY)
            {
                if(task.CheckForTDRHang(curTime, m_timeoutForTDR) || !m_bSingleThreadMode)
                    return MFX_ERR_GPU_HANG;
                else
                    return MFX_WRN_DEVICE_BUSY;
            }
            else if(sts != MFX_ERR_NONE)
                return MFX_ERR_DEVICE_FAILED;

            // Return event to the EventCache
            m_EventCache->ReturnEvent(task.m_GpuEvent[fieldId].gpuSyncEvent);
            task.m_GpuEvent[fieldId].gpuSyncEvent = INVALID_HANDLE_VALUE;
        }

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "H264 encode DDIQueryTask");
        // If the task was skipped or blocking sync call is succeded try to get the current task status
        sts = QueryStatusAsync(task, fieldId);
        if(sts == MFX_WRN_DEVICE_BUSY)
            sts = MFX_ERR_GPU_HANG;
    }
    else {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "H264 encode DDIQueryTask");
        sts = QueryStatusAsync(task, fieldId);
    }

#else

    MFX_AUTO_LTR ACE(MFX_TRACE_LEVEL_HOTSPOTS, "H264 encode DDIQueryTask");
    sts = QueryStatusAsync(task, fieldId);
#endif

return sts;
}

#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE)
#endif // #if defined (MFX_VA_WIN)
/* EOF */
