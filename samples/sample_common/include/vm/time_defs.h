/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2012-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __TIME_DEFS_H__
#define __TIME_DEFS_H__

#include "mfxdefs.h"
#include "mfx_itt_trace.h"

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

#define MSDK_SLEEP(msec) Sleep(msec)

#define MSDK_USLEEP(usec) \
{ \
    LARGE_INTEGER due; \
    due.QuadPart = -(10*(int)usec); \
    HANDLE t = CreateWaitableTimer(NULL, TRUE, NULL); \
    SetWaitableTimer(t, &due, 0, NULL, NULL, 0); \
    WaitForSingleObject(t, INFINITE); \
    CloseHandle(t); \
}

#else // #if defined(_WIN32) || defined(_WIN64)

#include <unistd.h>

#define MSDK_SLEEP(msec) \
  do { \
    MFX_ITT_TASK_BEGIN("MSDK_SLEEP"); \
    usleep(1000*msec); \
    MFX_ITT_TASK_END(); \
  } while(0)

#define MSDK_USLEEP(usec) \
  do { \
    MFX_ITT_TASK_BEGIN("MSDK_USLEEP"); \
    usleep(usec); \
    MFX_ITT_TASK_END(); \
  } while(0)

#endif // #if defined(_WIN32) || defined(_WIN64)

#define MSDK_GET_TIME(T,S,F) ((mfxF64)((T)-(S))/(mfxF64)(F))

typedef mfxI64 msdk_tick;

msdk_tick msdk_time_get_tick(void);
msdk_tick msdk_time_get_frequency(void);
mfxU64 rdtsc(void);

#endif // #ifndef __TIME_DEFS_H__
