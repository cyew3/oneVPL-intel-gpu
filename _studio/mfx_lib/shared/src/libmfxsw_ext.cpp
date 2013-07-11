/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: libmfxsw.cpp

\* ****************************************************************************** */

#include <mfxvideo.h>
#include <mfxaudio.h>
#include <mfx_session.h>
#include <mfx_check_hardware_support.h>
#include <mfx_trace.h>

#include <ippcore.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <tchar.h>
#endif

#if !defined(_WIN32) && !defined(_WIN64)
/* These string constants set Media SDK version information for Linux, Android, OSX. */
#ifndef MFX_FILE_VERSION
    #define MFX_FILE_VERSION "0.0.0.0"
#endif
#ifndef MFX_PRODUCT_VERSION
    #define MFX_PRODUCT_VERSION "0.0.000.0000"
#endif

const char* g_MfxProductName = "mediasdk_product_name: Intel(r) Media SDK";
const char* g_MfxCopyright = "mediasdk_copyright: Copyright(c) 2007-2013 Intel Corporation";
const char* g_MfxFileVersion = "mediasdk_file_version: " MFX_FILE_VERSION;
const char* g_MfxProductVersion = "mediasdk_product_version: " MFX_PRODUCT_VERSION;
#endif
