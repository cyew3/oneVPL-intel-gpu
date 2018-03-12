//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2018 Intel Corporation. All Rights Reserved.
//

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
        typedef Ipp32s MapKey; // index
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
        EVENT_TYPE GetFreeEventAndMap(Ipp32s index, Ipp32u fieldId);

        /*
        Get events by index from map
        One of events may be INVALID_HANDLE_VALUE 
        if only single field added by GetFreeEventAndMap
        both events is INVALID_HANDLE_VALUE
        if no associated events found
        */
        EventCache::MapValue EventCache::GetEvents(Ipp32s index);
        
        /*
        return Events to cache and mark it as free
        MFX_ERR_NONE if successes
        */
        Status FreeEvents(Ipp32s index);

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



