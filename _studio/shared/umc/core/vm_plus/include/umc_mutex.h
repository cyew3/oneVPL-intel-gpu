/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2010 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __UMC_MUTEX_H__
#define __UMC_MUTEX_H__

#include "vm_debug.h"
#include "vm_mutex.h"
#include "umc_structures.h"

namespace UMC
{

class Mutex
{
public:
    // Default constructor
    Mutex(void);
    // Destructor
    virtual ~Mutex(void);

    void Lock(void);
    // Release ownership of mutex
    void Unlock(void);
    // Try to get ownership of mutex
    Status TryLock(void);
    // Get ownership of initialized mutex
    void LockIfInitialized(void);
    // Release ownership of initialized mutex
    void UnlockIfInitialized(void);
    // Is mutex inited
    bool IsInited(void);

    // Extract mutex handle for using in AutomaticMutex class
    vm_mutex &ExtractHandle(void);

protected:
    vm_mutex m_handle;                                          // (vm_mutex) handle to system mutex
};

inline
void Mutex::Lock(void)
{
    Status umcRes = UMC_OK;

    if (!vm_mutex_is_valid(&m_handle))
        umcRes = vm_mutex_init(&m_handle);

    if ((UMC_OK == umcRes) &&
        (VM_OK != vm_mutex_lock(&m_handle)))
        VM_ASSERT(false);

} // void Mutex::Lock(void)

inline
void Mutex::Unlock(void)
{
    if (!vm_mutex_is_valid(&m_handle))
        vm_mutex_init(&m_handle);
    else if (VM_OK != vm_mutex_unlock(&m_handle))
        VM_ASSERT(false);

} // void Mutex::Unlock(void)

inline
Status Mutex::TryLock(void)
{
    Status umcRes = UMC_OK;

    if (!vm_mutex_is_valid(&m_handle))
        umcRes = vm_mutex_init(&m_handle);

    if (UMC_OK == umcRes)
        umcRes = vm_mutex_try_lock(&m_handle);

    return umcRes;

} // Status Mutex::TryLock(void)

inline
void Mutex::LockIfInitialized(void)
{
    if (vm_mutex_is_valid(&m_handle) &&
        (VM_OK != vm_mutex_lock(&m_handle)))
        VM_ASSERT(false);

} // void Mutex::LockIfInitialized(void)

inline
void Mutex::UnlockIfInitialized(void)
{
    if (vm_mutex_is_valid(&m_handle) &&
        (VM_OK != vm_mutex_unlock(&m_handle)))
        VM_ASSERT(false);

} // void Mutex::UnlockIfInitialized(void)

inline
bool Mutex::IsInited(void)
{
    return (0 != vm_mutex_is_valid(&m_handle));

} // bool Mutex::IsInited(void)

inline
vm_mutex &Mutex::ExtractHandle(void)
{
    return m_handle;

} // vm_mutex &Mutex::ExtractHandle(void)


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  AutomaticUMCMutex class implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AutomaticUMCMutex
{
public:
    // Constructor
    AutomaticUMCMutex(UMC::Mutex &mutex)
        : m_Mutex(mutex)
        , m_lockedCounter(0)
    {
        Lock();
    }

    // Destructor
    ~AutomaticUMCMutex(void)
    {
        Unlock();
    }

    void Lock()
    {
        m_Mutex.Lock();
        m_lockedCounter++;
    }

    void Unlock()
    {
        if (m_lockedCounter)
        {
            m_lockedCounter--;
            m_Mutex.Unlock();
        }
    }

protected:
    UMC::Mutex &m_Mutex;
    Ipp32s      m_lockedCounter;

private:
    AutomaticUMCMutex & operator = (AutomaticUMCMutex &)
    {
        return *this;
    }
};

} // namespace UMC

#endif // __UMC_MUTEX_H__
