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

#ifndef __UMC_WIN_EVENT_CACHE_H__
#define __UMC_WIN_EVENT_CACHE_H__

#include "umc_va_base.h"
#include "vm_mutex.h"
#include <vector>
#include <map>
#include <array>

#if defined(UMC_VA_DXVA)

namespace UMC
{
    class EventCache
    {
    public:
        static const size_t MaxEventsPerIndex = 2; // aka fields count
        typedef HANDLE EVENT_TYPE;
        typedef int32_t MapKey; // index
        typedef std::array<EVENT_TYPE, MaxEventsPerIndex> MapValue; // index
        /*
        UMC_OK if successes
        */
        Status Init();

        /*
        free all allocated Event
        UMC_OK if successes
        */
        Status Close();

        /*
        return free Event and mark it as used
        associate index and fieldId with the Event
        returns INVALID_HANDLE_VALUE if such pair already exist (only in DEBUG build)
        */
        EVENT_TYPE GetFreeEventAndMap(int32_t index, uint32_t fieldId);

        /*
        Get events by index from map
        One of events may be INVALID_HANDLE_VALUE 
        if only single field added by GetFreeEventAndMap
        both events is INVALID_HANDLE_VALUE
        if no associated events found
        */
        EventCache::MapValue EventCache::GetEvents(int32_t index);
        
        /*
        return Events to cache and mark it as free
        MFX_ERR_NONE if successes
        */
        Status FreeEvents(int32_t index);

        virtual ~EventCache()
        {
            (void)Close();
        }
    private:
        std::vector<EVENT_TYPE> m_Free;
        std::map<MapKey, MapValue> eventCache;
        vm_mutex m_eventCacheGuard;

    };

}
#endif /* #if defined(UMC_VA_DXVA) */
#endif /* #ifndef __UMC_WIN_EVENT_CACHE_H__ */



