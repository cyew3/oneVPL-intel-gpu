/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2010 Intel Corporation. All Rights Reserved.
//
*/

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

#ifndef _WIN32_WCE
#include <process.h>
#endif /* _WIN32_WCE */

#include <windows.h>
#include <stdlib.h>
#include "vm_thread.h"
#include "vm_mutex.h"
#include "vm_debug.h"

#ifdef VM_THREAD_CATCHCRASH
static Ipp32u __stdcall vm_thread_exception_reaction(void* p)
{
    p;
    vm_debug_trace(VM_DEBUG_ALL, VM_STRING("exception\r\n"));
    return 0;
}
#endif

/* set the thread handler an invalid value */
void vm_thread_set_invalid(vm_thread *thread)
{
    /* check error(s) */
    if (NULL == thread)
        return;

    thread->handle = NULL;
    thread->i_wait_count = 0;
    vm_mutex_set_invalid(&(thread->access_mut));

#ifdef VM_THREAD_CATCHCRASH
    thread->protected_exception_reaction = vm_thread_exception_reaction;
#endif
} /* void vm_thread_set_invalid(vm_thread *thread)*/

/* verify if the thread handler is valid */
Ipp32s vm_thread_is_valid(vm_thread *thread)
{
    Ipp32s iRes = 1;

    /* check error(s) */
    if (NULL == thread)
        return 0;

    vm_mutex_lock(&thread->access_mut);
    if (NULL != thread->handle)
    {
        if (WAIT_OBJECT_0 == WaitForSingleObject(thread->handle, 0))
        {
            iRes = 0;
            if (0 == thread->i_wait_count)
            {
                CloseHandle(thread->handle);
                thread->handle = NULL;
            }
        }
    }

    if (NULL == thread->handle)
        iRes = 0;

    vm_mutex_unlock(&(thread->access_mut));
    return iRes;

} /* Ipp32s vm_thread_is_valid(vm_thread *thread) */

#ifdef VM_THREAD_CATCHCRASH
static Ipp32u __stdcall vm_thread_protectedfunc(void* params)
{
    vm_thread* thread = (vm_thread*)params;
    if (NULL == thread)
        return 0;

    __try
    {
        thread->protected_func(thread->protected_arg);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        thread->protected_exception_reaction(thread->protected_arg);
    };
    return 0;
}
#endif

/* create a thread.
 * return NULL if failed to create the thread */
Ipp32s vm_thread_create(vm_thread *thread, vm_thread_callback func, void *arg)
{
    Ipp32s i_res = 1;

    /* check error(s) */
    if ((NULL == thread) ||
        (NULL == func))
        return 0;

#ifdef VM_THREAD_CATCHCRASH
    thread->protected_func = func;
    thread->protected_arg  = arg;
#endif

    if ((!vm_mutex_is_valid(&thread->access_mut)) &&
        (VM_OK != vm_mutex_init(&thread->access_mut)))
        i_res = 0;

    if (i_res)
    {
        if (NULL != thread->handle)
            vm_thread_wait(thread);

        vm_mutex_lock(&thread->access_mut);

        thread->handle = (HANDLE)
#if defined(_WIN32_WCE) /* _WIN32_WCE */
                                 CreateThread(0,
                                              0,
                                              (LPTHREAD_START_ROUTINE) func,
                                              arg,
                                              0,
                                              0);
#elif defined(VM_THREAD_CATCHCRASH)
                                 _beginthreadex(0,
                                                0,
                                                &vm_thread_protectedfunc,
                                                thread,
                                                0,
                                                0);
#else
                                 _beginthreadex(0,
                                                0,
                                                (Ipp32u (__stdcall *)(void*))func,
                                                arg,
                                                0,
                                                0);

#endif /* _WIN32_WCE */

        i_res = (Ipp32s) ((thread->handle) ? (1) : (0));
        vm_mutex_unlock(&thread->access_mut);
    }
    return i_res;

} /* Ipp32s vm_thread_create(vm_thread *thread, vm_thread_callback func, void *arg) */

/* attach to current thread. return 1 if successful  */
Ipp32s vm_thread_attach(vm_thread *thread, vm_thread_callback func, void *arg)
{
    Ipp32s i_res = 1;

    func;
    arg;

    /* check error(s) */
    if ((NULL == thread) ||
        (NULL == func))
        return 0;

#ifdef VM_THREAD_CATCHCRASH
    thread->protected_func = func;
    thread->protected_arg = arg;
#endif

    if ((!vm_mutex_is_valid(&thread->access_mut)) &&
        (VM_OK != vm_mutex_init(&thread->access_mut)))
        i_res = 0;

    if (i_res)
    {
        if (NULL != thread->handle)
            vm_thread_wait(thread);

        vm_mutex_lock(&thread->access_mut);
        i_res = (Ipp32s)((thread->handle) ? (1) : (0));
        thread->i_wait_count = 1; // prevent vm_thread_wait from closing this thread handle
        vm_mutex_unlock(&thread->access_mut);
    }
    return i_res;


} /* Ipp32s vm_thread_attach(vm_thread *thread, vm_thread_callback func, void *arg) */

Ipp32s vm_thread_set_scheduling(vm_thread* thread, void* params)
{
    thread; params;
    return 0; // not supported
}

/* set thread priority. return 1 if success */
Ipp32s vm_thread_set_priority(vm_thread *thread, vm_thread_priority priority)
{
    Ipp32s i_res = 0;

    /* check error(s) */
    if (NULL == thread)
        return 0;

    if (NULL != thread->handle)
    {
        vm_mutex_lock(&thread->access_mut);

        switch (priority) {
        case VM_THREAD_PRIORITY_HIGHEST:
            i_res = SetThreadPriority(thread->handle,
                                      THREAD_PRIORITY_HIGHEST);
            break;

        case VM_THREAD_PRIORITY_HIGH:
            i_res = SetThreadPriority(thread->handle,
                                      THREAD_PRIORITY_ABOVE_NORMAL);
            break;

        case VM_THREAD_PRIORITY_NORMAL:
            i_res = SetThreadPriority(thread->handle,
                                      THREAD_PRIORITY_NORMAL);
            break;

        case VM_THREAD_PRIORITY_LOW:
            i_res = SetThreadPriority(thread->handle,
                                      THREAD_PRIORITY_BELOW_NORMAL);
            break;

        case VM_THREAD_PRIORITY_LOWEST:
            i_res = SetThreadPriority(thread->handle,
                                      THREAD_PRIORITY_LOWEST);
            break;

        default:
            i_res = 0;
            break;
        }
        vm_mutex_unlock(&thread->access_mut);
    }
    return i_res;

} /* Ipp32s vm_thread_set_priority(vm_thread *thread, vm_thread_priority priority) */

/* wait until a thread exists */
void vm_thread_wait(vm_thread *thread)
{
    /* check error(s) */
    if (NULL == thread)
        return;

    vm_mutex_lock(&thread->access_mut);
    if (thread->handle)
    {
        thread->i_wait_count++;
        vm_mutex_unlock(&thread->access_mut);

        WaitForSingleObject(thread->handle, INFINITE);

        vm_mutex_lock(&thread->access_mut);
        thread->i_wait_count--;
        if (0 == thread->i_wait_count)
        {
            CloseHandle(thread->handle);
            thread->handle = NULL;
        }
    }
    vm_mutex_unlock(&thread->access_mut);

} /* void vm_thread_wait(vm_thread *thread) */

/* close thread after all */
void vm_thread_close(vm_thread *thread)
{
    /* check error(s) */
    if (NULL == thread)
        return;

    vm_thread_wait(thread);
    vm_mutex_destroy(&thread->access_mut);

} /* void vm_thread_close(vm_thread *thread) */

vm_thread_priority vm_get_current_thread_priority()
{
    return (vm_thread_priority)GetThreadPriority(GetCurrentThread());
}

void vm_set_current_thread_priority(vm_thread_priority priority)
{
    SetThreadPriority(GetCurrentThread(), (int)priority);
}

void vm_set_thread_affinity_mask(vm_thread *thread, Ipp64u mask)
{
#if !defined(WIN_TRESHOLD_MOBILE)
    // 32 is maximum number of processors supported for WIN32 - ignore high 32 bits of mask
    SetThreadAffinityMask(thread->handle, (DWORD_PTR)mask);
#else
    mask = 0;
    thread = NULL;
#endif
}

#endif /* defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE) */
