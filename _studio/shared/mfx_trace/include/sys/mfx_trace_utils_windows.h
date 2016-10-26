//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TRACE_UTILS_WINDOWS_H__
#define __MFX_TRACE_UTILS_WINDOWS_H__

#if defined(_WIN32) || defined(_WIN64)

#ifdef MFX_TRACE_ENABLE

#include <windows.h>
#include <tchar.h>
#include <share.h>

/*------------------------------------------------------------------------------*/

// registry path to trace options and parameters
#define MFX_TRACE_REG_ROOT          HKEY_CURRENT_USER
#define MFX_TRACE_REG_PARAMS_PATH   _T("Software\\Intel\\MediaSDK\\Tracing")
#define MFX_TRACE_REG_CATEGORIES_PATH MFX_TRACE_REG_PARAMS_PATH _T("\\Categories")
// trace registry options and parameters
#define MFX_TRACE_REG_OMODE_TYPE    _T("Output")
#define MFX_TRACE_REG_CATEGORIES    _T("Categories")
#define MFX_TRACE_REG_LEVEL         _T("Level")

/*------------------------------------------------------------------------------*/

HRESULT mfx_trace_get_reg_dword(HKEY BaseKey,
                                const TCHAR *pPath, const TCHAR* pName,
                                UINT32* pValue);
HRESULT mfx_trace_get_reg_qword(HKEY BaseKey,
                                const TCHAR *pPath, const TCHAR* pName,
                                UINT64* pValue);
HRESULT mfx_trace_get_reg_string(HKEY BaseKey,
                                 const TCHAR *pPath, const TCHAR* pName,
                                 TCHAR* pValue, UINT32 cValueMaxSize);
HRESULT mfx_trace_get_reg_mstring(HKEY BaseKey,
                                  const TCHAR *pPath, const TCHAR* pName,
                                  TCHAR* pValue, UINT32 cValueMaxSize);

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif // #ifdef MFX_TRACE_ENABLE

#endif // #ifndef __MFX_TRACE_UTILS_WINDOWS_H__
