// Copyright (c) 2020 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined(MFX_VA_LINUX) && defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)

#include "av1ehw_base_blocking_sync_win.h"
#include "mfx_session.h"
#include "libmfx_core_interface.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace AV1EHW
{
Windows::Base::BlockingSync::~BlockingSync()
{
    if (m_pSheduler)
        m_pSheduler->Release();
}

void Windows::Base::BlockingSync::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_InitInternal
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(global);
        auto  pEnable = (bool *)core.QueryCoreInterface(MFXBlockingTaskSyncEnabled_GUID);

        m_bEnabled = pEnable && *pEnable;

        if (core.GetHWType() >= MFX_HW_JSL)
        {
            const mfxU32 DEFAULTE_AV1_TIMEOUT_MS_SIM = 3600000; // 1 hour
            SetTimeout(DEFAULTE_AV1_TIMEOUT_MS_SIM);
        }

        return MFX_ERR_NONE;
    });
}

void Windows::Base::BlockingSync::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_InitAlloc
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(m_bEnabled, MFX_ERR_NONE);

        auto& core = Glob::VideoCore::Get(global);

        m_EventCache.reset(new EventCache());
        m_EventCache->Init(Glob::AllocBS::Get(global).GetResponse().NumFrameActual);

        {
            DDIExecParam submit;
            submit.Function = DXVA2_SET_GPU_TASK_EVENT_HANDLE;

            Glob::DDI_SubmitParam::GetOrConstruct(global).emplace_front(std::move(submit));
        }

        MFX_CHECK(!m_pSheduler, MFX_ERR_NONE);

        m_pSheduler = (MFXIScheduler2 *)core.GetSession()->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);
        MFX_CHECK(m_pSheduler, MFX_ERR_UNDEFINED_BEHAVIOR);

        MFX_SCHEDULER_PARAM schedule_Param;
        mfxStatus paramsts = m_pSheduler->GetParam(&schedule_Param);

        m_bSingleThreadMode = (paramsts == MFX_ERR_NONE && schedule_Param.flags == MFX_SINGLE_THREAD);

        m_EventCache->SetGlobalHwEvent(m_pSheduler->GetHwEvent());

        return MFX_ERR_NONE;
    });
}

void Windows::Base::BlockingSync::AllocTask(const FeatureBlocks& /*blocks*/, TPushAT Push)
{
    Push(BLK_AllocTask
        , [this](
            StorageR& /*global*/
            , StorageRW& task) -> mfxStatus
    {
        MFX_CHECK(m_bEnabled, MFX_ERR_NONE);

        task.Insert(Task::TaskEvent::Key, make_storable<EventDescr>());

        return MFX_ERR_NONE;
    });
}

void Windows::Base::BlockingSync::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(m_bEnabled, MFX_ERR_NONE);

        auto& task     = Task::Common::Get(s_task);
        auto& gpuEvent = Task::TaskEvent::Get(s_task);

        gpuEvent = { { GPU_COMPONENT_ENCODE, INVALID_HANDLE_VALUE }, task.StatusReportId };

        MFX_CHECK(task.SkipCMD & SKIPCMD_NeedDriverCall, MFX_ERR_NONE);

        mfxStatus sts = m_EventCache->GetEvent(gpuEvent.Handle.gpuSyncEvent);
        MFX_CHECK_STS(sts);

        auto& par       = Glob::DDI_SubmitParam::Get(global);
        auto itEventPar = std::find_if(par.begin(), par.end()
            , [](const DDIExecParam& x) { return x.Function == DXVA2_SET_GPU_TASK_EVENT_HANDLE; });

        MFX_CHECK(itEventPar != par.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

        itEventPar->In.pData = &gpuEvent.Handle;
        itEventPar->In.Size  = sizeof(gpuEvent.Handle);

        return MFX_ERR_NONE;
    });
}

void Windows::Base::BlockingSync::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_WaitTask
        , [this](
            StorageW& /*global*/
            , StorageW& task) -> mfxStatus
    {
        MFX_CHECK(m_bEnabled, MFX_ERR_NONE);

        auto& gpuEvent = Task::TaskEvent::Get(task);

        MFX_CHECK(gpuEvent.Handle.gpuSyncEvent != INVALID_HANDLE_VALUE, MFX_ERR_NONE);

        mfxStatus sts = MFX_ERR_NONE;
        mfxU32 timeOutMs = m_timeOutMs;

        if (m_bSingleThreadMode)
        {
            sts = m_pSheduler->GetTimeout(timeOutMs);
            MFX_CHECK_STS(sts);
        }

        sts = WaitTaskSync(
            gpuEvent.Handle.gpuSyncEvent
            , gpuEvent.ReportID
            , timeOutMs);

        // Need to add a check for TDRHang
        MFX_CHECK(!(sts == MFX_WRN_DEVICE_BUSY && m_bSingleThreadMode), MFX_TASK_BUSY);
        MFX_CHECK(!(sts == MFX_WRN_DEVICE_BUSY && !m_bSingleThreadMode), MFX_ERR_GPU_HANG);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_DEVICE_FAILED);

        // Return event to the EventCache
        m_EventCache->ReturnEvent(gpuEvent.Handle.gpuSyncEvent);
        gpuEvent = EventDescr();

        return MFX_ERR_NONE;
    });

    Push(BLK_ReportHang
        , [this](
            StorageW& global
            , StorageW& task) -> mfxStatus
    {
        MFX_CHECK(m_bEnabled, MFX_ERR_NONE);

        auto  id    = Task::Common::Get(task).StatusReportId;
        auto& ddiFB = Glob::DDI_Feedback::Get(global);
        auto  pFB   = (const ENCODE_QUERY_STATUS_PARAMS_AV1*)ddiFB.Get(id);
        bool  bHang = ddiFB.bNotReady || (pFB && pFB->bStatus == ENCODE_NOTREADY);

        MFX_CHECK(bHang, MFX_ERR_NONE);

        MFX_LTRACE_1(
            MFX_TRACE_LEVEL_HOTSPOTS
            , "ERROR !!! QueryStatus"
            , "(ReportNumber==%d) => sts == MFX_WRN_DEVICE_BUSY"
            , id);
        return MFX_ERR_GPU_HANG;
    });
}

mfxStatus Windows::Base::BlockingSync::WaitTaskSync(
    HANDLE gpuSyncEvent
    , mfxU32 reportID
    , mfxU32 timeOutMs)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "BlockingSync::WaitTaskSync");
    mfxStatus sts     = MFX_ERR_NONE;
    HRESULT   waitRes = WaitForSingleObject(gpuSyncEvent, timeOutMs);

    if (WAIT_OBJECT_0 != waitRes)
    {
        MFX_LTRACE_1(
            MFX_TRACE_LEVEL_HOTSPOTS
            , "WaitForSingleObject"
            , "(WAIT_OBJECT_0 != waitRes) => sts = MFX_WRN_DEVICE_BUSY"
            , reportID);
        sts = MFX_WRN_DEVICE_BUSY;
    }
    else
    {
        MFX_LTRACE_2(
            MFX_TRACE_LEVEL_HOTSPOTS
            , "WaitForSingleObject Event %p"
            , "ReportNumber = %d"
            , gpuSyncEvent
            , reportID);
    }

    return sts;
}

} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
