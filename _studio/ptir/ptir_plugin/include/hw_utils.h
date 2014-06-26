/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: hw_utils.h

\* ****************************************************************************** */

#if !defined(__MFX_PTIR_HW_UTILS_PLUGIN_INCLUDED__)
#define __MFX_PTIR_HW_UTILS_PLUGIN_INCLUDED__

#if defined(_WIN32) || defined(_WIN64)
#include <d3d9.h>
#include <dxva2api.h>
#include <d3d11.h>
#endif
#include "mfxvideo.h"
#include "mfxvideo++int.h"

eMFXHWType GetHWType(const mfxU32 adapterNum, mfxIMPL impl, mfxHDL* mfxDeviceHdl);

#endif  // __MFX_PTIR_HW_UTILS_PLUGIN_INCLUDED__