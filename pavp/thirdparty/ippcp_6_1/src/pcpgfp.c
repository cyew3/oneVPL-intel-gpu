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
//    GF(p) definitinons
//
//    Context:
//       ippsGFPGetSize()
//       ippsGFPInit()
//       ippsGFPGet()
//
//       ippsGFPElementGetSize()
//       ippsGFPElementInit()
//       ippsGFPRefElement()      -removed
//
//       ippsGFPSetElement()
//       ippsGFPSetElementZero()
//       ippsGFPSetElementPower2()
//       ippsGFPSetElementRandom()
//       ippsGFPGetElement()
//       ippsGFPCmpElement()
//       ippsGFPCpyElement()
//
//       ippsGFPNeg()
//       ippsGFPInv()
//       ippsGFPSqrt()
//       ippsGFPAdd()
//       ippsGFPSub()
//       ippsGFPMul()
//       ippsGFPExp()
//
//       ippsGFPMontEncode()
//       ippsGFPMontDecode()
//       ippsGFPMontMul()    - removed
//       ippsGFPMontExp()    - removed
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpbnuext.h"
#include "pcpbn.h"
#include "pcpmontgomery.h"
#include "pcpgfp.h"


#define GFP_STRUCT_SIZE       (IPP_MULTIPLE_OF(sizeof(IppsGFPState), 16))


IPPFUN(IppStatus, ippsGFPGetSize,(Ipp32u bitSize, Ipp32u* pSizeInBytes))
{
   IPP_BAD_PTR1_RET(pSizeInBytes);
   IPP_BADARG_RET((bitSize < 2) || (bitSize > FF_MAX_BITSIZE), ippStsSizeErr);

   {
      Ipp32u elementSize32 = BITS2WORD32_SIZE(bitSize);
      Ipp32u elementSize = IPP_MULTIPLE_OF(elementSize32, BNUBASE_TYPE_SIZE);
      Ipp32u poolElementSize = elementSize + BNUBASE_TYPE_SIZE;
      int montgomeryCtxSize;
      ippsMontGetSize(IppsBinaryMethod, elementSize32, &montgomeryCtxSize);

      *pSizeInBytes = GFP_STRUCT_SIZE /*sizeof(IppsGFPState)*/
                     +elementSize*sizeof(Ipp32u) /* modulus */
                     +elementSize*sizeof(Ipp32u) /* half of modulus */
                     +elementSize*sizeof(Ipp32u) /* udentity */
                     +elementSize*sizeof(Ipp32u) /* mont udentity */
                     +elementSize*sizeof(Ipp32u) /* random quadratic non-residue */
                     +montgomeryCtxSize
                     +poolElementSize*sizeof(Ipp32u)*GF_POOL_SIZE;
      return ippStsNoErr;
   }
}


static void gfpInitSqrt(IppsGFPState* pGFpCtx)
{
   Ipp32u elementSize = GFP_FESIZE32(pGFpCtx);
   cpFFchunk* e = gfpGetPool(1, pGFpCtx);
   cpFFchunk* t = gfpGetPool(1, pGFpCtx);

   /* (modulus-1)/2 */
   LSR_BNU(GFP_MODULUS(pGFpCtx), e, elementSize, 1);

   /* find a non-square g, where g^{(modulus-1)/2} = -1 */
   gfpFFelementCopy(GFP_MONTUNITY(pGFpCtx), GFP_QNR(pGFpCtx), elementSize);
   do {
      gfpAdd(GFP_MONTUNITY(pGFpCtx), GFP_QNR(pGFpCtx), GFP_QNR(pGFpCtx), pGFpCtx);
      gfpMontExp(GFP_QNR(pGFpCtx), e, elementSize, t, pGFpCtx);
      gfpNeg(t, t, pGFpCtx);
   } while( !GFP_EQ(GFP_MONTUNITY(pGFpCtx), t, elementSize) );

   gfpReleasePool(2, pGFpCtx);
}

IPPFUN(IppStatus, ippsGFPInit,(IppsGFPState* pGFpCtx, const Ipp32u* pPrime, Ipp32u bitSize))
{
   IPP_BAD_PTR2_RET(pPrime, pGFpCtx);
   IPP_BADARG_RET((bitSize < 2 ) || (bitSize > FF_MAX_BITSIZE), ippStsSizeErr);

   {
      Ipp8u* ptr = (Ipp8u*)pGFpCtx;

      Ipp32u elementSize32 = BITS2WORD32_SIZE(bitSize);
      Ipp32u elementSize = IPP_MULTIPLE_OF(elementSize32, BNUBASE_TYPE_SIZE);
      Ipp32u poolElementSize = elementSize + BNUBASE_TYPE_SIZE;
      int montgomeryCtxSize;
      ippsMontGetSize(IppsBinaryMethod, elementSize32, &montgomeryCtxSize);

      GFP_ID(pGFpCtx) = idCtxGFP;
      GFP_DEGREE(pGFpCtx) = 0;
      GFP_FESIZE(pGFpCtx) = elementSize;
      GFP_FESIZE32(pGFpCtx) = elementSize32;
      GFP_GROUNDGF(pGFpCtx) = 0;

      ptr += GFP_STRUCT_SIZE; /*sizeof(IppsGFPState);*/
      GFP_MODULUS(pGFpCtx)  = (cpFFchunk*)(ptr);      ptr += elementSize*sizeof(Ipp32u);
      GFP_HMODULUS(pGFpCtx) = (cpFFchunk*)(ptr);      ptr += elementSize*sizeof(Ipp32u);
      GFP_UNITY(pGFpCtx)    = (cpFFchunk*)(ptr);      ptr += elementSize*sizeof(Ipp32u);
      GFP_MONTUNITY(pGFpCtx)= (cpFFchunk*)(ptr);      ptr += elementSize*sizeof(Ipp32u);
      GFP_QNR(pGFpCtx)      = (cpFFchunk*)(ptr);      ptr += elementSize*sizeof(Ipp32u);
      GFP_MONT(pGFpCtx)     = (IppsMontState*)(ptr);  ptr += montgomeryCtxSize;
      GFP_POOL(pGFpCtx)     = (cpFFchunk*)(ptr);      ptr += poolElementSize*sizeof(Ipp32u)*GF_POOL_SIZE;

      IPP_MAKE_ALIGNED_MONT(GFP_MONT(pGFpCtx));
      ippsMontInit(IppsBinaryMethod, elementSize32, GFP_MONT(pGFpCtx));
      ippsMontSet(pPrime, elementSize32, GFP_MONT(pGFpCtx));

      /* modulus */
      gfpFFelementPadd(0, GFP_MODULUS(pGFpCtx), elementSize);
      gfpFFelementCopy(pPrime, GFP_MODULUS(pGFpCtx), elementSize32);
      /* half of modulus */
      gfpFFelementPadd(0, GFP_HMODULUS(pGFpCtx), elementSize);
      LSR_BNU(pPrime, GFP_HMODULUS(pGFpCtx), elementSize32, 1);
      /* unity */
      gfpFFelementPadd(0, GFP_UNITY(pGFpCtx), elementSize);
      GFP_UNITY(pGFpCtx)[0] = 1;
      /* montgomery unity */
      gfpFFelementPadd(0, GFP_MONTUNITY(pGFpCtx), elementSize);
      gfpMontEnc(GFP_UNITY(pGFpCtx), GFP_MONTUNITY(pGFpCtx), pGFpCtx);

      /* do some additional initialization to make sqrt operation faster */
      gfpFFelementPadd(0, GFP_QNR(pGFpCtx), elementSize);
      gfpInitSqrt(pGFpCtx);

      return ippStsNoErr;
   }
}

#if 0
IPPFUN(IppStatus, ippsGFPSetModulus,(const Ipp32u* pModulus, Ipp32u modulusLen, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR1_RET(pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );

   if(pModulus && modulusLen) {
      Ipp32u elementSize = GFP_FESIZE(pGFpCtx);
      if(elementSize>modulusLen) IPP_ERROR_RET(ippStsSizeErr);

      /* setup Montgomery engine */
      ippsMontSet(pModulus, elementSize, GFP_MONT(pGFpCtx));
      /* store modulus value */
      gfpFFelementCopy(pModulus, GFP_MODULUS(pGFpCtx), elementSize);
      /* store modulus/2 */
      LSR_BNU(pModulus, GFP_HMODULUS(pGFpCtx), elementSize, 1);
      /* unity */
      GFP_UNITY(pGFpCtx)[0] = 1;
      /* montgomery unity */
      gfpMontEnc(GFP_UNITY(pGFpCtx), GFP_MONTUNITY(pGFpCtx), pGFpCtx);
      /* pre-computr quadratic non-residue */
      gfpInitSqrt(pGFpCtx);
   }

   return ippStsNoErr;
}
#endif

#if 0
IPPFUN(IppStatus, ippsGFPRefModulus,(const IppsGFPState* pGFpCtx, Ipp32u** const ppModulus))
{
   IPP_BAD_PTR2_RET(ppModulus, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );

   *ppModulus = GFP_MODULUS(pGFpCtx);
   return ippStsNoErr;
}
#endif

#if 0
IPPFUN(IppStatus, ippsGFPElementBitSize, (const IppsGFPState* pGFpCtx, Ipp32u* pBitSize))
{
   IPP_BAD_PTR2_RET(pGFpCtx, pBitSize);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );

   *pBitSize = GFP_FEBITSIZE(pGFpCtx);
   return ippStsNoErr;
}
#endif


IPPFUN(IppStatus, ippsGFPGet,(const IppsGFPState* pGFpCtx, const Ipp32u** ppModulus, Ipp32u* pElementLen))
{
   IPP_BAD_PTR1_RET(pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );

   if(ppModulus)
      *ppModulus = GFP_MODULUS(pGFpCtx);
   if(pElementLen)
      *pElementLen = GFP_FESIZE(pGFpCtx);
   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPElementGetSize,(const IppsGFPState* pGFpCtx, Ipp32u* pElementSize))
{
   IPP_BAD_PTR2_RET(pElementSize, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );

   *pElementSize = sizeof(IppsGFPElement)
                  +GFP_FESIZE(pGFpCtx)*sizeof(cpFFchunk);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPElementInit,(IppsGFPElement* pR, const Ipp32u* pA, Ipp32u lenA, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( lenA>GF_POOL_SIZE*GFP_PESIZE(pGFpCtx), ippStsSizeErr );

   {
      Ipp8u* ptr = (Ipp8u*)pR;

      GFPE_ID(pR) = idCtxGFPE;

      ptr += sizeof(IppsGFPElement);
      GFPE_DATA(pR) = (cpFFchunk*)ptr;

      if(pA && lenA>0)
         gfpReduce(pA, lenA, GFPE_DATA(pR), pGFpCtx);
      else
         gfpFFelementPadd(0, GFPE_DATA(pR), GFP_FESIZE(pGFpCtx));

      return ippStsNoErr;
   }
}


#if 0
IPPFUN(IppStatus, ippsGFPElementRef, (const IppsGFPElement* pA, Ipp32u** const ppDataA))
{
   IPP_BAD_PTR2_RET(pA, ppDataA);
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );

   *ppDataA = GFPE_DATA(pA);
   return ippStsNoErr;
}
#endif


IPPFUN(IppStatus, ippsGFPSetElement,(const Ipp32u* pA, Ipp32u lenA, IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );
   IPP_BADARG_RET( !(0<lenA && lenA<=GF_POOL_SIZE*GFP_PESIZE(pGFpCtx)), ippStsSizeErr );

   gfpReduce(pA, lenA, GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPSetElementZero,(IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpFFelementPadd(0, GFPE_DATA(pR), GFP_FESIZE(pGFpCtx));

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPSetElementPower2,(Ipp32u power, IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );
   IPP_BADARG_RET( (power+1) > GF_POOL_SIZE*GFP_PESIZE(pGFpCtx)*BITSIZE(ipp32u), ippStsBadArgErr);

   {
      Ipp32u moduloBitSize = GFP_FESIZE(pGFpCtx)*BITSIZE(ipp32u) - NLZ32u(GFP_MODULUS(pGFpCtx)[GFP_FESIZE(pGFpCtx)-1]);
      if(moduloBitSize>power) {
         gfpFFelementPadd(0, GFPE_DATA(pR), GFP_FESIZE(pGFpCtx));
         SET_BIT(GFPE_DATA(pR), power+1);
      }
      else {
         Ipp32u dataLen = BITS2WORD32_SIZE(power);
         Ipp32u* pData = GFP_POOL(pGFpCtx);
         gfpFFelementPadd(0, pData, dataLen);
         SET_BIT(pData, power+1);
         gfpReduce(pData, dataLen, GFPE_DATA(pR), pGFpCtx);
      }
      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPSetElementRandom,(IppsGFPElement* pR, IppsGFPState* pGFpCtx,
                                           IppBitSupplier rndFunc, void* pRndParam))
{
   IPP_BAD_PTR2_RET(pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpRand(GFPE_DATA(pR), pGFpCtx, rndFunc, pRndParam);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPGetElement, (const IppsGFPElement* pA, Ipp32u* pDataA, Ipp32u dataLen, const IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pDataA, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !(0<dataLen && dataLen<=GFP_FESIZE(pGFpCtx)), ippStsSizeErr );

   {
      Ipp32u size = IPP_MIN(dataLen, GFP_FESIZE(pGFpCtx));
      gfpFFelementCopyPadd(GFPE_DATA(pA), size, pDataA, dataLen);

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPCmpElement,(const IppsGFPElement* pA, const IppsGFPElement* pB,
                                     IppsElementCmpResult* pResult,
                                     const IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pB, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );

   {
      int flag = gfpFFelementCmp(GFPE_DATA(pA), GFPE_DATA(pB), GFP_FESIZE(pGFpCtx));
      *pResult = (0==flag)? IppsElementEQ : (flag>0)? IppsElementGT : IppsElementLT;
      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPCpyElement, (const IppsGFPElement* pA, IppsGFPElement* pR, const IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpFFelementCopy(GFPE_DATA(pA), GFPE_DATA(pR), GFP_FESIZE(pGFpCtx));

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPNeg,(const IppsGFPElement* pA,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpNeg(GFPE_DATA(pA), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPInv,(const IppsGFPElement* pA,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );
   IPP_BADARG_RET( GFP_IS_ZERO(GFPE_DATA(pA),GFP_FESIZE(pGFpCtx)), ippStsDivByZeroErr );

   gfpInv(GFPE_DATA(pA), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPSqrt,(const IppsGFPElement* pA,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   return gfpSqrt(GFPE_DATA(pA), GFPE_DATA(pR), pGFpCtx)? ippStsNoErr : ippStsSqrtNegArg;
}


IPPFUN(IppStatus, ippsGFPAdd,(const IppsGFPElement* pA, const IppsGFPElement* pB,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpAdd(GFPE_DATA(pA), GFPE_DATA(pB), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPSub,(const IppsGFPElement* pA, const IppsGFPElement* pB,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpSub(GFPE_DATA(pA), GFPE_DATA(pB), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPMul,(const IppsGFPElement* pA, const IppsGFPElement* pB,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpMul(GFPE_DATA(pA), GFPE_DATA(pB), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPExp,(const IppsGFPElement* pA, const IppsGFPElement* pE,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR4_RET(pA, pE, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pE), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpExp(GFPE_DATA(pA), GFPE_DATA(pE), GFP_FESIZE(pGFpCtx), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPMontEncode,(const IppsGFPElement* pA,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpMontEnc(GFPE_DATA(pA), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGFPMontDecode,(const IppsGFPElement* pA,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpMontDec(GFPE_DATA(pA), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}


#if 0
IPPFUN(IppStatus, ippsGFPMontMul,(const IppsGFPElement* pA, const IppsGFPElement* pB,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpMontMul(GFPE_DATA(pA), GFPE_DATA(pB), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}
#endif

#if 0
/* assumed A and R are into the Montgomery Space */
IPPFUN(IppStatus, ippsGFPMontExp,(const IppsGFPElement* pA, const IppsGFPElement* pE,
                                    IppsGFPElement* pR, IppsGFPState* pGFpCtx))
{
   IPP_BAD_PTR4_RET(pA, pE, pR, pGFpCtx);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFpCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pE), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pR), ippStsContextMatchErr );

   gfpMontExp(GFPE_DATA(pA), GFPE_DATA(pE), GFP_FESIZE(pGFpCtx), GFPE_DATA(pR), pGFpCtx);

   return ippStsNoErr;
}
#endif
