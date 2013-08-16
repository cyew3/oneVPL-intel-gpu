/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal EC over GF(p) methods
//
//    Context:
//       ecgfpGetAffinePoint()
//
//       ecgfpIsPointEquial()
//       ecgfpIsPointOnCurve()
//
//       ecgfpNegPoint()
//       ecgfpDblPoint()
//       ecgfpAddPoint()
//       ecgfpMulPoint()
//
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpbn.h"
#include "pcpecgfp.h"


Ipp32u ecgfpGetAffinePoint(const IppsGFPECPoint* pPoint, cpFFchunk* pX, cpFFchunk* pY, IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   //Ipp32u elementSize = GFP_FESIZE(pGF);

   if( !IS_ECP_FINITE_POINT(pPoint) ) {
      //GFP_ZERO(pX, elementSize);
      //if( GFP_IS_ZERO(ECP_B(pEC), elementSize) )
      //   GFP_ONE(pY, elementSize);
      //else
      //   GFP_ZERO(pY, elementSize);
      //return;
      return 0;
   }

   /* case Z == 1 */
   if( IS_ECP_AFFINE_POINT(pPoint) ) {
      if(pX)
         gfpMontDec(ECP_POINT_X(pPoint), pX, pGF);
      if(pY)
         gfpMontDec(ECP_POINT_Y(pPoint), pY, pGF);
   }

   /* case Z != 1 */
   else {
      /* T = (1/Z)*(1/Z) */
      cpFFchunk* pT    = gfpGetPool(1, pGF);
      cpFFchunk* pZinv = gfpGetPool(1, pGF);
      gfpMontInv(ECP_POINT_Z(pPoint), pZinv, pGF);
      gfpMontMul(pZinv, pZinv, pT, pGF);
      gfpMontDec(pT, pT, pGF);

      if(pX)
         gfpMontMul(ECP_POINT_X(pPoint), pT, pX, pGF);
      if(pY) {
         gfpMontMul(pZinv, pT, pT, pGF);
         gfpMontMul(ECP_POINT_Y(pPoint), pT, pY, pGF);
      }

      gfpReleasePool(2, pGF);
   }

   return 1;
}

int ecgfpIsPointEquial(const IppsGFPECPoint* pP, const IppsGFPECPoint* pQ, IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   Ipp32u elementSize = GFP_FESIZE(pGF);

   /* P or/and Q at Infinity */
   if( !IS_ECP_FINITE_POINT(pP) )
      return !IS_ECP_FINITE_POINT(pQ)? 1:0;
   if( !IS_ECP_FINITE_POINT(pQ) )
      return !IS_ECP_FINITE_POINT(pP)? 1:0;

   /* Px==Qx && Py==Qy && Pz==Qz */
   if(  GFP_EQ(ECP_POINT_Z(pP), ECP_POINT_Z(pQ), elementSize)
      &&GFP_EQ(ECP_POINT_X(pP), ECP_POINT_X(pQ), elementSize)
      &&GFP_EQ(ECP_POINT_Y(pP), ECP_POINT_Y(pQ), elementSize))
      return 1;

   else {
      int isEqu = 1;

      cpFFchunk* pPtmp = gfpGetPool(1, pGF);
      cpFFchunk* pQtmp = gfpGetPool(1, pGF);
      cpFFchunk* pPz   = gfpGetPool(1, pGF);
      cpFFchunk* pQz   = gfpGetPool(1, pGF);

      if(isEqu) {
         /* Px*Qz^2 ~ Qx*Pz^2 */
         if( IS_ECP_AFFINE_POINT(pQ) ) /* Ptmp = Px * Qz^2 */
            gfpFFelementCopy(ECP_POINT_X(pP), pPtmp, elementSize);
         else {
            gfpMontMul(ECP_POINT_Z(pQ), ECP_POINT_Z(pQ), pQz, pGF);
            gfpMontMul(pQz, ECP_POINT_X(pP), pPtmp, pGF);
         }
         if( IS_ECP_AFFINE_POINT(pP) ) /* Qtmp = Qx * Pz^2 */
            gfpFFelementCopy(ECP_POINT_X(pQ), pQtmp, elementSize);
         else {
            gfpMontMul(ECP_POINT_Z(pP), ECP_POINT_Z(pP), pPz, pGF);
            gfpMontMul(pPz, ECP_POINT_X(pQ), pQtmp, pGF);
         }
         isEqu = GFP_EQ(pPtmp, pQtmp, elementSize);
      }

      if(isEqu) {
         /* Py*Qz^3 ~ Qy*Pz^3 */
         if( IS_ECP_AFFINE_POINT(pQ) ) /* Ptmp = Py * Qz^3 */
            gfpFFelementCopy(ECP_POINT_Y(pP), pPtmp, elementSize);
         else {
            gfpMontMul(ECP_POINT_Z(pQ), pQz, pQz, pGF);
            gfpMontMul(pQz, ECP_POINT_Y(pP), pPtmp, pGF);
         }
         if( IS_ECP_AFFINE_POINT(pP) ) /* Qtmp = Qy * Pz^3 */
            gfpFFelementCopy(ECP_POINT_Y(pQ), pQtmp, elementSize);
         else {
            gfpMontMul(ECP_POINT_Z(pP), pPz, pPz, pGF);
            gfpMontMul(pPz, ECP_POINT_Y(pQ), pQtmp, pGF);
         }
         isEqu = GFP_EQ(pPtmp, pQtmp, elementSize);
      }

      gfpReleasePool(4, pGF);
      return isEqu;
   }
}

int ecgfpIsPointOnCurve(const IppsGFPECPoint* pPoint, IppsGFPECState* pEC)
{
   /* point at infinity belongs curve */
   if( !IS_ECP_FINITE_POINT(pPoint) )
      return 1;

   /* test that 0 == R = (Y^2) - (X^3 + A*X*(Z^4) + B*(Z^6)) */
   else {
      int isOnCurve = 0;

      IppsGFPState* pGF = ECP_FIELD(pEC);

      cpFFchunk* pX = ECP_POINT_X(pPoint);
      cpFFchunk* pY = ECP_POINT_Y(pPoint);
      cpFFchunk* pZ = ECP_POINT_Z(pPoint);

      cpFFchunk* pR = gfpGetPool(1, pGF);
      cpFFchunk* pT = gfpGetPool(1, pGF);

      gfpMontMul(pY, pY, pR, pGF);  /* R = Y^2 */
      gfpMontMul(pX, pX, pT, pGF);  /* T = X^3 */
      gfpMontMul(pX, pT, pT, pGF);
      gfpSub(pR, pT, pR, pGF);      /* R -= T */

      if( IS_ECP_AFFINE_POINT(pPoint) ) {
         gfpMontMul(pX, ECP_A(pEC), pT, pGF);   /* T = A*X */
         gfpSub(pR, pT, pR, pGF);               /* R -= T */
         gfpSub(pR, ECP_B(pEC), pR, pGF);       /* R -= B */
      }
      else {
         cpFFchunk* pZ4 = gfpGetPool(1, pGF);
         cpFFchunk* pZ6 = gfpGetPool(1, pGF);

         gfpMontMul(pZ, pZ, pZ6, pGF);       /* Z^2 */
         gfpMontMul(pZ6, pZ6, pZ4, pGF);     /* Z^4 */
         gfpMontMul(pZ6, pZ4, pZ6, pGF);     /* Z^6 */

         gfpMontMul(pZ4, pX, pZ4, pGF);         /* X*(Z^4) */
         gfpMontMul(pZ4, ECP_A(pEC), pZ4, pGF); /* A*X*(Z^4) */
         gfpMontMul(pZ6, ECP_B(pEC), pZ6, pGF); /* B*(Z^4) */

         gfpSub(pR, pZ4, pR, pGF);           /* R -= A*X*(Z^4) */
         gfpSub(pR, pZ6, pR, pGF);           /* R -= B*(Z^6)   */

         gfpReleasePool(2, pGF);
      }

      isOnCurve = GFP_IS_ZERO(pR, GFP_FESIZE(pGF));
      gfpReleasePool(2, pGF);
      return isOnCurve;
   }
}

IppsGFPECPoint* ecgfpNegPoint (const IppsGFPECPoint* pP, IppsGFPECPoint* pR, IppsGFPECState* pEC)
{
   Ipp32u elementSize = GFP_FESIZE(ECP_FIELD(pEC));

   if(pP!=pR)
      ecgfpCopyPoint(pP, pR, elementSize);

   if( IS_ECP_FINITE_POINT(pR) )
      gfpNeg(ECP_POINT_Y(pR), ECP_POINT_Y(pR), ECP_FIELD(pEC));
   return pR;
}

IppsGFPECPoint* ecgfpDblPoint (const IppsGFPECPoint* pP, IppsGFPECPoint* pR, IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   Ipp32u elementSize = GFP_FESIZE(pGF);

   /* P at infinity => R at infinity */
   if( !IS_ECP_FINITE_POINT(pP) )
      ecgfpSetProjectivePointAtInfinity(pR, elementSize);

   else {
      cpFFchunk* pX = ECP_POINT_X(pP);
      cpFFchunk* pY = ECP_POINT_Y(pP);
      cpFFchunk* pZ = ECP_POINT_Z(pP);

      cpFFchunk* pU = gfpGetPool(1, pGF);
      cpFFchunk* pM = gfpGetPool(1, pGF);
      cpFFchunk* pS = gfpGetPool(1, pGF);

      /* M = 3*X^2 + A*Z^4 */
      gfpMontMul(pX, pX, pU, pGF);
      gfpAdd(pU, pU, pM, pGF);
      gfpAdd(pU, pM, pM, pGF);
      if( IS_ECP_AFFINE_POINT(pP) )
         gfpAdd(ECP_A(pEC), pM, pM, pGF);
      else {
         gfpMontMul(pZ, pZ, pU, pGF);
         gfpMontMul(pU, pU, pU, pGF);
         gfpMontMul(ECP_A(pEC), pU, pU, pGF);
         gfpAdd(pM, pU, pM, pGF);
      }

      /* U = 2*Y */
      gfpAdd(pY, pY, pU, pGF);

      /* Rz = 2*Y*Z */
      if( IS_ECP_AFFINE_POINT(pP) )
         gfpFFelementCopy(pU, ECP_POINT_Z(pR), elementSize);
      else
         gfpMontMul(pU, pZ, ECP_POINT_Z(pR), pGF);

      /* S = X*(U^2) = 4*X*Y^2 */
      gfpMontMul(pU, pU, pU, pGF);
      gfpMontMul(pX, pU, pS, pGF);

      /* Rx = M^2 - 2*S */
      gfpMontMul(pM, pM, ECP_POINT_X(pR), pGF);
      gfpSub(ECP_POINT_X(pR), pS, ECP_POINT_X(pR), pGF);
      gfpSub(ECP_POINT_X(pR), pS, ECP_POINT_X(pR), pGF);

      /* U = (U^2)/2 = (16*Y^4)/2 = 8*Y^4 */
      gfpMontMul(pU, pU, pU, pGF);
      gfpHalve(pU, pU, pGF);

      /* Ry = M*(S - Rx) - U */
      gfpSub(pS, ECP_POINT_X(pR), pS, pGF);
      gfpMontMul(pM, pS, pS, pGF);
      gfpSub(pS, pU, ECP_POINT_Y(pR), pGF);

      //ECP_POINT_FLAGS(pR) = GFP_EQ(ECP_POINT_Z(pR), GFP_MONTUNITY(pGF), elementSize)? ECP_AFFINE_POINT|ECP_FINITE_POINT : ECP_FINITE_POINT;
      ECP_POINT_FLAGS(pR) = ECP_FINITE_POINT;

      gfpReleasePool(3, pGF);
   }

   return pR;
}

IppsGFPECPoint* ecgfpAddPoint (const IppsGFPECPoint* pPointP, const IppsGFPECPoint* pPointQ, IppsGFPECPoint* pPointR, IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   Ipp32u elementSize = GFP_FESIZE(pGF);

   /* test stupid call */
   if( pPointP == pPointQ )
      return ecgfpDblPoint(pPointP, pPointR, pEC);

   /* prevent operation with point at Infinity */
   if( !IS_ECP_FINITE_POINT(pPointP) )
      return ecgfpCopyPoint(pPointQ, pPointR, elementSize);
   if( !IS_ECP_FINITE_POINT(pPointQ) )
      return ecgfpCopyPoint(pPointP, pPointR, elementSize);

   /*
   // addition
   */
   {
      cpFFchunk* pU0 = ecgfpGetPool(2, pEC);
      cpFFchunk* pS0 = pU0 + elementSize;
      cpFFchunk* pW  = pS0 + elementSize;
      cpFFchunk* pU1 = pW  + elementSize;
      cpFFchunk* pS1 = pU1 + elementSize;
      cpFFchunk* pR  = pS1 + elementSize;
      cpFFchunk* pT  = pU0;
      cpFFchunk* pM  = pS0;

      /* coordinates of P */
      cpFFchunk* pPx = ECP_POINT_X(pPointP);
      cpFFchunk* pPy = ECP_POINT_Y(pPointP);
      cpFFchunk* pPz = ECP_POINT_Z(pPointP);

      /* coordinates of Q */
      cpFFchunk* pQx = ECP_POINT_X(pPointQ);
      cpFFchunk* pQy = ECP_POINT_Y(pPointQ);
      cpFFchunk* pQz = ECP_POINT_Z(pPointQ);

      /* U0 = Px * Qz^2 */
      /* S0 = Py * Qz^3 */
      if( IS_ECP_AFFINE_POINT(pPointQ) ) {
         gfpFFelementCopy(pPx, pU0, elementSize);
         gfpFFelementCopy(pPy, pS0, elementSize);
      }
      else {
         gfpMontMul(pQz, pQz, pU0, pGF);
         gfpMontMul(pQz, pU0, pS0, pGF);
         gfpMontMul(pU0, pPx, pU0, pGF);
         gfpMontMul(pS0, pPy, pS0, pGF);
      }

      /* U1 = Qx * Pz^2 */
      /* S1 = Qy * Pz^3 */
      if( IS_ECP_AFFINE_POINT(pPointP) ) {
         gfpFFelementCopy(pQx, pU1, elementSize);
         gfpFFelementCopy(pQy, pS1, elementSize);
      }
      else {
         gfpMontMul(pPz, pPz, pU1, pGF);
         gfpMontMul(pPz, pU1, pS1, pGF);
         gfpMontMul(pU1, pQx, pU1, pGF);
         gfpMontMul(pS1, pQy, pS1, pGF);
      }

      /* W = U0-U1 */
      /* R = S0-S1 */
      gfpSub(pU0, pU1, pW, pGF);
      gfpSub(pS0, pS1, pR, pGF);

      if( GFP_IS_ZERO(pW, elementSize) ) {
         ecgfpReleasePool(2, pEC);
         if( GFP_IS_ZERO(pR, elementSize) )
            return ecgfpDblPoint(pPointP, pPointR, pEC);
         else
            return ecgfpSetProjectivePointAtInfinity(pPointR, elementSize);
      }

      /* T = U0+U1 */
      /* M = S0+S1 */
      gfpAdd(pU0, pU1, pT, pGF);
      gfpAdd(pS0, pS1, pM, pGF);

      /* Rz = Pz * Qz * W */
      if( IS_ECP_AFFINE_POINT(pPointP) && IS_ECP_AFFINE_POINT(pPointQ) )
         gfpFFelementCopy(pW, ECP_POINT_Z(pPointR), elementSize);
      else {
         if( IS_ECP_AFFINE_POINT(pPointQ) )
            gfpFFelementCopy(pPz, pU1, elementSize);
         else if ( IS_ECP_AFFINE_POINT(pPointP) )
            gfpFFelementCopy(pQz, pU1, elementSize);
         else
            gfpMontMul(pPz, pQz, pU1, pGF);
         gfpMontMul(pU1, pW, ECP_POINT_Z(pPointR), pGF);
      }

      gfpMontMul(pW, pW, pU1, pGF); /* U1 = W^2     */
      gfpMontMul(pU1,pT, pS1, pGF); /* S1 = T * W^2 */

      /* Rx = R^2 - T * W^2 */
      gfpMontMul(pR, pR, ECP_POINT_X(pPointR), pGF);
      gfpSub(ECP_POINT_X(pPointR), pS1, ECP_POINT_X(pPointR), pGF);

      /* V = T * W^2 - 2 * Rx  => S1 -= 2*Rx */
      gfpSub(pS1, ECP_POINT_X(pPointR), pS1, pGF);
      gfpSub(pS1, ECP_POINT_X(pPointR), pS1, pGF);

      /* Ry = (V * R - M * W^3) /2 */
      gfpMontMul(pS1, pR, pS1, pGF);
      gfpMontMul(pU1, pW, pU1, pGF);
      gfpMontMul(pU1, pM, pU1, pGF);
      gfpSub(pS1, pU1, pS1, pGF);
      gfpHalve(pS1, ECP_POINT_Y(pPointR), pGF);

      //ECP_POINT_FLAGS(pPointR) = GFP_EQ(ECP_POINT_Z(pPointR), GFP_MONTUNITY(pGF), elementSize)? ECP_AFFINE_POINT|ECP_FINITE_POINT : ECP_FINITE_POINT;
      ECP_POINT_FLAGS(pPointR) = ECP_FINITE_POINT;

      ecgfpReleasePool(2, pEC);
      return pPointR;
   }
}

#if 0
IppsGFPECPoint* ecgfpMulPoint(const IppsGFPECPoint* pPointP, const cpFFchunk* pN, Ipp32u nSize, IppsGFPECPoint* pPointR, IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   Ipp32u elementSize = GFP_FESIZE(pGF);

   /* test scalar and input point */
   if( GFP_IS_ZERO(pN, nSize) || !IS_ECP_FINITE_POINT(pPointP) )
      return ecgfpSetProjectivePointAtInfinity(pPointR, elementSize);

   /* remove leding zeros */
   FIX_BNU(pN, nSize);
   /* R = P */
   ecgfpCopyPoint(pPointP, pPointR, elementSize);

   if( GFP_IS_ONE(pN, nSize) )
      return pPointR;

   /*
   // scalar multiplication
   */
   else {
      Ipp32u i;

      Ipp32u* pH = gfpGetPool(1, pGF);
      Ipp32u* pK = gfpGetPool(1, pGF);

      IppsGFPECPoint T, U;
      ecgfpNewPoint(&T, ecgfpGetPool(1, pEC),0, pEC);
      ecgfpNewPoint(&U, ecgfpGetPool(1, pEC),0, pEC);

      /* H = 3*N */
      gfpFFelementCopy(pN, pK, nSize);
      pK[nSize] = 0;
      cpAdd_BNU(pK, pK, pH, nSize+1, &i);
      cpAdd_BNU(pH, pK, pH, nSize+1, &i);

      /* T = affine(P) */
      if( IS_ECP_AFFINE_POINT(pPointP) )
         ecgfpCopyPoint(pPointP, &T, elementSize);
      else {
         ecgfpGetAffinePoint(pPointP, ECP_POINT_X(&T), ECP_POINT_Y(&T), pEC);
         ecgfpSetAffinePoint(ECP_POINT_X(&T), ECP_POINT_Y(&T), &T, pEC);
      }
      /* U = affine(-P) */
      ecgfpNegPoint(&T, &U, pEC);

      /*
      // point multiplication
      */
      for(i=MSB_BNU(pH, nSize+1)-1; i>0; i--) {
         Ipp32u hBit = TST_BIT(pH, i);
         Ipp32u kBit = TST_BIT(pK, i);
         ecgfpDblPoint(pPointR, pPointR, pEC);
         if( hBit && !kBit )
            ecgfpAddPoint(&T, pPointR, pPointR, pEC);
         if(!hBit &&  kBit )
            ecgfpAddPoint(&U, pPointR, pPointR, pEC);
      }

      ecgfpReleasePool(2, pEC);
      gfpReleasePool(2, pGF);

      return pPointR;
   }
}
#endif

IppsGFPECPoint* ecgfpMulPoint(const IppsGFPECPoint* pPointP, const cpFFchunk* pN, Ipp32u nSize, IppsGFPECPoint* pPointR, IppsGFPECState* pEC)
{
   IppsGFPState* pGF = ECP_FIELD(pEC);
   Ipp32u elementSize = GFP_FESIZE(pGF);

   /* test scalar and input point */
   if( GFP_IS_ZERO(pN, nSize) || !IS_ECP_FINITE_POINT(pPointP) )
      return ecgfpSetProjectivePointAtInfinity(pPointR, elementSize);

   /* remove leding zeros */
   FIX_BNU(pN, nSize);

   /* case N==1 => R = P */
   if( GFP_IS_ONE(pN, nSize) ) {
      ecgfpCopyPoint(pPointP, pPointR, elementSize);
      return pPointR;
   }

   /*
   // scalar multiplication
   */
   else {
      Ipp32u i;

      Ipp32u* pH = gfpGetPool(1, pGF);
      Ipp32u* pK = gfpGetPool(1, pGF);

      IppsGFPECPoint T, U;
      ecgfpNewPoint(&T, ecgfpGetPool(1, pEC),0, pEC);
      ecgfpNewPoint(&U, ecgfpGetPool(1, pEC),0, pEC);

      /* H = 3*N */
      gfpFFelementCopy(pN, pK, nSize);
      pK[nSize] = 0;
      cpAdd_BNU(pK, pK, pH, nSize+1, &i);
      cpAdd_BNU(pH, pK, pH, nSize+1, &i);

      /* T = affine(P) */
      if( IS_ECP_AFFINE_POINT(pPointP) )
         ecgfpCopyPoint(pPointP, &T, elementSize);
      else {
         ecgfpGetAffinePoint(pPointP, ECP_POINT_X(&T), ECP_POINT_Y(&T), pEC);
         ecgfpSetAffinePoint(ECP_POINT_X(&T), ECP_POINT_Y(&T), &T, pEC);
      }
      /* U = affine(-P) */
      ecgfpNegPoint(&T, &U, pEC);

      /* R = T = affine(P) */
      ecgfpCopyPoint(&T, pPointR, elementSize);

      /*
      // point multiplication
      */
      for(i=MSB_BNU(pH, nSize+1)-1; i>0; i--) {
         Ipp32u hBit = TST_BIT(pH, i);
         Ipp32u kBit = TST_BIT(pK, i);
         ecgfpDblPoint(pPointR, pPointR, pEC);
         if( hBit && !kBit )
            ecgfpAddPoint(&T, pPointR, pPointR, pEC);
         if(!hBit &&  kBit )
            ecgfpAddPoint(&U, pPointR, pPointR, pEC);
      }

      ecgfpReleasePool(2, pEC);
      gfpReleasePool(2, pGF);

      return pPointR;
   }
}
