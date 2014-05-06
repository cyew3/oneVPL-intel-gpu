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
//    ippsECCPPublicKey()
//
//
//    Created: Fri 18-Jul-2003 15:42
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccppoint.h"
#include "pcpeccpmethod.h"
#include "pcpeccpmethodcom.h"


/*F*
//    Name: ippsECCPPublicKey
//
// Purpose: Calculate Public Key
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pECC
//                            NULL == pPrivate
//                            NULL == pPublic
//
//    ippStsContextMatchErr   illegal pECC->idCtx
//                            illegal pPrivate->idCtx
//                            illegal pPublic->idCtx
//
//    ippStsIvalidPrivateKey  !(0 < pPrivate < order)
//
//    ippStsNoErr             no errors
//
// Parameters:
//    pPrivate    pointer to the private key
//    pPublic     pointer to the resultant public key
//    pECC        pointer to the ECCP context
//
*F*/
IPPFUN(IppStatus, ippsECCPPublicKey, (const IppsBigNumState* pPrivate,
                                      IppsECCPPointState* pPublic,
                                      IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test public key */
   IPP_BAD_PTR1_RET(pPublic);
   pPublic = (IppsECCPPointState*)( IPP_ALIGNED_PTR(pPublic, ALIGN_VAL) );
   IPP_BADARG_RET(!ECP_POINT_VALID_ID(pPublic), ippStsContextMatchErr);

   /* test private keys */
   IPP_BAD_PTR1_RET(pPrivate);
   pPrivate = (IppsBigNumState*)( IPP_ALIGNED_PTR(pPrivate, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pPrivate), ippStsContextMatchErr);
   IPP_BADARG_RET(!((0<Tst_BN(pPrivate)) &&
                    (0>Cmp_BN(pPrivate, MNT_MODULO(ECP_RMONT(pECC))))
                   ), ippStsIvalidPrivateKey);

   /* calculates public key */
   #if !defined(_IPP_TRS)
   ECP_METHOD(pECC)->MulPoint(ECP_GENC(pECC), pPrivate, pPublic, pECC, ECP_BNCTX(pECC));
   #else
   ECCP_MulPoint(ECP_GENC(pECC), pPrivate, pPublic, pECC, ECP_BNCTX(pECC));
   #endif

   return ippStsNoErr;
}

#endif /* _IPP_v50_ */
