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



#include "stdio.h"
#include "assert.h"
#include "immintrin.h"
#include "string.h"

#include "mfx_av1_opts_intrin.h"
#include "mfx_av1_intrapred_common.h"
#include "mfx_av1_get_intra_pred_pels.h"

namespace AV1PP
{
    namespace {
        inline AV1_FORCEINLINE __m128i abs_diff(__m128i a, __m128i b) {
            return _mm_sub_epi8(_mm_max_epu8(a,b), _mm_min_epu8(a,b));
        }
        inline AV1_FORCEINLINE __m256i abs_diff(__m256i a, __m256i b) {
            return _mm256_sub_epi8(_mm256_max_epu8(a,b), _mm256_min_epu8(a,b));
        }

        inline AV1_FORCEINLINE __m256i filter_row_paeth(__m256i above_row, __m256i left_pel, __m256i above_left_pel, __m256i diff_left_line, __m256i diff_above_pel) {
            // calculate diff_aboveleft = |above + left - 2 * aboveleft| = |avg(above, left) - aboveleft| + |avg(above, left) - correction - aboveleft|
            __m256i correction = _mm256_xor_si256(above_row, left_pel);
            correction = _mm256_and_si256(correction, _mm256_set1_epi8(1));
            __m256i avg0 = _mm256_avg_epu8(above_row, left_pel);
            __m256i avg1 = _mm256_sub_epi8(avg0, correction);
            avg0 = abs_diff(avg0, above_left_pel);
            avg1 = abs_diff(avg1, above_left_pel);

            // add with saturation to stay within 8 bits
            // we don't care if diff_aboveleft > 255
            // beacause diff_above and diff_left are always <= 255
            __m256i diff_aboveleft = _mm256_adds_epu8(avg0, avg1);

            // first choose between left and above
            __m256i min_diff = _mm256_min_epu8(diff_left_line, diff_above_pel);
            __m256i mask_pick_above = _mm256_cmpeq_epi8(diff_left_line, min_diff);
            __m256i row = _mm256_blendv_epi8(above_row, left_pel, mask_pick_above);

            // then choose between previous choice and aboveleft
            __m256i mask_pick_aboveleft = _mm256_cmpeq_epi8(min_diff, _mm256_min_epu8(min_diff, diff_aboveleft));
            return _mm256_blendv_epi8(above_left_pel, row, mask_pick_aboveleft);
        }

        ALIGN_DECL(32) const uint8_t shuftab_paeth_8[32] = { 0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3 };
    };

    template <int size, int haveLeft, int haveAbove, typename PixType> void predict_intra_dc_av1_avx2(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int, int, int);
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   0, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        topPels; leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3;
        int dc = 0x80808080;
        *(int*)(dst) = dc; *(int*)(dst+p) = dc; *(int*)(dst+p2) = dc; *(int*)(dst+p3) = dc;
    }
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   0, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        topPels; leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        uint64_t dc = 0x8080808080808080;
        *(uint64_t*)(dst) = dc; *(uint64_t*)(dst+p) = dc; *(uint64_t*)(dst+p2) = dc; *(uint64_t*)(dst+p3) = dc; dst += p4;
        *(uint64_t*)(dst) = dc; *(uint64_t*)(dst+p) = dc; *(uint64_t*)(dst+p2) = dc; *(uint64_t*)(dst+p3) = dc;
    }
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 0, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        topPels; leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m256i dc = _mm256_set1_epi8(128);
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc); dst += p4;
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc); dst += p4;
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc); dst += p4;
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 0, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        topPels; leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m256i dc = _mm256_set1_epi8(128);
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   0, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3;
        __m128i above = _mm_cvtsi32_si128(*(int*)(topPels));
        __m128i sad = _mm_sad_epu8(above, _mm_setzero_si128());
        __m128i sum = _mm_add_epi16(sad, _mm_set1_epi16(2));
        sum = _mm_srli_epi16(sum, 2);
        int dc = _mm_cvtsi128_si32(_mm_broadcastb_epi8(sum));
        *(int*)(dst) = dc; *(int*)(dst+p) = dc; *(int*)(dst+p2) = dc; *(int*)(dst+p3) = dc;
    }
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   0, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m128i above = loadl_epi64(topPels);
        __m128i sad = _mm_sad_epu8(above, _mm_setzero_si128());
        __m128i sum = _mm_add_epi16(sad, _mm_set1_epi16(4));
        sum = _mm_srli_epi16(sum, 3);
        __m128i dc = _mm_broadcastb_epi8(sum);
        storel_epi64(dst, dc); storel_epi64(dst+p, dc); storel_epi64(dst+p2, dc); storel_epi64(dst+p3, dc); dst += p4;
        storel_epi64(dst, dc); storel_epi64(dst+p, dc); storel_epi64(dst+p2, dc); storel_epi64(dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 0, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m128i above = loada_si128(topPels);
        __m128i sad = _mm_sad_epu8(above, _mm_setzero_si128());
        __m128i sum =  _mm_add_epi16(sad, si128(_mm_movehl_ps(ps(sad),ps(sad))));
        sum = _mm_add_epi16(sum, _mm_set1_epi16(8));
        sum = _mm_srli_epi16(sum, 4);
        __m128i dc = _mm_broadcastb_epi8(sum);
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc); dst += p4;
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc); dst += p4;
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc); dst += p4;
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 0, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m256i above = loada_si256(topPels);
        __m256i sad = _mm256_sad_epu8(above, _mm256_setzero_si256());
        __m256i sum =  _mm256_add_epi16(sad, si256(_mm256_extracti128_si256(sad,1)));
        sum = _mm256_add_epi16(sum, _mm256_bsrli_epi128(sum, 8));
        sum = _mm256_add_epi16(sum, _mm256_set1_epi16(16));
        sum = _mm256_srli_epi16(sum, 5);
        __m256i dc = _mm256_broadcastb_epi8(si128(sum));
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   1, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<0, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   1, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<1, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 1, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<2, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 1, 0, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<3, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   1, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        int p = pitch, p2 = pitch*2, p3 = pitch*3;
        __m128i above = _mm_cvtsi32_si128(*(int*)(topPels));
        __m128i left  = _mm_cvtsi32_si128(*(int*)(leftPels));
        __m128i pred = _mm_unpacklo_epi32(above, left);
        __m128i sad = _mm_sad_epu8(pred, _mm_setzero_si128());
        __m128i sum =  _mm_add_epi16(sad, si128(_mm_movehl_ps(ps(sad),ps(sad))));
        sum = _mm_add_epi16(sum, _mm_set1_epi16(4));
        sum = _mm_srli_epi16(sum, 3);
        int dc = _mm_cvtsi128_si32(_mm_broadcastb_epi8(sum));
        *(int*)(dst) = dc; *(int*)(dst+p) = dc; *(int*)(dst+p2) = dc; *(int*)(dst+p3) = dc;
    }
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   1, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m128i pred = loadh_epi64(loadl_epi64(topPels), leftPels);
        __m128i sad = _mm_sad_epu8(pred, _mm_setzero_si128());
        __m128i sum =  _mm_add_epi16(sad, si128(_mm_movehl_ps(ps(sad),ps(sad))));
        sum = _mm_add_epi16(sum, _mm_set1_epi16(8));
        sum = _mm_srli_epi16(sum, 4);
        __m128i dc = _mm_broadcastb_epi8(sum);
        storel_epi64(dst, dc); storel_epi64(dst+p, dc); storel_epi64(dst+p2, dc); storel_epi64(dst+p3, dc); dst += p4;
        storel_epi64(dst, dc); storel_epi64(dst+p, dc); storel_epi64(dst+p2, dc); storel_epi64(dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 1, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m256i pred = _mm256_inserti128_si256(si256(loada_si128(topPels)), loada_si128(leftPels), 1);
        __m256i sad = _mm256_sad_epu8(pred, _mm256_setzero_si256());
        __m256i sum = _mm256_add_epi16(sad, si256(_mm256_extracti128_si256(sad,1)));
        sum = _mm256_add_epi16(sum, _mm256_bsrli_epi128(sum, 8));
        sum = _mm256_add_epi16(sum, _mm256_set1_epi16(16));
        sum = _mm256_srli_epi16(sum, 5);
        __m128i dc = _mm_broadcastb_epi8(si128(sum));
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc); dst += p4;
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc); dst += p4;
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc); dst += p4;
        storea_si128(dst, dc); storea_si128(dst+p, dc); storea_si128(dst+p2, dc); storea_si128(dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 1, 1, uint8_t>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m256i left = loada_si256(leftPels);
        __m256i above = loada_si256(topPels);
        __m256i sad0 = _mm256_sad_epu8(left, _mm256_setzero_si256());
        __m256i sad1 = _mm256_sad_epu8(above, _mm256_setzero_si256());
        __m256i sum = _mm256_add_epi16(sad0, sad1);
        sum = _mm256_add_epi16(sum, si256(_mm256_extracti128_si256(sum,1)));
        sum = _mm256_add_epi16(sum, _mm256_bsrli_epi128(sum, 8));
        sum = _mm256_add_epi16(sum, _mm256_set1_epi16(32));
        sum = _mm256_srli_epi16(sum, 6);
        __m256i dc = _mm256_broadcastb_epi8(si128(sum));
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc); dst += p4;
        storea_si256(dst, dc); storea_si256(dst+p, dc); storea_si256(dst+p2, dc); storea_si256(dst+p3, dc);
    }

    namespace details {
        #define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n) - 1))) >> (n))
        uint8_t clip_pixel(int val) { return (val > 255) ? 255 : (val < 0) ? 0 : val; }

        template <int size> void predict_intra_ver_av1_avx2(const uint8_t *topPels, uint8_t *dst, int pitch);
        template <> void predict_intra_ver_av1_avx2<TX_4X4>(const uint8_t *topPels, uint8_t *dst, int pitch) {
            int p = *(int *)topPels;
            *(int *)dst = p; *(int *)(dst + pitch) = p; dst += pitch * 2;
            *(int *)dst = p; *(int *)(dst + pitch) = p;
        }
        template <> void predict_intra_ver_av1_avx2<TX_8X8>(const uint8_t *topPels, uint8_t *dst, int pitch) {
            uint64_t p = *(uint64_t *)topPels;
            *(uint64_t *)dst = p; *(uint64_t *)(dst + pitch) = p; dst += pitch * 2;
            *(uint64_t *)dst = p; *(uint64_t *)(dst + pitch) = p; dst += pitch * 2;
            *(uint64_t *)dst = p; *(uint64_t *)(dst + pitch) = p; dst += pitch * 2;
            *(uint64_t *)dst = p; *(uint64_t *)(dst + pitch) = p;
        }
        template <> void predict_intra_ver_av1_avx2<TX_16X16>(const uint8_t *topPels, uint8_t *dst, int pitch) {
            __m128i p = loada_si128(topPels);
            storea_si128(dst, p); storea_si128(dst + pitch, p); dst += 2*pitch;
            storea_si128(dst, p); storea_si128(dst + pitch, p); dst += 2*pitch;
            storea_si128(dst, p); storea_si128(dst + pitch, p); dst += 2*pitch;
            storea_si128(dst, p); storea_si128(dst + pitch, p); dst += 2*pitch;
            storea_si128(dst, p); storea_si128(dst + pitch, p); dst += 2*pitch;
            storea_si128(dst, p); storea_si128(dst + pitch, p); dst += 2*pitch;
            storea_si128(dst, p); storea_si128(dst + pitch, p); dst += 2*pitch;
            storea_si128(dst, p); storea_si128(dst + pitch, p);
        }
        template <> void predict_intra_ver_av1_avx2<TX_32X32>(const uint8_t *topPels, uint8_t *dst, int pitch) {
            __m256i p = loadu_si256(topPels);
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p); dst += 2*pitch;
            storea_si256(dst, p); storea_si256(dst+pitch, p);
        }

        template <int size> void predict_intra_hor_av1_avx2(const uint8_t *leftPels, uint8_t *dst, int pitch);
        template <> void predict_intra_hor_av1_avx2<TX_4X4>(const uint8_t *leftPels, uint8_t *dst, int pitch) {
            __m128i l = _mm_cvtsi32_si128(*(int *)leftPels);    // L0 L1 L2 L3 ** ** ** ** ** ** ** ** ** ** ** **
            l = _mm_unpacklo_epi8(l, l);                        // L0 L0 L1 L1 L2 L2 L3 L3 ** ** ** ** ** ** ** **
            l = _mm_unpacklo_epi8(l, l);                        // L0 L0 L0 L0 L1 L1 L1 L1 L2 L2 L2 L2 L3 L3 L3 L3
            int p = pitch;
            *(int *)(dst)         = _mm_cvtsi128_si32(l);
            *(int *)(dst + p)     = _mm_cvtsi128_si32(_mm_bsrli_si128(l,4));
            *(int *)(dst + p * 2) = _mm_cvtsi128_si32(_mm_bsrli_si128(l,8));
            *(int *)(dst + p * 3) = _mm_cvtsi128_si32(_mm_bsrli_si128(l,12));
        }
        template <> void predict_intra_hor_av1_avx2<TX_8X8>(const uint8_t *leftPels, uint8_t *dst, int pitch) {
            int p = pitch, p2 = pitch*2;
            __m128i l = loadl_epi64(leftPels);                          // L0 L1 L2 L3 L4 L5 L6 L7 ** ** ** ** ** ** ** **
            l = _mm_unpacklo_epi8(l, l);                                // L0 L0 L1 L1 L2 L2 L3 L3 L4 L4 L5 L5 L6 L6 L7 L7
            __m128i row0123 = _mm_unpacklo_epi16(l,l);                  // L0 L0 L0 L0 L1 L1 L1 L1 L2 L2 L2 L2 L3 L3 L3 L3
            __m128i row4567 = _mm_unpackhi_epi16(l,l);                  // L4 L4 L4 L4 L5 L5 L5 L5 L6 L6 L6 L6 L7 L7 L7 L7
            __m128i row01 = _mm_unpacklo_epi32(row0123,row0123);        // L0 L0 L0 L0 L0 L0 L0 L0 L1 L1 L1 L1 L1 L1 L1 L1
            __m128i row23 = _mm_unpackhi_epi32(row0123,row0123);        // L2 L2 L2 L2 L2 L2 L2 L2 L3 L3 L3 L3 L3 L3 L3 L3
            __m128i row45 = _mm_unpacklo_epi32(row4567,row4567);        // L4 L4 L4 L4 L4 L4 L4 L4 L5 L5 L5 L5 L5 L5 L5 L5
            __m128i row67 = _mm_unpackhi_epi32(row4567,row4567);        // L6 L6 L6 L6 L6 L6 L6 L6 L7 L7 L7 L7 L7 L7 L7 L7

            storel_epi64(dst, row01); storeh_epi64(dst+p, row01); dst += p2;
            storel_epi64(dst, row23); storeh_epi64(dst+p, row23); dst += p2;
            storel_epi64(dst, row45); storeh_epi64(dst+p, row45); dst += p2;
            storel_epi64(dst, row67); storeh_epi64(dst+p, row67); dst += p2;
        }
        template <> void predict_intra_hor_av1_avx2<TX_16X16>(const uint8_t *leftPels, uint8_t *dst, int pitch) {
            __m256i l = si256(loada_si128(leftPels));           // 0123456789abcdef ****************
            l = _mm256_permute4x64_epi64(l, PERM4x64(0,0,1,1)); // 0123456701234567 89abcdrf89abcdrf
            l = _mm256_unpacklo_epi8(l,l);                      // 0011223344556677 8899aabbccddeeff
            __m256i lo = _mm256_unpacklo_epi16(l,l);            // 0000111122223333 88889999aaaabbbb
            __m256i hi = _mm256_unpackhi_epi16(l,l);            // 4444555566667777 ccccddddeeeeffff
            __m256i lolo = _mm256_unpacklo_epi32(lo,lo);        // 0000000011111111 8888888899999999
            __m256i lohi = _mm256_unpackhi_epi32(lo,lo);        // 2222222233333333 aaaaaaaabbbbbbbb
            __m256i hilo = _mm256_unpacklo_epi32(hi,hi);        // 4444444455555555 ccccccccdddddddd
            __m256i hihi = _mm256_unpackhi_epi32(hi,hi);        // 6666666677777777 eeeeeeeeffffffff

            const int p = pitch;
            const int p8 = pitch * 8;
            storeu2_m128i(dst, dst + p8, _mm256_unpacklo_epi64(lolo, lolo)); dst += p;  // 0000000000000000 8888888888888888
            storeu2_m128i(dst, dst + p8, _mm256_unpackhi_epi64(lolo, lolo)); dst += p;  // 1111111111111111 9999999999999999
            storeu2_m128i(dst, dst + p8, _mm256_unpacklo_epi64(lohi, lohi)); dst += p;  // 2222222222222222 aaaaaaaaaaaaaaaa
            storeu2_m128i(dst, dst + p8, _mm256_unpackhi_epi64(lohi, lohi)); dst += p;  // 3333333333333333 bbbbbbbbbbbbbbbb
            storeu2_m128i(dst, dst + p8, _mm256_unpacklo_epi64(hilo, hilo)); dst += p;  // 4444444444444444 cccccccccccccccc
            storeu2_m128i(dst, dst + p8, _mm256_unpackhi_epi64(hilo, hilo)); dst += p;  // 5555555555555555 dddddddddddddddd
            storeu2_m128i(dst, dst + p8, _mm256_unpacklo_epi64(hihi, hihi)); dst += p;  // 6666666666666666 eeeeeeeeeeeeeeee
            storeu2_m128i(dst, dst + p8, _mm256_unpackhi_epi64(hihi, hihi));            // 7777777777777777 ffffffffffffffff
        }
        template <> void predict_intra_hor_av1_avx2<TX_32X32>(const uint8_t *leftPels, uint8_t *dst, int pitch) {
            __m256i l = loada_si256(leftPels);                      // 0123456789abcdef ghiklmnopqrstuvw
            __m256i lo = _mm256_unpacklo_epi8(l,l);                 // 0011223344556677 gghhiikkllmmnnoo
            __m256i hi = _mm256_unpackhi_epi8(l,l);                 // 8899aabbccddeeff ppqqrrssttuuvvww

            int p = pitch;
            int p2 = pitch*2;
            int p16 = pitch*16;
            for (int i = 0; i < 2; i++, lo=hi) {
                __m256i lolo = _mm256_unpacklo_epi16(lo,lo);        // 0000111122223333 gggghhhhiiiikkkk
                __m256i lohi = _mm256_unpackhi_epi16(lo,lo);        // 4444555566667777 llllmmmmnnnnoooo
                __m256i lololo = _mm256_unpacklo_epi16(lolo,lolo);  // 0000000011111111 gggggggghhhhhhhh
                __m256i lolohi = _mm256_unpackhi_epi16(lolo,lolo);  // 2222222233333333 iiiiiiiikkkkkkkk
                __m256i lohilo = _mm256_unpacklo_epi16(lohi,lohi);  // 4444444455555555 llllllllmmmmmmmm
                __m256i lohihi = _mm256_unpackhi_epi16(lohi,lohi);  // 6666666677777777 nnnnnnnnoooooooo

                storea_si256(dst,       _mm256_permute4x64_epi64(lololo, PERM4x64(0,0,0,0))); // 0,8
                storea_si256(dst+p,     _mm256_permute4x64_epi64(lololo, PERM4x64(1,1,1,1))); // 1,9
                storea_si256(dst+p16,   _mm256_permute4x64_epi64(lololo, PERM4x64(2,2,2,2))); // 16,24
                storea_si256(dst+p16+p, _mm256_permute4x64_epi64(lololo, PERM4x64(3,3,3,3))); // 17,25
                dst += p2;
                storea_si256(dst,       _mm256_permute4x64_epi64(lolohi, PERM4x64(0,0,0,0))); // 2,10
                storea_si256(dst+p,     _mm256_permute4x64_epi64(lolohi, PERM4x64(1,1,1,1))); // 3,11
                storea_si256(dst+p16,   _mm256_permute4x64_epi64(lolohi, PERM4x64(2,2,2,2))); // 18,26
                storea_si256(dst+p16+p, _mm256_permute4x64_epi64(lolohi, PERM4x64(3,3,3,3))); // 19,27
                dst += p2;
                storea_si256(dst,       _mm256_permute4x64_epi64(lohilo, PERM4x64(0,0,0,0))); // 4,12
                storea_si256(dst+p,     _mm256_permute4x64_epi64(lohilo, PERM4x64(1,1,1,1))); // 5,13
                storea_si256(dst+p16,   _mm256_permute4x64_epi64(lohilo, PERM4x64(2,2,2,2))); // 20,28
                storea_si256(dst+p16+p, _mm256_permute4x64_epi64(lohilo, PERM4x64(3,3,3,3))); // 21,29
                dst += p2;
                storea_si256(dst,       _mm256_permute4x64_epi64(lohihi, PERM4x64(0,0,0,0))); // 6,14
                storea_si256(dst+p,     _mm256_permute4x64_epi64(lohihi, PERM4x64(1,1,1,1))); // 7,15
                storea_si256(dst+p16,   _mm256_permute4x64_epi64(lohihi, PERM4x64(2,2,2,2))); // 22,30
                storea_si256(dst+p16+p, _mm256_permute4x64_epi64(lohihi, PERM4x64(3,3,3,3))); // 23,31
                dst += p2;
            }
        }

        template <int size> void predict_intra_paeth_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch);
        template <> void predict_intra_paeth_av1_avx2<TX_4X4  >(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;
            __m128i above_left_pel = _mm_set1_epi8(topPels[-1]);    // dddddddddddddddd
            __m128i above_row = _mm_set1_epi32(*(uint32_t *)pa);      // 0123012301230123
            __m128i left_col = _mm_cvtsi32_si128(*(uint32_t *)pl);    // 0123............
            left_col = _mm_unpacklo_epi8(left_col, left_col);       // 00112233........
            left_col = _mm_unpacklo_epi16(left_col, left_col);      // 0000111122223333

            __m128i diff_left = abs_diff(above_row, above_left_pel);    // diff_left  = |base - left | = |above + left - above_left - left | = |above - above_left|
            __m128i diff_above = abs_diff(left_col, above_left_pel);    // diff_above = |base - above| = |above + left - above_left - above| = |left  - above_left|

            // calculate diff_aboveleft = |above + left - 2 * aboveleft| = |avg(above, left) - aboveleft| + |avg(above, left) - correction - aboveleft|
            __m128i correction = _mm_xor_si128(above_row, left_col);
            correction = _mm_and_si128(correction, _mm_set1_epi8(1));
            __m128i avg0 = _mm_avg_epu8(above_row, left_col);
            __m128i avg1 = _mm_sub_epi8(avg0, correction);
            avg0 = abs_diff(avg0, above_left_pel);
            avg1 = abs_diff(avg1, above_left_pel);

            // add with saturation to stay within 8 bits
            // we don't care if diff_aboveleft > 255
            // beacause diff_above and diff_left are always <= 255
            __m128i diff_aboveleft = _mm_adds_epu8(avg0, avg1);

            // first choose between left and above
            __m128i min_diff = _mm_min_epu8(diff_left, diff_above);
            __m128i mask_pick_above = _mm_cmpeq_epi8(diff_left, min_diff);
            __m128i row = _mm_blendv_epi8(above_row, left_col, mask_pick_above);

            // then choose between previous choice and aboveleft
            __m128i mask_pick_aboveleft = _mm_cmpeq_epi8(min_diff, _mm_min_epu8(min_diff, diff_aboveleft));
            row = _mm_blendv_epi8(above_left_pel, row, mask_pick_aboveleft);

            storel_si32(dst + 0 * pitch, row);
            storel_si32(dst + 1 * pitch, _mm_srli_si128(row, 4));
            storel_si32(dst + 2 * pitch, _mm_srli_si128(row, 8));
            storel_si32(dst + 3 * pitch, _mm_srli_si128(row, 12));
        }
        template <> void predict_intra_paeth_av1_avx2<TX_8X8  >(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;

            __m128i a_ = loaddup_epi64(pa);                                 // 0123456701234567
            __m128i l_ = loaddup_epi64(pl);                                 // 0123456701234567
            __m256i above_row = _mm256_setr_m128i(a_, a_);                  // 0123456701234567 0123456701234567
            __m256i left_col = _mm256_setr_m128i(l_, l_);                   // 0123456701234567 0123456701234567
            __m256i above_left_pel = _mm256_set1_epi8(topPels[-1]);         // dddddddddddddddd dddddddddddddddd

            __m256i diff_left = abs_diff(above_row, above_left_pel);    // diff_left  = |base - left | = |above + left - above_left - left | = |above - above_left|
            __m256i diff_above = abs_diff(left_col, above_left_pel);    // diff_above = |base - above| = |above + left - above_left - above| = |left  - above_left|
            __m256i shuftab = loada_si256(shuftab_paeth_8);

            __m256i left_pel, diff_above_pel, row;

            left_pel = _mm256_shuffle_epi8(left_col, shuftab);                            // 0000000011111111 2222222233333333
            diff_above_pel = _mm256_shuffle_epi8(diff_above, shuftab);
            row = filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel);
            storel_epi64(dst + 0 * pitch, si128_lo(row));
            storeh_epi64(dst + 1 * pitch, si128_lo(row));
            storel_epi64(dst + 2 * pitch, si128_hi(row));
            storeh_epi64(dst + 3 * pitch, si128_hi(row));
            dst += pitch * 4;

            left_pel = _mm256_shuffle_epi8(_mm256_srli_si256(left_col, 4), shuftab);    // 4444444455555555 6666666677777777
            diff_above_pel = _mm256_shuffle_epi8(_mm256_srli_si256(diff_above, 4), shuftab);
            row = filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel);
            storel_epi64(dst + 0 * pitch, si128_lo(row));
            storeh_epi64(dst + 1 * pitch, si128_lo(row));
            storel_epi64(dst + 2 * pitch, si128_hi(row));
            storeh_epi64(dst + 3 * pitch, si128_hi(row));
        }
        template <> void predict_intra_paeth_av1_avx2<TX_16X16>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const __m128i a = loada_si128(topPels);
            const __m128i l = loada_si128(leftPels);
            const __m256i above_row = _mm256_broadcastsi128_si256(a);  // 0123456789abcdef 0123456789abcdef
            const __m256i above_left_pel = _mm256_set1_epi8(topPels[-1]);

            const __m256i diff_left = abs_diff(above_row, above_left_pel);
            __m256i left_col = _mm256_setr_m128i(l, _mm_srli_si128(l, 1));  // 0123456789abcdef 123456789abcdef.
            __m256i diff_above = abs_diff(left_col, above_left_pel);

            for (int i = 0; i < 16; i += 2) {
                const __m256i left_pel = _mm256_shuffle_epi8(left_col, _mm256_setzero_si256());
                const __m256i diff_above_pel = _mm256_shuffle_epi8(diff_above, _mm256_setzero_si256());
                const __m256i pred = filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel);
                left_col = _mm256_srli_si256(left_col, 2);
                diff_above = _mm256_srli_si256(diff_above, 2);
                storeu2_m128i(dst, dst + pitch, pred);
                dst += pitch * 2;
            }
        }
        template <> void predict_intra_paeth_av1_avx2<TX_32X32>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const __m256i above_row = loada_si256(topPels);
            const __m256i left_col = loada_si256(leftPels);
            const __m256i above_left_pel = _mm256_set1_epi8(topPels[-1]);
            const __m256i diff_left = abs_diff(above_row, above_left_pel);

            alignas(32) uint8_t diff_above_buf[32];
            storea_si256(diff_above_buf, abs_diff(left_col, above_left_pel));

            for (int i = 0; i < 32; i++, dst += pitch) {
                const __m256i left_pel = _mm256_set1_epi8(leftPels[i]);
                const __m256i diff_above_pel = _mm256_set1_epi8(diff_above_buf[i]);
                const __m256i pred = filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel);
                storea_si256(dst, pred);
            }
        }

        template <int size> void predict_intra_smooth_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch);
        template <> void predict_intra_smooth_av1_avx2<TX_4X4>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;

            const __m256i zero = _mm256_setzero_si256();
            const __m256i below = _mm256_set1_epi16(pl[3]);                 // 16w: B B B B B B B B | B B B B B B B B
            const __m256i right = _mm256_set1_epi16(pa[3]);                 // 16w: R R R R R R R R | R R R R R R R R
            __m128i above_ = _mm_set1_epi32(*(int*)pa);                     // 16b: A0 A1 A2 A3 A0 A1 A2 A3 A0 A1 A2 A3 A0 A1 A2 A3
            __m256i above = _mm256_cvtepu8_epi16(above_);                   // 16w: A0 A1 A2 A3 A0 A1 A2 A3 A0 A1 A2 A3 A0 A1 A2 A3
            __m128i left_ = _mm_set1_epi32(*(int*)pl);                      // 16b: L0 L1 L2 L3 L0 L1 L2 L3 L0 L1 L2 L3 L0 L1 L2 L3
            left_ = _mm_unpacklo_epi8(left_, left_);
            left_ = _mm_unpacklo_epi16(left_, left_);
            __m256i left = _mm256_cvtepu8_epi16(left_);                     // 16w: L0 L0 L0 L0 L1 L1 L1 L1 L2 L2 L2 L2 L3 L3 L3 L3
            __m128i weights_ = _mm_set1_epi32(*(int*)smooth_weights_4);     // 16b: W0 W1 W2 W3 W0 W1 W2 W3 W0 W1 W2 W3 W0 W1 W2 W3
            __m256i weights = _mm256_cvtepu8_epi16(weights_);               // 16w: W0 W1 W2 W3 W0 W1 W2 W3 W0 W1 W2 W3 W0 W1 W2 W3
            __m256i weights_inv = _mm256_sub_epi8(zero, weights);
            weights_ = _mm_unpacklo_epi8(weights_, weights_);               // 16b: W0 W0 W1 W1 W2 W2 W3 W3 W0 W0 W1 W1 W2 W2 W3 W3
            weights_ = _mm_unpacklo_epi8(weights_, weights_);               // 16b: W0 W0 W0 W0 W1 W1 W1 W1 W2 W2 W2 W2 W3 W3 W3 W3
            __m256i weights_dup = _mm256_cvtepu8_epi16(weights_);           // 16w: W0 W0 W0 W0 W1 W1 W1 W1 W2 W2 W2 W2 W3 W3 W3 W3
            __m256i weights_inv_dup = _mm256_sub_epi8(zero, weights_dup);

            __m256i a = _mm256_mullo_epi16(above, weights_dup);     // above[c] * sm_weights[r]
            __m256i b = _mm256_mullo_epi16(below, weights_inv_dup); // below_pred * (scale - sm_weights[r])
            __m256i c = _mm256_mullo_epi16(left, weights);          // left[r] * sm_weights[c]
            __m256i d = _mm256_mullo_epi16(right, weights_inv);     // right_pred * (scale - sm_weights[c])
            a = _mm256_add_epi16(a, b);
            b = _mm256_add_epi16(c, d);

            __m256i lsb = _mm256_and_si256(_mm256_and_si256(a, b), _mm256_set1_epi16(1));
            a = _mm256_srli_epi16(a, 1);
            b = _mm256_srli_epi16(b, 1);
            a = _mm256_adds_epu16(a, b);
            a = _mm256_adds_epu16(a, lsb);
            a = _mm256_adds_epu16(a, _mm256_set1_epi16(128));
            a = _mm256_srli_epi16(a, 8);                            // 32b: A0 00 A1 00 A2 00 A3 00 A4 00 A5 00 A6 00 A7 00 | A8 00 A9 00 Aa 00 Ab 00 Ac 00 Ad 00 Ae 00 Af 00
            a = _mm256_packus_epi16(a, a);                          // 32b: A0 A1 A2 A3 A4 A5 A6 A7 A0 A1 A2 A3 A4 A5 A6 A7 | A8 A9 Aa Ab Ac Ad Ae Af A8 A9 Aa Ab Ac Ad Ae Af

            __m128i lo = si128_lo(a);
            __m128i hi = si128_hi(a);

            storel_si32(dst + 0 * pitch, lo);
            storel_si32(dst + 1 * pitch, _mm_srli_si128(lo, 4));
            storel_si32(dst + 2 * pitch, hi);
            storel_si32(dst + 3 * pitch, _mm_srli_si128(hi, 4));
        }
        template <> void predict_intra_smooth_av1_avx2<TX_8X8>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            // above[c] * sm_weights[r]
            // below_pred * (scale - sm_weights[r])
            // left[r] * sm_weights[c]
            // right_pred * (scale - sm_weights[c])

            const int width = 8;

            const __m128i k0 = _mm_setzero_si128();
            const __m256i k1 = _mm256_set1_epi16(1);
            const __m256i k128 = _mm256_set1_epi16(128);
            const __m256i shuftab = _mm256_set1_epi16(0x100);

            const __m128i a = _mm_unpacklo_epi8(loadl_epi64(topPels), k0);
            const __m128i l = _mm_unpacklo_epi8(loadl_epi64(leftPels), k0);
            const __m128i w = _mm_unpacklo_epi8(loadl_epi64(smooth_weights_8), k0);
            const __m128i iw = _mm_subs_epi16(_mm_set1_epi16(256), w);
            const __m128i b = _mm_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[width - 1]));
            const __m128i r = _mm_broadcastw_epi16(_mm_cvtsi32_si128(topPels[width - 1]));
            const __m128i bw = _mm_mullo_epi16(b, iw);

            const __m256i a2 = _mm256_broadcastsi128_si256(a);
            const __m256i w2 = _mm256_broadcastsi128_si256(w);
            const __m256i rw2 = _mm256_broadcastsi128_si256(_mm_mullo_epi16(r, iw));
            __m256i w_shifted = _mm256_setr_m128i(w, _mm_srli_si128(w, 2));
            __m256i l_shifted = _mm256_setr_m128i(l, _mm_srli_si128(l, 2));
            __m256i bw_shifted = _mm256_setr_m128i(bw, _mm_srli_si128(bw, 2));

            __m256i w1, l1, bw1, res_part1, res_part2, row1, row2, lsb;

            for (int i = 0; i < width; i += 4, dst += pitch * 4) {
                w1 = _mm256_shuffle_epi8(w_shifted, shuftab);
                l1 = _mm256_shuffle_epi8(l_shifted, shuftab);
                bw1 = _mm256_shuffle_epi8(bw_shifted, shuftab);
                w_shifted = _mm256_srli_si256(w_shifted, 4);
                l_shifted = _mm256_srli_si256(l_shifted, 4);
                bw_shifted = _mm256_srli_si256(bw_shifted, 4);
                res_part1 = _mm256_mullo_epi16(a2, w1);
                res_part2 = _mm256_mullo_epi16(l1, w2);
                res_part1 = _mm256_add_epi16(res_part1, bw1);
                res_part2 = _mm256_add_epi16(res_part2, rw2);

                lsb = _mm256_and_si256(_mm256_and_si256(res_part1, res_part2), k1);
                res_part1 = _mm256_srli_epi16(res_part1, 1);
                res_part2 = _mm256_srli_epi16(res_part2, 1);
                res_part1 = _mm256_adds_epu16(res_part1, res_part2);
                res_part1 = _mm256_adds_epu16(res_part1, lsb);
                res_part1 = _mm256_adds_epu16(res_part1, k128);
                row1 = _mm256_srli_epi16(res_part1, 8);

                w1 = _mm256_shuffle_epi8(w_shifted, shuftab);
                l1 = _mm256_shuffle_epi8(l_shifted, shuftab);
                bw1 = _mm256_shuffle_epi8(bw_shifted, shuftab);
                w_shifted = _mm256_srli_si256(w_shifted, 4);
                l_shifted = _mm256_srli_si256(l_shifted, 4);
                bw_shifted = _mm256_srli_si256(bw_shifted, 4);
                res_part1 = _mm256_mullo_epi16(a2, w1);
                res_part2 = _mm256_mullo_epi16(l1, w2);
                res_part1 = _mm256_add_epi16(res_part1, bw1);
                res_part2 = _mm256_add_epi16(res_part2, rw2);

                lsb = _mm256_and_si256(_mm256_and_si256(res_part1, res_part2), k1);
                res_part1 = _mm256_srli_epi16(res_part1, 1);
                res_part2 = _mm256_srli_epi16(res_part2, 1);
                res_part1 = _mm256_adds_epu16(res_part1, res_part2);
                res_part1 = _mm256_adds_epu16(res_part1, lsb);
                res_part1 = _mm256_adds_epu16(res_part1, k128);
                row2 = _mm256_srli_epi16(res_part1, 8);

                row1 = _mm256_packus_epi16(row1, row2);
                storel_epi64(dst + 0 * pitch, si128_lo(row1));
                storel_epi64(dst + 1 * pitch, si128_hi(row1));
                storeh_epi64(dst + 2 * pitch, si128_lo(row1));
                storeh_epi64(dst + 3 * pitch, si128_hi(row1));
            }
        }
        template <> void predict_intra_smooth_av1_avx2<TX_16X16>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            // above[c] * sm_weights[r]
            // below_pred * (scale - sm_weights[r])
            // left[r] * sm_weights[c]
            // right_pred * (scale - sm_weights[c])

            const int width = 16;

            const __m256i k1 = _mm256_set1_epi16(1);
            const __m256i k128 = _mm256_set1_epi16(128);
            const __m256i a = _mm256_cvtepu8_epi16(loada_si128(topPels));
            const __m256i w = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_16));
            const __m256i b = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[width - 1]));
            const __m256i r = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(topPels[width - 1]));
            const __m256i iw = _mm256_subs_epi16(_mm256_set1_epi16(256), w);
            const __m256i rw = _mm256_mullo_epi16(r, iw);
            const __m256i bw = _mm256_mullo_epi16(b, iw);

            alignas(32) uint16_t bw_buf[width]; // below_pred * weight
            storea_si256(bw_buf, bw);

            for (int i = 0; i < width; i += 2) {
                __m256i w1, l1, bw1, res_part1, res_part2, row1, row2, lsb;

                w1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_16[i + 0]));
                l1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[i + 0]));
                bw1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(bw_buf[i + 0]));
                res_part1 = _mm256_mullo_epi16(a, w1);
                res_part2 = _mm256_mullo_epi16(l1, w);
                res_part1 = _mm256_add_epi16(res_part1, bw1);
                res_part2 = _mm256_add_epi16(res_part2, rw);

                lsb = _mm256_and_si256(_mm256_and_si256(res_part1, res_part2), k1);
                res_part1 = _mm256_srli_epi16(res_part1, 1);
                res_part2 = _mm256_srli_epi16(res_part2, 1);
                res_part1 = _mm256_adds_epu16(res_part1, res_part2);
                res_part1 = _mm256_adds_epu16(res_part1, lsb);
                res_part1 = _mm256_adds_epu16(res_part1, k128);
                row1 = _mm256_srli_epi16(res_part1, 8);

                w1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_16[i + 1]));
                l1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[i + 1]));
                bw1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(bw_buf[i + 1]));
                res_part1 = _mm256_mullo_epi16(a, w1);
                res_part2 = _mm256_mullo_epi16(l1, w);
                res_part1 = _mm256_add_epi16(res_part1, bw1);
                res_part2 = _mm256_add_epi16(res_part2, rw);

                lsb = _mm256_and_si256(_mm256_and_si256(res_part1, res_part2), k1);
                res_part1 = _mm256_srli_epi16(res_part1, 1);
                res_part2 = _mm256_srli_epi16(res_part2, 1);
                res_part1 = _mm256_adds_epu16(res_part1, res_part2);
                res_part1 = _mm256_adds_epu16(res_part1, lsb);
                res_part1 = _mm256_adds_epu16(res_part1, k128);
                row2 = _mm256_srli_epi16(res_part1, 8);

                storea2_m128i(dst, dst + pitch, permute4x64(_mm256_packus_epi16(row1, row2), 0, 2, 1, 3));
                dst += pitch * 2;
            }
        }
        template <> void predict_intra_smooth_av1_avx2<TX_32X32>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            // above[c] * sm_weights[r]
            // below_pred * (scale - sm_weights[r])
            // left[r] * sm_weights[c]
            // right_pred * (scale - sm_weights[c])

            const int width = 32;

            const __m256i k1 = _mm256_set1_epi16(1);
            const __m256i k128 = _mm256_set1_epi16(128);
            const __m256i a = loada_si256(topPels);
            const __m256i w = loada_si256(smooth_weights_32);
            const __m256i b = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[width - 1]));
            const __m256i r = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(topPels[width - 1]));
            const __m256i a_lo = _mm256_unpacklo_epi8(a, _mm256_setzero_si256());
            const __m256i a_hi = _mm256_unpackhi_epi8(a, _mm256_setzero_si256());
            const __m256i w_lo = _mm256_unpacklo_epi8(w, _mm256_setzero_si256());
            const __m256i w_hi = _mm256_unpackhi_epi8(w, _mm256_setzero_si256());
            const __m256i iw_lo = _mm256_subs_epi16(_mm256_set1_epi16(256), w_lo);
            const __m256i iw_hi = _mm256_subs_epi16(_mm256_set1_epi16(256), w_hi);
            const __m256i rw_lo = _mm256_mullo_epi16(r, iw_lo);
            const __m256i rw_hi = _mm256_mullo_epi16(r, iw_hi);
            const __m256i bw_lo = _mm256_mullo_epi16(b, iw_lo);
            const __m256i bw_hi = _mm256_mullo_epi16(b, iw_hi);

            alignas(32) uint16_t bw_buf[width]; // below_pred * weight
            storea_si256(bw_buf + 0, permute2x128(bw_lo, bw_hi, 0, 2));
            storea_si256(bw_buf + 16, permute2x128(bw_lo, bw_hi, 1, 3));

            for (int i = 0; i < width; i++) {
                const __m256i w1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_32[i]));
                const __m256i l1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[i]));
                const __m256i bw1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(bw_buf[i]));
                __m256i res_part1_lo = _mm256_mullo_epi16(a_lo, w1);
                __m256i res_part1_hi = _mm256_mullo_epi16(a_hi, w1);
                __m256i res_part2_lo = _mm256_mullo_epi16(l1, w_lo);
                __m256i res_part2_hi = _mm256_mullo_epi16(l1, w_hi);
                res_part1_lo = _mm256_add_epi16(res_part1_lo, bw1);
                res_part1_hi = _mm256_add_epi16(res_part1_hi, bw1);
                res_part2_lo = _mm256_add_epi16(res_part2_lo, rw_lo);
                res_part2_hi = _mm256_add_epi16(res_part2_hi, rw_hi);

                __m256i lsb_lo = _mm256_and_si256(_mm256_and_si256(res_part1_lo, res_part2_lo), k1);
                __m256i lsb_hi = _mm256_and_si256(_mm256_and_si256(res_part1_hi, res_part2_hi), k1);
                res_part1_lo = _mm256_srli_epi16(res_part1_lo, 1);
                res_part1_hi = _mm256_srli_epi16(res_part1_hi, 1);
                res_part2_lo = _mm256_srli_epi16(res_part2_lo, 1);
                res_part2_hi = _mm256_srli_epi16(res_part2_hi, 1);
                res_part1_lo = _mm256_adds_epu16(res_part1_lo, res_part2_lo);
                res_part1_hi = _mm256_adds_epu16(res_part1_hi, res_part2_hi);
                res_part1_lo = _mm256_adds_epu16(res_part1_lo, lsb_lo);
                res_part1_hi = _mm256_adds_epu16(res_part1_hi, lsb_hi);
                res_part1_lo = _mm256_adds_epu16(res_part1_lo, k128);
                res_part1_hi = _mm256_adds_epu16(res_part1_hi, k128);
                res_part1_lo = _mm256_srli_epi16(res_part1_lo, 8);
                res_part1_hi = _mm256_srli_epi16(res_part1_hi, 8);
                storea_si256(dst, _mm256_packus_epi16(res_part1_lo, res_part1_hi));
                dst += pitch;
            }
        }

        template <int txSize> void predict_intra_smooth_v_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch);
        template<> void predict_intra_smooth_v_av1_avx2<0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            __m128i w_tmp = loadl_si32(smooth_weights_4);   // 0123************
            w_tmp = _mm_unpacklo_epi8(w_tmp, w_tmp);        // 00112233********
            w_tmp = _mm_unpacklo_epi16(w_tmp, w_tmp);       // 0000111122223333
            __m256i w = _mm256_cvtepu8_epi16(w_tmp);        // 0000111122223333

            __m256i a = _mm256_broadcastq_epi64(_mm_unpacklo_epi8(loadl_si32(topPels), _mm_setzero_si128())); // 0123012301230123
            __m256i a_minus_b = _mm256_sub_epi16(a, _mm256_set1_epi16(leftPels[3]));
            __m256i b_by256_plus128 = _mm256_set1_epi16((leftPels[3] << 8) + 128);

            __m256i pred = _mm256_mullo_epi16(a_minus_b, w);
            pred = _mm256_add_epi16(pred, b_by256_plus128);
            pred = _mm256_srli_epi16(pred, 8);              // row0 row1 row2 row3
            pred = _mm256_packus_epi16(pred, pred);         // row01 row01 row23 row23
            storel_si32(dst + 0 * pitch, si128_lo(pred));
            storel_si32(dst + 1 * pitch, _mm_srli_si128(si128_lo(pred), 4));
            storel_si32(dst + 2 * pitch, si128_hi(pred));
            storel_si32(dst + 3 * pitch, _mm_srli_si128(si128_hi(pred), 4));
        }
        template<> void predict_intra_smooth_v_av1_avx2<1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            __m128i w_tmp = loadl_epi64(smooth_weights_8);                          // 01234567********
            w_tmp = _mm_unpacklo_epi8(w_tmp, w_tmp);                                // 0011223344556677
            __m128i w0123 = _mm_unpacklo_epi16(w_tmp, w_tmp);                       // 0000111122223333
            __m128i w4567 = _mm_unpackhi_epi16(w_tmp, w_tmp);                       // 4444555566667777
            __m256i w01 = _mm256_cvtepu8_epi16(_mm_unpacklo_epi32(w0123, w0123));   // 0000000011111111
            __m256i w23 = _mm256_cvtepu8_epi16(_mm_unpackhi_epi32(w0123, w0123));   // 2222222233333333
            __m256i w45 = _mm256_cvtepu8_epi16(_mm_unpacklo_epi32(w4567, w4567));   // 4444444455555555
            __m256i w67 = _mm256_cvtepu8_epi16(_mm_unpackhi_epi32(w4567, w4567));   // 6666666677777777

            __m256i a = _mm256_broadcastsi128_si256(_mm_unpacklo_epi8(loadl_epi64(topPels), _mm_setzero_si128()));
            __m256i a_minus_b = _mm256_sub_epi16(a, _mm256_set1_epi16(leftPels[7]));
            __m256i b_by256_plus128 = _mm256_set1_epi16((leftPels[7] << 8) + 128);

            __m256i pred01, pred23, pred45, pred67;
            pred01 = _mm256_mullo_epi16(a_minus_b, w01);
            pred23 = _mm256_mullo_epi16(a_minus_b, w23);
            pred45 = _mm256_mullo_epi16(a_minus_b, w45);
            pred67 = _mm256_mullo_epi16(a_minus_b, w67);

            pred01 = _mm256_add_epi16(pred01, b_by256_plus128);
            pred23 = _mm256_add_epi16(pred23, b_by256_plus128);
            pred45 = _mm256_add_epi16(pred45, b_by256_plus128);
            pred67 = _mm256_add_epi16(pred67, b_by256_plus128);

            pred01 = _mm256_srli_epi16(pred01, 8);
            pred23 = _mm256_srli_epi16(pred23, 8);
            pred45 = _mm256_srli_epi16(pred45, 8);
            pred67 = _mm256_srli_epi16(pred67, 8);

            pred01 = _mm256_packus_epi16(pred01, pred23);
            pred45 = _mm256_packus_epi16(pred45, pred67);

            storel_epi64(dst + 0 * pitch, si128_lo(pred01));
            storel_epi64(dst + 1 * pitch, si128_hi(pred01));
            storeh_epi64(dst + 2 * pitch, si128_lo(pred01));
            storeh_epi64(dst + 3 * pitch, si128_hi(pred01));
            storel_epi64(dst + 4 * pitch, si128_lo(pred45));
            storel_epi64(dst + 5 * pitch, si128_hi(pred45));
            storeh_epi64(dst + 6 * pitch, si128_lo(pred45));
            storeh_epi64(dst + 7 * pitch, si128_hi(pred45));
        }
        template<> void predict_intra_smooth_v_av1_avx2<2>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const __m256i a = _mm256_cvtepu8_epi16(loada_si128(topPels));
            const __m256i b = _mm256_set1_epi16(leftPels[15]);
            const __m256i b_by256_plus128 = _mm256_set1_epi16((leftPels[15] << 8) + 128);
            const __m256i a_minus_b = _mm256_sub_epi16(a, b);

            __m256i pred0, pred1, w0, w1;
            for (int r = 0; r < 16; r += 2) {
                w0 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_16[r+0]));
                w1 = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_16[r+1]));
                pred0 = _mm256_mullo_epi16(a_minus_b, w0);
                pred1 = _mm256_mullo_epi16(a_minus_b, w1);
                pred0 = _mm256_add_epi16(pred0, b_by256_plus128);
                pred1 = _mm256_add_epi16(pred1, b_by256_plus128);
                pred0 = _mm256_srli_epi16(pred0, 8);
                pred1 = _mm256_srli_epi16(pred1, 8);
                pred0 = _mm256_packus_epi16(pred0, pred1);
                pred0 = _mm256_permute4x64_epi64(pred0, PERM4x64(0,2,1,3));
                storea2_m128i(dst, dst + pitch, pred0);
                dst += pitch * 2;
            }
        }
        template<> void predict_intra_smooth_v_av1_avx2<3>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const __m256i a_lo = _mm256_cvtepu8_epi16(loada_si128(topPels+0));
            const __m256i a_hi = _mm256_cvtepu8_epi16(loada_si128(topPels+16));
            const __m256i b = _mm256_set1_epi16(leftPels[31]);
            const __m256i b_by256_plus128 = _mm256_set1_epi16((leftPels[31] << 8) + 128);
            const __m256i a_lo_minus_b = _mm256_sub_epi16(a_lo, b);
            const __m256i a_hi_minus_b = _mm256_sub_epi16(a_hi, b);

            __m256i pred_lo, pred_hi, w;
            for (int r = 0; r < 32; r++) {
                w = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_32[r]));
                pred_lo = _mm256_mullo_epi16(a_lo_minus_b, w);
                pred_hi = _mm256_mullo_epi16(a_hi_minus_b, w);
                pred_lo = _mm256_add_epi16(pred_lo, b_by256_plus128);
                pred_hi = _mm256_add_epi16(pred_hi, b_by256_plus128);
                pred_lo = _mm256_srli_epi16(pred_lo, 8);
                pred_hi = _mm256_srli_epi16(pred_hi, 8);
                pred_lo = _mm256_packus_epi16(pred_lo, pred_hi);
                pred_lo = _mm256_permute4x64_epi64(pred_lo, PERM4x64(0,2,1,3));
                storea_si256(dst, pred_lo);
                dst += pitch;
            }
        }

        template <int txSize> void predict_intra_smooth_h_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch);
        template<> void predict_intra_smooth_h_av1_avx2<0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            __m256i w = _mm256_broadcastq_epi64(_mm_unpacklo_epi8(loadl_si32(smooth_weights_4), _mm_setzero_si128())); // 01230123 01230123
            __m256i r_by256_plus128 = _mm256_set1_epi16((topPels[3] << 8) + 128);

            __m128i tmp = loadl_si32(leftPels);                             // 0123************
            tmp = _mm_unpacklo_epi8(tmp, tmp);                              // 00112233********
            __m256i l = _mm256_cvtepu8_epi16(_mm_unpacklo_epi16(tmp, tmp)); // 0000111122223333
            __m256i r = _mm256_set1_epi16(topPels[3]);
            __m256i l_minus_r = _mm256_sub_epi16(l, r);
            __m256i pred;

            pred = _mm256_mullo_epi16(l_minus_r, w);
            pred = _mm256_add_epi16(pred, r_by256_plus128);
            pred = _mm256_srli_epi16(pred, 8);              // row0 row1 row2 row3
            pred = _mm256_packus_epi16(pred, pred);         // row01 row01 row23 row23

            storel_si32(dst + 0 * pitch, si128_lo(pred));
            storel_si32(dst + 1 * pitch, _mm_srli_si128(si128_lo(pred), 4));
            storel_si32(dst + 2 * pitch, si128_hi(pred));
            storel_si32(dst + 3 * pitch, _mm_srli_si128(si128_hi(pred), 4));
        }
        template<> void predict_intra_smooth_h_av1_avx2<1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            __m256i w = _mm256_broadcastsi128_si256(_mm_unpacklo_epi8(loadl_epi64(smooth_weights_8), _mm_setzero_si128())); // 01234567 01234567
            __m256i r_by256_plus128 = _mm256_set1_epi16((topPels[7] << 8) + 128);

            __m128i l = _mm_unpacklo_epi8(loadl_epi64(leftPels), _mm_setzero_si128());
            __m128i r = _mm_set1_epi16(topPels[7]);
            __m128i tmp = _mm_sub_epi16(l, r);                                          // 01234567
            __m256i l_minus_r = _mm256_setr_m128i(tmp, _mm_srli_si128(tmp, 2));         // 01234567 1234567*
            __m256i l01_minus_r, l23_minus_r, l45_minus_r, l67_minus_r;
            l23_minus_r = _mm256_unpacklo_epi16(l_minus_r, l_minus_r);          // 00112233 11223344
            l01_minus_r = _mm256_shuffle_epi32(l23_minus_r, SHUF32(0,0,0,0));   // 00000000 11111111
            l23_minus_r = _mm256_shuffle_epi32(l23_minus_r, SHUF32(2,2,2,2));   // 22222222 33333333
            l67_minus_r = _mm256_unpackhi_epi16(l_minus_r, l_minus_r);          // 44556677 556677**
            l45_minus_r = _mm256_shuffle_epi32(l67_minus_r, SHUF32(0,0,0,0));   // 44444444 55555555
            l67_minus_r = _mm256_shuffle_epi32(l67_minus_r, SHUF32(2,2,2,2));   // 66666666 77777777

            __m256i pred01, pred23, pred45, pred67;
            pred01 = _mm256_mullo_epi16(l01_minus_r, w);
            pred23 = _mm256_mullo_epi16(l23_minus_r, w);
            pred45 = _mm256_mullo_epi16(l45_minus_r, w);
            pred67 = _mm256_mullo_epi16(l67_minus_r, w);

            pred01 = _mm256_add_epi16(pred01, r_by256_plus128);
            pred23 = _mm256_add_epi16(pred23, r_by256_plus128);
            pred45 = _mm256_add_epi16(pred45, r_by256_plus128);
            pred67 = _mm256_add_epi16(pred67, r_by256_plus128);

            pred01 = _mm256_srli_epi16(pred01, 8);
            pred23 = _mm256_srli_epi16(pred23, 8);
            pred45 = _mm256_srli_epi16(pred45, 8);
            pred67 = _mm256_srli_epi16(pred67, 8);

            pred01 = _mm256_packus_epi16(pred01, pred23);
            pred45 = _mm256_packus_epi16(pred45, pred67);

            storel_epi64(dst + 0 * pitch, si128_lo(pred01));
            storel_epi64(dst + 1 * pitch, si128_hi(pred01));
            storeh_epi64(dst + 2 * pitch, si128_lo(pred01));
            storeh_epi64(dst + 3 * pitch, si128_hi(pred01));
            storel_epi64(dst + 4 * pitch, si128_lo(pred45));
            storel_epi64(dst + 5 * pitch, si128_hi(pred45));
            storeh_epi64(dst + 6 * pitch, si128_lo(pred45));
            storeh_epi64(dst + 7 * pitch, si128_hi(pred45));
        }
        template<> void predict_intra_smooth_h_av1_avx2<2>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const __m256i w = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_16));
            const __m256i r_by256_plus128 = _mm256_set1_epi16((topPels[15] << 8) + 128);

            const __m256i l = _mm256_cvtepu8_epi16(loada_si128(leftPels));
            const __m256i r = _mm256_set1_epi16(topPels[15]);
            __m256i l_minus_r = _mm256_sub_epi16(l, r);
            __m256i l0_minus_r, l1_minus_r;

            ALIGN_DECL(32) short buf_l_minus_r[16];
            storea_si256(buf_l_minus_r + 0, l_minus_r);

            __m256i pred_lo, pred_hi;
            for (int r = 0; r < 16; r += 2) {
                l0_minus_r = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(buf_l_minus_r[r+0]));
                l1_minus_r = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(buf_l_minus_r[r+1]));
                pred_lo = _mm256_mullo_epi16(l0_minus_r, w);
                pred_hi = _mm256_mullo_epi16(l1_minus_r, w);
                pred_lo = _mm256_add_epi16(pred_lo, r_by256_plus128);
                pred_hi = _mm256_add_epi16(pred_hi, r_by256_plus128);
                pred_lo = _mm256_srli_epi16(pred_lo, 8);
                pred_hi = _mm256_srli_epi16(pred_hi, 8);
                pred_lo = _mm256_packus_epi16(pred_lo, pred_hi);
                pred_lo = _mm256_permute4x64_epi64(pred_lo, PERM4x64(0,2,1,3));
                storea2_m128i(dst, dst + pitch, pred_lo);
                dst += pitch * 2;
            }
        }
        template<> void predict_intra_smooth_h_av1_avx2<3>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const __m256i w_lo = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_32 + 0));
            const __m256i w_hi = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_32 + 16));
            const __m256i r_by256_plus128 = _mm256_set1_epi16((topPels[31] << 8) + 128);

            const __m256i l_lo = _mm256_cvtepu8_epi16(loada_si128(leftPels+0));
            const __m256i l_hi = _mm256_cvtepu8_epi16(loada_si128(leftPels+16));
            const __m256i r = _mm256_set1_epi16(topPels[31]);
            __m256i l_lo_minus_r = _mm256_sub_epi16(l_lo, r);
            __m256i l_hi_minus_r = _mm256_sub_epi16(l_hi, r);
            __m256i l_minus_r;

            ALIGN_DECL(32) short buf_l_minus_r[32];
            storea_si256(buf_l_minus_r + 0,  l_lo_minus_r);
            storea_si256(buf_l_minus_r + 16, l_hi_minus_r);

            __m256i pred_lo, pred_hi;
            for (int r = 0; r < 32; r++) {
                l_minus_r = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(buf_l_minus_r[r]));
                pred_lo = _mm256_mullo_epi16(l_minus_r, w_lo);
                pred_hi = _mm256_mullo_epi16(l_minus_r, w_hi);
                pred_lo = _mm256_add_epi16(pred_lo, r_by256_plus128);
                pred_hi = _mm256_add_epi16(pred_hi, r_by256_plus128);
                pred_lo = _mm256_srli_epi16(pred_lo, 8);
                pred_hi = _mm256_srli_epi16(pred_hi, 8);
                pred_lo = _mm256_packus_epi16(pred_lo, pred_hi);
                pred_lo = _mm256_permute4x64_epi64(pred_lo, PERM4x64(0,2,1,3));
                storea_si256(dst, pred_lo);
                dst += pitch;
            }
        }
    };

    template <int size> void convert_ref_pels_av1(const uint8_t *refPel, uint8_t *refPelU, uint8_t *refPelV, int upscale) {
        refPelU[-1] = refPel[-2];
        refPelV[-1] = refPel[-1];
        if (size == 0) {
            if (upscale) {
                refPelU[-2] = refPel[-4];
                refPelV[-2] = refPel[-3];
                __m256i mask = _mm256_set1_epi16(0xff);
                __m256i uv = loada_si256(refPel);           // above 16*2 bytes  // u0 v0 ... u15 v15
                __m256i u  = _mm256_and_si256(uv, mask);    // 16w: u0 .. u15
                __m256i v  = _mm256_srli_epi16(uv, 8);      // 16w: v0 .. v15
                __m256i tmp = _mm256_packus_epi16(u, v);    // 32b: u0 .. u7 v0 .. v7 u8 .. u15 v8 .. v15
                tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
                storea2_m128i(refPelU, refPelV, tmp);
                return;
            }
            __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
            __m128i a = loada_si128(refPel);  // above 8*2 bytes
            __m128i u  = _mm_and_si128(a, mask);
            __m128i v  = _mm_srli_epi16(a, 8);
            __m128i tmp = _mm_packus_epi16(u, v);
            storel_epi64(refPelU, tmp);
            storeh_epi64(refPelV, tmp);

        } else if (size == 1) {
            if (upscale) {
                refPelU[-2] = refPel[-4];
                refPelV[-2] = refPel[-3];
                __m256i mask = _mm256_set1_epi16(0xff);
                __m256i uv = loada_si256(refPel);           // above 16*2 bytes  // u0 v0 ... u15 v15
                __m256i u  = _mm256_and_si256(uv, mask);    // 16w: u0 .. u15
                __m256i v  = _mm256_srli_epi16(uv, 8);      // 16w: v0 .. v15
                __m256i tmp = _mm256_packus_epi16(u, v);    // 32b: u0 .. u7 v0 .. v7 u8 .. u15 v8 .. v15
                tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
                storea2_m128i(refPelU, refPelV, tmp);

                uv = loada_si256(refPel + 16 * 2);
                u  = _mm256_and_si256(uv, mask);
                v  = _mm256_srli_epi16(uv, 8);
                tmp = _mm256_packus_epi16(u, v);
                tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
                storea2_m128i(refPelU + 16, refPelV + 16, tmp);
                return;
            }
            __m256i mask = _mm256_set1_epi16(0xff);
            __m256i uv = loada_si256(refPel);           // above 16*2 bytes  // u0 v0 ... u15 v15
            __m256i u  = _mm256_and_si256(uv, mask);    // 16w: u0 .. u15
            __m256i v  = _mm256_srli_epi16(uv, 8);      // 16w: v0 .. v15
            __m256i tmp = _mm256_packus_epi16(u, v);    // 32b: u0 .. u7 v0 .. v7 u8 .. u15 v8 .. v15
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU, refPelV, tmp);

        } else if (size == 2) {
            __m256i mask = _mm256_set1_epi16(0xff);
            __m256i uv = loada_si256(refPel);           // above 16*2 bytes  // u0 v0 ... u15 v15
            __m256i u  = _mm256_and_si256(uv, mask);    // 16w: u0 .. u15
            __m256i v  = _mm256_srli_epi16(uv, 8);      // 16w: v0 .. v15
            __m256i tmp = _mm256_packus_epi16(u, v);    // 32b: u0 .. u7 v0 .. v7 u8 .. u15 v8 .. v15
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU, refPelV, tmp);

            uv = loada_si256(refPel + 16 * 2);
            u  = _mm256_and_si256(uv, mask);
            v  = _mm256_srli_epi16(uv, 8);
            tmp = _mm256_packus_epi16(u, v);
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU + 16, refPelV + 16, tmp);

        } else {
            __m256i mask = _mm256_set1_epi16(0xff);
            for (int32_t i = 0; i < 64; i += 16) {
                __m256i uv = loada_si256(refPel + 2 * i);  // above 16*2 bytes  // u0 v0 ... u15 v15
                __m256i u  = _mm256_and_si256(uv, mask);       // 16w: u0 .. u15
                __m256i v  = _mm256_srli_epi16(uv, 8);         // 16w: v0 .. v15
                __m256i tmp = _mm256_packus_epi16(u, v);       // 32b: u0 .. u7 v0 .. v7 u8 .. u15 v8 .. v15
                tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
                storea2_m128i(refPelU + i, refPelV + i, tmp);
            }
        }
    }

    template <int size> void convert_pred_block(uint8_t *dst, int pitch) {
        const int width = 4 << size;
        if (size == 0) {
            __m128i tmp0 = loadu2_epi64(dst + pitch * 0, dst + pitch * 2);  // u0 v0 u2 v2
            __m128i tmp1 = loadu2_epi64(dst + pitch * 1, dst + pitch * 3);  // u1 v1 u3 v3
            __m128i tmp2 = _mm_unpacklo_epi32(tmp0, tmp1);                  // u0 u1 v0 v1
            __m128i tmp3 = _mm_unpackhi_epi32(tmp0, tmp1);                  // u2 u3 v2 v3
            __m128i u = _mm_unpacklo_epi64(tmp2, tmp3);                     // u0 u1 u2 u3
            __m128i v = _mm_unpackhi_epi64(tmp2, tmp3);                     // v0 v1 v2 v3
            __m128i uv = _mm_unpacklo_epi8(u, v);
            storel_epi64(dst + pitch * 0, uv);
            storeh_epi64(dst + pitch * 1, uv);
            uv = _mm_unpackhi_epi8(u, v);
            storel_epi64(dst + pitch * 2, uv);
            storeh_epi64(dst + pitch * 3, uv);

        } else if (size == 1) {
            int32_t p = pitch, p2 = pitch * 2, p3 = pitch * 3, p4 = pitch * 4;
            __m256i tmp0 = loada2_m128i(dst,      dst + p);     // u0 v0 u1 v1
            __m256i tmp1 = loada2_m128i(dst + p2, dst + p3);    // u2 v2 u3 v3
            __m256i u_ = _mm256_unpacklo_epi64(tmp0, tmp1);     // u0 u2 u1 u3
            __m256i v_ = _mm256_unpackhi_epi64(tmp0, tmp1);     // v0 v2 v1 v3
            __m256i uv01 = _mm256_unpacklo_epi8(u_, v_);        // uv0 uv1
            __m256i uv23 = _mm256_unpackhi_epi8(u_, v_);        // uv2 uv3
            storea2_m128i(dst,      dst + p,  uv01);
            storea2_m128i(dst + p2, dst + p3, uv23);
            dst += p4;
            tmp0 = loada2_m128i(dst,      dst + p);     // u4 v4 u5 v5
            tmp1 = loada2_m128i(dst + p2, dst + p3);    // u6 v6 u7 v7
            u_ = _mm256_unpacklo_epi64(tmp0, tmp1);     // u4 u6 u5 u7
            v_ = _mm256_unpackhi_epi64(tmp0, tmp1);     // v4 v6 v5 v7
            uv01 = _mm256_unpacklo_epi8(u_, v_);        // uv4 uv5
            uv23 = _mm256_unpackhi_epi8(u_, v_);        // uv6 uv7
            storea2_m128i(dst,      dst + p,  uv01);
            storea2_m128i(dst + p2, dst + p3, uv23);

        } else if (size == 2) {
            int32_t p = pitch, p2 = pitch * 2;
            for (int32_t i = 0; i < width; i += 2, dst += p2) {
                __m256i tmp0 = loada_si256(dst);                            // u0 .. u15 v0 .. v15
                tmp0 = _mm256_permute4x64_epi64(tmp0, PERM4x64(0,2,1,3));   // u0..u7 v0..v7 u8..u15 v8..v15
                __m256i tmp1 = _mm256_bsrli_epi128(tmp0, 8);                // v0..v7 000000 v8..v15 000000
                __m256i uv = _mm256_unpacklo_epi8(tmp0, tmp1);              // u0 v0 .. u15 v15
                storea_si256(dst, uv);

                tmp0 = loada_si256(dst + p);
                tmp0 = _mm256_permute4x64_epi64(tmp0, PERM4x64(0,2,1,3));
                tmp1 = _mm256_bsrli_epi128(tmp0, 8);
                uv = _mm256_unpacklo_epi8(tmp0, tmp1);
                storea_si256(dst + p, uv);
            }

        } else {
            int32_t p = pitch, p2 = pitch * 2;
            for (int32_t i = 0; i < width; i += 2, dst += p2) {
                __m256i u = loada_si256(dst + 0);           // u0 .. u31
                __m256i v = loada_si256(dst + 32);          // v0 .. v31
                __m256i uv0 = _mm256_unpacklo_epi8(u, v);   // u0 v0 .. u7 v7    u16 v16 .. u23 v23
                __m256i uv1 = _mm256_unpackhi_epi8(u, v);   // u8 v8 .. u15 v15  u24 v24 .. u31 v31
                storea_si256(dst + 0,  _mm256_permute2x128_si256(uv0, uv1, PERM2x128(0,2)));
                storea_si256(dst + 32, _mm256_permute2x128_si256(uv0, uv1, PERM2x128(1,3)));

                u = loada_si256(dst + p + 0);
                v = loada_si256(dst + p + 32);
                uv0 = _mm256_unpacklo_epi8(u, v);
                uv1 = _mm256_unpackhi_epi8(u, v);
                storea_si256(dst + p + 0,  _mm256_permute2x128_si256(uv0, uv1, PERM2x128(0,2)));
                storea_si256(dst + p + 32, _mm256_permute2x128_si256(uv0, uv1, PERM2x128(1,3)));
            }
        }
    }

    template <int size, int haveLeft, int haveAbove, typename PixType>
    void predict_intra_nv12_dc_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        const int width = 4 << size;
        assert(size_t(topPels)  % 32 == 0);
        assert(size_t(leftPels) % 32 == 0);
        IntraPredPels<uint8_t, width> predPelsU;
        IntraPredPels<uint8_t, width> predPelsV;

        convert_ref_pels_av1<size>(topPels,  predPelsU.top,  predPelsV.top, 0);
        convert_ref_pels_av1<size>(leftPels, predPelsU.left, predPelsV.left, 0);
        predict_intra_dc_av1_avx2<size, haveLeft, haveAbove>(predPelsU.top, predPelsU.left, dst + 0, pitch, 0, 0, 0);
        predict_intra_dc_av1_avx2<size, haveLeft, haveAbove>(predPelsV.top, predPelsV.left, dst + width, pitch, 0, 0, 0);
        convert_pred_block<size>(dst, pitch);
    }

    template<> void predict_intra_nv12_dc_av1_avx2<0, 0, 0, uint8_t>(const uint8_t*, const uint8_t*, uint8_t *dst, int pitch, int, int, int)
    {
        const __m128i dc = _mm_set1_epi32(0x80808080);
        const int pitch2 = pitch * 2;
        storel_epi64(dst, dc);
        storel_epi64(dst + pitch, dc);
        dst += pitch2;
        storel_epi64(dst, dc);
        storel_epi64(dst + pitch, dc);
    }

    template void predict_intra_nv12_dc_av1_avx2<0, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<0, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<0, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template<> void predict_intra_nv12_dc_av1_avx2<1, 0, 0, uint8_t>(const uint8_t*, const uint8_t*, uint8_t *dst, int pitch, int, int, int)
    {
        const __m128i dc = _mm_set1_epi32(0x80808080);
        const int pitch2 = pitch * 2;
        storea_si128(dst, dc);
        storea_si128(dst + pitch, dc);
        dst += pitch2;
        storea_si128(dst, dc);
        storea_si128(dst + pitch, dc);
        dst += pitch2;
        storea_si128(dst, dc);
        storea_si128(dst + pitch, dc);
        dst += pitch2;
        storea_si128(dst, dc);
        storea_si128(dst + pitch, dc);
    }

    template void predict_intra_nv12_dc_av1_avx2<1, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<1, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<1, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template<> void predict_intra_nv12_dc_av1_avx2<2, 0, 0, uint8_t>(const uint8_t*, const uint8_t*, uint8_t *dst, int pitch, int, int, int)
    {
        const __m256i dc = _mm256_set1_epi32(0x80808080);
        const int pitch2 = pitch * 2;
        for (int i = 0; i < 16; i += 2) {
            storea_si256(dst, dc);
            storea_si256(dst + pitch, dc);
            dst += pitch2;
        }
    }

    template void predict_intra_nv12_dc_av1_avx2<2, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<2, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<2, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template<> void predict_intra_nv12_dc_av1_avx2<3, 0, 0, uint8_t>(const uint8_t*, const uint8_t*, uint8_t *dst, int pitch, int, int, int)
    {
        const __m256i dc = _mm256_set1_epi32(0x80808080);
        const int pitch2 = pitch * 2;
        for (int i = 0; i < 32; i += 2) {
            storea_si256(dst, dc);
            storea_si256(dst + 32, dc);
            storea_si256(dst + pitch, dc);
            storea_si256(dst + pitch + 32, dc);
            dst += pitch2;
        }
    }

    template void predict_intra_nv12_dc_av1_avx2<3, 0, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<3, 1, 0, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<3, 1, 1, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    namespace details {
        template <int txSize> void predict_intra_z1_av1_avx2(const uint8_t *topPels, uint8_t *dst, int pitch, int upTop, int dx);
        template <> void predict_intra_z1_av1_avx2<TX_4X4>(const uint8_t *topPels, uint8_t *dst, int pitch, int upTop, int dx)
        {
            upTop;
            assert(upTop == 0);
            assert(dx > 0);

            ALIGN_DECL(32) uint16_t aboveBy256[16];
            ALIGN_DECL(32) int16_t aboveDiff[16];

            // pre-filter above pixels
            // store in temp buffers:
            //   above[x] * 32 + 16
            //   above[x+1] - above[x]
            // final pixels will be caluculated as:
            //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

            __m128i a0, a1, ar, diff, a256;
            a0 = loadl_epi64(topPels);                          // 16b: 01234567 ........
            a0 = _mm_unpacklo_epi8(a0, _mm_setzero_si128());    // 8w:  01234567
            ar = _mm_shuffle_epi8(a0, _mm_set1_epi16(0xF0E));   // 8w:  77777777
            a1 = _mm_alignr_epi8(ar, a0, 2);                    // 8w:  12345677
            diff = _mm_sub_epi16(a1, a0);                       // 8w: a[x+1] - a[x]
            a256 = _mm_slli_epi16(a0, 5);                       // 8w: a[x] * 32
            a256 = _mm_add_epi16(a256, _mm_set1_epi16(16));     // 8w: a[x] * 32 + 16
            storea_si128(aboveBy256, a256);
            storea_si128(aboveDiff, diff);

            a256 = _mm_slli_epi16(ar, 5);                       // 8w: a[x] * 32
            diff = _mm_setzero_si128();                         // 8w: a[x+1] - a[x]
            storea_si128(aboveBy256 + 8, a256);
            storea_si128(aboveDiff + 8, diff);

            int x = dx;
            __m256i inc = _mm256_set1_epi16(dx);                                    // 16w: dx dx dx dx dx dx dx dx | dx dx dx dx dx dx dx dx
            __m256i inc2 = _mm256_add_epi16(inc, inc);                              // 16w: 2dx 2dx 2dx 2dx 2dx 2dx 2dx 2dx | 2dx 2dx 2dx 2dx 2dx 2dx 2dx 2dx
            inc = _mm256_unpacklo_epi64(inc, inc2);                                 // 16w: dx dx dx dx 2dx 2dx 2dx 2dx | dx dx dx dx 2dx 2dx 2dx 2dx
            __m256i tmp = _mm256_add_epi16(inc, inc2);                              // 16w: 3dx 3dx 3dx 3dx 4dx 4dx 4dx 4dx | 3dx 3dx 3dx 3dx 4dx 4dx 4dx 4dx
            __m256i shift = _mm256_permute2x128_si256(inc, tmp, PERM2x128(0,2));    // 16w: dx dx dx dx 2dx 2dx 2dx 2dx | 3dx 3dx 3dx 3dx 4dx 4dx 4dx 4dx
            shift = _mm256_and_si256(shift, _mm256_set1_epi16(0x3f));
            shift = _mm256_srli_epi16(shift, 1);

            int base0 = x >> 6; x += dx;
            int base1 = x >> 6; x += dx;
            int base2 = x >> 6; x += dx;
            int base3 = x >> 6;

            __m256i a = _mm256_setr_epi64x(*(int64_t*)(aboveBy256 + base0), *(int64_t*)(aboveBy256 + base1),
                                           *(int64_t*)(aboveBy256 + base2), *(int64_t*)(aboveBy256 + base3));
            __m256i b = _mm256_setr_epi64x(*(int64_t*)(aboveDiff + base0), *(int64_t*)(aboveDiff + base1),
                                           *(int64_t*)(aboveDiff + base2), *(int64_t*)(aboveDiff + base3));
            b = _mm256_mullo_epi16(b, shift);
            __m256i res = _mm256_add_epi16(a, b);
            res = _mm256_srli_epi16(res, 5);

            res = _mm256_packus_epi16(res, res);    // 0000 1111 0000 1111 | 2222 3333 2222 3333
            __m128i lo = si128(res);
            __m128i hi = _mm256_extracti128_si256(res, 1);
            storel_si32(dst, lo);
            storel_si32(dst + pitch, _mm_bsrli_si128(lo, 4));
            storel_si32(dst + pitch * 2, hi);
            storel_si32(dst + pitch * 3, _mm_bsrli_si128(hi, 4));
        }
        template <> void predict_intra_z1_av1_avx2<TX_8X8>(const uint8_t *topPels, uint8_t *dst, int pitch, int upTop, int dx)
        {
            upTop;
            assert(upTop == 0);
            assert(dx > 0);

            const int width = 4 << TX_8X8;
            const uint8_t* pa = topPels;

            ALIGN_DECL(32) uint16_t aboveBy256[32];
            ALIGN_DECL(32) int16_t aboveDiff[32];

            // pre-filter above pixels
            // store in temp buffers:
            //   above[x] * 32 + 16
            //   above[x+1] - above[x]
            // final pixels will be caluculated as:
            //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

            __m256i a0, a1, ar, diff, a256;
            a0 = _mm256_cvtepu8_epi16(loada_si128(pa));             // 16w: 01234567 89abcdef
            ar = _mm256_bsrli_epi128(a0, 14);                       // 16w: 7....... f.......
            ar = _mm256_shuffle_epi8(ar, _mm256_set1_epi16(256));   // 16w: 77777777 ffffffff
            a1 = _mm256_permute4x64_epi64(a0, PERM4x64(2,3,2,3));   // 16w: 89abcdef 89abcdef
            a1 = _mm256_permute2x128_si256(a1, ar, PERM2x128(0,3)); // 16w: 89abcdef ffffffff
            a1 = _mm256_alignr_epi8(a1, a0, 2);                     // 16w: 12345678 9abcdeff
            diff = _mm256_sub_epi16(a1, a0);                        // 16w: a[x+1] - a[x]
            a256 = _mm256_slli_epi16(a0, 5);                        // 16w: a[x] * 32
            a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));     // 16w: a[x] * 32 + 16
            storea_si256(aboveBy256, a256);
            storea_si256(aboveDiff, diff);

            ar = _mm256_permute4x64_epi64(ar, PERM4x64(2,3,2,3));   // 16w: ffffffff ffffffff
            a256 = _mm256_slli_epi16(ar, 5);                        // 16w: a[x] * 32
            diff = _mm256_setzero_si256();                          // 16w: a[x+1] - a[x]
            storea_si256(aboveBy256 + 16, a256);
            storea_si256(aboveDiff + 16, diff);

            int x = dx;
            __m256i inc = _mm256_set1_epi16(dx);                                    // 16w: dx dx dx dx dx dx dx dx | dx dx dx dx dx dx dx dx
            __m256i inc2 = _mm256_add_epi16(inc, inc);                              // 16w: 2dx 2dx 2dx 2dx 2dx 2dx 2dx 2dx | 2dx 2dx 2dx 2dx 2dx 2dx 2dx 2dx
            __m256i shift = _mm256_permute2x128_si256(inc, inc2, PERM2x128(0,2));   // 16w: dx dx dx dx dx dx dx dx | 2dx 2dx 2dx 2dx 2dx 2dx 2dx 2dx
            for (int r = 0; r < width; r += 4, dst += 4 * pitch) {
                __m256i a, b, res1, res2, res;

                int base0 = x >> 6;
                x += dx;
                int base1 = x >> 6;
                x += dx;
                a = loadu2_m128i(aboveBy256 + base0, aboveBy256 + base1);
                b = loadu2_m128i(aboveDiff + base0, aboveDiff + base1);
                shift = _mm256_and_si256(shift, _mm256_set1_epi16(0x3f));
                b = _mm256_mullo_epi16(b, _mm256_srli_epi16(shift, 1));
                res1 = _mm256_add_epi16(a, b);
                res1 = _mm256_srli_epi16(res1, 5);
                shift = _mm256_add_epi16(shift, inc2);

                base0 = x >> 6;
                x += dx;
                base1 = x >> 6;
                x += dx;
                a = loadu2_m128i(aboveBy256 + base0, aboveBy256 + base1);
                b = loadu2_m128i(aboveDiff + base0, aboveDiff + base1);
                shift = _mm256_and_si256(shift, _mm256_set1_epi16(0x3f));
                b = _mm256_mullo_epi16(b, _mm256_srli_epi16(shift, 1));
                res2 = _mm256_add_epi16(a, b);
                res2 = _mm256_srli_epi16(res2, 5);
                shift = _mm256_add_epi16(shift, inc2);

                res = _mm256_packus_epi16(res1, res2);
                storel_epi64(dst, si128(res));
                storel_epi64(dst + pitch, _mm256_extracti128_si256(res, 1));
                storeh_epi64(dst + pitch * 2, si128(res));
                storeh_epi64(dst + pitch * 3, _mm256_extracti128_si256(res, 1));
            }
        }
        template <> void predict_intra_z1_av1_avx2<TX_16X16>(const uint8_t *topPels, uint8_t *dst, int pitch, int upTop, int dx)
        {
            upTop;
            assert(upTop == 0);
            assert(dx > 0);

            const int width = 4 << TX_16X16;
            const uint8_t* pa = topPels;

            ALIGN_DECL(32) uint16_t aboveBy256[48];
            ALIGN_DECL(32) int16_t aboveDiff[48];

            // pre-filter above pixels
            // store in temp buffers:
            //   above[x] * 32 + 16
            //   above[x+1] - above[x]
            // final pixels will be caluculated as:
            //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 6

            __m256i a0, a1, ar, diff, a256;
            a0 = _mm256_cvtepu8_epi16(loada_si128(pa));             // 16w: 01234567 89abcdef
            a1 = _mm256_cvtepu8_epi16(loadu_si128(pa+1));           // 16w: 12345678 9abcdefg
            diff = _mm256_sub_epi16(a1, a0);                        // 16w: a[x+1] - a[x]
            a256 = _mm256_slli_epi16(a0, 5);                        // 16w: a[x] * 32
            a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));   // 16w: a[x] * 32 + 16
            storea_si256(aboveBy256, a256);
            storea_si256(aboveDiff, diff);

            a0 = _mm256_cvtepu8_epi16(loada_si128(pa+16));          // 16w: ghijklmn opqrstuv
            ar = _mm256_bsrli_epi128(a0, 14);                       // 16w: n....... v.......
            ar = _mm256_shuffle_epi8(ar, _mm256_set1_epi16(256));   // 16w: nnnnnnnn vvvvvvvv
            a1 = _mm256_permute4x64_epi64(a0, PERM4x64(2,3,2,3));   // 16w: opqrstuv opqrstuv
            a1 = _mm256_permute2x128_si256(a1, ar, PERM2x128(0,3)); // 16w: opqrstuv vvvvvvvv
            a1 = _mm256_alignr_epi8(a1, a0, 2);                     // 16w: hijklmno pqrstuvv
            diff = _mm256_sub_epi16(a1, a0);                        // 16w: a[x+1] - a[x]
            a256 = _mm256_slli_epi16(a0, 5);                        // 16w: a[x] * 32
            a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));   // 16w: a[x] * 32 + 16
            storea_si256(aboveBy256 + 16, a256);
            storea_si256(aboveDiff + 16, diff);

            ar = _mm256_permute4x64_epi64(ar, PERM4x64(2,3,2,3));   // 16w: vvvvvvvv vvvvvvvv
            a256 = _mm256_slli_epi16(ar, 5);                        // 16w: a[x] * 32
            diff = _mm256_setzero_si256();                          // 16w: a[x+1] - a[x]
            storea_si256(aboveBy256 + 32, a256);
            storea_si256(aboveDiff + 32, diff);

            int x = dx;
            __m256i inc = _mm256_set1_epi16(dx);
            __m256i shift = _mm256_and_si256(inc, _mm256_set1_epi16(0x3f));
            for (int r = 0; r < width; r += 2, dst += 2 * pitch) {
                __m256i a, b, res1, res2, res;

                int base = x >> 6;
                a = loadu_si256(aboveBy256 + base);
                b = loadu_si256(aboveDiff + base);
                b = _mm256_mullo_epi16(b, _mm256_srli_epi16(shift, 1));
                res1 = _mm256_add_epi16(a, b);
                res1 = _mm256_srli_epi16(res1, 5);
                x += dx;
                shift = _mm256_and_si256(_mm256_add_epi16(shift, inc), _mm256_set1_epi16(0x3f));

                base = x >> 6;
                a = loadu_si256(aboveBy256 + base);
                b = loadu_si256(aboveDiff + base);
                b = _mm256_mullo_epi16(b, _mm256_srli_epi16(shift, 1));
                res2 = _mm256_add_epi16(a, b);
                res2 = _mm256_srli_epi16(res2, 5);
                x += dx;
                shift = _mm256_and_si256(_mm256_add_epi16(shift, inc), _mm256_set1_epi16(0x3f));

                res = _mm256_packus_epi16(res1, res2);
                res = _mm256_permute4x64_epi64(res, PERM4x64(0,2,1,3));
                storeu2_m128i(dst, dst + pitch, res);
            }
        }
        template <> void predict_intra_z1_av1_avx2<TX_32X32>(const uint8_t *topPels, uint8_t *dst, int pitch, int upTop, int dx)
        {
            upTop;
            assert(upTop == 0);
            assert(dx > 0);

            const int width = 4 << TX_32X32;
            const uint8_t* pa = topPels;

            ALIGN_DECL(32) uint16_t aboveBy256[80];
            ALIGN_DECL(32) int16_t aboveDiff[80];

            // pre-filter above pixels
            // store in temp buffers:
            //   above[x] * 32 + 16
            //   above[x+1] - above[x]
            // final pixels will be caluculated as:
            //   (above[x] * 32 + 16 + (above[x+1] - above[x]) * shift) >> 5

            __m256i a0, a1, ar, diff, a256;
            a0 = _mm256_cvtepu8_epi16(loada_si128(pa));             // 16w: 01234567 89abcdef
            a1 = _mm256_cvtepu8_epi16(loadu_si128(pa+1));           // 16w: 12345678 9abcdefg
            diff = _mm256_sub_epi16(a1, a0);                        // 16w: a[x+1] - a[x]
            a256 = _mm256_slli_epi16(a0, 5);                        // 16w: a[x] * 32
            a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));   // 16w: a[x] * 32 + 16
            storea_si256(aboveBy256, a256);
            storea_si256(aboveDiff, diff);

            a0 = _mm256_cvtepu8_epi16(loada_si128(pa+16));          // 16w: 01234567 89abcdef
            a1 = _mm256_cvtepu8_epi16(loadu_si128(pa+17));          // 16w: 12345678 9abcdefg
            diff = _mm256_sub_epi16(a1, a0);                        // 16w: a[x+1] - a[x]
            a256 = _mm256_slli_epi16(a0, 5);                        // 16w: a[x] * 32
            a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));   // 16w: a[x] * 32 + 16
            storea_si256(aboveBy256 + 16, a256);
            storea_si256(aboveDiff + 16, diff);

            a0 = _mm256_cvtepu8_epi16(loada_si128(pa+32));          // 16w: 01234567 89abcdef
            a1 = _mm256_cvtepu8_epi16(loadu_si128(pa+33));          // 16w: 12345678 9abcdefg
            diff = _mm256_sub_epi16(a1, a0);                        // 16w: a[x+1] - a[x]
            a256 = _mm256_slli_epi16(a0, 5);                        // 16w: a[x] * 32
            a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));   // 16w: a[x] * 32 + 16
            storea_si256(aboveBy256 + 32, a256);
            storea_si256(aboveDiff + 32, diff);

            a0 = _mm256_cvtepu8_epi16(loada_si128(pa+48));          // 16w: ghijklmn opqrstuv
            ar = _mm256_bsrli_epi128(a0, 14);                       // 16w: n....... v.......
            ar = _mm256_shuffle_epi8(ar, _mm256_set1_epi16(256));   // 16w: nnnnnnnn vvvvvvvv
            a1 = _mm256_permute4x64_epi64(a0, PERM4x64(2,3,2,3));   // 16w: opqrstuv opqrstuv
            a1 = _mm256_permute2x128_si256(a1, ar, PERM2x128(0,3)); // 16w: opqrstuv vvvvvvvv
            a1 = _mm256_alignr_epi8(a1, a0, 2);                     // 16w: hijklmno pqrstuvv
            diff = _mm256_sub_epi16(a1, a0);                        // 16w: a[x+1] - a[x]
            a256 = _mm256_slli_epi16(a0, 5);                        // 16w: a[x] * 32
            a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));   // 16w: a[x] * 32 + 16
            storea_si256(aboveBy256 + 48, a256);
            storea_si256(aboveDiff + 48, diff);

            ar = _mm256_permute4x64_epi64(ar, PERM4x64(2,3,2,3));   // 16w: vvvvvvvv vvvvvvvv
            a256 = _mm256_slli_epi16(ar, 5);                        // 16w: a[x] * 32
            diff = _mm256_setzero_si256();                          // 16w: a[x+1] - a[x]
            storea_si256(aboveBy256 + 64, a256);
            storea_si256(aboveDiff + 64, diff);

            int x = dx;
            __m256i inc = _mm256_set1_epi16(dx);
            __m256i shift = _mm256_and_si256(inc, _mm256_set1_epi16(0x3f));
            for (int r = 0; r < width; r++, dst += pitch) {
                __m256i a, b, res1, res2, res;

                int base = x >> 6;
                a = loadu_si256(aboveBy256 + base);
                b = loadu_si256(aboveDiff + base);
                b = _mm256_mullo_epi16(b, _mm256_srli_epi16(shift, 1));
                res1 = _mm256_add_epi16(a, b);
                res1 = _mm256_srli_epi16(res1, 5);

                a = loadu_si256(aboveBy256 + base + 16);
                b = loadu_si256(aboveDiff + base + 16);
                b = _mm256_mullo_epi16(b, _mm256_srli_epi16(shift, 1));
                res2 = _mm256_add_epi16(a, b);
                res2 = _mm256_srli_epi16(res2, 5);

                x += dx;
                shift = _mm256_and_si256(_mm256_add_epi16(shift, inc), _mm256_set1_epi16(0x3f));

                res = _mm256_packus_epi16(res1, res2);
                res = _mm256_permute4x64_epi64(res, PERM4x64(0,2,1,3));
                storea_si256(dst, res);
            }
        }

        template <bool blend> void predict_4x4_sse4(const uint8_t* refPel, __m128i *dst, int dx)
        {
            const int width = 4;

            // topPels[i + 1] - topPels[i]
            alignas(16) int16_t diffBuf[2 * width];
            __m128i a0, a1;
            a0 = loadl_epi64(refPel - 1);                     // 16b: -10123456********
            a0 = _mm_unpacklo_epi8(a0, _mm_setzero_si128());  // 8w:  -10123456
            a1 = _mm_slli_si128(_mm_srli_si128(a0, 2), 6);    // 8w:   zzz 01234
            a0 = _mm_slli_si128(a0, 6);                       // 8w:   zzz-10123
            storea_si128(diffBuf, _mm_sub_epi16(a1, a0));     // 8w:   zzz-10123

            // topPels[i + 1] * 32 + 16
            alignas(16) int16_t by32Buf[2 * width];
            storea_si128(by32Buf, _mm_add_epi16(_mm_slli_epi16(a0, 5), _mm_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            __m128i shift_inc = _mm_set1_epi16(-dx);
            __m128i shift_inc2 = _mm_add_epi16(shift_inc, shift_inc);
            __m128i shift01 = _mm_add_epi16(shift_inc, _mm_slli_si128(shift_inc, 8));
            __m128i shift23 = _mm_add_epi16(shift01, shift_inc2);
            __m128i shift_mask = _mm_set1_epi16(0x3F);

            int base0 = (-dx * 1) >> 6;
            int base1 = (-dx * 2) >> 6;
            int base2 = (-dx * 3) >> 6;
            int base3 = (-dx * 4) >> 6;

            int base0_ = (base0 < -33) ? -33 : base0;
            int base1_ = (base1 < -33) ? -33 : base1;
            int base2_ = (base2 < -33) ? -33 : base2;
            int base3_ = (base3 < -33) ? -33 : base3;

            if (base0 < -width) base0 = -width;
            if (base1 < -width) base1 = -width;
            if (base2 < -width) base2 = -width;
            if (base3 < -width) base3 = -width;

            __m128i diff0 = loadu2_epi64(pdiff + base0, pdiff + base1);
            __m128i diff1 = loadu2_epi64(pdiff + base2, pdiff + base3);
            __m128i res0 = _mm_mullo_epi16(diff0, _mm_srli_epi16(_mm_and_si128(shift01, shift_mask), 1));
            __m128i res1 = _mm_mullo_epi16(diff1, _mm_srli_epi16(_mm_and_si128(shift23, shift_mask), 1));
            res0 = _mm_srli_epi16(_mm_add_epi16(res0, loadu2_epi64(pby32 + base0, pby32 + base1)), 5);
            res1 = _mm_srli_epi16(_mm_add_epi16(res1, loadu2_epi64(pby32 + base2, pby32 + base3)), 5);
            res0 = _mm_packus_epi16(res0, res1);

            if (blend) {
                const uint8_t *pmask = z2_blend_mask + 65;
                res0 = _mm_blendv_epi8(*dst, res0, loadu4_epi32(pmask + base0_, pmask + base1_, pmask + base2_, pmask + base3_));
            }
            *dst = res0;
        }
        template <bool blend> void predict_8x8_sse4(const uint8_t* refPel, uint8_t* dst, int pitch, int dx)
        {
            const int width = 8;

            // topPels[i + 1] - topPels[i]
            alignas(32) int16_t diffBuf[2 * width];
            __m128i z = _mm_setzero_si128();
            __m128i a0_bytes = loada_si128(refPel + 0);
            __m128i a0 = _mm_unpacklo_epi8(a0_bytes, z);
            __m128i a1 = _mm_unpacklo_epi8(_mm_srli_si128(a0_bytes, 1), z);
            storea_si128(diffBuf + 0, z);
            diffBuf[width - 1] = refPel[0] - refPel[-1];
            storea_si128(diffBuf + 8, _mm_sub_epi16(a1, a0));

            // topPels[i + 1] * 32 + 16
            alignas(32) int16_t by32Buf[2 * width];
            storea_si128(by32Buf + 0, z);
            by32Buf[width - 1] = (refPel[-1] << 5) + 16;
            storea_si128(by32Buf + 8, _mm_add_epi16(_mm_slli_epi16(a0, 5), _mm_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            int x = -dx;
            int dx2 = dx * 2;
            __m128i shift_inc = _mm_set1_epi16(-dx);
            __m128i shift_mask = _mm_set1_epi16(0x3F);
            __m128i shift = _mm_setzero_si128();
            for (int r = 0; r < width; r += 2, x -= dx2, dst += 2 * pitch) {
                const int base0 = x >> 6;
                const int base1 = (x - dx) >> 6;
                if (base0 < -width)
                    break;

                shift = _mm_add_epi16(shift, shift_inc);
                __m128i diff0 = loadu_si128(pdiff + base0);
                __m128i diff1 = loadu_si128(pdiff + base1);
                __m128i res0 = _mm_mullo_epi16(diff0, _mm_srli_epi16(_mm_and_si128(shift, shift_mask), 1));
                shift = _mm_add_epi16(shift, shift_inc);
                __m128i res1 = _mm_mullo_epi16(diff1, _mm_srli_epi16(_mm_and_si128(shift, shift_mask), 1));

                res0 = _mm_srli_epi16(_mm_add_epi16(res0, loadu_si128(pby32 + base0)), 5);
                res1 = _mm_srli_epi16(_mm_add_epi16(res1, loadu_si128(pby32 + base1)), 5);
                res0 = _mm_packus_epi16(res0, res1);

                if (blend) {
                    __m128i m = loadu2_epi64(z2_blend_mask + 65 + base0, z2_blend_mask + 65 + base1);
                    __m128i l = loadu2_epi64(dst, dst + pitch);
                    res0 = _mm_blendv_epi8(l, res0, m);
                }

                storel_epi64(dst, res0);
                storeh_epi64(dst + pitch, res0);
            }
        }
        template <bool blend> void predict_16x16_avx2(const uint8_t* refPel, uint8_t* dst, int pitch, int dx)
        {
            const int width = 16;

            // topPels[i + 1] - topPels[i]
            alignas(32) int16_t diffBuf[2 * width];
            __m128i z = _mm_setzero_si128();
            __m128i a0 = loada_si128(refPel + 0);
            __m128i a0_lo = _mm_unpacklo_epi8(a0, z);
            __m128i a0_hi = _mm_unpackhi_epi8(a0, z);
            __m128i a1 = loadu_si128(refPel + 1);
            storea_si128(diffBuf + 0, z);
            storea_si128(diffBuf + 8, z);
            diffBuf[width - 1] = refPel[0] - refPel[-1];
            storea_si128(diffBuf + 16, _mm_sub_epi16(_mm_unpacklo_epi8(a1, z), a0_lo));
            storea_si128(diffBuf + 24, _mm_sub_epi16(_mm_unpackhi_epi8(a1, z), a0_hi));

            // topPels[i + 1] * 32 + 16
            alignas(32) int16_t by32Buf[2 * width];
            storea_si128(by32Buf + 0, z);
            storea_si128(by32Buf + 8, z);
            by32Buf[width - 1] = (refPel[-1] << 5) + 16;
            storea_si128(by32Buf + 16, _mm_add_epi16(_mm_slli_epi16(a0_lo, 5), _mm_set1_epi16(16)));
            storea_si128(by32Buf + 24, _mm_add_epi16(_mm_slli_epi16(a0_hi, 5), _mm_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            const int dx2 = dx * 2;
            const __m256i shift_inc = _mm256_set1_epi16(-dx);
            const __m256i shift_mask = _mm256_set1_epi16(0x3F);
            __m256i shift = _mm256_setzero_si256();
            for (int r = 0, x = -dx; r < width; r += 2, x -= dx2, dst += 2 * pitch) {
                const int base0 = x >> 6;
                const int base1 = (x - dx) >> 6;
                if (base0 < -width)
                    break;

                shift = _mm256_add_epi16(shift, shift_inc);
                __m256i diff0 = loadu_si256(pdiff + base0);
                __m256i diff1 = loadu_si256(pdiff + base1);
                __m256i res0 = _mm256_mullo_epi16(diff0, _mm256_srli_epi16(_mm256_and_si256(shift, shift_mask), 1));
                shift = _mm256_add_epi16(shift, shift_inc);
                __m256i res1 = _mm256_mullo_epi16(diff1, _mm256_srli_epi16(_mm256_and_si256(shift, shift_mask), 1));

                res0 = _mm256_srli_epi16(_mm256_add_epi16(res0, loadu_si256(pby32 + base0)), 5);
                res1 = _mm256_srli_epi16(_mm256_add_epi16(res1, loadu_si256(pby32 + base1)), 5);
                res0 = permute4x64(_mm256_packus_epi16(res0, res1), 0, 2, 1, 3);

                if (blend) {
                    const __m256i m = loadu2_m128i(z2_blend_mask + 65 + base0, z2_blend_mask + 65 + base1);
                    res0 = _mm256_blendv_epi8(loada2_m128i(dst, dst + pitch), res0, m);
                }

                storea2_m128i(dst, dst + pitch, res0);
            }
        }
        template <bool blend> void predict_32x32_avx2(const uint8_t* refPel, uint8_t* dst, int pitch, int dx)
        {
            const int width = 32;

            // topPels[i + 1] - topPels[i]
            alignas(32) int16_t diffBuf[2 * width];
            __m128i z = _mm_setzero_si128();
            __m128i a0 = loada_si128(refPel + 0);
            __m128i a16 = loada_si128(refPel + 16);
            __m128i a0_lo = _mm_unpacklo_epi8(a0, z);
            __m128i a0_hi = _mm_unpackhi_epi8(a0, z);
            __m128i a16_lo = _mm_unpacklo_epi8(a16, z);
            __m128i a16_hi = _mm_unpackhi_epi8(a16, z);
            __m128i a1 = loadu_si128(refPel + 1);
            __m128i a17 = loadu_si128(refPel + 17);
            storea_si128(diffBuf + 0, z);
            storea_si128(diffBuf + 8, z);
            storea_si128(diffBuf + 16, z);
            storea_si128(diffBuf + 24, z);
            diffBuf[width - 1] = refPel[0] - refPel[-1];
            storea_si128(diffBuf + 32, _mm_sub_epi16(_mm_unpacklo_epi8(a1, z), a0_lo));
            storea_si128(diffBuf + 40, _mm_sub_epi16(_mm_unpackhi_epi8(a1, z), a0_hi));
            storea_si128(diffBuf + 48, _mm_sub_epi16(_mm_unpacklo_epi8(a17, z), a16_lo));
            storea_si128(diffBuf + 56, _mm_sub_epi16(_mm_unpackhi_epi8(a17, z), a16_hi));

            // topPels[i + 1] * 32 + 16
            alignas(32) int16_t by32Buf[2 * width];
            storea_si128(by32Buf + 0, z);
            storea_si128(by32Buf + 8, z);
            storea_si128(by32Buf + 16, z);
            storea_si128(by32Buf + 24, z);
            by32Buf[width - 1] = (refPel[-1] << 5) + 16;
            storea_si128(by32Buf + 32, _mm_add_epi16(_mm_slli_epi16(a0_lo, 5), _mm_set1_epi16(16)));
            storea_si128(by32Buf + 40, _mm_add_epi16(_mm_slli_epi16(a0_hi, 5), _mm_set1_epi16(16)));
            storea_si128(by32Buf + 48, _mm_add_epi16(_mm_slli_epi16(a16_lo, 5), _mm_set1_epi16(16)));
            storea_si128(by32Buf + 56, _mm_add_epi16(_mm_slli_epi16(a16_hi, 5), _mm_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            int x = -dx;
            __m256i shift_inc = _mm256_set1_epi16(-dx);
            __m256i shift_mask = _mm256_set1_epi16(0x3F);
            __m256i shift = _mm256_setzero_si256();
            for (int r = 0; r < width; r++, x -= dx, dst += pitch) {
                const int base = x >> 6;
                if (base < -width)
                    break;

                shift = _mm256_add_epi16(shift, shift_inc);
                __m256i diff0 = loadu_si256(pdiff + base);
                __m256i diff1 = loadu_si256(pdiff + base + 16);
                __m256i sh = _mm256_srli_epi16(_mm256_and_si256(shift, shift_mask), 1);
                __m256i res0 = _mm256_mullo_epi16(diff0, sh);
                __m256i res1 = _mm256_mullo_epi16(diff1, sh);

                res0 = _mm256_srli_epi16(_mm256_add_epi16(res0, loadu_si256(pby32 + base)), 5);
                res1 = _mm256_srli_epi16(_mm256_add_epi16(res1, loadu_si256(pby32 + base + 16)), 5);
                res0 = permute4x64(_mm256_packus_epi16(res0, res1), 0, 2, 1, 3);

                if (blend) {
                    const __m256i m = loadu_si256(z2_blend_mask + 65 + base);
                    res0 = _mm256_blendv_epi8(loada_si256(dst), res0, m);
                }
                storea_si256(dst, res0);
            }
        }

        ALIGN_DECL(16) const uint8_t shuftab_trans4[16] = {0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15};
        template <int txSize> void transpose(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst) {
            const int width = 4 << txSize;
            for (int j = 0; j < width; j += 8)
                for (int i = 0; i < width; i += 8)
                    transpose<TX_8X8>(src + i * pitchSrc + j, pitchSrc, dst + j * pitchDst + i, pitchDst);
        }
        template <> void transpose<TX_4X4>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst) {
            pitchSrc;
            assert(pitchSrc == 4);
            __m128i s = loada_si128(src);                           // 01234567 89abcdef
            s = _mm_shuffle_epi8(s, loada_si128(shuftab_trans4));   // 048c159d 26ae37bf
            storel_si32(dst, s);
            storel_si32(dst + pitchDst, _mm_bsrli_si128(s, 4));
            storel_si32(dst + pitchDst * 2, _mm_bsrli_si128(s, 8));
            storel_si32(dst + pitchDst * 3, _mm_bsrli_si128(s, 12));
        }
        template <> void transpose<TX_8X8>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst) {
            __m128i r0, r1, r2, r3, r4, r5, r6, r7;
            r0 = loadl_epi64(src + 0 * pitchSrc);   // 07,06,05,04,03,02,01,00
            r1 = loadl_epi64(src + 1 * pitchSrc);   // 17,16,15,14,13,12,11,10
            r2 = loadl_epi64(src + 2 * pitchSrc);   // 27,26,25,24,23,22,21,20
            r3 = loadl_epi64(src + 3 * pitchSrc);   // 37,36,35,34,33,32,31,30
            r4 = loadl_epi64(src + 4 * pitchSrc);   // 47,46,45,44,43,42,41,40
            r5 = loadl_epi64(src + 5 * pitchSrc);   // 57,56,55,54,53,52,51,50
            r6 = loadl_epi64(src + 6 * pitchSrc);   // 67,66,65,64,63,62,61,60
            r7 = loadl_epi64(src + 7 * pitchSrc);   // 77,76,75,74,73,72,71,70

            r0 = _mm_unpacklo_epi8(r0, r1);
            r2 = _mm_unpacklo_epi8(r2, r3);
            r4 = _mm_unpacklo_epi8(r4, r5);
            r6 = _mm_unpacklo_epi8(r6, r7);

            r1 = r0;
            r0 = _mm_unpacklo_epi16(r0, r2);
            r1 = _mm_unpackhi_epi16(r1, r2);
            r5 = r4;
            r4 = _mm_unpacklo_epi16(r4, r6);
            r5 = _mm_unpackhi_epi16(r5, r6);
            r2 = r0;
            r0 = _mm_unpacklo_epi32(r0, r4);
            r2 = _mm_unpackhi_epi32(r2, r4);
            r3 = r1;
            r1 = _mm_unpacklo_epi32(r1, r5);
            r3 = _mm_unpackhi_epi32(r3, r5);

            storel_epi64(dst + 0 * pitchDst, r0);
            storeh_epi64(dst + 1 * pitchDst, r0);
            storel_epi64(dst + 2 * pitchDst, r2);
            storeh_epi64(dst + 3 * pitchDst, r2);
            storel_epi64(dst + 4 * pitchDst, r1);
            storeh_epi64(dst + 5 * pitchDst, r1);
            storel_epi64(dst + 6 * pitchDst, r3);
            storeh_epi64(dst + 7 * pitchDst, r3);
        }

        template <int txSize> void predict_intra_z2_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int dx, int dy);
        template <> void predict_intra_z2_av1_avx2<TX_4X4>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int dx, int dy)
        {
            __m128i out;
            predict_4x4_sse4<false>(leftPels, &out, dy);
            out = _mm_shuffle_epi8(out, loada_si128(shuftab_trans4));   // transpose
            predict_4x4_sse4<true>(topPels, &out, dx);
            storel_si32(dst + 0 * pitch, out);
            storel_si32(dst + 1 * pitch, _mm_srli_si128(out, 4));
            storel_si32(dst + 2 * pitch, _mm_srli_si128(out, 8));
            storel_si32(dst + 3 * pitch, _mm_srli_si128(out, 12));
        }
        template <> void predict_intra_z2_av1_avx2<TX_8X8>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int dx, int dy)
        {
            predict_8x8_sse4<false>(leftPels, dst, pitch, dy);
            transpose<TX_8X8>(dst, pitch, dst, pitch);
            predict_8x8_sse4<true>(topPels, dst, pitch, dx);
        }
        template <> void predict_intra_z2_av1_avx2<TX_16X16>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int dx, int dy)
        {
            const int width = 16;
            alignas(32) uint8_t tmp[width * width];

            predict_16x16_avx2<false>(leftPels, tmp, width, dy);
            transpose<TX_16X16>(tmp, width, dst, pitch);
            predict_16x16_avx2<true>(topPels, dst, pitch, dx);
        }
        template <> void predict_intra_z2_av1_avx2<TX_32X32>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int dx, int dy)
        {
            const int width = 32;
            alignas(32) uint8_t tmp[width * width];

            predict_32x32_avx2<false>(leftPels, tmp, width, dy);
            transpose<TX_32X32>(tmp, width, dst, pitch);
            predict_32x32_avx2<true>(topPels, dst, pitch, dx);
        }

        template <int txSize> void predict_intra_z3_av1_avx2(const uint8_t *leftPels, uint8_t *dst, int pitch, int upLeft, int dy)
        {
            const int width = 4 << txSize;
            alignas(32) uint8_t dstT[width * width];
            predict_intra_z1_av1_avx2<txSize>(leftPels, dstT, width, upLeft, dy);
            transpose<txSize>(dstT, width, dst, pitch);
        }
    };

    template <int size, int mode, typename PixType>
    void predict_intra_av1_avx2(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int delta, int upTop, int upLeft) {
        if      (mode ==  9) details::predict_intra_smooth_av1_avx2<size>(topPels, leftPels, dst, pitch);
        else if (mode == 10) details::predict_intra_smooth_v_av1_avx2<size>(topPels, leftPels, dst, pitch);
        else if (mode == 11) details::predict_intra_smooth_h_av1_avx2<size>(topPels, leftPels, dst, pitch);
        else if (mode == 12) details::predict_intra_paeth_av1_avx2 <size>(topPels, leftPels, dst, pitch);
        else {
            assert(upTop == 0 && upLeft == 0);
            assert(mode >= 1 && mode <= 8);
            const int angle = mode_to_angle_map[mode] + delta * 3;

            if (angle > 0 && angle < 90) {
                const int dx = dr_intra_derivative[angle];
                details::predict_intra_z1_av1_avx2<size>(topPels, dst, pitch, upTop, dx);
            } else if (angle > 90  && angle < 180) {
                const int dx = dr_intra_derivative[180 - angle];
                const int dy = dr_intra_derivative[angle - 90];
                details::predict_intra_z2_av1_avx2<size>(topPels, leftPels, dst, pitch, dx, dy);
            } else if (angle > 180 && angle < 270) {
                const int dy = dr_intra_derivative[270 - angle];
                details::predict_intra_z3_av1_avx2<size>(leftPels, dst, pitch, upLeft, dy);
            } else if (angle == 90) {
                details::predict_intra_ver_av1_avx2<size>(topPels, dst, pitch);
            } else if (angle == 180) {
                details::predict_intra_hor_av1_avx2<size>(leftPels, dst, pitch);
            }
        }
    }

    template void predict_intra_av1_avx2<0, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 1,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 2,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 3,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 4,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 5,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 6,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 7,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 8,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 9,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3,10,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3,11,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3,12,uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template <int size, int mode, typename PixType>
    void predict_intra_nv12_av1_avx2(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int delta, int upTop, int upLeft)
    {
        const int width = 4 << size;
        assert(size_t(topPels)  % 32 == 0);
        assert(size_t(leftPels) % 32 == 0);
        IntraPredPels<uint8_t> predPelsU;
        IntraPredPels<uint8_t> predPelsV;

        convert_ref_pels_av1<size>(topPels,  predPelsU.top,  predPelsV.top,  upTop);
        convert_ref_pels_av1<size>(leftPels, predPelsU.left, predPelsV.left, upLeft);
        predict_intra_av1_avx2<size, mode>(predPelsU.top, predPelsU.left, dst,         pitch, delta, upTop, upLeft);
        predict_intra_av1_avx2<size, mode>(predPelsV.top, predPelsV.left, dst + width, pitch, delta, upTop, upLeft);
        convert_pred_block<size>(dst, pitch);
    }
    template void predict_intra_nv12_av1_avx2<TX_4X4,   V_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   V_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, V_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, V_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   H_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   H_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, H_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, H_PRED,        uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D45_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D45_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D45_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D45_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D63_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D63_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D63_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D63_PRED,      uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D117_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D117_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D117_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D117_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D135_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D135_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D135_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D135_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D153_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D153_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D153_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D153_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D207_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D207_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D207_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D207_PRED,     uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   PAETH_PRED,    uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   PAETH_PRED,    uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, PAETH_PRED,    uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, PAETH_PRED,    uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_PRED,   uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_PRED,   uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_PRED,   uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_PRED,   uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_V_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_V_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_V_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_V_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_H_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_H_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_H_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_H_PRED, uint8_t>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);


    template <int size> void predict_intra_palette_avx2(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map);

    template <> void predict_intra_palette_avx2<0>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map)
    {
        const __m128i plt = loaddup_epi64(palette);
        __m128i map = _mm_setr_epi32(
            *(int *)(color_map + 0 * 64),
            *(int *)(color_map + 1 * 64),
            *(int *)(color_map + 2 * 64),
            *(int *)(color_map + 3 * 64));
        __m128i pred = _mm_shuffle_epi8(plt, map);
        storel_si32(dst + 0 * pitch, pred);
        storel_si32(dst + 1 * pitch, _mm_srli_si128(pred, 4));
        storel_si32(dst + 2 * pitch, _mm_srli_si128(pred, 8));
        storel_si32(dst + 3 * pitch, _mm_srli_si128(pred, 12));
    }

    template <> void predict_intra_palette_avx2<1>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map)
    {
        const __m256i plt = _mm256_broadcastsi128_si256(loaddup_epi64(palette));
        __m256i map1 = _mm256_setr_m128i(
            loadu2_epi64(color_map + 0 * 64, color_map + 1 * 64),
            loadu2_epi64(color_map + 2 * 64, color_map + 3 * 64));
        __m256i map2 = _mm256_setr_m128i(
            loadu2_epi64(color_map + 4 * 64, color_map + 5 * 64),
            loadu2_epi64(color_map + 6 * 64, color_map + 7 * 64));
        __m256i pred1 = _mm256_shuffle_epi8(plt, map1);
        __m256i pred2 = _mm256_shuffle_epi8(plt, map2);
        storel_epi64(dst + 0 * pitch, si128_lo(pred1));
        storeh_epi64(dst + 1 * pitch, si128_lo(pred1));
        storel_epi64(dst + 2 * pitch, si128_hi(pred1));
        storeh_epi64(dst + 3 * pitch, si128_hi(pred1));
        storel_epi64(dst + 4 * pitch, si128_lo(pred2));
        storeh_epi64(dst + 5 * pitch, si128_lo(pred2));
        storel_epi64(dst + 6 * pitch, si128_hi(pred2));
        storeh_epi64(dst + 7 * pitch, si128_hi(pred2));
    }

    template <> void predict_intra_palette_avx2<2>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map)
    {
        const __m256i plt = _mm256_broadcastsi128_si256(loaddup_epi64(palette));
        const int32_t pitch2 = pitch * 2;
        __m256i map, pred;
        for (int32_t i = 0; i < 16; i += 2, dst += pitch2) {
            map = loada2_m128i(color_map + i * 64, color_map + i * 64 + 64);
            pred = _mm256_shuffle_epi8(plt, map);
            storea2_m128i(dst, dst + pitch, pred);
        }
    }

    template <> void predict_intra_palette_avx2<3>(uint8_t *dst, int32_t pitch, const uint8_t *palette, const uint8_t *color_map)
    {
        const __m256i plt = _mm256_broadcastsi128_si256(loaddup_epi64(palette));
        __m256i map, pred;
        for (int32_t i = 0; i < 32; i++, dst += pitch) {
            map = loada_si256(color_map + i * 64);
            pred = _mm256_shuffle_epi8(plt, map);
            storea_si256(dst, pred);
        }
    }

};  // namespace AV1PP
