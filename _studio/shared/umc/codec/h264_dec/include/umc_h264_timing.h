// Copyright (c) 2003-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

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
        double cpu_freq = (double)vm_time_get_frequency();
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
#endif // MFX_ENABLE_H264_VIDEO_DECODE
