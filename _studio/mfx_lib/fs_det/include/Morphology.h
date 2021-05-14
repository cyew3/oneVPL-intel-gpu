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
