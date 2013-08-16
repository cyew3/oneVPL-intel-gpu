/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//    This software is supplied under the terms of a license agreement or
//    nondisclosure agreement with Intel Corporation and may not be copied
//    or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.
//
//    Intel(R) Integrated Performance Primitives
//    Cryptographic Primitives (ippcp)
//    GF(p) extension internal definitinons
//
//    Context:
//      gfpxAdd
//      gfpxAddGFP
//      gfpxSub
//      gfpxSubGFP
//      gfpxMul
//      gfpxMulGFP
//      gfpxNeg
//      gfpxDiv
//      gfpxDivisionCoeffs
//      gfpxInv
//      gfpxGetRandom
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpgfp.h"
#include "pcpgfpext.h"
#include "pcpbnuext.h"


Ipp32u gfpxIsUserModulusValid(const Ipp32u* pA, Ipp32u lenA, const IppsGFPXState* pGFPExtCtx)
{
   IppsGFPState* pGF = GFPX_GROUNDGF(pGFPExtCtx);
   Ipp32u usrCoeffSize = GFP_FESIZE32(pGF);
   /* expected modulus degree */
   Ipp32u degree = GFPX_DEGREE(pGFPExtCtx) +1;

   /* check if poly's coeffs belongs to GF(p) */
   Ipp32u isValid = gfpIsArrayOverGF(pA, lenA, pGF);

   if(isValid)
      /* check if x doesn't divide the poly */
      isValid = GFP_IS_ZERO(pA, usrCoeffSize)? 0:1;

   if(isValid)
      /* check if the poly is monic */
      isValid = GFP_IS_ONE(GFPX_IDX_ELEMENT(pA,degree,usrCoeffSize), 1)? 1:0;

   return isValid;
}

#if 0
cpFFchunk* gfpxCopyMod(const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pR, const IppsGFPXState* pGFPExtCtx)
{
   unsigned int i;
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   IppsGFPState* pGF = GFPX_GROUNDGF(pGFPExtCtx);
   Ipp32u offset = 0;

   for(i=0; i<=GFPX_DEGREE(pGFPExtCtx); i++) {
      if(pA && lenA>0) {
         gfpReduce(pA+offset, elementSize, pR+offset, pGF);
            if (lenA < elementSize)
               lenA = 0;
            else
               lenA -= elementSize;
      }
      else
         gfpFFelementPadd(0, pR+offset, elementSize);

      offset += elementSize;
   }

   return pR;
}
#endif

cpFFchunk* gfpxReduce(const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx)
{
   IppsGFPState* pGF = GFPX_GROUNDGF(pGFPExtCtx);
   Ipp32u usrCoeffSize = GFP_FESIZE32(pGF);
   Ipp32u ctxCoeffSize = GFP_FESIZE(pGF);

   /* degree of input polynomial */
   Ipp32u degA = (lenA+usrCoeffSize-1)/usrCoeffSize -1;
   /* degree of modulus */
   Ipp32u degM = GFPX_DEGREE(pGFPExtCtx)+1;

   if(degA<degM)
      gfpSetPolynomial(pA, lenA, pR, GFPX_DEGREE(pGFPExtCtx), pGF);

   else {
      Ipp32u degE = GFPX_DEGREE(pGFPExtCtx); /* element degree */
      cpFFchunk* pModulus = GFPX_MODULUS(pGFPExtCtx);
      cpFFchunk* pQ = gfpGetPool(1, pGF);
      cpFFchunk* pT = gfpGetPool(1, pGF);

      /* copy upper part of the input polynomial */
      pA = GFPX_IDX_ELEMENT(pA, degA-degM, usrCoeffSize);
      gfpSetPolynomial(pA+usrCoeffSize, lenA-(degA-degM+1)*usrCoeffSize, pR, degE, pGF);

      while( degA>=degM ) {
         Ipp32u i;

         /* q = R[degE] */
         gfpFFelementCopy(GFPX_IDX_ELEMENT(pR, degE, ctxCoeffSize), pQ, ctxCoeffSize);

         /* R[i] = R[i-1] - q*M[i] */
         for(i=degE; i>0; i--) {
            gfpMontMul(pQ, GFPX_IDX_ELEMENT(pModulus, i, ctxCoeffSize), pT, pGF);
            gfpSub(GFPX_IDX_ELEMENT(pR, i-1, ctxCoeffSize), pT, GFPX_IDX_ELEMENT(pR, i, ctxCoeffSize), pGF);
         }
         gfpFFelementCopyPadd(pA, usrCoeffSize, pR, ctxCoeffSize);
         gfpMontMul(pQ, pModulus, pT, pGF);
         gfpSub(pR, pT, pR, pGF);

         degA--;
         pA -= usrCoeffSize;
      }

      gfpReleasePool(2, pGF);
   }

   return pR;
}

Ipp32s gfpxFFelementIsEqu(const cpFFchunk* pA, const cpFFchunk* pB, const IppsGFPXState* pGFPExtCtx)
{
   return gfpFFelementCmp(pA, pB, GFPX_FESIZE(pGFPExtCtx)) == 0;
}

#if 0
cpFFchunk* gfpxGetElement(const cpFFchunk* pA, Ipp32u* pR, Ipp32u rLen, const IppsGFPXState* pGFPExtCtx)
{
   IppsGFPState* pGF = GFPX_GROUNDGF(pGFPExtCtx);
   Ipp32u usrElementSize = GFP_FESIZE32(pGF);
   Ipp32u ctxElementSize = GFP_FESIZE(pGF);
   Ipp32u degA = GFPX_DEGREE(pGFPExtCtx);

   cpFFchunk* pTmpR = pR;
   Ipp32u d;
   for (d=0; d<=degA && rLen; d++) {
      Ipp32u cpyLen = (rLen>=usrElementSize)? usrElementSize : rLen;
      gfpFFelementCopy(pA, pTmpR, cpyLen);
      pA += ctxElementSize;
      pTmpR += cpyLen;
      rLen -= cpyLen;
   }
   if(rLen)
      gfpFFelementPadd(0, pTmpR, rLen);
   return pR;
}
#endif

cpFFchunk* gfpxAdd(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR,
                   IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   Ipp32u i;
   Ipp32u offset=0;
   for(i=0; i<=GFPX_DEGREE(pGFPExtCtx); i++) {
      gfpAdd(pA+offset, pB+offset, pR+offset, GFPX_GROUNDGF(pGFPExtCtx));
      offset += elementSize;
   }
   return pR;
}

cpFFchunk* gfpxAdd_GFP(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR,
                       IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);

   gfpAdd(pA, pB, pR, GFPX_GROUNDGF(pGFPExtCtx));
   gfpFFelementCopy(pA+elementSize, pR+elementSize, elementSize*(GFPX_DEGREE(pGFPExtCtx)));
   return pR;
}

cpFFchunk* gfpxSub(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR,
                   IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   Ipp32u i;
   Ipp32u offset=0;

   for(i=0; i<=GFPX_DEGREE(pGFPExtCtx); i++) {
      gfpSub(pA+offset, pB+offset, pR+offset, GFPX_GROUNDGF(pGFPExtCtx));
      offset += elementSize;
   }
   return pR;
}

cpFFchunk* gfpxSub_GFP(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR,
                       IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);

   gfpSub(pA, pB, pR, GFPX_GROUNDGF(pGFPExtCtx));
   gfpFFelementCopy(pA+elementSize, pR+elementSize, elementSize*(GFPX_DEGREE(pGFPExtCtx)));
   return pR;
}

cpFFchunk* gfpxMontMul(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR,
                           IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   Ipp32u extElementSize = elementSize*(GFPX_DEGREE(pGFPExtCtx)+1);
   if (GFPX_IS_ZERO(pA, pGFPExtCtx) || GFPX_IS_ZERO(pB, pGFPExtCtx)) {
      gfpFFelementPadd(0, pR, extElementSize);
   } else { /* Non-trivial case */
      cpFFchunk* pCoeffs = GFPX_MODULUS(pGFPExtCtx);
      int degA = degree(pA, pGFPExtCtx);
      int degB = degree(pB, pGFPExtCtx);
      Ipp32u degR = GFPX_DEGREE(pGFPExtCtx)+1;
      IppsGFPState* pGFP = GFPX_GROUNDGF(pGFPExtCtx);
      int i, j;
      const Ipp32u numTmpVars = 3;
      cpFFchunk* b = gfpGetPool(numTmpVars, pGFP);
      cpFFchunk* r = b + elementSize;
      cpFFchunk* tmpGF = r + elementSize;
      cpFFchunk* tmpGFX = gfpxGetPool(1, pGFPExtCtx);


      if(degA<0 || degB<0) { /* Multiplication by 0 */
         GFP_ZERO(pR, extElementSize);
         gfpReleasePool(numTmpVars, pGFP);
         gfpxReleasePool(1, pGFPExtCtx);
         return pR;
      }
      gfpxMontMul_GFP(pA, GFPX_IDX_ELEMENT(pB, degB, elementSize), tmpGFX, pGFPExtCtx);
      for(i=degA+1; i<(int)degR; i++)
         gfpFFelementPadd(0,GFPX_IDX_ELEMENT(tmpGFX, i, elementSize), elementSize);

      for(i=degB-1; i>=0; i--) {
         int dm;

         gfpFFelementCopy(GFPX_IDX_ELEMENT(pB, i, elementSize), b, elementSize);
         gfpFFelementCopy(GFPX_IDX_ELEMENT(tmpGFX, degR-1, elementSize), r, elementSize);

         /* Shift left tmpGFX */
         for (j=degR-1; j>=1; j--)
            gfpFFelementCopy(GFPX_IDX_ELEMENT(tmpGFX, j-1, elementSize), GFPX_IDX_ELEMENT(tmpGFX, j, elementSize), elementSize);
         gfpFFelementPadd(0, tmpGFX, elementSize);

         if(gfpElementActualSize(r, elementSize) != 0) {
            for(dm=0; dm<(int)degR; dm++) {
               gfpMontMul(GFPX_IDX_ELEMENT(pCoeffs, dm, elementSize), r, tmpGF, pGFP);
               gfpSub(GFPX_IDX_ELEMENT(tmpGFX, dm, elementSize), tmpGF, GFPX_IDX_ELEMENT(tmpGFX, dm, elementSize), pGFP);
            }
         }
         for(dm=0; dm<=degA; dm++) {
            gfpMontMul(GFPX_IDX_ELEMENT(pA, dm, elementSize), b, tmpGF, pGFP);
            gfpAdd(GFPX_IDX_ELEMENT(tmpGFX, dm, elementSize), tmpGF, GFPX_IDX_ELEMENT(tmpGFX, dm, elementSize), pGFP);
         }
      }
      /* Copy tmpGFX to PR */
      gfpxFFelementCopy(tmpGFX, pR, pGFPExtCtx);
      /* release tmp vars */
      gfpReleasePool(numTmpVars, pGFP);
      gfpxReleasePool(1, pGFPExtCtx);
   }
   return pR;
}

cpFFchunk* gfpxMontMul_GFP(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR,
                           IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   Ipp32u i;
   Ipp32u offset=0;
   for(i=0; i<=GFPX_DEGREE(pGFPExtCtx); i++) {
      gfpMontMul(pA+offset, pB, pR+offset, GFPX_GROUNDGF(pGFPExtCtx));
      offset += elementSize;
   }
   return pR;
}

cpFFchunk* gfpxNeg(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   Ipp32u i;
   Ipp32u offset=0;

   for(i=0; i<=GFPX_DEGREE(pGFPExtCtx); i++) {
      gfpNeg(pA+offset, pR+offset, GFPX_GROUNDGF(pGFPExtCtx));
      offset += elementSize;
   }

   return pR;
}

cpFFchunk* gfpxDiv(const cpFFchunk* pA, const cpFFchunk* pB,
                   cpFFchunk* pQ, cpFFchunk* pR,
                   IppsGFPXState* pGFPExtCtx)
{
   int i, j, degA, degB;
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   Ipp32u pxElementSize = GFPX_PESIZE(pGFPExtCtx);
   IppsGFPState* pGFP = GFPX_GROUNDGF(pGFPExtCtx);
   Ipp32u elementPoolSize = GFP_PESIZE(pGFP);
   cpFFchunk* pTmpInvB;
   cpFFchunk* pTmp;
   cpFFchunk* pxTmpQ;
   cpFFchunk* pxTmpR;

   degA = degree(pA, pGFPExtCtx);
   degB = degree(pB, pGFPExtCtx);
   if(degB == -1) {    /* Division by 0 */
      return NULL;
   }
   pTmp = gfpGetPool(2, pGFP);
   if(degB == 0) {
      gfpMontInv(pB, pTmp, pGFP);
      gfpxMontMul_GFP(pA, pTmp, pQ, pGFPExtCtx);
      GFP_ZERO(pR, pxElementSize);
      gfpReleasePool(2, pGFP);
      return pQ;
   }
   pxTmpQ = gfpxGetPool(2, pGFPExtCtx);
   pxTmpR = pxTmpQ+pxElementSize;
   gfpxFFelementCopy(pA, pxTmpR, pGFPExtCtx);
   GFP_ZERO(pxTmpQ, pxElementSize);
   if(degA < degB) {
      gfpxFFelementCopy(pxTmpQ, pQ, pGFPExtCtx);
      gfpxFFelementCopy(pxTmpR, pR, pGFPExtCtx);
      gfpReleasePool(2, pGFP);
      gfpxReleasePool(2, pGFPExtCtx);
      return pQ;
   }
   pTmpInvB = pTmp+elementPoolSize;

   gfpMontInv(GFPX_IDX_ELEMENT(pB, degB, elementSize), pTmpInvB, pGFP);

   for(i=0; i<=degA-degB; i++) {
      if(GFP_IS_ZERO(GFPX_IDX_ELEMENT(pxTmpR, degA-i, elementSize), elementSize)) {
         GFP_ZERO(GFPX_IDX_ELEMENT(pxTmpQ, degA-degB-i, elementSize), elementSize);
         continue;
      }
      gfpMontMul(GFPX_IDX_ELEMENT(pxTmpR, degA-i, elementSize), pTmpInvB, GFPX_IDX_ELEMENT(pxTmpQ, degA-degB-i, elementSize), pGFP);
      GFP_ZERO(GFPX_IDX_ELEMENT(pxTmpR, degA-i, elementSize), elementSize);
      for(j=0; j<degB; j++) {
         gfpMontMul(GFPX_IDX_ELEMENT(pB, j, elementSize), GFPX_IDX_ELEMENT(pxTmpQ, degA-degB-i, elementSize), pTmp, pGFP);
         gfpSub(GFPX_IDX_ELEMENT(pxTmpR, degA-degB-i+j, elementSize), pTmp, GFPX_IDX_ELEMENT(pxTmpR, degA-degB-i+j, elementSize), pGFP);
      }
   }

   gfpxFFelementCopy(pxTmpQ, pQ, pGFPExtCtx);
   gfpxFFelementCopy(pxTmpR, pR, pGFPExtCtx);

   gfpReleasePool(2, pGFP);
   gfpxReleasePool(2, pGFPExtCtx);

   return pQ;
}

static cpFFchunk* gfpxDivCoeffs(const cpFFchunk* pB, cpFFchunk* pQ, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   IppsGFPState* pGFP = GFPX_GROUNDGF(pGFPExtCtx);
   cpFFchunk* t = gfpGetPool(2, pGFP);
   cpFFchunk* t1 = t + GFP_PESIZE(pGFP);
   Ipp32u degB = degree(pB, pGFPExtCtx);
   Ipp32u i;

   gfpxFFelementCopy(GFPX_MODULUS(pGFPExtCtx), pR, pGFPExtCtx);
   GFP_ZERO(pQ, GFPX_PESIZE(pGFPExtCtx));
   gfpMontInv(GFPX_IDX_ELEMENT(pB, degB, elementSize), t, pGFP);
   for(i=0; i<degB; i++) {
      cpFFchunk* ptr;
      gfpMontMul(GFPX_IDX_ELEMENT(pB, i, elementSize), t, t1, pGFP);
      ptr = GFPX_IDX_ELEMENT(pR, GFPX_DEGREE(pGFPExtCtx)+1-degB+i, elementSize);
      gfpSub(ptr, t1, ptr, pGFP);
   }
   gfpxDiv(pR, pB, pQ, pR, pGFPExtCtx);
   gfpFFelementCopy(t, GFPX_IDX_ELEMENT(pQ, GFPX_DEGREE(pGFPExtCtx)+1-degB, elementSize), elementSize);
   gfpReleasePool(2, pGFP);
   return pQ;
}
cpFFchunk* gfpxMontInv(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXState* pGFPExtCtx)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   if(GFPX_DEGREE(pGFPExtCtx) == 0) { /* a special case for degree=0 */
      gfpInv(pA, pR, GFPX_GROUNDGF(pGFPExtCtx));
   } else {
      const Ipp32u pxVars = 6;
      const Ipp32u pVars = 1;
      Ipp32u extElementSize = GFPX_PESIZE(pGFPExtCtx);
      cpFFchunk *ptr, *lastrem, *rem, *quo, *lastaux, *aux, *temp;
      cpFFchunk* invRem;
      IppsGFPState* pGFP = GFPX_GROUNDGF(pGFPExtCtx);

      ptr = gfpxGetPool(pxVars,pGFPExtCtx);
      lastrem = ptr;  ptr += extElementSize;
      rem = ptr;      ptr += extElementSize;
      quo = ptr;      ptr += extElementSize;
      lastaux = ptr;  ptr += extElementSize;
      aux = ptr;      ptr += extElementSize;
      temp = ptr;
      invRem = gfpGetPool(pVars, pGFP);

      gfpxFFelementCopy(pA, lastrem, pGFPExtCtx);
      GFPX_SET_MONT_ONE(lastaux, pGFPExtCtx);
      gfpxDivCoeffs(pA, quo, rem, pGFPExtCtx);
      gfpxNeg(quo, aux, pGFPExtCtx);
      while(degree(rem, pGFPExtCtx) > 0) {
         gfpxDiv(lastrem, rem, quo, temp, pGFPExtCtx);
         //gfpxFFelementCopy(rem, lastrem, pGFPExtCtx);
         SWAP_PTR(rem, lastrem); //
         //gfpxFFelementCopy(temp, rem, pGFPExtCtx);
         SWAP_PTR(temp, rem);
         gfpxNeg(quo, quo, pGFPExtCtx);
         gfpxMontMul(quo, aux, temp, pGFPExtCtx);
         gfpxAdd(temp, lastaux, temp, pGFPExtCtx);
         //gfpxFFelementCopy(aux, lastaux, pGFPExtCtx);
         SWAP_PTR(aux, lastaux);
         //gfpxFFelementCopy(temp, aux, pGFPExtCtx);
         SWAP_PTR(temp, aux);
      }
      if (GFP_IS_ZERO(rem, elementSize)) { /* gcd != 1 */
         gfpReleasePool(pVars, pGFP);
         gfpxReleasePool(pxVars, pGFPExtCtx);
         return NULL;
      }
      gfpMontInv(rem, invRem, pGFP);
      gfpxMontMul_GFP(aux, invRem, pR,pGFPExtCtx);

      gfpReleasePool(pVars, pGFP);
      gfpxReleasePool(pxVars, pGFPExtCtx);
   }
   return pR;
}


cpFFchunk* gfpxRand(cpFFchunk* pR, IppsGFPXState* pGFPExtCtx, IppBitSupplier rndFunc, void* pRndParam)
{
   Ipp32u elementSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
   Ipp32u i;
   Ipp32u offset=0;

   for(i=0; i<=GFPX_DEGREE(pGFPExtCtx); i++) {
      gfpRand(pR+offset, GFPX_GROUNDGF(pGFPExtCtx), rndFunc, pRndParam);
      offset += elementSize;
   }
   return pR;
}


cpFFchunk* gfpxMontExp(const cpFFchunk* pA, const Ipp32u* pB, Ipp32u bSize, cpFFchunk* pR,
                       IppsGFPXState* pGFPExtCtx)
{
   int bit;
   cpFFchunk* tmp = gfpxGetPool(1, pGFPExtCtx);
   GFPX_SET_MONT_ONE(tmp, pGFPExtCtx);
   for(bit=(int)MSB_BNU(pB, bSize); bit>=0; bit--) {
      gfpxMontMul(tmp, tmp, tmp, pGFPExtCtx);
      if(gfpxBit(pB, bit))
         gfpxMontMul(pA, tmp, tmp, pGFPExtCtx);
   }
   gfpxFFelementCopy(tmp, pR, pGFPExtCtx);
   gfpxReleasePool(1, pGFPExtCtx);
   return pR;
}

cpFFchunk* gfpxMontEnc(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXState* pGFpExtCtx)
{
   Ipp32u i;
   Ipp32u elementGroundSize = GFPX_GROUNDFESIZE(pGFpExtCtx);
   Ipp32u offset=0;
   for (i=0; i<=GFPX_DEGREE(pGFpExtCtx); i++) {
      gfpMontEnc(pA+offset, pR+offset, GFPX_GROUNDGF(pGFpExtCtx));
      offset += elementGroundSize;
   }
   return pR;
}

cpFFchunk* gfpxMontDec(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXState* pGFpExtCtx)
{
   Ipp32u i;
   Ipp32u elementGroundSize = GFPX_GROUNDFESIZE(pGFpExtCtx);
   Ipp32u offset=0;
   for (i=0; i<=GFPX_DEGREE(pGFpExtCtx); i++) {
      gfpMontDec(pA+offset, pR+offset, GFPX_GROUNDGF(pGFpExtCtx));
      offset += elementGroundSize;
   }
   return pR;
}
