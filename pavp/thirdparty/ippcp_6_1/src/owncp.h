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
//  Purpose:
//
//  Author(s): Alexey Korchuganov
//
//  Created: 2-Aug-1999 15:40
//
//  Some addition for ippCP especial by Sergey Kirillov
*/
#ifndef __OWNCP_H__
#define __OWNCP_H__

#ifndef __OWNDEFS_H__
  #include "owndefs.h"
#endif

#ifndef __IPPCP_H__
  #include "ippcp.h"
#endif

#ifdef _MSC_VER
  #pragma warning( disable : 4711 4206)
#endif

#ifdef _WIN32_WCE
  #pragma warning( disable : 4206 4710 4115)
#endif


#include <stdlib.h>

#include "pcpvariant.h"
//#include "pcpvariant_mapd.h"

/*
// Common ippCP Macros
*/

/* size of cache line (bytes) */
#if   ((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2))
#  define CACHE_LINE_SIZE     (32)
#elif ((_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
       (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
       (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9))
#  define CACHE_LINE_SIZE     (64)
#elif ((_IPP32E==_IPP32E_M7) || \
       (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
       (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))
#  define CACHE_LINE_SIZE     (64)
#elif (_IPP64==_IPP64_I7)
#  define CACHE_LINE_SIZE     (64)
#else
#  define CACHE_LINE_SIZE      (1)
#endif

/* swap data & pointers */
#define SWAP_CPTR(pX,pY)   { const void* internPtr=(pX); (pX)=(pY); (pY)=internPtr; }
#define SWAP_PTR(pX,pY)    {       void* internPtr=(pX); (pX)=(pY); (pY)=internPtr; }
#define SWAP(x,y)          (x)^=(y); (y)^=(x); (x)^=(y)

/* alignment value */
#define ALIGN_VAL ((int)sizeof(void*))

/* bitsize */
#define BYTESIZE     (8)
#define BITSIZE(x)   ((int)(sizeof(x)*BYTESIZE))

/* bit length -> byte/word length conversion */
#define BITS2WORD8_SIZE(x)  (((x)+ 7)>>3)
#define BITS2WORD16_SIZE(x) (((x)+15)>>4)
#define BITS2WORD32_SIZE(x) (((x)+31)>>5)

/* WORD and DWORD manipulators */
#define LODWORD(x)    ((Ipp32u)(x))
#define HIDWORD(x)    ((Ipp32u)(((Ipp64u)(x) >>32) & 0xFFFFFFFF))

#define MAKEHWORD(bLo,bHi) ((Ipp16u)(((Ipp8u)(bLo))  | ((Ipp16u)((Ipp8u)(bHi))) << 8))
#define MAKEWORD(hLo,hHi)  ((Ipp32u)(((Ipp16u)(hLo)) | ((Ipp32u)((Ipp16u)(hHi))) << 16))
#define MAKEDWORD(wLo,wHi) ((Ipp64u)(((Ipp32u)(wLo)) | ((Ipp64u)((Ipp32u)(wHi))) << 32))

/* hexString <-> Ipp32u conversion */
#define HSTRING_TO_U32(ptrByte)  \
         (((ptrByte)[0]) <<24)   \
        +(((ptrByte)[1]) <<16)   \
        +(((ptrByte)[2]) <<8)    \
        +((ptrByte)[3])
#define U32_TO_HSTRING(ptrByte, x)  \
   (ptrByte)[0] = (Ipp8u)((x)>>24); \
   (ptrByte)[1] = (Ipp8u)((x)>>16); \
   (ptrByte)[2] = (Ipp8u)((x)>>8);  \
   (ptrByte)[3] = (Ipp8u)(x)

/* 32-bit mask for MSB of nbits-sequence */
#define MAKEMASK32(nbits) (0xFFFFFFFF >>((32 - ((nbits)&0x1F)) &0x1F))

/* Logical Shifts (right and left) of WORD */
#define LSR32(x,nBits)  ((x)>>(nBits))
#define LSL32(x,nBits)  ((x)<<(nBits))

/* Rorate (right and left) of WORD */
#if defined(_MSC_VER)
#  define ROR32(x, nBits)  _lrotr((x),(nBits))
#  define ROL32(x, nBits)  _lrotl((x),(nBits))
#else
#  define ROR32(x, nBits) (LSR32((x),(nBits)) | LSL32((x),32-(nBits)))
#  define ROL32(x, nBits) ROR32((x),(32-(nBits)))
#endif

/* Logical Shifts (right and left) of DWORD */
#define LSR64(x,nBits)  ((x)>>(nBits))
#define LSL64(x,nBits)  ((x)<<(nBits))

/* Rorate (right and left) of DWORD */
#define ROR64(x, nBits) (LSR64((x),(nBits)) | LSL64((x),64-(nBits)))
#define ROL64(x, nBits) ROR64((x),(64-(nBits)))

#define ENDIANNESS(x) ((ROR32((x), 24) & 0x00ff00ff) | (ROR32((x), 8) & 0xff00ff00))

#define IPP_MAKE_MULTIPLE_OF_8(x) ((x) = ((x)+7)&(~7))
#define IPP_MAKE_MULTIPLE_OF_16(x) ((x) = ((x)+15)&(~15))

/* 64-bit constant */
#if !defined(__GNUC__)
   #define CONST_64(x)  (x) /*(x##i64)*/
#else
   #define CONST_64(x)  (x##LL)
#endif

#if defined(_MSC_VER)
#pragma warning( disable : 4514 )
#endif

#endif /* __OWNCP_H__ */
/* ////////////////////////// End of file "owncp.h" ////////////////////////// */
