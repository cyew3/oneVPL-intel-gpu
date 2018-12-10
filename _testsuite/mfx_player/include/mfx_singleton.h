/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2018 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "umc_mutex.h"
#include <memory>

//NOTE: not thread safety single tone due to local static objects are not thread safety implemented
//it shouldn't be used in multithreaded code
template<class T>
class MFXNTSSingleton
{
public:
    static T& Instance(void)
    {
        static T m_pInstance;
        return m_pInstance;
    }
};

//this class couldn't be used to initialise global static variables, since it uses global static mutex that 
template<class T>
class MFXSingleton
{
public:
    static T& Instance(void);

private:
    static UMC::Mutex m_createMutex;
    static std::auto_ptr<T> m_pInstance;
};

template<class T>
UMC::Mutex MFXSingleton<T>::m_createMutex;

template<class T>
std::auto_ptr<T> MFXSingleton<T>::m_pInstance;

template<class T>
T& MFXSingleton<T>::Instance(void)
{
    if (!m_pInstance.get())
    {
        UMC::AutomaticUMCMutex lock(m_createMutex);
        if (!m_pInstance.get())
        {
            m_pInstance.reset(new T);
        }
    }
    return *m_pInstance.get();
}
