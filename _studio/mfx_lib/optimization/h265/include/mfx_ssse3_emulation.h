//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

/* this file needs to be included AFTER all instances of #include <intrin.h> (or variations) */

#ifndef _MFX_SSSE3_EMULATION_H
#define _MFX_SSSE3_EMULATION_H

/* comment this out to disable emulated intrinsics */
#ifdef MFX_EMULATE_SSSE3

/* force the MM_LOAD_EPI64 workaround to be enabled in release build too
 * the "real" pmovzx instructions can take an unaligned m64 address as the src 
 *   but the emulated version expects a 128-bit register
 */
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <immintrin.h>

/* SSE4 intrinsics to be emulated with SSSE3 ops */
#define _mm_cvtepu8_epi16   _mm_cvtepu8_epi16_SSSE3
#define _mm_cvtepi16_epi32  _mm_cvtepi16_epi32_SSSE3
#define _mm_blendv_epi8     _mm_blendv_epi8_SSSE3
#define _mm_extract_epi8    _mm_extract_epi8_SSSE3
#define _mm_extract_epi16   _mm_extract_epi16_SSSE3
#define _mm_extract_epi32   _mm_extract_epi32_SSSE3
#define _mm_insert_epi8     _mm_insert_epi8_SSSE3
#define _mm_insert_epi32    _mm_insert_epi32_SSSE3

#endif

/* zero-extend 8 bits to 16 bits: unpack vs. zero register */
static __inline __m128i _mm_cvtepu8_epi16_SSSE3(__m128i a)
{
    return _mm_unpacklo_epi8(a, _mm_setzero_si128());
}

/* sign-extend 16 bits to 32 bits: unpack vs. zero register, sll, then sra */
static __inline __m128i _mm_cvtepi16_epi32_SSSE3(__m128i a)
{
    __m128i x;

    x = _mm_unpacklo_epi16(a, _mm_setzero_si128());
    x = _mm_slli_epi32(x, 16);
    
    return _mm_srai_epi32(x, 16);
}

/* blend bytes from v2 into v1, selected based on high bit of corresponding byte in mask */
static __inline __m128i _mm_blendv_epi8_SSSE3(__m128i v1, __m128i v2, __m128i mask)
{
    __m128i mask2;

    /* replicate high bit of each byte into all bits (0 > anything that starts with a 1) */
    mask2 = _mm_setzero_si128();
    mask2 = _mm_cmpgt_epi8(mask2, mask);
    v2 = _mm_and_si128(v2, mask2);
    mask2 = _mm_andnot_si128(mask2, v1);

    return _mm_or_si128(mask2, v2);
}

/* extract one byte from xmm register: store 16 bytes to stack, read back the byte we want */
static __inline int _mm_extract_epi8_SSSE3(__m128i src, const int ndx)
{
    ALIGN_DECL(16) unsigned char src8[16];

    _mm_store_si128((__m128i *)src8, src);

    return (int)(src8[ndx]);
}

/* extract one word from xmm register: store 8 words to stack, read back the word we want */
static __inline int _mm_extract_epi16_SSSE3(__m128i src, int ndx)
{
    ALIGN_DECL(16) unsigned short src16[8];

    _mm_store_si128((__m128i *)src16, src);

    return (int)(src16[ndx]);
}

/* extract one dword from xmm register: store 4 dwords to stack, read back the dword we want */
static __inline int _mm_extract_epi32_SSSE3(__m128i src, const int ndx)
{
    ALIGN_DECL(16) unsigned int src32[4];

    _mm_store_si128((__m128i *)src32, src);

    return (int)(src32[ndx]);
}

/* insert one byte into xmm register: store 16 bytes to stack, insert the byte we want, read back whole xmm register */
static __inline __m128i _mm_insert_epi8_SSSE3(__m128i s1, int s2, const int ndx)
{
    ALIGN_DECL(16) unsigned char src8[16];

    _mm_store_si128((__m128i *)src8, s1);
    src8[ndx] = (unsigned char)s2;
    s1 = _mm_load_si128((__m128i *)src8);
    
    return s1;
}

/* insert one dword into xmm register: store 4 dwords to stack, insert the dword we want, read back whole xmm register */
static __inline __m128i _mm_insert_epi32_SSSE3(__m128i s1, int s2, int ndx)
{
    ALIGN_DECL(16) unsigned int src32[4];

    _mm_store_si128((__m128i *)src32, s1);
    src32[ndx] = (unsigned int)s2;
    s1 = _mm_load_si128((__m128i *)src32);
    
    return s1;
}

#endif /* _MFX_SSSE3_EMULATION_H */
