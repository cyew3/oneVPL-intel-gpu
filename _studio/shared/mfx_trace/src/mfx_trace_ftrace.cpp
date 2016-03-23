/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_trace_ftrace.cpp

Mapping of mfxTraceStaticHandle:
itt1 - void* - int ftrace file handle

Mapping of mfxTraceTaskHandle:
itt1 - UINT32 - task is traced

*********************************************************************************/

#include "mfx_trace.h"

#if defined(MFX_TRACE_ENABLE_FTRACE) && defined(LINUX32)


extern "C" {

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "mfx_trace_utils.h"
#include "mfx_trace_ftrace.h"


static int trace_handle = -1;
static char debugfs_prefix[MAX_PATH+1];
static char trace_file_name[MAX_PATH+1];

namespace 
{
    // not thread-safe
    const char *get_debugfs_prefix()
    {        
        FILE *fp = fopen("/proc/mounts","r");

        if (!fp)
        {
            return "noperm";
        }

        bool found = false;
        while (!found)
        {
            char type[100];
            if (fscanf(fp, "%*s %256s %99s %*s %*d %*d\n", debugfs_prefix, type) != 2)
            {
                // eof reached
                break;
            }
            if (strcmp(type, "debugfs") == 0)
            {
                // got debugfs line
                found = true;
            }
        }
        fclose(fp);

        if (!found)
        {
            return "notfound";
        }

        return debugfs_prefix;
    }

    // not thread-safe
    const char *get_tracing_file(const char *file_name)
    {
        snprintf(trace_file_name, MAX_PATH, "%s/%s", get_debugfs_prefix(), file_name);
        return trace_file_name;
    }

}
/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceFtrace_Init(const mfxTraceChar * /*filename*/, mfxTraceU32 output_mode)
{
    if (!(output_mode & MFX_TRACE_OUTPUT_FTRACE)) return 1;

    MFXTraceFtrace_Close();

    const char* file_name = get_tracing_file("tracing/trace_marker");
    trace_handle = open(file_name , O_WRONLY);
    if (trace_handle == -1){
        // printf("@@@@ Can't open trace file!\n");
        return 1;
    }

    //printf("@@@@ Trace file opened!\n");
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceFtrace_Close(void)
{
    if (trace_handle != -1)
        close(trace_handle);
    trace_handle = -1;

    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceFtrace_SetLevel(mfxTraceChar* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceFtrace_DebugMessage(mfxTraceStaticHandle* static_handle,
    const char * file_name, mfxTraceU32 line_num,
    const char * function_name,
    mfxTraceChar* category, mfxTraceLevel level,
    const char * message, const char * format, ...)
{
    mfxTraceU32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceFtrace_vDebugMessage(static_handle,
        file_name , line_num,
        function_name,
        category, level,
        message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceFtrace_vDebugMessage(mfxTraceStaticHandle* //static_handle
    , const char * //file_name
    , mfxTraceU32 //line_num
    , const char * function_name
    , mfxTraceChar* //category
    , mfxTraceLevel level
    , const char * message
    , const char * format
    , va_list args
    )
{
    if (trace_handle == -1) return 1;
    
    if (MFX_TRACE_LEVEL_INTERNAL_VTUNE != level)
        return 0;

    size_t len = MFX_TRACE_MAX_LINE_LENGTH;
    char str[MFX_TRACE_MAX_LINE_LENGTH] = {0}, *p_str = str;

    
//     if (file_name /*&& !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME)*/)
//     {
//         p_str = mfx_trace_sprintf(p_str, len, "%-60s: ", file_name);
//     }
//     if (line_num /*&& !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM)*/)
//     {
//         p_str = mfx_trace_sprintf(p_str, len, "%4d: ", line_num);
//     }
//     if (category /*&& !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_CATEGORY)*/)
//     {
//         p_str = mfx_trace_sprintf(p_str, len, "%S: ", category);
//     }
//     if (!(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_LEVEL))
//     {
//         p_str = mfx_trace_sprintf(p_str, len, "LEV_%d: ", level);
//     }

    p_str = mfx_trace_sprintf(p_str, len, "msdk_v1: ");

//     if (function_name /*&& !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_FUNCTION_NAME)*/)
//     {
//         p_str = mfx_trace_sprintf(p_str, len, "%-40s: ", function_name);
//     }
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

    int ret_value = 0;

    if (write(trace_handle, str, strlen(str)) == -1)
    {
        ret_value = 1;
    }

    // printf("@@@ %s\n", str);

    if (fsync(trace_handle) == -1)
    {
        ret_value = 1;
    }

    return ret_value;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceFtrace_BeginTask(mfxTraceStaticHandle *static_handle
    , const char * //file_name
    , mfxTraceU32 //line_num
    , const char * //function_name
    , mfxTraceChar* //category
    , mfxTraceLevel level
    , const char * task_name
    , mfxTraceTaskHandle *handle
    , const void * /*task_params*/)
{
    if (trace_handle == -1) return 1;

    if (MFX_TRACE_LEVEL_INTERNAL_VTUNE == level)
    {
        // TODO: ...
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceFtrace_EndTask(mfxTraceStaticHandle * //static_handle
    , mfxTraceTaskHandle *handle
    )
{
    if (trace_handle == -1) return 1;


    return 0;
}

} // extern "C"


#endif  // #if defined(MFX_TRACE_ENABLE_FTRACE) && defined(LINUX32)