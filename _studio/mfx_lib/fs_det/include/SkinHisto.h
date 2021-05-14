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
 * File: SkinHisto.h
 *
 * Routines for skin probability estimation.
 * 
 ********************************************************************************/
#ifndef _SKIN_HISTO_H_
#define _SKIN_HISTO_H_

#include "Def.h"
#include "FrameBuff.h"

//Builds skin probability map (dynamic fast)
void BuildSkinMap_dyn_fast(BYTE* skinProb, BYTE* src, Dim* dim, uint bg, uint yTh, int hist_ind, int mode);

//Builds skin probability map (dynamic fast, slice-based)
void BuildSkinMap_dyn_fast_slice(BYTE* skinProb, BYTE* src, Dim* dim, uint bg, uint yTh, int hist_ind, int mode, uint slice_offset, int slice_nlines);

//Compute dynamic luma thresholds
void Compute_dyn_luma_thresh(FrameBuffElementPtr *src, Dim* dim, uint *bg, uint *yTh);

//Performs skin probabilities histogram selection
int SelSkinMap(BYTE* skinProb, BYTE* src, Dim* dim, int EnableHystoDynEnh, int mode);

//Obtain skin map based on faces
void New_Skin_Map_from_face(BYTE *skinProb, BYTE *pInYUV, int w, int h, BYTE hist[][256]);

//Obtain skin map based on faces (slice-based)
void New_Skin_Map_from_face_slice(BYTE *skinProb, BYTE *pInYUV, int w, int h, BYTE hist[][256], uint slice_offset, int slice_nlines);

#endif
