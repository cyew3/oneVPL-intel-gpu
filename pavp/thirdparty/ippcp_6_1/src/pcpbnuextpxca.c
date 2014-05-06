/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Addition BNU functionality
//
// Contents:
//    Cpy_BNU()
//    Set_BNU()
//    Tst_BNU()
//    Cmp_BNU()
//
//    LSL_BNU()
//    LSR_BNU()
//
//    LSB_BNU()
//    MSB_BNU()
//    MSD_BNU()
//
//    NLZ32u()
//    NTZ32u()
//
//
//    Created: Thu 09-Dec-2004 14:05
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpbnu.h"

/*
// Copy
*/
void Cpy_BNU(const Ipp32u* pSrc, Ipp32u* pDst, int size)
{
   int n;
   for(n=0; n<size; n++)
      pDst[n] = pSrc[n];
}

/*
// Set
*/
void Set_BNU(Ipp32u w, Ipp32u* pDst, int size)
{
   int n;
   for(n=0; n<size; n++)
      pDst[n] = w;
}

/*
// Tst_BNU
//
// Test BNU
//
// Returns
//     0, if A = 0
//    >0, if A > 0
//    <0, looks like impossible (or error) case
*/
int Tst_BNU(const Ipp32u* pA, int aSize)
{
   for(; (aSize>0) && (0==pA[aSize-1]); aSize--) ;
   return aSize;
}

/*
// Cmp_BNU
//
// Compare two arbitrary BNUs
//
// Returns
//    -1, if A < B
//     0, if A = B
//    +1, if A > B
*/
int Cmp_BNU(const Ipp32u* pA, int aSize, const Ipp32u* pB, int bSize)
{
   int code;

   FIX_BNU(pA,aSize);
   FIX_BNU(pB,bSize);

   /* code = (aSize>bSize)? 1 : (aSize==bSize)? 0 : -1; */
   code = aSize-bSize;

   for(; (aSize>0 && !code); aSize--) {
      if(pA[aSize-1] > pB[aSize-1])
         code = 1;
      else if(pA[aSize-1] < pB[aSize-1])
         code =-1;
   }
   return code;
}

/*
// LSL_BNU
//
// Logical Shift Left
//
// Returns new length
//
// Note:
//    nBits <32
//    inplace mode works correct
*/
int LSL_BNU(const Ipp32u* pSrc, Ipp32u* pDst, int ns, int nBits)
{
   int clz = NLZ32u(pSrc[ns-1]);
   Ipp32u hi = 0;
   int n;

   nBits &= 0x1F;

   for(n=ns; n>0; n--) {
      Ipp32u lo = pSrc[n-1];
      Ipp64u tmp = MAKEDWORD(lo,hi);
      tmp <<= nBits;
      pDst[n] = HIDWORD(tmp);
      hi = lo;
   }
   hi = HIDWORD( MAKEDWORD(0,hi) << nBits);
   pDst[0] = hi;

   return (clz<nBits)? ns+1 : ns;
}

/*
// LSR_BNU
//
// Logical Shift Right
//
// Returns new length
//
// Note:
//    nBits <32
//    inplace mode works correct
*/
int LSR_BNU(const Ipp32u* pSrc, Ipp32u* pDst, int ns, int nBits)
{
   int cnz = 32-NLZ32u(pSrc[ns-1]);
   Ipp32u hi = 0;
   Ipp32u n;

   nBits &= 0x1F;

   for(n=ns; n>0; n--) {
      Ipp32u lo = pSrc[n-1];
      Ipp64u tmp = MAKEDWORD(lo,hi);
      tmp >>= nBits;
      pDst[n-1] = LODWORD(tmp);
      hi = lo;
   }

   //return (ns>1 && cnz>nBits)? ns : ns-1;
   return (ns==1 || cnz>nBits)? ns : ns-1;
}

/*
// Returns Most Significant "Digit" of the BNU
// Note:
//    if BNU==0, than size will return
*/
int MSD_BNU(const Ipp32u* pData, int size)
{
   for(; (size>1) && (0==pData[size-1]); size--) ;
   return size;
}

/*
// Returns Last Significant Bit of the BNU
// Note:
//    if BNU==0, than 32*size will return
*/
int LSB_BNU(const Ipp32u* pData, int size)
{
   int n;
   int lsb = 0;
   for(n=0; n<size; n++) {
      if(0==pData[n])
         lsb += 32;
      else {
         Ipp32u x = pData[n];
         for(; 0==(x&1); lsb++, x>>=1) {}
         return lsb;
      }
   }
   return lsb;
}

/*
// Returns Most Significant Bit of the BNU
// Note:
//    if BNU==0, -1 will return
*/
int MSB_BNU(const Ipp32u* pData, int size)
{
   int msb  = size*32 -1;
   int n;
   for(n=size-1; n>=0; n--) {
      if(0==pData[n])
         msb -= 32;
      else {
         Ipp32u x = pData[n];
         for(; 0==(x&0x80000000); msb--, x<<=1) {}
         return msb;
      }
   }
   return msb;
}

/* Returns number of leading zeros */
int NLZ32u(Ipp32u x)
{
   if(x) {
      int nlz = 0;
      if( 0==(x & 0xFFFF0000) ) { nlz+=16; x<<=16; }
      if( 0==(x & 0xFF000000) ) { nlz+= 8; x<<= 8; }
      if( 0==(x & 0xF0000000) ) { nlz+= 4; x<<= 4; }
      if( 0==(x & 0xC0000000) ) { nlz+= 2; x<<= 2; }
      if( 0==(x & 0x80000000) ) { nlz++; }
      return nlz;
   }
   else
      return 32;
}

/* Returns number of trailing zeros */
int NTZ32u(Ipp32u x)
{
   if(x) {
      int ntz = 0;
      if( 0==(x & 0x0000FFFF) ) { ntz+=16; x>>=16; }
      if( 0==(x & 0x000000FF) ) { ntz+= 8; x>>= 8; }
      if( 0==(x & 0x0000000F) ) { ntz+= 4; x>>= 4; }
      if( 0==(x & 0x00000003) ) { ntz+= 2; x>>= 2; }
      if( 0==(x & 0x00000001) ) { ntz++; }
      return ntz;
   }
   else
      return 32;
}

