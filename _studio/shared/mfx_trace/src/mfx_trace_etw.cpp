//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE_ETW
extern "C"
{

#include "mfx_trace_utils.h"
#include "mfx_trace_etw.h"

#ifndef MFX_VA

mfxTraceU32 MFXTraceETW_Init()
{
    return 0;
}

mfxTraceU32 MFXTraceETW_Close(void)
{
    return 0;
}

mfxTraceU32 MFXTraceETW_SetLevel(mfxTraceChar*, mfxTraceLevel)
{
    return 0;
}

mfxTraceU32 MFXTraceETW_DebugMessage(mfxTraceStaticHandle* ,
                                 const char *, mfxTraceU32 ,
                                 const char *,
                                 mfxTraceChar* , mfxTraceLevel ,
                                 const char *, const char *, ...)
{
    return 0;
}

mfxTraceU32 MFXTraceETW_vDebugMessage(mfxTraceStaticHandle* /*handle*/,
                                 const char* /*file_name*/, mfxTraceU32 ,
                                 const char* /*function_name*/,
                                 mfxTraceChar* /*category*/, mfxTraceLevel,
                                 const char*,
                                 const char*, va_list)
{
    return 0;
}

mfxTraceU32 MFXTraceETW_SendNamedEvent(mfxTraceStaticHandle *,
                                  mfxTraceTaskHandle *,
                                  UCHAR,
                                  const mfxTraceU32 *)
{
    return 0;
}

mfxTraceU32 MFXTraceETW_BeginTask(mfxTraceStaticHandle *,
                             const char * /*file_name*/, mfxTraceU32,
                             const char *,
                             mfxTraceChar* /*category*/, mfxTraceLevel /*level*/,
                             const char *, mfxTraceTaskHandle*,
                             const void *)
{
    return 0;
}

mfxTraceU32 MFXTraceETW_EndTask(mfxTraceStaticHandle *,
                           mfxTraceTaskHandle*)
{
    return 0;
}

#else

#include "mfx_trace_utils.h"
#include "mfx_trace_etw.h"

#include <stdio.h>
#include <math.h>

//#include "mfx_evntprov.h"
#include <evntprov.h>

/*------------------------------------------------------------------------------*/

static const GUID MFX_TRACE_ETW_GUID =
    {0x2D6B112A, 0xD21C, 0x4a40, 0x9B, 0xF2, 0xA3, 0xED, 0xF2, 0x12, 0xF6, 0x24};

/*------------------------------------------------------------------------------*/

static REGHANDLE g_EventRegistered = 0;

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

static mfxETWGlobalHandle g_ETWGlobalHandle;

/*------------------------------------------------------------------------------*/

enum
{
    MPA_ETW_OPCODE_START         = 0x01, // Task start
    MPA_ETW_OPCODE_STOP          = 0x02, // Task end
    MPA_ETW_OPCODE_MESSAGE       = 0x03, // String message
    MPA_ETW_OPCODE_RELATIONS     = 0x04, // Relations between tasks
    MPA_ETW_OPCODE_SET_ID        = 0x05, // Set new id for further usage instead of string name
    MPA_ETW_OPCODE_PARAM_INTEGER = 0x10, // Param: int
    MPA_ETW_OPCODE_PARAM_DOUBLE  = 0x11, // Param: double
    MPA_ETW_OPCODE_PARAM_POINTER = 0x12, // Param: void*
    MPA_ETW_OPCODE_PARAM_STRING  = 0x13, // Param: char*
    MPA_ETW_OPCODE_BUFFER        = 0x14, // Param: buffer
    MPA_ETW_OPCODE_NAMEID        = 0x80, // Bitmask with other opcodes. Name passed as ID instead of string.
};

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_GetRegistryParams(void)
{
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_Init()
{
    TCHAR reg_filename[MAX_PATH];
    mfxTraceU32 sts = 0;

    if (g_EventRegistered) return 0; // initilized already

    reg_filename[0] = 0;
    mfx_trace_get_reg_string(HKEY_CURRENT_USER, MFX_TRACE_REG_PARAMS_PATH, _T("ETW"), reg_filename, sizeof(reg_filename));
    if (!reg_filename[0])
    {
        mfx_trace_get_reg_string(HKEY_LOCAL_MACHINE, MFX_TRACE_REG_PARAMS_PATH, _T("ETW"), reg_filename, sizeof(reg_filename));
    }
    if (!reg_filename[0])
    {
        return 1;
    }

    //sts = MFXTraceETW_Close();

    //if (!sts)
    //{
    //    sts = MFXTraceETW_GetRegistryParams();
    //}
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

mfxTraceU32 MFXTraceETW_Close(void)
{
    g_ETWGlobalHandle.Close();
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_SetLevel(mfxTraceChar* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_DebugMessage(mfxTraceStaticHandle* handle,
                                 const char *file_name, mfxTraceU32 line_num,
                                 const char *function_name,
                                 mfxTraceChar* category, mfxTraceLevel level,
                                 const char *message, const char *format, ...)
{
    mfxTraceU32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceETW_vDebugMessage(handle,
                                    file_name , line_num,
                                    function_name,
                                    category, level,
                                    message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_vDebugMessage(mfxTraceStaticHandle* /*handle*/,
                                 const char* /*file_name*/, mfxTraceU32 line_num,
                                 const char* /*function_name*/,
                                 mfxTraceChar* /*category*/, mfxTraceLevel level,
                                 const char* message,
                                 const char* format, va_list args)
{
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data_descriptor[4];
    char format_UNK[MFX_TRACE_MAX_LINE_LENGTH] = {0};
    ULONG count = 0;

    if (!message) return 1;

    memset(&descriptor, 0, sizeof(EVENT_DESCRIPTOR));
    descriptor.Level = (UCHAR)level;
    descriptor.Task = (USHORT)line_num;
    if (!EventEnabled(g_EventRegistered, &descriptor))
    {
        // no ETW consumer
        return 1;
    }

    //memset(data_descriptor, 0, sizeof(data_descriptor));
    EventDataDescCreate(&data_descriptor[count++], message, (ULONG)strlen(message) + 1);

    if (format == NULL)
    {
        descriptor.Opcode = MPA_ETW_OPCODE_MESSAGE;
    } else if (message[0] == '^')
    {
        if (!strncmp(message, "^ModuleHandle^", 14))
        {
            HMODULE hModule = (HMODULE)va_arg(args, void*);
            if (hModule)
            {
                if (SUCCEEDED(GetModuleFileNameA(hModule, format_UNK, sizeof(format_UNK) - 1)))
                {
                    descriptor.Opcode = MPA_ETW_OPCODE_PARAM_STRING;
                    EventDataDescCreate(&data_descriptor[count++], format_UNK, (ULONG)strlen(format_UNK) + 1);
                }
            }
        }
    }
    if (!descriptor.Opcode && format != NULL && format[0] == '%')
    {
        if (format[2] == 0)
        {
            switch (format[1])
            {
            case 'd':
            case 'x':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_INTEGER;
                descriptor.Keyword = va_arg(args, int);
                break;
            case 'p':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_POINTER;
                descriptor.Keyword = (UINT64)va_arg(args, void*);
                break;
            case 'f':
            case 'g':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_DOUBLE;
                *(double*)&descriptor.Keyword = va_arg(args, double);
                break;
            case 's':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_STRING;
                char *str = va_arg(args, char*);
                EventDataDescCreate(&data_descriptor[count++], str, (ULONG)strlen(str) + 1);
                break;
            }
        } else
        {
            if (!strcmp(format, "%p[%d]"))
            {
                descriptor.Opcode = MPA_ETW_OPCODE_BUFFER;
                char *buffer = va_arg(args, char*);
                int size = va_arg(args, int);
                EventDataDescCreate(&data_descriptor[count++], buffer, size);
            }
        }
    }
    if (!descriptor.Opcode)
    {
        descriptor.Opcode = MPA_ETW_OPCODE_PARAM_STRING;
        size_t format_UNK_len = MFX_TRACE_MAX_LINE_LENGTH;
        mfx_trace_vsprintf(format_UNK, format_UNK_len, format, args);
        EventDataDescCreate(&data_descriptor[count++], format_UNK, (ULONG)strlen(format_UNK) + 1);
    }

    // send ETW event
    if (!count ||
        (ERROR_SUCCESS != EventWrite(g_EventRegistered, &descriptor, count, data_descriptor)))
    {
        return 1;
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_SendNamedEvent(mfxTraceStaticHandle *static_handle,
                                  mfxTraceTaskHandle *handle,
                                  UCHAR opcode,
                                  const mfxTraceU32 *task_params = NULL)
{
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data_descriptor;
    char* task_name = NULL;

    if (!handle) return 1;

    task_name = handle->etw1.str;
    if (!task_name) return 1;

    memset(&descriptor, 0, sizeof(EVENT_DESCRIPTOR));
    memset(&data_descriptor, 0, sizeof(EVENT_DATA_DESCRIPTOR));

    descriptor.Opcode = opcode;
    descriptor.Level = (static_handle) ? (UCHAR)static_handle->level : 0;
    descriptor.Task = (USHORT)handle->etw2.uint32;
    if (task_params)
    {
        descriptor.Id = 1;
        descriptor.Keyword = *task_params;
    }

    EventDataDescCreate(&data_descriptor, task_name, (ULONG)(strlen(task_name) + 1));

    if (ERROR_SUCCESS != EventWrite(g_EventRegistered, &descriptor, 1, &data_descriptor))
    {
        return 1;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_BeginTask(mfxTraceStaticHandle *static_handle,
                             const char * /*file_name*/, mfxTraceU32 line_num,
                             const char *function_name,
                             mfxTraceChar* /*category*/, mfxTraceLevel /*level*/,
                             const char *task_name, mfxTraceTaskHandle* task_handle,
                             const void *task_params)
{
    if (!task_handle) return 1;

    task_handle->etw1.str    = (char*)((task_name) ? task_name : function_name);
    task_handle->etw2.uint32 = line_num;

    return MFXTraceETW_SendNamedEvent(static_handle, task_handle, MPA_ETW_OPCODE_START, (const UINT32*)task_params);
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_EndTask(mfxTraceStaticHandle *static_handle,
                           mfxTraceTaskHandle* task_handle)
{
    if (!task_handle) return 1;

    return MFXTraceETW_SendNamedEvent(static_handle, task_handle, MPA_ETW_OPCODE_STOP);
}

#endif

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE_ETW
