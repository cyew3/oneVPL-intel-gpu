/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2006 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
*/

#if !defined(_CP_PRIMEGEN_H)
#define _CP_PRIMEGEN_H

#include "pcpmontgomery.h"


/*
// Prime context
*/

struct _cpPrime {
   IppCtxId         idCtx;      /* Prime context identifier */
// int              bitSize;    /* prime number size (bits) */
   IppsBigNumState* pPrime;     /* prime number             */
   IppsBigNumState* pTemp;      /* temporary number         */
   IppsMontState*   pMont;      /* montgomery engine        */
};

/* alignment */
#define PRIME_ALIGNMENT (ALIGN_VAL)

/* Prime accessory macros */
#define PRIME_ID(ctx)       ((ctx)->idCtx)
#define PRIME_NUMBER(ctx)   ((ctx)->pPrime)
#define PRIME_TEMP(ctx)     ((ctx)->pTemp)
#define PRIME_MONT(ctx)     ((ctx)->pMont)

#define PRIME_ID_TEST(ctx) (PRIME_ID((ctx))==idCtxPrimeNumber)

/* easy prime test */
int MimimalPrimeTest(const Ipp32u *pPrime, int size);

#endif /* _CP_PRIMEGEN_H */
