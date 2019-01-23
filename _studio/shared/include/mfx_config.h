// Copyright (c) 2016-2019 Intel Corporation
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

#define CMAPIUPDATE

#ifndef OPEN_SOURCE
// disable additional features
//#define MFX_FADE_DETECTION_FEATURE_DISABLE
//#define MFX_PRIVATE_AVC_ENCODE_CTRL_DISABLE
//#define MFX_DEC_VIDEO_POSTPROCESS_DISABLE
//#define MFX_EXT_BRC_DISABLE
//#define MFX_CLOSED_PLATFORMS_DISABLE
//#define MFX_ADAPTIVE_PLAYBACK_DISABLE
//#define MFX_EXT_DPB_HEVC_DISABLE
//#define MFX_CAMERA_FEATURE_DISABLE
//#define MFX_FUTURE_FEATURE_DISABLE
//#define MFX_AVC_ENCODING_UNIT_DISABLE

#define MFX_EXTBUFF_GPU_HANG_ENABLE
#define MFX_UNDOCUMENTED_QUANT_MATRIX
#define MFX_UNDOCUMENTED_DUMP_FILES
#define MFX_UNDOCUMENTED_VPP_VARIANCE_REPORT
#define MFX_UNDOCUMENTED_CO_DDI
//#define MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE

//#define MFX_ENABLE_SVC_VIDEO_DECODE
#define MFX_ENABLE_VPP_SVC
#endif // #ifndef OPEN_SOURCE

#ifdef UMC_VA_LINUX
    #undef  MFX_VA
    #define MFX_VA
#endif // UMC_VA_LINUX

#if defined(_WIN32) || defined(_WIN64)
#undef MFX_DEBUG_TOOLS
#define MFX_DEBUG_TOOLS

#if defined(DEBUG) || defined(_DEBUG)
#undef  MFX_DEBUG_TOOLS // to avoid redefinition
#define MFX_DEBUG_TOOLS
#endif
#endif // #if defined(_WIN32) || defined(_WIN64)

#ifdef MFX_VA
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

#elif defined(__APPLE__)
    #undef  MFX_VA_OSX
    #define MFX_VA_OSX

    #undef  UMC_VA_OSX
    #define UMC_VA_OSX
#endif // #if defined(LINUX32) || defined(LINUX64)
#endif // MFX_VA

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #if defined(HEVCE_EVALUATION)
        #define MFX_MAX_ENCODE_FRAMES 1000
    #endif
    #if defined(HEVCD_EVALUATION)
        #define MFX_MAX_DECODE_FRAMES 1000
    #endif
#endif

#if defined(_WIN32) || defined(_WIN64) || !defined(ANDROID) && !defined(OPEN_SOURCE)
    #define MFX_ENABLE_KERNELS

    #if ((MFX_VERSION >= 1026) && (!AS_CAMERA_PLUGIN))
    #define MFX_ENABLE_MCTF
    #endif

    #define MFX_ENABLE_MPEG2_VIDEO_DECODE
    #define MFX_ENABLE_MJPEG_VIDEO_DECODE
    #define MFX_ENABLE_H264_VIDEO_DECODE
    #if defined(MFX_VA)
        #define MFX_ENABLE_H265_VIDEO_DECODE
        #define MFX_ENABLE_VP8_VIDEO_DECODE_HW
        #define MFX_ENABLE_VP9_VIDEO_DECODE_HW
    #endif
    #define MFX_ENABLE_VC1_VIDEO_DECODE

    #define MFX_ENABLE_MPEG2_VIDEO_ENCODE
    #define MFX_ENABLE_MJPEG_VIDEO_ENCODE
    #define MFX_ENABLE_H264_VIDEO_ENCODE
    #define MFX_ENABLE_H264_VIDEO_ENCODE_HW
    #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN) || defined(MFX_VA)
        #define MFX_ENABLE_H265_VIDEO_ENCODE
    #endif
#elif defined(ANDROID)
    // placeholder: #include "mfx_android_defs.h"
#else
    // mfxconfig.h is auto-generated file containing mediasdk per-component
    // enable defines
    #include "mfxconfig.h"
#endif

#if !defined(LINUX_TARGET_PLATFORM) || defined(LINUX_TARGET_PLATFORM_BDW) || defined(LINUX_TARGET_PLATFORM_CFL) || defined(LINUX_TARGET_PLATFORM_BXT) || defined(LINUX_TARGET_PLATFORM_BSW)  || defined(LINUX_TARGET_PLATFORM_ATS)
    #if defined(LINUX_TARGET_PLATFORM_BDW) || defined(LINUX_TARGET_PLATFORM_CFL) || defined(LINUX_TARGET_PLATFORM_BXT) || defined(LINUX_TARGET_PLATFORM_BSW)
        #define PRE_SI_GEN 11
    #endif

    #if !defined(ANDROID)
        // h265d
        #if defined(MFX_VA)
            #define MFX_ENABLE_VP9_VIDEO_ENCODE_HW
            #if (MFX_VERSION >= MFX_VERSION_NEXT) && (!defined(LINUX_TARGET_PLATFORM) || defined(LINUX_TARGET_PLATFORM_ATS)) // TODO: change to VAAPI version check
                #define MFX_ENABLE_AV1_VIDEO_DECODE
            #endif
        #endif

        #if defined(AS_AV1E_PLUGIN)
            #define MFX_ENABLE_AV1_VIDEO_ENCODE
        #endif

        #if defined(MFX_VA_LINUX) && (MFX_VERSION >= 1025)
            #if !defined(AS_H264LA_PLUGIN)
                #define MFX_ENABLE_MFE
            #endif
        #endif

        #define MFX_ENABLE_HW_ONLY_MPEG2_DECODER
        #define MFX_ENABLE_MPEG2_VIDEO_PAK
        #define MFX_ENABLE_MPEG2_VIDEO_ENC

        // vpp
        #define MFX_ENABLE_DENOISE_VIDEO_VPP
        #define MFX_ENABLE_IMAGE_STABILIZATION_VPP
        #define MFX_ENABLE_VPP
        #define MFX_ENABLE_MJPEG_WEAVE_DI_VPP

        //#define MFX_ENABLE_H264_VIDEO_ENC_HW
        #define MFX_ENABLE_MVC_VIDEO_ENCODE_HW
        #if defined(AS_H264LA_PLUGIN)
            #define MFX_ENABLE_LA_H264_VIDEO_HW
        #endif

        // H265 FEI plugin
        #if defined(AS_H265FEI_PLUGIN)
            #define MFX_ENABLE_H265FEI_HW
        #endif

        // user plugin for decoder, encoder, and vpp
        #define MFX_ENABLE_USER_DECODE
        #define MFX_ENABLE_USER_ENCODE
        #define MFX_ENABLE_USER_ENC
        #define MFX_ENABLE_USER_VPP

        // aac
        #define MFX_ENABLE_AAC_AUDIO_DECODE
        #define MFX_ENABLE_AAC_AUDIO_ENCODE

        //mp3
        #define MFX_ENABLE_MP3_AUDIO_DECODE

        #if defined(_WIN32) || defined(_WIN64)
            #define MFX_ENABLE_MVC_VIDEO_ENCODE
            #define MFX_ENABLE_SVC_VIDEO_ENCODE_HW
        #elif defined(__linux__)
            // Unsupported on Linux:
            #define MFX_PROTECTED_FEATURE_DISABLE
            #define MFX_CAMERA_FEATURE_DISABLE

            #if (MFX_VERSION < MFX_VERSION_NEXT)
                #define MFX_CLOSED_PLATFORMS_DISABLE
                #define MFX_EXT_DPB_HEVC_DISABLE
                #define MFX_ADAPTIVE_PLAYBACK_DISABLE
                #define MFX_FUTURE_FEATURE_DISABLE
            #endif
            #define MFX_ENABLE_MJPEG_ROTATE_VPP
            #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
        #endif
        #if defined (MFX_VA_LINUX)
            #define SYNCHRONIZATION_BY_VA_MAP_BUFFER
            #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
        #endif
    #else // #if !defined(ANDROID)
        #include "mfx_android_defs.h"

        #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE

    #endif // #if !defined(ANDROID)

    #if defined(AS_H264LA_PLUGIN)
        #undef MFX_ENABLE_MJPEG_VIDEO_DECODE
        #undef MFX_ENABLE_MJPEG_VIDEO_ENCODE
        #undef MFX_ENABLE_H264_VIDEO_FEI_ENCODE
        #if defined(__linux__)
            #undef MFX_ENABLE_VPP
        #endif
    #endif

    #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN) || defined(AS_VP8D_PLUGIN) || defined(AS_VP8E_PLUGIN) || defined(AS_VP9D_PLUGIN) || defined(AS_CAMERA_PLUGIN) || defined (MFX_RT)
        #undef MFX_ENABLE_H265_VIDEO_DECODE
        #undef MFX_ENABLE_H265_VIDEO_ENCODE
        #undef MFX_ENABLE_H264_VIDEO_DECODE
        #undef MFX_ENABLE_H264_VIDEO_ENCODE
        #undef MFX_ENABLE_MVC_VIDEO_ENCODE
        #undef MFX_ENABLE_MPEG2_VIDEO_DECODE
        #undef MFX_ENABLE_MPEG2_VIDEO_ENCODE
        #undef MFX_ENABLE_MPEG2_VIDEO_PAK
        #undef MFX_ENABLE_MPEG2_VIDEO_ENC
        #undef MFX_ENABLE_VC1_VIDEO_DECODE
        #undef MFX_ENABLE_MJPEG_VIDEO_DECODE
        #undef MFX_ENABLE_MJPEG_VIDEO_ENCODE
        #undef MFX_ENABLE_DENOISE_VIDEO_VPP
        #undef MFX_ENABLE_IMAGE_STABILIZATION_VPP
        #undef MFX_ENABLE_VPP
        #undef MFX_ENABLE_MJPEG_WEAVE_DI_VPP
        #undef MFX_ENABLE_H264_VIDEO_ENCODE_HW
        #undef MFX_ENABLE_MVC_VIDEO_ENCODE_HW
        #undef MFX_ENABLE_USER_DECODE
        #undef MFX_ENABLE_USER_ENCODE
        #undef MFX_ENABLE_AAC_AUDIO_DECODE
        #undef MFX_ENABLE_AAC_AUDIO_ENCODE
        #undef MFX_ENABLE_MP3_AUDIO_DECODE
        #undef MFX_ENABLE_H264_VIDEO_FEI_ENCODE
        #undef MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE
        #undef MFX_ENABLE_AV1_VIDEO_DECODE
        #if defined(__linux__) // for MFX_RT
            #undef MFX_ENABLE_VP8_VIDEO_DECODE
            #undef MFX_ENABLE_VP8_VIDEO_DECODE_HW
            #undef MFX_ENABLE_VP9_VIDEO_DECODE_HW
            #undef MFX_ENABLE_VP9_VIDEO_ENCODE_HW
        #endif
    #endif // #if defined(AS_HEVCD_PLUGIN)
    #if defined(AS_CAMERA_PLUGIN)
        #define MFX_ENABLE_VPP
        #define MFX_ENABLE_HW_ONLY_VPP
    #endif
    #if defined(AS_HEVCD_PLUGIN)
        #define MFX_ENABLE_H265_VIDEO_DECODE
    #endif
    #if defined(AS_HEVCE_PLUGIN)
        #define MFX_ENABLE_H265_VIDEO_ENCODE
    #endif

    #if defined(AS_AV1E_PLUGIN)
        #define MFX_ENABLE_AV1_VIDEO_ENCODE
    #endif

    #if defined(AS_VP8DHW_PLUGIN)
        #define MFX_ENABLE_VP8_VIDEO_DECODE
        #define MFX_ENABLE_VP8_VIDEO_DECODE_HW
    #endif

    #if defined(AS_VP8D_PLUGIN)
        #define MFX_ENABLE_VP8_VIDEO_DECODE
        #ifdef MFX_VA
            #define MFX_ENABLE_VP8_VIDEO_DECODE_HW
        #endif
    #endif

    #if defined(AS_VP9D_PLUGIN)
        #ifdef MFX_VA
            #define MFX_ENABLE_VP9_VIDEO_DECODE_HW
        #endif
    #endif

    #if defined(AS_VP9E_PLUGIN)
        #ifdef MFX_VA
            #define MFX_ENABLE_VP9_VIDEO_ENCODE_HW
        #endif
    #endif

    #if defined (MFX_RT)
        #define MFX_ENABLE_VPP
        #define MFX_ENABLE_USER_DECODE
        #define MFX_ENABLE_USER_ENCODE
        #define MFX_ENABLE_MJPEG_WEAVE_DI_VPP
    #endif

#else // LINUX_TARGET_PLATFORM
    #if defined(LINUX_TARGET_PLATFORM_BXTMIN) // PRE_SI_GEN == 11
        #include "mfx_common_linux_bxtmin.h"
    #else
        #error "Target platform should be specified!"
    #endif
#endif // LINUX_TARGET_PLATFORM

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

    #if defined (ENABLE_PRE_SI_FEATURES)

        #define PRE_SI_TARGET_PLATFORM_GEN12P5 // target generation is Gen12p5 (TGL HP)
        //#define PRE_SI_TARGET_PLATFORM_GEN12 // target generation is Gen12 (TGL LP, LKF)

        #if defined (PRE_SI_TARGET_PLATFORM_GEN12P5)
            #define PRE_SI_TARGET_PLATFORM_GEN12 // assume that all Gen12 features are supported on Gen12p5
        #endif // PRE_SI_TARGET_PLATFORM_GEN12P5

    #endif // ENABLE_PRE_SI_FEATURES
#endif

// Here follows per-codec feature enable options which as of now we don't
// want to expose on build system level since they are too detailed.
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)
    #ifdef OPEN_SOURCE
    #define MFX_ENABLE_H264_VIDEO_ENCODE_HW
    #endif
    #if MFX_VERSION >= 1023
        #define MFX_ENABLE_H264_REPARTITION_CHECK
        #if defined(MFX_VA_WIN)
            #define ENABLE_H264_MBFORCE_INTRA
        #endif
    #endif
    #if MFX_VERSION >= 1027
        #define MFX_ENABLE_H264_ROUNDING_OFFSET
    #endif
    #if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCODE)
        #define MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
        #define MFX_ENABLE_H264_VIDEO_FEI_PREENC
        #define MFX_ENABLE_H264_VIDEO_FEI_ENC
        #define MFX_ENABLE_H264_VIDEO_FEI_PAK
    #endif
    #ifndef OPEN_SOURCE
        #define MFX_ENABLE_AVCE_DIRTY_RECTANGLE
        #define MFX_ENABLE_AVCE_MOVE_RECTANGLE
        #if defined(PRE_SI_TARGET_PLATFORM_GEN12P5)
            #define MFX_ENABLE_AVCE_VDENC_B_FRAMES
        #endif
        #if (MFX_VERSION >= MFX_VERSION_NEXT)
            #ifdef _WIN32
                #define MFX_ENABLE_AVC_CUSTOM_QMATRIX
            #endif
        #endif
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
        #endif
        #if defined(PRE_SI_TARGET_PLATFORM_GEN12)
            #define MFX_ENABLE_HEVCE_SCC
        #endif
    #endif
#endif

#ifdef OPEN_SOURCE
#if defined(MFX_ENABLE_VP9_VIDEO_ENCODE)
    #define MFX_ENABLE_VP9_VIDEO_ENCODE_HW
#endif

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE)
#define MFX_ENABLE_VP9_VIDEO_DECODE_HW
#endif

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE)
#define MFX_ENABLE_VP8_VIDEO_DECODE_HW
#endif
#endif // #ifdef OPEN_SOURCE

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    #define MFX_ENABLE_HEVCD_SUBSET
#endif

#if !defined(PRE_SI_TARGET_PLATFORM_GEN12)
    #undef MFX_ENABLE_AV1_VIDEO_DECODE
#endif

#define MFX_ENABLE_GET_CM_DEVICE

#if defined (AS_VPP_SCD_PLUGIN) || defined (AS_ENC_SCD_PLUGIN)
    #undef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
    #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP

    #if defined (AS_VPP_SCD_PLUGIN)
        #define MFX_ENABLE_VPP_SCD_PARALLEL_COPY
    #endif //defined (AS_VPP_SCD_PLUGIN)
#endif //defined (AS_VPP_SCD_PLUGIN) || defined (AS_ENC_SCD_PLUGIN)

#if defined(_WIN32) || defined(_WIN64)
    #define MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    #define MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H264D

#if defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_MPEG2D) || \
    defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H264D)  || \
    defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_VC1D)   || \
    defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_JPEGD)  || \
    defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_VP8D)   || \
    defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H265D)  || \
    defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_VP9D)

    #define MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    #if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_AV1D) && !defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE)
        #define MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    #endif
#endif

#endif//#if defined(_WIN32) || defined(_WIN64)

#if defined(MFX_ENABLE_VPP)
    #define MFX_ENABLE_VPP_COMPOSITION
    #define MFX_ENABLE_VPP_ROTATION
    #define MFX_ENABLE_VPP_VIDEO_SIGNAL
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

#if defined(MFX_VA_LINUX)
    #if VA_CHECK_VERSION(1,3,0)
        #define MFX_ENABLE_QVBR
    #endif
#endif

#endif // _MFX_CONFIG_H_
