//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_PTIR_HW_UTILS_PLUGIN_INCLUDED__)
#define __MFX_PTIR_HW_UTILS_PLUGIN_INCLUDED__

#if defined(_WIN32) || defined(_WIN64)
#include <d3d9.h>
#include <dxva2api.h>
#include <d3d11.h>
#include <atlbase.h>
#endif

#if (defined(LINUX32) || defined(LINUX64))
#include "va/va.h"
#include <va/va_backend.h>

#include <sys/ioctl.h>
#define I915_PARAM_CHIPSET_ID            4
#define DRM_I915_GETPARAM   0x06
#define DRM_IOCTL_BASE          'd'
#define DRM_COMMAND_BASE                0x40
#define DRM_IOWR(nr,type)       _IOWR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOCTL_I915_GETPARAM         DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GETPARAM, drm_i915_getparam_t)
#endif

#include "mfxvideo.h"
#include "mfxvideo++int.h"

eMFXHWType GetHWType(const mfxU32 adapterNum, mfxIMPL impl, mfxHDL& mfxDeviceHdl);

#endif  // __MFX_PTIR_HW_UTILS_PLUGIN_INCLUDED__