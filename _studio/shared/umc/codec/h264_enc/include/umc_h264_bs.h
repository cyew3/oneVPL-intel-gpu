// Copyright (c) 2004-2019 Intel Corporation
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
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

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

#endif //MFX_ENABLE_H264_VIDEO_ENCODE
