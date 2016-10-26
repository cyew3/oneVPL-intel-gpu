//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_TIMING_H
#define __UMC_H264_TIMING_H

//#define USE_DETAILED_H264_TIMING

#include "ippcore.h"
#include "vm_time.h"

namespace UMC
{

#ifdef USE_DETAILED_H264_TIMING

#define GET_TIMING_TICKS vm_time_get_tick()

class TimingInfo
{
public:
    TimingInfo()
    {
        Reset();
    }

    ~TimingInfo()
    {
        Ipp64f cpu_freq = (Ipp64f)vm_time_get_frequency();
        vm_string_printf(VM_STRING("decode_time - %.2f\t\n"), decode_time/cpu_freq);
        vm_string_printf(VM_STRING("reconstruction_time - %.2f\t\n"), reconstruction_time/cpu_freq);
        vm_string_printf(VM_STRING("deblocking_time - %.2f\t\n"), deblocking_time/cpu_freq);
        vm_string_printf(VM_STRING("all - %.2f\t\n"), (deblocking_time + decode_time + reconstruction_time)/cpu_freq);
    }

    void Reset()
    {
        vm_tick start_tick, end_tick;
        start_tick = GET_TIMING_TICKS;
        end_tick = GET_TIMING_TICKS;

        decode_time = 0;
        reconstruction_time = 0;
        deblocking_time = 0;

        m_compensative_const = 0;//end_tick - start_tick;
    }

    vm_tick decode_time;
    vm_tick reconstruction_time;
    vm_tick deblocking_time;

    vm_tick m_compensative_const;
};

extern TimingInfo* GetGlobalTimingInfo();

#define INIT_TIMING \
    GetGlobalTimingInfo();

#define START_TICK      vm_tick start_tick = GET_TIMING_TICKS;

#define END_TICK(x)         \
{\
    vm_tick end_tick = GET_TIMING_TICKS;\
    GetGlobalTimingInfo()->x += (end_tick - start_tick) - GetGlobalTimingInfo()->m_compensative_const;\
}

#define START_TICK1      vm_tick start_tick1 = GET_TIMING_TICKS;
#define END_TICK1(x)         \
{\
    vm_tick end_tick = GET_TIMING_TICKS;\
    GetGlobalTimingInfo()->x += (end_tick - start_tick1) - GetGlobalTimingInfo()->m_compensative_const;\
}

//#define ENABLE_OPTIONAL
#if ENABLE_OPTIONAL
    #define START_TICK_OPTIONAL
    #define END_TICK_OPTIONAL(x)
#else
    #define START_TICK_OPTIONAL START_TICK
    #define END_TICK_OPTIONAL(x)  END_TICK(x)

#endif

#else

#define INIT_TIMING
#define START_TICK
#define START_TICK1
#define END_TICK(x)
#define END_TICK1(x)

#endif

} // namespace UMC

#endif // __UMC_H264_TIMING_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
