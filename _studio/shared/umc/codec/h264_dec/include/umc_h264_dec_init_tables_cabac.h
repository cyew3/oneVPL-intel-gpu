// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_DEC_INIT_TABLES_CABAC_H__
#define __UMC_H264_DEC_INIT_TABLES_CABAC_H__

#include "umc_h264_dec_defs_dec.h"

namespace UMC
{

extern const
uint8_t rangeTabLPS[128][4];

extern const
uint8_t transIdxMPS[128];

extern const
uint8_t transIdxLPS[128];

extern const
uint32_t NumBitsToGetTableSmall[4];

extern const
uint8_t NumBitsToGetTbl[512];

} // namespace UMC

#endif //__UMC_H264_DEC_INIT_TABLES_CABAC_H__
#endif // MFX_ENABLE_H264_VIDEO_DECODE
