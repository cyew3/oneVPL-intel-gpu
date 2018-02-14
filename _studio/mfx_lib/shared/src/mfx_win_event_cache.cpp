//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_win_event_cache.h"
#include "vm_mutex.h"

#if defined (MFX_VA_WIN)

/*
allocates nPreallocEvents
MFX_ERR_NONE if successes
MFX_ERR_MEMORY_ALLOC if failed to allocate event
MFX_ERR_NOT_INITIALIZED if already initialized
*/

//vm_mutex gLock = { 0 };

mfxStatus EventCache::Init(size_t nPreallocEvents)
{
    for (size_t i = 0; i < nPreallocEvents; i++)
    {
        EVENT_TYPE ev= CreateEvent(NULL, FALSE, FALSE, NULL);
        m_Free.push_back(ev);
    }
    m_nInitNumberOfEvents = (mfxU16)nPreallocEvents;

    return MFX_ERR_NONE;
}

/*
free all allocated Event
MFX_ERR_NONE if successes
MFX_WRN_IN_EXECUTION if some event are not marked as unused
MFX_ERR_NOT_INITIALIZED if not initialized
*/
mfxStatus EventCache::Close()
{
    if (m_Free.size() != m_nInitNumberOfEvents)
        return MFX_ERR_NOT_FOUND;

    while (m_Free.empty() == false)
    {
        CloseHandle(m_Free.back());
        m_Free.pop_back();
    }
    return MFX_ERR_NONE;
}

/*
return free Event and mark it as used
MFX_ERR_NONE if successes
*/
mfxStatus  EventCache::GetEvent(EVENT_TYPE& event)
{
    event = m_Free.back();
    m_Free.pop_back();
    return MFX_ERR_NONE;
}

/*
return Event to cache and mark it as free
MFX_ERR_NONE if successes
*/
mfxStatus  EventCache::ReturnEvent(EVENT_TYPE& event)
{
    m_Free.push_back(event);
    return MFX_ERR_NONE;
}

#endif // #if defined (MFX_VA_WIN)
/* EOF */
