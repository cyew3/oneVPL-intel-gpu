/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxvideo.h"

#if (1 <= MFX_VERSION_MAJOR) && (4 <= MFX_VERSION_MINOR)
    #define MFX_PIPELINE_SUPPORT_VP8
#endif

#if defined(_WIN32) 
#include <sdkddkver.h>
#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
    #define MFX_D3D11_SUPPORT 1 // Enable D3D11 support if SDK allows
#else
    #define MFX_D3D11_SUPPORT 0
#endif
#endif


//d3d surafece and d3d11 surfaces supported on windows only
#if defined (_WIN32) && !defined(D3D_SURFACES_SUPPORT)
    #define D3D_SURFACES_SUPPORT
#endif


