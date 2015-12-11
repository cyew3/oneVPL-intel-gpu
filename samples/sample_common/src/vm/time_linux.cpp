/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if !defined(_WIN32) && !defined(_WIN64)

#include "vm/time_defs.h"
#include <sys/time.h>

#define MSDK_TIME_MHZ 1000000

msdk_tick msdk_time_get_tick(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (msdk_tick)tv.tv_sec * (msdk_tick)MSDK_TIME_MHZ + (msdk_tick)tv.tv_usec;
}

msdk_tick msdk_time_get_frequency(void)
{
    return (msdk_tick)MSDK_TIME_MHZ;
}

mfxU64 rdtsc(void){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((mfxU64)hi << 32) | lo;
}


#endif // #if !defined(_WIN32) && !defined(_WIN64)
