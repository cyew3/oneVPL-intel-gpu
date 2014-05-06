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
//    GF(p) extension definitinons
//
//    Context:
//       ippsGFPXGetSize
//       ippsGFPXInit
//       ippsGFPXGet
//
//       ippsGFPXGetElementSize
//       ippsGFPXElementInit
//
//       ippsGFPXSetElement
//       ippsGFPXSetElementZero
//       ippsGFPXSetElementPowerX
//       ippsGFPXSetElementRandom
//       ippsGFPXGetElement
//       ippsGFPXCmpElement
//       ippsGFPXCpyElement
//
//       ippsGFPXAdd
//       ippsGFPXAdd_GFP
//       ippsGFPXSub
//       ippsGFPXSub_GFP
//       ippsGFPXNeg
//       ippsGFPXMul
//       ippsGFPXMul_GFP
//       ippsGFPXExp
//       ippsGFPXInv
//       ippsGFPXDiv
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpbn.h"
#include "pcpgfp.h"
#include "pcpgfpext.h"

#define GFPX_STRUCT_SIZE        (IPP_MULTIPLE_OF(sizeof(IppsGFPXState), 16))

/* Get context size */
IPPFUN(IppStatus, ippsGFPXGetSize, (const IppsGFPState* pGFPCtx, Ipp32u degree, Ipp32u* pStateSizeInBytes))
{
   IPP_BAD_PTR2_RET(pGFPCtx, pStateSizeInBytes);
   IPP_BADARG_RET( !GFP_TEST_ID(pGFPCtx), ippStsContextMatchErr );

   {
      Ipp32u elementGroundSize = GFP_FESIZE(pGFPCtx);
      *pStateSizeInBytes = GFPX_STRUCT_SIZE /* structure itself */
                       +elementGroundSize * sizeof(cpFFchunk) * (degree+1) /* polynomial coefficients. All coeffs including leading 1 */
                       +elementGroundSize * degree * sizeof(cpFFchunk) * GFPX_POOL_SIZE; /* pool of temporary variables */
      return ippStsNoErr;
   }
}

/* Init context with prime, degree and coeffs */
IPPFUN(IppStatus, ippsGFPXInit, (IppsGFPXState* pGFPExtCtx, IppsGFPState* pGroundGF,
                                 Ipp32u degree, const Ipp32u* pModulus))
{
   IPP_BAD_PTR3_RET(pGFPExtCtx, pGroundGF, pModulus);
   IPP_BADARG_RET(degree > (Ipp32u)GFPX_MAX_DEGREE, ippStsSizeErr);

   {
      Ipp32u elementGroundSize = GFP_FESIZE(pGroundGF);
      Ipp32u elementGroundSize32 = GFP_FESIZE32(pGroundGF);
      Ipp8u* ptr = (Ipp8u*)pGFPExtCtx;

      /* Set context identifier */
      GFPX_ID(pGFPExtCtx) = (IppCtxId)idCtxGFPX;
      /* Set degree */
      GFPX_DEGREE(pGFPExtCtx) = degree-1; /* Note: group element size is 1 less than basic polynomial degree */
      /* Remember GF(P) context address */
      GFPX_GROUNDGF(pGFPExtCtx) = pGroundGF;
      /* Set element size */
      GFPX_FESIZE(pGFPExtCtx)= degree*elementGroundSize;
      /* Set coefficients and pool addresses */
      ptr += GFPX_STRUCT_SIZE;
      GFPX_MODULUS(pGFPExtCtx) = (cpFFchunk*) ptr;
      ptr += elementGroundSize*(degree+1)*sizeof(cpFFchunk);
      GFPX_POOL(pGFPExtCtx) = (cpFFchunk*) ptr;

      gfpFFelementPadd(0, GFPX_MODULUS(pGFPExtCtx), GFPX_FESIZE(pGFPExtCtx)+elementGroundSize);

      /* Save coefficients */
      if(gfpxIsUserModulusValid(pModulus, degree*elementGroundSize32+1, pGFPExtCtx)) {
         Ipp32u d;
         cpFFchunk* pFieldPoly = GFPX_MODULUS(pGFPExtCtx);

         /* set up modulus */
         gfpSetPolynomial(pModulus, degree*elementGroundSize32+1, pFieldPoly, degree, pGroundGF);
         /* convert coefficients */
         for(d=0; d<=degree; d++) {
            gfpMontEnc(pFieldPoly, pFieldPoly, pGroundGF);
            pFieldPoly += elementGroundSize;
         }
         return ippStsNoErr;
      }
      else
         return ippStsBadModulusErr;
   }
}

#if 0
/* Re-init existing context with new content.
   Limitations: (new GFP elementsize)*(new degree) <= (old_GFP elementsize)*(old degree)
   */
IPPFUN(IppStatus, ippsGFPXSet, (IppsGFPXState* pGFPExtCtx, IppsGFPState* pGroundGF, Ipp32u degree, const Ipp32u* pModulus))
{
   IPP_BAD_PTR3_RET(pGFPExtCtx, pGroundGF, pModulus);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr);
   IPP_BADARG_RET( !GFP_TEST_ID(pGroundGF), ippStsContextMatchErr);
   IPP_BADARG_RET(degree > (Ipp32u)GFPX_MAX_DEGREE, ippStsSizeErr);
   IPP_BADARG_RET((GFP_FESIZE(pGroundGF)*degree) > (GFPX_FESIZE(pGFPExtCtx)+GFPX_GROUNDFESIZE(pGFPExtCtx)), ippStsSizeErr);
   {
      /* it's ok. can be reinitialized */
      return ippsGFPXInit(pGFPExtCtx, pGroundGF, degree, pModulus);
   }
}
#endif

/* Get various data from the structure */
IPPFUN(IppStatus, ippsGFPXGet, (const IppsGFPXState* pGFPExtCtx,
                                const IppsGFPState** ppGroundField, Ipp32u* pModulus, Ipp32u* pModulusDegree, Ipp32u* pModulusLen,
                                Ipp32u* pElementLen))
{
   IPP_BAD_PTR1_RET(pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr);

   {
      IppsGFPState* pGF = GFPX_GROUNDGF(pGFPExtCtx);

      if (ppGroundField)
         *ppGroundField = pGF; /* return pointer to IppsGFPState if asked */

      if (pModulus) {
         Ipp32u usrElementSize = GFP_FESIZE32(pGF);
         Ipp32u ctxElementSize = GFP_FESIZE(pGF);
         Ipp32u modulusDegree = GFPX_DEGREE(pGFPExtCtx)+1;
         cpFFchunk* pCtxModulus = GFPX_MODULUS(pGFPExtCtx);

         cpFFchunk* pTmp = gfpGetPool(1, pGF);
         Ipp32u d;
         for (d=0; d<=modulusDegree; d++, pCtxModulus+=ctxElementSize, pModulus+=usrElementSize) {
            gfpMontDec(pCtxModulus, pTmp, pGF);
            gfpFFelementCopy(pTmp, pModulus, usrElementSize);
         }
         gfpReleasePool(1, pGF);
      }

      if (pModulusDegree)
            *pModulusDegree = GFPX_DEGREE(pGFPExtCtx)+1;

      if (pModulusLen)
         *pModulusLen = GFP_FESIZE32(pGF)*(GFPX_DEGREE(pGFPExtCtx)+2);

      if (pElementLen)
         *pElementLen = GFP_FESIZE32(pGF)*(GFPX_DEGREE(pGFPExtCtx)+1);

      return ippStsNoErr;
   }
}

/* Get element size to store GFP extension element in 32-bit words */
IPPFUN(IppStatus, ippsGFPXElementGetSize, (const IppsGFPXState* pGFPExtCtx, Ipp32u* pElementSize))
{
   IPP_BAD_PTR2_RET(pElementSize, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );

   *pElementSize = sizeof(IppsGFPXElement)
                 + GFPX_FESIZE(pGFPExtCtx) * sizeof(cpFFchunk);
   return ippStsNoErr;
}

/* GF(p) ext. element initialization */
IPPFUN(IppStatus, ippsGFPXElementInit, (IppsGFPXElement* pR, const Ipp32u* pA, Ipp32u lenA, IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );

   {
      Ipp8u* ptr = (Ipp8u*)pR;

      ptr += sizeof(IppsGFPXElement);
      GFPXE_DATA(pR) = (cpFFchunk*)ptr;
      GFPXE_ID(pR) = (IppCtxId)idCtxGFPXE;

      /* default data set up */
      gfpFFelementPadd(0, GFPXE_DATA(pR), GFPX_FESIZE(pGFPExtCtx));

      if(pA && lenA) {
         if( !gfpIsArrayOverGF(pA, lenA, GFPX_GROUNDGF(pGFPExtCtx)) )
            return ippStsBadArgErr;
         gfpxReduce(pA, lenA, GFPXE_DATA(pR), pGFPExtCtx);
      }

      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsGFPXSetElement, (const Ipp32u* pA, Ipp32u lenA, IppsGFPXElement* pR, IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   if( !gfpIsArrayOverGF(pA, lenA, GFPX_GROUNDGF(pGFPExtCtx)) )
      return ippStsBadArgErr;
   gfpxReduce(pA, lenA, GFPXE_DATA(pR), pGFPExtCtx);
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXGetElement, (const IppsGFPXElement* pA,
                                       Ipp32u* pDataA, Ipp32u dataLen,
                                       const IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pDataA, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );

   gfpGetPolynomial(GFPXE_DATA(pA), GFPX_DEGREE(pGFPExtCtx), pDataA, dataLen, GFPX_GROUNDGF(pGFPExtCtx));
   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXCmpElement, (const IppsGFPXElement* pA, const IppsGFPXElement* pB,
                                       IppsElementCmpResult* pResult,
                                       const IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pResult, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pB), ippStsContextMatchErr );
   {
      Ipp32s flag = gfpxFFelementCmp(GFPXE_DATA(pA), GFPXE_DATA(pB), pGFPExtCtx);
      *pResult = (0==flag)? IppsElementEQ : IppsElementNE;
      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsGFPXSetElementZero, (IppsGFPXElement* pR, const IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpFFelementPadd(0, GFPXE_DATA(pR), GFPX_FESIZE(pGFPExtCtx));

   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsGFPXSetElementPowerX, (Ipp32u power, IppsGFPXElement* pR, IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR2_RET(pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   {
      Ipp32u groundSize = GFPX_GROUNDFESIZE(pGFPExtCtx);
      cpFFchunk* pData = GFPXE_DATA(pR);

      if (power <= GFPX_DEGREE(pGFPExtCtx)) { /* reasonable degree */
         GFPX_ZERO(pData, pGFPExtCtx);
         gfpFFelementSet32u(1, GFPX_IDX_ELEMENT(pData, power, groundSize), groundSize);
         return ippStsNoErr;
      }
      else {
         GFPX_ZERO(pData, pGFPExtCtx);
         gfpFFelementSet32u(1, GFPX_IDX_ELEMENT(pData, 1, groundSize), groundSize);
         return ippsGFPXExp(pR, &power,1, pR, pGFPExtCtx);
      }
   }
}

/* Copy GF(p) ext. element */
IPPFUN(IppStatus, ippsGFPXCpyElement, (const IppsGFPXElement* pA, IppsGFPXElement* pR, const IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpxFFelementCopy(GFPXE_DATA(pA), GFPXE_DATA(pR), pGFPExtCtx);

   return ippStsNoErr;
}

/* Add two GFPExt elements */
IPPFUN(IppStatus, ippsGFPXAdd, (const IppsGFPXElement* pA, const IppsGFPXElement* pB, IppsGFPXElement* pR,
                                IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpxAdd(GFPXE_DATA(pA), GFPXE_DATA(pB), GFPXE_DATA(pR), pGFPExtCtx);
   return ippStsNoErr;
}

/* Add a big integer to GFPExt element */
IPPFUN(IppStatus, ippsGFPXAdd_GFP, (const IppsGFPXElement* pA, const IppsGFPElement* pB, IppsGFPXElement* pR,
                                    IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpxAdd_GFP(GFPXE_DATA(pA), GFPE_DATA(pB), GFPXE_DATA(pR), pGFPExtCtx);
   return ippStsNoErr;
}

/* Sub GFPExt element from another element */
IPPFUN(IppStatus, ippsGFPXSub, (const IppsGFPXElement* pA, const IppsGFPXElement* pB, IppsGFPXElement* pR,
                                IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpxSub(GFPXE_DATA(pA), GFPXE_DATA(pB), GFPXE_DATA(pR), pGFPExtCtx);
   return ippStsNoErr;
}

/* Sub GFP element from a GFPExt element element */
IPPFUN(IppStatus, ippsGFPXSub_GFP, (const IppsGFPXElement* pA, const IppsGFPElement* pB, IppsGFPXElement* pR,
                                    IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpxSub_GFP(GFPXE_DATA(pA), GFPE_DATA(pB), GFPXE_DATA(pR), pGFPExtCtx);
   return ippStsNoErr;
}

/* Multiply two GFPExt elements */
IPPFUN(IppStatus, ippsGFPXMul, (const IppsGFPXElement* pA, const IppsGFPXElement* pB, IppsGFPXElement* pR,
                                IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );

   {
      cpFFchunk* tmp = gfpxGetPool(1, pGFPExtCtx);//
      gfpxMontEnc(GFPXE_DATA(pA), tmp, pGFPExtCtx);//
      gfpxMontMul(tmp, GFPXE_DATA(pB), GFPXE_DATA(pR), pGFPExtCtx);
      gfpxReleasePool(1, pGFPExtCtx);
      return ippStsNoErr;
   }
}

/* Multiply GFPExt element by a big integer */
IPPFUN(IppStatus, ippsGFPXMul_GFP, (const IppsGFPXElement* pA, const IppsGFPElement* pB, IppsGFPXElement* pR,
                                    IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );

   {
      IppsGFPState* pGFP  = GFPX_GROUNDGF(pGFPExtCtx);
      cpFFchunk* tmp = gfpGetPool(1, pGFP);

      gfpMontEnc(GFPE_DATA(pB), tmp, pGFP);
      gfpxMontMul_GFP(GFPXE_DATA(pA), tmp, GFPXE_DATA(pR), pGFPExtCtx);
      gfpReleasePool(1, pGFP);
      return ippStsNoErr;
   }
}

/* Negate GFPExt element */
IPPFUN(IppStatus, ippsGFPXNeg, (const IppsGFPXElement* pA, IppsGFPXElement* pR,
                                IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );

   gfpxNeg(GFPXE_DATA(pA), GFPXE_DATA(pR), pGFPExtCtx);
   return ippStsNoErr;
}

/* Exponent GFPExt element to a BIG integer degree */
IPPFUN(IppStatus, ippsGFPXExp, (const IppsGFPXElement* pA, const Ipp32u* pB, Ipp32u bSize, IppsGFPXElement* pR,
                                IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );

   {
      cpFFchunk* tmp = gfpxGetPool(1, pGFPExtCtx);
      gfpxMontEnc(GFPE_DATA(pA), tmp, pGFPExtCtx);
      gfpxMontExp(tmp, pB, bSize, GFPXE_DATA(pR), pGFPExtCtx);
      gfpxMontDec(GFPXE_DATA(pR), GFPXE_DATA(pR), pGFPExtCtx);
      gfpxReleasePool(1, pGFPExtCtx);
      return ippStsNoErr;
   }
}

/* Get inversion of GFPExt element */
IPPFUN(IppStatus, ippsGFPXInv, (const IppsGFPXElement* pA, IppsGFPXElement* pR,
                                IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR3_RET(pA, pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( (degree(GFPXE_DATA(pA), pGFPExtCtx)<1), ippStsDivByZeroErr );

   {
      cpFFchunk* tmp = gfpxGetPool(1, pGFPExtCtx);
      gfpxMontEnc(GFPXE_DATA(pA), tmp, pGFPExtCtx);
      if (NULL == gfpxMontInv(tmp, GFPXE_DATA(pR), pGFPExtCtx)) {
         gfpxReleasePool(1, pGFPExtCtx);
         return ippStsBadModulusErr; /* Likely cause - reducible polynomial */
      }
      gfpxMontDec(GFPXE_DATA(pR), GFPXE_DATA(pR), pGFPExtCtx);
      gfpxReleasePool(1, pGFPExtCtx);
      return ippStsNoErr;
   }
}

/* Divide two GFPExt elements */
IPPFUN(IppStatus, ippsGFPXDiv, (const IppsGFPXElement* pA, const IppsGFPXElement* pB,
                                IppsGFPXElement* pQ, IppsGFPXElement* pR,
                                IppsGFPXState* pGFPExtCtx))
{
   IPP_BAD_PTR4_RET(pA, pB, pQ, pR);
   IPP_BAD_PTR1_RET(pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );

   {
      cpFFchunk* tmp = gfpxGetPool(1, pGFPExtCtx);
      gfpxMontEnc(GFPXE_DATA(pB), tmp, pGFPExtCtx);

      if (NULL == gfpxDiv(GFPXE_DATA(pA), tmp, GFPXE_DATA(pQ), GFPXE_DATA(pR), pGFPExtCtx)) {
         gfpxReleasePool(1, pGFPExtCtx);
         return ippStsDivByZeroErr;
      }
      gfpxReleasePool(1, pGFPExtCtx);
      return ippStsNoErr;
   }
}

/* Pick a random GFPExt element */
IPPFUN(IppStatus, ippsGFPXSetElementRandom, (IppsGFPXElement* pR, IppsGFPXState* pGFPExtCtx,
                                             IppBitSupplier rndFunc, void* pRndParam))
{
   IPP_BAD_PTR2_RET(pR, pGFPExtCtx);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGFPExtCtx), ippStsContextMatchErr );
   IPP_BADARG_RET( !GFPXE_TEST_ID(pR), ippStsContextMatchErr );

   gfpxRand(GFPXE_DATA(pR), pGFPExtCtx, rndFunc, pRndParam);
   return ippStsNoErr;
}
