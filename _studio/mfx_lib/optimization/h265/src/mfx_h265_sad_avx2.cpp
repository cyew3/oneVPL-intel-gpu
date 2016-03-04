//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
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
#include "assert.h"
//#include <emmintrin.h> //SSE2
//#include <pmmintrin.h> //SSE3 (Prescott) Pentium(R) 4 processor (_mm_lddqu_si128() present )
//#include <tmmintrin.h> // SSSE3
//#include <smmintrin.h> // SSE4.1, Intel(R) Core(TM) 2 Duo
//#include <nmmintrin.h> // SSE4.2, Intel(R) Core(TM) 2 Duo
//#include <wmmintrin.h> // AES and PCLMULQDQ
#include <immintrin.h> // AVX, AVX2
//#include <ammintrin.h> // AMD extention SSE5 ? FMA4

#ifndef _mm256_set_m128i
#define _mm256_set_m128i(/* __m128i */ hi, /* __m128i */ lo) \
    _mm256_insertf128_si256(_mm256_castsi128_si256(lo), (hi), 0x1)
#endif // _mm256_set_m128i

#ifndef _mm256_storeu2_m128i
#define _mm256_storeu2_m128i(/* __m128i* */ hiaddr, /* __m128i* */ loaddr, /* __m256i */ a) \
    do { __m256i _a = (a); /* reference a only once in macro body */ \
        _mm_storeu_si128((loaddr), _mm256_castsi256_si128(_a)); \
        _mm_storeu_si128((hiaddr), _mm256_extractf128_si256(_a, 0x1)); \
    } while (0)
#endif // _mm256_storeu2_m128i


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

#if 1

#define block_stride (64)

#define SAD_CALLING_CONVENTION H265_FASTCALL
#define SAD_PARAMETERS_LIST const unsigned char *image,  const unsigned char *block, int img_stride

#define SAD_PARAMETERS_LOAD const int lx1 = img_stride; \
    const Ipp8u *__restrict blk1 = image; \
    const Ipp8u *__restrict blk2 = block

#elif 0

//#define block_stride 64

#define SAD_PARAMETERS_LIST const unsigned char *image,  const unsigned char *block, int img_stride, int block_stride

#define SAD_PARAMETERS_LOAD const int lx1 = img_stride; \
    const Ipp8u *__restrict blk1 = image; \
    const Ipp8u *__restrict blk2 = block

#else

struct sad_pars
{
    const  unsigned char *block;
    int img_stride
    int block_stride;
};

#define SAD_PARAMETERS_LIST const unsigned char *image,  const struct sad_pars *pars

#define SAD_PARAMETERS_LOAD const int lx1 = pars->img_stride; \
    const int block_stride = pars->block_stride; \
    const Ipp8u *__restrict blk1 = image; \
    const Ipp8u *__restrict blk2 = pars->block

#endif

namespace MFX_HEVC_PP
{

// the 128-bit SSE2 cant be full loaded with 4 bytes rows...
// the layout of block-buffer (32x32 for all blocks sizes or 4x4, 8x8 etc. depending on block-size) is not yet
// defined, for 4x4 whole buffer can be readed at once, and the implementation with two _mm_sad_epu8()
// will be, probably the fastest.

int SAD_CALLING_CONVENTION SAD_4x4_avx2(SAD_PARAMETERS_LIST)
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



int SAD_CALLING_CONVENTION SAD_4x8_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_4x16_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_8x4_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_8x8_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_8x16_avx2(SAD_PARAMETERS_LIST)
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
int SAD_CALLING_CONVENTION SAD_8x32_avx2(SAD_PARAMETERS_LIST)
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
int SAD_CALLING_CONVENTION SAD_8x32_avx2(SAD_PARAMETERS_LIST)
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
 

int SAD_CALLING_CONVENTION SAD_12x16_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_16x4_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_16x8_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_16x12_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_16x16_avx2(SAD_PARAMETERS_LIST)
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



int SAD_CALLING_CONVENTION SAD_16x32_avx2(SAD_PARAMETERS_LIST)
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


int SAD_CALLING_CONVENTION SAD_16x64_avx2(SAD_PARAMETERS_LIST)
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



int SAD_CALLING_CONVENTION SAD_24x32_avx2(SAD_PARAMETERS_LIST)
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





int SAD_CALLING_CONVENTION SAD_32x8_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_32x16_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_32x24_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_32x32_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_32x64_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_64x64_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_48x64_avx2(SAD_PARAMETERS_LIST) //OK
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
            __m256i tmp1 = _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1)), load128_unaligned( blk1 + lx1), 0x1);
            __m256i tmp2 = _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2)), load128_block_data( blk2 + block_stride), 0x1);
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
                __m256i tmp1 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1)), load128_unaligned( blk1 + lx1), 0x1);
                __m256i tmp2 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2)), load128_block_data( blk2 + block_stride), 0x1);
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
            __m256i tmp1 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1 + 32)), load128_unaligned( blk1 + 32 + lx1), 0x1);
            __m256i tmp2 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2 + 32)), load128_block_data( blk2 + 32 + block_stride), 0x1);
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
                __m256i tmp1 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_unaligned(blk1 + 32)), load128_unaligned( blk1 + 32 + lx1), 0x1);
                __m256i tmp2 =  _mm256_insertf128_si256(_mm256_castsi128_si256(load128_block_data(blk2 + 32)), load128_block_data( blk2 + 32 + block_stride), 0x1);
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



int SAD_CALLING_CONVENTION SAD_64x48_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_64x32_avx2(SAD_PARAMETERS_LIST) //OK
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



int SAD_CALLING_CONVENTION SAD_64x16_avx2(SAD_PARAMETERS_LIST) //OK
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



//int getSAD_avx2(const unsigned char *image,  const unsigned char *ref, int stride, int Size)
//{
//    if (Size == 4)
//        return SAD_4x4_avx2(image,  ref, stride);
//    else if (Size == 8)
//        return SAD_8x8_avx2(image,  ref, stride);
//    else if (Size == 16)
//        return SAD_16x16_avx2(image,  ref, stride);
//    return SAD_32x32_avx2(image,  ref, stride);
//}

#ifndef MFX_TARGET_OPTIMIZATION_AUTO

    int h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)
{
    if (SizeX == 4)
    {
        if(SizeY == 4) return SAD_4x4_avx2(image,  ref, stride);
        if(SizeY == 8) return SAD_4x8_avx2(image,  ref, stride);
        else           return SAD_4x16_avx2(image,  ref, stride);
    }
    else if (SizeX == 8)
    {
        if(SizeY ==  4) return SAD_8x4_avx2(image,  ref, stride);
        if(SizeY ==  8) return SAD_8x8_avx2(image,  ref, stride);
        if(SizeY == 16) return SAD_8x16_avx2(image,  ref, stride);
        else            return SAD_8x32_avx2(image,  ref, stride);
    }
    else if (SizeX == 12)
    {
                        return SAD_12x16_avx2(image,  ref, stride);
    }
    else if (SizeX == 16)
    {
        if(SizeY ==  4) return SAD_16x4_avx2(image,  ref, stride);
        if(SizeY ==  8) return SAD_16x8_avx2(image,  ref, stride);
        if(SizeY == 12) return SAD_16x12_avx2(image,  ref, stride);
        if(SizeY == 16) return SAD_16x16_avx2(image,  ref, stride);
        if(SizeY == 32) return SAD_16x32_avx2(image,  ref, stride);
        else            return SAD_16x64_avx2(image,  ref, stride);
    }
    else if (SizeX == 24)
    {
                        return SAD_24x32_avx2(image,  ref, stride);
    }
    else if (SizeX == 32)
    {
        if(SizeY ==  8) return SAD_32x8_avx2(image,  ref, stride);
        if(SizeY == 16) return SAD_32x16_avx2(image,  ref, stride);
        if(SizeY == 24) return SAD_32x24_avx2(image,  ref, stride);
        if(SizeY == 32) return SAD_32x32_avx2(image,  ref, stride);
        else            return SAD_32x64_avx2(image,  ref, stride);
    }
    else if (SizeX == 48)
    {
                        return SAD_48x64_avx2(image,  ref, stride);
    }
    else if (SizeX == 64)
    {
        if(SizeY == 16) return SAD_64x16_avx2(image,  ref, stride);
        if(SizeY == 32) return SAD_64x32_avx2(image,  ref, stride);
        if(SizeY == 48) return SAD_64x48_avx2(image,  ref, stride);
        else            return SAD_64x64_avx2(image,  ref, stride);
    }
    else return -1;
}
#endif // #ifndef MFX_TARGET_OPTIMIZATION_AUTO

/* special case - assume ref block has fixed stride of 64 and is 32-byte aligned */

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

ALIGN_DECL(32) static const short ones16s[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

template <int height>
static int SAD_4xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
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
        xmm2 = _mm_loadl_epi64((__m128i *)(ref + 0*block_stride));
        xmm2 = _mm_loadh_epi64(xmm2, (ref + 1*block_stride));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref + 2*block_stride));
        xmm3 = _mm_loadh_epi64(xmm3, (ref + 3*block_stride));
        ref += 4*block_stride;

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
static int SAD_8xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm7;

    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 2) {
        /* load 8x2 src block into two registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src + 0*src_stride));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 1*src_stride));
        src += 2*src_stride;

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref + 0*block_stride));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref + 1*block_stride));
        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        ref += 2*block_stride;
        
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
static int SAD_12xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
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
        xmm4 = _mm_loadu_si128((__m128i *)(ref + 0*block_stride));
        xmm5 = _mm_loadl_epi64((__m128i *)(ref + 0*block_stride + 8));
        xmm6 = _mm_loadu_si128((__m128i *)(ref + 1*block_stride));
        xmm5 = _mm_loadh_epi64(xmm5, (ref + 1*block_stride + 8));
        ref += 2*block_stride;

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
static int SAD_16xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
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

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, *(__m256i *)(ref + 0*block_stride));
        ymm1 = _mm256_sub_epi16(ymm1, *(__m256i *)(ref + 1*block_stride));
        ymm0 = _mm256_abs_epi16(ymm0);
        ymm1 = _mm256_abs_epi16(ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ref += 2*block_stride;

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
static int SAD_24xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
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
        ymm4 = _mm256_loadu_si256((__m256i *)(ref + 0*block_stride));
        ymm5 = mm256(_mm_loadu_si128((__m128i *)(ref + 0*block_stride + 16)));
        ymm6 = _mm256_loadu_si256((__m256i *)(ref + 1*block_stride));
        xmm0 = _mm_loadu_si128((__m128i *)(ref + 1*block_stride + 16));
        ymm5 = _mm256_permute2x128_si256(ymm5, mm256(xmm0), 0x20);
        ref += 2*block_stride;

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
static int SAD_32xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
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

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, *(__m256i *)(ref + 0*block_stride +  0));
        ymm1 = _mm256_sub_epi16(ymm1, *(__m256i *)(ref + 0*block_stride + 16));
        ymm2 = _mm256_sub_epi16(ymm2, *(__m256i *)(ref + 1*block_stride +  0));
        ymm3 = _mm256_sub_epi16(ymm3, *(__m256i *)(ref + 1*block_stride + 16));
        ref += 2*block_stride;

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
static int SAD_48xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
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

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, *(__m256i *)(ref +  0));
        ymm1 = _mm256_sub_epi16(ymm1, *(__m256i *)(ref + 16));
        ymm2 = _mm256_sub_epi16(ymm2, *(__m256i *)(ref + 32));
        ref += block_stride;

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
static int SAD_64xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
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

        /* calculate SAD */
        ymm0 = _mm256_sub_epi16(ymm0, *(__m256i *)(ref +  0));
        ymm1 = _mm256_sub_epi16(ymm1, *(__m256i *)(ref + 16));
        ymm2 = _mm256_sub_epi16(ymm2, *(__m256i *)(ref + 32));
        ymm3 = _mm256_sub_epi16(ymm3, *(__m256i *)(ref + 48));
        ref += block_stride;

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

int H265_FASTCALL MAKE_NAME(h265_SAD_MxN_16s)(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s width, Ipp32s height)
{
    switch (width) {
    case 4:
        if (height == 4)        return SAD_4xN< 4> (src, src_stride, ref);
        else if (height == 8)   return SAD_4xN< 8> (src, src_stride, ref);
        else if (height == 16)  return SAD_4xN<16> (src, src_stride, ref);
        break;
    case 8:
        if (height == 4)        return SAD_8xN< 4> (src, src_stride, ref);
        else if (height == 8)   return SAD_8xN< 8> (src, src_stride, ref);
        else if (height == 16)  return SAD_8xN<16> (src, src_stride, ref);
        else if (height == 32)  return SAD_8xN<32> (src, src_stride, ref);
        break;
    case 12:
        if (height == 16)       return SAD_12xN<16>(src, src_stride, ref);
        break;
    case 16:
        if (height == 4)        return SAD_16xN< 4>(src, src_stride, ref);
        else if (height == 8)   return SAD_16xN< 8>(src, src_stride, ref);
        else if (height == 12)  return SAD_16xN<12>(src, src_stride, ref);
        else if (height == 16)  return SAD_16xN<16>(src, src_stride, ref);
        else if (height == 32)  return SAD_16xN<32>(src, src_stride, ref);
        else if (height == 64)  return SAD_16xN<64>(src, src_stride, ref);
        break;
    case 24:
        if (height == 32)       return SAD_24xN<32>(src, src_stride, ref);
        break;
    case 32:
        if (height == 8)        return SAD_32xN< 8>(src, src_stride, ref);
        else if (height == 16)  return SAD_32xN<16>(src, src_stride, ref);
        else if (height == 24)  return SAD_32xN<24>(src, src_stride, ref);
        else if (height == 32)  return SAD_32xN<32>(src, src_stride, ref);
        else if (height == 64)  return SAD_32xN<64>(src, src_stride, ref);
        break;
    case 48:
        if (height == 64)       return SAD_48xN<64>(src, src_stride, ref);
        break;
    case 64:
        if (height == 16)       return SAD_64xN<16>(src, src_stride, ref);
        else if (height == 32)  return SAD_64xN<32>(src, src_stride, ref);
        else if (height == 48)  return SAD_64xN<48>(src, src_stride, ref);
        else if (height == 64)  return SAD_64xN<64>(src, src_stride, ref);
        break;
    default:
        break;
    }

    /* error - unsupported size */
    VM_ASSERT(0);
    return -1;
}


namespace SseDetails {
    template <class T, Ipp32s wstep>
    __m256i Load(const T *p, Ipp32s pitch)
    {
        if (wstep == 16) {
            return (sizeof(T) == 1)
                ? _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)p))
                : _mm256_loadu_si256((__m256i *)p);
        } else if (wstep == 8) {
            return (sizeof(T) == 1)
                ? _mm256_cvtepu8_epi16(_mm_unpacklo_epi64(_mm_loadl_epi64((__m128i *)p), _mm_loadl_epi64((__m128i *)(p + pitch))))
                : _mm256_set_m128i(_mm_loadu_si128((__m128i *)(p + pitch)), _mm_loadu_si128((__m128i *)p));
        } else {
            assert(wstep == 4);
            return (sizeof(T) == 1)
                ? _mm256_cvtepu8_epi16(_mm_set_epi32(*(Ipp32s *)(p + 3 * pitch), *(Ipp32s *)(p + 2 * pitch),
                                                     *(Ipp32s *)(p + 1 * pitch), *(Ipp32s *)(p + 0 * pitch)))
                : _mm256_set_epi64x(*(Ipp64u *)(p + 3 * pitch), *(Ipp64u *)(p + 2 * pitch),
                                    *(Ipp64u *)(p + 1 * pitch), *(Ipp64u *)(p + 0 * pitch));
        }
    }

    template <class T, Ipp32s width, Ipp32s wstep>
    Ipp32s Impl(const T *src1, Ipp32s pitchSrc1, const T *src2, Ipp32s pitchSrc2, Ipp32s height, Ipp32s shift)
    {
        const Ipp32s hstep = 16 / wstep;
        assert(width % wstep == 0);
        assert(height % hstep == 0);
        assert(sizeof(T) <= 2);

        __m256i sse = _mm256_setzero_si256();
        __m128i shift128 = _mm_cvtsi32_si128(shift);
        for (Ipp32s y = 0; y < height; y += hstep) {
            for (Ipp32s x = 0; x < width; x += wstep, src1 += wstep, src2 += wstep) {
                __m256i s1 = Load<T, wstep>(src1, pitchSrc1);
                __m256i s2 = Load<T, wstep>(src2, pitchSrc2);
                __m256i diff = _mm256_sub_epi16(s1, s2);
                if (sizeof(T) == 1) {
                    diff = _mm256_madd_epi16(diff, diff);   // i.e. no shift
                    sse = _mm256_add_epi32(sse, diff);
                } else {
                    // shift needs to happen before addition
                    __m256i diffLo = _mm256_unpacklo_epi16(diff, _mm256_setzero_si256());
                    __m256i diffHi = _mm256_unpackhi_epi16(diff, _mm256_setzero_si256());
                    diffLo = _mm256_madd_epi16(diffLo, diffLo);
                    diffHi = _mm256_madd_epi16(diffHi, diffHi);
                    diffLo = _mm256_sra_epi32(diffLo, shift128);
                    diffHi = _mm256_sra_epi32(diffHi, shift128);
                    sse = _mm256_add_epi32(sse, diffLo);
                    sse = _mm256_add_epi32(sse, diffHi);
                }
            }
            src1 += hstep * pitchSrc1 - width;
            src2 += hstep * pitchSrc2 - width;
        }

        __m128i a = _mm256_castsi256_si128(sse);
        __m128i b = _mm256_extractf128_si256(sse, 1);
        a = _mm_add_epi32(a, b);
        a = _mm_hadd_epi32(a, a);
        return _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }
}

template <class T>
Ipp32s H265_FASTCALL MAKE_NAME(h265_SSE)(const T *src1, Ipp32s pitchSrc1, const T *src2, Ipp32s pitchSrc2, Ipp32s width, Ipp32s height, Ipp32s shift)
{
    if (width == 4)
        return SseDetails::Impl<T,4,4>(src1, pitchSrc1, src2, pitchSrc2, height, shift);
    if (width == 8)
        return SseDetails::Impl<T,8,8>(src1, pitchSrc1, src2, pitchSrc2, height, shift);
    if (width == 12)
        return SseDetails::Impl<T,8,8>(src1,   pitchSrc1, src2,   pitchSrc2, height, shift) +
               SseDetails::Impl<T,4,4>(src1+8, pitchSrc1, src2+8, pitchSrc2, height, shift);
    else if (width == 16)
        return SseDetails::Impl<T,16,16>(src1, pitchSrc1, src2, pitchSrc2, height, shift);
    if (width == 24)
        return SseDetails::Impl<T,16,16>(src1,    pitchSrc1, src2,    pitchSrc2, height, shift) +
               SseDetails::Impl<T,8,8>  (src1+16, pitchSrc1, src2+16, pitchSrc2, height, shift);
    else if (width == 32)
        return SseDetails::Impl<T,32,16>(src1, pitchSrc1, src2, pitchSrc2, height, shift);
    else if (width == 48)
        return SseDetails::Impl<T,48,16>(src1, pitchSrc1, src2, pitchSrc2, height, shift);
    else if (width == 64)
        return SseDetails::Impl<T,64,16>(src1, pitchSrc1, src2, pitchSrc2, height, shift);
    assert(0);
    return 0;
}
template Ipp32s H265_FASTCALL MAKE_NAME(h265_SSE)<Ipp8u> (const Ipp8u  *src1, Ipp32s pitchSrc1, const Ipp8u  *src2, Ipp32s pitchSrc2, Ipp32s width, Ipp32s height, Ipp32s shift);
template Ipp32s H265_FASTCALL MAKE_NAME(h265_SSE)<Ipp16u>(const Ipp16u *src1, Ipp32s pitchSrc1, const Ipp16u *src2, Ipp32s pitchSrc2, Ipp32s width, Ipp32s height, Ipp32s shift);


namespace DiffNv12Details {
    template <class T, Ipp32s wstep> __m256i Load(const T *p, Ipp32s pitch)
    {
        if (wstep == 8) {
            return (sizeof(T) == 1)
                ? _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)p))
                : _mm256_load_si256((__m256i *)p);
        } else {
            assert(wstep == 4);
            return (sizeof(T) == 1)
                ? _mm256_cvtepu8_epi16(_mm_unpacklo_epi64(_mm_loadl_epi64((__m128i *)p), _mm_loadl_epi64((__m128i *)(p + pitch))))
                : _mm256_set_m128i(_mm_load_si128((__m128i *)(p + pitch)), _mm_load_si128((__m128i *)p));
        }
    }

    template <Ipp32s wstep> void Store(Ipp16s *p1, Ipp32s pitch1, Ipp16s *p2, Ipp32s pitch2, __m256i ymm)
    {
        if (wstep == 8) {
            _mm256_storeu2_m128i((__m128i *)p2, (__m128i *)p1, ymm);
        } else {
            assert(wstep == 4);
            __m128i half1 = _mm256_castsi256_si128(ymm);
            _mm_storel_epi64((__m128i *)p1, half1);
            _mm_storeh_pd((double *)(p1 + pitch1), _mm_castsi128_pd(half1));
            __m128i half2 = _mm256_extracti128_si256(ymm, 1);
            _mm_storel_epi64((__m128i *)p2, half2);
            _mm_storeh_pd((double *)(p2 + pitch2), _mm_castsi128_pd(half2));
        }
    }

    template <class T>
    void ImplW2(const T *src, Ipp32s pitchSrc, const T *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s height)
    {
        __m128i s, p;
        for (Ipp32s y = 0; y < height; y += 2) {
            if (sizeof(T) == 1) {
                s = _mm_cvtepu8_epi16(_mm_set_epi32(0, 0, *(Ipp32s *)(src  + pitchSrc),  *(Ipp32s *)src));
                p = _mm_cvtepu8_epi16(_mm_set_epi32(0, 0, *(Ipp32s *)(pred + pitchPred), *(Ipp32s *)pred));
            } else {
                s = _mm_set_epi64x(*(Ipp64u *)(src  + pitchSrc),  *(Ipp64u *)src);
                p = _mm_set_epi64x(*(Ipp64u *)(pred + pitchPred), *(Ipp64u *)pred);
            }

            __m128i d = _mm_sub_epi16(s, p);
            d = _mm_shufflelo_epi16(d, 0xD8);
            d = _mm_shufflehi_epi16(d, 0xD8);

            // store
            *(Ipp32s *)(diff1 + 0 * pitchDiff1) = (Ipp32s)_mm_extract_epi32(d, 0);
            *(Ipp32s *)(diff1 + 1 * pitchDiff1) = (Ipp32s)_mm_extract_epi32(d, 2);
            *(Ipp32s *)(diff2 + 0 * pitchDiff2) = (Ipp32s)_mm_extract_epi32(d, 1);
            *(Ipp32s *)(diff2 + 1 * pitchDiff2) = (Ipp32s)_mm_extract_epi32(d, 3);

            src   += 2 * pitchSrc;
            pred  += 2 * pitchPred;
            diff1 += 2 * pitchDiff1;
            diff2 += 2 * pitchDiff2;
        }
    }

    ALIGN_DECL(32) static const Ipp8s shuffleTab[32] = { 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15, 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15 };

    template <class T, Ipp32s width, Ipp32s wstep>
    void Impl(const T *src, Ipp32s pitchSrc, const T *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s height)
    {
        const Ipp32s hstep = 8 / wstep;
        assert(width % wstep == 0);
        assert(height % hstep == 0);
        assert(sizeof(T) <= 2);

        for (Ipp32s y = 0; y < height; y += hstep) {
            _mm_prefetch((const char *)(src + pitchSrc), _MM_HINT_T0);
            _mm_prefetch((const char *)(pred + pitchPred), _MM_HINT_T0);
            for (Ipp32s x = 0; x < width; x += wstep, src += 2*wstep, pred += 2*wstep, diff1 += wstep, diff2 += wstep) {
                __m256i s = Load<T, wstep>(src, pitchSrc);
                __m256i p = Load<T, wstep>(pred, pitchPred);
                __m256i d = _mm256_sub_epi16(s, p);
                d = _mm256_shuffle_epi8(d, *(__m256i *)shuffleTab);
                d = _mm256_permute4x64_epi64(d, 0xD8);
                Store<wstep>(diff1, pitchDiff1, diff2, pitchDiff2, d);
            }

            src   += hstep*pitchSrc - 2*width;
            pred  += hstep*pitchPred - 2*width;
            diff1 += hstep*pitchDiff1 - width;
            diff2 += hstep*pitchDiff2 - width;
        }
    }
};

template <class T>
void H265_FASTCALL MAKE_NAME(h265_DiffNv12)(const T *src, Ipp32s pitchSrc, const T *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s width, Ipp32s height)
{
    if (width == 2)
        DiffNv12Details::ImplW2(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else if (width == 4)
        DiffNv12Details::Impl<T,4,4>(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else if (width == 6 && height >= 4) {
        DiffNv12Details::Impl<T,4,4>(src,   pitchSrc, pred,   pitchPred, diff1,   pitchDiff1, diff2,   pitchDiff2, height);
        DiffNv12Details::ImplW2(src+8, pitchSrc, pred+8, pitchPred, diff1+4, pitchDiff1, diff2+4, pitchDiff2, height);
    }
    else if (width == 8)
        DiffNv12Details::Impl<T,8,8>(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else if (width == 12) {
        DiffNv12Details::Impl<T,8,8>(src,    pitchSrc, pred,    pitchPred, diff1,   pitchDiff1, diff2,   pitchDiff2, height);
        DiffNv12Details::Impl<T,4,4>(src+16, pitchSrc, pred+16, pitchPred, diff1+8, pitchDiff1, diff2+8, pitchDiff2, height);
    }
    else if (width == 16)
        DiffNv12Details::Impl<T,16,8>(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else if (width == 24)
        DiffNv12Details::Impl<T,24,8>(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else if (width == 32)
        DiffNv12Details::Impl<T,32,8>(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else if (width == 48)
        DiffNv12Details::Impl<T,48,8>(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else if (width == 64)
        DiffNv12Details::Impl<T,64,8>(src, pitchSrc, pred, pitchPred, diff1, pitchDiff1, diff2, pitchDiff2, height);
    else
        assert(0);
}
template void H265_FASTCALL MAKE_NAME(h265_DiffNv12)<Ipp8u> (const Ipp8u  *src, Ipp32s pitchSrc, const Ipp8u  *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s width, Ipp32s height);
template void H265_FASTCALL MAKE_NAME(h265_DiffNv12)<Ipp16u>(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s width, Ipp32s height);


template <class T>
void H265_FASTCALL MAKE_NAME(h265_Diff)(const T *src, Ipp32s pitchSrc, const T *pred, Ipp32s pitchPred, Ipp16s *diff, Ipp32s pitchDiff, Ipp32s size)
{
    assert(size == 4 || size == 8 || (size & 0xf) == 0);

    if (sizeof(T) == 1) {
        __m256i zero = _mm256_setzero_si256();
        __m256i d, s, p, sHalf, pHalf;
        if (size == 4) {
            __m128i zero = _mm_setzero_si128();
            __m128i s, p, d;
            s = _mm_setr_epi32(*(Ipp32u *)(src + 0*pitchSrc), *(Ipp32u *)(src + 1*pitchSrc),
                               *(Ipp32u *)(src + 2*pitchSrc), *(Ipp32u *)(src + 3*pitchSrc));
            p = _mm_setr_epi32(*(Ipp32u *)(pred + 0*pitchPred), *(Ipp32u *)(pred + 1*pitchPred),
                               *(Ipp32u *)(pred + 2*pitchPred), *(Ipp32u *)(pred + 3*pitchPred));
            d = _mm_sub_epi16(_mm_unpacklo_epi8(s, zero), _mm_unpacklo_epi8(p, zero));
            _mm_storel_epi64((__m128i*)diff, d);
            _mm_storeh_pd((double*)(diff+pitchDiff), _mm_castsi128_pd(d));
            d = _mm_sub_epi16(_mm_unpackhi_epi8(s, zero), _mm_unpackhi_epi8(p, zero));
            _mm_storel_epi64((__m128i*)(diff+2*pitchDiff), d);
            _mm_storeh_pd((double*)(diff+3*pitchDiff), _mm_castsi128_pd(d));
        } else if (size == 8) {
            for (Ipp32s y = 0; y < 8; y += 4, src += 4*pitchSrc, pred += 4*pitchPred, diff += 4*pitchDiff) {
                s = _mm256_setr_epi64x(*(Ipp64u*)src, *(Ipp64u*)(src+2*pitchSrc), *(Ipp64u*)(src+pitchSrc), *(Ipp64u*)(src+3*pitchSrc));
                p = _mm256_setr_epi64x(*(Ipp64u*)pred, *(Ipp64u*)(pred+2*pitchPred), *(Ipp64u*)(pred+pitchPred), *(Ipp64u*)(pred+3*pitchPred));
                sHalf = _mm256_unpacklo_epi8(s, zero);
                pHalf = _mm256_unpacklo_epi8(p, zero);
                d = _mm256_sub_epi16(sHalf, pHalf);
                _mm256_storeu2_m128i((__m128i*)(diff+pitchDiff), (__m128i*)(diff), d);
                sHalf = _mm256_unpackhi_epi8(s, zero);
                pHalf = _mm256_unpackhi_epi8(p, zero);
                d = _mm256_sub_epi16(sHalf, pHalf);
                _mm256_storeu2_m128i((__m128i*)(diff+3*pitchDiff), (__m128i*)(diff+2*pitchDiff), d);
            }
        } else if (size == 16) {
            for (Ipp32s y = 0; y < 16; y += 2, src += 2*pitchSrc, pred += 2*pitchPred, diff += 2*pitchDiff) {
                s = _mm256_loadu2_m128i((__m128i*)(src+pitchSrc), (__m128i*)src);
                p = _mm256_loadu2_m128i((__m128i*)(pred+pitchPred), (__m128i*)pred);
                //_mm_prefetch((char*)(src+2*pitchSrc), _MM_HINT_T0);
                //_mm_prefetch((char*)(pred+2*pitchPred), _MM_HINT_T0);
                s = _mm256_permute4x64_epi64(s, 0xd8);
                p = _mm256_permute4x64_epi64(p, 0xd8);
                sHalf = _mm256_unpacklo_epi8(s, zero);
                pHalf = _mm256_unpacklo_epi8(p, zero);
                d = _mm256_sub_epi16(sHalf, pHalf);
                _mm256_store_si256((__m256i*)(diff), d);
                sHalf = _mm256_unpackhi_epi8(s, zero);
                pHalf = _mm256_unpackhi_epi8(p, zero);
                d = _mm256_sub_epi16(sHalf, pHalf);
                _mm256_store_si256((__m256i*)(diff+pitchDiff), d);
            }
        } else {
            for (Ipp32s y = 0; y < size; y++, src += pitchSrc, pred += pitchPred, diff += pitchDiff) {
                //_mm_prefetch((char*)(src+pitchSrc), _MM_HINT_T0);
                //_mm_prefetch((char*)(pred+pitchPred), _MM_HINT_T0);
                for (Ipp32s x = 0; x < size; x += 32) {
                    s = _mm256_load_si256((__m256i*)(src+x));
                    p = _mm256_load_si256((__m256i*)(pred+x));
                    s = _mm256_permute4x64_epi64(s, 0xd8);
                    p = _mm256_permute4x64_epi64(p, 0xd8);
                    sHalf = _mm256_unpacklo_epi8(s, zero);
                    pHalf = _mm256_unpacklo_epi8(p, zero);
                    d = _mm256_sub_epi16(sHalf, pHalf);
                    _mm256_store_si256((__m256i*)(diff+x+0), d);
                    sHalf = _mm256_unpackhi_epi8(s, zero);
                    pHalf = _mm256_unpackhi_epi8(p, zero);
                    d = _mm256_sub_epi16(sHalf, pHalf);
                    _mm256_store_si256((__m256i*)(diff+x+16), d);
                }
            }
        }
    } else {
        __m256i s, p, d;
        if (size == 4) {
            s = _mm256_setr_epi64x(*(Ipp64u*)src, *(Ipp64u*)(src+pitchSrc), *(Ipp64u*)(src+2*pitchSrc), *(Ipp64u*)(src+3*pitchSrc));
            p = _mm256_setr_epi64x(*(Ipp64u*)pred, *(Ipp64u*)(pred+pitchPred), *(Ipp64u*)(pred+2*pitchPred), *(Ipp64u*)(pred+3*pitchPred));
            d = _mm256_sub_epi16(s, p);
            __m128i half1 = _mm256_castsi256_si128(d);
            _mm_storel_epi64((__m128i*)(diff), half1);
            _mm_storeh_pd((double*)(diff+pitchDiff), _mm_castsi128_pd(half1));
            __m128i half2 = _mm256_extracti128_si256(d, 1);
            _mm_storel_epi64((__m128i*)(diff+2*pitchDiff), half2);
            _mm_storeh_pd((double*)(diff+3*pitchDiff), _mm_castsi128_pd(half2));
        } else if (size == 8) {
            for (Ipp32s y = 0; y < size; y += 2, src += 2*pitchSrc, pred += 2*pitchPred, diff += 2*pitchDiff) {
                s = _mm256_loadu2_m128i((__m128i*)(src+pitchSrc), (__m128i*)(src));
                p = _mm256_loadu2_m128i((__m128i*)(pred+pitchPred), (__m128i*)(pred));
                d = _mm256_sub_epi16(s, p);
                _mm256_storeu2_m128i((__m128i*)(diff+pitchDiff), (__m128i*)(diff), d);
            }
        } else {
            for (Ipp32s y = 0; y < size; y++, src += pitchSrc, pred += pitchPred, diff += pitchDiff) {
                for (Ipp32s x = 0; x < size; x += 16) {
                    s = _mm256_load_si256((__m256i*)(src+x));
                    p = _mm256_load_si256((__m256i*)(pred+x));
                    d = _mm256_sub_epi16(s, p);
                    _mm256_store_si256((__m256i*)(diff+x), d);
                }
            }
        }
    }
}
template void H265_FASTCALL MAKE_NAME(h265_Diff)<Ipp8u> (const Ipp8u  *src, Ipp32s pitchSrc, const Ipp8u  *pred, Ipp32s pitchPred, Ipp16s *diff, Ipp32s pitchDiff, Ipp32s size);
template void H265_FASTCALL MAKE_NAME(h265_Diff)<Ipp16u>(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *pred, Ipp32s pitchPred, Ipp16s *diff, Ipp32s pitchDiff, Ipp32s size);


#define CoeffsType Ipp16s

void MAKE_NAME(h265_AddClipNv12UV_8u)(Ipp8u *dstNv12, Ipp32s pitchDst, 
                                      const Ipp8u *src1Nv12, Ipp32s pitchSrc1, 
                                      const CoeffsType *src2Yv12U,
                                      const CoeffsType *src2Yv12V, Ipp32s pitchSrc2,
                                      Ipp32s size)
{ 
    if (size == 4) { 
        __m256i residU = _mm256_setr_epi64x(*(Ipp64u*)(src2Yv12U+0*pitchSrc2), *(Ipp64u*)(src2Yv12U+2*pitchSrc2),
                                            *(Ipp64u*)(src2Yv12U+1*pitchSrc2), *(Ipp64u*)(src2Yv12U+3*pitchSrc2));
        __m256i residV = _mm256_setr_epi64x(*(Ipp64u*)(src2Yv12V+0*pitchSrc2), *(Ipp64u*)(src2Yv12V+2*pitchSrc2),
                                            *(Ipp64u*)(src2Yv12V+1*pitchSrc2), *(Ipp64u*)(src2Yv12V+3*pitchSrc2));
        __m256i residUV1 = _mm256_unpacklo_epi16(residU, residV);
        __m256i residUV2 = _mm256_unpackhi_epi16(residU, residV);
        __m256i pred1 = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(Ipp64u*)(src1Nv12+1*pitchSrc1), *(Ipp64u*)(src1Nv12+0*pitchSrc1)));
        __m256i pred2 = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(Ipp64u*)(src1Nv12+3*pitchSrc1), *(Ipp64u*)(src1Nv12+2*pitchSrc1)));
        __m256i dst1 = _mm256_add_epi16(pred1, residUV1);
        __m256i dst2 = _mm256_add_epi16(pred2, residUV2);
        __m256i dst = _mm256_packus_epi16(dst1, dst2);
        __m128i half1 = _mm256_castsi256_si128(dst);
        __m128i half2 = _mm256_extractf128_si256(dst, 1);
        _mm_storel_epi64((__m128i *)(dstNv12+0*pitchDst), half1);
        _mm_storel_epi64((__m128i *)(dstNv12+1*pitchDst), half2);
        _mm_storeh_pd   ((double  *)(dstNv12+2*pitchDst), _mm_castsi128_pd(half1));
        _mm_storeh_pd   ((double  *)(dstNv12+3*pitchDst), _mm_castsi128_pd(half2));
    } else if (size == 8) {
        for (Ipp32s y = 0; y < 8; y += 2) {
            __m256i residU = _mm256_loadu2_m128i((__m128i *)(src2Yv12U+1*pitchSrc2), (__m128i *)src2Yv12U);
            __m256i residV = _mm256_loadu2_m128i((__m128i *)(src2Yv12V+1*pitchSrc2), (__m128i *)src2Yv12V);
            residU = _mm256_permute4x64_epi64(residU, 0xd8);
            residV = _mm256_permute4x64_epi64(residV, 0xd8);
            __m256i residUV1 = _mm256_unpacklo_epi16(residU, residV);
            __m256i residUV2 = _mm256_unpackhi_epi16(residU, residV);
            __m256i pred1 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+0*pitchSrc1)));
            __m256i pred2 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+1*pitchSrc1)));
            __m256i dst1 = _mm256_add_epi16(pred1, residUV1);
            __m256i dst2 = _mm256_add_epi16(pred2, residUV2);
            __m256i dst = _mm256_packus_epi16(dst1, dst2);
            dst = _mm256_permute4x64_epi64(dst, 0xd8);
            _mm256_storeu2_m128i((__m128i *)(dstNv12+1*pitchDst), (__m128i *)dstNv12, dst);
            src2Yv12U += 2*pitchSrc2;
            src2Yv12V += 2*pitchSrc2;
            src1Nv12  += 2*pitchSrc1;
            dstNv12   += 2*pitchDst;
        }
    } else {
        for (Ipp32s y = 0; y < size; y++) {
            for (Ipp32s x = 0; x < size; x += 16) {
                __m256i residU = _mm256_load_si256((__m256i *)(src2Yv12U+x));
                __m256i residV = _mm256_load_si256((__m256i *)(src2Yv12V+x));
                residU = _mm256_permute4x64_epi64(residU, 0xd8);
                residV = _mm256_permute4x64_epi64(residV, 0xd8);
                __m256i residUV1 = _mm256_unpacklo_epi16(residU, residV);
                __m256i residUV2 = _mm256_unpackhi_epi16(residU, residV);
                __m256i pred1 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+2*x+0)));
                __m256i pred2 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+2*x+16)));
                __m256i dst1 = _mm256_add_epi16(pred1, residUV1);
                __m256i dst2 = _mm256_add_epi16(pred2, residUV2);
                __m256i dst = _mm256_packus_epi16(dst1, dst2);
                dst = _mm256_permute4x64_epi64(dst, 0xd8);
                _mm256_store_si256((__m256i *)(dstNv12+2*x), dst);
            }
            src2Yv12U += pitchSrc2;
            src2Yv12V += pitchSrc2;
            src1Nv12  += pitchSrc1;
            dstNv12   += pitchDst;
        }
    }
} 

template <class PixType> Ipp32s MAKE_NAME(h265_DiffDc)(const PixType *src, Ipp32s pitchSrc, const PixType *pred, Ipp32s pitchPred, Ipp32s width);
template<> Ipp32s MAKE_NAME(h265_DiffDc)<Ipp8u>(const Ipp8u *src, Ipp32s pitchSrc, const Ipp8u *pred, Ipp32s pitchPred, Ipp32s width)
{
    assert(width == 8 || width == 16 || width == 32);
    if (width == 8) {
        __m256i s = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(Ipp64u*)(src+pitchSrc), *(Ipp64u*)src));
        __m256i p = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(Ipp64u*)(pred+pitchPred), *(Ipp64u*)pred));
        __m256i dc = _mm256_sub_epi16(s,p);
        for (Ipp32s y = 2; y < 8; y += 2) {
            s = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(Ipp64u*)(src+(y+1)*pitchSrc), *(Ipp64u*)(src+y*pitchSrc)));
            p = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(Ipp64u*)(pred+(y+1)*pitchPred), *(Ipp64u*)(pred+y*pitchPred)));
            dc = _mm256_add_epi16(dc, _mm256_sub_epi16(s,p));
        }
        __m128i res = _mm_add_epi16(_mm256_castsi256_si128(dc), _mm256_extracti128_si256(dc,1));
        res = _mm_hadd_epi16(res, res);
        res = _mm_hadd_epi16(res, res);
        res = _mm_hadd_epi16(res, res);
        return (Ipp16s)_mm_extract_epi16(res, 0);
    }
    else if (width == 16) {
        __m256i s = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)src));
        __m256i p = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)pred));
        __m256i dc = _mm256_sub_epi16(s,p);
        src  += pitchSrc;
        pred += pitchPred;
        for (Ipp32s y = 1; y < 16; y++, src+=pitchSrc, pred+=pitchPred) {
            s = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)src));
            p = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)pred));
            dc = _mm256_add_epi16(dc, _mm256_sub_epi16(s,p));
        }
        __m128i res = _mm_add_epi16(_mm256_castsi256_si128(dc), _mm256_extracti128_si256(dc,1));
        res = _mm_hadd_epi16(res, res);
        res = _mm_hadd_epi16(res, res);
        return (Ipp16s)_mm_extract_epi16(res, 0) + (Ipp16s)_mm_extract_epi16(res, 1);
    }
    else {
        __m256i s = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)src));
        __m256i p = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)pred));
        __m256i dc = _mm256_sub_epi16(s,p);
        s = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)(src+16)));
        p = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)(pred+16)));
        dc = _mm256_add_epi16(dc, _mm256_sub_epi16(s,p));
        src  += pitchSrc;
        pred += pitchPred;
        for (Ipp32s y = 1; y < 32; y++, src+=pitchSrc, pred+=pitchPred) {
            for (Ipp32s x = 0; x < 32; x+=16) {
                s = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)(src+x)));
                p = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i*)(pred+x)));
                dc = _mm256_add_epi16(dc, _mm256_sub_epi16(s,p));
            }
        }
        __m128i res = _mm_add_epi16(_mm256_castsi256_si128(dc), _mm256_extracti128_si256(dc,1));
        res = _mm_hadd_epi16(res, res);
        return (Ipp16s)_mm_extract_epi16(res,0) + (Ipp16s)_mm_extract_epi16(res,1) + (Ipp16s)_mm_extract_epi16(res,2) + (Ipp16s)_mm_extract_epi16(res,3);
    }
}
//template Ipp32s MAKE_NAME(h265_DiffDc)<Ipp8u> (const Ipp8u  *src, Ipp32s pitchSrc, const Ipp8u  *pred, Ipp32s pitchPred, Ipp32s width);
//template Ipp32s MAKE_NAME(h265_DiffDc)<Ipp16u>(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *pred, Ipp32s pitchPred, Ipp32s width);

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
