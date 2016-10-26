//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#ifndef _H263DEC_H_
#define _H263DEC_H_

#ifdef __cplusplus
extern "C" {
#endif

//#ifdef __INTEL_COMPILER
//#include <emmintrin.h>
//#define USE_INTRINSIC
//#else
#undef USE_INTRINSIC
//#endif

#define h263_Div2(a) ((a) >= 0 ? ((a) >> 1) : (((a)+1) >> 1))
#define h263_Div2Round(a) (((a) >> 1) | ((a) & 1))
#define h263_DivRoundInf(a, b) ((a) < 0 ? (((a) - ((b) >> 1)) / (b)) : (((a) + ((b) >> 1)) / (b)))

__INLINE Ipp16s h263_Median(Ipp16s a, Ipp16s b, Ipp16s c)
{
    if (a > b) {
        Ipp16s  t = a; a = b; b = t;
    }
    return (Ipp16s)((b <= c) ? b : (c >= a) ? c : a);
}

__INLINE void h263_ComputeChromaMV(const IppMotionVector *mvLuma, IppMotionVector *mvChroma)
{
    mvChroma->dx = (Ipp16s)h263_Div2Round(mvLuma->dx);
    mvChroma->dy = (Ipp16s)h263_Div2Round(mvLuma->dy);
}

__INLINE void h263_ComputeChroma4MV(const IppMotionVector mvLuma[4], IppMotionVector *mvChroma)
{
    Ipp32s  dx, dy, cdx, cdy, adx, ady;

    dx = mvLuma[0].dx + mvLuma[1].dx + mvLuma[2].dx + mvLuma[3].dx;
    dy = mvLuma[0].dy + mvLuma[1].dy + mvLuma[2].dy + mvLuma[3].dy;
    adx = abs(dx);
    ady = abs(dy);
    cdx = h263_cCbCrMvRound16[adx & 15] + (adx >> 4) * 2;
    cdy = h263_cCbCrMvRound16[ady & 15] + (ady >> 4) * 2;
    mvChroma->dx = (Ipp16s)((dx >= 0) ? cdx : -cdx);
    mvChroma->dy = (Ipp16s)((dy >= 0) ? cdy : -cdy);
}


#define limitMV(dx, xmin, xmax, mvd) \
{                                    \
    if ((dx) < (xmin))               \
        mvd = (Ipp16s)(xmin);        \
    else if ((dx) >= (xmax))         \
        mvd = (Ipp16s)(xmax);        \
    else                             \
        mvd = (Ipp16s)(dx);          \
}

__INLINE void h263_LimitMV(const IppMotionVector *pSrcMV, IppMotionVector *pDstMV, const IppiRect *limitRect, Ipp32s x, Ipp32s y, Ipp32s size)
{
    limitMV(pSrcMV->dx, (limitRect->x - x) << 1, (limitRect->x - x + limitRect->width  - size) << 1, pDstMV->dx);
    limitMV(pSrcMV->dy, (limitRect->y - y) << 1, (limitRect->y - y + limitRect->height - size) << 1, pDstMV->dy);
}

__INLINE void h263_Limit4MV(const IppMotionVector *pSrcMV, IppMotionVector *pDstMV, const IppiRect *limitRect, Ipp32s x, Ipp32s y, Ipp32s size)
{
    h263_LimitMV(&pSrcMV[0], &pDstMV[0], limitRect, x       , y,        size);
    h263_LimitMV(&pSrcMV[1], &pDstMV[1], limitRect, x + size, y,        size);
    h263_LimitMV(&pSrcMV[2], &pDstMV[2], limitRect, x       , y + size, size);
    h263_LimitMV(&pSrcMV[3], &pDstMV[3], limitRect, x + size, y + size, size);
}

__INLINE void h263_LimitFMV(const IppMotionVector *pSrcMV, IppMotionVector *pDstMV, const IppiRect *limitRect, Ipp32s x, Ipp32s y, Ipp32s size)
{
    limitMV(pSrcMV->dx, (limitRect->x - x) << 1, (limitRect->x - x + limitRect->width  - size) << 1, pDstMV->dx);
    limitMV(pSrcMV->dy << 1, (limitRect->y - y) << 1, (limitRect->y - y + limitRect->height - size) << 1, pDstMV->dy);
    pDstMV->dy >>= 1;
}

#define reconstructMV(mv, mvdx, mvdy, leftlim, rightlim) \
{ \
  Ipp32s dx, dy; \
  dx = (mv)->dx + (mvdx); \
  if (dx < (leftlim)) \
    dx += 64; \
  else if (dx > (rightlim)) \
    dx -= 64; \
  (mv)->dx = (Ipp16s)dx; \
  dy = (mv)->dy + (mvdy); \
  if (dy < (leftlim)) \
    dy += 64; \
  else if (dy > (rightlim)) \
    dy -= 64; \
  (mv)->dy = (Ipp16s)dy; \
}

#define H263_MV_OFF_HP(dx, dy, step) \
    (((dx) >> 1) + (step) * ((dy) >> 1))

#define H263_MV_ACC_HP(dx, dy) \
    ((((dy) & 1) << 1) + ((dx) & 1))

#define H263_MV_OFF_QP(dx, dy, step) \
    (((dx) >> 2) + (step) * ((dy) >> 2))

#define H263_MV_ACC_QP(dx, dy) \
    ((((dy) & 3) << 2) + ((dx) & 3))

#define h263_Copy8x8HP_8u(pSrc, srcStep, pDst, dstStep, mv, rc) \
    ippiCopy8x8HP_8u_C1R(pSrc + H263_MV_OFF_HP((mv)->dx, (mv)->dy, srcStep), srcStep, pDst, dstStep, H263_MV_ACC_HP((mv)->dx, (mv)->dy), rc)

#define h263_Copy16x16HP_8u(pSrc, srcStep, pDst, dstStep, mv, rc) \
    ippiCopy16x16HP_8u_C1R(pSrc + H263_MV_OFF_HP((mv)->dx, (mv)->dy, srcStep), srcStep, pDst, dstStep, H263_MV_ACC_HP((mv)->dx, (mv)->dy), rc)

#define h263_Add8x8HP_16s8u(pSrc, srcStep, pResid, pDst, dstStep, mv, rc) \
    ippiAdd8x8HP_16s8u_C1RS(pResid, 16, pSrc + H263_MV_OFF_HP((mv)->dx, (mv)->dy, srcStep), srcStep, pDst, dstStep, H263_MV_ACC_HP((mv)->dx, (mv)->dy), rc)

#define h263_Add8x8_16s8u(pSrcDst, pResid, srcDstStep) \
    ippiAdd8x8_16s8u_C1IRS(pResid, 16, pSrcDst, srcDstStep)

#define h263_AddBackPredPB_8u(pSrc, srcStep, roi, pSrcDst, srcDstStep, mv) \
  ippiAddBackPredPB_H263_8u_C1R(pSrc, srcStep, roi, pSrcDst, srcDstStep, H263_MV_ACC_HP((mv)->dx, (mv)->dy));

#define h263_UpdateQuant(pInfo, quant) \
{ \
    quant += h263_dquant[h263_GetBits9(pInfo, 2)]; \
    h263_CLIP(quant, 1, 31); \
}

#define h263_UpdateQuant_B(pInfo, quant) \
if (h263_GetBit(pInfo) != 0) { \
    quant += (h263_GetBit(pInfo) == 0) ? -2 : 2; \
    h263_CLIP(quant, 1, 31); \
}

__INLINE void h263_Set8x8_8u(Ipp8u *p, Ipp32s step, Ipp8u v)
{
#ifdef USE_INTRINSIC
    __m64 _p_v = _mm_set1_pi8(v);
    *(__m64*)p = _p_v;
    *(__m64*)(p+step) = _p_v;
    p += 2 * step;
    *(__m64*)p = _p_v;
    *(__m64*)(p+step) = _p_v;
    p += 2 * step;
    *(__m64*)p = _p_v;
    *(__m64*)(p+step) = _p_v;
    p += 2 * step;
    *(__m64*)p = _p_v;
    *(__m64*)(p+step) = _p_v;
    _mm_empty();
#else
    Ipp32u val;

    val = v + (v <<  8);
    val += val << 16;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val; p += step;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val; p += step;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val; p += step;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val; p += step;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val; p += step;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val; p += step;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val; p += step;
    ((Ipp32u*)p)[0] = val; ((Ipp32u*)p)[1] = val;
#endif
}

__INLINE void h263_Set16x16_8u(Ipp8u *p, Ipp32s step, Ipp8u val)
{
    Ipp32s i, j;

    for (i = 0; i < 16; i ++) {
        for (j = 0; j < 16; j ++)
            p[j] = val;
        p += step;
    }
}

#ifdef USE_INTRINSIC

#define h263_Zero4MV(mv) \
    memset(mv, 0, 4 * sizeof(IppMotionVector));

#define h263_Zero64_16s(pDst) \
{ \
    __m128i _p_val = _mm_setzero_si128(); \
    ((__m128i*)(pDst))[0] = _p_val; \
    ((__m128i*)(pDst))[1] = _p_val; \
    ((__m128i*)(pDst))[2] = _p_val; \
    ((__m128i*)(pDst))[3] = _p_val; \
    ((__m128i*)(pDst))[4] = _p_val; \
    ((__m128i*)(pDst))[5] = _p_val; \
    ((__m128i*)(pDst))[6] = _p_val; \
    ((__m128i*)(pDst))[7] = _p_val; \
}

#define h263_Set64_16s(val, pDst) \
{ \
    __m128i _p_val = _mm_set1_epi16((Ipp16s)(val)); \
    ((__m128i*)(pDst))[0] = _p_val; \
    ((__m128i*)(pDst))[1] = _p_val; \
    ((__m128i*)(pDst))[2] = _p_val; \
    ((__m128i*)(pDst))[3] = _p_val; \
    ((__m128i*)(pDst))[4] = _p_val; \
    ((__m128i*)(pDst))[5] = _p_val; \
    ((__m128i*)(pDst))[6] = _p_val; \
    ((__m128i*)(pDst))[7] = _p_val; \
}

#else

#define h263_Zero4MV(mv) \
    (mv)[0].dx = (mv)[0].dy = (mv)[1].dx = (mv)[1].dy = (mv)[2].dx = (mv)[2].dy = (mv)[3].dx = (mv)[3].dy = 0

#define h263_Zero64_16s(pDst) \
{ \
    Ipp32s  i; \
    for (i = 0; i < 16; i += 4) {\
        ((Ipp64u*)(pDst))[i] = 0; \
        ((Ipp64u*)(pDst))[i+1] = 0; \
        ((Ipp64u*)(pDst))[i+2] = 0; \
        ((Ipp64u*)(pDst))[i+3] = 0; \
    } \
}

#define h263_Set64_16s(val, pDst) \
{ \
    Ipp32s  i; \
    Ipp32u  v; \
    v = ((val) << 16) + (Ipp16u)(val); \
    for (i = 0; i < 32; i += 8) { \
        ((Ipp32u*)(pDst))[i] = v; \
        ((Ipp32u*)(pDst))[i+1] = v; \
        ((Ipp32u*)(pDst))[i+2] = v; \
        ((Ipp32u*)(pDst))[i+3] = v; \
        ((Ipp32u*)(pDst))[i+4] = v; \
        ((Ipp32u*)(pDst))[i+5] = v; \
        ((Ipp32u*)(pDst))[i+6] = v; \
        ((Ipp32u*)(pDst))[i+7] = v; \
    } \
}

#endif

#define h263_MC_HP(pat, pRef, stepRef, pCur, stepCur, coeffMB, mv, rc) \
{ \
    if (pat) { \
        h263_Add8x8HP_16s8u(pRef, stepRef, coeffMB, pCur, stepCur, mv, rc); \
    } else { \
        h263_Copy8x8HP_8u(pRef, stepRef, pCur, stepCur, mv, rc); \
    } \
}

#define h263_MC(pat, pRef, stepRef, pCur, stepCur, coeffMB) \
{ \
  if (pat) { \
    ippiAdd8x8HP_16s8u_C1RS(coeffMB, 16, pRef, stepRef, pCur, stepCur, 0, 0); \
  } else { \
    ippiCopy8x8_8u_C1R(pRef, stepRef, pCur, stepCur); \
  } \
}

#define h263_AddResidual(pat, pc, stepc, coeffMB) \
{ \
    if (pat) { \
        h263_Add8x8_16s8u(pc, coeffMB, stepc); \
    } \
}
#ifndef UMC_H263_DISABLE_SORENSON
#define h263_DecodeMCBlockInter(pInfo, quant, pat, pRef, pCur, step, coeffMB, mv, rounding) \
{ \
  if (pat) { \
    Ipp32s lnz; \
    Ipp32s modQ = pInfo->VideoSequence.VideoPicture.oppmodes.modQuant; \
    if (pInfo->VideoSequence.VideoPicture.sorenson_format) { \
      if (h263_ReconstructCoeffsInterSorenson_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB, &lnz, quant) != ippStsNoErr) { \
        h263_Error("Error when decoding coefficients of Inter block"); \
        goto Err_1; \
      } \
    } else if (!pInfo->VideoSequence.VideoPicture.oppmodes.altInterVLC) { \
      if (ippiReconstructCoeffsInter_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB, &lnz, quant, modQ) != ippStsNoErr) { \
        h263_Error("Error when decoding coefficients of Inter block"); \
        goto Err_1; \
      } \
    } else { \
      if (ippiDecodeCoeffsInter_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB+64, &lnz, modQ, IPPVC_SCAN_NONE) != ippStsNoErr) { \
        if (ippiDecodeCoeffsIntra_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB+64, &lnz, 1, modQ, IPPVC_SCAN_NONE) != ippStsNoErr) { \
          h263_Error("Error when decoding coefficients of a block in advanced INTER VLC mode"); \
          goto Err_1; \
        } \
      } \
      ippiQuantInvInter_H263_16s_C1I(coeffMB+64, lnz, quant, modQ); \
      ippiScanInv_16s_C1(coeffMB+64, coeffMB, lnz, IPPVC_SCAN_ZIGZAG); \
    } \
    if (lnz != 0) { \
      if ((lnz <= 4) && (coeffMB[16] == 0)) \
        ippiDCT8x8Inv_2x2_16s_C1I(coeffMB); \
      else if ((lnz <= 13) && (coeffMB[32] == 0)) \
        ippiDCT8x8Inv_4x4_16s_C1I(coeffMB); \
      else \
        ippiDCT8x8Inv_16s_C1I(coeffMB); \
    } else { \
      h263_Set64_16s((Ipp16s)((coeffMB[0] + 4) >> 3), coeffMB); \
    } \
    h263_Add8x8HP_16s8u(pRef, step, coeffMB, pCur, step, mv, rounding); \
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_C); \
  } else { \
    h263_Copy8x8HP_8u(pRef, step, pCur, step, mv, rounding); \
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC); \
  } \
}
#else
#define h263_DecodeMCBlockInter(pInfo, quant, pat, pRef, pCur, step, coeffMB, mv, rounding) \
{ \
  if (pat) { \
    Ipp32s lnz; \
    Ipp32s modQ = pInfo->VideoSequence.VideoPicture.oppmodes.modQuant; \
    if (!pInfo->VideoSequence.VideoPicture.oppmodes.altInterVLC) { \
      if (ippiReconstructCoeffsInter_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB, &lnz, quant, modQ) != ippStsNoErr) { \
        h263_Error("Error when decoding coefficients of Inter block"); \
        goto Err_1; \
      } \
    } else { \
      if (ippiDecodeCoeffsInter_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB+64, &lnz, modQ, IPPVC_SCAN_NONE) != ippStsNoErr) { \
        if (ippiDecodeCoeffsIntra_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB+64, &lnz, 1, modQ, IPPVC_SCAN_NONE) != ippStsNoErr) { \
          h263_Error("Error when decoding coefficients of a block in advanced INTER VLC mode"); \
          goto Err_1; \
        } \
      } \
      ippiQuantInvInter_H263_16s_C1I(coeffMB+64, lnz, quant, modQ); \
      ippiScanInv_16s_C1(coeffMB+64, coeffMB, lnz, IPPVC_SCAN_ZIGZAG); \
    } \
    if (lnz != 0) { \
      if ((lnz <= 4) && (coeffMB[16] == 0)) \
        ippiDCT8x8Inv_2x2_16s_C1I(coeffMB); \
      else if ((lnz <= 13) && (coeffMB[32] == 0)) \
        ippiDCT8x8Inv_4x4_16s_C1I(coeffMB); \
      else \
        ippiDCT8x8Inv_16s_C1I(coeffMB); \
    } else { \
      h263_Set64_16s((Ipp16s)((coeffMB[0] + 4) >> 3), coeffMB); \
    } \
    h263_Add8x8HP_16s8u(pRef, step, coeffMB, pCur, step, mv, rounding); \
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_C); \
  } else { \
    h263_Copy8x8HP_8u(pRef, step, pCur, step, mv, rounding); \
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC); \
  } \
}
#endif
#define h263_DecodeMCBlockInter_B(pInfo, quant, pat, pRef, stepR, pB, stepB, coeffMB) \
{ \
  if (pat) { \
    Ipp32s lnz; \
    Ipp32s modQ = pInfo->VideoSequence.VideoPicture.oppmodes.modQuant; \
    if (!pInfo->VideoSequence.VideoPicture.oppmodes.altInterVLC) { \
      if (ippiReconstructCoeffsInter_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB, &lnz, quant, modQ) != ippStsNoErr) { \
        h263_Error("Error when decoding coefficients of Inter block"); \
        goto Err_1; \
      } \
    } else { \
      if (ippiDecodeCoeffsInter_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB+64, &lnz, modQ, IPPVC_SCAN_NONE) != ippStsNoErr) { \
        if (ippiDecodeCoeffsIntra_H263_1u16s(&pInfo->bufptr, &pInfo->bitoff, coeffMB+64, &lnz, 1, modQ, IPPVC_SCAN_NONE) != ippStsNoErr) { \
          h263_Error("Error when decoding coefficients of a block in advanced INTER VLC mode"); \
          goto Err_1; \
        } \
      } \
      ippiQuantInvInter_H263_16s_C1I(coeffMB+64, lnz, quant, modQ); \
      ippiScanInv_16s_C1(coeffMB+64, coeffMB, lnz, IPPVC_SCAN_ZIGZAG); \
    } \
    if (lnz != 0) { \
      if ((lnz <= 4) && (coeffMB[16] == 0)) \
        ippiDCT8x8Inv_2x2_16s_C1I(coeffMB); \
      else if ((lnz <= 13) && (coeffMB[32] == 0)) \
        ippiDCT8x8Inv_4x4_16s_C1I(coeffMB); \
      else \
        ippiDCT8x8Inv_16s_C1I(coeffMB); \
    } else { \
      h263_Set64_16s((Ipp16s)((coeffMB[0] + 4) >> 3), coeffMB); \
    } \
    ippiAdd8x8HP_16s8u_C1RS(coeffMB, 16, pRef, stepR, pB, stepB, 0, 0); \
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_C); \
  } else { \
    ippiCopy8x8_8u_C1R(pRef, stepR, pB, stepB); \
    h263_StatisticInc_(&pInfo->VideoSequence.Statistic.nB_INTER_NC); \
  } \
}

/*
//  decode cbpy for Intra MB
*/
__INLINE h263_Status h263_DecodeCBPY_I(h263_Info* pInfo, Ipp32s *yPattern)
{
  Ipp32u      code;

  code = h263_ShowBits9(pInfo, 6);
  *yPattern = h263_cbpy[code].code;
  if (h263_cbpy[code].len == 255) {
    h263_Error("Error when decoding cbpy of I-Frame macroblock");
    return H263_STATUS_ERROR;
  } else {
    h263_FlushBits(pInfo, h263_cbpy[code].len);
    return H263_STATUS_OK;
  }
}

/*
//  decode cbpy for Inter-Frame MB
*/
__INLINE h263_Status h263_DecodeCBPY_P(h263_Info* pInfo, Ipp32s *yPattern, Ipp32s mbType)
{
  Ipp32u code;

  code = h263_ShowBits9(pInfo, 6);
  if (mbType < IPPVC_MBTYPE_INTRA || mbType == IPPVC_MBTYPE_INTER4V_Q)
    *yPattern = 15 - h263_cbpy[code].code;
  else
    *yPattern = h263_cbpy[code].code;
  if (h263_cbpy[code].len == 255) {
    h263_Error("Error when decoding cbpy of P-Frame macroblock");
    return H263_STATUS_ERROR;
  } else {
    h263_FlushBits(pInfo, h263_cbpy[code].len);
    return H263_STATUS_OK;
  }
}

/*
//  decode cbpy for B-Frame MB
*/
__INLINE h263_Status h263_DecodeCBPY_B(h263_Info* pInfo, Ipp32s *yPattern, Ipp32s mbType)
{
  Ipp32u code;

  code = h263_ShowBits9(pInfo, 6);
  if (mbType == IPPVC_MBTYPE_INTRA || mbType == IPPVC_MBTYPE_INTRA_Q)
    *yPattern = h263_cbpy[code].code;
  else
    *yPattern = 15 - h263_cbpy[code].code;

  if (h263_cbpy[code].len == 255) {
    h263_Error("Error when decoding cbpy of B-Frame macroblock");
    return H263_STATUS_ERROR;
  } else {
    h263_FlushBits(pInfo, h263_cbpy[code].len);
    return H263_STATUS_OK;
  }
}

#define h263_Decode_CBPC_B(pInfo, cbpc) \
{ \
  Ipp32u code; \
  h263_VLC1 vlc; \
  code = h263_ShowBits9(pInfo, 3); \
  vlc = h263_Bmb_cbpc[code]; \
  cbpc = vlc.code; \
  h263_FlushBits(pInfo, vlc.len); \
}

#define h263_AdvI_PredMode(pInfo, predMode) { \
  if (h263_GetBit(pInfo)) { \
    if (h263_GetBit(pInfo)) \
      predMode = IPPVC_SCAN_VERTICAL; \
    else \
      predMode = IPPVC_SCAN_HORIZONTAL; \
  } else \
    predMode = IPPVC_SCAN_ZIGZAG; \
}

#define H263_MB_INTERNAL_HOR_DEBLOCKING_LUM(pYc, stepYc, quant) \
{ \
  ippiFilterDeblocking8x8HorEdge_H263_8u_C1IR(pYc + 8*stepYc, stepYc, quant); \
  ippiFilterDeblocking8x8HorEdge_H263_8u_C1IR(pYc + 8 + 8*stepYc, stepYc, quant); \
}

#define H263_MB_INTERNAL_VER_DEBLOCKING_LUM(pYc, stepYc, quant) \
{ \
  ippiFilterDeblocking8x8VerEdge_H263_8u_C1IR(pYc + 8, stepYc, quant); \
  ippiFilterDeblocking8x8VerEdge_H263_8u_C1IR(pYc + 8 + 8*stepYc, stepYc, quant); \
}

#define H263_MB_HOR_DEBLOCKING(pYc, stepYc, pCbc, stepCbc, pCrc, stepCrc, quant, quant_c) \
{ \
  ippiFilterDeblocking8x8HorEdge_H263_8u_C1IR(pYc, stepYc, quant); \
  ippiFilterDeblocking8x8HorEdge_H263_8u_C1IR(pYc + 8, stepYc, quant); \
  ippiFilterDeblocking8x8HorEdge_H263_8u_C1IR(pCbc, stepCbc, quant_c); \
  ippiFilterDeblocking8x8HorEdge_H263_8u_C1IR(pCrc, stepCrc, quant_c); \
}

#define H263_MB_VER_DEBLOCKING(pYc, stepYc, pCbc, stepCbc, pCrc, stepCrc, quant, quant_c) \
{ \
  ippiFilterDeblocking8x8VerEdge_H263_8u_C1IR(pYc, stepYc, quant); \
  ippiFilterDeblocking8x8VerEdge_H263_8u_C1IR(pYc + 8*stepYc, stepYc, quant); \
  ippiFilterDeblocking8x8VerEdge_H263_8u_C1IR(pCbc, stepCbc, quant_c); \
  ippiFilterDeblocking8x8VerEdge_H263_8u_C1IR(pCrc, stepCrc, quant_c); \
}

extern void h263_DecodeMODB_iPB(h263_Info* pInfo, Ipp32s *bmb_type, Ipp32s *cbpb, Ipp32s *mvdb);
extern h263_Status h263_PredictDecodeMV(h263_Info *pInfo, h263_MacroBlock *MBcurr, Ipp32s frGOB, Ipp32s y, Ipp32s x, Ipp32s adv);
extern h263_Status h263_DecodeMVD(h263_Info *pInfo, Ipp32s *mvdx,  Ipp32s *mvdy, Ipp32s fcode);
extern h263_Status h263_DecodeMV(h263_Info *pInfo, IppMotionVector *mv, Ipp32s leftlim, Ipp32s rightlim);
extern h263_Status h263_DecodeMVD_umvplus(h263_Info *pInfo, Ipp32s *mvdx, Ipp32s *mvdy);
extern h263_Status h263_DecodeMacroBlockIntra(h263_Info *pInfo, Ipp32s pat, Ipp32s quant, Ipp32s quant_c, Ipp8u *pR[], Ipp32s stepR[]);
extern h263_Status h263_DecodeMacroBlockIntra_AdvI(h263_Info *pInfo, Ipp32s x, Ipp32s pat, Ipp32s quant, Ipp32s quant_c,
                                                   Ipp32s scan, Ipp8u *pR[], Ipp32s stepR[]);
extern h263_Status h263_DecodeMacroBlockInter(h263_Info *pInfo, Ipp16s *coeffMB, Ipp32s quant, Ipp32s quant_c, Ipp32s pat);
extern h263_Status h263_BidirPredMacroblock(Ipp8u *pYr, Ipp32s stepYr, Ipp8u *pCbr, Ipp8u* pCrr, Ipp32s stepCbr,
                                            Ipp8u *pYc, Ipp32s stepYc, Ipp8u *pCbc, Ipp8u* pCrc, Ipp32s stepCbc,
                                            Ipp8u *predMB, IppMotionVector *mvForw, IppMotionVector *mvFCbCr,
                                            IppMotionVector *mvBack,  IppMotionVector *mvBCbCr, Ipp32s fourMVmode);
extern h263_Status h263_DecodeVideoPicture(h263_Info* pInfo);
extern h263_Status h263_DecodeMCBPC_P(h263_Info* pInfo, Ipp32s *mbType, Ipp32s *mbPattern, Ipp32s stat);
extern h263_Status h263_PredictDecode4MV(h263_Info *pInfo, h263_MacroBlock *MBcurr, Ipp32s frGOB, Ipp32s y, Ipp32s x);
extern void h263_OBMC(h263_Info *pInfo, h263_MacroBlock *pMBinfo, IppMotionVector *mvCur, Ipp32s colNum, Ipp32s rowNum,
                      IppiRect limitRectL, Ipp8u *pYc, Ipp32s stepYc, Ipp8u *pYr, Ipp32s stepYr, Ipp32s cbpy, Ipp16s *coeffMB);
extern h263_Status h263_DecodeFrame_I(h263_Info* pInfo);
extern h263_Status h263_DecodeFrame_P(h263_Info* pInfo, h263_Frame* refFrame);
extern h263_Status h263_DecodeFrame_B(h263_Info* pInfo, h263_EnhancedLayer *Layer, h263_Frame* refFrame);
extern h263_Status h263_DecodeFrame_EI(h263_Info* pInfo, h263_EnhancedLayer *Layer);
extern h263_Status h263_DecodeFrame_EP(h263_Info* pInfo, h263_EnhancedLayer *Layer, h263_Frame* refFrame);
extern Ipp32s h263_UpdateQuant_Mod(h263_Info *pInfo);
extern void h263_DeblockingFilter_P(h263_Info *pInfo, h263_Frame *frame);
extern void h263_DeblockingFilter_I(h263_Info *pInfo, h263_Frame *frame);
extern h263_Status h263_DecodeMBTYPE_B(h263_Info* pInfo, Ipp32s *mbType, Ipp32s *cbp, Ipp32s *dquant);
extern h263_Status h263_DecodeMCBPC_EI(h263_Info* pInfo, Ipp32s *mbType, Ipp32s *mbPattern);
extern h263_Status h263_DecodeMBTYPE_EP(h263_Info* pInfo, Ipp32s *mbType, Ipp32s *cbp, Ipp32s *dquant);
extern void h263_CopyMacroBlocks(h263_Frame *rFrame, h263_Frame *cFrame, Ipp32s mbPerRow, Ipp32s rowNum, Ipp32s colNum, Ipp32s n);

#ifndef UMC_H263_DISABLE_SORENSON
extern IppStatus h263_ReconstructCoeffsIntraSorenson_H263_1u16s(
  Ipp8u** ppBitStream,
  int*    pBitOffset,
  Ipp16s* pCoef,
  int*    pIndxLastNonZero,
  int     cbp,
  int     QP);

extern IppStatus h263_ReconstructCoeffsInterSorenson_H263_1u16s(
  Ipp8u** ppBitStream,
  int*    pBitOffset,
  Ipp16s* pCoef,
  int*    pIndxLastNonZero,
  int     QP);
#endif

#ifdef _OMP_KARABAS

#define H263_NUM_THREADS 2

extern h263_Status h263_DecodeFrame_P_MT(h263_Info* pInfo, h263_Frame *refFrame);
extern h263_Status h263_DecodeFrame_I_MT(h263_Info* pInfo);
extern h263_Status h263_DecodeDequantMacroBlockIntra(h263_Info *pInfo, Ipp32s x, Ipp32s pat, Ipp32s quant, Ipp32s quant_c,
                                                     Ipp32s scan, Ipp16s *coeffMB, Ipp8u lastNZ[]);
extern h263_Status h263_DecodeDequantMacroBlockInter(h263_Info *pInfo, Ipp32s pat, Ipp32s quant, Ipp32s quant_c,
                                                     Ipp16s *coeffMB, Ipp8u lastNZ[]);
extern void h263_DCT_MacroBlockInter(Ipp16s *coeffMB, Ipp8u lastNZ[], Ipp32s pat);
extern void h263_DeblockingFilterHor_P_MT(h263_Info *pInfo, Ipp32s curRow);
extern void h263_DeblockingFilterVer_P_MT(h263_Info *pInfo, Ipp32s curRow);
extern void h263_DeblockingFilterHor_I_MT(h263_Info *pInfo, Ipp32s curRow);
extern void h263_DeblockingFilterVer_I_MT(h263_Info *pInfo, Ipp32s curRow);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* _H263DEC_H_ */
