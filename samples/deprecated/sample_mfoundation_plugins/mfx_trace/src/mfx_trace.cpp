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

#include "mfx_trace.h"
#include "mfx_printf.h"
#include "mfx_stat.h"
#include "mfx_etw.h"

/*------------------------------------------------------------------------------*/

typedef UINT32 (*MFXTrace_InitFn)(const TCHAR *filename, UINT32 output_mode);

typedef UINT32 (*MFXTrace_SetLevelFn)(TCHAR* category,
                                      mfxTraceLevel level);

typedef UINT32 (*MFXTrace_DebugMessageFn)(mfxTraceStaticHandle *static_handle,
                                          const char *file_name, UINT32 line_num,
                                          const char *function_name,
                                          TCHAR* category, mfxTraceLevel level,
                                          const char *message, const char *format, ...);

typedef UINT32 (*MFXTrace_vDebugMessageFn)(mfxTraceStaticHandle *static_handle,
                                           const char *file_name, UINT32 line_num,
                                           const char *function_name,
                                           TCHAR* category, mfxTraceLevel level,
                                           const char *message,
                                           const char *format, va_list args);

typedef UINT32 (*MFXTrace_BeginTaskFn)(mfxTraceStaticHandle *static_handle,
                                       const char *file_name, UINT32 line_num,
                                       const char *function_name,
                                       TCHAR* category, mfxTraceLevel level,
                                       const char *task_name, mfxTaskHandle *task_handle);

typedef UINT32 (*MFXTrace_EndTaskFn)(mfxTraceStaticHandle *static_handle,
                                     mfxTaskHandle *task_handle);

typedef UINT32 (*MFXTrace_CloseFn)(void);

struct mfxTraceAlgorithm
{
    UINT32                   m_OutputMode;
    MFXTrace_InitFn          m_InitFn;
    MFXTrace_SetLevelFn      m_SetLevelFn;
    MFXTrace_DebugMessageFn  m_DebugMessageFn;
    MFXTrace_vDebugMessageFn m_vDebugMessageFn;
    MFXTrace_BeginTaskFn     m_BeginTaskFn;
    MFXTrace_EndTaskFn       m_EndTaskFn;
    MFXTrace_CloseFn         m_CloseFn;
};

/*------------------------------------------------------------------------------*/

TCHAR* g_mfxTraceDefCategory = _T("def_category");
UINT32 g_OutputMode = MFX_TRACE_OMODE_TRASH;
UINT32 g_Level      = MFX_TRACE_LEVEL_DEFAULT;
mfxTraceAlgorithm g_TraceAlgorithms[] =
{
    {
        MFX_TRACE_OMODE_FILE,
        MFXTracePrintf_Init,
        MFXTracePrintf_SetLevel,
        MFXTracePrintf_DebugMessage,
        MFXTracePrintf_vDebugMessage,
        MFXTracePrintf_BeginTask,
        MFXTracePrintf_EndTask,
        MFXTracePrintf_Close
    },
    {
        MFX_TRACE_OMODE_STAT,
        MFXTraceStat_Init,
        MFXTraceStat_SetLevel,
        MFXTraceStat_DebugMessage,
        MFXTraceStat_vDebugMessage,
        MFXTraceStat_BeginTask,
        MFXTraceStat_EndTask,
        MFXTraceStat_Close
    },
    {
        MFX_TRACE_OMODE_ETW,
        MFXTraceETW_Init,
        MFXTraceETW_SetLevel,
        MFXTraceETW_DebugMessage,
        MFXTraceETW_vDebugMessage,
        MFXTraceETW_BeginTask,
        MFXTraceETW_EndTask,
        MFXTraceETW_Close
    }
};
UINT32 g_mfxTraceCategoriesNum = 0;
mfxTraceCategoryItem* g_mfxTraceCategoriesTable = NULL;
mfxTraceTick g_mfxTraceFrequency = mfx_trace_get_frequency();

/*------------------------------------------------------------------------------*/

mfxTraceTick mfx_trace_get_frequency(void)
{
    LARGE_INTEGER t;

    QueryPerformanceFrequency(&t);
    return t.QuadPart;
}

/*------------------------------------------------------------------------------*/

mfxTraceTick mfx_trace_get_tick(void)
{
    LARGE_INTEGER t;

    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

/*------------------------------------------------------------------------------*/

char* mfx_trace_vsprintf(char* str, size_t& str_size, const char* format, va_list args)
{
    char* p_str = str;

    if (str_size > 0)
    {
        vsnprintf_s(str, str_size, _TRUNCATE, format, args);
        p_str += _vscprintf(format, args);
        str_size -= p_str-str;
    }
    return p_str;
}

/*------------------------------------------------------------------------------*/

char* mfx_trace_sprintf(char* str, size_t& str_size, const char* format, ...)
{
    va_list args;

    va_start(args, format);
    str = mfx_trace_vsprintf(str, str_size, format, args);
    va_end(args);
    return str;
}

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

HRESULT mfx_get_reg_string(HKEY BaseKey,
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
    if (SUCCEEDED(hr) && (REG_SZ != type)) hr = E_FAIL;
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mfx_get_reg_mstring(HKEY BaseKey,
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

/*------------------------------------------------------------------------------*/

UINT32 mfx_trace_get_category_index(TCHAR* category, UINT32& index)
{
    if (category && g_mfxTraceCategoriesTable)
    {
        UINT32 i = 0;
        for (i = 0; i < g_mfxTraceCategoriesNum; ++i)
        {
            if (!_tcscmp(g_mfxTraceCategoriesTable[i].m_name, category))
            {
                index = i;
                return 0;
            }
        }
    }
    return 1;
}

/*------------------------------------------------------------------------------*/

inline bool MFXTrace_IsPrintableCategoryAndLevel(TCHAR* category, UINT32 level)
{
    UINT32 index = 0;

    if (!mfx_trace_get_category_index(category, index))
    {
        if (level <= g_mfxTraceCategoriesTable[index].m_level) return true;
    }
    else if (!g_mfxTraceCategoriesTable)
    {
        if (level <= g_Level) return true;
    }
    return false;
}

/*------------------------------------------------------------------------------*/

#define CATEGORIES_BUFFER_SIZE 1024

UINT32 MFXTrace_GetRegistryParams(void)
{
    UINT32 sts = 0;
    UINT32 value = 0;
    TCHAR categories[CATEGORIES_BUFFER_SIZE] = {0}, *p = categories;

    if (SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                          _T(MFX_TRACE_REG_PARAMS_PATH),
                                          _T(MFX_TRACE_REG_OMODE_TYPE),
                                          &value)))
    {
        g_OutputMode = value;
    }
    if (SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                          _T(MFX_TRACE_REG_PARAMS_PATH),
                                          _T(MFX_TRACE_REG_LEVEL),
                                          &value)))
    {
        g_Level = value;
    }
    if (SUCCEEDED(mfx_get_reg_mstring(MFX_TRACE_REG_ROOT,
                                      _T(MFX_TRACE_REG_PARAMS_PATH),
                                      _T(MFX_TRACE_REG_CATEGORIES),
                                      categories, sizeof(categories))))
    {
        mfxTraceCategoryItem* pCategoriesTable = NULL;

        while (*p)
        {
            pCategoriesTable = (mfxTraceCategoryItem*)realloc(g_mfxTraceCategoriesTable, (g_mfxTraceCategoriesNum+1)*sizeof(mfxTraceCategoryItem));
            if (NULL != pCategoriesTable)
            {
                g_mfxTraceCategoriesTable = pCategoriesTable;
                // storing category name in the table
                swprintf_s(g_mfxTraceCategoriesTable[g_mfxTraceCategoriesNum].m_name, sizeof(g_mfxTraceCategoriesTable[g_mfxTraceCategoriesNum].m_name)/sizeof(TCHAR), _T("%s"), p);
                // setting default ("global") level
                g_mfxTraceCategoriesTable[g_mfxTraceCategoriesNum].m_level = g_Level;
                // trying to obtain specific level for this category
                if (SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                                      _T(MFX_TRACE_REG_CATEGORIES_PATH),
                                                      g_mfxTraceCategoriesTable[g_mfxTraceCategoriesNum].m_name,
                                                      &value)))
                {
                    g_mfxTraceCategoriesTable[g_mfxTraceCategoriesNum].m_level = value;
                }
                // shifting to the next category
                ++g_mfxTraceCategoriesNum;
                p += _tcslen(p) + 1;
            }
            else break;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTrace_Init(const TCHAR *filename, UINT32 output_mode)
{
    UINT32 sts = 0;
    UINT32 i = 0;

    sts = MFXTrace_GetRegistryParams();
    if (!sts && (g_OutputMode & output_mode))
    {
        for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
        {
            if (output_mode & g_TraceAlgorithms[i].m_OutputMode)
            {
                sts = g_TraceAlgorithms[i].m_InitFn(filename, output_mode);
                break;
            }
        }
        // checking search failure
        if (!sts && sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm) == i)
        {
            sts = 1;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTrace_Close(void)
{
    UINT32 sts = 0, res = 0;
    UINT32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputMode)
        {
            res = g_TraceAlgorithms[i].m_CloseFn();
            if (!sts && res) sts = res;

            g_OutputMode &= ~(g_TraceAlgorithms[i].m_OutputMode);
        }
    }
    g_Level = MFX_TRACE_LEVEL_DEFAULT;
    if (g_mfxTraceCategoriesTable)
    {
        free(g_mfxTraceCategoriesTable);
        g_mfxTraceCategoriesTable = NULL;
    }
    g_mfxTraceCategoriesNum = 0;
    return res;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTrace_SetLevel(TCHAR* category, mfxTraceLevel level)
{
    UINT32 sts = 0, res = 0;
    UINT32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputMode)
        {
            res = g_TraceAlgorithms[i].m_SetLevelFn(category, level);
            if (!sts && res) sts = res;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTrace_DebugMessage(mfxTraceStaticHandle *static_handle,
                             const char *file_name, UINT32 line_num,
                             const char *function_name,
                             TCHAR* category, mfxTraceLevel level,
                             const char *message, const char *format, ...)
{
    if (!MFXTrace_IsPrintableCategoryAndLevel(category, level)) return 0;

    UINT32 sts = 0, res = 0;
    UINT32 i = 0;
    va_list args;

    va_start(args, format);
    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputMode)
        {
            res = g_TraceAlgorithms[i].m_vDebugMessageFn(static_handle,
                                                         file_name, line_num,
                                                         function_name,
                                                         category, level,
                                                         message, format, args);
            if (!sts && res) sts = res;
        }
    }
    va_end(args);
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTrace_vDebugMessage(mfxTraceStaticHandle *static_handle,
                              const char *file_name, UINT32 line_num,
                              const char *function_name,
                              TCHAR* category, mfxTraceLevel level,
                              const char *message,
                              const char *format, va_list args)
{
    if (!MFXTrace_IsPrintableCategoryAndLevel(category, level)) return 0;

    UINT32 sts = 0, res = 0;
    UINT32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputMode)
        {
            res = g_TraceAlgorithms[i].m_vDebugMessageFn(static_handle,
                                                         file_name, line_num,
                                                         function_name,
                                                         category, level,
                                                         message, format, args);
            if (!sts && res) sts = res;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTrace_BeginTask(mfxTraceStaticHandle *static_handle,
                          const char *file_name, UINT32 line_num,
                          const char *function_name,
                          TCHAR* category, mfxTraceLevel level,
                          const char *task_name, mfxTaskHandle *task_handle)
{
    if (static_handle)
    {
        static_handle->category = category;
        static_handle->level    = level;
    }
    if (!MFXTrace_IsPrintableCategoryAndLevel(category, level)) return 0;

    UINT32 sts = 0, res = 0;
    UINT32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputMode)
        {
            res = g_TraceAlgorithms[i].m_BeginTaskFn(static_handle,
                                                     file_name, line_num,
                                                     function_name,
                                                     category, level,
                                                     task_name, task_handle);
            if (!sts && res) sts = res;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTrace_EndTask(mfxTraceStaticHandle *static_handle,
                        mfxTaskHandle *task_handle)
{
    TCHAR* category = NULL;
    mfxTraceLevel level = MFX_TRACE_LEVEL_DEFAULT;

    if (static_handle)
    {
        category = static_handle->category;
        level    = static_handle->level;
    }
    if (!MFXTrace_IsPrintableCategoryAndLevel(category, level)) return 0;

    UINT32 sts = 0, res = 0;
    UINT32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputMode)
        {
            res = g_TraceAlgorithms[i].m_EndTaskFn(static_handle, task_handle);
            if (!sts && res) sts = res;
        }
    }
    return sts;
}
