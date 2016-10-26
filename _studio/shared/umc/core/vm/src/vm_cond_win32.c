//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

#include "vm_cond.h"

#include <windows.h>

#if (_WIN32_WINNT >= 0x0600)

/* Invalidate a condvar */
void vm_cond_set_invalid(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return;
    }

    memset(cond, 0, sizeof(vm_cond));

} /* void vm_cond_set_invalid(vm_cond *cond) */

/* Verify if a condvar is valid */
Ipp32s vm_cond_is_valid(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return 0;
    }

    return (cond->handle) ? (1) : (0);

} /* Ipp32s vm_cond_is_valid(vm_cond *cond) */

/* Init a condvar, return 1 if successful */
vm_status vm_cond_init(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return VM_NULL_PTR;
    }

    /* before the initialization destroy the previous instance */
    vm_cond_destroy(cond);
    /* allocate the condition variable structure */
    cond->handle = malloc(sizeof(CONDITION_VARIABLE));
    if (NULL == cond->handle)
    {
        return VM_OPERATION_FAILED;
    }
    /* initialize the critical section */
    __try
    {
        InitializeConditionVariable(cond->handle);
    }
    /* exception raised when memory is too low */
    __except(STATUS_NO_MEMORY)
    {
        free(cond->handle);
        cond->handle = NULL;
        return VM_OPERATION_FAILED;
    }

    return VM_OK;

} /* vm_status vm_cond_init(vm_cond *cond) */

/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_wait(vm_cond *cond, vm_mutex *mutex)
{
    /* check error(s) */
    if (!cond || !mutex)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle || !mutex->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    if (!SleepConditionVariableCS(cond->handle, mutex->handle, INFINITE))
        return VM_TIMEOUT;

    return VM_OK;

} /* vm_status vm_cond_wait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec) */


/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_timedwait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec)
{
    /* check error(s) */
    if (!cond || !mutex)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle || !mutex->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    if (!SleepConditionVariableCS(cond->handle, mutex->handle, msec))
        return VM_TIMEOUT;

    return VM_OK;

} /* vm_status vm_cond_timedwait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec) */

/* Wake a single thread waiting on the specified condition variable */
vm_status vm_cond_signal(vm_cond *cond)
{
    /* check error(s) */
    if (!cond)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    WakeConditionVariable(cond->handle);

    return VM_OK;

} /* vm_status vm_cond_signal(vm_cond *cond) */

/* Wake all threads waiting on the specified condition variable */
vm_status vm_cond_broadcast(vm_cond *cond)
{
    /* check error(s) */
    if (!cond)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    WakeAllConditionVariable(cond->handle);

    return VM_OK;

} /* vm_status vm_cond_broadcast(vm_cond *cond) */

/* Destroy the condvar */
void vm_cond_destroy(vm_cond *cond)
{
    /* check error(s) */
    if ((!cond) ||
        (!cond->handle))
    {
        return;
    }

    /* delete the condition variable structure */
    free(cond->handle);

    cond->handle = NULL;

} /* void vm_cond_destroy(vm_cond *cond) */


#else
// NOT IMPLEMENTED YET ///

/* Invalidate a condvar */
void vm_cond_set_invalid(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return;
    }

    memset(cond, 0, sizeof(vm_cond));

} /* void vm_cond_set_invalid(vm_cond *cond) */

/* Verify if a condvar is valid */
Ipp32s vm_cond_is_valid(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return 0;
    }

    return (cond->handle) ? (1) : (0);

} /* Ipp32s vm_cond_is_valid(vm_cond *cond) */

/* Init a condvar, return 1 if successful */
vm_status vm_cond_init(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return VM_NULL_PTR;
    }

    /* before the initialization destroy the previous instance */
    vm_cond_destroy(cond);

    // NOT IMPLEMENTED YET ///
    return VM_OPERATION_FAILED;

} /* vm_status vm_cond_init(vm_cond *cond) */

/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_wait(vm_cond *cond, vm_mutex *mutex)
{
    /* check error(s) */
    if (!cond || !mutex)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle || !mutex->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    // NOT IMPLEMENTED YET ///
    return VM_OPERATION_FAILED;

} /* vm_status vm_cond_wait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec) */


/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_timedwait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec)
{
    /* check error(s) */
    if (!cond || !mutex)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle || !mutex->handle)
    {
        return VM_NOT_INITIALIZED;
    }
    msec;

    // NOT IMPLEMENTED YET ///
    return VM_OPERATION_FAILED;

} /* vm_status vm_cond_timedwait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec) */

/* Wake a single thread waiting on the specified condition variable */
vm_status vm_cond_signal(vm_cond *cond)
{
    /* check error(s) */
    if (!cond)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    // NOT IMPLEMENTED YET ///
    return VM_OPERATION_FAILED;

} /* vm_status vm_cond_signal(vm_cond *cond) */

/* Wake all threads waiting on the specified condition variable */
vm_status vm_cond_broadcast(vm_cond *cond)
{
    /* check error(s) */
    if (!cond)
    {
        return VM_NULL_PTR;
    }
    if (!cond->handle)
    {
        return VM_NOT_INITIALIZED;
    }

    // NOT IMPLEMENTED YET ///
    return VM_OPERATION_FAILED;
} /* vm_status vm_cond_broadcast(vm_cond *cond) */

/* Destroy the condvar */
void vm_cond_destroy(vm_cond *cond)
{
    /* check error(s) */
    if ((!cond) ||
        (!cond->handle))
    {
        return;
    }

    cond->handle = NULL;
} /* void vm_cond_destroy(vm_cond *cond) */

#endif

#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */
