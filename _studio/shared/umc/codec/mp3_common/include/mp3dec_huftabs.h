// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __MP3DEC_HUFTABS_H__
#define __MP3DEC_HUFTABS_H__

#include "ippdc.h"
#include "ippac.h"

#ifdef __cplusplus
extern "C" {
#endif

extern Ipp32s mp3dec_VLCShifts[];
extern Ipp32s mp3dec_VLCOffsets[];
extern Ipp32s mp3dec_VLCTypes[];
extern Ipp32s mp3dec_VLCTableSizes[];
extern Ipp32s mp3dec_VLCNumSubTables[];
extern Ipp32s *mp3dec_VLCSubTablesSizes[];
extern IppsVLCTable_32s *mp3dec_VLCBooks[];

extern Ipp32s mp3idec_VLCShifts[];
extern Ipp32s mp3idec_VLCOffsets[];
extern Ipp32s mp3idec_VLCTypes[];
extern Ipp32s mp3idec_VLCTableSizes[];
extern Ipp32s mp3idec_VLCNumSubTables[];
extern Ipp32s *mp3idec_VLCSubTablesSizes[];
extern IppsVLCTable_32s *mp3idec_VLCBooks[];

#ifdef __cplusplus
}
#endif

#endif //__HUFTABS_H__
