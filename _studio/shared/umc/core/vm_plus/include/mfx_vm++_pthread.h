//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2015 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_VMPLUSPLUS_PTHREAD_H__
#define __MFX_VMPLUSPLUS_PTHREAD_H__

#include "mfxdefs.h"

#include <new>

/* Getting platform-specific definitions. */
#include "sys/mfx_vm++_pthread_windows.h"
#include "sys/mfx_vm++_pthread_unix.h"

/*------------------------------------------------------------------------------*/

class MfxMutex
{
public:
    MfxMutex(void);
    virtual ~MfxMutex(void);

    mfxStatus Lock(void);
    mfxStatus Unlock(void);
    bool TryLock(void);
    
private: // variables
    MfxMutexHandle m_handle;

private: // functions
    MfxMutex(const MfxMutex&);
    MfxMutex& operator=(const MfxMutex&);
};

/*------------------------------------------------------------------------------*/

class MfxAutoMutex
{
public:
    MfxAutoMutex(MfxMutex& mutex);
    virtual ~MfxAutoMutex(void);

    mfxStatus Lock(void);
    mfxStatus Unlock(void);

private: // variables
    MfxMutex& m_rMutex;
    bool m_bLocked;

private: // functions
    MfxAutoMutex(const MfxAutoMutex&);
    MfxAutoMutex& operator=(const MfxAutoMutex&);
    // The following overloading 'new' operators prohibit
    // creating the object on the heap which is inefficient from
    // performance/memory point of view
    void* operator new(size_t);
    void* operator new(size_t, void*);
    void* operator new[](size_t);
    void* operator new[](size_t, void*);
};

/*------------------------------------------------------------------------------*/

#endif //__MFX_VMPLUSPLUS_PTHREAD_H__
