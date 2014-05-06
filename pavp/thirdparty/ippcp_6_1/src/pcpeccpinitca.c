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
//    EC over Prime Finite Field (initialization)
//
// Contents:
//    ippsECCPGetSize()
//    ippsECCPInit()
//
//
//    Created: Mon 30-Jun-2003 10:02
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccp.h"
#include "pcpeccrandomizer.h"
#include "pcpeccppoint.h"
#include "pcpbnresource.h"
#include "pcpeccpmethod.h"
#include "pcpeccpmethod128.h"
#include "pcpeccpmethod192.h"
#include "pcpeccpmethod224.h"
#include "pcpeccpmethod256.h"
#include "pcpeccpmethod384.h"
#include "pcpeccpmethod521.h"
#include "pcpeccpmethodcom.h"
#include "pcppma.h"


/*F*
//    Name: ippsECCPGetSize
//
// Purpose: Returns size of ECC context (bytes).
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pSize
//
//    ippStsSizeErr              2>feBitSize
//
//    ippStsNoErr                no errors
//
// Parameters:
//    feBitSize   size of field element (bits)
//    pSize       pointer to the size of internal ECC context
//
*F*/
IPPFUN(IppStatus, ippsECCPGetSize, (int feBitSize, int *pSize))
{
   /* test size's pointer */
   IPP_BAD_PTR1_RET(pSize);

   /* test size of field element */
   IPP_BADARG_RET((2>feBitSize), ippStsSizeErr);

   {
      int bn1Size;
      int bn2Size;
      int pointSize;
      int mont1Size;
      int mont2Size;
      #if defined(_USE_NN_VERSION_)
      int randSize;
      int randCntSize;
      #endif
      int primeSize;
      int listSize;

      /* size of field element */
      int gfeSize = BITS2WORD32_SIZE(feBitSize);
      /* size of order */
      int ordSize = BITS2WORD32_SIZE(feBitSize+1);

      /* size of BigNum over GF(p) */
      ippsBigNumGetSize(gfeSize, &bn1Size);

      /* size of BigNum over GF(r) */
      ippsBigNumGetSize(ordSize, &bn2Size);

      /* size of EC point over GF(p) */
      ippsECCPPointGetSize(feBitSize, &pointSize);

      /* size of montgomery engine over GF(p) */
      ippsMontGetSize(IppsBinaryMethod, BITS2WORD32_SIZE(feBitSize), &mont1Size);

      /* size of montgomery engine over GF(r) */
      ippsMontGetSize(IppsBinaryMethod, BITS2WORD32_SIZE(feBitSize+1), &mont2Size);

      #if defined(_USE_NN_VERSION_)
      /* size of randomizer engine */
      ippsPRNGGetSize(feBitSize+1, &randSize);
      ippsBigNumGetSize(RAND_CONTENT_LEN, &randCntSize);
      #endif

      /* size of prime engine */
      #if defined(_USE_NN_VERSION_)
      ippsPrimeGetSize(IppsBinaryMethod, feBitSize+1, &primeSize);
      #else
      ippsPrimeGetSize(feBitSize+1, &primeSize);
      #endif

      /* size of big num list (big num in the list preserve 32 bit word) */
      listSize = cpBigNumListGetSize(feBitSize+1+32, BNLISTSIZE);

      *pSize = sizeof(IppsECCPState)
              #if !defined(_IPP_TRS)
              +sizeof(ECCP_METHOD)  /* methods       */
              #endif

              +bn1Size              /* prime         */
              +bn1Size              /* A             */
              +bn1Size              /* B             */

              +bn1Size              /* GX            */
              +bn1Size              /* GY            */
              +bn2Size              /* order         */

              +bn1Size              /* Aenc          */
              +bn1Size              /* Benc          */
              +mont1Size            /* montgomery(p) */

              +pointSize            /* Genc          */
              +bn2Size              /* cofactor      */
              +mont2Size            /* montgomery(r) */

              +bn2Size              /* private       */
              +pointSize            /* public        */

              +bn2Size              /* eph private   */
              +pointSize            /* eph public    */

              #if defined(_USE_NN_VERSION_)
              +randSize             /* randomizer eng*/
              +randCntSize          /* randomizer bit*/
              #endif

              +primeSize            /* prime engine  */

              +listSize             /* temp big num  */
              +(ALIGN_VAL-1);
   }

   return ippStsNoErr;
}


/*F*
//    Name: ippsECCPInit
//
// Purpose: Init ECC context.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pECC
//
//    ippStsSizeErr              2>feBitSize
//
//    ippStsNoErr                no errors
//
// Parameters:
//    feBitSize   size of field element (bits)
//    pECC        pointer to the ECC context
//
*F*/
IPPFUN(IppStatus, ippsECCPInit, (int feBitSize, IppsECCPState* pECC))
{
   /* test pECC pointer */
   IPP_BAD_PTR1_RET(pECC);
   /* use aligned EC context */
   pECC = (IppsECCPState*)( IPP_ALIGNED_PTR(pECC, ALIGN_VAL) );

   /* test size of field element */
   IPP_BADARG_RET((2>feBitSize), ippStsSizeErr);

   /* context ID */
   ECP_ID(pECC) = idCtxECCP;

   /* generic EC */
   ECP_TYPE(pECC) = 0;

   /* size of field element & BP order */
   ECP_GFEBITS(pECC) = feBitSize;
   ECP_ORDBITS(pECC) = feBitSize+1;

   /*
   // init other context fields
   */
   {
      int bn1Size;
      int bn2Size;
      int pointSize;
      int mont1Size;
      int mont2Size;
      #if defined(_USE_NN_VERSION_)
      int randSize;
      int randCntSize;
      #endif
      int primeSize;
    //int listSize;

      /* size of field element */
      int gfeSize = BITS2WORD32_SIZE(feBitSize);
      /* size of order */
      int ordSize = BITS2WORD32_SIZE(feBitSize+1);

      Ipp8u* ptr = (Ipp8u*)pECC;

      /* size of BigNum over GF(p) */
      ippsBigNumGetSize(gfeSize, &bn1Size);

      /* size of BigNum over GF(r) */
      ippsBigNumGetSize(ordSize, &bn2Size);

      /* size of EC point over GF(p) */
      ippsECCPPointGetSize(feBitSize, &pointSize);

      /* size of montgomery engine over GF(p) */
      ippsMontGetSize(IppsBinaryMethod, BITS2WORD32_SIZE(feBitSize), &mont1Size);

      /* size of montgomery engine over GF(r) */
      ippsMontGetSize(IppsBinaryMethod, BITS2WORD32_SIZE(feBitSize+1), &mont2Size);

      #if defined(_USE_NN_VERSION_)
      /* size of randomizer engine */
      ippsPRNGGetSize(feBitSize+1, &randSize);
      ippsBigNumGetSize(RAND_CONTENT_LEN, &randCntSize);
      #endif

      /* size of prime engine */
      #if defined(_USE_NN_VERSION_)
      ippsPrimeGetSize(EXPONENT_METHOD, feBitSize+1, &primeSize);
      #else
      ippsPrimeGetSize(feBitSize+1, &primeSize);
      #endif

      /* size of big num list */
    //listSize = cpBigNumListGetSize(feBitSize+1+32, BNLISTSIZE);

      /* allocate buffers */
      ptr += sizeof(IppsECCPState);

      #if !defined(_IPP_TRS)
      ECP_METHOD(pECC)  = (ECCP_METHOD*)  (ptr);
      ptr += sizeof(ECCP_METHOD);
      #endif

      ECP_PRIME(pECC)   = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn1Size;
      ECP_A(pECC)       = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn1Size;
      ECP_B(pECC)       = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      ptr += bn1Size;
      ECP_GX(pECC)      = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn1Size;
      ECP_GY(pECC)      = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn1Size;
      ECP_ORDER(pECC)   = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      ptr += bn2Size;
      ECP_AENC(pECC)    = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn1Size;
      ECP_BENC(pECC)    = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn1Size;
      ECP_PMONT(pECC)   = (IppsMontState*)     ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      ptr += mont1Size;
      ECP_GENC(pECC)    = (IppsECCPPointState*)( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += pointSize;
      ECP_COFACTOR(pECC)= (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn2Size;
      ECP_RMONT(pECC)   = (IppsMontState*)     ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      ptr += mont2Size;
      ECP_PRIVATE(pECC) = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn2Size;
      ECP_PUBLIC(pECC)  = (IppsECCPPointState*)( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      ptr += pointSize;
      ECP_PRIVATE_E(pECC) = (IppsBigNumState*) ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bn2Size;
      ECP_PUBLIC_E(pECC) =(IppsECCPPointState*)( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      ptr += pointSize;
      #if defined(_USE_NN_VERSION_)
      ECP_RAND(pECC)    = (IppsPRNGState*)     ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += randSize;
      ECP_RANDCNT(pECC) = (IppsBigNumState*)   ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      ptr += randCntSize;
      #endif
      ECP_PRIMARY(pECC) = (IppsPrimeState*)    ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += primeSize;
      ECP_BNCTX(pECC)   = (BigNumNode*)        ( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      /* init buffers */
      ippsBigNumInit(gfeSize,  ECP_PRIME(pECC));
      ippsBigNumInit(gfeSize,  ECP_A(pECC));
      ippsBigNumInit(gfeSize,  ECP_B(pECC));

      ippsBigNumInit(gfeSize,  ECP_GX(pECC));
      ippsBigNumInit(gfeSize,  ECP_GY(pECC));
      ippsBigNumInit(ordSize,  ECP_ORDER(pECC));

      ippsBigNumInit(gfeSize,  ECP_AENC(pECC));
      ippsBigNumInit(gfeSize,  ECP_BENC(pECC));
      ippsMontInit(IppsBinaryMethod, BITS2WORD32_SIZE(feBitSize), ECP_PMONT(pECC));

      ippsECCPPointInit(feBitSize, ECP_GENC(pECC));
      ippsBigNumInit(ordSize,    ECP_COFACTOR(pECC));
      ippsMontInit(IppsBinaryMethod, BITS2WORD32_SIZE(feBitSize+1), ECP_RMONT(pECC));

      ippsBigNumInit(ordSize,   ECP_PRIVATE(pECC));
      ippsECCPPointInit(feBitSize,ECP_PUBLIC(pECC));

      ippsBigNumInit(ordSize,   ECP_PRIVATE_E(pECC));
      ippsECCPPointInit(feBitSize,ECP_PUBLIC_E(pECC));

      #if defined(_USE_NN_VERSION_)
      ippsPRNGInit(feBitSize+1, ECP_RAND(pECC));
      ippsBigNumInit(RAND_CONTENT_LEN, ECP_RANDCNT(pECC));
      #endif

      cpBigNumListInit(feBitSize+1+32, BNLISTSIZE, ECP_BNCTX(pECC));
   }

   return ippStsNoErr;
}

#endif /* _IPP_v50_ */
