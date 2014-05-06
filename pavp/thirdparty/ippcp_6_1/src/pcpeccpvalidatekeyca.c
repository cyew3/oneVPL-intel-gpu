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
//    EC over Prime Finite Field (EC Key Generation)
//
// Contents:
//    ippsECCPValidateKeyPair()
//
//
//    Created: Wed 09-Jul-2003 15:05
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccp.h"
#include "pcpeccppoint.h"
#include "pcpeccpmethod.h"
#include "pcpeccpmethodcom.h"


/*F*
//    Name: ippsECCPValidateKeyPair
//
// Purpose: Validate (private,public) Key Pair
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pECC
//                            NULL == pPrivate
//                            NULL == pPublic
//                            NULL == pResult
//
//    ippStsContextMatchErr   illegal pECC->idCtx
//                            illegal pPrivate->idCtx
//                            illegal pPublic->idCtx
//
//    ippStsNoErr             no errors
//
// Parameters:
//    pPrivate    pointer to the private key
//    pPublic     pointer to the public  key
//    pResult     pointer to the result:
//                ippECValid/ippECInvalidPrivateKey/ippECPointIsAtInfinite/ippECInvalidPublicKey
//    pECC        pointer to the ECCP context
//
*F*/
IPPFUN(IppStatus, ippsECCPValidateKeyPair, (const IppsBigNumState* pPrivate,
                                            const IppsECCPPointState* pPublic,
                                            IppECResult* pResult,
                                            IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test result */
   IPP_BAD_PTR1_RET(pResult);

   *pResult = ippECValid;

   /* private key validation request */
   if( pPrivate ) {
      pPrivate = (IppsBigNumState*)( IPP_ALIGNED_PTR(pPrivate, ALIGN_VAL) );
      IPP_BADARG_RET(!BN_VALID_ID(pPrivate), ippStsContextMatchErr);

      /* check private key */
      if( IsZero_BN(pPrivate) || (0 <= Cmp_BN(pPrivate, ECP_ORDER(pECC))) ) {
         *pResult = ippECInvalidPrivateKey;
         return ippStsNoErr;
      }
   }

   /* public key validation request */
   if( pPublic ) {
      pPublic = (IppsECCPPointState*)( IPP_ALIGNED_PTR(pPublic, ALIGN_VAL) );
      IPP_BADARG_RET(!ECP_POINT_VALID_ID(pPublic), ippStsContextMatchErr);

      /* check public key */

      /* public != point_at_Infinity */
      if( ECCP_IsPointAtInfinity(pPublic) ) {
         *pResult = ippECPointIsAtInfinite;
         return ippStsNoErr;
      }

      else {
         BigNumNode* pList = ECP_BNCTX(pECC);
         IppsECCPPointState Tmp;
         ECP_POINT_X(&Tmp) = cpBigNumListGet(&pList);
         ECP_POINT_Y(&Tmp) = cpBigNumListGet(&pList);
         ECP_POINT_Z(&Tmp) = cpBigNumListGet(&pList);

         /* order*public == point_at_Infinity */
         #if !defined(_IPP_TRS)
         ECP_METHOD(pECC)->MulPoint(pPublic, ECP_ORDER(pECC), &Tmp, pECC, pList);
         #else
         ECCP_MulPoint(pPublic, ECP_ORDER(pECC), &Tmp, pECC, pList);
         #endif
         if( !ECCP_IsPointAtInfinity(&Tmp) ) {
            *pResult = ippECInvalidPublicKey;
            return ippStsNoErr;
         }

         /* addition test: private*BasePoint == public */
         #if !defined(_IPP_TRS)
         if(pPrivate) {
            ECP_METHOD(pECC)->MulPoint(ECP_GENC(pECC), pPrivate, &Tmp, pECC, pList);
            if( ECP_METHOD(pECC)->ComparePoint(&Tmp, pPublic, pECC, pList) ) {
               *pResult = ippECInvalidKeyPair;
               return ippStsNoErr;
            }
         }
         #else
         if(pPrivate) {
            ECCP_MulPoint(ECP_GENC(pECC), pPrivate, &Tmp, pECC, pList);
            if( ECCP_ComparePoint(&Tmp, pPublic, pECC, pList) ) {
               *pResult = ippECInvalidKeyPair;
               return ippStsNoErr;
            }
         }
         #endif

         /* Note. All other tests:
         //    0 < public_x < prime
         //    0 < public_y < prime
         //    public lie on EC
         // already pass while public was constructed
         */
      }
   }

   return ippStsNoErr;
}

#endif /* _IPP_v50_ */
