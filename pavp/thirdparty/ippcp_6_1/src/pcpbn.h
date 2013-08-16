/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
//
*/
#if !defined(_CP_BIGNUM_H)
#define _CP_BIGNUM_H

#include "pcpbnu.h"


/*
// Big Number context
*/
typedef struct _cpBigNum
{
    IppCtxId      idCtx;         /* BigNumbers spec identifier          */
    IppsBigNumSGN sgn;           /* sign of the number                  */
    int           size;          /* real size of big number (in DWORDs) */
    int           buffer_size;   /* the maximum length of big number    */
    Ipp32u        *number;       /* Buffer with length = buffer_size    */
    Ipp32u        *workBuffer;   /* Buffer with length = buffer_size    */
} ippcpBigNum;


/*
// Useful macros
*/
#define IPP_MULTIPLE_OF(N, MULTIPLIER) ( ( ( (N)+(MULTIPLIER)-1 ) / (MULTIPLIER) ) * (MULTIPLIER) )

#if  ((_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
#  define BNUBASE_TYPE_SIZE (2)
#else
#  define BNUBASE_TYPE_SIZE (1)
#endif

#define BN_ALIGN_SIZE (ALIGN_VAL)
#define BN_ALIGNMENT  (BN_ALIGN_SIZE)
#define IPP_MAKE_ALIGNED_BN(pCtx) ( (pCtx)=(IppsBigNumState*)(IPP_ALIGNED_PTR((pCtx), (BN_ALIGN_SIZE))) )

#define BN_BUFFERSIZE(length) ( IPP_MULTIPLE_OF(sizeof(IppsBigNumState) + 2*IPP_MULTIPLE_OF(length, BNUBASE_TYPE_SIZE)*sizeof(Ipp32u) + BN_ALIGN_SIZE, 16) )

/* BN accessory macros */
#define BN_ID(pBN)         ((pBN)->idCtx)
#define BN_SIGN(pBN)       ((pBN)->sgn)
#define BN_POSITIVE(pBN)   (BN_SIGN(pBN)==IppsBigNumPOS)
#define BN_NEGATIVE(pBN)   (BN_SIGN(pBN)==IppsBigNumNEG)
#define BN_NUMBER(pBN)     ((pBN)->number)
#define BN_BUFFER(pBN)     ((pBN)->workBuffer)
#define BN_ROOM(pBN)       ((pBN)->buffer_size)
#define BN_SIZE(pBN)       ((pBN)->size)
#define BN_VALID_ID(pBN)   (BN_ID((pBN))==idCtxBigNum)

#define INVERSE_SIGN(s)    (((s)==IppsBigNumPOS)? IppsBigNumNEG : IppsBigNumPOS)


/*
// internal functions
*/
/*
int cpCompare_BNUs(const Ipp32u* pA, int aSize, const Ipp32u* pB, int bSize);

int  cpAdd_BNUs(const Ipp32u *a, int  size_a,
                const Ipp32u *b, int  size_b,
                      Ipp32u *r, int *size_r, int buffer_size_r);
void cpSub_BNUs(const Ipp32u *a, int  size_a,
                const Ipp32u *b, int  size_b,
                      Ipp32u *r, int *size_r, IppsBigNumSGN *sgn_r);
int  cpMul_BNUs(const Ipp32u *a, int  a_size,
                const Ipp32u *b, int  b_size,
                      Ipp32u *r, int *r_size);
void cpDiv_BNU(Ipp32u *x, int  size_x,
               Ipp32u *y, int  size_y,
               Ipp32u *q, int *size_q, int *size_r);

#define cpMod_BNU(pX,sizeX, pM,sizeM, psizeR) \
               cpDiv_BNU((pX),(sizeX), (pM),(sizeM), NULL,NULL, (psizeR))
*/

void cpBigNumInitSet(IppsBigNumState *pCtx, IppsBigNumSGN sgn, Ipp32u value);

Ipp32u cpGCD32u(Ipp32u a, Ipp32u b);

/*
IppStatus cpModInv_BNU(const Ipp32u *a, int  size_a,
                             Ipp32u *m, int  size_m,
                             Ipp32u *r, int *size_r,
                             Ipp32u *buf_a, Ipp32u *buf_m, Ipp32u *buf_r);
*/

#include "pcpbnext.h"
#include "pcpbn32.h"

#endif /* _CP_BIGNUM_H */
