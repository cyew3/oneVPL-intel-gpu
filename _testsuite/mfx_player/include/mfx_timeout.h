/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxdefs.h"

//generic timeout
#define PIPELINE_TIMEOUT_GENERIC 1
//some specific
#define PIPELINE_TIMEOUT_FFS 2

//declaring friend function since de dont have an implementation
void SetTimeout(mfxI32 timeout_id, mfxU32 timeout_val);
//timeout id's support helper
template <mfxI32 init_val = PIPELINE_TIMEOUT_GENERIC>
class TimeoutVal
{
    static mfxU32 &g_val;
public:
    static inline const mfxU32 & val()
    {
        return g_val;
    }
 
    //set timeout for specific id
    friend void SetTimeout(mfxI32 timeout_id, mfxU32 timeout_val);
};

//helper class that counts timeout value at compile time, no multiplication at runtime required
template <mfxI32 granularity, mfxI32 id = PIPELINE_TIMEOUT_GENERIC>
class Timeout
{
    mfxI32 m_nTicks;
    mfxI32 m_timeout;
    static const char * mpa_name;
    static       char * get_mpa_task_name() 
    { 
        static  char   pmpa_name[32];
        sprintf(pmpa_name, "Sleep(%d)", interval); 
        return pmpa_name;
    }

public:
    static const mfxI32 interval = granularity;


    Timeout()
        : m_nTicks()
        , m_timeout(TimeoutVal<id>::val() / granularity)
    {}
    //true if wait
    //false if timeout happened
    inline bool wait(const char * wait_name = mpa_name)
    {
        if (m_nTicks >= m_timeout)
            return false;

        MPA_TRACE(wait_name); 
        vm_time_sleep(granularity); 
        m_nTicks++;

        return true;
    }

    //every call increments wait counter. used for external waiting
    inline int wait_0()
    {
        if (m_nTicks >= m_timeout)
            return false;

        m_nTicks++;

        return true;        
    }
    //true  if timeout
    inline operator bool()const { return m_nTicks >= m_timeout; }
    inline int elapsed()const { return m_nTicks*granularity; }
};

template<mfxI32 g, mfxI32 id>
const char *Timeout<g, id>::mpa_name = Timeout<g, id>::get_mpa_task_name();