/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_singleton.h"
#include "mfx_itime.h"
#include "vm_time.h"

class PerfCounterTime
    : public MFXSingleton<PerfCounterTime>
    , public ITime
{
    vm_tick m_starTime;
    vm_tick m_Freq;
public:
    PerfCounterTime()
    {
        //We can substract
        m_starTime = vm_time_get_tick();
        m_Freq = vm_time_get_frequency();
    }
    virtual mfxU64 GetTick()
    {
        vm_tick tick = vm_time_get_tick();
        return tick - m_starTime;
    }
    virtual mfxU64 GetFreq()
    {
        return m_Freq;
    }
    virtual mfxF64 Current()
    {
        return (mfxF64)GetTick() / (mfxF64)GetFreq();
    }
    virtual void Wait(mfxU32 nMs)
    {
        ::vm_time_sleep(nMs);
    }
};