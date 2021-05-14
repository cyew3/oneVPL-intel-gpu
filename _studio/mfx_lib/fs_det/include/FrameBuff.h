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
 * File: FrameBuff.h
 *
 * Functions for manipulating frame buffer
 * 
 ********************************************************************************/
#ifndef _FRAME_BUFF_H_
#define _FRAME_BUFF_H_
#include "Dim.h"

//Element of buffer of frames
typedef struct {
    BYTE *frameY; //full frame
    BYTE *frameU; //full frame
    BYTE *frameV; //full frame
    int w;
    int h;
    int p;
    int pc;
    int nv12;
    BYTE *bSAD;	 //block SAD
} FrameBuffElementPtr;


int InitFrameBuffPtr(FrameBuffElementPtr **frameBuff, Dim* dim, int mode, int cc);
void DeinitFrameBuffPtr(FrameBuffElementPtr **frameBuff, Dim* dim);
void SwapFrameBuffPtrStackPointers(FrameBuffElementPtr **p, int num);


#endif
