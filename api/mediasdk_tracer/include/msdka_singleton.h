
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2012 Intel Corporation. All Rights Reserved.

File Name: msdka_singletone.h

\* ****************************************************************************** */

#pragma once

#include <windows.h>
#include <memory>

class Mutex
{
public:
    CRITICAL_SECTION m_Sec;
public:
    Mutex()
    {
        InitializeCriticalSection(&m_Sec);
    }
    ~Mutex()
    {
        DeleteCriticalSection(&m_Sec);
    }
    void Lock()
    {
        EnterCriticalSection(&m_Sec);
    }
    void Unlock()
    {
        LeaveCriticalSection(&m_Sec);
    }
};

class AutoMutex
{
    Mutex &m_ref;
public:
    AutoMutex(Mutex & mut) 
        : m_ref(mut)
    {
        m_ref.Lock();
    }
    ~AutoMutex() 
    {
        m_ref.Unlock();
    }

private:
    AutoMutex & operator = (AutoMutex &  /*other*/){return  *this; }
};

//this class couldn't be used in initialization of global static variables, since it uses global static mutex 
//that might be uninitialized 
template<class T>
class Singleton
{
public:
    static T& Instance(void);

private:
    static Mutex m_createMutex;
    static std::auto_ptr<T> m_pInstance;
};

template<class T>
Mutex Singleton<T>::m_createMutex;

template<class T>
std::auto_ptr<T> Singleton<T>::m_pInstance(0);

template<class T>
T& Singleton<T>::Instance(void)
{
    if (!m_pInstance.get())
    {
        AutoMutex lock(m_createMutex);
        if (!m_pInstance.get())
        {
            m_pInstance.reset(new T);
        }
    }
    return *m_pInstance.get();
}
