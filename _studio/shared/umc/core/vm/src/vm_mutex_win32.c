//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2009 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

#include "vm_mutex.h"

#include <windows.h>

/* Invalidate a mutex */
void vm_mutex_set_invalid(vm_mutex *mutex)
{
    /* check error(s) */
    if (NULL == mutex)
    {
        return;
    }

    memset(mutex, 0, sizeof(vm_mutex));

} /* void vm_mutex_set_invalid(vm_mutex *mutex) */

/* Verify if a mutex is valid */
Ipp32s vm_mutex_is_valid(vm_mutex *mutex)
{
    /* check error(s) */
    if (NULL == mutex)
    {
        return 0;
    }

    return (mutex->handle) ? (1) : (0);

} /* Ipp32s vm_mutex_is_valid(vm_mutex *mutex) */

/* Init a mutex, return 1 if successful */
vm_status vm_mutex_init(vm_mutex *mutex)
{
    /* check error(s) */
    if (NULL == mutex)
    {
        return VM_NULL_PTR;
    }

    /* before the initialization destroy the previous instance */
    vm_mutex_destroy(mutex);
    /* allocate the critical section structure */
    mutex->handle = malloc(sizeof(CRITICAL_SECTION));
    if (NULL == mutex->handle)
    {
        return VM_OPERATION_FAILED;
    }
    /* initialize the critical section */
    __try
    {
        InitializeCriticalSection(mutex->handle);
    }
    /* exception raised when memory is too low */
    __except(1)
    {
        free(mutex->handle);
        mutex->handle = NULL;
        return VM_OPERATION_FAILED;
    }

    return VM_OK;

} /* vm_status vm_mutex_init(vm_mutex *mutex) */

/* Lock the mutex with blocking. */
vm_status vm_mutex_lock(vm_mutex *mutex)
{
    /* check error(s) */
    if (NULL == mutex)
    {
        return VM_NULL_PTR;
    }
    if (NULL == mutex->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    /* lock the critical section */
    __try
    {
        EnterCriticalSection(mutex->handle);
    }
    __except(1)
    {
        return VM_OPERATION_FAILED;
    }

    return VM_OK;

} /* vm_status vm_mutex_lock(vm_mutex *mutex) */

/* Unlock the mutex. */
vm_status vm_mutex_unlock(vm_mutex *mutex)
{
    /* check error(s) */
    if (NULL == mutex)
    {
        return VM_NULL_PTR;
    }
    if (NULL == mutex->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    /* lock the critical section */
    LeaveCriticalSection(mutex->handle);

    return VM_OK;

} /* vm_status vm_mutex_unlock(vm_mutex *mutex) */

/* Lock the mutex without blocking, return 1 if success */
vm_status vm_mutex_try_lock(vm_mutex *mutex)
{
    BOOL bRes;

    /* check error(s) */
    if (NULL == mutex)
    {
        return VM_NULL_PTR;
    }
    if (NULL == mutex->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    /* lock the critical section */
    bRes = TryEnterCriticalSection(mutex->handle);
    if (TRUE != bRes)
    {
        return VM_OPERATION_FAILED;
    }

    return VM_OK;

} /* vm_status vm_mutex_try_lock(vm_mutex *mutex) */

/* Destroy the mutex */
void vm_mutex_destroy(vm_mutex *mutex)
{
    /* check error(s) */
    if ((NULL == mutex) ||
        (NULL == mutex->handle))
    {
        return;
    }

    /* deinitialize the critical section */
    DeleteCriticalSection(mutex->handle);
    /* delete the critical section structure */
    free(mutex->handle);

    mutex->handle = NULL;

} /* void vm_mutex_destroy(vm_mutex *mutex) */

#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */
