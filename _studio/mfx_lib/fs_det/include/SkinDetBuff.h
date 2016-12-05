//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: SkinDetBuff.h
 *
 * Buffers and buffer-related routines for skin detection
 * 
 ********************************************************************************/
#ifndef _SKIN_DET_BUFF_H_
#define _SKIN_DET_BUFF_H_

#include "Def.h"

typedef struct {
	BYTE *pInYUV;		//YUV frame buffer
	BYTE *pYCbCr;		//YCrCb frame buffer
} SkinDetBuff;

typedef struct {
    int sz3;    //w*h + w*h + w*h = (3*w*h)
    int szb1_5; //(w/bs)*(h/bs) + ((w/bs)/2)*((h/bs)/2) + ((w/bs)/2)*((h/bs)/2) = (3*(w/bs)*(h/bs))/2
    int szb3;   //(w/bs)*(h/bs) + (w/bs)*(h/bs) + (w/bs)*(h/bs) = (3*(w/bs)*(h/bs))
} Sizes;

//Get buffer sizes
void GetBufferSizes(Sizes *sz, int w, int h,  int bs);

//Allocates memory buffers
int AllocateBuffers(SkinDetBuff *sdb, Sizes *sz);

//De-allocates memory buffers
void FreeBuffers(SkinDetBuff *sdb);

#endif