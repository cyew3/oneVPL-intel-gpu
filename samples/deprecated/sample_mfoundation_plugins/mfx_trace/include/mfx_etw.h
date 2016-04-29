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
