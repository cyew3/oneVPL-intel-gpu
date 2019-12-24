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
using namespace VP9PP;

enum { PITCH, P64, PW };

static inline int hsum_i32(__m256i r) {
    __m128i a = _mm_add_epi32(si128_lo(r), si128_hi(r));
    a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
    return _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
}

static inline __m256i update_sse(__m256i sse, __m256i src1, __m256i src2) {
    __m256i d, d0, d1;

    d = _mm256_or_si256(_mm256_subs_epu8(src1, src2),
                        _mm256_subs_epu8(src2, src1));  // d=abs(s1-s2)
    d0 = _mm256_unpacklo_epi8(d, _mm256_setzero_si256());
    d1 = _mm256_unpackhi_epi8(d, _mm256_setzero_si256());
    d0 = _mm256_madd_epi16(d0, d0);
    d1 = _mm256_madd_epi16(d1, d1);
    sse = _mm256_add_epi32(sse, d0);
    sse = _mm256_add_epi32(sse, d1);
    return sse;
}

template <int h, int layout1, int layout2>
static inline int sse_w4(const uint8_t *src1, int pitch1_, const uint8_t *src2, int pitch2_, int h_)
{
    const int width = 4;
    const int height = h > 0 ? h : h_;
    const int pitch1 = (layout1 == PITCH) ? pitch1_ : (layout1 == P64) ? 64 : width;
    const int pitch2 = (layout2 == PITCH) ? pitch2_ : (layout2 == P64) ? 64 : width;

    __m256i zero = _mm256_setzero_si256();
    __m256i sse = _mm256_setzero_si256();
    __m256i s1, s2, d;
    for (int y = 0; y < height; y += 4, src1 += 4 * pitch1, src2 += 4 * pitch2) {
        s1 = (layout1 == PW)
            ? _mm256_permute4x64_epi64(si256(loada_si128(src1)), PERM4x64(0,0,1,1))
            : _mm256_setr_m128i(
                _mm_unpacklo_epi32(loadl_si32(src1 + 0 * pitch1), loadl_si32(src1 + 1 * pitch1)),
                _mm_unpacklo_epi32(loadl_si32(src1 + 2 * pitch1), loadl_si32(src1 + 3 * pitch1)));
        s2 = (layout2 == PW)
            ? _mm256_permute4x64_epi64(si256(loada_si128(src2)), PERM4x64(0,0,1,1))
            : _mm256_setr_m128i(
                _mm_unpacklo_epi32(loadl_si32(src2 + 0 * pitch2), loadl_si32(src2 + 1 * pitch2)),
                _mm_unpacklo_epi32(loadl_si32(src2 + 2 * pitch2), loadl_si32(src2 + 3 * pitch2)));

        s1 = _mm256_unpacklo_epi8(s1, zero);
        s2 = _mm256_unpacklo_epi8(s2, zero);
        d = _mm256_sub_epi16(s1, s2);
        d = _mm256_madd_epi16(d, d);
        sse = _mm256_add_epi32(sse, d);
    }
    return hsum_i32(sse);
}

template <int h, int layout1, int layout2>
static inline int sse_w8(const uint8_t *src1, int pitch1_, const uint8_t *src2, int pitch2_, int h_)
{
    const int width = 8;
    const int height = h > 0 ? h : h_;
    const int pitch1 = (layout1 == PITCH) ? pitch1_ : (layout1 == P64) ? 64 : width;
    const int pitch2 = (layout2 == PITCH) ? pitch2_ : (layout2 == P64) ? 64 : width;

    __m256i sse = _mm256_setzero_si256();
    for (int y = 0; y < height; y += 4, src1 += 4 * pitch1, src2 += 4 * pitch2) {
        __m256i s1 = (layout1 == PW)
            ? loada_si256(src1)
            : _mm256_setr_m128i(loadu2_epi64(src1 + 0 * pitch1, src1 + 1 * pitch1),
                                loadu2_epi64(src1 + 2 * pitch1, src1 + 3 * pitch1));

        __m256i s2 = (layout2 == PW)
            ? loada_si256(src2)
            : _mm256_setr_m128i(loadu2_epi64(src2 + 0 * pitch2, src2 + 1 * pitch2),
                                loadu2_epi64(src2 + 2 * pitch2, src2 + 3 * pitch2));

        sse = update_sse(sse, s1, s2);
    }
    return hsum_i32(sse);
}

template <int h, int layout1, int layout2>
static inline int sse_w16(const uint8_t *src1, int pitch1_, const uint8_t *src2, int pitch2_, int h_)
{
    const int width = 16;
    const int height = h > 0 ? h : h_;
    const int pitch1 = (layout1 == PITCH) ? pitch1_ : (layout1 == P64) ? 64 : width;
    const int pitch2 = (layout2 == PITCH) ? pitch2_ : (layout2 == P64) ? 64 : width;

    __m256i sse = _mm256_setzero_si256();
    for (int y = 0; y < height; y += 2, src1 += 2 * pitch1, src2 += 2 * pitch2) {
        __m256i s1 = (layout1 == PW) ? loada_si256(src1) : loada2_m128i(src1, src1 + pitch1);
        __m256i s2 = (layout2 == PW) ? loada_si256(src2) : loada2_m128i(src2, src2 + pitch2);
        sse = update_sse(sse, s1, s2);
    }
    return hsum_i32(sse);
}

template <int h, int layout1, int layout2>
static inline int sse_w32(const uint8_t *src1, int pitch1_, const uint8_t *src2, int pitch2_, int h_)
{
    const int width = 32;
    const int height = h > 0 ? h : h_;
    const int pitch1 = (layout1 == PITCH) ? pitch1_ : (layout1 == P64) ? 64 : width;
    const int pitch2 = (layout2 == PITCH) ? pitch2_ : (layout2 == P64) ? 64 : width;

    __m256i sse = _mm256_setzero_si256();
    for (int y = 0; y < height; y += 4, src1 += 4 * pitch1, src2 += 4 * pitch2) {
        sse = update_sse(sse, loada_si256(src1 + 0 * pitch1), loada_si256(src2 + 0 * pitch2));
        sse = update_sse(sse, loada_si256(src1 + 1 * pitch1), loada_si256(src2 + 1 * pitch2));
        sse = update_sse(sse, loada_si256(src1 + 2 * pitch1), loada_si256(src2 + 2 * pitch2));
        sse = update_sse(sse, loada_si256(src1 + 3 * pitch1), loada_si256(src2 + 3 * pitch2));
    }
    return hsum_i32(sse);
}

template <int h, int layout1, int layout2>
static inline int sse_w64(const uint8_t *src1, int pitch1_, const uint8_t *src2, int pitch2_, int h_)
{
    const int width = 64;
    const int height = h > 0 ? h : h_;
    const int pitch1 = (layout1 == PITCH) ? pitch1_ : (layout1 == P64) ? 64 : width;
    const int pitch2 = (layout2 == PITCH) ? pitch2_ : (layout2 == P64) ? 64 : width;

    __m256i sse = _mm256_setzero_si256();
    for (int y = 0; y < height; y += 2, src1 += 2 * pitch1, src2 += 2 * pitch2) {
        sse = update_sse(sse, loada_si256(src1 + 0 * pitch1 +  0), loada_si256(src2 + 0 * pitch2 +  0));
        sse = update_sse(sse, loada_si256(src1 + 0 * pitch1 + 32), loada_si256(src2 + 0 * pitch2 + 32));
        sse = update_sse(sse, loada_si256(src1 + 1 * pitch1 +  0), loada_si256(src2 + 1 * pitch2 +  0));
        sse = update_sse(sse, loada_si256(src1 + 1 * pitch1 + 32), loada_si256(src2 + 1 * pitch2 + 32));
    }
    return hsum_i32(sse);
}

namespace VP9PP
{
    template <int w, int h> int sse_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);

    template <> int sse_avx2<4,4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w4<4,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<4,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w4<8,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<8,4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w8<4,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<8,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w8<8,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<8,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w8<16,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<16,4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w16<4,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<16,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w16<8,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<16,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w16<16,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<16,32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w16<32,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<32,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w32<8,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<32,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w32<16,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<32,32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w32<32,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<32,64>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w32<64,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<64,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w64<16,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<64,32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w64<32,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }
    template <> int sse_avx2<64,64>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return sse_w64<64,PITCH,PITCH>(src1, pitch1, src2, pitch2, 0);
    }

    template <int w, int h> int sse_p64_pw_avx2(const uint8_t *src1, const uint8_t *src2);

    template <> int sse_p64_pw_avx2<4,4>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w4<4,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<4,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w4<8,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<8,4>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w8<4,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<8,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w8<8,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<8,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w8<16,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<16,4>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<4,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<16,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<8,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<16,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<16,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<16,32>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<32,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<32,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<8,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<32,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<16,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<32,32>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<32,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<32,64>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<64,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<64,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w64<16,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<64,32>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w64<32,P64,PW>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_pw_avx2<64,64>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w64<64,P64,PW>(src1, 0, src2, 0, 0);
    }

    template <int w, int h> int sse_p64_p64_avx2(const uint8_t *src1, const uint8_t *src2);

    template <> int sse_p64_p64_avx2<4,4>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w4<4,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<4,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w4<8,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<8,4>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w8<4,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<8,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w8<8,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<8,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w8<16,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<16,4>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<4,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<16,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<8,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<16,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<16,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<16,32>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w16<32,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<32,8>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<8,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<32,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<16,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<32,32>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<32,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<32,64>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w32<64,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<64,16>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w64<16,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<64,32>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w64<32,P64,P64>(src1, 0, src2, 0, 0);
    }
    template <> int sse_p64_p64_avx2<64,64>(const uint8_t *src1, const uint8_t *src2) {
        return sse_w64<64,P64,P64>(src1, 0, src2, 0, 0);
    }

    template <int w> int sse_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h);
    template <> int sse_avx2<4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return sse_w4<0,PITCH,PITCH>(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return sse_w8<0,PITCH,PITCH>(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return sse_w16<0,PITCH,PITCH>(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return sse_w32<0,PITCH,PITCH>(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<64>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return sse_w64<0,PITCH,PITCH>(src1, pitch1, src2, pitch2, h);
    }

    // sse function for continuous blocks, ie. pitch == width for both block.
    // limited to multiple-of-32 pixels for now
    int sse_cont_8u_avx2(const uint8_t *src1, const uint8_t *src2, int numPixels) {
        assert((numPixels & 31) == 0);
        __m256i sse = _mm256_setzero_si256();
        for (; numPixels > 0; numPixels -= 32, src1 += 32, src2 += 32)
            sse = update_sse(sse, loada_si256(src1), loada_si256(src2));
        return hsum_i32(sse);
    }

    int64_t sse_cont_avx2(const int16_t *src1, const int16_t *src2, int block_size)
    {
        __m256i zero = _mm256_setzero_si256();
        __m256i sse = _mm256_setzero_si256();
        __m256i s1, s2, d, lo, hi;
        for (int32_t i = 0; i < block_size; i += 16) {
            s1 = loada_si256(src1 + i);
            s2 = loada_si256(src2 + i);
            d = _mm256_sub_epi16(s1, s2);
            d = _mm256_madd_epi16(d, d);
            lo = _mm256_unpacklo_epi32(d, zero);
            hi = _mm256_unpackhi_epi32(d, zero);
            sse = _mm256_add_epi64(sse, lo);
            sse = _mm256_add_epi64(sse, hi);
        }

        sse = _mm256_add_epi64(sse, _mm256_srli_si256(sse, 8));
        return _mm_cvtsi128_si64(si128_lo(sse)) + _mm_cvtsi128_si64(si128_hi(sse));
    }

    int64_t ssz_cont_avx2(const int16_t *src, int block_size)
    {
        __m256i zero = _mm256_setzero_si256();
        __m256i sse = _mm256_setzero_si256();
        __m256i s, lo, hi;
        for (int32_t i = 0; i < block_size; i += 16) {
            s = loada_si256(src + i);
            s = _mm256_madd_epi16(s, s);
            lo = _mm256_unpacklo_epi32(s, zero);
            hi = _mm256_unpackhi_epi32(s, zero);
            sse = _mm256_add_epi64(sse, lo);
            sse = _mm256_add_epi64(sse, hi);
        }

        sse = _mm256_add_epi64(sse, _mm256_srli_si256(sse, 8));
        return _mm_cvtsi128_si64(_mm_add_epi64(si128_lo(sse), si128_hi(sse)));
    }

};  // namespace VP9PP
