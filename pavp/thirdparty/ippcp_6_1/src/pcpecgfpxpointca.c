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
//    Internal EC over GF(p^k) methods
//
//    Context:
//       ecgfpxGetAffinePoint()
//
//       ecgfpxIsPointEquial()
//       ecgfpxIsPointOnCurve()
//
//       ecgfpxNegPoint()
//       ecgfpxDblPoint()
//       ecgfpxAddPoint()
//       ecgfpxMulPoint()
//
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpecgfpx.h"


Ipp32u ecgfpxGetAffinePoint(const IppsGFPXECPoint* pPoint, cpFFchunk* pX, cpFFchunk* pY, IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   //Ipp32u elementSize = GFPX_FESIZE(pGF);

   if( !IS_ECPX_FINITE_POINT(pPoint) ) {
      //GFPX_ZERO(pX, pGF);
      //if( GFPX_IS_ZERO(ECPX_B(pEC), pGF) )
      //   GFPX_ONE(pY, pGF);
      //else
      //   GFPX_ZERO(pY, pGF);
      //return;
      return 0;
   }

   /* case Z == 1 */
   if( IS_ECPX_AFFINE_POINT(pPoint) ) {
      if(pX)
         gfpxMontDec(ECPX_POINT_X(pPoint), pX, pGF);
      if(pY)
         gfpxMontDec(ECPX_POINT_Y(pPoint), pY, pGF);
   }

   /* case Z != 1 */
   else {
      /* T = (1/Z)*(1/Z) */
      cpFFchunk* pT    = gfpxGetPool(1, pGF);
      cpFFchunk* pZinv = gfpxGetPool(1, pGF);
      gfpxMontInv(ECPX_POINT_Z(pPoint), pZinv, pGF);
      gfpxMontMul(pZinv, pZinv, pT, pGF);
      gfpxMontDec(pT, pT, pGF);

      if(pX)
         gfpxMontMul(ECPX_POINT_X(pPoint), pT, pX, pGF);
      if(pY) {
         gfpxMontMul(pZinv, pT, pT, pGF);
         gfpxMontMul(ECPX_POINT_Y(pPoint), pT, pY, pGF);
      }

      gfpxReleasePool(2, pGF);
   }

   return 1;
}

int ecgfpxIsPointEquial(const IppsGFPXECPoint* pP, const IppsGFPXECPoint* pQ, IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   Ipp32u elementSize = GFPX_FESIZE(pGF);

   /* P or/and Q at Infinity */
   if( !IS_ECPX_FINITE_POINT(pP) )
      return !IS_ECPX_FINITE_POINT(pQ)? 1:0;
   if( !IS_ECPX_FINITE_POINT(pQ) )
      return !IS_ECPX_FINITE_POINT(pP)? 1:0;

   /* Px==Qx && Py==Qy && Pz==Qz */
   if(  GFP_EQ(ECPX_POINT_Z(pP), ECPX_POINT_Z(pQ), elementSize)
      &&GFP_EQ(ECPX_POINT_X(pP), ECPX_POINT_X(pQ), elementSize)
      &&GFP_EQ(ECPX_POINT_Y(pP), ECPX_POINT_Y(pQ), elementSize))
      return 1;

   else {
      int isEqu = 1;

      cpFFchunk* pPtmp = gfpxGetPool(1, pGF);
      cpFFchunk* pQtmp = gfpxGetPool(1, pGF);
      cpFFchunk* pPz   = gfpxGetPool(1, pGF);
      cpFFchunk* pQz   = gfpxGetPool(1, pGF);

      if(isEqu) {
         /* Px*Qz^2 ~ Qx*Pz^2 */
         if( IS_ECPX_AFFINE_POINT(pQ) ) /* Ptmp = Px * Qz^2 */
            gfpFFelementCopy(ECPX_POINT_X(pP), pPtmp, elementSize);
         else {
            gfpxMontMul(ECPX_POINT_Z(pQ), ECPX_POINT_Z(pQ), pQz, pGF);
            gfpxMontMul(pQz, ECPX_POINT_X(pP), pPtmp, pGF);
         }
         if( IS_ECPX_AFFINE_POINT(pP) ) /* Qtmp = Qx * Pz^2 */
            gfpFFelementCopy(ECPX_POINT_X(pQ), pQtmp, elementSize);
         else {
            gfpxMontMul(ECPX_POINT_Z(pP), ECPX_POINT_Z(pP), pPz, pGF);
            gfpxMontMul(pPz, ECPX_POINT_X(pQ), pQtmp, pGF);
         }
         isEqu = GFP_EQ(pPtmp, pQtmp, elementSize);
      }

      if(isEqu) {
         /* Py*Qz^3 ~ Qy*Pz^3 */
         if( IS_ECPX_AFFINE_POINT(pQ) ) /* Ptmp = Py * Qz^3 */
            gfpFFelementCopy(ECPX_POINT_Y(pP), pPtmp, elementSize);
         else {
            gfpxMontMul(ECPX_POINT_Z(pQ), pQz, pQz, pGF);
            gfpxMontMul(pQz, ECPX_POINT_Y(pP), pPtmp, pGF);
         }
         if( IS_ECPX_AFFINE_POINT(pP) ) /* Qtmp = Qy * Pz^3 */
            gfpFFelementCopy(ECPX_POINT_Y(pQ), pQtmp, elementSize);
         else {
            gfpxMontMul(ECPX_POINT_Z(pP), pPz, pPz, pGF);
            gfpxMontMul(pPz, ECPX_POINT_Y(pQ), pQtmp, pGF);
         }
         isEqu = GFP_EQ(pPtmp, pQtmp, elementSize);
      }

      gfpxReleasePool(4, pGF);
      return isEqu;
   }
}

int ecgfpxIsPointOnCurve(const IppsGFPXECPoint* pPoint, IppsGFPXECState* pEC)
{
   /* point at infinity belongs curve */
   if( !IS_ECPX_FINITE_POINT(pPoint) )
      return 1;

   /* test that 0 == R = (Y^2) - (X^3 + A*X*(Z^4) + B*(Z^6)) */
   else {
      int isOnCurve = 0;

      IppsGFPXState* pGF = ECPX_FIELD(pEC);

      cpFFchunk* pX = ECPX_POINT_X(pPoint);
      cpFFchunk* pY = ECPX_POINT_Y(pPoint);
      cpFFchunk* pZ = ECPX_POINT_Z(pPoint);

      cpFFchunk* pR = gfpxGetPool(1, pGF);
      cpFFchunk* pT = gfpxGetPool(1, pGF);

      gfpxMontMul(pY, pY, pR, pGF);  /* R = Y^2 */
      gfpxMontMul(pX, pX, pT, pGF);  /* T = X^3 */
      gfpxMontMul(pX, pT, pT, pGF);
      gfpxSub(pR, pT, pR, pGF);      /* R -= T */

      if( IS_ECPX_AFFINE_POINT(pPoint) ) {
         gfpxMontMul(pX, ECPX_A(pEC), pT, pGF);  /* T = A*X */
         gfpxSub(pR, pT, pR, pGF);               /* R -= T */
         gfpxSub(pR, ECPX_B(pEC), pR, pGF);      /* R -= B */
      }
      else {
         cpFFchunk* pZ4 = gfpxGetPool(1, pGF);
         cpFFchunk* pZ6 = gfpxGetPool(1, pGF);

         gfpxMontMul(pZ, pZ, pZ6, pGF);       /* Z^2 */
         gfpxMontMul(pZ6, pZ6, pZ4, pGF);     /* Z^4 */
         gfpxMontMul(pZ6, pZ4, pZ6, pGF);     /* Z^6 */

         gfpxMontMul(pZ4, pX, pZ4, pGF);          /* X*(Z^4) */
         gfpxMontMul(pZ4, ECPX_A(pEC), pZ4, pGF); /* A*X*(Z^4) */
         gfpxMontMul(pZ6, ECPX_B(pEC), pZ6, pGF); /* B*(Z^4) */

         gfpxSub(pR, pZ4, pR, pGF);           /* R -= A*X*(Z^4) */
         gfpxSub(pR, pZ6, pR, pGF);           /* R -= B*(Z^6)   */

         gfpxReleasePool(2, pGF);
      }

      isOnCurve = GFPX_IS_ZERO(pR, pGF);
      gfpxReleasePool(2, pGF);
      return isOnCurve;
   }
}

static cpFFchunk* gfpxHalve(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXState* pGFPX)
{
   Ipp32u degree = GFPX_DEGREE(pGFPX);
   IppsGFPState* pGroundGF = GFPX_GROUNDGF(pGFPX);
   Ipp32u grounfElementSize = GFP_FESIZE(pGroundGF);

   Ipp32u i;
   cpFFchunk* pT = pR;
   for(i=0; i<=degree; i++, pA+=grounfElementSize, pT+=grounfElementSize)
        gfpHalve(pA, pT, pGroundGF);

    return pR;
}

IppsGFPXECPoint* ecgfpxNegPoint (const IppsGFPXECPoint* pP, IppsGFPXECPoint* pR, IppsGFPXECState* pEC)
{
   Ipp32u elementSize = GFPX_FESIZE(ECPX_FIELD(pEC));

   if(pP!=pR)
      ecgfpxCopyPoint(pP, pR, elementSize);

   if( IS_ECPX_FINITE_POINT(pR) )
      gfpxNeg(ECPX_POINT_Y(pR), ECPX_POINT_Y(pR), ECPX_FIELD(pEC));
   return pR;
}

IppsGFPXECPoint* ecgfpxDblPoint (const IppsGFPXECPoint* pP, IppsGFPXECPoint* pR, IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   Ipp32u elementSize = GFPX_FESIZE(pGF);

   /* P at infinity => R at infinity */
   if( !IS_ECPX_FINITE_POINT(pP) )
      ecgfpxSetProjectivePointAtInfinity(pR, elementSize);

   else {
      cpFFchunk* pX = ECPX_POINT_X(pP);
      cpFFchunk* pY = ECPX_POINT_Y(pP);
      cpFFchunk* pZ = ECPX_POINT_Z(pP);

      cpFFchunk* pU = gfpxGetPool(1, pGF);
      cpFFchunk* pM = gfpxGetPool(1, pGF);
      cpFFchunk* pS = gfpxGetPool(1, pGF);

      /* M = 3*X^2 + A*Z^4 */
      gfpxMontMul(pX, pX, pU, pGF);
      gfpxAdd(pU, pU, pM, pGF);
      gfpxAdd(pU, pM, pM, pGF);
      if( IS_ECPX_AFFINE_POINT(pP) )
         gfpxAdd(ECPX_A(pEC), pM, pM, pGF);
      else {
         gfpxMontMul(pZ, pZ, pU, pGF);
         gfpxMontMul(pU, pU, pU, pGF);
         gfpxMontMul(ECPX_A(pEC), pU, pU, pGF);
         gfpxAdd(pM, pU, pM, pGF);
      }

      /* U = 2*Y */
      gfpxAdd(pY, pY, pU, pGF);

      /* Rz = 2*Y*Z */
      if( IS_ECPX_AFFINE_POINT(pP) )
         gfpFFelementCopy(pU, ECPX_POINT_Z(pR), elementSize);
      else
         gfpxMontMul(pU, pZ, ECPX_POINT_Z(pR), pGF);

      /* S = X*(U^2) = 4*X*Y^2 */
      gfpxMontMul(pU, pU, pU, pGF);
      gfpxMontMul(pX, pU, pS, pGF);

      /* Rx = M^2 - 2*S */
      gfpxMontMul(pM, pM, ECPX_POINT_X(pR), pGF);
      gfpxSub(ECPX_POINT_X(pR), pS, ECPX_POINT_X(pR), pGF);
      gfpxSub(ECPX_POINT_X(pR), pS, ECPX_POINT_X(pR), pGF);

      /* U = (U^2)/2 = (16*Y^4)/2 = 8*Y^4 */
      gfpxMontMul(pU, pU, pU, pGF);
      gfpxHalve(pU, pU, pGF);

      /* Ry = M*(S - Rx) - U */
      gfpxSub(pS, ECPX_POINT_X(pR), pS, pGF);
      gfpxMontMul(pM, pS, pS, pGF);
      gfpxSub(pS, pU, ECPX_POINT_Y(pR), pGF);

      //ECP_POINT_FLAGS(pR) = GFP_EQ(ECP_POINT_Z(pR), GFP_MONTUNITY(pGF), elementSize)? ECP_AFFINE_POINT|ECP_FINITE_POINT : ECP_FINITE_POINT;
      ECPX_POINT_FLAGS(pR) = ECPX_FINITE_POINT;

      gfpxReleasePool(3, pGF);
   }

   return pR;
}

IppsGFPXECPoint* ecgfpxAddPoint (const IppsGFPXECPoint* pPointP, const IppsGFPXECPoint* pPointQ, IppsGFPXECPoint* pPointR, IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   Ipp32u elementSize = GFPX_FESIZE(pGF);

   /* test stupid call */
   if( pPointP == pPointQ )
      return ecgfpxDblPoint(pPointP, pPointR, pEC);

   /* prevent operation with point at Infinity */
   if( !IS_ECPX_FINITE_POINT(pPointP) )
      return ecgfpxCopyPoint(pPointQ, pPointR, elementSize);
   if( !IS_ECPX_FINITE_POINT(pPointQ) )
      return ecgfpxCopyPoint(pPointP, pPointR, elementSize);

   /*
   // addition
   */
   {
      cpFFchunk* pU0 = ecgfpxGetPool(2, pEC);
      cpFFchunk* pS0 = pU0 + elementSize;
      cpFFchunk* pW  = pS0 + elementSize;
      cpFFchunk* pU1 = pW  + elementSize;
      cpFFchunk* pS1 = pU1 + elementSize;
      cpFFchunk* pR  = pS1 + elementSize;
      cpFFchunk* pT  = pU0;
      cpFFchunk* pM  = pS0;

      /* coordinates of P */
      cpFFchunk* pPx = ECPX_POINT_X(pPointP);
      cpFFchunk* pPy = ECPX_POINT_Y(pPointP);
      cpFFchunk* pPz = ECPX_POINT_Z(pPointP);

      /* coordinates of Q */
      cpFFchunk* pQx = ECPX_POINT_X(pPointQ);
      cpFFchunk* pQy = ECPX_POINT_Y(pPointQ);
      cpFFchunk* pQz = ECPX_POINT_Z(pPointQ);

      /* U0 = Px * Qz^2 */
      /* S0 = Py * Qz^3 */
      if( IS_ECPX_AFFINE_POINT(pPointQ) ) {
         gfpxFFelementCopy(pPx, pU0, pGF);
         gfpxFFelementCopy(pPy, pS0, pGF);
      }
      else {
         gfpxMontMul(pQz, pQz, pU0, pGF);
         gfpxMontMul(pQz, pU0, pS0, pGF);
         gfpxMontMul(pU0, pPx, pU0, pGF);
         gfpxMontMul(pS0, pPy, pS0, pGF);
      }

      /* U1 = Qx * Pz^2 */
      /* S1 = Qy * Pz^3 */
      if( IS_ECPX_AFFINE_POINT(pPointP) ) {
         gfpxFFelementCopy(pQx, pU1, pGF);
         gfpxFFelementCopy(pQy, pS1, pGF);
      }
      else {
         gfpxMontMul(pPz, pPz, pU1, pGF);
         gfpxMontMul(pPz, pU1, pS1, pGF);
         gfpxMontMul(pU1, pQx, pU1, pGF);
         gfpxMontMul(pS1, pQy, pS1, pGF);
      }

      /* W = U0-U1 */
      /* R = S0-S1 */
      gfpxSub(pU0, pU1, pW, pGF);
      gfpxSub(pS0, pS1, pR, pGF);

      if( GFPX_IS_ZERO(pW, pGF) ) {
         ecgfpxReleasePool(2, pEC);
         if( GFPX_IS_ZERO(pR, pGF) )
            return ecgfpxDblPoint(pPointP, pPointR, pEC);
         else
            return ecgfpxSetProjectivePointAtInfinity(pPointR, elementSize);
      }

      /* T = U0+U1 */
      /* M = S0+S1 */
      gfpxAdd(pU0, pU1, pT, pGF);
      gfpxAdd(pS0, pS1, pM, pGF);

      /* Rz = Pz * Qz * W */
      if( IS_ECPX_AFFINE_POINT(pPointP) && IS_ECPX_AFFINE_POINT(pPointQ) )
         gfpxFFelementCopy(pW, ECPX_POINT_Z(pPointR), pGF);
      else {
         if( IS_ECPX_AFFINE_POINT(pPointQ) )
            gfpxFFelementCopy(pPz, pU1, pGF);
         else if ( IS_ECPX_AFFINE_POINT(pPointP) )
            gfpxFFelementCopy(pQz, pU1, pGF);
         else
            gfpxMontMul(pPz, pQz, pU1, pGF);
         gfpxMontMul(pU1, pW, ECPX_POINT_Z(pPointR), pGF);
      }

      gfpxMontMul(pW, pW, pU1, pGF); /* U1 = W^2     */
      gfpxMontMul(pU1,pT, pS1, pGF); /* S1 = T * W^2 */

      /* Rx = R^2 - T * W^2 */
      gfpxMontMul(pR, pR, ECPX_POINT_X(pPointR), pGF);
      gfpxSub(ECPX_POINT_X(pPointR), pS1, ECPX_POINT_X(pPointR), pGF);

      /* V = T * W^2 - 2 * Rx  => S1 -= 2*Rx */
      gfpxSub(pS1, ECPX_POINT_X(pPointR), pS1, pGF);
      gfpxSub(pS1, ECPX_POINT_X(pPointR), pS1, pGF);

      /* Ry = (V * R - M * W^3) /2 */
      gfpxMontMul(pS1, pR, pS1, pGF);
      gfpxMontMul(pU1, pW, pU1, pGF);
      gfpxMontMul(pU1, pM, pU1, pGF);
      gfpxSub(pS1, pU1, pS1, pGF);
      gfpxHalve(pS1, ECPX_POINT_Y(pPointR), pGF);

      ECPX_POINT_FLAGS(pPointR) = ECPX_FINITE_POINT;

      ecgfpxReleasePool(2, pEC);
      return pPointR;
   }
}

IppsGFPXECPoint* ecgfpxMulPoint(const IppsGFPXECPoint* pPointP, const cpFFchunk* pN, Ipp32u nSize, IppsGFPXECPoint* pPointR, IppsGFPXECState* pEC)
{
   IppsGFPXState* pGF = ECPX_FIELD(pEC);
   Ipp32u elementSize = GFPX_FESIZE(pGF);

   /* test scalar and input point */
   if( GFP_IS_ZERO(pN, nSize) || !IS_ECPX_FINITE_POINT(pPointP) )
      return ecgfpxSetProjectivePointAtInfinity(pPointR, elementSize);

   /* remove leding zeros */
   FIX_BNU(pN, nSize);

   /* case N==1 => R = P */
   if( GFP_IS_ONE(pN, nSize) ) {
      ecgfpxCopyPoint(pPointP, pPointR, elementSize);
      return pPointR;
   }

   /*
   // scalar multiplication
   */
   else {
      Ipp32u i;

      Ipp32u* pBuffer = gfpxGetPool(3, pGF);
      Ipp32u* pH = pBuffer;
      Ipp32u* pK = pBuffer+nSize+1;

      IppsGFPXECPoint T, U;
      ecgfpxNewPoint(&T, ecgfpxGetPool(1, pEC),0, pEC);
      ecgfpxNewPoint(&U, ecgfpxGetPool(1, pEC),0, pEC);

      /* H = 3*N */
      gfpFFelementCopy(pN, pK, nSize);
      pK[nSize] = 0;
      cpAdd_BNU(pK, pK, pH, nSize+1, &i);
      cpAdd_BNU(pH, pK, pH, nSize+1, &i);

      /* T = affine(P) */
      if( IS_ECPX_AFFINE_POINT(pPointP) )
         ecgfpxCopyPoint(pPointP, &T, elementSize);
      else {
         ecgfpxGetAffinePoint(pPointP, ECPX_POINT_X(&T), ECPX_POINT_Y(&T), pEC);
         ecgfpxSetAffinePoint(ECPX_POINT_X(&T), ECPX_POINT_Y(&T), &T, pEC);
      }
      /* U = affine(-P) */
      ecgfpxNegPoint(&T, &U, pEC);

      /* R = T = affine(P) */
      ecgfpxCopyPoint(&T, pPointR, elementSize);

      /*
      // point multiplication
      */
      for(i=MSB_BNU(pH, nSize+1)-1; i>0; i--) {
         Ipp32u hBit = TST_BIT(pH, i);
         Ipp32u kBit = TST_BIT(pK, i);
         ecgfpxDblPoint(pPointR, pPointR, pEC);
         if( hBit && !kBit )
            ecgfpxAddPoint(&T, pPointR, pPointR, pEC);
         if(!hBit &&  kBit )
            ecgfpxAddPoint(&U, pPointR, pPointR, pEC);
      }

      ecgfpxReleasePool(2, pEC);
      gfpxReleasePool(3, pGF);

      return pPointR;
   }
}
