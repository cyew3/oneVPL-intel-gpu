//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Dim.h
 *
 * Structures and routines for video dimensions
 * 
 ********************************************************************************/
#ifndef _DIM_H_
#define _DIM_H_
#include "Def.h"

//Dimensions related to current video
typedef struct {
    uint w;         //width  of video frames
    uint h;         //height of video frames

    uint wb;        //width  of video frames in blocks
    uint hb;        //height of video frames in blocks
    uint ubOff;     //U plane offset in blocks (hb * wb)
    uint vbOff;     //V plane offset in blocks (2 * hb * wb)
    uint frbSize;   //size of YUV Frame in blocks (wb * hb * 3)

    uint blSz;      //algorithm's blocks size (width and height)
    uint blSzPx;    //number of pixels in block  (blSz * blSz)
    uint blHNum;    //number of blocks in horizontal dimension  (w / blSz)
    uint blVNum;    //number of blocks in vertical   dimension  (h / blSz)
    uint blTotal;   //total number of blocks  (blHNum * blVNum)
    uint numFr;     //number of frames in Frame Stack
} Dim;

//Performs initialization of Dim structure (variables that depends on dimensions)
void cDimInit(Dim *dim, int w, int h, int blSz, int numFr);

#endif