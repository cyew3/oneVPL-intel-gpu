//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TRACE_TAL_H__
#define __MFX_TRACE_TAL_H__

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE_TAL

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTAL_Init(const mfxTraceChar *filename, mfxTraceU32 output_mode);

mfxTraceU32 MFXTraceTAL_SetLevel(mfxTraceChar* category,
                            mfxTraceLevel level);

mfxTraceU32 MFXTraceTAL_DebugMessage(mfxTraceStaticHandle *static_handle,
                                const char *file_name, mfxTraceU32 line_num,
                                const char *function_name,
                                mfxTraceChar* category, mfxTraceLevel level,
                                const char *message,
                                const char *format, ...);

mfxTraceU32 MFXTraceTAL_vDebugMessage(mfxTraceStaticHandle *static_handle,
                                 const char *file_name, mfxTraceU32 line_num,
                                 const char *function_name,
                                 mfxTraceChar* category, mfxTraceLevel level,
                                 const char *message,
                                 const char *format, va_list args);

mfxTraceU32 MFXTraceTAL_BeginTask(mfxTraceStaticHandle *static_handle,
                             const char *file_name, mfxTraceU32 line_num,
                             const char *function_name,
                             mfxTraceChar* category, mfxTraceLevel level,
                             const char *task_name, mfxTraceTaskHandle *task_handle,
                             const void *task_params);

mfxTraceU32 MFXTraceTAL_EndTask(mfxTraceStaticHandle *static_handle,
                           mfxTraceTaskHandle *task_handle);

mfxTraceU32 MFXTraceTAL_Close(void);

#endif // #ifdef MFX_TRACE_ENABLE_TAL
#endif // #ifndef __MFX_TRACE_TAL_H__
