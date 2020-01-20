// Copyright (c) 2009-2019 Intel Corporation
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
#if defined (MFX_VA_WIN)

/*
allocates nPreallocEvents
MFX_ERR_NONE if successes
MFX_ERR_MEMORY_ALLOC if failed to allocate event
MFX_ERR_NOT_INITIALIZED if already initialized
*/

mfxStatus EventCache::Init(size_t nPreallocEvents)
{
    for (size_t i = 0; i < nPreallocEvents; i++)
    {
        EVENT_TYPE ev= CreateEvent(NULL, FALSE, FALSE, NULL);
        // WA for avoid cases whan global HW (BB completion) event is equal created event
        // should be removed after enablin event based synchronization for all components
        if (m_pGlobalHwEvent != nullptr && ev == *m_pGlobalHwEvent)
        {
            EVENT_TYPE ev1 = CreateEvent(NULL, FALSE, FALSE, NULL);

            CloseHandle(ev);
            ev = ev1;
        }
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
    m_pGlobalHwEvent = nullptr;

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
Get free Event from pool, reset it, mark it as used, then return it via event argument
MFX_ERR_NONE if successes
*/
mfxStatus  EventCache::GetEvent(EVENT_TYPE& event)
{
    std::lock_guard<std::mutex> lock(m_guard);
    if (m_Free.empty())
    {
        event = INVALID_HANDLE_VALUE;
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    event = m_Free.front();
    ResetEvent(event);
    m_Free.pop_front();
    return MFX_ERR_NONE;
}

/*
return Event to cache and mark it as free
MFX_ERR_NONE if successes
*/
mfxStatus  EventCache::ReturnEvent(EVENT_TYPE& event)
{
    std::lock_guard<std::mutex> lock(m_guard);
    m_Free.push_back(event);
    return MFX_ERR_NONE;
}

#endif // #if defined (MFX_VA_WIN)
/* EOF */
