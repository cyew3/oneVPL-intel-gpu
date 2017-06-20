//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2017 Intel Corporation. All Rights Reserved.

#ifndef __MFX_SCHEDULER_CORE_THREAD_H
#define __MFX_SCHEDULER_CORE_THREAD_H

#include <mfxdefs.h>

#include <vm_thread.h>
#include <vm_cond.h>

// forward declaration of the owning class
class mfxSchedulerCore;

struct MFX_SCHEDULER_THREAD_CONTEXT
{
    MFX_SCHEDULER_THREAD_CONTEXT()
      : state(State::Waiting)
      , pSchedulerCore(NULL)
      , threadNum(0)
      , threadHandle()
      , workTime(0)
      , sleepTime(0)
    {
        vm_thread_set_invalid(&threadHandle);
        vm_cond_set_invalid(&taskAdded);
    }

    enum State {
        Waiting, // thread is waiting for incoming tasks
        Running  // thread is executing a task
    };

    State state;                      // thread state, waiting or running
    mfxSchedulerCore *pSchedulerCore; // pointer to the owning core
    mfxU32 threadNum;                 // thread number assigned by the core
    vm_thread threadHandle;           // thread handle
    vm_cond taskAdded;                // cond. variable to signal new tasks

    mfxU64 workTime;                  // integral working time
    mfxU64 sleepTime;                 // integral sleeping time
};

#endif // #ifndef __MFX_SCHEDULER_CORE_THREAD_H
