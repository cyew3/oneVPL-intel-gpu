// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_TTMB_TBL_H__
#define __UMC_VC1_COMMON_TTMB_TBL_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 53: High Rate (PQUANT < 5) TTMB VLC Table
const extern int32_t VC1_HighRateTTMB[];

//VC-1 Table 54: Medium Rate (5 <= PQUANT < 13) TTMB VLC Table
const extern int32_t VC1_MediumRateTTMB[];

//VC-1 Table 55: Low Rate (PQUANT >= 13) TTMB VLC Table
const extern int32_t VC1_LowRateTTMB[];

//VC-1 Table 61: High Rate (PQUANT < 5) TTBLK VLC Table
const extern int32_t VC1_HighRateTTBLK[];

//VC-1 Table 62: Medium Rate (5 =< PQUANT < 13) TTBLK VLC Table
const extern int32_t VC1_MediumRateTTBLK[];

//VC-1 Table 63: Low Rate (PQUANT >= 13) TTBLK VLC Table
const extern int32_t VC1_LowRateTTBLK[];

//VC-1 Table 64: High Rate (PQUANT < 5) SUBBLKPAT VLC Table
const extern int32_t VC1_HighRateSBP[];

//VC-1 Table 65: Medium Rate (5 =< PQUANT < 13) SUBBLKPAT VLC Table
const extern int32_t VC1_MediumRateSBP[];

//VC-1 Table 66: Low Rate (PQUANT >= 13) SUBBLKPAT VLC Table
const extern int32_t VC1_LowRateSBP[];

#endif //__umc_vc1_common_ttmb_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
