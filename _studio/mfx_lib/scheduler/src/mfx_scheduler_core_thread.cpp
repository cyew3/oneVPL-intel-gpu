//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2017 Intel Corporation. All Rights Reserved.
//

#include <mfx_scheduler_core.h>
#include <mfx_scheduler_core_task.h>
#include "mfx_scheduler_dx11_event.h"

#include <mfx_trace.h>
#include <stdio.h>
#include <vm_time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

mfxStatus mfxSchedulerCore::StartWakeUpThread(void)
{
    // stop the thread before creating it again
    // don't try to check thread status, it might lead to interesting effects.
    if (m_hwWakeUpThread.handle)
        StopWakeUpThread();

    m_timer_hw_event = MFX_THREAD_TIME_TO_WAIT;

#ifdef MFX_VLV_PLATFORM    
    m_timer_hw_event = 10; //temporary fix for VLV
#endif

#if defined(_WIN32) || defined(_WIN64)
    if (!m_hwTaskDone.handle)
        m_hwTaskDone.handle = CreateEventExW(NULL, 
                                            _T("Global\\IGFXKMDNotifyBatchBuffersComplete"), 
                                            CREATE_EVENT_MANUAL_RESET, 
                                            STANDARD_RIGHTS_ALL | EVENT_MODIFY_STATE);
    if (m_hwTaskDone.handle)
    {
        // create 'hardware task done' thread
        Ipp32s iRes = vm_thread_create(&m_hwWakeUpThread,
                                        scheduler_wakeup_thread_proc,
                                        this);
        if (0 == iRes)
        {
            return MFX_ERR_UNKNOWN;
        }
    }
#endif // defined(_WIN32) || defined(_WIN64)

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::StartWakeUpThread(void)

mfxStatus mfxSchedulerCore::StopWakeUpThread(void)
{
#if defined(_WIN32) || defined(_WIN64)
    m_bQuitWakeUpThread = true;
    m_timer_hw_event = 1; // we need close threads ASAP
    vm_event_signal(&m_hwTaskDone); 

    // close hardware listening tools
    vm_thread_wait(&m_hwWakeUpThread); 
    vm_thread_close(&m_hwWakeUpThread);
    //no specific path to obtain event
    // let close handle
#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
    if (!m_pdx11event)
    {
        vm_event_destroy(&m_hwTaskDone);
    }
    else
    {
        delete m_pdx11event;
        m_pdx11event = 0;
        m_hwTaskDone.handle = 0; // handle has been obtained by UMD
    }
#endif
#endif

    m_bQuitWakeUpThread = false;
    vm_event_set_invalid(&m_hwTaskDone);
    vm_thread_set_invalid(&m_hwWakeUpThread);
#endif // defined(_WIN32) || defined(_WIN64)

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::StopWakeUpThread(void)

Ipp32u mfxSchedulerCore::scheduler_thread_proc(void *pParam)
{
    MFX_SCHEDULER_THREAD_CONTEXT *pContext = (MFX_SCHEDULER_THREAD_CONTEXT *) pParam;

    {
        char thread_name[30] = {};
#if defined(_WIN32) || defined(_WIN64)
        _snprintf_s(thread_name, sizeof(thread_name) - 1, _TRUNCATE, "ThreadName=MSDK#%d", pContext->threadNum);
#else
        snprintf(thread_name, sizeof(thread_name)-1, "ThreadName=MSDK#%d", pContext->threadNum);
#endif
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, thread_name);
    }

    pContext->pSchedulerCore->ThreadProc(pContext);
    return (0x0cced00 + pContext->threadNum);
}

void mfxSchedulerCore::ThreadProc(MFX_SCHEDULER_THREAD_CONTEXT *pContext)
{
    mfxTaskHandle previousTaskHandle = {};
    const Ipp32u threadNum = pContext->threadNum;

    // main working cycle for threads
    while (false == m_bQuit)
    {
        MFX_CALL_INFO call = {};
        mfxStatus mfxRes;

        mfxRes = GetTask(call, previousTaskHandle, threadNum);
        if (MFX_ERR_NONE == mfxRes)
        {
            // perform asynchronous operation
            call_pRoutine(call);

            pContext->workTime += call.timeSpend;
            // save the previous task's handle
            previousTaskHandle = call.taskHandle;

            // mark the task completed,
            // set the sync point into the high state if any.
            MarkTaskCompleted(&call, threadNum);
            //timer1.Stop(0);
        }
        else
        {
            mfxU64 start, stop;

#if defined(MFX_SCHEDULER_LOG)
            mfxLogWriteA(m_hLog,
                         "[% 4u] thread's sleeping\n", threadNum);
#endif // defined(MFX_SCHEDULER_LOG)

            // mark beginning of sleep period
            start = GetHighPerformanceCounter();

            // there is no any task.
            // sleep for a while until the event is signaled.
            Wait(threadNum);

            // mark end of sleep period
            stop = GetHighPerformanceCounter();

            // update thread statistic
            pContext->sleepTime += (stop - start);

#if defined(MFX_SCHEDULER_LOG)
            mfxLogWriteA(m_hLog,
                         "[% 4u] thread woke up\n", threadNum);
#endif // defined(MFX_SCHEDULER_LOG)
        }
    }
}

Ipp32u mfxSchedulerCore::scheduler_wakeup_thread_proc(void *pParam)
{
    mfxSchedulerCore * const pSchedulerCore = (mfxSchedulerCore *) pParam;

    {
        const char thread_name[30] = "ThreadName=MSDKHWL#0";
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, thread_name);
    }

    pSchedulerCore->WakeupThreadProc();
    return 0x0ccedff;
}

void mfxSchedulerCore::WakeupThreadProc()
{
    // main working cycle for threads
    while (false == m_bQuitWakeUpThread)
    {
        vm_status vmRes;

        vmRes = vm_event_timed_wait(&m_hwTaskDone, m_timer_hw_event);

        // HW event is signaled. Reset all HW waiting tasks.
        if (VM_OK == vmRes||
            VM_TIMEOUT == vmRes)
        {
            vmRes = vm_event_reset(&m_hwTaskDone);

            //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "HW Event");
            IncrementHWEventCounter();
            WakeUpThreads((mfxU32) MFX_INVALID_THREAD_ID, MFX_SCHEDULER_HW_BUFFER_COMPLETED);
        }
    }
}
