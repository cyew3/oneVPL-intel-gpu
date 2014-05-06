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
//  File:       pcpbnpxca.c
//  Created:    2003/12/11 11:43
//
//  2003/12/11 modified by Andrey Somsikov
//      Functions redesigned as preparation for opening the source
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"


/*F*
// Name: cpBigNumInitSet
//
// Purpose: Helper function. Helps to create ONE and ZERO big numbers
// Usage:
//  IppsBigNumState *one;
//  Ipp8u one_buf[BN_BUFFERSIZE(1) + 8];
//  ...
//  one = (IppsBigNumState*)( IPP_ALIGNED_PTR(one_buf, 8) );
//  cpBigNumInitSet(one, IppsBigNumPOS, 1);
//  ...
//
*F*/
void cpBigNumInitSet(IppsBigNumState *pCtx, IppsBigNumSGN sgn, Ipp32u value)
{
    ippsBigNumInit(1, pCtx);
    ippsSet_BN(sgn, 1, &value, pCtx);
}


int cpCompare_BNUs(const Ipp32u *a, int size_a, const Ipp32u *b, int size_b)
{
    int i;

    if (size_a > size_b)
        return 1;
    else if (size_a < size_b)
        return -1;

    for (i = size_a-1; i >= 0; i--)
    {
        if (a[i] != b[i])
            break;
    }
    if (i == -1)
        return 0;
    else if (a[i] > b[i])
        return 1;

    return -1;
}


/*
Description:
    For x = (x[size_x - 1] ... x[1] x[0]) and m = (m[size_m - 1] ... m[1] m[0])
    compute the modulo r = (r[size_m - 1] ... r[1] r[0]) such that
    r = x mod m, 0 <= r < m

    x <- r = x mod y
*/
#if 0
void cpMod_BNU(Ipp32u *x, Ipp32u size_x, Ipp32u *m, Ipp32u size_m,
    Ipp32u *size_r)
{
    cpDiv_BNU(x, size_x, m, size_m, NULL, NULL, size_r);
}
#endif

/*
Description:
    For x = (x[size_x - 1] ... x[1] x[0]) and y = (y[size_y - 1] ... y[1] y[0])
    compute the quotient q = (q[size_x - size_y] ... y[1] y[0]) and remainder
    r = (r[size_y - 1] ... r[1] r[0]) such that x = q*y + r, 0 <= r < y

    q <- Int(x/y)
    x <- r = x mod y
*/
#if 0
/*
   Bad code:
      puzzed
      don't inherit optimazation
      contains implicit little-endian issue
   Gres
*/
#pragma warning (disable: 279)
void cpDiv_BNU(Ipp32u *x, Ipp32u size_x, Ipp32u *y, Ipp32u size_y,
    Ipp32u *q, Ipp32u *size_q, Ipp32u *size_r)
{
    int i, j, borrow, shift;
    Ipp32u xHi, r, tmp;
    Ipp32u qi;
    Ipp32u _size_q;
    union
    {
        Ipp64u u_64;
        struct
        {
            Ipp32u lo;
            Ipp32u hi;
        } u_32;
    } tmp64, qi_64;

/*  //must be checked before call
    if (y[size_y-1] == 0)
        return ippStsDivByZeroErr;
*/
    /* ensure that BNU size is the real size */
    while (size_x > 1 && x[size_x - 1] == 0)
        size_x--;
    while (size_y > 1 && y[size_y - 1] == 0)
        size_y--;

    if (size_x < size_y)
    {
        *size_r = size_x;
        if (q != NULL)
        {
            *size_q = 1;
            q[0] = 0;
        }
        return;
    }

    if (size_x == 1 && size_y == 1)
    {
        if (q != NULL)
        {
            q[0] = x[0] / y[0];
            *size_q = 1;
            x[0] = x[0] - q[0]*y[0];
        }
        else
            x[0] = x[0] % y[0];
        *size_r = 1;
        return;
    }
    else if (size_x <= 2 && size_y <= 2)
    {
        Ipp64u q64, x64, y64;
        x64 = (Ipp64u)(x[0]) | ( size_x == 2 ? ((Ipp64u)(x[1]) << 32) : ((Ipp64u)0));
        y64 = (Ipp64u)(y[0]) | ( size_y == 2 ? ((Ipp64u)(y[1]) << 32) : ((Ipp64u)0));
        if (q != NULL)
        {
            q64 = x64 / y64;
            q[0] = (Ipp32u)q64;
            q[1] = (Ipp32u)(q64 >> 32);
            *size_q = q[1] ? 2 : 1;
            x64 = x64 - q64*y64;
            x[0] = (Ipp32u)x64;
            x[1] = (Ipp32u)(x64 >> 32);
        }
        else
        {
            x64 = x64 % y64;
            x[0] = (Ipp32u)x64;
            x[1] = (Ipp32u)(x64 >> 32);
        }
        *size_r = x[1] ? 2 : 1;
        return;
    }

    *size_r = size_y;
    _size_q = size_x - size_y + 1;

    if (q != NULL)
        cpMemset32u(q, 0, _size_q);

    /*
    // Normalization
    // shift is so that y[t]*2^shift >= b/2
    */
    for (shift = 0; shift < 32; shift++)
    {
        if ( y[size_y - 1] & (1<<(31-shift)) )
            break;
    }

    /* Normalize: y *= 2^shift, x *= 2^shift */
    if (shift != 0)
    {
        /* Normalize x */
        xHi = x[size_x-1] >> (32 - shift);
        for (i = size_x - 1; i > 0; i--)
            x[i] = (x[i] << shift) | (x[i-1] >> (32 - shift));
        x[i] = x[i] << shift;

        /* Normalize y */
        for (i = size_y - 1; i > 0; i--)
            y[i] = (y[i] << shift) | (y[i-1] >> (32 - shift));
        y[i] = y[i] << shift;
    }
    else
        xHi = 0;

    for (i = _size_q - 1; i >= 0; i--)
    {
        /* compute qi approximation */
        if (i + size_y == size_x)
            tmp64.u_32.hi = xHi;
        else
            tmp64.u_32.hi = x[i + size_y];
        tmp64.u_32.lo = x[i + size_y -1];
        qi_64.u_64 = tmp64.u_64 / (Ipp64u)y[size_y-1];
        if (size_y >= 2)
            r = (Ipp32u)(tmp64.u_64 - (Ipp64u)y[size_y-1] * qi_64.u_64);
        while (1)
        {
            if (qi_64.u_32.hi == 1 && qi_64.u_32.lo == 0)
            {
                qi_64.u_64--;
                r += y[size_y - 1];
                if (r < y[size_y - 1])
                    break;
            }
            else if (size_y >= 2)
            {
                tmp64.u_64 = (Ipp64u)qi_64.u_64*y[size_y - 2];
                if (tmp64.u_32.hi > r ||
                    (tmp64.u_32.hi == r && tmp64.u_32.lo > x[i + size_y - 2]) )
                {
                    qi_64.u_64--;
                    r += y[size_y - 1];
                    if (r < y[size_y - 1])
                        break;
                }
                else
                    break;
            }
            else
                break;
        }
        qi = qi_64.u_32.lo;

        borrow = 0;
        for (j = 0; j < size_y; j++)
        {
            borrow = (tmp = x[j + i] - borrow) > IPP_MAX_32U - borrow;
            tmp64.u_64 = (Ipp64u)qi * y[j];
            borrow += tmp64.u_32.hi;
            if ( (x[j + i] = tmp - tmp64.u_32.lo) > IPP_MAX_32U - tmp64.u_32.lo )
                borrow++;
        }
        if (j + i == size_x)
            borrow = (xHi = xHi - borrow) > IPP_MAX_32U - borrow;
        else
            borrow = (x[j + i] = x[j + i] - borrow) > IPP_MAX_32U - borrow;

        if (borrow != 0)
        {
            qi--;

            tmp = 0;
            for (j = 0; j < size_y; j++)
            {
                tmp64.u_64 = (Ipp64u)( (Ipp64u)(x[j + i]) + y[j] + tmp );
                x[j + i] = tmp64.u_32.lo;
                tmp = tmp64.u_32.hi;
            }
            if (j + i == size_x)
                xHi += tmp;
            else
                x[j + i] += tmp;
        }
        if (q != NULL)
            q[i] = qi;
    }

    /*
    // Denormalization
    */
    if (shift != 0)
    {
        /* Denormalize x */
        for (i = 0; i < size_x - 1; i++)
            x[i] = (x[i] >> shift) | (x[i+1] << (32 - shift));
        x[i] = (x[i] >> shift) | (xHi << (32 - shift));

        /* Denormalize y */
        for (i = 0; i < size_y - 1; i++)
            y[i] = (y[i] >> shift) | (y[i+1] << (32 - shift));
        y[i] = y[i] >> shift;
    }


    /* Compute result sizes */
    if (q != NULL)
    {
        for (i = _size_q - 1; i > 0; i--)
        {
            if (q[i] != 0)
                break;
        }
        *size_q = i + 1;
    }

    for (i = *size_r - 1; i > 0; i--)
    {
        if (x[i] != 0)
            break;
    }
    *size_r = i + 1;
}
#pragma warning (default: 279)
#endif

#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
void cpDiv_BNU(Ipp32u* pX, int  sizeX,
               Ipp32u* pY, int  sizeY,
               Ipp32u* pQ, int* sizeQ, int* sizeR)
{
   FIX_BNU(pY,sizeY);
   FIX_BNU(pX,sizeX);

   /* special case */
   if(sizeX < sizeY) {
      *sizeR = sizeX;

      if(pQ) {
         pQ[0] = 0;
         *sizeQ = 1;
      }

      return;
   }

   /* special case */
   if(1 == sizeY) {
      int i;
      Ipp32u r = 0;
      for(i=sizeX-sizeY; i>=0; i--) {
         Ipp64u tmp = MAKEDWORD(pX[i],r);
         Ipp32u q = LODWORD(tmp / pY[0]);
         r = LODWORD(tmp - q*pY[0]);
         if(pQ) pQ[i] = q;
      }

      pX[0] = r;
      *sizeR = 1;

      if(pQ) {
         FIX_BNU(pQ,sizeX);
         *sizeQ = sizeX;
      }

      return;
   }


   /* common case */
   {
      int i;

      int qs = sizeX-sizeY+1;

      int nlz = NLZ32u(pY[sizeY-1]);

      /* normalization */
      pX[sizeX] = 0;
      if(nlz) {
         pX[sizeX] = pX[sizeX-1] >> (32-nlz);
         for(i=sizeX-1; i>0; i--)
            pX[i] = (pX[i]<<nlz) | (pX[i-1]>>(32-nlz));
         pX[0] <<= nlz;

         for(i=sizeY-1; i>0; i--)
            pY[i] = (pY[i]<<nlz) | (pY[i-1]>>(32-nlz));
         pY[0] <<= nlz;
      }

      /*
      // division
      */
      {
         Ipp32u yHi = pY[sizeY-1];

         for(i=qs-1; i>=0; i--) {
            Ipp32u extend;

            /* estimate digit of quotient */
            Ipp64u tmp = MAKEDWORD(pX[i+sizeY-1], pX[i+sizeY]);
            Ipp64u q = tmp / yHi;
            Ipp64u r = tmp - q*yHi;

            /* tune estimation above */
            for(; (q>=CONST_64(0x100000000)) || (Ipp64u)q*pY[sizeY-2] > MAKEDWORD(pX[i+sizeY-2],r); ) {
               q -= 1;
               r += yHi;
               if( HIDWORD(r) )
                  break;
            }

            /* multiply and subtract */
            cpSubMulDgt_BNU(pY, pX+i, sizeY, (Ipp32u)q, &extend);
            extend = (pX[i+sizeY] -= extend);

            if(extend) { /* subtracted too much */
               q -= 1;
               cpAdd_BNU(pY, pX+i, pX+i, sizeY, &extend);
               pX[i+sizeY] += extend;
            }

            /* store quotation digit */
            if(pQ) pQ[i] = LODWORD(q);
         }
      }

      /* de-normalization */
      if(nlz) {
         for(i=0; i<sizeX; i++)
            pX[i] = (pX[i]>>nlz) | (pX[i+1]<<(32-nlz));
         for(i=0; i<sizeY-1; i++)
            pY[i] = (pY[i]>>nlz) | (pY[i+1]<<(32-nlz));
         pY[sizeY-1] >>= nlz;
      }

      FIX_BNU(pX,sizeX);
      *sizeR = sizeX;

      if(pQ) {
         FIX_BNU(pQ,qs);
         *sizeQ = qs;
      }

      return;
   }
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
int cpMul_BNUs(const Ipp32u *a, int  a_size,
               const Ipp32u *b, int  b_size,
                     Ipp32u *r, int *r_size)
{
    int size = a_size + b_size;
    int i, j;
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

    /* gres. error, if *r_size = (a_size+b_size)-1 */
    #if 0
    for( i = 0; i < a_size + b_size; i++ )
        r[i] = 0;
    #endif
    for( i = 0; i < a_size; i++ )
        r[i] = 0;

    tmp.u_32.hi = 0; /* carry = 0 */
    for( i = 0; i < b_size; i++ )
    {
        r[i + a_size - 1] = tmp.u_32.hi;
        /* MACOne_BNU step */
        tmp.u_32.hi = 0; /* carry = 0 */
        for( j = 0; j < a_size; j++ )
        {
            tmp.u_64 = (Ipp64u)a[j] * (Ipp64u)b[i] + (Ipp64u)r[i + j] + (Ipp64u)tmp.u_32.hi;
            r[i + j] = tmp.u_32.lo;
        }
    }

    if (tmp.u_32.hi != 0)
    {
        if (*r_size >= size)
            r[size-1] = tmp.u_32.hi;
        else
            return 0;
    }
    else
        size--;

    /* compute real size */
    while (size > 1 && r[size - 1] == 0)
        size--;
    *r_size = size;

    return 1;
}
#endif


/*
// cpADD_BNUs
//
// ADDition
// Computes r <- a+b, returns real size of the r in the size_r variable
// Returns 0 if there are no enought buffer size to write to r[size_bigger]
// Returns 1 if no error
//
// Note:
//  Works correct in inplace mode
//  The minimum buffer size for the r must be MAX(size_a, size_b)
*/
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
int cpAdd_BNUs(const Ipp32u *a, int size_a,
               const Ipp32u *b, int size_b,
                     Ipp32u *r, int *size_r, int buffer_size_r)
{
    int i, size_smaller, size_bigger;
    const Ipp32u *bigger;
    Ipp32u carry;

    /* determine the longer operand */
    if (size_a >= size_b)
    {
        bigger = a;
        size_smaller = size_b;
        size_bigger = size_a;
    }
    else
    {
        bigger = b;
        size_smaller = size_a;
        size_bigger = size_b;
    }

    carry = 0;
    cpAdd_BNU(a, b, r, size_smaller, &carry);
    for(i = size_smaller; i < size_bigger; i++)
        carry = (r[i] = bigger[i] + carry) < carry;

    if(carry)
    {
        if (buffer_size_r <= size_bigger)
            return 0;
        r[size_bigger] = carry;
        *size_r = size_bigger + 1;
    }
    else
    {
        *size_r = size_bigger;

        /* compute real size */
        while ((*size_r) > 1 && r[(*size_r) - 1] == 0)
            (*size_r)--;
    }

    return 1;
}
#endif


/*
// cpSUB_BNUs
//
// SUBstraction
// Computes r <- a - b, returns real size and sign of the r in the size_r
//  and sign_r variables respectively
//
// Note:
//  Works correct in inplace mode
//  The buffer size for the r must be MAX(size_a, size_b)
*/
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
void cpSub_BNUs(const Ipp32u *a, int size_a,
                const Ipp32u *b, int size_b,
                      Ipp32u *r, int *size_r, IppsBigNumSGN *sgn_r)
{
    Ipp32u carry;
    int cmp_res;
    int i;

    *sgn_r = IppsBigNumPOS;

    cmp_res = cpCompare_BNUs(a, size_a, b, size_b);
    if (cmp_res < 0)
    {
        /* swap a and b to ensure that a > b */
        const Ipp32u* tmp_ptr;
        int tmp_32u;
        tmp_ptr = a; a = b; b = tmp_ptr;
        tmp_32u = size_a; size_a = size_b; size_b = tmp_32u;
        *sgn_r = IppsBigNumNEG;
    }
    else if (cmp_res == 0)
    {
        r[0] = 0;
        *size_r = 1;
        return;
    }

    carry = 0;
    cpSub_BNU(a, b, r, size_b, &carry);
    *size_r = size_a;
    for(i = size_b; i < size_a; i++)
        carry = (r[i] = a[i] - carry) > IPP_MAX_32U - carry;

    /* compute real size */
    while ((*size_r) > 1 && r[(*size_r) - 1] == 0)
        (*size_r)--;
}
#endif


/*
// cpMAC_BNU
//
// Multiply with ACcumulation
// Computes r <- r + a * b, returns real size of the r in the size_r variable
// Returns 0 if there are no enought buffer size to write to r[MAX(size_r + 1, size_a + size_b) - 1]
// Returns 1 if no error
//
// Note:
//  DO NOT run in inplace mode
//  The minimum buffer size for the r must be (size_a + size_b - 1)
//      the maximum buffer size for the r is MAX(size_r + 1, size_a + size_b)
*/
int cpMAC_BNU(const Ipp32u* a, int  size_a,
              const Ipp32u* b, int  size_b,
                    Ipp32u* r, int *size_r, int buffer_size_r)
{
    int i, j, k;
    int size_ab = size_a + size_b - 1;
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

    /* cleanup the rest of destination buffer */
    for (i = *size_r; i < size_ab; i++)
        r[i] = 0;
    *size_r = IPP_MAX(size_ab, *size_r);


    tmp.u_32.hi = 0; /* carry = 0 */
    for (j = 0; j < size_b; j++)
    {

        for (i = 0; i < size_a; i++)
        {
            tmp.u_64 = (Ipp64u)r[i + j] + (Ipp64u)a[i] * (Ipp64u)b[j] + tmp.u_32.hi;
            r[i + j] = tmp.u_32.lo;
        }
        if (tmp.u_32.hi != 0)
        {
            k = size_a + j;
            do
            {
                if ( (*size_r) <= k ) /* r[k] isn't touched before so write carry into and break */
                {
                    if (buffer_size_r <= k)
                        return 0;
                    r[k] = tmp.u_32.hi;
                    tmp.u_32.hi = 0;
                    *size_r = k + 1;
                    break;
                }
                tmp.u_64 = (Ipp64u)r[k] + tmp.u_32.hi;
                r[k] = tmp.u_32.lo;
                k++;
            } while (tmp.u_32.hi != 0);
        }
    }

    /* compute real size */
    while ((*size_r) > 1 && r[(*size_r) - 1] == 0)
        (*size_r)--;

    return 1;
}


/*
// cpModInv_BNU
//
// Inverse Modulus
// Computes r <- 1/a mod m, returns real size of the r in the size_r variable
//
// Return values:
//  ippStsNoErr     - Returns no error
//  ippStsBadArgErr - Returns an error when a > m, or gcd(a,m) > 1, or m <= 0
//
// Note:
//  DO NOT run in inplace mode
//  a != 0, m != 0
//  (size(buf_r) == size(r)) >= (size(buf_m) = size(m))
*/
#if 0
/*
// Gres.
// The implementation below contains the error.
//
// One can try:
//    a = 0x9E4C2B41 and
//    m = 0x100000000
// This case cpModInv_BNU() returns ippStsBadModulusErr.
//
// The error rise because FIXED itteration steps (nSteps).
// Usage this "tric" (see above) means that
// an author DON'T UNDERSTAND Inversion Alorithm at all
*/
IppStatus cpModInv_BNU(
    const Ipp32u *a, Ipp32u size_a, Ipp32u *m, Ipp32u size_m, Ipp32u *r, Ipp32u *size_r,
    Ipp32u *buf_a, Ipp32u *buf_m, Ipp32u *buf_r)
{
    Ipp32u *x1 = r;
    Ipp32u *x2 = buf_m;
    Ipp32u *q = buf_r;
    Ipp32u size_x1, size_x2, size_q;
    IppsBigNumSGN sgn;
    int nSteps;
    int buffer_size_m = size_m;

    /* 1 mod m = 1 */
    if (size_a == 1 && a[0] == 1)
    {
        r[0] = 1;
        *size_r = 1;

        return ippStsNoErr;

    }

    cpMemcpy32u(buf_a, a, size_a);

    size_x1 = 1;
    x1[0] = 0;

    size_x2 = 1;
    x2[0] = 1;

    nSteps = (size_a*32 + 1)/2;
    while (nSteps > 0)
    {
        cpDiv_BNU(m, size_m, buf_a, size_a, q, &size_q, &size_m);
        cpMAC_BNU(q, size_q, x2, size_x2, x1, &size_x1, buffer_size_m);
        if (size_m == 1 && m[0] == 1)
        {
            cpMAC_BNU(x1, size_x1, buf_a, size_a, x2, &size_x2, buffer_size_m);
            cpMemcpy32u(m, x2, size_x2);
            cpSub_BNUs(x2, size_x2, x1, size_x1, r, size_r, &sgn);
            return ippStsNoErr;
        }
        else if (size_m == 1 && m[0] == 0)
        {
            #if(_IPP64==_IPP64_I7)
            if(size_x1 &1)
               x1[size_x1] = 0;
            if(size_a &1)
               buf_a[size_a] = 0;
            cpMul_BNU_FullSize(x1, (size_x1+1)&~1, buf_a, (size_a+1)&~1, m);
            #else
            cpMul_BNU_FullSize(x1, size_x1, buf_a, size_a, m);
            #endif
            /* gcd = buf_a */
            return ippStsBadModulusErr;
        }

        cpDiv_BNU(buf_a, size_a, m, size_m, q, &size_q, &size_a);
        cpMAC_BNU(q, size_q, x1, size_x1, x2, &size_x2, buffer_size_m);
        if (size_a == 1 && buf_a[0] == 1)
        {
            cpMAC_BNU(x2, size_x2, m, size_m, x1, &size_x1, buffer_size_m);
            cpMemcpy32u(m, x1, size_x1);
            cpMemcpy32u(r, x2, size_x2);
            *size_r = size_x2;
            return ippStsNoErr;
        }
        else if (size_a == 1 && buf_a[0] == 0)
        {
            /* gcd = m */
            cpMemcpy32u(x1, m, size_m);
            #if(_IPP64==_IPP64_I7)
            if(size_x2 &1)
               x2[size_x2] = 0;
            if(size_x1 &1)
               x1[size_x1] = 0;
            cpMul_BNU_FullSize(x2, (size_x2+1)&~1, x1, (size_x1+1)&~1, m);
            #else
            cpMul_BNU_FullSize(x2, size_x2, x1, size_x1, m);
            #endif
            return ippStsBadModulusErr;
        }
        nSteps--;
    }

    return ippStsBadModulusErr;
}
#endif

/*
// The same, but without nSteps.
//                        Gres.
//
// Of course, the implementation isn't effective!!!
//
// Pay attention,
// the main reason of cpModInv_BNU(), at least for ippCP library,
// is wide usege of Montgomery technique inside ippCP.
// This case range of modulus (m) is very limited (0x100000000 or 0x10000000000000000)
// and therefore MUST TO BE significantly improved.
//                                            Gres.
*/
IppStatus cpModInv_BNU(const Ipp32u *a, int  size_a,
                             Ipp32u *m, int  size_m,
                             Ipp32u *r, int *size_r,
                             Ipp32u *buf_a, Ipp32u *buf_m, Ipp32u *buf_r)
{
    Ipp32u *x1 = r;
    Ipp32u *x2 = buf_m;
    Ipp32u *q = buf_r;
    int    size_x1, size_x2, size_q;
    IppsBigNumSGN sgn;
    int buffer_size_m = size_m;

    /* 1 mod m = 1 */
    if (size_a == 1 && a[0] == 1)
    {
        r[0] = 1;
        *size_r = 1;

        return ippStsNoErr;

    }

    cpMemcpy32u(buf_a, a, size_a);

    size_x1 = 1;
    x1[0] = 0;

    size_x2 = 1;
    x2[0] = 1;

    for(;;)
    {
        cpDiv_BNU(m, size_m, buf_a, size_a, q, &size_q, &size_m);
        cpMAC_BNU(q, size_q, x2, size_x2, x1, &size_x1, buffer_size_m);
        if (size_m == 1 && m[0] == 1)
        {
            cpMAC_BNU(x1, size_x1, buf_a, size_a, x2, &size_x2, buffer_size_m);
            cpMemcpy32u(m, x2, size_x2);
            cpSub_BNUs(x2, size_x2, x1, size_x1, r, size_r, &sgn);
            return ippStsNoErr;
        }
        else if (size_m == 1 && m[0] == 0)
        {
            #if(_IPP64==_IPP64_I7)
            if(size_x1 &1)
               x1[size_x1] = 0;
            if(size_a &1)
               buf_a[size_a] = 0;
            cpMul_BNU_FullSize(x1, (size_x1+1)&~1, buf_a, (size_a+1)&~1, m);
            #else
            cpMul_BNU_FullSize(x1, size_x1, buf_a, size_a, m);
            #endif
            /* gcd = buf_a */
            return ippStsBadModulusErr;
        }

        cpDiv_BNU(buf_a, size_a, m, size_m, q, &size_q, &size_a);
        cpMAC_BNU(q, size_q, x1, size_x1, x2, &size_x2, buffer_size_m);
        if (size_a == 1 && buf_a[0] == 1)
        {
            cpMAC_BNU(x2, size_x2, m, size_m, x1, &size_x1, buffer_size_m);
            cpMemcpy32u(m, x1, size_x1);
            cpMemcpy32u(r, x2, size_x2);
            *size_r = size_x2;
            return ippStsNoErr;
        }
        else if (size_a == 1 && buf_a[0] == 0)
        {
            /* gcd = m */
            cpMemcpy32u(x1, m, size_m);
            #if(_IPP64==_IPP64_I7)
            if(size_x2 &1)
               x2[size_x2] = 0;
            if(size_x1 &1)
               x1[size_x1] = 0;
            cpMul_BNU_FullSize(x2, (size_x2+1)&~1, x1, (size_x1+1)&~1, m);
            #else
            cpMul_BNU_FullSize(x2, size_x2, x1, size_x1, m);
            #endif
            return ippStsBadModulusErr;
        }
    }
}



Ipp32u cpGCD32u(Ipp32u a, Ipp32u b)
{
    Ipp32u greatest, t, r;

    if(a > b){
        greatest = a;
        t = b;
    } else {
        t = a;
        greatest = b;
    }

    while (t != 0)    {
        r = greatest % t;
        greatest = t;
        t = r;
    }
    return greatest;
}
