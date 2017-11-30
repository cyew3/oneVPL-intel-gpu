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
#include <umc_event.h>

// forward declaration of the owning class
class mfxSchedulerCore;

struct MFX_SCHEDULER_THREAD_CONTEXT
{
    MFX_SCHEDULER_THREAD_CONTEXT()
      : pSchedulerCore(NULL)
      , threadNum(0)
      , threadHandle()
      , workTime(0)
      , sleepTime(0)
    {
        vm_thread_set_invalid(&threadHandle);
    }

    mfxSchedulerCore *pSchedulerCore;                           // (mfxSchedulerCore *) pointer to the owning core
    mfxU32 threadNum;                                           // (mfxU32) number of the thread
    vm_thread threadHandle;                                     // (vm_thread) handle to the thread
    UMC::Event taskAdded;                                       // Objects for waiting on in case of nothing to do

    mfxU64 workTime;                                            // (mfxU64) integral working time
    mfxU64 sleepTime;                                           // (mfxU64) integral sleeping time
};

#endif // #ifndef __MFX_SCHEDULER_CORE_THREAD_H
