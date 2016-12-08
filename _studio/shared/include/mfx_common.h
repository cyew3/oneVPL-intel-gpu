//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_COMMON_H_
#define _MFX_COMMON_H_

#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "ippdefs.h"
#include "mfx_utils.h"
#include <stdio.h>
#include <string.h>

#include <string>
#include <stdexcept> /* for std exceptions on Linux/Android */

#include "mfx_config.h"

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <stddef.h>
#endif

#define MFX_BIT_IN_KB 8*1000

#define MFX_AUTO_ASYNC_DEPTH_VALUE  5
#define MFX_MAX_ASYNC_DEPTH_VALUE   15

#endif //_MFX_COMMON_H_
