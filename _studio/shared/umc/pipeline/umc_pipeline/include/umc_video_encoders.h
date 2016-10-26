//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VIDEO_ENCODERS_H__
#define __UMC_VIDEO_ENCODERS_H__

#ifdef UMC_ENABLE_MPEG2_VIDEO_ENCODER
#include "umc_mpeg2_video_encoder.h"
#endif

#ifdef UMC_ENABLE_MPEG4_VIDEO_ENCODER
#include "umc_mpeg4_video_encoder.h"
#endif

#ifdef UMC_ENABLE_H264_VIDEO_ENCODER
#include "umc_h264_video_encoder.h"
#endif

#ifdef UMC_ENABLE_DV_VIDEO_ENCODER
#include "umc_dv_video_encoder.h"

  #ifdef UMC_ENABLE_DV50_VIDEO_ENCODER
  #include "umc_dv50_video_encoder.h"
  #endif

#endif

//#ifdef UMC_ENABLE_MJPEG_VIDEO_ENCODER
//#include "umc_mjpeg_video_encoder.h"
//#endif

#ifdef UMC_ENABLE_H263_VIDEO_ENCODER
#include "umc_h263_video_encoder.h"
#endif

#ifdef UMC_ENABLE_H261_VIDEO_ENCODER
#include "umc_h261_video_encoder.h"
#endif

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
#include "umc_vc1_video_encoder.h"
#endif

#ifdef UMC_ENABLE_DVHD_VIDEO_ENCODER
#include "umc_dv100_video_encoder.h"
#endif

#ifdef UMC_ENABLE_AVS_VIDEO_ENCODER
#include "umc_avs_enc.h"
#endif

#endif // __UMC_VIDEO_ENCODERS_H__
