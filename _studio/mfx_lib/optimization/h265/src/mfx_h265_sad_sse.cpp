//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

// Compare SAD-functions: plain-C vs. intrinsics
//
// Simulate motion search algorithm:
// - both buffers are 8-bits (unsigned char),
// - reference buffer is linear (without srtride) ans aligned
// - image buffer has stride and could be unaligned


#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4) || \
    defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_SSSE3)


#include <math.h>
//#include <emmintrin.h> //SSE2
#include <pmmintrin.h> //SSE3 (Prescott) Pentium(R) 4 processor (_mm_lddqu_si128() present )
//#include <tmmintrin.h> // SSSE3
//#include <smmintrin.h> // SSE4.1, Intel(R) Core(TM) 2 Duo
//#include <nmmintrin.h> // SSE4.2, Intel(R) Core(TM) 2 Duo
//#include <wmmintrin.h> // AES and PCLMULQDQ
//#include <immintrin.h> // AVX
//#include <ammintrin.h> // AMD extention SSE5 ? FMA4
#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

#define ALIGNED_SSE2 ALIGN_DECL(16)

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (const double *)(p)))

#ifdef _INCLUDED_PMM
#define load_unaligned(x) _mm_lddqu_si128(x)
#else
#define load_unaligned(x) _mm_loadu_si128(x)
#endif

#define block_stride 64

#define SAD_CALLING_CONVENTION H265_FASTCALL
#define SAD_PARAMETERS_LIST const unsigned char *image,  const unsigned char *block, int img_stride
#define SAD_PARAMETERS_LOAD const int lx1 = img_stride; \
    const Ipp8u *__restrict blk1 = image; \
    const Ipp8u *__restrict blk2 = block


namespace MFX_HEVC_PP
{
    // the 128-bit SSE2 cant be full loaded with 4 bytes rows...
    // the layout of block-buffer (32x32 for all blocks sizes or 4x4, 8x8 etc. depending on block-size) is not yet
    // defined, for 4x4 whole buffer can be readed at once, and the implementation with two _mm_sad_epu8()
    // will be, probably the fastest.

    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_4x4)(SAD_PARAMETERS_LIST)
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



    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_4x8)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_4x16)(SAD_PARAMETERS_LIST)
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

    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_8x4)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_8x8)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_8x16)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_8x32)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_8x32)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_12x16)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_16x4)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_16x8)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_16x12)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_16x16)(SAD_PARAMETERS_LIST)
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



    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_16x32)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_16x64)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_16x64)(SAD_PARAMETERS_LIST)
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



#if 0
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_24x32)(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load_unaligned( (const __m128i *) (blk1));
        __m128i s2 = load_unaligned( (const __m128i *) (blk1 + lx1));
        s1 = _mm_sad_epu8( s1, *( (const __m128i *) (blk2)));
        s2 = _mm_sad_epu8( s2, *( (const __m128i *) (blk2 + block_stride)));
        __m128i s3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1 + 16)), (const __m128i *) (blk1 + lx1 + 16));
        s3 = _mm_sad_epu8( s3, _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2 + 16)), (const __m128i *) (blk2 + block_stride + 16)));

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
            {
                __m128i tmp3 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk1 + 16)), (const __m128i *) (blk1 + lx1 + 16));
                __m128i tmp4 = _mm_loadh_epi64( _mm_loadl_epi64( (const __m128i *) (blk2 + 16)), (const __m128i *) (blk2 + block_stride + 16));
                s3 = _mm_add_epi32( s3, _mm_sad_epu8( tmp3, tmp4));
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
#else
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_24x32)(SAD_PARAMETERS_LIST)
    {
        SAD_PARAMETERS_LOAD;

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
#endif



#if 1
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x8)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x8)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x16)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x16)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x24)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x24)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x32)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x32)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x64)(SAD_PARAMETERS_LIST)
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
    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_32x64)(SAD_PARAMETERS_LIST)
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



    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_64x64)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_48x64)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_64x48)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_64x32)(SAD_PARAMETERS_LIST)
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


    int SAD_CALLING_CONVENTION MAKE_NAME(SAD_64x16)(SAD_PARAMETERS_LIST)
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

#ifndef MFX_TARGET_OPTIMIZATION_AUTO

    int h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)
    {
        if (SizeX == 4)
        {
            if(SizeY == 4) { return MAKE_NAME(SAD_4x4)(image,  ref, stride); }
            if(SizeY == 8) { return MAKE_NAME(SAD_4x8)(image,  ref, stride); }
            else           { return MAKE_NAME(SAD_4x16)(image,  ref, stride); }
        }
        else if (SizeX == 8)
        {
            if(SizeY ==  4) { return MAKE_NAME(SAD_8x4)(image,  ref, stride); }
            if(SizeY ==  8) { return MAKE_NAME(SAD_8x8)(image,  ref, stride); }
            if(SizeY == 16) { return MAKE_NAME(SAD_8x16)(image,  ref, stride); }
            else            { return MAKE_NAME(SAD_8x32)(image,  ref, stride); }
        }
        else if (SizeX == 12)
        {            
            return MAKE_NAME(SAD_12x16)(image,  ref, stride);
        }
        else if (SizeX == 16)
        {
            if(SizeY ==  4) { return MAKE_NAME(SAD_16x4)(image,  ref, stride); }
            if(SizeY ==  8) { return MAKE_NAME(SAD_16x8)(image,  ref, stride); }
            if(SizeY == 12) { return MAKE_NAME(SAD_16x12)(image,  ref, stride);}
            if(SizeY == 16) { return MAKE_NAME(SAD_16x16)(image,  ref, stride);}
            if(SizeY == 32) { return MAKE_NAME(SAD_16x32)(image,  ref, stride);}
            else            { return MAKE_NAME(SAD_16x64)(image,  ref, stride);}
        }
        else if (SizeX == 24)
        {
           return MAKE_NAME(SAD_24x32)(image,  ref, stride);
        }
        else if (SizeX == 32)
        {
            if(SizeY ==  8) { return MAKE_NAME(SAD_32x8)(image,  ref, stride); }
            if(SizeY == 16) { return MAKE_NAME(SAD_32x16)(image,  ref, stride);}
            if(SizeY == 24) { return MAKE_NAME(SAD_32x24)(image,  ref, stride); }
            if(SizeY == 32) { return MAKE_NAME(SAD_32x32)(image,  ref, stride);}
            else            { return MAKE_NAME(SAD_32x64)(image,  ref, stride);}
        }
        else if (SizeX == 48)
        {
            return MAKE_NAME(SAD_48x64)(image,  ref, stride);
        }
        else if (SizeX == 64)
        {
            if(SizeY == 16) { return MAKE_NAME(SAD_64x16)(image,  ref, stride);}
            if(SizeY == 32) { return MAKE_NAME(SAD_64x32)(image,  ref, stride);}
            if(SizeY == 48) { return MAKE_NAME(SAD_64x48)(image,  ref, stride);}
            else            { return MAKE_NAME(SAD_64x64)(image,  ref, stride);}
        }

        //funcTimes[index] += (endTime - startTime);
        else return -1;
        //return cost;
    }
#endif

/* special case - assume ref block has fixed stride of 64 and is 16-byte aligned */
ALIGN_DECL(16) static const short ones16s[8] = {1, 1, 1, 1, 1, 1, 1, 1};

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

        /* load 12x2 ref block into three registers */
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
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm_acc;

    xmm_acc = _mm_setzero_si128();

    for (h = 0; h < height; h += 2) {
        /* load 16x2 src block into four registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src + 0*src_stride));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 0*src_stride + 8));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 1*src_stride));
        xmm3 = _mm_loadu_si128((__m128i *)(src + 1*src_stride + 8));
        src += 2*src_stride;

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref + 0*block_stride));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref + 0*block_stride + 8));
        xmm2 = _mm_sub_epi16(xmm2, *(__m128i *)(ref + 1*block_stride));
        xmm3 = _mm_sub_epi16(xmm3, *(__m128i *)(ref + 1*block_stride + 8));
        ref += 2*block_stride;

        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);
        xmm3 = _mm_abs_epi16(xmm3);

        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);
        xmm0 = _mm_add_epi16(xmm0, xmm3);

        /* add to 32-bit accumulator */
        xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
        xmm_acc = _mm_add_epi32(xmm_acc, xmm0);
    }

    /* add up columns */
    xmm_acc = _mm_add_epi32(xmm_acc, _mm_shuffle_epi32(xmm_acc, 0x0e));
    xmm_acc = _mm_add_epi32(xmm_acc, _mm_shuffle_epi32(xmm_acc, 0x01));

    return _mm_cvtsi128_si32(xmm_acc);
}

template <int height>
static int SAD_24xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm4, xmm5, xmm6, xmm7;

    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load 24x1 src block into three registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadu_si128((__m128i *)(src +  8));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 16));
        src += src_stride;

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref +  0));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref +  8));
        xmm2 = _mm_sub_epi16(xmm2, *(__m128i *)(ref + 16));
        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);
        ref += block_stride;
        
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
static int SAD_32xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm_acc;

    xmm_acc = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load 32x1 src block into four registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadu_si128((__m128i *)(src +  8));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 16));
        xmm3 = _mm_loadu_si128((__m128i *)(src + 24));
        src += src_stride;

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref +  0));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref +  8));
        xmm2 = _mm_sub_epi16(xmm2, *(__m128i *)(ref + 16));
        xmm3 = _mm_sub_epi16(xmm3, *(__m128i *)(ref + 24));
        ref += block_stride;

        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);
        xmm3 = _mm_abs_epi16(xmm3);

        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);
        xmm0 = _mm_add_epi16(xmm0, xmm3);

        /* add to 32-bit accumulator */
        xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
        xmm_acc = _mm_add_epi32(xmm_acc, xmm0);
    }

    /* add up columns */
    xmm_acc = _mm_add_epi32(xmm_acc, _mm_shuffle_epi32(xmm_acc, 0x0e));
    xmm_acc = _mm_add_epi32(xmm_acc, _mm_shuffle_epi32(xmm_acc, 0x01));

    return _mm_cvtsi128_si32(xmm_acc);
}

template <int height>
static int SAD_48xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load first 24x1 src block into three registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadu_si128((__m128i *)(src +  8));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 16));

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref +  0));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref +  8));
        xmm2 = _mm_sub_epi16(xmm2, *(__m128i *)(ref + 16));

        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);

        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);
        
        /* load second 24x1 src block into three registers */
        xmm3 = _mm_loadu_si128((__m128i *)(src + 24));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 32));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 40));

        /* calculate SAD */
        xmm3 = _mm_sub_epi16(xmm3, *(__m128i *)(ref + 24));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref + 32));
        xmm2 = _mm_sub_epi16(xmm2, *(__m128i *)(ref + 40));

        xmm3 = _mm_abs_epi16(xmm3);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);

        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);
        xmm0 = _mm_add_epi16(xmm0, xmm3);

        /* add to 32-bit accumulator */
        xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
        xmm7 = _mm_add_epi32(xmm7, xmm0);

        src += src_stride;
        ref += block_stride;
    }

    /* add up columns */
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x0e));
    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x01));

    return _mm_cvtsi128_si32(xmm7);
}

template <int height>
static int SAD_64xN(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref)
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm_acc, xmm_tmp;

    xmm_acc = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load first 32x1 src block into four registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadu_si128((__m128i *)(src +  8));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 16));
        xmm3 = _mm_loadu_si128((__m128i *)(src + 24));

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref +  0));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref +  8));
        xmm2 = _mm_sub_epi16(xmm2, *(__m128i *)(ref + 16));
        xmm3 = _mm_sub_epi16(xmm3, *(__m128i *)(ref + 24));

        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);
        xmm3 = _mm_abs_epi16(xmm3);

        xmm_tmp = _mm_add_epi16(xmm0, xmm1);
        xmm_tmp = _mm_add_epi16(xmm_tmp, xmm2);
        xmm_tmp = _mm_add_epi16(xmm_tmp, xmm3);

        /* load second 32x1 src block into four registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src + 32));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 40));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 48));
        xmm3 = _mm_loadu_si128((__m128i *)(src + 56));

        /* calculate SAD */
        xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref + 32));
        xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref + 40));
        xmm2 = _mm_sub_epi16(xmm2, *(__m128i *)(ref + 48));
        xmm3 = _mm_sub_epi16(xmm3, *(__m128i *)(ref + 56));

        xmm0 = _mm_abs_epi16(xmm0);
        xmm1 = _mm_abs_epi16(xmm1);
        xmm2 = _mm_abs_epi16(xmm2);
        xmm3 = _mm_abs_epi16(xmm3);

        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);
        xmm0 = _mm_add_epi16(xmm0, xmm3);
        xmm0 = _mm_add_epi16(xmm0, xmm_tmp);  /* add first half */

        /* add to 32-bit accumulator */
        xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
        xmm_acc = _mm_add_epi32(xmm_acc, xmm0);

        src += src_stride;
        ref += block_stride;
    }

    /* add up columns */
    xmm_acc = _mm_add_epi32(xmm_acc, _mm_shuffle_epi32(xmm_acc, 0x0e));
    xmm_acc = _mm_add_epi32(xmm_acc, _mm_shuffle_epi32(xmm_acc, 0x01));

    return _mm_cvtsi128_si32(xmm_acc);
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


#define CoeffsType Ipp16s

void MAKE_NAME(h265_AddClipNv12UV_8u)(Ipp8u *dstNv12, Ipp32s pitchDst, 
                          const Ipp8u *src1Nv12, Ipp32s pitchSrc1, 
                          const CoeffsType *src2Yv12U,
                          const CoeffsType *src2Yv12V, Ipp32s pitchSrc2,
                          Ipp32s size)
{ 
    int i, j;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4;
    __m128i xmm7 = _mm_setzero_si128();
    ALIGN_DECL(16) static const signed char uvShuf[16] = { 0,  128,  2,  128,  4,  128,  6,  128,  8,  128,  10,  128,  12,  128, 14, 128 };
    if (size == 4) { 
        for (i = 0; i < size; i++) {
            xmm0 = _mm_loadl_epi64((__m128i*)(src1Nv12));       // Load NV12
            xmm1 = _mm_srli_si128 (xmm0, 1);                    // 1 pixel shift  
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i*)(uvShuf)); // Remove interleaving, make Short
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i*)(uvShuf)); // Remove interleaving, make Short
            xmm2 = _mm_loadl_epi64((__m128i*)(src2Yv12U));      // Load U Resid
            xmm3 = _mm_loadl_epi64((__m128i*)(src2Yv12V));      // Load V Resid
            
            xmm2 = _mm_add_epi16(xmm2, xmm0);                   // Add
            xmm3 = _mm_add_epi16(xmm3, xmm1);
            xmm2 = _mm_packus_epi16(xmm2, xmm2);                // Saturate
            xmm3 = _mm_packus_epi16(xmm3, xmm3);
            xmm4 = _mm_unpacklo_epi8(xmm2, xmm3);               // Interleave
            //store
            _mm_storel_epi64((__m128i*)dstNv12, xmm4);

            src1Nv12  += pitchSrc1;
            src2Yv12U += pitchSrc2;
            src2Yv12V += pitchSrc2;
            dstNv12   += pitchDst;
        }
    } else { 
        for (i = 0; i < size; i++) {
            for (j = 0; j < size; j += 8) {
                xmm0 = _mm_loadu_si128((__m128i*)(src1Nv12+2*j));     // Load NV12
                xmm1 = _mm_srli_si128 (xmm0, 1);                      // 1 pixel shift 
                xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i*)(uvShuf));   // Remove interleaving, make Short
                xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i*)(uvShuf));   // Remove interleaving, make Short
                xmm2 = _mm_loadu_si128((__m128i*)(src2Yv12U+j));      // Load U Resid
                xmm3 = _mm_loadu_si128((__m128i*)(src2Yv12V+j));      // Load V Resid

                xmm2 = _mm_add_epi16(xmm2, xmm0);                     // Add
                xmm3 = _mm_add_epi16(xmm3, xmm1);
                xmm2 = _mm_packus_epi16(xmm2, xmm2);                  // Saturate
                xmm3 = _mm_packus_epi16(xmm3, xmm3);
                xmm4 = _mm_unpacklo_epi8(xmm2, xmm3);                 // Interleave
                //store
                _mm_storeu_si128((__m128i*)(dstNv12+2*j), xmm4);
            }
            src1Nv12  += pitchSrc1;
            src2Yv12U += pitchSrc2;
            src2Yv12V += pitchSrc2;
            dstNv12   += pitchDst;
        }
    } 
} 

} // end namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
