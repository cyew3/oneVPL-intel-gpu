//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#ifndef UMC_H264_BS_H__
#define UMC_H264_BS_H__

#include "umc_h264_defs.h"
#include "umc_base_bitstream.h"

namespace UMC_H264_ENCODER
{

#define PIXBITS 8
#include "umc_h264_bs_tmpl.h"
#define FAKE_BITSTREAM
    #include "umc_h264_bs_tmpl.h"
#undef FAKE_BITSTREAM
#undef PIXBITS

#ifdef BITDEPTH_9_12

    #define PIXBITS 16
    #include "umc_h264_bs_tmpl.h"
    #define FAKE_BITSTREAM
        #include "umc_h264_bs_tmpl.h"
    #undef FAKE_BITSTREAM
    #undef PIXBITS

#endif // BITDEPTH_9_12

IppStatus ippiCABACGetContextsWrap_H264(
    Ipp8u*           pContexts,
    Ipp32u           from,
    Ipp32u           num,
    IppvcCABACState* pCabacState);

Status H264BsReal_ResidualBlock_CABAC(
    const Ipp16s*   pResidualCoeffs,
    Ipp32u          nLastNonZeroCoeff,
    Ipp32u          ctxBlockCat,
    Ipp32u          bFrameBlock,
    H264BsBase*     bs,
    Ipp32s          idx_start,
    Ipp32s          idx_end);

Status H264BsFake_ResidualBlock_CABAC(
    const Ipp16s*   pResidualCoeffs,
    Ipp32u          nLastNonZeroCoeff,
    Ipp32u          ctxBlockCat,
    Ipp32u          bFrameBlock,
    H264BsBase*     bs,
    Ipp32s          idx_start,
    Ipp32s          idx_end);

IppStatus ippiCABACEncodeBinOwn_H264(
    Ipp32u ctxIdx,
    Ipp32u code,
    IppvcCABACState* pCabacState_);

IppStatus ippiCABACEncodeBinBypassOwn_H264(
    Ipp32u code,
    IppvcCABACState* pCabacState_);

IppStatus ippiCABACTerminateSliceOwn_H264(
    Ipp32u* pBitStreamBytes,
    IppvcCABACState* pCabacState_);

IppStatus ippiCABACGetSizeOwn_H264(
    Ipp32u* pSize);

IppStatus ippiCABACInitOwn_H264(
    IppvcCABACState* pCabacState_,
    Ipp8u*          pBitStream,
    Ipp32u          nBitStreamOffsetBits,
    Ipp32u          nBitStreamSize,
    Ipp32s          SliceQPy,
    Ipp32s          cabacInitIdc);

IppStatus ippiCABACInitAEEOwn_H264( 
    IppvcCABACState* pCabacState_,
    Ipp8u*          pBitStream,
    Ipp32u          nBitStreamOffsetBits,
    Ipp32u          nBitStreamSize);

IppStatus ippiCABACInitAllocOwn_H264(
    IppvcCABACState** ppCabacState,
    Ipp8u*           pBitStream,
    Ipp32u           nBitStreamOffsetBits,
    Ipp32u           uBitStreamSize,
    Ipp32s           SliceQPy,
    Ipp32s           cabacInitIdc);

IppStatus ippiCABACFreeOwn_H264(
    IppvcCABACState* pCabacState);

void InitIntraContextExtra_H264(
                                Ipp8u* pExtraState, // after 460th
                                Ipp32s SliceQPy);
void InitInterContextExtra_H264(
                                Ipp8u* pExtraState, // after 460th
                                Ipp32s SliceQPy);

} //namespace UMC_H264_ENCODER

#endif // UMC_H264_BS_H__

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
