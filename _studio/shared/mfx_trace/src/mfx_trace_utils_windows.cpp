//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE
extern "C"
{

#include "mfx_trace_utils.h"

/*------------------------------------------------------------------------------*/

HRESULT mfx_trace_get_reg_dword(HKEY BaseKey,
                                const TCHAR *pPath, const TCHAR* pName,
                                UINT32* pValue)
{
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;
    DWORD size = sizeof(UINT32), type = 0;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(BaseKey, pPath, 0, KEY_QUERY_VALUE, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        res = RegQueryValueEx(key, pName, NULL, &type, (LPBYTE)pValue, &size);
        RegCloseKey(key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr) && (REG_DWORD != type)) hr = E_FAIL;
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mfx_trace_get_reg_qword(HKEY BaseKey,
                                const TCHAR *pPath, const TCHAR* pName,
                                UINT64* pValue)
{
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;
    DWORD size = sizeof(UINT64), type = 0;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(BaseKey, pPath, 0, KEY_QUERY_VALUE, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        res = RegQueryValueEx(key, pName, NULL, &type, (LPBYTE)pValue, &size);
        RegCloseKey(key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr) && (REG_QWORD != type)) hr = E_FAIL;
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mfx_trace_get_reg_string(HKEY BaseKey,
                                 const TCHAR *pPath, const TCHAR* pName,
                                 TCHAR* pValue, UINT32 cValueMaxSize)
{
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;
    DWORD size = cValueMaxSize*sizeof(TCHAR);
    DWORD type = 0;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(BaseKey, pPath, 0, KEY_QUERY_VALUE, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        res = RegQueryValueEx(key, pName, NULL, &type, (LPBYTE)pValue, &size);
        RegCloseKey(key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr) && (REG_SZ != type)) hr = E_FAIL;
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mfx_trace_get_reg_mstring(HKEY BaseKey,
                                  const TCHAR *pPath, const TCHAR* pName,
                                  TCHAR* pValue, UINT32 cValueMaxSize)
{
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;
    DWORD size = cValueMaxSize*sizeof(TCHAR), type = 0;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(BaseKey, pPath, 0, KEY_QUERY_VALUE, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        res = RegQueryValueEx(key, pName, NULL, &type, (LPBYTE)pValue, &size);
        RegCloseKey(key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr) && (REG_MULTI_SZ != type)) hr = E_FAIL;
    return hr;
}

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE

#endif // #if defined(_WIN32) || defined(_WIN64)
