//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#ifndef _MOTIONESTIMATIONENGINE_H_
#define _MOTIONESTIMATIONENGINE_H_

#include "ASCstructures.h"
#include "asc.h"

namespace ns_asc {

void MotionRangeDeliveryF(mfxI16 xLoc, mfxI16 yLoc, mfxI16 *limitXleft, mfxI16 *limitXright, mfxI16 *limitYup, mfxI16 *limitYdown, ASCImDetails dataIn);

/* 4x4 Block size Functions */
bool MVcalcSAD4x4(ASCMVector MV, pmfxU8 curY, pmfxU8 refY, ASCImDetails dataIn, mfxU32 *bestSAD, mfxI32 *distance);
/* 8x8 Block size Functions */
bool MVcalcSAD8x8(ASCMVector MV, pmfxU8 curY, pmfxU8 refY, ASCImDetails *dataIn, mfxU32 *bestSAD, mfxI32 *distance);
mfxU16 __cdecl ME_simple(ASCVidRead *videoIn, mfxI32 fPos, ASCImDetails *dataIn, ASCimageData *scale, ASCimageData *scaleRef, bool first, ASCVidData *limits, t_ME_SAD_8x8_Block_Search ME_SAD_8x8_Block_Search);
/* All Block sizes -general case- */
bool MVcalcSAD(ASCMVector MV, pmfxU8 curY, pmfxU8 refY, ASCImDetails dataIn, mfxI32 fPos,mfxI32 xLoc, mfxI32 yLoc, mfxU32 *bestSAD, mfxI32 *distance);
/* ------------------------------ */
void SearchLimitsCalc(mfxI32 xLoc, mfxI32 yLoc, mfxI32 *limitXleft, mfxI32 *limitXright, mfxI32 *limitYup, mfxI32 *limitYdown, ASCImDetails dataIn, mfxI32 range, ASCMVector mv, ASCVidData limits);
mfxF32 Dist(ASCMVector vector);
mfxI32 DistInt(ASCMVector vector);
void MVpropagationCheck(mfxI32 xLoc, mfxI32 yLoc, ASCImDetails dataIn, ASCMVector *propagatedMV);
};
#endif //_MOTIONESTIMATIONENGINE_H_