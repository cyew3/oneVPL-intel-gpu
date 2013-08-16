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
//    Tate Pairing
//
//    Context:
//       ippsTatePairingDE3GetSize()
//       ippsTatePairingDE3Init()
//       ippsTatePairingDE3Get()
//
//       ippsTatePairingDE3Apply()
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcptatep.h"
#include "pcpgfp.h"
#include "pcpgfpext.h"
#include "pcpgfpqext.h"
#include "pcpecgfp.h"
#include "pcpecgfpx.h"


#define TATE_STRUCT_SIZE       (IPP_MULTIPLE_OF(sizeof(IppsGFPState), 16))


IPPFUN(IppStatus, ippsTatePairingDE3GetSize,(const IppsGFPECState* pG1,
                                             const IppsGFPXECState* pG2,
                                             Ipp32u* pSizeInBytes))
{
   IPP_BAD_PTR1_RET(pSizeInBytes);
   IPP_BAD_PTR2_RET(pG1, pG2);
   IPP_BADARG_RET( !ECP_TEST_ID(pG1), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_TEST_ID(pG2), ippStsContextMatchErr );

   {
      /* length of (q^2-q+1)/r */
      Ipp32u expLen = 2*GFP_FESIZE(ECP_FIELD(pG1));
      /* length of x^(q*i) array, i=0..GFPX_DEGREE() */
      IppsGFPXState* pGFPX = ECPX_FIELD(pG2);
      Ipp32u alphaTo = (GFPX_DEGREE(pGFPX)+1)*GFPX_FESIZE(pGFPX);

      *pSizeInBytes = TATE_STRUCT_SIZE
                     +expLen*sizeof(Ipp32u)
                     +alphaTo*sizeof(Ipp32u)
                     ;
      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsTatePairingDE3Init,(IppsTatePairingDE3State* pPairingCtx,
                                   const IppsGFPECState* pG1,
                                   const IppsGFPXECState* pG2,
                                   const IppsGFPXQState* pGT))
{
   IPP_BAD_PTR1_RET(pPairingCtx);
   IPP_BAD_PTR3_RET(pG1, pG2, pGT);

   IPP_BADARG_RET( !ECP_TEST_ID(pG1), ippStsContextMatchErr );
   IPP_BADARG_RET( !ECPX_TEST_ID(pG2), ippStsContextMatchErr );
   IPP_BADARG_RET(!GFPXQX_TEST_ID(pGT), ippStsContextMatchErr);

   {
      Ipp8u* ptr = (Ipp8u*)pPairingCtx;

      /* length of (q^2-q+1)/r */
      Ipp32u expLen = 2*GFP_FESIZE(ECP_FIELD(pG1));
      /* length of x^(q*i) array, i=0..GFPX_DEGREE() */
      IppsGFPXState* pGFPX = ECPX_FIELD(pG2);
      Ipp32u alphaTo = (GFPX_DEGREE(pGFPX)+1)*GFPX_FESIZE(pGFPX);

      PAIRING_ID(pPairingCtx) = idCtxPairing;

      PAIRING_G1(pPairingCtx) = (IppsGFPECState*)pG1;
      PAIRING_G2(pPairingCtx) = (IppsGFPXECState*)pG2;
      PAIRING_GT(pPairingCtx) = (IppsGFPXQState*)pGT;

      ptr += TATE_STRUCT_SIZE;
      PAIRING_EXPLEN(pPairingCtx) = 0;
      PAIRING_EXP(pPairingCtx)    = (cpFFchunk*)(ptr); ptr += expLen*sizeof(Ipp32u);
      PAIRING_ALPHATO(pPairingCtx)= (cpFFchunk*)(ptr); ptr += alphaTo*sizeof(Ipp32u);

      /* set zero */
      GFP_ZERO(PAIRING_EXP(pPairingCtx), expLen);
      GFP_ZERO(PAIRING_ALPHATO(pPairingCtx), alphaTo);

      /* compute exp = (q^2-q+1)/r */
      {
         Ipp32u isZeroReminder;
         IppsGFPState* pGF = ECP_FIELD(PAIRING_G1(pPairingCtx));

         cpFFchunk* pModulo = GFP_MODULUS(pGF);
         Ipp32u elementSize = GFP_FESIZE(pGF);

         cpFFchunk* pOrder = ECP_R(PAIRING_G1(pPairingCtx));
         Ipp32u orderSize = BITS2WORD32_SIZE(ECP_ORDBITSIZE(PAIRING_G1(pPairingCtx)));

         cpFFchunk* pExp = PAIRING_EXP(pPairingCtx);
         Ipp32u expSize;

         cpFFchunk* pSqr = gfpGetPool(2, pGF);
         Ipp32u sqrSize = elementSize*2;

         cpMul_BNUs(pModulo, elementSize,
                    pModulo, elementSize,
                    pSqr, (int*)&sqrSize);
         cpSub_BNUs(pSqr, sqrSize,
                    pModulo, elementSize,
                    pSqr, (int*)&sqrSize, (IppsBigNumSGN*)&isZeroReminder);
         isZeroReminder = 1;
         cpAdd_BNUs(pSqr, sqrSize,
                    &isZeroReminder, 1,
                    pSqr, (int*)&sqrSize, sqrSize);

         cpDiv_BNU(pSqr, sqrSize,
                   pOrder, orderSize,
                   pExp, (int*)&expSize, (int*)&sqrSize);

         FIX_BNU(pSqr, sqrSize);
         isZeroReminder = (1==sqrSize) && (0==pSqr[0]);
         gfpReleasePool(2, pGF);

         if(!isZeroReminder)
            IPP_ERROR_RET(ippStsBadArgErr); /* order does not divide (q^2-q+1) */

         /* set up actual size of exponent */
         PAIRING_EXPLEN(pPairingCtx) = expSize;
      }

      /* compute array of of x^(q*i) */
      {
         IppsGFPXState* pGFX = ECPX_FIELD(PAIRING_G2(pPairingCtx));
         Ipp32u elementSize = GFPX_FESIZE(pGFX);

         IppsGFPState* pGF = GFPX_GROUNDGF(pGFX);
         cpFFchunk* pExp = GFP_MODULUS(pGF);
         Ipp32u grElementSize = GFPX_GROUNDFESIZE(pGFX);

         {
            cpFFchunk* pDst = PAIRING_ALPHATO(pPairingCtx);
            GFP_ZERO(pDst, elementSize); /* x^(0*q) */
            GFP_MONT_ONE(GFPX_IDX_ELEMENT(pDst, 0, grElementSize), pGF);

            pDst += elementSize;
            GFP_ZERO(pDst, elementSize); /* x^(1*q) */
            GFP_MONT_ONE(GFPX_IDX_ELEMENT(pDst, 1, grElementSize), pGF);
            gfpxMontExp(pDst, pExp, grElementSize, pDst, pGFX);

            pDst += elementSize;  /* x^(2*q) */
            gfpxMontMul(pDst-elementSize, pDst-elementSize, pDst, pGFX);
         }
      }

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsTatePairingDE3Get,(const IppsTatePairingDE3State* pPairingCtx,
                                  const IppsGFPECState** ppG1,
                                  const IppsGFPXECState** ppG2,
                                  const IppsGFPXQState** ppGT))
{
   IPP_BAD_PTR1_RET(pPairingCtx);
   IPP_BADARG_RET(!PAIRING_TEST_ID(pPairingCtx), ippStsContextMatchErr);

   {
      if(ppG1) {
         *ppG1 = PAIRING_G1(pPairingCtx);
      }
      if(ppG2) {
         *ppG2 = PAIRING_G2(pPairingCtx);
      }
      if(ppGT) {
         *ppGT = PAIRING_GT(pPairingCtx);
      }
      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsTatePairingDE3Apply,(const IppsGFPECPoint* pPointG1,
                                           const IppsGFPXECPoint* pPointG2,
                                           IppsGFPXQElement* pElementGT,
                                           IppsTatePairingDE3State* pPairingCtx))
{
   IPP_BAD_PTR1_RET(pPairingCtx);
   IPP_BADARG_RET(!PAIRING_TEST_ID(pPairingCtx), ippStsContextMatchErr);
   IPP_BAD_PTR1_RET(pPointG1);
   IPP_BADARG_RET( !ECP_POINT_TEST_ID(pPointG1), ippStsContextMatchErr );
   IPP_BAD_PTR1_RET(pPointG2);
   IPP_BADARG_RET( !ECPX_POINT_TEST_ID(pPointG2), ippStsContextMatchErr );
   IPP_BAD_PTR1_RET(pElementGT);
   IPP_BADARG_RET(!GFPXQXE_TEST_ID(pElementGT), ippStsContextMatchErr);

   {
      IppsGFPXQState*  pGT = PAIRING_GT(pPairingCtx);
      IppsGFPECState*  pECPF = PAIRING_G1(pPairingCtx);
      IppsGFPXECState* pECFF = PAIRING_G2(pPairingCtx);
      IppsGFPState* pGFP = ECP_FIELD(pECPF);

      if( !IS_ECP_FINITE_POINT(pPointG1) || !IS_ECPX_FINITE_POINT(pPointG2) ) {
         GFPXQX_SET_MONT_ONE(GFPXQXE_DATA(pElementGT), pGT);
         return ippStsNoErr;
      }

      else {
         IppsGFPECPoint  affinePointG1;
         IppsGFPXECPoint affinePointG2;
         ecgfpNewPoint (&affinePointG1, ecgfpGetPool(1, pECPF),0, pECPF);
         ecgfpxNewPoint(&affinePointG2, ecgfpxGetPool(1, pECFF),0, pECFF);

         if( IS_ECP_AFFINE_POINT(pPointG1) )
            ecgfpCopyPoint(pPointG1, &affinePointG1, GFP_FESIZE(ECPX_FIELD(pECPF)));
         else {
            ecgfpGetAffinePoint(pPointG1, ECP_POINT_X(&affinePointG1), ECP_POINT_Y(&affinePointG1), pECPF);
            ecgfpSetAffinePoint(ECP_POINT_X(&affinePointG1), ECP_POINT_Y(&affinePointG1), &affinePointG1, pECPF);
         }

         if( IS_ECPX_AFFINE_POINT(pPointG2) )
            ecgfpxCopyPoint(pPointG2, &affinePointG2, GFPX_FESIZE(ECPX_FIELD(pECFF)));
         else {
            ecgfpxGetAffinePoint(pPointG2, ECPX_POINT_X(&affinePointG2), ECPX_POINT_Y(&affinePointG2), pECFF);
            ecgfpxSetAffinePoint(ECPX_POINT_X(&affinePointG2), ECPX_POINT_Y(&affinePointG2), &affinePointG2, pECFF);
         }

         {
            cpFFchunk* pQx = gfpxqxGetPool(1, pGT);
            cpFFchunk* pQy = gfpxqxGetPool(1, pGT);

            cpFFchunk* pQnrInv = gfpGetPool(1, pGFP);
            gfpNeg (GFPXQX_MODULUS(pGT), pQnrInv, pGFP); /* -(-qnr) = qnr */
            //gfpMontInv(GFPXQX_MODULUS(pGT), pQnrInv, pGFP);
            gfpMontInv(pQnrInv, pQnrInv, pGFP);

            GFP_ZERO(pQx, GFPXQX_FESIZE(pGT));
            GFP_ZERO(pQy, GFPXQX_FESIZE(pGT));
            {
               IppsGFPXState* pGFPX= ECPX_FIELD(PAIRING_G2(pPairingCtx));
               Ipp32u elementSize = GFPX_FESIZE(pGFPX);
               gfpxMontMul_GFP(ECPX_POINT_X(&affinePointG2), pQnrInv, pQx, pGFPX);
               gfpMontMul(pQnrInv, pQnrInv, pQnrInv, pGFP);
               gfpxMontMul_GFP(ECPX_POINT_Y(&affinePointG2), pQnrInv, pQy+elementSize, pGFPX);
            }

            gfpReleasePool(1, pGFP);

            cpTatePairing(ECP_POINT_DATA(&affinePointG1), pQx,pQy, GFPXQXE_DATA(pElementGT), pPairingCtx);

            gfpxqxReleasePool(2, pGT);
         }

         ecgfpxReleasePool(1, pECFF);
         ecgfpReleasePool(1, pECPF);

         return ippStsNoErr;
      }
   }
}
