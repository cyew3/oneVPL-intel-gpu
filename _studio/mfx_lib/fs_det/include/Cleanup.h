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
 * File: Cleanup.h
 *
 * Structures and routines for binary mask cleanup (as postprocessing)
 * 
 ********************************************************************************/
#ifndef _CLEANUP_H_
#define _CLEANUP_H_
#include "Def.h"

//Non-recursively flood-fills a 4-connected component in a block
void flood_fill4(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int stack[], int& stackptr, int MAX_BLK_RESOLUTION);

//Non-recursively flood-fills a 4-connected component in a block and collects the component's size
void flood_fill4_sz(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int *sz, int stack[], int& stackptr, int MAX_BLK_RESOLUTION);

//Non-recursively flood-fills a 4-connected component in a block and collects the component's size and bounding box
void flood_fill4_sz_bb(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int *sz, int *min_x, int *max_x, int *min_y, int *max_y, int stack[], int& stackptr, int MAX_BLK_RESOLUTION);

//Non-recursively flood-fills a 4-connected component in a block and collects the component's size, bounding box and sums of elements from the masks
void flood_fill4_sz_bb_sum(int x, int y, int nc, int oc, int w, int h, BYTE *frm, int *sz, int *min_x, int *max_x, int *min_y, int *max_y, BYTE *mask, int *sum, BYTE *mask2, int *sum2, int stack[], int& stackptr, int MAX_BLK_RESOLUTION);

//Smooths the 0/255 binary mask with 1-pel smoothing size
void smooth_mask(BYTE *mask, int w, int h, int pitch);


#endif