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

#if defined _OPENMP
#include "omp.h"
#endif // _OPENMP
#if defined _WIN32
#include <windows.h>
#undef true
#undef false
#endif

#include <math.h>
#include <string.h>
#include "vm_strings.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_core_enc.h"
#include "umc_video_data.h"
#include "umc_video_processing.h"
#include "umc_h264_to_ipp.h"

#ifdef FRAME_QP_FROM_FILE
#include <list>
#endif


#include "umc_h264_tables.h"
#include "umc_h264_wrappers.h"

namespace UMC_H264_ENCODER
{
#ifdef FRAME_QP_FROM_FILE
static std::list<char> frame_type;
static std::list<int> frame_qp;
#endif

#define PIXBITS 8
#include "umc_h264_gen_enc_tmpl.cpp.h"
#undef PIXBITS

#if defined BITDEPTH_9_12

#define PIXBITS 16
#include "umc_h264_gen_enc_tmpl.cpp.h"
#undef PIXBITS

#endif // BITDEPTH_9_12

} //namespace UMC_H264_ENCODER

#endif //MFX_ENABLE_H264_VIDEO_ENCODE


