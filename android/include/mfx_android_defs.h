/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MFX_ANDROID_DEFS_H__
#define __MFX_ANDROID_DEFS_H__

#define gmin         1 //CHT
#define broxton      2
#define kabylake     3
#define cannonlake   4
#define icelakeu     5
#define tigerlake    6
#define elkhartlake  7

#if (MFX_ANDROID_PLATFORM == gmin) || \
    (MFX_ANDROID_PLATFORM == broxton) || \
    (MFX_ANDROID_PLATFORM == kabylake) || \
    (MFX_ANDROID_PLATFORM == cannonlake) || \
    (MFX_ANDROID_PLATFORM == icelakeu) || \
    (MFX_ANDROID_PLATFORM == tigerlake) || \
    (MFX_ANDROID_PLATFORM == elkhartlake)

    #define PRE_SI_GEN 11

    #ifdef MFX_VA

        #define MFX_ENABLE_VC1_VIDEO_DECODE

        #define MFX_ENABLE_MPEG2_VIDEO_DECODE

        #define MFX_ENABLE_H265_VIDEO_DECODE
        #define MFX_TARGET_OPTIMIZATION_ATOM
        #define MFX_MAKENAME_ATOM

        #define MFX_ENABLE_H264_VIDEO_DECODE

        #define MFX_ENABLE_H264_VIDEO_ENCODE
        #define MFX_ENABLE_H264_VIDEO_BRC
        #define MFX_ENABLE_H264_VIDEO_ENCODE_HW
        #define MFX_ENABLE_MVC_VIDEO_ENCODE_HW
        #define LOWPOWERENCODE_AVC

        #define MFX_ENABLE_H265_VIDEO_ENCODE

        //#define MFX_ENABLE_VP9_VIDEO_ENCODE_HW

        #define MFX_ENABLE_MJPEG_VIDEO_DECODE
        #define MFX_ENABLE_MJPEG_VIDEO_DECODE_HW

        #define MFX_ENABLE_MJPEG_VIDEO_ENCODE

        #define MFX_ENABLE_VP8_VIDEO_DECODE
        #define MFX_ENABLE_VP8_VIDEO_DECODE_HW

        #define MFX_ENABLE_VP9_VIDEO_DECODE
        #define MFX_ENABLE_VP9_VIDEO_DECODE_HW

        #define MFX_ENABLE_VPP

        #define MFX_ENABLE_USER_ENCODE

    #else // MFX_VA

        #define MFX_ENABLE_H265_VIDEO_DECODE
        #define MFX_TARGET_OPTIMIZATION_ATOM
        #define MFX_MAKENAME_ATOM

    #endif // MFX_VA

#else
    #error "undefined platform"
#endif

#endif // #ifndef __MFX_ANDROID_DEFS_H__
