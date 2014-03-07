/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if !defined(_WIN32) && !defined(_WIN64)

#include <stdint.h> /* for uintptr_t */
#include <stdio.h>

#include "vm/thread_defs.h"

MSDKMutex::MSDKMutex(void)
{
    m_bInitialized = true;
    if (pthread_mutex_init(&m_mutex, NULL))
    {
        m_bInitialized = false;
    }
}

MSDKMutex::~MSDKMutex(void)
{
    if (m_bInitialized)
    {
        pthread_mutex_destroy(&m_mutex);
    }
}

mfxStatus MSDKMutex::Lock(void)
{
    if (!m_bInitialized) return MFX_ERR_NOT_INITIALIZED;
    return (pthread_mutex_lock(&m_mutex))? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

mfxStatus MSDKMutex::Unlock(void)
{
    if (!m_bInitialized) return MFX_ERR_NOT_INITIALIZED;
    return (pthread_mutex_unlock(&m_mutex))? MFX_ERR_UNKNOWN: MFX_ERR_NONE;
}

int MSDKMutex::Try(void)
{
    if (!m_bInitialized) return 0;
    return (pthread_mutex_trylock(&m_mutex))? 0: 1;
}

/* ****************************************************************************** */

AutomaticMutex::AutomaticMutex(MSDKMutex& mutex)
{
    m_pMutex = &mutex;
    m_bLocked = false;
    Lock();
};
AutomaticMutex::~AutomaticMutex(void)
{
    Unlock();
}

void AutomaticMutex::Lock(void)
{
    if (!m_bLocked)
    {
        if (!m_pMutex->Try())
        {
            m_pMutex->Lock();
        }
        m_bLocked = true;
    }
}

void AutomaticMutex::Unlock(void)
{
    if (m_bLocked)
    {
        m_pMutex->Unlock();
        m_bLocked = false;
    }
}

/* ****************************************************************************** */

MSDKSemaphore::MSDKSemaphore(mfxStatus &sts, mfxU32 count)
{
    sts = MFX_ERR_NONE;
    m_count = count;
    pthread_cond_init(&m_semaphore, NULL);
    pthread_mutex_init(&m_mutex, NULL);
}

MSDKSemaphore::~MSDKSemaphore(void)
{
    pthread_cond_destroy(&m_semaphore);
    pthread_mutex_destroy(&m_mutex);
}

void MSDKSemaphore::Post(void)
{
    if (0 <= m_count)
    {
        pthread_mutex_lock(&m_mutex);
        if (0 == m_count++) pthread_cond_signal(&m_semaphore);
        pthread_mutex_unlock(&m_mutex);
    }
}

void MSDKSemaphore::Wait(void)
{
    if (0 <= m_count)
    {
        bool bError = false;

        pthread_mutex_lock(&m_mutex);

        if ((0 == m_count) && (0 != pthread_cond_wait(&m_semaphore, &m_mutex)))
            bError = true;
        if (!bError) --m_count;

        pthread_mutex_unlock(&m_mutex);
    }
}

/* ****************************************************************************** */

MSDKEvent::MSDKEvent(mfxStatus &sts, bool manual, bool state)
{
    sts = MFX_ERR_NONE;

    m_manual = manual;
    m_state = state;
    pthread_cond_init(&m_event, NULL);
    pthread_mutex_init(&m_mutex, NULL);
}

MSDKEvent::~MSDKEvent(void)
{
    pthread_cond_destroy(&m_event);
    pthread_mutex_destroy(&m_mutex);
}

void MSDKEvent::Signal(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res)
    {
        if (!m_state)
        {
            m_state = true;
            if (m_manual) pthread_cond_broadcast(&m_event);
            else pthread_cond_signal(&m_event);
        }
        res = pthread_mutex_unlock(&m_mutex);
    }
}

void MSDKEvent::Reset(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res)
    {
        if (m_state) m_state = false;
        res = pthread_mutex_unlock(&m_mutex);
    }
}

void MSDKEvent::Wait(void)
{
    int res = pthread_mutex_lock(&m_mutex);
    if (!res)
    {
        if (!m_state) pthread_cond_wait(&m_event, &m_mutex);
        if (!m_manual) m_state = false;
        res = pthread_mutex_unlock(&m_mutex);
    }
}

mfxStatus MSDKEvent::TimedWait(mfxU32 msec)
{
    if(MFX_INFINITE == msec) return MFX_ERR_UNSUPPORTED;
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
            if (0 == res) mfx_res = MFX_ERR_NONE;
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
    uintptr_t res = 0;

    if (arg)
    {
        MSDKThread* thread = (MSDKThread*)arg;

        if (thread->m_func) res = thread->m_func(thread->m_arg);
        thread->m_event->Signal();
    }
    return (void*)res;
}

/* ****************************************************************************** */

MSDKThread::MSDKThread(mfxStatus &sts, msdk_thread_callback func, void* arg)
{
    m_event = new MSDKEvent(sts, false, false);
    if (MFX_ERR_NONE == sts)
    {
        m_func = func;
        m_arg = arg;
        if (!pthread_create(&(m_thread), NULL, msdk_thread_start, this)) sts = MFX_ERR_NONE;
    }
}

MSDKThread::~MSDKThread(void)
{
    if (m_event)
    {
        delete m_event;
        m_event = NULL;
    }
}

void MSDKThread::Wait(void)
{
    pthread_join(m_thread, NULL);
}

mfxStatus MSDKThread::TimedWait(mfxU32 msec)
{
    if (MFX_INFINITE == msec) return MFX_ERR_UNSUPPORTED;
    if (!m_event) return MFX_ERR_NOT_INITIALIZED;

    mfxStatus mfx_res = m_event->TimedWait(msec);

    if (MFX_ERR_NONE == mfx_res)
    {
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
