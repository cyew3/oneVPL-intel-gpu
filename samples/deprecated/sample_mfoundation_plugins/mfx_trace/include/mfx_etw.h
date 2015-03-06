/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_etw.h

Purpose: contains definition data for ETW (Event Tracing for Windows) MFX tracing.

*********************************************************************************/

#ifndef __MFX_ETW_H__
#define __MFX_ETW_H__

#include "mfx_trace.h"

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceETW_Init(const TCHAR *filename, UINT32 output_mode);

UINT32 MFXTraceETW_SetLevel(TCHAR* category,
                            mfxTraceLevel level);

UINT32 MFXTraceETW_DebugMessage(mfxTraceStaticHandle *static_handle,
                                const char *file_name, UINT32 line_num,
                                const char *function_name,
                                TCHAR* category, mfxTraceLevel level,
                                const char *message,
                                const char *format, ...);

UINT32 MFXTraceETW_vDebugMessage(mfxTraceStaticHandle *static_handle,
                                 const char *file_name, UINT32 line_num,
                                 const char *function_name,
                                 TCHAR* category, mfxTraceLevel level,
                                 const char *message,
                                 const char *format, va_list args);

UINT32 MFXTraceETW_BeginTask(mfxTraceStaticHandle *static_handle,
                             const char *file_name, UINT32 line_num,
                             const char *function_name,
                             TCHAR* category, mfxTraceLevel level,
                             const char *task_name, mfxTaskHandle *task_handle);

UINT32 MFXTraceETW_EndTask(mfxTraceStaticHandle *static_handle,
                           mfxTaskHandle *task_handle);

UINT32 MFXTraceETW_Close(void);

#endif // #ifndef __MFX_ETW_H__
