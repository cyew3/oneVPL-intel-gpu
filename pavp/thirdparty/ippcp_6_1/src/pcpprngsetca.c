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
//    ippsPRNGSetModulus()
//    ippsPRNGSetSeed()
//    ippsPRNGSetAugment()
//    ippsPRNGSetH0()
//
//
//    Created: Mon 29-Nov-2004 12:23
//  Author(s): Sergey Kirillov
*/
#if !defined(_USE_NN_VERSION_)
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpprng.h"


/*F*
// Name: ippsPRNGSetModulus
//
// Purpose: Sets 160-bit modulus Q.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//                               NULL == pMod
//
//    ippStsContextMatchErr      illegal pRnd->idCtx
//                               illegal pMod->idCtx
//
//    ippStsBadArgErr            160 != bitsize(pMOd)
//
//    ippStsNoErr                no error
//
// Parameters:
//    pMod     pointer to the 160-bit modulus
//    pRnd     pointer to the context
*F*/
IPPFUN(IppStatus, ippsPRNGSetModulus, (const IppsBigNumState* pMod, IppsPRNGState* pRnd))
{
   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRnd = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );
   IPP_BADARG_RET(!RAND_VALID_ID(pRnd), ippStsContextMatchErr);

   /* test modulus */
   IPP_BAD_PTR1_RET(pMod);
   pMod = (IppsBigNumState*)( IPP_ALIGNED_PTR(pMod, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pMod), ippStsContextMatchErr);
   IPP_BADARG_RET(160 != BNU_BITSIZE(BN_NUMBER(pMod),BN_SIZE(pMod)), ippStsBadArgErr);

   COPY_BNU(BN_NUMBER(pMod), RAND_Q(pRnd), 5);
   return ippStsNoErr;
}


/*F*
// Name: ippsPRNGSetH0
//
// Purpose: Sets 160-bit parameter of G() function.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//                               NULL == pH0
//
//    ippStsContextMatchErr      illegal pRnd->idCtx
//                               illegal pH0->idCtx
//
//    ippStsNoErr                no error
//
// Parameters:
//    pH0      pointer to the parameter used into G() function
//    pRnd     pointer to the context
*F*/
IPPFUN(IppStatus, ippsPRNGSetH0,(const IppsBigNumState* pH0, IppsPRNGState* pRnd))
{
   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRnd = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );
   IPP_BADARG_RET(!RAND_VALID_ID(pRnd), ippStsContextMatchErr);

   /* test H0 */
   IPP_BAD_PTR1_RET(pH0);
   pH0 = (IppsBigNumState*)( IPP_ALIGNED_PTR(pH0, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pH0), ippStsContextMatchErr);

   ZEXPAND_COPY_BNU(BN_NUMBER(pH0), BN_SIZE(pH0), RAND_T(pRnd),5);
   return ippStsNoErr;
}


/*F*
// Name: ippsPRNGSetSeed
//
// Purpose: Sets the initial state with the SEED value
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//                               NULL == pSeed
//
//    ippStsContextMatchErr      illegal pRnd->idCtx
//                               illegal pSeed->idCtx
//
//    ippStsNoErr                no error
//
// Parameters:
//    pSeed    pointer to the SEED
//    pRnd     pointer to the context
*F*/
IPPFUN(IppStatus, ippsPRNGSetSeed, (const IppsBigNumState* pSeed, IppsPRNGState* pRnd))
{
   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRnd = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );
   IPP_BADARG_RET(!RAND_VALID_ID(pRnd), ippStsContextMatchErr);

   /* test seed */
   IPP_BAD_PTR1_RET(pSeed);
   pSeed = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSeed, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pSeed), ippStsContextMatchErr);

   {
      int sSize = BITS2WORD32_SIZE( RAND_SEEDBITS(pRnd) );
      //gres 05/14/05: Ipp32u mask = 0xFFFFFFFF >> (32 - RAND_SEEDBITS(pRnd)&0x1F);
      Ipp32u mask = MAKEMASK32(RAND_SEEDBITS(pRnd));
      int size = IPP_MIN(BN_SIZE(pSeed), sSize);

      ZEXPAND_COPY_BNU(BN_NUMBER(pSeed), size,
                       RAND_XKEY(pRnd), BITS2WORD32_SIZE(MAX_XKEY_SIZE));
      RAND_XKEY(pRnd)[sSize-1] &= mask;

      return ippStsNoErr;
   }
}


/*F*
// Name: ippsPRNGSetAugment
//
// Purpose: Sets the Entropy Augmentation
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//                               NULL == pAug
//
//    ippStsContextMatchErr      illegal pRnd->idCtx
//                               illegal pAug->idCtx
//
//    ippStsLengthErr            nBits < 1
//                               nBits > MAX_XKEY_SIZE
//    ippStsNoErr                no error
//
// Parameters:
//    pAug  pointer to the entropy eugmentation
//    pRnd  pointer to the context
*F*/
IPPFUN(IppStatus, ippsPRNGSetAugment, (const IppsBigNumState* pAug, IppsPRNGState* pRnd))
{
   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRnd = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );
   IPP_BADARG_RET(!RAND_VALID_ID(pRnd), ippStsContextMatchErr);

   /* test augmentation */
   IPP_BAD_PTR1_RET(pAug);
   pAug = (IppsBigNumState*)( IPP_ALIGNED_PTR(pAug, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pAug), ippStsContextMatchErr);

   {
      int sSize = BITS2WORD32_SIZE( RAND_SEEDBITS(pRnd) );
      //gres 05/14/05: Ipp32u mask = 0xFFFFFFFF >> (32 - RAND_SEEDBITS(pRnd)&0x1F);
      Ipp32u mask = MAKEMASK32(RAND_SEEDBITS(pRnd));
      int size = IPP_MIN(BN_SIZE(pAug), sSize);

      ZEXPAND_COPY_BNU(BN_NUMBER(pAug), size,
                       RAND_XAUGMENT(pRnd), BITS2WORD32_SIZE(MAX_XKEY_SIZE));
      RAND_XAUGMENT(pRnd)[sSize-1] &= mask;

      return ippStsNoErr;
   }
}

/*F*
// Name: ippsPRNGGetSeed
//
// Purpose: Get current SEED value from the state
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//                               NULL == pSeed
//
//    ippStsContextMatchErr      illegal pRnd->idCtx
//                               illegal pSeed->idCtx
//    ippStsOutOfRangeErr        lengtrh of the actual SEED > length SEED destination
//
//    ippStsNoErr                no error
//
// Parameters:
//    pRnd     pointer to the context
//    pSeed    pointer to the SEED
*F*/
IPPFUN(IppStatus, ippsPRNGGetSeed, (const IppsPRNGState* pRnd, IppsBigNumState* pSeed))
{
   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRnd = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );
   IPP_BADARG_RET(!RAND_VALID_ID(pRnd), ippStsContextMatchErr);

   /* test seed */
   IPP_BAD_PTR1_RET(pSeed);
   pSeed = (IppsBigNumState*)( IPP_ALIGNED_PTR(pSeed, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pSeed), ippStsContextMatchErr);
   
   return ippsSet_BN(IppsBigNumPOS,
                     BITS2WORD32_SIZE(RAND_SEEDBITS(pRnd)),
                     RAND_XKEY(pRnd),
                     pSeed);
}

#endif /* _IPP_v50_ */
#endif
