/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2013 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __UMC_MUTEX_H__
#define __UMC_MUTEX_H__

#include "mfx_vm++_pthread.h"
#include "vm_debug.h"
#include "vm_mutex.h"
#include "umc_structures.h"

namespace UMC
{

class Mutex : public MfxMutex
{
public:
    Mutex(void): MfxMutex() {}
    // Try to get ownership of mutex
    inline Status TryLock(void) { return (MfxMutex::TryLock())? UMC_OK: UMC_ERR_TIMEOUT; }

private: // functions
    Mutex(const Mutex&);
    Mutex& operator=(const Mutex&);
};

class AutomaticUMCMutex : public MfxAutoMutex
{
public:
    // Constructor
    AutomaticUMCMutex(UMC::Mutex &mutex)
        : MfxAutoMutex(mutex)
    {
    }

private: // functions
    AutomaticUMCMutex(const AutomaticUMCMutex&);
    AutomaticUMCMutex& operator=(const AutomaticUMCMutex&);
};

} // namespace UMC

#endif // __UMC_MUTEX_H__
