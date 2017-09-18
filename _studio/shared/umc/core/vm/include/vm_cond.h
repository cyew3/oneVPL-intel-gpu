//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015-2017 Intel Corporation. All Rights Reserved.
//

#ifndef __VM_COND_H__
#define __VM_COND_H__

#include "vm_types.h"
#include "vm_mutex.h"
#include "vm_time.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Invalidate a condvar */
void vm_cond_set_invalid(vm_cond *cond);

/* Verify if a condvar is valid */
Ipp32s  vm_cond_is_valid(vm_cond *cond);

/* Init a condvar, return VM_OK if success */
vm_status vm_cond_init(vm_cond *cond);

/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_wait(vm_cond *cond, vm_mutex *mutex);

/* Sleeps on the specified condition variable and releases the specified critical section as an atomic operation */
vm_status vm_cond_timedwait(vm_cond *cond, vm_mutex *mutex, Ipp32u msec);


vm_status vm_cond_timed_uwait(vm_cond *cond, vm_mutex *mutex, vm_tick usec);

/* Wake a single thread waiting on the specified condition variable */
vm_status vm_cond_signal(vm_cond *cond);

/* Wake all threads waiting on the specified condition variable */
vm_status vm_cond_broadcast(vm_cond *cond);

/* Destroy the condvar */
void vm_cond_destroy(vm_cond *cond);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_COND_H__ */
