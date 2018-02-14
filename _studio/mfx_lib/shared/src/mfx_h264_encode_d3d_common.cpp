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
#include "mfx_h264_encode_hw_utils.h"
#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_VA_WIN)

#include "mfx_h264_encode_d3d_common.h"

using namespace MfxHwH264Encode;

#define DEFAULT_TIMEOUT_AVCE_HW 60000

D3DXCommonEncoder::D3DXCommonEncoder()
    :pSheduler(NULL)
    ,m_bSingleThreadMode(false)
{
}

D3DXCommonEncoder::~D3DXCommonEncoder()
{
    (void)Destroy();
}

// Init
mfxStatus D3DXCommonEncoder::Init(VideoCORE *core)
{
    pSheduler = (MFXIScheduler2 *)core->GetSession()->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);
    if (pSheduler == NULL)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_SCHEDULER_PARAM schedule_Param;
    mfxStatus paramsts = pSheduler->GetParam(&schedule_Param);
    if (paramsts == MFX_ERR_NONE && schedule_Param.flags == MFX_SINGLE_THREAD)
    {
        m_bSingleThreadMode = true;
    }

    return MFX_ERR_NONE;
}

// Destroy
mfxStatus D3DXCommonEncoder::Destroy()
{
    if (pSheduler != NULL)
    {
        m_bSingleThreadMode = false;
        pSheduler->Release();
        pSheduler = NULL;
    }
    return MFX_ERR_NONE;
}

// sync call
mfxStatus D3DXCommonEncoder::WaitTaskSync(
    DdiTask & task,
    mfxU32    fieldId,
    mfxU32    timeOutMs)
{
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::WaitTaskSync");
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

mfxStatus D3DXCommonEncoder::QueryStatus(DdiTask & task, mfxU32 fieldId)
{
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::QueryStatus");
        mfxStatus sts = MFX_ERR_NONE;

        // use GPUTaskSync call to wait task completion.
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)

        mfxU32 timeOutMs = DEFAULT_TIMEOUT_AVCE_HW;

        if (m_bSingleThreadMode)
        {
            sts = pSheduler->GetTimeout(timeOutMs);
            MFX_CHECK_STS(sts);
        }

        sts = WaitTaskSync(task, fieldId, timeOutMs);
        // for single thread mode the timeOutMs can be too small, need to use an extra check
        mfxU32 curTime = vm_time_get_current_time();
        if (sts == MFX_WRN_DEVICE_BUSY && m_bSingleThreadMode)
        {
            if (task.CheckForTDRHang(curTime, MFX_H264ENC_HW_TASK_TIMEOUT))
                return MFX_ERR_GPU_HANG;
            else
                return MFX_WRN_DEVICE_BUSY;
        }
        else if(sts == MFX_WRN_DEVICE_BUSY && !m_bSingleThreadMode)
            return MFX_ERR_GPU_HANG;

        sts = QueryStatusAsync(task, fieldId);
        // if WaitTaskSync() call was successful but the task isn't ready in QueryStatusAsync()
        // it means synchronization is broken.
        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "ERROR !!! QueryStatus", "(ReportNumber==%d) => sts == MFX_ERR_DEVICE_FAILED", task.m_statusReportNumber[fieldId]);
            sts = MFX_ERR_DEVICE_FAILED;
        }
#else
        sts = QueryStatusAsync(task, fieldId);
#endif
        return sts;
    }
}

#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_VA_WIN)
/* EOF */
