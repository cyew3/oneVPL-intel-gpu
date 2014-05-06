/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: test_statistics.h

\* ****************************************************************************** */

#ifndef __TEST_STATISTICS_H__
#define __TEST_STATISTICS_H__

#include "mfxdefs.h"
#include "vm_time.h"
#include "vm_file.h"
#include "vm_strings.h"

#include <stdio.h>

class Timer
{
    static const vm_tick NOT_STARTED = -1;

public:
    Timer();
    Ipp64f OverallTiming() const { return m_accum; }
    Ipp64f LastTiming() const { return m_lastTiming; }
    void Start() { m_start = vm_time_get_tick(); }
    void Reset() { m_accum = 0; }
    Ipp64f Stop();
    Ipp64f CurrentTiming();

private:
    Ipp64f m_freq;
    vm_tick m_start;
    Ipp64f m_accum;
    Ipp64f m_lastTiming;
};

class PerfWriter
{
public:
    PerfWriter();
    ~PerfWriter();
    bool Init(const vm_char* perfFileName, const vm_char* streamName, vm_file * fdIn = NULL);
    void WritePerfResult(const Timer& timer, mfxU32 frameCount);

protected:
    void Close();

private:
    char m_streamName[MAX_PATH + 1];
    bool m_isWritten;
    vm_file* m_fd;
    bool  m_bOwnFD;
};

#endif //__TEST_STATISTICS_H__