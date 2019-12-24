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
#ifdef _WIN32
#include "intrin.h"
#endif
#include "immintrin.h"
#include "stdlib.h"
#include "mfx_av1_opts_intrin.h"

#include  <algorithm>

#define ALIGN_POWER_OF_TWO(value, n) (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

/* We only need to buffer three horizontal pixels too, but let's align to
   16 bytes (8 x 16 bits) to make vectorization easier. */
#define CDEF_HBORDER (8)
#define CDEF_BLOCKSIZE 64
#define CDEF_BSTRIDE ALIGN_POWER_OF_TWO(CDEF_BLOCKSIZE + 2 * CDEF_HBORDER, 3)
#define CDEF_VERY_LARGE (30000)

namespace AV1PP
{
    typedef __m128i v128;
    typedef __m128i v64;
    struct v256 { v128 lo, hi; };

#define SIMD_INLINE static AV1_FORCEINLINE

    SIMD_INLINE v128 v128_dup_16(uint16_t x) { return _mm_set1_epi16(x); }

    SIMD_INLINE v128 v128_load_unaligned(const void *p) { return _mm_loadu_si128((__m128i *)p); }

    SIMD_INLINE v128 v128_sub_16(v128 a, v128 b) { return _mm_sub_epi16(a, b); }

    #define v128_shl_n_16(a, c) _mm_slli_epi16(a, c)

    #define v128_shr_n_s16(a, c) _mm_srai_epi16(a, c)

    #define v128_shr_n_byte(a, c) _mm_srli_si128(a, c)

    SIMD_INLINE v128 v128_shr_s16(v128 a, unsigned int c) { return _mm_sra_epi16(a, _mm_cvtsi32_si128(c)); }


    static inline int get_msb(unsigned int n) {
#ifdef _WIN32
        unsigned long first_set_bit = 0;
#else
        unsigned int first_set_bit = 0;
#endif
        assert(n != 0);
        _BitScanReverse(&first_set_bit, n);
        return first_set_bit;
    }

    ALIGN_DECL(32) static const int cdef_directions[8][2] = {
        { -1 * CDEF_BSTRIDE + 1, -2 * CDEF_BSTRIDE + 2 },
        {  0 * CDEF_BSTRIDE + 1, -1 * CDEF_BSTRIDE + 2 },
        {  0 * CDEF_BSTRIDE + 1,  0 * CDEF_BSTRIDE + 2 },
        {  0 * CDEF_BSTRIDE + 1,  1 * CDEF_BSTRIDE + 2 },
        {  1 * CDEF_BSTRIDE + 1,  2 * CDEF_BSTRIDE + 2 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 1 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 0 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE - 1 }
    };
    ALIGN_DECL(32) static const int cdef_directions_nv12[8][2] = {
        { -1 * CDEF_BSTRIDE + 2, -2 * CDEF_BSTRIDE + 4 },
        {  0 * CDEF_BSTRIDE + 2, -1 * CDEF_BSTRIDE + 4 },
        {  0 * CDEF_BSTRIDE + 2,  0 * CDEF_BSTRIDE + 4 },
        {  0 * CDEF_BSTRIDE + 2,  1 * CDEF_BSTRIDE + 4 },
        {  1 * CDEF_BSTRIDE + 2,  2 * CDEF_BSTRIDE + 4 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 2 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE + 0 },
        {  1 * CDEF_BSTRIDE + 0,  2 * CDEF_BSTRIDE - 2 }
    };
    ALIGN_DECL(16) static const int cdef_pri_taps[2][2] = { { 4, 2 }, { 3, 3 } };
    //static const int cdef_sec_taps[2][2] = { { 2, 1 }, { 2, 1 } };

    /* partial A is a 16-bit vector of the form:
    [x8 x7 x6 x5 x4 x3 x2 x1] and partial B has the form:
    [0  y1 y2 y3 y4 y5 y6 y7].
    This function computes (x1^2+y1^2)*C1 + (x2^2+y2^2)*C2 + ...
    (x7^2+y2^7)*C7 + (x8^2+0^2)*C8 where the C1..C8 constants are in const1
    and const2. */
    __m128i fold_mul_and_sum(__m128i partiala, __m128i partialb, __m128i const1, __m128i const2) {
        __m128i tmp;
        /* Reverse partial B. */
        partialb = _mm_shuffle_epi8(
            partialb,
            _mm_set_epi8(15, 14, 1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12));
        /* Interleave the x and y values of identical indices and pair x8 with 0. */
        tmp = partiala;
        partiala = _mm_unpacklo_epi16(partiala, partialb);
        partialb = _mm_unpackhi_epi16(tmp, partialb);
        /* Square and add the corresponding x and y values. */
        partiala = _mm_madd_epi16(partiala, partiala);
        partialb = _mm_madd_epi16(partialb, partialb);
        /* Multiply by constant. */
        partiala = _mm_mullo_epi32(partiala, const1);
        partialb = _mm_mullo_epi32(partialb, const2);
        /* Sum all results. */
        partiala = _mm_add_epi32(partiala, partialb);
        return partiala;
    }

    static inline __m128i hsum4(__m128i x0, __m128i x1, __m128i x2, __m128i x3) {
        __m128i t0, t1, t2, t3;
        t0 = _mm_unpacklo_epi32(x0, x1);
        t1 = _mm_unpacklo_epi32(x2, x3);
        t2 = _mm_unpackhi_epi32(x0, x1);
        t3 = _mm_unpackhi_epi32(x2, x3);
        x0 = _mm_unpacklo_epi64(t0, t1);
        x1 = _mm_unpackhi_epi64(t0, t1);
        x2 = _mm_unpacklo_epi64(t2, t3);
        x3 = _mm_unpackhi_epi64(t2, t3);
        return _mm_add_epi32(_mm_add_epi32(x0, x1), _mm_add_epi32(x2, x3));
    }

    /* Computes cost for directions 0, 5, 6 and 7. We can call this function again
    to compute the remaining directions. */
    static inline __m128i compute_directions(__m128i lines[8], int tmp_cost1[4])
    {
        __m128i partial4a, partial4b, partial5a, partial5b, partial7a, partial7b;
        __m128i partial6;
        __m128i tmp;
        /* Partial sums for lines 0 and 1. */
        partial4a = _mm_slli_si128(lines[0], 14);
        partial4b = _mm_srli_si128(lines[0], 2);
        partial4a = _mm_add_epi16(partial4a, _mm_slli_si128(lines[1], 12));
        partial4b = _mm_add_epi16(partial4b, _mm_srli_si128(lines[1], 4));
        tmp = _mm_add_epi16(lines[0], lines[1]);
        partial5a = _mm_slli_si128(tmp, 10);
        partial5b = _mm_srli_si128(tmp, 6);
        partial7a = _mm_slli_si128(tmp, 4);
        partial7b = _mm_srli_si128(tmp, 12);
        partial6 = tmp;

        /* Partial sums for lines 2 and 3. */
        partial4a = _mm_add_epi16(partial4a, _mm_slli_si128(lines[2], 10));
        partial4b = _mm_add_epi16(partial4b, _mm_srli_si128(lines[2], 6));
        partial4a = _mm_add_epi16(partial4a, _mm_slli_si128(lines[3], 8));
        partial4b = _mm_add_epi16(partial4b, _mm_srli_si128(lines[3], 8));
        tmp = _mm_add_epi16(lines[2], lines[3]);
        partial5a = _mm_add_epi16(partial5a, _mm_slli_si128(tmp, 8));
        partial5b = _mm_add_epi16(partial5b, _mm_srli_si128(tmp, 8));
        partial7a = _mm_add_epi16(partial7a, _mm_slli_si128(tmp, 6));
        partial7b = _mm_add_epi16(partial7b, _mm_srli_si128(tmp, 10));
        partial6 = _mm_add_epi16(partial6, tmp);

        /* Partial sums for lines 4 and 5. */
        partial4a = _mm_add_epi16(partial4a, _mm_slli_si128(lines[4], 6));
        partial4b = _mm_add_epi16(partial4b, _mm_srli_si128(lines[4], 10));
        partial4a = _mm_add_epi16(partial4a, _mm_slli_si128(lines[5], 4));
        partial4b = _mm_add_epi16(partial4b, _mm_srli_si128(lines[5], 12));
        tmp = _mm_add_epi16(lines[4], lines[5]);
        partial5a = _mm_add_epi16(partial5a, _mm_slli_si128(tmp, 6));
        partial5b = _mm_add_epi16(partial5b, _mm_srli_si128(tmp, 10));
        partial7a = _mm_add_epi16(partial7a, _mm_slli_si128(tmp, 8));
        partial7b = _mm_add_epi16(partial7b, _mm_srli_si128(tmp, 8));
        partial6 = _mm_add_epi16(partial6, tmp);

        /* Partial sums for lines 6 and 7. */
        partial4a = _mm_add_epi16(partial4a, _mm_slli_si128(lines[6], 2));
        partial4b = _mm_add_epi16(partial4b, _mm_srli_si128(lines[6], 14));
        partial4a = _mm_add_epi16(partial4a, lines[7]);
        tmp = _mm_add_epi16(lines[6], lines[7]);
        partial5a = _mm_add_epi16(partial5a, _mm_slli_si128(tmp, 4));
        partial5b = _mm_add_epi16(partial5b, _mm_srli_si128(tmp, 12));
        partial7a = _mm_add_epi16(partial7a, _mm_slli_si128(tmp, 10));
        partial7b = _mm_add_epi16(partial7b, _mm_srli_si128(tmp, 6));
        partial6 = _mm_add_epi16(partial6, tmp);

        /* Compute costs in terms of partial sums. */
        partial4a =
            fold_mul_and_sum(partial4a, partial4b, _mm_set_epi32(210, 280, 420, 840),
            _mm_set_epi32(105, 120, 140, 168));
        partial7a =
            fold_mul_and_sum(partial7a, partial7b, _mm_set_epi32(210, 420, 0, 0),
            _mm_set_epi32(105, 105, 105, 140));
        partial5a =
            fold_mul_and_sum(partial5a, partial5b, _mm_set_epi32(210, 420, 0, 0),
            _mm_set_epi32(105, 105, 105, 140));
        partial6 = _mm_madd_epi16(partial6, partial6);
        partial6 = _mm_mullo_epi32(partial6, _mm_set1_epi32(105));

        partial4a = hsum4(partial4a, partial5a, partial6, partial7a);
        _mm_storeu_si128((__m128i *)tmp_cost1, partial4a);
        return partial4a;
    }

    /* transpose and reverse the order of the lines -- equivalent to a 90-degree
    counter-clockwise rotation of the pixels. */
    static inline void array_reverse_transpose_8x8(__m128i *in, __m128i *res) {
        const __m128i tr0_0 = _mm_unpacklo_epi16(in[0], in[1]);
        const __m128i tr0_1 = _mm_unpacklo_epi16(in[2], in[3]);
        const __m128i tr0_2 = _mm_unpackhi_epi16(in[0], in[1]);
        const __m128i tr0_3 = _mm_unpackhi_epi16(in[2], in[3]);
        const __m128i tr0_4 = _mm_unpacklo_epi16(in[4], in[5]);
        const __m128i tr0_5 = _mm_unpacklo_epi16(in[6], in[7]);
        const __m128i tr0_6 = _mm_unpackhi_epi16(in[4], in[5]);
        const __m128i tr0_7 = _mm_unpackhi_epi16(in[6], in[7]);

        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
        const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_4, tr0_5);
        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
        const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_4, tr0_5);
        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_2, tr0_3);
        const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_2, tr0_3);
        const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);

        res[7] = _mm_unpacklo_epi64(tr1_0, tr1_1);
        res[6] = _mm_unpackhi_epi64(tr1_0, tr1_1);
        res[5] = _mm_unpacklo_epi64(tr1_2, tr1_3);
        res[4] = _mm_unpackhi_epi64(tr1_2, tr1_3);
        res[3] = _mm_unpacklo_epi64(tr1_4, tr1_5);
        res[2] = _mm_unpackhi_epi64(tr1_4, tr1_5);
        res[1] = _mm_unpacklo_epi64(tr1_6, tr1_7);
        res[0] = _mm_unpackhi_epi64(tr1_6, tr1_7);
    }

    int cdef_find_dir_avx2(const uint16_t *img, int stride, int *var, int coeff_shift)
    {
        int i;
        int cost[8];
        int best_cost = 0;
        int best_dir = 0;
        v128 lines[8];
        for (i = 0; i < 8; i++) {
            lines[i] = v128_load_unaligned(&img[i * stride]);
            lines[i] =
                v128_sub_16(v128_shr_s16(lines[i], coeff_shift), v128_dup_16(128));
        }

        /* Compute "mostly vertical" directions. */
        __m128i dir47 = compute_directions(lines, cost + 4);

        array_reverse_transpose_8x8(lines, lines);

        /* Compute "mostly horizontal" directions. */
        __m128i dir03 = compute_directions(lines, cost);

        __m128i max = _mm_max_epi32(dir03, dir47);
        max = _mm_max_epi32(max, _mm_shuffle_epi32(max, _MM_SHUFFLE(1, 0, 3, 2)));
        max = _mm_max_epi32(max, _mm_shuffle_epi32(max, _MM_SHUFFLE(2, 3, 0, 1)));
        best_cost = _mm_cvtsi128_si32(max);
        __m128i t = _mm_packs_epi32(_mm_cmpeq_epi32(max, dir03), _mm_cmpeq_epi32(max, dir47));
        best_dir = _mm_movemask_epi8(_mm_packs_epi16(t, t));
        best_dir = get_msb(best_dir ^ (best_dir - 1));  // Count trailing zeros

        /* Difference between the optimal variance and the variance along the
        orthogonal direction. Again, the sum(x^2) terms cancel out. */
        *var = best_cost - cost[(best_dir + 4) & 7];
        /* We'd normally divide by 840, but dividing by 1024 is close enough
        for what we're going to do with this. */
        *var >>= 10;
        return best_dir;
    }

    // sign(a - b) * min(abs(a - b), max(0, strength - (abs(a - b) >> adjdamp)))
    SIMD_INLINE __m256i constrain(__m256i a, __m256i b, __m256i strength, __m128i adjdamp)
    {
        __m256i diff = _mm256_subs_epi16(a, b);
        const __m256i abs_diff = _mm256_abs_epi16(diff);
        const __m256i s = _mm256_subs_epu16(strength, _mm256_srl_epi16(abs_diff, adjdamp));
        return _mm256_sign_epi16(_mm256_min_epi16(abs_diff, s), diff);
    }

    SIMD_INLINE __m256i constrain(__m256i a, __m256i b, int strength, int adjdamp)
    {
        __m256i diff = _mm256_subs_epi16(a, b);
        const __m256i abs_diff = _mm256_abs_epi16(diff);
        const __m256i s = _mm256_subs_epu16(_mm256_set1_epi16(strength), _mm256_srl_epi16(abs_diff, _mm_cvtsi32_si128(adjdamp)));
        return _mm256_sign_epi16(_mm256_min_epi16(abs_diff, s), diff);
    }

    __m256i mask_large(__m256i a) {
        return _mm256_andnot_si256(_mm256_cmpeq_epi16(a, _mm256_set1_epi16(CDEF_VERY_LARGE)), a);
    }

    template <typename TDst>
    void cdef_filter_block_4x4_avx2(TDst *dst, int dstride, const uint16_t *in, int pri_strength,
                                    int sec_strength, int dir, int pri_damping, int sec_damping)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pdirs = cdef_directions[dir];
        const int *s0dirs = cdef_directions[(dir + 2) & 7];
        const int *s1dirs = cdef_directions[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));
        if (sec_strength)
            sec_damping = std::max(0, sec_damping - get_msb(sec_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m256i sec_str = _mm256_set1_epi16(sec_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        const __m128i sec_dmp = _mm_cvtsi32_si128(sec_damping);

        __m256i min, max, sum;
        __m256i x = loadu4_epi64(in, in + CDEF_BSTRIDE, in + CDEF_BSTRIDE * 2, in + CDEF_BSTRIDE * 3);

        __m256i p0 = loadu4_epi64(in + pdirs[0],  in + CDEF_BSTRIDE + pdirs[0],  in + CDEF_BSTRIDE * 2 + pdirs[0],  in + CDEF_BSTRIDE * 3 + pdirs[0] );
        __m256i p1 = loadu4_epi64(in - pdirs[0],  in + CDEF_BSTRIDE - pdirs[0],  in + CDEF_BSTRIDE * 2 - pdirs[0],  in + CDEF_BSTRIDE * 3 - pdirs[0] );
        __m256i p2 = loadu4_epi64(in + pdirs[1],  in + CDEF_BSTRIDE + pdirs[1],  in + CDEF_BSTRIDE * 2 + pdirs[1],  in + CDEF_BSTRIDE * 3 + pdirs[1] );
        __m256i p3 = loadu4_epi64(in - pdirs[1],  in + CDEF_BSTRIDE - pdirs[1],  in + CDEF_BSTRIDE * 2 - pdirs[1],  in + CDEF_BSTRIDE * 3 - pdirs[1] );
        __m256i s0 = loadu4_epi64(in + s0dirs[0], in + CDEF_BSTRIDE + s0dirs[0], in + CDEF_BSTRIDE * 2 + s0dirs[0], in + CDEF_BSTRIDE * 3 + s0dirs[0]);
        __m256i s1 = loadu4_epi64(in - s0dirs[0], in + CDEF_BSTRIDE - s0dirs[0], in + CDEF_BSTRIDE * 2 - s0dirs[0], in + CDEF_BSTRIDE * 3 - s0dirs[0]);
        __m256i s2 = loadu4_epi64(in + s1dirs[0], in + CDEF_BSTRIDE + s1dirs[0], in + CDEF_BSTRIDE * 2 + s1dirs[0], in + CDEF_BSTRIDE * 3 + s1dirs[0]);
        __m256i s3 = loadu4_epi64(in - s1dirs[0], in + CDEF_BSTRIDE - s1dirs[0], in + CDEF_BSTRIDE * 2 - s1dirs[0], in + CDEF_BSTRIDE * 3 - s1dirs[0]);
        __m256i s4 = loadu4_epi64(in + s0dirs[1], in + CDEF_BSTRIDE + s0dirs[1], in + CDEF_BSTRIDE * 2 + s0dirs[1], in + CDEF_BSTRIDE * 3 + s0dirs[1]);
        __m256i s5 = loadu4_epi64(in - s0dirs[1], in + CDEF_BSTRIDE - s0dirs[1], in + CDEF_BSTRIDE * 2 - s0dirs[1], in + CDEF_BSTRIDE * 3 - s0dirs[1]);
        __m256i s6 = loadu4_epi64(in + s1dirs[1], in + CDEF_BSTRIDE + s1dirs[1], in + CDEF_BSTRIDE * 2 + s1dirs[1], in + CDEF_BSTRIDE * 3 + s1dirs[1]);
        __m256i s7 = loadu4_epi64(in - s1dirs[1], in + CDEF_BSTRIDE - s1dirs[1], in + CDEF_BSTRIDE * 2 - s1dirs[1], in + CDEF_BSTRIDE * 3 - s1dirs[1]);

        min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
        min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
        min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

        max = _mm256_max_epi16(x,   mask_large(p0));
        max = _mm256_max_epi16(max, mask_large(p1));
        max = _mm256_max_epi16(max, mask_large(p2));
        max = _mm256_max_epi16(max, mask_large(p3));
        max = _mm256_max_epi16(max, mask_large(s0));
        max = _mm256_max_epi16(max, mask_large(s1));
        max = _mm256_max_epi16(max, mask_large(s2));
        max = _mm256_max_epi16(max, mask_large(s3));
        max = _mm256_max_epi16(max, mask_large(s4));
        max = _mm256_max_epi16(max, mask_large(s5));
        max = _mm256_max_epi16(max, mask_large(s6));
        max = _mm256_max_epi16(max, mask_large(s7));

        p0 = constrain(p0, x, pri_str, pri_dmp);
        p1 = constrain(p1, x, pri_str, pri_dmp);
        p2 = constrain(p2, x, pri_str, pri_dmp);
        p3 = constrain(p3, x, pri_str, pri_dmp);
        s0 = constrain(s0, x, sec_str, sec_dmp);
        s1 = constrain(s1, x, sec_str, sec_dmp);
        s2 = constrain(s2, x, sec_str, sec_dmp);
        s3 = constrain(s3, x, sec_str, sec_dmp);
        s4 = constrain(s4, x, sec_str, sec_dmp);
        s5 = constrain(s5, x, sec_str, sec_dmp);
        s6 = constrain(s6, x, sec_str, sec_dmp);
        s7 = constrain(s7, x, sec_str, sec_dmp);

        p0 = _mm256_add_epi16(p0, p1);
        p2 = _mm256_add_epi16(p2, p3);
        s0 = _mm256_add_epi16(_mm256_add_epi16(s0, s1), _mm256_add_epi16(s2, s3));
        s4 = _mm256_add_epi16(_mm256_add_epi16(s4, s5), _mm256_add_epi16(s6, s7));

        sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
        sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));
        sum = _mm256_add_epi16(sum, _mm256_add_epi16(s0, s0));
        sum = _mm256_add_epi16(sum, s4);

        sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
        sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
        sum = _mm256_srai_epi16(sum, 4);
        x = _mm256_add_epi16(x, sum);
        x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

        if (sizeof(TDst) == 2) {
            storel_epi64(dst + 0 * dstride, si128_lo(x));
            storeh_epi64(dst + 1 * dstride, si128_lo(x));
            storel_epi64(dst + 2 * dstride, si128_hi(x));
            storeh_epi64(dst + 3 * dstride, si128_hi(x));
        } else {
            x = _mm256_packus_epi16(x, x);
            storel_si32(dst + 0 * dstride, si128_lo(x));
            storel_si32(dst + 1 * dstride, _mm_srli_si128(si128_lo(x), 4));
            storel_si32(dst + 2 * dstride, si128_hi(x));
            storel_si32(dst + 3 * dstride, _mm_srli_si128(si128_hi(x), 4));
        }
    }
    template void cdef_filter_block_4x4_avx2<uint8_t> (uint8_t*, int,const uint16_t*,int,int,int,int,int);
    template void cdef_filter_block_4x4_avx2<uint16_t>(uint16_t*,int,const uint16_t*,int,int,int,int,int);

    template <typename TDst>
    void cdef_filter_block_4x4_nv12_avx2(TDst *dst, int dstride, const uint16_t *in, int pri_strength,
                                         int sec_strength, int dir, int pri_damping, int sec_damping)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions_nv12[dir];
        const int *sec0_dirs = cdef_directions_nv12[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions_nv12[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));
        if (sec_strength)
            sec_damping = std::max(0, sec_damping - get_msb(sec_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m256i sec_str = _mm256_set1_epi16(sec_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        const __m128i sec_dmp = _mm_cvtsi32_si128(sec_damping);

        for (int y = 0; y < 4; y += 2) {
            __m256i min, max, sum;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            p0 = constrain(p0, x, pri_str, pri_dmp);
            p1 = constrain(p1, x, pri_str, pri_dmp);
            p2 = constrain(p2, x, pri_str, pri_dmp);
            p3 = constrain(p3, x, pri_str, pri_dmp);
            s0 = constrain(s0, x, sec_str, sec_dmp);
            s1 = constrain(s1, x, sec_str, sec_dmp);
            s2 = constrain(s2, x, sec_str, sec_dmp);
            s3 = constrain(s3, x, sec_str, sec_dmp);
            s4 = constrain(s4, x, sec_str, sec_dmp);
            s5 = constrain(s5, x, sec_str, sec_dmp);
            s6 = constrain(s6, x, sec_str, sec_dmp);
            s7 = constrain(s7, x, sec_str, sec_dmp);

            p0 = _mm256_add_epi16(p0, p1);
            p2 = _mm256_add_epi16(p2, p3);
            s0 = _mm256_add_epi16(_mm256_add_epi16(s0, s1), _mm256_add_epi16(s2, s3));
            s4 = _mm256_add_epi16(_mm256_add_epi16(s4, s5), _mm256_add_epi16(s6, s7));

            sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
            sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));
            sum = _mm256_add_epi16(sum, _mm256_add_epi16(s0, s0));
            sum = _mm256_add_epi16(sum, s4);

            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            x = _mm256_add_epi16(x, sum);
            x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

            if (sizeof(TDst) == 2) {
                storea2_m128i(dst, dst + dstride, x);
            } else {
                x = _mm256_packus_epi16(x, x);
                storel_epi64(dst + 0 * dstride, si128_lo(x));
                storel_epi64(dst + 1 * dstride, si128_hi(x));
            }

            in += CDEF_BSTRIDE * 2;
            dst += dstride * 2;
        }
    }
    template void cdef_filter_block_4x4_nv12_avx2<uint8_t> (uint8_t*, int,const uint16_t*,int,int,int,int,int);
    template void cdef_filter_block_4x4_nv12_avx2<uint16_t>(uint16_t*,int,const uint16_t*,int,int,int,int,int);

    template <typename TDst>
    void cdef_filter_block_8x8_avx2(TDst *dst, int dstride, const uint16_t *in, int pri_strength,
                                  int sec_strength, int dir, int pri_damping, int sec_damping)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions[dir];
        const int *sec0_dirs = cdef_directions[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));
        if (sec_strength)
            sec_damping = std::max(0, sec_damping - get_msb(sec_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m256i sec_str = _mm256_set1_epi16(sec_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        const __m128i sec_dmp = _mm_cvtsi32_si128(sec_damping);

        for (int y = 0; y < 8; y += 2) {
            __m256i min, max, sum;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            p0 = constrain(p0, x, pri_str, pri_dmp);
            p1 = constrain(p1, x, pri_str, pri_dmp);
            p2 = constrain(p2, x, pri_str, pri_dmp);
            p3 = constrain(p3, x, pri_str, pri_dmp);
            s0 = constrain(s0, x, sec_str, sec_dmp);
            s1 = constrain(s1, x, sec_str, sec_dmp);
            s2 = constrain(s2, x, sec_str, sec_dmp);
            s3 = constrain(s3, x, sec_str, sec_dmp);
            s4 = constrain(s4, x, sec_str, sec_dmp);
            s5 = constrain(s5, x, sec_str, sec_dmp);
            s6 = constrain(s6, x, sec_str, sec_dmp);
            s7 = constrain(s7, x, sec_str, sec_dmp);

            p0 = _mm256_add_epi16(p0, p1);
            p2 = _mm256_add_epi16(p2, p3);
            s0 = _mm256_add_epi16(_mm256_add_epi16(s0, s1), _mm256_add_epi16(s2, s3));
            s4 = _mm256_add_epi16(_mm256_add_epi16(s4, s5), _mm256_add_epi16(s6, s7));

            sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
            sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));
            sum = _mm256_add_epi16(sum, _mm256_add_epi16(s0, s0));
            sum = _mm256_add_epi16(sum, s4);

            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            x = _mm256_add_epi16(x, sum);
            x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

            if (sizeof(TDst) == 2) {
                storea2_m128i(dst, dst + dstride, x);
            } else {
                x = _mm256_packus_epi16(x, x);
                storel_epi64(dst + 0 * dstride, si128_lo(x));
                storel_epi64(dst + 1 * dstride, si128_hi(x));
            }

            in += CDEF_BSTRIDE * 2;
            dst += dstride * 2;
        }
    }
    template void cdef_filter_block_8x8_avx2<uint8_t> (uint8_t*, int,const uint16_t*,int,int,int,int,int);
    template void cdef_filter_block_8x8_avx2<uint16_t>(uint16_t*,int,const uint16_t*,int,int,int,int,int);

    void cdef_estimate_block_4x4_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                      int sec_strength, int dir, int pri_damping, int sec_damping, int *sse)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pdirs = cdef_directions[dir];
        const int *s0dirs = cdef_directions[(dir + 2) & 7];
        const int *s1dirs = cdef_directions[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));
        if (sec_strength)
            sec_damping = std::max(0, sec_damping - get_msb(sec_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m256i sec_str = _mm256_set1_epi16(sec_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        const __m128i sec_dmp = _mm_cvtsi32_si128(sec_damping);
        __m256i err = _mm256_setzero_si256();

        __m256i min, max, sum;
        __m256i x = loadu4_epi64(in, in + CDEF_BSTRIDE, in + CDEF_BSTRIDE * 2, in + CDEF_BSTRIDE * 3);

        __m256i p0 = loadu4_epi64(in + pdirs[0],  in + CDEF_BSTRIDE + pdirs[0],  in + CDEF_BSTRIDE * 2 + pdirs[0],  in + CDEF_BSTRIDE * 3 + pdirs[0] );
        __m256i p1 = loadu4_epi64(in - pdirs[0],  in + CDEF_BSTRIDE - pdirs[0],  in + CDEF_BSTRIDE * 2 - pdirs[0],  in + CDEF_BSTRIDE * 3 - pdirs[0] );
        __m256i p2 = loadu4_epi64(in + pdirs[1],  in + CDEF_BSTRIDE + pdirs[1],  in + CDEF_BSTRIDE * 2 + pdirs[1],  in + CDEF_BSTRIDE * 3 + pdirs[1] );
        __m256i p3 = loadu4_epi64(in - pdirs[1],  in + CDEF_BSTRIDE - pdirs[1],  in + CDEF_BSTRIDE * 2 - pdirs[1],  in + CDEF_BSTRIDE * 3 - pdirs[1] );
        __m256i s0 = loadu4_epi64(in + s0dirs[0], in + CDEF_BSTRIDE + s0dirs[0], in + CDEF_BSTRIDE * 2 + s0dirs[0], in + CDEF_BSTRIDE * 3 + s0dirs[0]);
        __m256i s1 = loadu4_epi64(in - s0dirs[0], in + CDEF_BSTRIDE - s0dirs[0], in + CDEF_BSTRIDE * 2 - s0dirs[0], in + CDEF_BSTRIDE * 3 - s0dirs[0]);
        __m256i s2 = loadu4_epi64(in + s1dirs[0], in + CDEF_BSTRIDE + s1dirs[0], in + CDEF_BSTRIDE * 2 + s1dirs[0], in + CDEF_BSTRIDE * 3 + s1dirs[0]);
        __m256i s3 = loadu4_epi64(in - s1dirs[0], in + CDEF_BSTRIDE - s1dirs[0], in + CDEF_BSTRIDE * 2 - s1dirs[0], in + CDEF_BSTRIDE * 3 - s1dirs[0]);
        __m256i s4 = loadu4_epi64(in + s0dirs[1], in + CDEF_BSTRIDE + s0dirs[1], in + CDEF_BSTRIDE * 2 + s0dirs[1], in + CDEF_BSTRIDE * 3 + s0dirs[1]);
        __m256i s5 = loadu4_epi64(in - s0dirs[1], in + CDEF_BSTRIDE - s0dirs[1], in + CDEF_BSTRIDE * 2 - s0dirs[1], in + CDEF_BSTRIDE * 3 - s0dirs[1]);
        __m256i s6 = loadu4_epi64(in + s1dirs[1], in + CDEF_BSTRIDE + s1dirs[1], in + CDEF_BSTRIDE * 2 + s1dirs[1], in + CDEF_BSTRIDE * 3 + s1dirs[1]);
        __m256i s7 = loadu4_epi64(in - s1dirs[1], in + CDEF_BSTRIDE - s1dirs[1], in + CDEF_BSTRIDE * 2 - s1dirs[1], in + CDEF_BSTRIDE * 3 - s1dirs[1]);

        min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
        min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
        min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

        max = _mm256_max_epi16(x,   mask_large(p0));
        max = _mm256_max_epi16(max, mask_large(p1));
        max = _mm256_max_epi16(max, mask_large(p2));
        max = _mm256_max_epi16(max, mask_large(p3));
        max = _mm256_max_epi16(max, mask_large(s0));
        max = _mm256_max_epi16(max, mask_large(s1));
        max = _mm256_max_epi16(max, mask_large(s2));
        max = _mm256_max_epi16(max, mask_large(s3));
        max = _mm256_max_epi16(max, mask_large(s4));
        max = _mm256_max_epi16(max, mask_large(s5));
        max = _mm256_max_epi16(max, mask_large(s6));
        max = _mm256_max_epi16(max, mask_large(s7));

        p0 = constrain(p0, x, pri_str, pri_dmp);
        p1 = constrain(p1, x, pri_str, pri_dmp);
        p2 = constrain(p2, x, pri_str, pri_dmp);
        p3 = constrain(p3, x, pri_str, pri_dmp);
        s0 = constrain(s0, x, sec_str, sec_dmp);
        s1 = constrain(s1, x, sec_str, sec_dmp);
        s2 = constrain(s2, x, sec_str, sec_dmp);
        s3 = constrain(s3, x, sec_str, sec_dmp);
        s4 = constrain(s4, x, sec_str, sec_dmp);
        s5 = constrain(s5, x, sec_str, sec_dmp);
        s6 = constrain(s6, x, sec_str, sec_dmp);
        s7 = constrain(s7, x, sec_str, sec_dmp);

        p0 = _mm256_add_epi16(p0, p1);
        p2 = _mm256_add_epi16(p2, p3);
        s0 = _mm256_add_epi16(_mm256_add_epi16(s0, s1), _mm256_add_epi16(s2, s3));
        s4 = _mm256_add_epi16(_mm256_add_epi16(s4, s5), _mm256_add_epi16(s6, s7));

        sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
        sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));
        sum = _mm256_add_epi16(sum, _mm256_add_epi16(s0, s0));
        sum = _mm256_add_epi16(sum, s4);

        sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
        sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
        sum = _mm256_srai_epi16(sum, 4);
        x = _mm256_add_epi16(x, sum);
        x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

        __m256i o = _mm256_cvtepu8_epi16(loadu4_epi32(org, org + ostride, org + 2 * ostride, org + 3 * ostride));
        o = _mm256_sub_epi16(o, x);
        err = _mm256_madd_epi16(o, o);

        __m128i a = _mm_add_epi32(si128_lo(err), si128_hi(err));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        *sse = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_4x4_nv12_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                           int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint8_t *dst, int dstride)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions_nv12[dir];
        const int *sec0_dirs = cdef_directions_nv12[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions_nv12[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));
        if (sec_strength)
            sec_damping = std::max(0, sec_damping - get_msb(sec_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m256i sec_str = _mm256_set1_epi16(sec_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        const __m128i sec_dmp = _mm_cvtsi32_si128(sec_damping);
        __m256i err = _mm256_setzero_si256();

        for (int y = 0; y < 4; y += 2) {
            __m256i min, max, sum;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            p0 = constrain(p0, x, pri_str, pri_dmp);
            p1 = constrain(p1, x, pri_str, pri_dmp);
            p2 = constrain(p2, x, pri_str, pri_dmp);
            p3 = constrain(p3, x, pri_str, pri_dmp);
            s0 = constrain(s0, x, sec_str, sec_dmp);
            s1 = constrain(s1, x, sec_str, sec_dmp);
            s2 = constrain(s2, x, sec_str, sec_dmp);
            s3 = constrain(s3, x, sec_str, sec_dmp);
            s4 = constrain(s4, x, sec_str, sec_dmp);
            s5 = constrain(s5, x, sec_str, sec_dmp);
            s6 = constrain(s6, x, sec_str, sec_dmp);
            s7 = constrain(s7, x, sec_str, sec_dmp);

            p0 = _mm256_add_epi16(p0, p1);
            p2 = _mm256_add_epi16(p2, p3);
            s0 = _mm256_add_epi16(_mm256_add_epi16(s0, s1), _mm256_add_epi16(s2, s3));
            s4 = _mm256_add_epi16(_mm256_add_epi16(s4, s5), _mm256_add_epi16(s6, s7));

            sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
            sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));
            sum = _mm256_add_epi16(sum, _mm256_add_epi16(s0, s0));
            sum = _mm256_add_epi16(sum, s4);

            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            x = _mm256_add_epi16(x, sum);
            x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));
            o = _mm256_sub_epi16(o, x);
            o = _mm256_madd_epi16(o, o);
            err = _mm256_add_epi32(err, o);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
            {
                x = _mm256_packus_epi16(x, x);
                storel_epi64(dst + 0 * dstride, si128_lo(x));
                storel_epi64(dst + 1 * dstride, si128_hi(x));
                dst += dstride * 2;
            }
        }
        __m128i a = _mm_add_epi32(si128_lo(err), si128_hi(err));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        *sse = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_4x4_nv12_sec0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint8_t *dst, int dstride)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions_nv12[dir];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        __m256i err = _mm256_setzero_si256();

        for (int y = 0; y < 4; y += 2) {
            __m256i min, max, sum;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));

            p0 = constrain(p0, x, pri_str, pri_dmp);
            p1 = constrain(p1, x, pri_str, pri_dmp);
            p2 = constrain(p2, x, pri_str, pri_dmp);
            p3 = constrain(p3, x, pri_str, pri_dmp);

            p0 = _mm256_add_epi16(p0, p1);
            p2 = _mm256_add_epi16(p2, p3);

            sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
            sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));

            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            x = _mm256_add_epi16(x, sum);
            x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));
            o = _mm256_sub_epi16(o, x);
            o = _mm256_madd_epi16(o, o);
            err = _mm256_add_epi32(err, o);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;

            // store
            {
                x = _mm256_packus_epi16(x, x);
                storel_epi64(dst + 0 * dstride, si128_lo(x));
                storel_epi64(dst + 1 * dstride, si128_hi(x));
                dst += dstride * 2;
            }
        }
        __m128i a = _mm_add_epi32(si128_lo(err), si128_hi(err));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        *sse = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_8x8_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                      int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint8_t *dst, int dstride)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions[dir];
        const int *sec0_dirs = cdef_directions[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));
        if (sec_strength)
            sec_damping = std::max(0, sec_damping - get_msb(sec_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m256i sec_str = _mm256_set1_epi16(sec_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        const __m128i sec_dmp = _mm_cvtsi32_si128(sec_damping);
        __m256i err = _mm256_setzero_si256();

        for (int y = 0; y < 8; y += 2) {
            __m256i min, max, sum;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            p0 = constrain(p0, x, pri_str, pri_dmp);
            p1 = constrain(p1, x, pri_str, pri_dmp);
            p2 = constrain(p2, x, pri_str, pri_dmp);
            p3 = constrain(p3, x, pri_str, pri_dmp);
            s0 = constrain(s0, x, sec_str, sec_dmp);
            s1 = constrain(s1, x, sec_str, sec_dmp);
            s2 = constrain(s2, x, sec_str, sec_dmp);
            s3 = constrain(s3, x, sec_str, sec_dmp);
            s4 = constrain(s4, x, sec_str, sec_dmp);
            s5 = constrain(s5, x, sec_str, sec_dmp);
            s6 = constrain(s6, x, sec_str, sec_dmp);
            s7 = constrain(s7, x, sec_str, sec_dmp);

            p0 = _mm256_add_epi16(p0, p1);
            p2 = _mm256_add_epi16(p2, p3);
            s0 = _mm256_add_epi16(_mm256_add_epi16(s0, s1), _mm256_add_epi16(s2, s3));
            s4 = _mm256_add_epi16(_mm256_add_epi16(s4, s5), _mm256_add_epi16(s6, s7));

            sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
            sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));
            sum = _mm256_add_epi16(sum, _mm256_add_epi16(s0, s0));
            sum = _mm256_add_epi16(sum, s4);

            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            x = _mm256_add_epi16(x, sum);
            x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));
            o = _mm256_sub_epi16(o, x);
            o = _mm256_madd_epi16(o, o);
            err = _mm256_add_epi32(err, o);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
            {
                x = _mm256_packus_epi16(x, x);
                storel_epi64(dst + 0 * dstride, si128_lo(x));
                storel_epi64(dst + 1 * dstride, si128_hi(x));
                dst += dstride * 2;
            }
        }
        __m128i a = _mm_add_epi32(si128_lo(err), si128_hi(err));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        *sse = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_8x8_sec0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint8_t *dst, int dstride)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions[dir];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));

        const __m256i pri_str = _mm256_set1_epi16(pri_strength);
        const __m128i pri_dmp = _mm_cvtsi32_si128(pri_damping);
        __m256i err = _mm256_setzero_si256();

        for (int y = 0; y < 8; y += 2) {
            __m256i min, max, sum;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));

            p0 = constrain(p0, x, pri_str, pri_dmp);
            p1 = constrain(p1, x, pri_str, pri_dmp);
            p2 = constrain(p2, x, pri_str, pri_dmp);
            p3 = constrain(p3, x, pri_str, pri_dmp);

            p0 = _mm256_add_epi16(p0, p1);
            p2 = _mm256_add_epi16(p2, p3);

            sum = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), p0);
            sum = _mm256_add_epi16(sum, _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), p2));

            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            x = _mm256_add_epi16(x, sum);
            x = _mm256_min_epi16(_mm256_max_epi16(x, min), max);

            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));
            o = _mm256_sub_epi16(o, x);
            o = _mm256_madd_epi16(o, o);
            err = _mm256_add_epi32(err, o);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;

            // store
            {
                x = _mm256_packus_epi16(x, x);
                storel_epi64(dst + 0 * dstride, si128_lo(x));
                storel_epi64(dst + 1 * dstride, si128_hi(x));
                dst += dstride * 2;
            }
        }
        __m128i a = _mm_add_epi32(si128_lo(err), si128_hi(err));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        *sse = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_4x4_pri0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse)
    {
        const int pri_strength = 0;
        const int pri_damping = 3; // not used
        const int dir = 0; // not used
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength, 1, dir, pri_damping, sec_damping, sse + 0);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength, 2, dir, pri_damping, sec_damping, sse + 1);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength, 4, dir, pri_damping, sec_damping, sse + 2);
    }

    void cdef_estimate_block_4x4_nv12_pri0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse)
    {
        const int *pri_dirs = cdef_directions_nv12[0];
        const int *sec0_dirs = cdef_directions_nv12[2];
        const int *sec1_dirs = cdef_directions_nv12[6];

        __m256i err1 = _mm256_setzero_si256();
        __m256i err2 = _mm256_setzero_si256();
        __m256i err4 = _mm256_setzero_si256();

        for (int y = 0; y < 4; y += 2) {
            __m256i min, max, sum, filtx, diff;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);
            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            // filter for sec_strength = 1
            __m256i cs0 = constrain(s0, x, 1, sec_damping);
            __m256i cs1 = constrain(s1, x, 1, sec_damping);
            __m256i cs2 = constrain(s2, x, 1, sec_damping);
            __m256i cs3 = constrain(s3, x, 1, sec_damping);
            __m256i cs4 = constrain(s4, x, 1, sec_damping);
            __m256i cs5 = constrain(s5, x, 1, sec_damping);
            __m256i cs6 = constrain(s6, x, 1, sec_damping);
            __m256i cs7 = constrain(s7, x, 1, sec_damping);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(cs0, cs0);
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err1 = _mm256_add_epi32(err1, diff);

            // filter for sec_strength = 2
            cs0 = constrain(s0, x, 2, sec_damping - 1);
            cs1 = constrain(s1, x, 2, sec_damping - 1);
            cs2 = constrain(s2, x, 2, sec_damping - 1);
            cs3 = constrain(s3, x, 2, sec_damping - 1);
            cs4 = constrain(s4, x, 2, sec_damping - 1);
            cs5 = constrain(s5, x, 2, sec_damping - 1);
            cs6 = constrain(s6, x, 2, sec_damping - 1);
            cs7 = constrain(s7, x, 2, sec_damping - 1);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(cs0, cs0);
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err2 = _mm256_add_epi32(err2, diff);

            // filter for sec_strength = 4
            cs0 = constrain(s0, x, 4, sec_damping - 2);
            cs1 = constrain(s1, x, 4, sec_damping - 2);
            cs2 = constrain(s2, x, 4, sec_damping - 2);
            cs3 = constrain(s3, x, 4, sec_damping - 2);
            cs4 = constrain(s4, x, 4, sec_damping - 2);
            cs5 = constrain(s5, x, 4, sec_damping - 2);
            cs6 = constrain(s6, x, 4, sec_damping - 2);
            cs7 = constrain(s7, x, 4, sec_damping - 2);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(cs0, cs0);
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err4 = _mm256_add_epi32(err4, diff);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
        }

        __m128i a;
        a = _mm_add_epi32(si128_lo(err1), si128_hi(err1));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[0] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err2), si128_hi(err2));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[1] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err4), si128_hi(err4));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[2] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_8x8_pri0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse)
    {
        const int *pri_dirs = cdef_directions[0];
        const int *sec0_dirs = cdef_directions[2];
        const int *sec1_dirs = cdef_directions[6];

        __m256i err1 = _mm256_setzero_si256();
        __m256i err2 = _mm256_setzero_si256();
        __m256i err4 = _mm256_setzero_si256();

        for (int y = 0; y < 8; y += 2) {
            __m256i min, max, sum, filtx, diff;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);
            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            // filter for sec_strength = 1
            __m256i cs0 = constrain(s0, x, 1, sec_damping);
            __m256i cs1 = constrain(s1, x, 1, sec_damping);
            __m256i cs2 = constrain(s2, x, 1, sec_damping);
            __m256i cs3 = constrain(s3, x, 1, sec_damping);
            __m256i cs4 = constrain(s4, x, 1, sec_damping);
            __m256i cs5 = constrain(s5, x, 1, sec_damping);
            __m256i cs6 = constrain(s6, x, 1, sec_damping);
            __m256i cs7 = constrain(s7, x, 1, sec_damping);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(cs0, cs0);
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err1 = _mm256_add_epi32(err1, diff);

            // filter for sec_strength = 2
            cs0 = constrain(s0, x, 2, sec_damping - 1);
            cs1 = constrain(s1, x, 2, sec_damping - 1);
            cs2 = constrain(s2, x, 2, sec_damping - 1);
            cs3 = constrain(s3, x, 2, sec_damping - 1);
            cs4 = constrain(s4, x, 2, sec_damping - 1);
            cs5 = constrain(s5, x, 2, sec_damping - 1);
            cs6 = constrain(s6, x, 2, sec_damping - 1);
            cs7 = constrain(s7, x, 2, sec_damping - 1);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(cs0, cs0);
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err2 = _mm256_add_epi32(err2, diff);

            // filter for sec_strength = 4
            cs0 = constrain(s0, x, 4, sec_damping - 2);
            cs1 = constrain(s1, x, 4, sec_damping - 2);
            cs2 = constrain(s2, x, 4, sec_damping - 2);
            cs3 = constrain(s3, x, 4, sec_damping - 2);
            cs4 = constrain(s4, x, 4, sec_damping - 2);
            cs5 = constrain(s5, x, 4, sec_damping - 2);
            cs6 = constrain(s6, x, 4, sec_damping - 2);
            cs7 = constrain(s7, x, 4, sec_damping - 2);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(cs0, cs0);
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err4 = _mm256_add_epi32(err4, diff);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
        }

        __m128i a;
        a = _mm_add_epi32(si128_lo(err1), si128_hi(err1));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[0] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err2), si128_hi(err2));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[1] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err4), si128_hi(err4));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[2] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_4x4_all_sec_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                              int dir, int pri_damping, int sec_damping, int *sse)
    {
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength, 0, dir, pri_damping, sec_damping, sse + 0);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength, 1, dir, pri_damping, sec_damping, sse + 1);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength, 2, dir, pri_damping, sec_damping, sse + 2);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength, 4, dir, pri_damping, sec_damping, sse + 3);
    }

    void cdef_estimate_block_4x4_nv12_all_sec_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                                   int dir, int pri_damping, int sec_damping, int *sse, uint8_t** dst, int dstride)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions_nv12[dir];
        const int *sec0_dirs = cdef_directions_nv12[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions_nv12[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));

        __m256i err0 = _mm256_setzero_si256();
        __m256i err1 = _mm256_setzero_si256();
        __m256i err2 = _mm256_setzero_si256();
        __m256i err4 = _mm256_setzero_si256();

        for (int y = 0; y < 4; y += 2) {
            __m256i min, max, sum, pri_sum, filtx, diff;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);
            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            // filter for sec_strength = 0
            __m256i cp0 = constrain(p0, x, pri_strength, pri_damping);
            __m256i cp1 = constrain(p1, x, pri_strength, pri_damping);
            __m256i cp2 = constrain(p2, x, pri_strength, pri_damping);
            __m256i cp3 = constrain(p3, x, pri_strength, pri_damping);
            cp0 = _mm256_add_epi16(cp0, cp1);
            cp2 = _mm256_add_epi16(cp2, cp3);
            cp0 = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), cp0);
            cp2 = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), cp2);
            pri_sum = _mm256_add_epi16(cp0, cp2);
            sum = _mm256_add_epi16(pri_sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), pri_sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err0 = _mm256_add_epi32(err0, diff);

            {
                filtx = _mm256_packus_epi16(filtx, filtx);
                storel_epi64(dst[0] + 0 * dstride, si128_lo(filtx));
                storel_epi64(dst[0] + 1 * dstride, si128_hi(filtx));
                dst[0] += dstride * 2;
            }
            // filter for sec_strength = 1
            __m256i cs0 = constrain(s0, x, 1, sec_damping);
            __m256i cs1 = constrain(s1, x, 1, sec_damping);
            __m256i cs2 = constrain(s2, x, 1, sec_damping);
            __m256i cs3 = constrain(s3, x, 1, sec_damping);
            __m256i cs4 = constrain(s4, x, 1, sec_damping);
            __m256i cs5 = constrain(s5, x, 1, sec_damping);
            __m256i cs6 = constrain(s6, x, 1, sec_damping);
            __m256i cs7 = constrain(s7, x, 1, sec_damping);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(pri_sum, _mm256_add_epi16(cs0, cs0));
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err1 = _mm256_add_epi32(err1, diff);

            {
                __m256i x = _mm256_packus_epi16(filtx, filtx);
                storel_epi64(dst[1] + 0 * dstride, si128_lo(x));
                storel_epi64(dst[1] + 1 * dstride, si128_hi(x));
                dst[1] += dstride * 2;
            }
            // filter for sec_strength = 2
            if (dst[2]) {
            cs0 = constrain(s0, x, 2, sec_damping - 1);
            cs1 = constrain(s1, x, 2, sec_damping - 1);
            cs2 = constrain(s2, x, 2, sec_damping - 1);
            cs3 = constrain(s3, x, 2, sec_damping - 1);
            cs4 = constrain(s4, x, 2, sec_damping - 1);
            cs5 = constrain(s5, x, 2, sec_damping - 1);
            cs6 = constrain(s6, x, 2, sec_damping - 1);
            cs7 = constrain(s7, x, 2, sec_damping - 1);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(pri_sum, _mm256_add_epi16(cs0, cs0));
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err2 = _mm256_add_epi32(err2, diff);

                {
                    __m256i x = _mm256_packus_epi16(filtx, filtx);
                    storel_epi64(dst[2] + 0 * dstride, si128_lo(x));
                    storel_epi64(dst[2] + 1 * dstride, si128_hi(x));
                    dst[2] += dstride * 2;
                }
            }
            // filter for sec_strength = 4
            if (dst[3]) {
            cs0 = constrain(s0, x, 4, sec_damping - 2);
            cs1 = constrain(s1, x, 4, sec_damping - 2);
            cs2 = constrain(s2, x, 4, sec_damping - 2);
            cs3 = constrain(s3, x, 4, sec_damping - 2);
            cs4 = constrain(s4, x, 4, sec_damping - 2);
            cs5 = constrain(s5, x, 4, sec_damping - 2);
            cs6 = constrain(s6, x, 4, sec_damping - 2);
            cs7 = constrain(s7, x, 4, sec_damping - 2);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(pri_sum, _mm256_add_epi16(cs0, cs0));
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err4 = _mm256_add_epi32(err4, diff);

                {
                    __m256i x = _mm256_packus_epi16(filtx, filtx);
                    storel_epi64(dst[3] + 0 * dstride, si128_lo(x));
                    storel_epi64(dst[3] + 1 * dstride, si128_hi(x));
                    dst[3] += dstride * 2;
                }
            }
            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
        }

        __m128i a;
        a = _mm_add_epi32(si128_lo(err0), si128_hi(err0));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[0] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err1), si128_hi(err1));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[1] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        if (dst[2]) {
        a = _mm_add_epi32(si128_lo(err2), si128_hi(err2));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[2] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
        }

        if (dst[3]) {
        a = _mm_add_epi32(si128_lo(err4), si128_hi(err4));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[3] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
        }
    }

    void cdef_estimate_block_8x8_all_sec_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength,
                                              int dir, int pri_damping, int sec_damping, int *sse, uint8_t **dst, int dstride)
    {
        const int *pri_taps = cdef_pri_taps[pri_strength & 1];
        const int *pri_dirs = cdef_directions[dir];
        const int *sec0_dirs = cdef_directions[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions[(dir + 6) & 7];

        if (pri_strength)
            pri_damping = std::max(0, pri_damping - get_msb(pri_strength));

        __m256i err0 = _mm256_setzero_si256();
        __m256i err1 = _mm256_setzero_si256();
        __m256i err2 = _mm256_setzero_si256();
        __m256i err4 = _mm256_setzero_si256();

        for (int y = 0; y < 8; y += 2) {
            __m256i min, max, sum, pri_sum, filtx, diff;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);
            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            // filter for sec_strength = 0
            __m256i cp0 = constrain(p0, x, pri_strength, pri_damping);
            __m256i cp1 = constrain(p1, x, pri_strength, pri_damping);
            __m256i cp2 = constrain(p2, x, pri_strength, pri_damping);
            __m256i cp3 = constrain(p3, x, pri_strength, pri_damping);
            cp0 = _mm256_add_epi16(cp0, cp1);
            cp2 = _mm256_add_epi16(cp2, cp3);
            cp0 = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[0]), cp0);
            cp2 = _mm256_mullo_epi16(_mm256_set1_epi16(pri_taps[1]), cp2);
            pri_sum = _mm256_add_epi16(cp0, cp2);
            sum = _mm256_add_epi16(pri_sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), pri_sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err0 = _mm256_add_epi32(err0, diff);

            {
                __m256i x = _mm256_packus_epi16(filtx, filtx);
                storel_epi64(dst[0] + 0 * dstride, si128_lo(x));
                storel_epi64(dst[0] + 1 * dstride, si128_hi(x));
                dst[0] += dstride * 2;
            }
            // filter for sec_strength = 1
            __m256i cs0 = constrain(s0, x, 1, sec_damping);
            __m256i cs1 = constrain(s1, x, 1, sec_damping);
            __m256i cs2 = constrain(s2, x, 1, sec_damping);
            __m256i cs3 = constrain(s3, x, 1, sec_damping);
            __m256i cs4 = constrain(s4, x, 1, sec_damping);
            __m256i cs5 = constrain(s5, x, 1, sec_damping);
            __m256i cs6 = constrain(s6, x, 1, sec_damping);
            __m256i cs7 = constrain(s7, x, 1, sec_damping);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(pri_sum, _mm256_add_epi16(cs0, cs0));
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err1 = _mm256_add_epi32(err1, diff);

            {
                __m256i x = _mm256_packus_epi16(filtx, filtx);
                storel_epi64(dst[1] + 0 * dstride, si128_lo(x));
                storel_epi64(dst[1] + 1 * dstride, si128_hi(x));
                dst[1] += dstride * 2;
            }
            if (dst[2]) {
            // filter for sec_strength = 2
            cs0 = constrain(s0, x, 2, sec_damping - 1);
            cs1 = constrain(s1, x, 2, sec_damping - 1);
            cs2 = constrain(s2, x, 2, sec_damping - 1);
            cs3 = constrain(s3, x, 2, sec_damping - 1);
            cs4 = constrain(s4, x, 2, sec_damping - 1);
            cs5 = constrain(s5, x, 2, sec_damping - 1);
            cs6 = constrain(s6, x, 2, sec_damping - 1);
            cs7 = constrain(s7, x, 2, sec_damping - 1);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(pri_sum, _mm256_add_epi16(cs0, cs0));
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err2 = _mm256_add_epi32(err2, diff);

                {
                    __m256i x = _mm256_packus_epi16(filtx, filtx);
                    storel_epi64(dst[2] + 0 * dstride, si128_lo(x));
                    storel_epi64(dst[2] + 1 * dstride, si128_hi(x));
                    dst[2] += dstride * 2;
                }
            }
            if (dst[3]) {
            // filter for sec_strength = 4
            cs0 = constrain(s0, x, 4, sec_damping - 2);
            cs1 = constrain(s1, x, 4, sec_damping - 2);
            cs2 = constrain(s2, x, 4, sec_damping - 2);
            cs3 = constrain(s3, x, 4, sec_damping - 2);
            cs4 = constrain(s4, x, 4, sec_damping - 2);
            cs5 = constrain(s5, x, 4, sec_damping - 2);
            cs6 = constrain(s6, x, 4, sec_damping - 2);
            cs7 = constrain(s7, x, 4, sec_damping - 2);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sum = _mm256_add_epi16(pri_sum, _mm256_add_epi16(cs0, cs0));
            sum = _mm256_add_epi16(sum, cs4);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err4 = _mm256_add_epi32(err4, diff);

                {
                    __m256i x = _mm256_packus_epi16(filtx, filtx);
                    storel_epi64(dst[3] + 0 * dstride, si128_lo(x));
                    storel_epi64(dst[3] + 1 * dstride, si128_hi(x));
                    dst[3] += dstride * 2;
                }
            }
            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
        }

        __m128i a;
        a = _mm_add_epi32(si128_lo(err0), si128_hi(err0));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[0] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err1), si128_hi(err1));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[1] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        if (dst[2]) {
        a = _mm_add_epi32(si128_lo(err2), si128_hi(err2));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[2] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
        }

        if (dst[3]) {
        a = _mm_add_epi32(si128_lo(err4), si128_hi(err4));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[3] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
        }
    }

    void cdef_estimate_block_4x4_2pri_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1,
                                           int dir, int pri_damping, int sec_damping, int *sse)
    {
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength0, 0, dir, pri_damping, sec_damping, sse + 0);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength0, 1, dir, pri_damping, sec_damping, sse + 1);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength0, 2, dir, pri_damping, sec_damping, sse + 2);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength0, 4, dir, pri_damping, sec_damping, sse + 3);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength1, 0, dir, pri_damping, sec_damping, sse + 4);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength1, 1, dir, pri_damping, sec_damping, sse + 5);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength1, 2, dir, pri_damping, sec_damping, sse + 6);
        cdef_estimate_block_4x4_avx2(org, ostride, in, pri_strength1, 4, dir, pri_damping, sec_damping, sse + 7);
    }

    void cdef_estimate_block_4x4_nv12_2pri_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1,
                                                int dir, int pri_damping, int sec_damping, int *sse)
    {
        const int *pri0_taps = cdef_pri_taps[pri_strength0 & 1];
        const int *pri1_taps = cdef_pri_taps[pri_strength1 & 1];
        const int *pri_dirs = cdef_directions_nv12[dir];
        const int *sec0_dirs = cdef_directions_nv12[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions_nv12[(dir + 6) & 7];

        const int pri_damping0 = pri_strength0 ? std::max(0, pri_damping - get_msb(pri_strength0)) : pri_damping;
        const int pri_damping1 = pri_strength1 ? std::max(0, pri_damping - get_msb(pri_strength1)) : pri_damping;

        __m256i err00 = _mm256_setzero_si256();
        __m256i err01 = _mm256_setzero_si256();
        __m256i err02 = _mm256_setzero_si256();
        __m256i err04 = _mm256_setzero_si256();
        __m256i err10 = _mm256_setzero_si256();
        __m256i err11 = _mm256_setzero_si256();
        __m256i err12 = _mm256_setzero_si256();
        __m256i err14 = _mm256_setzero_si256();

        for (int y = 0; y < 4; y += 2) {
            __m256i min, max, sum, pri0_sum, pri1_sum, sec_sum, filtx, diff;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);
            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            // filter for pri_strength0 and sec_strength = 0
            __m256i cp0 = constrain(p0, x, pri_strength0, pri_damping0);
            __m256i cp1 = constrain(p1, x, pri_strength0, pri_damping0);
            __m256i cp2 = constrain(p2, x, pri_strength0, pri_damping0);
            __m256i cp3 = constrain(p3, x, pri_strength0, pri_damping0);
            cp0 = _mm256_add_epi16(cp0, cp1);
            cp2 = _mm256_add_epi16(cp2, cp3);
            cp0 = _mm256_mullo_epi16(_mm256_set1_epi16(pri0_taps[0]), cp0);
            cp2 = _mm256_mullo_epi16(_mm256_set1_epi16(pri0_taps[1]), cp2);
            pri0_sum = _mm256_add_epi16(cp0, cp2);
            sum = _mm256_add_epi16(pri0_sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), pri0_sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err00 = _mm256_add_epi32(err00, diff);

            // filter for pri_strength1 and sec_strength = 0
            cp0 = constrain(p0, x, pri_strength1, pri_damping1);
            cp1 = constrain(p1, x, pri_strength1, pri_damping1);
            cp2 = constrain(p2, x, pri_strength1, pri_damping1);
            cp3 = constrain(p3, x, pri_strength1, pri_damping1);
            cp0 = _mm256_add_epi16(cp0, cp1);
            cp2 = _mm256_add_epi16(cp2, cp3);
            cp0 = _mm256_mullo_epi16(_mm256_set1_epi16(pri1_taps[0]), cp0);
            cp2 = _mm256_mullo_epi16(_mm256_set1_epi16(pri1_taps[1]), cp2);
            pri1_sum = _mm256_add_epi16(cp0, cp2);
            sum = _mm256_add_epi16(pri1_sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), pri1_sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err10 = _mm256_add_epi32(err10, diff);

            // filter for sec_strength = 1
            __m256i cs0 = constrain(s0, x, 1, sec_damping);
            __m256i cs1 = constrain(s1, x, 1, sec_damping);
            __m256i cs2 = constrain(s2, x, 1, sec_damping);
            __m256i cs3 = constrain(s3, x, 1, sec_damping);
            __m256i cs4 = constrain(s4, x, 1, sec_damping);
            __m256i cs5 = constrain(s5, x, 1, sec_damping);
            __m256i cs6 = constrain(s6, x, 1, sec_damping);
            __m256i cs7 = constrain(s7, x, 1, sec_damping);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sec_sum = _mm256_add_epi16(cs0, cs0);
            sec_sum = _mm256_add_epi16(sec_sum, cs4);

            // for pri_strength0
            sum = _mm256_add_epi16(sec_sum, pri0_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err01 = _mm256_add_epi32(err01, diff);
            // for pri_strength1
            sum = _mm256_add_epi16(sec_sum, pri1_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err11 = _mm256_add_epi32(err11, diff);

            // filter for sec_strength = 2
            cs0 = constrain(s0, x, 2, sec_damping - 1);
            cs1 = constrain(s1, x, 2, sec_damping - 1);
            cs2 = constrain(s2, x, 2, sec_damping - 1);
            cs3 = constrain(s3, x, 2, sec_damping - 1);
            cs4 = constrain(s4, x, 2, sec_damping - 1);
            cs5 = constrain(s5, x, 2, sec_damping - 1);
            cs6 = constrain(s6, x, 2, sec_damping - 1);
            cs7 = constrain(s7, x, 2, sec_damping - 1);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sec_sum = _mm256_add_epi16(cs0, cs0);
            sec_sum = _mm256_add_epi16(sec_sum, cs4);

            // for pri_strength0
            sum = _mm256_add_epi16(sec_sum, pri0_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err02 = _mm256_add_epi32(err02, diff);
            // for pri_strength1
            sum = _mm256_add_epi16(sec_sum, pri1_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err12 = _mm256_add_epi32(err12, diff);

            // filter for sec_strength = 4
            cs0 = constrain(s0, x, 4, sec_damping - 2);
            cs1 = constrain(s1, x, 4, sec_damping - 2);
            cs2 = constrain(s2, x, 4, sec_damping - 2);
            cs3 = constrain(s3, x, 4, sec_damping - 2);
            cs4 = constrain(s4, x, 4, sec_damping - 2);
            cs5 = constrain(s5, x, 4, sec_damping - 2);
            cs6 = constrain(s6, x, 4, sec_damping - 2);
            cs7 = constrain(s7, x, 4, sec_damping - 2);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sec_sum = _mm256_add_epi16(cs0, cs0);
            sec_sum = _mm256_add_epi16(sec_sum, cs4);

            // for pri_strength0
            sum = _mm256_add_epi16(sec_sum, pri0_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err04 = _mm256_add_epi32(err04, diff);
            // for pri_strength1
            sum = _mm256_add_epi16(sec_sum, pri1_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err14 = _mm256_add_epi32(err14, diff);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
        }

        __m128i a;
        a = _mm_add_epi32(si128_lo(err00), si128_hi(err00));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[0] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err01), si128_hi(err01));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[1] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err02), si128_hi(err02));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[2] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err04), si128_hi(err04));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[3] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err10), si128_hi(err10));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[4] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err11), si128_hi(err11));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[5] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err12), si128_hi(err12));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[6] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err14), si128_hi(err14));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[7] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }

    void cdef_estimate_block_8x8_2pri_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1,
                                           int dir, int pri_damping, int sec_damping, int *sse)
    {
        const int *pri0_taps = cdef_pri_taps[pri_strength0 & 1];
        const int *pri1_taps = cdef_pri_taps[pri_strength1 & 1];
        const int *pri_dirs = cdef_directions[dir];
        const int *sec0_dirs = cdef_directions[(dir + 2) & 7];
        const int *sec1_dirs = cdef_directions[(dir + 6) & 7];

        const int pri_damping0 = pri_strength0 ? std::max(0, pri_damping - get_msb(pri_strength0)) : pri_damping;
        const int pri_damping1 = pri_strength1 ? std::max(0, pri_damping - get_msb(pri_strength1)) : pri_damping;

        __m256i err00 = _mm256_setzero_si256();
        __m256i err01 = _mm256_setzero_si256();
        __m256i err02 = _mm256_setzero_si256();
        __m256i err04 = _mm256_setzero_si256();
        __m256i err10 = _mm256_setzero_si256();
        __m256i err11 = _mm256_setzero_si256();
        __m256i err12 = _mm256_setzero_si256();
        __m256i err14 = _mm256_setzero_si256();

        for (int y = 0; y < 8; y += 2) {
            __m256i min, max, sum, pri0_sum, pri1_sum, sec_sum, filtx, diff;
            __m256i x = loada2_m128i(in, in + CDEF_BSTRIDE);
            __m256i o = _mm256_cvtepu8_epi16(loadu2_epi64(org, org + ostride));

            __m256i p0 = loadu2_m128i(in + pri_dirs[0], in + CDEF_BSTRIDE + pri_dirs[0]);
            __m256i p1 = loadu2_m128i(in - pri_dirs[0], in + CDEF_BSTRIDE - pri_dirs[0]);
            __m256i p2 = loadu2_m128i(in + pri_dirs[1], in + CDEF_BSTRIDE + pri_dirs[1]);
            __m256i p3 = loadu2_m128i(in - pri_dirs[1], in + CDEF_BSTRIDE - pri_dirs[1]);
            __m256i s0 = loadu2_m128i(in + sec0_dirs[0], in + CDEF_BSTRIDE + sec0_dirs[0]);
            __m256i s1 = loadu2_m128i(in - sec0_dirs[0], in + CDEF_BSTRIDE - sec0_dirs[0]);
            __m256i s2 = loadu2_m128i(in + sec1_dirs[0], in + CDEF_BSTRIDE + sec1_dirs[0]);
            __m256i s3 = loadu2_m128i(in - sec1_dirs[0], in + CDEF_BSTRIDE - sec1_dirs[0]);
            __m256i s4 = loadu2_m128i(in + sec0_dirs[1], in + CDEF_BSTRIDE + sec0_dirs[1]);
            __m256i s5 = loadu2_m128i(in - sec0_dirs[1], in + CDEF_BSTRIDE - sec0_dirs[1]);
            __m256i s6 = loadu2_m128i(in + sec1_dirs[1], in + CDEF_BSTRIDE + sec1_dirs[1]);
            __m256i s7 = loadu2_m128i(in - sec1_dirs[1], in + CDEF_BSTRIDE - sec1_dirs[1]);

            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(x,   p0), p1), p2), p3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s0), s1), s2), s3);
            min = _mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(_mm256_min_epi16(min, s4), s5), s6), s7);

            max = _mm256_max_epi16(x,   mask_large(p0));
            max = _mm256_max_epi16(max, mask_large(p1));
            max = _mm256_max_epi16(max, mask_large(p2));
            max = _mm256_max_epi16(max, mask_large(p3));
            max = _mm256_max_epi16(max, mask_large(s0));
            max = _mm256_max_epi16(max, mask_large(s1));
            max = _mm256_max_epi16(max, mask_large(s2));
            max = _mm256_max_epi16(max, mask_large(s3));
            max = _mm256_max_epi16(max, mask_large(s4));
            max = _mm256_max_epi16(max, mask_large(s5));
            max = _mm256_max_epi16(max, mask_large(s6));
            max = _mm256_max_epi16(max, mask_large(s7));

            // filter for pri_strength0 and sec_strength = 0
            __m256i cp0 = constrain(p0, x, pri_strength0, pri_damping0);
            __m256i cp1 = constrain(p1, x, pri_strength0, pri_damping0);
            __m256i cp2 = constrain(p2, x, pri_strength0, pri_damping0);
            __m256i cp3 = constrain(p3, x, pri_strength0, pri_damping0);
            cp0 = _mm256_add_epi16(cp0, cp1);
            cp2 = _mm256_add_epi16(cp2, cp3);
            cp0 = _mm256_mullo_epi16(_mm256_set1_epi16(pri0_taps[0]), cp0);
            cp2 = _mm256_mullo_epi16(_mm256_set1_epi16(pri0_taps[1]), cp2);
            pri0_sum = _mm256_add_epi16(cp0, cp2);
            sum = _mm256_add_epi16(pri0_sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), pri0_sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err00 = _mm256_add_epi32(err00, diff);

            // filter for pri_strength1 and sec_strength = 0
            cp0 = constrain(p0, x, pri_strength1, pri_damping1);
            cp1 = constrain(p1, x, pri_strength1, pri_damping1);
            cp2 = constrain(p2, x, pri_strength1, pri_damping1);
            cp3 = constrain(p3, x, pri_strength1, pri_damping1);
            cp0 = _mm256_add_epi16(cp0, cp1);
            cp2 = _mm256_add_epi16(cp2, cp3);
            cp0 = _mm256_mullo_epi16(_mm256_set1_epi16(pri1_taps[0]), cp0);
            cp2 = _mm256_mullo_epi16(_mm256_set1_epi16(pri1_taps[1]), cp2);
            pri1_sum = _mm256_add_epi16(cp0, cp2);
            sum = _mm256_add_epi16(pri1_sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), pri1_sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);

            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err10 = _mm256_add_epi32(err10, diff);

            // filter for sec_strength = 1
            __m256i cs0 = constrain(s0, x, 1, sec_damping);
            __m256i cs1 = constrain(s1, x, 1, sec_damping);
            __m256i cs2 = constrain(s2, x, 1, sec_damping);
            __m256i cs3 = constrain(s3, x, 1, sec_damping);
            __m256i cs4 = constrain(s4, x, 1, sec_damping);
            __m256i cs5 = constrain(s5, x, 1, sec_damping);
            __m256i cs6 = constrain(s6, x, 1, sec_damping);
            __m256i cs7 = constrain(s7, x, 1, sec_damping);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sec_sum = _mm256_add_epi16(cs0, cs0);
            sec_sum = _mm256_add_epi16(sec_sum, cs4);

            // for pri_strength0
            sum = _mm256_add_epi16(sec_sum, pri0_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err01 = _mm256_add_epi32(err01, diff);
            // for pri_strength1
            sum = _mm256_add_epi16(sec_sum, pri1_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err11 = _mm256_add_epi32(err11, diff);

            // filter for sec_strength = 2
            cs0 = constrain(s0, x, 2, sec_damping - 1);
            cs1 = constrain(s1, x, 2, sec_damping - 1);
            cs2 = constrain(s2, x, 2, sec_damping - 1);
            cs3 = constrain(s3, x, 2, sec_damping - 1);
            cs4 = constrain(s4, x, 2, sec_damping - 1);
            cs5 = constrain(s5, x, 2, sec_damping - 1);
            cs6 = constrain(s6, x, 2, sec_damping - 1);
            cs7 = constrain(s7, x, 2, sec_damping - 1);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sec_sum = _mm256_add_epi16(cs0, cs0);
            sec_sum = _mm256_add_epi16(sec_sum, cs4);

            // for pri_strength0
            sum = _mm256_add_epi16(sec_sum, pri0_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err02 = _mm256_add_epi32(err02, diff);
            // for pri_strength1
            sum = _mm256_add_epi16(sec_sum, pri1_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err12 = _mm256_add_epi32(err12, diff);

            // filter for sec_strength = 4
            cs0 = constrain(s0, x, 4, sec_damping - 2);
            cs1 = constrain(s1, x, 4, sec_damping - 2);
            cs2 = constrain(s2, x, 4, sec_damping - 2);
            cs3 = constrain(s3, x, 4, sec_damping - 2);
            cs4 = constrain(s4, x, 4, sec_damping - 2);
            cs5 = constrain(s5, x, 4, sec_damping - 2);
            cs6 = constrain(s6, x, 4, sec_damping - 2);
            cs7 = constrain(s7, x, 4, sec_damping - 2);

            cs0 = _mm256_add_epi16(_mm256_add_epi16(cs0, cs1), _mm256_add_epi16(cs2, cs3));
            cs4 = _mm256_add_epi16(_mm256_add_epi16(cs4, cs5), _mm256_add_epi16(cs6, cs7));
            sec_sum = _mm256_add_epi16(cs0, cs0);
            sec_sum = _mm256_add_epi16(sec_sum, cs4);

            // for pri_strength0
            sum = _mm256_add_epi16(sec_sum, pri0_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err04 = _mm256_add_epi32(err04, diff);
            // for pri_strength1
            sum = _mm256_add_epi16(sec_sum, pri1_sum);
            sum = _mm256_add_epi16(sum, _mm256_cmpgt_epi16(_mm256_setzero_si256(), sum));
            sum = _mm256_add_epi16(sum, _mm256_set1_epi16(8));
            sum = _mm256_srai_epi16(sum, 4);
            filtx = _mm256_add_epi16(x, sum);
            filtx = _mm256_min_epi16(_mm256_max_epi16(filtx, min), max);
            diff = _mm256_sub_epi16(o, filtx);
            diff = _mm256_madd_epi16(diff, diff);
            err14 = _mm256_add_epi32(err14, diff);

            in += CDEF_BSTRIDE * 2;
            org += ostride * 2;
        }

        __m128i a;
        a = _mm_add_epi32(si128_lo(err00), si128_hi(err00));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[0] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err01), si128_hi(err01));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[1] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err02), si128_hi(err02));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[2] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err04), si128_hi(err04));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[3] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err10), si128_hi(err10));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[4] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err11), si128_hi(err11));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[5] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err12), si128_hi(err12));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[6] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);

        a = _mm_add_epi32(si128_lo(err14), si128_hi(err14));
        a = _mm_add_epi32(a, _mm_srli_si128(a, 8));
        sse[7] = _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
    }
};
