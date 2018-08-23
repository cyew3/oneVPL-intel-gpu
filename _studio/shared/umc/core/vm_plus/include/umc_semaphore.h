//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_SEMAPHORE_H__
#define __UMC_SEMAPHORE_H__

#include "vm_debug.h"
#include "vm_semaphore.h"
#include "umc_structures.h"

namespace UMC
{

class Semaphore
{
public:
    // Default constructor
    Semaphore(void);
    // Destructor
    virtual ~Semaphore(void);

    // Initialize semaphore
    Status Init(int32_t iInitCount);
    Status Init(int32_t iInitCount, int32_t iMaxCount);
    // Check semaphore state
    bool IsValid(void);
    // Try to obtain semaphore
    Status TryWait(void);
    // Wait until semaphore is signaled
    Status Wait(uint32_t msec);
    // Infinitely wait until semaphore is signaled
    Status Wait(void);
    // Set semaphore to signaled state
    Status Signal(uint32_t count = 1);

protected:
    vm_semaphore m_handle;                                      // (vm_semaphore) handle to system semaphore
};

inline
bool Semaphore::IsValid(void)
{
    return vm_semaphore_is_valid(&m_handle) ? true : false;

} // bool Semaphore::IsValid(void)

inline
Status Semaphore::TryWait(void)
{
    return vm_semaphore_try_wait(&m_handle);

} // Status Semaphore::TryWait(void)

inline
Status Semaphore::Wait(uint32_t msec)
{
    return vm_semaphore_timedwait(&m_handle, msec);

} // Status Semaphore::Wait(uint32_t msec)

inline
Status Semaphore::Wait(void)
{
    return vm_semaphore_wait(&m_handle);

} // Status Semaphore::Wait(void)

inline
Status Semaphore::Signal(uint32_t count)
{
    return vm_semaphore_post_many(&m_handle, count);

} // Status Semaphore::Signal(uint32_t count)

} // namespace UMC

#endif // __UMC_SEMAPHORE_H__
