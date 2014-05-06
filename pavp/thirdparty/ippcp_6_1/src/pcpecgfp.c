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
//    EC over GF(p) definitinons
//
//    Context:
//       ippsGFPECGetSize()
//       ippsGFPECInit()
//
//       ippsGFPECSet()
//       ippsGFPECGet()
//       ippsGFPECVerify()
//
*/
#include "precomp.h"
#include "owncp.h"

#include "pcpgfp.h"
#include "pcpecgfp.h"
#include "pcpbn.h"
#include "pcpmontgomery.h"


#define GFPEC_STRUCT_SIZE       (IPP_MULTIPLE_OF(sizeof(IppsGFPECState), 16))


IPPFUN(IppStatus, ippsGFPECGetSize,(const IppsGFPState* pGF, Ipp32u* pSizeInBytes))
{
   IPP_BAD_PTR2_RET(pGF, pSizeInBytes);
   IPP_BADARG_RET( !GFP_TEST_ID(pGF), ippStsContextMatchErr );

   {
      Ipp32u elementSize = GFP_FESIZE(pGF);
      Ipp32u orderSize = IPP_MULTIPLE_OF(BITS2WORD32_SIZE(1+GFP_FEBITSIZE(pGF)), BNUBASE_TYPE_SIZE);
      //int montgomeryCtxSize;
      //ippsMontGetSize(IppsBinaryMethod, orderSize, &montgomeryCtxSize);

      *pSizeInBytes = GFPEC_STRUCT_SIZE
                     +elementSize*sizeof(Ipp32u) /* EC coeff    A */
                     +elementSize*sizeof(Ipp32u) /* EC coeff    B */
                     +elementSize*sizeof(Ipp32u) /* generator G.x */
                     +elementSize*sizeof(Ipp32u) /* generator G.y */
                     +elementSize*sizeof(Ipp32u) /* generator G.z */
                     +orderSize  *sizeof(Ipp32u) /* base_point order */
                     +elementSize*sizeof(Ipp32u)*3*EC_POOL_SIZE
                   //+montgomeryCtxSize
                     ;
      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPECInit,(IppsGFPECState* pEC,
                     const IppsGFPElement* pA, const IppsGFPElement* pB,
                     const IppsGFPElement* pX, const IppsGFPElement* pY,
                     const Ipp32u* pOrder, Ipp32u orderLen,
                     Ipp32u cofactor,
                     IppsGFPState* pGF))
{
   IPP_BAD_PTR2_RET(pEC, pGF);
   IPP_BADARG_RET( !GFP_TEST_ID(pGF), ippStsContextMatchErr );

   {
      Ipp8u* ptr = (Ipp8u*)pEC;

      Ipp32u elementSize = GFP_FESIZE(pGF);
      Ipp32u orderSize = IPP_MULTIPLE_OF(BITS2WORD32_SIZE(1+GFP_FEBITSIZE(pGF)), BNUBASE_TYPE_SIZE);
      //int montgomeryCtxSize;
      //ippsMontGetSize(IppsBinaryMethod, orderSize, &montgomeryCtxSize);

      ECP_ID(pEC) = idCtxGFPEC;
      ECP_FESIZE(pEC) = elementSize*3;
      ECP_FIELD(pEC) = pGF;

      ptr += GFPEC_STRUCT_SIZE;
      ECP_ORDBITSIZE(pEC) = 1+GFP_FEBITSIZE(pGF);
      ECP_A(pEC)    = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u);
      ECP_B(pEC)    = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u);
      ECP_G(pEC)    = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u)*3;
      ECP_R(pEC)    = (cpFFchunk*)(ptr);  ptr += orderSize*sizeof(Ipp32u);
      ECP_POOL(pEC) = (cpFFchunk*)(ptr);  ptr += elementSize*sizeof(Ipp32u)*EC_POOL_SIZE;
      //ECP_MONTR(pEC)= (IppsMontState*)(ptr);

      gfpFFelementPadd(0, ECP_A(pEC), elementSize);
      gfpFFelementPadd(0, ECP_B(pEC), elementSize);
      gfpFFelementPadd(0, ECP_G(pEC), elementSize*3);
      gfpFFelementPadd(0, ECP_R(pEC), orderSize);
      ECP_COFACTOR(pEC) = 0;

      //ippsMontInit(IppsBinaryMethod, orderSize, ECP_MONTR(pEC));

      return ippsGFPECSet(pA,pB, pX,pY, pOrder,orderLen, cofactor, pEC);
   }
}


IPPFUN(IppStatus, ippsGFPECSet,(const IppsGFPElement* pA, const IppsGFPElement* pB,
                     const IppsGFPElement* pX, const IppsGFPElement* pY,
                     const Ipp32u* pOrder, Ipp32u orderLen,
                     Ipp32u cofactor,
                     IppsGFPECState* pEC))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );

   {
      IppsGFPState* pGF = ECP_FIELD(pEC);
      Ipp32u elementSize = GFP_FESIZE(pGF);

      if(pA) {
         IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
         gfpMontEnc(GFPE_DATA(pA), ECP_A(pEC), pGF);
      }

      if(pB) {
         IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
         gfpMontEnc(GFPE_DATA(pB), ECP_B(pEC), pGF);
      }

      if(pX && pY) {
         IPP_BADARG_RET( !GFPE_TEST_ID(pX), ippStsContextMatchErr );
         IPP_BADARG_RET( !GFPE_TEST_ID(pY), ippStsContextMatchErr );
         gfpMontEnc(GFPE_DATA(pX), ECP_G(pEC), pGF);
         gfpMontEnc(GFPE_DATA(pY), ECP_G(pEC)+elementSize, pGF);
         gfpFFelementCopy(GFP_MONTUNITY(pGF), ECP_G(pEC)+elementSize*2, elementSize);
      }

      if(pOrder && orderLen) {
         Ipp32u inOrderBitSize;
         Ipp32u orderSize;
         FIX_BNU(pOrder, orderLen);
         inOrderBitSize = orderLen*BITSIZE(Ipp32u) - NLZ32u(pOrder[orderLen-1]);
         IPP_BADARG_RET(inOrderBitSize>ECP_ORDBITSIZE(pEC), ippStsRangeErr)

         ECP_ORDBITSIZE(pEC) = inOrderBitSize;
         orderSize = BITS2WORD32_SIZE(inOrderBitSize);
         gfpFFelementCopy(pOrder, ECP_R(pEC), orderSize);
         //ippsMontSet(pOrder, orderSize, ECP_MONTR(pEC));
      }

      if(cofactor)
         ECP_COFACTOR(pEC) = cofactor;

      return ippStsNoErr;
   }
}

#if 0
IPPFUN(IppStatus, ippsGFPECGetOrderBitSize,(const IppsGFPECState* pEC, Ipp32u* pBitSize))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );

   *pBitSize = ECP_ORDBITSIZE(pEC);
   return ippStsNoErr;
}
#endif

#if 0
IPPFUN(IppStatus, ippsGFPECGetElementBitSize,(const IppsGFPECState* pEC, Ipp32u* pBitSize))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );

   *pBitSize = GFP_FEBITSIZE(ECP_FIELD(pEC));
   return ippStsNoErr;
}
#endif


IPPFUN(IppStatus, ippsGFPECGet,(const IppsGFPECState* pEC,
                     const IppsGFPState** ppGF, Ipp32u* pElementLen,
                     IppsGFPElement* pA, IppsGFPElement* pB,
                     IppsGFPElement* pX, IppsGFPElement* pY,
                     const Ipp32u** ppOrder, Ipp32u* pOrderLen,
                     Ipp32u* pCofactor))
{
   IPP_BAD_PTR1_RET(pEC);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );

   {
      IppsGFPState* pGF = ECP_FIELD(pEC);
      Ipp32u elementSize = GFP_FESIZE(pGF);

      if(ppGF) {
         *ppGF = pGF;
      }
      if(pElementLen) {
         *pElementLen = elementSize;
      }

      if(pA) {
         IPP_BADARG_RET( !GFPE_TEST_ID(pA), ippStsContextMatchErr );
         gfpMontDec(ECP_A(pEC), GFPE_DATA(pA), pGF);
      }
      if(pB) {
         IPP_BADARG_RET( !GFPE_TEST_ID(pB), ippStsContextMatchErr );
         gfpMontDec(ECP_B(pEC), GFPE_DATA(pB), pGF);
      }

      if(pX) {
         IPP_BADARG_RET( !GFPE_TEST_ID(pX), ippStsContextMatchErr );
         gfpMontDec(ECP_G(pEC), GFPE_DATA(pX), pGF);
      }
      if(pY) {
         IPP_BADARG_RET( !GFPE_TEST_ID(pY), ippStsContextMatchErr );
         gfpMontDec(ECP_G(pEC)+elementSize, GFPE_DATA(pY), pGF);
      }

      if(ppOrder) {
         //gfpFFelementCopy(ECP_R(pEC), pOrder, BITS2WORD32_SIZE(ECP_ORDBITSIZE(pEC)));
         *ppOrder = ECP_R(pEC);
      }
      if(pOrderLen) {
         *pOrderLen = BITS2WORD32_SIZE(ECP_ORDBITSIZE(pEC));
      }

      if(pCofactor)
         *pCofactor = ECP_COFACTOR(pEC);

      return ippStsNoErr;
   }
}


IPPFUN(IppStatus, ippsGFPECVerify,(IppsGFPECState* pEC, IppECResult* pResult))
{
   IPP_BAD_PTR2_RET(pEC, pResult);
   IPP_BADARG_RET( !ECP_TEST_ID(pEC), ippStsContextMatchErr );

   *pResult = ippECValid;

   {
      IppsGFPState* pGF = ECP_FIELD(pEC);
      Ipp32u elementSize = GFP_FESIZE(pGF);

      /*
      // check discriminant ( 4*A^3 + 27*B^2 != 0 mod P)
      */
      if(ippECValid == *pResult) {
         cpFFchunk* pT = gfpGetPool(1, pGF);
         cpFFchunk* pU = gfpGetPool(1, pGF);

         gfpAdd(ECP_A(pEC), ECP_A(pEC), pT, pGF);
         gfpMontMul(pT, pT, pT, pGF);
         gfpMontMul(pT, ECP_A(pEC), pT, pGF);

         gfpAdd(ECP_B(pEC), ECP_B(pEC), pU, pGF);
         gfpAdd(ECP_B(pEC), pU, pU, pGF);
         gfpMontMul(pU, pU, pU, pGF);

         gfpAdd(pT, pU, pT, pGF);
         gfpAdd(pT, pU, pT, pGF);
         gfpAdd(pT, pU, pT, pGF);

         *pResult = GFP_IS_ZERO(pT, elementSize)? ippECIsZeroDiscriminant: ippECValid;

         gfpReleasePool(2, pGF);
      }

      /*
      // check base point and it order
      */
      if(ippECValid == *pResult) {
         IppsGFPECPoint G;
         ecgfpNewPoint(&G, ECP_G(pEC),ECP_AFFINE_POINT|ECP_FINITE_POINT, pEC);

         /* check G != infinity */
         *pResult = ecgfpIsProjectivePointAtInfinity(&G, elementSize)? ippECPointIsAtInfinite : ippECValid;

         /* check G lies on EC */
         if(ippECValid == *pResult)
            *pResult = ecgfpIsPointOnCurve(&G, pEC)? ippECValid : ippECPointIsNotValid;

         /* check Gorder*G = infinity */
         if(ippECValid == *pResult) {
            IppsGFPECPoint T;
            ecgfpNewPoint(&T, ecgfpGetPool(1, pEC),0, pEC);

            ecgfpMulPoint(&G, ECP_R(pEC), BITS2WORD32_SIZE(ECP_ORDBITSIZE(pEC)), &T, pEC);
            *pResult = ecgfpIsProjectivePointAtInfinity(&T, elementSize)? ippECValid : ippECInvalidOrder;

            ecgfpReleasePool(1, pEC);
         }
      }

      return ippStsNoErr;
   }
}

