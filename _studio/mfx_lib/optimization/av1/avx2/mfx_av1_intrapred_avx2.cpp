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
#include "mfx_av1_get_intra_pred_pels.h"

namespace AV1PP
{
    namespace {
        enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4 };
        enum { DC_PRED=0, V_PRED=1, H_PRED=2, D45_PRED=3, D135_PRED=4,
               D117_PRED=5, D153_PRED=6, D207_PRED=7, D63_PRED=8, TM_PRED=9,
               SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12 };

        inline AV1_FORCEINLINE __m128i filt_121(__m128i x, __m128i y, __m128i z) {
            __m128i res = _mm_avg_epu8(x, z);
            __m128i tmp = _mm_xor_si128(x, z);
            tmp = _mm_and_si128(tmp, _mm_set1_epi8(1));
            res = _mm_sub_epi8(res, tmp);
            return _mm_avg_epu8(res, y);
        }
        inline AV1_FORCEINLINE __m256i filt_121(__m256i x, __m256i y, __m256i z) {
            __m256i res = _mm256_avg_epu8(x, z);
            __m256i tmp = _mm256_xor_si256(x, z);
            tmp = _mm256_and_si256(tmp, _mm256_set1_epi8(1));
            res = _mm256_sub_epi8(res, tmp);
            return _mm256_avg_epu8(res, y);
        }

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

        ALIGN_DECL(32) const uint8_t shuftab_reverse[32] = {15,14,13,12, 11,10,9,8, 7,6,5,4, 3,2,1,0, 15,14,13,12, 11,10,9,8, 7,6,5,4, 3,2,1,0};
        ALIGN_DECL(16) const uint8_t shuftab_d153_4_load[16] = {12,11,10,9, 0,1,2,3, };
        ALIGN_DECL(16) const uint8_t shuftab_d153_4_final[16] = {0,1,2,3, 2,3,4,5, 4,5,6,7, 6,7,9,11};
        ALIGN_DECL(16) const uint8_t shuftab_d153_8_x[16] = {7,6,5,4, 3,2,1,0, 10,11,12,13, 14,15,15,15};
        ALIGN_DECL(16) const uint8_t shuftab_d153_8_y[16] = {6,5,4,3, 2,1,0,8, 9,10,11,12, 13,14,15,15};
        ALIGN_DECL(16) const uint8_t shuftab_d153_8_z[16] = {5,4,3,2, 1,0,8,9, 8,9,10,11, 12,13,14,15};
        ALIGN_DECL(16) const uint8_t shuftab_d207_4[16] = {0,1,1,2, 2,3,3,3, 3,3,3,3, 3,3,3,3 };
//        ALIGN_DECL(16) const uint8_t shuftab_av1_d207_4[16] = {0,1,1,2, 2,3,3,4, 4,5,5,6, 6,7,7,7 };
        ALIGN_DECL(32) const uint8_t shuftab_d117_32[32] = {0,2,4,6, 8,10,12,14, 1,3,5,7, 9,11,13,15, 0,2,4,6, 8,10,12,14, 1,3,5,7, 9,11,13,15};
        ALIGN_DECL(16) const uint8_t shuftab_d117_8_load[16] = {9,0,1,2, 3,4,5,6, 0,0,9,10, 11,12,13,0};
        ALIGN_DECL(16) const uint8_t shuftab_d117_8_sep[16] = {0,0,0,0, 0,0,0,0, 0,0,14,12, 10,13,11,9};
        ALIGN_DECL(16) const uint8_t shuftab_d135_8_load[16] = {0,1,2,3, 4,5,6,7, 15,14,13,12, 11,10,9,8};
        ALIGN_DECL(16) const uint8_t shuftab_d135_4_x[16] = {9,0,1,2, 0,0,0,0, 0,0,9,10, 11,0,0,0};
        ALIGN_DECL(16) const uint8_t shuftab_d135_4_final[16] = {0,1,2,3, 9,0,1,2, 10,9,0,1, 11,10,9,0};
        ALIGN_DECL(16) const uint8_t shuftab_d45_4_final[16] = {0,1,2,3, 1,2,3,4, 2,3,4,5, 3,4,5,7};
        ALIGN_DECL(32) const uint8_t shuftab_nv12_to_yv12[32] = { 0,2,4,6,8,10,12,14, 1,3,5,7,9,11,13,15, 0,2,4,6,8,10,12,14, 1,3,5,7,9,11,13,15 };
        ALIGN_DECL(32) const uint8_t shuftab_nv12_to_yv12_w4[32] = { 0,2,4,6, 1,3,5,7, 8,10,12,14, 9,11,13,15, 0,2,4,6, 1,3,5,7, 8,10,12,14, 9,11,13,15 };
        ALIGN_DECL(32) const uint8_t shuftab_paeth_8[32] = { 0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3 };

        ALIGN_DECL(16) const uint8_t smooth_weights_4[4] = { 255, 149, 85, 64 };
        ALIGN_DECL(16) const uint8_t smooth_weights_8[8] = { 255, 197, 146, 105, 73, 50, 37, 32 };
        ALIGN_DECL(32) const uint8_t smooth_weights_16[32] = {
            255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16,
            255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16
        };
        ALIGN_DECL(32) const uint8_t smooth_weights_32[32] = {
            255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92,  83,  74,
            66,  59,  52,  45,  39,  34,  29,  25,  21,  17,  14,  12,  10,  9,   8,   8
        };
    };

    template <int size, int haveLeft, int haveAbove> void predict_intra_dc_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int);
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   0, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        topPels; leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3;
        int dc = 0x80808080;
        *(int*)(dst) = dc; *(int*)(dst+p) = dc; *(int*)(dst+p2) = dc; *(int*)(dst+p3) = dc;
    }
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   0, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        topPels; leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        uint64_t dc = 0x8080808080808080;
        *(uint64_t*)(dst) = dc; *(uint64_t*)(dst+p) = dc; *(uint64_t*)(dst+p2) = dc; *(uint64_t*)(dst+p3) = dc; dst += p4;
        *(uint64_t*)(dst) = dc; *(uint64_t*)(dst+p) = dc; *(uint64_t*)(dst+p2) = dc; *(uint64_t*)(dst+p3) = dc;
    }
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 0, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        topPels; leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3, p4 = pitch*4;
        __m256i dc = _mm256_set1_epi8(128);
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc); dst += p4;
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc); dst += p4;
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc); dst += p4;
        storeu2_m128i(dst, dst+p, dc); storeu2_m128i(dst+p2, dst+p3, dc);
    }
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 0, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   0, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        leftPels;
        int p = pitch, p2 = pitch*2, p3 = pitch*3;
        __m128i above = _mm_cvtsi32_si128(*(int*)(topPels));
        __m128i sad = _mm_sad_epu8(above, _mm_setzero_si128());
        __m128i sum = _mm_add_epi16(sad, _mm_set1_epi16(2));
        sum = _mm_srli_epi16(sum, 2);
        int dc = _mm_cvtsi128_si32(_mm_broadcastb_epi8(sum));
        *(int*)(dst) = dc; *(int*)(dst+p) = dc; *(int*)(dst+p2) = dc; *(int*)(dst+p3) = dc;
    }
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   0, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 0, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 0, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   1, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<0, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   1, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<1, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 1, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<2, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 1, 0>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
        predict_intra_dc_av1_avx2<3, 0, 1>(leftPels, topPels, dst, pitch, 0, 0, 0);
    }
    template <> void predict_intra_dc_av1_avx2<TX_4X4,   1, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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
    template <> void predict_intra_dc_av1_avx2<TX_8X8,   1, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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
    template <> void predict_intra_dc_av1_avx2<TX_16X16, 1, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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
    template <> void predict_intra_dc_av1_avx2<TX_32X32, 1, 1>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int, int, int) {
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

    template <int size, int haveLeft, int haveAbove> void predict_intra_dc_vp9_avx2(const uint8_t *predPel, uint8_t *dst, int pitch) {
        predict_intra_dc_av1_avx2<size, haveLeft, haveAbove>(predPel + 1, predPel + 1 + (8 << size), dst, pitch, 0, 0, 0);
    }
    template void predict_intra_dc_vp9_avx2<TX_4X4,   0, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_4X4,   0, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_4X4,   1, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_4X4,   1, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_8X8,   0, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_8X8,   0, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_8X8,   1, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_8X8,   1, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_16X16, 0, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_16X16, 0, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_16X16, 1, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_16X16, 1, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_32X32, 0, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_32X32, 0, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_32X32, 1, 0>(const uint8_t *predPel, uint8_t *dst, int pitch);
    template void predict_intra_dc_vp9_avx2<TX_32X32, 1, 1>(const uint8_t *predPel, uint8_t *dst, int pitch);

    template <int size, int mode> void predict_intra_vp9_avx2(const uint8_t *refPel, uint8_t *dst, int pitch);

    template <> void predict_intra_vp9_avx2<TX_4X4, V_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        int p = *(int*)(refPel+1);
        *(int*)(dst) = p; *(int*)(dst+pitch) = p; dst += pitch*2;
        *(int*)(dst) = p; *(int*)(dst+pitch) = p;
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, V_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        uint64_t p = *(uint64_t*)(refPel+1);
        *(uint64_t*)(dst) = p; *(uint64_t*)(dst+pitch) = p; dst += pitch*2;
        *(uint64_t*)(dst) = p; *(uint64_t*)(dst+pitch) = p; dst += pitch*2;
        *(uint64_t*)(dst) = p; *(uint64_t*)(dst+pitch) = p; dst += pitch*2;
        *(uint64_t*)(dst) = p; *(uint64_t*)(dst+pitch) = p;
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, V_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m128i p = loada_si128(refPel+1);
        storea_si128(dst, p); storea_si128(dst+pitch, p); dst += 2*pitch;
        storea_si128(dst, p); storea_si128(dst+pitch, p); dst += 2*pitch;
        storea_si128(dst, p); storea_si128(dst+pitch, p); dst += 2*pitch;
        storea_si128(dst, p); storea_si128(dst+pitch, p); dst += 2*pitch;
        storea_si128(dst, p); storea_si128(dst+pitch, p); dst += 2*pitch;
        storea_si128(dst, p); storea_si128(dst+pitch, p); dst += 2*pitch;
        storea_si128(dst, p); storea_si128(dst+pitch, p); dst += 2*pitch;
        storea_si128(dst, p); storea_si128(dst+pitch, p);
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, V_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m256i p = loadu_si256(refPel+1);
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

    template <> void predict_intra_vp9_avx2<TX_4X4, H_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m128i l = _mm_cvtsi32_si128(*(int *)(refPel+1+2*4));    // L0 L1 L2 L3 ** ** ** ** ** ** ** ** ** ** ** **
        l = _mm_unpacklo_epi8(l, l);                                // L0 L0 L1 L1 L2 L2 L3 L3 ** ** ** ** ** ** ** **
        l = _mm_unpacklo_epi8(l, l);                                // L0 L0 L0 L0 L1 L1 L1 L1 L2 L2 L2 L2 L3 L3 L3 L3
        int p = pitch;
        *(int*)(dst)     = _mm_cvtsi128_si32(l);
        *(int*)(dst+p)   = _mm_cvtsi128_si32(_mm_bsrli_si128(l,4));
        *(int*)(dst+p*2) = _mm_cvtsi128_si32(_mm_bsrli_si128(l,8));
        *(int*)(dst+p*3) = _mm_cvtsi128_si32(_mm_bsrli_si128(l,12));
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, H_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        int p = pitch, p2 = pitch*2;
        __m128i l = loadl_epi64(refPel+1+2*8);                      // L0 L1 L2 L3 L4 L5 L6 L7 ** ** ** ** ** ** ** **
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
    template <> void predict_intra_vp9_avx2<TX_16X16, H_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m256i l = si256(loada_si128(refPel + 1 + 2*16));  // 0123456789abcdef ****************
        l = _mm256_permute4x64_epi64(l, PERM4x64(0,0,1,1)); // 0123456701234567 89abcdrf89abcdrf
        l = _mm256_unpacklo_epi8(l,l);                      // 0011223344556677 8899aabbccddeeff
        __m256i lo = _mm256_unpacklo_epi16(l,l);            // 0000111122223333 88889999aaaabbbb
        __m256i hi = _mm256_unpackhi_epi16(l,l);            // 4444555566667777 ccccddddeeeeffff
        __m256i lolo = _mm256_unpacklo_epi32(lo,lo);        // 0000000011111111 8888888899999999
        __m256i lohi = _mm256_unpackhi_epi32(lo,lo);        // 2222222233333333 aaaaaaaabbbbbbbb
        __m256i hilo = _mm256_unpacklo_epi32(hi,hi);        // 4444444455555555 ccccccccdddddddd
        __m256i hihi = _mm256_unpackhi_epi32(hi,hi);        // 6666666677777777 eeeeeeeeffffffff

        int p = pitch;
        int p8 = pitch*8;
        storeu2_m128i(dst, dst+p8, _mm256_unpacklo_epi64(lolo, lolo)); dst+=p;  // 0000000000000000 8888888888888888
        storeu2_m128i(dst, dst+p8, _mm256_unpackhi_epi64(lolo, lolo)); dst+=p;  // 1111111111111111 9999999999999999
        storeu2_m128i(dst, dst+p8, _mm256_unpacklo_epi64(lohi, lohi)); dst+=p;  // 2222222222222222 aaaaaaaaaaaaaaaa
        storeu2_m128i(dst, dst+p8, _mm256_unpackhi_epi64(lohi, lohi)); dst+=p;  // 3333333333333333 bbbbbbbbbbbbbbbb
        storeu2_m128i(dst, dst+p8, _mm256_unpacklo_epi64(hilo, hilo)); dst+=p;  // 4444444444444444 cccccccccccccccc
        storeu2_m128i(dst, dst+p8, _mm256_unpackhi_epi64(hilo, hilo)); dst+=p;  // 5555555555555555 dddddddddddddddd
        storeu2_m128i(dst, dst+p8, _mm256_unpacklo_epi64(hihi, hihi)); dst+=p;  // 6666666666666666 eeeeeeeeeeeeeeee
        storeu2_m128i(dst, dst+p8, _mm256_unpackhi_epi64(hihi, hihi));          // 7777777777777777 ffffffffffffffff
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, H_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m256i l = loada_si256(refPel + 1 + 2*32);             // 0123456789abcdef ghiklmnopqrstuvw
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

    template <> void predict_intra_vp9_avx2<TX_4X4, D45_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        __m128i x = loada_si128(pa);                            // 01234567********
        __m128i f = _mm_broadcastb_epi8(_mm_bsrli_si128(x,7));  // 7777777777777777
        x = _mm_unpacklo_epi64(x,f);                            // 0123456777777777
        __m128i y = _mm_bsrli_si128(x, 1);                      // 1234567777777777
        __m128i z = _mm_bsrli_si128(x, 2);                      // 2345677777777777

        __m128i row = filt_121(x, y, z);                        // 0123456777777777 // first row (filtered above pels)
        __m128i shuftab = loada_si128(shuftab_d45_4_final);
        __m128i pred = _mm_shuffle_epi8(row, shuftab);          // 0123 1234 2345 3457

        int p = pitch;
        *(int*)(dst)     = _mm_cvtsi128_si32(pred);
        *(int*)(dst+p)   = _mm_cvtsi128_si32(_mm_bsrli_si128(pred,4));
        *(int*)(dst+p*2) = _mm_cvtsi128_si32(_mm_bsrli_si128(pred,8));
        *(int*)(dst+p*3) = _mm_cvtsi128_si32(_mm_bsrli_si128(pred,12));
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, D45_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        __m128i x = loada_si128(pa);                            // 01234567********
        __m128i f = _mm_broadcastb_epi8(_mm_bsrli_si128(x,7));  // 7777777777777777
        x = _mm_unpacklo_epi64(x,f);                            // 0123456777777777
        __m128i y = _mm_bsrli_si128(x, 1);                      // 1234567777777777
        __m128i z = _mm_bsrli_si128(x, 2);                      // 2345677777777777

        __m128i row = filt_121(x, y, z);                        // 0123456777777777 // first row (filtered above pels)

        int p = pitch;
        storel_epi64(dst, row); dst+=p;                         // 01234567
        storel_epi64(dst, _mm_bsrli_si128(row,1)); dst+=p;      // 12345677
        storel_epi64(dst, _mm_bsrli_si128(row,2)); dst+=p;      // 23456777
        storel_epi64(dst, _mm_bsrli_si128(row,3)); dst+=p;      // 34567777
        storel_epi64(dst, _mm_bsrli_si128(row,4)); dst+=p;      // 45677777
        storel_epi64(dst, _mm_bsrli_si128(row,5)); dst+=p;      // 56777777
        storel_epi64(dst, _mm_bsrli_si128(row,6)); dst+=p;      // 67777777
        storel_epi64(dst, f);                                   // 77777777
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, D45_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        __m128i x = loada_si128(pa);                            // 0123456789abcdef
        __m128i f = _mm_broadcastb_epi8(_mm_bsrli_si128(x,15)); // ffffffffffffffff
        __m128i y = _mm_alignr_epi8(f, x, 1);                   // 123456789abcdeff
        __m128i z = _mm_alignr_epi8(f, x, 2);                   // 23456789abcdefff

        __m128i row0 = filt_121(x, y, z);                       // 0123456789abcdef // first row (filtered above pels)
        __m128i row1 = _mm_alignr_epi8(f,row0,1);               // 123456789abcdeff

        __m256i row = _mm256_setr_m128i(row0, row1);           // 0123456789abcdef 123456789abcdeff
        __m256i fff = _mm256_broadcastsi128_si256(f);          // ffffffffffffffff ffffffffffffffff

        int p = pitch;
        int p2 = pitch*2;

        storeu2_m128i(dst, dst+p, row); dst += p2;                              // 0123456789abcdef 123456789abcdeff
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(fff,row,2)); dst += p2;    // 23456789abcdefff 3456789abcdeffff
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(fff,row,4)); dst += p2;    // 456789abcdefffff 56789abcdeffffff
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(fff,row,6)); dst += p2;    // 6789abcdefffffff 789abcdeffffffff
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(fff,row,8)); dst += p2;    // 89abcdefffffffff 9abcdeffffffffff
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(fff,row,10)); dst += p2;   // abcdefffffffffff bcdeffffffffffff
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(fff,row,12)); dst += p2;   // cdefffffffffffff deffffffffffffff
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(fff,row,14));              // efffffffffffffff ffffffffffffffff
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, D45_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        __m256i right = _mm256_set1_epi8(pa[31]);
        __m256i above0 = loadu_si256(pa);
        __m256i tmp = _mm256_permute2x128_si256(right, above0, PERM2x128(3,0)); // construct register for _mm256_alignr_epi8
        __m256i above1 = _mm256_alignr_epi8(tmp, above0, 1);
        __m256i above2 = _mm256_alignr_epi8(tmp, above0, 2);

        __m256i row = filt_121(above0, above1, above2);

        storea_si256(dst, row); dst += pitch;
        tmp = _mm256_permute2x128_si256(right, row, PERM2x128(3,0));
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 1)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 2)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 3)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 5)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 7)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 9)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,11)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,13)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,14)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,15)); dst += pitch;
        storea_si256(dst, tmp); dst += pitch;
        row = tmp;
        tmp = _mm256_permute2x128_si256(right, row, PERM2x128(3,0));
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 1)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 2)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 3)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 5)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 7)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row, 9)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,11)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,13)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,14)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(tmp, row,15));
    }

    template <> void predict_intra_vp9_avx2<TX_4X4, D135_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m128i y = loadu_si128(refPel);            // AL A0 A1 A2 A3 ** ** ** ** L0 L1 L2 L3 ** ** **
        __m128i shuftab = loada_si128(shuftab_d135_4_x);
        __m128i x = _mm_shuffle_epi8(y, shuftab);   // L0 AL A0 A1 ** ** ** ** ** AL L0 L1 L2 ** ** **
        __m128i z = _mm_bsrli_si128(y, 1);          // A0 A1 A2 A3 ** ** ** ** L0 L1 L2 L3 ** ** ** **
        __m128i f = filt_121(x, y, z);              // B0 B1 B2 B3 ** ** ** ** ** C1 C2 C3 ** ** ** ** // 1st row | 1st col (both 121-filtered)

        shuftab = loada_si128(shuftab_d135_4_final);
        f = _mm_shuffle_epi8(f, shuftab);           // B0 B1 B2 B3 C1 B0 B1 B2 C2 C1 B0 B1 C3 C2 C1 B0 // final predition
        int p = pitch;
        *(int*)(dst)     = _mm_cvtsi128_si32(f);                     // B0 B1 B2 B3
        *(int*)(dst+p)   = _mm_cvtsi128_si32(_mm_bsrli_si128(f,4));  // C1 B0 B1 B2
        *(int*)(dst+p*2) = _mm_cvtsi128_si32(_mm_bsrli_si128(f,8));  // C2 C1 B0 B1
        *(int*)(dst+p*3) = _mm_cvtsi128_si32(_mm_bsrli_si128(f,12)); // C3 C2 C1 B0
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, D135_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        const uint8_t* pl = refPel + 1 + 2*8;
        __m128i al = _mm_set1_epi8(pa[-1]);                         // AL AL AL AL AL AL AL AL AL AL AL AL AL AL AL AL
        __m128i above_x = loadl_epi64(pa);                          // A0 A1 A2 A3 A4 A5 A6 A7 ** ** ** ** ** ** ** **
        __m128i above_y = _mm_alignr_epi8(above_x, al, 15);         // AL A0 A1 A2 A3 A4 A5 A6 A7 ** ** ** ** ** ** **
        __m128i y = loadh_epi64(above_y, pl);                       // AL A0 A1 A2 A3 A4 A5 A6 L0 L1 L2 L3 L4 L5 L6 L7
        __m128i shuftab = loada_si128(shuftab_d135_8_load);
        y = _mm_shuffle_epi8(y, shuftab);                           // AL A0 A1 A2 A3 A4 A5 A6 L7 L6 L5 L4 L3 L2 L1 L0 // above_y | left_y
        __m128i z = _mm_alignr_epi8(y, y, 15);                      // L0 AL A0 A1 A2 A3 A4 A5 A6 L7 L6 L5 L4 L3 L2 L1 // above_z | left_z

        __m128i x = _mm_alignr_epi8(y, y, 1);                       // A0 A1 A2 A3 A4 A5 A6 L7 L6 L5 L4 L3 L2 L1 L0 AL
        x = si128(_mm_shuffle_pd(pd(above_x), pd(x), SHUFPD(0,1))); // A0 A1 A2 A3 A4 A5 A6 A7 L6 L5 L4 L3 L2 L1 L0 AL // above_x | left_x

        __m128i f = filt_121(x, y, z);                              // B0 B1 B2 B3 B4 B5 B6 B7 ** C7 C6 C5 C4 C3 C2 C1 // 1st row | 1st col (both 121-filtered)

        storel_epi64(dst, f); dst+=pitch;                           // B0 B1 B2 B3 B4 B5 B6 B7
        storel_epi64(dst, _mm_alignr_epi8(f,f,15)); dst+=pitch;     // C1 B0 B1 B2 B3 B4 B5 B6
        storel_epi64(dst, _mm_alignr_epi8(f,f,14)); dst+=pitch;     // C2 C1 B0 B1 B2 B3 B4 B5
        storel_epi64(dst, _mm_alignr_epi8(f,f,13)); dst+=pitch;     // C3 C2 C1 B0 B1 B2 B3 B4
        storel_epi64(dst, _mm_alignr_epi8(f,f,12)); dst+=pitch;     // C4 C3 C2 C1 B0 B1 B2 B3
        storel_epi64(dst, _mm_alignr_epi8(f,f,11)); dst+=pitch;     // C5 C4 C3 C2 C1 B0 B1 B2
        storel_epi64(dst, _mm_alignr_epi8(f,f,10)); dst+=pitch;     // C6 C5 C4 C3 C2 C1 B0 B1
        storel_epi64(dst, _mm_alignr_epi8(f,f, 9)); dst+=pitch;     // C7 C6 C5 C4 C3 C2 C1 B0
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, D135_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        const uint8_t* pl = refPel + 1 + 2*16;

        __m128i left_y = loada_si128(pl);                       // L0 L1 L2 L3 L4 L5 L6 L7 L8 L9 La Lb Lc Ld Le Lf
        __m128i shuftab = loada_si128(shuftab_reverse);
        left_y = _mm_shuffle_epi8(left_y, shuftab);             // Lf Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 L0 // left_y
        __m128i above_x = loada_si128(pa);                      // A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae Af // above_x
        __m128i above_y = _mm_loadu_si128((__m128i *)(pa-1));   // AL A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae // above_y
        __m128i above_z = _mm_alignr_epi8(above_y, left_y, 15); // L0 AL A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad // above_z
        __m128i left_x = _mm_bslli_si128(left_y, 1);            // ** Lf Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 // left_x
        __m128i left_z = _mm_alignr_epi8(above_y, left_y, 1);   // Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 L0 AL // left_z

        __m256i x = _mm256_setr_m128i(above_x, left_x);         // A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae Af | ** Lf Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 // above_x | left_x
        __m256i y = _mm256_setr_m128i(above_y, left_y);         // AL A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae | Lf Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 L0 // above_y | left_y
        __m256i z = _mm256_setr_m128i(above_z, left_z);         // L0 AL A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad | Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 L0 AL // above_z | left_z
        __m256i row_and_col = filt_121(x, y, z);                // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf | ** Cf Ce Cd Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 // 1st row | 1st col (both 121-filtered)
        __m256i tmp1 = _mm256_permute4x64_epi64(row_and_col, PERM4x64(2,3,2,3));
        __m256i tmp2 = _mm256_alignr_epi8(row_and_col,tmp1,15); // C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be | ** ** Cf Ce Cd Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2
        __m256i rows = _mm256_permute2x128_si256(row_and_col, tmp2, PERM2x128(0,2));
        __m256i col = _mm256_permute2x128_si256(row_and_col, tmp2, PERM2x128(1,3));

        col;    // ** Cf Ce Cd Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 | ** ** Cf Ce Cd Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2
        rows;   // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf | C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be
        int p = pitch;
        int p2 = pitch*2;
        storeu2_m128i(dst, dst+p, rows); dst+=p2;                               // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf | C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(rows,col,14)); dst+=p2;    // C2 C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd | C3 C2 C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(rows,col,12)); dst+=p2;    // C4 C3 C2 C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb | C5 C4 C3 C2 C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(rows,col,10)); dst+=p2;    // C6 C5 C4 C3 C2 C1 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 | C7 C6 C5 C4 C3 C2 C1 B0 B1 B2 B3 B4 B5 B6 B7 B8
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(rows,col, 8)); dst+=p2;    // C8 C7 C6 C5 C4 C3 C2 C1 B0 B1 B2 B3 B4 B5 B6 B7 | C9 C8 C7 C6 C5 C4 C3 C2 C1 B0 B1 B2 B3 B4 B5 B6
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(rows,col, 6)); dst+=p2;    // Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 B0 B1 B2 B3 B4 B5 | Cb Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 B0 B1 B2 B3 B4
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(rows,col, 4)); dst+=p2;    // Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 B0 B1 B2 B3 | Cd Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 B0 B1 B2
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(rows,col, 2)); dst+=p2;    // Ce Cd Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 B0 B1 | Cf Ce Cd Cc Cb Ca C9 C8 C7 C6 C5 C4 C3 C2 C1 B0
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, D135_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        const uint8_t* pl = refPel + 1 + 2*32;

        __m256i above_x = loada_si256(pa);                          // A00 A01 A02 A03 A04 A05 A06 A07 A08 A09 A10 A11 A12 A13 A14 A15 | A16 A17 A18 A19 A20 A21 A22 A23 A24 A25 A26 A27 A28 A29 A30 A31 // above_x
        __m256i above_y = loadu_si256(pa-1);                        // A/L A00 A01 A02 A03 A04 A05 A06 A07 A08 A09 A10 A11 A12 A13 A14 | A15 A16 A17 A18 A19 A20 A21 A22 A23 A24 A25 A26 A27 A28 A29 A30 // above_y
        __m256i left_y = loada_si256(pl);                           // L00 L01 L02 L03 L04 L05 L06 L07 L08 L09 L10 L11 L12 L13 L14 L15 | L16 L17 L18 L19 L20 L21 L22 L23 L24 L25 L26 L27 L28 L29 L30 L31
        __m256i shuftab = loada_si256(shuftab_reverse);
        left_y = _mm256_shuffle_epi8(left_y, shuftab);              // L15 L14 L13 L12 L11 L10 L09 L08 L07 L06 L05 L04 L03 L02 L01 L00 | L31 L30 L29 L28 L27 L26 L25 L24 L23 L22 L21 L20 L19 L18 L17 L16 // left_y
        __m256i addin = _mm256_permute2x128_si256(left_y, left_y, PERM2x128(1,1));
        __m256i left_x = _mm256_alignr_epi8(left_y, addin, 15);     // L16 L15 L14 L13 L12 L11 L10 L09 L08 L07 L06 L05 L04 L03 L02 L01 | *** L31 L30 L29 L28 L27 L26 L25 L24 L23 L22 L21 L20 L19 L18 L17 // left_x
        addin = _mm256_permute2x128_si256(above_y, left_y, PERM2x128(0,2));
        __m256i left_z = _mm256_alignr_epi8(addin, left_y, 1);      // L14 L13 L12 L11 L10 L09 L08 L07 L06 L05 L04 L03 L02 L01 L00 A-1 | L30 L29 L28 L27 L26 L25 L24 L23 L22 L21 L20 L19 L18 L17 L16 L15 // left_z
        addin = _mm256_permute2x128_si256(left_y, above_y, PERM2x128(0,2));
        __m256i above_z = _mm256_alignr_epi8(above_y, addin, 15);   // L00 A/L A00 A01 A02 A03 A04 A05 A06 A07 A08 A09 A10 A11 A12 A13 | A14 A15 A16 A17 A18 A19 A20 A21 A22 A23 A24 A25 A26 A27 A28 A29 // above_z

        __m256i row = filt_121(above_x, above_y, above_z);          // B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 | B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28 B29 B30 B31 // filtered row
        __m256i col = filt_121(left_x, left_y, left_z);             // C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 | *** C31 C30 C29 C28 C27 C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 // filtered col


        addin = _mm256_permute2x128_si256(col, row, PERM2x128(0,2));    // C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 | B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 // low parts

        int p = pitch;
        int p2 = pitch*2;

        storea_si256(dst, row);                                         // B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 | B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28 B29 B30 B31
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin,15)); dst+=p2; // C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 | B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28 B29 B30
        storea_si256(dst,   _mm256_alignr_epi8(row,addin,14));          // C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 | B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28 B29
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin,13)); dst+=p2; // C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 | B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28
        storea_si256(dst,   _mm256_alignr_epi8(row,addin,12));          // C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 | B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin,11)); dst+=p2; // C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 | B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26
        storea_si256(dst,   _mm256_alignr_epi8(row,addin,10));          // C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 | B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 9)); dst+=p2; // C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 | B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 8));          // C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 | B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 7)); dst+=p2; // C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 | B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 6));          // C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 | B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 5)); dst+=p2; // C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 | B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 4));          // C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 | B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 3)); dst+=p2; // C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 | B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 2));          // C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 | B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 1)); dst+=p2; // C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 | B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16

        row = addin;
        addin = _mm256_permute4x64_epi64(col, PERM4x64(2,3,0,1));       // *** C31 C30 C29 C28 C27 C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 | C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 // low parts

        storea_si256(dst, row);                                         // C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 | B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin,15)); dst+=p2; // C17 C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 | C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 |
        storea_si256(dst,   _mm256_alignr_epi8(row,addin,14));          // C18 C17 C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 | C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin,13)); dst+=p2; // C19 C18 C17 C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 | C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 |
        storea_si256(dst,   _mm256_alignr_epi8(row,addin,12));          // C20 C19 C18 C17 C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 | C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin,11)); dst+=p2; // C21 C20 C19 C18 C17 C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 | C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 |
        storea_si256(dst,   _mm256_alignr_epi8(row,addin,10));          // C22 C21 C20 C19 C18 C17 C16 C15 C14 C13 C12 C11 C10 C09 C08 C07 | C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 9)); dst+=p2; // C23 C22 C21 C20 C19 C18 C17 C16 C15 C14 C13 C12 C11 C10 C09 C08 | C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 B08 |
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 8));          // C24 C23 C22 C21 C20 C19 C18 C17 C16 C15 C14 C13 C12 C11 C10 C09 | C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 B07 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 7)); dst+=p2; // C25 C24 C23 C22 C21 C20 C19 C18 C17 C16 C15 C14 C13 C12 C11 C10 | C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 B06 |
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 6));          // C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 C16 C15 C14 C13 C12 C11 | C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 B05 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 5)); dst+=p2; // C27 C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 C16 C15 C14 C13 C12 | C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 B04 |
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 4));          // C28 C27 C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 C16 C15 C14 C13 | C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 B03 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 3)); dst+=p2; // C29 C28 C27 C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 C16 C15 C14 | C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 B02 |
        storea_si256(dst,   _mm256_alignr_epi8(row,addin, 2));          // C30 C29 C28 C27 C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 C16 C15 | C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 B01 |
        storea_si256(dst+p, _mm256_alignr_epi8(row,addin, 1));          // C31 C30 C29 C28 C27 C26 C25 C24 C23 C22 C21 C20 C19 C18 C17 C16 | C15 C14 C13 C12 C11 C10 C09 C08 C07 C06 C05 C04 C03 C02 C01 B00 |
    }

    template <> void predict_intra_vp9_avx2<TX_4X4, D117_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m128i y = loadu_si128(refPel);                        // AL A0 A1 A2 A3 ** ** ** ** L0 L1 L2 L3 ** ** **
        __m128i x = _mm_bsrli_si128(y, 1);                      // A0 A1 A2 A3 ** ** ** ** L0 L1 L2 L3 ** ** ** **
        __m128i shuftab = loada_si128(shuftab_d117_8_load);
        __m128i z = _mm_shuffle_epi8(y, shuftab);               // L0 AL A0 A1 A2 A3 ** ** ** AL L0 L1 L2 L3 ** **

        __m128i row0 = _mm_avg_epu8(x, y);                      // B0 B1 B2 B3 ** ** ** ** ** ** ** ** ** ** ** ** // first row (average)
        __m128i row1_and_col = filt_121(x, y, z);               // C0 C1 C2 C3 ** ** ** ** ** D2 D3 ** ** ** ** ** // second row and first col (121-filtered)
        __m128i row1 = row1_and_col;
        shuftab = loada_si128(shuftab_d117_8_sep);
        __m128i col0 = _mm_shuffle_epi8(row1_and_col, shuftab); // ** ** ** ** ** ** ** ** ** ** ** ** D3 ** ** D2
        __m128i col1 = _mm_bslli_si128(col0, 3);                // ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** D3

        *(int*)dst = _mm_cvtsi128_si32(row0); dst += pitch;                          // B0 B1 B2 B3
        *(int*)dst = _mm_cvtsi128_si32(row1); dst += pitch;                          // C0 C1 C2 C3
        *(int*)dst = _mm_cvtsi128_si32(_mm_alignr_epi8(row0,col0,15)); dst += pitch; // D2 B0 B1 B2
        *(int*)dst = _mm_cvtsi128_si32(_mm_alignr_epi8(row1,col1,15));               // D3 C0 C1 C2
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, D117_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        const uint8_t* pl = refPel + 1 + 2*8;

        __m128i al = _mm_set1_epi8(pa[-1]);                                 // AL AL AL AL AL AL AL AL AL AL AL AL AL AL AL AL
        __m128i y = loadh_epi64(loadl_epi64(pa), pl);                       // A0 A1 A2 A3 A4 A5 A6 A7 L0 L1 L2 L3 L4 L5 L6 L7
        y = _mm_alignr_epi8(y, al, 15);                                     // AL A0 A1 A2 A3 A4 A5 A6 A7 L0 L1 L2 L3 L4 L5 L6 // y
        __m128i x = _mm_bsrli_si128(y, 1);                                  // A0 A1 A2 A3 A4 A5 A6 A7 L0 L1 L2 L3 L4 L5 L6 ** // x
        __m128i z = _mm_shuffle_epi8(y, loada_si128(shuftab_d117_8_load));  // L0 AL A0 A1 A2 A3 A4 A5 ** AL L0 L1 L2 L3 L4 ** // z

        __m128i row0 = _mm_avg_epu8(x, y);                                  // B0 B1 B2 B3 B4 B5 B6 B7 ** ** ** ** ** ** ** ** // first row (average)
        __m128i row1_and_col = filt_121(x, y, z);                           // C0 C1 C2 C3 C4 C5 C6 C7 ** D2 D3 D4 D5 D6 D7 ** // second row and first col (121-filtered)
        __m128i row1 = row1_and_col;
        __m128i shuf_tab = loada_si128(shuftab_d117_8_sep);
        __m128i col0 = _mm_shuffle_epi8(row1_and_col, shuf_tab);            // ** ** ** ** ** ** ** ** ** ** D7 D5 D3 D6 D4 D2
        __m128i col1 = _mm_bslli_si128(col0, 3);                            // ** ** ** ** ** ** ** ** ** ** ** ** ** D7 D5 D3

        storel_epi64(dst, row0); dst += pitch;                              // B0 B1 B2 B3 B4 B5 B6 B7
        storel_epi64(dst, row1); dst += pitch;                              // C0 C1 C2 C3 C4 C5 C6 C7
        storel_epi64(dst, _mm_alignr_epi8(row0, col0,15)); dst += pitch;    // D2 B0 B1 B2 B3 B4 B5 B6
        storel_epi64(dst, _mm_alignr_epi8(row1, col1,15)); dst += pitch;    // D3 C0 C1 C2 C3 C4 C5 C6
        storel_epi64(dst, _mm_alignr_epi8(row0, col0,14)); dst += pitch;    // D4 D2 B0 B1 B2 B3 B4 B5
        storel_epi64(dst, _mm_alignr_epi8(row1, col1,14)); dst += pitch;    // D5 D3 C0 C1 C2 C3 C4 C5
        storel_epi64(dst, _mm_alignr_epi8(row0, col0,13)); dst += pitch;    // D6 D4 D2 B0 B1 B2 B3 B4
        storel_epi64(dst, _mm_alignr_epi8(row1, col1,13)); dst += pitch;    // D7 D5 D3 C0 C1 C2 C3 C4
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, D117_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        const uint8_t* pl = refPel + 1 + 2*16;

        __m128i left_y = loada_si128(pl);                                   // L0 L1 L2 L3 L4 L5 L6 L7 L8 L9 La Lb Lc Ld Le Lf
        left_y = _mm_shuffle_epi8(left_y, *(__m128i*)shuftab_reverse);      // Lf Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 L0 // left_y
        __m128i above_x = loada_si128(pa);                                  // A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae Af // above_x
        __m128i above_y = _mm_loadu_si128((__m128i *)(pa-1));               // AL A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae // above_y
        __m128i above_z = _mm_alignr_epi8(above_y, left_y, 15);             // L0 AL A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad // above_z
        __m128i left_x = _mm_bslli_si128(left_y, 1);                        // ** Lf Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 // left_x
        __m128i left_z = _mm_alignr_epi8(above_y, left_y, 1);               // Le Ld Lc Lb La L9 L8 L7 L6 L5 L4 L3 L2 L1 L0 AL // left_z

        __m256i x = _mm256_setr_m128i(above_x, left_x);
        __m256i y = _mm256_setr_m128i(above_y, left_y);
        __m256i z = _mm256_setr_m128i(above_z, left_z);
        __m256i row0 = _mm256_avg_epu8(x, y);                               // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf | ***** // first row (average)
        __m256i row1_and_col = filt_121(x, y, z);                           // C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf | ** ** Df De Dd Dc Db Da D9 D8 D7 D6 D5 D4 D3 D2 // second row and col (121-filtered)

        __m256i rows = _mm256_permute2x128_si256(row0, row1_and_col, PERM2x128(0,2));

        __m256i cols = _mm256_shuffle_epi8(row1_and_col, *(__m256i*)shuftab_d117_32);   // ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** | ** Df Dd Db D9 D7 D5 D3 ** De Dc Da D8 D6 D4 D2
        cols = _mm256_permute4x64_epi64(cols, PERM4x64(0,3,1,2));                       // ** ** ** ** ** ** ** ** ** Df Dd Db D9 D7 D5 D3 | ** ** ** ** ** ** ** ** ** De Dc Da D8 D6 D4 D2

        int p2 = pitch*2;
        storeu2_m128i(dst, dst+pitch, rows); dst+=p2;                               // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf | C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf
        storeu2_m128i(dst, dst+pitch, _mm256_alignr_epi8(rows, cols,15)); dst+=p2;  // D2 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be | D3 C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce
        storeu2_m128i(dst, dst+pitch, _mm256_alignr_epi8(rows, cols,14)); dst+=p2;  // D4 D2 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd | D5 D3 C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd
        storeu2_m128i(dst, dst+pitch, _mm256_alignr_epi8(rows, cols,13)); dst+=p2;  // D6 D4 D2 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc | D7 D5 D3 C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc
        storeu2_m128i(dst, dst+pitch, _mm256_alignr_epi8(rows, cols,12)); dst+=p2;  // D8 D6 D4 D2 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb | D9 D7 D5 D3 C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb
        storeu2_m128i(dst, dst+pitch, _mm256_alignr_epi8(rows, cols,11)); dst+=p2;  // Da D8 D6 D4 D2 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba | Db D9 D7 D5 D3 C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca
        storeu2_m128i(dst, dst+pitch, _mm256_alignr_epi8(rows, cols,10)); dst+=p2;  // Dc Da D8 D6 D4 D2 B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 | Dd Db D9 D7 D5 D3 C0 C1 C2 C3 C4 C5 C6 C7 C8 C9
        storeu2_m128i(dst, dst+pitch, _mm256_alignr_epi8(rows, cols, 9)); dst+=p2;  // De Dc Da D8 D6 D4 D2 B0 B1 B2 B3 B4 B5 B6 B7 B8 | Df Dd Db D9 D7 D5 D3 C0 C1 C2 C3 C4 C5 C6 C7 C8
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, D117_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t* pa = refPel + 1;
        const uint8_t* pl = refPel + 1 + 2*32;

        __m256i above_x = loadu_si256(pa);                                  // A00 A01 A02 A03 .. A12 A13 A14 A15 | A16 A17 A18 A19 .. A28 A29 A30 A31 // above_x
        __m256i above_y = loadu_si256(pa-1);                                // A/L A00 A01 A02 .. A11 A12 A13 A14 | A15 A16 A17 A18 .. A27 A28 A29 A30 // above_y
        __m256i left_y = loadu_si256(pl);                                   // L00 L01 L02 L03 .. L12 L13 L14 L15 | L16 L17 L18 L19 .. L28 L29 L30 L31
        left_y = _mm256_shuffle_epi8(left_y, *(__m256i*)shuftab_reverse);   // L15 L14 L13 L12 .. L03 L02 L01 L00 | L31 L30 L29 L28 .. L19 L18 L17 L16 // left_y
        __m256i addin = _mm256_permute2x128_si256(left_y, left_y, PERM2x128(1,1));
        __m256i left_x = _mm256_alignr_epi8(left_y, addin, 15);             // L16 L15 L14 L13 .. L04 L03 L02 L01 | *** L31 L30 L29 .. L20 L19 L18 L17 // left_x
        addin = _mm256_permute2x128_si256(above_y, left_y, PERM2x128(0,2));
        __m256i left_z = _mm256_alignr_epi8(addin, left_y, 1);              // L14 L13 L12 L11 .. L02 L01 L00 A-1 | L30 L29 L28 L27 .. L18 L17 L16 L15 // left_z
        addin = _mm256_permute2x128_si256(left_y, above_y, PERM2x128(0,2));
        __m256i above_z = _mm256_alignr_epi8(above_y, addin, 15);           // L00 A-1 A00 A01 .. A10 A11 A12 A13 | A14 A15 A16 A17 .. A26 A27 A28 A29 // above_z

        __m256i row0 = _mm256_avg_epu8(above_x, above_y);                   // B00 B01 B02 B03 .. B12 B13 B14 B15 | B16 B17 B18 B19 .. B28 B29 B30 B31 // first row (average)
        __m256i row1 = filt_121(above_x, above_y, above_z);                 // C00 C01 C02 C03 .. C12 C13 C14 C15 | C16 C17 C18 C19 .. C28 C29 C30 C31 // second row (121-filtered)
        __m256i col = filt_121(left_x, left_y, left_z);                     // D17 D16 D15 D14 .. D05 D04 D03 D02 | *** *** D31 D30 .. D21 D20 D19 D18 // first col (121-filtered)

        col = _mm256_shuffle_epi8(col, *(__m256i*)shuftab_d117_32);         // D17 D15 D13 D11 D09 D07 D05 D03 D16 D14 D12 D10 D08 D06 D04 D02 | *** D31 D29 D27 D25 D23 D21 D19 *** D30 D28 D26 D24 D22 D20 D18
        col = _mm256_permute4x64_epi64(col, PERM4x64(3,1,2,0));             // *** D30 D28 D26 D24 D22 D20 D18 D16 D14 D12 D10 D08 D06 D04 D02 | *** D31 D29 D27 D25 D23 D21 D19 D17 D15 D13 D11 D09 D07 D05 D03

        __m256i addin0 = _mm256_permute2x128_si256(col, row0, PERM2x128(0,2));
        __m256i addin1 = _mm256_permute2x128_si256(col, row1, PERM2x128(1,2));

        int p = pitch;
        int p2 = pitch*2;
        storea_si256(dst,   row0);                                          // B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 | B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28 B29 B30 B31
        storea_si256(dst+p, row1); dst+=p2;                                 // C00 C01 C02 C03 C04 C05 C06 C07 C08 C09 C10 C11 C12 C13 C14 C15 | C16 C17 C18 C19 C20 C21 C22 C23 C24 C25 C26 C27 C28 C29 C30 C31
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0,15));           // D02 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 | B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28 B29 B30
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1,15)); dst+=p2;  // D03 C00 C01 C02 C03 C04 C05 C06 C07 C08 C09 C10 C11 C12 C13 C14 | C15 C16 C17 C18 C19 C20 C21 C22 C23 C24 C25 C26 C27 C28 C29 C30
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0,14));           // D04 D02 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 | B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28 B29
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1,14)); dst+=p2;  // D05 D03 C00 C01 C02 C03 C04 C05 C06 C07 C08 C09 C10 C11 C12 C13 | C14 C15 C16 C17 C18 C19 C20 C21 C22 C23 C24 C25 C26 C27 C28 C29
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0,13));           // D06 D04 D02 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 | B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27 B28
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1,13)); dst+=p2;  // D07 D05 D03 C00 C01 C02 C03 C04 C05 C06 C07 C08 C09 C10 C11 C12 | C13 C14 C15 C16 C17 C18 C19 C20 C21 C22 C23 C24 C25 C26 C27 C28
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0,12));           // D08 D06 D04 D02 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 | B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26 B27
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1,12)); dst+=p2;  // D09 D07 D05 D03 C00 C01 C02 C03 C04 C05 C06 C07 C08 C09 C10 C11 | C12 C13 C14 C15 C16 C17 C18 C19 C20 C21 C22 C23 C24 C25 C26 C27
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0,11));           // D10 D08 D06 D04 D02 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 | B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25 B26
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1,11)); dst+=p2;  // D11 D09 D07 D05 D03 C00 C01 C02 C03 C04 C05 C06 C07 C08 C09 C10 | C11 C12 C13 C14 C15 C16 C17 C18 C19 C20 C21 C22 C23 C24 C25 C26
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0,10));           // D12 D10 D08 D06 D04 D02 B00 B01 B02 B03 B04 B05 B06 B07 B08 B09 | B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24 B25
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1,10)); dst+=p2;  // D13 D11 D09 D07 D05 D03 C00 C01 C02 C03 C04 C05 C06 C07 C08 C09 | C10 C11 C12 C13 C14 C15 C16 C17 C18 C19 C20 C21 C22 C23 C24 C25
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 9));           // D14 D12 D10 D08 D06 D04 D02 B00 B01 B02 B03 B04 B05 B06 B07 B08 | B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23 B24
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 9)); dst+=p2;  // D15 D13 D11 D09 D07 D05 D03 C00 C01 C02 C03 C04 C05 C06 C07 C08 | C09 C10 C11 C12 C13 C14 C15 C16 C17 C18 C19 C20 C21 C22 C23 C24
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 8));           // D16 D14 D12 D10 D08 D06 D04 D02 B00 B01 B02 B03 B04 B05 B06 B07 | B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22 B23
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 8)); dst+=p2;  // D17 D15 D13 D11 D09 D07 D05 D03 C00 C01 C02 C03 C04 C05 C06 C07 | C08 C09 C10 C11 C12 C13 C14 C15 C16 C17 C18 C19 C20 C21 C22 C23
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 7));           // D18 D16 D14 D12 D10 D08 D06 D04 D02 B00 B01 B02 B03 B04 B05 B06 | B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21 B22
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 7)); dst+=p2;  // D19 D17 D15 D13 D11 D09 D07 D05 D03 C00 C01 C02 C03 C04 C05 C06 | C07 C08 C09 C10 C11 C12 C13 C14 C15 C16 C17 C18 C19 C20 C21 C22
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 6));           // D20 D18 D16 D14 D12 D10 D08 D06 D04 D02 B00 B01 B02 B03 B04 B05 | B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20 B21
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 6)); dst+=p2;  // D21 D19 D17 D15 D13 D11 D09 D07 D05 D03 C00 C01 C02 C03 C04 C05 | C06 C07 C08 C09 C10 C11 C12 C13 C14 C15 C16 C17 C18 C19 C20 C21
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 5));           // D22 D20 D18 D16 D14 D12 D10 D08 D06 D04 D02 B00 B01 B02 B03 B04 | B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19 B20
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 5)); dst+=p2;  // D23 D21 D19 D17 D15 D13 D11 D09 D07 D05 D03 C00 C01 C02 C03 C04 | C05 C06 C07 C08 C09 C10 C11 C12 C13 C14 C15 C16 C17 C18 C19 C20
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 4));           // D24 D22 D20 D18 D16 D14 D12 D10 D08 D06 D04 D02 B00 B01 B02 B03 | B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18 B19
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 4)); dst+=p2;  // D25 D23 D21 D19 D17 D15 D13 D11 D09 D07 D05 D03 C00 C01 C02 C03 | C04 C05 C06 C07 C08 C09 C10 C11 C12 C13 C14 C15 C16 C17 C18 C19
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 3));           // D26 D24 D22 D20 D18 D16 D14 D12 D10 D08 D06 D04 D02 B00 B01 B02 | B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17 B18
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 3)); dst+=p2;  // D27 D25 D23 D21 D19 D17 D15 D13 D11 D09 D07 D05 D03 C00 C01 C02 | C03 C04 C05 C06 C07 C08 C09 C10 C11 C12 C13 C14 C15 C16 C17 C18
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 2));           // D28 D26 D24 D22 D20 D18 D16 D14 D12 D10 D08 D06 D04 D02 B00 B01 | B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16 B17
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 2)); dst+=p2;  // D29 D27 D25 D23 D21 D19 D17 D15 D13 D11 D09 D07 D05 D03 C00 C01 | C02 C03 C04 C05 C06 C07 C08 C09 C10 C11 C12 C13 C14 C15 C16 C17
        storea_si256(dst,   _mm256_alignr_epi8(row0, addin0, 1));           // D30 D28 D26 D24 D22 D20 D18 D16 D14 D12 D10 D08 D06 D04 D02 B00 | B01 B02 B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 B13 B14 B15 B16
        storea_si256(dst+p, _mm256_alignr_epi8(row1, addin1, 1));           // D31 D29 D27 D25 D23 D21 D19 D17 D15 D13 D11 D09 D07 D05 D03 C00 | C01 C02 C03 C04 C05 C06 C07 C08 C09 C10 C11 C12 C13 C14 C15 C16
    }

    template <> void predict_intra_vp9_avx2<TX_4X4, D153_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        __m128i x = loadu_si128(refPel);                            // [ AL A0 A1 A2 | A3 ** ** ** | ** L0 L1 L2 | L3 ** ** ** ]
        x = _mm_shuffle_epi8(x, *(__m128i*)shuftab_d153_4_load);    // [ L3 L2 L1 L0 | AL A0 A1 A2 | ** ** ** ** | ** ** ** ** ]
        __m128i y = _mm_bsrli_si128(x, 1);                          // [ L2 L1 L0 AL | A0 A1 A2 ** | ** ** ** ** | ** ** ** ** ]
        __m128i z = _mm_bsrli_si128(x, 2);                          // [ L1 L0 AL A0 | A1 A2 ** ** | ** ** ** ** | ** ** ** ** ]

        __m128i pred1 = filt_121(x, y, z);                                  // [ D3 D2 D1 D0 | R2 R3 ** ** | ** ** ** ** | ** ** ** ** ]
        __m128i pred2 = _mm_avg_epu8(x, y);                                 // [ C3 C2 C1 C0 | ** ** ** ** | ** ** ** ** | ** ** ** ** ]
        __m128i pred = _mm_unpacklo_epi8(pred2, pred1);                     // [ C3 D3 C2 D2 | C1 D1 C0 D0 | ** R2 ** R3 | ** ** ** ** ]
        pred = _mm_shuffle_epi8(pred, *(__m128i*)shuftab_d153_4_final);     // [ C3 D3 C2 D2 | C2 D2 C1 D1 | C1 D1 C0 D0 | C0 D0 R2 R3 ]

        int p = pitch;
        *(int*)(dst)     = _mm_cvtsi128_si32(_mm_bsrli_si128(pred,12));  // C0 D0 R2 R3
        *(int*)(dst+p)   = _mm_cvtsi128_si32(_mm_bsrli_si128(pred,8));   // C1 D1 C0 D0
        *(int*)(dst+p*2) = _mm_cvtsi128_si32(_mm_bsrli_si128(pred,4));   // C2 D2 C1 D1
        *(int*)(dst+p*3) = _mm_cvtsi128_si32(pred);                      // C3 D3 C2 D2
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, D153_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel+1;
        const uint8_t *pl = refPel+1+8*2;

        __m128i pred = _mm_set_epi64x(*(uint64_t*)(pa-1), *(uint64_t*)pl);      // [ L0 L1 L2 L3 L4 L5 L6 L7 AL A0 A1 A2 A3 A4 A5 A6 ]
        __m128i x = _mm_shuffle_epi8(pred, *(__m128i*)shuftab_d153_8_x);    // [ L7 L6 L5 L4 L3 L2 L1 L0 A1 A2 A3 A4 A5 A6 A6 A6 ]
        __m128i y = _mm_shuffle_epi8(pred, *(__m128i*)shuftab_d153_8_y);    // [ L6 L5 L4 L3 L2 L1 L0 AL A0 A1 A2 A3 A4 A5 A6 A6 ]
        __m128i z = _mm_shuffle_epi8(pred, *(__m128i*)shuftab_d153_8_z);    // [ L5 L4 L3 L2 L1 L0 AL A0 AL A0 A1 A2 A3 A4 A5 A6 ]
        __m128i c = _mm_avg_epu8(x,y);                                      // [ C7 C6 C5 C4 C3 C2 C1 C0 ** ** ** ** ** ** ** ** ]

        __m128i w = filt_121(x, y, z);                                      // [ D7 D6 D5 D4 D3 D2 D1 D0 R2 R3 R4 R5 R6 R7 ** ** ]
        __m128i row2 = _mm_unpacklo_epi8(c, w);                             // [ C7 D7 C6 D6 C5 D5 C4 D4 C3 D3 C2 D2 C1 D1 C0 D0 ]
        __m128i row1 = _mm_unpackhi_epi64(row2, w);                         // [ C3 D3 C2 D2 C1 D1 C0 D0 R2 R3 R4 R5 R6 R7 ** ** ]

        storel_epi64(dst, _mm_bsrli_si128(row1, 6)); dst += pitch;  // [ C0 D0 R2 R3 R4 R5 R6 R7 ]
        storel_epi64(dst, _mm_bsrli_si128(row1, 4)); dst += pitch;  // [ C1 D1 C0 D0 R2 R3 R4 R5 ]
        storel_epi64(dst, _mm_bsrli_si128(row1, 2)); dst += pitch;  // [ C2 D2 C1 D1 C0 D0 R2 R3 ]
        storel_epi64(dst, row1); dst += pitch;                      // [ C3 D3 C2 D2 C1 D1 C0 D0 ]
        storel_epi64(dst, _mm_bsrli_si128(row2, 6)); dst += pitch;  // [ C4 D4 C3 D3 C2 D2 C1 D1 ]
        storel_epi64(dst, _mm_bsrli_si128(row2, 4)); dst += pitch;  // [ C5 D5 C4 D4 C3 D3 C2 D2 ]
        storel_epi64(dst, _mm_bsrli_si128(row2, 2)); dst += pitch;  // [ C6 D6 C5 D5 C4 D4 C3 D3 ]
        storel_epi64(dst, row2);                                    // [ C7 D7 C6 D6 C5 D5 C4 D4 ]
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, D153_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel+1;
        const uint8_t *pl = refPel+1+16*2;

        __m128i left0  = loada_si128(pl);                                   // 16b: [ L0 L1 .. Ld Le Lf ]
        left0 = _mm_shuffle_epi8(left0, *(__m128i*)shuftab_reverse);        // 16b: [ Lf Le .. L2 L1 L0 ]
        __m128i aboveleft = _mm_set1_epi8(pa[-1]);                          // 16b: [ AL AL .. AL AL AL ]
        __m128i left1 = _mm_alignr_epi8(aboveleft, left0, 1);               // 16b: [ Le Ld .. L1 L0 AL ]
        __m128i above1 = loada_si128(pa);                                   // 16b: [ A0 A1 .. Ad Ae Af ]
        __m128i left2 = _mm_alignr_epi8(above1, left1, 1);                  // 16b: [ Ld Lc .. L0 AL A0 ]
        __m128i above0 = _mm_bsrli_si128(above1, 1);                        // 16b: [ A1 A2 .. Ae Af ** ]
        __m128i above2 = _mm_alignr_epi8(above1, aboveleft, 15);            // 16b: [ AL A0 .. Ac Ad Ae ]

        _mm256_zeroupper();

        __m256i col0 = si256(_mm_avg_epu8(left0, left1));                   // 32b: [ C0[f] .. C0[0] | 0 ]

        __m256i x = _mm256_set_m128i(above0, left0);                        // 32b: [ Lf Le .. L2 L1 L0 | A1 A2 .. Ae Af ** ]
        __m256i y = _mm256_set_m128i(above1, left1);                        // 32b: [ Le Ld .. L1 L0 AL | A0 A1 .. Ad Ae Af ]
        __m256i z = _mm256_set_m128i(above2, left2);                        // 32b: [ Ld Lc .. L0 AL A0 | AL A0 .. Ac Ad Ae ]
        __m256i w = filt_121(x, y, z);                                      // 32b: [ C1[f] .. C1[0] | R0[2] ..  R0[f] ** ** ]

        __m256i cols_hi = _mm256_unpacklo_epi8(col0, w);                    // 32b: [ f f e e d d c c b b a a 9 9 8 8 *** ] cols 8-15
        __m256i cols_lo = _mm256_unpackhi_epi8(col0, w);                    // 32b: [ 7 7 6 6 5 5 4 4 3 3 2 2 1 1 0 0 *** ] cols 0-7

        __m256i a = _mm256_inserti128_si256(w, si128(cols_lo), 0);          // 32b: [ C0[7] C1[7] .. C0[0] C0[0] | R0[2] .. R0[f] ** ** ]
        __m256i b = _mm256_inserti128_si256(cols_hi, si128(cols_lo), 1);    // 32b: [ C0[f] C1[f] .. C0[8] C1[8] | C0[7] C1[7] .. C0[0] C1[0] ]

        int pitch8 = pitch*8;

        __m256i row = _mm256_alignr_epi8(a, b, 14);                         // 32b: [ C0[8] C1[8] C0[7] C1[7] .. C0[1] C1[1] | C0[0] C1[0] R0[2] R0[3] .. R0[f] ]
        storea_si128((dst), _mm256_extracti128_si256(row,1));
        storea_si128((dst+pitch8), si128(row));
        dst += pitch;

        row = _mm256_alignr_epi8(a, b, 12);                                 // 32b: [ C0[9] C1[9] C0[8] C1[8] .. C0[2] C1[2] | C0[1] C1[1] C0[0] C1[0] .. R0[d] ]
        storea_si128((dst), _mm256_extracti128_si256(row,1));
        storea_si128((dst+pitch8), si128(row));
        dst += pitch;

        row = _mm256_alignr_epi8(a, b, 10);
        storea_si128((dst), _mm256_extracti128_si256(row,1));
        storea_si128((dst+pitch8), si128(row));
        dst += pitch;

        row = _mm256_alignr_epi8(a, b, 8);
        storea_si128((dst), _mm256_extracti128_si256(row,1));
        storea_si128((dst+pitch8), si128(row));
        dst += pitch;

        row = _mm256_alignr_epi8(a, b, 6);
        storea_si128((dst), _mm256_extracti128_si256(row,1));
        storea_si128((dst+pitch8), si128(row));
        dst += pitch;

        row = _mm256_alignr_epi8(a, b, 4);
        storea_si128((dst), _mm256_extracti128_si256(row,1));
        storea_si128((dst+pitch8), si128(row));
        dst += pitch;

        row = _mm256_alignr_epi8(a, b, 2);
        storea_si128((dst), _mm256_extracti128_si256(row,1));
        storea_si128((dst+pitch8), si128(row));
        dst += pitch;

        storea_si128((dst), si128(cols_lo));
        storea_si128((dst+pitch8), si128(cols_hi));
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, D153_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel+1;
        const uint8_t *pl = refPel+1+32*2;

        __m256i above2 = loadu_si256(pa-1);                                     // 32b: [ A/L A00 .. A28 A29 A30 ]
        __m256i above1 = loada_si256(pa);                                       // 32b: [ A00 A01 .. A29 A30 A31 ]
        __m256i above0 = loadu_si256(pa+1);                                     // 32b: [ A01 A02 .. A30 A31 *** ]

        __m256i left0  = loadu_si256(pl);                                       // 32b: [ L00 L01 .. L14 L15 | L16 L17 .. L30 L31 ]
        left0 = _mm256_shuffle_epi8(left0, *(__m256i*)shuftab_reverse);         // 32b: [ L15 L14 .. L01 L00 | L31 L30 .. L17 L16 ]
        __m256i tmp = _mm256_permute2x128_si256(above2, left0, PERM2x128(0,2)); // 32b: [ A/L A00 ..         | L15 L14 ..         ]
        __m256i left1 = _mm256_alignr_epi8(tmp, left0, 1);                      // 32b: [ L14 L13 .. L00 A/L | L30 L29 .. L16 L15 ]
        __m256i left2 = _mm256_alignr_epi8(tmp, left0, 2);                      // 32b: [ L13 L12 .. A/L A00 | L29 L28 .. L15 L14 ]

        __m256i row = filt_121(above0, above1, above2);                         // 32b: [ R02 .. R17 | R18 .. R31 *** *** ] - first row
        __m256i c1 = filt_121(left0, left1, left2);                             // 32b: [ D15 .. D00 | D31 .. D16 ] - second column
        __m256i c0 = _mm256_avg_epu8(left0, left1);                             // 32b: [ C15 .. C00 | C31 .. C16 ] - first column

        __m256i cols_hi = _mm256_unpacklo_epi8(c0, c1);                         // 32b: [ C15 D15 .. C08 D08 | C31 D31 .. C24 D24 ] - interleaved columns
        __m256i cols_lo = _mm256_unpackhi_epi8(c0, c1);                         // 32b: [ C07 D07 .. C00 D00 | C23 D23 .. C16 D16 ] - interleaved columns

        __m256i a = _mm256_permute2x128_si256(cols_lo, row, PERM2x128(0,2));    // 32b: [ C07 D07 C06 D06 .. C00 D00 | R02 R03 .. R15 R16 R17 ]
        __m256i b = row;                                                                // 32b: [ R02 R03 R04 R05 .. R16 R17 | R18 R19 .. R31 *** *** ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 14)); dst += pitch;  // 32b: [ C00 D00 R02 R03 .. R12 R13 | R14 R15 .. R27 R28 R29 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 12)); dst += pitch;  // 32b: [ C01 D01 C00 D00 .. R10 R11 | R12 R13 .. R25 R26 R27 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 10)); dst += pitch;  // 32b: [ C02 D02 C01 D01 .. R08 R09 | R10 R11 .. R23 R24 R25 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 8)); dst += pitch;   // 32b: [ C03 D03 C02 D02 .. R06 R07 | R08 R09 .. R21 R22 R23 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 6)); dst += pitch;   // 32b: [ C04 D04 C03 D03 .. R04 R05 | R06 R07 .. R19 R20 R21 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 4)); dst += pitch;   // 32b: [ C05 D05 C04 D04 .. R02 R03 | R04 R05 .. R17 R19 R19 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 2)); dst += pitch;   // 32b: [ C06 D06 C05 D05 .. R00 R01 | R02 R03 .. R15 R16 R17 ]
        storea_si256(dst, a); dst += pitch;                             // 32b: [ C07 D07 C06 D06 .. C00 D00 | R00 R01 .. R13 R14 R15 ]

        a = _mm256_permute2x128_si256(cols_hi, cols_lo, PERM2x128(0,2));                // 32b: [ C15 D15 C14 D14 .. C08 D08 | C07 D07 .. D01 C00 D00 ]
        b = _mm256_permute2x128_si256(cols_lo, row, PERM2x128(0,2));                    // 32b: [ C07 D07 C06 D06 .. C00 D00 | R02 R03 .. R15 R16 R17 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 14)); dst += pitch;  // 32b: [ C08 D08 C07 D07 .. C01 D01 | C00 D00 .. D11 R12 R13 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 2)); dst += pitch;
        storea_si256(dst, a); dst += pitch;                             // 32b: [ C15 D15 C14 D14 .. C08 D08 | C07 D07 .. D01 C00 D00 ]

        a = _mm256_permute2x128_si256(cols_lo, cols_hi, PERM2x128(1,2));                // 32b: [ C23 D23 C22 D22 .. C16 D16 | C15 D15 .. D09 C08 D08 ]
        b = _mm256_permute2x128_si256(cols_hi, cols_lo, PERM2x128(0,2));                // 32b: [ C15 D15 C14 D14 .. C08 D08 | C07 D07 .. D01 C00 D00 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 14)); dst += pitch;  // 32b: [ C16 D16 C15 D15 .. C09 D09 | C08 D08 .. D02 C01 D01 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 2)); dst += pitch;
        storea_si256(dst, a); dst += pitch;                             // 32b: [ C23 D23 C22 D22 .. C16 D16 | C15 D15 .. D09 C08 D08 ]

        a = _mm256_permute2x128_si256(cols_hi, cols_lo, PERM2x128(1,3));
        b = _mm256_permute2x128_si256(cols_lo, cols_hi, PERM2x128(1,2));
        storea_si256(dst, _mm256_alignr_epi8(b, a, 14)); dst += pitch;  // 32b: [ C24 D24 C23 D23 .. C17 D17 | C16 D16 .. D08 C07 D07 ]
        storea_si256(dst, _mm256_alignr_epi8(b, a, 12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 2)); dst += pitch;
        storea_si256(dst, a);
    }

    template <> void predict_intra_vp9_avx2<TX_4X4, D207_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pl = refPel+1+4*2;
        __m128i x = _mm_cvtsi32_si128(*(int*)pl);            // 16b: L0 L1 L2 L3 | 00 00 00 00 | 00 00 00 00 | 00 00 00 00
        x = _mm_shuffle_epi8(x, *(__m128i*)shuftab_d207_4);     // 16b: L0 L1 L1 L2 | L2 L3 L3 L3 | L3 L3 L3 L3 | L3 L3 L3 L3
        __m128i y = _mm_bsrli_si128(x, 2);                      // 16b: L1 L2 L2 L3 | L3 L3 L3 L3 | L3 L3 L3 L3 | L3 L3 00 00
        __m128i z = _mm_bsrli_si128(x, 4);                      // 16b: L2 L3 L3 L3 | L3 L3 L3 L3 | L3 L3 L3 L3 | 00 00 00 00
        __m128i avg = _mm_avg_epu8(x, y);                       // 16b: A0 A1 A1 A2 | A2 L3 L3 L3 | ** ** ** ** | ** ** ** ** // averavge
        __m128i filt = filt_121(x, y, z);                       // 16b: F0 F1 F1 F2 | F2 L3 L3 L3 | ** ** ** ** | ** ** ** ** // 121-filtered
        __m128i a = _mm_unpacklo_epi8(avg,filt);                // 16b: A0 F0 A1 F1 | A1 F1 A2 F2 | A2 F2 L3 L3 | L3 L3 L3 L3

        int p = pitch;
        *(int*)(dst)     = _mm_cvtsi128_si32(a);                     // C0 D0 R2 R3
        *(int*)(dst+p)   = _mm_cvtsi128_si32(_mm_bsrli_si128(a,4));  // C1 D1 C0 D0
        *(int*)(dst+p*2) = _mm_cvtsi128_si32(_mm_bsrli_si128(a,8));  // C2 D2 C1 D1
        *(int*)(dst+p*3) = _mm_cvtsi128_si32(_mm_bsrli_si128(a,12)); // C3 D3 C2 D2
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, D207_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pl = refPel+1+8*2;
        __m128i x = loada_si128(pl);                                // 16b: L0 L1 L2 L3 L4 L5 L6 L7 ** ** ** ** ** ** ** **
        __m128i last = _mm_broadcastb_epi8(_mm_bsrli_si128(x,7));   // 16b: L7 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7
        x = _mm_unpacklo_epi64(x, last);                            // 16b: L0 L1 L2 L3 L4 L5 L6 L7 L7 L7 L7 L7 L7 L7 L7 L7
        __m128i y = _mm_alignr_epi8(last, x, 1);                    // 16b: L1 L2 L3 L4 L5 L6 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7
        __m128i z = _mm_alignr_epi8(last, x, 2);                    // 16b: L2 L3 L4 L5 L6 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7 L7
        __m128i avg = _mm_avg_epu8(x, y);                           // 16b: A0 A1 A2 A3 A4 A5 A6 L7 L7 L7 L7 L7 L7 L7 L7 L7
        __m128i filt = filt_121(x, y, z);                           // 16b: F0 F1 F2 F3 F4 F5 F6 L7 L7 L7 L7 L7 L7 L7 L7 L7

        __m128i a = _mm_unpacklo_epi8(avg,filt);                    // 16b: A0 F0 A1 F1 A2 F2 A3 F3 A4 F4 A5 F5 A6 F6 A7 F7
        storel_epi64(dst, a); dst += pitch;                         // A0 F0 A1 F1 A2 F2 A3 F3
        storel_epi64(dst, _mm_bsrli_si128(a,2)); dst += pitch;      // A1 F1 A2 F2 A3 F3 A4 F4
        storel_epi64(dst, _mm_bsrli_si128(a,4)); dst += pitch;      // A2 F2 A3 F3 A4 F4 A5 F5
        storel_epi64(dst, _mm_bsrli_si128(a,6)); dst += pitch;      // A3 F3 A4 F4 A5 F5 A6 F6
        storel_epi64(dst, _mm_bsrli_si128(a,8)); dst += pitch;      // A4 F4 A5 F5 A6 F6 A7 F7

        storel_epi64(dst, _mm_alignr_epi8(last,a,10)); dst+=pitch;  // A5 F5 A6 F6 A7 F7 L7 L7
        storel_epi64(dst, _mm_alignr_epi8(last,a,12)); dst+=pitch;  // A6 F6 A7 F7 L7 L7 L7 L7
        storel_epi64(dst, last);                                    // L7 L7 L7 L7 L7 L7 L7 L7
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, D207_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pl = refPel+1+16*2;
        __m128i x = loada_si128(pl);                    // 16b: L00 L01 .. L14 L15
        __m128i last = _mm_bsrli_si128(x,15);
        last = _mm_broadcastb_epi8(last);               // 16b: L15 L15 .. L15 L15
        __m128i y = _mm_alignr_epi8(last, x, 1);        // 16b: L01 L02 .. L15 L15
        __m128i z = _mm_alignr_epi8(last, x, 2);        // 16b: L02 L03 .. L15 L15
        __m128i avg = _mm_avg_epu8(x, y);               // 16b: A00 A01 .. A14 A15  // average
        __m128i filt = filt_121(x, y, z);               // 16b: F00 F01 .. F14 F15  // 121-filtered
        x = _mm_unpacklo_epi8(avg, filt);               // 16b: A00 F00 .. A07 F07
        y = _mm_unpackhi_epi8(avg, filt);               // 16b: A08 F08 .. A15 F15
        __m256i a = _mm256_setr_m128i(x,y);
        __m256i b = _mm256_setr_m128i(y,last);
        uint8_t *dst2 = dst + 8*pitch;
        storea_si128(dst, x); dst += pitch;
        storea_si128(dst2, y); dst2 += pitch;
        storeu2_m128i(dst, dst2, _mm256_alignr_epi8(b,a,2)); dst += pitch; dst2 += pitch;
        storeu2_m128i(dst, dst2, _mm256_alignr_epi8(b,a,4)); dst += pitch; dst2 += pitch;
        storeu2_m128i(dst, dst2, _mm256_alignr_epi8(b,a,6)); dst += pitch; dst2 += pitch;
        storeu2_m128i(dst, dst2, _mm256_alignr_epi8(b,a,8)); dst += pitch; dst2 += pitch;
        storeu2_m128i(dst, dst2, _mm256_alignr_epi8(b,a,10)); dst += pitch; dst2 += pitch;
        storeu2_m128i(dst, dst2, _mm256_alignr_epi8(b,a,12)); dst += pitch; dst2 += pitch;
        storeu2_m128i(dst, dst2, _mm256_alignr_epi8(b,a,14));
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, D207_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pl = refPel+1+32*2;

        __m256i x = loadu_si256(pl);                                        // 32b: L00 L01 .. L14 L15 | L16 L17 .. L30 L31
        __m256i last = _mm256_set1_epi8(pl[31]);                            // 32b: L31 L31 .. L31 L31 | L31 L31 .. L31 L31
        __m256i addin = _mm256_permute2x128_si256(x, last, PERM2x128(1,2)); // 32b: L16 L17 .. L30 L31 | L31 L31 .. L31 L31
        __m256i y = _mm256_alignr_epi8(addin, x, 1);                        // 32b: L01 L02 .. L15 L16 | L17 L18 .. L31 L31
        __m256i z = _mm256_alignr_epi8(addin, x, 2);                        // 32b: L02 L03 .. L16 L17 | L18 L19 .. L31 L31

        __m256i avg = _mm256_avg_epu8(x, y);    // 32b: A00 A01 .. A14 A15 | A16 A17 .. A30 A31 // average

        __m256i filt = filt_121(x, y, z);       // 32b: F00 F01 .. F14 F15 | F16 F17 .. F30 F31 // 121-filtered

        __m256i a,b,c,d;
        x = _mm256_unpacklo_epi8(avg, filt);                    // 32b: A00 F00 .. A07 F07 | A16 F16 .. A23 F23
        y = _mm256_unpackhi_epi8(avg, filt);                    // 32b: A08 F08 .. A15 F15 | A24 F24 .. A31 F31
        a = _mm256_permute2x128_si256(x,y,PERM2x128(0,2));      // 32b: A00 F00 .. A07 F07 | A08 F08 .. A15 F15
        b = _mm256_permute2x128_si256(x,y,PERM2x128(2,1));      // 32b: A08 F08 .. A15 F15 | A16 F16 .. A23 F23
        c = _mm256_permute2x128_si256(x,y,PERM2x128(1,3));      // 32b: A16 F16 .. A23 F23 | A24 F24 .. A31 F31
        d = _mm256_permute2x128_si256(last,y,PERM2x128(3,0));   // 32b: A24 F24 .. A31 F31 | L31 L31 .. L31 L31

        storea_si256(dst, a); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 2)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a,10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a,12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(b, a,14)); dst += pitch;
        storea_si256(dst, b); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(c, b, 2)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(c, b, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(c, b, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(c, b, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(c, b,10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(c, b,12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(c, b,14)); dst += pitch;
        storea_si256(dst, c); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(d, c, 2)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(d, c, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(d, c, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(d, c, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(d, c,10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(d, c,12)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(d, c,14)); dst += pitch;
        storea_si256(dst, d); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(last, d, 2)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(last, d, 4)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(last, d, 6)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(last, d, 8)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(last, d,10)); dst += pitch;
        storea_si256(dst, _mm256_alignr_epi8(last, d,12)); dst += pitch;
        storea_si256(dst, last);
    }

    template <> void predict_intra_vp9_avx2<TX_4X4, D63_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel+1;
        __m128i x = loada_si128(pa);                    // 01234567********
        __m128i y = _mm_bsrli_si128(x, 1);              // 1234567*********
        __m128i z = _mm_bsrli_si128(x, 2);              // 234567**********
        __m128i row0 = _mm_avg_epu8(x, y);              // B0 B1 B2 B3 B4 B5 ** ** ** ** ** ** ** ** ** ** // 1st row - average
        __m128i row1 = filt_121(x, y, z);               // C0 C1 C2 C3 C4 C5 ** ** ** ** ** ** ** ** ** ** // 2nd row - 121-filtered
        __m128i row2 = _mm_bsrli_si128(row0, 1);        // B1 B2 B3 B4 B5 ** ** ** ** ** ** ** ** ** ** **
        __m128i row3 = _mm_bsrli_si128(row1, 1);        // C1 C2 C3 C4 C5 ** ** ** ** ** ** ** ** ** ** **
        int p = pitch;
        *(int*)(dst)     = _mm_cvtsi128_si32(row0);
        *(int*)(dst+p)   = _mm_cvtsi128_si32(row1);
        *(int*)(dst+p*2) = _mm_cvtsi128_si32(row2);
        *(int*)(dst+p*3) = _mm_cvtsi128_si32(row3);
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, D63_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel+1;
        __m128i x = loada_si128(pa);                    // 01234567********
        __m128i ar = _mm_set1_epi8(pa[7]);              // 7777777777777777
        x = _mm_unpacklo_epi64(x, ar);                  // 0123456777777777
        __m128i y = _mm_bsrli_si128(x, 1);              // 123456777777777*
        __m128i z = _mm_bsrli_si128(x, 2);              // 23456777777777**
        __m128i row0 = _mm_avg_epu8(x, y);              // B0 B1 B2 B3 B4 B5 B6 B7 ** ** ** ** ** ** ** ** // 1st row - average
        __m128i row1 = filt_121(x, y, z);               // C0 C1 C2 C3 C4 C5 C6 C7 ** ** ** ** ** ** ** ** // 2nd row - 121-filtered
        int p = pitch;
        int p2 = pitch*2;
        storel_epi64(dst,   row0);
        storel_epi64(dst+p, row1); dst+=p2;
        storel_epi64(dst,   _mm_bsrli_si128(row0,1));
        storel_epi64(dst+p, _mm_bsrli_si128(row1,1)); dst+=p2;
        storel_epi64(dst,   _mm_bsrli_si128(row0,2));
        storel_epi64(dst+p, _mm_bsrli_si128(row1,2)); dst+=p2;
        storel_epi64(dst,   _mm_bsrli_si128(row0,3));
        storel_epi64(dst+p, _mm_bsrli_si128(row1,3));
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, D63_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel+1;
        __m128i x = loada_si128(pa);                    // 0123456789abcdef
        __m128i ar = _mm_set1_epi8(pa[15]);             // above[15]
        __m128i y = _mm_alignr_epi8(ar, x, 1);          // 123456789abcdeff
        __m128i z = _mm_alignr_epi8(ar, x, 2);          // 23456789abcdefff

        __m128i row0 = _mm_avg_epu8(x, y);              // 1st row - average
        __m128i row1 = filt_121(x, y, z);               // 2nd row - 121-filtered
        __m256i rows = _mm256_setr_m128i(row0, row1);
        __m256i addin = _mm256_broadcastsi128_si256(ar);

        int p = pitch;
        int p2 = pitch*2;
        storeu2_m128i(dst, dst+p, rows); dst+=p2;
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(addin, rows, 1)); dst+=p2;
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(addin, rows, 2)); dst+=p2;
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(addin, rows, 3)); dst+=p2;
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(addin, rows, 4)); dst+=p2;
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(addin, rows, 5)); dst+=p2;
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(addin, rows, 6)); dst+=p2;
        storeu2_m128i(dst, dst+p, _mm256_alignr_epi8(addin, rows, 7));
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, D63_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel+1;
        __m256i x = loada_si256(pa);                    // 0123456789abcdef ghiklmnopqrstuvw
        __m256i ar = _mm256_set1_epi8(pa[31]);           // above[31]
        __m256i addin = _mm256_permute2x128_si256(x, ar, PERM2x128(1,2));
        __m256i y = _mm256_alignr_epi8(addin, x, 1);    // 123456789abcdefg hiklmnopqrstuvww
        __m256i z = _mm256_alignr_epi8(addin, x, 2);    // 23456789abcdefgh iklmnopqrstuvwww

        __m256i row0 = _mm256_avg_epu8(x, y);           // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf | Bg Bh Bi Bk Bl Bm Bn Bo Bp Bq Br Bs Bt Bu Bv Bw (even rows - average)
        __m256i row1 = filt_121(x, y, z);               // C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf | Cg Ch Ci Ck Cl Cm Cn Co Cp Cq Cr Cs Ct Cu Cv Cw (odd rows - 121-filtered)

        int p = pitch;
        int p2 = pitch*2;
        __m256i addin0 = _mm256_permute2x128_si256(row0, ar, PERM2x128(1,2));
        __m256i addin1 = _mm256_permute2x128_si256(row1, ar, PERM2x128(1,2));
        storea_si256(dst,   row0);                                      // B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf | Bg Bh Bi Bk Bl Bm Bn Bo Bp Bq Br Bs Bt Bu Bv Bw
        storea_si256(dst+p, row1); dst+=p2;                             // C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf | Cg Ch Ci Ck Cl Cm Cn Co Cp Cq Cr Cs Ct Cu Cv Cw
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,1));         // B1 B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf Bg | Bh Bi Bk Bl Bm Bn Bo Bp Bq Br Bs Bt Bu Bv Bw Bw
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,1)); dst+=p2;// C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf Cg | Ch Ci Ck Cl Cm Cn Co Cp Cq Cr Cs Ct Cu Cv Cw Cw
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,2));         // B2 B3 B4 B5 B6 B7 B8 B9 Ba Bb Bc Bd Be Bf Bg Bh | Bi Bk Bl Bm Bn Bo Bp Bq Br Bs Bt Bu Bv Bw Bw Bw
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,2)); dst+=p2;// C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf Cg Ch | Ci Ck Cl Cm Cn Co Cp Cq Cr Cs Ct Cu Cv Cw Cw Cw
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,3));         // etc
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,3)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,4));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,4)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,5));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,5)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,6));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,6)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,7));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,7)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,8));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,8)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,9));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,9)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,10));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,10)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,11));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,11)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,12));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,12)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,13));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,13)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,14));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,14)); dst+=p2;
        storea_si256(dst,   _mm256_alignr_epi8(addin0,row0,15));
        storea_si256(dst+p, _mm256_alignr_epi8(addin1,row1,15));
    }

    template <> void predict_intra_vp9_avx2<TX_4X4, TM_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *a = refPel + 1;
        const uint8_t *l = refPel + 1 + 8;
        __m128i above_128 = _mm_set1_epi32(*(int*)a);
        __m128i left_128 = _mm_cvtsi32_si128(*(int*)l);  // 0123************
        left_128 = _mm_unpacklo_epi8(left_128,left_128);    // 00112233********
        left_128 = _mm_unpacklo_epi16(left_128,left_128);   // 0000111122223333
        __m256i left = _mm256_cvtepu8_epi16(left_128);
        __m256i above = _mm256_cvtepu8_epi16(above_128);
        __m256i aboveLeft = _mm256_set1_epi16(a[-1]);
        __m256i res = _mm256_sub_epi16(above, aboveLeft);
        res = _mm256_add_epi16(res, left);
        res = _mm256_packus_epi16(res, res);

        __m128i res_128 = si128(res);
        int p = pitch;
        *(int*)(dst)   = _mm_cvtsi128_si32(res_128);
        *(int*)(dst+p) = _mm_cvtsi128_si32(_mm_bsrli_si128(res_128,4));

        res_128 = _mm256_extracti128_si256(res, 1);
        *(int*)(dst+p*2) = _mm_cvtsi128_si32(res_128);
        *(int*)(dst+p*3) = _mm_cvtsi128_si32(_mm_bsrli_si128(res_128,4));
    }
    template <> void predict_intra_vp9_avx2<TX_8X8, TM_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *pa = refPel + 1;
        const uint8_t *pl = refPel + 1 + 16;
        __m128i l = loadl_epi64(pl);                    // 01234567********
        l = _mm_unpacklo_epi8(l,l);                     // 0011223344556677
        __m128i lo = _mm_unpacklo_epi16(l,l);           // 0000111122223333
        __m128i hi = _mm_unpackhi_epi16(l,l);           // 4444555566667777
        __m128i lolo = _mm_unpacklo_epi32(lo,lo);       // 0000000011111111
        __m128i lohi = _mm_unpackhi_epi32(lo,lo);       // 2222222233333333
        __m128i hilo = _mm_unpacklo_epi32(hi,hi);       // 4444444455555555
        __m128i hihi = _mm_unpackhi_epi32(hi,hi);       // 6666666677777777

        __m256i a = _mm256_cvtepu8_epi16(si128(_mm_load1_pd((double*)pa)));     // 01234567 01234567
        __m256i al = _mm256_set1_epi16(pa[-1]);                                 // aboveleft
        __m256i above = _mm256_sub_epi16(a, al);
        __m256i row01 = _mm256_add_epi16(above, _mm256_cvtepu8_epi16(lolo));
        __m256i row23 = _mm256_add_epi16(above, _mm256_cvtepu8_epi16(lohi));
        __m256i row45 = _mm256_add_epi16(above, _mm256_cvtepu8_epi16(hilo));
        __m256i row67 = _mm256_add_epi16(above, _mm256_cvtepu8_epi16(hihi));

        __m256i row0213 = _mm256_packus_epi16(row01, row23);    // 00000000222222221111111133333333
        __m256i row4657 = _mm256_packus_epi16(row45, row67);    // 44444444666666665555555577777777

        int p = pitch;
        int p2 = pitch*2;
        storel_epi64(dst,   si128(row0213));
        storel_epi64(dst+p, _mm256_extracti128_si256(row0213,1)); dst+=p2;
        storeh_epi64(dst,   si128(row0213));
        storeh_epi64(dst+p, _mm256_extracti128_si256(row0213,1)); dst+=p2;
        storel_epi64(dst,   si128(row4657));
        storel_epi64(dst+p, _mm256_extracti128_si256(row4657,1)); dst+=p2;
        storeh_epi64(dst,   si128(row4657));
        storeh_epi64(dst+p, _mm256_extracti128_si256(row4657,1));
    }
    template <> void predict_intra_vp9_avx2<TX_16X16, TM_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *a = refPel + 1;
        const uint8_t *l = refPel + 1 + 32;
        __m128i above_128 = loada_si128(a);
        __m256i above = _mm256_cvtepu8_epi16(above_128);
        __m256i aboveLeft = _mm256_set1_epi16(a[-1]);
        above = _mm256_sub_epi16(above, aboveLeft);

        __m256i left = _mm256_cvtepu8_epi16(loada_si128(l));    // 0123456789abcdef
        __m256i lo = _mm256_unpacklo_epi16(left,left);          // 001122338899aabb
        __m256i hi = _mm256_unpackhi_epi16(left,left);          // 44556677ccddeeff
        __m256i lolo = _mm256_unpacklo_epi16(lo,lo);            // 0000111188889999
        __m256i lohi = _mm256_unpackhi_epi16(lo,lo);            // 22223333aaaabbbb
        __m256i hilo = _mm256_unpacklo_epi16(hi,hi);            // 44445555ccccdddd
        __m256i hihi = _mm256_unpackhi_epi16(hi,hi);            // 55556666eeeeffff
        __m256i l08 = _mm256_unpacklo_epi16(lolo,lolo);         // 0000000088888888
        __m256i l19 = _mm256_unpackhi_epi16(lolo,lolo);         // 1111111199999999
        __m256i l2a = _mm256_unpacklo_epi16(lohi,lohi);         // 22222222aaaaaaaa
        __m256i l3b = _mm256_unpackhi_epi16(lohi,lohi);         // 33333333bbbbbbbb
        __m256i l4c = _mm256_unpacklo_epi16(hilo,hilo);         // 44444444cccccccc
        __m256i l5d = _mm256_unpackhi_epi16(hilo,hilo);         // 55555555dddddddd
        __m256i l6e = _mm256_unpacklo_epi16(hihi,hihi);         // 66666666eeeeeeee
        __m256i l7f = _mm256_unpackhi_epi16(hihi,hihi);         // 77777777ffffffff

        __m256i above_lo = _mm256_permute2x128_si256(above,above,PERM2x128(0,0));
        __m256i above_hi = _mm256_permute2x128_si256(above,above,PERM2x128(1,1));

        __m256i rows;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l08), _mm256_add_epi16(above_hi, l08));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l19), _mm256_add_epi16(above_hi, l19));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l2a), _mm256_add_epi16(above_hi, l2a));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l3b), _mm256_add_epi16(above_hi, l3b));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l4c), _mm256_add_epi16(above_hi, l4c));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l5d), _mm256_add_epi16(above_hi, l5d));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l6e), _mm256_add_epi16(above_hi, l6e));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
        rows = _mm256_packus_epi16(_mm256_add_epi16(above_lo, l7f), _mm256_add_epi16(above_hi, l7f));
        storeu2_m128i(dst, dst+pitch*8, rows); dst += pitch;
    }
    template <> void predict_intra_vp9_avx2<TX_32X32, TM_PRED>(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const uint8_t *a = refPel + 1;
        const uint8_t *l = refPel + 1 + 64;
        __m256i above = loada_si256(a);
        __m256i aboveLeft = _mm256_set1_epi16(a[-1]);
        __m256i above_lo = _mm256_unpacklo_epi8(above, _mm256_setzero_si256());
        __m256i above_hi = _mm256_unpackhi_epi8(above, _mm256_setzero_si256());
        above_lo = _mm256_sub_epi16(above_lo, aboveLeft);
        above_hi = _mm256_sub_epi16(above_hi, aboveLeft);

        for (int i = 0; i < 32; i++, dst += pitch) {
            __m256i left = _mm256_set1_epi16(l[i]);
            __m256i row_lo = _mm256_add_epi16(above_lo, left);
            __m256i row_hi = _mm256_add_epi16(above_hi, left);
            __m256i row = _mm256_packus_epi16(row_lo, row_hi);
            storea_si256(dst, row);
        }
    }

    template <int32_t txSize> void make_above_row(const uint8_t *above, uint8_t *aboveRow);
    template <> void make_above_row<0>(const uint8_t *above, uint8_t *aboveRow) {
        __m128i a = loadl_si32(above); // 4 above pixels
        __m128i dup = _mm_shuffle_epi8(_mm_bsrli_si128(a, 3), _mm_setzero_si128()); // 3rd pixel broadcast
        storel_epi64(aboveRow, _mm_unpacklo_epi32(a, dup)); // 4 above + 4 * 3rd pixel
    }
    template <> void make_above_row<1>(const uint8_t *above, uint8_t *aboveRow) {
        __m128i a = loadl_epi64(above); // 8 above pixels
        __m128i dup = _mm_shuffle_epi8(_mm_bsrli_si128(a, 7), _mm_setzero_si128()); // 7th pixel broadcast
        storea_si128(aboveRow, _mm_unpacklo_epi64(a, dup)); // 8 above + 8*7th pixel
    }
    template <> void make_above_row<2>(const uint8_t *above, uint8_t *aboveRow) {
        __m128i a = loada_si128(above); // 16 above pixels
        storea_si128(aboveRow, a);
        __m128i dup = _mm_shuffle_epi8(_mm_bsrli_si128(a, 15), _mm_setzero_si128()); // 15th pixel broadcast
        storea_si128(aboveRow + 16, dup);
    }
    template <> void make_above_row<3>(const uint8_t *above, uint8_t *aboveRow) {
        __m128i a = loada_si128(above); // 16 above pixels
        storea_si128(aboveRow, a);
        a = loada_si128(above + 16); // another 16 above pixels
        storea_si128(aboveRow + 16, a);
        __m128i dup = _mm_shuffle_epi8(_mm_bsrli_si128(a, 15), _mm_setzero_si128()); // 31st pixel broadcast
        storea_si128(aboveRow + 32, dup);
        storea_si128(aboveRow + 48, dup);
    }

    template <int width, int fillValue> void fill_with(uint8_t *dst) {
        assert(width == 4 || ((width & 7) == 0));
        if (width == 4) {
            uint32_t fillValue32 = fillValue + (fillValue << 8);
            fillValue32 += fillValue32 << 16;
            *(uint32_t *)dst = fillValue32;
        } else {
            uint64_t fillValue64 = fillValue + (fillValue << 8);
            fillValue64 += fillValue64 << 16;
            fillValue64 += fillValue64 << 32;
            for (int32_t i = 0; i < (width >> 3); i++)
                ((uint64_t *)dst)[i] = fillValue64;
        }
    }

    template <int txSize, int haveLeft, int haveAbove> void get_pred_pels(const uint8_t* rec, int pitchRec, uint8_t *predPels) {
        const int width = 4 << txSize;
        uint8_t *aboveRow = predPels + 1;
        uint8_t *leftCol  = aboveRow + 2*width;

        const uint8_t *aboveRec = rec - pitchRec;
        const uint8_t *leftRec = rec - 1;

        if (haveAbove && haveLeft) {
            aboveRow[-1] = aboveRec[-1];
            make_above_row<txSize>(aboveRec, aboveRow);
            for (int32_t i = 0; i < width; i++, leftRec += pitchRec)
                leftCol[i] = *leftRec;
        } else if (haveAbove) {
            aboveRow[-1] = 129;
            make_above_row<txSize>(aboveRec, aboveRow);
            fill_with<width, 129>(leftCol);
        } else if (haveLeft) {
            aboveRow[-1] = 127;
            fill_with<width << 1, 127>(aboveRow);
            for (int32_t i = 0; i < width; i++, rec += pitchRec)
                leftCol[i] = rec[-1];
        } else {
            aboveRow[-1] = 127;
            fill_with<width << 1, 127>(aboveRow);
            fill_with<width, 129>(leftCol);
        }
    }

    template <int txSize, int haveLeft, int haveAbove> void predict_intra_all_avx2(const uint8_t* rec, int pitchRec, uint8_t *dst) {
        const int width = 4 << txSize;

        ALIGN_DECL(32) uint8_t predPels_[32 + width * 3];
        uint8_t *predPels = predPels_ + 31;

        get_pred_pels<txSize, haveLeft, haveAbove>(rec, pitchRec, predPels);

        predict_intra_dc_vp9_avx2<txSize, haveLeft, haveAbove>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, V_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, H_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, D45_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, D135_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, D117_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, D153_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, D207_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, D63_PRED>(predPels, dst, 10*width); dst += width;
        predict_intra_vp9_avx2<txSize, TM_PRED>(predPels, dst, 10*width);
    }
    template void predict_intra_all_avx2<0, 0, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<0, 0, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<0, 1, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<0, 1, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<1, 0, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<1, 0, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<1, 1, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<1, 1, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<2, 0, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<2, 0, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<2, 1, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<2, 1, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<3, 0, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<3, 0, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<3, 1, 0>(const uint8_t* rec, int pitchRec, uint8_t *dst);
    template void predict_intra_all_avx2<3, 1, 1>(const uint8_t* rec, int pitchRec, uint8_t *dst);

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
        template <> void predict_intra_paeth_av1_avx2<TX_4X4  >(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
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

            const int p = pitch;
            *(int*)(dst)     = _mm_cvtsi128_si32(row);
            *(int*)(dst+p)   = _mm_cvtsi128_si32(_mm_bsrli_si128(row, 4));
            *(int*)(dst+p*2) = _mm_cvtsi128_si32(_mm_bsrli_si128(row, 8));
            *(int*)(dst+p*3) = _mm_cvtsi128_si32(_mm_bsrli_si128(row, 12));
        }
        template <> void predict_intra_paeth_av1_avx2<TX_8X8  >(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
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
            storel_epi64(dst,         si128(row));
            storeh_epi64(dst+pitch,   si128(row));
            storel_epi64(dst+pitch*2, _mm256_extracti128_si256(row, 1));
            storeh_epi64(dst+pitch*3, _mm256_extracti128_si256(row, 1));
            dst += pitch*4;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col, 4), shuftab);    // 4444444455555555 6666666677777777
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above, 4), shuftab);
            row = filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel);
            storel_epi64(dst,         si128(row));
            storeh_epi64(dst+pitch,   si128(row));
            storel_epi64(dst+pitch*2, _mm256_extracti128_si256(row, 1));
            storeh_epi64(dst+pitch*3, _mm256_extracti128_si256(row, 1));
        }
        template <> void predict_intra_paeth_av1_avx2<TX_16X16>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;

            __m128i a_ = loada_si128(pa);
            __m128i l_ = loada_si128(pl);
            __m256i above_row = _mm256_setr_m128i(a_, a_);                      // 0123456789abcdef 0123456789abcdef
            __m256i left_col = _mm256_setr_m128i(l_, _mm_bsrli_si128(l_, 1));   // 0123456789abcdef 123456789abcdef.
            __m256i above_left_pel = _mm256_set1_epi8(topPels[-1]);

            __m256i diff_left = abs_diff(above_row, above_left_pel);
            __m256i diff_above = abs_diff(left_col, above_left_pel);

            __m256i left_pel, diff_above_pel;

            left_pel = _mm256_shuffle_epi8(left_col, _mm256_setzero_si256());                            // 0000000000000000 1111111111111111
            diff_above_pel = _mm256_shuffle_epi8(diff_above, _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col, 2), _mm256_setzero_si256());    // 2222222222222222 3333333333333333
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above, 2), _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col, 4), _mm256_setzero_si256());    // 4444444444444444 5555555555555555
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above, 4), _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col, 6), _mm256_setzero_si256());    // 6666666666666666 7777777777777777
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above, 6), _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col, 8), _mm256_setzero_si256());    // 8888888888888888 9999999999999999
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above, 8), _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col,10), _mm256_setzero_si256());    // aaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbb
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above,10), _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col,12), _mm256_setzero_si256());    // cccccccccccccccc dddddddddddddddd
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above,12), _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left_col,14), _mm256_setzero_si256());    // eeeeeeeeeeeeeeee ffffffffffffffff
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above,14), _mm256_setzero_si256());
            storeu2_m128i(dst, dst+pitch, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch*2;
        }
        template <> void predict_intra_paeth_av1_avx2<TX_32X32>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;

            __m256i above_row = loada_si256(pa);
            __m256i left_col = loada_si256(pl);
            __m256i above_left_pel = _mm256_set1_epi8(topPels[-1]);

            __m256i diff_left = abs_diff(above_row, above_left_pel);
            __m256i diff_above = abs_diff(left_col, above_left_pel);

            __m256i l_lo = _mm256_permute4x64_epi64(left_col, PERM4x64(0,1,0,1));               // 0123456789abcdef 0123456789abcdef
            __m256i l_hi = _mm256_permute4x64_epi64(left_col, PERM4x64(2,3,2,3));               // ghiklmnopqrstuvw ghiklmnopqrstuvw
            __m256i diff_above_lo = _mm256_permute4x64_epi64(diff_above, PERM4x64(0,1,0,1));    // 0123456789abcdef 0123456789abcdef
            __m256i diff_above_hi = _mm256_permute4x64_epi64(diff_above, PERM4x64(2,3,2,3));    // ghiklmnopqrstuvw ghiklmnopqrstuvw

            __m256i left_pel, diff_above_pel;

            left_pel = _mm256_shuffle_epi8(l_lo, _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(diff_above_lo, _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 1), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 1), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 2), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 2), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 3), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 3), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 4), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 4), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 5), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 5), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 6), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 6), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 7), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 7), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 8), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 8), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo, 9), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo, 9), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo,10), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo,10), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo,11), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo,11), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo,12), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo,12), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo,13), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo,13), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo,14), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo,14), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_lo,15), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_lo,15), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;


            left_pel = _mm256_shuffle_epi8(l_hi, _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(diff_above_hi, _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 1), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 1), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 2), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 2), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 3), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 3), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 4), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 4), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 5), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 5), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 6), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 6), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 7), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 7), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 8), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 8), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi, 9), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi, 9), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi,10), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi,10), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi,11), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi,11), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi,12), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi,12), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi,13), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi,13), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi,14), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi,14), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
            dst += pitch;

            left_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(l_hi,15), _mm256_setzero_si256());
            diff_above_pel = _mm256_shuffle_epi8(_mm256_bsrli_epi128(diff_above_hi,15), _mm256_setzero_si256());
            storea_si256(dst, filter_row_paeth(above_row, left_pel, above_left_pel, diff_left, diff_above_pel));
        }

        template <int size> void predict_intra_smooth_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch);
        template <> void predict_intra_smooth_av1_avx2<TX_4X4>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
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

            __m128i lo = si128(a);
            __m128i hi = _mm256_extracti128_si256(a, 1);

            *(int *)dst = _mm_cvtsi128_si32(lo); dst += pitch;
            *(int *)dst = _mm_cvtsi128_si32(_mm_bsrli_si128(lo, 4)); dst += pitch;
            *(int *)dst = _mm_cvtsi128_si32(hi); dst += pitch;
            *(int *)dst = _mm_cvtsi128_si32(_mm_bsrli_si128(hi, 4));
        }
        template <> void predict_intra_smooth_av1_avx2<TX_8X8>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;

            __m128i above_ = loaddup_epi64(pa);                         // 16b: A0 A1 A2 A3 A4 A5 A6 A7 A0 A1 A2 A3 A4 A5 A6 A7
            __m256i above = _mm256_cvtepu8_epi16(above_);               // 16w: A0 A1 A2 A3 A4 A5 A6 A7 | A0 A1 A2 A3 A4 A5 A6 A7
            __m128i left_ = loadl_epi64(pl);                            // 16b: L0 L1 L2 L3 L4 L5 L6 L7 ** ** ** ** ** ** ** **
            __m128i left_shifted = _mm_bsrli_si128(left_, 1);           // 16b: L1 L2 L3 L4 L5 L6 L7 ** ** ** ** ** ** ** ** **
            __m256i left = _mm256_set_m128i(left_shifted, left_);       // 32b: L0 L1 L2 L3 L4 L5 L6 L7 ** ** ** ** ** ** ** ** | L1 L2 L3 L4 L5 L6 L7 ** ** ** ** ** ** ** ** **
            __m256i below = _mm256_set1_epi16(pl[7]);                   // 16w: B B B B B B B B | B B B B B B B B
            __m256i right = _mm256_set1_epi16(pa[7]);                   // 16w: R R R R R R R R | R R R R R R R R
            __m128i weights_ = loadl_epi64(smooth_weights_8);           // 16b: W0 W1 W2 W3 W4 W5 W6 W7 ** ** ** ** ** ** ** **
            __m128i weights_shifted_ = _mm_bsrli_si128(weights_, 1);    // 16b: W1 W2 W3 W4 W5 W6 W7 ** ** ** ** ** ** ** ** **
            __m256i weights_dup     = _mm256_set_m128i(weights_, weights_);         // 32b: W0 W1 W2 W3 W4 W5 W6 W7 ** ** ** ** ** ** ** ** | W0 W1 W2 W3 W4 W5 W6 W7 ** ** ** ** ** ** ** **
            __m256i weights_shifted = _mm256_set_m128i(weights_shifted_, weights_); // 32b: W0 W1 W2 W3 W4 W5 W6 W7 ** ** ** ** ** ** ** ** | W1 W2 W3 W4 W5 W6 W7 ** ** ** ** ** ** ** ** **

            __m256i zero = _mm256_setzero_si256();
            __m256i weights_dup_16 = _mm256_unpacklo_epi8(weights_dup, zero);  // 16w: W0 W1 W2 W3 W4 W5 W6 W7 | W0 W1 W2 W3 W4 W5 W6 W7
            __m256i weights_inv_dup     = _mm256_sub_epi8(zero, weights_dup);
            __m256i weights_inv_shifted = _mm256_sub_epi8(zero, weights_shifted);
            __m256i weights_inv_dup_16     = _mm256_unpacklo_epi8(weights_inv_dup,     zero);
            __m256i weights_inv_shifted_16 = _mm256_unpacklo_epi8(weights_inv_shifted, zero);

            __m256i below_by_weight_inv = _mm256_mullo_epi16(below, weights_inv_shifted_16);   // 16w: 01234567 | 1234567*
            __m256i right_by_weight_inv = _mm256_mullo_epi16(right, weights_inv_dup_16);       // 16w: 01234567 | 01234567

            __m256i weight = weights_shifted;
            __m256i weight_r, left_r, a, b, c, d, lsb;

            for (int r = 0; r < 8; r += 4) {
                weight_r = _mm256_shuffle_epi8(weight, _mm256_set1_epi16(1<<15));           // 32b: W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 | W1 0 W1 0 W1 0 W1 0 W1 0 W1 0 W1 0 W1 0
                a = _mm256_mullo_epi16(above, weight_r);                                    // above[c] * sm_weights[r]
                b = _mm256_shuffle_epi8(below_by_weight_inv, _mm256_set1_epi16(1<<8));      // below_pred * (scale - sm_weights[r])
                a = _mm256_add_epi16(a, b);
                left_r = _mm256_shuffle_epi8(left, _mm256_set1_epi16(1<<15));
                b = _mm256_mullo_epi16(left_r, weights_dup_16);                             // left[r] * sm_weights[c]
                b = _mm256_add_epi16(b, right_by_weight_inv);                               // right_pred * (scale - sm_weights[c])

                lsb = _mm256_and_si256(_mm256_and_si256(a, b), _mm256_set1_epi16(1));
                a = _mm256_srli_epi16(a, 1);
                b = _mm256_srli_epi16(b, 1);
                a = _mm256_adds_epu16(a, b);
                a = _mm256_adds_epu16(a, lsb);
                a = _mm256_adds_epu16(a, _mm256_set1_epi16(128));
                a = _mm256_srli_epi16(a, 8);                                                // 32b: A0 00 A1 00 A2 00 A3 00 A4 00 A5 00 A6 00 A7 00 | A8 00 A9 00 Aa 00 Ab 00 Ac 00 Ad 00 Ae 00 Af 00


                weight_r = _mm256_shuffle_epi8(_mm256_bsrli_epi128(weight, 2), _mm256_set1_epi16(1<<15));
                c = _mm256_mullo_epi16(above, weight_r);
                d = _mm256_shuffle_epi8(_mm256_bsrli_epi128(below_by_weight_inv, 4), _mm256_set1_epi16(1<<8));
                c = _mm256_add_epi16(c, d);
                left_r = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left, 2), _mm256_set1_epi16(1<<15));
                d = _mm256_mullo_epi16(left_r, weights_dup_16);
                d = _mm256_add_epi16(d, right_by_weight_inv);

                lsb = _mm256_and_si256(_mm256_and_si256(c, d), _mm256_set1_epi16(1));
                c = _mm256_srli_epi16(c, 1);
                d = _mm256_srli_epi16(d, 1);
                c = _mm256_adds_epu16(c, d);
                c = _mm256_adds_epu16(c, lsb);
                c = _mm256_adds_epu16(c, _mm256_set1_epi16(128));
                c = _mm256_srli_epi16(c, 8);                                                // C0 00 C1 00 C2 00 C3 00 C4 00 C5 00 C6 00 C7 00 | C8 00 C9 00 Ca 00 Cb 00 Cc 00 Cd 00 Ce 00 Cf 00

                a = _mm256_packus_epi16(a, c);                                              // A0 A1 A2 A3 A4 A5 A6 A7 C0 C1 C2 C3 C4 C5 C6 C7 | A8 A9 Aa Ab Ac Ad Ae Af C8 C9 Ca Cb Cc Cd Ce Cf
                a = _mm256_permute4x64_epi64(a, PERM4x64(0,2,1,3));                         // A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae Af | C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf
                storel_epi64(dst, si128(a));
                storeh_epi64(dst+pitch, si128(a));
                storel_epi64(dst+pitch*2, _mm256_extracti128_si256(a,1));
                storeh_epi64(dst+pitch*3, _mm256_extracti128_si256(a,1));
                dst += pitch*4;

                left = _mm256_bsrli_epi128(left, 4);
                weight = _mm256_bsrli_epi128(weight, 4);
                below_by_weight_inv = _mm256_bsrli_epi128(below_by_weight_inv, 8);
            }
        }
        void predict_intra_smooth_16x16(__m256i above, __m256i left, __m256i below, __m256i right, __m256i weights_above, __m256i weights_left, uint8_t *dst, int pitch) {
            __m256i weights_above_16 = _mm256_cvtepu8_epi16(si128(weights_above));              // 16w: W0 W1 W2 W3 W4 W5 W6 W7 | W8 W9 Wa Wb Wc Wd We Wf
            __m256i weights_above_inv = _mm256_sub_epi8(_mm256_setzero_si256(), weights_above);
            __m256i weights_above_inv_16 = _mm256_cvtepu8_epi16(si128(weights_above_inv));

            __m256i weights_left_inv = _mm256_sub_epi8(_mm256_setzero_si256(), weights_left);
            __m256i weights_left_inv_16 = _mm256_cvtepu8_epi16(si128(weights_left_inv));

            __m256i below_by_weight_inv = _mm256_mullo_epi16(below, weights_left_inv_16);
            __m256i below_by_weight_inv_lo = _mm256_permute4x64_epi64(below_by_weight_inv, PERM4x64(0,1,0,1));  // 16w: 01234567 | 01234567
            __m256i below_by_weight_inv_hi = _mm256_permute4x64_epi64(below_by_weight_inv, PERM4x64(2,3,2,3));  // 16w: 89abcdef | 89abcdef

            __m256i right_by_weight_inv = _mm256_mullo_epi16(right, weights_above_inv_16);

            __m256i weight = weights_left;
            __m256i weight_r, left_r, a, b, c, d, lsb;

            for (int r = 0; r < 16; r += 2) {
                weight_r = _mm256_shuffle_epi8(weight, _mm256_set1_epi16(1<<15));           // 32b: W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 | W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 W0 0 W0 0
                a = _mm256_mullo_epi16(above, weight_r);                                    // above[c] * sm_weights[r]
                b = _mm256_shuffle_epi8(below_by_weight_inv_lo, _mm256_set1_epi16(1<<8));   // below_pred * (scale - sm_weights[r])
                a = _mm256_add_epi16(a, b);
                left_r = _mm256_shuffle_epi8(left, _mm256_set1_epi16(1<<15));
                b = _mm256_mullo_epi16(left_r, weights_above_16);                           // left[r] * sm_weights[c]
                b = _mm256_add_epi16(b, right_by_weight_inv);                               // right_pred * (scale - sm_weights[c])

                lsb = _mm256_and_si256(_mm256_and_si256(a, b), _mm256_set1_epi16(1));
                a = _mm256_srli_epi16(a, 1);
                b = _mm256_srli_epi16(b, 1);
                a = _mm256_adds_epu16(a, b);
                a = _mm256_adds_epu16(a, lsb);
                a = _mm256_adds_epu16(a, _mm256_set1_epi16(128));
                a = _mm256_srli_epi16(a, 8);                                                // 32b: A0 00 A1 00 A2 00 A3 00 A4 00 A5 00 A6 00 A7 00 | A8 00 A9 00 Aa 00 Ab 00 Ac 00 Ad 00 Ae 00 Af 00


                weight_r = _mm256_shuffle_epi8(_mm256_bsrli_epi128(weight, 1), _mm256_set1_epi16(1<<15));
                c = _mm256_mullo_epi16(above, weight_r);
                d = _mm256_shuffle_epi8(_mm256_alignr_epi8(below_by_weight_inv_hi, below_by_weight_inv_lo, 2), _mm256_set1_epi16(1<<8));
                c = _mm256_add_epi16(c, d);
                left_r = _mm256_shuffle_epi8(_mm256_bsrli_epi128(left, 1), _mm256_set1_epi16(1<<15));
                d = _mm256_mullo_epi16(left_r, weights_above_16);
                d = _mm256_add_epi16(d, right_by_weight_inv);

                lsb = _mm256_and_si256(_mm256_and_si256(c, d), _mm256_set1_epi16(1));
                c = _mm256_srli_epi16(c, 1);
                d = _mm256_srli_epi16(d, 1);
                c = _mm256_adds_epu16(c, d);
                c = _mm256_adds_epu16(c, lsb);
                c = _mm256_adds_epu16(c, _mm256_set1_epi16(128));
                c = _mm256_srli_epi16(c, 8);                                                // C0 00 C1 00 C2 00 C3 00 C4 00 C5 00 C6 00 C7 00 | C8 00 C9 00 Ca 00 Cb 00 Cc 00 Cd 00 Ce 00 Cf 00
                a = _mm256_packus_epi16(a, c);                                              // A0 A1 A2 A3 A4 A5 A6 A7 C0 C1 C2 C3 C4 C5 C6 C7 | A8 A9 Aa Ab Ac Ad Ae Af C8 C9 Ca Cb Cc Cd Ce Cf
                a = _mm256_permute4x64_epi64(a, PERM4x64(0,2,1,3));                         // A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 Aa Ab Ac Ad Ae Af | C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 Ca Cb Cc Cd Ce Cf
                storea_si128(dst, si128(a));
                storea_si128(dst+pitch, _mm256_extracti128_si256(a,1));
                dst += pitch*2;

                left = _mm256_bsrli_epi128(left, 2);
                weight = _mm256_bsrli_epi128(weight, 2);
                below_by_weight_inv_lo = _mm256_alignr_epi8(below_by_weight_inv_hi, below_by_weight_inv_lo, 4);
                below_by_weight_inv_hi = _mm256_bsrli_epi128(below_by_weight_inv_hi, 4);
            }
        }
        template <> void predict_intra_smooth_av1_avx2<TX_16X16>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;

            __m256i above = _mm256_cvtepu8_epi16(loada_si128(pa));      // 16w: A0 A1 A2 A3 A4 A5 A6 A7 | A8 A9 Aa Ab Ac Ad Ae Af
            __m128i left_ = loada_si128(pl);                            // 16b: L0 L1 L2 L3 L4 L5 L6 L7 L8 L9 La Lb Lc Ld Le Lf
            __m256i left = _mm256_set_m128i(left_, left_);              // 32b: L0 L1 L2 L3 L4 L5 L6 L7 L8 L9 La Lb Lc Ld Le Lf | L0 L1 L2 L3 L4 L5 L6 L7 L8 L9 La Lb Lc Ld Le Lf
            __m256i below = _mm256_set1_epi16(pl[15]);                  // 16w: B B B B B B B B | B B B B B B B B
            __m256i right = _mm256_set1_epi16(pa[15]);                  // 16w: R R R R R R R R | R R R R R R R R
            __m256i weights = loada_si256(smooth_weights_16);           // 32b: W0 W1 W2 W3 W4 W5 W6 W7 W8 W9 Wa Wb Wc Wd We Wf | W0 W1 W2 W3 W4 W5 W6 W7 W8 W9 Wa Wb Wc Wd We Wf

            predict_intra_smooth_16x16(above, left, below, right, weights, weights, dst, pitch);
        }
        template <> void predict_intra_smooth_av1_avx2<TX_32X32>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch) {
            const uint8_t *pa = topPels;
            const uint8_t *pl = leftPels;

            __m256i above_lo = _mm256_cvtepu8_epi16(loada_si128(pa));   // 16w: A0 A1 A2 A3 A4 A5 A6 A7 | A8 A9 Aa Ab Ac Ad Ae Af
            __m256i above_hi = _mm256_cvtepu8_epi16(loada_si128(pa+16));// 16w: Ag Ah Ai Ak Al Am An Ao | Ap Aq Ar As At Au Av Aw
            __m256i left = loada_si256(pl);                             // 32b: L0 L1 L2 L3 L4 L5 L6 L7 L8 L9 La Lb Lc Ld Le Lf | Lg Lh Li Lk Ll Lm Ln Lo Lp Lq Lr Ls Lt Lu Lv Lw
            __m256i left_lo = _mm256_permute4x64_epi64(left, PERM4x64(0,1,0,1));
            __m256i left_hi = _mm256_permute4x64_epi64(left, PERM4x64(2,3,2,3));
            __m256i below = _mm256_set1_epi16(pl[31]);                  // 16w: B B B B B B B B | B B B B B B B B
            __m256i right = _mm256_set1_epi16(pa[31]);                  // 16w: R R R R R R R R | R R R R R R R R
            __m256i weights = loada_si256(smooth_weights_32);           // 32b: W0 W1 W2 W3 W4 W5 W6 W7 W8 W9 Wa Wb Wc Wd We Wf | W0 W1 W2 W3 W4 W5 W6 W7 W8 W9 Wa Wb Wc Wd We Wf
            __m256i weights_lo = _mm256_permute4x64_epi64(weights, PERM4x64(0,1,0,1));
            __m256i weights_hi = _mm256_permute4x64_epi64(weights, PERM4x64(2,3,2,3));

            predict_intra_smooth_16x16(above_lo, left_lo, below, right, weights_lo, weights_lo, dst, pitch);
            predict_intra_smooth_16x16(above_hi, left_lo, below, right, weights_hi, weights_lo, dst + 16, pitch);
            predict_intra_smooth_16x16(above_lo, left_hi, below, right, weights_lo, weights_hi, dst + 16 * pitch, pitch);
            predict_intra_smooth_16x16(above_hi, left_hi, below, right, weights_hi, weights_hi, dst + 16 * pitch + 16, pitch);
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


    template <int size> void convert_ref_pels_vp9(const uint8_t *refPel, uint8_t *refPelU, uint8_t *refPelV) {
        const int width =  4 << size;
        refPelU[0] = refPel[0];
        refPelV[0] = refPel[1];
        if (size == 0) {
            __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
            __m128i a = loada_si128(refPel + 2);  // above 8*2 bytes
            __m128i u  = _mm_and_si128(a, mask);
            __m128i v  = _mm_srli_epi16(a, 8);
            __m128i tmp = _mm_packus_epi16(u, v);
            storel_epi64(refPelU + 1, tmp);
            storeh_epi64(refPelV + 1, tmp);
            __m128i l = loadl_epi64(refPel + 2 + 4 * width);  // left 4*2 bytes
            u  = _mm_and_si128(l, mask);
            v  = _mm_srli_epi16(l, 8);
            tmp = _mm_packus_epi16(u, v);
            storel_si32(refPelU + 1 + 2 * width, tmp);
            storel_si32(refPelV + 1 + 2 * width, _mm_bsrli_si128(tmp, 8));

        } else if (size == 1) {
            __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
            __m128i uv = loada_si128(refPel + 2);  // above 8*2 bytes
            __m128i u  = _mm_and_si128(uv, mask);
            __m128i v  = _mm_srli_epi16(uv, 8);
            __m128i tmp = _mm_packus_epi16(u, v);
            storel_epi64(refPelU + 1, tmp);
            storeh_epi64(refPelV + 1, tmp);

            uv  = loada_si128(refPel + 2 + 4 * width);  // left 8*2 bytes
            u  = _mm_and_si128(uv, mask);
            v  = _mm_srli_epi16(uv, 8);
            tmp = _mm_packus_epi16(u, v);
            storel_epi64(refPelU + 1 + 2 * width, tmp);
            storeh_epi64(refPelV + 1 + 2 * width, tmp);

        } else if (size == 2) {
            __m256i mask = _mm256_set1_epi16(0xff);
            __m256i uv = loada_si256(refPel + 2);       // above 16*2 bytes  // u0 v0 ... u15 v15
            __m256i u  = _mm256_and_si256(uv, mask);    // 16w: u0 .. u15
            __m256i v  = _mm256_srli_epi16(uv, 8);      // 16w: v0 .. v15
            __m256i tmp = _mm256_packus_epi16(u, v);    // 32b: u0 .. u7 v0 .. v7 u8 .. u15 v8 .. v15
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU + 1, refPelV + 1, tmp);

            uv  = loada_si256(refPel + 2 + 4 * width);  // left 16*2 bytes  // u0 v0 ... u15 v15
            u  = _mm256_and_si256(uv, mask);
            v  = _mm256_srli_epi16(uv, 8);
            tmp = _mm256_packus_epi16(u, v);
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU + 1 + 2 * width, refPelV + 1 + 2 * width, tmp);

        } else {
            __m256i mask = _mm256_set1_epi16(0xff);
            __m256i uv = loada_si256(refPel + 2);       // above 16*2 bytes  // u0 v0 ... u15 v15
            __m256i u  = _mm256_and_si256(uv, mask);    // 16w: u0 .. u15
            __m256i v  = _mm256_srli_epi16(uv, 8);      // 16w: v0 .. v15
            __m256i tmp = _mm256_packus_epi16(u, v);    // 32b: u0 .. u7 v0 .. v7 u8 .. u15 v8 .. v15
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU + 1, refPelV + 1, tmp);

            uv = loada_si256(refPel + 2 + 16 * 2);
            u  = _mm256_and_si256(uv, mask);
            v  = _mm256_srli_epi16(uv, 8);
            tmp = _mm256_packus_epi16(u, v);
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU + 1 + 16, refPelV + 1 + 16, tmp);

            uv  = loada_si256(refPel + 2 + 4 * width);  // left 16*2 bytes  // u0 v0 ... u15 v15
            u  = _mm256_and_si256(uv, mask);
            v  = _mm256_srli_epi16(uv, 8);
            tmp = _mm256_packus_epi16(u, v);
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU + 1 + 2 * width, refPelV + 1 + 2 * width, tmp);

            uv  = loada_si256(refPel + 2 + 4 * width + 16 * 2);  // left 16*2 bytes  // u0 v0 ... u15 v15
            u  = _mm256_and_si256(uv, mask);
            v  = _mm256_srli_epi16(uv, 8);
            tmp = _mm256_packus_epi16(u, v);
            tmp = _mm256_permute4x64_epi64(tmp, PERM4x64(0,2,1,3));
            storea2_m128i(refPelU + 1 + 2 * width + 16, refPelV + 1 + 2 * width + 16, tmp);
        }
    }

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

    template <int size, int haveLeft, int haveAbove> void predict_intra_nv12_dc_vp9_avx2(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const int width =  4 << size;
        assert(((size_t(refPel) + 2) & 31) == 0);
        ALIGN_DECL(32) uint8_t refPelU_[32+32+32+32];
        ALIGN_DECL(32) uint8_t refPelV_[32+32+32+32];
        uint8_t *refPelU = refPelU_ + 31;
        uint8_t *refPelV = refPelV_ + 31;

        convert_ref_pels_vp9<size>(refPel, refPelU, refPelV);
        predict_intra_dc_vp9_avx2<size, haveLeft, haveAbove>(refPelU, dst + 0, pitch);
        predict_intra_dc_vp9_avx2<size, haveLeft, haveAbove>(refPelV, dst + width, pitch);
        convert_pred_block<size>(dst, pitch);
    }
    template void predict_intra_nv12_dc_vp9_avx2<0, 0, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<0, 0, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<0, 1, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<0, 1, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<1, 0, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<1, 0, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<1, 1, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<1, 1, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<2, 0, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<2, 0, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<2, 1, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<2, 1, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<3, 0, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<3, 0, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<3, 1, 0>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_dc_vp9_avx2<3, 1, 1>(const uint8_t*,uint8_t*,int);

    template <int size, int haveLeft, int haveAbove>
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
    template void predict_intra_nv12_dc_av1_avx2<0, 0, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<0, 0, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<0, 1, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<0, 1, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<1, 0, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<1, 0, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<1, 1, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<1, 1, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<2, 0, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<2, 0, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<2, 1, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<2, 1, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<3, 0, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<3, 0, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<3, 1, 0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_dc_av1_avx2<3, 1, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template <int size, int mode> void predict_intra_nv12_vp9_avx2(const uint8_t *refPel, uint8_t *dst, int pitch) {
        const int width =  4 << size;
        assert(((size_t(refPel) + 2) & 31) == 0);
        ALIGN_DECL(32) uint8_t refPelU_[32+32+32+32];
        ALIGN_DECL(32) uint8_t refPelV_[32+32+32+32];
        uint8_t *refPelU = refPelU_ + 31;
        uint8_t *refPelV = refPelV_ + 31;

        convert_ref_pels_vp9<size>(refPel, refPelU, refPelV);
        predict_intra_vp9_avx2<size, mode>(refPelU, dst + 0, pitch);
        predict_intra_vp9_avx2<size, mode>(refPelV, dst + width, pitch);
        convert_pred_block<size>(dst, pitch);
    }
    template void predict_intra_nv12_vp9_avx2<0, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 2>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 3>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 4>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 5>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 6>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 7>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 8>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<0, 9>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 2>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 3>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 4>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 5>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 6>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 7>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 8>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<1, 9>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 2>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 3>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 4>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 5>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 6>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 7>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 8>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<2, 9>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 1>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 2>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 3>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 4>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 5>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 6>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 7>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 8>(const uint8_t*,uint8_t*,int);
    template void predict_intra_nv12_vp9_avx2<3, 9>(const uint8_t*,uint8_t*,int);

    template <int width, int haveAbove, int haveLeft>
    void get_pred_pels_chroma_nv12(const uint8_t *rec, int32_t pitch, uint8_t *predPelsU, uint8_t *predPelsV)
    {
        uint8_t *aboveRowU = predPelsU + 1;
        uint8_t *aboveRowV = predPelsV + 1;
        uint8_t *leftColU  = aboveRowU + 2 * width;
        uint8_t *leftColV  = aboveRowV + 2 * width;

        const uint8_t *above = rec - pitch;
        if (haveAbove) {
            if (width == 4) {
                ALIGN_DECL(16) const uint8_t shuftab_nv12_to_yv12_w4_replicate[16] = {0,2,4,6,6,6,6,6, 1,3,5,7,7,7,7,7};
                const __m128i shuf = loada_si128(shuftab_nv12_to_yv12_w4_replicate);
                __m128i src = loadl_epi64(rec - pitch);
                __m128i dst = _mm_shuffle_epi8(src, shuf);
                storel_epi64(predPelsU + 1, dst);
                storeh_epi64(predPelsV + 1, dst);
            } else if (width == 8) {
                const __m128i shuf = loada_si128(shuftab_nv12_to_yv12);
                __m128i src = loada_si128(rec - pitch);
                __m128i dst = _mm_shuffle_epi8(src, shuf);
                storel_epi64(predPelsU + 1, dst);
                storeh_epi64(predPelsV + 1, dst);
            } else if (width == 16) {
                const __m256i shuf = loada_si256(shuftab_nv12_to_yv12);
                __m256i src = loada_si256(rec - pitch);
                __m256i tmp = _mm256_permute4x64_epi64(_mm256_shuffle_epi8(src, shuf), PERM4x64(0,2,1,3));
                storea2_m128i(predPelsU + 1, predPelsV + 1, tmp);
            } else if (width == 32) {
                const __m256i shuf = loada_si256(shuftab_nv12_to_yv12);
                __m256i src0 = loada_si256(rec - pitch + 0);
                __m256i src1 = loada_si256(rec - pitch + 32);
                __m256i tmp0 = _mm256_permute4x64_epi64(_mm256_shuffle_epi8(src0, shuf), PERM4x64(0,2,1,3));
                __m256i tmp1 = _mm256_permute4x64_epi64(_mm256_shuffle_epi8(src1, shuf), PERM4x64(0,2,1,3));
                __m256i u = _mm256_permute2x128_si256(tmp0, tmp1, PERM2x128(0,2));
                __m256i v = _mm256_permute2x128_si256(tmp0, tmp1, PERM2x128(1,3));
                storea_si256(predPelsU + 1, u);
                storea_si256(predPelsV + 1, v);
            } else {
                for (int i = 0; i < width; i++) {
                    aboveRowU[i] = above[2 * i + 0];
                    aboveRowV[i] = above[2 * i + 1];
                }
                for (int i = 0; i < width; i++) {
                    aboveRowU[width + i] = above[2 * width - 2];
                    aboveRowV[width + i] = above[2 * width - 1];
                }
            }
        } else {
            if (width == 4) {
                const __m128i const_0x7f = _mm_set1_epi8(0x7f);
                storel_epi64(predPelsU + 1, const_0x7f);
                storel_epi64(predPelsV + 1, const_0x7f);
            } else if (width == 8) {
                const __m128i const_0x7f = _mm_set1_epi8(0x7f);
                storel_epi64(predPelsU + 1, const_0x7f);
                storel_epi64(predPelsV + 1, const_0x7f);
            } else if (width == 16) {
                const __m128i const_0x7f = _mm_set1_epi8(0x7f);
                storea_si128(predPelsU + 1, const_0x7f);
                storea_si128(predPelsV + 1, const_0x7f);
            } else if (width == 32) {
                const __m256i const_0x7f = _mm256_set1_epi8(0x7f);
                storea_si256(predPelsU + 1 + 0,  const_0x7f);
                storea_si256(predPelsV + 1 + 0,  const_0x7f);
            }
        }

        if (haveAbove && haveLeft)
            predPelsU[0] = rec[-pitch-2], predPelsV[0] = rec[-pitch-1];
        else if (haveAbove)
            predPelsU[0] = predPelsV[0] = 129;
        else
            predPelsU[0] = predPelsV[0] = 127;

        if (haveLeft) {
            const uint8_t *left = rec - 2;
            for (int i = 0; i < width; i++, left += pitch) {
                leftColU[i] = left[0];
                leftColV[i] = left[1];
            }
        } else {
            if (width == 4) {
                const __m128i const_0x81 = _mm_set1_epi8(0x81);
                storel_si32(predPelsU + 1 + 2 * width, const_0x81);
                storel_si32(predPelsV + 1 + 2 * width, const_0x81);
            } else if (width == 8) {
                const __m128i const_0x81 = _mm_set1_epi8(0x81);
                storel_epi64(predPelsU + 1 + 2 * width, const_0x81);
                storel_epi64(predPelsV + 1 + 2 * width, const_0x81);
            } else if (width == 16) {
                const __m128i const_0x81 = _mm_set1_epi8(0x81);
                storea_si128(predPelsU + 1 + 2 * width, const_0x81);
                storea_si128(predPelsV + 1 + 2 * width, const_0x81);
            } else if (width == 32) {
                const __m256i const_0x81 = _mm256_set1_epi8(0x81);
                storea_si256(predPelsU + 1 + 2 * width, const_0x81);
                storea_si256(predPelsV + 1 + 2 * width, const_0x81);
            } else {
                for (int i = 0; i < width; i++) {
                    leftColU[i] = 129;
                    leftColV[i] = 129;
                }
            }
        }
    }

    int sse_cont_8u_avx2(const uint8_t *src1, const uint8_t *src2, int numPixels);

    // nv12 implicit pitch is 64
    // yv12 implicit pitch is w (continuous block)
    // Input block nv12:
    //   u00 v00 u01 v01 <pitch>
    //   u10 v10 u11 v11 <pitch>
    // is converted to output block yv12 (pitch == width):
    //   u00 u01 v00 v01
    //   u10 u11 v10 v11
    template <int w> void convert_nv12_to_yv12(const uint8_t *nv12, uint8_t *yv12);
    template <> void convert_nv12_to_yv12<4>(const uint8_t *nv12, uint8_t *yv12) { // 8x4 -> 4x4 4x4
        __m256i src = _mm256_inserti128_si256(si256(loadu2_epi64(nv12, nv12 + 64)), loadu2_epi64(nv12 + 128, nv12 + 192), 1);
        __m256i dst = _mm256_shuffle_epi8(src, loada_si256(shuftab_nv12_to_yv12_w4));
        storea_si256(yv12, dst);
    }
    template <> void convert_nv12_to_yv12<8>(const uint8_t *nv12, uint8_t *yv12) { // 16x8 -> 8x8 8x8
        const __m256i shuf = loada_si256(shuftab_nv12_to_yv12);
        storea_si256(yv12 + 0,  _mm256_shuffle_epi8(loada2_m128i(nv12 + 0 * 64, nv12 + 1 * 64), shuf));
        storea_si256(yv12 + 32, _mm256_shuffle_epi8(loada2_m128i(nv12 + 2 * 64, nv12 + 3 * 64), shuf));
        storea_si256(yv12 + 64, _mm256_shuffle_epi8(loada2_m128i(nv12 + 4 * 64, nv12 + 5 * 64), shuf));
        storea_si256(yv12 + 96, _mm256_shuffle_epi8(loada2_m128i(nv12 + 6 * 64, nv12 + 7 * 64), shuf));
    }
    template <> void convert_nv12_to_yv12<16>(const uint8_t *nv12, uint8_t *yv12) { // 32x16 -> 16x16 16x16
        const __m256i shuf = loada_si256(shuftab_nv12_to_yv12);
        storea_si256(yv12 +  0 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  0 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  1 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  1 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  2 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  2 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  3 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  3 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  4 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  4 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  5 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  5 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  6 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  6 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  7 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  7 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  8 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  8 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 +  9 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 +  9 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 + 10 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + 10 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 + 11 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + 11 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 + 12 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + 12 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 + 13 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + 13 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 + 14 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + 14 * 64), shuf), PERM4x64(0,2,1,3)));
        storea_si256(yv12 + 15 * 32, _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + 15 * 64), shuf), PERM4x64(0,2,1,3)));
    }
    template <> void convert_nv12_to_yv12<32>(const uint8_t *nv12, uint8_t *yv12) { // 64x32 -> 32x32 32x32
        const __m256i shuf = loada_si256(shuftab_nv12_to_yv12);
        for (int i = 0; i < 32; i++) {
            __m256i dst0 = _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + i * 64 +  0), shuf), PERM4x64(0,2,1,3));
            __m256i dst1 = _mm256_permute4x64_epi64(_mm256_shuffle_epi8(loada_si256(nv12 + i * 64 + 32), shuf), PERM4x64(0,2,1,3));
            storea_si256(yv12 + 64 * i + 0,  _mm256_permute2x128_si256(dst0, dst1, PERM2x128(0,2)));
            storea_si256(yv12 + 64 * i + 32, _mm256_permute2x128_si256(dst0, dst1, PERM2x128(1,3)));
        }
    }

    template <int txSize, int haveLeft, int haveAbove>
    int pick_intra_nv12_avx2(const uint8_t *rec, int pitchRec, const uint8_t *src, float lambda, const uint16_t *modeBits) {
        const int width = 4 << txSize;
        const int size = 2 * width * width;
        ALIGN_DECL(32) uint8_t predPelsU_[32 + (4<<TX_32X32) * 3];
        ALIGN_DECL(32) uint8_t predPelsV_[32 + (4<<TX_32X32) * 3];
        ALIGN_DECL(64) uint8_t predBuf[size];
        ALIGN_DECL(64) uint8_t srcYv12[size];
        uint8_t *predPelsU = (uint8_t*)(predPelsU_ + 32 - 1);
        uint8_t *predPelsV = (uint8_t*)(predPelsV_ + 32 - 1);

        convert_nv12_to_yv12<width>(src, srcYv12);

        get_pred_pels_chroma_nv12<width, haveAbove, haveLeft>(rec, pitchRec, predPelsU, predPelsV);

        predict_intra_dc_vp9_avx2<txSize, haveLeft, haveAbove>(predPelsU, predBuf,         2 * width);
        predict_intra_dc_vp9_avx2<txSize, haveLeft, haveAbove>(predPelsV, predBuf + width, 2 * width);
        int bestMode = 0;
        float bestCost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[0];

        predict_intra_vp9_avx2<txSize, 1>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 1>(predPelsV, predBuf + width, 2 * width);
        float cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[1];
        if (bestCost > cost) bestCost = cost, bestMode = 1;

        predict_intra_vp9_avx2<txSize, 2>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 2>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[2];
        if (bestCost > cost) bestCost = cost, bestMode = 2;

        predict_intra_vp9_avx2<txSize, 3>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 3>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[3];
        if (bestCost > cost) bestCost = cost, bestMode = 3;

        predict_intra_vp9_avx2<txSize, 4>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 4>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[4];
        if (bestCost > cost) bestCost = cost, bestMode = 4;

        predict_intra_vp9_avx2<txSize, 5>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 5>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[5];
        if (bestCost > cost) bestCost = cost, bestMode = 5;

        predict_intra_vp9_avx2<txSize, 6>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 6>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[6];
        if (bestCost > cost) bestCost = cost, bestMode = 6;

        predict_intra_vp9_avx2<txSize, 7>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 7>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[7];
        if (bestCost > cost) bestCost = cost, bestMode = 7;

        predict_intra_vp9_avx2<txSize, 8>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 8>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[8];
        if (bestCost > cost) bestCost = cost, bestMode = 8;

        predict_intra_vp9_avx2<txSize, 9>(predPelsU, predBuf,         2 * width);
        predict_intra_vp9_avx2<txSize, 9>(predPelsV, predBuf + width, 2 * width);
        cost = sse_cont_8u_avx2(srcYv12, predBuf, size) + lambda * modeBits[9];
        if (bestCost > cost) bestCost = cost, bestMode = 9;

        return bestMode;
    }
    template int pick_intra_nv12_avx2<0, 0, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<0, 0, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<0, 1, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<0, 1, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<1, 0, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<1, 0, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<1, 1, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<1, 1, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<2, 0, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<2, 0, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<2, 1, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<2, 1, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<3, 0, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<3, 0, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<3, 1, 0>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);
    template int pick_intra_nv12_avx2<3, 1, 1>(const uint8_t*, int, const uint8_t*, float, const uint16_t*);

    namespace details {
        const uint8_t mode_to_angle_map[] = {0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0};

        const int16_t dr_intra_derivative[90] = {
            0,    0, 0,        //
            1023, 0, 0,        // 3, ...
            547,  0, 0,        // 6, ...
            372,  0, 0, 0, 0,  // 9, ...
            273,  0, 0,        // 14, ...
            215,  0, 0,        // 17, ...
            178,  0, 0,        // 20, ...
            151,  0, 0,        // 23, ... (113 & 203 are base angles)
            132,  0, 0,        // 26, ...
            116,  0, 0,        // 29, ...
            102,  0, 0, 0,     // 32, ...
            90,   0, 0,        // 36, ...
            80,   0, 0,        // 39, ...
            71,   0, 0,        // 42, ...
            64,   0, 0,        // 45, ... (45 & 135 are base angles)
            57,   0, 0,        // 48, ...
            51,   0, 0,        // 51, ...
            45,   0, 0, 0,     // 54, ...
            40,   0, 0,        // 58, ...
            35,   0, 0,        // 61, ...
            31,   0, 0,        // 64, ...
            27,   0, 0,        // 67, ... (67 & 157 are base angles)
            23,   0, 0,        // 70, ...
            19,   0, 0,        // 73, ...
            15,   0, 0, 0, 0,  // 76, ...
            11,   0, 0,        // 81, ...
            7,    0, 0,        // 84, ...
            3,    0, 0,        // 87, ...
        };

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

        ALIGN_DECL(32) uint8_t z2BlendMaskTab[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                                 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                                                 128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
                                                 128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128};

        void predict_4x4_sse4(const uint8_t* refPel, uint8_t* dst, int pitch, int dx)
        {
            int width = 4;

            __m128i a0, a1, diff, a256;
            a0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)refPel));      // 8w: -10123456
            a1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(refPel + 1)));  // 8w:  01234567
            diff = _mm_sub_epi16(a1, a0);                                    // 8w: a[x+1] - a[x]
            a256 = _mm_slli_epi16(a0, 5);                                    // 8w: a[x] * 32
            a256 = _mm_add_epi16(a256, _mm_set1_epi16(16));                  // 8w: a[x] * 32 + 16

            for (int r = 0; r < 4; r++) {
                __m128i b, res1;
                int x = -dx * (r + 1);
                int base1 = x >> 6;
                const int c_pos = -1 - base1;

                __m128i inc = _mm_set1_epi16(x);
                __m128i shift = _mm_and_si128(inc, _mm_set1_epi16(0x3f));

                b = _mm_mullo_epi16(diff, _mm_srli_epi16(shift, 1));
                res1 = _mm_add_epi16(a256, b);
                res1 = _mm_srli_epi16(res1, 5);

                res1 = _mm_packus_epi16(res1, res1);

                if (c_pos >= 0 && c_pos < width)
                    for (int i = 0; i < c_pos; i++)
                        res1 = _mm_bslli_si128(res1, 1);

                *(int *)&dst[r*pitch] = _mm_cvtsi128_si32(res1);
            }
        }
        void predict_8x8_sse4(const uint8_t* refPel, uint8_t* dst, int pitch, int dx)
        {
            int width = 8;
            ALIGN_DECL(32) uint8_t refPel0[64 + 2*8+1];
            memcpy(refPel0 + 16, refPel, 2*8+1);

            for (int r = 0; r < 8; r++) {
                __m128i b, res1;
                int x = -dx * (r + 1);
                int base1 = x >> 6;
                const int c_pos = -1 - base1;

                int base = 0;
                if (c_pos >= 0 && c_pos < width) {
                    base = c_pos;
                }

                __m128i a0, a1, diff, a256;
                a0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(refPel0+16 - base)));
                a1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(refPel0+16+1 - base)));
                diff = _mm_sub_epi16(a1, a0);
                a256 = _mm_slli_epi16(a0, 5);
                a256 = _mm_add_epi16(a256, _mm_set1_epi16(16));

                __m128i inc = _mm_set1_epi16(x);
                __m128i shift = _mm_and_si128(inc, _mm_set1_epi16(0x3f));

                b = _mm_mullo_epi16(diff, _mm_srli_epi16(shift, 1));
                res1 = _mm_add_epi16(a256, b);
                res1 = _mm_srli_epi16(res1, 5);

                res1 = _mm_packus_epi16(res1, res1);
                storel_epi64(dst + r*pitch, res1);
            }
        }
        void predict_16x16_avx2(const uint8_t* refPel, uint8_t* dst, int pitch, int dx)
        {
            int width = 16;
            int bs = width;

            ALIGN_DECL(32) uint8_t refPel0[64 + 4*16+1];
            memcpy(refPel0 + 16, refPel, 2*16+1);

            // pre-filter above pixels
            // store in temp buffers:
            //   above[x] * 256 + 128
            //   above[x+1] - above[x]
            // final pixels will be caluculated as:
            //   (above[x] * 256 + 128 + (above[x+1] - above[x]) * shift) >> 8

            for (int r = 0; r < bs; r++) {
                __m256i b, res1;
                int x = -dx * (r + 1);
                int base1 = x >> 6;
                const int c_pos = -1 - base1;

                __m256i inc = _mm256_set1_epi16(x);
                __m256i shift = _mm256_and_si256(inc, _mm256_set1_epi16(0x3f));

                __m256i a0, a1, diff, a256;
                int base = 0;
                if (c_pos > 0 && c_pos < width) {
                    base = c_pos;
                }

                a0 = _mm256_cvtepu8_epi16(loadu_si128(refPel0+16   -base));
                a1 = _mm256_cvtepu8_epi16(loadu_si128(refPel0+1+16 - base));
                diff = _mm256_sub_epi16(a1, a0);
                a256 = _mm256_slli_epi16(a0, 5);
                a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));

                b = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 1));
                res1 = _mm256_add_epi16(a256, b);

                res1 = _mm256_srli_epi16(res1, 5);

                res1 = _mm256_packus_epi16(res1, res1);
                res1 = _mm256_permute4x64_epi64(res1, PERM4x64(0, 2, 1, 3));

                storeu_si128(dst + r*pitch, si128(res1));
            }
        }
        void predict_32x32_sse4(const uint8_t* refPel, uint8_t* dst, int pitch, int dx)
        {
            int width = 32;
            int bs = width;
            uint8_t refPel0[64 + 4*32];
            memcpy(refPel0 + 32, refPel, 2*32+1);

            for (int r = 0; r < bs; r++) {
                __m256i b, res1;
                int x = -dx * (r + 1);
                int base1 = x >> 6;
                const int c_pos = -1 - base1;

                __m256i inc = _mm256_set1_epi16(x);
                __m256i shift = _mm256_and_si256(inc, _mm256_set1_epi16(0x3f));

                __m256i a0, a1, diff, a256;
                int base = 0;
                if (c_pos > 0 && c_pos < width) {
                    base = c_pos;
                }

                for (int i = 0; i < 32; i += 16) {
                    a0 = _mm256_cvtepu8_epi16(loadu_si128(refPel0+32 + i   -base));
                    a1 = _mm256_cvtepu8_epi16(loadu_si128(refPel0+1+32 + i - base));
                    diff = _mm256_sub_epi16(a1, a0);
                    a256 = _mm256_slli_epi16(a0, 5);
                    a256 = _mm256_add_epi16(a256, _mm256_set1_epi16(16));

                    b = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 1));
                    res1 = _mm256_add_epi16(a256, b);

                    res1 = _mm256_srli_epi16(res1, 5);

                    res1 = _mm256_packus_epi16(res1, res1);
                    res1 = _mm256_permute4x64_epi64(res1, PERM4x64(0, 2, 1, 3));

                    storeu_si128(dst + r*pitch + i, si128(res1));
                }
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

        template <int txSize> void predict_intra_z2_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int upTop, int upLeft, int dx, int dy);
        template <> void predict_intra_z2_av1_avx2<TX_4X4>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int upTop, int upLeft, int dx, int dy)
        {
            upTop, upLeft;
            assert(upTop == 0);
            assert(upLeft == 0);
            int width = 4;
            int bs = width;
            int r, x;

            ALIGN_DECL(32) uint8_t dstTmp[4 * 4] = { 0 };
            ALIGN_DECL(32) uint8_t dstTmpTransp[4 * 4] = { 0 };
            int pitchTmp = 4;

            predict_4x4_sse4(topPels - 1, dst, pitch, dx);
            predict_4x4_sse4(leftPels - 1, dstTmp, pitchTmp, dy);
            transpose<TX_4X4>(dstTmp, pitchTmp, dstTmpTransp, pitchTmp);

            for (r = 0; r < bs; r++) {
                x = -dx * (r + 1);
                int base1 = x >> 6;
                int c_pos = -1 - base1;
                int base = 0;
                if (c_pos >= 0 && c_pos < 32)
                    base = 32 - c_pos;

                __m128i m = loadu_si128(z2BlendMaskTab + base);
                __m128i a = loadu_si128(dst + r*pitch);
                __m128i l = loadu_si128(dstTmpTransp + r*pitchTmp);

                a = _mm_blendv_epi8(l, a, m);
                *(int *)&dst[r*pitch] = _mm_cvtsi128_si32(a);
            }
        }
        template <> void predict_intra_z2_av1_avx2<TX_8X8>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int upTop, int upLeft, int dx, int dy)
        {
            upTop, upLeft;
            assert(upTop == 0);
            assert(upLeft == 0);

            int width = 8;
            int bs = width;
            int r, x;

            uint8_t dstTmp[8 * 8] = { 0 };
            uint8_t dstTmpTransp[8 * 8] = { 0 };
            int pitchTmp = 8;

            predict_8x8_sse4(topPels - 1, dst, pitch, dx);
            predict_8x8_sse4(leftPels - 1, dstTmp, pitchTmp, dy);
            transpose<TX_8X8>(dstTmp, pitchTmp, dstTmpTransp, pitchTmp);

            for (r = 0; r < bs; r++) {
                x = -dx * (r + 1);
                int base1 = x >> 6;
                int c_pos = -1 - base1;
                int base = 0;
                if (c_pos >= 0 && c_pos < 32)
                    base = 32 - c_pos;
                __m128i m = loadu_si128(z2BlendMaskTab + base);
                __m128i a = loadu_si128(dst + r*pitch);
                __m128i l = loadu_si128(dstTmpTransp + r*pitchTmp);

                a = _mm_blendv_epi8(l, a, m);
                storel_epi64(dst + r*pitch, a);
            }
        }
        template <> void predict_intra_z2_av1_avx2<TX_16X16>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int upTop, int upLeft, int dx, int dy)
        {
            upTop, upLeft;
            assert(upTop == 0);
            assert(upLeft == 0);

            const int width = 4 << 2;
            int bs = width;
            int r, x;

            ALIGN_DECL(32) uint8_t dstTmp[16 * 16] = { 0 };
            ALIGN_DECL(32) uint8_t dstTmpTransp[16 * 16] = { 0 };
            int pitchTmp = 16;

            predict_16x16_avx2(topPels - 1, dst, pitch, dx);
            predict_16x16_avx2(leftPels - 1, dstTmp, pitchTmp, dy);
            transpose<TX_16X16>(dstTmp, pitchTmp, dstTmpTransp, pitchTmp);

            for (r = 0; r < bs; r++) {
                x = -dx * (r + 1);
                int base1 = x >> 6;
                int c_pos = -1 - base1;
                int base = 0;
                if (c_pos >= 0 && c_pos < 16)
                    base = 32 - c_pos;

                __m128i m = loadu_si128(z2BlendMaskTab + base);
                __m128i a = loadu_si128(dst + r*pitch);
                __m128i l = loadu_si128(dstTmpTransp + r*pitchTmp);

                a = _mm_blendv_epi8(l, a, m);
                storeu_si128(dst + r*pitch, a);
            }
        }
        template <> void predict_intra_z2_av1_avx2<TX_32X32>(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int upTop, int upLeft, int dx, int dy)
        {
            upTop, upLeft;
            assert(upTop == 0);
            assert(upLeft == 0);

            int width = 32;
            int bs = width;
            int r, c, x;

            ALIGN_DECL(32) uint8_t dstTmp[32 * 32] = { 0 };
            ALIGN_DECL(32) uint8_t dstTmpTransp[32 * 32] = { 0 };
            int pitchTmp = 32;

            predict_32x32_sse4(topPels - 1, dst, pitch, dx);
            predict_32x32_sse4(leftPels - 1, dstTmp, pitchTmp, dy);
            transpose<TX_32X32>(dstTmp, pitchTmp, dstTmpTransp, pitchTmp);

            for (r = 0; r < bs; r++) {
                x = -dx * (r + 1);
                int base1 = x >> 6;
                int c_pos = -1 - base1;
                int base = 0;
                if (c_pos >= 0 && c_pos < 32)
                    base = 32 - c_pos;

                for (c = 0; c < bs; c += 16) {
                    __m128i m = loadu_si128(z2BlendMaskTab + base + c);
                    __m128i a = loadu_si128(dst + r*pitch + c);
                    __m128i l = loadu_si128(dstTmpTransp + r*pitchTmp + c);
                    a = _mm_blendv_epi8(l, a, m);
                    storeu_si128(dst + r*pitch + c, a);
                }
            }
        }

        template <int txSize> void predict_intra_z3_av1_avx2(const uint8_t *leftPels, uint8_t *dst, int pitch, int upLeft, int dy)
        {
            const int width = 4 << txSize;
            ALIGN_DECL(32) uint8_t dstT[width * width];
            predict_intra_z1_av1_avx2<txSize>(leftPels, dstT, width, upLeft, dy);
            transpose<txSize>(dstT, width, dst, pitch);
        }

        template <int txSize> void predict_intra_z1_av1_px(const uint8_t *topPels, uint8_t *dst, int pitch, int upTop, int dx);
        template <int txSize> void predict_intra_z2_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int upTop, int upLeft, int dx, int dy);
        template <int txSize> void predict_intra_z3_av1_px(const uint8_t *leftPels, uint8_t *dst, int pitch, int upLeft, int dy);
    };

    template <int size, int mode>
    void predict_intra_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int delta, int upTop, int upLeft) {
        if      (mode ==  9) details::predict_intra_smooth_av1_avx2<size>(topPels, leftPels, dst, pitch);
        else if (mode == 10) details::predict_intra_smooth_v_av1_avx2<size>(topPels, leftPels, dst, pitch);
        else if (mode == 11) details::predict_intra_smooth_h_av1_avx2<size>(topPels, leftPels, dst, pitch);
        else if (mode == 12) details::predict_intra_paeth_av1_avx2 <size>(topPels, leftPels, dst, pitch);
        else {
            assert(upTop == 0 && upLeft == 0);
            assert(mode >= 1 && mode <= 8);
            const int angle = details::mode_to_angle_map[mode] + delta * 3;

            if (angle > 0 && angle < 90) {
                const int dx = details::dr_intra_derivative[angle];
                details::predict_intra_z1_av1_avx2<size>(topPels, dst, pitch, upTop, dx);
            } else if (angle > 90  && angle < 180) {
                const int dx = details::dr_intra_derivative[180 - angle];
                const int dy = details::dr_intra_derivative[angle - 90];
                details::predict_intra_z2_av1_avx2<size>(topPels, leftPels, dst, pitch, upTop, upLeft, dx, dy);
            } else if (angle > 180 && angle < 270) {
                const int dy = details::dr_intra_derivative[270 - angle];
                details::predict_intra_z3_av1_avx2<size>(leftPels, dst, pitch, upLeft, dy);
            } else if (angle == 90) {
                details::predict_intra_ver_av1_avx2<size>(topPels, dst, pitch);
            } else if (angle == 180) {
                details::predict_intra_hor_av1_avx2<size>(leftPels, dst, pitch);
            }
        }
    }

    template void predict_intra_av1_avx2<0, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<0,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<1,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<2,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_avx2<3,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    template <int size, int mode>
    void predict_intra_nv12_av1_avx2(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int delta, int upTop, int upLeft) {
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
    template void predict_intra_nv12_av1_avx2<TX_4X4,   V_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   V_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, V_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, V_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   H_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   H_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, H_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, H_PRED       >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D45_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D45_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D45_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D45_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D63_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D63_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D63_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D63_PRED     >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D117_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D117_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D117_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D117_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D135_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D135_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D135_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D135_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D153_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D153_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D153_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D153_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D207_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D207_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D207_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D207_PRED    >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   PAETH_PRED   >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   PAETH_PRED   >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, PAETH_PRED   >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, PAETH_PRED   >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_PRED  >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_PRED  >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_PRED  >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_PRED  >(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_V_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_V_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_V_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_V_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_H_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_H_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_H_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_H_PRED>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);


};  // namespace AV1PP
