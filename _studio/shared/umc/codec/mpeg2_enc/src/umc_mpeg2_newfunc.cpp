//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2000-2009 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include <stdlib.h>
#include "ippi.h"
#include "ippvc.h"
#include "umc_mpeg2_newfunc.h"

#define   IPPFUN(type,name,arg)                extern type __STDCALL name arg

#define IPP_BADARG_RET( expr, ErrCode )

#define IPP_BAD_SIZE_RET( n )\
  IPP_BADARG_RET( (n)<=0, ippStsSizeErr )

#define IPP_BAD_STEP_RET( n )\
  IPP_BADARG_RET( (n)<=0, ippStsStepErr )

#define IPP_BAD_PTR1_RET( ptr )\
  IPP_BADARG_RET( NULL==(ptr), ippStsNullPtrErr )

#define IPP_BAD_PTR2_RET( ptr1, ptr2 )\
  IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))), ippStsNullPtrErr)

#define IPP_BAD_PTR3_RET( ptr1, ptr2, ptr3 )\
  IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))||(NULL==(ptr3))),\
  ippStsNullPtrErr)

#define IPP_BAD_PTR4_RET( ptr1, ptr2, ptr3, ptr4 )\
                {IPP_BAD_PTR2_RET( ptr1, ptr2 ); IPP_BAD_PTR2_RET( ptr3, ptr4 )}


IPPFUN (IppStatus, tmp_ippiDCT8x8FwdUV_8u16s_C1R,
        ( const Ipp8u* pSrc, int srcStep, Ipp16s* pDst ))
{
  IPP_BAD_PTR2_RET ( pSrc, pDst );
  IPP_BAD_STEP_RET ( srcStep );

  Ipp8u src[8*16];
  int i, j;
  for(i=0; i<8; i++) {
    for(j=0; j<8; j++)
      src[j+i*16] = pSrc[j*2+i*srcStep];
  }

  return ippiDCT8x8Fwd_8u16s_C1R( src, 16, pDst );
}

IPPFUN (IppStatus, tmp_ippiDCT8x8InvUV_16s8u_C1R,
        ( const Ipp16s* pSrc, Ipp8u* pDst, int dstStep ))
{
  IPP_BAD_PTR2_RET ( pSrc, pDst );
  IPP_BAD_STEP_RET ( dstStep );

  Ipp8u dst[8*16];
  int i, j;
  IppStatus ret;

  ret = ippiDCT8x8Inv_16s8u_C1R ( pSrc, dst, 16 );
  for(i=0; i<8; i++) {
    for(j=0; j<8; j++)
      pDst[j*2+i*dstStep] = dst[j+i*16];
  }
  return ret;
}



IPPFUN(IppStatus, tmp_ippiGetDiff8x4_8u16s_C2P2, (const Ipp8u *pSrcCur,
       Ipp32s srcCurStep,
       const Ipp8u *pSrcRef,
       Ipp32s srcRefStep,
       Ipp16s *pDstDiff,
       Ipp32s dstDiffStep,
       Ipp16s *pDstPredictor,
       Ipp32s dstPredictorStep,
       Ipp32s mcType,
       Ipp32s roundControl))
{
  /* check error(s) */
  IPP_BAD_PTR3_RET(pSrcCur, pSrcRef,  pDstDiff);

  Ipp8u src[16*16], ref[16*16];
  int i, j;
  for(i=0; i<4; i++) {
    for(j=0; j<8; j++)
      src[j+i*16] = pSrcCur[j*2+i*srcCurStep];
  }
  for(i=0; i<((mcType&4)?5:4); i++) {
    for(j=0; j<((mcType&8)?9:8); j++)
      ref[j+i*16] = pSrcRef[j*2+i*srcRefStep];
  }

  return ippiGetDiff8x4_8u16s_C1(src,16,ref,16,pDstDiff,dstDiffStep,pDstPredictor,dstPredictorStep,mcType,roundControl);

} /* IPPFUN(IppStatus, ippiGetDiff8x4_8u16s_C1, (const Ipp8u *pSrcCur, */

IPPFUN(IppStatus, tmp_ippiGetDiff8x4B_8u16s_C2P2, (const Ipp8u *pSrcCur,
       Ipp32s srcCurStep,
       const Ipp8u *pSrcRefF,
       Ipp32s srcRefStepF,
       Ipp32s mcTypeF,
       const Ipp8u *pSrcRefB,
       Ipp32s srcRefStepB,
       Ipp32s mcTypeB,
       Ipp16s *pDstDiff,
       Ipp32s dstDiffStep,
       Ipp32s roundControl))
{
  /* check error(s) */
  IPP_BAD_PTR4_RET(pSrcCur, pSrcRefF, pSrcRefB,  pDstDiff);

  Ipp8u src[16*16], ref[16*16], refB[16*16];
  int i, j;
  for(i=0; i<4; i++) {
    for(j=0; j<8; j++)
      src[j+i*16] = pSrcCur[j*2+i*srcCurStep];
  }
  for(i=0; i<((mcTypeF&4)?5:4); i++) {
    for(j=0; j<((mcTypeF&8)?9:8); j++)
      ref[j+i*16] = pSrcRefF[j*2+i*srcRefStepF];
  }
  for(i=0; i<((mcTypeB&4)?5:4); i++) {
    for(j=0; j<((mcTypeB&8)?9:8); j++)
      refB[j+i*16] = pSrcRefB[j*2+i*srcRefStepB];
  }

  return ippiGetDiff8x4B_8u16s_C1(src,16,ref,16,mcTypeF,refB,16,mcTypeB,pDstDiff,dstDiffStep,roundControl);

} /* IPPFUN(IppStatus, ippiGetDiff8x4B_8u16s_C1, (const Ipp8u *pSrcCur, */

IPPFUN(IppStatus, tmp_ippiGetDiff8x8_8u16s_C2P2, (const Ipp8u *pSrcCur,
       Ipp32s srcCurStep,
       const Ipp8u *pSrcRef,
       Ipp32s srcRefStep,
       Ipp16s *pDstDiff,
       Ipp32s dstDiffStep,
       Ipp16s *pDstPredictor,
       Ipp32s dstPredictorStep,
       Ipp32s mcType,
       Ipp32s roundControl))
{
  /* check error(s) */
  IPP_BAD_PTR3_RET(pSrcCur, pSrcRef,  pDstDiff);

  Ipp8u src[16*16], ref[16*16];
  int i, j;
  for(i=0; i<8; i++) {
    for(j=0; j<8; j++)
      src[j+i*16] = pSrcCur[j*2+i*srcCurStep];
  }
  for(i=0; i<((mcType&4)?9:8); i++) {
    for(j=0; j<((mcType&8)?9:8); j++)
      ref[j+i*16] = pSrcRef[j*2+i*srcRefStep];
  }

  return ippiGetDiff8x8_8u16s_C1(src,16,ref,16,pDstDiff,dstDiffStep,pDstPredictor,dstPredictorStep,mcType,roundControl);

} /* IPPFUN(IppStatus, ippiGetDiff8x4_8u16s_C1, (const Ipp8u *pSrcCur, */

IPPFUN(IppStatus, tmp_ippiGetDiff8x8B_8u16s_C2P2, (const Ipp8u *pSrcCur,
       Ipp32s srcCurStep,
       const Ipp8u *pSrcRefF,
       Ipp32s srcRefStepF,
       Ipp32s mcTypeF,
       const Ipp8u *pSrcRefB,
       Ipp32s srcRefStepB,
       Ipp32s mcTypeB,
       Ipp16s *pDstDiff,
       Ipp32s dstDiffStep,
       Ipp32s roundControl))
{
  /* check error(s) */
  IPP_BAD_PTR4_RET(pSrcCur, pSrcRefF, pSrcRefB,  pDstDiff);

  Ipp8u src[16*16], ref[16*16], refB[16*16];
  int i, j;
  for(i=0; i<8; i++) {
    for(j=0; j<8; j++)
      src[j+i*16] = pSrcCur[j*2+i*srcCurStep];
  }
  for(i=0; i<((mcTypeF&4)?9:8); i++) {
    for(j=0; j<((mcTypeF&8)?9:8); j++)
      ref[j+i*16] = pSrcRefF[j*2+i*srcRefStepF];
  }
  for(i=0; i<((mcTypeB&4)?9:8); i++) {
    for(j=0; j<((mcTypeB&8)?9:8); j++)
      refB[j+i*16] = pSrcRefB[j*2+i*srcRefStepB];
  }

  return ippiGetDiff8x8B_8u16s_C1(src,16,ref,16,mcTypeF,refB,16,mcTypeB,pDstDiff,dstDiffStep,roundControl);

} /* IPPFUN(IppStatus, ippiGetDiff8x4B_8u16s_C1, (const Ipp8u *pSrcCur, */

IPPFUN(IppStatus, tmp_ippiMC8x8_8u_C2P2, (const Ipp8u *pSrcRef, Ipp32s srcStep,
       const Ipp16s *pSrcYData, Ipp32s srcYDataStep,
       Ipp8u *pDst, Ipp32s dstStep, Ipp32s mcType, Ipp32s roundControl))
{
  /* check error(s) */
  IPP_BAD_PTR3_RET(pSrcRef, pSrcYData,  pDst);

  IppStatus ret;
  Ipp8u dst[16*16], ref[16*16];
  int i, j;
  for(i=0; i<((mcType&4)?9:8); i++) {
    for(j=0; j<((mcType&8)?9:8); j++)
      ref[j+i*16] = pSrcRef[j*2+i*srcStep];
  }

  ret = ippiMC8x8_8u_C1(ref, 16,
    pSrcYData, srcYDataStep, dst, 16, mcType, roundControl);

  for(i=0; i<8; i++) {
    for(j=0; j<8; j++)
      pDst[j*2+i*dstStep] = dst[j+i*16];
  }
  return ret;

} /* IPPFUN(IppStatus, ippiMC8x8_8u_C1, (const Ipp8u *pSrcRef, Ipp32s srcStep, */

IPPFUN(IppStatus, tmp_ippiMC8x4_8u_C2P2, (const Ipp8u *pSrcRef, Ipp32s srcStep,
       const Ipp16s *pSrcYData, Ipp32s srcYDataStep,
       Ipp8u *pDst, Ipp32s dstStep, Ipp32s mcType, Ipp32s roundControl))
{
  /* check error(s) */
  IPP_BAD_PTR3_RET(pSrcRef, pSrcYData,  pDst);

  IppStatus ret;
  Ipp8u dst[16*16], ref[16*16];
  int i, j;
  for(i=0; i<((mcType&4)?5:4); i++) {
    for(j=0; j<((mcType&8)?9:8); j++)
      ref[j+i*16] = pSrcRef[j*2+i*srcStep];
  }

  ret = ippiMC8x4_8u_C1(ref, 16,
    pSrcYData, srcYDataStep, dst, 16, mcType, roundControl);

  for(i=0; i<4; i++) {
    for(j=0; j<8; j++)
      pDst[j*2+i*dstStep] = dst[j+i*16];
  }
  return ret;

} /* IPPFUN(IppStatus, ippiMC8x4_8u_C1, (const Ipp8u *pSrcRef, Ipp32s srcStep, */

IPPFUN(IppStatus, tmp_ippiMC8x8B_8u_C2P2, (const Ipp8u *pSrcRefF, Ipp32s srcStepF, Ipp32s mcTypeF,
       const Ipp8u *pSrcRefB, Ipp32s srcStepB, Ipp32s mcTypeB,
       const Ipp16s *pSrcYData, Ipp32s srcYDataStep,
       Ipp8u *pDst, Ipp32s dstStep, Ipp32s roundControl))
{
  /* check error(s) */
  IPP_BAD_PTR4_RET(pSrcRefF, pSrcRefB, pSrcYData,  pDst);

  IppStatus ret;
  Ipp8u dst[16*16], ref[16*16], refB[16*16];
  int i, j;
  for(i=0; i<((mcTypeF&4)?9:8); i++) {
    for(j=0; j<((mcTypeF&8)?9:8); j++)
      ref[j+i*16] = pSrcRefF[j*2+i*srcStepF];
  }
  for(i=0; i<((mcTypeB&4)?9:8); i++) {
    for(j=0; j<((mcTypeB&8)?9:8); j++)
      refB[j+i*16] = pSrcRefB[j*2+i*srcStepB];
  }

  ret = ippiMC8x8B_8u_C1(ref, 16, mcTypeF, refB, 16, mcTypeB,
    pSrcYData, srcYDataStep, dst, 16, roundControl);

  for(i=0; i<8; i++) {
    for(j=0; j<8; j++)
      pDst[j*2+i*dstStep] = dst[j+i*16];
  }
  return ret;

} /* IPPFUN(IppStatus, ippiMC8x8B_8u_C1, (const Ipp8u *pSrcRefF, Ipp32s srcStepF, Ipp32s mcTypeF, */

IPPFUN(IppStatus, tmp_ippiMC8x4B_8u_C2P2, (const Ipp8u *pSrcRefF, Ipp32s srcStepF, Ipp32s mcTypeF,
       const Ipp8u *pSrcRefB, Ipp32s srcStepB, Ipp32s mcTypeB,
       const Ipp16s *pSrcYData, Ipp32s srcYDataStep,
       Ipp8u *pDst, Ipp32s dstStep, Ipp32s roundControl))
{

  /* check error(s) */
  IPP_BAD_PTR4_RET(pSrcRefF, pSrcRefB, pSrcYData,  pDst);

  IppStatus ret;
  Ipp8u dst[16*16], ref[16*16], refB[16*16];
  int i, j;
  for(i=0; i<((mcTypeF&4)?5:4); i++) {
    for(j=0; j<((mcTypeF&8)?9:8); j++)
      ref[j+i*16] = pSrcRefF[j*2+i*srcStepF];
  }
  for(i=0; i<((mcTypeB&4)?5:4); i++) {
    for(j=0; j<((mcTypeB&8)?9:8); j++)
      refB[j+i*16] = pSrcRefB[j*2+i*srcStepB];
  }

  ret = ippiMC8x4B_8u_C1(ref, 16, mcTypeF, refB, 16, mcTypeB,
    pSrcYData, srcYDataStep, dst, 16, roundControl);

  for(i=0; i<4; i++) {
    for(j=0; j<8; j++)
      pDst[j*2+i*dstStep] = dst[j+i*16];
  }
  return ret;

} /* IPPFUN(IppStatus, ippiMC8x4B_8u_C1, (const Ipp8u *pSrcRefF, Ipp32s srcStepF, Ipp32s mcTypeF, */




IPPFUN(IppStatus, tmp_ippiSAD8x8_8u32s_C2R, (
    const Ipp8u* pSrcCur,
    int          srcCurStep,
    const Ipp8u* pSrcRef,
    int          srcRefStep,
    Ipp32s*      pDst,
    Ipp32s       mcType))
{
    int     i, j, r;
    IPP_BAD_PTR3_RET(pSrcCur, pSrcRef, pDst);

    r = 0;
    if (mcType == 0) {
        for (i = 0; i < 8; i ++, pSrcRef += srcRefStep, pSrcCur += srcCurStep)
            for (j = 0; j < 8; j ++)
                r += abs(pSrcCur[2*j] - (pSrcRef[2*j] /*>> 1*/));
    }
    else if (mcType == 4) {
        for (i = 0; i < 8; i ++, pSrcRef += srcRefStep, pSrcCur += srcCurStep)
            for (j = 0; j < 8; j ++)
                r += abs(pSrcCur[2*j] - ((pSrcRef[2*j] + pSrcRef[2*j+srcRefStep] + 1) >> 1));
    }
    else if (mcType == 8) {
        for (i = 0; i < 8; i ++, pSrcRef += srcRefStep, pSrcCur += srcCurStep)
            for (j = 0; j < 8; j ++)
                r += abs(pSrcCur[2*j] - ((pSrcRef[2*j] + pSrcRef[2*j+2] + 1) >> 1));
    }
    else if (mcType == 12) {
        for (i = 0; i < 8; i ++, pSrcRef += srcRefStep, pSrcCur += srcCurStep)
            for (j = 0; j < 8; j ++)
                r += abs(pSrcCur[2*j] - ((pSrcRef[2*j] + pSrcRef[2*j+2] + pSrcRef[2*j+srcRefStep] + pSrcRef[2*j+srcRefStep+2] + 2) >> 2));
    }
    *pDst = r;

    return ippStsNoErr;
}


IPPFUN ( IppStatus, tmp_ippiSet8x8UV_8u_C2R,
       ( Ipp8u value, Ipp8u* pDst, int dstStep )) {

    Ipp8u *dst = pDst;
    const int width = 8, height = 8;
    int h, n;

    IPP_BAD_PTR1_RET( pDst )

    for( h = 0; h < height; h++ ) {
        for( n = 0; n < width; n++ ) {
            dst[n*2] = value;
        }
        dst += dstStep;
    }

    return ippStsNoErr;
}
#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
