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

#if !defined(_CP_PRIMENUM_NNV_H)
#define _CP_PRIMENUM_NNV_H

/*
// Prime context
*/

typedef struct _cpPrime
{
    IppCtxId    idCtx;      /* Prime context identifier */
    int         bitlength;
    IppsBigNumState  *prime;
    IppsBigNumState  *r;
    IppsPRNGState    *rand;
    IppsMontState    *m;

} ippcpPrime;

/* Prime accessory macros */
#define PRIME_ID(ctx)       ((ctx)->idCtx)
#define PRIME_BITSIZE(ctx)  ((ctx)->bitlength)
#define PRIME_BITS(ctx)     ((ctx)->prime)
#define PRIME_CTX(ctx)      ((ctx)->r)
#define PRIME_PRNG(ctx)     ((ctx)->rand)
#define PRIME_MONT(ctx)     ((ctx)->m)

#define PRIME_ALIGN_SIZE (ALIGN_VAL)
#define IPP_MAKE_ALIGNED_PRIME(pCtx) ( (pCtx)=(IppsPrimeState*)(IPP_ALIGNED_PTR((pCtx), (PRIME_ALIGN_SIZE))) )

/* Internal functions */
void cpPrimeFilter(Ipp32u *prime, Ipp32u* buffer, int size, Ipp32u *result);

#endif /* _CP_PRIMENUM_NNV_H */
