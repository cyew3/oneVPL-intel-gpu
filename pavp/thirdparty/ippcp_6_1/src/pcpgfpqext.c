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
//    Quadratic Extension over GF(p^d) Definitinons
//
//    Context:
//       ippsGFPXQGetSize
//       ippsGFPXQInit
//       ippsGFPXQGet
//
//       ippsGFPXQElementGetSize
//       ippsGFPXQElementInit
//
//       ippsGFPXQSetElement
//       ippsGFPXQSetElementZero
//       ippsGFPXQSetElementPowerX
//       ippsGFPXQSetElementRandom
//       ippsGFPXQGetElement
//       ippsGFPXQCmpElement
//       ippsGFPXQCpyElement
//
//       ippsGFPXQAdd
//       ippsGFPXQSub
//       ippsGFPXQNeg
//       ippsGFPXQMul
//       ippsGFPXQMul_GFP
//       ippsGFPXQExp
//       ippsGFPXQInv
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpbn.h"
#include "pcpgfp.h"
#include "pcpgfpext.h"
#include "pcpgfpqext.h"

#define GFPXQ_STRUCT_SIZE        (IPP_MULTIPLE_OF(sizeof(IppsGFPXQState), 16))

IPPFUN(IppStatus, ippsGFPXQGetSize, (const IppsGFPXState* pGroundGF, Ipp32u* pStateSizeInBytes))
{
   IPP_BAD_PTR2_RET(pGroundGF, pStateSizeInBytes);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGroundGF), ippStsContextMatchErr );

   {
      Ipp32u elementGroundSize = GFPX_FESIZE(pGroundGF);
      *pStateSizeInBytes = GFPXQ_STRUCT_SIZE
                       +elementGroundSize * sizeof(cpFFchunk) * (GFPXQX_SAFEID_DEGREE+1) /* Place for modulus */
                       +elementGroundSize * sizeof(cpFFchunk) * GFPXQX_SAFEID_DEGREE * GFPXQX_POOL_SIZE;
      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsGFPXQInit, (IppsGFPXQState* pGFPQExtCtx, IppsGFPXState* pGroundGF,
                                  const Ipp32u* pModulus))
{
   IPP_BAD_PTR3_RET(pGroundGF, pGFPQExtCtx, pModulus);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGroundGF), ippStsContextMatchErr );

   {
      Ipp32u ctxCoeffSize = GFPX_FESIZE(pGroundGF);
      Ipp32u usrCoeffSize = (GFPX_DEGREE(pGroundGF)+1)*GFP_FESIZE32(GFPX_GROUNDGF(pGroundGF));
      Ipp8u* ptr = (Ipp8u*)pGFPQExtCtx;

      GFPXQX_ID(pGFPQExtCtx) = (IppCtxId)idCtxGFPXQX;
      GFPXQX_GROUNDGF(pGFPQExtCtx) = pGroundGF;
      GFPXQX_DEGREE(pGFPQExtCtx) = GFPXQX_SAFEID_DEGREE-1;
      GFPXQX_FESIZE(pGFPQExtCtx) = GFPXQX_SAFEID_DEGREE * ctxCoeffSize;
      ptr += GFPXQ_STRUCT_SIZE;
      GFPXQX_MODULUS(pGFPQExtCtx) = (cpFFchunk*)ptr;
      ptr += ctxCoeffSize*(GFPXQX_SAFEID_DEGREE+1)*sizeof(cpFFchunk); /* Module is stored with all coefficients */
      GFPXQX_POOL(pGFPQExtCtx) = (cpFFchunk*) ptr;

      gfpFFelementPadd(0, GFPXQX_MODULUS(pGFPQExtCtx), GFPXQX_FESIZE(pGFPQExtCtx)+ctxCoeffSize);

      /* Save coefficients */
      if(gfpxqxIsUserModulusValid(pModulus, GFPXQX_SAFEID_DEGREE*usrCoeffSize+1, pGFPQExtCtx)) {
         Ipp32u d;
         cpFFchunk* pFieldPoly = GFPXQX_MODULUS(pGFPQExtCtx);

         /* set up modulus */
         gfpSetPolynomial(pModulus, GFPXQX_SAFEID_DEGREE*usrCoeffSize+1,
                          pFieldPoly, GFPXQX_SAFEID_DEGREE*(GFPX_DEGREE(pGroundGF)+1), GFPX_GROUNDGF(pGroundGF));
         /* convert coefficients */
         for(d=0; d<=GFPXQX_SAFEID_DEGREE; d++) {
            gfpxMontEnc(pFieldPoly, pFieldPoly, pGroundGF);
            pFieldPoly += ctxCoeffSize;
         }
         return ippStsNoErr;
      }
      else
         return ippStsBadModulusErr;
   }
}
IPPFUN(IppStatus, ippsGFPXQGet, (const IppsGFPXQState* pGFPQExtCtx, const IppsGFPXState** ppGroundField, Ipp32u* pModulus, 
                                       Ipp32u* pModulusDegree, Ipp32u* pModulusLen, Ipp32u* pElementLen))
{
   IPP_BAD_PTR1_RET(pGFPQExtCtx);
   IPP_BADARG_RET( !GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr );

   {
      IppsGFPXState* pGF = GFPXQX_GROUNDGF(pGFPQExtCtx);

      if (ppGroundField)
         *ppGroundField = pGF;

      if (pModulus) {
         Ipp32u ctxCoeffSize = GFPX_FESIZE(pGF);
         Ipp32u usrCoeffSize = (GFPX_DEGREE(pGF)+1)*GFP_FESIZE32(GFPX_GROUNDGF(pGF));
         cpFFchunk* pCtxModulus = GFPXQX_MODULUS(pGFPQExtCtx);
         Ipp32u modulusDegree = GFPXQX_DEGREE(pGFPQExtCtx)+1;

         cpFFchunk* pDecodedCoeff = gfpxGetPool(1, pGF);
         Ipp32u d;
         for (d=0; d<=modulusDegree; d++, pCtxModulus+=ctxCoeffSize, pModulus+=usrCoeffSize) {
            gfpxMontDec(pCtxModulus, pDecodedCoeff, pGF);
            gfpGetPolynomial(pDecodedCoeff, GFPX_DEGREE(pGF), pModulus, usrCoeffSize, GFPX_GROUNDGF(pGF));
         }
         gfpxReleasePool(1, pGF);
      }

      if (pModulusDegree)
         *pModulusDegree = GFPXQX_DEGREE(pGFPQExtCtx)+1;

      if (pModulusLen)
         *pModulusLen = GFP_FESIZE32(GFPX_GROUNDGF(pGF))*(GFPXQX_DEGREE(pGFPQExtCtx)+2);

      if (pElementLen)
         *pElementLen = GFP_FESIZE32(GFPX_GROUNDGF(pGF))*(GFPXQX_DEGREE(pGFPQExtCtx)+1);

      return ippStsNoErr;
   }
}

#if 0
IPPFUN(IppStatus, ippsGFPXQSet, (IppsGFPXQState* pGFPQExtCtx, IppsGFPXState* pGroundField, const Ipp32u* pModulus))
{
   IPP_BAD_PTR1_RET(pGFPQExtCtx);
   IPP_BADARG_RET( !GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr );
   if (pGroundField) {
      IPP_BADARG_RET( !GFPX_TEST_ID(pGroundField), ippStsContextMatchErr );
      /* Check if new ground field fit into our pool */
      if (GFPXQX_FESIZE(pGFPQExtCtx) < GFPXQX_SAFEID_DEGREE * GFPX_FESIZE(pGroundField))
         return ippStsSizeErr;
      GFPXQX_GROUNDGF(pGFPQExtCtx) = pGroundField;
      GFPXQX_FESIZE(pGFPQExtCtx) = GFPXQX_SAFEID_DEGREE * GFPX_FESIZE(pGroundField);
   }
   if (pModulus) {
      Ipp32u elementGroundSize = GFPX_FESIZE(pGroundField);
      Ipp32u offset = elementGroundSize*(GFPXQX_DEGREE(pGFPQExtCtx)+1);
      gfpxqxMontEnc(pModulus, GFPXQX_MODULUS(pGFPQExtCtx), pGFPQExtCtx);
      gfpxMontEnc(pModulus+offset, GFPXQX_MODULUS(pGFPQExtCtx)+offset, GFPXQX_GROUNDGF(pGFPQExtCtx));
   }
   return ippStsNoErr;
}
#endif

IPPFUN(IppStatus, ippsGFPXQElementGetSize, (const IppsGFPXQState* pGFPQExtCtx, Ipp32u* pElementSize))
{
   IPP_BAD_PTR2_RET(pGFPQExtCtx, pElementSize);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);

   *pElementSize = sizeof(IppsGFPXQElement)
                 + GFPXQX_FESIZE(pGFPQExtCtx) * sizeof(cpFFchunk);
   *pElementSize = IPP_MULTIPLE_OF((*pElementSize), 16);
   return ippStsNoErr;
}

#if 0
IPPFUN(IppStatus, ippsGFPXQGetModulus, (const IppsGFPXQState* pGFPQExtCtx, Ipp32u* pData))
{
   //Ipp32u i;
   Ipp32u offset;
   IPP_BAD_PTR2_RET(pGFPQExtCtx, pData);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);

   gfpxqxMontDec(GFPXQX_MODULUS(pGFPQExtCtx), pData, pGFPQExtCtx);
   offset = GFPXQX_GROUNDFESIZE(pGFPQExtCtx) * (GFPXQX_DEGREE(pGFPQExtCtx)+1);
   gfpxMontDec(GFPXQX_MODULUS(pGFPQExtCtx)+offset, pData+offset, GFPXQX_GROUNDGF(pGFPQExtCtx));
   return ippStsNoErr;
}
#endif

IPPFUN(IppStatus, ippsGFPXQElementInit, (IppsGFPXQElement* pR, const Ipp32u* pA, Ipp32u lenA, IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR3_RET(pR, pA, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);

   {
      Ipp8u* ptr = (Ipp8u*)pR + sizeof(IppsGFPXQElement);
      GFPXQXE_DATA(pR) = (cpFFchunk*)ptr;
      GFPXQXE_ID(pR) = (IppCtxId)idCtxGFPXQXE;

      /* default data set up */
      gfpFFelementPadd(0, GFPXQXE_DATA(pR), GFPXQX_FESIZE(pGFPQExtCtx));

      if(pA && lenA) {
         if( !gfpIsArrayOverGF(pA, lenA, GFPX_GROUNDGF(GFPXQX_GROUNDGF(pGFPQExtCtx))) )
            return ippStsBadArgErr;
         gfpxqxReduce(pA, lenA, GFPXQXE_DATA(pR), pGFPQExtCtx);
      }

      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsGFPXQSetElement, (const Ipp32u* pA, Ipp32u lenA, IppsGFPXQElement* pR,
                                        IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFPQExtCtx);
   IPP_BADARG_RET( !GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr );

   if( !gfpIsArrayOverGF(pA, lenA, GFPX_GROUNDGF(GFPXQX_GROUNDGF(pGFPQExtCtx))) )
      return ippStsBadArgErr;
   gfpxqxReduce(pA, lenA, GFPXQXE_DATA(pR), pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQGetElement, (const IppsGFPXQElement* pA,
                                        Ipp32u* pDataA, Ipp32u dataLen,
                                        const IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pDataA, pGFPQExtCtx);
   IPP_BADARG_RET( !GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXQXE_TEST_ID(pA), ippStsContextMatchErr );

   {
      IppsGFPXState* pGFPX = GFPXQX_GROUNDGF(pGFPQExtCtx);
      Ipp32u degree = (GFPX_DEGREE(pGFPX)+1)*(GFPXQX_DEGREE(pGFPQExtCtx)+1) -1;

      gfpGetPolynomial(GFPXE_DATA(pA), degree, pDataA, dataLen, GFPX_GROUNDGF(pGFPX));
      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsGFPXQSetElementZero, (IppsGFPXQElement* pR, const IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFPQExtCtx);
   IPP_BADARG_RET( !GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXQXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpFFelementPadd(0, GFPXQXE_DATA(pR), GFPXQX_FESIZE(pGFPQExtCtx));
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQSetElementRandom, (IppsGFPXQElement* pR, IppsGFPXQState* pGFPQExtCtx, IppBitSupplier rndFunc, void* pRndParam))
{
   IPP_BAD_PTR2_RET(pR, pGFPQExtCtx);
   IPP_BADARG_RET( !GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXQXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpxqxRand(GFPXE_DATA(pR), pGFPQExtCtx, rndFunc, pRndParam);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQSetElementPowerX, (Ipp32u power, IppsGFPXQElement* pR, IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFPQExtCtx);
   IPP_BADARG_RET( !GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXQXE_TEST_ID(pR), ippStsContextMatchErr );

   {
      Ipp32u groundElementSize = GFPXQX_GROUNDFESIZE(pGFPQExtCtx);
      cpFFchunk* pData = GFPXQXE_DATA(pR);

      GFPXQX_ZERO(pData, pGFPQExtCtx);

      if (power <= GFPXQX_DEGREE(pGFPQExtCtx)) { /* reasonable degree */
         gfpFFelementSet32u(1, GFPX_IDX_ELEMENT(pData, power, groundElementSize), groundElementSize);
         return ippStsNoErr;
      }
      else {
         gfpFFelementSet32u(1, GFPX_IDX_ELEMENT(pData, 1, groundElementSize), groundElementSize);
         return ippsGFPXQExp(pR, &power, 1, pR, pGFPQExtCtx);
      }
   }
}


IPPFUN(IppStatus, ippsGFPXQCpyElement, (const IppsGFPXQElement* pA, IppsGFPXQElement* pR,
                                        const IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   gfpxqxFFelementCopy(GFPXQXE_DATA(pA), GFPXQXE_DATA(pR), pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQCmpElement, (const IppsGFPXQElement* pA, const IppsGFPXQElement* pB,
                                        IppsElementCmpResult* pResult,
                                        const IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pResult, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pB), ippStsContextMatchErr);

   {
      Ipp32s flag = gfpxqxFFelementCmp(GFPXQXE_DATA(pA), GFPXQXE_DATA(pB), pGFPQExtCtx);
      *pResult = (0==flag)? IppsElementEQ : IppsElementNE;
      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsGFPXQAdd, (const IppsGFPXQElement* pA, const IppsGFPXQElement* pB, IppsGFPXQElement* pR,
                                 IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pB), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   gfpxqxAdd(GFPXQXE_DATA(pA), GFPXQXE_DATA(pB), GFPXQXE_DATA(pR), pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQSub, (const IppsGFPXQElement* pA, const IppsGFPXQElement* pB, IppsGFPXQElement* pR,
                                 IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pB), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   gfpxqxSub(GFPXQXE_DATA(pA), GFPXQXE_DATA(pB), GFPXQXE_DATA(pR), pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQNeg, (const IppsGFPXQElement* pA, IppsGFPXQElement* pR,
                                 IppsGFPXQState* pGFPQExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   gfpxqxNeg(GFPXQXE_DATA(pA), GFPXQXE_DATA(pR), pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQMul, (const IppsGFPXQElement* pA, const IppsGFPXQElement* pB, IppsGFPXQElement* pR,
                                 IppsGFPXQState* pGFPQExtCtx))
{
   Ipp32u* pTmp;

   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pB), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   pTmp = gfpxqxGetPool(1, pGFPQExtCtx);
   gfpxqxMontEnc(GFPXQXE_DATA(pA), pTmp, pGFPQExtCtx);
   gfpxqxMontMul(pTmp, GFPXQXE_DATA(pB), GFPXQXE_DATA(pR), pGFPQExtCtx);
   gfpxqxReleasePool(1, pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQMul_GFP, (const IppsGFPXQElement* pA, const IppsGFPElement* pB, IppsGFPXQElement* pR,
                                     IppsGFPXQState* pGFPQExtCtx))
{
   cpFFchunk* pTmp;
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPE_TEST_ID(pB), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   pTmp = gfpxqxGetPool(1, pGFPQExtCtx);
   gfpxqxMontEnc(GFPXQXE_DATA(pA), pTmp, pGFPQExtCtx);
   gfpxqxMontMul_GFP(pTmp, GFPE_DATA(pB), GFPXQXE_DATA(pR), pGFPQExtCtx);
   gfpxqxReleasePool(1, pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQExp, (const IppsGFPXQElement* pA, const Ipp32u* pB, Ipp32u bSize, IppsGFPXQElement* pR,
                                 IppsGFPXQState* pGFPQExtCtx))
{
   cpFFchunk* pTmp;
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   pTmp = gfpxqxGetPool(1, pGFPQExtCtx);
   gfpxqxMontEnc(GFPXQXE_DATA(pA), pTmp, pGFPQExtCtx);
   gfpxqxMontExp(pTmp, pB, bSize, GFPXQXE_DATA(pR), pGFPQExtCtx);
   gfpxqxMontDec(GFPXQXE_DATA(pR), GFPXQXE_DATA(pR), pGFPQExtCtx);
   gfpxqxReleasePool(1, pGFPQExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXQInv, (const IppsGFPXQElement* pA, IppsGFPXQElement* pR,
                                 IppsGFPXQState* pGFPQExtCtx))
{
   cpFFchunk *pTmp;
   IPP_BAD_PTR3_RET(pA, pR, pGFPQExtCtx);
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGFPQExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pR), ippStsContextMatchErr);

   pTmp = gfpxqxGetPool(1, pGFPQExtCtx);
   gfpxqxMontEnc(GFPXQXE_DATA(pA), pTmp, pGFPQExtCtx);
   gfpxqxMontInv(pTmp, GFPXQXE_DATA(pR), pGFPQExtCtx);
   gfpxqxMontDec(GFPXQXE_DATA(pR), GFPXQXE_DATA(pR), pGFPQExtCtx);
   gfpxqxReleasePool(1, pGFPQExtCtx);
   return ippStsNoErr;
}
