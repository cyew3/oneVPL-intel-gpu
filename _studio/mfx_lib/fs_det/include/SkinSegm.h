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
 * File: SkinSegm.h
 *
 * Functions for skin detection-related segmentation
 * 
 ********************************************************************************/
#ifndef _SKIN_SEGM_H_
#define _SKIN_SEGM_H_

#include "Def.h"

//segment features structure
typedef struct {
    int x;          //x coordinate of the segment's 1st pixel (in raster-scan)
    int y;          //y coordinate of the segment's 1st pixel (in raster-scan)
    int min_x;      //bounding box starting x coordinate
    int max_x;      //bounding box ending x coordinate
    int min_y;      //bounding box starting y coordinate
    int max_y;      //bounding box ending y coordinate
    int size;       //segment size (percentage of frame size)
    float orient;   //segment orientation measure
    int avgprob;    //average skin probability  of the segment
    int rscs0;      //texture measure 0 (flat blocks percentage)
    int rscs1;      //texture measure 1 (low complexity texture blocks percentage)
    int rscs2;      //texture measure 2 (medium complexity texture blocks percentage)
    int rscs3;      //texture measure 3 (high complexity texture blocks percentage)
    float shape;    //shape complexity measure
    int skin;       //skintone flag
} SegmFeat;

//frame segments features structure
typedef struct {
    int num_segments;
    SegmFeat features[MAX_NUM_TRACK_SEGS*2];
} FrmSegmFeat;

//swap frame segments features
void SwapFrameSegFeatures(FrmSegmFeat *fsg, int frm_no);

#endif
