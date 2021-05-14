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
 * File: Dim.h
 *
 * Structures and routines for video dimensions
 * 
 ********************************************************************************/
#ifndef _DIM_H_
#define _DIM_H_
#include "Def.h"

//Dimensions related to current video
typedef struct {
    uint w;         //width  of video frames
    uint h;         //height of video frames

    uint wb;        //width  of video frames in blocks
    uint hb;        //height of video frames in blocks
    uint ubOff;     //U plane offset in blocks (hb * wb)
    uint vbOff;     //V plane offset in blocks (2 * hb * wb)
    uint frbSize;   //size of YUV Frame in blocks (wb * hb * 3)

    uint blSz;      //algorithm's blocks size (width and height)
    uint blSzPx;    //number of pixels in block  (blSz * blSz)
    uint blHNum;    //number of blocks in horizontal dimension  (w / blSz)
    uint blVNum;    //number of blocks in vertical   dimension  (h / blSz)
    uint blTotal;   //total number of blocks  (blHNum * blVNum)
    uint numFr;     //number of frames in Frame Stack
} Dim;

//Performs initialization of Dim structure (variables that depends on dimensions)
void cDimInit(Dim *dim, int w, int h, int blSz, int numFr);

#endif