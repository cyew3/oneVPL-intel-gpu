/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: plugin_version_linux.h

\* ****************************************************************************** */

// This file should be included by exactly one cpp in each MediaSDK plugin

#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

/* These string constants set Media SDK version information for Linux, Android, OSX. */
#ifndef MFX_PLUGIN_FILE_VERSION
#define MFX_PLUGIN_FILE_VERSION "0.0.0.0"
#endif
#ifndef MFX_PLUGIN_PRODUCT_NAME
#define MFX_PLUGIN_PRODUCT_NAME "Intel(R) Media SDK Plugin"
#endif
#ifndef MFX_PLUGIN_PRODUCT_VERSION
#define MFX_PLUGIN_PRODUCT_VERSION "0.0.000.0000"
#endif
#ifndef MFX_FILE_DESCRIPTION
#define MFX_FILE_DESCRIPTION "File info ought to be here"
#endif


const char* g_MfxProductName = "mediasdk_product_name: " MFX_PLUGIN_PRODUCT_NAME;
const char* g_MfxCopyright = "mediasdk_copyright: Copyright(c) 2007-2016 Intel Corporation";

#if defined(HEVCD_EVALUATION) || defined(HEVCE_EVALUATION)
const char* g_MfxProductVersion = "mediasdk_product_version: " MFX_PLUGIN_PRODUCT_VERSION " Evaluation version";
#else
const char* g_MfxProductVersion = "mediasdk_product_version: " MFX_PLUGIN_PRODUCT_VERSION;
#endif
                                                                        
const char* g_MfxFileVersion = "mediasdk_file_version: " MFX_PLUGIN_FILE_VERSION;
const char* g_MfxFileDescription = "mediasdk_file_description: " MFX_FILE_DESCRIPTION;

#endif // Linux-only code