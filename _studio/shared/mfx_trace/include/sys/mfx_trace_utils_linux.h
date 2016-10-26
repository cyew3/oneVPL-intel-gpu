//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TRACE_UTILS_LINUX_H__
#define __MFX_TRACE_UTILS_LINUX_H__

#if !defined(_WIN32) && !defined(_WIN64)

#ifdef MFX_TRACE_ENABLE

#include <stdio.h>

#define MFX_TRACE_CONFIG          "mfx_trace"
#define MFX_TRACE_CONFIG_PATH     "/etc"
#define MFX_TRACE_CONF_OMODE_TYPE "Output"
#define MFX_TRACE_CONF_LEVEL      "Level"

/*------------------------------------------------------------------------------*/
extern "C"
{
FILE* mfx_trace_open_conf_file(const char* name);

mfxTraceU32 mfx_trace_get_conf_dword(FILE* file,
                                     const char* pName,
                                     mfxTraceU32* pValue);
mfxTraceU32 mfx_trace_get_conf_string(FILE* file,
                                      const char* pName,
                                      char* pValue, mfxTraceU32 cValueMaxSize);
}

#endif // #ifdef MFX_TRACE_ENABLE

#endif // #if !defined(_WIN32) && !defined(_WIN64)

#endif // #ifndef __MFX_TRACE_UTILS_LINUX_H__
