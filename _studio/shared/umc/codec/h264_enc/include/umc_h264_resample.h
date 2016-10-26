//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2012 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#ifndef UMC_H264_RESAMPLE_H__
#define UMC_H264_RESAMPLE_H__

#include "umc_h264_video_encoder.h"

//#define PIXTYPE Ipp8u
//#define COEFFSTYPE Ipp16s
//#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

namespace UMC_H264_ENCODER
{
void LoadMBInfoFromRefLayer(//H264SegmentDecoderMultiThreaded * sd
                                              H264CoreEncoder_8u16s *curEnc,
                                              H264CoreEncoder_8u16s *refEnc);


Status DownscaleSurface(sH264EncoderFrame_8u16s* to, sH264EncoderFrame_8u16s* from, Ipp32u offsets[4]);

} // namespace UMC_H264_ENCODER


//#undef H264ENC_MAKE_NAME
//#undef COEFFSTYPE
//#undef PIXTYPE

#endif // UMC_H264_RESAMPLE_H__

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
