/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Prime Numbrer Generator
//
// Contents:
//    ippsPrimeGetSize()
//    ippsPrimeInit()
//
//
//    Created: Sun 05-Dec-2004 15:15
//  Author(s): Sergey Kirillov
*/
#if !defined(_USE_NN_VERSION_)
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpprimeg.h"


/*F*
// Name: ippsPrimeGetSize
//
// Purpose: Returns size of Prime Number Generator context (bytes).
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pSize
//    ippStsLengthErr            1 > nBits
//    ippStsNoErr                no error
//
// Parameters:
//    nBits    max length of a prime number
//    pSize    pointer to the size of internal context
*F*/
IPPFUN(IppStatus, ippsPrimeGetSize, (int nBits, int* pSize))
{
   IPP_BAD_PTR1_RET(pSize);
   IPP_BADARG_RET(nBits<1, ippStsLengthErr);

   {
      int bnSize;
      int montSize;

      ippsBigNumGetSize(BITS2WORD32_SIZE(nBits), &bnSize);
      ippsMontGetSize(IppsBinaryMethod, BITS2WORD32_SIZE(nBits), &montSize);

      *pSize = sizeof(IppsPrimeState)
              +bnSize
              +bnSize
              +montSize
              +PRIME_ALIGNMENT-1;

      return ippStsNoErr;
   }
}


/*F*
// Name: ippsPrimeInit
//
// Purpose: Initializes Prime Number Generator context
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pCtx
//    ippStsLengthErr            1 > nBits
//    ippStsNoErr                no error
//
// Parameters:
//    nBits    max length of a prime number
//    pCtx     pointer to the context to be initialized
*F*/
IPPFUN(IppStatus, ippsPrimeInit, (int nBits, IppsPrimeState* pCtx))
{
   IPP_BAD_PTR1_RET(pCtx);
   IPP_BADARG_RET(nBits<1, ippStsLengthErr);

   /* use aligned PRNG context */
   pCtx = (IppsPrimeState*)( IPP_ALIGNED_PTR(pCtx, PRIME_ALIGNMENT) );

   {
      int nWords = BITS2WORD32_SIZE(nBits);

      int bnSize;

      Ipp8u* ptr = (Ipp8u*)pCtx;

      ippsBigNumGetSize(nWords, &bnSize);

      PRIME_ID(pCtx) = idCtxPrimeNumber;
      //PRIME_BITSIZE(pCtx) = 0;

      ptr += sizeof(IppsPrimeState);
      PRIME_NUMBER(pCtx) = (IppsBigNumState*)( IPP_ALIGNED_PTR((ptr), (BN_ALIGN_SIZE)) );
      ippsBigNumInit(nWords, PRIME_NUMBER(pCtx));

      ptr += bnSize;
      PRIME_TEMP(pCtx) = (IppsBigNumState*)( IPP_ALIGNED_PTR((ptr), (BN_ALIGN_SIZE)) );
      ippsBigNumInit(nWords, PRIME_TEMP(pCtx));

      ptr += bnSize;
      PRIME_MONT(pCtx)   = (IppsMontState*)( IPP_ALIGNED_PTR((ptr), (MONT_ALIGN_SIZE)) );
      ippsMontInit(IppsBinaryMethod, nWords, PRIME_MONT(pCtx));

      return ippStsNoErr;
   }
}

#endif /* _IPP_v50_ */
#endif
