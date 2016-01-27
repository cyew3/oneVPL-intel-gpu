/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_TASK_H
#define __MFX_TASK_H

#include <mfxplugin.h>
#include <mfx_thread_task.h>
#include <mfx_task_threading_policy.h>

enum
{
    // Declare the maximum number of source or destination dependencies
    MFX_TASK_NUM_DEPENDENCIES   = 4
};

enum
{
    // Declare overall number of priorities
    MFX_PRIORITY_NUMBER = MFX_PRIORITY_HIGH + 1

};

// Declare implementation types
enum
{
    MFX_TYPE_HARDWARE = 0,
    MFX_TYPE_SOFTWARE = 1,
    MFX_TYPE_NUMBER
};

#define MFX_TASK_NEED_CONTINUE MFX_TASK_WORKING

enum
{
    MFX_TRACE_ID_DECODE  = 0x10000000,
    MFX_TRACE_ID_VPP     = 0x20000000,
    MFX_TRACE_ID_VPP2    = 0x30000000,
    MFX_TRACE_ID_ENCODE  = 0x40000000,
    MFX_TRACE_ID_ENCODE2 = 0x50000000,
    MFX_TRACE_ID_USER    = 0x60000000,
};

inline
bool isFailed(mfxStatus mfxRes)
{
    return (MFX_ERR_NONE > mfxRes);

} // bool isFailed(mfxStatus mfxRes)

/* declare type for MFX task callbacks.
Usually, most thread API involve a single parameter.
MFX callback function require 3 parameters to make developers' life.
pState - is a pointer to the performing object or something like this.
pParam - is an addition parameter to destingush one task from the another inside
    the entry point. It is possible to start different threads with the same
    object (pState), but different task (pParam). Usually, it is a handle or some
    internal pointer.
threadNumber - number of the current additional thread. It can be any number
    not exceeding the maximum required threads for given task. */
typedef mfxStatus (*mfxTaskRoutine) (void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
typedef mfxStatus (*mfxTaskCompleteProc) (void *pState, void *pParam, mfxStatus taskRes);
typedef mfxStatus (*mfxGetSubTaskProc) (void *pState, void *pParam, void **ppSubTask);
typedef mfxStatus (*mfxCompleteSubTaskProc) (void *pState, void *pParam, void *pSubTask, mfxStatus taskRes);

typedef
struct MFX_ENTRY_POINT
{
    // pointer to the task processing object
    void *pState;
    // pointer to the task's parameter
    void *pParam;

    // pointer to the task processing routine
    mfxTaskRoutine pRoutine;
    // pointer to the task completing procedure
    mfxTaskCompleteProc pCompleteProc;

    // pointer to get a sub-task from the component (NON-OBLIGATORY)
    mfxGetSubTaskProc pGetSubTaskProc;
    // sub-task is complete. Update the status of it (NON-OBLIGATORY)
    mfxCompleteSubTaskProc pCompleteSubTaskProc;

    // number of simultaneously allowed threads for the task
    mfxU32 requiredNumThreads;

    // name of routine - for debug and tracing purpose
    const char *pRoutineName;
} MFX_ENTRY_POINT;

struct MFX_TASK
{
    // Pointer to task owning object
    void *pOwner;

    // Task parameters provided by the component
    MFX_ENTRY_POINT entryPoint;

    // legacy parameters should be eliminated
    bool bObsoleteTask;
    MFX_THREAD_TASK_PARAMETERS obsolete_params;

    // these are not a source/destination parameters,
    // these are only in/out dependencies.

    // Array of source dependencies
    const void *(pSrc[MFX_TASK_NUM_DEPENDENCIES]);
    // Array of destination dependencies
    void *(pDst[MFX_TASK_NUM_DEPENDENCIES]);

    // Task's priority
    mfxPriority priority;
    // how the object processes the tasks
    mfxTaskThreadingPolicy threadingPolicy;

    unsigned int nTaskId;
    unsigned int nParentId;
};

#endif // __MFX_TASK_H
