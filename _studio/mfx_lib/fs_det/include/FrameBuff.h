//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
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
