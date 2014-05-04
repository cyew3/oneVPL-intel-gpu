
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 Intel Corporation. All Rights Reserved.

File Name: etl_processor.h

\* ****************************************************************************** */
#pragma once

#include <list>
#include <tchar.h>
#include <string>
#include <Windows.h>
#include "wmistr.h"
#include "evntrace.h"


class IEtlCallback
{
public:
    virtual ~IEtlCallback(){}
    virtual void OnEvent(EVENT_RECORD & eventData) = 0;
};

class ETLFileWriter 
    : public IEtlCallback
{
    std::basic_string<TCHAR> m_fileName;
    FILE * m_fDest;
    LONGLONG m_StartPTS;
    bool   m_bFirstEvent;

public:
    ETLFileWriter(const TCHAR * fileName);
    ~ETLFileWriter();
    virtual void OnEvent(EVENT_RECORD & eventData);
};

//uses callback to process input etlfile
class ETLProcessor
    : public std::list<IEtlCallback*>
{
    std::basic_string<TCHAR> m_fileName;
public:
    ETLProcessor(const TCHAR * filename)
        : m_fileName(filename)
    {
    }
    ULONG Start();

private:
    static void WINAPI MyEventCallBack(EVENT_RECORD *EventRecord);
};
