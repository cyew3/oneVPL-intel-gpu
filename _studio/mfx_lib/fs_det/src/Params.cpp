//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Params.c
 *
 * Routines for reading program parameters.
 * 
 ********************************************************************************/
#include "Def.h"
#include "Params.h"


//Sets default parameters for skin-detection
void SetDefaultParameters(SkinDetParameters* parameters) {
    parameters->numFr                       = 9;            //number of frames in frame buffer
    parameters->enableHystoDynEnh           = 1;            //flag for dynamical histogram enhancement
    parameters->minSegSize                  = 0;            //minimum segment size (as a percentage of frame size)
    parameters->maxSegSize                  = 100;          //maximum segment size (as a percentage of frame size)
}

//compute blocksize for face detection based on resolution
int get_bsz_fd(int w, int h) {
    FS_UNREFERENCED_PARAMETER(w);
    if      (h>=1080) return BLOCKSIZE;
    else if (h>= 720) return 2;
    else             return 1;
}
