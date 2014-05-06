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
//  File:       pcpmontmulpxca.c
//  Created:    2003/04/24    22:04
//
//  Last modified by Alexey Omeltchenko
*/

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpmontgomery.h"

#if defined(_USE_NN_MONTMUL_)

#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))

#define MONT_AddOneBNU_I( pSrcDst, length, value )\
{\
    int i=0; \
    Ipp32u tmp = (value);\
    do\
    {\
        (pSrcDst)[i] += tmp;\
        tmp = (pSrcDst)[i] < tmp;\
        i++;\
    } while( tmp && i <= (length));\
}

#define MONT_AddOneBNU( pSrc, pDst, length, carry )\
{\
    int i=0;\
    do\
    {\
        (pDst)[i] = (pSrc)[i] + (carry);\
        (carry) = (pDst)[i] < (carry);\
        i++;\
    } while( (carry) && i <= (length));\
    while( i <= (length))\
    {\
        (pDst)[i] = (pSrc)[i];\
        i++;\
    }\
}

static void MAC_shr_OneBNU(const Ipp32u *a, Ipp32u *r, int n, Ipp32u w, Ipp32u *c )
{
    int i;
    union
    {
        Ipp64u u_64;
        struct
        {
            #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
            Ipp32u hi;
            Ipp32u lo;
            #else
            Ipp32u lo;
            Ipp32u hi;
            #endif
        } u_32;
    } tmp;
    Ipp64u carry;

    tmp.u_64 = ((Ipp64u)a[0]) * ((Ipp64u)w) + ((Ipp64u)r[0]);
    carry = tmp.u_32.hi;

    for( i = 1; i < n; i++ )
    {
        tmp.u_64 = ((Ipp64u)a[i]) * ((Ipp64u)w) + ((Ipp64u)r[i]) + carry;
        r[i-1] = tmp.u_32.lo;
        carry = tmp.u_32.hi;
    }

    *c = (Ipp32u)carry;
}

/*          Algorithm: Montgomery multiplication
// INPUT:   integers m = (m[n-1] ...m[1]m[0])b,
//          x = (x[n-1] ... x[1]x[0])b,
//          y = (y[n-1] ... y[1]y[0])b
//          with 0 <= x,y < m, R = b^^n with gcd(m, b)= 1, and m0 = -(1/m)mod b.
// OUTPUT:  xyR^^-1 mod m.
// 1.   A <- 0. (Notation: A = (a[n]a[n-1] ... a[1]a[0])b.)
// 2.   For i from 0 to (n - 1) do the following:
// 2.1      u[i] <- (a[0] + x[i]y[0])m0 mod b.
// 2.2      A <- (A + x[i]y + u[i]m)/b.
// 3.   If A >= m then A <- A - m.
// 4.   Return(A).
*/
void cpMontMul(const Ipp32u *x, int  x_size,
               const Ipp32u *y, int  y_size,
               const Ipp32u *m, int  m_size,
                     Ipp32u *r, int *r_size,
               const Ipp32u *mHi, Ipp32u *A)
{
    Ipp32u m0 = *mHi;
    union
    {
        Ipp64u      u_64;
        struct
        {
            #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
            Ipp32u hi;
            Ipp32u lo;
            #else
            Ipp32u lo;
            Ipp32u hi;
            #endif
        } u_32;
    } U;
    int i;
    Ipp32u carry1, carry2;
    int n = m_size;
    //int _r_size;

    /* Step 1 */
    for( i = 0; i <= n+1; i++ )
        A[i] = 0;

    /* Step 2 */
    for( i = 0; i < x_size; i++ )
    {
        /* Step 2.1 */
        U.u_64 = ((Ipp64u)A[0] + (Ipp64u)x[i] * (Ipp64u)y[0]) * (Ipp64u)m0;

        /* Step 2.2 */
        cpAddMulDgt_BNU( y, A, y_size, x[i], &carry1 );
        MONT_AddOneBNU_I( A + y_size, n+1 - y_size, carry1 );
        MAC_shr_OneBNU( m, A, m_size, U.u_32.lo, &carry2 );
        MONT_AddOneBNU( A + m_size, A + (m_size-1), n+1 - m_size, carry2 );
    }
    for( /*i = x_size*/; i < n; i++ )
    {/* x[i] = 0 */
        /* Step 2.1 */
        U.u_64 = (Ipp64u)A[0] * (Ipp64u)m0;

        /* Step 2.2 */
        MAC_shr_OneBNU( m, A, m_size, U.u_32.lo, &carry2 );
        MONT_AddOneBNU( A + m_size, A + (m_size-1), n+1 - m_size, carry2 );
    }

    /* Step 3, 4 */
    if( (A[m_size] == 0) && (cpCompare_BNUs( A, m_size, m, m_size) <= 0 )) {
       COPY_BNU(A, r, m_size);
    }
    else
       cpSub_BNU( A, m, r, m_size, &carry1 );

    FIX_BNU(r, m_size);

    *r_size = m_size;
}

#endif
#endif



//#if !defined(_USE_NN_MONTMUL_)
#if !defined(_USE_NN_MONTMUL_)  && \
    !defined(_USE_XSC_MONTMUL_) && \
    !defined(_USE_EMT_MONTMUL_)

#if !((_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpMontMul(const Ipp32u* pX, int  xSize,
               const Ipp32u* pY, int  ySize,
               const Ipp32u* pModulo, int  mSize,
                     Ipp32u* pR, int* prSize,
               const Ipp32u *mHi, Ipp32u* pTmpProd, Ipp32u* pBuffer)
{
   Ipp32u m0 = *mHi; /* 32-bit version */

   #if !defined(_USE_KARATSUBA_)
   UNREFERENCED_PARAMETER(pBuffer);
   #endif

   MUL_BNU(pTmpProd, pX,xSize, pY,ySize, pBuffer);
   ZEXPAND_BNU(pTmpProd,xSize+ySize, 2*mSize);

   cpMontReduction(pR, pTmpProd, pModulo, mSize, m0);

   FIX_BNU(pR, mSize);
   *prSize = mSize;
}

#else
void cpMontMul(const Ipp32u* pX, int  xSize,
               const Ipp32u* pY, int  ySize,
               const Ipp32u* pModulo, int  mSize,
                     Ipp32u* pR, int* prSize,
               const Ipp32u *mHi, Ipp32u* pTmpProd, Ipp32u* pBuffer)
{
   Ipp64u m0 = MAKEDWORD(mHi[0],mHi[1]); /* 64-bit version */
   int emSize = mSize + (mSize&1);

   #if !defined(_USE_KARATSUBA_)
   UNREFERENCED_PARAMETER(pBuffer);
   #endif

   //cpMul_BNU_FullSize(pX,xSize, pY,ySize, pTmpProd);
   MUL_BNU(pTmpProd, pX,xSize, pY,ySize, pBuffer);
   ZEXPAND_BNU(pTmpProd,xSize+ySize, 2*emSize);

   cpMontReduction64(pR, pTmpProd, pModulo, mSize, m0);

   FIX_BNU(pR, mSize);
   *prSize = mSize;
}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
#if 0 // unfafe version
void cpMontReduction(Ipp32u* pR, Ipp32u* pBuffer,
               const Ipp32u* pModulo, int mSize, Ipp32u m0)
{
   Ipp32u gcarry = 0;

   int n;
   for(n=0; n<mSize; n++) {
      int i;
      Ipp32u u = pBuffer[n]*m0;

      Ipp64u t;
      Ipp32u lcarry = 0;

      for(i=0; i<mSize; i++) {
         t = (Ipp64u)u * pModulo[i] + pBuffer[n+i] + lcarry;
         pBuffer[n+i] = LODWORD(t);
         lcarry = HIDWORD(t);
      }

      t = (Ipp64u)(pBuffer[mSize+n]) + lcarry + gcarry;
      pBuffer[mSize+n] = LODWORD(t);
      gcarry = HIDWORD(t);
   }

   if(gcarry || (cpCmp_BNU(pBuffer+mSize, pModulo, mSize)>=0))
      cpSub_BNU(pBuffer+mSize, pModulo, pR, mSize, &gcarry);
   else
      COPY_BNU(pBuffer+mSize, pR, mSize);
}
#endif

#define MASKED_COPY_BNU(dst, mask, src1, src2, len) \
{ \
  int i; \
  for(i=0; i<(len); i++) (dst)[i] = ((mask) & (src1)[i]) | (~(mask) & (src2)[i]); \
}

void cpMontReduction(Ipp32u* pR, Ipp32u* pBuffer,
               const Ipp32u* pModulo, int mSize, Ipp32u m0)
{
   Ipp32u gcarry = 0;
   Ipp32u mask;

   int n;
   for(n=0; n<mSize; n++) {
      int i;
      Ipp32u u = pBuffer[n]*m0;

      Ipp64u t;
      Ipp32u lcarry = 0;

      for(i=0; i<mSize; i++) {
         t = (Ipp64u)u * pModulo[i] + pBuffer[n+i] + lcarry;
         pBuffer[n+i] = LODWORD(t);
         lcarry = HIDWORD(t);
      }

      t = (Ipp64u)(pBuffer[mSize+n]) + lcarry + gcarry;
      pBuffer[mSize+n] = LODWORD(t);
      gcarry = HIDWORD(t);
   }

   {
      Ipp32u borrow;
      cpSub_BNU(pBuffer+mSize, pModulo, pR, mSize, &borrow);
      mask = -(gcarry | (1-borrow));
      MASKED_COPY_BNU(pR, mask, pR, pBuffer+mSize, mSize);
   }
}

#endif

#endif
