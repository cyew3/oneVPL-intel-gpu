// Copyright (c) 2004-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

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

#endif //MFX_ENABLE_H264_VIDEO_ENCODE
