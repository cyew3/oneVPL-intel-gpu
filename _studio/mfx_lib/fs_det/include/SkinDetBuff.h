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
/********************************************************************************
 * 
 * File: SkinDetBuff.h
 *
 * Buffers and buffer-related routines for skin detection
 * 
 ********************************************************************************/
#ifndef _SKIN_DET_BUFF_H_
#define _SKIN_DET_BUFF_H_

#include "Def.h"

typedef struct {
	BYTE *pInYUV;		//YUV frame buffer
	BYTE *pYCbCr;		//YCrCb frame buffer
} SkinDetBuff;

typedef struct {
    int sz3;    //w*h + w*h + w*h = (3*w*h)
    int szb1_5; //(w/bs)*(h/bs) + ((w/bs)/2)*((h/bs)/2) + ((w/bs)/2)*((h/bs)/2) = (3*(w/bs)*(h/bs))/2
    int szb3;   //(w/bs)*(h/bs) + (w/bs)*(h/bs) + (w/bs)*(h/bs) = (3*(w/bs)*(h/bs))
} Sizes;

//Get buffer sizes
void GetBufferSizes(Sizes *sz, int w, int h,  int bs);

//Allocates memory buffers
int AllocateBuffers(SkinDetBuff *sdb, Sizes *sz);

//De-allocates memory buffers
void FreeBuffers(SkinDetBuff *sdb);

#endif