// Copyright (c) 2009-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
    event = m_Free.front();
    m_Free.pop_front();
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
