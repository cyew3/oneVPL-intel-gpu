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
//    ippsECCPSetKeyPair()
//
//
//    Created: Wed 09-Jul-2003 16:09
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
//    Name: ippsECCPSetKeyPair
//
// Purpose: Generate (private,public) Key Pair
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pECC
//                               NULL == pPrivate
//                               NULL == pPublic
//
//    ippStsContextMatchErr      illegal pECC->idCtx
//                               illegal pPrivate->idCtx
//                               illegal pPublic->idCtx
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pPrivate    pointer to the private key
//    pPublic     pointer to the public  key
//    regular     flag regular/ephemeral keys
//    pECC        pointer to the ECCP context
//
*F*/
IPPFUN(IppStatus, ippsECCPSetKeyPair, (const IppsBigNumState* pPrivate, const IppsECCPPointState* pPublic,
                                       IppBool regular,
                                       IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   {
      IppsBigNumState*  targetPrivate;
      IppsECCPPointState* targetPublic;

      if( regular ) {
         targetPrivate = ECP_PRIVATE(pECC);
         targetPublic  = ECP_PUBLIC(pECC);
      }
      else {
         targetPrivate = ECP_PRIVATE_E(pECC);
         targetPublic  = ECP_PUBLIC_E(pECC);
      }

      /* set up private key request */
      if( pPrivate ) {
         pPrivate = (IppsBigNumState*)( IPP_ALIGNED_PTR(pPrivate, ALIGN_VAL) );
         IPP_BADARG_RET(!BN_VALID_ID(pPrivate), ippStsContextMatchErr);
         ippsSet_BN(IppsBigNumPOS, BN_SIZE(pPrivate), BN_NUMBER(pPrivate), targetPrivate);
      }

      /* set up public  key request */
      if( pPublic ) {
         pPublic = (IppsECCPPointState*)( IPP_ALIGNED_PTR(pPublic, ALIGN_VAL) );
         IPP_BADARG_RET(!ECP_POINT_VALID_ID(pPublic), ippStsContextMatchErr);
         //ippsSet_BN(IppsBigNumPOS, BN_SIZE( ECP_POINT_X(pPublic) ), BN_NUMBER( ECP_POINT_X(pPublic) ), ECP_POINT_X(targetPublic));
         //ippsSet_BN(IppsBigNumPOS, BN_SIZE( ECP_POINT_Y(pPublic) ), BN_NUMBER( ECP_POINT_Y(pPublic) ), ECP_POINT_Y(targetPublic));
         //ippsSet_BN(IppsBigNumPOS, BN_SIZE( ECP_POINT_Z(pPublic) ), BN_NUMBER( ECP_POINT_Z(pPublic) ), ECP_POINT_Z(targetPublic));
         //ECP_POINT_AFFINE(targetPublic) = ECP_POINT_AFFINE(pPublic);
         #if !defined(_IPP_TRS)
         ECP_METHOD(pECC)->GetPointAffine(ECP_POINT_X(targetPublic), ECP_POINT_Y(targetPublic), pPublic, pECC, ECP_BNCTX(pECC));
         ECP_METHOD(pECC)->SetPointAffine(ECP_POINT_X(targetPublic), ECP_POINT_Y(targetPublic), targetPublic, pECC);
         #else
         ECCP_GetPointAffine(ECP_POINT_X(targetPublic), ECP_POINT_Y(targetPublic), pPublic, pECC, ECP_BNCTX(pECC));
         ECCP_SetPointAffine(ECP_POINT_X(targetPublic), ECP_POINT_Y(targetPublic), targetPublic, pECC);
         #endif
      }

      return ippStsNoErr;
   }
}

#endif /* _IPP_v50_ */
