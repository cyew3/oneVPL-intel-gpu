//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2012 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#include "ippdefs.h"
#include "umc_defs.h"
#include "umc_structures.h"
#include "umc_h264_defs.h"


namespace UMC_H264_ENCODER {


struct H264_AVBR
{
//protected :
    Ipp32s  mBitRate, mBitsDesiredFrame;
    Ipp64s  mBitsEncodedTotal, mBitsDesiredTotal;
    Ipp8u   *mRCqb;
    Ipp32s  mRCqh, mRCqs, mRCbf, mRClen;
    bool    mIsInit;
    Ipp32s  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantPrev, mQuantOffset;
    Ipp32s  mRCfap, mRCqap, mRCbap, mRCq;
    Ipp64f  mRCqa, mRCfa;
};

Status H264_AVBR_Create(
    H264_AVBR* state);

void H264_AVBR_Destroy(
    H264_AVBR* state);

UMC::Status H264_AVBR_Init(
    H264_AVBR* state,
    Ipp32s qas,
    Ipp32s fas,
    Ipp32s bas,
    Ipp32s bitRate,
    Ipp64f frameRate,
    Ipp32s fPixels,
    Ipp32s bitDepth,
    Ipp32s chromaSampling,
    Ipp32s alpha);

void H264_AVBR_Close(
    H264_AVBR* state);

void H264_AVBR_PostFrame(
    H264_AVBR* state,
    EnumPicCodType frameType,
    Ipp32s bEncoded);

Ipp32s H264_AVBR_GetQP(
    const H264_AVBR* state,
    EnumPicCodType frameType);

void H264_AVBR_SetQP(
    H264_AVBR* state,
    EnumPicCodType frameType,
    Ipp32s qp);

Ipp32s H264_AVBR_GetInitQP(
    H264_AVBR* state,
    Ipp32s bitRate,
    Ipp64f frameRate,
    Ipp32s fPixels,
    Ipp32s bitDepth,
    Ipp32s chromaSampling,
    Ipp32s alpha);

#define h264_Clip(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r
#define h264_ClipL(a, l) if (a < (l)) a = l
#define h264_ClipR(a, r) if (a > (r)) a = r
#define h264_Abs(a) ((a) >= 0 ? (a) : -(a))
}

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
