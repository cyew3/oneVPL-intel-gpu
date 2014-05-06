/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Initialization of Rijndael
//
// Contents:
//    ippsRijndael128GetSpecSize()
//    ippsRijndael128Init()
//
//
//    Created: Mon 20-May-2002 07:27
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcprij.h"
#include "pcprijtables.h"
#include "pcprij128safe.h"

/*
// number of rounds (use [NK] for access)
*/
#if !defined(_IPP_TRS)
static int rij128nRounds[3] = {NR128_128, NR128_192, NR128_256};
#endif

/*
// number of keys (estimation only!)  (use [NK] for access)
//
// accurate number of keys necassary for encrypt/decrypt are:
//    nKeys = NB * (NR+1)
//       where NB - data block size (32-bit words)
//             NR - number of rounds (depend on NB and keyLen)
//
// but the estimation
//    estnKeys = (NK*n) >= nKeys
// or
//    estnKeys = ( (NB*(NR+1) + (NK-1)) / NK) * NK
//       where NK - key length (words)
//             NB - data block size (word)
//             NR - number of rounds (depend on NB and keyLen)
//             nKeys - accurate numner of keys
// is more convinient when calculates key extension
*/
#if !defined(_IPP_TRS)
static int rij128nKeys[3] = {44,  54,  64 };
#endif

/*
// helper for nRounds[] and estnKeys[] access
// note: x is length in 32-bits words
*/
__INLINE int rij_index(int x)
{ return (x-NB(128))>>1; }


/*F*
//    Name: ippsRijndael128GetSize
//
// Purpose: Returns size of RIJ spec (bytes).
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSzie == NULL
//    ippStsNoErr             no errors
//
// Parameters:
//    pSize       pointer RIJ spec size
//
*F*/
IPPFUN(IppStatus, ippsRijndael128GetSize,(int* pSize))
{
   /* test size's pointer */
   IPP_BAD_PTR1_RET(pSize);

   *pSize = sizeof(IppsRijndael128Spec)
           +(RIJ_ALIGNMENT-1);

   return ippStsNoErr;
}


/*F*
//    Name: ippsRijndael128Init
//          ippsSafeRijndael128Init
//
// Purpose: Init RIJ spec for future usage.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pKey == NULL
//                            pCtx == NULL
//    ippStsLengthErr         keyLen != IppsRijndaelKey128
//                            keyLen != IppsRijndaelKey192
//                            keyLen != IppsRijndaelKey256
//
// Parameters:
//    pKey        security key
//    pCtx        pointer RIJ spec
//
*F*/
IPPFUN(IppStatus, ippsRijndael128Init,(const Ipp8u* pKey, IppsRijndaelKeyLength keyLen, IppsRijndael128Spec* pCtx))
{
   int keyWords;
   int nExpKeys;
   int nRounds;

   /* test RIJ keyLen */
   IPP_BADARG_RET(( (keyLen!=IppsRijndaelKey128) &&
                    (keyLen!=IppsRijndaelKey192) &&
                    (keyLen!=IppsRijndaelKey256)), ippStsLengthErr);
   /* test key's & spec's pointers */
   IPP_BAD_PTR2_RET(pKey, pCtx);
   /* use aligned Rijndael context */
   pCtx = (IppsRijndael128Spec*)( IPP_ALIGNED_PTR(pCtx, RIJ_ALIGNMENT) );

   {
      #if defined(_IPP_TRS)
      int rij128nRounds[3] = {NR128_128, NR128_192, NR128_256};
      int rij128nKeys[3] = {44,  54,  64 };
      #endif

      keyWords = NK(keyLen);
      nExpKeys = rij128nKeys  [ rij_index(keyWords) ];
      nRounds  = rij128nRounds[ rij_index(keyWords) ];

      /* init spec */
      RIJ_ID(pCtx) = idCtxRijndael;
      RIJ_NB(pCtx) = NB(128);
      RIJ_NK(pCtx) = keyWords;
      RIJ_NR(pCtx) = nRounds;
      RIJ_ENCODER(pCtx) = Encrypt_RIJ128; /* fast encoder */
      RIJ_DECODER(pCtx) = Decrypt_RIJ128; /* fast decoder */

      /* set key expansion */
      ExpandRijndaelKey(pKey, keyWords, NB(128), nRounds, nExpKeys,
                     RIJ_EKEYS(pCtx),
                     RIJ_DKEYS(pCtx));

      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsSafeRijndael128Init,(const Ipp8u* pKey, IppsRijndaelKeyLength keyLen, IppsRijndael128Spec* pCtx))
{
   #if defined(_IPP_TRS)
      UNREFERENCED_PARAMETER(pKey);
      UNREFERENCED_PARAMETER(keyLen);
      UNREFERENCED_PARAMETER(pCtx);
      return ippStsNotSupportedModeErr;

   #else
   IppStatus sts;
   sts = ippsRijndael128Init(pKey, keyLen, pCtx);

   /* use aligned Rijndael context */
   pCtx = (IppsRijndael128Spec*)( IPP_ALIGNED_PTR(pCtx, RIJ_ALIGNMENT) );

   if(ippStsNoErr==sts) {
      int nr;
      Ipp8u* pEnc_key = (Ipp8u*)(RIJ_EKEYS(pCtx));
      Ipp8u* pDec_key = (Ipp8u*)(RIJ_DKEYS(pCtx));

      RIJ_ENCODER(pCtx) = SafeEncrypt_RIJ128; /* safe encoder */
      RIJ_DECODER(pCtx) = SafeDecrypt_RIJ128; /* safe decoder */

      /* update key material */
      for(nr=0; nr<(1+RIJ_NR(pCtx)); nr++) {
         TransformNative2Composite(pEnc_key+16*nr, pEnc_key+16*nr, 16);
         TransformNative2Composite(pDec_key+16*nr, pDec_key+16*nr, 16);
      }
   }

   return sts;
   #endif
}
