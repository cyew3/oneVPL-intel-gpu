/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Cipher Tools
//
// Contents:
//    FillBlock8()
//    FillBlock16()
//    FillBlock24()
//    FillBlock32()
//
//    XorBlock()
//
//    CopyBlock()
//
//    PaddBlock()
//    TestPadding()
//
//    StdIncrement()
//
//    CmpBlock()
//
//
//    Created: Mon 22-Jul-2002 17:08
//  Author(s): Sergey Kirillov
//
//   Modified: Tue 13-Dec-2005 09:23
//  Author(s): Vasiliy Buzoverya
//             The functions CopyBlock8, CopyBlock16, CopyBlock24, CopyBlock32,
//             XorBlock8, XorBlock16, XorBlock24, XorBlock32 have been
//             implemented as macro definitions in pcpcihertool.h header file.
//
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpciphertool.h"

void FillBlock8(Ipp8u filler, const void* pSrc, void* pDst, int len)
{
   int n;
   for(n=0; n<len; n++) ((Ipp8u*)pDst)[n] = ((Ipp8u*)pSrc)[n];
   for(; n<8; n++) ((Ipp8u*)pDst)[n] = filler;
}

void FillBlock16(Ipp8u filler, const void* pSrc, void* pDst, int len)
{
   int n;
   for(n=0; n<len; n++) ((Ipp8u*)pDst)[n] = ((Ipp8u*)pSrc)[n];
   for(; n<16; n++) ((Ipp8u*)pDst)[n] = filler;
}

void FillBlock24(Ipp8u filler, const void* pSrc, void* pDst, int len)
{
   int n;
   for(n=0; n<len; n++) ((Ipp8u*)pDst)[n] = ((Ipp8u*)pSrc)[n];
   for(; n<24; n++) ((Ipp8u*)pDst)[n] = filler;
}

void FillBlock32(Ipp8u filler, const void* pSrc, void* pDst, int len)
{
   int n;
   for(n=0; n<len; n++) ((Ipp8u*)pDst)[n] = ((Ipp8u*)pSrc)[n];
   for(; n<32; n++) ((Ipp8u*)pDst)[n] = filler;
}

void XorBlock(const void* pSrc1, const void* pSrc2, void* pDst, int len)
{
   int n;
   for(n=0; n<len; n++)
      ((Ipp8u*)pDst)[n] = (Ipp8u)( ((Ipp8u*)pSrc1)[n]^((Ipp8u*)pSrc2)[n] );
}

void CopyBlock(const void* pSrc, void* pDst, int len)
{
   int n;
   for(n=0; n<len; n++) ((Ipp8u*)pDst)[n] = ((Ipp8u*)pSrc)[n];
}

void PaddBlock(Ipp8u filler, void* pDst, int len)
{
   int n;
   for(n=0; n<len; n++) ((Ipp8u*)pDst)[n] = filler;
}

int TestPadding(Ipp8u filler, void* pSrc, int len)
{
   int n;
   int x = 0;
   for(n=0; n<len; n++) x |= ((Ipp8u*)pSrc)[n];
   return x==filler;
}


/*F*
//    Name: StdIncrement
//
// Purpose: Inctrment numerical part of counter block
//          (see sp800-38a for details)
*F*/
#if ( _IPP_ARCH == _IPP_ARCH_XSC )
void StdIncrement(Ipp8u* pCounter, int blkSize, int numSize)
{
   int maskPosition = (blkSize-numSize)/8;
   Ipp8u mask = (Ipp8u)( 0xFF >> (blkSize-numSize)%8 );

   /* save crytical byte */
   Ipp8u save  = (Ipp8u)( pCounter[maskPosition] & ~mask );

   int len = BITS2WORD8_SIZE(blkSize);
   Ipp32u carry = 1;
   for(; (len>maskPosition) && carry; len--) {
      Ipp32u x = pCounter[len-1] + carry;
      pCounter[len-1] = (Ipp8u)x;
      carry = (x>>8) & 0xFF;
   }

   /* update crytical byte */
   pCounter[maskPosition] &= mask;
   pCounter[maskPosition] |= save;
}
#endif

/*
// Compare
*/
int EquBlock(const void* pSrc1, const void* pSrc2, int len)
{
   int i;
   for(i=0; i<len; i++) {
      if(((Ipp8u*)pSrc1)[i] != ((Ipp8u*)pSrc2)[i])
         return 0;
   }
   return 1;
}

