//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Params.h
 *
 * Routines for reading program parameters.
 * 
 ********************************************************************************/
#ifndef _PARAMS_H_
#define _PARAMS_H_

//Structure containing skin-detection parameters
typedef struct {
    int  numFr;                     //Number of previous frames 
    int  enableHystoDynEnh;         //rejects 20% of dark blocks
    int  minSegSize;                //minimal Segment size
    int  maxSegSize;                //maximum Segment size
} SkinDetParameters;

//compute blocksize for face detection based on resolution
int get_bsz_fd(int w, int h);

void SetDefaultParameters(SkinDetParameters* parameters);

#endif
