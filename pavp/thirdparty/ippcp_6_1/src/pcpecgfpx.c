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
//    EC over GF(p^k) definitinons
//
//    Context:
//       ippsGFPXECGetSize()
//       ippsGFPXECInit()
//
//       ippsGFPXECSet()
//       ippsGFPXECGet()
//       ippsGFPXECVerify()
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpecgfpx.h"


#define GFPXEC_STRUCT_SIZE       (IPP_MULTIPLE_OF(sizeof(IppsGFPXECState), 16))


IPPFUN(IppStatus, ippsGFPXECGetSize,(const IppsGFPXState* pGF, Ipp32u* pSizeInBytes))
{
   IPP_BAD_PTR2_RET(pGF, pSizeInBytes);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGF), ippStsContextMatchErr );

   {
      Ipp32u elementSize = GFPX_FESIZE(pGF);
      Ipp32u orderSize = GFPX_FESIZE(pGF);
      //int montgomeryCtxSize;
      //ippsMontGetSize(IppsBinaryMethod, orderSize, &montgomeryCtxSize);

      *pSizeInBytes = GFPXEC_STRUCT_SIZE
                     +elementSize*sizeof(Ipp32u) /* EC coeff    A */
                     +elementSize*sizeof(Ipp32u) /* EC coeff    B */
                     +elementSize*sizeof(Ipp32u) /* generator G.x */
                     +elementSize*sizeof(Ipp32u) /* generator G.y */
                     +elementSize*sizeof(Ipp32u) /* generator G.z */
                     +orderSize  *sizeof(Ipp32u) /* base_point order */
                     +elementSize*sizeof(Ipp32u)*3*ECPX_POOL_SIZE
                   //+montgomeryCtxSize
                     ;
      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPXECInit,(IppsGFPXECState* pEC,
                     const IppsGFPXElement* pA, const IppsGFPXElement* pB,
                     const IppsGFPXElement* pX, const IppsGFPXElement* pY,
                     const Ipp32u* pOrder, Ipp32u orderLen,
                     Ipp32u cofactor,
                     IppsGFPXState* pGF))
{
   IPP_BAD_PTR2_RET(pEC, pGF);
   IPP_BADARG_RET( !GFPX_TEST_ID(pGF), ippStsContextMatchErr );

   {
      Ipp8u* ptr = (Ipp8u*)pEC;

      Ipp32u elementSize = GFPX_FESIZE(pGF);
      Ipp32u orderSize = GFPX_FESIZE(pGF);
      //int montgomeryCtxSize;
      //ippsMontGetSize(IppsBinaryMethod, orderSize, &montgomeryCtxSize);

      ECPX_ID(pEC) = idCtxGFPXEC;
      ECPX_FESIZE(pEC) = elementSize*3;
      ECPX_FIELD(pEC) = pGF;

      ptr += GFPXEC_STRUCT_SIZE;
      ECPX_ORDBITSIZE(pEC) = GFPX_FESIZE(pGF)*BITSIZE(Ipp32u);
      ECPX_A(pEC)    = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u);
      ECPX_B(pEC)    = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u);
      ECPX_G(pEC)    = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u)*3;
      ECPX_R(pEC)    = (cpFFchunk*)(ptr);  ptr += orderSize*sizeof(Ipp32u);
      ECPX_POOL(pEC) = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u)*ECPX_POOL_SIZE;
      //ECPX_MONTR(pEC)= (IppsMontState*)(ptr);

      gfpFFelementPadd(0, ECPX_A(pEC), elementSize);
      gfpFFelementPadd(0, ECPX_B(pEC), elementSize);
      gfpFFelementPadd(0, ECPX_G(pEC), elementSize*3);
      gfpFFelementPadd(0, ECPX_R(pEC), orderSize);
      ECPX_COFACTOR(pEC) = 0;

      //ippsMontInit(IppsBinaryMethod, orderSize, ECPX_MONTR(pEC));

      return ippsGFPXECSet(pA,pB, pX,pY, pOrder,orderLen, cofactor, pEC);
   }
}


IPPFUN(IppStatus, ippsGFPXECSet,(const IppsGFPXElement* pA, const IppsGFPXElement* pB,
                     const IppsGFPXElement* pX, const IppsGFPXElement* pY,
                     const Ipp32u* pOrder, Ipp32u orderLen,
                     Ipp32u cofactor,
                     IppsGFPXECState* pEC))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );

   {
      IppsGFPXState* pGF = ECPX_FIELD(pEC);
      Ipp32u elementSize = GFPX_FESIZE(pGF);

      if(pA) {
         IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
         gfpxMontEnc(GFPXE_DATA(pA), ECPX_A(pEC), pGF);
      }

      if(pB) {
         IPP_BADARG_RET( !GFPXE_TEST_ID(pB), ippStsContextMatchErr );
         gfpxMontEnc(GFPXE_DATA(pB), ECPX_B(pEC), pGF);
      }

      if(pX && pY) {
         IPP_BADARG_RET( !GFPXE_TEST_ID(pX), ippStsContextMatchErr );
         IPP_BADARG_RET( !GFPXE_TEST_ID(pY), ippStsContextMatchErr );
         gfpxMontEnc(GFPXE_DATA(pX), ECPX_G(pEC), pGF);
         gfpxMontEnc(GFPXE_DATA(pY), ECPX_G(pEC)+elementSize, pGF);
         GFPX_MONT_ONE(ECPX_G(pEC)+2*elementSize, pGF);
      }

      if(pOrder) {
         Ipp32u inOrderBitSize;
         Ipp32u orderSize;
         FIX_BNU(pOrder, orderLen);
         inOrderBitSize = orderLen*BITSIZE(Ipp32u) - NLZ32u(pOrder[orderLen-1]);
         IPP_BADARG_RET(inOrderBitSize>ECPX_ORDBITSIZE(pEC), ippStsRangeErr)

         ECPX_ORDBITSIZE(pEC) = inOrderBitSize;
         orderSize = BITS2WORD32_SIZE(inOrderBitSize);
         gfpFFelementCopy(pOrder, ECPX_R(pEC), orderSize);
         // I didn't found the requirement that "order" should be prime (or even)
         //ippsMontSet(pOrder, orderSize, ECPX_MONTR(pEC));
      }

      if(cofactor)
         ECPX_COFACTOR(pEC) = cofactor;

      return ippStsNoErr;
   }
}


#if 0
IPPFUN(IppStatus, ippsGFPXECGetOrderBitSize,(const IppsGFPXECState* pEC, Ipp32u* pBitSize))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );

   *pBitSize = ECPX_ORDBITSIZE(pEC);
   return ippStsNoErr;
}
#endif

#if 0
IPPFUN(IppStatus, ippsGFPXECGetElementBitSize,(const IppsGFPXECState* pEC, Ipp32u* pBitSize))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );

   //*pBitSize = GFPX_FEBITSIZE(ECPX_FIELD(pEC));
   return ippStsNoErr;
}
#endif


IPPFUN(IppStatus, ippsGFPXECGet,(const IppsGFPXECState* pEC,
                     const IppsGFPXState** ppGF, Ipp32u* pElementLen,
                     IppsGFPXElement* pA, IppsGFPXElement* pB,
                     IppsGFPXElement* pX, IppsGFPXElement* pY,
                     const Ipp32u** ppOrder, Ipp32u* pOrderLen,
                     Ipp32u* pCofactor))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );

   {
      IppsGFPXState* pGF = ECPX_FIELD(pEC);
      Ipp32u elementSize = GFPX_FESIZE(pGF);

      if(ppGF) {
         *ppGF = pGF;
      }
      if(pElementLen) {
         *pElementLen = elementSize;
      }

      if(pA) {
         IPP_BADARG_RET( !GFPXE_TEST_ID(pA), ippStsContextMatchErr );
         gfpxMontDec(ECPX_A(pEC), GFPXE_DATA(pA), pGF);
      }
      if(pB) {
         IPP_BADARG_RET( !GFPXE_TEST_ID(pB), ippStsContextMatchErr );
         gfpxMontDec(ECPX_B(pEC), GFPXE_DATA(pB), pGF);
      }

      if(pX) {
         IPP_BADARG_RET( !GFPXE_TEST_ID(pX), ippStsContextMatchErr );
         gfpxMontDec(ECPX_G(pEC), GFPXE_DATA(pX), pGF);
      }
      if(pY) {
         IPP_BADARG_RET( !GFPXE_TEST_ID(pY), ippStsContextMatchErr );
         gfpxMontDec(ECPX_G(pEC)+elementSize, GFPE_DATA(pY), pGF);
      }

      if(ppOrder) {
         //gfpFFelementCopy(ECPX_R(pEC), pOrder, BITS2WORD32_SIZE(ECPX_ORDBITSIZE(pEC)));
         *ppOrder = ECPX_R(pEC);
      }
      if(pOrderLen) {
         *pOrderLen = BITS2WORD32_SIZE(ECPX_ORDBITSIZE(pEC));
      }

      if(pCofactor)
         *pCofactor = ECPX_COFACTOR(pEC);

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPXECVerify,(IppsGFPXECState* pEC, IppECResult* pResult))
{
   IPP_BAD_PTR2_RET(pEC, pResult);
   IPP_BADARG_RET( !ECPX_TEST_ID(pEC), ippStsContextMatchErr );

   *pResult = ippECValid;

   {
      IppsGFPXState* pGF = ECPX_FIELD(pEC);
      Ipp32u elementSize = GFPX_FESIZE(pGF);

      /*
      // check discriminant ( 4*A^3 + 27*B^2 != 0 mod P)
      */
      if(ippECValid == *pResult) {
         cpFFchunk* pT = gfpxGetPool(1, pGF);
         cpFFchunk* pU = gfpxGetPool(1, pGF);

         gfpxAdd(ECPX_A(pEC), ECPX_A(pEC), pT, pGF);
         gfpxMontMul(pT, pT, pT, pGF);
         gfpxMontMul(pT, ECPX_A(pEC), pT, pGF);

         gfpxAdd(ECPX_B(pEC), ECPX_B(pEC), pU, pGF);
         gfpxAdd(ECPX_B(pEC), pU, pU, pGF);
         gfpxMontMul(pU, pU, pU, pGF);

         gfpxAdd(pT, pU, pT, pGF);
         gfpxAdd(pT, pU, pT, pGF);
         gfpxAdd(pT, pU, pT, pGF);

         *pResult = GFPX_IS_ZERO(pT, pGF)? ippECIsZeroDiscriminant: ippECValid;

         gfpxReleasePool(2, pGF);
      }

      /*
      // check base point and it order
      */
      if(ippECValid == *pResult) {
         IppsGFPXECPoint G;
         ecgfpxNewPoint(&G, ECPX_G(pEC),ECPX_AFFINE_POINT|ECPX_FINITE_POINT, pEC);

         /* check G != infinity */
         *pResult = ecgfpxIsProjectivePointAtInfinity(&G, elementSize)? ippECPointIsAtInfinite : ippECValid;

         /* check G lies on EC */
         if(ippECValid == *pResult)
            *pResult = ecgfpxIsPointOnCurve(&G, pEC)? ippECValid : ippECPointIsNotValid;

         /* check Gorder*G = infinity */
         if(ippECValid == *pResult) {
            IppsGFPXECPoint T;
            ecgfpxNewPoint(&T, ecgfpxGetPool(1, pEC),0, pEC);

            ecgfpxMulPoint(&G, ECPX_R(pEC), BITS2WORD32_SIZE(ECPX_ORDBITSIZE(pEC)), &T, pEC);
            *pResult = ecgfpxIsProjectivePointAtInfinity(&T, elementSize)? ippECValid : ippECInvalidOrder;

            ecgfpxReleasePool(1, pEC);
         }
      }

      return ippStsNoErr;
   }
}

