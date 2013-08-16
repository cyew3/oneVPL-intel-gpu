/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2009 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    External EC over GF(p^k) Functions
//
//    Context:
//       ippsGFPXECPointGetSize()
//       ippsGFPXECPointInit()
//
//       ippsGFPXECSetPointAtInfinity()
//       ippsGFPXECSetPoint()
//       ippsGFPXECSetPointRandom()
//       ippsGFPXECCpyPoint()
//       ippsGFPXECGetPoint()
//
//       ippsGFPXECCmpPoint()
//       ippsGFPXECNegPoint()
//       ippsGFPXECAddPoint()
//       ippsGFPXECMulPointScalar()
//       ippsGFPXECVerifyPoint()
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpecgfpx.h"


IPPFUN(IppStatus, ippsGFPXECPointGetSize,(const IppsGFPXECState* pEC, Ipp32u* pSizeInBytes))
{
   IPP_BAD_PTR2_RET(pEC, pSizeInBytes);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );

   {
      Ipp32u elementSize = GFPX_FESIZE(ECPX_FIELD(pEC));
      *pSizeInBytes = sizeof(IppsGFPXECPoint)
                     +elementSize*sizeof(Ipp32u) /* X */
                     +elementSize*sizeof(Ipp32u) /* Y */
                     +elementSize*sizeof(Ipp32u);/* Z */
      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPXECPointInit,(IppsGFPXECPoint* pPoint,
                                 const IppsGFPXElement* pX, const IppsGFPXElement* pY,
                                       IppsGFPXECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );

   {
      Ipp8u* ptr = (Ipp8u*)pPoint;
      Ipp32u elementSize = GFPX_FESIZE(ECPX_FIELD(pEC));

      ECPX_POINT_ID(pPoint) = idCtxGFPXECPoint;
      ECPX_POINT_FLAGS(pPoint) = 0;
      ECPX_POINT_FESIZE(pPoint) = elementSize;
      ptr += sizeof(IppsGFPXECPoint);
      ECPX_POINT_DATA(pPoint) = (cpFFchunk*)(ptr);

      if(pX && pY)
         return ippsGFPXECSetPoint(pX, pY, pPoint, pEC);
      else {
         ecgfpxSetProjectivePointAtInfinity(pPoint, elementSize);
         return ippStsNoErr;
      }
   }
}


IPPFUN(IppStatus, ippsGFPXECSetPointAtInfinity,(IppsGFPXECPoint* pPoint, IppsGFPXECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   ecgfpxSetProjectivePointAtInfinity(pPoint, GFPX_FESIZE(ECPX_FIELD(pEC)));
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPXECSetPoint,(const IppsGFPXElement* pX, const IppsGFPXElement* pY,
                                            IppsGFPXECPoint* pPoint,
                                            IppsGFPXECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   IPP_BAD_PTR2_RET(pX, pY);
   IPP_BADARG_RET( !GFPXE_TEST_ID(pX), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pY), ippStsContextMatchErr );

   ecgfpxSetAffinePoint(GFPXE_DATA(pX), GFPXE_DATA(pY), pPoint, pEC);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPXECSetPointRandom,(IppsGFPXECPoint* pPoint,
                                            IppsGFPXECState* pEC,
                                            IppBitSupplier rndFunc, void* pRndParam))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   IPP_BAD_PTR1_RET(rndFunc);

   {
      IppsGFPXState* pGFX = ECPX_FIELD(pEC);
      IppsGFPState* pGF = GFPX_GROUNDGF(pGFX);

      /* allocate random exponent */
      Ipp32u exponentSize = GFP_FESIZE(pGF);
      cpFFchunk* pE = gfpGetPool(1, pGF);

      /* setup base point */
      IppsGFPXECPoint G;
      ecgfpxNewPoint(&G, ECPX_G(pEC),ECPX_AFFINE_POINT|ECPX_FINITE_POINT, pEC);

      /* random exponent */
      gfpRand(pE, pGF, rndFunc, pRndParam);

      /* compute random point */
      ecgfpxMulPoint(&G, pE,exponentSize, pPoint, pEC);

      gfpReleasePool(1, pGF);

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPXECGetPoint,(const IppsGFPXECPoint* pPoint,
                                            IppsGFPXElement* pX, IppsGFPXElement* pY,
                                            IppsGFPXECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   IPP_BADARG_RET( !IS_ECPX_FINITE_POINT(pPoint), ippStsPointAtInfinity);

   IPP_BADARG_RET( pX && !GFPXE_TEST_ID(pX), ippStsContextMatchErr );
   IPP_BADARG_RET( pY && !GFPXE_TEST_ID(pY), ippStsContextMatchErr );

   ecgfpxGetAffinePoint(pPoint, GFPXE_DATA(pX), GFPXE_DATA(pY), pEC);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPXECCpyPoint,(const IppsGFPXECPoint* pA,
                                            IppsGFPXECPoint* pR,
                                            IppsGFPXECState* pEC))
{
   IPP_BAD_PTR3_RET(pA, pR, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pR), ippStsContextMatchErr );

   ecgfpxCopyPoint(pA, pR, GFPX_FESIZE(ECPX_FIELD(pEC)));
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPXECCmpPoint,(const IppsGFPXECPoint* pP, const IppsGFPXECPoint* pQ,
                                            IppsElementCmpResult* pResult,
                                            IppsGFPXECState* pEC))
{
   IPP_BAD_PTR4_RET(pP, pQ, pResult, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pQ), ippStsContextMatchErr );

   *pResult = ecgfpxIsPointEquial(pP, pQ, pEC)? IppsElementEQ: IppsElementNE;
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPXECVerifyPoint,(const IppsGFPXECPoint* pP,
                                               IppECResult* pResult,
                                               IppsGFPXECState* pEC))
{
   IPP_BAD_PTR3_RET(pP, pResult, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pP), ippStsContextMatchErr );

   {
      Ipp32u elementSize = GFPX_FESIZE(ECPX_FIELD(pEC));

      if( ecgfpxIsProjectivePointAtInfinity(pP, elementSize) )
         *pResult = ippECPointIsAtInfinite;
      else if( !ecgfpxIsPointOnCurve(pP, pEC) )
         *pResult = ippECPointIsNotValid;
      else {
         IppsGFPXECPoint T;
         ecgfpxNewPoint(&T, ecgfpxGetPool(1, pEC),0, pEC);
         ecgfpxMulPoint(pP, ECPX_R(pEC), BITS2WORD32_SIZE(ECPX_ORDBITSIZE(pEC)), &T, pEC);
         *pResult = ecgfpxIsProjectivePointAtInfinity(&T, elementSize)? ippECValid : ippECPointOutOfGroup;
         ecgfpxReleasePool(1, pEC);
      }

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPXECNegPoint,(const IppsGFPXECPoint* pP,
                                            IppsGFPXECPoint* pR,
                                            IppsGFPXECState* pEC))
{
   IPP_BAD_PTR3_RET(pP, pR, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pR), ippStsContextMatchErr );

   ecgfpxNegPoint(pP, pR, pEC);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPXECAddPoint,(const IppsGFPXECPoint* pP, const IppsGFPXECPoint* pQ, IppsGFPXECPoint* pR,
                  IppsGFPXECState* pEC))
{
   IPP_BAD_PTR4_RET(pP, pQ, pR, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pQ), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pR), ippStsContextMatchErr );

   ecgfpxAddPoint(pP, pQ, pR, pEC);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPXECMulPointScalar,(const IppsGFPXECPoint* pP,
                                            const Ipp32u* pN, Ipp32u nSize,
                                                  IppsGFPXECPoint* pR,
                                                  IppsGFPXECState* pEC))
{
   IPP_BAD_PTR3_RET(pP, pR, pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pR), ippStsContextMatchErr );

   IPP_BAD_PTR1_RET(pN);

   ecgfpxMulPoint(pP, pN, nSize, pR, pEC);
   return ippStsNoErr;
}
