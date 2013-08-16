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
//    Quadratic Extension of GF(p^d) Internal Functions
//
//    Content:
//      gfpxqxCopyMod
//      gfpxqxReduce
//      gfpxqxFFelementIsEqu
//      gfpxqxAdd
//      gfpxqxSub
//      gfpxqxNeg
//      gfpxqxMontMul_GFP
//      gfpxqxMontMul
//      gfpxqxMontExp
//      gfpxqxMontInv
//      gfpxqxMontEnc
//      gfpxqxMontDec
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpgfp.h"
#include "pcpgfpext.h"
#include "pcpbnuext.h"
#include "pcpgfpqext.h"


Ipp32u gfpxqxIsUserModulusValid(const Ipp32u* pA, Ipp32u lenA, const IppsGFPXQState* pGFPQExtCtx)
{
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);
   IppsGFPState* pGF = GFPX_GROUNDGF(pGFX);
   Ipp32u usrCoeffSize = GFP_FESIZE32(pGF) * (GFPX_DEGREE(pGFX)+1);
   /* expected modulus degree */
   Ipp32u degree = GFPXQX_DEGREE(pGFPQExtCtx) +1;

   /* check if poly's coeffs belongs to GF(p) */
   Ipp32u isValid = gfpIsArrayOverGF(pA, lenA, pGF);

   if(isValid)
      /* check if x doesn't divide the poly */
      isValid = GFP_IS_ZERO(pA, usrCoeffSize)? 0:1;

   if(isValid)
      /* check if the poly is monic */
      isValid = GFP_IS_ONE(GFPXQX_IDX_ELEMENT(pA,degree,usrCoeffSize), 1)? 1:0;

   return isValid;
}

#if 0
cpFFchunk* gfpxqxCopyMod(const cpFFchunk* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   unsigned int i;
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);

   for (i=0; i<=GFPXQX_DEGREE(pGFPQExtCtx); i++) {
      if (pA && lenA>0) {
         gfpxReduce(GFPXQX_IDX_ELEMENT(pA,i,elementSize), lenA>elementSize? elementSize:lenA, GFPXQX_IDX_ELEMENT(pR,i,elementSize), pGFX);
         if (lenA < elementSize) lenA = 0;
         else lenA -= elementSize;
      }
      else
         GFPX_ZERO(GFPXQX_IDX_ELEMENT(pR,i,elementSize),pGFX);
   }
   return pR;
}
#endif

#if 0
cpFFchunk* gfpxqxReduce(const cpFFchunk* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
#if 0
   //int i;
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);
   cpFFchunk* pModulus = GFPXQX_MODULUS(pGFPQExtCtx);
   cpFFchunk *t, *tmpres, *t1;
   int argDegree = ((int)lenA-1)/elementSize;
   int degree = GFPXQX_DEGREE(pGFPQExtCtx)+1;
   if (pA==NULL || lenA==0)
      return pR; /* No initialization required */
   if (argDegree < degree) {
      gfpxqxCopyMod(pA, lenA, pR, pGFPQExtCtx); /* Copy with gfpx reducing */
      return pR;
   }
   tmpres = gfpxqxGetPool(1, pGFPQExtCtx);
   if (GFPX_IS_ONE(GFPX_IDX_ELEMENT(pA, 2, elementSize), pGFX)) {
      gfpxqxMontEnc(pA, tmpres, pGFPQExtCtx);
      gfpxqxSub(tmpres, pModulus, tmpres, pGFPQExtCtx);
      gfpxqxMontDec(tmpres, pR, pGFPQExtCtx);
      gfpxqxReleasePool(1, pGFPQExtCtx);
      return pR;
   }
   t = gfpxGetPool(2, pGFX);
   t1 = t + elementSize;
   gfpxqxCopyMod(pA, elementSize*2, tmpres, pGFPQExtCtx);
   gfpxReduce(GFPX_IDX_ELEMENT(pA, 2, elementSize),elementSize,t,pGFX); /* arg[2] */

   gfpxMontMul(t, GFPX_IDX_ELEMENT(pModulus, 1, elementSize), t1, pGFX);
   gfpxSub(GFPX_IDX_ELEMENT(tmpres, 1, elementSize), t1, GFPX_IDX_ELEMENT(pR, 1, elementSize), pGFX);

   gfpxMontMul(GFPX_IDX_ELEMENT(tmpres, 2, elementSize), pModulus, t1, pGFX);
   gfpxSub(tmpres, t1, pR, pGFX);
   gfpxReleasePool(2, pGFX);
   gfpxqxReleasePool(1, pGFPQExtCtx);
   return pR;
#else
   int i;
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);
   cpFFchunk* pModulus = GFPXQX_MODULUS(pGFPQExtCtx);
   cpFFchunk *t, *tnew, *tmpres, *tmp;
   int argDegree;
   int degree = GFPXQX_DEGREE(pGFPQExtCtx)+1;
   if (pA==NULL || lenA==0 )
      return pR;
   argDegree = (gfpElementActualSize(pA, lenA)-1)/elementSize;
   if (argDegree < degree) {
      gfpxqxCopyMod(pA, lenA, pR, pGFPQExtCtx);
      return pR;
   }
   tmpres = gfpxqxGetPool(1, pGFPQExtCtx);
   gfpxqxCopyMod(GFPX_IDX_ELEMENT(pA, argDegree-degree, elementSize), GFPXQX_FESIZE(pGFPQExtCtx), tmpres, pGFPQExtCtx);
   t = gfpxGetPool(3, pGFX);
   tnew = t+elementSize;
   tmp = tnew+elementSize;
   gfpxReduce(GFPX_IDX_ELEMENT(pA, argDegree, elementSize), lenA-argDegree*elementSize, t, pGFX);   
   while(argDegree >= degree) {
      gfpxMontMul(t, GFPX_IDX_ELEMENT(pModulus, degree-1, elementSize), tmp, pGFX);
      gfpxSub(GFPX_IDX_ELEMENT(tmpres, degree-1, elementSize), tmp, tnew, pGFX);
      for (i=degree-2; i>=0; i--) {
         gfpxMontMul(t, GFPX_IDX_ELEMENT(pModulus, i, elementSize), tmp, pGFX);
         gfpxSub(GFPX_IDX_ELEMENT(tmpres, i, elementSize), tmp, GFPX_IDX_ELEMENT(tmpres, i+1, elementSize), pGFX);
      }
      if (argDegree <= degree)
         break;
      gfpFFelementCopy(GFPX_IDX_ELEMENT(pA, argDegree-degree-1, elementSize), tmpres, elementSize);
      gfpFFelementCopy(tnew, t, elementSize);
      argDegree--;
   }
   for (i=0; i<degree-1; i++)
      gfpFFelementCopy(GFPX_IDX_ELEMENT(tmpres, i+1, elementSize), GFPX_IDX_ELEMENT(pR, i, elementSize), elementSize);
   gfpFFelementCopy(tnew, GFPX_IDX_ELEMENT(pR, degree-1, elementSize), elementSize);
   gfpxqxReleasePool(1, pGFPQExtCtx);
   gfpxReleasePool(3, pGFX);
   return pR;
#endif
}
#endif
cpFFchunk* gfpxqxReduce(const cpFFchunk* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);
   IppsGFPState* pGF = GFPX_GROUNDGF(pGFX);
   Ipp32u usrCoeffSize = GFP_FESIZE32(pGF) * (GFPX_DEGREE(pGFX)+1);
   Ipp32u ctxCoeffSize = GFP_FESIZE(pGF) * (GFPX_DEGREE(pGFX)+1);

   /* degree of input polynomial */
   Ipp32u degA = (lenA+usrCoeffSize-1)/usrCoeffSize -1;
   /* degree of modulus */
   Ipp32u degM = GFPXQX_DEGREE(pGFPQExtCtx)+1;

   if(degA<degM)
      gfpSetPolynomial(pA, lenA, pR, (GFPX_DEGREE(pGFX)+1)*(GFPXQX_DEGREE(pGFPQExtCtx)+1)-1, pGF);

   else {
      Ipp32u degE = GFPXQX_DEGREE(pGFPQExtCtx); /* element degree */
      cpFFchunk* pModulus = GFPXQX_MODULUS(pGFPQExtCtx);
      cpFFchunk* pQ = gfpxGetPool(1, pGFX);
      cpFFchunk* pT = gfpxGetPool(1, pGFX);

      /* copy upper part of the input polynomial */
      pA = GFPXQX_IDX_ELEMENT(pA, degA-degM, usrCoeffSize);
      gfpSetPolynomial(pA+usrCoeffSize, lenA-(degA-degM+1)*usrCoeffSize, pR, (GFPX_DEGREE(pGFX)+1)*(GFPXQX_DEGREE(pGFPQExtCtx)+1)-1, pGF);

      while( degA>=degM ) {

         /* q = R[degE] */
         gfpxFFelementCopy(GFPXQX_IDX_ELEMENT(pR, degE, ctxCoeffSize), pQ, pGFX);

         /* R[i] = R[i-1] - q*M[i] */
         gfpxMontMul(pQ, GFPXQX_IDX_ELEMENT(pModulus, 1, ctxCoeffSize), pT, pGFX);
         gfpxSub(GFPXQX_IDX_ELEMENT(pR, 0, ctxCoeffSize), pT, GFPXQX_IDX_ELEMENT(pR, 1, ctxCoeffSize), pGFX);

         gfpSetPolynomial(pA, usrCoeffSize, pR, GFPX_DEGREE(pGFX), pGF);
         gfpxMontMul(pQ, pModulus, pT, pGFX);
         gfpxSub(pR, pT, pR, pGFX);

         degA--;
         pA -= usrCoeffSize;
      }

      gfpxReleasePool(2, pGFX);
   }

   return pR;
}


cpFFchunk* gfpxqxAdd(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   Ipp32u i;

   for (i=0; i<=GFPXQX_DEGREE(pGFPQExtCtx); i++)
      gfpxAdd(GFPXQX_IDX_ELEMENT(pA,i,elementSize), GFPXQX_IDX_ELEMENT(pB,i,elementSize),
              GFPXQX_IDX_ELEMENT(pR,i,elementSize), GFPXQX_GROUNDGF(pGFPQExtCtx));
   return pR;
}

cpFFchunk* gfpxqxSub(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   Ipp32u i;

   for(i=0; i<=GFPXQX_DEGREE(pGFPQExtCtx); i++)
      gfpxSub(GFPXQX_IDX_ELEMENT(pA, i, elementSize), GFPXQX_IDX_ELEMENT(pB, i, elementSize),
              GFPXQX_IDX_ELEMENT(pR, i, elementSize), GFPXQX_GROUNDGF(pGFPQExtCtx));
   return pR;
}

cpFFchunk* gfpxqxNeg(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   Ipp32u i;

   for(i=0; i<=GFPXQX_DEGREE(pGFPQExtCtx); i++)
      gfpxNeg(GFPXQX_IDX_ELEMENT(pA, i, elementSize), GFPXQX_IDX_ELEMENT(pR, i, elementSize),
              GFPXQX_GROUNDGF(pGFPQExtCtx));

   return pR;
}

cpFFchunk* gfpxqxMontMul_GFP(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);

   gfpxMontMul_GFP(pA, pB, pR, pGFX);
   gfpxMontMul_GFP(pA+elementSize, pB, pR+elementSize, pGFX);
   return pR;
}

cpFFchunk* gfpxqxFFelementCopy(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXQState* pGFPQExtCtx)
{
   gfpFFelementCopy(pA, pR, GFPXQX_FESIZE(pGFPQExtCtx));
   return pR;
}

cpFFchunk* gfpxqxMontMul(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   /* SafeID's version. I.e. degree=2 and polynomial is x^2+0*x-qnr */
   /* We need a GFPXQX own pool of 3 variables */
   Ipp32u egSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);
   cpFFchunk* pMod = GFPXQX_MODULUS(pGFPQExtCtx);

   cpFFchunk* t0 = gfpxqxGetPool(2, pGFPQExtCtx);      /* We'll use our own pool */
   cpFFchunk* t1 = t0 + egSize;
   cpFFchunk* t2 = t1 + egSize;

   gfpxMontMul(pA+egSize, pB+egSize, t0, pGFX);    /* t0 <- a[1]*b[1] */
   gfpxMontMul(pA+egSize, pB, t1, pGFX);           /* t1 <- a[1]*b[0] */
   gfpxMontMul(pA, pB+egSize, t2, pGFX);           /* t2 <- a[0]*b[1] */
   gfpxAdd(t2, t1, t2, pGFX);                      /* t2 <- t2+t1 */
   gfpxMontMul(t0, pMod+egSize, t1, pGFX);         /* t1 <- t0*q[1] */
   gfpxSub(t2, t1, pR+egSize, pGFX);               /* r[1] <- t2-t1 */
   gfpxMontMul(pA, pB, t1, pGFX);                  /* t1 <- a[0]*b[0] */
   gfpxMontMul(t0, pMod, t2, pGFX);                /* t2 <- t0*q[0] */
   gfpxSub(t1, t2, pR, pGFX);                      /* r[0] <- t1-t2 */
   gfpxqxReleasePool(2, pGFPQExtCtx);
   return pR;
}

cpFFchunk* gfpxqxMontExp(const cpFFchunk* pA, const cpFFchunk* pB, Ipp32u bSize, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   int bit;
   cpFFchunk* tmp = gfpxqxGetPool(1, pGFPQExtCtx);
   GFPXQX_SET_MONT_ONE(tmp, pGFPQExtCtx);
   for(bit=(int)MSB_BNU(pB, bSize); bit>=0; bit--) {
      gfpxqxMontMul(tmp, tmp, tmp, pGFPQExtCtx);
      if(gfpxBit(pB, bit))
         gfpxqxMontMul(pA, tmp, tmp, pGFPQExtCtx);
   }
   gfpxqxFFelementCopy(tmp, pR, pGFPQExtCtx);
   gfpxqxReleasePool(1, pGFPQExtCtx);
   return pR;
}

cpFFchunk* gfpxqxMontInv(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx)
{
   Ipp32u egSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   IppsGFPXState* pGFX = GFPXQX_GROUNDGF(pGFPQExtCtx);
   cpFFchunk *t, *t1, *t2, *t3, *t4;
   cpFFchunk *pMod = GFPXQX_MODULUS(pGFPQExtCtx);

   t = gfpxqxGetPool(3, pGFPQExtCtx);
   t1 = t + egSize;
   t2 = t1 + egSize;
   t3 = t2 + egSize;
   t4 = t3 + egSize;

   gfpxMontMul(pA+egSize, pMod+egSize, t1, pGFX);  /* t1=a[1]*mod[1] */
   gfpxSub(t1, pA, t2, pGFX);                      /* t2 = t1-a[0] */
   gfpxMontMul(pA, t2, t3, pGFX);                  /* t3 = t2 * a[0] */
   gfpxMontMul(pA+egSize, pA+egSize, t4, pGFX);    /* t4 = a[1]*a[1] */
   gfpxMontMul(t4, pMod, t4, pGFX);                /* t3 = t3 * mod[0] */
   gfpxSub(t3, t4, t, pGFX);                       /* t = t2 - t3 */
   gfpxMontInv(t, t, pGFX);                        /* t = inv(t) */
   gfpxMontMul(pA+egSize, t, pR+egSize, pGFX);     /* r[1] = a[1] * t */
   gfpxMontMul(t2, t, pR, pGFX);                   /* r[0] = t2 * t */
   gfpxqxReleasePool(3, pGFPQExtCtx);
   return pR;
}

cpFFchunk* gfpxqxMontEnc(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXQState* pGFpQExtCtx)
{
   Ipp32u i;
   Ipp32u elementGroundSize = GFPXQX_GROUNDFESIZE(pGFpQExtCtx);
   for (i=0; i<=GFPXQX_DEGREE(pGFpQExtCtx); i++) {
      gfpxMontEnc(GFPXQX_IDX_ELEMENT(pA,i,elementGroundSize), GFPXQX_IDX_ELEMENT(pR,i,elementGroundSize),
                  GFPXQX_GROUNDGF(pGFpQExtCtx));
   }
   return pR;
}

cpFFchunk* gfpxqxMontDec(const cpFFchunk* pA, cpFFchunk* pR, const IppsGFPXQState* pGFpQExtCtx)
{
   Ipp32u i;
   Ipp32u elementGroundSize = GFPXQX_GROUNDFESIZE(pGFpQExtCtx);
   for (i=0; i<=GFPXQX_DEGREE(pGFpQExtCtx); i++) {
      gfpxMontDec(GFPXQX_IDX_ELEMENT(pA,i,elementGroundSize), GFPXQX_IDX_ELEMENT(pR,i,elementGroundSize),
                  GFPXQX_GROUNDGF(pGFpQExtCtx));
   }
   return pR;
}

cpFFchunk* gfpxqxRand(cpFFchunk* pR, IppsGFPXQState* pGFPQExtCtx, IppBitSupplier rndFunc, void* pRndParam)
{
   Ipp32u elementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
   Ipp32u i;
   Ipp32u offset=0;

   for(i=0; i<=GFPXQX_DEGREE(pGFPQExtCtx); i++) {
      gfpxRand(pR+offset, GFPXQX_GROUNDGF(pGFPQExtCtx), rndFunc, pRndParam);
      offset += elementSize;
   }
   return pR;
}

