// Copyright (c) 2014-2018 Intel Corporation
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

#ifndef PTIR_DEINTERLACER_H
#define PTIR_DEINTERLACER_H

#pragma once

#include "../api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLIPCROP(x, _min_, _max_, _min_v_, _max_v_, _def_v_) ((x) < (_min_) ? (_min_v_) : (x) > (_max_) ? (_max_v_) : (_def_v_))

#if defined(__GNUC__)
// Use GNU89 inline semantics (https://gcc.gnu.org/gcc-5/porting_to.html)
#define ATTR_INLINE __attribute__ ((gnu_inline))
#else
#define ATTR_INLINE
#endif

ATTR_INLINE __inline unsigned int EDIError(BYTE* PrvLinePixel, BYTE* NxtLinePixel, int dir);
void FilterMask_Main(Plane *s, Plane *d, unsigned int BotBase, unsigned int ybegin, unsigned int yend);
void FillBaseLinesIYUV(Frame *pSrc, Frame* pDst, int BottomLinesBaseY, int BottomLinesBaseUV);
void BilinearDeint(Frame *This, int BotBase);
void MedianDeinterlace(Frame *This, int BotBase);
void BuildLowEdgeMask_Main(Frame **frmBuffer, unsigned int frame, unsigned int BotBase);
void CalculateEdgesIYUV(Frame **frmBuffer, unsigned int frame, int BotBase);
void EdgeDirectionalIYUV_Main(Frame **frmBuffer, unsigned int curFrame, int BotBase);
void DeinterlaceBorders(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase);
void DeinterlaceBilinearFilter(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase);
void DeinterlaceMedianFilter(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase);

#ifdef __cplusplus
}
#endif 
#endif