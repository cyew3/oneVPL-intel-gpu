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

#ifndef __MP3ENC_PSYCHOACOUSTIC_H__
#define __MP3ENC_PSYCHOACOUSTIC_H__

#define MAX_PPT_SHORT 47
//#define MAX_PPT_LONG  63
#define MAX_PPT_LONG  72

#define CBANDS_s      43
#define CBANDS_l      63
#define HBLOCK        513
#define BLOCK         1024
#define BLOCK_S       256
#define HBLOCK_S      129
#define SBBND_L       21
#define SBBND_S       12

#define CBANDS_12     58

#define NORM_TYPE     0
#define START_TYPE    1
#define SHORT_TYPE    2
#define STOP_TYPE     3

#define NUM_UNPRED_LINES_LONG 128

#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

#endif //__MP3ENC_PSYCHOACOUSTIC_H__
