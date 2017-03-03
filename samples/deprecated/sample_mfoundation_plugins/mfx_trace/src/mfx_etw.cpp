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

File: mfx_etw.cpp

Mapping of mfxTraceStaticHandle:
    etw1 - char* - file name
    etw2 - UINT32 - line number
    etw3 - char* - function name
    etw4 - char* - task name

Mapping of mfxTaskHandle:
    n/a

*********************************************************************************/
#include "mfx_etw.h"

#include <stdio.h>
#include <math.h>
#include <evntprov.h>

/*------------------------------------------------------------------------------*/


static const GUID MFX_TRACE_ETW_GUID =
    {0x2D6B112A, 0xD21C, 0x4a40, 0x9B, 0xF2, 0xA3, 0xED, 0xF2, 0x12, 0xF6, 0x24};
REGHANDLE g_EventRegistered = 0;

/*------------------------------------------------------------------------------*/

class mfxETWGlobalHandle
{
public:
    ~mfxETWGlobalHandle(void)
    {
        Close();
    };

    void Close(void)
    {
        if (g_EventRegistered)
        {
            EventUnregister(g_EventRegistered);
            g_EventRegistered = 0;
        }
    };
};

mfxETWGlobalHandle g_ETWGlobalHandle;

/*------------------------------------------------------------------------------*/

enum
{
    MFX_TRACE_ETW_EVENT_START = 0x01,
    MFX_TRACE_ETW_EVENT_STOP  = 0x02,
    MFX_TRACE_ETW_EVENT_I     = 0xE0, // int
    MFX_TRACE_ETW_EVENT_F     = 0xE1, // double
    MFX_TRACE_ETW_EVENT_P     = 0xE2, // void*
    MFX_TRACE_ETW_EVENT_S     = 0xE3, // char*
    MFX_TRACE_ETW_EVENT_UNK   = 0xE4, // unknown format
};

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_GetRegistryParams(void)
{
    UINT32 sts = 0;
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_Init(const TCHAR* /*filename*/, UINT32 output_mode)
{
    UINT32 sts = 0;
    if (!(output_mode & MFX_TRACE_OMODE_ETW)) return 1;

    sts = MFXTraceETW_Close();
    if (!sts) sts = MFXTraceETW_GetRegistryParams();
    if (!sts)
    {
        if (ERROR_SUCCESS != EventRegister(&MFX_TRACE_ETW_GUID, NULL, NULL, &g_EventRegistered))
        {
            return 1;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_Close(void)
{
    g_ETWGlobalHandle.Close();
    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_SetLevel(TCHAR* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_DebugMessage(mfxTraceStaticHandle* static_handle,
                                 const char *file_name, UINT32 line_num,
                                 const char *function_name,
                                 TCHAR* category, mfxTraceLevel level,
                                 const char *message, const char *format, ...)
{
    UINT32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceETW_vDebugMessage(static_handle,
                                    file_name , line_num,
                                    function_name,
                                    category, level,
                                    message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_vDebugMessage(mfxTraceStaticHandle* /*static_handle*/,
                                 const char* /*file_name*/, UINT32 /*line_num*/,
                                 const char* /*function_name*/,
                                 TCHAR* /*category*/, mfxTraceLevel /*level*/,
                                 const char* message,
                                 const char* format, va_list args)
{
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data_descriptor[2];
    ULONG count = 0;
    int format_I = 0;
    void* format_P = NULL;
    double format_F = 0.0;
    char format_UNK[MFX_TRACE_MAX_LINE_LENGTH] = {0};
    size_t format_UNK_len = MFX_TRACE_MAX_LINE_LENGTH;

    memset(&descriptor, 0, sizeof(EVENT_DESCRIPTOR));
    memset(&data_descriptor, 0, sizeof(data_descriptor));

    if (!format && !message) return 1;

    if (!format)
    {
        descriptor.Opcode = MFX_TRACE_ETW_EVENT_S;

        EventDataDescCreate(&data_descriptor[count++], message, (ULONG)(strlen(message) + 1));
    }
    else if (!strcmp(format, MFX_TRACE_FORMAT_I) ||
             !strcmp(format, MFX_TRACE_FORMAT_X) ||
             !strcmp(format, MFX_TRACE_FORMAT_D))
    {
        descriptor.Opcode = MFX_TRACE_ETW_EVENT_I;

        if (message) EventDataDescCreate(&data_descriptor[count++], message, (ULONG)(strlen(message) + 1));
        format_I = va_arg(args, int);
        EventDataDescCreate(&data_descriptor[count++], &format_I, sizeof(format_I));
    }
    else if (!strcmp(format, MFX_TRACE_FORMAT_P))
    {
        descriptor.Opcode = MFX_TRACE_ETW_EVENT_P;

        if (message) EventDataDescCreate(&data_descriptor[count++], message, (ULONG)(strlen(message) + 1));
        format_P = va_arg(args, void*);
        EventDataDescCreate(&data_descriptor[count++], &format_P, sizeof(format_P));
    }
    else if (!strcmp(format, MFX_TRACE_FORMAT_F))
    {
        descriptor.Opcode = MFX_TRACE_ETW_EVENT_F;

        if (message) EventDataDescCreate(&data_descriptor[count++], message, (ULONG)(strlen(message) + 1));
        format_F = va_arg(args, double);
        EventDataDescCreate(&data_descriptor[count++], &format_F, sizeof(format_F));
    }
    else
    {
        descriptor.Opcode = MFX_TRACE_ETW_EVENT_UNK;

        if (message) EventDataDescCreate(&data_descriptor[count++], message, (ULONG)(strlen(message) + 1));
        mfx_trace_vsprintf(format_UNK, format_UNK_len, format, args);
        EventDataDescCreate(&data_descriptor[count++], &format_UNK, sizeof(format_UNK) + 1);
    }
    if (!count ||
        (ERROR_SUCCESS != EventWrite(g_EventRegistered, &descriptor, count, data_descriptor)))
    {
        return 1;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_SendNamedEvent(mfxTraceStaticHandle *static_handle,
                                  UCHAR opcode)
{
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data_descriptor;
    char* function_name = NULL;
    char* task_name = NULL;

    if (!static_handle) return 1;

    task_name = static_handle->etw4.str;
    function_name = static_handle->etw3.str;

    memset(&descriptor, 0, sizeof(EVENT_DESCRIPTOR));
    memset(&data_descriptor, 0, sizeof(EVENT_DATA_DESCRIPTOR));

    descriptor.Opcode = opcode;

    if (task_name)
    {
        EventDataDescCreate(&data_descriptor, task_name, (ULONG)(strlen(task_name) + 1));
    }
    else if (function_name)
    {
        EventDataDescCreate(&data_descriptor, function_name, (ULONG)(strlen(function_name) + 1));
    }
    else return 1;

    if (ERROR_SUCCESS != EventWrite(g_EventRegistered, &descriptor, 1, &data_descriptor))
    {
        return 1;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_BeginTask(mfxTraceStaticHandle *static_handle,
                             const char *file_name, UINT32 line_num,
                             const char *function_name,
                             TCHAR* /*category*/, mfxTraceLevel /*level*/,
                             const char *task_name, mfxTaskHandle* /*task_handle*/)
{
    if (!static_handle) return 1;

    static_handle->etw1.str    = (char*)file_name;
    static_handle->etw2.uint32 = line_num;
    static_handle->etw3.str    = (char*)function_name;
    static_handle->etw4.str    = (char*)task_name;

    return MFXTraceETW_SendNamedEvent(static_handle, MFX_TRACE_ETW_EVENT_START);
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_EndTask(mfxTraceStaticHandle *static_handle,
                           mfxTaskHandle* /*task_handle*/)
{
    if (!static_handle) return 1;

    return MFXTraceETW_SendNamedEvent(static_handle, MFX_TRACE_ETW_EVENT_STOP);
}
