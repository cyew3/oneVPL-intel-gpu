/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.
//
*/

#if defined(LINUX32) || defined(__APPLE__)

#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "vm_cond.h"

static void vm_cond_set_invalid_internal(vm_cond *cond)
{
    memset(cond, 0, sizeof(vm_cond));
}

/* Invalidate a condvar */
void vm_cond_set_invalid(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return;
    }

    vm_cond_set_invalid_internal(cond);
} /* void vm_cond_set_invalid(vm_cond *cond) */

/* Verify if a condvar is valid */
Ipp32s vm_cond_is_valid(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
    {
        return 0;
    }

    return cond->is_valid;
} /* Ipp32s vm_cond_is_valid(vm_cond *cond) */

/* Init a condvar, return 1 if successful */
vm_status vm_cond_init(vm_cond *cond)
{
//    pthread_condattr_t cond_attr;
    int res = 0;

    /* check error(s) */
    if (NULL == cond)
        return VM_NULL_PTR;

    vm_cond_destroy(cond);

//    res = pthread_condattr_init(&cond_attr);
//    if (!res)
    {
        res = pthread_cond_init(&cond->handle, NULL);
        cond->is_valid = !res;
        if (res)
        {
            vm_cond_set_invalid_internal(cond);
        }
//        pthread_condattr_destroy(&mcond_attr);
    }
    return (res)? VM_OPERATION_FAILED: VM_OK;
} /* vm_status vm_cond_init(vm_cond *cond) */

/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_wait(vm_cond *cond, vm_mutex *mutex)
{
    vm_status umc_res = VM_NOT_INITIALIZED;

    /* check error(s) */
    if (NULL == cond || NULL == mutex)
        return VM_NULL_PTR;

    if (cond->is_valid && mutex->is_valid)
    {
        if (0 == pthread_cond_wait(&cond->handle, &mutex->handle))
            umc_res = VM_OK;
        else
            umc_res = VM_OPERATION_FAILED;
    }
    return umc_res;
} /* vm_status vm_cond_wait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec) */


/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_timedwait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec)
{
    vm_status umc_res = VM_NOT_INITIALIZED;

    /* check error(s) */
    if (NULL == cond || NULL == mutex)
        return VM_NULL_PTR;

    if (cond->is_valid && mutex->is_valid)
    {
        struct timeval tval;
        struct timespec tspec;
        Ipp32s res;
        Ipp64u micro_sec;

        gettimeofday(&tval, NULL);
        // NOTE: micro_sec _should_ be Ipp64u, not Ipp32u to avoid overflow
        micro_sec = 1000 * msec + tval.tv_usec;
        tspec.tv_sec = tval.tv_sec + (Ipp32u)(micro_sec / 1000000);
        tspec.tv_nsec = (Ipp32u)(micro_sec % 1000000) * 1000;

        res = pthread_cond_timedwait(&cond->handle, &mutex->handle, &tspec);
        if (0 == res)
            umc_res = VM_OK;
        else if (ETIMEDOUT == res)
            umc_res = VM_TIMEOUT;
        else
            umc_res = VM_OPERATION_FAILED;
    }
    return umc_res;
} /* vm_status vm_cond_timedwait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec) */

/* Wake a single thread waiting on the specified condition variable */
vm_status vm_cond_signal(vm_cond *cond)
{
    vm_status umc_res = VM_NOT_INITIALIZED;

    /* check error(s) */
    if (NULL == cond)
        return VM_NULL_PTR;

    if (cond->is_valid )
    {
        if (0 == pthread_cond_signal(&cond->handle))
            umc_res = VM_OK;
        else
            umc_res = VM_OPERATION_FAILED;
    }
    return umc_res;
} /* vm_status vm_cond_signal(vm_cond *cond) */

/* Wake all threads waiting on the specified condition variable */
vm_status vm_cond_broadcast(vm_cond *cond)
{
    vm_status umc_res = VM_NOT_INITIALIZED;

    /* check error(s) */
    if (NULL == cond)
        return VM_NULL_PTR;

    if (cond->is_valid )
    {
        if (0 == pthread_cond_broadcast(&cond->handle))
            umc_res = VM_OK;
        else
            umc_res = VM_OPERATION_FAILED;
    }
    return umc_res;
} /* vm_status vm_cond_broadcast(vm_cond *cond) */

/* Destroy the condvar */
void vm_cond_destroy(vm_cond *cond)
{
    /* check error(s) */
    if (NULL == cond)
        return;

    if (cond->is_valid)
    {
        int res = pthread_cond_destroy(&cond->handle);
        assert(!res); // we experienced undefined behavior
        vm_cond_set_invalid_internal(cond);
    }
} /* void vm_cond_destroy(vm_cond *cond) */

#else
# pragma warning( disable: 4206 )
#endif /* LINUX32 */
