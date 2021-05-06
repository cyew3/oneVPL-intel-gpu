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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_DC_TBL_H__
#define __UMC_VC1_COMMON_DC_TBL_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 174: Low-motion Luma DC Differential VLC Table
const extern int32_t VC1_LowMotionLumaDCDiff[];

//VC-1 Table 175: Low-motion Chroma DC Differential VLC Table
const extern int32_t VC1_LowMotionChromaDCDiff[];

//VC-1 Table 176: High-motion Luma DC Differential VLC Table
const extern int32_t VC1_HighMotionLumaDCDiff[];

//VC-1 Table 177: High-motion Chroma DC Differential VLC Table
const extern int32_t VC1_HighMotionChromaDCDiff[];

#endif //__umc_vc1_common_dc_tbl_H__
#endif //MFX_ENABLE_VC1_VIDEO_DECODE
