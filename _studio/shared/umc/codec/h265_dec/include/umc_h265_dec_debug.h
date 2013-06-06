/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_DEBUG_H
#define __UMC_H265_DEC_DEBUG_H

#include "vm_time.h"
#include "umc_h265_dec_defs_dec.h"

namespace UMC_HEVC_DECODER
{

void Trace(vm_char * format, ...);

#if 1
#if defined _DEBUG
#define DEBUG_PRINT(x) Trace x
static int pppp = 0;
#define DEBUG_PRINT1(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT1(x)
#endif

#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT1(x)
#endif

/*using of class UMCTimeCounter:

static UMCTimeCounter counter("function1") // global definition

counter.startTimer();
function1()
counter.suspendTimer();
*/
class UMCTimeCounter
{
public:
    UMCTimeCounter(char * str = 0)
        : m_ticks(0)
        , description(str)
    {
        resolution temp_ticks1 = getTicks();
        resolution temp_ticks2 = getTicks();
        delta = temp_ticks2 - temp_ticks1;
        number_of_calls = 0;
    }

    ~UMCTimeCounter()
    {
        if (description)
        {
            printf("\nnumber of ticks %s : %I64u\n", description, m_ticks);
            printf("\nnumber of ticks per call %s : %I64u\n", description, m_ticks/number_of_calls);
        }
        else
        {
            printf("\nnumber of ticks: %I64u\n", m_ticks);
            printf("\nnumber of ticks per call: %I64u\n", m_ticks/number_of_calls);
        }
    }

    void startTimer()
    {
        temp_ticks = getTicks();
    }

    void suspendTimer()
    {
        resolution  temp_temp_ticks = getTicks();
        m_ticks += temp_temp_ticks - temp_ticks - delta;
        number_of_calls++;
    }

private:
#if defined( _WIN32 ) || defined ( _WIN64 )
    typedef unsigned __int64 resolution;
#else
    typedef unsigned long long resolution;
#endif

    resolution m_ticks;
    resolution temp_ticks;
    resolution delta;

    char * description;

    int number_of_calls;

    resolution getTicks()
    {
#if defined(_MSC_VER)
        return (resolution) __rdtsc();
#else // !defined(_MSC_VER)
        return (resolution) vm_time_get_tick();
#endif // defined(_MSC_VER)
    }
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_DEC_DEBUG_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
