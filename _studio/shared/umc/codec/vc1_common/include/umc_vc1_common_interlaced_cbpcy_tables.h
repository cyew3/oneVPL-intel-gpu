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

#ifndef __UMC_VC1_COMMON_INTERLACE_CBPCY_TABLES_H__
#define __UMC_VC1_COMMON_INTERLACE_CBPCY_TABLES_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 125: interlaced CBPCY table 0
const extern int32_t VC1_InterlacedCBPCYTable0[];

//VC-1 Table 126: interlaced CBPCY table 1
const extern int32_t VC1_InterlacedCBPCYTable1[];

//VC-1 Table 127: interlaced CBPCY table 2
const extern int32_t VC1_InterlacedCBPCYTable2[];

//VC-1 Table 128: interlaced CBPCY table 3
const extern int32_t VC1_InterlacedCBPCYTable3[];

//VC-1 Table 129: interlaced CBPCY table 4
const extern int32_t VC1_InterlacedCBPCYTable4[];

//VC-1 Table 130: interlaced CBPCY table 5
const extern int32_t VC1_InterlacedCBPCYTable5[];

//VC-1 Table 131: interlaced CBPCY table 6
const extern int32_t VC1_InterlacedCBPCYTable6[];

//VC-1 Table 132: interlaced CBPCY table 7
const extern int32_t VC1_InterlacedCBPCYTable7[];

#endif //__VC1_DEC_INTERLACE_CBPCY_TABLES_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
