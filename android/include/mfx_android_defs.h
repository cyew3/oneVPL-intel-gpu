/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MFX_ANDROID_DEFS_H__
#define __MFX_ANDROID_DEFS_H__

#define mfx_generic 1
#define baytrail    2
#define merrifield  3

#if MFX_ANDROID_PLATFORM == merrifield

  #define MFX_ENABLE_H265_VIDEO_DECODE

  // H.265 optimization
  #define MFX_MAKENAME_ATOM

  #define MFX_OMX_PVR_RENDER

#elif MFX_ANDROID_PLATFORM == baytrail

  #define MFX_ENABLE_H264_VIDEO_DECODE
  #define MFX_ENABLE_H265_VIDEO_DECODE

  #define MFX_ENABLE_H264_VIDEO_ENCODE
  #define MFX_ENABLE_H264_VIDEO_BRC
  #define MFX_ENABLE_H264_VIDEO_ENCODE_HW
  #define MFX_ENABLE_MVC_VIDEO_ENCODE_HW

  #define MFX_ENABLE_VC1_VIDEO_DECODE

  #define MFX_ENABLE_VPP

  // H.265 optimization
  #define MFX_MAKENAME_ATOM

#else

  #error "undefined platform"

#endif

#endif // #ifndef __MFX_ANDROID_DEFS_H__
