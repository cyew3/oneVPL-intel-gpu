/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.

File Name: exports.cpp

\* ****************************************************************************** */

#include "configuration.h"
#include "etl_processor.h"
#include <string>
#include <sstream>
#include <vector>
#include <mfx_dxva2_device.h>
#include <etw_event.h>
#include "shared_mem.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")



#define SDK_ANALYZER_EXPORT(type) extern "C" __declspec(dllexport) type  __stdcall 

extern int main(int argc,  char **, bool bUsePrefix);

SDK_ANALYZER_EXPORT(void) msdk_analyzer_dll()
{
    //flag function to distinguish from native mediasdk DLL
    //could print a version info here
}

SDK_ANALYZER_EXPORT(void) convert_etl_to_text(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(hinst);
    UNREFERENCED_PARAMETER(nCmdShow);

    //find text delimiter not in quotas
    bool bEscape = false;

    std::basic_stringstream<TCHAR> current_stream;
    std::vector<std::basic_string<TCHAR>> args;
    
    for(int i = 0; lpszCmdLine[i]!=0;i++)
    {
        TCHAR &c = lpszCmdLine[i];

        if (c =='"')
        {
            bEscape = !bEscape;
            continue;
        }

        if (c == ' ' && !bEscape)
        {
            args.push_back(current_stream.str());
            current_stream.str(_T(""));
            continue;
        }

        current_stream<<c;
    }

    args.push_back(current_stream.str());

    if (args.size() < 2)
        return;

    ETLProcessor proc(args[0].c_str());
    ETLFileWriter writer(args[1].c_str());
    proc.push_back(&writer);
    proc.Start();
}


typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;

    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.

    fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

    if(NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            //handle error
        }
    }
    return bIsWow64;
}

struct KeyData 
{
    bool   bCreateNew;
    TCHAR  valueName[32];
    DWORD  dwType;
    DWORD  value;
    TCHAR* pStr;//not conforming C99 standard limitation, union can't be used for static initialization
};

void SetValues(HKEY key, KeyData *values, int nValues)
{
    for (int nKey = 0; nKey < nValues; nKey++)
    {
        BYTE *pValue = (BYTE *)&values[nKey].value;
        int nSize = 0;
        if (values[nKey].dwType == REG_SZ)
        {
            pValue = (BYTE *)values[nKey].pStr;
            nSize = sizeof(TCHAR)*(DWORD)(1+_tcslen(values[nKey].pStr));
        }
        else if (values[nKey].dwType == REG_DWORD)
        {
            nSize = sizeof(DWORD);
        }

        bool bKeyMissed = true;

        if (!values[nKey].bCreateNew)
        {
            bKeyMissed = (ERROR_SUCCESS != RegQueryValueEx(key, values[nKey].valueName, 0, NULL, NULL, NULL ));
        }

        if (bKeyMissed)
        {
            RegSetValueEx(key, 
                values[nKey].valueName,
                0,
                values[nKey].dwType,
                pValue,
                nSize);
        }
    }
}

//TODO: need to import keys somehow
void CreateDefaultKeys(TCHAR *appdata)
{
    HKEY key;
    if (ERROR_SUCCESS!=RegCreateKeyEx(HKEY_CURRENT_USER
        ,_T("Software\\Intel\\MediaSDK\\Debug\\Analyzer")
        ,0
        ,NULL
        ,REG_OPTION_NON_VOLATILE
        ,KEY_QUERY_VALUE | KEY_SET_VALUE
        ,NULL
        ,&key
        ,NULL))  
    {
        return ;
    }

    KeyData keysToAdd[] = 
    {
        {true,  _T("SDK DLL"),     REG_SZ,     0, _T("") },
        {false, _T("SDK Version"), REG_SZ,     0, _T("1.1") },
        {false, _T("Log File"),    REG_SZ,     0, appdata },
        {true,  _T("Async Dump"),  REG_DWORD,  2, NULL },//etw dump
        {true,  _T("Record Configuration"),  REG_DWORD,  1, NULL },
        {true,  _T("Record Configuration Per Frame"),  REG_DWORD,  1, NULL },
    };

    SetValues(key, keysToAdd, sizeof(keysToAdd)/sizeof(keysToAdd[0]));

    RegCloseKey(key);
}

SDK_ANALYZER_EXPORT(UINT) uninstall();
SDK_ANALYZER_EXPORT(UINT) install(TCHAR *installDir,
                                  TCHAR *appdata) 
{
    //previous versions may be not correctly uninstalled and dispatch key still contains subkeys
    uninstall();
    CreateDefaultKeys(appdata);
    //we can intercept dispatcher even on first iteration with default deviceID
    //however if library doesn't support this adapter we may need to intercept all other
    //adapters

    TCHAR msdk_analyzers_keys[][32] = {_T("tracer32"),
        _T("tracer64")};

#ifdef _DEBUG
    TCHAR msdk_analyzers_dlls[][32] = {_T("\\tracer_core32_d.dll"),
        _T("\\tracer_core64_d.dll")};
#else
    TCHAR msdk_analyzers_dlls[][32] = {_T("\\tracer_core32.dll"),
        _T("\\tracer_core64.dll")};

#endif

    struct 
    {
        DWORD dw; 
        TCHAR str[10];
    }  msdk_analyzer_versions[] = {0x101, _T("v1.1"), 0x103, _T("v1.2"), 0x103, _T("v1.3"), 0x104, _T("v1.4"), 0x106, _T("v1.6"), 0x107, _T("v1.7"), 0x108, _T("v1.8"), 0x109, _T("v1.9"), 0x010a, _T("v1.10"), 0x010b, _T("v1.11")};


    int nPlatforms = IsWow64() ? 2 : 1;
    //int nAdapters  = 

    HKEY key;
    if (ERROR_SUCCESS!=RegCreateKeyEx(HKEY_CURRENT_USER
        ,_T("Software\\Intel\\MediaSDK\\Dispatch\\")
        ,0
        ,NULL
        ,REG_OPTION_NON_VOLATILE
        ,KEY_CREATE_SUB_KEY | KEY_QUERY_VALUE | KEY_SET_VALUE
        ,NULL
        ,&key
        ,NULL))  
    {
        return 0;
    }

    MFX::DXVA2Device device;

    //1 required to add software device
    bool bSoftwareDevice = false;

    for (int i = 0; !bSoftwareDevice; i++)
    {
        if (!device.InitDXGI1(i))
        {
            bSoftwareDevice = true;
        }

        mfxU32 deviceID = bSoftwareDevice ? 0 : device.GetDeviceID();
        mfxU32 vendorID = bSoftwareDevice ? 0 : device.GetVendorID();

        for (int nVersion  = 0; nVersion < sizeof(msdk_analyzer_versions) / sizeof(msdk_analyzer_versions[0]); nVersion++)
        {

            for (int nPlatform = 0; nPlatform < nPlatforms; nPlatform++)
            {

                TCHAR reg_path[32];
                TCHAR adapter_number[6];

                _stprintf_s(adapter_number, 
                    sizeof(adapter_number) / sizeof(adapter_number[0]), 
                    _T("hw%d"), i);

                _stprintf_s(reg_path, 
                    sizeof(reg_path) / sizeof(reg_path[0]),
                    _T("%s_%s_%s"), msdk_analyzers_keys[nPlatform], adapter_number, msdk_analyzer_versions[nVersion].str);


                HKEY key2;

                if (ERROR_SUCCESS != RegOpenKey(key, reg_path, &key2))
                {
                    //we need to create keys 
                    DWORD dwDisposistion;
                    if (ERROR_SUCCESS != RegCreateKeyEx(key, reg_path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &key2, &dwDisposistion))
                    {
                        RegCloseKey(key);
                        return 0;
                    }

                    //TODO: improve this key generation

                    TCHAR sdk_analyzer_path[1024];
                    _stprintf_s(sdk_analyzer_path, sizeof(sdk_analyzer_path) / sizeof(sdk_analyzer_path[0]),
                        _T("%s%s"),installDir, msdk_analyzers_dlls[nPlatform]);

                    KeyData keysToAdd[] = 
                    {
                        {true, _T("APIVersion"), REG_DWORD, msdk_analyzer_versions[nVersion].dw,0},
                        {true, _T("Merit"), REG_DWORD, 0x1, 0},
                        {true, _T("Path"), REG_SZ, 0,sdk_analyzer_path},
                    };

                   SetValues(key2, keysToAdd, sizeof(keysToAdd)/sizeof(keysToAdd[0]));
                }

                RegSetValueEx(key2, _T("DeviceId"),0,REG_DWORD,(BYTE*)&deviceID,sizeof(deviceID));
                RegSetValueEx(key2, _T("VendorId"),0,REG_DWORD,(BYTE*)&vendorID,sizeof(vendorID));

                RegCloseKey(key2);
            }//nplatform
        }//nversions
    }

    RegCloseKey(key);
    run_shared_memory_server();

    return 1;
}

SDK_ANALYZER_EXPORT(UINT) uninstall()
{
    HKEY key;
    if (ERROR_SUCCESS!=RegOpenKey(HKEY_CURRENT_USER,_T("Software\\Intel\\MediaSDK\\"),&key)) 
    {
        return 0;
    }

    SHDeleteKey(key,_T("Dispatch"));
    RegCloseKey(key);

    stop_shared_memory_server();

    return 1;
}


