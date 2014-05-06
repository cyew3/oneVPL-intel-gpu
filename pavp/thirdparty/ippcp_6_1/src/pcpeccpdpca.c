/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    EC over Prime Finite Field (setup/retrieve domain parameters)
//
// Contents:
//    ippsECCPSet()
//    ippsECCPSetStd()
//
//    ippsECCPGet()
//    ippsECCPGetBitSizeOrder()
//
//
//    Created: Mon 30-Jun-2003 10:02
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccp.h"
#include "pcpeccrandomizer.h"
#include "pcpeccppoint.h"
#include "pcpbnresource.h"
#include "pcpeccpmethod.h"
#include "pcpeccpmethod128.h"
#include "pcpeccpmethod192.h"
#include "pcpeccpmethod224.h"
#include "pcpeccpmethod256.h"
#include "pcpeccpmethod384.h"
#include "pcpeccpmethod521.h"
#include "pcpeccpmethodcom.h"
#include "pcppma.h"


/*F*
//    Name: ippsECCPSet
//
// Purpose: Set EC Domain Parameters.
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pPrime
//                            NULL == pA
//                            NULL == pB
//                            NULL == pGX
//                            NULL == pGY
//                            NULL == pOrder
//                            NULL == pECC
//
//    ippStsContextMatchErr   illegal pPrime->idCtx
//                            illegal pA->idCtx
//                            illegal pB->idCtx
//                            illegal pGX->idCtx
//                            illegal pGY->idCtx
//                            illegal pOrder->idCtx
//                            illegal pECC->idCtx
//
//    ippStsRangeErr          not enough room for:
//                            pPrime
//                            pA, pB,
//                            pGX,pGY
//                            pOrder
//
//    ippStsRangeErr          0>= cofactor
//
//    ippStsNoErr             no errors
//
// Parameters:
//    pPrime   pointer to the prime (specify FG(p))
//    pA       pointer to the A coefficient of EC equation
//    pB       pointer to the B coefficient of EC equation
//    pGX,pGY  pointer to the Base Point (x and y coordinates) of EC
//    pOrder   pointer to the Base Point order
//    cofactor cofactor value
//    pECC     pointer to the ECC context
//
*F*/
static
void ECCPSetDP(IppECCType flag,
               int primeSize, const Ipp32u* pPrime,
               int aSize,     const Ipp32u* pA,
               int bSize,     const Ipp32u* pB,
               int gxSize,    const Ipp32u* pGx,
               int gySize,    const Ipp32u* pGy,
               int orderSize, const Ipp32u* pOrder,
               Ipp32u cofactor,
               IppsECCPState* pECC)
{
   ECP_TYPE(pECC) = flag;

   /* reset size (bits) of field element */
   ECP_GFEBITS(pECC) = MSB_BNU(pPrime, primeSize) +1;
   /* reset size (bits) of Base Point order */
   ECP_ORDBITS(pECC) = MSB_BNU(pOrder, orderSize) +1;

   /* set up prime */
   ippsSet_BN(IppsBigNumPOS, primeSize, pPrime, ECP_PRIME(pECC));
   /* set up A */
   ippsSet_BN(IppsBigNumPOS, aSize, pA, ECP_A(pECC));
   /* test A */
   BN_Word(ECP_B(pECC), 3);
   PMA_add(ECP_B(pECC), ECP_A(pECC), ECP_B(pECC), ECP_PRIME(pECC));
   ECP_AMI3(pECC) = IsZero_BN(ECP_B(pECC));
   /* set up B */
   ippsSet_BN(IppsBigNumPOS, bSize, pB, ECP_B(pECC));

   /* set up affine coordinates of Base Point and order */
   ippsSet_BN(IppsBigNumPOS, gxSize, pGx, ECP_GX(pECC));
   ippsSet_BN(IppsBigNumPOS, gySize, pGy, ECP_GY(pECC));
   ippsSet_BN(IppsBigNumPOS, orderSize, pOrder, ECP_ORDER(pECC));

   /* set up cofactor */
   //ippsSet_BN(IppsBigNumPOS, 1, &((Ipp32u)cofactor), ECP_COFACTOR(pECC));
   ippsSet_BN(IppsBigNumPOS, 1, &cofactor, ECP_COFACTOR(pECC));

   #if defined(_USE_NN_VERSION_)
   /* set up randomizer */
   //gres 05/14/05: ECP_RANDMASK(pECC) = 0xFFFFFFFF >> ((32 -(ECP_ORDBITS(pECC)&0x1F)) &0x1F);
   ECP_RANDMASK(pECC) = MAKEMASK32(ECP_ORDBITS(pECC));
   ECP_RANDMASK(pECC) &= ~pOrder[orderSize-1];
   /* reinit randomizer */
   ippsPRNGInit(ECP_ORDBITS(pECC), ECP_RAND(pECC));
   /* default randomizer settings */
   {
      Ipp32u seed[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      ippsPRNGSetSeed(seed, ECP_RAND(pECC));
      ippsSet_BN(IppsBigNumPOS, RAND_CONTENT_LEN, seed, ECP_RANDCNT(pECC));
   }
   #endif

   /* montgomery engine (prime) */
   if( ippStsNoErr == ippsMontSet(BN_NUMBER(ECP_PRIME(pECC)), BN_SIZE(ECP_PRIME(pECC)), ECP_PMONT(pECC)) ) {
      /* modulo reduction and montgomery form of A and B */
      PMA_mod(ECP_AENC(pECC), ECP_A(pECC),    ECP_PRIME(pECC));
      PMA_enc(ECP_AENC(pECC), ECP_AENC(pECC), ECP_PMONT(pECC));
      PMA_mod(ECP_BENC(pECC), ECP_B(pECC),    ECP_PRIME(pECC));
      PMA_enc(ECP_BENC(pECC), ECP_BENC(pECC), ECP_PMONT(pECC));
      /* projective coordinates and montgomery form of of Base Point */
      if( ( IsZero_BN(ECP_BENC(pECC)) && ECCP_IsPointAtAffineInfinity1(ECP_GX(pECC), ECP_GY(pECC))) ||
          (!IsZero_BN(ECP_BENC(pECC)) && ECCP_IsPointAtAffineInfinity0(ECP_GX(pECC), ECP_GY(pECC))) )
         ECCP_SetPointToInfinity(ECP_GENC(pECC));
      else {
         #if defined(_IPP_TRS)
         DECLARE_BN_Word(_trs_one32_, 1);
         INIT_BN_Word(_trs_one32_);

         ECCP_SetPointProjective(ECP_GX(pECC), ECP_GY(pECC), BN_ONE_REF(),
                                 ECP_GENC(pECC),
                                 pECC);
         #else
         ECP_METHOD(pECC)->SetPointProjective(ECP_GX(pECC), ECP_GY(pECC), BN_ONE_REF(),
                                 ECP_GENC(pECC),
                                 pECC);
         #endif
      }
   }

   /* montgomery engine (order) */
   if( ippStsNoErr == ippsMontSet(BN_NUMBER(ECP_ORDER(pECC)), BN_SIZE(ECP_ORDER(pECC)), ECP_RMONT(pECC)) )
      PMA_enc(ECP_COFACTOR(pECC), ECP_COFACTOR(pECC), ECP_RMONT(pECC));

   /* set zero private keys */
   BN_Word(ECP_PRIVATE(pECC), 0);
   BN_Word(ECP_PRIVATE_E(pECC), 0);

   /* set infinity public keys */
   ECCP_SetPointToInfinity(ECP_PUBLIC(pECC));
   ECCP_SetPointToInfinity(ECP_PUBLIC_E(pECC));
}


IPPFUN(IppStatus, ippsECCPSet, (const IppsBigNumState* pPrime,
                                const IppsBigNumState* pA, const IppsBigNumState* pB,
                                const IppsBigNumState* pGX,const IppsBigNumState* pGY,const IppsBigNumState* pOrder,
                                int cofactor,
                                IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test pPrime */
   IPP_BAD_PTR1_RET(pPrime);
   pPrime = (IppsBigNumState*)( IPP_ALIGNED_PTR(pPrime, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pPrime), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_SIZE(pPrime)>BITS2WORD32_SIZE(ECP_GFEBITS(pECC))), ippStsRangeErr);

   /* test pA and pB */
   IPP_BAD_PTR2_RET(pA,pB);
   pA = (IppsBigNumState*)( IPP_ALIGNED_PTR(pA, ALIGN_VAL) );
   pB = (IppsBigNumState*)( IPP_ALIGNED_PTR(pB, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pB), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_SIZE(pA)>BITS2WORD32_SIZE(ECP_GFEBITS(pECC))), ippStsRangeErr);
   IPP_BADARG_RET(((int)BN_SIZE(pB)>BITS2WORD32_SIZE(ECP_GFEBITS(pECC))), ippStsRangeErr);

   /* test pG and pGorder pointers */
   IPP_BAD_PTR3_RET(pGX,pGY, pOrder);
   pGX    = (IppsBigNumState*)( IPP_ALIGNED_PTR(pGX,    ALIGN_VAL) );
   pGY    = (IppsBigNumState*)( IPP_ALIGNED_PTR(pGY,    ALIGN_VAL) );
   pOrder = (IppsBigNumState*)( IPP_ALIGNED_PTR(pOrder, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pGX),    ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pGY),    ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pOrder), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_SIZE(pGX)>BITS2WORD32_SIZE(ECP_GFEBITS(pECC))),    ippStsRangeErr);
   IPP_BADARG_RET(((int)BN_SIZE(pGY)>BITS2WORD32_SIZE(ECP_GFEBITS(pECC))),    ippStsRangeErr);
   IPP_BADARG_RET(((int)BN_SIZE(pOrder)>BITS2WORD32_SIZE(ECP_ORDBITS(pECC))), ippStsRangeErr);

   /* test cofactor */
   IPP_BADARG_RET(!(0<cofactor), ippStsRangeErr);

   /* set general methods */
   #if !defined(_IPP_TRS)
   *(ECP_METHOD(pECC)) = *(ECCPcom_Methods());//ECCPcom;
   #endif

   /* set domain parameters */
   ECCPSetDP(IppECCArbitrary,
             BN_SIZE(pPrime), BN_NUMBER(pPrime),
             BN_SIZE(pA),     BN_NUMBER(pA),
             BN_SIZE(pB),     BN_NUMBER(pB),
             BN_SIZE(pGX),    BN_NUMBER(pGX),
             BN_SIZE(pGY),    BN_NUMBER(pGY),
             BN_SIZE(pOrder), BN_NUMBER(pOrder),
             cofactor,
             pECC);

   return ippStsNoErr;
}


/*F*
//    Name: ippsECCPSetStd
//
// Purpose: Set Standard ECC Domain Parameter.
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pECC
//
//    ippStsContextMatchErr   illegal pECC->idCtx
//
//    ippStsECCInvalidFlagErr invalid flag
//
//    ippStsNoErr             no errors
//
// Parameters:
//    flag     specify standard ECC parameter(s) to be setup
//    pECC     pointer to the ECC context
//
*F*/
IPPFUN(IppStatus, ippsECCPSetStd, (IppECCType flag, IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   #if !defined(_IPP_TRS)
   *(ECP_METHOD(pECC)) = *(ECCPcom_Methods());//ECCPcom;
   #endif

   switch(flag) {
      case IppECCPStd112r1:
         ECCPSetDP(IppECCPStd112r1,
            BITS2WORD32_SIZE(112), secp112r1_p,
            BITS2WORD32_SIZE(112), secp112r1_a,
            BITS2WORD32_SIZE(112), secp112r1_b,
            BITS2WORD32_SIZE(112), secp112r1_gx,
            BITS2WORD32_SIZE(112), secp112r1_gy,
            BITS2WORD32_SIZE(112), secp112r1_r,
            secp112r1_h, pECC);
         break;
      case IppECCPStd112r2:
         ECCPSetDP(IppECCPStd112r2,
            BITS2WORD32_SIZE(112), secp112r2_p,
            BITS2WORD32_SIZE(112), secp112r2_a,
            BITS2WORD32_SIZE(112), secp112r2_b,
            BITS2WORD32_SIZE(112), secp112r2_gx,
            BITS2WORD32_SIZE(112), secp112r2_gy,
            BITS2WORD32_SIZE(112), secp112r2_r,
            secp112r2_h, pECC);
         break;
      case IppECCPStd128r1:
         #if !defined(_IPP_TRS)
         *(ECP_METHOD(pECC)) = *(ECCP128_Methods());//ECCP128;
         #endif
         ECCPSetDP(IppECCPStd128r1,
            BITS2WORD32_SIZE(128), secp128r1_p,
            BITS2WORD32_SIZE(128), secp128r1_a,
            BITS2WORD32_SIZE(128), secp128r1_b,
            BITS2WORD32_SIZE(128), secp128r1_gx,
            BITS2WORD32_SIZE(128), secp128r1_gy,
            BITS2WORD32_SIZE(128), secp128r1_r,
            secp128r1_h, pECC);
         break;
      case IppECCPStd128r2:
         #if !defined(_IPP_TRS)
         *(ECP_METHOD(pECC)) = *(ECCP128_Methods());//ECCP128;
         #endif
         ECCPSetDP(IppECCPStd128r2,
            BITS2WORD32_SIZE(128), secp128r2_p,
            BITS2WORD32_SIZE(128), secp128r2_a,
            BITS2WORD32_SIZE(128), secp128r2_b,
            BITS2WORD32_SIZE(128), secp128r2_gx,
            BITS2WORD32_SIZE(128), secp128r2_gy,
            BITS2WORD32_SIZE(128), secp128r2_r,
            secp128r2_h, pECC);
         break;
      case IppECCPStd160r1:
         ECCPSetDP(IppECCPStd160r1,
            BITS2WORD32_SIZE(160), secp160r1_p,
            BITS2WORD32_SIZE(160), secp160r1_a,
            BITS2WORD32_SIZE(160), secp160r1_b,
            BITS2WORD32_SIZE(160), secp160r1_gx,
            BITS2WORD32_SIZE(160), secp160r1_gy,
            BITS2WORD32_SIZE(161), secp160r1_r,
            secp160r1_h, pECC);
         break;
      case IppECCPStd160r2:
         ECCPSetDP(IppECCPStd160r2,
            BITS2WORD32_SIZE(160), secp160r2_p,
            BITS2WORD32_SIZE(160), secp160r2_a,
            BITS2WORD32_SIZE(160), secp160r2_b,
            BITS2WORD32_SIZE(160), secp160r2_gx,
            BITS2WORD32_SIZE(160), secp160r2_gy,
            BITS2WORD32_SIZE(161), secp160r2_r,
            secp160r2_h, pECC);
         break;
      case IppECCPStd192r1:
         #if !defined(_IPP_TRS)
         *(ECP_METHOD(pECC)) = *(ECCP192_Methods());//ECCP192;
         #endif
         ECCPSetDP(IppECCPStd192r1,
            BITS2WORD32_SIZE(192), secp192r1_p,
            BITS2WORD32_SIZE(192), secp192r1_a,
            BITS2WORD32_SIZE(192), secp192r1_b,
            BITS2WORD32_SIZE(192), secp192r1_gx,
            BITS2WORD32_SIZE(192), secp192r1_gy,
            BITS2WORD32_SIZE(192), secp192r1_r,
            secp192r1_h, pECC);
         break;
      case IppECCPStd224r1:
         #if !defined(_IPP_TRS)
         *(ECP_METHOD(pECC)) = *(ECCP224_Methods());//ECCP224;
         #endif
         ECCPSetDP(IppECCPStd224r1,
            BITS2WORD32_SIZE(224), secp224r1_p,
            BITS2WORD32_SIZE(224), secp224r1_a,
            BITS2WORD32_SIZE(224), secp224r1_b,
            BITS2WORD32_SIZE(224), secp224r1_gx,
            BITS2WORD32_SIZE(224), secp224r1_gy,
            BITS2WORD32_SIZE(224), secp224r1_r,
            secp224r1_h, pECC);
         break;
      case IppECCPStd256r1:
         #if !defined(_IPP_TRS)
         *(ECP_METHOD(pECC)) = *(ECCP256_Methods());//ECCP256;
         #endif
         ECCPSetDP(IppECCPStd256r1,
            BITS2WORD32_SIZE(256), secp256r1_p,
            BITS2WORD32_SIZE(256), secp256r1_a,
            BITS2WORD32_SIZE(256), secp256r1_b,
            BITS2WORD32_SIZE(256), secp256r1_gx,
            BITS2WORD32_SIZE(256), secp256r1_gy,
            BITS2WORD32_SIZE(256), secp256r1_r,
            secp256r1_h, pECC);
         break;
      case IppECCPStd384r1:
         #if !defined(_IPP_TRS)
         *(ECP_METHOD(pECC)) = *(ECCP384_Methods());//ECCP384;
         #endif
         ECCPSetDP(IppECCPStd384r1,
            BITS2WORD32_SIZE(384), secp384r1_p,
            BITS2WORD32_SIZE(384), secp384r1_a,
            BITS2WORD32_SIZE(384), secp384r1_b,
            BITS2WORD32_SIZE(384), secp384r1_gx,
            BITS2WORD32_SIZE(384), secp384r1_gy,
            BITS2WORD32_SIZE(384), secp384r1_r,
            secp384r1_h, pECC);
         break;
      case IppECCPStd521r1:
         #if !defined(_IPP_TRS)
         *(ECP_METHOD(pECC)) = *(ECCP521_Methods());//ECCP521;
         #endif
         ECCPSetDP(IppECCPStd521r1,
            BITS2WORD32_SIZE(521), secp521r1_p,
            BITS2WORD32_SIZE(521), secp521r1_a,
            BITS2WORD32_SIZE(521), secp521r1_b,
            BITS2WORD32_SIZE(521), secp521r1_gx,
            BITS2WORD32_SIZE(521), secp521r1_gy,
            BITS2WORD32_SIZE(521), secp521r1_r,
            secp521r1_h, pECC);
         break;
      default:
         return ippStsECCInvalidFlagErr;
   }

   return ippStsNoErr;
}


/*F*
//    Name: ippsECCPGet
//
// Purpose: Retrieve ECC Domain Parameter.
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pPrime
//                            NULL == pA
//                            NULL == pB
//                            NULL == pGX
//                            NULL == pGY
//                            NULL == pOrder
//                            NULL == cofactor
//                            NULL == pECC
//
//    ippStsContextMatchErr   illegal pPrime->idCtx
//                            illegal pA->idCtx
//                            illegal pB->idCtx
//                            illegal pGX->idCtx
//                            illegal pGY->idCtx
//                            illegal pOrder->idCtx
//                            illegal pECC->idCtx
//
//    ippStsRangeErr          not enough room for:
//                            pPrime
//                            pA, pB,
//                            pGX,pGY
//                            pOrder
//
//    ippStsNoErr             no errors
//
// Parameters:
//    pPrime   pointer to the retrieval prime (specify FG(p))
//    pA       pointer to the retrieval A coefficient of EC equation
//    pB       pointer to the retrieval B coefficient of EC equation
//    pGX,pGY  pointer to the retrieval Base Point (x and y coordinates) of EC
//    pOrder   pointer to the retrieval Base Point order
//    cofactor pointer to the retrieval cofactor value
//    pECC     pointer to the ECC context
//
*F*/
IPPFUN(IppStatus, ippsECCPGet, (IppsBigNumState* pPrime,
                                IppsBigNumState* pA, IppsBigNumState* pB,
                                IppsBigNumState* pGX,IppsBigNumState* pGY,IppsBigNumState* pOrder,
                                int* cofactor,
                                IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test pPrime */
   IPP_BAD_PTR1_RET(pPrime);
   pPrime = (IppsBigNumState*)( IPP_ALIGNED_PTR(pPrime, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pPrime), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_ROOM(pPrime)<BITS2WORD32_SIZE(ECP_GFEBITS(pECC))), ippStsRangeErr);

   /* test pA and pB */
   IPP_BAD_PTR2_RET(pA,pB);
   pA = (IppsBigNumState*)( IPP_ALIGNED_PTR(pA, ALIGN_VAL) );
   pB = (IppsBigNumState*)( IPP_ALIGNED_PTR(pB, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pB), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_ROOM(pA)<BITS2WORD32_SIZE(ECP_GFEBITS(pECC))), ippStsRangeErr);
   IPP_BADARG_RET(((int)BN_ROOM(pB)<BITS2WORD32_SIZE(ECP_GFEBITS(pECC))), ippStsRangeErr);

   /* test pG and pGorder pointers */
   IPP_BAD_PTR3_RET(pGX,pGY, pOrder);
   pGX   = (IppsBigNumState*)( IPP_ALIGNED_PTR(pGX,   ALIGN_VAL) );
   pGY   = (IppsBigNumState*)( IPP_ALIGNED_PTR(pGY,   ALIGN_VAL) );
   pOrder= (IppsBigNumState*)( IPP_ALIGNED_PTR(pOrder,ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pGX),    ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pGY),    ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pOrder), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_ROOM(pGX)<BITS2WORD32_SIZE(ECP_GFEBITS(pECC))),    ippStsRangeErr);
   IPP_BADARG_RET(((int)BN_ROOM(pGY)<BITS2WORD32_SIZE(ECP_GFEBITS(pECC))),    ippStsRangeErr);
   IPP_BADARG_RET(((int)BN_ROOM(pOrder)<BITS2WORD32_SIZE(ECP_ORDBITS(pECC))), ippStsRangeErr);

   /* test cofactor */
   IPP_BAD_PTR1_RET(cofactor);

   /* retrieve ECC parameter */
   {
      #if defined(_IPP_TRS)
      DECLARE_BN_Word(_trs_one32_, 1);
      INIT_BN_Word(_trs_one32_);
      #endif
      PMA_dec(pOrder, ECP_COFACTOR(pECC), ECP_RMONT(pECC));
   }
   *cofactor = BN_NUMBER(pOrder)[0];
   ippsSet_BN(BN_SIGN(ECP_PRIME(pECC)), BN_SIZE(ECP_PRIME(pECC)), BN_NUMBER(ECP_PRIME(pECC)), pPrime);
   ippsSet_BN(BN_SIGN(ECP_A(pECC)),     BN_SIZE(ECP_A(pECC)),     BN_NUMBER(ECP_A(pECC)),     pA);
   ippsSet_BN(BN_SIGN(ECP_B(pECC)),     BN_SIZE(ECP_B(pECC)),     BN_NUMBER(ECP_B(pECC)),     pB);
   ippsSet_BN(BN_SIGN(ECP_GX(pECC)),    BN_SIZE(ECP_GX(pECC)),    BN_NUMBER(ECP_GX(pECC)),    pGX);
   ippsSet_BN(BN_SIGN(ECP_GY(pECC)),    BN_SIZE(ECP_GY(pECC)),    BN_NUMBER(ECP_GY(pECC)),    pGY);
   ippsSet_BN(BN_SIGN(ECP_ORDER(pECC)), BN_SIZE(ECP_ORDER(pECC)), BN_NUMBER(ECP_ORDER(pECC)), pOrder);

   return ippStsNoErr;
}


/*F*
//    Name: ippsECCPGetOrderBitSize
//
// Purpose: Retrieve size of Pase Point Order (in bits).
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pECC
//                            NULL == pBitSize
//
//    ippStsContextMatchErr   illegal pECC->idCtx
//
//    ippStsNoErr             no errors
//
// Parameters:
//    pBitSize pointer to the size of base point order
//    pECC     pointer to the ECC context
//
*F*/
IPPFUN(IppStatus, ippsECCPGetOrderBitSize,(int* pBitSize, IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use 4-byte aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, 4) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test pBitSize*/
   IPP_BAD_PTR1_RET(pBitSize);

   *pBitSize = ECP_ORDBITS(pECC);

   return ippStsNoErr;
}

#endif /* _IPP_v50_ */
