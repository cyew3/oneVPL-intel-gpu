// Copyright (c) 2021 Intel Corporation
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

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE /* for RTLD_NEXT */
#endif

#include <dlfcn.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct drm_i915_getparam {
    int param;
    int *value;
} drm_i915_getparam_t;

#define DRM_I915_GETPARAM       0x06
#define DRM_IOCTL_BASE          'd'
#define DRM_COMMAND_BASE        0x40
#define DRM_IOWR(nr,type)       _IOWR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOCTL_I915_GETPARAM DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GETPARAM, drm_i915_getparam_t)

extern "C"
{
    extern void* _dl_sym(void*, const char*, ...);

    int ioctl(int fd, unsigned long request, ...) {    // initialize on first call
        static int (*libc_ioctl)(int fd, unsigned long request, ...);
        if (libc_ioctl == NULL) {
            libc_ioctl = (int (*)(int, long unsigned int, ...))_dl_sym(RTLD_NEXT, "ioctl", ioctl);
        }
        va_list args;
        void* argp;
        va_start(args, request);
        argp = va_arg(args, void*);
        va_end(args);
        drm_i915_getparam_t gp = *(drm_i915_getparam_t*)(argp);
        switch (request) {
            case DRM_IOCTL_I915_GETPARAM:
                // *gp.value = 0x9A40;  // use HW_TGL_LP
                *gp.value = 0x0201;  // use MFX_HW_XE_HP
                return 0;
            default:
                return libc_ioctl(fd, request, argp);
        }
    }
}
