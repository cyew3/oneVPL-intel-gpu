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
//    EC over Prime Finite Field (Verify Signature, DSA version)
//
// Contents:
//    ippsECCPVerifyDSA()
//
//
//    Created: Mon 14-Jul-2003 10:43
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
//    Name: ippsECCPVerifyDSA
//
// Purpose: Verify Signature (DSA version).
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pECC
//                               NULL == pMsgDigest
//                               NULL == pSignX
//                               NULL == pSignY
//                               NULL == pResult
//
//    ippStsContextMatchErr      illegal pECC->idCtx
//                               illegal pMsgDigest->idCtx
//                               illegal pSignX->idCtx
//                               illegal pSignY->idCtx
//
//    ippStsMessageErr           MsgDigest >= order
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pMsgDigest     pointer to the message representative to be signed
//    pSignX,pSignY  pointer to the signature
//    pResult        pointer to the result: ippECValid/ippECInvalidSignature
//    pECC           pointer to the ECCP context
//
// Note:
//    - signer's key must be set up in ECCP context
//      before ippsECCPVerifyDSA() usage
//
*F*/
IPPFUN(IppStatus, ippsECCPVerifyDSA,(const IppsBigNumState* pMsgDigest,
                                     const IppsBigNumState* pSignX, const IppsBigNumState* pSignY,
                                     IppECResult* pResult,
                                     IppsECCPState* pECC))
{
   IppsMontState* rMont;
   IppsBigNumState* pOrder;

   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test message representative */
   IPP_BAD_PTR1_RET(pMsgDigest);
   pMsgDigest = (IppsBigNumState*)( IPP_ALIGNED_PTR(pMsgDigest, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pMsgDigest), ippStsContextMatchErr);
   rMont  = ECP_RMONT(pECC);
   pOrder = MNT_MODULO(rMont);
   IPP_BADARG_RET((0<=Cmp_BN(pMsgDigest, pOrder)), ippStsMessageErr);

   /* test result */
   IPP_BAD_PTR1_RET(pResult);


   /* test signature */
   IPP_BAD_PTR2_RET(pSignX,pSignY);
   pSignX = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSignX, ALIGN_VAL) );
   pSignY = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSignY, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pSignX), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pSignY), ippStsContextMatchErr);

   /* test signature value */
   if( (0>Tst_BN(pSignX)) || (0>Tst_BN(pSignY)) ||
       (0<=Cmp_BN(pSignX, pOrder)) || (0<=Cmp_BN(pSignY, pOrder)) ) {
      *pResult = ippECInvalidSignature;
      return ippStsNoErr;
   }

   /* validate signature */
   else {
      BigNumNode* pList = ECP_BNCTX(pECC);
      IppsBigNumState* pH1 = cpBigNumListGet(&pList);
      IppsBigNumState* pH2 = cpBigNumListGet(&pList);
      IppsECCPPointState P1;
      ECP_POINT_X(&P1) = cpBigNumListGet(&pList);
      ECP_POINT_Y(&P1) = cpBigNumListGet(&pList);
      ECP_POINT_Z(&P1) = cpBigNumListGet(&pList);

      PMA_inv(pH1, (IppsBigNumState*)pSignY, pOrder);/* h = 1/signY (mod order) */
      PMA_enc(pH1, pH1, rMont);
      PMA_mule(pH2, (IppsBigNumState*)pSignX,     pH1, rMont);   /* h2 = pSignX     * h (mod order) */
      PMA_mule(pH1, (IppsBigNumState*)pMsgDigest, pH1, rMont);   /* h1 = pMsgDigest * h (mod order) */

      /* compute h1*BasePoint + h2*publicKey */
      #if !defined(_IPP_TRS)
      ECP_METHOD(pECC)->ProdPoint(ECP_GENC(pECC),   pH1,
                                  ECP_PUBLIC(pECC), pH2,
                                  &P1, pECC, pList);
      #else
      ECCP_ProdPoint(ECP_GENC(pECC),   pH1,
                                  ECP_PUBLIC(pECC), pH2,
                                  &P1, pECC, pList);
      #endif

      if( ECCP_IsPointAtInfinity(&P1) ) {
         *pResult = ippECInvalidSignature;
         return ippStsNoErr;
      }
      /* extract X component */
      #if !defined(_IPP_TRS)
      ECP_METHOD(pECC)->GetPointAffine(pH1, NULL, &P1, pECC, pList);
      #else
      ECCP_GetPointAffine(pH1, NULL, &P1, pECC, pList);
      #endif
      /* compare with signX */
      PMA_mod(pH1, pH1, pOrder);
      *pResult = (0==Cmp_BN(pH1, pSignX))? ippECValid : ippECInvalidSignature;
      return ippStsNoErr;
   }
}

#endif /* _IPP_v50_ */
