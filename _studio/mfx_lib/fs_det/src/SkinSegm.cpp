//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: SkinSegm.cpp
 *
 * Functions for skin detection-related segmentation
 * 
 ********************************************************************************/
#include "SkinSegm.h"

//swap frame segments features
void SwapFrameSegFeatures(FrmSegmFeat *fsg, int frm_no) {
    int i;
    if (frm_no < NUM_SEG_TRACK_FRMS) return;
    for (i=1; i<NUM_SEG_TRACK_FRMS; i++) {
        fsg[i-1] = fsg[i];
    }
}

