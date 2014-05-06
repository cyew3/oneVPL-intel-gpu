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
//  File:       pcpmulbnupxca.c
//  Created:    2003/04/24    17:40
//
//  Last modified by Alexey Omeltchenko
//  2005/03/30 cpSqr_BNU added by gres
*/

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"

#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )

void cpMul_BNU_FullSize(const Ipp32u *a, int a_size,
                        const Ipp32u *b, int b_size,
                              Ipp32u *r)
{
    int i, j;
    Ipp32u carry;
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

    for( i = 0; i < a_size + b_size; i++ )
        r[i] = 0;

    for( i = 0; i < b_size; i++ )
    {
        carry = 0;
        for( j = 0; j < a_size; j++ )
        {
            tmp.u_64 = (Ipp64u)a[j] * (Ipp64u)b[i] + (Ipp64u)r[i + j] + (Ipp64u)carry;
            r[i + j] = tmp.u_32.lo;
            carry = tmp.u_32.hi;
        }
        r[i + j] = carry;
    }
}

#endif


#if defined(_USE_SQR_)

#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )
void cpSqr_BNU(Ipp32u* pR, const Ipp32u* pA, int aSize)
{
   int i;
   Ipp32u carry;
   Ipp64u a;

   /* init result */
   pR[0] = 0;
   a = pA[0];
   for(i=1, carry=0; i<aSize; i++) {
      Ipp64u t = a * pA[i] + carry;
      pR[i] = LODWORD(t);
      carry = HIDWORD(t);
   }
   pR[i] = carry;

   /* add other a[i]*a[j] */
   for(i=1; i<aSize; i++) {
      int j;
      a = pA[i];
      for(j=i+1, carry=0; j<aSize; j++) {
         Ipp64u t = a * pA[j] + pR[i+j] + carry;
         pR[i+j] = LODWORD(t);
         carry = HIDWORD(t);
      }
      pR[i+j] = carry;
   }

   /* double a[i]*a[j] */
   for(i=1, carry=0; i<(2*aSize-1); i++) {
      Ipp64u t = (Ipp64u)pR[i] + pR[i] + carry;
      pR[i] = LODWORD(t);
      carry = HIDWORD(t);
   }
   pR[i] = carry;

   /* add a[i]^2 */
   for(i=0, carry=0; i<aSize; i++) {
      Ipp64u t = (Ipp64u)pA[i] * pA[i];
      Ipp64u
      x = (Ipp64u)pR[2*i+0] + LODWORD(t) + carry;
      pR[2*i+0] = LODWORD(x);
      carry = HIDWORD(x);
      x = (Ipp64u)pR[2*i+1] + HIDWORD(t) + carry;
      pR[2*i+1] = LODWORD(x);
      carry = HIDWORD(x);
   }
}
#endif

#endif
