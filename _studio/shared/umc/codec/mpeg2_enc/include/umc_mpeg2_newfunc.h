//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2009 Intel Corporation. All Rights Reserved.
//

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
