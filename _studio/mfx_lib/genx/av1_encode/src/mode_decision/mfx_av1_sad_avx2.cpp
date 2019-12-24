// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "assert.h"
#include "immintrin.h"
#include "mfx_av1_opts_intrin.h"

#include <stdint.h>

#define SAD_PARAMETERS_LOAD const int lx1 = img_stride; \
    const uint8_t *__restrict blk1 = image; \
    const uint8_t *__restrict blk2 = block
#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (const double *)(p)))
#define load128_unaligned(x)    _mm_loadu_si128( (const __m128i *)( x ))
#define load256_unaligned(x) _mm256_loadu_si256( (const __m256i *)( x ))
#define load128_block_data( p)  *(const __m128i *)( p )
#define load256_block_data( p)  _mm256_loadu_si256( (const __m256i *)( p ))
#define load256_block_data2( p) *(const __m256i *)( p )
#define load_12_bytes(a) _mm_loadh_epi64(_mm_cvtsi32_si128( *(const int *) ((const char*)(a) + 8)), a)
#define mm128(s) _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s) _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */
ALIGN_DECL(32) static const short ones16s[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

namespace VP9PP
{
    const int32_t special_block_stride = 64;

    template <int W, int H> int sad_special_avx2(const uint8_t *p1, int pitch1, const uint8_t *p2);
    template <int W, int H> int sad_general_avx2(const uint8_t *p1, int pitch1, const uint8_t *p2, int pitch2);

    template <int W, int H> int sad16_special_avx2(const uint16_t *p1, int pitch1, const uint16_t *p2);
    template <int W, int H> int sad16_general_avx2(const uint16_t *p1, int pitch1, const uint16_t *p2, int pitch2);

    template<> int sad_special_avx2<4,4>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1   = _mm_cvtsi32_si128( *(const int *) (blk1));
        __m128i tmp1 = _mm_cvtsi32_si128( *(const int *) (blk1 + lx1));

        __m128i tmp2 = _mm_cvtsi32_si128( *(const int *) (blk2));
        __m128i tmp3 = _mm_cvtsi32_si128( *(const int *) (blk2 + special_block_stride));

        s1   = _mm_unpacklo_epi32(s1, tmp1);
        tmp2 = _mm_unpacklo_epi32(tmp2, tmp3);

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        __m128i s2   = _mm_cvtsi32_si128( *(const int *) (blk1));
        __m128i tmp4 = _mm_cvtsi32_si128( *(const int *) (blk1 + lx1));

        __m128i tmp5 = _mm_cvtsi32_si128( *(const int *) (blk2));
        __m128i tmp6 = _mm_cvtsi32_si128( *(const int *) (blk2 + special_block_stride));

        s2   = _mm_unpacklo_epi32(s2,  tmp4);
        tmp5 = _mm_unpacklo_epi32(tmp5, tmp6);

        s1 = _mm_sad_epu8( s1, tmp2);
        s2 = _mm_sad_epu8( s2, tmp5);

        s1 = _mm_add_epi32( s1, s2);

        return _mm_cvtsi128_si32( s1);
    }
    template<> int sad_special_avx2<4,8>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
        __m128i tmp1 = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + special_block_stride)));

        s1 = _mm_sad_epu8( s1, tmp1);

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        __m128i s2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
        __m128i tmp2 = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + special_block_stride)));

        s2 = _mm_sad_epu8( s2, tmp2);

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        __m128i t1   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
        tmp1         = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + special_block_stride)));

        t1 = _mm_sad_epu8( t1, tmp1);

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        __m128i t2   = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk1)), _mm_cvtsi32_si128( *(const int *) (blk1 + lx1)));
        tmp2         = _mm_unpacklo_epi32(_mm_cvtsi32_si128( *(const int *) (blk2)), _mm_cvtsi32_si128( *(const int *) (blk2 + special_block_stride)));

        t2 = _mm_sad_epu8( t2, tmp2);

        s1 = _mm_add_epi32( s1, t1);
        s2 = _mm_add_epi32( s2, t2);

        s1 = _mm_add_epi32( s1, s2);

        return _mm_cvtsi128_si32( s1);
    }
    template<> int sad_special_avx2<8,4>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
        {
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            s1 = _mm_sad_epu8( s1, tmp3);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
        s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
        {
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            s2 = _mm_sad_epu8(s2, tmp3);
        }


        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
    template<> int sad_special_avx2<8,8>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
        {
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            s1 = _mm_sad_epu8( s1, tmp3);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
        s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
        {
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            s2 = _mm_sad_epu8(s2, tmp3);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s1 = _mm_add_epi32( s1, tmp1);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s2 = _mm_add_epi32( s2, tmp1);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
    template<> int sad_special_avx2<8,16>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = _mm_loadl_epi64( (const __m128i *) (blk1));
        s1 = _mm_loadh_epi64(s1, (const __m128i *) (blk1 + lx1));
        {
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            s1 = _mm_sad_epu8( s1, tmp3);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        __m128i s2 = _mm_loadl_epi64( (const __m128i *) (blk1));
        s2 = _mm_loadh_epi64(s2, (const __m128i *) (blk1 + lx1));
        {
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            s2 = _mm_sad_epu8(s2, tmp3);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s1 = _mm_add_epi32( s1, tmp1);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s2 = _mm_add_epi32( s2, tmp1);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s1 = _mm_add_epi32( s1, tmp1);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s2 = _mm_add_epi32( s2, tmp1);
        }
        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s1 = _mm_add_epi32( s1, tmp1);
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = _mm_loadl_epi64( (const __m128i *) (blk1));
            tmp1 = _mm_loadh_epi64(tmp1, (const __m128i *) (blk1 + lx1));
            __m128i tmp3 = _mm_loadl_epi64( (const __m128i *) (blk2));
            tmp3 = _mm_loadh_epi64(tmp3, (const __m128i *) (blk2 + special_block_stride));
            tmp1 = _mm_sad_epu8(tmp1, tmp3);
            s2 = _mm_add_epi32( s2, tmp1);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
    template<> int sad_special_avx2<16,8>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load128_unaligned( blk1);
        __m128i s2 = load128_unaligned( blk1+lx1);
        s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
        s2 = _mm_sad_epu8( s2, load128_block_data( blk2+special_block_stride));

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 2*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 3*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 4*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 5*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 6*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 7*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
    template<> int sad_special_avx2<16,16>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load128_unaligned( blk1);
        __m128i s2 = load128_unaligned( blk1 + lx1);
        s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
        s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + special_block_stride));

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 2*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 3*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 4*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 5*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 6*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 7*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 8*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 9*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 10*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 11*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 12*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 13*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }


        blk1 += 2*lx1;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2 + 14*special_block_stride));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + 15*special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
    template<> int sad_special_avx2<16,32>(const uint8_t *image, int img_stride, const uint8_t *block)
    {
        SAD_PARAMETERS_LOAD;

        __m128i s1 = load128_unaligned( blk1);
        __m128i s2 = load128_unaligned( blk1 + lx1);
        s1 = _mm_sad_epu8( s1, load128_block_data( blk2));
        s2 = _mm_sad_epu8( s2, load128_block_data( blk2 + special_block_stride));

        for(int i = 2; i < 30; i += 4)
        {
            blk1 += 2*lx1;
            blk2 += 2*special_block_stride;
            {
                __m128i tmp1 = load128_unaligned( blk1);
                __m128i tmp2 = load128_unaligned( blk1 + lx1);
                tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
                tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + special_block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }

            blk1 += 2*lx1;
            blk2 += 2*special_block_stride;

            {
                __m128i tmp1 = load128_unaligned( blk1);
                __m128i tmp2 = load128_unaligned( blk1 + lx1);
                tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
                tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + special_block_stride));
                s1 = _mm_add_epi32( s1, tmp1);
                s2 = _mm_add_epi32( s2, tmp2);
            }
        }

        blk1 += 2*lx1;
        blk2 += 2*special_block_stride;

        {
            __m128i tmp1 = load128_unaligned( blk1);
            __m128i tmp2 = load128_unaligned( blk1 + lx1);
            tmp1 = _mm_sad_epu8( tmp1, load128_block_data( blk2));
            tmp2 = _mm_sad_epu8( tmp2, load128_block_data( blk2 + special_block_stride));
            s1 = _mm_add_epi32( s1, tmp1);
            s2 = _mm_add_epi32( s2, tmp2);
        }

        s1 = _mm_add_epi32( s1, s2);
        s2 = _mm_movehl_epi64( s2, s1);
        s2 = _mm_add_epi32( s2, s1);

        return _mm_cvtsi128_si32( s2);
    }
    template<> int sad_special_avx2<32,16>(const uint8_t *image, int img_stride, const uint8_t *block) //OK
    {
        SAD_PARAMETERS_LOAD;

        __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

        s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
        s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + special_block_stride));

        for(int i = 0; i < 7; ++i)
        {
            blk1 += 2*lx1;
            blk2 += 2*special_block_stride;
            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + special_block_stride));
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
    template<> int sad_special_avx2<32,32>(const uint8_t *image, int img_stride, const uint8_t *block) //OK
    {
        SAD_PARAMETERS_LOAD;

        __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

        s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
        s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + special_block_stride));

        for(int i = 0; i < 15; ++i)
        {
            blk1 += 2*lx1;
            blk2 += 2*special_block_stride;
            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + special_block_stride));
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
    template<> int sad_special_avx2<32,64>(const uint8_t *image, int img_stride, const uint8_t *block) //OK
    {
        SAD_PARAMETERS_LOAD;

        __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

        s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
        s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + special_block_stride));

        for(int i = 0; i < 31; ++i)
        {
            blk1 += 2*lx1;
            blk2 += 2*special_block_stride;
            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + special_block_stride));
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
    template<> int sad_special_avx2<64,32>(const uint8_t *image, int img_stride, const uint8_t *block) //OK
    {
        SAD_PARAMETERS_LOAD;

        __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

        s1 = _mm256_sad_epu8( s1, load256_block_data(blk2));
        s2 = _mm256_sad_epu8( s2, load256_block_data(blk2 + special_block_stride));

        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + special_block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 32; i += 2)
        {
            blk1 += 2*lx1;
            blk2 += 2*special_block_stride;

            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + special_block_stride));
                s1 = _mm256_add_epi32( s1, tmp1);
                s2 = _mm256_add_epi32( s2, tmp2);
            }
            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + special_block_stride));
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
    template<> int sad_special_avx2<64,64>(const uint8_t *image, int img_stride, const uint8_t *block) //OK
    {
        SAD_PARAMETERS_LOAD;

        __m256i s1 = load256_unaligned( (const __m256i *) (blk1));
        __m256i s2 = load256_unaligned( (const __m256i *) (blk1 + lx1));

        s1 = _mm256_sad_epu8( s1, load256_block_data( blk2));
        s2 = _mm256_sad_epu8( s2, load256_block_data( blk2 + special_block_stride));

        {
            __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
            __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
            tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2 + 32));
            tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + 32 + special_block_stride));
            s1 = _mm256_add_epi32( s1, tmp1);
            s2 = _mm256_add_epi32( s2, tmp2);
        }

        for(int i = 2; i < 64; i += 2)
        {
            blk1 += 2*lx1;
            blk2 += 2*special_block_stride;

            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data( blk2));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data( blk2 + special_block_stride));
                s1 = _mm256_add_epi32( s1, tmp1);
                s2 = _mm256_add_epi32( s2, tmp2);
            }
            {
                __m256i tmp1 = load256_unaligned( (const __m256i *) (blk1 + 32));
                __m256i tmp2 = load256_unaligned( (const __m256i *) (blk1 + 32 + lx1));
                tmp1 = _mm256_sad_epu8( tmp1, load256_block_data(blk2 + 32));
                tmp2 = _mm256_sad_epu8( tmp2, load256_block_data(blk2 + 32 + special_block_stride));
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

    namespace {
        template <int height> int sad_4xN_special(const uint16_t* src, int32_t src_stride, const uint16_t* ref)
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
                xmm2 = _mm_loadl_epi64((__m128i *)(ref + 0*special_block_stride));
                xmm2 = _mm_loadh_epi64(xmm2, (ref + 1*special_block_stride));
                xmm3 = _mm_loadl_epi64((__m128i *)(ref + 2*special_block_stride));
                xmm3 = _mm_loadh_epi64(xmm3, (ref + 3*special_block_stride));
                ref += 4*special_block_stride;

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
        template <int height> int sad_8xN_special(const uint16_t* src, int32_t src_stride, const uint16_t* ref)
        {
            int h;
            __m128i xmm0, xmm1, xmm7;

            xmm7 = _mm_setzero_si128();

            for (h = 0; h < height; h += 2) {
                /* load 8x2 src block into two registers */
                xmm0 = _mm_loadu_si128((__m128i *)(src + 0*src_stride));
                xmm1 = _mm_loadu_si128((__m128i *)(src + 1*src_stride));
                src += 2*src_stride;

                /* calculate SAD */
                xmm0 = _mm_sub_epi16(xmm0, *(__m128i *)(ref + 0*special_block_stride));
                xmm1 = _mm_sub_epi16(xmm1, *(__m128i *)(ref + 1*special_block_stride));
                xmm0 = _mm_abs_epi16(xmm0);
                xmm1 = _mm_abs_epi16(xmm1);
                xmm0 = _mm_add_epi16(xmm0, xmm1);
                ref += 2*special_block_stride;

                /* add to 32-bit accumulator */
                xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)ones16s);
                xmm7 = _mm_add_epi32(xmm7, xmm0);
            }

            /* add up columns */
            xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x0e));
            xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x01));

            return _mm_cvtsi128_si32(xmm7);
        }
        template <int height> int sad_16xN_special(const uint16_t* src, int32_t src_stride, const uint16_t* ref)
        {
            int h;
            __m256i ymm0, ymm1, ymm7;

            _mm256_zeroupper();

            ymm7 = _mm256_setzero_si256();

            for (h = 0; h < height; h += 2) {
                /* load 16x2 src block into two registers */
                ymm0 = _mm256_loadu_si256((__m256i *)(src + 0*src_stride));
                ymm1 = _mm256_loadu_si256((__m256i *)(src + 1*src_stride));
                src += 2*src_stride;

                /* calculate SAD */
                ymm0 = _mm256_sub_epi16(ymm0, *(__m256i *)(ref + 0*special_block_stride));
                ymm1 = _mm256_sub_epi16(ymm1, *(__m256i *)(ref + 1*special_block_stride));
                ymm0 = _mm256_abs_epi16(ymm0);
                ymm1 = _mm256_abs_epi16(ymm1);
                ymm0 = _mm256_add_epi16(ymm0, ymm1);
                ref += 2*special_block_stride;

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
        template <int height> int sad_32xN_special(const uint16_t* src, int32_t src_stride, const uint16_t* ref)
        {
            int h;
            __m256i ymm0, ymm1, ymm2, ymm3, ymm_acc;

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
                ymm0 = _mm256_sub_epi16(ymm0, *(__m256i *)(ref + 0*special_block_stride +  0));
                ymm1 = _mm256_sub_epi16(ymm1, *(__m256i *)(ref + 0*special_block_stride + 16));
                ymm2 = _mm256_sub_epi16(ymm2, *(__m256i *)(ref + 1*special_block_stride +  0));
                ymm3 = _mm256_sub_epi16(ymm3, *(__m256i *)(ref + 1*special_block_stride + 16));
                ref += 2*special_block_stride;

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
        template <int height> int sad_64xN_special(const uint16_t* src, int32_t src_stride, const uint16_t* ref)
        {
            int h;
            __m256i ymm0, ymm1, ymm2, ymm3, ymm_acc;

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
                ref += special_block_stride;

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
    }

    template <> int sad16_special_avx2<4,4>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_4xN_special<4>(src,src_stride,ref); }
    template <> int sad16_special_avx2<4,8>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_4xN_special<8>(src,src_stride,ref); }
    template <> int sad16_special_avx2<8,4>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_8xN_special<4>(src,src_stride,ref); }
    template <> int sad16_special_avx2<8,8>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_8xN_special<8>(src,src_stride,ref); }
    template <> int sad16_special_avx2<8,16>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_8xN_special<16>(src,src_stride,ref); }
    template <> int sad16_special_avx2<16,8>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_16xN_special<8>(src,src_stride,ref); }
    template <> int sad16_special_avx2<16,16>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_16xN_special<16>(src,src_stride,ref); }
    template <> int sad16_special_avx2<16,32>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_16xN_special<32>(src,src_stride,ref); }
    template <> int sad16_special_avx2<32,16>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_32xN_special<16>(src,src_stride,ref); }
    template <> int sad16_special_avx2<32,32>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_32xN_special<32>(src,src_stride,ref); }
    template <> int sad16_special_avx2<32,64>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_32xN_special<64>(src,src_stride,ref); }
    template <> int sad16_special_avx2<64,32>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_64xN_special<32>(src,src_stride,ref); }
    template <> int sad16_special_avx2<64,64>(const uint16_t* src, int32_t src_stride, const uint16_t* ref) { return sad_64xN_special<64>(src,src_stride,ref); }


    template<> int sad_general_avx2<4,4>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<4,8>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<8,4>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<8,8>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<8,16>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<16,8>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<16,16>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<16,32>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride)
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
    template<> int sad_general_avx2<32,16>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride) //OK
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
    template<> int sad_general_avx2<32,32>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride) //OK
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
    template<> int sad_general_avx2<32,64>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride) //OK
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
    template<> int sad_general_avx2<64,32>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride) //OK
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
    template<> int sad_general_avx2<64,64>(const uint8_t *image, int img_stride, const uint8_t *block, int block_stride) //OK
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


    namespace {
        template <int height> int sad_4xN_general(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride)
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
        template <int height> int sad_8xN_general(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride)
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
        template <int height> int sad_16xN_general(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride)
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
        template <int height> int sad_32xN_general(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride)
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
        template <int height> int sad_64xN_general(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride)
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
    };

    template <> int sad16_general_avx2<4,4>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_4xN_general<4>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<4,8>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_4xN_general<8>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<8,4>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_8xN_general<4>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<8,8>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_8xN_general<8>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<8,16>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_8xN_general<16>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<16,8>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_16xN_general<8>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<16,16>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_16xN_general<16>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<16,32>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_16xN_general<32>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<32,16>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_32xN_general<16>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<32,32>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_32xN_general<32>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<32,64>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_32xN_general<64>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<64,32>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_64xN_general<32>(src,src_stride,ref,ref_stride); }
    template <> int sad16_general_avx2<64,64>(const uint16_t* src, int32_t src_stride, const uint16_t* ref, int32_t ref_stride) { return sad_64xN_general<64>(src,src_stride,ref,ref_stride); }


    template <int size> int sad_store8x8_avx2(const unsigned char *p1, const unsigned char *p2, int *sads8x8);
    template<> int sad_store8x8_avx2<8> (const unsigned char *p1, const unsigned char *p2, int *sads8x8) {
        __m256i a = _mm256_setr_epi64x(*(int64_t*)p1, *(int64_t*)(p1+64), *(int64_t*)(p1+128), *(int64_t*)(p1+192));
        __m256i b = _mm256_setr_epi64x(*(int64_t*)p2, *(int64_t*)(p2+64), *(int64_t*)(p2+128), *(int64_t*)(p2+192));
        __m256i s = _mm256_sad_epu8(a, b);

        a = _mm256_setr_epi64x(*(int64_t*)(p1+256), *(int64_t*)(p1+320), *(int64_t*)(p1+384), *(int64_t*)(p1+448));
        b = _mm256_setr_epi64x(*(int64_t*)(p2+256), *(int64_t*)(p2+320), *(int64_t*)(p2+384), *(int64_t*)(p2+448));
        s = _mm256_add_epi64(s, _mm256_sad_epu8(a, b));
        __m128i tmp = _mm_add_epi32(si128(s), si128_hi(s));
        int sad = _mm_cvtsi128_si32(tmp) + _mm_extract_epi32(tmp, 2);
        *sads8x8 = sad;
        return sad;
    }
    template<> int sad_store8x8_avx2<16>(const unsigned char *p1, const unsigned char *p2, int *sads8x8) {
        __m256i sad = _mm256_sad_epu8(loada2_m128i(p1, p1 + 64*8), loada2_m128i(p2, p2 + 64*8));
        p1 += 64, p2 += 64;
        for (int i = 1; i < 8; i++, p1 += 64, p2 += 64)
            sad = _mm256_add_epi64(sad, _mm256_sad_epu8(loada2_m128i(p1, p1 + 64*8), loada2_m128i(p2, p2 + 64*8)));
        sad = _mm256_shuffle_epi32(sad, SHUF32(0, 2, 0, 0));
        __m128i lo = si128_lo(sad);
        __m128i hi = si128_hi(sad);
        storel_epi64(sads8x8 + 0, lo);
        storel_epi64(sads8x8 + 8, hi);
        __m128i tmp = _mm_add_epi64(lo, hi);
        return _mm_cvtsi128_si32(tmp) + _mm_extract_epi32(tmp, 1);
    }
    template<> int sad_store8x8_avx2<32>(const unsigned char *p1, const unsigned char *p2, int *sads8x8) {
        const __m256i shufmask = _mm256_setr_epi32(0, 2, 4, 6, 0, 0, 0, 0);
        __m256i totalSad = _mm256_setzero_si256();
        for (int y = 0; y < 32; y += 8, sads8x8 += 8, p1 += 8 * 64, p2 += 8 * 64) {
            __m256i sad = _mm256_sad_epu8(loada_si256(p1), loada_si256(p2));
            for (int i = 1; i < 8; i++)
                sad = _mm256_add_epi64(sad, _mm256_sad_epu8(loada_si256(p1 + i * 64), loada_si256(p2 + i * 64)));
            totalSad = _mm256_add_epi64(totalSad, sad);
            sad = _mm256_permutevar8x32_epi32(sad, shufmask);
            storea_si128(sads8x8, si128(sad));
        }
        __m128i tmp = _mm_add_epi64(si128_lo(totalSad), si128_hi(totalSad));
        return _mm_cvtsi128_si32(tmp) + _mm_extract_epi32(tmp, 2);
    }
    template<> int sad_store8x8_avx2<64>(const unsigned char *p1, const unsigned char *p2, int *sads8x8) {
        __m256i totalSad = _mm256_setzero_si256();
        for (int y = 0; y < 64; y += 8, sads8x8 += 8, p1 += 8 * 64, p2 += 8 * 64) {
            __m256i sad0 = _mm256_sad_epu8(loada_si256(p1 +  0), loada_si256(p2 +  0));
            __m256i sad1 = _mm256_sad_epu8(loada_si256(p1 + 32), loada_si256(p2 + 32));
            for (int i = 1; i < 8; i++) {
                sad0 = _mm256_add_epi64(sad0, _mm256_sad_epu8(loada_si256(p1 + i * 64 +  0), loada_si256(p2 + i * 64 +  0)));
                sad1 = _mm256_add_epi64(sad1, _mm256_sad_epu8(loada_si256(p1 + i * 64 + 32), loada_si256(p2 + i * 64 + 32)));
            }
            sad0 = _mm256_packus_epi32(sad0, sad1);
            totalSad = _mm256_add_epi32(totalSad, sad0);
            sad0 = _mm256_permute4x64_epi64(sad0, PERM4x64(0,2,1,3));
            storea_si256(sads8x8, sad0);
        }
        __m128i tmp = _mm_add_epi64(si128_lo(totalSad), si128_hi(totalSad));
        tmp = _mm_hadd_epi32(tmp, tmp);
        return _mm_cvtsi128_si32(tmp) + _mm_extract_epi32(tmp, 1);
    }
}
