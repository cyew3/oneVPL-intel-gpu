//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: Def.h
 *
 * Basic definitions and macros 
 * 
 ********************************************************************************/
#ifndef _DEF_H_
#define _DEF_H_

#include <stdlib.h>
#include <string.h>
#include "ipps.h"

//Define number of slices per frame for slice-based processing
#define NSLICES			8

//Define blocksize for skin detection
#define BLOCKSIZE		8

//Define limits for segment tracking
#define NUM_SEG_TRACK_FRMS		8
#define MAX_NUM_TRACK_SEGS		32

//Defines uint as unsigned int
typedef unsigned int  uint;

//Defines BYTE as unsigned char
typedef unsigned char BYTE;

typedef int BOOL;

typedef unsigned short WORD;

//Define NULL if undefined
#ifndef NULL
#define NULL    0
#endif

//Defines MAX macro
#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

//Defines MIN macro
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

//Defines ABS macro
#ifndef ABS
#define ABS(a) (((a) < 0) ? (-(a)) : (a))
#endif

//Defines macro for memory allocation
#ifndef INIT_MEMORY_C
#define INIT_MEMORY_C(_mem, _val, _size, _type) {_mem = (_type *)malloc((_size) * sizeof(_type)); if (_mem != NULL) memset(_mem, _val, (_size) * sizeof(_type)); }
#endif

//Defines macro for memory release
#ifndef DEINIT_MEMORY_C
#define DEINIT_MEMORY_C(_val) {if (_val != NULL) free(_val); _val = NULL;}
#endif

//Define CLIP macro
#ifndef CLIP
#define  CLIP(a,_min_,_max_)  (((a) > (_max_)) ? (_max_) : MAX(a,_min_))
#endif

//Defines ceiling to nearest multiple of 16
#define Allign32(a) ((a)+31)&~31
#define Allign16(a) ((a)+15)&~15
#define Allign8(a) ((a)+7)&~7


//Defines macro to check whether x belongs to the segment [a, b]
#ifndef INSIDE
#define INSIDE(x, a, b) (((x) >= (a)) && ((x) <= (b)))
#endif

//Defines macro to check if the specified point is located in the inner area of the frame
#ifndef INNER_POINT
#define INNER_POINT(y, x, h, w) (INSIDE(y, 0, (h)-1) && INSIDE(x, 0, (w)-1))
#endif


#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __FS_ALIGN64 __declspec (align(64))
#define __FS_ALIGN32 __declspec (align(32))
#define __FS_ALIGN16 __declspec (align(16))
#define __FS_ALIGN8 __declspec (align(8))
#elif defined(__GNUC__)
#define __FS_ALIGN64 __attribute__ ((aligned (64)))
#define __FS_ALIGN32 __attribute__ ((aligned (32)))
#define __FS_ALIGN16 __attribute__ ((aligned (16)))
#define __FS_ALIGN8 __attribute__ ((aligned (8)))
#else
#define __FS_ALIGN64
#define __FS_ALIGN32
#define __FS_ALIGN16
#define __FS_ALIGN8
#endif

#define FS_UNREFERENCED_PARAMETER(X) (X)

extern int g_FS_OPT_init;
extern int g_FS_OPT_AVX2;
extern int g_FS_OPT_SSE4;

// Use for all memcpy (secure)
static inline int memcpy_byte_s(void* pDst, size_t nDstSize, const void* pSrc, size_t nCount) 
{
#if defined(_WIN32) || defined(_WIN64)
    return memcpy_s(pDst, nDstSize, pSrc, nCount);
#else
    ippsCopy_8u((Ipp8u*)pSrc, (Ipp8u*)pDst, nCount);
    return 0;
#endif
}

// Use only for small memcpy (performance)
static inline void memcpy_byte_p( void* dst, const void* src, int len )
{
    ippsCopy_8u((const Ipp8u*)src, (Ipp8u*)dst, len);
}

#endif
