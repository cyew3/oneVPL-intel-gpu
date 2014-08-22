/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.

File Name: configuration.cpp

\* ****************************************************************************** */
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>
#include <algorithm>
#include <functional>
#include <vector>
#include "etw_event.h"
#include "configuration.h"
#include "msdka_serial.h"

GlobalConfiguration gc;

GlobalConfiguration::GlobalConfiguration(void) {
    sdk_dll[0]=0;

    instant_init = false;
    record_configuration=0;
    record_configuration_per_frame=0;
    edit_configuration=0;
    edit_configuration_per_frame=0;
    level = 1;

    memset(&decode, sizeof (decode),0);
    memset(&vpp   , sizeof (vpp),0);
    memset(&encode, sizeof (encode),0);
    debug_view=0;

    record_raw_bits_start=0;
    record_raw_bits_end=0;
    record_raw_bits_module=0;
    record_timestamp_mode=TIMESTAMP_NONE;

    edit_configuration_lines=0;

    _tcscpy_s(cfg_file,sizeof(cfg_file)/sizeof(TCHAR)-1,TEXT("sdk-analyzer.cfg"));
    _tcscpy_s(log_file,sizeof(log_file)/sizeof(TCHAR)-1,TEXT("sdk-analyzer.log"));
    _tcscpy_s(raw_file,sizeof(raw_file)/sizeof(TCHAR)-1,TEXT("sdk-analyzer.raw"));

    tls_idx_threadinfo = TlsAlloc();
    threads_info.reserve(20);

    InitializeCriticalSection(&threadinfo_access);
    InitializeCriticalSection(&surfacesdata_access);
}

GlobalConfiguration::~GlobalConfiguration(void) {
    while (edit_configuration_lines) {
        LinkedString *ls=edit_configuration_lines->next;
        delete edit_configuration_lines->string;
        edit_configuration_lines=ls;
    }
    DeleteCriticalSection(&surfacesdata_access);
    DeleteCriticalSection(&threadinfo_access);
    //lets lose this handles for now
    //CloseHandle(&surfacesdata_access);
    //CloseHandle(&threadinfo_access);
    WriteLogData();
    TlsFree(tls_idx_threadinfo);
}

namespace
{
    const TCHAR KEY_ANALYZER[] = TEXT("Software\\Intel\\MediaSDK\\Debug\\Analyzer");

    const TCHAR KEY_SDK_DLL[] =  TEXT("SDK DLL");
    const TCHAR KEY_LOG_FILE [] = TEXT("Log File");
    const TCHAR KEY_CFG_FILE [] = TEXT("CFG File");
    const TCHAR KEY_RAW_FILE [] = TEXT("Raw File");
    const TCHAR KEY_WORK_WOUT_GUI [] = TEXT("Work Without Gui");
    const TCHAR KEY_REC_CFG [] = TEXT("Record Configuration"); 
    const TCHAR KEY_REC_CFG_PER_FRAME [] = TEXT("Record Configuration Per Frame"); 
    const TCHAR KEY_REC_POINTERS [] = TEXT("Record Pointers"); 
    const TCHAR KEY_REC_THREAD_ID [] = TEXT("Record ThreadID"); 
    const TCHAR KEY_EDT_CFG [] = TEXT("Edit Configuration"); 
    const TCHAR KEY_EDT_CFG_PER_FRAME [] = TEXT("Edit Configuration Per Frame"); 
    const TCHAR KEY_SKIP_DECODE_SYNC [] = TEXT("Skip DECODE Sync Operation"); 
    const TCHAR KEY_SKIP_VPP_SYNC [] = TEXT("Skip VPP Sync Operation"); 
    const TCHAR KEY_SYNC_DECODE [] = TEXT("Sync Decode"); 
    const TCHAR KEY_SYNC_ENCODE [] = TEXT("Sync Encode"); 
    const TCHAR KEY_SYNC_VPP [] = TEXT("Sync VPP"); 
    const TCHAR KEY_HANDLE_DEV_BUSY_DECODE [] = TEXT("Handle DeviceBusy Decode");
    const TCHAR KEY_HANDLE_DEV_BUSY_VPP [] = TEXT("Handle DeviceBusy VPP");
    const TCHAR KEY_HANDLE_DEV_BUSY_ENCODE[] = TEXT("Handle DeviceBusy Encode");
    const TCHAR KEY_STUB_DECOED [] = TEXT("Stub Decode");
    const TCHAR KEY_STUB_VPP [] = TEXT("Stub VPP");
    const TCHAR KEY_STUB_ENCODE [] = TEXT("Stub Encode");
    const TCHAR KEY_DBG_VIEW [] = TEXT("Debug View");
    const TCHAR KEY_ASYNC_DUMP [] = TEXT("Async Dump");
    const TCHAR KEY_REC_START_TIME [] = TEXT("Record Raw Bits Start");
    const TCHAR KEY_REC_END_TIME [] = TEXT("Record Raw Bits End");
    const TCHAR KEY_REC_MODULE [] = TEXT("Record Raw Bits Module");
        const TCHAR KEY_VALUE_ENCODE [] = TEXT("encode");
        const TCHAR KEY_VALUE_DECODE [] = TEXT("decode");
        const TCHAR KEY_VALUE_VPP [] = TEXT("vpp");
        const TCHAR KEY_VALUE_OUTPUT [] = TEXT("output");
        const TCHAR KEY_VALUE_INPUT [] = TEXT("input");
        const TCHAR KEY_VALUE_HEADER [] = TEXT("header");
        const TCHAR KEY_VALUE_FORCE [] = TEXT("force");
    const TCHAR KEY_RECORD_PTS [] = TEXT("Record Timestamp");
        const TCHAR KEY_VALUE_LOCAL [] = TEXT("LOCAL");
        const TCHAR KEY_VALUE_UTC [] = TEXT("UTC");
        const TCHAR KEY_VALUE_RALATIVE [] = TEXT("relative");
    const TCHAR KEY_SDK_VER [] = TEXT("SDK Version");
    const TCHAR FORCE_METRO_APP [] = TEXT("Force Metro App");
    
}


class CaseInsensitiveEqual
    : public std::binary_function<std::string, std::string, bool>
{
public:
    bool operator ()(const std::string & a, const std::string & b)const
    {
        return !_stricmp(a.c_str(), b.c_str()) ;
    }
};

//TODO: validate parse params
void GlobalConfiguration::ValidateRegistry(HKEY hkey)
{
    std::vector<std::basic_string<TCHAR> > options;

    options.push_back(KEY_SDK_DLL);
    options.push_back(KEY_LOG_FILE );
    options.push_back(KEY_CFG_FILE );
    options.push_back(KEY_RAW_FILE );
    options.push_back(KEY_WORK_WOUT_GUI );
    options.push_back(KEY_REC_CFG );
    options.push_back(KEY_REC_CFG_PER_FRAME );
    options.push_back(KEY_REC_POINTERS );
    options.push_back(KEY_REC_THREAD_ID );
    options.push_back(KEY_EDT_CFG );
    options.push_back(KEY_EDT_CFG_PER_FRAME );
    options.push_back(KEY_SKIP_DECODE_SYNC );
    options.push_back(KEY_SKIP_VPP_SYNC );
    options.push_back(KEY_SYNC_DECODE );
    options.push_back(KEY_SYNC_ENCODE );
    options.push_back(KEY_SYNC_VPP );
    options.push_back(KEY_HANDLE_DEV_BUSY_DECODE );
    options.push_back(KEY_HANDLE_DEV_BUSY_VPP );
    options.push_back(KEY_HANDLE_DEV_BUSY_ENCODE);
    options.push_back(KEY_STUB_DECOED );
    options.push_back(KEY_STUB_VPP );
    options.push_back(KEY_STUB_ENCODE );
    options.push_back(KEY_DBG_VIEW );
    options.push_back(KEY_ASYNC_DUMP );
    options.push_back(KEY_REC_START_TIME );
    options.push_back(KEY_REC_END_TIME );
    options.push_back(KEY_REC_MODULE );
    options.push_back(KEY_RECORD_PTS );
    options.push_back(KEY_SDK_VER );
    options.push_back(FORCE_METRO_APP); 

    HKEY key;
    if (ERROR_SUCCESS!=RegOpenKey(hkey, KEY_ANALYZER, &key)) return;

    for(size_t i = 0; ; i++)
    {
        TCHAR value_name [2048];
        DWORD value_name_len = 2048;
        if (ERROR_SUCCESS != RegEnumValue(key, i, value_name, &value_name_len, NULL, NULL, NULL, NULL))
        {
            break;
        }
        std::vector<std::basic_string<TCHAR> >::iterator it = 
            std::find_if(options.begin(), options.end(), std::bind1st(CaseInsensitiveEqual(), value_name));
        if (it == options.end())
        {
            RECORD_CONFIGURATION({
                dump_format_wprefix(fd, level, 1,TEXT("ERROR: unknown registry parameter: "), TEXT("HKCU\\%s\\%s, skipped"), KEY_ANALYZER, value_name);
            });
        }
    }

    RegCloseKey(key);
}

void GlobalConfiguration::ReadRegistrySetting(HKEY hkey) {

    /* load registry settings */
    //Sleep(100000);

    HKEY key;
    if (ERROR_SUCCESS!=RegOpenKey(hkey,KEY_ANALYZER,&key)) return;
    DWORD sdk_dll_size=sizeof(sdk_dll);
    RegQueryValueEx(key,KEY_SDK_DLL,0,0,(LPBYTE)sdk_dll,&sdk_dll_size);
    use_dispatcher = TEXT('\0') == gc.sdk_dll[0];//in case of empty record in registry
    DWORD log_file_size=sizeof(log_file);
    RegQueryValueEx(key,KEY_LOG_FILE,0,0,(LPBYTE)log_file,&log_file_size);
    DWORD cfg_file_size=sizeof(cfg_file);
    RegQueryValueEx(key,KEY_CFG_FILE,0,0,(LPBYTE)cfg_file,&cfg_file_size);
    DWORD raw_file_size=sizeof(raw_file);
    RegQueryValueEx(key,KEY_RAW_FILE,0,0,(LPBYTE)raw_file, &raw_file_size);

    DWORD dword_size=sizeof(DWORD);
    RegQueryValueEx(key, FORCE_METRO_APP ,0,0,(LPBYTE)&is_metro_app,&dword_size);
    RegQueryValueEx(key,TEXT("Work Without Gui"),0,0,(LPBYTE)&run_without_gui,&dword_size);
    RegQueryValueEx(key,TEXT("Record Configuration"),0,0,(LPBYTE)&record_configuration,&dword_size);
    RegQueryValueEx(key,TEXT("Record Configuration Per Frame"),0,0,(LPBYTE)&record_configuration_per_frame,&dword_size);
    RegQueryValueEx(key,TEXT("Record Pointers"),0,0,(LPBYTE)&record_pointers,&dword_size);
    RegQueryValueEx(key,TEXT("Record ThreadID"),0,0,(LPBYTE)&record_thread_id,&dword_size);
    RegQueryValueEx(key,TEXT("Edit Configuration"),0,0,(LPBYTE)&edit_configuration,&dword_size);
    RegQueryValueEx(key,TEXT("Edit Configuration Per Frame"),0,0,(LPBYTE)&edit_configuration_per_frame,&dword_size);
    RegQueryValueEx(key,TEXT("Skip DECODE Sync Operation"),0,0,(LPBYTE)&decode.skip_sync_operation,&dword_size);
    RegQueryValueEx(key,TEXT("Skip VPP Sync Operation"),0,0,(LPBYTE)&vpp.skip_sync_operation,&dword_size);
    RegQueryValueEx(key,TEXT("Sync Decode"),0,0,(LPBYTE)&decode.sync,&dword_size);
    RegQueryValueEx(key,TEXT("Sync Encode"),0,0,(LPBYTE)&encode.sync,&dword_size);
    RegQueryValueEx(key,TEXT("Sync VPP"),0,0,(LPBYTE)&vpp.sync,&dword_size);
    RegQueryValueEx(key,TEXT("Handle DeviceBusy Decode"),0,0,(LPBYTE)&decode.handle_device_busy,&dword_size);
    RegQueryValueEx(key,TEXT("Handle DeviceBusy VPP"),0,0,(LPBYTE)&vpp.handle_device_busy,&dword_size);
    RegQueryValueEx(key,TEXT("Handle DeviceBusy Encode"),0,0,(LPBYTE)&encode.handle_device_busy,&dword_size);
    RegQueryValueEx(key,TEXT("Level"),0,0,(LPBYTE)&level,&dword_size);

    RegQueryValueEx(key,TEXT("Stub Decode"),0,0,(LPBYTE)&decode.stub_mode, &dword_size);
    RegQueryValueEx(key,TEXT("Stub VPP"),0,0,(LPBYTE)&vpp.stub_mode, &dword_size);
    RegQueryValueEx(key,TEXT("Stub Encode"),0,0,(LPBYTE)&encode.stub_mode,&dword_size);


    //very slow dump actually uses global mutex , file mapping, and depends on consumer performance
    RegQueryValueEx(key,TEXT("Debug View"),0,0,(LPBYTE)&debug_view,&dword_size);
    //dump when dll is unloaded, log is limiting in memory 
    RegQueryValueEx(key,TEXT("Async Dump"),0,0,(LPBYTE)&async_dump,&dword_size);


    TCHAR module[1024];
    DWORD module_size=sizeof(module);
    RegQueryValueEx(key,TEXT("Record Raw Bits Start"),0,0,(LPBYTE)&record_raw_bits_start,&dword_size);
    RegQueryValueEx(key,TEXT("Record Raw Bits End"),0,0,(LPBYTE)&record_raw_bits_end,&dword_size);
    RegQueryValueEx(key,TEXT("Record Raw Bits Module"),0,0,(LPBYTE)module, &module_size);

    if (is_metro_app || (true==(is_metro_app = IsMetroProcess())))
    {
        CreateMetroSpecificObjects();
    }

    //Parsing module

    record_raw_bits_module = 0;
    
    if (0!=_tcsstr(module,TEXT("encode"))){record_raw_bits_module|=ENCODE; }else
    if (0!=_tcsstr(module,TEXT("decode"))){record_raw_bits_module|=DECODE;  }else
    if (0!=_tcsstr(module,TEXT("vpp")))   {record_raw_bits_module|=VPP;    }
    
    if (0!=_tcsstr(module,TEXT("output"))){record_raw_bits_module|=DIR_OUTPUT; } else
    if (0!=_tcsstr(module,TEXT("input"))) {record_raw_bits_module|=DIR_INPUT; } else
    if (0!=_tcsstr(module,TEXT("header"))) {record_raw_bits_module|=DIR_HEADER; }

    if (0!=_tcsstr(module,TEXT("force"))) {record_raw_bits_module|=FORCE_NO_SYNC; }

    
    if (!GET_COMPONENT(record_raw_bits_module) || 
        !GET_DIRECTION(record_raw_bits_module))
        record_raw_bits_start=record_raw_bits_end=0;

    TCHAR timestamp[1024];
    DWORD timestamp_size=sizeof(timestamp);
    RegQueryValueEx(key,TEXT("Record Timestamp"),0,0,(LPBYTE)timestamp, &timestamp_size);
    
    if (!_tcsicmp(timestamp,TEXT("LOCAL"))) record_timestamp_mode = TIMESTAMP_LOCAL; else
    if (!_tcsicmp(timestamp,TEXT("UTC"))) record_timestamp_mode = TIMESTAMP_UTC; else
    {
        if (!_tcsicmp(timestamp,TEXT("relative"))) 
        {
            QueryPerformanceCounter(&qpc_start);
            QueryPerformanceFrequency(&qpc_freq);
            record_timestamp_mode = TIMESTAMP_RELATIVE; 
        }else
            record_timestamp_mode = TIMESTAMP_NONE;
    }

    TCHAR version_string[1024];
    DWORD version_string_size=sizeof(version_string);
    
    RegQueryValueEx(key,TEXT("SDK Version"),0,0,(LPBYTE)version_string, &version_string_size);

    if(!_tcsicmp(version_string, _T("1.0")))
    {
        use_version.Major = 1;
        use_version.Minor = 0;
    }
    else if(!_tcsicmp(version_string, _T("1.1")))
    {
        use_version.Major = 1;
        use_version.Minor = 1;
    }
    else if(!_tcsicmp(version_string, _T("1.2")))
    {
        use_version.Major = 1;
        use_version.Minor = 2;
    }

    RegCloseKey(key);
}

void GlobalConfiguration::DetectGUIPresence(void)
{
    if (run_without_gui)
    {
        gc.is_gui_present = true;
        return;
    }

    HANDLE mut = CreateMutex(NULL, TRUE, _T("mediasdk_tracer_lock"));
    if (mut)
    {
        //no errors means GUI not owned mutex
        gc.is_gui_present = (GetLastError() != 0);

        TCHAR msg[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, msg, 1024, 0);
        RECORD_CONFIGURATION({
            dump_format_wprefix(fd, level, 1,TEXT("Acquiring named mutex mediasdk_tracer_lock : GetLastError()= "),TEXT("%s"), msg);
        });

        CloseHandle(mut);
    }

}

void GlobalConfiguration::ReadConfiguration(void) {
    FILE *fd=NULL;
    _tfopen_s(&fd,cfg_file,TEXT("r"));
    if (fd) {
        LinkedString **pls=&edit_configuration_lines;
        while (!feof(fd)) { \
            LinkedString *ls=new LinkedString;
            if (!ls) break;
            ls->next=0;
            ls->string=new TCHAR[256];
            _fgetts(ls->string,255,fd);
            *pls=ls;
            pls=&(ls->next);
        }
        fclose(fd);
    }
}

GlobalConfiguration::ThreadData & GlobalConfiguration::GetThreadData()
    {
    if (0 == TlsGetValue(gc.tls_idx_threadinfo))
    {
        EnterCriticalSection(&gc.threadinfo_access);
        gc.threads_info.push_back(GlobalConfiguration::ThreadData());
        gc.threads_info.back().thread_id = (int)gc.threads_info.size();
        TlsSetValue(gc.tls_idx_threadinfo, (LPVOID)&gc.threads_info.back());
        LeaveCriticalSection(&gc.threadinfo_access);
    }

    return *((GlobalConfiguration::ThreadData*) TlsGetValue(gc.tls_idx_threadinfo));
}

void GlobalConfiguration::WriteLogData()
{
    if (async_dump != ASYNC)
        return;

    //::MessageBoxA(0,"GlobalConfiguration", "WriteLogData", MB_OK);    
    async_dump = ASYNC_NONE;

    FILE *fd=NULL;  
    _tfopen_s(&fd,log_file,TEXT("a+")); 
    if (!fd)
        return;

    std::vector<std::list <TLSData>::iterator> sorted_iterators;
    for (size_t i = 0; i<threads_info.size(); i++)
    {
        sorted_iterators.push_back(threads_info[i].messages.begin());
    }

    bool bShowTimestamps =  (gc.record_timestamp_mode == gc.TIMESTAMP_RELATIVE);
    gc.record_timestamp_mode = gc.TIMESTAMP_NONE;

    TLSData *pDataMin = NULL;
    
    for (;;)
    {
        TLSData *pData = NULL;
        size_t it_min = 0;

        for (size_t i = 0;i < sorted_iterators.size(); i++)
        {
            if(sorted_iterators[i] == threads_info[i].messages.end())
                continue;
         
            GlobalConfiguration::TLSData &refData = *sorted_iterators[i];

            if (!pData || refData.timestamp.QuadPart < pData->timestamp.QuadPart)
            {
                pData = &refData;
                it_min = i;
            }
        } 

        if (NULL == pData)
            break ;

        if (NULL == pDataMin)
            pDataMin = pData;

        //moving iterator
        sorted_iterators[it_min]++;

        TCHAR pre_str[1024] = TEXT("");

        for (int i = 0; i < sizeof (pData->prefix) / sizeof(pData->prefix[0]);i++)
        {
            if (!pData->prefix[i])
                break;
             
            _tcscat_s(pre_str, sizeof (pre_str)/sizeof (pre_str[0]), pData->prefix[i]);
        }

        if (bShowTimestamps)
        {
            double diff = ((double) pData->timestamp.QuadPart - (double)pDataMin->timestamp.QuadPart)/
                (double)(gc.qpc_freq.QuadPart);

            if (gc.record_thread_id)
            {
                _ftprintf(fd, TEXT("%d:%.3f:%s%s\n"), it_min+1, diff, pre_str,pData->str.c_str());
            }
            else
            {
                _ftprintf(fd, TEXT("%.3f:%s%s\n"), diff, pre_str,pData->str.c_str());
            }
        }
        else
        {
            if (gc.record_thread_id)
            {
                _ftprintf(fd, TEXT("%d:%s%s\n"), it_min+1, pre_str,pData->str.c_str());
            }
            else
            {
                _ftprintf(fd, TEXT("%s%s\n"), pre_str,pData->str.c_str());
            }
        }
    }
    
    if (fd)
    {
        fclose(fd);
    }
}

void GlobalConfiguration::RegisterSurface(int component, int component_entry, mfxFrameSurface1 *pSurface)
{
    if (NULL == pSurface)
        return;
    EnterCriticalSection(&surfacesdata_access);
    m_surfacesData[std::pair<int, int>(component, component_entry)].insert(pSurface->Data.MemId);
    LeaveCriticalSection(&surfacesdata_access);
}

int  GlobalConfiguration::GetSurfacesCount(int component, int component_entry)
{
    return (int)m_surfacesData[std::pair<int,int>(component, component_entry)].size();
}

//either use local storage for events or ETW
void GlobalConfiguration::TLSDataList::push_back(const TLSData& refData)
{
    if (gc.async_dump != ASYNC_ETW)
    {
        list::push_back(refData);
        return;
    }

    TCHAR pre_str[1024] = TEXT("");
    
    for (int i = 0; i < sizeof (refData.prefix) / sizeof(refData.prefix[0]); i++)
    {
        if (!refData.prefix[i])
            break;

        _tcscat_s(pre_str, sizeof (pre_str)/sizeof (pre_str[0]), refData.prefix[i]);
    }
    _tcscat_s(pre_str, sizeof (pre_str)/sizeof (pre_str[0]), refData.str.c_str());

    if (gc.is_metro_app)
    {
        if (refData.level <= gc.level)
        {
            if (WaitForSingleObject((HANDLE)gc.MetroHandles.BufferReadyEvent.hdl, 500) == WAIT_OBJECT_0)
            {
                ResetEvent(gc.MetroHandles.BufferReadyEvent.hdl);

                _tcscpy_s(gc.m_sharedBuffer->szString
                    , sizeof (gc.m_sharedBuffer->szString)/sizeof (gc.m_sharedBuffer->szString[0])
                    , pre_str);
                gc.m_sharedBuffer->dwLevel = refData.level;
                SetEvent((HANDLE)gc.MetroHandles.DataReadyEvent.hdl);
            }
            else
            {
                //we missed the event - wakeup push thread
                SetEvent((HANDLE)gc.MetroHandles.DataReadyEvent.hdl);
            }
        }
    }
    else{
        ETWMsdkAnalyzerEvent<TRACER_GUID>::Instance().Write(1, refData.level, pre_str);
    }
}
#include <KtmW32.h>
#pragma comment(lib, "KtmW32.lib")

void GlobalConfiguration::CreateMetroSpecificObjects(void)
{
     //Create the shared memory segment and the two events. If we can't, exit.z, if can register those objects in registry
    char sMetroKeyPath[255] = {};
    HKEY key;
    mfxU32 CurProcId = GetCurrentProcessId();

    RegCreateKey(HKEY_CURRENT_USER, "Software\\Intel\\MediaSDK\\Debug\\Analyzer\\MetroApplications", &key);
    sprintf_s(sMetroKeyPath,"%s%d","software\\intel\\mediasdk\\debug\\analyzer\\MetroApplications\\",CurProcId);

    //creating that events assume that OutputDebugString will use them
    MetroHandles.BufferReadyEvent.hdl = CreateEvent(NULL, false, false, TEXT("DBWIN_BUFFER_READY1"));
    MetroHandles.DataReadyEvent.hdl = CreateEvent(NULL, false, false, TEXT("DBWIN_DATA_READY1"));
    MetroHandles.BufferMapping.hdl = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4*1024, TEXT("DBWIN_BUFFER1"));
    m_sharedBuffer =  (DebugBuffer*)MapViewOfFile(MetroHandles.BufferMapping.hdl,FILE_MAP_ALL_ACCESS,0,0,4*1024);
    

    HANDLE createProcessKeyTransact = CreateTransaction(NULL, 0, TRANSACTION_DO_NOT_PROMOTE, 0, 0, 0, NULL);
    RegCreateKeyTransacted(HKEY_CURRENT_USER
        , sMetroKeyPath
        , NULL
        , NULL
        , REG_OPTION_NON_VOLATILE
        , KEY_ALL_ACCESS 
        , NULL
        , &key
        , NULL
        , createProcessKeyTransact
        , NULL);
    RegSetValueEx(key, TEXT("SHMemHDL"), 0, REG_QWORD, (BYTE*)&MetroHandles.BufferMapping, sizeof(MetroHandles.BufferMapping));
    RegSetValueEx(key, TEXT("EVTBufReady"), 0, REG_QWORD, (BYTE*)&MetroHandles.BufferReadyEvent, sizeof(MetroHandles.BufferReadyEvent));
    RegSetValueEx(key, TEXT("EVTDataReady"), 0, REG_QWORD, (BYTE*)&MetroHandles.DataReadyEvent, sizeof(MetroHandles.DataReadyEvent));
    CommitTransaction(createProcessKeyTransact);
    CloseHandle(createProcessKeyTransact);

    SetEvent((HANDLE)MetroHandles.DataReadyEvent.hdl);
    if (WaitForSingleObject((HANDLE)MetroHandles.BufferReadyEvent.hdl, 5000) == WAIT_OBJECT_0)
        run_without_gui = true;
}

bool GlobalConfiguration::IsMetroProcess()
{
    typedef BOOL (WINAPI* IsImmersiveProcessFunc)(HANDLE process);
    IsImmersiveProcessFunc IsImmersiveProcess;
    HMODULE user32Handle = GetModuleHandle(TEXT("user32.dll"));
    if (!user32Handle)
        return false;
    IsImmersiveProcess = (IsImmersiveProcessFunc)GetProcAddress(user32Handle, "IsImmersiveProcess");
    if (!IsImmersiveProcess)
        return false;
    if (!IsImmersiveProcess(GetCurrentProcess()))
        return false;
    return true;
};
