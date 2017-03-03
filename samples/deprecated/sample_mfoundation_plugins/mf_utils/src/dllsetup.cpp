/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mf_guids.h"

/*--------------------------------------------------------------------*/

#define MAX_KEY_LEN 260
#define NUMELMS(aa) (sizeof(aa)/sizeof((aa)[0]))
#define AmHresultFromWin32(x) (MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, x))

/*--------------------------------------------------------------------*/
// EliminateSubKey: try to enumerate all keys under this one. If we find
// anything, delete it completely. Otherwise just delete it.

STDAPI EliminateSubKey(HKEY hkey, LPCTSTR strSubKey)
{
    HKEY hk;
    TCHAR Buffer[MAX_KEY_LEN];
    DWORD dw = MAX_KEY_LEN;
    FILETIME ft;

    if (0 == lstrlen(strSubKey)) return E_FAIL;

    LSTATUS res = RegOpenKeyEx(hkey, strSubKey, 0, MAXIMUM_ALLOWED, &hk);
    assert((ERROR_SUCCESS == res) ||
           (ERROR_FILE_NOT_FOUND == res) ||
           (ERROR_INVALID_HANDLE == res));

    // Keep on enumerating the first (zero-th) key and deleting that
    while (ERROR_SUCCESS == res)
    {
        res = RegEnumKeyEx(hk, 0, Buffer, &dw, NULL, NULL, NULL, &ft);
        assert((ERROR_SUCCESS == res) || (ERROR_NO_MORE_ITEMS == res));

        if (ERROR_SUCCESS == res) EliminateSubKey(hk, Buffer);
    }
    RegCloseKey(hk);
    RegDeleteKey(hkey, strSubKey);
    return NOERROR;
}

/*--------------------------------------------------------------------*/
// AMovieSetupRegisterServer:
// registers specified file "szFileName" as server for
// CLSID "clsServer".  A description is also required.
// The ThreadingModel and ServerType are optional, as
// they default to InprocServer32 (i.e. dll) and Both.

STDAPI AMovieSetupRegisterServer(CLSID   clsServer,
                                 LPCWSTR szDescription,
                                 LPCWSTR szFileName,
                                 LPCWSTR szThreadingModel,
                                 LPCWSTR szServerType)
{
    HRESULT hr = S_OK;
    LSTATUS res = ERROR_SUCCESS;
    HKEY hkey, hsubkey;
    TCHAR achTemp[MAX_PATH];
    // convert CLSID uuid to string and write out subkey as string - CLSID\{}
    OLECHAR szCLSID[CHARS_IN_GUID];

    hr = StringFromGUID2(clsServer, szCLSID, CHARS_IN_GUID);
    assert(SUCCEEDED(hr));

    // create key
    StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("CLSID\\%ls"), szCLSID);

    res = RegCreateKey(HKEY_CLASSES_ROOT,(LPCTSTR)achTemp, &hkey);
    if (ERROR_SUCCESS != res) return AmHresultFromWin32(res);

    // set description string
    StringCchPrintf( achTemp, NUMELMS(achTemp), TEXT("%ls"), szDescription );
    res = RegSetValue(hkey, (LPCTSTR)NULL, REG_SZ, achTemp, sizeof(achTemp));
    if (ERROR_SUCCESS != res)
    {
        RegCloseKey(hkey);
        return AmHresultFromWin32(res);
    }

    // create CLSID\\{"CLSID"}\\"ServerType" key,
    // using key to CLSID\\{"CLSID"} passed back by
    // last call to RegCreateKey().
    StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("%ls"), szServerType);
    res = RegCreateKey(hkey, achTemp, &hsubkey);
    if (ERROR_SUCCESS != res)
    {
        RegCloseKey(hkey);
        return AmHresultFromWin32(res);
    }

    // set Server string
    StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("%ls"), szFileName);
    res = RegSetValue(hsubkey, (LPCTSTR)NULL, REG_SZ, (LPCTSTR)achTemp, sizeof(TCHAR) * (lstrlen(achTemp)+1));
    if (ERROR_SUCCESS != res)
    {
        RegCloseKey(hkey);
        RegCloseKey(hsubkey);
        return AmHresultFromWin32(res);
    }

    StringCchPrintf(achTemp, NUMELMS(achTemp), TEXT("%ls"), szThreadingModel);
    res = RegSetValueEx(hsubkey, TEXT("ThreadingModel"), 0L, REG_SZ, (CONST BYTE *)achTemp, sizeof(TCHAR) * (lstrlen(achTemp)+1));

    // close hkeys
    RegCloseKey(hkey);
    RegCloseKey(hsubkey);

    return HRESULT_FROM_WIN32(res);
}

/*--------------------------------------------------------------------*/
// AMovieSetupUnregisterServer:
// default ActiveMovie dll setup function - to use must be called from
// an exported function named DllRegisterServer()

STDAPI AMovieSetupUnregisterServer(CLSID clsServer)
{
    HRESULT hr = S_OK;
    // convert CLSID uuid to string and write
    // out subkey CLSID\{}
    OLECHAR szCLSID[CHARS_IN_GUID];
    TCHAR achBuffer[MAX_KEY_LEN];

    hr = StringFromGUID2(clsServer, szCLSID, CHARS_IN_GUID);
    assert(SUCCEEDED(hr));

    // delete subkey
    StringCchPrintf(achBuffer, NUMELMS(achBuffer), TEXT("CLSID\\%ls"), szCLSID);
    hr = EliminateSubKey(HKEY_CLASSES_ROOT, achBuffer);
    assert(SUCCEEDED(hr));

    return NOERROR;
}
