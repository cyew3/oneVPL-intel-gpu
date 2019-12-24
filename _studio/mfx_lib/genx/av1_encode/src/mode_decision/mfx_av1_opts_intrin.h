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

#pragma once

#include "assert.h"
#include "immintrin.h"
#include "mfx_av1_opts_common.h"

#define SHUF16(c0, c1, c2, c3) c0+(c1<<2)+(c2<<4)+(c3<<6)
#define SHUF32(c0, c1, c2, c3) c0+(c1<<2)+(c2<<4)+(c3<<6)
#define SHUFPD(c0, c1) c0+(c1<<1)
#define PERM4x64(c0, c1, c2, c3) c0+(c1<<2)+(c2<<4)+(c3<<6)
#define PERM2x128(c0, c1) c0+(c1<<4)

namespace VP9PP {
    inline H265_FORCEINLINE __m128  ps(__m128i r) { return _mm_castsi128_ps(r); }
    inline H265_FORCEINLINE __m128d pd(__m128i r) { return _mm_castsi128_pd(r); }
    inline H265_FORCEINLINE __m256  ps(__m256i r) { return _mm256_castsi256_ps(r); }
    inline H265_FORCEINLINE __m256d pd(__m256i r) { return _mm256_castsi256_pd(r); }
    inline H265_FORCEINLINE __m128i si128(__m128  r) { return _mm_castps_si128(r); }
    inline H265_FORCEINLINE __m128i si128(__m128d r) { return _mm_castpd_si128(r); }
    inline H265_FORCEINLINE __m128i si128(__m256i r) { return _mm256_castsi256_si128(r); }
    inline H265_FORCEINLINE __m128i si128_lo(__m256i r) { return si128(r); }
    inline H265_FORCEINLINE __m128i si128_hi(__m256i r) { return _mm256_extracti128_si256(r, 1); }
    inline H265_FORCEINLINE __m256i si256(__m128i r) { return _mm256_castsi128_si256(r); }
    inline H265_FORCEINLINE __m256i si256(__m256  r) { return _mm256_castps_si256(r); }
    inline H265_FORCEINLINE __m256i si256(__m256d r) { return _mm256_castpd_si256(r); }

    inline H265_FORCEINLINE __m128i loada_si128(const void *p) { assert(!(size_t(p)&15)); return _mm_load_si128((__m128i *)p); }
    inline H265_FORCEINLINE __m256i loada_si256(const void *p) { assert(!(size_t(p)&31)); return _mm256_load_si256((__m256i *)p); }
    inline H265_FORCEINLINE __m128i loadu_si128(const void *p) { return _mm_loadu_si128((__m128i *)p); }
    inline H265_FORCEINLINE __m128i loaddup_epi64(const void *p) { return si128(_mm_loaddup_pd((const double *)p)); }
    inline H265_FORCEINLINE __m256i loadu_si256(const void *p) { return _mm256_loadu_si256((__m256i *)p); }
    inline H265_FORCEINLINE __m128i loadl_epi64(const void *p) { return _mm_loadl_epi64((__m128i*)p); }
    inline H265_FORCEINLINE __m128i loadl_si32(const void *p) { return si128(_mm_load_ss((const float*)p)); }
    inline H265_FORCEINLINE __m128i loadh_epi64(__m128i r, const void *p) { return si128(_mm_loadh_pd(pd(r), (const double *)p)); }
    inline H265_FORCEINLINE __m128i loadh_epi64(const void *p) { return loadh_epi64(_mm_setzero_si128(), p); }
    inline H265_FORCEINLINE __m256i loada2_m128i(const void *lo, const void *hi) { return _mm256_insertf128_si256(si256(loada_si128(lo)), loada_si128(hi), 1); }
    inline H265_FORCEINLINE __m256i loadu2_m128i(const void *lo, const void *hi) { return _mm256_loadu2_m128i((const __m128i *)hi, (const __m128i *)lo); }
    inline H265_FORCEINLINE __m128i loadu2_epi64(const void *lo, const void *hi) { return loadh_epi64(loadl_epi64(lo), hi); }
    inline H265_FORCEINLINE __m128i loadu_si32(const void *p) { return si128(_mm_load_ss((const float*)p)); }
    inline H265_FORCEINLINE __m256i loadu4_epi64(const void *p0, const void *p1, const void *p2, const void *p3) {
        return _mm256_setr_epi64x(*(long long *)p0, *(long long *)p1, *(long long *)p2, *(long long *)p3);
    }
    inline H265_FORCEINLINE __m128i loadu4_epi32(const void *p0, const void *p1, const void *p2, const void *p3) {
        return _mm_setr_epi32(*(int *)p0, *(int *)p1, *(int *)p2, *(int *)p3);
    }

    inline H265_FORCEINLINE void storea_si128(void *p, __m128i r) { assert(!(size_t(p)&15)); return _mm_store_si128((__m128i *)p, r); }
    inline H265_FORCEINLINE void storea_si256(void *p, __m256i r) { assert(!(size_t(p)&31)); return _mm256_store_si256((__m256i *)p, r); }
    inline H265_FORCEINLINE void storeu_si128(void *p, __m128i r) { _mm_storeu_si128((__m128i *)p, r); }
    inline H265_FORCEINLINE void storeu_si256(void *p, __m256i r) { _mm256_storeu_si256((__m256i *)p, r); }
    inline H265_FORCEINLINE void storel_si128(void *p, __m256i r) { storea_si128(p, si128(r)); }
    inline H265_FORCEINLINE void storel_epi64(void *p, __m128i r) { _mm_storel_epi64((__m128i *)p, r); }
    inline H265_FORCEINLINE void storel_si32(void *p, __m128i r) { _mm_store_ss((float *)p, ps(r)); }
    inline H265_FORCEINLINE void storeh_si128(void *p, __m256i r) { storea_si128(p, _mm256_extracti128_si256(r, 1)); }
    inline H265_FORCEINLINE void storeh_epi64(void *p, __m128i r) { _mm_storeh_pd((double*)p, pd(r)); }
    inline H265_FORCEINLINE void storeu2_m128i(void *lo, void *hi, __m256i r) { _mm256_storeu2_m128i((__m128i *)hi, (__m128i *)lo, r); }
    inline H265_FORCEINLINE void storea2_m128i(void *lo, void *hi, __m256i r) { storea_si128(lo, si128(r)); storea_si128(hi, _mm256_extracti128_si256(r, 1)); }

};  // namesapce VP9PP
