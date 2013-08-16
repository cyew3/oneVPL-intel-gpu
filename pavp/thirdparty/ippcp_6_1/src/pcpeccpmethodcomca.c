/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    EC methods over common GF(p)
//
// Contents:
//    ECCP_CopyPoint()
//    ECCP_SetPointProjective()
//    ECCP_SetPointAffine()
//
//    ECCP_GetPointProjective()
//    ECCP_GetPointAffine()
//
//    ECCP_SetPointToInfinity()
//    ECCP_SetPointToAffineInfinity0()
//    ECCP_SetPointToAffineInfinity1()
//
//    ECCP_IsPointAtInfinity()
//    ECCP_IsPointAtAffineInfinity0()
//    ECCP_IsPointAtAffineInfinity1()
//
//    ECCP_IsPointOnCurve()
//
//    ECCP_ComparePoint()
//    ECCP_NegPoint()
//    ECCP_DblPoint()
//    ECCP_AddPoint()
//    ECCP_MulPoint()
//    ECCP_ProdPoint()
//
//
//    Created: Tue 01-Jul-2003 12:43
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccppoint.h"
#include "pcpeccpmethod.h"
#include "pcpeccpmethodcom.h"
#include "pcppma.h"


#if !defined(_IPP_TRS)
static
ECCP_METHOD ECCPcom = {
   //ECCP_CopyPoint,

   ECCP_SetPointProjective,
   ECCP_SetPointAffine,

   ECCP_GetPointProjective,
   ECCP_GetPointAffine,

   //ECCP_SetPointToInfinity,
   //ECCP_SetPointToAffineInfinity0,
   //ECCP_SetPointToAffineInfinity1,

   //ECCP_IsPointAtInfinity,
   //ECCP_IsPointAtAffineInfinity0,
   //ECCP_IsPointAtAffineInfinity1,
   ECCP_IsPointOnCurve,

   ECCP_ComparePoint,
   ECCP_NegPoint,
   ECCP_DblPoint,
   ECCP_AddPoint,
   ECCP_MulPoint,
   ECCP_ProdPoint
};
#endif


/*
// Returns reference
*/
#if !defined(_IPP_TRS)
ECCP_METHOD* ECCPcom_Methods(void)
{
   return &ECCPcom;
}
#endif


/*
// Copy Point
*/
void ECCP_CopyPoint(const IppsECCPPointState* pSrc, IppsECCPPointState* pDst)
{
   cpBN_copy(ECP_POINT_X(pSrc), ECP_POINT_X(pDst));
   cpBN_copy(ECP_POINT_Y(pSrc), ECP_POINT_Y(pDst));
   cpBN_copy(ECP_POINT_Z(pSrc), ECP_POINT_Z(pDst));
   ECP_POINT_AFFINE(pDst) = ECP_POINT_AFFINE(pSrc);
}

/*
// ECCP_PoinSettProjective
// Converts regular projective triplet (pX,pY,pZ) into pPoint
*/
void ECCP_SetPointProjective(const IppsBigNumState* pX,
                             const IppsBigNumState* pY,
                             const IppsBigNumState* pZ,
                             IppsECCPPointState* pPoint,
                             const IppsECCPState* pECC)
{
   IppsMontState* pMont = ECP_PMONT(pECC);

   #if defined(_IPP_TRS)
   DECLARE_BN_Word(_trs_one32_, 1);
   INIT_BN_Word(_trs_one32_);
   #endif

   PMA_enc(ECP_POINT_X(pPoint), (IppsBigNumState*)pX, pMont);
   PMA_enc(ECP_POINT_Y(pPoint), (IppsBigNumState*)pY, pMont);
   PMA_enc(ECP_POINT_Z(pPoint), (IppsBigNumState*)pZ, pMont);
   ECP_POINT_AFFINE(pPoint) = Cmp_BN(pZ, BN_ONE_REF())==0;
}

/*
// ECCP_PointAffineSet
// Converts regular affine pair (pX,pY) into pPoint
*/
void ECCP_SetPointAffine(const IppsBigNumState* pX,
                         const IppsBigNumState* pY,
                         IppsECCPPointState* pPoint,
                         const IppsECCPState* pECC)
{
   IppsMontState* pMont = ECP_PMONT(pECC);
   PMA_enc(ECP_POINT_X(pPoint), (IppsBigNumState*)pX, pMont);
   PMA_enc(ECP_POINT_Y(pPoint), (IppsBigNumState*)pY, pMont);
   PMA_set(ECP_POINT_Z(pPoint), MNT_1(pMont));
   ECP_POINT_AFFINE(pPoint) = 1;
}

/*
// ECCP_GetPointProjective
// Converts pPoint into regular projective triplet (pX,pY,pZ)
*/
void ECCP_GetPointProjective(IppsBigNumState* pX,
                             IppsBigNumState* pY,
                             IppsBigNumState* pZ,
                             const IppsECCPPointState* pPoint,
                             const IppsECCPState* pECC)
{
   IppsMontState* pMont = ECP_PMONT(pECC);

   #if defined(_IPP_TRS)
   DECLARE_BN_Word(_trs_one32_, 1);
   INIT_BN_Word(_trs_one32_);
   #endif

   PMA_dec(pX, ECP_POINT_X(pPoint), pMont);
   PMA_dec(pY, ECP_POINT_Y(pPoint), pMont);
   PMA_dec(pZ, ECP_POINT_Z(pPoint), pMont);
}

/*
// ECCP_GetPointAffine
//
// Converts pPoint into regular affine pair (pX,pY)
//
// Note:
// pPoint is not point at Infinity
// transform  (X, Y, Z)  into  (x, y) = (X/Z^2, Y/Z^3)
*/
void ECCP_GetPointAffine(IppsBigNumState* pX, IppsBigNumState* pY,
                         const IppsECCPPointState* pPoint,
                         const IppsECCPState* pECC,
                         BigNumNode* pList)
{
   IppsMontState* pMont = ECP_PMONT(pECC);

   #if defined(_IPP_TRS)
   DECLARE_BN_Word(_trs_one32_, 1);
   INIT_BN_Word(_trs_one32_);
   #endif

   /* case Z == 1 */
   if( ECP_POINT_AFFINE(pPoint) ) {
      if(pX) {
         PMA_dec(pX, ECP_POINT_X(pPoint), pMont);
      }
      if(pY) {
         PMA_dec(pY, ECP_POINT_Y(pPoint), pMont);
      }
   }

   /* case Z != 1 */
   else {
      IppsBigNumState* pT = cpBigNumListGet(&pList);
      IppsBigNumState* pU = cpBigNumListGet(&pList);
      IppsBigNumState* pModulo = MNT_MODULO(pMont);

      /* decode Z */
      PMA_dec(pU, ECP_POINT_Z(pPoint), pMont);
      /* regular T = Z^-1 */
      PMA_inv(pT, pU, pModulo);
      /* montgomery U = Z^-1 */
      PMA_enc(pU, pT, pMont);
      /* regular T = Z^-2 */
      PMA_mule(pT, pU, pT, pMont);

      if(pX) {
         PMA_mule(pX,pT, ECP_POINT_X(pPoint), pMont);
      }
      if(pY) {
         /* regular U = Z^-3 */
         PMA_mule(pU, pU, pT, pMont);
         PMA_mule(pY,pU, ECP_POINT_Y(pPoint), pMont);
      }
   }
}

/*
// ECCP_SetPointToInfinity
// ECCP_SetPointToAffineInfinity0
// ECCP_SetPointToAffineInfinity1
//
// Set point to Infinity
*/
void ECCP_SetPointToInfinity(IppsECCPPointState* pPoint)
{
   BN_Zero(ECP_POINT_Z(pPoint));
   ECP_POINT_AFFINE(pPoint) = 0;
}

void ECCP_SetPointToAffineInfinity0(IppsBigNumState* pX, IppsBigNumState* pY)
{
   if(pX) BN_Zero(pX);
   if(pY) BN_Zero(pY);
}

void ECCP_SetPointToAffineInfinity1(IppsBigNumState* pX, IppsBigNumState* pY)
{
   if(pX) BN_Zero(pX);
   if(pY) BN_Word(pY,1);
}

/*
// ECCP_IsPointAtInfinity
// ECCP_IsPointAtAffineInfinity0
// ECCP_IsPointAtAffineInfinity1
//
// Test point is at Infinity
*/
int ECCP_IsPointAtInfinity(const IppsECCPPointState* pPoint)
{
   return IsZero_BN( ECP_POINT_Z(pPoint) );
}

int ECCP_IsPointAtAffineInfinity0(const IppsBigNumState* pX, const IppsBigNumState* pY)
{
   return IsZero_BN(pX) && IsZero_BN(pY);
}

int ECCP_IsPointAtAffineInfinity1(const IppsBigNumState* pX, const IppsBigNumState* pY)
{
   return IsZero_BN(pX) && !IsZero_BN(pY);
}

/*
// ECCP_IsPointOnCurve
//
// Test point is lie on curve
//
// Note
// We deal with equation: y^2 = x^3 + A*x + B.
// Or in projective coordinates: Y^2 = X^3 + a*X*Z^4 + b*Z^6.
// The point under test is given by projective triplet (X,Y,Z),
// which represents actually (x,y) = (X/Z^2,Y/Z^3).
*/
int ECCP_IsPointOnCurve(const IppsECCPPointState* pPoint,
                        const IppsECCPState* pECC,
                        BigNumNode* pList)
{
   /* let think Infinity point is on the curve */
   if( ECCP_IsPointAtInfinity(pPoint) )
      return 1;

   else {
      IppsMontState*   pMont = ECP_PMONT(pECC);
      IppsBigNumState* pModulo = MNT_MODULO(pMont);
      IppsBigNumState* pR = cpBigNumListGet(&pList);
      IppsBigNumState* pT = cpBigNumListGet(&pList);

      PMA_sqre(pR, ECP_POINT_X(pPoint), pMont);      // R = X^3
      PMA_mule(pR, pR, ECP_POINT_X(pPoint), pMont);

      /* case Z != 1 */
      if( !ECP_POINT_AFFINE(pPoint) ) {
         IppsBigNumState* pZ4 = cpBigNumListGet(&pList);
         IppsBigNumState* pZ6 = cpBigNumListGet(&pList);
         PMA_sqre(pT,  ECP_POINT_Z(pPoint), pMont);  // Z^2
         PMA_sqre(pZ4, pT,                pMont);  // Z^4
         PMA_mule(pZ6, pZ4, pT,           pMont);  // Z^6

         PMA_mule(pT, pZ4, ECP_POINT_X(pPoint), pMont); // T = X*Z^4
         if( ECP_AMI3(pECC) ) {
            IppsBigNumState* pU = cpBigNumListGet(&pList);
               PMA_add(pU, pT, pT, pModulo);             // R = X^3 +a*X*Z^4
               PMA_add(pU, pU, pT, pModulo);
               PMA_sub(pR, pR, pU, pModulo);
         }
         else {
            PMA_mule(pT, pT, ECP_AENC(pECC), pMont);  // R = X^3 +a*X*Z^4
            PMA_add(pR, pR, pT, pModulo);
         }
           PMA_mule(pT, pZ6, ECP_BENC(pECC), pMont);    // R = X^3 +a*X*Z^4 + b*Z^6
           PMA_add(pR, pR, pT, pModulo);

      }
      /* case Z == 1 */
      else {
         if( ECP_AMI3(pECC) ) {
               PMA_add(pT, ECP_POINT_X(pPoint), ECP_POINT_X(pPoint), pModulo); // R = X^3 +a*X
               PMA_add(pT, pT, ECP_POINT_X(pPoint), pModulo);
               PMA_sub(pR, pR, pT, pModulo);
         }
         else {
               PMA_mule(pT, ECP_POINT_X(pPoint), ECP_AENC(pECC), pMont);  // R = X^3 +a*X
               PMA_add(pR, pR, pT, pModulo);
         }
         PMA_add(pR, pR, ECP_BENC(pECC), pModulo);                   // R = X^3 +a*X + b
      }
      PMA_sqre(pT, ECP_POINT_Y(pPoint), pMont);  // T = Y^2
      return 0==Cmp_BN(pR, pT);
   }
}

/*
// ECCP_ComparePoint
//
// Compare two points:
//    returns 0 => pP==pQ (maybe both pP and pQ are at Infinity)
//    returns 1 => pP!=pQ
//
// Note
// In general we chech:
//    P_X*Q_Z^2 ~ Q_X*P_Z^2
//    P_Y*Q_Z^3 ~ Q_Y*P_Z^3
*/
int ECCP_ComparePoint(const IppsECCPPointState* pP,
                      const IppsECCPPointState* pQ,
                      const IppsECCPState* pECC,
                      BigNumNode* pList)
{
   /* P or/and Q at Infinity */
   if( ECCP_IsPointAtInfinity(pP) )
      return ECCP_IsPointAtInfinity(pQ)? 0:1;
   if( ECCP_IsPointAtInfinity(pQ) )
      return ECCP_IsPointAtInfinity(pP)? 0:1;

   /* (P_Z==1) && (Q_Z==1) */
    if( ECP_POINT_AFFINE(pP) && ECP_POINT_AFFINE(pQ) )
      return ((0==Cmp_BN(ECP_POINT_X(pP),ECP_POINT_X(pQ))) && (0==Cmp_BN(ECP_POINT_Y(pP),ECP_POINT_Y(pQ))))? 0:1;

   {
      IppsMontState* pMont = ECP_PMONT(pECC);

      IppsBigNumState* pPtmp = cpBigNumListGet(&pList);
      IppsBigNumState* pQtmp = cpBigNumListGet(&pList);
      IppsBigNumState* pPZ   = cpBigNumListGet(&pList);
      IppsBigNumState* pQZ   = cpBigNumListGet(&pList);

      /* P_X*Q_Z^2 ~ Q_X*P_Z^2 */
      if( !ECP_POINT_AFFINE(pQ) ) {
         PMA_sqre(pQZ, ECP_POINT_Z(pQ), pMont);      /* Ptmp = P_X*Q_Z^2 */
         PMA_mule(pPtmp, ECP_POINT_X(pP), pQZ, pMont);
      }
      else {
         PMA_set(pPtmp, ECP_POINT_X(pP));
      }
      if( !ECP_POINT_AFFINE(pP) ) {
         PMA_sqre(pPZ, ECP_POINT_Z(pP), pMont);      /* Qtmp = Q_X*P_Z^2 */
         PMA_mule(pQtmp, ECP_POINT_X(pQ), pPZ, pMont);
      }
      else {
         PMA_set(pQtmp, ECP_POINT_X(pQ));
      }
      if ( Cmp_BN(pPtmp, pQtmp) )
         return 1;   /* poionts are different: (P_X*Q_Z^2) != (Q_X*P_Z^2) */

      /* P_Y*Q_Z^3 ~ Q_Y*P_Z^3 */
      if( !ECP_POINT_AFFINE(pQ) ) {
         PMA_mule(pQZ, pQZ, ECP_POINT_Z(pQ), pMont); /* Ptmp = P_Y*Q_Z^3 */
         PMA_mule(pPtmp, ECP_POINT_Y(pP), pQZ, pMont);
      }
      else {
         PMA_set(pPtmp, ECP_POINT_Y(pP));
      }
      if( !ECP_POINT_AFFINE(pP) ) {
         PMA_mule(pPZ, pPZ, ECP_POINT_Z(pP), pMont); /* Qtmp = Q_Y*P_Z^3 */
         PMA_mule(pQtmp, ECP_POINT_Y(pQ), pPZ, pMont);
      }
      else {
         PMA_set(pQtmp, ECP_POINT_Y(pQ));
      }
      return Cmp_BN(pPtmp, pQtmp)? 1:0;
   }
}

/*
// ECCP_NegPoint
//
// Negative point
*/
void ECCP_NegPoint(const IppsECCPPointState* pP,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC)
{
   /* test point at Infinity */
   if( ECCP_IsPointAtInfinity(pP) )
      ECCP_SetPointToInfinity(pR);

   else {
      IppsMontState* pMont = ECP_PMONT(pECC);
      IppsBigNumState* pModulo = MNT_MODULO(pMont);

      if( pP!=pR ) {
         PMA_set(ECP_POINT_X(pR), ECP_POINT_X(pP));
         PMA_set(ECP_POINT_Z(pR), ECP_POINT_Z(pP));
      }
      PMA_sub(ECP_POINT_Y(pR), pModulo, ECP_POINT_Y(pP), pModulo);
      ECP_POINT_AFFINE(pR) = ECP_POINT_AFFINE(pP);
   }
}

/*
// ECCP_DblPoint
//
// Double point
*/
void ECCP_DblPoint(const IppsECCPPointState* pP,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList)
{
   /* P at infinity */
   if( ECCP_IsPointAtInfinity(pP) )
      ECCP_SetPointToInfinity(pR);

   else {
      IppsMontState* pMont = ECP_PMONT(pECC);
      IppsBigNumState* pModulo = MNT_MODULO(pMont);

      IppsBigNumState* bnV = cpBigNumListGet(&pList);
      IppsBigNumState* bnU = cpBigNumListGet(&pList);
      IppsBigNumState* bnM = cpBigNumListGet(&pList);
      IppsBigNumState* bnS = cpBigNumListGet(&pList);
      IppsBigNumState* bnT = cpBigNumListGet(&pList);

      /* M = 3*X^2 + A*Z^4 */
       if( ECP_POINT_AFFINE(pP) ) {
           PMA_sqre(bnU, ECP_POINT_X(pP), pMont);
           PMA_add(bnM, bnU, bnU, pModulo);
           PMA_add(bnM, bnM, bnU, pModulo);
           PMA_add(bnM, bnM, ECP_AENC(pECC), pModulo);
        }
       else if( ECP_AMI3(pECC) ) {
           PMA_sqre(bnU, ECP_POINT_Z(pP), pMont);
           PMA_add(bnS, ECP_POINT_X(pP), bnU, pModulo);
           PMA_sub(bnT, ECP_POINT_X(pP), bnU, pModulo);
           PMA_mule(bnM, bnS, bnT, pMont);
           PMA_add(bnU, bnM, bnM, pModulo);
           PMA_add(bnM, bnU, bnM, pModulo);
        }
       else {
           PMA_sqre(bnU, ECP_POINT_X(pP), pMont);
           PMA_add(bnM, bnU, bnU, pModulo);
           PMA_add(bnM, bnM, bnU, pModulo);
           PMA_sqre(bnU, ECP_POINT_Z(pP), pMont);
           PMA_sqre(bnU, bnU, pMont);
           PMA_mule(bnU, bnU, ECP_AENC(pECC), pMont);
           PMA_add(bnM, bnM, bnU, pModulo);
        }

      PMA_add(bnV, ECP_POINT_Y(pP), ECP_POINT_Y(pP), pModulo);

      /* R_Z = 2*Y*Z */
      if( ECP_POINT_AFFINE(pP) ) {
         PMA_set(ECP_POINT_Z(pR), bnV);
      }
      else {
         PMA_mule(ECP_POINT_Z(pR), bnV, ECP_POINT_Z(pP), pMont);
      }

      /* S = 4*X*Y^2 */
      PMA_sqre(bnT, bnV, pMont);
      PMA_mule(bnS, bnT, ECP_POINT_X(pP), pMont);

      /* R_X = M^2 - 2*S */
      PMA_sqre(bnU, bnM, pMont);
      PMA_sub(bnU, bnU, bnS, pModulo);
      PMA_sub(ECP_POINT_X(pR), bnU, bnS, pModulo);

      /* T = 8*Y^4 */
      PMA_mule(bnV, bnV, ECP_POINT_Y(pP), pMont);
      PMA_mule(bnT, bnT, bnV, pMont);

      /* R_Y = M*(S - R_X) - T */
      PMA_sub(bnS, bnS, ECP_POINT_X(pR), pModulo);
      PMA_mule(bnS, bnS, bnM, pMont);
      PMA_sub(ECP_POINT_Y(pR), bnS, bnT, pModulo);

      ECP_POINT_AFFINE(pR) = 0;
   }
}

/*
// ECCP_AddPoint
//
// Add points
*/
void ECCP_AddPoint(const IppsECCPPointState* pP,
                   const IppsECCPPointState* pQ,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList)
{
   /* test stupid call */
   if( pP == pQ ) {
      ECCP_DblPoint(pP, pR, pECC, pList);
      return;
   }

   /* prevent operation with point at Infinity */
   if( ECCP_IsPointAtInfinity(pP) ) {
      ECCP_CopyPoint(pQ, pR);
      return;
   }
   if( ECCP_IsPointAtInfinity(pQ) ) {
      ECCP_CopyPoint(pP, pR);
      return;
   }

   /*
   // addition
   */
   {
      IppsMontState* pMont = ECP_PMONT(pECC);
      IppsBigNumState* pModulo = MNT_MODULO(pMont);

      IppsBigNumState* bnU0 = cpBigNumListGet(&pList);
      IppsBigNumState* bnS0 = cpBigNumListGet(&pList);
      IppsBigNumState* bnU1 = cpBigNumListGet(&pList);
      IppsBigNumState* bnS1 = cpBigNumListGet(&pList);
      IppsBigNumState* bnW  = cpBigNumListGet(&pList);
      IppsBigNumState* bnR  = cpBigNumListGet(&pList);
      IppsBigNumState *bnT  = bnU0;
      IppsBigNumState *bnM  = bnS0;

      /* U0 = P_X * Q_Z^2 */
      /* S0 = P_Y * Q_Z^3 */
      if( ECP_POINT_AFFINE(pQ) ) {
         PMA_set(bnU0, ECP_POINT_X(pP));
         PMA_set(bnS0, ECP_POINT_Y(pP));
      }
      else {
         PMA_sqre(bnW, ECP_POINT_Z(pQ),      pMont);
         PMA_mule(bnU0,ECP_POINT_X(pP), bnW, pMont);
         PMA_mule(bnW, ECP_POINT_Z(pQ), bnW, pMont);
         PMA_mule(bnS0,ECP_POINT_Y(pP), bnW, pMont);
      }

      /* U1 = Q_X * P_Z^2 */
      /* S1 = Q_Y * P_Z^3 */
      if( ECP_POINT_AFFINE(pP) ) {
         PMA_set(bnU1, ECP_POINT_X(pQ));
         PMA_set(bnS1, ECP_POINT_Y(pQ));
      }
      else {
         PMA_sqre(bnW, ECP_POINT_Z(pP),      pMont);
         PMA_mule(bnU1,ECP_POINT_X(pQ), bnW, pMont);
         PMA_mule(bnW, ECP_POINT_Z(pP), bnW, pMont);
         PMA_mule(bnS1,ECP_POINT_Y(pQ), bnW, pMont);
      }

      /* W = U0-U1 */
      /* R = S0-S1 */
      PMA_sub(bnW, bnU0, bnU1, pModulo);
      PMA_sub(bnR, bnS0, bnS1, pModulo);

      if( IsZero_BN(bnW) ) {
         if( IsZero_BN(bnR) ) {
            ECCP_DblPoint(pP, pR, pECC, pList);
            return;
         }
         else {
            ECCP_SetPointToInfinity(pR);
            return;
         }
      }

      /* T = U0+U1 */
      /* M = S0+S1 */
      PMA_add(bnT, bnU0, bnU1, pModulo);
      PMA_add(bnM, bnS0, bnS1, pModulo);

      /* R_Z = P_Z * Q_Z * W */
      if( ECP_POINT_AFFINE(pQ) && ECP_POINT_AFFINE(pP) ) {
         PMA_set(ECP_POINT_Z(pR), bnW);
      }
      else {
         if( ECP_POINT_AFFINE(pQ) ) {
            PMA_set(bnU1, ECP_POINT_Z(pP));
         }
         else if( ECP_POINT_AFFINE(pP) ) {
            PMA_set(bnU1, ECP_POINT_Z(pQ));
         }
         else {
            PMA_mule(bnU1, ECP_POINT_Z(pP), ECP_POINT_Z(pQ), pMont);
         }
         PMA_mule(ECP_POINT_Z(pR), bnU1, bnW, pMont);
      }

      PMA_sqre(bnU1, bnW, pMont);         /* U1 = W^2     */
      PMA_mule(bnS1, bnT, bnU1, pMont);   /* S1 = T * W^2 */

      /* R_X = R^2 - T * W^2 */
      PMA_sqre(ECP_POINT_X(pR), bnR, pMont);
      PMA_sub(ECP_POINT_X(pR), ECP_POINT_X(pR), bnS1, pModulo);

      /* V = T * W^2 - 2 * R_X  (S1) */
      PMA_sub(bnS1, bnS1, ECP_POINT_X(pR), pModulo);
      PMA_sub(bnS1, bnS1, ECP_POINT_X(pR), pModulo);

      /* R_Y = (V * R - M * W^3) /2 */
      PMA_mule(ECP_POINT_Y(pR), bnS1, bnR, pMont);
      PMA_mule(bnU1, bnU1, bnW, pMont);
      PMA_mule(bnU1, bnU1, bnM, pMont);
      PMA_sub(bnU1, ECP_POINT_Y(pR), bnU1, pModulo);
      PMA_div2(ECP_POINT_Y(pR), bnU1, pModulo);

      ECP_POINT_AFFINE(pR) = 0;
   }
}

/*
// ECCP_MulPoint
//
// Multiply point by scalar
*/
void ECCP_MulPoint(const IppsECCPPointState* pP,
                   const IppsBigNumState* bnN,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList)
{
   /* test zero scalar or input point at Infinity */
   if( IsZero_BN(bnN) || ECCP_IsPointAtInfinity(pP) ) {
      ECCP_SetPointToInfinity(pR);
      return;
   }

   /*
   // scalar multiplication
   */
   else {
      IppsECCPPointState T;
      IppsECCPPointState U;
      IppsBigNumState* bnKH = cpBigNumListGet(&pList);
      Ipp32u* pK;
      Ipp32u* pH;
      Ipp32u carry;
      int lenKH;
      int bitH;

      /* init result */
      ECCP_CopyPoint(pP, pR);

      /* if scalar is negative */
      if( IppsBigNumNEG == BN_SIGN(bnN) ) {
         /* negative R */
         ECCP_NegPoint(pR, pR, pECC);
      }

      /* copy K = N and compute H=3*K */
      lenKH = BN_SIZE(bnN)+1;
      pK = BN_NUMBER(bnKH);
      pH = BN_BUFFER(bnKH);
      Cpy_BNU(BN_NUMBER(bnN), pK, BN_SIZE(bnN));
      pK[lenKH-1] = 0;
      ippsAdd_BNU(pK, pK, pH, lenKH, &carry);
      ippsAdd_BNU(pK, pH, pH, lenKH, &carry);

      /* init temporary T = (X/Z^2, Y/Z^3, 1) */
      ECP_POINT_X(&T) = cpBigNumListGet(&pList);
      ECP_POINT_Y(&T) = cpBigNumListGet(&pList);
      ECP_POINT_Z(&T) = cpBigNumListGet(&pList);
      ECCP_GetPointAffine(ECP_POINT_X(&T), ECP_POINT_Y(&T), pR, pECC, pList);
      ECCP_SetPointAffine(ECP_POINT_X(&T), ECP_POINT_Y(&T), &T, pECC);

      /* temporary point U =-T */
      ECP_POINT_X(&U) = cpBigNumListGet(&pList);
      ECP_POINT_Y(&U) = cpBigNumListGet(&pList);
      ECP_POINT_Z(&U) = cpBigNumListGet(&pList);
      ECCP_NegPoint(&T, &U, pECC);

      for(bitH=MSB_BNU(pH, lenKH)-1; bitH>0; bitH--) {
         int hBit = TST_BIT(pH, bitH);
         int kBit = TST_BIT(pK, bitH);
         ECCP_DblPoint(pR, pR, pECC, pList);
         if( hBit && !kBit )
            ECCP_AddPoint(pR, &T, pR, pECC, pList);
         if(!hBit &&  kBit )
            ECCP_AddPoint(pR, &U, pR, pECC, pList);
      }
   }
}
#if 0
void ECCP_MulPoint(const IppsECCPPointState* pP,
                   const IppsBigNumState* bnN,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList)
{
   /* test zero scalar or input point at Infinity */
   if( IsZero_BN(bnN) || ECCP_IsPointAtInfinity(pP) ) {
      ECCP_SetPointToInfinity(pR);
      return;
   }

   /*
   // scalar multiplication
   */
   else {
      int n;

      IppsECCPPointState tmpPoint[15];
      IppsBigNumState* pX = cpBigNumListGet(&pList);
      IppsBigNumState* pY = cpBigNumListGet(&pList);

      /* allocate temporary points */
      {
         for(n=0; n<15; n++) {
            ECP_POINT_X(&tmpPoint[n]) = cpBigNumListGet(&pList);
            ECP_POINT_Y(&tmpPoint[n]) = cpBigNumListGet(&pList);
            ECP_POINT_Z(&tmpPoint[n]) = cpBigNumListGet(&pList);
         }
      }

      /* precomputation */
      if( IppsBigNumPOS == BN_SIGN(bnN) )
         ECCP_CopyPoint(pP, &tmpPoint[0]);
      else
         ECCP_NegPoint(pP, &tmpPoint[0], pECC);

      ECCP_GetPointAffine(pX, pY, &tmpPoint[0], pECC, pList);
      ECCP_SetPointAffine(pX, pY, &tmpPoint[0], pECC);

      for(n=1; n<15; n+=2) {
         ECCP_DblPoint(&tmpPoint[n/2], &tmpPoint[n], pECC, pList);
         ECCP_GetPointAffine(pX, pY, &tmpPoint[n], pECC, pList);
         ECCP_SetPointAffine(pX, pY, &tmpPoint[n], pECC);

         ECCP_AddPoint(&tmpPoint[n],   &tmpPoint[0], &tmpPoint[n+1], pECC, pList);
         ECCP_GetPointAffine(pX, pY, &tmpPoint[n+1], pECC, pList);
         ECCP_SetPointAffine(pX, pY, &tmpPoint[n+1], pECC);
      }

      /* init result */
      ECCP_SetPointToInfinity(pR);

      for(n=BN_SIZE(bnN); n>0; n--) {
         Ipp32u scalar = BN_NUMBER(bnN)[n-1];

         int shift;
         for(shift=(32-4); shift>=0; shift-=4) {
            int m;
            int tblIdx = (scalar>>shift) & 0xF;

            if( !ECCP_IsPointAtInfinity(pR) ) {
               for(m=0; m<4; m++)
                  ECCP_DblPoint(pR, pR, pECC, pList);
            }
            if( tblIdx )
               ECCP_AddPoint(pR, &tmpPoint[tblIdx-1], pR, pECC, pList);
         }
      }
   }
}
#endif

/*
// ECCP_ProdPoint
//
// Point product
*/
void ECCP_ProdPoint(const IppsECCPPointState* pP,
                    const IppsBigNumState*    bnPscalar,
                    const IppsECCPPointState* pQ,
                    const IppsBigNumState*    bnQscalar,
                    IppsECCPPointState* pR,
                    const IppsECCPState* pECC,
                    BigNumNode* pList)
{
   /* test zero scalars */
   if( IsZero_BN(bnPscalar) ) {
      ECCP_MulPoint(pQ, bnQscalar, pR, pECC, pList);
      return;
   }
   if( IsZero_BN(bnQscalar) ) {
      ECCP_MulPoint(pP, bnPscalar, pR, pECC, pList);
      return;
   }

   /*
   // point product
   */
   else {
      int n;
      Ipp32u* pbnPscalar = BN_NUMBER(bnPscalar);
      int bnPscalarSize  = BN_SIZE(bnPscalar);
      Ipp32u* pbnQscalar = BN_NUMBER(bnQscalar);
      int bnQscalarSize  = BN_SIZE(bnQscalar);

      int size = bnPscalarSize>bnQscalarSize? bnPscalarSize : bnQscalarSize;

      IppsECCPPointState* pPointPQ[4] = {NULL, NULL, NULL, NULL};

      /* allocate temporary PQ point */
      IppsECCPPointState PQ;
      ECP_POINT_X(&PQ) = cpBigNumListGet(&pList);
      ECP_POINT_Y(&PQ) = cpBigNumListGet(&pList);
      ECP_POINT_Z(&PQ) = cpBigNumListGet(&pList);

      /* init temporary point array */
      if(IppsBigNumPOS == BN_SIGN(bnPscalar))
         pPointPQ[1] = (IppsECCPPointState*)pP;
      else {
         IppsECCPPointState P;
         ECP_POINT_X(&P) = cpBigNumListGet(&pList);
         ECP_POINT_Y(&P) = cpBigNumListGet(&pList);
         ECP_POINT_Z(&P) = cpBigNumListGet(&pList);
         ECCP_NegPoint(pP, &P, pECC);
         pPointPQ[1] = &P;
      }
      if(IppsBigNumPOS == BN_SIGN(bnQscalar))
         pPointPQ[2] = (IppsECCPPointState*)pQ;
      else {
         IppsECCPPointState Q;
         ECP_POINT_X(&Q) = cpBigNumListGet(&pList);
         ECP_POINT_Y(&Q) = cpBigNumListGet(&pList);
         ECP_POINT_Z(&Q) = cpBigNumListGet(&pList);
         ECCP_NegPoint(pQ, &Q, pECC);
         pPointPQ[2] = &Q;
      }

      ECCP_AddPoint(pPointPQ[1], pPointPQ[2], &PQ, pECC, pList);
      ECCP_GetPointAffine(ECP_POINT_X(pR), ECP_POINT_Y(pR), &PQ, pECC, pList);
      ECCP_SetPointAffine(ECP_POINT_X(pR), ECP_POINT_Y(pR), &PQ, pECC);
      pPointPQ[3] = &PQ;

      /* pad scalars by zeros */
      ZEXPAND_BNU(pbnPscalar,bnPscalarSize, size);
      ZEXPAND_BNU(pbnQscalar,bnQscalarSize, size);

      /* init result */
      ECCP_SetPointToInfinity(pR);

      for(n=size; n>0; n--) {
         Ipp32u scalarPn = pbnPscalar[n-1];
         Ipp32u scalarQn = pbnQscalar[n-1];

         int nBit;
         for(nBit=31; nBit>=0; nBit--) {
            int
            PnQnBits = scalarPn&0x80000000? 1:0;
            PnQnBits+= scalarQn&0x80000000? 2:0;

            if( !ECCP_IsPointAtInfinity(pR) )
               ECCP_DblPoint(pR, pR, pECC, pList);
            if( PnQnBits )
               ECCP_AddPoint(pR, pPointPQ[PnQnBits], pR, pECC, pList);

            scalarPn <<= 1;
            scalarQn <<= 1;
         }
      }
   }
}

#endif /* _IPP_v50_ */
