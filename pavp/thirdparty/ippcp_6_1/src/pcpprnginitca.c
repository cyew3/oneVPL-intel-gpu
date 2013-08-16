/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    PRNG Functions
//
// Contents:
//    ippsPRNGGetSize()
//    ippsPRNGInit()
//
//
//    Created: Mon 29-Nov-2004 11:46
//  Author(s): Sergey Kirillov
*/
#if !defined(_USE_NN_VERSION_)
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpprng.h"


/*F*
//    Name: ippsPRNGGetSize
//
// Purpose: Returns size of PRNG context (bytes).
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pSize
//
//    ippStsNoErr                no error
//
// Parameters:
//    pSize       pointer to the size of internal context
*F*/
IPPFUN(IppStatus, ippsPRNGGetSize, (int* pSize))
{
   IPP_BAD_PTR1_RET(pSize);

   *pSize = sizeof(IppsPRNGState)
           +PRNG_ALIGNMENT-1;
   return ippStsNoErr;
}


/*F*
// Name: ippsPRNGInit
//
// Purpose: Initializes PRNG context
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pRnd
//
//    ippStsLengthErr            seedBits < 1
//                               seedBits < MAX_XKEY_SIZE
//                               seedBits%8 !=0
//
//    ippStsNoErr                no error
//
// Parameters:
//    seedBits    seed bitsize
//    pRnd        pointer to the context to be initialized
*F*/
IPPFUN(IppStatus, ippsPRNGInit, (int seedBits, IppsPRNGState* pRnd))
{
   DigestSHA1 H0_SHA1 = FIPS_180_SHA1_HASH0;

   /* test PRNG context */
   IPP_BAD_PTR1_RET(pRnd);
   pRnd = (IppsPRNGState*)( IPP_ALIGNED_PTR(pRnd, PRNG_ALIGNMENT) );

   /* test sizes */
   IPP_BADARG_RET((1>seedBits) || (seedBits>MAX_XKEY_SIZE) ||(seedBits&7), ippStsLengthErr);

   /*
   // init context
   */
   RAND_ID(pRnd) = idCtxPRNG;
   RAND_SEEDBITS(pRnd) = seedBits;

   /* default Q parameter */
   RAND_Q(pRnd)[0] = 0xFFFFFFFF;
   RAND_Q(pRnd)[1] = 0xFFFFFFFF;
   RAND_Q(pRnd)[2] = 0xFFFFFFFF;
   RAND_Q(pRnd)[3] = 0xFFFFFFFF;
   RAND_Q(pRnd)[4] = 0xFFFFFFFF;

   /* default T parameter */
   RAND_T(pRnd)[0] = H0_SHA1[0];
   RAND_T(pRnd)[1] = H0_SHA1[1];
   RAND_T(pRnd)[2] = H0_SHA1[2];
   RAND_T(pRnd)[3] = H0_SHA1[3];
   RAND_T(pRnd)[4] = H0_SHA1[4];

   /* default augmentation */
   ZEXPAND_BNU(RAND_XAUGMENT(pRnd), 0, BITS2WORD32_SIZE(MAX_XKEY_SIZE));
   /* default seed */
   ZEXPAND_BNU(RAND_XKEY(pRnd), 0, BITS2WORD32_SIZE(MAX_XKEY_SIZE));

   return ippStsNoErr;
}

#endif /* _IPP_v50_ */
#endif

