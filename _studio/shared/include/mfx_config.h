//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2017 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_CONFIG_H_
#define _MFX_CONFIG_H_

#include "mfxdefs.h"

#define CMAPIUPDATE
#define MFX_ENABLE_VPP_COMPOSITION
//#define MFX_ENABLE_VPP_FRC
#define MFX_ENABLE_VPP_ROTATION
#define MFX_ENABLE_VPP_VIDEO_SIGNAL

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
#define MFX_ENABLE_AVCE_DIRTY_RECTANGLE
#define MFX_ENABLE_AVCE_MOVE_RECTANGLE
//#define MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE

#undef MFX_ENABLE_H264_REPARTITION_CHECK

//#define MFX_ENABLE_SVC_VIDEO_DECODE
#define MFX_ENABLE_VPP_SVC
#define MFX_ENABLE_HEVCE_HDR_SEI
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
        #undef  MFX_VA_LINUX
        #define MFX_VA_LINUX

        #undef  UMC_VA_LINUX
        #define UMC_VA_LINUX

        /* Android and Linux uses one video acceleration library: LibVA, but
         * it is possible that their versions are different (especially during
         * development). To simplify code development MFX_VA_ANDROID macro is introduced.
         */
        #if defined(ANDROID)
            #define MFX_VA_ANDROID
        #endif // #if defined(ANDROID)

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

#if !defined(LINUX_TARGET_PLATFORM)
    #if !defined(ANDROID)
        // h264d
        #define MFX_ENABLE_H264_VIDEO_DECODE
        // h265d
        #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
            #define MFX_ENABLE_H265_VIDEO_DECODE
        #endif

        //h264e
        #define MFX_ENABLE_H264_VIDEO_ENCODE
        #if defined(MFX_VA_WIN) && MFX_VERSION >= 1023
            #define ENABLE_H264_MBFORCE_INTRA
        #endif

        //h265e
        #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
            #define MFX_ENABLE_H265_VIDEO_ENCODE
        #endif
        #define MFX_ENABLE_MVC_VIDEO_ENCODE
        //#define MFX_ENABLE_H264_VIDEO_PAK
        //#define MFX_ENABLE_H264_VIDEO_ENC
        #if defined(LINUX64)
            #define MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
            #define MFX_ENABLE_H264_VIDEO_FEI_PREENC
            #define MFX_ENABLE_H264_VIDEO_FEI_ENC
            #define MFX_ENABLE_H264_VIDEO_FEI_PAK

            //hevc FEI ENCODE
            #if defined(AS_HEVC_FEI_ENCODE_PLUGIN) && MFX_VERSION >= 1024
                #define MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE
            #endif
        #endif
        // mpeg2
        #define MFX_ENABLE_MPEG2_VIDEO_DECODE
        #define MFX_ENABLE_HW_ONLY_MPEG2_DECODER
        #define MFX_ENABLE_MPEG2_VIDEO_ENCODE
        #define MFX_ENABLE_MPEG2_VIDEO_PAK
        #define MFX_ENABLE_MPEG2_VIDEO_ENC

        //// vc1
        #define MFX_ENABLE_VC1_VIDEO_DECODE

        // mjpeg
        #define MFX_ENABLE_MJPEG_VIDEO_DECODE
        #define MFX_ENABLE_MJPEG_VIDEO_ENCODE

        // vpp
        #define MFX_ENABLE_DENOISE_VIDEO_VPP
        #define MFX_ENABLE_IMAGE_STABILIZATION_VPP
        #define MFX_ENABLE_VPP
        #define MFX_ENABLE_MJPEG_WEAVE_DI_VPP

        #define MFX_ENABLE_H264_VIDEO_ENCODE_HW
        #define MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW
        //#define MFX_ENABLE_H264_VIDEO_ENC_HW
        #define MFX_ENABLE_MVC_VIDEO_ENCODE_HW
        #define MFX_ENABLE_SVC_VIDEO_ENCODE_HW
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

    #else // #if !defined(ANDROID)
        #include "mfx_android_defs.h"

        #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE

    #endif // #if !defined(ANDROID)

    #if defined (MFX_VA_LINUX)
        #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
    #endif

    #if defined(AS_H264LA_PLUGIN)
        #undef MFX_ENABLE_MJPEG_VIDEO_DECODE
        #undef MFX_ENABLE_MJPEG_VIDEO_ENCODE
        #undef MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
        #undef MFX_ENABLE_H264_VIDEO_FEI_PREENC
        #undef MFX_ENABLE_H264_VIDEO_FEI_ENC
        #undef MFX_ENABLE_H264_VIDEO_FEI_PAK
    #endif

    #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN) || defined(AS_VP8D_PLUGIN) || defined(AS_VP8E_PLUGIN) || defined(AS_VP9D_PLUGIN) || defined(AS_CAMERA_PLUGIN) || defined (MFX_RT) || (defined(AS_HEVC_FEI_ENCODE_PLUGIN) && MFX_VERSION >= 1024)
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
        #undef MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW
        #undef MFX_ENABLE_MVC_VIDEO_ENCODE_HW
        #undef MFX_ENABLE_USER_DECODE
        #undef MFX_ENABLE_USER_ENCODE
        #undef MFX_ENABLE_AAC_AUDIO_DECODE
        #undef MFX_ENABLE_AAC_AUDIO_ENCODE
        #undef MFX_ENABLE_MP3_AUDIO_DECODE
        #undef MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
        #undef MFX_ENABLE_H264_VIDEO_FEI_PREENC
        #undef MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE
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
        #if !defined(__APPLE__)
            #define MFX_ENABLE_CM
        #endif
    #endif
    #if defined(AS_HEVC_FEI_ENCODE_PLUGIN) && MFX_VERSION >= 1024
        #define MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE
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
        //#define MFX_ENABLE_VP9_VIDEO_DECODE
        #ifdef MFX_VA
            #define MFX_ENABLE_VP9_VIDEO_DECODE_HW
        #endif
    #endif

    #if defined (MFX_RT)
        #define MFX_ENABLE_VPP
        #define MFX_ENABLE_USER_DECODE
        #define MFX_ENABLE_USER_ENCODE
        #define MFX_ENABLE_MJPEG_WEAVE_DI_VPP
    #endif

#else // LINUX_TARGET_PLATFORM
    #if defined(LINUX_TARGET_PLATFORM_CFL)      // PRE_SI_GEN == 9
        #include "mfx_common_linux_cfl.h"
    #elif defined(LINUX_TARGET_PLATFORM_BXTMIN) // PRE_SI_GEN == 9
        #include "mfx_common_linux_bxtmin.h"
    #elif defined(LINUX_TARGET_PLATFORM_BXT)    // PRE_SI_GEN == 9
        #include "mfx_common_linux_bxt.h"
    #elif defined(LINUX_TARGET_PLATFORM_BSW)
        #include "mfx_common_linux_bsw.h"
    #elif defined(LINUX_TARGET_PLATFORM_BDW)    // PRE_SI_GEN == 9
        #include "mfx_common_linux_bdw.h"
    #elif defined(LINUX_TARGET_PLATFORM_TBD)
        #include "mfx_common_lnx_tbd.h"
    #else
        #error "Target platform should be specified!"
    #endif
#endif // LINUX_TARGET_PLATFORM

#if defined (PRE_SI_GEN)
    #define ENABLE_PRE_SI_FEATURES
    #if PRE_SI_GEN == 9
        #ifdef ENABLE_PRE_SI_FEATURES
             #undef ENABLE_PRE_SI_FEATURES
        #endif
        #ifdef PRE_SI_TARGET_PLATFORM_GEN10
             #undef PRE_SI_TARGET_PLATFORM_GEN10
        #endif
        #ifdef PRE_SI_TARGET_PLATFORM_GEN11
             #undef PRE_SI_TARGET_PLATFORM_GEN11
        #endif
    #elif PRE_SI_GEN == 10
        #define PRE_SI_TARGET_PLATFORM_GEN10
    #elif PRE_SI_GEN == 11
        #define PRE_SI_TARGET_PLATFORM_GEN11
        #define PRE_SI_TARGET_PLATFORM_GEN10 // assume that all Gen10 features are supported on Gen11
    #elif PRE_SI_GEN == 12
        #define PRE_SI_TARGET_PLATFORM_GEN12
        #define PRE_SI_TARGET_PLATFORM_GEN11
        #define PRE_SI_TARGET_PLATFORM_GEN10 // assume that all Gen10\Ge11 features are supported on Gen12
    #else
        #pragma message("ERROR:\nWrong value of PRE_SI_GEN.\nValue should be 9, 10, 11 or 12. \
        \n9:\n\tENABLE_PRE_SI_FEATURES = off\n\tPRE_SI_TARGET_PLATFORM_GEN10 = off\n\tPRE_SI_TARGET_PLATFORM_GEN11 = off\n\tPRE_SI_TARGET_PLATFORM_GEN12 = off\n \
        \n10:\n\tENABLE_PRE_SI_FEATURES = on\n\tPRE_SI_TARGET_PLATFORM_GEN10 = on\n \
        \n11:\n\tENABLE_PRE_SI_FEATURES = on\n\tPRE_SI_TARGET_PLATFORM_GEN11 = on\n \
        \n12:\n\tENABLE_PRE_SI_FEATURES = on\n\tPRE_SI_TARGET_PLATFORM_GEN12 = on\n")
        #error Wrong value of PRE_SI_GEN
    #endif
#else
    #define ENABLE_PRE_SI_FEATURES

    #if defined (ENABLE_PRE_SI_FEATURES)

        #define PRE_SI_TARGET_PLATFORM_GEN12P5 // target generation is Gen12p5 (TGL HP)
        //#define PRE_SI_TARGET_PLATFORM_GEN12 // target generation is Gen12 (TGL LP, LKF)
        //#define PRE_SI_TARGET_PLATFORM_GEN11 // target generation is Gen11 (ICL, CNL-H, CNX-G)

        #if defined (PRE_SI_TARGET_PLATFORM_GEN12P5)
            #define PRE_SI_TARGET_PLATFORM_GEN12 // assume that all Gen12 features are supported on Gen12p5
        #endif // PRE_SI_TARGET_PLATFORM_GEN12P5

        #if defined (PRE_SI_TARGET_PLATFORM_GEN12)
            #define PRE_SI_TARGET_PLATFORM_GEN11 // assume that all Gen11 features are supported on Gen12
        #endif // PRE_SI_TARGET_PLATFORM_GEN12

        #if defined (PRE_SI_TARGET_PLATFORM_GEN11)
           #define PRE_SI_TARGET_PLATFORM_GEN10 // assume that all Gen10 features are supported on Gen11
        #endif // PRE_SI_TARGET_PLATFORM_GEN11

        //#define PRE_SI_TARGET_PLATFORM_GEN10 // target generation is Gen10 (CNL)

    #endif // ENABLE_PRE_SI_FEATURES
#endif

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)
    #define MFX_ENABLE_HEVCE_ROI
    #define MFX_ENABLE_HEVCE_DIRTY_RECT
    #define MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION

    #if (MFX_VERSION >= MFX_VERSION_NEXT)
        #define MFX_ENABLE_HEVCE_UNITS_INFO
    #endif

    #if defined (PRE_SI_TARGET_PLATFORM_GEN11)
        #if defined (MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)
            #define MFX_ENABLE_HEVCE_FADE_DETECTION
        #endif // MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION
    #endif // PRE_SI_TARGET_PLATFORM_GEN11
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    #define MFX_ENABLE_HEVCD_WPP
#endif // PRE_SI_TARGET_PLATFORM_GEN12

#define MFX_ENABLE_HEVCE_INTERLACE
#define MFX_ENABLE_GET_CM_DEVICE

#if defined (AS_VPP_SCD_PLUGIN) || defined (AS_ENC_SCD_PLUGIN)
    #undef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
    #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP

    #if defined (AS_VPP_SCD_PLUGIN)
        #define MFX_ENABLE_VPP_SCD_PARALLEL_COPY
    #endif //defined (AS_VPP_SCD_PLUGIN)
#endif //defined (AS_VPP_SCD_PLUGIN) || defined (AS_ENC_SCD_PLUGIN)

#endif // _MFX_CONFIG_H_
