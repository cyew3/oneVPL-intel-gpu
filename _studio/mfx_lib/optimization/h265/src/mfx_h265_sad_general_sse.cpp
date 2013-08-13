//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//


// Compare SAD-functions: plain-C vs. intrinsics
//
// Simulate motion search algorithm:
// - both buffers are 8-bits (unsigned char),
// - image/block buffers have stride and could be unaligned


#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_ATOM)

#include "mfx_h265_defs.h"

#include <math.h>
//#include <emmintrin.h> //SSE2
#include <pmmintrin.h> //SSE3 (Prescott) Pentium(R) 4 processor (_mm_lddqu_si128() present )
//#include <tmmintrin.h> // SSSE3
//#include <smmintrin.h> // SSE4.1, Intel(R) Core(TM) 2 Duo
//#include <nmmintrin.h> // SSE4.2, Intel(R) Core(TM) 2 Duo
//#include <wmmintrin.h> // AES and PCLMULQDQ
//#include <immintrin.h> // AVX
//#include <ammintrin.h> // AMD extention SSE5 ? FMA4


#define ALIGNED_SSE2 ALIGN_DECL(16)

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (const double *)(p)))

#ifdef _INCLUDED_PMM
#define load_unaligned(x) _mm_lddqu_si128(x)
#else
#define load_unaligned(x) _mm_loadu_si128(x)
#endif

#define SAD_CALLING_CONVENTION H265_FASTCALL


#define SAD_PARAMETERS_LIST const unsigned char *image,  const unsigned char *block, int img_stride, int block_stride

#define SAD_PARAMETERS_LOAD const int lx1 = img_stride; \
    const Ipp8u *__restrict blk1 = image; \
    const Ipp8u *__restrict blk2 = block


namespace MFX_HEVC_ENCODER
{
    // the 128-bit SSE2 cant be full loaded with 4 bytes rows...
    // the layout of block-buffer (32x32 for all blocks sizes or 4x4, 8x8 etc. depending on block-size) is not yet
    // defined, for 4x4 whole buffer can be readed at once, and the implementation with two _mm_sad_epu8()
    // will be, probably the fastest.

    int SAD_CALLING_CONVENTION SAD_4x4_general_sse(SAD_PARAMETERS_LIST)
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



    int SAD_CALLING_CONVENTION SAD_4x8_general_sse(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION SAD_4x16_general_sse(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION SAD_8x4_general_sse(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION SAD_8x8_general_sse(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION SAD_8x16_general_sse(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION SAD_8x32_general_sse(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION SAD_8x32_sse(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION SAD_12x16_general_sse(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION SAD_16x4_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1+lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2+block_stride)));

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 2*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 3*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


    int SAD_CALLING_CONVENTION SAD_16x8_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1+lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2+block_stride)));

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 2*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 3*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 4*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 5*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 6*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 7*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


    int SAD_CALLING_CONVENTION SAD_16x12_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1+lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2+block_stride)));

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 2*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 3*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 4*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 5*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 6*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 7*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 8*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 9*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 10*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 11*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


    int SAD_CALLING_CONVENTION SAD_16x16_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1+lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2+block_stride)));

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 2*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 3*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 4*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 5*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 6*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 7*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 8*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 9*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 10*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 11*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 12*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 13*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }


        blk1 += 2*lx1;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1+lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 14*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 15*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }



    int SAD_CALLING_CONVENTION SAD_16x32_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        for(int i = 2; i < 30; i += 4)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }

            blk1 += 2*lx1;    
            blk2 += 2*block_stride;

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


#if 1
    int SAD_CALLING_CONVENTION SAD_16x64_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        for(int i = 2; i < 62; i += 4)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }

            blk1 += 2*lx1;    
            blk2 += 2*block_stride;

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#else
    int SAD_CALLING_CONVENTION SAD_16x64_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        for(int i = 2; i < 64; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#endif



    int SAD_CALLING_CONVENTION SAD_24x32_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        if( (blk2 - (Ipp8u *)0) & 15 )
        {
            // 8-byte aligned

            __m128i s3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1)), (const __m128i *) (blk1 + lx1));
            s3 = _mm_sad_epu8( s3, _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2)), (const __m128i *) (blk2 + block_stride)));
            __m128i s1 = load_unaligned( (const __m128i *) (blk1 + 8));
            __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 8));
            s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2 + 8)));
            s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride + 8)));

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
                    __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 8));
                    __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 8));
                    tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 8));
                    tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 8));
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

            __m128i s1 = load_unaligned( (const __m128i *) (blk1));
            __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
            s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));
            __m128i s3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1 + 16)), (const __m128i *) (blk1 + lx1 + 16));
            s3 = _mm_sad_epu8( s3, _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2 + 16)), (const __m128i *) (blk2 + block_stride + 16)));

            for(int i = 2; i < 32; i += 2)
            {
                blk1 += 2*lx1;    
                blk2 += 2*block_stride;
                {
                    __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                    __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                    tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                    tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
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



#if 1
    int SAD_CALLING_CONVENTION SAD_32x8_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;
        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);

            __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
            tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);

            __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
            tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);

            __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
            tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#else
    int SAD_CALLING_CONVENTION SAD_32x8_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 8; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#endif



#if 0
    int SAD_CALLING_CONVENTION SAD_32x16_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 14; i += 4)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }

            blk1 += 2*lx1;    
            blk2 += 2*block_stride;

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);

            __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
            tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#else
    int SAD_CALLING_CONVENTION SAD_32x16_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 16; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#endif



#if 0
    int SAD_CALLING_CONVENTION SAD_32x24_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 22; i += 4)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }

            blk1 += 2*lx1;    
            blk2 += 2*block_stride;

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);

            __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
            tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#else
    int SAD_CALLING_CONVENTION SAD_32x24_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 24; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#endif



#if 0
    int SAD_CALLING_CONVENTION SAD_32x32_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;    
        blk2 += 2*block_stride;

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);

            __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
            tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        for(int i = 4; i < 32; i += 4)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }

            blk1 += 2*lx1;    
            blk2 += 2*block_stride;

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#else
    int SAD_CALLING_CONVENTION SAD_32x32_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 32; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#endif



#if 0
    // is is strange, but more instruction in the loop makes the code running slower
    int SAD_CALLING_CONVENTION SAD_32x64_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;    

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 2*block_stride));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 3*block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);

            __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 2*block_stride + 16));
            tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + 3*block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp3);
            s2 = _mm_add_epi32( s2, tmp4);
        }

        for(int i = 4; i < 64; i += 4)
        {
            blk1 += 2*lx1;    
            blk2 += 4*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }

            blk1 += 2*lx1;    

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 2*block_stride));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + 3*block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);

                __m128i tmp3 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp4 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp3 = _mm_sad_epu8( tmp3, *(const __m128i *) (blk2 + 2*block_stride + 16));
                tmp4 = _mm_sad_epu8( tmp4, *(const __m128i *) (blk2 + 3*block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp3);
                s2 = _mm_add_epi32( s2, tmp4);
            }
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#else
    int SAD_CALLING_CONVENTION SAD_32x64_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 0; i < 31; ++i)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
#endif



    int SAD_CALLING_CONVENTION SAD_64x64_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }
        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 64; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


    int SAD_CALLING_CONVENTION SAD_48x64_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }
        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }


        for(int i = 2; i < 64; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


    int SAD_CALLING_CONVENTION SAD_64x48_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }
        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 48; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


    int SAD_CALLING_CONVENTION SAD_64x32_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }
        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 32; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }


    int SAD_CALLING_CONVENTION SAD_64x16_general_sse(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }
        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        {
            __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
            __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
            tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
            tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 16; i += 2)
        {
            blk1 += 2*lx1;    
            blk2 += 2*block_stride;
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 16));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 16));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 16));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 16));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 32));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 32));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 32));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 32));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }

            {
                __m128i tmp1 = load_unaligned( (const __m128i *) (blk1 + 48));
                __m128i tmp2 = load_unaligned( (const __m128i *) (blk1 + lx1 + 48));
                tmp1 = _mm_sad_epu8( tmp1, *(const __m128i *) (blk2 + 48));
                tmp2 = _mm_sad_epu8( tmp2, *(const __m128i *) (blk2 + block_stride + 48));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
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
    //int getSAD_sse(const unsigned char *image,  const unsigned char *ref, int stride, int Size)
    //{
    //    if (Size == 4)
    //        return SAD_4x4_general_sse(image,  ref, stride);
    //    else if (Size == 8)
    //        return SAD_8x8_sse(image,  ref, stride);
    //    else if (Size == 16)
    //        return SAD_16x16_sse(image,  ref, stride);
    //    return SAD_32x32_sse(image,  ref, stride);
    //}

    int h265_SAD_MxN_general_8u(const unsigned char *image,  int stride_img, const unsigned char *ref, int stride_ref, int SizeX, int SizeY)
    {
        int index = 0;
        int cost; 
        if (SizeX == 4)
        {
            if(SizeY == 4) { return SAD_4x4_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 8) { return SAD_4x8_general_sse(image,  ref, stride_img, stride_ref); }
            else           { return SAD_4x16_general_sse(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 8)
        {
            if(SizeY ==  4) { return SAD_8x4_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY ==  8) { return SAD_8x8_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 16) { return SAD_8x16_general_sse(image,  ref, stride_img, stride_ref); }
            else             { return SAD_8x32_general_sse(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 12)
        {
            return SAD_12x16_general_sse(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 16)
        {
            if(SizeY ==  4) { return SAD_16x4_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY ==  8) { return SAD_16x8_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 12) { return SAD_16x12_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 16) { return SAD_16x16_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 32) { return SAD_16x32_general_sse(image,  ref, stride_img, stride_ref); }
            else            { return SAD_16x64_general_sse(image,  ref, stride_img, stride_ref); }
        }
        else if (SizeX == 24)
        {
            return SAD_24x32_general_sse(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 32)
        {
            if(SizeY ==  8) { return SAD_32x8_general_sse(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 16) { return SAD_32x16_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 24) { return SAD_32x24_general_sse(image,  ref, stride_img, stride_ref); }
            else if(SizeY == 32) { return SAD_32x32_general_sse(image,  ref, stride_img, stride_ref);}
            else            { return SAD_32x64_general_sse(image,  ref, stride_img, stride_ref);}
        }
        else if (SizeX == 48)
        {
            return SAD_48x64_general_sse(image,  ref, stride_img, stride_ref);
        }
        else if (SizeX == 64)
        {
            if(SizeY == 16) { return SAD_64x16_general_sse(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 32) { return SAD_64x32_general_sse(image,  ref, stride_img, stride_ref);}
            else if(SizeY == 48) { return SAD_64x48_general_sse(image,  ref, stride_img, stride_ref);}
            else            { return SAD_64x64_general_sse(image,  ref, stride_img, stride_ref); }
        }
        
        else return -1;

    } // int h265_SAD_MxN_general_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)

} // end namespace MFX_HEVC_ENCODER

#endif // #if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
