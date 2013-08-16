/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    PRNG Functions
//
// Contents:
//    ippsPRNGen()
//    ippsPRNGen_BN()
//
//
//    Created: Mon 29-Nov-2004 13:04
//  Author(s): Sergey Kirillov
*/
#if !defined(_USE_NN_VERSION_)
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpshs.h"
#include "pcpprng.h"
#include "pcpciphertool.h"

/*
// G() function based on SHA1
//
// Parameters:
//    T           160 bit parameter
//    pHexStr     input hex string
//    hexStrLen   size of hex string (Ipp8u segnments)
//    xBNU        160 bit BNU result
//
// Note 1:
//    must to be hexStrLen <= 64 (512 bits)
*/
static
void SHA1_G(Ipp32u* xBNU, const Ipp32u* T, Ipp8u* pHexStr, int hexStrLen)
{
   /* pad HexString zeros */
   PaddBlock(0, pHexStr+hexStrLen, BITS2WORD8_SIZE(MAX_XKEY_SIZE)-hexStrLen);

   /* reset initial HASH value */
   xBNU[0] = T[0];
   xBNU[1] = T[1];
   xBNU[2] = T[2];
   xBNU[3] = T[3];
   xBNU[4] = T[4];

   /* SHA1 */
   UpdateSHA1(pHexStr, xBNU);

   /* swap back */
   SWAP(xBNU[0],xBNU[4]);
   SWAP(xBNU[1],xBNU[3]);
}

/*
// Returns bitsize of the bitstring has beed added
*/
int cpPRNGen(Ipp32u* pRand, int nBits, IppsPRNGState* pRnd)
{
   Ipp32u  Xj  [BITS2WORD32_SIZE(MAX_XKEY_SIZE)];
   Ipp32u  XVAL[BITS2WORD32_SIZE(MAX_XKEY_SIZE)+1];
   Ipp32u* pXVAL = IPP_ALIGNED_PTR(XVAL,8);

   Ipp8u  TXVAL[BITS2WORD8_SIZE(MAX_XKEY_SIZE)+7]; /* 8-bytes aligned buffer */
   Ipp8u* pTXVAL = IPP_ALIGNED_PTR(TXVAL,8);

   /* XKEY length in words */
   int xKeyLen = BITS2WORD32_SIZE(RAND_SEEDBITS(pRnd));
   /* XKEY length in bytes */
   int xKeySize= BITS2WORD8_SIZE(RAND_SEEDBITS(pRnd));
   /* XKEY word's mask */
   Ipp32u xKeyMsk = MAKEMASK32(RAND_SEEDBITS(pRnd));

   /* number of dwords to be generated */
   int genlen = BITS2WORD32_SIZE(nBits);

   /* number of 160-bit shrink to be generated */
   //int m = (genlen + 4) / 5;

   /* constant */
   Ipp32u one = 1;

   //int j;
   while(genlen) {
      Ipp32u carry;
      int sizeXj;
      int len;

      /* Step 1: XVAL=(Xkey+Xseed) mod 2^b */
      cpAdd_BNU(RAND_XKEY(pRnd), RAND_XAUGMENT(pRnd), pXVAL, xKeyLen, &carry);
      pXVAL[xKeyLen-1] &= xKeyMsk;

      /* Step 2: xj=G(t, XVAL) mod Q */
      BNU_OS(pTXVAL, xKeySize, pXVAL,xKeyLen);
      SHA1_G(Xj, RAND_T(pRnd), pTXVAL, xKeySize);

      if(0<=Cmp_BNU(Xj,5, RAND_Q(pRnd),5))
         cpMod_BNU(Xj, 5, RAND_Q(pRnd), 5, &sizeXj);
      else
         sizeXj = 5;
      FIX_BNU(Xj, sizeXj);
      ZEXPAND_BNU(Xj, sizeXj, BITS2WORD32_SIZE(MAX_XKEY_SIZE));

      /* Step 3: Xkey=(1+Xkey+Xj) mod 2^b */
      ADDC_BNU_I(RAND_XKEY(pRnd), xKeyLen, one);
      cpAdd_BNU(RAND_XKEY(pRnd), Xj, RAND_XKEY(pRnd), xKeyLen, &carry);
      //gres:03-aug-2006: pXVAL[xKeyLen-1] &= xKeyMsk;
      RAND_XKEY(pRnd)[xKeyLen-1] &= xKeyMsk;

      /* fill out result */
      len = genlen<BITS2WORD32_SIZE(SHA1_DIGEST_BITSIZE)? genlen : BITS2WORD32_SIZE(SHA1_DIGEST_BITSIZE);
      COPY_BNU(Xj, pRand, len);

      pRand  += len;
      genlen -= len;
   }

   return nBits;
}


/*F*
// Name: ippsPRNGen
//
// Purpose: Generates a pseudorandom bit sequence of the specified nBit length.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//                               NULL == pRand
//
//    ippStsContextMatchErr      illegal pRnd->idCtx
//
//    ippStsLengthErr            1 > nBits
//
//    ippStsNoErr                no error
//
// Parameters:
//    pRand    pointer to the buffer
//    nBits    number of bits be requested
//    pRndCtx  pointer to the context
*F*/
IPPFUN(IppStatus, ippsPRNGen,(Ipp32u* pRand, int nBits, void* pRnd))
{
   IppsPRNGState* pRndCtx;

   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRndCtx = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );
   IPP_BADARG_RET(!RAND_VALID_ID(pRndCtx), ippStsContextMatchErr);

   /* test target */
   IPP_BAD_PTR1_RET(pRand);

   /* test sizes */
   IPP_BADARG_RET(nBits< 1, ippStsLengthErr);

   {
      int rndSize = BITS2WORD32_SIZE(nBits);
      int rndMask = MAKEMASK32(nBits);

      cpPRNGen(pRand, nBits, pRndCtx);

      pRand[rndSize-1] &= rndMask;

      return ippStsNoErr;
   }
}


/*F*
// Name: ippsPRNGen_BN
//
// Purpose: Generates a pseudorandom bit sequence of the specified nBit length.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//                               NULL == pRandBN
//
//    ippStsContextMatchErr      illegal pRnd->idCtx
//                               illegal pRandBN->idCtx
//
//    ippStsLengthErr            1 > nBits
//                               nBits > BN_ROOM(pRandBN)
//
//    ippStsNoErr                no error
//
// Parameters:
//    pRandBN  pointer to the BN random
//    nBits    number of bits be requested
//    pRndCtx  pointer to the context
*F*/
IPPFUN(IppStatus, ippsPRNGen_BN,(IppsBigNumState* pRandBN, int nBits, void* pRnd))
{
   IppsPRNGState* pRndCtx;

   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRndCtx = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );
   IPP_BADARG_RET(!RAND_VALID_ID(pRndCtx), ippStsContextMatchErr);

   /* test random BN */
   IPP_BAD_PTR1_RET(pRandBN);
   pRandBN = (IppsBigNumState*)( IPP_ALIGNED_PTR(pRandBN, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pRandBN), ippStsContextMatchErr);

   /* test sizes */
   IPP_BADARG_RET(nBits< 1, ippStsLengthErr);
   IPP_BADARG_RET(nBits> BN_ROOM(pRandBN)*32, ippStsLengthErr);


   {
      Ipp32u* pRand = BN_NUMBER(pRandBN);
      int rndSize = BITS2WORD32_SIZE(nBits);
      int rndMask = MAKEMASK32(nBits);

      cpPRNGen(pRand, nBits, pRndCtx);
      pRand[rndSize-1] &= rndMask;

      FIX_BNU(pRand, rndSize);
      BN_SIZE(pRandBN) = rndSize;
      BN_SIGN(pRandBN) = IppsBigNumPOS;

      return ippStsNoErr;
   }
}

#endif /* _IPP_v50_ */
#endif

