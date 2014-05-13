/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#if !defined(_WIN32) && !defined(_WIN64)

#include <new> // std::bad_alloc
#include <stdio.h> // setrlimit

#include "vm/thread_defs.h"

MSDKMutex::MSDKMutex(void)
{
    int res = pthread_mutex_init(&m_mutex, NULL);
    if (res) throw std::bad_alloc();
}

MSDKMutex::~MSDKMutex(void)
{
    pthread_mutex_destroy(&m_mutex);
}

mfxStatus MSDKMutex::Lock(void)
{
    return (pthread_mutex_lock(&m_mutex))? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MSDKMutex::Unlock(void)
{
    return (pthread_mutex_unlock(&m_mutex))? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

int MSDKMutex::Try(void)
{
    return (pthread_mutex_trylock(&m_mutex))? 0: 1;
}

/* ****************************************************************************** */

MSDKSemaphore::MSDKSemaphore(mfxStatus &sts, mfxU32 count):
    msdkSemaphoreHandle(count)
{
    sts = MFX_ERR_NONE;
    int res = pthread_cond_init(&m_semaphore, NULL);
    if (!res) {
        res = pthread_mutex_init(&m_mutex, NULL);
        if (res) {
            pthread_cond_destroy(&m_semaphore);
        }
    }
    if (res) throw std::bad_alloc();
}

MSDKSemaphore::~MSDKSemaphore(void)
{
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_semaphore);
}

mfxStatus MSDKSemaphore::Post(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res) {
        if (0 == m_count++) res = pthread_cond_signal(&m_semaphore);
    }
    int sts = pthread_mutex_unlock(&m_mutex);
    if (!res) res = sts;
    return (res)? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MSDKSemaphore::Wait(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res) {
        if (!m_count) {
            res = pthread_cond_wait(&m_semaphore, &m_mutex);
        }
        if (!res) --m_count;
        int sts = pthread_mutex_unlock(&m_mutex);
        if (!res) res = sts;
    }
    return (res)? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

/* ****************************************************************************** */

MSDKEvent::MSDKEvent(mfxStatus &sts, bool manual, bool state):
    msdkEventHandle(manual, state)
{
    sts = MFX_ERR_NONE;

    int res = pthread_cond_init(&m_event, NULL);
    if (!res) {
        res = pthread_mutex_init(&m_mutex, NULL);
        if (res) {
            pthread_cond_destroy(&m_event);
        }
    }
    if (res) throw std::bad_alloc();
}

MSDKEvent::~MSDKEvent(void)
{
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_event);
}

mfxStatus MSDKEvent::Signal(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res) {
        if (!m_state) {
            m_state = true;
            if (m_manual) res = pthread_cond_broadcast(&m_event);
            else res = pthread_cond_signal(&m_event);
        }
        int sts = pthread_mutex_unlock(&m_mutex);
        if (!res) res = sts;
    }
    return (res)? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MSDKEvent::Reset(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res)
    {
        if (m_state) m_state = false;
        res = pthread_mutex_unlock(&m_mutex);
    }
    return (res)? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MSDKEvent::Wait(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res)
    {
        if (!m_state) res = pthread_cond_wait(&m_event, &m_mutex);
        if (!m_manual) m_state = false;
        int sts = pthread_mutex_unlock(&m_mutex);
        if (!res) res = sts;
    }
    return (res)? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MSDKEvent::TimedWait(mfxU32 msec)
{
    if (MFX_INFINITE == msec) return MFX_ERR_UNSUPPORTED;
    mfxStatus mfx_res = MFX_ERR_NOT_INITIALIZED;

    int res = pthread_mutex_lock(&m_mutex);
    if (!res)
    {
        if (!m_state)
        {
            struct timeval tval;
            struct timespec tspec;
            mfxI32 res;

            gettimeofday(&tval, NULL);
            msec = 1000 * msec + tval.tv_usec;
            tspec.tv_sec = tval.tv_sec + msec / 1000000;
            tspec.tv_nsec = (msec % 1000000) * 1000;
            res = pthread_cond_timedwait(&m_event,
                                         &m_mutex,
                                         &tspec);
            if (!res) mfx_res = MFX_ERR_NONE;
            else if (ETIMEDOUT == res) mfx_res = MFX_TASK_WORKING;
            else mfx_res = MFX_ERR_UNKNOWN;
        }
        else mfx_res = MFX_ERR_NONE;
        if (!m_manual)
            m_state = false;

        res = pthread_mutex_unlock(&m_mutex);
        if (res) mfx_res = MFX_ERR_UNKNOWN;
    }
    else mfx_res = MFX_ERR_UNKNOWN;

    return mfx_res;
}

/* ****************************************************************************** */

void* msdk_thread_start(void* arg)
{
    if (arg) {
        MSDKThread* thread = (MSDKThread*)arg;

        if (thread->m_func) thread->m_func(thread->m_arg);
        thread->m_event->Signal();
    }
    return NULL;
}

/* ****************************************************************************** */

MSDKThread::MSDKThread(mfxStatus &sts, msdk_thread_callback func, void* arg):
    msdkThreadHandle(func, arg)
{
    m_event = new MSDKEvent(sts, false, false);
    if (pthread_create(&(m_thread), NULL, msdk_thread_start, this)) {
        delete(m_event);
        throw std::bad_alloc();
    }
}

MSDKThread::~MSDKThread(void)
{
    delete m_event;
}

mfxStatus MSDKThread::Wait(void)
{
    int res = pthread_join(m_thread, NULL);
    return (res)? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MSDKThread::TimedWait(mfxU32 msec)
{
    if (MFX_INFINITE == msec) return MFX_ERR_UNSUPPORTED;

    mfxStatus mfx_res = m_event->TimedWait(msec);

    if (MFX_ERR_NONE == mfx_res) {
        return (pthread_join(m_thread, NULL))? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
    }
    return mfx_res;
}

mfxStatus MSDKThread::GetExitCode()
{
    if (!m_event) return MFX_ERR_NOT_INITIALIZED;

    /** @todo: Need to add implementation. */
    return MFX_ERR_NONE;
}

/* ****************************************************************************** */

mfxStatus msdk_setrlimit_vmem(mfxU64 size)
{
    struct rlimit limit;

    limit.rlim_cur = size;
    limit.rlim_max = size;
    if (setrlimit(RLIMIT_AS, &limit)) return MFX_ERR_UNKNOWN;
    return MFX_ERR_NONE;
}

#endif // #if !defined(_WIN32) && !defined(_WIN64)
