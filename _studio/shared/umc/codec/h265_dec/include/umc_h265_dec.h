/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_H__
#define __UMC_H265_DEC_H__

#include "umc_video_decoder.h"
#include "umc_media_buffer.h"

namespace UMC_HEVC_DECODER
{

enum
{
    H265_LEVEL_1    = 10,
    H265_LEVEL_2    = 20,
    H265_LEVEL_21   = 21,

    H265_LEVEL_3    = 30,
    H265_LEVEL_31   = 31,

    H265_LEVEL_4    = 40,
    H265_LEVEL_41   = 41,

    H265_LEVEL_5    = 50,
    H265_LEVEL_51   = 51,
    H265_LEVEL_52   = 52,

    H265_LEVEL_6    = 60,
    H265_LEVEL_61   = 61,
    H265_LEVEL_62   = 62,
    H265_LEVEL_MAX  = 62
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_DEC_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
