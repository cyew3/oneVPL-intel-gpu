// Copyright (c) 2010-2020 Intel Corporation
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

#include <mfx_scheduler_core.h>

#include <mfx_scheduler_core_task.h>
#include <mfx_scheduler_core_handle.h>

#include <vm_time.h>
#include <vm_sys_info.h>
#include <mfx_trace.h>
#include <cassert>

#if defined  (MFX_D3D11_ENABLED)
#include "mfx_scheduler_dx11_event.h"
#endif

#if defined(MFX_SCHEDULER_LOG)
#if defined(MFX_VA_WIN)
#include <windows.h>
#else
#if defined(MFX_VA_LINUX)
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <time.h>
#endif
#endif
#include <stdio.h>
#endif // defined(MFX_SCHEDULER_LOG)



enum
{
    MFX_TIME_INFINITE           = 0x7fffffff,
    MFX_TIME_TO_WAIT            = 5
};

mfxStatus mfxSchedulerCore::Initialize(const MFX_SCHEDULER_PARAM *pParam)
{
    MFX_SCHEDULER_PARAM2 param2;
    memset(&param2, 0, sizeof(param2));
    if (pParam) {
        MFX_SCHEDULER_PARAM* casted_param2 = &param2;
        *casted_param2 = *pParam;
    }
    if (!param2.numberOfThreads) {
        // that's just in case: core, which calls scheduler, is doing
        // exactly the same what we are doing below
        param2.numberOfThreads = vm_sys_info_get_cpu_num();
    }
    return Initialize2(&param2);
}

mfxStatus mfxSchedulerCore::Initialize2(const MFX_SCHEDULER_PARAM2 *pParam)
{
    UMC::Status umcRes;
    mfxU32 i;

    // release the object before initialization
    Close();


    // save the parameters
    if (pParam)
    {
        m_param = *pParam;
    }

#if defined(MFX_SCHEDULER_LOG)
    {
        char cPath[256];
#if defined(MFX_VA_WIN)
        SYSTEMTIME time;

        GetLocalTime(&time);
        // create the log path
        sprintf_s(cPath, sizeof(cPath),
                  "c:\\temp\\mfx_scheduler_log(%02u-%02u-%02u %x).txt",
                  time.wHour, time.wMinute, time.wSecond,
                  this);
#else
#if defined(MFX_VA_LINUX)
        time_t seconds;
        tm s_time;

        time(&seconds);
        localtime_r(&seconds, &s_time);

        sprintf(cPath, "~/temp/mfx_scheduler_log(%02u-%02u-%02u %x).txt", s_time.tm_hour, s_time.tm_min, s_time.tm_sec);
#endif
#endif
        // initialize the log
        m_hLog = mfxLogInit(cPath);
    }
#endif // defined(MFX_SCHEDULER_LOG)

    // clean up the task look up table
    m_ppTaskLookUpTable.resize(MFX_MAX_NUMBER_TASK, nullptr);

    // allocate the dependency table
    m_pDependencyTable.resize(MFX_MAX_NUMBER_TASK * 2, MFX_DEPENDENCY_ITEM());

    // allocate the thread assignment object table.
    // its size should be equal to the number of task,
    // larger table is not required.
    m_occupancyTable.resize(MFX_MAX_NUMBER_TASK, MFX_THREAD_ASSIGNMENT());

    if (MFX_SINGLE_THREAD != m_param.flags)
    {
        if (m_param.numberOfThreads && m_param.params.NumThread) {
            // use user-overwritten number of threads
            m_param.numberOfThreads = m_param.params.NumThread;
        }
#if !defined(MFX_EXTERNAL_THREADING)
        if (!m_param.numberOfThreads) {
            return MFX_ERR_UNSUPPORTED;
        }
#endif
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
        if (m_param.numberOfThreads == 1) {
            // we need at least 2 threads to avoid dead locks
            return MFX_ERR_UNSUPPORTED;
        }
#endif

#if defined(MFX_EXTERNAL_THREADING)
        // decide the number of threads
        mfxU32 numThreads = m_param.numberOfThreads;

        try
        {
            // allocate 'free task' event
            umcRes = m_freeTasks.Init(MFX_MAX_NUMBER_TASK, MFX_MAX_NUMBER_TASK);
            if (UMC::UMC_OK != umcRes)
            {
                return MFX_ERR_UNKNOWN;
            }

            // initialize the waiting objects, allocate and start threads
            m_param.numberOfThreads = 0;
            for (i = 0; i < numThreads; i += 1)
            {
                MFX_SCHEDULER_THREAD_CONTEXT * pContext = new MFX_SCHEDULER_THREAD_CONTEXT;

                // prepare context
                pContext->pSchedulerCore = this;
                pContext->threadNum = AddThreadToPool(pContext); // m_param.numberOfThreads modified here
            }

        }
        catch(...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }

#else // MFX_EXTERNAL_THREADING

        try
        {
            // allocate 'free task' event
            umcRes = m_freeTasks.Init(MFX_MAX_NUMBER_TASK, MFX_MAX_NUMBER_TASK);
            if (UMC::UMC_OK != umcRes)
            {
                return MFX_ERR_UNKNOWN;
            }

            // allocate thread contexts
            m_pThreadCtx = new MFX_SCHEDULER_THREAD_CONTEXT[m_param.numberOfThreads];

            // start threads
            for (i = 0; i < m_param.numberOfThreads; i += 1)
            {
                // prepare context
                m_pThreadCtx[i].threadNum = i;
                m_pThreadCtx[i].pSchedulerCore = this;
                umcRes = m_pThreadCtx[i].taskAdded.Init(0, 0);
                if (UMC::UMC_OK != umcRes) {
                    return MFX_ERR_UNKNOWN;
                }
                // spawn a thread
                m_pThreadCtx[i].threadHandle = std::thread([this, i]() { ThreadProc(&m_pThreadCtx[i]); });
                if (!SetScheduling(m_pThreadCtx[i].threadHandle)) {
                    return MFX_ERR_UNKNOWN;
                }
            }
        }
        catch (...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }

#endif // MFX_EXTERNAL_THREADING

        SetThreadsAffinityToSockets();
    }
    else
    {
        // to run HW listen thread. Will be enabled if tests are OK
#if defined(MFX_VA_WIN)
        m_hwTaskDone.handle = CreateEventExW(NULL, 
            _T("Global\\IGFXKMDNotifyBatchBuffersComplete"), 
            CREATE_EVENT_MANUAL_RESET, 
            STANDARD_RIGHTS_ALL | EVENT_MODIFY_STATE);
#endif

    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::Initialize(mfxSchedulerFlags flags, mfxU32 numberOfThreads)

mfxStatus mfxSchedulerCore::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint)
{
    return AddTask(task, pSyncPoint, NULL, 0);

} // mfxStatus mfxSchedulerCore::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint)

mfxStatus mfxSchedulerCore::Synchronize(mfxSyncPoint syncPoint, mfxU32 timeToWait)
{
    mfxTaskHandle handle;
    mfxStatus mfxRes;

    // check error(s)
    if (NULL == syncPoint)
    {
        return MFX_ERR_NULL_PTR;
    }

    // cast the pointer to handle and make syncing
    handle.handle = (size_t) syncPoint;

#if defined(MFX_SCHEDULER_LOG)
    Ipp32u start = vm_time_get_current_time();
#endif // defined(MFX_SCHEDULER_LOG)

    // waiting on task's handle
    mfxRes = Synchronize(handle, timeToWait);

#if defined(MFX_SCHEDULER_LOG)

    Ipp32u stop = vm_time_get_current_time();

#if defined(MFX_VA_WIN)
    mfxLogWriteA(m_hLog,
                 "[% u4] WAIT task - %u, job - %u took %u msec (priority - %d)\n",
                 GetCurrentThreadId(),
                 handle.taskID,
                 handle.jobID,
                 stop - start,
                 GetThreadPriority(GetCurrentThread()));
#else
#if defined(MFX_VA_LINUX)
    mfxLogWriteA(m_hLog,
                 "[% u4] WAIT task - %u, job - %u took %u msec (priority - %d)\n",
                 syscall(SYS_gettid),
                 handle.taskID,
                 handle.jobID,
                 stop - start,
                 syscall(SYS_getpriority));
#endif
#endif

#endif // defined(MFX_SCHEDULER_LOG)

    return mfxRes;

} // mfxStatus mfxSchedulerCore::Synchronize(mfxSyncPoint syncPoint, mfxU32 timeToWait)

mfxStatus mfxSchedulerCore::Synchronize(mfxTaskHandle handle, mfxU32 timeToWait)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    // look up the task
    MFX_SCHEDULER_TASK *pTask = m_ppTaskLookUpTable.at(handle.taskID);

    if (nullptr == pTask)
    {
        return MFX_ERR_NULL_PTR;
    }

    if (MFX_SINGLE_THREAD == m_param.flags)
    {
        //let really run task to 
        MFX_CALL_INFO call = {};
        mfxTaskHandle previousTaskHandle = {};

        mfxStatus task_sts = MFX_ERR_NONE;
        mfxU64 start = vm_time_get_tick();
        mfxU64 frequency = vm_time_get_frequency();
#if defined(MFX_VA_WIN)

        DWORD tid = GetCurrentThreadId();
        {
            // lock
            std::lock_guard<std::mutex> guard(m_guard);
            m_timeToRunMap[tid] = timeToWait;
            // unlock
        }
#endif
        while (MFX_WRN_IN_EXECUTION == pTask->opRes)
        {
            task_sts = GetTask(call, previousTaskHandle, 0);

            if (task_sts != MFX_ERR_NONE)
                continue;

            call.res = call.pTask->entryPoint.pRoutine(call.pTask->entryPoint.pState,
                                                       call.pTask->entryPoint.pParam,
                                                       call.threadNum,
                                                       call.callNum);


            // save the previous task's handle
            previousTaskHandle = call.taskHandle;

            MarkTaskCompleted(&call, 0);

            // safe version of (vm_time_get_tick() - start) / (frequency / 1000)) >= timeToWait
            // 1000 needed to convert seconds (frequency) to ms (timeTowait)
            if (((mfxU64)vm_time_get_tick() * 1000) >= (start * 1000 + timeToWait * frequency))
                break;

#if defined(MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT)
            if (MFX_ERR_NONE_PARTIAL_OUTPUT == call.res) break;
            if (MFX_TASK_BUSY == call.res &&
                (call.pTask->threadingPolicy & MFX_TASK_POLLING)) continue; //respin task by skipping wait
#endif
            if (MFX_TASK_DONE!= call.res)
            {
                vm_status vmRes;
                vmRes = vm_event_timed_wait(&m_hwTaskDone, 15 /*ms*/);
                if (VM_OK == vmRes|| VM_TIMEOUT == vmRes)
                {
                    vmRes = vm_event_reset(&m_hwTaskDone);
                    IncrementHWEventCounter();
                }
            }
        }
#if defined(MFX_VA_WIN)
        {
            // lock
            std::lock_guard<std::mutex> guard(m_guard);
            m_timeToRunMap.erase(tid);
            // unlock
        }
#endif
        //
        // inspect the task
        //

        // pertial output is ready, but the task is not done
#if defined(MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT)
        const MFX_TASK& tsk = pTask->param.task;
        if(tsk.entryPoint.pOutputPostProc) { //if postprocessing required
            mfxStatus sts = tsk.entryPoint.pOutputPostProc(tsk.entryPoint.pState, tsk.entryPoint.pParam);
            MFX_CHECK_STS(sts);
        }

        if (MFX_ERR_NONE_PARTIAL_OUTPUT == pTask->opRes)
        {
            pTask->opRes = MFX_WRN_IN_EXECUTION;
            pTask->curStatus = MFX_TASK_NEED_CONTINUE;
            pTask->param.bWaiting = false;

            return MFX_ERR_NONE_PARTIAL_OUTPUT;
        }
#endif

        // the handle is outdated,
        // the previous task job is over and completed with successful status
        // NOTE: it make sense to read task result and job ID the following order.
        if ((MFX_ERR_NONE == pTask->opRes) ||
            (pTask->jobID != handle.jobID))
        {
            return MFX_ERR_NONE;
        }

        // wait the result on the event
        if (MFX_WRN_IN_EXECUTION == pTask->opRes)
        {
            return MFX_WRN_IN_EXECUTION;
        }

        // check error status
        if ((MFX_ERR_NONE != pTask->opRes) &&
            (pTask->jobID == handle.jobID))
        {
            return pTask->opRes;
        }

        // in all other cases task is complete
        return MFX_ERR_NONE;
    }
    else
    {
        std::unique_lock<std::mutex> guard(m_guard);

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "Scheduler::Wait");
        MFX_LTRACE_1(MFX_TRACE_LEVEL_SCHED, "^Depends^on", "%d", pTask->param.task.nParentId);
        MFX_LTRACE_I(MFX_TRACE_LEVEL_SCHED, timeToWait);

        pTask->done.wait_for(guard, std::chrono::milliseconds(timeToWait), [pTask, handle] {
            return (pTask->jobID != handle.jobID) || (MFX_WRN_IN_EXECUTION != pTask->opRes);
        });

#if defined(MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT)
        const MFX_TASK& tsk = pTask->param.task;
        if(tsk.entryPoint.pOutputPostProc) { //if postprocessing required
            mfxStatus sts = tsk.entryPoint.pOutputPostProc(tsk.entryPoint.pState, tsk.entryPoint.pParam);
            MFX_CHECK_STS(sts);
        }

        if (MFX_ERR_NONE_PARTIAL_OUTPUT == pTask->opRes)
        {
            pTask->opRes = MFX_WRN_IN_EXECUTION;
            pTask->curStatus = MFX_TASK_NEED_CONTINUE;
            pTask->param.bWaiting = false;
            WakeUpThreads();

            return MFX_ERR_NONE_PARTIAL_OUTPUT;
        }
#endif

        if (pTask->jobID == handle.jobID) {
            return pTask->opRes;
        } else {
            /* Notes:
             *  - task executes next job already, we _lost_ task status and can only assume that
             *  everything was OK or FAILED, we will assume that task succeeded
             */
            return MFX_ERR_NONE;
        }
    }
}

mfxStatus mfxSchedulerCore::GetTimeout(mfxU32& maxTimeToRun)
{
#if defined(MFX_VA_WIN)
    if (MFX_SINGLE_THREAD != m_param.flags)
        return MFX_ERR_UNSUPPORTED;

    DWORD tid = GetCurrentThreadId();

    {
        std::lock_guard<std::mutex> guard(m_guard);

        try
        {
            maxTimeToRun = m_timeToRunMap[tid];
        }
        catch (std::out_of_range e)
        {
            return MFX_ERR_NOT_FOUND;
        }
    }

    return MFX_ERR_NONE;
#else
    (void)maxTimeToRun;

    return MFX_ERR_UNSUPPORTED;
#endif
}

#if defined (MFX_ENABLE_GLOBAL_HW_EVENT)
void ** mfxSchedulerCore::GetHwEvent()
{
    return &m_hwTaskDone.handle;
}
#endif

mfxStatus mfxSchedulerCore::WaitForDependencyResolved(const void *pDependency)
{
    mfxTaskHandle waitHandle = {};
    bool bFind = false;

    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (NULL == pDependency)
    {
        return MFX_ERR_NONE;
    }

    // find a handle to wait
    {
        std::lock_guard<std::mutex> guard(m_guard);
        mfxU32 curIdx;

        for (curIdx = 0; curIdx < m_numDependencies; curIdx += 1)
        {
            if (m_pDependencyTable.at(curIdx).p == pDependency)
            {
                // get the handle before leaving protected section
                waitHandle.taskID = m_pDependencyTable[curIdx].pTask->taskID;
                waitHandle.jobID = m_pDependencyTable[curIdx].pTask->jobID;

                // handle is found, go to wait
                bFind = true;
                break;
            }
        }
        // leave the protected section
    }

    // the dependency is still in the table,
    // wait until it leaves the table.
    if (bFind)
    {
        return Synchronize(waitHandle, MFX_TIME_INFINITE);
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::WaitForDependencyResolved(const void *pDependency)

mfxStatus mfxSchedulerCore::WaitForAllTasksCompletion(const void *pOwner)
{
    mfxTaskHandle waitHandle;

    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (NULL == pOwner)
    {
        return MFX_ERR_NULL_PTR;
    }

    // make sure that threads are running
    {
        std::lock_guard<std::mutex> guard(m_guard);

        ResetWaitingTasks(pOwner);
    }
    WakeUpThreads();

    do
    {
        // reset waiting handle
        waitHandle.handle = 0;

        // enter protected section.
        // make searching in a separate code block,
        // to avoid dead-locks with other threads.
        {
            std::lock_guard<std::mutex> guard(m_guard);
            int priority;

            priority = MFX_PRIORITY_HIGH;
            do
            {
                int type;

                for (type = MFX_TYPE_HARDWARE; type <= MFX_TYPE_SOFTWARE; type += 1)
                {
                    const MFX_SCHEDULER_TASK *pTask = m_pTasks[priority][type];

                    // look through tasks list. Find the specified object.
                    while (pTask)
                    {
                        // the task with equal ID has been found.
                        if ((pTask->param.task.pOwner == pOwner) &&
                            (MFX_WRN_IN_EXECUTION == pTask->opRes))
                        {
                            // create wait handle to make waiting more precise
                            waitHandle.taskID = pTask->taskID;
                            waitHandle.jobID = pTask->jobID;
                            break;
                        }

                        // get the next task
                        pTask = pTask->pNext;
                    }
                }

            } while ((0 == waitHandle.handle) && (--priority >= MFX_PRIORITY_LOW));

            // leave the protected section
        }

        // wait for a while :-)
        // for a while and infinite - diffrent things )
        if (waitHandle.handle)
        {
            Synchronize(waitHandle, MFX_TIME_TO_WAIT);
        }

    } while (waitHandle.handle);

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::WaitForAllTasksCompletion(const void *pOwner)

mfxStatus mfxSchedulerCore::ResetWaitingStatus(const void *pOwner)
{
    // reset 'waiting' tasks belong to the given state
    ResetWaitingTasks(pOwner);

    // wake up sleeping threads
    WakeUpThreads();

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::ResetWaitingStatus(const void *pOwner)

mfxStatus mfxSchedulerCore::GetState(void)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::GetState(void)

mfxStatus mfxSchedulerCore::GetParam(MFX_SCHEDULER_PARAM *pParam)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (NULL == pParam)
    {
        return MFX_ERR_NULL_PTR;
    }

    // copy the parameters
    *pParam = m_param;

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::GetParam(MFX_SCHEDULER_PARAM *pParam)

mfxStatus mfxSchedulerCore::Reset(void)
{
    // check error(s)
#if !defined(MFX_EXTERNAL_THREADING)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
#endif

    if (NULL == m_pFailedTasks)
    {
        return MFX_ERR_NONE;
    }

    // enter guarded section
    {
        std::lock_guard<std::mutex> guard(m_guard);

        // clean up the working queue
        ScrubCompletedTasks(true);
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::Reset(void)

mfxStatus mfxSchedulerCore::AdjustPerformance(const mfxSchedulerMessage message)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    switch(message)
    {
        // reset the scheduler to the performance default state
    case MFX_SCHEDULER_RESET_TO_DEFAULTS:
        break;

    case MFX_SCHEDULER_START_HW_LISTENING:
        if (m_param.flags != MFX_SINGLE_THREAD)
        {
            mfxRes = StartWakeUpThread();
        }
        break;

    case MFX_SCHEDULER_STOP_HW_LISTENING:
        if (m_param.flags != MFX_SINGLE_THREAD)
        {
            mfxRes = StopWakeUpThread();
        }
        break;

        // unknown message
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }

    return mfxRes;

} // mfxStatus mfxSchedulerCore::AdjustPerformance(const mfxSchedulerMessage message)

#if defined(MFX_SCHEDULER_LOG)

const
char *taskType[] =
{
    "UNKNOWN (0)",
    "MFX_TASK_THREADING_INTRA",
    "MFX_TASK_THREADING_INTER",
    "UNKNOWN (3)",
    "UNKNOWN (4)",
    "MFX_TASK_THREADING_DEDICATED",
    "UNKNOWN (6)",
    "UNKNOWN (7)",
    "MFX_TASK_THREADING_SHARED",
    "UNKNOWN (9)",
    "UNKNOWN (10)",
    "UNKNOWN (11)",
    "UNKNOWN (12)",
    "UNKNOWN (13)",
    "UNKNOWN (14)",
    "UNKNOWN (15)",
    "UNKNOWN (16)",
    "UNKNOWN (17)",
    "UNKNOWN (18)",
    "UNKNOWN (19)",
    "UNKNOWN (20)",
    "UNKNOWN (21)",
    "MFX_TASK_THREADING_DEDICATED_WAIT"
};

void GetNumTasks(MFX_SCHEDULER_TASK * const (ppTasks[MFX_PRIORITY_NUMBER][MFX_TYPE_NUMBER]), TASK_STAT &stat)
{
    mfxU32 priority;

    memset(&stat, 0, sizeof(stat));

    for (priority = 0; priority < MFX_PRIORITY_NUMBER; priority += 1)
    {
        int type;

        for (type = MFX_TYPE_HARDWARE; type <= MFX_TYPE_SOFTWARE; type += 1)
        {
            const MFX_SCHEDULER_TASK *pTask = ppTasks[priority][type];

            // run over the tasks with particular priority
            while (pTask)
            {
                stat.total += 1;
                if ((MFX_WRN_IN_EXECUTION == pTask->curStatus) ||
                    (MFX_TASK_WORKING == pTask->curStatus) ||
                    (MFX_TASK_BUSY == pTask->curStatus))
                {
                    if (pTask->IsDependenciesResolved())
                    {
                        stat.ready += 1;
                    }
                    else
                    {
                        stat.waiting += 1;
                    }
                }
                else if (isFailed(pTask->curStatus))
                {
                    stat.failed += 1;
                }
                else
                {
                    stat.done += 1;
                }

                // advance the task pointer
                pTask = pTask->pNext;
            }
        }
    }

} // void GetNumTasks(MFX_SCHEDULER_TASK * const (ppTasks[MFX_PRIORITY_NUMBER][MFX_TYPE_NUMBER]), TASK_STAT &stat)

#endif // defined(MFX_SCHEDULER_LOG)

mfxStatus mfxSchedulerCore::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint,
                                    const char *pFileName, int lineNumber)
{
#ifdef MFX_TRACE_ENABLE
    MFX_LTRACE_1(MFX_TRACE_LEVEL_SCHED, "^Enqueue^", "%d", task.nTaskId);
#endif
    Ipp32u numThreads;
    mfxU32 requiredNumThreads;

#if defined  (MFX_D3D11_ENABLED)
    {
        std::lock_guard<std::mutex> guard(m_guard);
        if (!m_pdx11event && !m_hwTaskDone.handle)
        {
            m_pdx11event = new DX11GlobalEvent(m_param.pCore);
            m_hwTaskDone.handle = m_pdx11event->CreateBatchBufferEvent();

            //back original sleep
            if (!m_hwTaskDone.handle)
            {
                delete m_pdx11event;
                m_pdx11event = 0;
            }

            if (m_param.flags != MFX_SINGLE_THREAD)
                StartWakeUpThread();
        }
    }
#endif

    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if ((NULL == task.entryPoint.pRoutine) ||
        (NULL == pSyncPoint))
    {
        return MFX_ERR_NULL_PTR;
    }

    // make sure that there is enough free task objects
    m_freeTasks.Wait();

    // enter protected section
    {
        std::lock_guard<std::mutex> guard(m_guard);
        mfxStatus mfxRes;
        MFX_SCHEDULER_TASK *pTask, **ppTemp;
        mfxTaskHandle handle = {};
        MFX_THREAD_ASSIGNMENT *pAssignment = nullptr;
        mfxU32 occupancyIdx;
        int type;

        // Make sure that there is an empty task object

        mfxRes = AllocateEmptyTask();
        if (MFX_ERR_NONE != mfxRes)
        {
            // better to return error instead of WRN  (two-tasks per component scheme)
            return MFX_ERR_MEMORY_ALLOC;
        }

        // initialize the task
        m_pFreeTasks->ResetDependency();
        mfxRes = m_pFreeTasks->Reset();
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
        m_pFreeTasks->param.task = task;
        mfxRes = GetOccupancyTableIndex(occupancyIdx, &task);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
        if (m_occupancyTable.size() <= occupancyIdx)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        pAssignment = &(m_occupancyTable[occupancyIdx]);

        // update the thread assignment parameters
        if (MFX_TASK_INTRA & task.threadingPolicy)
        {
            // last entries in the dependency arrays must be empty
            if ((m_pFreeTasks->param.task.pSrc[MFX_TASK_NUM_DEPENDENCIES - 1]) ||
                (m_pFreeTasks->param.task.pDst[MFX_TASK_NUM_DEPENDENCIES - 1]))
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            // fill INTRA task dependencies
            m_pFreeTasks->param.task.pSrc[MFX_TASK_NUM_DEPENDENCIES - 1] = pAssignment->pLastTask;
            m_pFreeTasks->param.task.pDst[MFX_TASK_NUM_DEPENDENCIES - 1] = m_pFreeTasks;
            // update the last intra task pointer
            pAssignment->pLastTask = m_pFreeTasks;
        }
        // do not save the pointer to thread assigment instance
        // until all checking have been done
        m_pFreeTasks->param.pThreadAssignment = pAssignment;
        pAssignment->m_numRefs += 1;

        // saturate the number of available threads
        numThreads = m_pFreeTasks->param.task.entryPoint.requiredNumThreads;
        numThreads = (0 == numThreads) ? (m_param.numberOfThreads) : (numThreads);
        numThreads = UMC::get_min(m_param.numberOfThreads, numThreads);
        numThreads = (Ipp32u) UMC::get_min(sizeof(pAssignment->threadMask) * 8, numThreads);
        m_pFreeTasks->param.task.entryPoint.requiredNumThreads = numThreads;

        // set the advanced task's info
        m_pFreeTasks->param.sourceInfo.pFileName = pFileName;
        m_pFreeTasks->param.sourceInfo.lineNumber = lineNumber;
        // set the sync point for the task
        handle.taskID = m_pFreeTasks->taskID;
        handle.jobID = m_pFreeTasks->jobID;
        *pSyncPoint = (mfxSyncPoint) handle.handle;

        // Register task dependencies
        RegisterTaskDependencies(m_pFreeTasks);

#if defined(MFX_SCHEDULER_LOG)
#if defined(MFX_VA_WIN)
        mfxLogWriteA(m_hLog,
                     "[% 4u]  ADD %s task - %u, job - %u (priority - %d)\n",
                     GetCurrentThreadId(),
                     taskType[m_pFreeTasks->param.task.threadingPolicy],
                     m_pFreeTasks->taskID, m_pFreeTasks->jobID,
                     GetThreadPriority(GetCurrentThread()));
#else
#if defined(MFX_VA_LINUX)
        mfxLogWriteA(m_hLog,
                     "[% 4u]  ADD %s task - %u, job - %u (priority - %d)\n",
                     syscall(SYS_gettid),
                     taskType[m_pFreeTasks->param.task.threadingPolicy],
                     m_pFreeTasks->taskID, m_pFreeTasks->jobID,
                     syscall(SYS_getpriority));
#endif
#endif
        TASK_STAT stat;

        GetNumTasks(m_pTasks, stat);
        stat.total += 1;
        if (m_pFreeTasks->IsDependenciesResolved())
        {
            stat.ready += 1;
        }
        else
        {
            stat.waiting += 1;
        }
        mfxLogWriteA(m_hLog,
                     "[    ] Tasks: total - %u, ready - %u, waiting - %u, done - %u, failed - %u\n",
                     stat.total,
                     stat.ready,
                     stat.waiting,
                     stat.done,
                     stat.failed);
#endif // defined(MFX_SCHEDULER_LOG)

        //
        // move task to the corresponding task
        //

        // remove the task from the 'free' queue
        pTask = m_pFreeTasks;
        m_pFreeTasks = m_pFreeTasks->pNext;
        pTask->pNext = NULL;

        // find the end of the corresponding queue
        type = (task.threadingPolicy & MFX_TASK_DEDICATED) ? (MFX_TYPE_HARDWARE) : (MFX_TYPE_SOFTWARE);
        ppTemp = m_pTasks[task.priority] + type;
        while (*ppTemp)
        {
            ppTemp = &((*ppTemp)->pNext);
        }

        // add the task to the end of the corresponding queue
        *ppTemp = pTask;

        // reset all 'waiting' tasks to prevent freezing
        // so called 'permanent' tasks.
        ResetWaitingTasks(pTask->param.task.pOwner);

        // increment the number of available tasks
        (MFX_TASK_DEDICATED & task.threadingPolicy) ? (m_numHwTasks += 1) : (m_numSwTasks += 1);

        requiredNumThreads = GetNumResolvedSwTasks();
        // leave the protected section
    }

    // if there are tasks for waiting threads, then wake up these threads
    WakeUpNumThreads(requiredNumThreads, (mfxU32)MFX_INVALID_THREAD_ID);

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCore::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint,

mfxStatus mfxSchedulerCore::DoWork()
{
#if defined(MFX_EXTERNAL_THREADING)
    MFX_SCHEDULER_THREAD_CONTEXT * pContext = new MFX_SCHEDULER_THREAD_CONTEXT;
    pContext->pSchedulerCore = this;
    pContext->threadNum = AddThreadToPool(pContext);
    pContext->pSchedulerCore->ThreadProc(pContext);
    return MFX_ERR_NONE;
#else // !MFX_EXTERNAL_THREADING
    return MFX_ERR_UNSUPPORTED;
#endif // MFX_EXTERNAL_THREADING
} // mfxStatus mfxSchedulerCore::DoWork()


#if defined(MFX_EXTERNAL_THREADING)
mfxU32 mfxSchedulerCore::AddThreadToPool(MFX_SCHEDULER_THREAD_CONTEXT * pContext)
{
    std::lock_guard<std::mutex>  guard(m_guard);

    size_t thNumber = m_ppThreadCtx.size();
    m_ppThreadCtx.push_back(pContext);
    pContext->threadNum = (mfxU32)thNumber;
    pContext->taskAdded.Init(0, 0);

    m_param.numberOfThreads++;

    return (mfxU32)thNumber;
}

void mfxSchedulerCore::RemoveThreadFromPool(MFX_SCHEDULER_THREAD_CONTEXT * pContext)
{
    std::lock_guard<std::mutex> guard(m_guard);

    if (!pContext)
    {
        assert(!"Can't remove null context from pool");
        return;
    }

    size_t index = pContext->threadNum;

    if (index >= m_ppThreadCtx.size())
    {
        assert(!"Can't remove context with non-existing index");
        return;
    }
    delete m_ppThreadCtx[index];
    m_ppThreadCtx[index] = 0;

    m_param.numberOfThreads--;
}
#endif


