//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: SkinDetBuff.c
 *
 * Buffers and buffer-related routines for skin detection
 * 
 ********************************************************************************/

#include "Def.h"
#include "SkinDetBuff.h"

//Get buffer sizes
void GetBufferSizes(Sizes *sz, int w, int h,  int bs) 
{
    sz->sz3     = (3*w*h);
    sz->szb1_5  = (3*(w/bs)*(h/bs))/2;
    sz->szb3    = (3*(w/bs)*(h/bs));
}

//Allocates memory buffers
int AllocateBuffers(SkinDetBuff *sdb, Sizes *sz) 
{
    INIT_MEMORY_C(sdb->pInYUV, 0, sz->szb3, BYTE);
    if (sdb->pInYUV == NULL) return 1;
    
    INIT_MEMORY_C(sdb->pYCbCr, 0, sz->szb3, BYTE);
    if (sdb->pYCbCr == NULL) return 1;
    
    return 0;
}

//De-allocates memory buffers
void FreeBuffers(SkinDetBuff *sdb) 
{
    DEINIT_MEMORY_C(sdb->pInYUV);
    DEINIT_MEMORY_C(sdb->pYCbCr);
}
