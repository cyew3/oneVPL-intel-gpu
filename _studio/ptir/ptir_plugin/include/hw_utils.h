// Copyright (c) 2014-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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