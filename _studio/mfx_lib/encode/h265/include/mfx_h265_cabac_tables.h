// Copyright (c) 2012-2018 Intel Corporation
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

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CABAC_TABLES_H__
#define __MFX_H265_CABAC_TABLES_H__

#include "mfx_h265_cabac.h"

namespace H265Enc {

extern const Ipp32s tab_cabacPBits[128];
extern const Ipp8u  tab_cabacTransTbl[2][128];
extern const Ipp32u tab_ctxIdxSize[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC];
extern const Ipp32u tab_ctxIdxOffset[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC];
extern const Ipp8u  tab_cabacRangeTabLps[128][4];

} // namespace

#endif // __MFX_H265_CABAC_TABLES_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
