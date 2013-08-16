/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    SHA256 based HMAC Functions
//
// Contents:
//    ippsHMACSHA256GetSize()
//    ippsHMACSHA256Init()
//    ippsHMACSHA1Duplicate()
//    ippsHMACSHA256Update()
//    ippsHMACSHA256GetTag()
//    ippsHMACSHA256Final()
//    ippsHMACSHA256MessageDigest()
//
//
//    Created: Mon 03-Jun-2002 23:05
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcphmac.h"
#include "pcpciphertool.h"


/*F*
//    Name: ippsHMACSHA256GetSize
//
// Purpose: Returns size of HMAC (SHA256) state (bytes).
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSzie == NULL
//    ippStsNoErr             no errors
//
// Parameters:
//    pSize       pointer to the HMAC state size
//
*F*/
IPPFUN(IppStatus, ippsHMACSHA256GetSize,(int* pSize))
{
   /* test size's pointer */
   IPP_BAD_PTR1_RET(pSize);

   *pSize = sizeof(IppsHMACSHA256State)
           +(HMACSHA256_ALIGNMENT-1);

   return ippStsNoErr;
}


/*F*
//    Name: ippsHMACSHA256Init
//
// Purpose: Init HMAC (SHA256) state.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pKey == NULL
//                            pState == NULL
//    ippStsLengthErr         keyLen <0
//    ippStsNoErr             no errors
//
// Parameters:
//    pKey        pointer to the secret key
//    keyLen      length (bytes) of the secret key
//    pState      pointer to the HMAC state
//
*F*/
IPPFUN(IppStatus, ippsHMACSHA256Init,(const Ipp8u* pKey, int keyLen,
                                      IppsHMACSHA256State* pState))
{
   int n;

   /* test pState pointer */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned HMAC context */
   pState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(pState, HMACSHA256_ALIGNMENT) );

   /* test key pointer and key length */
   IPP_BAD_PTR1_RET(pKey);
   IPP_BADARG_RET((0>keyLen), ippStsLengthErr);

   /* set state ID */
   HMAC_ID(pState) = idCtxHMAC;

   /* set SHS pointer and init SHA256 */
   HASH_STATE(pState) = (IppsSHA256State*)( IPP_ALIGNED_PTR(pState->shsState, SHA256_ALIGNMENT) );
   ippsSHA256Init( HASH_STATE(pState) );

   /* test secret key length and reset by SHS(key) if keyLen>SHS_BSIZE() */
   if(keyLen>MBS_SHA256) {
      ippsSHA256Update(pKey, keyLen, HASH_STATE(pState) );
      ippsSHA256Final(SHS_BUFF( HASH_STATE(pState) ), HASH_STATE(pState));
      pKey = SHS_BUFF( HASH_STATE(pState) );
      keyLen = sizeof(DigestSHA256);
   }

   /* XOR-ing key */
   for(n=0; n<keyLen; n++) {
      pState->ipadKey[n] = (Ipp8u)( pKey[n] ^ IPAD );
      pState->opadKey[n] = (Ipp8u)( pKey[n] ^ OPAD );
   }
   for(; n<MBS_SHA256; n++) {
      pState->ipadKey[n] = (Ipp8u)IPAD;
      pState->opadKey[n] = (Ipp8u)OPAD;
   }

   /* start 1-step */
   ippsSHA256Update(pState->ipadKey, MBS_SHA256, HASH_STATE(pState));

   return ippStsNoErr;
}


/*F*
//    Name: ippsHMACSHA256Duplicate
//
// Purpose: Clone HMAC (SHA256) state.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSrcState == NULL
//                            pDstState == NULL
//    ippStsContextMatchErr   pSrcState->idCtx != idCtxHMAC
//                            pDstState->idCtx != idCtxHMAC
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrcState   pointer to the source HMAC state
//    pDstState   pointer to the target HMAC state
//
// Note:
//    pDstState may to be uninitialized by ippsHMACSHA256Init()
//
*F*/
IPPFUN(IppStatus, ippsHMACSHA256Duplicate,(const IppsHMACSHA256State* pSrcState, IppsHMACSHA256State* pDstState))
{
   /* test state pointers */
   IPP_BAD_PTR2_RET(pSrcState, pDstState);
   /* use aligned context */
   pSrcState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(pSrcState, HMACSHA256_ALIGNMENT) );
   pDstState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(pDstState, HMACSHA256_ALIGNMENT) );
   /* test states ID */
   IPP_BADARG_RET(idCtxHMAC !=HMAC_ID(pSrcState), ippStsContextMatchErr);
   //IPP_BADARG_RET(idCtxHMAC !=HMAC_ID(pDstState), ippStsContextMatchErr);

   /* init */
   HMAC_ID(pDstState) = idCtxHMAC;
   HASH_STATE(pDstState) = (IppsSHA256State*)( IPP_ALIGNED_PTR(pDstState->shsState, SHA256_ALIGNMENT) );

   /* copy HMAC state */
   CopyBlock(pSrcState->ipadKey, pDstState->ipadKey, MBS_SHA256);
   CopyBlock(pSrcState->opadKey, pDstState->opadKey, MBS_SHA256);
   CopyBlock(HASH_STATE(pSrcState), HASH_STATE(pDstState), sizeof(IppsSHA256State));

   return ippStsNoErr;
}


/*F*
//    Name: ippsHMACSHA256Update
//
// Purpose: Updates intermadiate MAC based on input stream.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSrc == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxHMAC
//    ippStsLengthErr         len <0
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrc        pointer to the input stream
//    len         input stream length
//    pState      pointer to the HMAC state
//
*F*/
IPPFUN(IppStatus, ippsHMACSHA256Update,(const Ipp8u* pSrc, int len, IppsHMACSHA256State* pState))
{
   /* test state pointers */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned HMAC context */
   pState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(pState, HMACSHA256_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxHMAC !=HMAC_ID(pState), ippStsContextMatchErr);
   /* test input length */
   IPP_BADARG_RET((len<0), ippStsLengthErr);
   /* test source pointer */
   IPP_BADARG_RET((len && !pSrc), ippStsNullPtrErr);

   if(len)
      return ippsSHA256Update(pSrc, len, HASH_STATE(pState));
   else
      return ippStsNoErr;
}


/*F*
//    Name: ippsHMACSHA256Final
//
// Purpose: Stop message digesting and return digest.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pMD == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxHMAC
//    ippStsLengthErr         sizeof(DigestSHA256) < mdLen <1
//    ippStsNoErr             no errors
//
// Parameters:
//    pMD         address of the output digest
//    pState      pointer to the HMAC state
//
*F*/
IPPFUN(IppStatus, ippsHMACSHA256Final,(Ipp8u* pMD, int mdLen, IppsHMACSHA256State* pState))
{
   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned HMAC context */
   pState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(pState, HMACSHA256_ALIGNMENT) );

   IPP_BADARG_RET(idCtxHMAC !=HMAC_ID(pState), ippStsContextMatchErr);
   /* test MD pointer */
   IPP_BAD_PTR1_RET(pMD);
   IPP_BADARG_RET((mdLen<1)||(sizeof(DigestSHA256)<mdLen), ippStsLengthErr);

   {
      DigestSHA256 md;
      IppsSHA256State* pSHS = HASH_STATE(pState);

      /* finalize 1-st step */
      ippsSHA256Final((Ipp8u*)md, pSHS);

      /* perform outer */
      ippsSHA256Update(pState->opadKey, MBS_SHA256, pSHS);
      ippsSHA256Update((Ipp8u*)md, sizeof(DigestSHA256), pSHS);
      ippsSHA256Final ((Ipp8u*)md, pSHS);

      /* return truncated digest */
      CopyBlock(md, pMD, mdLen);

      /* re-start HMAC */
      ippsSHA256Update(pState->ipadKey, MBS_SHA256, HASH_STATE(pState));

      return ippStsNoErr;
   }
}


/*F*
//    Name: ippsHMACSHA256GetTag
//
// Purpose: Compute digest with further digesting ability.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pMD == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxHMAC
//    ippStsLengthErr         sizeof(DigestSHA256) < mdLen <1
//    ippStsNoErr             no errors
//
// Parameters:
//    pMD         address of the output digest
//    mdLen       length of the digest
//    pState      pointer to the HMAC state
//
*F*/
IPPFUN(IppStatus, ippsHMACSHA256GetTag,(Ipp8u* pMD, int mdLen, const IppsHMACSHA256State* pState))
{
   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned HMAC context */
   pState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(pState, HMACSHA256_ALIGNMENT) );

   IPP_BADARG_RET(idCtxHMAC !=HMAC_ID(pState), ippStsContextMatchErr);
   /* test MD pointer */
   IPP_BAD_PTR1_RET(pMD);
   IPP_BADARG_RET((mdLen<1)||(sizeof(DigestSHA256)<mdLen), ippStsLengthErr);

   {
      Ipp8u blob[sizeof(IppsHMACSHA256State)+HMACSHA256_ALIGNMENT-1];
      IppsHMACSHA256State* pTmpState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(&blob, HMACSHA256_ALIGNMENT) );

      CopyBlock(pState, pTmpState, sizeof(IppsHMACSHA256State));
      HASH_STATE(pTmpState) = (IppsSHA256State*)( IPP_ALIGNED_PTR(pTmpState->shsState, SHA256_ALIGNMENT) );
      CopyBlock(HASH_STATE(pState), HASH_STATE(pTmpState), sizeof(IppsSHA256State));

      ippsHMACSHA256Final(pMD, mdLen, pTmpState);

      return ippStsNoErr;
   }
}


/*F*
//    Name: ippsHMACSHA256MessageDigest
//
// Purpose: MAC (SHA256) of the whole message.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pMsg == NULL
//                            pKey == NULL
//                            pMD == NULL
//    ippStsLengthErr         msgLen <0
//                            keyLen <0
//                            sizeof(DigestSHA256) < mdLen <1
//    ippStsNoErr             no errors
//
// Parameters:
//    pMsg        pointer to the input message
//    msgLen      input message length
//    pKey        pointer to the secret key
//    keyLen      secret key length
//    pMD         pointer to message digest
//    mdLen       MD length
//
*F*/
IPPFUN(IppStatus, ippsHMACSHA256MessageDigest,(const Ipp8u* pMsg, int msgLen,
                                               const Ipp8u* pKey, int keyLen,
                                               Ipp8u* pMD, int mdLen))
{
   /* test MD pointer and length */
   IPP_BAD_PTR1_RET(pMD);
   IPP_BADARG_RET((mdLen<1)||(sizeof(DigestSHA256)<mdLen), ippStsLengthErr);
   /* test secret key pointer and length */
   IPP_BAD_PTR1_RET(pKey);
   IPP_BADARG_RET((keyLen<0), ippStsLengthErr);
   /* test input message pointer and length */
   IPP_BADARG_RET((msgLen<0), ippStsLengthErr);
   IPP_BADARG_RET((msgLen && !pMsg), ippStsNullPtrErr);

   {
      Ipp8u blob[sizeof(IppsHMACSHA256State)+HMACSHA256_ALIGNMENT-1];
      IppsHMACSHA256State* pState = (IppsHMACSHA256State*)( IPP_ALIGNED_PTR(&blob, HMACSHA256_ALIGNMENT) );

      ippsHMACSHA256Init (pKey,keyLen, pState);
      if(msgLen)
         ippsSHA256Update(pMsg,msgLen, HASH_STATE(pState));
      ippsHMACSHA256Final(pMD,mdLen,   pState);

      return ippStsNoErr;
   }
}
