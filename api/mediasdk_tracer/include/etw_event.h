
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2012 Intel Corporation. All Rights Reserved.

File Name: etw_event.h

\* ****************************************************************************** */
#pragma once

#include "msdka_singleton.h"
#include <Windows.h>
#include "wmistr.h"
#include "evntrace.h"
#include "evntprov.h"

#define MPA_EVENT_GUID 0x102562d4, 0x61c6, 0x435a, 0xac, 0x2d, 0xf0, 0x4a, 0xf2, 0xd5, 0x07, 0xa0
#define TRACER_GUID 0x9B540B39, 0x3730, 0x4cc0, 0xB7, 0x53, 0x16, 0x30, 0x2E, 0xD0, 0x06, 0xD2

class ETWEventHelper
{
public:
    static void Write(REGHANDLE handle, int opcode, int level, const TCHAR * msg);
};

template<unsigned long  g1,
         unsigned short g2, 
         unsigned short g3, 
         unsigned char  g4,
         unsigned char  g5,
         unsigned char  g6,
         unsigned char  g7,
         unsigned char  g8,
         unsigned char  g9,
         unsigned char  g10,
         unsigned char  g11>
class ETWMsdkAnalyzerEvent 
    : public Singleton<ETWMsdkAnalyzerEvent<g1,g2,g3,g4,g5,g6,g7,g8,g9,g10,g11> >
{
public:
    ETWMsdkAnalyzerEvent()
        : m_EventHandle()
        , m_bProviderEnable()
    {
        GUID rguid = {g1,g2,g3,g4,g5,g6,g7,g8,g9,g10,g11};

        EventRegister(&rguid, NULL, NULL, &m_EventHandle);
        m_bProviderEnable = 0 != EventProviderEnabled(m_EventHandle, 1,0);
    }

    ETWMsdkAnalyzerEvent::~ETWMsdkAnalyzerEvent()
    {
        if (m_EventHandle)
        {
            EventUnregister(m_EventHandle);
        } 
    }

    virtual void Write(int opcode, int level, const TCHAR * msg)
    {
        ETWEventHelper::Write(m_EventHandle, opcode, level, msg);
    }

protected:
    //consumer is attached, reduce formating overhead 
    //submits event only if consumer attached
    bool      m_bProviderEnable;
    REGHANDLE m_EventHandle;
};

class AutoTrace
{
public:
    AutoTrace(int level, const char *msg)
        : m_level(level)
    {
        ETWMsdkAnalyzerEvent<MPA_EVENT_GUID>::Instance().Write(1, level, msg);
    }

    AutoTrace::~AutoTrace()
    {
        ETWMsdkAnalyzerEvent<MPA_EVENT_GUID>::Instance().Write(2, m_level, "");
    }
protected:
    int m_level;
};

#define TRACER_TASK_PREFIX "TRACER::"
#define MPA_TRACE(TASK_NAME)  AutoTrace trace(0, TASK_NAME);
#define MPA_TRACE_FUNCTION()  MPA_TRACE(TRACER_TASK_PREFIX __FUNCTION__)
