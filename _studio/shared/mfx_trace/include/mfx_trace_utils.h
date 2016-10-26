//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TRACE_UTILS_H__
#define __MFX_TRACE_UTILS_H__

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE

#include "sys/mfx_trace_utils_windows.h"
#include "sys/mfx_trace_utils_linux.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// defines maximum length of output line
#define MFX_TRACE_MAX_LINE_LENGTH 1024

/*------------------------------------------------------------------------------*/

struct mfxTraceCategoryItem
{
    mfxTraceChar m_name[MAX_PATH];
    mfxTraceU32  m_level;
};

/*------------------------------------------------------------------------------*/
// help functions
extern "C"
{
char* mfx_trace_vsprintf(char* str, size_t& str_size, const char* format, va_list args);
char* mfx_trace_sprintf(char* str, size_t& str_size, const char* format, ...);
int mfx_trace_tcmp(const mfxTraceChar* str1, const mfxTraceChar* str2);
}
/*------------------------------------------------------------------------------*/
// help macroses

#if defined(_WIN32) || defined(_WIN64)

#define mfx_trace_fopen(FNAME, FMODE) _fsopen(FNAME, FMODE, _SH_DENYNO)
#define mfx_trace_tfopen(FNAME, FMODE) _tfsopen(FNAME, FMODE, _SH_DENYNO)

#else

#define mfx_trace_fopen(FNAME, FMODE) fopen(FNAME, FMODE)
#define mfx_trace_tfopen(FNAME, FMODE) mfx_trace_fopen(FNAME, FMODE)

#endif // #if defined(_WIN32) || defined(_WIN64)

#endif // #ifdef MFX_TRACE_ENABLE

#endif // #ifndef __MFX_TRACE_UTILS_H__
