/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "trace_utils.h"

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>
#include <stdio.h>

#undef WINVER
#define WINVER _WIN32_WINNT_VISTA
#include <evntprov.h>
//#include <evntrace.h>

static const GUID MPA_GUID_CUSTOM = { 0x102562d4, 0x61c6, 0x435a, 0xac, 0x2d, 0xf0, 0x4a, 0xf2, 0xd5, 0x07, 0xa0 };

////////////////////////////////////////////////////////////////////////////
// Class to keep ETW handle

class EventHandle
{
public:
    EventHandle()
    {
        m_EventHandle = 0;
    }
    ~EventHandle()
    {
        if (m_EventHandle)
        {
            EventUnregister(m_EventHandle);
        }
    }
    REGHANDLE GetHandle()
    {
        if (!m_EventHandle)
        {
            EventRegister(&MPA_GUID_CUSTOM, NULL, NULL, &m_EventHandle);
        }
        return m_EventHandle;
    }
protected:
    REGHANDLE m_EventHandle;
};

static EventHandle ETWGlobalHandle;

////////////////////////////////////////////////////////////////////////////
// AutoTrace class implementation

AutoTrace::AutoTrace(const char *name, int level)
{
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data_descriptor;

    m_level = level;

    memset(&descriptor, 0, sizeof(EVENT_DESCRIPTOR));
    descriptor.Opcode = 1; //EVENT_TRACE_TYPE_START
    descriptor.Level = (UCHAR)level;
    EventDataDescCreate(&data_descriptor, name, (ULONG)(strlen(name) + 1));

    EventWrite(ETWGlobalHandle.GetHandle(), &descriptor, 1, &data_descriptor);
}

AutoTrace::~AutoTrace()
{
    EVENT_DESCRIPTOR descriptor;

    memset(&descriptor, 0, sizeof(EVENT_DESCRIPTOR));
    descriptor.Opcode = 2; //EVENT_TRACE_TYPE_END
    descriptor.Level = (UCHAR)m_level;

    EventWrite(ETWGlobalHandle.GetHandle(), &descriptor, 0, 0);
}

#endif // #if defined(_WIN32) || defined(_WIN64)

#if defined(LINUX32) || defined(LINUX64)
#if defined (MFX_TRACE_MPA_VTUNE)

#include <ittnotify.h>

static __itt_domain* GetDomain(){
    static __itt_domain* pTheDomain  = __itt_domain_create("mfx_tools");
    return pTheDomain;
}

AutoTrace::AutoTrace(const char *name, int level)
{
    __itt_string_handle *pSH = __itt_string_handle_create(name);
    __itt_task_begin(GetDomain(), __itt_null, __itt_null, pSH);
}

AutoTrace::~AutoTrace()
{
    __itt_task_end(GetDomain());
}

#endif
#endif //#if defined(LINUX32) || defined(LINUX64)
