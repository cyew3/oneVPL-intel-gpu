//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.
//
#include "../include/iofunctions.h"
#include "asc_structures.h"
#include "asc_defs.h"

using namespace ns_asc;

void TimeStart(ASCTime* timer) {
#if defined (_WIN32) || (_WIN64)
    QueryPerformanceFrequency(&timer->tFrequency);
    QueryPerformanceCounter(&timer->tStart);
#endif
}

void TimeStart(ASCTime* timer, int index) {
#if defined (_WIN32) || (_WIN64)
    QueryPerformanceCounter(&timer->tPause[index]);
#endif
}

void TimeStop(ASCTime* timer) {
#if defined (_WIN32) || (_WIN64)
    QueryPerformanceCounter(&timer->tStop);
#endif
}

mfxF64 CatchTime(ASCTime *timer, const char* message)
{
    mfxF64
        timeval = 0.0;
    timeval = TimeMeasurement(timer->tStart, timer->tStop, timer->tFrequency);
    ASC_PRINTF("%s %0.3f ms.\n", message, timeval);
    return timeval;
}

mfxF64 CatchTime(ASCTime *timer, int index, const char* message) {
#if defined (_WIN32) || (_WIN64)
    mfxF64
        timeval = 0.0;
    QueryPerformanceCounter(&timer->tPause[index]);
    timeval = TimeMeasurement(timer->tStart, timer->tPause[index], timer->tFrequency);
    ASC_PRINTF("%s %0.3f ms.\n", message, timeval);
    return timeval;
#else
    return 0.0;
#endif
}

mfxF64 CatchTime(ASCTime *timer, int indexInit, int indexEnd, const char* message) {
#if defined (_WIN32) || (_WIN64)
    mfxF64
        timeval = 0.0;
    QueryPerformanceCounter(&timer->tPause[indexEnd]);
    timeval = TimeMeasurement(timer->tPause[indexInit], timer->tPause[indexEnd], timer->tFrequency);
    ASC_PRINTF("%s %0.3f ms.\n", message, timeval);
    return timeval;
#else
    return 0.0;
#endif
}



void imageInit(ASCYUV *buffer) {
    memset(buffer, 0, sizeof(ASCYUV));
}

void nullifier(ASCimageData *Buffer) {
    memset(Buffer, 0, sizeof(ASCimageData));
}

void ImDetails_Init(ASCImDetails *Rdata) {
    memset(Rdata, 0, sizeof(ASCImDetails));
}

mfxStatus ASCTSCstat_Init(ASCTSCstat **logic) {
    for(int i = 0; i < TSCSTATBUFFER; i++)
    {
        try
        {
            logic[i] = new ASCTSCstat;
        }
        catch (...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        memset(logic[i],0,sizeof(ASCTSCstat));
    }
    return MFX_ERR_NONE;
}