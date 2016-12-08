//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "vm_time.h"

#if defined(LINUX32) || defined(__APPLE__)

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>

/* yield the execution of current thread for msec miliseconds */
void vm_time_sleep(Ipp32u msec)
{
    if (msec)
        usleep(1000 * msec);
    else
        sched_yield();
} /* void vm_time_sleep(Ipp32u msec) */

Ipp32u vm_time_get_current_time(void)
{
#if 1
    struct timeval tval;

    if (0 != gettimeofday(&tval, NULL)) return 0;
    return 1000 * tval.tv_sec + (Ipp32u)((Ipp64f)tval.tv_usec/(Ipp64f)1000);
#else
    struct timespec tspec;

    /* Note: clock_gettime function will required librt.a library to link with */
    if (0 != clock_gettime(CLOCK_MONOTONIC, &tspec)) return (Ipp32u)-1;
    return 1000 * tspec.tv_sec + (Ipp32u)((Ipp64f)tspec.tv_nsec/(Ipp64f)1000000);
#endif
} /* Ipp32u vm_time_get_current_time(void) */

#define VM_TIME_MHZ 1000000

/* obtain the clock tick of an uninterrupted master clock */
vm_tick vm_time_get_tick(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (vm_tick)tv.tv_sec * (vm_tick)VM_TIME_MHZ + (vm_tick)tv.tv_usec;

} /* vm_tick vm_time_get_tick(void) */

/* obtain the master clock resolution */
vm_tick vm_time_get_frequency(void)
{
    return (vm_tick)VM_TIME_MHZ;

} /* vm_tick vm_time_get_frequency(void) */

/* Create the object of time measure */
vm_status vm_time_open(vm_time_handle *handle)
{
   if (NULL == handle)
       return VM_NULL_PTR;

   *handle = -1;
   return VM_OK;

} /* vm_status vm_time_open(vm_time_handle *handle) */

/* Initialize the object of time measure */
vm_status vm_time_init(vm_time *m)
{
   if (NULL == m)
       return VM_NULL_PTR;
   m->start = 0;
   m->diff = 0;
   m->freq = vm_time_get_frequency();
   return VM_OK;

} /* vm_status vm_time_init(vm_time *m) */

/* Close the object of time measure */
vm_status vm_time_close(vm_time_handle *handle)
{
   if (NULL == handle)
       return VM_NULL_PTR;

   *handle = -1;
   return VM_OK;

} /* vm_status vm_time_close(vm_time_handle *handle) */

/* Start the process of time measure */
vm_status vm_time_start(vm_time_handle handle, vm_time *m)
{
   if (NULL == m)
       return VM_NULL_PTR;

   /*  touch unreferenced parameters.
       Take into account Intel's compiler. */
   handle = handle;

   m->start = vm_time_get_tick();
   return VM_OK;

} /* vm_status vm_time_start(vm_time_handle handle, vm_time *m) */

/* Stop the process of time measure and obtain the sampling time in seconds */
Ipp64f vm_time_stop(vm_time_handle handle, vm_time *m)
{
   Ipp64f speed_sec;
   Ipp64s end;

   /*  touch unreferenced parameters.
       Take into account Intel's compiler. */
   handle = handle;

   end = vm_time_get_tick();
   m->diff += (end - m->start);

   if(m->freq == 0) m->freq = vm_time_get_frequency();
   speed_sec = (Ipp64f)m->diff/(Ipp64f)m->freq;

   return speed_sec;

} /* Ipp64f vm_time_stop(vm_time_handle handle, vm_time *m) */

vm_status vm_time_gettimeofday( struct vm_timeval *TP, struct vm_timezone *TZP ) {
  return (gettimeofday(TP, TZP) == 0) ? 0 : VM_NOT_INITIALIZED;

}
#endif /* LINUX32 */
