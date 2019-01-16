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

#include "mfx_av1_defs.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_probabilities.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_tables.h"

using namespace H265Enc;

__m128i get_base_context(__m128i l0, __m128i l1, __m128i l2, __m128i l3, __m128i l4, __m128i offset)
{
    const __m128i k0 = _mm_setzero_si128();
    const __m128i k4 = _mm_set1_epi8(4);
    __m128i sum;
    sum = _mm_add_epi8(l0, l1);
    sum = _mm_add_epi8(sum, l2);
    sum = _mm_add_epi8(sum, l3);
    sum = _mm_add_epi8(sum, l4);
    sum = _mm_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm_min_epu8(sum, k4);
    sum = _mm_add_epi8(sum, offset);
    return sum;
}

__m256i get_base_context_avx2(__m256i l0, __m256i l1, __m256i l2, __m256i l3, __m256i l4, __m256i offset)
{
    const __m256i k0 = _mm256_setzero_si256();
    const __m256i k4 = _mm256_set1_epi8(4);
    __m256i sum;
    sum = _mm256_add_epi8(_mm256_add_epi8(l0, l1),
                          _mm256_add_epi8(l2, l3));
    sum = _mm256_add_epi8(sum, l4);
    sum = _mm256_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm256_min_epu8(sum, k4);
    sum = _mm256_add_epi8(sum, offset);
    return sum;
}

__m128i get_br_context(__m128i l0, __m128i l1, __m128i l2, __m128i offset)
{
    const __m128i k0 = _mm_setzero_si128();
    const __m128i k6 = _mm_set1_epi8(6);
    __m128i sum = _mm_add_epi8(l0, l1);
    sum = _mm_add_epi8(sum, l2);
    sum = _mm_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm_min_epu8(sum, k6);
    sum = _mm_add_epi8(sum, offset);
    return sum;
}

__m256i get_br_context_avx2(__m256i l0, __m256i l1, __m256i l2, __m256i offset)
{
    const __m256i k0 = _mm256_setzero_si256();
    const __m256i k6 = _mm256_set1_epi8(6);
    __m256i sum = _mm256_add_epi8(l0, l1);
    sum = _mm256_add_epi8(sum, l2);
    sum = _mm256_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
    sum = _mm256_min_epu8(sum, k6);
    sum = _mm256_add_epi8(sum, offset);
    return sum;
}

int32_t get_coeff_contexts_4x4(const int16_t *coeff, uint8_t *baseCtx, uint8_t *brCtx)
{
    const __m128i kzero = _mm_setzero_si128();
    const __m128i k3 = _mm_set1_epi8(NUM_BASE_LEVELS + 1);
    const __m128i k15 = _mm_set1_epi8(MAX_BASE_BR_RANGE);

    __m128i l = _mm_abs_epi8(_mm_packs_epi16(loadu2_epi64(coeff + 0, coeff + 4),
                                             loadu2_epi64(coeff + 8, coeff + 12)));
    __m128i acc = _mm_sad_epu8(l, kzero);
    int32_t culLevel = IPP_MIN(COEFF_CONTEXT_MASK, _mm_cvtsi128_si32(acc) + _mm_extract_epi32(acc, 2));

    const __m128i br_ctx_offset = _mm_setr_epi8( 0,  7, 14, 14,
                                                 7,  7, 14, 14,
                                                14, 14, 14, 14,
                                                14, 14, 14, 14);

    const __m128i base_ctx_offset = _mm_setr_epi8( 0,  1,  6, 6,
                                                   1,  6,  6, 11,
                                                   6,  6, 11, 11,
                                                   6, 11, 11, 11);

    __m128i a, b, c, d, e, base_ctx, l_sat15;
    l = _mm_min_epu8(k15, l);
    a = _mm_srli_epi32(l, 8);   // + 1
    b = _mm_srli_si128(l, 4);   // + pitch
    c = _mm_srli_si128(a, 4);   // + 1 + pitch
    storea_si128(brCtx, get_br_context(a, b, c, br_ctx_offset));

    l_sat15 = l;
    l = _mm_min_epu8(k3, l);
    a = _mm_srli_epi32(l, 8);   // + 1
    b = _mm_srli_si128(l, 4);   // + pitch
    c = _mm_srli_si128(a, 4);   // + 1 + pitch
    d = _mm_srli_epi32(l, 16);  // + 2
    e = _mm_srli_si128(l, 8);   // + 2 * pitch
    base_ctx = get_base_context(a, b, c, d, e, base_ctx_offset);
    base_ctx = _mm_slli_epi16(base_ctx, 4);
    base_ctx = _mm_or_si128(base_ctx, l_sat15);
    storea_si128(baseCtx, base_ctx);

    return culLevel;
}

inline H265_FORCEINLINE __m128i si128(__m256i r) { return _mm256_castsi256_si128(r); }
inline H265_FORCEINLINE __m128i si128_lo(__m256i r) { return si128(r); }
inline H265_FORCEINLINE __m128i si128_hi(__m256i r) { return _mm256_extracti128_si256(r, 1); }

inline H265_FORCEINLINE __m256i loada2_m128i(const void *lo, const void *hi) {
    return _mm256_insertf128_si256(_mm256_castsi128_si256(loada_si128(lo)), loada_si128(hi), 1);
}

#define permute4x64(r, m0, m1, m2, m3) _mm256_permute4x64_epi64(r, (m0) + ((m1) << 2) + ((m2) << 4) + ((m3) << 6))
#define permute2x128(r0, r1, m0, m1) _mm256_permute2x128_si256(r0, r1, (m0) + ((m1) << 4))

ALIGNED(32) static const uint8_t tab_base_ctx_offset[64] = {
     0,  1,  6,  6, 11, 11, 11, 11,
     1,  6,  6, 11, 11, 11, 11, 11,
     6,  6, 11, 11, 11, 11, 11, 11,
     6, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11
};

ALIGNED(32) static const uint8_t tab_br_ctx_offset[64] = {
     0,  7, 14, 14, 14, 14, 14, 14,
     7,  7, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14
};

int32_t get_coeff_contexts_8x8(const int16_t *coeff, uint8_t *baseCtx, uint8_t *brCtx)
{
    const __m256i kzero = _mm256_setzero_si256();
    const __m256i k3 = _mm256_set1_epi8(NUM_BASE_LEVELS + 1);
    const __m256i k15 = _mm256_set1_epi8(MAX_BASE_BR_RANGE);

    const __m256i br_ctx_offset0 = loada_si256(tab_br_ctx_offset + 0);
    const __m256i br_ctx_offset1 = loada_si256(tab_br_ctx_offset + 32);
    const __m256i base_ctx_offset0 = loada_si256(tab_base_ctx_offset + 0);
    const __m256i base_ctx_offset1 = loada_si256(tab_base_ctx_offset + 32);

    __m256i rows0123 = _mm256_abs_epi8(_mm256_packs_epi16(loada_si256(coeff + 0 * 8), loada_si256(coeff + 2 * 8)));
    __m256i rows4567 = _mm256_abs_epi8(_mm256_packs_epi16(loada_si256(coeff + 4 * 8), loada_si256(coeff + 6 * 8)));
    __m256i sum = _mm256_adds_epu8(rows0123, rows4567);
    sum = _mm256_sad_epu8(sum, kzero);

    rows0123 = permute4x64(rows0123, 0, 2, 1, 3);
    rows4567 = permute4x64(rows4567, 0, 2, 1, 3);
    rows0123 = _mm256_min_epu8(k15, rows0123);
    rows4567 = _mm256_min_epu8(k15, rows4567);

    __m256i br_ctx, base_ctx;
    __m256i a, b, c, d, e, tmp;

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8); // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);   // + pitch
    c = _mm256_srli_epi64(b, 8);        // + 1 + pitch
    storea_si256(brCtx, get_br_context_avx2(a, b, c, br_ctx_offset0));

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    storea_si256(brCtx + 32, get_br_context_avx2(a, b, c, br_ctx_offset1));

    __m256i rows0123_sat15 = rows0123;
    __m256i rows4567_sat15 = rows4567;
    rows0123 = _mm256_min_epu8(k3, rows0123);
    rows4567 = _mm256_min_epu8(k3, rows4567);

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8);         // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);           // + pitch
    c = _mm256_srli_epi64(b, 8);                // + 1 + pitch
    d = _mm256_srli_epi64(rows0123, 16);        // + 2
    e = permute2x128(rows0123, rows4567, 1, 2); // + 2 * pitch
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset0);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows0123_sat15);
    storea_si256(baseCtx, base_ctx);

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    d = _mm256_srli_epi64(rows4567, 16);
    e = permute2x128(rows4567, rows4567, 1, 8);
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset1);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows4567_sat15);
    storea_si256(baseCtx + 32, base_ctx);

    __m128i sum128 = _mm_add_epi64(si128_lo(sum), si128_hi(sum));
    int32_t culLevel = _mm_cvtsi128_si32(sum128) + _mm_extract_epi32(sum128, 2);
    culLevel = IPP_MIN(COEFF_CONTEXT_MASK, culLevel);
    return culLevel;
}

int32_t get_coeff_contexts_16x16(const int16_t *coeff, uint8_t *baseCtx, uint8_t *brCtx, int32_t txSize)
{
    const __m256i kzero = _mm256_setzero_si256();
    const __m256i k3 = _mm256_set1_epi8(NUM_BASE_LEVELS + 1);
    const __m256i k15 = _mm256_set1_epi8(MAX_BASE_BR_RANGE);

    const __m256i br_ctx_offset0 = loada_si256(tab_br_ctx_offset + 0);
    const __m256i br_ctx_offset1 = loada_si256(tab_br_ctx_offset + 32);
    const __m256i base_ctx_offset0 = loada_si256(tab_base_ctx_offset + 0);
    const __m256i base_ctx_offset1 = loada_si256(tab_base_ctx_offset + 32);

    const int32_t pitch = 4 << txSize;
    __m256i rows0123 = _mm256_abs_epi8(_mm256_packs_epi16(loada2_m128i(coeff + 0 * pitch, coeff + 2 * pitch),
                                                          loada2_m128i(coeff + 1 * pitch, coeff + 3 * pitch)));
    __m256i rows4567 = _mm256_abs_epi8(_mm256_packs_epi16(loada2_m128i(coeff + 4 * pitch, coeff + 6 * pitch),
                                                          loada2_m128i(coeff + 5 * pitch, coeff + 7 * pitch)));
    __m256i sum = _mm256_adds_epu8(rows0123, rows4567);
    sum = _mm256_sad_epu8(sum, kzero);

    rows0123 = _mm256_min_epu8(k15, rows0123);
    rows4567 = _mm256_min_epu8(k15, rows4567);

    __m256i br_ctx, base_ctx;
    __m256i a, b, c, d, e, tmp;

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8); // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);   // + pitch
    c = _mm256_srli_epi64(b, 8);        // + 1 + pitch
    storea_si256(brCtx, get_br_context_avx2(a, b, c, br_ctx_offset0));

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    storea_si256(brCtx + 32, get_br_context_avx2(a, b, c, br_ctx_offset1));

    __m256i rows0123_sat15 = rows0123;
    __m256i rows4567_sat15 = rows4567;
    rows0123 = _mm256_min_epu8(k3, rows0123);
    rows4567 = _mm256_min_epu8(k3, rows4567);

    tmp = _mm256_blend_epi32(rows0123, rows4567, 3);  // row4 row1 row2 row3
    a = _mm256_srli_epi64(rows0123, 8);         // + 1
    b = permute4x64(tmp, 1, 2, 3, 0);           // + pitch
    c = _mm256_srli_epi64(b, 8);                // + 1 + pitch
    d = _mm256_srli_epi64(rows0123, 16);        // + 2
    e = permute2x128(rows0123, rows4567, 1, 2); // + 2 * pitch
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset0);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows0123_sat15);
    storea_si256(baseCtx, base_ctx);

    tmp = _mm256_blend_epi32(rows4567, kzero, 3);  // zeros row5 row6 row7
    a = _mm256_srli_epi64(rows4567, 8);
    b = permute4x64(tmp, 1, 2, 3, 0);
    c = _mm256_srli_epi64(b, 8);
    d = _mm256_srli_epi64(rows4567, 16);
    e = permute2x128(rows4567, rows4567, 1, 8);
    base_ctx = get_base_context_avx2(a, b, c, d, e, base_ctx_offset1);
    base_ctx = _mm256_slli_epi16(base_ctx, 4);
    base_ctx = _mm256_or_si256(base_ctx, rows4567_sat15);
    storea_si256(baseCtx + 32, base_ctx);

    __m128i sum128 = _mm_add_epi64(si128_lo(sum), si128_hi(sum));
    int32_t culLevel = _mm_cvtsi128_si32(sum128) + _mm_extract_epi32(sum128, 2);
    culLevel = IPP_MIN(COEFF_CONTEXT_MASK, culLevel);
    return culLevel;
}

//static volatile unsigned lock = 0;

// bits = a * BSR(level + 1) + b
static const int32_t HIGH_FREQ_COEFF_FIT_CURVE[TOKEN_CDF_Q_CTXS][TX_32X32 + 1][PLANE_TYPES][2] = {
    { // q <= 20
        //     luma          chroma
        { {  895, 240 }, {  991, 170 } }, // TX_4X4
        { {  895, 240 }, {  991, 170 } }, // TX_8X8
        { { 1031, 177 }, { 1110, 125 } }, // TX_16X16
        { { 1005, 192 }, { 1559,  72 } }, // TX_32X32
    },
    { // q <= 60
        { {  895, 240 }, {  991, 170 } }, // TX_4X4
        { {  895, 240 }, {  991, 170 } }, // TX_8X8
        { { 1031, 177 }, { 1110, 125 } }, // TX_16X16
        { { 1005, 192 }, { 1559,  72 } }, // TX_32X32
    },
    { // q <= 120
        { {  895, 240 }, {  991, 170 } }, // TX_4X4
        { {  895, 240 }, {  991, 170 } }, // TX_8X8
        { { 1031, 177 }, { 1110, 125 } }, // TX_16X16
        { { 1005, 192 }, { 1559,  72 } }, // TX_32X32
    },
    { // q > 120
        { {  882, 203 }, { 1273, 100 } }, // TX_4X4
        { {  882, 203 }, { 1273, 100 } }, // TX_8X8
        { { 1094, 132 }, { 1527,  69 } }, // TX_16X16
        { { 1234, 102 }, { 1837,  61 } }, // TX_32X32
    },
};


uint32_t H265Enc::EstimateCoefsAv1(const TxbBitCounts &bc, int32_t txbSkipCtx, int32_t numNonZero,
                                 const CoeffsType *coeffs, int32_t *culLevel)
{
    assert(numNonZero > 0);
    const uint32_t EffNum = 118; // Average efficiency of estimate.
    uint32_t bits = 0;
    bits += bc.txbSkip[txbSkipCtx][0]; // non skip
    bits += 512 * numNonZero; // signs

    if (numNonZero == 1 && coeffs[0] != 0) {
        const int32_t level = abs(coeffs[0]);
        *culLevel = IPP_MIN(COEFF_CONTEXT_MASK, level);
        bits += bc.eobMulti[0]; // eob
        bits += bc.coeffBaseEob[0][IPP_MIN(3, level) - 1];
        bits += bc.coeffBrAcc[0][IPP_MIN(15, level)];
        return ((bits * EffNum) >> 7);
    }

    ALIGNED(32) uint8_t base_contexts[8 * 8];
    ALIGNED(32) uint8_t br_contexts[8 * 8];

    *culLevel = (bc.txSize == TX_4X4)
        ? get_coeff_contexts_4x4(coeffs, base_contexts, br_contexts)
        : (bc.txSize == TX_8X8)
        ? get_coeff_contexts_8x8(coeffs, base_contexts, br_contexts)
        : get_coeff_contexts_16x16(coeffs, base_contexts, br_contexts, bc.txSize);

    const int32_t level = base_contexts[0] & 15;
    bits += bc.coeffBase[0][IPP_MIN(3, level)];
    bits += bc.coeffBrAcc[br_contexts[0]][level];
    if (level)
        --numNonZero;
    assert(numNonZero > 0);

    int32_t size = (4 << bc.txSize);
    int32_t log2size = (2 + bc.txSize);
    int32_t diag = 0;

    const int16_t *scan = av1_default_scan[bc.txSize != TX_4X4];
    for (int32_t c = 1; c < 32; c++) {
        const int32_t pos = scan[c];
        const int32_t baseCtx = base_contexts[pos] >> 4;
        const int32_t level = base_contexts[pos] & 15;
        const int32_t baseLevel = IPP_MIN(3, level);

        if (level) {
            int x = pos & 7;
            int y = pos >> 3;
            diag = MAX(diag, x + y);

            bits += bc.coeffBrAcc[br_contexts[pos]][level];
            if (--numNonZero == 0) {
                bits += bc.coeffBaseEob[1][baseLevel - 1]; // last coef
                if (bc.txSize < TX_8X8) {
                    bits += bc.eobMulti[BSR(c) + 1]; // eob
                    return ((bits * EffNum) >> 7);
                } else {
                    break;
                }
            }
        }

        bits += bc.coeffBase[baseCtx][baseLevel];
    }

    const int32_t a = HIGH_FREQ_COEFF_FIT_CURVE[bc.qpi][bc.txSize][bc.plane][0];
    const int32_t b = HIGH_FREQ_COEFF_FIT_CURVE[bc.qpi][bc.txSize][bc.plane][1];
    scan = av1_default_scan[bc.txSize];

    uint32_t sum = 0;

    for (int32_t y = 0; y < size; y++) {
        // skip first 32 coefs in scan order
        int32_t startx = MAX(0, 7 - y);
        if (y < 4)
            startx++;
        for (int32_t x = startx; x < size; x++) {
            if (const int32_t coeff = coeffs[y * size + x]) {
                const int32_t level = abs(coeff);
                sum += BSR(level + 1);
                diag = MAX(diag, x + y);
            }
        }
    }

    int c;
    if (bc.txSize == TX_8X8)
        c = (diag < 8) ? (diag * diag + 2 * diag) / 2 : (63 - (15 - diag) * (15 - diag) / 2);
    else if (bc.txSize == TX_16X16)
        c = (diag < 16) ? (diag * diag + 2 * diag) / 2 : (255 - (31 - diag) * (31 - diag) / 2);
    else if (bc.txSize == TX_32X32)
        c = (diag < 32) ? (diag * diag + 2 * diag) / 2 : (1023 - (63 - diag) * (63 - diag) / 2);

    //int sum_ = 0;
    //int bits_ = bits;
    //for (int32_t c = 32; ; c++) {
    //    if (const int32_t coeff = coeffs[scan[c]]) {
    //        const int32_t level = abs(coeff);
    //        sum_ += BSR(level + 1);
    //        if (--numNonZero == 0) {
    //            int pos = scan[c];
    //            int x = pos & (size - 1);
    //            int y = pos >> log2size;
    //            int diag = x + y;
    //            int cc;
    //            if (bc.txSize == TX_8X8)
    //                cc = (diag < 8) ? (diag * diag + 2 * diag) / 2 : (63 - (15 - diag) * (15 - diag) / 2);
    //            else if (bc.txSize == TX_16X16)
    //                cc = (diag < 16) ? (diag * diag + 2 * diag) / 2 : (255 - (31 - diag) * (31 - diag) / 2);
    //            else if (bc.txSize == TX_32X32)
    //                cc = (diag < 32) ? (diag * diag + 2 * diag) / 2 : (1023 - (63 - diag) * (63 - diag) / 2);
    //            bits_ += sum_ * a + (cc - 32) * b;
    //            bits_ += bc.eobMulti[BSR(cc) + 1]; // eob
    //            break;
    //        }
    //    }
    //}

    bits += sum * a + MAX(0, c - 32) * b;
    bits += bc.eobMulti[BSR(c) + 1]; // eob

    return ((bits * EffNum) >> 7);
}

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
uint32_t H265Enc::EstimateCoefsAv1_MD(const TxbBitCounts &bc, int32_t txbSkipCtx, int32_t numNonZero,
                                    const CoeffsType *coeffs, int32_t *culLevel)
{
    assert(bc.txSize == TX_8X8);
    assert(numNonZero > 0);

    uint32_t bits = 0;
    bits += bc.txbSkip[txbSkipCtx][0]; // non skip
    bits += 512 * numNonZero; // signs

    if (numNonZero == 1 && coeffs[0] != 0) {
        const int32_t level = abs(coeffs[0]);
        *culLevel = IPP_MIN(COEFF_CONTEXT_MASK, level);
        bits += bc.eobMulti[0]; // eob
        bits += bc.coeffBaseEob[0][ IPP_MIN(3, level) - 1 ];
        bits += bc.coeffBrAcc[0][ IPP_MIN(15, level) ];
        return bits;
    }

    ALIGNED(32) uint8_t base_contexts[8 * 8];
    ALIGNED(32) uint8_t br_contexts[8 * 8];

    *culLevel = get_coeff_contexts_8x8(coeffs, base_contexts, br_contexts);

    const int32_t level = base_contexts[0] & 15;
    bits += bc.coeffBase[0][ IPP_MIN(3, level) ];
    bits += bc.coeffBrAcc[ br_contexts[0] ][level];
    if (level)
        --numNonZero;
    assert(numNonZero > 0);

    const int16_t *scan = av1_default_scan[bc.txSize != TX_4X4];
    for (int32_t c = 1; c < 64; c++) {
        const int32_t pos = scan[c];
        const int32_t baseCtx = base_contexts[pos] >> 4;
        const int32_t level = base_contexts[pos] & 15;
        const int32_t baseLevel = IPP_MIN(3, level);

        if (level) {
            bits += bc.coeffBrAcc[ br_contexts[pos] ][level];
            if (--numNonZero == 0) {
                bits += bc.coeffBaseEob[1][baseLevel - 1]; // last coef
                bits += bc.eobMulti[ BSR(c) + 1 ]; // eob
                return bits;
            }
        }

        bits += bc.coeffBase[baseCtx][baseLevel];
    }

    assert(0);
    return bits;
}
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
