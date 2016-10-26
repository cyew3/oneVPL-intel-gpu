//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

/* Compare SAD-functions: plain-C vs. intrinsics
*
* Simulate motion search algorithm:
* - both buffers are 8-bits (unsigned char),
* - reference buffer is linear (without srtride) ans aligned
* - image buffer has stride and could be unaligned
**********************************************************************************/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO)


#include <math.h>
//#include <emmintrin.h> //SSE2
//#include <pmmintrin.h> //SSE3 (Prescott) Pentium(R) 4 processor (_mm_lddqu_si128() present )
//#include <tmmintrin.h> // SSSE3
//#include <smmintrin.h> // SSE4.1, Intel(R) Core(TM) 2 Duo
//#include <nmmintrin.h> // SSE4.2, Intel(R) Core(TM) 2 Duo
//#include <wmmintrin.h> // AES and PCLMULQDQ
#include <immintrin.h> // AVX, AVX2
//#include <ammintrin.h> // AMD extention SSE5 ? FMA4

#define ALIGNED_SSE2 ALIGN_DECL(16)

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (const double *)(p)))



//#define load128_unaligned(x)    _mm_lddqu_si128( (const __m128i *)( x ))
//#define load256_unaligned(x) _mm256_lddqu_si256( (const __m256i *)( x ))
#define load128_unaligned(x)    _mm_loadu_si128( (const __m128i *)( x ))
#define load256_unaligned(x) _mm256_loadu_si256( (const __m256i *)( x ))
#if 0// 32
// alignement 32 for "original" data block (typically cached in linear space)
    #define load256_block_data( p)  *(const __m256i *)( p )
    #define load128_block_data( p)  *(const __m128i *)( p )
    #define load256_block_data2( p) *(const __m256i *)( p )
#elif 16
// alignement 16 for "original" data block (typically cached in linear space)
    #define load128_block_data( p)  *(const __m128i *)( p )
    #define load256_block_data( p)  _mm256_loadu_si256( (const __m256i *)( p ))
    #define load256_block_data2( p) *(const __m256i *)( p )
#else
// original block is not aligned at all
    #define load128_block_data( p)  _mm_loadu_si128( (const __m128i *)( p ))
    #define load256_block_data( p)  _mm256_loadu_si256( (const __m256i *)( p ))
    #define load256_block_data2( p) _mm256_loadu_si256( (const __m256i *)( p ))
#endif

#define SAD_CALLING_CONVENTION H265_FASTCALL


#define SAD_PARAMETERS_LIST const unsigned char *image,  const unsigned char *block, int img_stride, int block_stride

#define SAD_PARAMETERS_LOAD const int lx1 = img_stride; \
    const Ipp8u *__restrict blk1 = image; \
    const Ipp8u *__restrict blk2 = block



namespace MFX_HEVC_PP
{

// the 128-bit SSE2 cant be full loaded with 4 bytes rows...
// the layout of block-buffer (32x32 for all blocks sizes or 4x4, 8x8 etc. depending on block-size) is not yet
// defined, for 4x4 whole buffer can be readed at once, and the implementation with two _mm_sad_epu8()
// will be, probably the fastest.

int SAD_CALLING_CONVENTION SAD_4x4_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1   = _mm_cvtsi32_si128( *(const int *) (blk1));
    __m128i tmp1 = _mm_cvtsi32_si128( *(const int *) (blk1 + lx1));

    __m128i tmp2 = _mm_cvtsi32_si128( *(const int *) (blk2));
    __m128i tmp3 = _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride));

    s1   = _mm_unpacklo_epi32(s1, tmp1);
    tmp2 = _mm_unpacklo_epi32(tmp2, tmp3);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2   = _mm_cvtsi32_si128( *(const int *) (blk1));
    __m128i tmp4 = _mm_cvtsi32_si128( *(const int *) (blk1 + lx1));

    __m128i tmp5 = _mm_cvtsi32_si128( *(const int *) (blk2));
    __m128i tmp6 = _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride));

    s2   = _mm_unpacklo_epi32(s2,  tmp4);
    tmp5 = _mm_unpacklo_epi32(tmp5, tmp6);

    s1 = _mm_sad_epu8( s1, tmp2);
    s2 = _mm_sad_epu8( s2, tmp5);

    s1 = _mm_add_epi32( s1, s2);

    return _mm_cvtsi128_si32( s1);
}



int SAD_CALLING_CONVENTION SAD_4x8_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    __m128i tmp1 = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    s1 = _mm_sad_epu8( s1, tmp1);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    __m128i tmp2 = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    s2 = _mm_sad_epu8( s2, tmp2);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i t1   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp1         = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t1 = _mm_sad_epu8( t1, tmp1);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i t2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp2         = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t2 = _mm_sad_epu8( t2, tmp2);

    s1 = _mm_add_epi32( s1, t1);
    s2 = _mm_add_epi32( s2, t2);

    s1 = _mm_add_epi32( s1, s2);

    return _mm_cvtsi128_si32( s1);
}


int SAD_CALLING_CONVENTION SAD_4x16_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    __m128i tmp1 = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    s1 = _mm_sad_epu8( s1, tmp1);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    __m128i tmp2 = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    s2 = _mm_sad_epu8( s2, tmp2);


    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i t1   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp1         = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t1 = _mm_sad_epu8( t1, tmp1);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i t2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp2         = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t2 = _mm_sad_epu8( t2, tmp2);

    s1 = _mm_add_epi32( s1, t1);
    s2 = _mm_add_epi32( s2, t2);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    t1    = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp1  = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t1 = _mm_sad_epu8( t1, tmp1);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    t2     = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t2 = _mm_sad_epu8( t2, tmp2);

    s1 = _mm_add_epi32( s1, t1);
    s2 = _mm_add_epi32( s2, t2);
    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    t1     = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp1   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t1 = _mm_sad_epu8( t1, tmp1);

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    t2     = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
    tmp2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + block_stride)));

    t2 = _mm_sad_epu8( t2, tmp2);

    s1 = _mm_add_epi32( s1, t1);
    s2 = _mm_add_epi32( s2, t2);


    s1 = _mm_add_epi32( s1, s2);

    return _mm_cvtsi128_si32( s1);
}


int SAD_CALLING_CONVENTION SAD_8x4_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s1 = _mm_sad_epu8( s1, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s2 = _mm_sad_epu8(s2, tmp3);
    }


    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


int SAD_CALLING_CONVENTION SAD_8x8_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s1 = _mm_sad_epu8( s1, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s2 = _mm_sad_epu8(s2, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


int SAD_CALLING_CONVENTION SAD_8x16_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s1 = _mm_sad_epu8( s1, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s2 = _mm_sad_epu8(s2, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }
    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


//no difference, use no loop version
#if 1
int SAD_CALLING_CONVENTION SAD_8x32_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s1 = _mm_sad_epu8( s1, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s2 = _mm_sad_epu8(s2, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }
    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }
    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }
    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s1 = _mm_add_epi32( s1, tmp1);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        tmp1 = _mm_sad_epu8(tmp1, tmp3);
        s2 = _mm_add_epi32( s2, tmp1);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}
#else
int SAD_CALLING_CONVENTION SAD_8x32_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s1 = _mm_sad_epu8( s1, tmp3);
    }

    blk1 += 2*lx1;
    blk2 += 2*block_stride;

    __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
    s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
    {
        __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
        tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
        s2 = _mm_sad_epu8(s2, tmp3);
    }

    for(int i=4; i<32; i+=4)
    {
        blk1 += 2*lx1;
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s1 = _mm_add_epi32( s1, tmp1);
        }

        blk1 += 2*lx1;
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s2 = _mm_add_epi32( s2, tmp1);
        }
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}
#endif


#define load_12_bytes(a) _mm_loadh_epi64(_mm_cvtsi32_si128( *(const int *) ((const char*)(a) + 8)), a)
 

int SAD_CALLING_CONVENTION SAD_12x16_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = load_12_bytes( (const __m128i *) (blk1));
    __m128i s2 = load_12_bytes( (const __m128i *) (blk1+lx1));
    s1 = _mm_sad_epu8( s1, load_12_bytes( (const __m128i *) (blk2)));
    s2 = _mm_sad_epu8( s2, load_12_bytes( (const __m128i *) (blk2 + block_stride)));

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load_12_bytes( (const __m128i *) (blk1));
        __m128i tmp2 = load_12_bytes( (const __m128i *) (blk1+lx1));
        tmp1 = _mm_sad_epu8( tmp1, load_12_bytes( (const __m128i *) (blk2 + 2*block_stride)));
        tmp2 = _mm_sad_epu8( tmp2, load_12_bytes( (const __m128i *) (blk2 + 3*block_stride)));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load_12_bytes( (const __m128i *) (blk1));
        __m128i tmp2 = load_12_bytes( (const __m128i *) (blk1+lx1));
        tmp1 = _mm_sad_epu8( tmp1, load_12_bytes( (const __m128i *) (blk2 + 4*block_stride)));
        tmp2 = _mm_sad_epu8( tmp2, load_12_bytes( (const __m128i *) (blk2 + 5*block_stride)));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load_12_bytes( (const __m128i *) (blk1));
        __m128i tmp2 = load_12_bytes( (const __m128i *) (blk1+lx1));
        tmp1 = _mm_sad_epu8( tmp1, load_12_bytes( (const __m128i *) (blk2 + 6*block_stride)));
        tmp2 = _mm_sad_epu8( tmp2, load_12_bytes( (const __m128i *) (blk2 + 7*block_stride)));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load_12_bytes( (const __m128i *) (blk1));
        __m128i tmp2 = load_12_bytes( (const __m128i *) (blk1+lx1));
        tmp1 = _mm_sad_epu8( tmp1, load_12_bytes( (const __m128i *) (blk2 + 8*block_stride)));
        tmp2 = _mm_sad_epu8( tmp2, load_12_bytes( (const __m128i *) (blk2 + 9*block_stride)));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load_12_bytes( (const __m128i *) (blk1));
        __m128i tmp2 = load_12_bytes( (const __m128i *) (blk1+lx1));
        tmp1 = _mm_sad_epu8( tmp1, load_12_bytes( (const __m128i *) (blk2 + 10*block_stride)));
        tmp2 = _mm_sad_epu8( tmp2, load_12_bytes( (const __m128i *) (blk2 + 11*block_stride)));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }


    blk1 += 2*lx1;

    {
        __m128i tmp1 = load_12_bytes( (const __m128i *) (blk1));
        __m128i tmp2 = load_12_bytes( (const __m128i *) (blk1+lx1));
        tmp1 = _mm_sad_epu8( tmp1, load_12_bytes( (const __m128i *) (blk2 + 12*block_stride)));
        tmp2 = _mm_sad_epu8( tmp2, load_12_bytes( (const __m128i *) (blk2 + 13*block_stride)));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }


    blk1 += 2*lx1;

    {
        __m128i tmp1 = load_12_bytes( (const __m128i *) (blk1));
        __m128i tmp2 = load_12_bytes( (const __m128i *) (blk1+lx1));
        tmp1 = _mm_sad_epu8( tmp1, load_12_bytes( (const __m128i *) (blk2 + 14*block_stride)));
        tmp2 = _mm_sad_epu8( tmp2, load_12_bytes( (const __m128i *) (blk2 + 15*block_stride)));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


int SAD_CALLING_CONVENTION SAD_16x4_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = load128_unaligned( blk1);
    __m128i s2 = load128_unaligned( blk1 + lx1);
    s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
    s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + block_stride));

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 2*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 3*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }


    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


int SAD_CALLING_CONVENTION SAD_16x8_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = load128_unaligned( blk1);
    __m128i s2 = load128_unaligned( blk1+lx1);
    s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
    s2 = _mm_sad_epu8( s2, load128_block_data( blk2+block_stride));

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 2*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 3*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 4*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 5*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 6*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 7*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


int SAD_CALLING_CONVENTION SAD_16x12_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = load128_unaligned( blk1);
    __m128i s2 = load128_unaligned( blk1 + lx1);
    s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
    s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + block_stride));

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 2*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 3*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 4*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 5*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 6*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 7*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 8*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 9*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 10*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 11*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


int SAD_CALLING_CONVENTION SAD_16x16_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = load128_unaligned( blk1);
    __m128i s2 = load128_unaligned( blk1 + lx1);
    s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
    s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + block_stride));

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 2*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 3*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 4*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 5*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 6*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 7*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 8*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 9*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 10*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 11*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 12*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 13*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }


    blk1 += 2*lx1;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 14*block_stride));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 15*block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}



int SAD_CALLING_CONVENTION SAD_16x32_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = load128_unaligned( blk1);
    __m128i s2 = load128_unaligned( blk1 + lx1);
    s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
    s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + block_stride));

    for(int i = 2; i < 30; i += 4)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;
        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }
    }

    blk1 += 2*lx1;    
    blk2 += 2*block_stride;

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
        tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride));
        s1 = _mm_add_epi32( s1, tmp1);
        s2 = _mm_add_epi32( s2, tmp2);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}


int SAD_CALLING_CONVENTION SAD_16x64_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    __m128i s1 = load128_unaligned( blk1);
    __m128i s2 = load128_unaligned( blk1 + lx1);
    s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
    s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + block_stride));

    blk1 += 2*lx1;    
    blk2 += 2*block_stride;

    for(int i = 2; i < 62; i += 4)
    {
        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            __m128i tmp3 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
            __m128i tmp4 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            __m128i tmp3 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
            __m128i tmp4 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;
    }

    {
        __m128i tmp1 = load128_unaligned( blk1);
        __m128i tmp2 = load128_unaligned( blk1 + lx1);
        __m128i tmp3 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
        __m128i tmp4 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride));
        s1 = _mm_add_epi32( s1, tmp3);
        s2 = _mm_add_epi32( s2, tmp4);
    }

    s1 = _mm_add_epi32( s1, s2);
    s2 = _mm_movehl_epi64( s2, s1);
    s2 = _mm_add_epi32( s2, s1);

    return _mm_cvtsi128_si32( s2);
}



int SAD_CALLING_CONVENTION SAD_24x32_general_avx2(SAD_PARAMETERS_LIST)
{
    SAD_PARAMETERS_LOAD;

    if( (blk2 - (Ipp8u *)0) & 15 )
    {
        // 8-byte aligned

        __m128i s3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1)), (const __m128i *) (blk1 + lx1));
        s3 = _mm_sad_epu8( s3, _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2)), (const __m128i *) (blk2 + block_stride)));
        __m128i s1 = load128_unaligned( blk1 + 8);
        __m128i s2 = load128_unaligned( blk1 + lx1 + 8);
        s1 = _mm_sad_epu8( s1, load128_block_data( blk2 + 8));
        s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + block_stride + 8));

        for(int i = 2; i < 32; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1)), (const __m128i *) (blk1 + lx1));
                __m128i tmp4 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2)), (const __m128i *) (blk2 + block_stride));
                s3 = _mm_add_epi32( s3, _mm_sad_epu8( tmp3, tmp4));
            }
            {
                __m128i tmp1 = load128_unaligned( blk1 + 8);
                __m128i tmp2 = load128_unaligned( blk1 + lx1 + 8);
                tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 8));
                tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride + 8));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s1 = _mm_add_epi32( s1, s3);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
    else
    {
        // 16-bytes aligned

        __m128i s1 = load128_unaligned( blk1);
        __m128i s2 = load128_unaligned( blk1 + lx1);
        s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
        s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + block_stride));
        __m128i s3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1 + 16)), (const __m128i *) (blk1 + lx1 + 16));
        s3 = _mm_sad_epu8( s3, _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2 + 16)), (const __m128i *) (blk2 + block_stride + 16)));

        for(int i = 2; i < 32; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load128_unaligned( blk1);
                __m128i tmp2 = load128_unaligned( blk1 + lx1);
                tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
                tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1 + 16)), (const __m128i *) (blk1 + lx1 + 16));
                __m128i tmp4 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2 + 16)), (const __m128i *) (blk2 + block_stride + 16));
                s3 = _mm_add_epi32( s3, _mm_sad_epu8( tmp3, tmp4));
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s1 = _mm_add_epi32( s1, s3);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
}





int SAD_CALLING_CONVENTION SAD_32x8_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + block_stride));

    blk1 += 2*lx1;    
    blk2 += 2*block_stride;
    {
        __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
        tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
        tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + block_stride));
        s1 = _mm256_add_epi32( s1, tmp1);
        s2 = _mm256_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;    
    blk2 += 2*block_stride;
    {
        __m256i tmp1 = _mm256_lddqu_si256( (const __m256i *) (blk1));
        __m256i tmp2 = _mm256_lddqu_si256( (const __m256i *) (blk1 + lx1));
        tmp1 = _mm256_sad_epu8( tmp1, *( (const __m256i *) (blk2)));
        tmp2 = _mm256_sad_epu8( tmp2, *( (const __m256i *) (blk2 + block_stride)));
        s1 = _mm256_add_epi32( s1, tmp1);
        s2 = _mm256_add_epi32( s2, tmp2);
    }

    blk1 += 2*lx1;    
    blk2 += 2*block_stride;
    {
        __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
        tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
        tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + block_stride));
        s1 = _mm256_add_epi32( s1, tmp1);
        s2 = _mm256_add_epi32( s2, tmp2);
    }

    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_32x16_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + block_stride));

    for(int i = 0; i < 7; ++i)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }

    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_32x24_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + block_stride));

    for(int i = 0; i < 11; ++i)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }

    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_32x32_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + block_stride));

    for(int i = 0; i < 15; ++i)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }

    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_32x64_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + block_stride));

    for(int i = 0; i < 31; ++i)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }

    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_64x64_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + block_stride));

    {
        __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
        __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
        tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2 + 32));
        tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + 32 + block_stride));
        s1 = _mm256_add_epi32( s1, tmp1);
        s2 = _mm256_add_epi32( s2, tmp2);
    }

    for(int i = 2; i < 64; i += 2)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }


    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_48x64_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    if( (blk2 - (Ipp8u *)0) & 31 )
    {
        __m256i s1 = load256_unaligned( (const __m256i *) (blk1 + 16));
        __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + 16 + lx1));

        s1 = _mm256_sad_epu8( s1, load256_block_data2(blk2 + 16));
        s2 = _mm256_sad_epu8( s2, load256_block_data2(blk2 + 16 + block_stride));

        {
#ifndef __GNUC__
            __m256i tmp1 = _mm256_set_m128i( load128_unaligned( blk1), load128_unaligned( blk1 + lx1));
            __m256i tmp2 = _mm256_set_m128i( load128_block_data( blk2), load128_block_data( blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#else
            __m256i tmp1 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1)), load128_unaligned(blk1 + lx1), 0x1);
            __m256i tmp2 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2)), load128_block_data(blk2 + block_stride), 0x1);
            s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#endif
        }

        for(int i = 2; i < 64; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;

            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 16));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 16 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data2(blk2 + 16));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data2(blk2 + 16 + block_stride));
                s1 = _mm256_add_epi32( s1, tmp1);
                s2 = _mm256_add_epi32( s2, tmp2);
            }
            {
#ifndef __GNUC__
                __m256i tmp1 = _mm256_set_m128i( load128_unaligned( blk1), load128_unaligned( blk1 + lx1));
                __m256i tmp2 = _mm256_set_m128i( load128_block_data( blk2), load128_block_data( blk2 + block_stride));
                s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#else
                __m256i tmp1 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1)), load128_unaligned(blk1 + lx1), 0x1);
                __m256i tmp2 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2)), load128_block_data(blk2 + block_stride), 0x1);
                s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#endif
            }
        }

        s1 = _mm256_add_epi32( s1, s2);

        __m128i a = _mm256_extractf128_si256(s1, 1);
        __m128i b = _mm256_castsi256_si128(s1);
        b = _mm_add_epi32(b, a);
        a = _mm_movehl_epi64( a, b);
        a = _mm_add_epi32( a, b);
        return _mm_cvtsi128_si32( a);
    }
    else
    {
        __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

        s1 = _mm256_sad_epu8( s1, load256_block_data2(blk2));
        s2 = _mm256_sad_epu8( s2, load256_block_data2(blk2 + block_stride));

        {
#ifndef __GNUC__
            __m256i tmp1 = _mm256_set_m128i( load128_unaligned( blk1 + 32), load128_unaligned( blk1 + 32 + lx1));
            __m256i tmp2 = _mm256_set_m128i( load128_block_data( blk2 + 32), load128_block_data( blk2 + 32 + block_stride));
            s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#else
            __m256i tmp1 = _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1 + 32)), load128_unaligned(blk1 + 32 + lx1), 0x1);
            __m256i tmp2 = _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2 + 32)), load128_block_data(blk2 + 32 + block_stride), 0x1);
            s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#endif
        }

        for(int i = 2; i < 64; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;

            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data2(blk2));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data2(blk2 + block_stride));
                s1 = _mm256_add_epi32( s1, tmp1);
                s2 = _mm256_add_epi32( s2, tmp2);
            }
            {
#ifndef __GNUC__
                __m256i tmp1 = _mm256_set_m128i( load128_unaligned( blk1 + 32), load128_unaligned( blk1 + 32 + lx1));
                __m256i tmp2 = _mm256_set_m128i( load128_block_data( blk2 + 32), load128_block_data( blk2 + 32 + block_stride));
                s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#else
                __m256i tmp1 = _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1 + 32)), load128_unaligned(blk1 + 32 + lx1), 0x1);
                __m256i tmp2 = _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2 + 32)), load128_block_data(blk2 + 32 + block_stride), 0x1);
                s1 = _mm256_add_epi32( s1, _mm256_sad_epu8( tmp1, tmp2));
#endif
            }
        }

        s1 = _mm256_add_epi32( s1, s2);

        __m128i a = _mm256_extractf128_si256(s1, 1);
        __m128i b = _mm256_castsi256_si128(s1);
        b = _mm_add_epi32(b, a);
        a = _mm_movehl_epi64( a, b);
        a = _mm_add_epi32( a, b);
        return _mm_cvtsi128_si32( a);
    }
}



int SAD_CALLING_CONVENTION SAD_64x48_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data(blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data(blk2 + block_stride));

    {
        __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
        __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
        tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
        tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + block_stride));
        s1 = _mm256_add_epi32( s1, tmp1);
        s2 = _mm256_add_epi32( s2, tmp2);
    }

    for(int i = 2; i < 48; i += 2)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }


    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_64x32_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data(blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data(blk2 + block_stride));

    {
        __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
        __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
        tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
        tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + block_stride));
        s1 = _mm256_add_epi32( s1, tmp1);
        s2 = _mm256_add_epi32( s2, tmp2);
    }

    for(int i = 2; i < 32; i += 2)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }


    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}



int SAD_CALLING_CONVENTION SAD_64x16_general_avx2(SAD_PARAMETERS_LIST) //OK
{
    SAD_PARAMETERS_LOAD;

    __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
    __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

    s1 = _mm256_sad_epu8( s1, load256_block_data(blk2));
    s2 = _mm256_sad_epu8( s2, load256_block_data(blk2 + block_stride));

    {
        __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
        __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
        tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
        tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + block_stride));
        s1 = _mm256_add_epi32( s1, tmp1);
        s2 = _mm256_add_epi32( s2, tmp2);
    }

    for(int i = 2; i < 16; i += 2)
    {
        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }
    }


    s1 = _mm256_add_epi32( s1, s2);

    __m128i a = _mm256_extractf128_si256(s1, 1);
    __m128i b = _mm256_castsi256_si128(s1);
    b = _mm_add_epi32(b, a);
    a = _mm_movehl_epi64( a, b);
    a = _mm_add_epi32( a, b);
    return _mm_cvtsi128_si32( a);
}


    //static 
    //int getSAD_ref(const unsigned char *image,  const unsigned char *block, int img_stride, int size)
    //{
    //    int sad = 0;

    //    for (int y=0; y<size; y++)
    //    {
    //        const unsigned char *p1 = image + y * img_stride;
    //        const unsigned char *p2 = block + y * size;   // stride in block buffer = size (linear buffer)

    //        for (int x=0; x<size; x++)
    //        {
    //            sad += abs(p1[x] - p2[x]);
    //        }
    //    }

    //    return sad;
    //}



    //static
    //int getSAD_ref2_block (const unsigned char *image,  const unsigned char *block, int img_stride, int SizeX, int SizeY)
    //{
    //    int sad = 0;

    //    for (int y=0; y<SizeY; y++)
    //    {
    //        const unsigned char *p1 = image + y * img_stride;
    //        const unsigned char *p2 = block + y * block_stride;   // stride in block buffer = size (linear buffer)

    //        for (int x=0; x<SizeX; x++)
    //        {
    //            sad += abs(p1[x] - p2[x]);
    //        }
    //    }

    //    return sad;
    //}
    //
    //static
    //int getSAD_avx2(const unsigned char *image,  const unsigned char *ref, int stride, int Size)
    //{
    //    if (Size == 4)
    //        return SAD_4x4_general_avx2(image,  ref, stride);
    //    else if (Size == 8)
    //        return SAD_8x8_avx2(image,  ref, stride);
    //    else if (Size == 16)
    //        return SAD_16x16_avx2(image,  ref, stride);
    //    return SAD_32x32_avx2(image,  ref, stride);
    //}

#ifndef MFX_TARGET_OPTIMIZATION_AUTO

    int h265_SAD_MxN_general_8u(const unsigned char *image,  int stride_img, const unsigned char *ref, int stride_ref, int SizeX, int SizeY)
{
        if (SizeX == 4)
        {
            if(SizeY == 4) { return SAD_4x4_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 8) { return SAD_4x8_general_avx2(image,  ref, stride_img, stride_ref); }
            else           { return SAD_4x16_general_avx2(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 8)
        {
            if(SizeY ==  4) { return SAD_8x4_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY ==  8) { return SAD_8x8_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 16) { return SAD_8x16_general_avx2(image,  ref, stride_img, stride_ref); }
            else             { return SAD_8x32_general_avx2(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 12)
        {
            return SAD_12x16_general_avx2(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 16)
        {
            if(SizeY ==  4) { return SAD_16x4_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY ==  8) { return SAD_16x8_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 12) { return SAD_16x12_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 16) { return SAD_16x16_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 32) { return SAD_16x32_general_avx2(image,  ref, stride_img, stride_ref); }
            else            { return SAD_16x64_general_avx2(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 24)
        {
            return SAD_24x32_general_avx2(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 32)
        {
            if(SizeY ==  8) { return SAD_32x8_general_avx2(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 16) { return SAD_32x16_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 24) { return SAD_32x24_general_avx2(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 32) { return SAD_32x32_general_avx2(image,  ref, stride_img, stride_ref);}
            else            { return SAD_32x64_general_avx2(image,  ref, stride_img, stride_ref);}
        }
        else if (SizeX == 48)
        {
            return SAD_48x64_general_avx2(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 64)
        {
            if(SizeY == 16) { return SAD_64x16_general_avx2(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 32) { return SAD_64x32_general_avx2(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 48) { return SAD_64x48_general_avx2(image,  ref, stride_img, stride_ref);}
            else            { return SAD_64x64_general_avx2(image,  ref, stride_img, stride_ref); }
        }
        
        else return -1;

    } // int h265_SAD_MxN_general_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)

#endif // defined(MFX_TARGET_OPTIMIZATION_AUTO)

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

ALIGN_DECL(32) static const short ones16s[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

template <int height>
static int SAD_4xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm7;

    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 4) {
        /* load 4x4 src block into two registers */
        xmm0 = _mm_loadl_epi64((__m128i *)(src + 0*src_stride));
        xmm0 = _mm_loadh_epi64(xmm0, (src + 1*src_stride));
        xmm1 = _mm_loadl_epi64((__m128i *)(src + 2*src_stride));
        xmm1 = _mm_loadh_epi64(xmm1, (src + 3*src_stride));
        src += 4*src_stride;

        /* load 4x4 ref block into two registers */
        xmm2 = _mm_loadl_epi64((__m128i *)(ref + 0*ref_stride));
        xmm2 = _mm_loadh_epi64(xmm2, (ref + 1*ref_stride));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref + 2*ref_stride));
        xmm3 = _mm_loadh_epi64(xmm3, (ref + 3*ref_stride));
        ref += 4*ref_stride;

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, xmm2);
        xmm1 = _mm_sub_epi16(xmm1, xmm3);
        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        
        /* add to 32-bit accumulator (leaving as 16-bit lanes could overflow depending on height and bit depth) */
        xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
        xmm7 = _mm_add_epi32(xmm7, xmm0);
    }

    /* add up columns */
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x0e));
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x01));

    return _mm_cvtsi128_si32(xmm7);
}

template <int height>
static int SAD_8xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm7;

    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 2) {
        /* load 8x2 src block into two registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src + 0*src_stride));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 1*src_stride));
        src += 2*src_stride;

        /* load 8x2 ref block into two registers */
        xmm2 = _mm_loadu_si128((__m128i *)(ref + 0*ref_stride));
        xmm3 = _mm_loadu_si128((__m128i *)(ref + 1*ref_stride));
        ref += 2*ref_stride;

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, xmm2);
        xmm1 = _mm_sub_epi16(xmm1, xmm3);
        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        
        /* add to 32-bit accumulator */
        xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
        xmm7 = _mm_add_epi32(xmm7, xmm0);
    }

    /* add up columns */
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x0e));
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x01));

    return _mm_cvtsi128_si32(xmm7);
}

template <int height>
static int SAD_12xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm4, xmm5, xmm6, xmm7;

    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 2) {
        /* load 12x2 src block into three registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src + 0*src_stride));
        xmm1 = _mm_loadl_epi64((__m128i *)(src + 0*src_stride + 8));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 1*src_stride));
        xmm1 = _mm_loadh_epi64(xmm1, (src + 1*src_stride + 8));
        src += 2*src_stride;

        /* load 12x2 ref block into two registers */
        xmm4 = _mm_loadu_si128((__m128i *)(ref + 0*ref_stride));
        xmm5 = _mm_loadl_epi64((__m128i *)(ref + 0*ref_stride + 8));
        xmm6 = _mm_loadu_si128((__m128i *)(ref + 1*ref_stride));
        xmm5 = _mm_loadh_epi64(xmm5, (ref + 1*ref_stride + 8));
        ref += 2*ref_stride;

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, xmm4);
        xmm1 = _mm_sub_epi16(xmm1, xmm5);
        xmm2 = _mm_sub_epi16(xmm2, xmm6);
        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);
        
        /* add to 32-bit accumulator */
        xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
        xmm7 = _mm_add_epi32(xmm7, xmm0);
    }

    /* add up columns */
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x0e));
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x01));

    return _mm_cvtsi128_si32(xmm7);
}

template <int height>
static int SAD_16xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm7;

    _mm256_zeroupper();

    ymm7 = _mm256_setzero_si256();

    for (h = 0; h < height; h += 2) {
        /* load 16x2 src block into two registers */
        ymm0 = _mm256_loadu_si256((__m256i *)(src + 0*src_stride));
        ymm1 = _mm256_loadu_si256((__m256i *)(src + 1*src_stride));
        src += 2*src_stride;

        /* load 16x2 ref block into two registers */
        ymm2 = _mm256_loadu_si256((__m256i *)(ref + 0*ref_stride));
        ymm3 = _mm256_loadu_si256((__m256i *)(ref + 1*ref_stride));
        ref += 2*ref_stride;

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, ymm2);
        ymm1 = _mm256_sub_epi16(ymm1, ymm3);
        ymm0 = _mm256_abs_epi16(ymm0);
        ymm1 = _mm256_abs_epi16(ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm1);

        /* add to 32-bit accumulator */
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)ones16s);
        ymm7 = _mm256_add_epi32(ymm7, ymm0);
    }

    /* add up columns */
    ymm7 = _mm256_add_epi32(ymm7, _mm256_shuffle_epi32(ymm7, 0x0e));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_shuffle_epi32(ymm7, 0x01));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_permute2x128_si256(ymm7, ymm7, 0x01));

    return _mm_cvtsi128_si32(mm128(ymm7));
}

template <int height>
static int SAD_24xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m128i xmm0;
    __m256i ymm0, ymm1, ymm2, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    ymm7 = _mm256_setzero_si256();

    for (h = 0; h < height; h += 2) {
        /* load 24x2 src block into three registers  */
        ymm0 = _mm256_loadu_si256((__m256i *)(src + 0*src_stride));
        ymm1 = mm256(_mm_loadu_si128((__m128i *)(src + 0*src_stride + 16)));
        ymm2 = _mm256_loadu_si256((__m256i *)(src + 1*src_stride));
        xmm0 = _mm_loadu_si128((__m128i *)(src + 1*src_stride + 16));
        ymm1 = _mm256_permute2x128_si256(ymm1, mm256(xmm0), 0x20);
        src += 2*src_stride;

        /* load 24x2 ref block into three registers */
        ymm4 = _mm256_loadu_si256((__m256i *)(ref + 0*ref_stride));
        ymm5 = mm256(_mm_loadu_si128((__m128i *)(ref + 0*ref_stride + 16)));
        ymm6 = _mm256_loadu_si256((__m256i *)(ref + 1*ref_stride));
        xmm0 = _mm_loadu_si128((__m128i *)(ref + 1*ref_stride + 16));
        ymm5 = _mm256_permute2x128_si256(ymm5, mm256(xmm0), 0x20);
        ref += 2*ref_stride;

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, ymm4);
        ymm1 = _mm256_sub_epi16(ymm1, ymm5);
        ymm2 = _mm256_sub_epi16(ymm2, ymm6);

        ymm0 = _mm256_abs_epi16(ymm0);
        ymm1 = _mm256_abs_epi16(ymm1);
        ymm2 = _mm256_abs_epi16(ymm2);

        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm2);

        /* add to 32-bit accumulator */
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)ones16s);
        ymm7 = _mm256_add_epi32(ymm7, ymm0);
    }

    /* add up columns */
    ymm7 = _mm256_add_epi32(ymm7, _mm256_shuffle_epi32(ymm7, 0x0e));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_shuffle_epi32(ymm7, 0x01));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_permute2x128_si256(ymm7, ymm7, 0x01));

    return _mm_cvtsi128_si32(mm128(ymm7));
}

template <int height>
static int SAD_32xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm_acc;

    _mm256_zeroupper();

    ymm_acc = _mm256_setzero_si256();

    for (h = 0; h < height; h += 2) {
        /* load 32x2 src block into four registers  */
        ymm0 = _mm256_loadu_si256((__m256i *)(src + 0*src_stride +  0));
        ymm1 = _mm256_loadu_si256((__m256i *)(src + 0*src_stride + 16));
        ymm2 = _mm256_loadu_si256((__m256i *)(src + 1*src_stride +  0));
        ymm3 = _mm256_loadu_si256((__m256i *)(src + 1*src_stride + 16));
        src += 2*src_stride;

        /* load 32x2 ref block into four registers  */
        ymm4 = _mm256_loadu_si256((__m256i *)(ref + 0*ref_stride +  0));
        ymm5 = _mm256_loadu_si256((__m256i *)(ref + 0*ref_stride + 16));
        ymm6 = _mm256_loadu_si256((__m256i *)(ref + 1*ref_stride +  0));
        ymm7 = _mm256_loadu_si256((__m256i *)(ref + 1*ref_stride + 16));
        ref += 2*ref_stride;

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, ymm4);
        ymm1 = _mm256_sub_epi16(ymm1, ymm5);
        ymm2 = _mm256_sub_epi16(ymm2, ymm6);
        ymm3 = _mm256_sub_epi16(ymm3, ymm7);

        ymm0 = _mm256_abs_epi16(ymm0);
        ymm1 = _mm256_abs_epi16(ymm1);
        ymm2 = _mm256_abs_epi16(ymm2);
        ymm3 = _mm256_abs_epi16(ymm3);

        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm0 = _mm256_add_epi16(ymm0, ymm3);

        /* add to 32-bit accumulator */
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)ones16s);
        ymm_acc = _mm256_add_epi32(ymm_acc, ymm0);
    }

    /* add up columns */
    ymm_acc = _mm256_add_epi32(ymm_acc, _mm256_shuffle_epi32(ymm_acc, 0x0e));
    ymm_acc = _mm256_add_epi32(ymm_acc, _mm256_shuffle_epi32(ymm_acc, 0x01));
    ymm_acc = _mm256_add_epi32(ymm_acc, _mm256_permute2x128_si256(ymm_acc, ymm_acc, 0x01));

    return _mm_cvtsi128_si32(mm128(ymm_acc));
}

template <int height>
static int SAD_48xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m256i ymm0, ymm1, ymm2, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    ymm7 = _mm256_setzero_si256();

    for (h = 0; h < height; h++) {
        /* load 48x1 src block into three registers  */
        ymm0 = _mm256_loadu_si256((__m256i *)(src +  0));
        ymm1 = _mm256_loadu_si256((__m256i *)(src + 16));
        ymm2 = _mm256_loadu_si256((__m256i *)(src + 32));
        src += src_stride;

        /* load 48x1 ref block into three registers  */
        ymm4 = _mm256_loadu_si256((__m256i *)(ref +  0));
        ymm5 = _mm256_loadu_si256((__m256i *)(ref + 16));
        ymm6 = _mm256_loadu_si256((__m256i *)(ref + 32));
        ref += ref_stride;

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, ymm4);
        ymm1 = _mm256_sub_epi16(ymm1, ymm5);
        ymm2 = _mm256_sub_epi16(ymm2, ymm6);

        ymm0 = _mm256_abs_epi16(ymm0);
        ymm1 = _mm256_abs_epi16(ymm1);
        ymm2 = _mm256_abs_epi16(ymm2);

        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm2);

        /* add to 32-bit accumulator */
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)ones16s);
        ymm7 = _mm256_add_epi32(ymm7, ymm0);
    }

    /* add up columns */
    ymm7 = _mm256_add_epi32(ymm7, _mm256_shuffle_epi32(ymm7, 0x0e));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_shuffle_epi32(ymm7, 0x01));
    ymm7 = _mm256_add_epi32(ymm7, _mm256_permute2x128_si256(ymm7, ymm7, 0x01));

    return _mm_cvtsi128_si32(mm128(ymm7));
}

template <int height>
static int SAD_64xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride)
{
    int h;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm_acc;

    _mm256_zeroupper();

    ymm_acc = _mm256_setzero_si256();

    for (h = 0; h < height; h++) {
        /* load 64x1 src block into four registers */
        ymm0 = _mm256_loadu_si256((__m256i *)(src +  0));
        ymm1 = _mm256_loadu_si256((__m256i *)(src + 16));
        ymm2 = _mm256_loadu_si256((__m256i *)(src + 32));
        ymm3 = _mm256_loadu_si256((__m256i *)(src + 48));
        src += src_stride;

        /* load 64x1 ref block into four registers */
        ymm4 = _mm256_loadu_si256((__m256i *)(ref +  0));
        ymm5 = _mm256_loadu_si256((__m256i *)(ref + 16));
        ymm6 = _mm256_loadu_si256((__m256i *)(ref + 32));
        ymm7 = _mm256_loadu_si256((__m256i *)(ref + 48));
        ref += ref_stride;

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, ymm4);
        ymm1 = _mm256_sub_epi16(ymm1, ymm5);
        ymm2 = _mm256_sub_epi16(ymm2, ymm6);
        ymm3 = _mm256_sub_epi16(ymm3, ymm7);

        ymm0 = _mm256_abs_epi16(ymm0);
        ymm1 = _mm256_abs_epi16(ymm1);
        ymm2 = _mm256_abs_epi16(ymm2);
        ymm3 = _mm256_abs_epi16(ymm3);

        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm0 = _mm256_add_epi16(ymm0, ymm3);

        /* add to 32-bit accumulator */
        ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)ones16s);
        ymm_acc = _mm256_add_epi32(ymm_acc, ymm0);
    }

    /* add up columns */
    ymm_acc = _mm256_add_epi32(ymm_acc, _mm256_shuffle_epi32(ymm_acc, 0x0e));
    ymm_acc = _mm256_add_epi32(ymm_acc, _mm256_shuffle_epi32(ymm_acc, 0x01));
    ymm_acc = _mm256_add_epi32(ymm_acc, _mm256_permute2x128_si256(ymm_acc, ymm_acc, 0x01));

    return _mm_cvtsi128_si32(mm128(ymm_acc));

}

int H265_FASTCALL MAKE_NAME(h265_SAD_MxN_general_16s)(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride, Ipp32s width, Ipp32s height)
{
    switch (width) {
    case 4:
        if (height == 4)        return SAD_4xN< 4> (src, src_stride, ref, ref_stride);
        else if (height == 8)   return SAD_4xN< 8> (src, src_stride, ref, ref_stride);
        else if (height == 16)  return SAD_4xN<16> (src, src_stride, ref, ref_stride);
        break;
    case 8:
        if (height == 4)        return SAD_8xN< 4> (src, src_stride, ref, ref_stride);
        else if (height == 8)   return SAD_8xN< 8> (src, src_stride, ref, ref_stride);
        else if (height == 16)  return SAD_8xN<16> (src, src_stride, ref, ref_stride);
        else if (height == 32)  return SAD_8xN<32> (src, src_stride, ref, ref_stride);
        break;
    case 12:
        if (height == 16)       return SAD_12xN<16>(src, src_stride, ref, ref_stride);
        break;
    case 16:
        if (height == 4)        return SAD_16xN< 4>(src, src_stride, ref, ref_stride);
        else if (height == 8)   return SAD_16xN< 8>(src, src_stride, ref, ref_stride);
        else if (height == 12)  return SAD_16xN<12>(src, src_stride, ref, ref_stride);
        else if (height == 16)  return SAD_16xN<16>(src, src_stride, ref, ref_stride);
        else if (height == 32)  return SAD_16xN<32>(src, src_stride, ref, ref_stride);
        else if (height == 64)  return SAD_16xN<64>(src, src_stride, ref, ref_stride);
        break;
    case 24:
        if (height == 32)       return SAD_24xN<32>(src, src_stride, ref, ref_stride);
        break;
    case 32:
        if (height == 8)        return SAD_32xN< 8>(src, src_stride, ref, ref_stride);
        else if (height == 16)  return SAD_32xN<16>(src, src_stride, ref, ref_stride);
        else if (height == 24)  return SAD_32xN<24>(src, src_stride, ref, ref_stride);
        else if (height == 32)  return SAD_32xN<32>(src, src_stride, ref, ref_stride);
        else if (height == 64)  return SAD_32xN<64>(src, src_stride, ref, ref_stride);
        break;
    case 48:
        if (height == 64)       return SAD_48xN<64>(src, src_stride, ref, ref_stride);
        break;
    case 64:
        if (height == 16)       return SAD_64xN<16>(src, src_stride, ref, ref_stride);
        else if (height == 32)  return SAD_64xN<32>(src, src_stride, ref, ref_stride);
        else if (height == 48)  return SAD_64xN<48>(src, src_stride, ref, ref_stride);
        else if (height == 64)  return SAD_64xN<64>(src, src_stride, ref, ref_stride);
        break;
    default:
        break;
    }

    /* error - unsupported size */
    VM_ASSERT(0);
    return -1;
}

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO)
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
