//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//
#pragma once
#ifndef _ASC_AVX2_IMPL_H_
#define _ASC_AVX2_IMPL_H_

#if defined(__AVX2__)
#include "asc_common_impl.h"

void ME_SAD_8x8_Block_Search_AVX2(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,
    mfxU16 *bestSAD, int *bestX, int *bestY);
void ME_SAD_8x8_Block_FSearch_AVX2(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,
    mfxU32 *bestSAD, int *bestX, int *bestY);
void RsCsCalc_4x4_AVX2(pmfxU8 pSrc, int srcPitch, int wblocks, int hblocks, pmfxU16 pRs,
    pmfxU16 pCs);
void RsCsCalc_bound_AVX2(pmfxU16 pRs, pmfxU16 pCs, pmfxU16 pRsCs, pmfxU32 pRsFrame,
    pmfxU32 pCsFrame, int wblocks, int hblocks);
void RsCsCalc_diff_AVX2(pmfxU16 pRs0, pmfxU16 pCs0, pmfxU16 pRs1, pmfxU16 pCs1, int wblocks,
    int hblocks, pmfxU32 pRsDiff, pmfxU32 pCsDiff);
void ImageDiffHistogram_AVX2(pmfxU8 pSrc, pmfxU8 pRef, mfxU32 pitch, mfxU32 width, mfxU32 height,
    mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC);
void GainOffset_AVX2(pmfxU8 *pSrc, pmfxU8 *pDst, mfxU16 width, mfxU16 height, mfxU16 pitch,
    mfxI16 gainDiff);
mfxStatus Calc_RaCa_pic_AVX2(mfxU8 *pSrc, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxF64 &RsCs);
mfxI16 AvgLumaCalc_AVX2(pmfxU32 pAvgLineVal, int len);

#endif //defined(__AVX2__)

#endif //_ASC_AVX2_IMPL_H_