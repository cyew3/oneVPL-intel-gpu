//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TRACE_ETW_H__
#define __MFX_TRACE_ETW_H__

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE_ETW

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_Init();

mfxTraceU32 MFXTraceETW_SetLevel(mfxTraceChar* category,
                            mfxTraceLevel level);

mfxTraceU32 MFXTraceETW_DebugMessage(mfxTraceStaticHandle *static_handle,
                                const char *file_name, mfxTraceU32 line_num,
                                const char *function_name,
                                mfxTraceChar* category, mfxTraceLevel level,
                                const char *message,
                                const char *format, ...);

mfxTraceU32 MFXTraceETW_vDebugMessage(mfxTraceStaticHandle *static_handle,
                                 const char *file_name, mfxTraceU32 line_num,
                                 const char *function_name,
                                 mfxTraceChar* category, mfxTraceLevel level,
                                 const char *message,
                                 const char *format, va_list args);

mfxTraceU32 MFXTraceETW_BeginTask(mfxTraceStaticHandle *static_handle,
                             const char *file_name, mfxTraceU32 line_num,
                             const char *function_name,
                             mfxTraceChar* category, mfxTraceLevel level,
                             const char *task_name, mfxTraceTaskHandle *task_handle,
                             const void *task_params);

mfxTraceU32 MFXTraceETW_EndTask(mfxTraceStaticHandle *static_handle,
                           mfxTraceTaskHandle *task_handle);

mfxTraceU32 MFXTraceETW_Close(void);

#endif // #ifdef MFX_TRACE_ENABLE_ETW
#endif // #ifndef __MFX_TRACE_ETW_H__
