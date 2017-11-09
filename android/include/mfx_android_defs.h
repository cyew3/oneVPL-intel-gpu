/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MFX_ANDROID_DEFS_H__
#define __MFX_ANDROID_DEFS_H__

#define mfx_generic 1
#define baytrail    2
#define merrifield  3
#define moorefield  4
#define clovertrail 5
#define fugu        6

#if (MFX_ANDROID_PLATFORM == merrifield) || (MFX_ANDROID_PLATFORM == moorefield) || (MFX_ANDROID_PLATFORM == fugu)

  #define MFX_ENABLE_H265_VIDEO_DECODE

  // H.265 optimization
  #define MFX_TARGET_OPTIMIZATION_ATOM
  #define MFX_MAKENAME_ATOM

#elif (MFX_ANDROID_PLATFORM == clovertrail)

  #define MFX_ENABLE_H265_VIDEO_DECODE
  #define MFX_TARGET_OPTIMIZATION_SSSE3
  #define MFX_MAKENAME_SSSE3
  #define MFX_EMULATE_SSSE3

#elif (MFX_ANDROID_PLATFORM == baytrail) || (MFX_ANDROID_PLATFORM == bigcore)

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

  #define MFX_ENABLE_MJPEG_VIDEO_DECODE
  #define MFX_ENABLE_MJPEG_VIDEO_DECODE_HW

  #define MFX_ENABLE_MJPEG_VIDEO_ENCODE

  #define MFX_ENABLE_VP8_VIDEO_DECODE
  #define MFX_ENABLE_VP8_VIDEO_DECODE_HW

  #define MFX_ENABLE_VP8_VIDEO_ENCODE_HW

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
