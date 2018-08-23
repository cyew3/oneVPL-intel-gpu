//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_win_event_cache.h"
#include "umc_automatic_mutex.h"

#if defined (UMC_VA_DXVA)

namespace UMC
{

    Status EventCache::Init()
    {
        vm_mutex_set_invalid(&m_eventCacheGuard);
        vm_mutex_init(&m_eventCacheGuard);
        return UMC_OK;
    }


    Status EventCache::Close()
    {
        if ( vm_mutex_is_valid(&m_eventCacheGuard))
        {
            {
                UMC::AutomaticMutex guard(m_eventCacheGuard);

                while (m_Free.empty() == false)
                {
                    CloseHandle(m_Free.back());
                    m_Free.pop_back();
                }

#if defined(_DEBUG)
                // at this time eventCache must be empty.  i.e. all task synchronized
                MFX_TRACE_1("\n!!! EventCache::Close( ", "eventCache.size() = %d \n", (int)eventCache.size());
#endif

                for (auto ie = eventCache.begin(); ie != eventCache.end(); ++ie)
                {
                    if (ie->second[0] != INVALID_HANDLE_VALUE)
                        CloseHandle(ie->second[0]);
                    if (ie->second[1] != INVALID_HANDLE_VALUE)
                        CloseHandle(ie->second[1]);
                }
                eventCache.clear();
            } // unlock mutex before destroy
            vm_mutex_destroy(&m_eventCacheGuard);
        }

        return UMC_OK;
    }

    EventCache::EVENT_TYPE EventCache::GetFreeEventAndMap(int32_t index, uint32_t fieldId)
    {
        UMC::AutomaticMutex guard(m_eventCacheGuard);
        EVENT_TYPE event = INVALID_HANDLE_VALUE;

        if ( fieldId >= MaxEventsPerIndex)
            return  INVALID_HANDLE_VALUE;

        if (m_Free.size() > 0)
        {
            event = m_Free.back();
            m_Free.pop_back();
        }
        else
        {
            event = CreateEvent(NULL, FALSE, FALSE, NULL);
        }

        auto iFoundE = eventCache.find(index);

        if (iFoundE == eventCache.end()) //  new index
        {
            MapValue events = { {INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE } };
            events[fieldId] = event;
            eventCache.insert(std::pair<MapKey,MapValue>(index,events));
        }
        else // add field
        {
#ifdef _DEBUG
            if (iFoundE->second[fieldId] != INVALID_HANDLE_VALUE)
            {
                m_Free.push_back(event);
                return  INVALID_HANDLE_VALUE;
            }
#endif
            iFoundE->second[fieldId] = event;
        }


        return event;
    }


    EventCache::MapValue EventCache::GetEvents(int32_t index)
    {
        UMC::AutomaticMutex guard(m_eventCacheGuard);
        auto iFoundE = eventCache.find(index);

        if (iFoundE == eventCache.end()) //  new index
        {
            MapValue invalid = { { INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE } };
            return invalid;
        }

        return iFoundE->second;
    }

    /*
    return Event to cache and mark it as free
    MFX_ERR_NONE if successes
    */
    Status EventCache::FreeEvents(int32_t index)
    {
        UMC::AutomaticMutex guard(m_eventCacheGuard);
        auto iFoundE = eventCache.find(index);

        if (iFoundE == eventCache.end()) //  new index
        {
            return UMC::UMC_ERR_INVALID_PARAMS;
        }

        MapValue& events = iFoundE->second;

        //for (auto e = events.begin(); e != events.end(); ++e)
        //{
        //    if (*e != INVALID_HANDLE_VALUE)
        //        m_Free.push_back(*e);
        //}
        if (events[0] != INVALID_HANDLE_VALUE)
            m_Free.push_back(events[0]);
        if (events[1] != INVALID_HANDLE_VALUE)
            m_Free.push_back(events[1]);

        eventCache.erase(iFoundE);
        return UMC_OK;
    }
}
#endif // #if defined (MFX_VA_WIN)
/* EOF */
