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

#ifndef __UMC_VC1_COMMON_MV_BLOCK_PATTERN_TABLES_H__
#define __UMC_VC1_COMMON_MV_BLOCK_PATTERN_TABLES_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 117: 4MV block pattern table 0
const extern Ipp32s VC1_MV4BlockPatternTable0[];

//VC-1 Table 118: 4MV block pattern table 1
const extern Ipp32s VC1_MV4BlockPatternTable1[];

//VC-1 Table 119: 4MV block pattern table 2
const extern Ipp32s VC1_MV4BlockPatternTable2[];

//VC-1 Table 120: 4MV block pattern table 3
const extern Ipp32s VC1_MV4BlockPatternTable3[];

//VC-1 Table 121: 2MV block pattern table 0
const extern Ipp32s VC1_MV2BlockPatternTable0[];

//VC-1 Table 122: 2MV block pattern table 1
const extern Ipp32s VC1_MV2BlockPatternTable1[];

//VC-1 Table 123: 2MV block pattern table 2
const extern Ipp32s VC1_MV2BlockPatternTable2[];

//VC-1 Table 124: 2MV block pattern table 3
const extern Ipp32s VC1_MV2BlockPatternTable3[];

#endif //__umc_vc1_common_mv_block_pattern_tables_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
