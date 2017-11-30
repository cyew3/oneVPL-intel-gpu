//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2017 Intel Corporation. All Rights Reserved.
//

#include <mfx_scheduler_core_task.h>
#include <mfx_scheduler_core.h>


#include <memory.h>

MFX_SCHEDULER_TASK::MFX_SCHEDULER_TASK(mfxU32 taskID, mfxSchedulerCore *pSchedulerCore) :
    taskID(taskID),
    jobID(0),
    pNext(NULL),
    m_pSchedulerCore(pSchedulerCore)
{
    vm_cond_set_invalid(&done);
    if (VM_OK != vm_cond_init(&done)) {
        throw std::bad_alloc();
    }
    // reset task parameters
    memset(&param, 0, sizeof(param));
}

MFX_SCHEDULER_TASK::~MFX_SCHEDULER_TASK(void)
{
    vm_cond_destroy(&done);
}

mfxStatus MFX_SCHEDULER_TASK::Reset(void)
{
    // reset task parameters
    memset(&param, 0, sizeof(param));

    opRes = MFX_WRN_IN_EXECUTION;
    curStatus = MFX_TASK_WORKING;

    return MFX_ERR_NONE;

} // mfxStatus MFX_SCHEDULER_TASK::Reset(void)

void MFX_SCHEDULER_TASK::OnDependencyResolved(mfxStatus result)
{
    if (isFailed(result))
    {

        // waiting task inherits status from the parent task
        // need to propogate error status to all dependent tasks.
        //if (MFX_TASK_WAIT & param.task.threadingPolicy)
        {
            opRes = result;
            curStatus = result;
        }
        // all other tasks are aborted
        //else
        //{
        //    opRes = MFX_ERR_ABORTED;
        //    curStatus = MFX_ERR_ABORTED;
        //}

        // need to update dependency table for all tasks dependent from failed 
        m_pSchedulerCore->ResolveDependencyTable(this);
        vm_cond_broadcast(&done);

        // release the current task resources
        ReleaseResources();

        CompleteTask(MFX_ERR_ABORTED);
    }

    // call the parent's method
    mfxDependencyItem<MFX_TASK_NUM_DEPENDENCIES>::OnDependencyResolved(result);

}

mfxStatus MFX_SCHEDULER_TASK::CompleteTask(mfxStatus res)
{
    mfxStatus sts;
    MFX_ENTRY_POINT &entryPoint = param.task.entryPoint;

    if (!entryPoint.pCompleteProc) return MFX_ERR_NONE;

    try {
        // release the component's resources
        sts = entryPoint.pCompleteProc(
            entryPoint.pState,
            entryPoint.pParam,
            res);
    } catch(...) {
        sts = MFX_ERR_UNKNOWN;
    }
    return sts;
}

void MFX_SCHEDULER_TASK::ReleaseResources(void)
{
    if (param.pThreadAssignment)
    {
        param.pThreadAssignment->m_numRefs -= 1;
        if (param.pThreadAssignment->pLastTask == this)
        {
            param.pThreadAssignment->pLastTask = NULL;
        }
    }

    // thread assignment info is not required for the task any more
    param.pThreadAssignment = NULL;

} // void MFX_SCHEDULER_TASK::ReleaseResources(void)
