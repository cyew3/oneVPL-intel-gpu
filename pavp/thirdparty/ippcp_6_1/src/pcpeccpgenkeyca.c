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
//    ippsECCPGenKeyPair()
//
//
//    Created: Wed 09-Jul-2003 10:25
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccppoint.h"
#include "pcpeccpmethod.h"
#include "pcpeccpmethodcom.h"


/*F*
//    Name: ippsECCPGenKeyPair
//
// Purpose: Generate (private,public) Key Pair
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
//    ippStsNoErr             no errors
//
// Parameters:
//    pPrivate    pointer to the resultant private key
//    pPublic     pointer to the resultant public  key
//    pECC        pointer to the ECCP context
//
*F*/
#if defined(_USE_NN_VERSION_)
IPPFUN(IppStatus, ippsECCPGenKeyPair, (IppsBigNumState* pPrivate, IppsECCPPointState* pPublic,
                                       IppsECCPState* pECC))
{
   /* test pECC */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test private/public keys */
   IPP_BAD_PTR2_RET(pPrivate,pPublic);
   pPrivate = (IppsBigNumState*)( IPP_ALIGNED_PTR(pPrivate, ALIGN_VAL) );
   pPublic = (IppsECCPPointState*)( IPP_ALIGNED_PTR(pPublic, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pPrivate), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_ROOM(pPrivate)<ECP_ORDSIZE(pECC)), ippStsSizeErr);
   IPP_BADARG_RET(!ECP_POINT_VALID_ID(pPublic), ippStsContextMatchErr);

   {
      int curLength;
      int reqBitLen = ECP_ORDBITS(pECC);

      Ipp32u* pOrder = BN_NUMBER(MONT_MODULO(ECP_RMONT(pECC)));
      int orderSize  = BN_SIZE(MONT_MODULO(ECP_RMONT(pECC)));

      Ipp32u* buffer = BN_BUFFER(pPrivate);
      int bufSize;

      /* generate private key */
      do {
         ippsPRNGAdd(ECP_RANDCNT(pECC), reqBitLen, ECP_RAND(pECC));
         ippsPRNGGetRand(&curLength, buffer, ECP_RAND(pECC));

         /* mask hight bits */
         bufSize = BITS2WORD32_SIZE(reqBitLen);
         buffer[bufSize-1] &= ECP_RANDMASK(pECC);

         /* test private */
      } while( (0 == Tst_BNU(buffer, bufSize)) ||
               (0 <= Cmp_BNU(buffer,bufSize, pOrder,orderSize)) );

      /* setup private key */
      ippsSet_BN(IppsBigNumPOS, bufSize,buffer, pPrivate);

      /* calculate public key */
      ECP_METHOD(pECC)->MulPoint(ECP_GENC(pECC), pPrivate, pPublic, pECC, ECP_BNCTX(pECC));

      return ippStsNoErr;
   }
}

#else
IPPFUN(IppStatus, ippsECCPGenKeyPair, (IppsBigNumState* pPrivate, IppsECCPPointState* pPublic,
                                       IppsECCPState* pECC,
                                       IppBitSupplier rndFunc, void* pRndParam))
{
   IPP_BAD_PTR2_RET(pECC, rndFunc);

   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );
   /* test ID */
   IPP_BADARG_RET(!ECP_VALID_ID(pECC), ippStsContextMatchErr);

   /* test private/public keys */
   IPP_BAD_PTR2_RET(pPrivate,pPublic);
   pPrivate = (IppsBigNumState*)( IPP_ALIGNED_PTR(pPrivate, ALIGN_VAL) );
   pPublic = (IppsECCPPointState*)( IPP_ALIGNED_PTR(pPublic, ALIGN_VAL) );
   IPP_BADARG_RET(!BN_VALID_ID(pPrivate), ippStsContextMatchErr);
   IPP_BADARG_RET(((int)BN_ROOM(pPrivate)<ECP_ORDSIZE(pECC)), ippStsSizeErr);
   IPP_BADARG_RET(!ECP_POINT_VALID_ID(pPublic), ippStsContextMatchErr);

   {
      /*
      // generate random private key X:  0 < X < R
      */
      int reqBitLen = ECP_ORDBITS(pECC);

      Ipp32u* pOrder = BN_NUMBER(MNT_MODULO(ECP_RMONT(pECC)));
      int orderSize  = BN_SIZE(MNT_MODULO(ECP_RMONT(pECC)));

      int xSize;
      Ipp32u* pX = BN_NUMBER(pPrivate);
      //gres 05/14/05: Ipp32u xMask = 0xFFFFFFFF >>(32 - reqBitLen&0x1F);
      Ipp32u xMask = MAKEMASK32(reqBitLen);

      BN_SIGN(pPrivate) = IppsBigNumPOS;
      do {
         xSize = BITS2WORD32_SIZE(reqBitLen);
         rndFunc(pX, reqBitLen, pRndParam);
         pX[xSize-1] &= xMask;
         FIX_BNU(pX, xSize);
         BN_SIZE(pPrivate) = xSize;
      } while( (0 == Tst_BNU(pX,xSize)) ||
               (0 <= Cmp_BNU(pX,xSize, pOrder,orderSize)) );

      /* calculate public key */
      #if defined(_IPP_TRS)
      ECCP_MulPoint(ECP_GENC(pECC), pPrivate, pPublic, pECC, ECP_BNCTX(pECC));
      #else
      ECP_METHOD(pECC)->MulPoint(ECP_GENC(pECC), pPrivate, pPublic, pECC, ECP_BNCTX(pECC));
      #endif

      return ippStsNoErr;
   }
}
#endif

#endif /* _IPP_v50_ */
