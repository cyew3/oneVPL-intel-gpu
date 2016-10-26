//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2013 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_SCHEDULER_CORE_HANDLE_H)
#define __MFX_SCHEDULER_CORE_HANDLE_H

#include "vm_types.h"

enum
{
    MFX_BITS_FOR_HANDLE = 32,

    // declare constans for task objects
    MFX_BITS_FOR_TASK_NUM = 10,
    MFX_MAX_NUMBER_TASK = 1 << MFX_BITS_FOR_TASK_NUM,

    // declare constans for job objects
    MFX_BITS_FOR_JOB_NUM = MFX_BITS_FOR_HANDLE - MFX_BITS_FOR_TASK_NUM,
    MFX_MAX_NUMBER_JOB = 1 << MFX_BITS_FOR_JOB_NUM
};

// Type mfxTaskHandle is a composite type,
// which structure is close to a handle structure.
// Few LSBs are used for internal task object indentification.
// Rest bits are used to identify job.
typedef
union mfxTaskHandle
{
    struct
    {
    unsigned int taskID : MFX_BITS_FOR_TASK_NUM;
    unsigned int jobID : MFX_BITS_FOR_JOB_NUM;
    };

    size_t handle;

} mfxTaskHandle;

#endif // !defined(__MFX_SCHEDULER_CORE_HANDLE_H)
