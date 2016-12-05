//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
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