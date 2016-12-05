//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Video.h
 *
 * Structures and routines for basic video processing
 * 
 ********************************************************************************/
#ifndef _VIDEO_H_
#define _VIDEO_H_
#include "Def.h"

//Converts frame from YUV444 to Yrg
void ConvertColorSpaces444(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch);

//Converts frame from YUV444 to Yrg (slice-based)
void ConvertColorSpaces444_slice(const BYTE* pYUV, BYTE* pSkinColorSpace, const int iW, const int iH, int pitch, int slice_offset, int slice_nlines);


#endif