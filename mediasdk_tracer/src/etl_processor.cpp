
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2014 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "etl_processor.h"
#include "evntcons.h"
#include "configuration.h"

ULONG ETLProcessor::Start()
{
    ULONG Status = 0;
    EVENT_TRACE_LOGFILE LogFile;
    TRACEHANDLE Handle;

    ZeroMemory(&LogFile, sizeof(EVENT_TRACE_LOGFILE));

    LogFile.LogFileName         = (LPTSTR)m_fileName.c_str();
    LogFile.EventRecordCallback = MyEventCallBack;
    LogFile.Context             = (PVOID)this;
    LogFile.ProcessTraceMode   |= PROCESS_TRACE_MODE_EVENT_RECORD;


    Handle = OpenTrace(&LogFile);
    if (Handle == INVALID_PROCESSTRACE_HANDLE) {
        return Status;
    }

    Status = ProcessTrace(&Handle, 1, NULL, NULL);
    if (Status != ERROR_SUCCESS) {
    }

    Status = CloseTrace(Handle);
    if (Status != ERROR_SUCCESS) {
    }

    return Status;
}


void WINAPI ETLProcessor::MyEventCallBack(PEVENT_RECORD EventRecord)
{
    if (EventRecord)
    {
        ETLProcessor * pEtl = reinterpret_cast<ETLProcessor *>(EventRecord->UserContext);
        for(ETLProcessor::iterator it = pEtl->begin(); it != pEtl->end(); it++){
            (*it)->OnEvent(*EventRecord);
        }
    }
}

ETLFileWriter::ETLFileWriter(const TCHAR * fileName)
: m_fileName(fileName)
, m_fDest()
, m_StartPTS(0)
, m_bFirstEvent(true){
}

void ETLFileWriter::OnEvent(EVENT_RECORD & eventData)
{
    if (!m_fDest)
    {
        _tfopen_s(&m_fDest,  m_fileName.c_str(), _T("a"));
    }

    
    if (m_fDest)
    {
        EVENT_HEADER &hdr = eventData.EventHeader;

        //first event is ussually from some classic provider, lets skip it
        if (EVENT_HEADER_FLAG_CLASSIC_HEADER & hdr.Flags)
        {
            return;
        }

        if (m_bFirstEvent)
        {
            m_StartPTS    = hdr.TimeStamp.QuadPart;
            hdr.TimeStamp.QuadPart = 0;
            m_bFirstEvent = false;
        }else
        {
            hdr.TimeStamp.QuadPart -= m_StartPTS;
        }

        if (gc.record_thread_id)
        {
            _ftprintf_s(m_fDest, _T("%d:"), hdr.ThreadId);
        }

        if (gc.record_timestamp_mode != GlobalConfiguration::TIMESTAMP_NONE)
        {
            _ftprintf_s(m_fDest, _T("%.5f "), (double)hdr.TimeStamp.QuadPart / 1e7);
        }
        
        _ftprintf_s(m_fDest, _T("%s\n"), eventData.UserData);
    }
}

ETLFileWriter::~ETLFileWriter()
{
    if (m_fDest)
    {
        fclose(m_fDest);
    }
}