// Copyright (c) 2003-2018 Intel Corporation
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
int32_t vm_mutex_is_valid(vm_mutex *mutex)
{
    /* check error(s) */
    if (NULL == mutex)
    {
        return 0;
    }

    return (mutex->handle) ? (1) : (0);

} /* int32_t vm_mutex_is_valid(vm_mutex *mutex) */

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
