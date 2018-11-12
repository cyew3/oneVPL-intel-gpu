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

#ifndef PTIR_PICFFRMTCONV_H
#define PTIR_PICFFRMTCONV_H

#include "../api.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIAN_3(a,b,c)    ((a > b) ?            \
    ((b > c) ? (b) : ((a > c) ? (c) : (a))) :    \
    ((a > c) ? (a) : ((b > c) ? (c) : (b))))

#define MABS(v) (tmp=(v),((tmp)^(tmp>>31)))
#define ABS(a)  (((a) < 0) ? (-(a)) : (a))
#define MSQUARED(v) ((v) * (v))


void ReSample(Frame *frmOut, Frame *frmIn);

void Filter_CCIR601(Plane *plaOut, Plane *plaIn, int *data);
#ifdef __cplusplus
}
#endif 
#endif