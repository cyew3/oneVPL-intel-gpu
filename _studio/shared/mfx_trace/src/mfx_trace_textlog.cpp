/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_trace_printf.cpp

Mapping of mfxTraceStaticHandle:
    fd1 - char* - file name
    fd2 - UINT32 - line number
    fd3 - char* - function name
    fd4 - char* - task name

Mapping of mfxTraceTaskHandle:
    n/a

*********************************************************************************/

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE_TEXTLOG
extern "C"
{

#if defined(_WIN32) || defined(_WIN64)
#define MFT_TRACE_PATH_TO_TEMP_LIBLOG MFX_TRACE_STRING("C:\\temp\\mfxlib.log")
#else
#define MFT_TRACE_PATH_TO_TEMP_LIBLOG MFX_TRACE_STRING("/tmp/mfxlib.log")
#endif

#include <stdio.h>
#include "mfx_trace_utils.h"
#include "mfx_trace_textlog.h"

/*------------------------------------------------------------------------------*/

mfxTraceU32 g_PrintfSuppress = MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME |
                          MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM |
                          MFX_TRACE_TEXTLOG_SUPPRESS_LEVEL;
FILE* g_mfxTracePrintfFile = NULL;
mfxTraceChar g_mfxTracePrintfFileName[MAX_PATH] = MFT_TRACE_PATH_TO_TEMP_LIBLOG;

/*------------------------------------------------------------------------------*/

#if defined(_WIN32) || defined(_WIN64)
mfxTraceU32 MFXTraceTextLog_GetRegistryParams(void)
{
    UINT32 sts = 0;
    HRESULT hr = S_OK;
    UINT32 value = 0;

    if (SUCCEEDED(hr))
    {
        mfx_trace_get_reg_string(MFX_TRACE_REG_ROOT,
                                 MFX_TRACE_REG_PARAMS_PATH,
                                 MFX_TRACE_TEXTLOG_REG_FILE_NAME,
                                 g_mfxTracePrintfFileName,
                                 sizeof(g_mfxTracePrintfFileName));
    }
    if (SUCCEEDED(hr) && SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                                           MFX_TRACE_REG_PARAMS_PATH,
                                                           MFX_TRACE_TEXTLOG_REG_SUPPRESS,
                                                           &value)))
    {
        g_PrintfSuppress = value;
    }
    if (SUCCEEDED(hr) && SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                                           MFX_TRACE_REG_PARAMS_PATH,
                                                           MFX_TRACE_TEXTLOG_REG_PERMIT,
                                                           &value)))
    {
        g_PrintfSuppress &= ~value;
    }
    return sts;
}

#else // #if defined(_WIN32) || defined(_WIN64)

mfxTraceU32 MFXTraceTextLog_GetRegistryParams(void)
{
    FILE* conf_file = mfx_trace_open_conf_file(MFX_TRACE_CONFIG);
    mfxTraceU32 value = 0;

    if (!conf_file) return 1;
    mfx_trace_get_conf_string(conf_file,
                              MFX_TRACE_TEXTLOG_REG_FILE_NAME,
                              g_mfxTracePrintfFileName,
                              sizeof(g_mfxTracePrintfFileName));

    if (!mfx_trace_get_conf_dword(conf_file,
                                  MFX_TRACE_TEXTLOG_REG_SUPPRESS,
                                  &value))
    {
        g_PrintfSuppress = value;
    }
    if (!mfx_trace_get_conf_dword(conf_file,
                                  MFX_TRACE_TEXTLOG_REG_PERMIT,
                                  &value))
    {
        g_PrintfSuppress &= ~value;
    }
    fclose(conf_file);
    return 0;
}

#endif // #if defined(_WIN32) || defined(_WIN64)

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_Init(const mfxTraceChar *filename, mfxTraceU32 output_mode)
{
    mfxTraceU32 sts = 0;

    if (!(output_mode & MFX_TRACE_OUTPUT_TEXTLOG)) return 1;


    sts = MFXTraceTextLog_Close();
    if (!sts) sts = MFXTraceTextLog_GetRegistryParams();
    if (!sts)
    {
        if (!g_mfxTracePrintfFile)
        {
            if (!filename) filename = g_mfxTracePrintfFileName;
            if (!mfx_trace_tcmp(filename, MFX_TRACE_STRING("stdout"))) g_mfxTracePrintfFile = stdout;
            else g_mfxTracePrintfFile = mfx_trace_tfopen(filename, MFX_TRACE_STRING("a"));
        }
        if (!g_mfxTracePrintfFile) return 1;
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_Close(void)
{
    g_PrintfSuppress = //MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME |
                       //MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM |
                       MFX_TRACE_TEXTLOG_SUPPRESS_LEVEL;
    if (g_mfxTracePrintfFile)
    {
        fclose(g_mfxTracePrintfFile);
        g_mfxTracePrintfFile = NULL;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_SetLevel(mfxTraceChar* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_DebugMessage(mfxTraceStaticHandle* static_handle,
                                   const char *file_name, mfxTraceU32 line_num,
                                   const char *function_name,
                                   mfxTraceChar* category, mfxTraceLevel level,
                                   const char *message, const char *format, ...)
{
    mfxTraceU32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceTextLog_vDebugMessage(static_handle,
                                       file_name , line_num,
                                       function_name,
                                       category, level,
                                       message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_vDebugMessage(mfxTraceStaticHandle* /*static_handle*/,
                                    const char *file_name, mfxTraceU32 line_num,
                                    const char *function_name,
                                    mfxTraceChar* category, mfxTraceLevel level,
                                    const char *message,
                                    const char *format, va_list args)
{
    if (!g_mfxTracePrintfFile) return 1;

    size_t len = MFX_TRACE_MAX_LINE_LENGTH;
    char str[MFX_TRACE_MAX_LINE_LENGTH] = {0}, *p_str = str;

    if (file_name && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%-60s: ", file_name);
    }
    if (line_num && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%4d: ", line_num);
    }
    if (category && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_CATEGORY))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%S: ", category);
    }
    if (!(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_LEVEL))
    {
        p_str = mfx_trace_sprintf(p_str, len, "LEV_%d: ", level);
    }
    if (function_name && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_FUNCTION_NAME))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%-40s: ", function_name);
    }
    if (message)
    {
        p_str = mfx_trace_sprintf(p_str, len, "%s", message);
    }
    if (format)
    {
        p_str = mfx_trace_vsprintf(p_str, len, format, args);
    }
    {
        p_str = mfx_trace_sprintf(p_str, len, "\n");
    }
    fprintf(g_mfxTracePrintfFile, "%s", str);
    fflush(g_mfxTracePrintfFile);
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_BeginTask(mfxTraceStaticHandle *static_handle,
                                const char *file_name, mfxTraceU32 line_num,
                                const char *function_name,
                                mfxTraceChar* category, mfxTraceLevel level,
                                const char *task_name, mfxTraceTaskHandle* handle,
                                const void * /*task_params*/)
{
    if (handle)
    {
        handle->fd1.str    = (char*)file_name;
        handle->fd2.uint32 = line_num;
        handle->fd3.str    = (char*)function_name;
        handle->fd4.str    = (char*)task_name;
    }
    return MFXTraceTextLog_DebugMessage(static_handle,
                                       file_name, line_num,
                                       function_name,
                                       category, level,
                                       task_name, (task_name)? ": ENTER": "ENTER");
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_EndTask(mfxTraceStaticHandle *static_handle,
                              mfxTraceTaskHandle *handle)
{
    if (!handle) return 1;

    char *file_name = NULL, *function_name = NULL, *task_name = NULL;
    mfxTraceU32 line_num = 0;
    mfxTraceChar* category = NULL;
    mfxTraceLevel level = MFX_TRACE_LEVEL_DEFAULT;

    if (handle)
    {
        category      = static_handle->category;
        level         = static_handle->level;

        file_name     = handle->fd1.str;
        line_num      = handle->fd2.uint32;
        function_name = handle->fd3.str;
        task_name     = handle->fd4.str;
    }
    return MFXTraceTextLog_DebugMessage(static_handle,
                                       file_name, line_num,
                                       function_name,
                                       category, level,
                                       task_name, (task_name)? ": EXIT": "EXIT");
}

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE_TEXTLOG
