/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: test_h264_dec_command_line.cpp

\* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <atlbase.h>
#endif // #if defined(_WIN32) || defined(_WIN64)
#include "test_statistics.h"

Timer::Timer()
: m_freq((Ipp64f)vm_time_get_frequency())
, m_start(NOT_STARTED)
, m_accum(0)
, m_lastTiming(0)
{
}

Ipp64f Timer::Stop()
{
    vm_tick stop = vm_time_get_tick();
    m_lastTiming = (stop - m_start) / m_freq;
    if (m_start != NOT_STARTED)
        m_accum += m_lastTiming;

    m_start = NOT_STARTED;
    return m_lastTiming;
}

Ipp64f Timer::CurrentTiming()
{
    vm_tick stop = vm_time_get_tick();
    m_lastTiming = (stop - m_start) / m_freq;
    return m_lastTiming;
}

PerfWriter::PerfWriter()
: m_isWritten(false)
, m_fd(NULL)
, m_bOwnFD(false)
{
}

PerfWriter::~PerfWriter()
{
    Close();
}

bool PerfWriter::Init(const vm_char* perfFileName, const vm_char* streamName, vm_file * fdIn)
{
    // try to open new file
    if (NULL == fdIn)
    {
        vm_file* fd = vm_file_fopen(perfFileName, VM_STRING("a"));
        if (!fd)
            return false;

        // new file opened update state now
        Close();
        m_fd = fd;
    }
    else
    {
        m_fd = fdIn;
    }
    
    m_bOwnFD = (NULL == fdIn);

#if defined(_WIN32) || defined(_WIN64)
    USES_CONVERSION;
    const char* ansiStreamName = (sizeof(streamName[0]) == sizeof(char)) ? (char *)streamName : W2A(streamName);
    strncpy(m_streamName, ansiStreamName, MAX_PATH);
#else
    vm_string_strncpy(m_streamName, streamName, MAX_PATH);
#endif
    m_isWritten = false;
    return true;
}

void PerfWriter::WritePerfResult(const Timer& timer, mfxU32 frameCount)
{
    vm_file_fprintf(m_fd, VM_STRING("%s, "), m_streamName);
    if (timer.OverallTiming() > 0)
        vm_file_fprintf(m_fd, VM_STRING("%.3f, %d\n"), frameCount / timer.OverallTiming(), frameCount);
    else
        vm_file_fprintf(m_fd, VM_STRING("NaN, %d\n"), frameCount);
    m_isWritten = true;
}

void PerfWriter::Close()
{
    if (m_fd && m_bOwnFD)
    {
        if (!m_isWritten)
            vm_file_fprintf(m_fd, VM_STRING("%s, FAILED\n"), m_streamName);
        vm_file_fclose(m_fd);
    }
    m_fd = 0;
}
