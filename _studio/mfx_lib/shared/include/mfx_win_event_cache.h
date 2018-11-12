// Copyright (c) 2008-2018 Intel Corporation
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

#ifndef __MFX_WIN_EVENT_CACHE_H__
#define __MFX_WIN_EVENT_CACHE_H__

#include "mfx_common.h"
#include <deque>
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
    std::deque<EVENT_TYPE> m_Free;
    mfxU16 m_nInitNumberOfEvents;
};
#endif /* #if defined(MFX_VA_WIN) */
#endif /* #ifndef __MFX_WIN_EVENT_CACHE_H__ */



