/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2002-2003 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Definitions and
//    Internal Pseudo Random Generator Function Prototypes
*/
#if !defined(_CP_PRNG_H)
#define _CP_PRNG_H

#include "pcpshs.h"

/*
// Pseudo-random generation context
*/

#define MAX_XKEY_SIZE       512
#define DEFAULT_XKEY_SIZE   512 /* must be >=160 || <=512 */

struct _cpPRNG {
   IppCtxId idCtx;                                 /* PRNG identifier            */
   int      seedBits;                              /* secret seed-key bitsize    */
   Ipp32u   Q[5];                                  /* modulus                    */
   Ipp32u   T[5];                                  /* parameter of SHA_G() funct */
   Ipp32u   xAug[BITS2WORD32_SIZE(MAX_XKEY_SIZE)]; /* optional entropy augment   */
   Ipp32u   xKey[BITS2WORD32_SIZE(MAX_XKEY_SIZE)]; /* secret seed-key            */
};

/* alignment */
#define PRNG_ALIGNMENT ((int)(sizeof(Ipp32u)))

#define RAND_ID(ctx)       ((ctx)->idCtx)
#define RAND_SEEDBITS(ctx) ((ctx)->seedBits)
#define RAND_Q(ctx)        ((ctx)->Q)
#define RAND_T(ctx)        ((ctx)->T)
#define RAND_XAUGMENT(ctx) ((ctx)->xAug)
#define RAND_XKEY(ctx)     ((ctx)->xKey)

#define RAND_VALID_ID(ctx)  (RAND_ID((ctx))==idCtxPRNG)

int cpPRNGen(Ipp32u* prand, int bitLen, IppsPRNGState* pCtx);

#endif /* _CP_PRNG_H */
