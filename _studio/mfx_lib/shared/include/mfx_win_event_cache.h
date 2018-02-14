//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_WIN_EVENT_CACHE_H__
#define __MFX_WIN_EVENT_CACHE_H__

#include "mfx_common.h"

#if defined(MFX_VA_WIN)

class EventCache
{
public:
    typedef HANDLE EVENT_TYPE;
    /*
    allocates nPreallocEvents
    MFX_ERR_NONE if successes
    MFX_ERR_MEMORY_ALLOC if failed to allocate event
    MFX_ERR_NOT_INITIALIZED if already initialized
    */
    mfxStatus Init(size_t nPreallocEvents);

    /*
    free all allocated Event
    MFX_ERR_NONE if successes
    MFX_WRN_IN_EXECUTION if some event are not marked as unused
    MFX_ERR_NOT_INITIALIZED if not initialized
    */
    mfxStatus Close();

    /*
    return free Event and mark it as used
    MFX_ERR_NONE if successes
    */
    mfxStatus  GetEvent(EVENT_TYPE& event);

    /*
    return Event to cache and mark it as free
    MFX_ERR_NONE if successes
    */
    mfxStatus  ReturnEvent(EVENT_TYPE& event);

    virtual ~EventCache()
    {
        (void)Close();
    }
private:
    std::vector<EVENT_TYPE> m_Free;
    mfxU16 m_nInitNumberOfEvents;
};

extern vm_mutex gLock;

#endif /* #if defined(MFX_VA_WIN) */
#endif /* #ifndef __MFX_WIN_EVENT_CACHE_H__ */



