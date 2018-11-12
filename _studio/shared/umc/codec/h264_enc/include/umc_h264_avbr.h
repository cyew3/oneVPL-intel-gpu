// Copyright (c) 2006-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
