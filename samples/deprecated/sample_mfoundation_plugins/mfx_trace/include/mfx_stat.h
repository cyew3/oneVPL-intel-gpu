/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_stat.h

Purpose: contains definition data for statistics MFX tracing.

*********************************************************************************/

#ifndef __MFX_STAT_H__
#define __MFX_STAT_H__

#include "mfx_trace.h"

/*------------------------------------------------------------------------------*/

// trace registry options and parameters
#define MFX_TRACE_STAT_REG_SUPPRESS  "MfxTraceStat_Suppress"
#define MFX_TRACE_STAT_REG_PERMIT    "MfxTraceStat_Permit"

// defines suppresses of the output (where applicable)
enum
{
    MFX_TRACE_STAT_SUPPRESS_FILE_NAME     = 0x01,
    MFX_TRACE_STAT_SUPPRESS_LINE_NUM      = 0x02,
    MFX_TRACE_STAT_SUPPRESS_CATEGORY      = 0x04,
    MFX_TRACE_STAT_SUPPRESS_LEVEL         = 0x08
};

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceStat_Init(const TCHAR *filename, UINT32 output_mode);

UINT32 MFXTraceStat_SetLevel(TCHAR* category,
                             mfxTraceLevel level);

UINT32 MFXTraceStat_DebugMessage(mfxTraceStaticHandle *static_handle,
                                 const char *file_name, UINT32 line_num,
                                 const char *function_name,
                                 TCHAR* category, mfxTraceLevel level,
                                 const char *message,
                                 const char *format, ...);

UINT32 MFXTraceStat_vDebugMessage(mfxTraceStaticHandle *static_handle,
                                  const char *file_name, UINT32 line_num,
                                  const char *function_name,
                                  TCHAR* category, mfxTraceLevel level,
                                  const char *message,
                                  const char *format, va_list args);

UINT32 MFXTraceStat_BeginTask(mfxTraceStaticHandle *static_handle,
                              const char *file_name, UINT32 line_num,
                              const char *function_name,
                              TCHAR* category, mfxTraceLevel level,
                              const char *task_name, mfxTaskHandle *task_handle);

UINT32 MFXTraceStat_EndTask(mfxTraceStaticHandle *static_handle,
                              mfxTaskHandle *task_handle);

UINT32 MFXTraceStat_Close(void);

#endif // #ifndef __MFX_STAT_H__
