//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Morphology.h
 *
 * Structures and routines for morphological operations
 * 
 ********************************************************************************/
#ifndef _MORPHOLOGY_H_
#define _MORPHOLOGY_H_
#include "Def.h"

//define maximum size = max_diameter*max_diameter
//#define MAX_MORPH_SIZE 81

//structure which stores parameters of window which can be used for morphology
typedef struct struct_WindowMask WindowMask;

//defines mask for window with specified form and radius
const WindowMask* Morphology_GetWindowMask(int r, int circle);

//performs morphology dilation operation
void Dilation_Value_byte(BYTE *mask, BYTE *mask_buf, int w, int h, const WindowMask *wnd);

//performs morphology dilation operation (faster)
void Dilation_Value_byte_faster( BYTE *mask, BYTE *mask_buf, int w, int h, const WindowMask *wnd);

//performs morphology erosion operation
void Erosion_Value_byte(BYTE *mask, BYTE *mask_buf, int w, int h, const WindowMask *wnd);

//perform morphological opening (erosion followed by dilation with same morphology window)
void Open_Value_byte(BYTE *mask, BYTE *mask_buf, int w, int h, const WindowMask *wnd);

//perform morphological closing (dilation followed by erosion with same morphology window)
void Close_Value_byte(BYTE *mask, BYTE *mask_buf, int w, int h, const WindowMask *wnd);

#endif
