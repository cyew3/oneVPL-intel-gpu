/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2003 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
*/

#if !defined(_CP_PRNG_NNV_H)
#define _CP_PRNG_NNV_H

#include "pcpshs.h"

/*
// Pseudo-random generation spec structure
*/

#define MAX_XKEY_SIZE       512
#define DEFAULT_XKEY_SIZE   512 /* must be >=160 || <=512 */

typedef struct _cpPRNG
{
    IppCtxId    idCtx;      /* Pseudo-random generation spec identifier */
    int CurLength;          /* generated random number bit length       */
    int RecLength;          /* required random number bit length        */
    int b;                  /* XKey size                                */
    Ipp32u Q[5];            /* Module                                   */
    Ipp32u *XKey;           /* XKey                                     */
    Ipp32u *rand;           /* generated number                         */
    IppsSHA1State *pHashCtx;
} ippcpPRNG;

#define RAND_ID(ctx)       ((ctx)->idCtx)
#define RAND_ROOM(ctx)     ((ctx)->RecLength)
#define RAND_Q(ctx)        ((ctx)->Q)
#define RAND_XKEYLEN(ctx)  ((ctx)->b)
#define RAND_XKEY(ctx)     ((ctx)->XKey)
#define RAND_BITSIZE(ctx)  ((ctx)->CurLength)
#define RAND_BITS(ctx)     ((ctx)->rand)
#define RAND_HASH(ctx)     ((ctx)->pHashCtx)

#define PRNG_ALIGN_SIZE (ALIGN_VAL)
#define IPP_MAKE_ALIGNED_PRNG(pCtx) ( (pCtx)=(IppsPRNGState*)(IPP_ALIGNED_PTR((pCtx), (PRNG_ALIGN_SIZE))) )

IppStatus PRNGAdd(IppsBigNumState *c, int bitlength, Ipp32u* H0_SHA1, IppsPRNGState *r);

#endif /* _CP_PRNG_NNV_H */
