/* ****************************************************************************** *\

Copyright (C) 2020 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

\* ****************************************************************************** */
#if defined(_WIN32) || defined(_WIN64)

#include <string>
#include <sstream>
#include <vector>
#include <tchar.h>
#include "../loggers/log.h"

#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

HANDLE g_RegScannerThread = NULL;
bool g_bQuit = false;
CRITICAL_SECTION g_threadAcc = {};

void stop_shared_memory_server()
{
    g_bQuit = true;
    if (g_RegScannerThread)
    {
        //EnterCriticalSection(&g_threadAcc);
        //WaitForSingleObject(g_RegScannerThread, 100);
        TerminateThread(g_RegScannerThread, 0);
        CloseHandle(g_RegScannerThread);
        g_RegScannerThread = NULL;
        //LeaveCriticalSection(&g_threadAcc);
        //DeleteCriticalSection(&g_threadAcc);
    }
}

void registry_scaner(LPVOID)
{
    DWORD start_flag = 0;
    DWORD size = sizeof(DWORD);
    HKEY key;
    for (;;)
    {
        
        RegGetValue(HKEY_LOCAL_MACHINE, ("Software\\Intel\\MediaSDK\\Dispatch\\tracer"), ("_start"), REG_DWORD, 0, (BYTE*)&start_flag, &size); 
        if ((start_flag == 0) && (Log::useGUI))
        {
            Log::clear();
        }
        if (ERROR_SUCCESS != RegOpenKey(HKEY_LOCAL_MACHINE,("Software\\Intel\\MediaSDK\\Dispatch\\tracer"),&key))
        {
            stop_shared_memory_server();
        }
        Sleep(100);
    }
}

void run_shared_memory_server()
{
     g_bQuit = false;
     if (!g_RegScannerThread)
     {
        //InitializeCriticalSection(&g_threadAcc);
        g_RegScannerThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)registry_scaner,NULL,0,NULL);
        if (!g_RegScannerThread)
        {
            //DeleteCriticalSection(&g_threadAcc);
        }
     }
}
#endif
