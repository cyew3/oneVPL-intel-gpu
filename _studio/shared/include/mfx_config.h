// Copyright (c) 2016-2021 Intel Corporation
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

#ifndef _MFX_CONFIG_H_
#define _MFX_CONFIG_H_

#include "mfxdefs.h"

#define MFX_EXTBUFF_GPU_HANG_ENABLE
#define MFX_UNDOCUMENTED_CO_DDI

#if defined(_WIN32) || defined(_WIN64)
#if defined(DEBUG) || defined(_DEBUG)
#undef  MFX_DEBUG_TOOLS // to avoid redefinition
#define MFX_DEBUG_TOOLS
#endif
#endif // #if defined(_WIN32) || defined(_WIN64)

#if defined(LINUX32) || defined(LINUX64)
    #include <va/va_version.h>
    #undef  MFX_VA_LINUX
    #define MFX_VA_LINUX

    #undef  UMC_VA_LINUX
    #define UMC_VA_LINUX

#elif defined(_WIN32) || defined(_WIN64)

    #undef  MFX_VA_WIN
    #define MFX_VA_WIN

    #undef  UMC_VA_DXVA
    #define UMC_VA_DXVA

    #define MFX_D3D9_ENABLED

#endif // #if defined(LINUX32) || defined(LINUX64)


#if defined(_WIN32) || defined(_WIN64)
    // mfxconfig.h is auto-generated file containing mediasdk per-component
    // enable defines
    #include "mfxconfig.h"

    #define MFX_ENABLE_H265_VIDEO_ENCODE
#elif defined(ANDROID)
    #include "mfx_android_defs.h"

    #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#else
    // mfxconfig.h is auto-generated file containing mediasdk per-component
    // enable defines
    #include "mfxconfig.h"
#endif

#define MFX_PRIVATE_AVC_ENCODE_CTRL_DISABLE

// closed source fixed-style defines
#if !defined(OPEN_SOURCE) && !defined(ANDROID)
    #if defined(LINUX_TARGET_PLATFORM_BDW) || defined(LINUX_TARGET_PLATFORM_CFL) || defined(LINUX_TARGET_PLATFORM_BXT) || defined(LINUX_TARGET_PLATFORM_BSW)
        #define PRE_SI_GEN 11
    #endif

    #if (MFX_VERSION >= MFX_VERSION_NEXT)
        #define MFX_ENABLE_AV1_VIDEO_ENCODE
    #endif

    #if defined(MFX_ENABLE_VPP)
        #define MFX_ENABLE_MJPEG_WEAVE_DI_VPP
    #endif //MFX_ENABLE_VPP

    #if defined(__linux__)
        // Unsupported on Linux:
        #define MFX_PROTECTED_FEATURE_DISABLE
        #define MFX_CAMERA_FEATURE_DISABLE

        #if (MFX_VERSION < MFX_VERSION_NEXT)
            #define MFX_EXT_DPB_HEVC_DISABLE
            #define MFX_ADAPTIVE_PLAYBACK_DISABLE
            #define MFX_FUTURE_FEATURE_DISABLE
        #endif
        #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
    #endif
    #if defined (MFX_VA_LINUX)
        #define SYNCHRONIZATION_BY_VA_MAP_BUFFER
        #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
    #endif

#if defined (PRE_SI_GEN)
    #define ENABLE_PRE_SI_FEATURES
    #if PRE_SI_GEN == 11
        #ifdef ENABLE_PRE_SI_FEATURES
                #undef ENABLE_PRE_SI_FEATURES
        #endif
        #ifdef PRE_SI_TARGET_PLATFORM_GEN12
                #undef PRE_SI_TARGET_PLATFORM_GEN12
        #endif
    #elif PRE_SI_GEN == 12
        #define PRE_SI_TARGET_PLATFORM_GEN12
    #else
        #pragma message("ERROR:\nWrong value of PRE_SI_GEN.\nValue should be 11 or 12. \
        \n11:\n\tENABLE_PRE_SI_FEATURES = off\n\tPRE_SI_TARGET_PLATFORM_GEN12 = off\n \
        \n12:\n\tENABLE_PRE_SI_FEATURES = on\n\tPRE_SI_TARGET_PLATFORM_GEN12 = on\n")
        #error Wrong value of PRE_SI_GEN
    #endif
#else
    #define ENABLE_PRE_SI_FEATURES

    #define PRE_SI_TARGET_PLATFORM_GEN12P5 // target generation is Gen12p5 (TGL HP)
    //#define PRE_SI_TARGET_PLATFORM_GEN12 // target generation is Gen12 (TGL LP, LKF)

    #if defined (PRE_SI_TARGET_PLATFORM_GEN12P5)
        #define PRE_SI_TARGET_PLATFORM_GEN12 // assume that all Gen12 features are supported on Gen12p5
    #endif // PRE_SI_TARGET_PLATFORM_GEN12P5

#endif

#define MFX_ENABLE_GET_CM_DEVICE

#if defined(_WIN32) || defined(_WIN64)
    #define MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    #define MFX_ENABLE_VPP_HW_BLOCKING_TASK_SYNC

#define DEFAULT_WAIT_HW_TIMEOUT_MS 60000

#endif//#if defined(_WIN32) || defined(_WIN64)

#endif // #ifndef OPEN_SOURCE

// Here follows per-codec feature enable options which as of now we don't
// want to expose on build system level since they are too detailed.

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)
    #if MFX_VERSION >= 1023
        #define MFX_ENABLE_H264_REPARTITION_CHECK
        #if defined(MFX_VA_WIN)
            #define ENABLE_H264_MBFORCE_INTRA
        #endif
    #endif
    #if MFX_VERSION >= 1027
        #define MFX_ENABLE_H264_ROUNDING_OFFSET
    #endif
    #ifndef OPEN_SOURCE
        #define MFX_ENABLE_AVCE_DIRTY_RECTANGLE
        #define MFX_ENABLE_AVCE_MOVE_RECTANGLE
        #if defined(PRE_SI_TARGET_PLATFORM_GEN12P5)
            #define MFX_ENABLE_AVCE_VDENC_B_FRAMES
        #endif
        #if (MFX_VERSION >= MFX_VERSION_NEXT)
            #ifdef MFX_VA_WIN
                #define MFX_ENABLE_AVC_CUSTOM_QMATRIX
                #define MFX_ENABLE_GPU_BASED_SYNC
            #endif
        #endif
    #endif
    #if defined(MFX_ENABLE_MCTF) && defined(MFX_ENABLE_KERNELS)
        #define MFX_ENABLE_MCTF_IN_AVC
    #endif
#endif

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
    #define MFX_ENABLE_HEVCE_INTERLACE
    #define MFX_ENABLE_HEVCE_ROI
    #ifndef OPEN_SOURCE
        #define MFX_ENABLE_HEVCE_DIRTY_RECT
        #define MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION
        #if defined (MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)
            #define MFX_ENABLE_HEVCE_FADE_DETECTION
        #endif
        #define MFX_ENABLE_HEVCE_HDR_SEI
        #if (MFX_VERSION >= MFX_VERSION_NEXT)
            #define MFX_ENABLE_HEVCE_UNITS_INFO
            #ifdef MFX_VA_WIN
                #define MFX_ENABLE_HEVC_CUSTOM_QMATRIX
            #endif
        #endif
        #ifndef SKIP_EMBARGO
            #define MFX_ENABLE_HEVCE_SCC
        #endif
    #endif
#endif

    #if defined(MFX_VA_WIN)
        #define MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        #define MFX_ENABLE_VPP_HW_BLOCKING_TASK_SYNC
    #endif

    #if defined (MFX_VA_LINUX)
        #define SYNCHRONIZATION_BY_VA_MAP_BUFFER
        #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
    #endif

#if defined(MFX_VA_WIN)
    #define MFX_ENABLE_SINGLE_THREAD
#endif

#if defined(MFX_ENABLE_VPP)
    #define MFX_ENABLE_VPP_COMPOSITION
    #define MFX_ENABLE_VPP_ROTATION
    #define MFX_ENABLE_VPP_VIDEO_SIGNAL

    #if defined(OPEN_SOURCE)
        #define MFX_ENABLE_DENOISE_VIDEO_VPP
        #define MFX_ENABLE_MJPEG_WEAVE_DI_VPP
        #define MFX_ENABLE_MJPEG_ROTATE_VPP
        #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
        #define MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION
    #endif

    #if MFX_VERSION >= MFX_VERSION_NEXT
        #define MFX_ENABLE_VPP_RUNTIME_HSBC
    #endif
    //#define MFX_ENABLE_VPP_FRC
#endif

#if defined(MFX_ENABLE_ASC)
    #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
#endif

#if (MFX_VERSION >= MFX_VERSION_NEXT) && defined(MFX_ENABLE_MCTF)
    #define MFX_ENABLE_MCTF_EXT // extended MCTF interface
#endif

#if MFX_VERSION >= 1028
    #define MFX_ENABLE_RGBP
    #define MFX_ENABLE_FOURCC_RGB565
#endif

#if MFX_VERSION >= 1031
    #define MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT
#endif

#if defined(MFX_VA_LINUX)
    #if VA_CHECK_VERSION(1,3,0)
        #define MFX_ENABLE_QVBR
    #endif
#endif

#define CMAPIUPDATE

#define UMC_ENABLE_FIO_READER
#define UMC_ENABLE_VC1_SPLITTER

#if !defined(NDEBUG)
#define MFX_ENV_CFG_ENABLE
#endif

#if (defined(_WIN32) || defined(_WIN64)) && !defined(STRIP_EMBARGO)
    #define MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif

#ifdef MFX_ENABLE_USER_ENCTOOLS
    #define MFX_ENABLE_ENCTOOLS
    #if defined(_WIN32) || defined(_WIN64) 
        #define MFX_ENABLE_ENCTOOLS_LPLA
        #define MFX_ENABLE_LP_LOOKAHEAD
    #endif
#endif

#if defined(MFX_ENABLE_CPLIB) || !defined(MFX_PROTECTED_FEATURE_DISABLE)
#define MFX_ENABLE_CP
#endif

// Per component configs
#include "mfx_config_decode.h"
#include "mfx_config_encode.h"
#include "mfx_config_vpp.h"

#endif // _MFX_CONFIG_H_
