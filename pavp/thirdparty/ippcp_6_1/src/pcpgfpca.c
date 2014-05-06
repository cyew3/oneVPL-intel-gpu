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
//    GF(p) definitinons of internal functions
//
//    Context:
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpbn.h"
#include "pcpbnuext.h"
#include "pcpmontgomery.h"
#include "pcpgfp.h"


IppsBigNumState* gfpInitBigNum(IppsBigNumState* pBN, Ipp32u size, Ipp32u* pNumBuffer, Ipp32u* pTmpBuffer)
{
   BN_ID(pBN)     = idCtxBigNum;
   BN_SIGN(pBN)   = IppsBigNumPOS;
   BN_NUMBER(pBN) = pNumBuffer;
   BN_BUFFER(pBN) = pTmpBuffer;
   BN_ROOM(pBN)   = size;
   BN_SIZE(pBN)   = 0;
   return pBN;
}

IppsBigNumState* gfpSetBigNum(IppsBigNumState* pBN, Ipp32u size, const Ipp32u* pBNU, Ipp32u* pTmpBuffer)
{
   gfpInitBigNum(pBN, size, (Ipp32u*)pBNU, pTmpBuffer);
   BN_SIZE(pBN)   = size;
   return pBN;
}

Ipp32u gfpIsArrayOverGF(const Ipp32u* pA, Ipp32u lenA, const IppsGFPState* pGF)
{
   Ipp32u elementSize32 = GFP_FESIZE32(pGF);
   cpFFchunk* pModulus = GFP_MODULUS(pGF);
   Ipp32u isValid = 1;
   while(lenA>=elementSize32 && isValid) {
      isValid = (0 > gfpFFelementCmp(pA, pModulus, elementSize32));
      pA += elementSize32;
      lenA -= elementSize32;
   }
   return isValid;
}

cpFFchunk* gfpSetPolynomial(const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pPoly, Ipp32u degree, const IppsGFPState* pGF)
{
   Ipp32u elementSize = GFP_FESIZE(pGF);
   Ipp32u elementSize32 = GFP_FESIZE32(pGF);

   cpFFchunk* pTmpPoly = pPoly;
   Ipp32u d;

   /* clear polynomial */
   gfpFFelementPadd(0, pTmpPoly, elementSize*(degree+1));

   for(d=0; d<=degree && (lenA>=elementSize32); d++) {
      gfpFFelementCopyPadd(pA, elementSize32, pTmpPoly, elementSize);
      pTmpPoly += elementSize;
      pA += elementSize32;
      lenA -= elementSize32;
   }
   if(d<=degree && lenA)
      gfpFFelementCopyPadd(pA, lenA, pTmpPoly, elementSize);

   return pPoly;
}

Ipp32u* gfpGetPolynomial(const cpFFchunk* pPoly, Ipp32u degree, Ipp32u* pA, Ipp32u lenA, const IppsGFPState* pGF)
{
   Ipp32u elementSize = GFP_FESIZE(pGF);
   Ipp32u elementSize32 = GFP_FESIZE32(pGF);

   Ipp32u* pTmpA = pA;
   Ipp32u d;

   /* clear user memory */
   gfpFFelementPadd(0, pA, lenA);

   for(d=0; d<=degree && (lenA>=elementSize32); d++) {
      gfpFFelementCopy(pPoly, pTmpA, elementSize32);
      pPoly += elementSize;
      pTmpA += elementSize32;
      lenA -= elementSize32;
   }
   if(d<=degree && lenA)
      gfpFFelementCopy(pPoly, pTmpA, lenA);

   return pA;
}

cpFFchunk* gfpReduce(const Ipp32u* pA, Ipp32u lenA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   int rSize;
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   gfpFFelementCopyPadd(pA, lenA, GFP_POOL(pGFpCtx), elementSize);

   cpMod_BNU(GFP_POOL(pGFpCtx), lenA, GFP_MODULUS(pGFpCtx), elementSize, &rSize);
   return gfpFFelementCopyPadd(GFP_POOL(pGFpCtx),rSize, pR,elementSize);
}

cpFFchunk* gfpNeg(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   if( Tst_BNU(pA, elementSize) ) {
      Ipp32u borrow;
      cpSub_BNU(GFP_MODULUS(pGFpCtx), pA, pR, elementSize, &borrow);
   }
   else
      gfpFFelementPadd(0, pR, elementSize);
   return pR;
}

cpFFchunk* gfpInv(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   Ipp32u elementSize32 = GFP_FESIZE32(pGFpCtx);
   Ipp32u poolElementSize = GFP_PESIZE(pGFpCtx);
   Ipp32u rSize;

   cpFFchunk* tmpM = gfpGetPool(4, pGFpCtx);
   cpFFchunk* tmpX1= tmpM +poolElementSize;
   cpFFchunk* tmpX2= tmpX1+poolElementSize;
   cpFFchunk* tmpX3= tmpX2+poolElementSize;

   /* copy Modulus */
   gfpFFelementCopy(GFP_MODULUS(pGFpCtx), tmpM, elementSize);

   cpModInv_BNU(pA,elementSize32, tmpM, elementSize32, pR,(int*)&rSize, tmpX1,tmpX2,tmpX3);
   gfpReleasePool(4, pGFpCtx);

   if(elementSize > rSize)
      gfpFFelementPadd(0, pR+rSize, (elementSize-rSize));
   return pR;
}

cpFFchunk* gfpHalve(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   if(GFP_IS_EVEN(pA)) {
      LSR_BNU(pA, pR, elementSize, 1);
   }
   else {
      Ipp32u carry;
      cpAdd_BNU(pA, GFP_UNITY(pGFpCtx), pR, elementSize, &carry);
      LSR_BNU(pR, pR, elementSize, 1);
      cpAdd_BNU(pR, GFP_HMODULUS(pGFpCtx), pR, elementSize, &carry);
   }
   return pR;
}

#if 0
cpFFchunk* gfpMontInv(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   Ipp32u poolElementSize = GFP_PESIZE(pGFpCtx);
   Ipp32u rSize;

   cpFFchunk* tmpM = gfpGetPool(4, pGFpCtx);
   cpFFchunk* tmpX1= tmpM +poolElementSize;
   cpFFchunk* tmpX2= tmpX1+poolElementSize;
   cpFFchunk* tmpX3= tmpX2+poolElementSize;

   /* copy Modulus */
   gfpFFelementCopy(GFP_MODULUS(pGFpCtx), tmpM, elementSize);

   cpModInv_BNU(pA,elementSize, tmpM, elementSize, pR,(int*)&rSize, tmpX1,tmpX2,tmpX3);
   gfpReleasePool(4, pGFpCtx);

   if(elementSize > rSize)
      gfpFFelementPadd(0, pR+rSize, (elementSize-rSize));
   return gfpMontMul(pR, BN_NUMBER(MNT_CUBE_R(GFP_MONT(pGFpCtx))), pR, pGFpCtx);
}
#endif
cpFFchunk* gfpMontInv(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   gfpInv(pA, pR, pGFpCtx);
   return gfpMontMul(pR, BN_NUMBER(MNT_CUBE_R(GFP_MONT(pGFpCtx))), pR, pGFpCtx);
}


cpFFchunk* gfpAdd(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   Ipp32u carry;
   cpAdd_BNU(pA, pB, pR, elementSize, &carry);
   if(carry || cpCmp_BNU(pR, GFP_MODULUS(pGFpCtx), elementSize) >= 0)
      cpSub_BNU(pR, GFP_MODULUS(pGFpCtx), pR, elementSize, &carry);
   return pR;
}

cpFFchunk* gfpSub(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   Ipp32u borrow = 0;
   cpSub_BNU(pA, pB, pR, elementSize, &borrow);
   if(borrow)
      cpAdd_BNU(pR, GFP_MODULUS(pGFpCtx), pR, elementSize, &borrow);
   return pR;
}

cpFFchunk* gfpMul(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   cpFFchunk* t = gfpGetPool(1, pGFpCtx);
   gfpMontEnc(pA, t, pGFpCtx);
   gfpMontMul(t, pB, t, pGFpCtx);
   gfpFFelementCopy(t, pR, GFP_FESIZE(pGFpCtx));
   gfpReleasePool(1, pGFpCtx);
   return pR;
}

cpFFchunk* gfpExp(const cpFFchunk* pA, const cpFFchunk* pE, Ipp32u eSize, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   gfpMontEnc(pA, pR, pGFpCtx);
   gfpMontExp(pR, pE, eSize, pR, pGFpCtx);
   return gfpMontDec(pR, pR, pGFpCtx);
}

cpFFchunk* gfpExp2(const cpFFchunk* pA, Ipp32u e, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   gfpMontEnc(pA, pR, pGFpCtx);
   gfpMontExp2(pR, e, pR, pGFpCtx);
   return gfpMontDec(pR, pR, pGFpCtx);
}


cpFFchunk* gfpMontEnc(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   return gfpMontMul(pA, BN_NUMBER(MNT_SQUARE_R(GFP_MONT(pGFpCtx))), pR, pGFpCtx);
}

cpFFchunk* gfpMontDec(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   return gfpMontMul(pA, GFP_UNITY(pGFpCtx), pR, pGFpCtx);
}

cpFFchunk* gfpMontMul(const cpFFchunk* pA, const cpFFchunk* pB, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   Ipp32u elementSize32 = GFP_FESIZE32(pGFpCtx);
   cpFFchunk* pPool = GFP_POOL(pGFpCtx);
   Ipp32u rSize;

   if((pA!=pR) && (pB!=pR)) {
      cpMontMul(pA, gfpElementActualSize(pA, elementSize32),
                pB, gfpElementActualSize(pB, elementSize32),
                GFP_MODULUS(pGFpCtx), elementSize32,
                pR, (int*)&rSize,
                MNT_HELPER(GFP_MONT(pGFpCtx)), pPool, NULL);
      gfpFFelementPadd(0, pR+rSize, (elementSize-rSize));
      return pR;
   }
   else {
      cpMontMul(pA, gfpElementActualSize(pA, elementSize32),
                pB, gfpElementActualSize(pB, elementSize32),
                GFP_MODULUS(pGFpCtx), elementSize32,
                pPool, (int*)&rSize,
                MNT_HELPER(GFP_MONT(pGFpCtx)), pPool+GFP_PESIZE(pGFpCtx), NULL);
      return gfpFFelementCopyPadd(pPool,rSize, pR,elementSize);
   }
}

cpFFchunk* gfpMontExp(const cpFFchunk* pA, const cpFFchunk* pE, Ipp32u eSize, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   IppsBigNumState A;
   IppsBigNumState E;
   IppsBigNumState R;

   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   cpFFchunk* pPool = GFP_POOL(pGFpCtx);
   Ipp32u poolElementSize = GFP_PESIZE(pGFpCtx);

   gfpSetBigNum(&A, gfpElementActualSize(pA, elementSize), pA, pPool+0*poolElementSize);
   gfpSetBigNum(&E, gfpElementActualSize(pE, eSize), pE, pPool+1*poolElementSize);
   gfpInitBigNum(&R,elementSize, pR, pPool+2*poolElementSize);

   cpMontExp_Binary(&R, &A, &E, GFP_MONT(pGFpCtx));
   if(elementSize > (Ipp32u)BN_SIZE(&R))
      gfpFFelementPadd(0, pR+BN_SIZE(&R), (elementSize-BN_SIZE(&R)));
   return pR;
}

cpFFchunk* gfpMontExp2(const cpFFchunk* pA, Ipp32u e, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   gfpFFelementCopy(pA, pR, GFP_FESIZE(pGFpCtx));
   while(e--) {
      gfpMontMul(pR, pR, pR, pGFpCtx);
   }
   return pR;
}

static Ipp32u factor2(cpFFchunk* pA, Ipp32u size)
{
   Ipp32u factor = 0;
   Ipp32u bits;
   Ipp32u i;
   for(i=0; i<size; i++) {
      Ipp32u ntz = NTZ32u(pA[i]);
      factor += ntz;
      if(ntz<32)
         break;
   }

   bits = factor;
   if(bits >= 32) {
      Ipp32u nw = bits/32;
      gfpFFelementCopyPadd(pA+nw, size-nw, pA, size);
      bits %= 32;
   }
   if(bits)
      LSR_BNU(pA, pA, size, bits);

   return factor;
}

/* returns:
   0, if a - qnr
   1, if sqrt is found
*/
int gfpSqrt(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   gfpMontEnc(pA, pR, pGFpCtx);

   if( gfpMontSqrt(pR, pR, pGFpCtx) ) {
      gfpMontDec(pR, pR, pGFpCtx);
      return 1;
   }
   else
      return 0;
}

int gfpMontSqrt(const cpFFchunk* pA, cpFFchunk* pR, IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   int resultFlag = 1;

   /* case A==0 */
   if( 0==Tst_BNU(pA, elementSize) )
      gfpFFelementPadd(0, pR, elementSize);

   /* general case */
   else {
      cpFFchunk* q = gfpGetPool(1, pGFpCtx);
      cpFFchunk* x = gfpGetPool(1, pGFpCtx);
      cpFFchunk* y = gfpGetPool(1, pGFpCtx);
      cpFFchunk* z = gfpGetPool(1, pGFpCtx);

      Ipp32u s;


      /* (modulus-1) = 2^s*q */
      gfpSub(GFP_MODULUS(pGFpCtx), GFP_UNITY(pGFpCtx), q, pGFpCtx);
      s = factor2(q, elementSize);

      /*
      // initialization
      */

      /* y = qnr^q */
      gfpMontExp(GFP_QNR(pGFpCtx), q,elementSize, y, pGFpCtx);
      /* x = a^((q-1)/2) */
      gfpSub(q, GFP_UNITY(pGFpCtx), q, pGFpCtx);
      LSR_BNU(q, q, elementSize, 1);
      gfpMontExp(pA, q,elementSize, x, pGFpCtx);
      /* z = a*x^2 */
      gfpMontMul(x, x, z, pGFpCtx);
      gfpMontMul(pA, z, z, pGFpCtx);
      /* R = a*x */
      gfpMontMul(pA, x, pR, pGFpCtx);

      while( !GFP_EQ(z, GFP_MONTUNITY(pGFpCtx), elementSize) ) {
         Ipp32u m = 0;
         gfpFFelementCopy(z, q, elementSize);

         for(m=1; m<s; m++) {
            gfpMontMul(q, q, q, pGFpCtx);
            if( GFP_EQ(q, GFP_MONTUNITY(pGFpCtx), elementSize) )
               break;
         }

         if(m==s) {
            /* A is quadratic non-residue */
            resultFlag = 0;
            break;
         }
         else {
            /* exponent reduction */
            gfpMontExp2(y, (Ipp32u)(s-m-1), q, pGFpCtx); /* q = y^(2^(s-m-1)) */
            gfpMontMul(q, q, y, pGFpCtx);                /* y = q^2 */
            gfpMontMul(pR, q, pR, pGFpCtx);              /* R = q*R */
            gfpMontMul(z, y, z, pGFpCtx);                /* z = z*y */
            s = m;
         }
      }

      gfpReleasePool(4, pGFpCtx);
   }

   return resultFlag;
}


cpFFchunk* gfpRand(cpFFchunk* pR, IppsGFPState* pGFpCtx, IppBitSupplier rndFunc, void* pRndParam)
{
   Ipp32u reqBitSize = GFP_FEBITSIZE(pGFpCtx)+GF_RAND_ADD_BITS;
   Ipp32u reqSize = BITS2WORD32_SIZE(reqBitSize);

   Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
   Ipp32u elementSize32 = GFP_FESIZE32(pGFpCtx);
   cpFFchunk* pPool = GFP_POOL(pGFpCtx);
   Ipp32u rSize;

   rndFunc(pPool, reqBitSize, pRndParam);
   cpMod_BNU(pPool, reqSize, GFP_MODULUS(pGFpCtx), elementSize32, (int*)&rSize);
   return gfpFFelementCopyPadd(GFP_POOL(pGFpCtx),rSize, pR,elementSize);
}
