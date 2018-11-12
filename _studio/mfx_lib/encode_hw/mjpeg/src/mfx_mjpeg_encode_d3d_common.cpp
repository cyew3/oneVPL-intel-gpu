// Copyright (c) 2011-2018 Intel Corporation
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

#include  "mfxvideo++int.h"
#include "libmfx_core_interface.h"
#include "mfx_session.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "mfx_mjpeg_encode_d3d_common.h"

using namespace MfxHwMJpegEncode;

#define DEFAULT_TIMEOUT_MJPEG_HW 60000

D3DXCommonEncoder::D3DXCommonEncoder()
{
}

D3DXCommonEncoder::~D3DXCommonEncoder()
{
}

mfxStatus D3DXCommonEncoder::WaitTaskSync(DdiTask & task, mfxU32 timeOutMs)
{
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::WaitTaskSync");

    HRESULT waitRes = WaitForSingleObject(task.m_GpuEvent.gpuSyncEvent, timeOutMs);

    if (WAIT_OBJECT_0 != waitRes)
        return MFX_ERR_GPU_HANG;

    return MFX_ERR_NONE;
#else
    (void)task;
    (void)timeOutMs;
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxStatus D3DXCommonEncoder::QueryStatus(DdiTask & task)
{
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3DXCommonEncoder::QueryStatus");
        mfxStatus sts = MFX_ERR_NONE;

        // use GPUTaskSync call to wait task completion.
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)

        mfxU32 timeOutMs = DEFAULT_TIMEOUT_MJPEG_HW;

        sts = WaitTaskSync(task, timeOutMs);
        MFX_CHECK_STS(sts);

        m_EventCache->ReturnEvent(task.m_GpuEvent.gpuSyncEvent);
        task.m_GpuEvent.gpuSyncEvent = INVALID_HANDLE_VALUE;

        sts = QueryStatusAsync(task);
        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_HOTSPOTS, "ERROR !!! QueryStatus", "(ReportNumber==%d) => sts == MFX_WRN_DEVICE_BUSY", task.m_statusReportNumber);
            sts = MFX_ERR_DEVICE_FAILED;
        }
#else
        sts = QueryStatusAsync(task);
#endif
        return sts;
    }
}

mfxStatus D3DXCommonEncoder::Execute(DdiTask &task, mfxHDL surface)
{
    mfxStatus sts = MFX_ERR_NONE;

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    // Put dummy event in the task
    task.m_GpuEvent.gpuSyncEvent = INVALID_HANDLE_VALUE;
#endif

    sts = ExecuteImpl(task, surface);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN)
/* EOF */
