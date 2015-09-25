/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel’s Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2012-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __FILE_DEFS_H__
#define __FILE_DEFS_H__

#include "mfxdefs.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)

#define MSDK_FOPEN(file, name, mode) _tfopen_s(&file, name, mode)

#define msdk_fgets  _fgetts
#else // #if defined(_WIN32) || defined(_WIN64)
#include <unistd.h>

#define MSDK_FOPEN(file, name, mode) file = fopen(name, mode)

#define msdk_fgets  fgets
#endif // #if defined(_WIN32) || defined(_WIN64)

#endif // #ifndef __FILE_DEFS_H__
