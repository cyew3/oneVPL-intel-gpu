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
 * File: Params.h
 *
 * Routines for reading program parameters.
 * 
 ********************************************************************************/
#ifndef _PARAMS_H_
#define _PARAMS_H_

//Structure containing skin-detection parameters
typedef struct {
    int  numFr;                     //Number of previous frames 
    int  enableHystoDynEnh;         //rejects 20% of dark blocks
    int  minSegSize;                //minimal Segment size
    int  maxSegSize;                //maximum Segment size
} SkinDetParameters;

//compute blocksize for face detection based on resolution
int get_bsz_fd(int w, int h);

void SetDefaultParameters(SkinDetParameters* parameters);

#endif
