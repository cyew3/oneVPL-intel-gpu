//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
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
