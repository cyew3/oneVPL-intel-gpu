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
//
*/
#if !defined(_CP_BNU_H)
#define _CP_BNU_H

#include "pcpbnuext.h"

void*   cpMemset(void *dest, int c, int count);
void*   cpMemcpy(void *dest, const void *src, int count);
Ipp32u* cpMemset32u(Ipp32u *dest, Ipp32u c, int count);
Ipp32u* cpMemcpy32u(Ipp32u *dest, const Ipp32u *src, unsigned int count);

void cpEndianChange(Ipp32u *buffer, int size);

int  cpCmp_BNU(const Ipp32u* pA, const Ipp32u* pB, int ns);

void cpAdd_BNU(const Ipp32u* pA, const Ipp32u* pB, Ipp32u *pR, int ns, Ipp32u* pCarry);
void cpSub_BNU(const Ipp32u* pA, const Ipp32u* pB, Ipp32u *pR, int ns, Ipp32u* pBorrow);

void cpMulDgt_BNU   (const Ipp32u *a, Ipp32u *r, int n, Ipp32u w, Ipp32u *carry);
void cpAddMulDgt_BNU(const Ipp32u *a, Ipp32u *r, int n, Ipp32u w, Ipp32u *carry);
void cpSubMulDgt_BNU(const Ipp32u *a, Ipp32u *r, int n, Ipp32u w, Ipp32u *borrow);

void cpMul_BNU8(const Ipp32u *a, const Ipp32u *b, Ipp32u *r);
void cpMul_BNU4(const Ipp32u *a, const Ipp32u *b, Ipp32u *r);
void cpSqr_BNU8(const Ipp32u *a, Ipp32u *r);
void cpSqr_BNU4(const Ipp32u *a, Ipp32u *r);

void cpMul_BNU_FullSize(const Ipp32u* pA, int aSize,
                        const Ipp32u* pB, int bSsize,
                              Ipp32u* pR);

int cpMAC_BNU(const Ipp32u* a, int  size_a,
              const Ipp32u* b, int  size_b,
                    Ipp32u* r, int *size_r, int buffer_size_r);

Ipp64u cpDiv64u32u(Ipp64u a, Ipp32u b);

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

IppStatus cpModInv_BNU(const Ipp32u *a, int  size_a,
                             Ipp32u *m, int  size_m,
                             Ipp32u *r, int *size_r,
                             Ipp32u *buf_a, Ipp32u *buf_m, Ipp32u *buf_r);

#if (_IPP64==_IPP64_I7)
void cpMul_BNU_FullEvenSize(const Ipp32u *a, int a_size,
                               const Ipp32u *b, int b_size, Ipp32u *r);
void cpMul_BNU_FullShortSize(const Ipp32u *a, const Ipp32u *b,
                                  int size, Ipp32u *r);
void cpSqr_BNU(const Ipp32u *a, int size, Ipp32u *r);
#endif

#endif /* _CP_BNU_H */
