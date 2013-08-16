/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//    This software is supplied under the terms of a license agreement or
//    nondisclosure agreement with Intel Corporation and may not be copied
//    or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 Intel Corporation. All Rights Reserved.
//
//    Intel(R) Integrated Performance Primitives
//    Cryptographic Primitives (ippcp)
//    Asymmetric Tate Pairing. Millar's Algorithm with Denominator Elimination.
//
//    Context:
//       ()
//       ()
//
//       ()
//       ()
//       ()
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcptatep.h"
#include "pcpgfp.h"
#include "pcpgfpext.h"
#include "pcpgfpqext.h"
#include "pcpecgfp.h"
#include "pcpbnu.h"


__INLINE cpFFchunk* gfpxqxAdd_GFP(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPXQ)
{
   if(pA!=pR)
      gfpFFelementCopy(pA, pR, GFPXQX_FESIZE(pGFPXQ));
   return gfpAdd(pR, pB, pR, GFPX_GROUNDGF(GFPXQX_GROUNDGF(pGFPXQ)));
}
__INLINE cpFFchunk* gfpxqxSub_GFP(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPXQ)
{
   if(pA!=pR)
      gfpFFelementCopy(pA, pR, GFPXQX_FESIZE(pGFPXQ));
   return gfpSub(pR, pB, pR, GFPX_GROUNDGF(GFPXQX_GROUNDGF(pGFPXQ)));
}

static void transform(const cpFFchunk* pA, const cpFFchunk* pTrans, cpFFchunk* pT, IppsGFPXState* pFF)
{
   cpFFchunk* pTmp = gfpxGetPool(1, pFF);
   cpFFchunk* pR   = gfpxGetPool(1, pFF);

   Ipp32u elemSize = GFPX_FESIZE(pFF);
   Ipp32u groundElemSize = GFPX_GROUNDFESIZE(pFF);
   Ipp32u d;

   GFPX_ZERO(pR, pFF);

   for(d=0; d<=GFPX_DEGREE(pFF); d++) {
      gfpxMontMul_GFP(pTrans, pA, pTmp, pFF);
      gfpxAdd(pTmp, pR, pR, pFF);
      pTrans += elemSize;
      pA += groundElemSize;
   }
   gfpxFFelementCopy(pR, pT, pFF);

   gfpxReleasePool(2, pFF);
}


void cpTatePairing(const cpFFchunk* pPoint,
                   const cpFFchunk* pQx,
                   const cpFFchunk* pQy,
                   cpFFchunk* pR,
                   IppsTatePairingDE3State* pCtx)
{
   int i;

   /* target field context */
   IppsGFPXQState* pGT = PAIRING_GT(pCtx);

   /* EC over prime field context */
   IppsGFPECState* pEC = PAIRING_G1(pCtx);

   /* prime field context  */
   IppsGFPState* pGFP = ECP_FIELD(pEC);
   Ipp32u elementSize = GFP_FESIZE(pGFP);

   /* get binary representation of EC's order */
   Ipp32u* pOrder = ECP_R(pEC);
   int len = (int)( ECP_ORDBITSIZE(pEC) );

   /* some temporary over prime GF */
   cpFFchunk* w = ecgfpGetPool(3, pEC);
   cpFFchunk* v = w+elementSize;
   cpFFchunk* t1= v+elementSize;
   cpFFchunk* t2=t1+elementSize;
   cpFFchunk* t3=t2+elementSize;
   cpFFchunk* ty=t3+elementSize;
   cpFFchunk* ry=ty+elementSize;
   //cpFFchunk* px=ry+elementSize;
   //cpFFchunk* py=px+elementSize;
   const cpFFchunk* px = pPoint;
   const cpFFchunk* py = pPoint+elementSize;

   /* some temporary over quadratic extension of GF(q^d) */
   cpFFchunk* tx  = gfpxqxGetPool(4, pGT);
   cpFFchunk* rx  = tx+GFPXQX_FESIZE(pGT);
   cpFFchunk* tt1 = rx+GFPXQX_FESIZE(pGT);
   cpFFchunk* tt2 =tt1+GFPXQX_FESIZE(pGT);

   /* two temporary points and their coordinates */
   cpFFchunk *T, *X, *Y, *Z;
   cpFFchunk *T2,*X2,*Y2,*Z2;
   T = ecgfpGetPool(1, pEC);
   X = T;
   Y = T+elementSize;
   Z = T+elementSize*2;
   T2= ecgfpGetPool(1, pEC);
   X2= T2;
   Y2= T2+elementSize;
   Z2= T2+elementSize*2;

   /* copy input EC point */
   gfpFFelementCopy(pPoint, X, elementSize*3);

   /* init */
   GFPXQX_SET_MONT_ONE(rx, pGT);
   GFP_MONT_ONE(ry, pGFP);


   /*
   // Miller's algorithm
   */
   for(i=len-2; i>=0; i--) {
      Ipp32u bit = TST_BIT(pOrder, i);

      /* double point (T1=T+T) */
      gfpMontMul(Z, Z, ty, pGFP);            // ty = Z^2
      gfpMontMul(ty, ty, t1, pGFP);          // t1 = Z^4
      gfpMontMul(t1, ECP_A(pEC), t1, pGFP);  // t1 = a*Z^4
      gfpMontMul(X, X, w, pGFP);             // w = X^2
      gfpAdd(w, w, t2, pGFP);                // t2 = 2 * X^2
      gfpAdd(w, t2, w, pGFP);                // w = 3 * X^2
      gfpAdd(w, t1, w, pGFP);                // w = 3 * X^2 + a * Z^4
      gfpMontMul(Y, Y, t1, pGFP);            // t1 = Y^2
      gfpAdd(t1, t1, t3, pGFP);              // t3 = 2* Y^2
      gfpMontMul(t3, X, v, pGFP);            // v = 2 * X * Y^2
      gfpAdd(v, v, v, pGFP);                 // v = 4 * X * Y^2

      gfpMontMul(w, w, X2, pGFP);            // X2 = w^2
      gfpSub(X2, v, X2, pGFP);               // X2 = w^2 - v
      gfpSub(X2, v, X2, pGFP);               // X2 = w^2 - 2 * w
      gfpMontMul(t3, t3, t3, pGFP);          // t3 = 4 * Y^4
      gfpAdd(t3, t3, t3, pGFP);              // t3 = 8 * Y^4
      gfpSub(v, X2, Y2, pGFP);               // Y2 = v - X2
      gfpMontMul(Y2, w, Y2, pGFP);           // Y2 = w * (v - X2)
      gfpSub(Y2, t3, Y2, pGFP);              // Y2 = w * (v - X2) - 8 * Y^4
      gfpMontMul(Y, Z, Z2, pGFP);            // Z2 = Y * Z
      gfpAdd(Z2, Z2, Z2, pGFP);              // Z2 = 2 * Y * Z

      /* compute line */
      gfpMontMul(ty, w, t2, pGFP);           // t2 = w * Z^2
      gfpxqxMontMul_GFP(pQx, t2, tt1, pGT);  // tt1 = w * Z^2 * Qx
      gfpMontMul(w, X, t2, pGFP);            // t2 = w * X
      gfpSub(t2, t1, t2, pGFP);              // t2 = w * X - Y^2
      gfpSub(t2, t1, t2, pGFP);              // t2 = w * X - 2 * Y^2
      gfpMontMul(ty, Z2, ty, pGFP);          // ty = Z2 * Z^2
      gfpxqxMontMul_GFP(pQy, ty, tx, pGT);   // tx = ty * Qy
      gfpxqxSub(tx, tt1, tx, pGT);           // tx = ty * Qy - w * Z^2 * Qx
      gfpxqxAdd_GFP(tx, t2, tx, pGT);        // tx = ty * Qy - w * Z^2 * Qx + w * X - 2 * Y^2

      /* copy T=T2 */
      gfpFFelementCopy(X2, X, elementSize*3);

      /* udpate rx, ry */
      gfpxqxMontMul(rx, rx, tt1, pGT);       // tt1 = rx * rx
      gfpxqxMontMul(tx, tt1, rx, pGT);       // rx = tx * rx * rx
      gfpMontMul(ry, ry, t1, pGFP);          // t1 = ry * ry
      gfpMontMul(ty, t1, ry, pGFP);          // ry = ty * ry * ry


      if(bit && i) {
         /* add point (T1=T1+T) */
         gfpMontMul(Z, Z, t1, pGFP);         // t1 = Z^2
         gfpMontMul(px, t1, w, pGFP);        // w = px * Z^2
         gfpSub(w, X, w, pGFP);              // w = px * Z^2 - X
         gfpMontMul(t1, Z, t1, pGFP);        // t1 = Z^3
         gfpMontMul(py, t1, v, pGFP);        // v = py * Z^3
         gfpSub(v, Y, v, pGFP);              // v = py * Z^3 - Y

         gfpMontMul(w, w, t1, pGFP);         // t1 = w^2
         gfpMontMul(w, t1, t2, pGFP);        // t2 = w^3
         gfpMontMul(X, t1, t3, pGFP);        // t3 = X * w^2
         gfpMontMul(v, v, X2, pGFP);         // X2 = v^2
         gfpSub(X2, t2, X2, pGFP);           // X2 = v^2 - w^3
         gfpSub(X2, t3, X2, pGFP);           // X2 = v^2 - w^3 - X * w^2
         gfpSub(X2, t3, X2, pGFP);           // X2 = v^2 - w^3 - 2 * X * w^2
         gfpSub(t3, X2, Y2, pGFP);           // Y2 = X * w^2 - X2
         gfpMontMul(Y2, v, Y2, pGFP);        // Y2 = v * (X * w^2 - X2)
         gfpMontMul(t2, Y, t2, pGFP);        // t2 = Y * w^3
         gfpSub(Y2, t2, Y2, pGFP);           // Y2 = v * (X * w^2 - X2) - Y * w^3
         gfpMontMul(w, Z, Z2, pGFP);         // Z2 = w * Z

         /* compute tx, ty */
         gfpFFelementCopy(Z2, ty, elementSize); // ty = Z2
         gfpxqxSub_GFP(pQy, py, tx, pGT);       // tx = Qy - py
         gfpxqxMontMul_GFP(tx, Z2, tx, pGT);    // tx = Z2 * (Qy - py)
         gfpxqxSub_GFP(pQx, px, tt1, pGT);      // tt1 = Qx - px
         gfpxqxMontMul_GFP(tt1, v, tt1, pGT);   // tt1 = v * (Qx - px)
         gfpxqxSub(tx, tt1, tx, pGT);           // tx = Z2 * (Qy - py) - v * (Qx - px)

         /* copy T=T1 */
         gfpFFelementCopy(X2, X, elementSize*3);

         /* udpate rx, ry */
         gfpxqxMontMul(rx, tx, rx, pGT);        // rx = rx * tx
         gfpMontMul(ry, ty, ry, pGFP);          // ry = ry * ty
      }
   }

   gfpMontInv(ry, ry, pGFP);
   gfpxqxMontMul_GFP(rx, ry, pR, pGT);

   /*
   // do the final exponentation now
   */
   {
      cpFFchunk* pTrans = PAIRING_ALPHATO(pCtx);
      IppsGFPXState* pFF = GFPXQX_GROUNDGF(pGT);
      Ipp32u groundElemSize= GFPXQX_GROUNDFESIZE(pGT);

      transform(pR, pTrans, tt1, pFF);
      transform(pR+groundElemSize, pTrans, tt1+groundElemSize, pFF);

      gfpFFelementCopy(pR, tt2, groundElemSize*2);
      gfpxNeg(tt2+groundElemSize, tt2+groundElemSize, pFF);
      gfpxqxMontMul(tt1, tt2, tt2, pGT);

      gfpxNeg(tt1+groundElemSize, tt1+groundElemSize, pFF);
      gfpxqxMontMul(tt1, pR, tt1, pGT);

      gfpxqxMontInv(tt1, tt1, pGT);
      gfpxqxMontMul(tt1, tt2, pR, pGT);
   }
   gfpxqxMontExp(pR, PAIRING_EXP(pCtx), PAIRING_EXPLEN(pCtx), pR, pGT);
   gfpxqxMontDec(pR, pR, pGT); // excess operatinon

   gfpxqxReleasePool(4, pGT);
   ecgfpReleasePool(3+2, pEC);
}
