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
//    External EC over GF(p) Functions
//
//    Context:
//       ippsECGFpPointGetSize()
//       ippsECGFpPointInit()
//
//       ippsGFPECSetPointAtInfinity()
//       ippsGFPECSetPoint()
//       ippsGFPECSetPointRandom()
//       ippsGFPECGetPoint()
//       ippsGFPECCpyPoint()
//
//       ippsGFPECCmpPoint()
//       ippsGFPECNegPoint()
//       ippsGFPECAddPoint()
//       ippsGFPECMulPointScalar()
//       ippsGFPECVerifyPoint()
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpecgfp.h"


IPPFUN(IppStatus, ippsGFPECPointGetSize,(const IppsGFPECState* pEC, Ipp32u* pSizeInBytes))
{
   IPP_BAD_PTR2_RET(pEC, pSizeInBytes);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );

   {
      Ipp32u elementSize = GFP_FESIZE(ECP_FIELD(pEC));
      *pSizeInBytes = sizeof(IppsGFPECPoint)
                     +elementSize*sizeof(Ipp32u) /* X */
                     +elementSize*sizeof(Ipp32u) /* Y */
                     +elementSize*sizeof(Ipp32u);/* Z */
      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPECPointInit,(IppsGFPECPoint* pPoint,
                                const IppsGFPElement* pX, const IppsGFPElement* pY,
                                      IppsGFPECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );

   {
      Ipp8u* ptr = (Ipp8u*)pPoint;
      Ipp32u elementSize = GFP_FESIZE(ECP_FIELD(pEC));

      ECP_POINT_ID(pPoint) = idCtxGFPPoint;
      ECP_POINT_FLAGS(pPoint) = 0;
      ECP_POINT_FESIZE(pPoint) = elementSize;
      ptr += sizeof(IppsGFPECPoint);
      ECP_POINT_DATA(pPoint) = (cpFFchunk*)(ptr);

      if(pX && pY)
         return ippsGFPECSetPoint(pX, pY, pPoint, pEC);
      else {
         ecgfpSetProjectivePointAtInfinity(pPoint, elementSize);
         return ippStsNoErr;
      }
   }
}


IPPFUN(IppStatus, ippsGFPECSetPointAtInfinity,(IppsGFPECPoint* pPoint, IppsGFPECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   ecgfpSetProjectivePointAtInfinity(pPoint, GFP_FESIZE(ECP_FIELD(pEC)));
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPECSetPoint,(const IppsGFPElement* pX, const IppsGFPElement* pY,
                                           IppsGFPECPoint* pPoint,
                                           IppsGFPECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   IPP_BAD_PTR2_RET(pX, pY);
   IPP_BADARG_RET( !GFPE_TEST_ID(pX), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pY), ippStsContextMatchErr );

   ecgfpSetAffinePoint(GFPE_DATA(pX), GFPE_DATA(pY), pPoint, pEC);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPECSetPointRandom,(IppsGFPECPoint* pPoint,
                                           IppsGFPECState* pEC,
                                           IppBitSupplier rndFunc, void* pRndParam))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   IPP_BAD_PTR1_RET(rndFunc);

   {
      IppsGFPState* pGF = ECP_FIELD(pEC);
      Ipp32u elementSize = GFP_FESIZE(pGF);

      cpFFchunk* pX = gfpGetPool(1, pGF);
      cpFFchunk* pY = gfpGetPool(1, pGF);

      do {
         /* get random X */
         gfpRand(pX, pGF, rndFunc, pRndParam);

         /* compute T = X^3+A*X +B */
         gfpMontMul(pX, pX, pY, pGF);
         gfpAdd(pY, ECP_A(pEC), pY, pGF);
         gfpMontMul(pX, pY, pY, pGF);
         gfpAdd(pY, ECP_B(pEC), pY, pGF);
         /* compute square root */
      } while( !gfpMontSqrt(pY, pY, pGF) );

      /* R = (X,Y) */
      gfpFFelementCopy(pX, ECP_POINT_X(pPoint), elementSize);
      gfpFFelementCopy(pY, ECP_POINT_Y(pPoint), elementSize);
      gfpFFelementCopy(GFP_MONTUNITY(pGF), ECP_POINT_Z(pPoint), elementSize);
      ECP_POINT_FLAGS(pPoint) = ECP_AFFINE_POINT | ECP_FINITE_POINT;

      gfpReleasePool(2, pGF);

      /* R = cofactor*R */
      ecgfpMulPoint(pPoint, &ECP_COFACTOR(pEC), 1, pPoint, pEC);

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPECGetPoint,(const IppsGFPECPoint* pPoint,
                                           IppsGFPElement* pX, IppsGFPElement* pY,
                                           IppsGFPECState* pEC))
{
   IPP_BAD_PTR2_RET(pPoint, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pPoint), ippStsContextMatchErr );

   IPP_BADARG_RET( !IS_ECP_FINITE_POINT(pPoint), ippStsPointAtInfinity);

   IPP_BADARG_RET( pX && !GFPE_TEST_ID(pX), ippStsContextMatchErr );
   IPP_BADARG_RET( pY && !GFPE_TEST_ID(pY), ippStsContextMatchErr );

   ecgfpGetAffinePoint(pPoint, (pX)? GFPE_DATA(pX):0, (pY)?GFPE_DATA(pY):0, pEC);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPECCpyPoint,(const IppsGFPECPoint* pA,
                                           IppsGFPECPoint* pR,
                                           IppsGFPECState* pEC))
{
   IPP_BAD_PTR3_RET(pA, pR, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pR), ippStsContextMatchErr );

   ecgfpCopyPoint(pA, pR, GFP_FESIZE(ECP_FIELD(pEC)));
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPECCmpPoint,(const IppsGFPECPoint* pP, const IppsGFPECPoint* pQ,
                                           IppsElementCmpResult* pResult,
                                           IppsGFPECState* pEC))
{
   IPP_BAD_PTR4_RET(pP, pQ, pResult, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pQ), ippStsContextMatchErr );

   *pResult = ecgfpIsPointEquial(pP, pQ, pEC)? IppsElementEQ: IppsElementNE;
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPECVerifyPoint,(const IppsGFPECPoint* pP,
                                              IppECResult* pResult,
                                              IppsGFPECState* pEC))
{
   IPP_BAD_PTR3_RET(pP, pResult, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pP), ippStsContextMatchErr );

   {
      Ipp32u elementSize = GFP_FESIZE(ECP_FIELD(pEC));

      if( ecgfpIsProjectivePointAtInfinity(pP, elementSize) )
         *pResult = ippECPointIsAtInfinite;
      else if( !ecgfpIsPointOnCurve(pP, pEC) )
         *pResult = ippECPointIsNotValid;
      else {
         IppsGFPECPoint T;
         ecgfpNewPoint(&T, ecgfpGetPool(1, pEC),0, pEC);
         ecgfpMulPoint(pP, ECP_R(pEC), BITS2WORD32_SIZE(ECP_ORDBITSIZE(pEC)), &T, pEC);
         *pResult = ecgfpIsProjectivePointAtInfinity(&T, elementSize)? ippECValid : ippECPointOutOfGroup;
         ecgfpReleasePool(1, pEC);
      }

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPECNegPoint,(const IppsGFPECPoint* pP,
                                           IppsGFPECPoint* pR,
                                           IppsGFPECState* pEC))
{
   IPP_BAD_PTR3_RET(pP, pR, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pR), ippStsContextMatchErr );

   ecgfpNegPoint(pP, pR, pEC);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPECAddPoint,(const IppsGFPECPoint* pP, const IppsGFPECPoint* pQ, IppsGFPECPoint* pR,
                  IppsGFPECState* pEC))
{
   IPP_BAD_PTR4_RET(pP, pQ, pR, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pQ), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pR), ippStsContextMatchErr );

   ecgfpAddPoint(pP, pQ, pR, pEC);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPECMulPointScalar,(const IppsGFPECPoint* pP,
                                           const Ipp32u* pN, Ipp32u nSize,
                                                 IppsGFPECPoint* pR,
                                                 IppsGFPECState* pEC))
{
   IPP_BAD_PTR3_RET(pP, pR, pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pP), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pR), ippStsContextMatchErr );

   IPP_BAD_PTR1_RET(pN);

   ecgfpMulPoint(pP, pN, nSize, pR, pEC);
   return ippStsNoErr;
}
