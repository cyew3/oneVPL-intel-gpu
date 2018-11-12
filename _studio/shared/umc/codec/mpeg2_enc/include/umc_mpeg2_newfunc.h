// Copyright (c) 2002-2018 Intel Corporation
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
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#ifndef __UMC_MPEG2_NEWFUNC_H
#define __UMC_MPEG2_NEWFUNC_H

#include "ippdefs.h"


IPPAPI (IppStatus, tmp_ippiDCT8x8FwdUV_8u16s_C1R,
                   ( const Ipp8u* pSrc, int srcStep, Ipp16s* pDst ))

IPPAPI (IppStatus, tmp_ippiDCT8x8InvUV_16s8u_C1R,
                   ( const Ipp16s* pSrc, Ipp8u* pDst, int dstStep ))


IPPAPI(IppStatus, tmp_ippiGetDiff8x8_8u16s_C2P2, (
  const Ipp8u*  pSrcCur,
        Ipp32s  srcCurStep,
  const Ipp8u*  pSrcRef,
        Ipp32s  srcRefStep,
        Ipp16s* pDstDiff,
        Ipp32s  dstDiffStep,
        Ipp16s* pDstPredictor,
        Ipp32s  dstPredictorStep,
        Ipp32s  mcType,
        Ipp32s  roundControl))

IPPAPI(IppStatus, tmp_ippiGetDiff8x4_8u16s_C2P2, (
  const Ipp8u*  pSrcCur,
        Ipp32s  srcCurStep,
  const Ipp8u*  pSrcRef,
        Ipp32s  srcRefStep,
        Ipp16s* pDstDiff,
        Ipp32s  dstDiffStep,
        Ipp16s* pDstPredictor,
        Ipp32s  dstPredictorStep,
        Ipp32s  mcType,
        Ipp32s  roundControl))

IPPAPI(IppStatus, tmp_ippiGetDiff8x8B_8u16s_C2P2, (
  const Ipp8u*       pSrcCur,
        Ipp32s       srcCurStep,
  const Ipp8u*       pSrcRefF,
        Ipp32s       srcRefStepF,
        Ipp32s       mcTypeF,
  const Ipp8u*       pSrcRefB,
        Ipp32s       srcRefStepB,
        Ipp32s       mcTypeB,
        Ipp16s*      pDstDiff,
        Ipp32s       dstDiffStep,
        Ipp32s       roundControl))

IPPAPI(IppStatus, tmp_ippiGetDiff8x4B_8u16s_C2P2, (
  const Ipp8u*       pSrcCur,
        Ipp32s       srcCurStep,
  const Ipp8u*       pSrcRefF,
        Ipp32s       srcRefStepF,
        Ipp32s       mcTypeF,
  const Ipp8u*       pSrcRefB,
        Ipp32s       srcRefStepB,
        Ipp32s       mcTypeB,
        Ipp16s*      pDstDiff,
        Ipp32s       dstDiffStep,
        Ipp32s       roundControl))


IPPAPI(IppStatus, tmp_ippiMC8x8_8u_C2P2, (
  const Ipp8u*       pSrcRef,
        Ipp32s       srcStep,
  const Ipp16s*      pSrcYData,
        Ipp32s       srcYDataStep,
        Ipp8u*       pDst,
        Ipp32s       dstStep,
        Ipp32s       mcType,
        Ipp32s       roundControl))

IPPAPI(IppStatus, tmp_ippiMC8x4_8u_C2P2, (
  const Ipp8u*       pSrcRef,
        Ipp32s       srcStep,
  const Ipp16s*      pSrcYData,
        Ipp32s       srcYDataStep,
        Ipp8u*       pDst,
        Ipp32s       dstStep,
        Ipp32s       mcType,
        Ipp32s       roundControl))

IPPAPI(IppStatus, tmp_ippiMC8x8B_8u_C2P2, (
  const Ipp8u*       pSrcRefF,
        Ipp32s       srcStepF,
        Ipp32s       mcTypeF,
  const Ipp8u*       pSrcRefB,
        Ipp32s       srcStepB,
        Ipp32s       mcTypeB,
  const Ipp16s*      pSrcYData,
        Ipp32s       srcYDataStep,
        Ipp8u*       pDst,
        Ipp32s       dstStep,
        Ipp32s       roundControl))

IPPAPI(IppStatus, tmp_ippiMC8x4B_8u_C2P2, (
  const Ipp8u*       pSrcRefF,
        Ipp32s       srcStepF,
        Ipp32s       mcTypeF,
  const Ipp8u*       pSrcRefB,
        Ipp32s       srcStepB,
        Ipp32s       mcTypeB,
  const Ipp16s*      pSrcYData,
        Ipp32s       srcYDataStep,
        Ipp8u*       pDst,
        Ipp32s       dstStep,
        Ipp32s       roundControl))


IPPAPI(IppStatus, tmp_ippiSAD8x8_8u32s_C2R, (
  const Ipp8u*  pSrcCur,
        int     srcCurStep,
  const Ipp8u*  pSrcRef,
        int     srcRefStep,
        Ipp32s* pDst,
        Ipp32s  mcType))

IPPAPI ( IppStatus, tmp_ippiSet8x8UV_8u_C2R,
       ( Ipp8u value, Ipp8u* pDst, int dstStep ))


#endif // __UMC_MPEG2_NEWFUNC_H

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
