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

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <stddef.h>
#endif

#ifdef UMC_VA_LINUX
    #undef  MFX_VA
    #define MFX_VA
#endif // UMC_VA_LINUX

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

        #if !defined( WIN_TRESHOLD_MOBILE )
            #define MFX_D3D9_ENABLED
        #endif

    #elif defined(__APPLE__)
        #undef  MFX_VA_OSX
        #define MFX_VA_OSX

        #undef  UMC_VA_OSX
        #define UMC_VA_OSX
    #endif // #if defined(LINUX32) || defined(LINUX64)
#endif // MFX_VA

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #if defined(HEVCE_EVALUATION)
        //#define MFX_ENABLE_WATERMARK
        #define MFX_MAX_ENCODE_FRAMES 1000
    #endif
    #if defined(HEVCD_EVALUATION)
        #define MFX_MAX_DECODE_FRAMES 1000
    #endif
#endif

//#define MFX_ENABLE_C2CPP_DEBUG          // debug the C to C++ layer only.
#ifndef MFX_ENABLE_C2CPP_DEBUG
#include "umc_structures.h"

#define MFX_BIT_IN_KB 8*1000
#endif

#if !defined(LINUX_TARGET_PLATFORM)
    #if !defined(ANDROID)
        // h264
        #define MFX_ENABLE_H264_VIDEO_DECODE
        #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
            #define MFX_ENABLE_H265_VIDEO_DECODE
        #endif

        #define MFX_ENABLE_H264_VIDEO_ENCODE
        #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
            #define MFX_ENABLE_H265_VIDEO_ENCODE
        #endif
        #define MFX_ENABLE_MVC_VIDEO_ENCODE
        //#define MFX_ENABLE_H264_VIDEO_PAK
        //#define MFX_ENABLE_H264_VIDEO_ENC
        #define MFX_ENABLE_H264_VIDEO_BRC
        #if defined(LINUX64)
            #define MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
            #define MFX_ENABLE_H264_VIDEO_FEI_PREENC
            #define MFX_ENABLE_H264_VIDEO_FEI_ENC
            #define MFX_ENABLE_H264_VIDEO_FEI_PAK
        #endif
        // mpeg2
        #define MFX_ENABLE_MPEG2_VIDEO_DECODE
        #define MFX_ENABLE_HW_ONLY_MPEG2_DECODER
        #define MFX_ENABLE_MPEG2_VIDEO_ENCODE
        #define MFX_ENABLE_MPEG2_VIDEO_PAK
        #define MFX_ENABLE_MPEG2_VIDEO_ENC
        #define MFX_ENABLE_MPEG2_VIDEO_BRC

        //// vc1
        #define MFX_ENABLE_VC1_VIDEO_DECODE
        //#define MFX_ENABLE_VC1_VIDEO_BRC
        //
        //#define MFX_ENABLE_VC1_VIDEO_ENCODE
        //#define MFX_ENABLE_VC1_VIDEO_PAK
        //#define MFX_ENABLE_VC1_VIDEO_ENC
        //#define MFX_ENABLE_VC1_VIDEO_BRC

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
        //#define SYNCHRONIZATION_BY_VA_MAP_BUFFER
    #endif

    #if defined(AS_H264LA_PLUGIN)
        #undef MFX_ENABLE_MJPEG_VIDEO_DECODE
        #undef MFX_ENABLE_MJPEG_VIDEO_ENCODE
        #undef MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
        #undef MFX_ENABLE_H264_VIDEO_FEI_PREENC
        #undef MFX_ENABLE_H264_VIDEO_FEI_ENC
        #undef MFX_ENABLE_H264_VIDEO_FEI_PAK
    #endif

    #if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN) || defined(AS_VP8D_PLUGIN) || defined(AS_VP8E_PLUGIN) || defined(AS_VP9D_PLUGIN) || defined(AS_CAMERA_PLUGIN) || defined (MFX_RT)
        #undef MFX_ENABLE_H265_VIDEO_DECODE
        #undef MFX_ENABLE_H265_VIDEO_ENCODE
        #undef MFX_ENABLE_H264_VIDEO_DECODE
        #undef MFX_ENABLE_H264_VIDEO_ENCODE
        #undef MFX_ENABLE_MVC_VIDEO_ENCODE
        #undef MFX_ENABLE_H264_VIDEO_BRC
        #undef MFX_ENABLE_MPEG2_VIDEO_DECODE
        #undef MFX_ENABLE_MPEG2_VIDEO_ENCODE
        #undef MFX_ENABLE_MPEG2_VIDEO_PAK
        #undef MFX_ENABLE_MPEG2_VIDEO_ENC
        #undef MFX_ENABLE_MPEG2_VIDEO_BRC
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
        //#define MFX_ENABLE_H265_PAQ
        #if !defined(__APPLE__)
            #define MFX_ENABLE_CM
        #endif
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
    #if defined(LINUX_TARGET_PLATFORM_BXTMIN)
        #include "mfx_common_linux_bxtmin.h"
    #elif defined(LINUX_TARGET_PLATFORM_BXT)
        #include "mfx_common_linux_bxt.h"
    #elif defined(LINUX_TARGET_PLATFORM_BSW)
        #include "mfx_common_linux_bsw.h"
    #elif defined(LINUX_TARGET_PLATFORM_BDW)
        #include "mfx_common_linux_bdw.h"
    #elif defined(LINUX_TARGET_PLATFORM_TBD)
        #include "mfx_common_lnx_tbd.h"
    #else
        #error "Target platform should be specified!"
    #endif
#endif // LINUX_TARGET_PLATFORM

class MfxException
{
public:
    MfxException (mfxStatus st, const char *msg = NULL)
        : m_message (msg)
        , m_status (st)
    {
    }

    const char * GetMessage() const
    {
        return m_message;
    }

    mfxStatus GetStatus() const
    {
        return m_status;
    }

protected:
    const char *m_message;
    mfxStatus m_status;
};

#define MFX_AUTO_ASYNC_DEPTH_VALUE  5
#define MFX_MAX_ASYNC_DEPTH_VALUE   15

#endif //_MFX_COMMON_H_
