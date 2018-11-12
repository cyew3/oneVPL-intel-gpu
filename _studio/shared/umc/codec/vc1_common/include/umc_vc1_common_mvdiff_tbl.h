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

#ifndef _UMC_VC1_COMMON_MVDIFF_TBL_H__
#define _UMC_VC1_COMMON_MVDIFF_TBL_H__

//VC-1 Table 247: Motion Vector Differential VLC Table 0
const extern Ipp32s VC1_Progressive_MV_Diff_tbl0[];

//VC-1 Table 248: Motion Vector Differential VLC Table 1
const extern Ipp32s VC1_Progressive_MV_Diff_tbl1[];

//VC-1 Table 249: Motion Vector Differential VLC Table 2
const extern Ipp32s VC1_Progressive_MV_Diff_tbl2[];
const extern Ipp32s VC1_Progressive_MV_Diff_tbl3[];

//VC-1 Table 250: Motion Vector Differential VLC Table 3

//Table 74: k_x and k_y specified by MVRANGE
const extern VC1MVRange VC1_MVRangeTbl[4];


#endif //_umc_vc1_common_mvdiff_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
