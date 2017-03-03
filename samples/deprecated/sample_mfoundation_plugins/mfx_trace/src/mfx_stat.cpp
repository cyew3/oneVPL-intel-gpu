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

/*********************************************************************************

File: mfx_stat.cpp

Mapping of mfxTraceStaticHandle:
    sd1 - char* - file name
    sd2 - UINT32 - line number
    sd3 - char* - function name
    sd4 - char* - task name
    sd5 - UINT32 - call number
    sd6 - mfxTraceTick - for average value calculation
    sd7 - mfxTraceTick - for standard deviation calculation

Mapping of mfxTaskHandle:
    sd1 - mfxTraceTick - task start tick

*********************************************************************************/

#include <stdio.h>
#include <math.h>

#include "mfx_stat.h"

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_PrintHeader(void);
UINT32 MFXTraceStat_PrintInfo(mfxTraceStaticHandle* static_handle);

#define FORMAT_FN_NAME    "%-40s: "
#define FORMAT_TASK_NAME  "%-20s: "
#define FORMAT_FULL_STAT  "%10.6f, %6d, %10.6f, %10.6f"
#define FORMAT_SHORT_STAT "%10.6f, %6d, %10s, %10s"
#define FORMAT_CAT        ": %S"
#define FORMAT_LEV        ": LEV_%-02d"
#define FORMAT_FILE_NAME  ": %s"
#define FORMAT_LINE_NUM   ": %d"

#define FORMAT_HDR_FN_NAME   "%-40s: "
#define FORMAT_HDR_TASK_NAME "%-20s: "
#define FORMAT_HDR_STAT      "%10s, %6s, %10s, %10s"
#define FORMAT_HDR_CAT       FORMAT_CAT
#define FORMAT_HDR_LEV       ": %-6s"
#define FORMAT_HDR_FILE_NAME FORMAT_FILE_NAME
#define FORMAT_HDR_LINE_NUM  ": %s"

/*------------------------------------------------------------------------------*/

#define MFX_TRACE_STAT_NUM_OF_STAT_ITEMS 100

class mfxStatGlobalHandle
{
public:
    mfxStatGlobalHandle(void) :
        m_StatTableIndex(0),
        m_StatTableSize(0),
        m_pStatTable(NULL)
    {
        m_pStatTable = (mfxTraceStaticHandle**)malloc(MFX_TRACE_STAT_NUM_OF_STAT_ITEMS * sizeof(mfxTraceStaticHandle*));
        if (m_pStatTable) m_StatTableSize = MFX_TRACE_STAT_NUM_OF_STAT_ITEMS;
    };

    ~mfxStatGlobalHandle(void)
    {
        Close();
        if (m_pStatTable)
        {
            free(m_pStatTable);
            m_pStatTable = NULL;
        }
    };

    UINT32 AddItem(mfxTraceStaticHandle* pStatHandle)
    {
        if (!pStatHandle) return 1;
#if 0 // is done on different level (see MFXTraceStat_EndTask)
        UINT32 i = 0;

        for (i = 0; i < m_StatTableIndex; ++i)
        {
            if (m_pStatTable[m_StatTableIndex] == pStatHandle) return;
        }
#endif
        if (m_StatTableIndex >= m_StatTableSize)
        {
            mfxTraceStaticHandle** pStatTable = NULL;

            pStatTable = (mfxTraceStaticHandle**)realloc(m_pStatTable, (m_StatTableSize + MFX_TRACE_STAT_NUM_OF_STAT_ITEMS) * sizeof(mfxTraceStaticHandle*));
            if (pStatTable)
            {
                m_pStatTable = pStatTable;
                m_StatTableSize += MFX_TRACE_STAT_NUM_OF_STAT_ITEMS;
            }
            else return 1;
        }
        if (!m_pStatTable || (m_StatTableIndex >= m_StatTableSize)) return 1;
        if (m_StatTableIndex < m_StatTableSize)
        {
            m_pStatTable[m_StatTableIndex] = pStatHandle;
            ++m_StatTableIndex;
#if MFX_STAT_TICKS
            memset( pStatHandle->ticks, 0, sizeof(pStatHandle->ticks));
#endif
        }
        return 0;
    };

    void Close(void)
    {
        UINT32 i = 0;

        if (m_StatTableIndex) MFXTraceStat_PrintHeader();
        for (i = 0; i < m_StatTableIndex; ++i)
        {
            MFXTraceStat_PrintInfo(m_pStatTable[i]);
        }
        m_StatTableIndex = 0;
    };

protected:
    UINT32 m_StatTableIndex;
    UINT32 m_StatTableSize;
    mfxTraceStaticHandle** m_pStatTable;
};

/*------------------------------------------------------------------------------*/

UINT32 g_StatSuppress = MFX_TRACE_STAT_SUPPRESS_FILE_NAME |
                        MFX_TRACE_STAT_SUPPRESS_LINE_NUM |
                        MFX_TRACE_STAT_SUPPRESS_LEVEL;
FILE* g_mfxTraceStatFile = NULL;
mfxStatGlobalHandle g_StatGlobalHandle;

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_GetRegistryParams(void)
{
    UINT32 sts = 0;
    HRESULT hr = S_OK;
    UINT32 value = 0;

    if (SUCCEEDED(hr) && SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                                           _T(MFX_TRACE_REG_PARAMS_PATH),
                                                           _T(MFX_TRACE_STAT_REG_SUPPRESS),
                                                           &value)))
    {
        g_StatSuppress = value;
    }
    if (SUCCEEDED(hr) && SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                                           _T(MFX_TRACE_REG_PARAMS_PATH),
                                                           _T(MFX_TRACE_STAT_REG_PERMIT),
                                                           &value)))
    {
        g_StatSuppress &= ~value;
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_Init(const TCHAR *filename, UINT32 output_mode)
{
    UINT32 sts = 0;
    if (!(output_mode & MFX_TRACE_OMODE_STAT)) return 1;
    if (!filename) return 1;

    sts = MFXTraceStat_Close();
    if (!sts) sts = MFXTraceStat_GetRegistryParams();
    if (!sts)
    {
        if (!g_mfxTraceStatFile)
        {
            g_mfxTraceStatFile = mfx_trace_tfopen(filename, L"a");
        }
        if (!g_mfxTraceStatFile) return 1;
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_Close(void)
{
    g_StatGlobalHandle.Close();
    if (g_mfxTraceStatFile)
    {
        fclose(g_mfxTraceStatFile);
        g_mfxTraceStatFile = NULL;
    }
    g_StatSuppress = MFX_TRACE_STAT_SUPPRESS_FILE_NAME |
                     MFX_TRACE_STAT_SUPPRESS_LINE_NUM |
                     MFX_TRACE_STAT_SUPPRESS_LEVEL;
    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_SetLevel(TCHAR* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_DebugMessage(mfxTraceStaticHandle* static_handle,
                                 const char *file_name, UINT32 line_num,
                                 const char *function_name,
                                 TCHAR* category, mfxTraceLevel level,
                                 const char *message, const char *format, ...)
{
    UINT32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceStat_vDebugMessage(static_handle,
                                     file_name , line_num,
                                     function_name,
                                     category, level,
                                     message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_vDebugMessage(mfxTraceStaticHandle* /*static_handle*/,
                                  const char* /*file_name*/, UINT32 /*line_num*/,
                                  const char* /*function_name*/,
                                  TCHAR* /*category*/, mfxTraceLevel /*level*/,
                                  const char* /*message*/,
                                  const char* /*format*/, va_list /*args*/)
{
    if (!g_mfxTraceStatFile) return 1;
    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_PrintHeader(void)
{
    if (!g_mfxTraceStatFile) return 1;

    char str[MFX_TRACE_MAX_LINE_LENGTH] = {0}, *p_str = str;
    size_t len = MFX_TRACE_MAX_LINE_LENGTH;

    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_HDR_FN_NAME, "Function name");
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_HDR_TASK_NAME, "Task name");
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_HDR_STAT, "Total time", "Number", "Avg. time", "Std. dev.");
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_CATEGORY))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_HDR_CAT, _T("Category"));
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_LEVEL))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_HDR_LEV, "Level");
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_FILE_NAME))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_HDR_FILE_NAME, "File name");
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_LINE_NUM))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_HDR_LINE_NUM, "Line number");
    }
    {
        p_str = mfx_trace_sprintf(p_str, len, "\n");
    }
    fprintf(g_mfxTraceStatFile, "%s", str);
    fflush(g_mfxTraceStatFile);

    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_PrintInfo(mfxTraceStaticHandle* static_handle)
{
    if (!g_mfxTraceStatFile) return 1;
    if (!static_handle) return 1;

    char str[MFX_TRACE_MAX_LINE_LENGTH] = {0}, *p_str = str;
    size_t len = MFX_TRACE_MAX_LINE_LENGTH;

    char *file_name = NULL, *function_name = NULL, *task_name = NULL;
    UINT32 line_num = 0;
    TCHAR* category = NULL;
    mfxTraceLevel level = MFX_TRACE_LEVEL_DEFAULT;
    bool b_avg_not_applicable = (1 == static_handle->sd5.uint32)? true: false;
    double time = 0, avg_time = 0, std_div = 0;

    // restoring info
    category      = static_handle->category;
    level         = static_handle->level;

    file_name     = static_handle->sd1.str;
    line_num      = static_handle->sd2.uint32;
    function_name = static_handle->sd3.str;
    task_name     = static_handle->sd4.str;

    // calculating statistics
    time = mfx_trace_get_time(static_handle->sd6.tick, 0, g_mfxTraceFrequency);
    if (!b_avg_not_applicable)
    {
        avg_time = time/(double)static_handle->sd5.uint32;
        // that's approximation of standard deviation:
        // should be: n/(n-1) * [ sum (xk - x_avg)^2 ]
        // approximation is: n/(n-1) * [ 1/n * sum (xk*xk) - x_avg * x_avg]
        std_div = mfx_trace_get_time(static_handle->sd7.tick, 0, g_mfxTraceFrequency*g_mfxTraceFrequency)/(double)static_handle->sd5.uint32;
        std_div -= avg_time*avg_time;
        std_div *= (double)static_handle->sd5.uint32/((double)(static_handle->sd5.uint32 - 1));
        std_div = sqrt(std_div);
    }
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_FN_NAME, function_name);
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_TASK_NAME, task_name);
    }
    if (!b_avg_not_applicable)
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_FULL_STAT,
                                  time,
                                  static_handle->sd5.uint32,
                                  avg_time, std_div);
    }
    else
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_SHORT_STAT,
                                  time,
                                  static_handle->sd5.uint32, "", "");
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_CATEGORY))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_CAT, category);
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_LEVEL))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_LEV, level);
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_FILE_NAME))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_FILE_NAME, file_name);
    }
    if (!(g_StatSuppress & MFX_TRACE_STAT_SUPPRESS_LINE_NUM))
    {
        p_str = mfx_trace_sprintf(p_str, len, FORMAT_LINE_NUM, line_num);
    }
    {
        p_str = mfx_trace_sprintf(p_str, len, "\n");
    }
    fprintf(g_mfxTraceStatFile, "%s", str);

#if MFX_STAT_TICKS
    for (UINT32 i = 0; i < static_handle->sd5.uint32 && i < MFX_STAT_TICKS; i++)
    {
        time = mfx_trace_get_time(static_handle->ticks[i], 0, g_mfxTraceFrequency);
        //unlike total time in seconds the particular tasks times are logged in ms
        fprintf(g_mfxTraceStatFile, "%6d, %10.6f\n", i, time*1000.);
    }
#endif

    fflush(g_mfxTraceStatFile);

    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_BeginTask(mfxTraceStaticHandle *static_handle,
                              const char *file_name, UINT32 line_num,
                              const char *function_name,
                              TCHAR* /*category*/, mfxTraceLevel /*level*/,
                              const char *task_name, mfxTaskHandle *task_handle)
{
    if (!static_handle || !task_handle) return 1;

    static_handle->sd1.str    = (char*)file_name;
    static_handle->sd2.uint32 = line_num;
    static_handle->sd3.str    = (char*)function_name;
    static_handle->sd4.str    = (char*)task_name;

    task_handle->sd1.tick = mfx_trace_get_tick();

    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_EndTask(mfxTraceStaticHandle *static_handle,
                            mfxTaskHandle *task_handle)
{
    if (!static_handle || !task_handle) return 1;

    mfxTraceTick time = 0;

    ++(static_handle->sd5.uint32);
    time = mfx_trace_get_tick() - task_handle->sd1.tick;
    static_handle->sd6.tick += time;
    static_handle->sd7.tick += time*time;

    UINT32 result = 0;
    if (1 == static_handle->sd5.uint32)
    {
        result = g_StatGlobalHandle.AddItem(static_handle);
    }
#if MFX_STAT_TICKS
    if (static_handle->sd5.uint32 > 0 && static_handle->sd5.uint32 < MFX_STAT_TICKS)
    {
        static_handle->ticks[static_handle->sd5.uint32 - 1] = time;
    }
#endif
    return result;
}
