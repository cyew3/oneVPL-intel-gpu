//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
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
