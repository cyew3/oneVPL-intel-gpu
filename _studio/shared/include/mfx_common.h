/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
//
//
//
//
*/
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

//#define SYHCHRONIZATION_BY_VA_SYNC_SURFACE

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

    #elif defined(__APPLE__)
        #undef  MFX_VA_OSX
        #define MFX_VA_OSX

        #undef  UMC_VA_OSX
        #define UMC_VA_OSX
    #endif // #if defined(LINUX32) || defined(LINUX64)
#endif // MFX_VA


#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #if defined(HEVCE_EVALUATION)
        #define MFX_ENABLE_WATERMARK
    #endif
    #if defined(HEVCD_EVALUATION)
        #define MFX_MAX_DECODE_FRAMES 1000
    #endif
#endif

//#define MFX_ENABLE_C2CPP_DEBUG          // debug the C to C++ layer only.
#ifndef MFX_ENABLE_C2CPP_DEBUG
#include "umc_structures.h"

#if !defined(ANDROID)

// h264
#define MFX_ENABLE_H264_VIDEO_DECODE
#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_DECODE
#endif
//#define MFX_ENABLE_H264_VIDEO_BSD
//#define MFX_ENABLE_H264_VIDEO_DEC

#define MFX_ENABLE_H264_VIDEO_ENCODE
#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_ENCODE
#endif
#define MFX_ENABLE_MVC_VIDEO_ENCODE
//#define MFX_ENABLE_H264_VIDEO_PAK
//#define MFX_ENABLE_H264_VIDEO_ENC
#define MFX_ENABLE_H264_VIDEO_BRC


// mpeg2
#define MFX_ENABLE_MPEG2_VIDEO_DECODE
//#define MFX_ENABLE_MPEG2_VIDEO_BSD
//#define MFX_ENABLE_MPEG2_VIDEO_DEC
#define MFX_ENABLE_MPEG2_VIDEO_ENCODE
#define MFX_ENABLE_MPEG2_VIDEO_PAK
#define MFX_ENABLE_MPEG2_VIDEO_ENC
#define MFX_ENABLE_MPEG2_VIDEO_BRC


//// vc1
#define MFX_ENABLE_VC1_VIDEO_DECODE
//#define MFX_ENABLE_VC1_VIDEO_BSD
//#define MFX_ENABLE_VC1_VIDEO_DEC
//#define MFX_ENABLE_VC1_VIDEO_BRC
//
//#define MFX_ENABLE_VC1_VIDEO_ENCODE
//#define MFX_ENABLE_VC1_VIDEO_PAK
//#define MFX_ENABLE_VC1_VIDEO_ENC
//#define MFX_ENABLE_VC1_VIDEO_BRC


// mjpeg
#define MFX_ENABLE_MJPEG_VIDEO_DECODE
#define MFX_ENABLE_MJPEG_VIDEO_ENCODE

// vp8
//#ifndef MFX_VA
//#define MFX_ENABLE_VP8_VIDEO_ENCODE
//#define MFX_ENABLE_VP8_VIDEO_ENCODE_HW
#define MFX_ENABLE_VP8_VIDEO_PAK
//#endif
//#define MFX_ENABLE_VP8_VIDEO_DECODE
//#define MFX_ENABLE_VP8_VIDEO_DECODE_HW


// vpp
#define MFX_ENABLE_DENOISE_VIDEO_VPP
#define MFX_ENABLE_IMAGE_STABILIZATION_VPP
#define MFX_ENABLE_VPP


#define MFX_ENABLE_H264_VIDEO_ENCODE_HW
#define MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW
//#define MFX_ENABLE_H264_VIDEO_ENC_HW
#define MFX_ENABLE_MVC_VIDEO_ENCODE_HW

// user plugin for decoder, encoder, and vpp
#define MFX_ENABLE_USER_DECODE
#define MFX_ENABLE_USER_ENCODE
#define MFX_ENABLE_USER_VPP

// aac
#define MFX_ENABLE_AAC_AUDIO_DECODE
#define MFX_ENABLE_AAC_AUDIO_ENCODE

//mp3
#define MFX_ENABLE_MP3_AUDIO_DECODE
// linux support
#if defined(LINUX32) || defined(LINUX64) || defined(__APPLE__)
    //#undef MFX_ENABLE_MP3_AUDIO_DECODE
    // HW limitation
    #if defined (MFX_VA)
        /** @note
         *  According to PRD for the Media SDK 2013 for Linux Server project
         * we support only the following components (in HW variants only):
         *   - H.264 decoder/encoder
         *   - Mpeg2 decoder
         *   - VPP
         */
        // h265
        //#undef MFX_ENABLE_H265_VIDEO_DECODE
        // h264
        //#undef MFX_ENABLE_H264_VIDEO_ENCODE
        #undef MFX_ENABLE_MVC_VIDEO_ENCODE
        //#undef MFX_ENABLE_H264_VIDEO_PAK
        //#undef MFX_ENABLE_H264_VIDEO_BRC

        //#undef MFX_ENABLE_H264_VIDEO_ENCODE_HW
        //#undef MFX_ENABLE_MVC_VIDEO_ENCODE_HW

        // mpeg2
        //#undef MFX_ENABLE_MPEG2_VIDEO_DECODE
        //#undef MFX_ENABLE_MPEG2_VIDEO_ENCODE
        //#undef MFX_ENABLE_MPEG2_VIDEO_PAK
        //#undef MFX_ENABLE_MPEG2_VIDEO_ENC
        //#undef MFX_ENABLE_MPEG2_VIDEO_BRC

        // mjpeg
        #undef MFX_ENABLE_MJPEG_VIDEO_DECODE
        #undef MFX_ENABLE_MJPEG_VIDEO_ENCODE

        // vpp
        //#undef MFX_ENABLE_DENOISE_VIDEO_VPP
        //#undef MFX_ENABLE_VPP

        // vc1
        //#undef MFX_ENABLE_VC1_VIDEO_DECODE

        // vp8
        #undef MFX_ENABLE_VP8_VIDEO_DECODE

        //#undef MFX_ENABLE_H265_VIDEO_ENCODE
    // SW limitation
    #else // #if defined (MFX_VA)
        #undef MFX_ENABLE_VP8_VIDEO_DECODE

        //#undef MFX_ENABLE_H265_VIDEO_ENCODE
    #endif // #if defined (MFX_VA)
#endif // #if defined(LINUX32) || defined(LINUX64)

#else // #if !defined(ANDROID)
    #include "mfx_android_defs.h"
#endif // #if !defined(ANDROID)

#define MFX_BIT_IN_KB 8*1000
#endif

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN) || defined(AS_VP8DHW_PLUGIN) || defined(AS_VP8D_PLUGIN) || defined(AS_VP8E_PLUGIN) || defined (MFX_RT)
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
    #undef MFX_ENABLE_VP8_VIDEO_ENCODE
    #undef MFX_ENABLE_VP8_VIDEO_DECODE
    #undef MFX_ENABLE_VP8_VIDEO_DECODE_HW
    #undef MFX_ENABLE_DENOISE_VIDEO_VPP
    #undef MFX_ENABLE_IMAGE_STABILIZATION_VPP
    #undef MFX_ENABLE_VPP
    #undef MFX_ENABLE_H264_VIDEO_ENCODE_HW
    #undef MFX_ENABLE_VP8_VIDEO_ENCODE_HW
    #undef MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW
    #undef MFX_ENABLE_MVC_VIDEO_ENCODE_HW
    #undef MFX_ENABLE_USER_DECODE
    #undef MFX_ENABLE_USER_ENCODE
    #undef MFX_ENABLE_AAC_AUDIO_DECODE
    #undef MFX_ENABLE_AAC_AUDIO_ENCODE
    #undef MFX_ENABLE_MP3_AUDIO_DECODE
#endif // #if defined(AS_HEVCD_PLUGIN)
#if defined(AS_HEVCD_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_DECODE
#endif
#if defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_ENCODE
#endif
#if defined(AS_VP8DHW_PLUGIN)
    #define MFX_ENABLE_VP8_VIDEO_DECODE
    #define MFX_ENABLE_VP8_VIDEO_DECODE_HW
#endif
#if defined(AS_VP8D_PLUGIN)
    #define MFX_ENABLE_VP8_VIDEO_DECODE
#endif
#if defined(AS_VP8E_PLUGIN)
    #define MFX_ENABLE_VP8_VIDEO_ENCODE
#endif

#if defined (MFX_RT)
#define MFX_ENABLE_VPP
#define MFX_ENABLE_USER_DECODE
#define MFX_ENABLE_USER_ENCODE
#endif

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
