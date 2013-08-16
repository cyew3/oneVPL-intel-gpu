/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Safe Rijndael Encrypt, Decrypt
//    (It's the special free from Sbox/tables implementation)
//
// Contents:
//    ippsRijndael128EncryptCFB()
//
//
//    Created: Mon 08-Oct-2007 18:24
//  Author(s): Sergey Kirillov
*/
#if !defined(_IPP_TRS)

#include "precomp.h"
#include "owncp.h"
#include "pcprij128safe.h"

Ipp8u TransformByte(Ipp8u x, const Ipp8u Transformation[])
{
   Ipp32u y = 0;

   Ipp32u testBit = 0x01;
   int bit;
   for(bit=0; bit<8; bit++) {
      Ipp32u mask = (x & testBit)? 0xFF : 0;
      y ^= Transformation[bit] & mask;
      testBit <<= 1;
   }

   return (Ipp8u)y;
}

void TransformNative2Composite(Ipp8u out[], const Ipp8u inp[], int len)
{
   Ipp8u Native2CompositeTransformation[] = {0x01,0x2E,0x49,0x43,0x35,0xD0,0x3D,0xE9};
   int n;
   for(n=0; n<len; n++)
      out[n] = TransformByte(inp[n], Native2CompositeTransformation);
}

#if !((_IPP ==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8))

void TransformComposite2Native(Ipp8u out[], const Ipp8u inp[], int len)
{
   Ipp8u Composite2NativeTransformation[] = {0x01,0x5C,0xE0,0x50,0x1F,0xEE,0x55,0x6A};
   int n;
   for(n=0; n<len; n++)
      out[n] = TransformByte(inp[n], Composite2NativeTransformation);
}

void AddRoundKey(Ipp8u out[], const Ipp8u inp[], const Ipp8u pKey[])
{
   int n;
   for(n=0; n<16; n++)
      out[n] = (Ipp8u)( inp[n] ^ pKey[n] );
}

#define MASK_BIT(x,n)   ((Ipp32s)((x)<<(31-n)) >>31)
#define GF16mulX(x)     ( ((x)<<1) ^ ( MASK_BIT(x,3) & 0x13) )

static Ipp8u GF16mul(Ipp8u a, Ipp8u b)
{
   Ipp32u a0 = a;
   Ipp32u a1 = GF16mulX(a0);
   Ipp32u a2 = GF16mulX(a1);
   Ipp32u a4 = GF16mulX(a2);

   Ipp32u r = (a0 & MASK_BIT(b,0))
             ^(a1 & MASK_BIT(b,1))
             ^(a2 & MASK_BIT(b,2))
             ^(a4 & MASK_BIT(b,3));

   return (Ipp8u)r;
}

Ipp8u InverseComposite(Ipp8u x)
{
   Ipp8u GF16_sqr[] = {0x00,0x01,0x04,0x05,0x03,0x02,0x07,0x06,
                       0x0C,0x0D,0x08,0x09,0x0F,0x0E,0x0B,0x0A};
   Ipp8u GF16_sqr1[]= {0x00,0x09,0x02,0x0B,0x08,0x01,0x0A,0x03,
                       0x06,0x0F,0x04,0x0D,0x0E,0x07,0x0C,0x05};
   Ipp8u GF16_inv[] = {0x00,0x01,0x09,0x0E,0x0D,0x0B,0x07,0x06,
                       0x0F,0x02,0x0C,0x05,0x0A,0x04,0x03,0x08};


   /* split x = {bc} => b*t + c */
   int b = (x>>4) & 0x0F;
   int c = x & 0x0F;

   int D = GF16mul((Ipp8u)b, (Ipp8u)c)
          ^GF16_sqr[c]
          ^GF16_sqr1[b];

   D = GF16_inv[D];

   c = GF16mul((Ipp8u)(b^c), (Ipp8u)D);
   b = GF16mul((Ipp8u)b, (Ipp8u)D);

   /* merge p*t + q => {pq} = x */
   x = (Ipp8u)((b<<4) + c);
   return x;
}

#endif

#endif
