/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 - 2013 Intel Corporation. All Rights Reserved.

File Name: shared_mem.cpp

\* ****************************************************************************** */

#include "shared_mem.h"
#include "msdka_structures.h"
#include <list>
#include "etw_event.h"
#include <Shlwapi.h>
#include "msdka_defs.h"
#include "configuration.h"
#include "msdka_serial.h"

namespace
{
    HANDLE g_RegScannerThread = NULL;
    bool g_bQuit = false;
    CRITICAL_SECTION g_threadAcc = {};

    struct ProcessListenerInfo
    { 
        mfxU32 pid;
        HANDLE Handle;
        HANDLE DataReadyEvent;
        HANDLE BufferReadyEvent;
        DebugBuffer* SharedBuffer;
    };

    std::list<ProcessListenerInfo> g_ListenedProcIds;

    unsigned long  WINAPI shared_memory_process_handler(LPVOID pParams);

    void registry_scaner(LPVOID )
    {
        HKEY key;
        char SubKeyName[255];

        //start-up clean previously registered PIDS
        if (ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, TEXT("Software\\Intel\\MediaSDK\\Debug\\Analyzer\\"),&key)) 
        {
            return;
        }
        SHDeleteKey(key,_T("MetroApplications"));
        RegCloseKey(key);
        if (ERROR_SUCCESS != RegCreateKey(HKEY_CURRENT_USER, TEXT("Software\\Intel\\MediaSDK\\Debug\\Analyzer\\MetroApplications"), &key))
        {
            return;
        }

        for (int nRegKey = 0; !g_bQuit;)
        {
            HANDLE hMetroProcess = NULL;
            ProcessListenerInfo pinfo = {0};
            for (; !g_bQuit ; nRegKey++)
            {
                if(RegEnumKey(key, nRegKey, SubKeyName, sizeof(SubKeyName)) != ERROR_SUCCESS)
                {
                    //no clients registered
                    break;
                }
                //keys found
                if (1 != sscanf_s(SubKeyName,"%d",&pinfo.pid))
                {
                    ETWMsdkAnalyzerEvent<TRACER_GUID>::Instance().Write(1,1, "METRO: Invalid PID value");
                    continue;
                }
                bool IsCurrentPidListened = false;
                std::list<ProcessListenerInfo>::iterator it;
                for (it = g_ListenedProcIds.begin(); it != g_ListenedProcIds.end(); it++)
                {
                    if (it->pid == pinfo.pid)
                    {
                        IsCurrentPidListened = true;
                        break;
                    }
                }
                if (IsCurrentPidListened) 
                {
                    continue;
                }
                if(NULL == (hMetroProcess = OpenProcess(PROCESS_ALL_ACCESS, false, pinfo.pid)))
                {
                    ETWMsdkAnalyzerEvent<TRACER_GUID>::Instance().Write(1,1, "METRO: Cannot open process");
                    continue;
                }
                break;
            }
            if (!hMetroProcess)
            {
                Sleep(100);
                nRegKey = 0;
                continue;
            }

            DWORD dwType = REG_QWORD;
            __int64 numVal = 0;
            DWORD cbData = sizeof(numVal);
            
            HKEY hSubkey;
            RegOpenKey(key, SubKeyName, &hSubkey);
            for(;;)
            {
                HANDLE CurrentProcess = GetCurrentProcess();
                if (!CurrentProcess)
                {
                    break;
                }
                if (RegQueryValueEx(hSubkey, TEXT("EVTBufReady"), NULL, &dwType, (BYTE*)&numVal, &cbData) != ERROR_SUCCESS)
                {
                    break;
                }
                if (numVal > 0xFFFFFFFF) break;
                if (!DuplicateHandle(hMetroProcess,(HANDLE)numVal,CurrentProcess,&pinfo.BufferReadyEvent,0,false,DUPLICATE_SAME_ACCESS))
                {
                    break;
                }
                if (RegQueryValueEx(hSubkey, TEXT("EVTDataReady"), NULL, &dwType, (BYTE*)&numVal, &cbData) != ERROR_SUCCESS)
                {
                    CloseHandle(pinfo.BufferReadyEvent);
                    break;
                }
                if (numVal > 0xFFFFFFFF)
                {
                    CloseHandle(pinfo.BufferReadyEvent);
                    break;
                }
                if (!DuplicateHandle(hMetroProcess,(HANDLE)numVal,CurrentProcess,&pinfo.DataReadyEvent,0,false,DUPLICATE_SAME_ACCESS))
                {
                    CloseHandle(pinfo.BufferReadyEvent);
                    break;
                }
                if (RegQueryValueEx(hSubkey, TEXT("SHMemHDL"), NULL, &dwType, (BYTE*)&numVal, &cbData)!= ERROR_SUCCESS)
                {
                    CloseHandle(pinfo.BufferReadyEvent);
                    CloseHandle(pinfo.DataReadyEvent);
                    break;
                }
                if (numVal > 0xFFFFFFFF)
                {
                    CloseHandle(pinfo.BufferReadyEvent);
                    CloseHandle(pinfo.DataReadyEvent);
                    break;
                }
                HANDLE BufferMapping;
                if (!DuplicateHandle(hMetroProcess,(HANDLE)numVal,CurrentProcess,&BufferMapping,0,false,DUPLICATE_SAME_ACCESS))
                {
                    CloseHandle(pinfo.BufferReadyEvent);
                    CloseHandle(pinfo.DataReadyEvent);
                    break;
                }
            
                pinfo.SharedBuffer =  (DebugBuffer*)MapViewOfFile(BufferMapping,FILE_MAP_ALL_ACCESS,0,0,4*1024);

                EnterCriticalSection(&g_threadAcc);
                g_ListenedProcIds.push_back(pinfo);
                g_ListenedProcIds.back().Handle = CreateThread(NULL, 0, shared_memory_process_handler, &g_ListenedProcIds.back(), 0, NULL);
            
                if (!g_ListenedProcIds.back().Handle)
                {
                    RegDeleteKey(key,SubKeyName);
                    g_ListenedProcIds.pop_back();
                }
                LeaveCriticalSection(&g_threadAcc);
                CloseHandle(BufferMapping);
                break;
            }
            CloseHandle(hMetroProcess);
            RegCloseKey(hSubkey);
        }
    }


    unsigned long  WINAPI shared_memory_process_handler(LPVOID pParams)
    {
        ProcessListenerInfo & env = *(reinterpret_cast<ProcessListenerInfo*>(pParams));

        for(;!g_bQuit;)
        {
            if (WaitForSingleObject(env.DataReadyEvent, 1000) == WAIT_OBJECT_0)
            {                
                ResetEvent(env.DataReadyEvent);                
                ETWMsdkAnalyzerEvent<TRACER_GUID>::Instance().Write(1,env.SharedBuffer->dwLevel, env.SharedBuffer->szString);
                SetEvent(env.BufferReadyEvent);
            }
        }
        return 1;
    }

}

//////////////////////////////////////////////////////////////////////////
//exports

void run_shared_memory_server()
{
     g_bQuit = false;
     if (!g_RegScannerThread)
     {
        InitializeCriticalSection(&g_threadAcc);
        g_RegScannerThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)registry_scaner,NULL,0,NULL);
        if (!g_RegScannerThread)
        {
            DeleteCriticalSection(&g_threadAcc);
        }
     }
}

void stop_shared_memory_server()
{
    g_bQuit = true;
    if (g_RegScannerThread)
    {
        EnterCriticalSection(&g_threadAcc);
        WaitForSingleObject(g_RegScannerThread, 10 * 1000);
        std::list<ProcessListenerInfo>::iterator it;
        for (it = g_ListenedProcIds.begin(); it != g_ListenedProcIds.end(); it++)
        {
            UnmapViewOfFile(it->SharedBuffer);
            WaitForSingleObject(it->Handle, 10 * 1000);
            CloseHandle(it->Handle);
        }
        CloseHandle(g_RegScannerThread);
        g_RegScannerThread = NULL;
        LeaveCriticalSection(&g_threadAcc);
        DeleteCriticalSection(&g_threadAcc);
    }
}
