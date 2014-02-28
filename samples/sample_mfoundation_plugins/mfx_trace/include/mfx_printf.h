/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_printf.h

Purpose: contains definition data for file MFX tracing.

*********************************************************************************/

#ifndef __MFX_PRINTF_H__
#define __MFX_PRINTF_H__

#include "mfx_trace.h"

/*------------------------------------------------------------------------------*/

// trace registry options and parameters
#define MFX_TRACE_PRINTF_REG_SUPPRESS  "MfxTracePrintf_Suppress"
#define MFX_TRACE_PRINTF_REG_PERMIT    "MfxTracePrintf_Permit"

// defines suppresses of the output (where applicable)
enum
{
    MFX_TRACE_PRINTF_SUPPRESS_FILE_NAME     = 0x01,
    MFX_TRACE_PRINTF_SUPPRESS_LINE_NUM      = 0x02,
    MFX_TRACE_PRINTF_SUPPRESS_CATEGORY      = 0x04,
    MFX_TRACE_PRINTF_SUPPRESS_LEVEL         = 0x08,
    MFX_TRACE_PRINTF_SUPPRESS_FUNCTION_NAME = 0x10
};

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_Init(const TCHAR *filename, UINT32 output_mode);

UINT32 MFXTracePrintf_SetLevel(TCHAR* category,
                               mfxTraceLevel level);

UINT32 MFXTracePrintf_DebugMessage(mfxTraceStaticHandle *static_handle,
                                   const char *file_name, UINT32 line_num,
                                   const char *function_name,
                                   TCHAR* category, mfxTraceLevel level,
                                   const char *message,
                                   const char *format, ...);

UINT32 MFXTracePrintf_vDebugMessage(mfxTraceStaticHandle *static_handle,
                                    const char *file_name, UINT32 line_num,
                                    const char *function_name,
                                    TCHAR* category, mfxTraceLevel level,
                                    const char *message,
                                    const char *format, va_list args);

UINT32 MFXTracePrintf_BeginTask(mfxTraceStaticHandle *static_handle,
                                const char *file_name, UINT32 line_num,
                                const char *function_name,
                                TCHAR* category, mfxTraceLevel level,
                                const char *task_name, mfxTaskHandle *task_handle);

UINT32 MFXTracePrintf_EndTask(mfxTraceStaticHandle *static_handle,
                              mfxTaskHandle *task_handle);

UINT32 MFXTracePrintf_Close(void);

#endif // #ifndef __MFX_PRINTF_H__
