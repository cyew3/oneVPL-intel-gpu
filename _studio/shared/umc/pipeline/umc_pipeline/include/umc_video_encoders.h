// Copyright (c) 2003-2018 Intel Corporation
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
