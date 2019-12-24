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

namespace VP9PP
{
    template <int w> void diff_nxn_avx2(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff);
    template <> void diff_nxn_avx2<4>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff) {
        __m256i s1, s2, d;
        __m128i s, p;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        s = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(src), loadu_si32(src+pitchSrc)),
                               _mm_unpacklo_epi32(loadu_si32(src+pitchSrc2), loadu_si32(src+pitchSrc3)));
        p = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(pred), loadu_si32(pred+pitchPred)),
                               _mm_unpacklo_epi32(loadu_si32(pred+pitchPred2), loadu_si32(pred+pitchPred3)));
        s1 = _mm256_cvtepu8_epi16(s);
        s2 = _mm256_cvtepu8_epi16(p);
        d = _mm256_sub_epi16(s1, s2);
        storel_epi64(diff, si128(d));
        storeh_epi64(diff+pitchDiff, si128(d));
        storel_epi64(diff+pitchDiff2, _mm256_extracti128_si256(d,1));
        storeh_epi64(diff+pitchDiff3, _mm256_extracti128_si256(d,1));
    }
    template <> void diff_nxn_avx2<8>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        int pitchSrc4 = pitchSrc*4, pitchPred4 = pitchPred*4, pitchDiff4 = pitchDiff*4;
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src, src+pitchSrc));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred, pred+pitchPred));
        storeu2_m128i(diff, diff+pitchDiff, _mm256_sub_epi16(s1, s2));
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src+pitchSrc2, src+pitchSrc3));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred+pitchPred2, pred+pitchPred3));
        storeu2_m128i(diff+pitchDiff2, diff+pitchDiff3, _mm256_sub_epi16(s1, s2));
        src += pitchSrc4;
        pred += pitchPred4;
        diff += pitchDiff4;
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src, src+pitchSrc));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred, pred+pitchPred));
        storeu2_m128i(diff, diff+pitchDiff, _mm256_sub_epi16(s1, s2));
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src+pitchSrc2, src+pitchSrc3));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred+pitchPred2, pred+pitchPred3));
        storeu2_m128i(diff+pitchDiff2, diff+pitchDiff3, _mm256_sub_epi16(s1, s2));
    }
    template <> void diff_nxn_avx2<16>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        int pitchSrc4 = pitchSrc*4, pitchPred4 = pitchPred*4, pitchDiff4 = pitchDiff*4;
        for (int y = 0; y < 16; y += 4, src += pitchSrc4, pred += pitchPred4, diff += pitchDiff4) {
            s1 = _mm256_cvtepu8_epi16(loada_si128(src));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred));
            storea_si256(diff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred));
            storea_si256(diff+pitchDiff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc2));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred2));
            storea_si256(diff+pitchDiff2, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc3));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred3));
            storea_si256(diff+pitchDiff3, _mm256_sub_epi16(s1, s2));
        }
    }
    template <> void diff_nxn_avx2<32>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        for (int y = 0; y < 32; y+=2, src += pitchSrc2, pred += pitchPred2, diff += pitchDiff2) {
            s1 = _mm256_cvtepu8_epi16(loada_si128(src));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred));
            storea_si256(diff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+16));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+16));
            storea_si256(diff+16, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred));
            storea_si256(diff+pitchDiff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+16+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+16+pitchPred));
            storea_si256(diff+16+pitchDiff, _mm256_sub_epi16(s1, s2));
        }
    }
    template <> void diff_nxn_avx2<64>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        for (int y = 0; y < 64; y+=2, src += pitchSrc2, pred += pitchPred2, diff += pitchDiff2) {
            s1 = _mm256_cvtepu8_epi16(loada_si128(src));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred));
            storea_si256(diff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+16));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+16));
            storea_si256(diff+16, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+32));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+32));
            storea_si256(diff+32, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+48));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+48));
            storea_si256(diff+48, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred));
            storea_si256(diff+pitchDiff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc+16));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred+16));
            storea_si256(diff+pitchDiff+16, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc+32));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred+32));
            storea_si256(diff+pitchDiff+32, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc+48));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred+48));
            storea_si256(diff+pitchDiff+48, _mm256_sub_epi16(s1, s2));
        }
    }

    template <int w> void diff_nxn_p64_p64_pw_avx2(const uint8_t *src, const uint8_t *pred, int16_t *diff) {
        diff_nxn_avx2<w>(src, 64, pred, 64, diff, w);
    }
    template <> void diff_nxn_p64_p64_pw_avx2<4>(const uint8_t *src, const uint8_t *pred, int16_t *diff) {
        __m256i s1, s2, d;
        __m128i s, p;
        s = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(src), loadu_si32(src+64)),
                               _mm_unpacklo_epi32(loadu_si32(src+128), loadu_si32(src+192)));
        p = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(pred), loadu_si32(pred+64)),
                               _mm_unpacklo_epi32(loadu_si32(pred+128), loadu_si32(pred+192)));
        s1 = _mm256_cvtepu8_epi16(s);
        s2 = _mm256_cvtepu8_epi16(p);
        d = _mm256_sub_epi16(s1, s2);
        storea_si256(diff, d);
    }
    template <> void diff_nxn_p64_p64_pw_avx2<8>(const uint8_t *src, const uint8_t *pred, int16_t *diff) {
        __m256i s1, s2;
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src, src+64));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred, pred+64));
        storea_si256(diff, _mm256_sub_epi16(s1, s2));
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src+128, src+192));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred+128, pred+192));
        storea_si256(diff+16, _mm256_sub_epi16(s1, s2));
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src+256, src+320));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred+256, pred+320));
        storea_si256(diff+32, _mm256_sub_epi16(s1, s2));
        s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src+384, src+448));
        s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred+384, pred+448));
        storea_si256(diff+48, _mm256_sub_epi16(s1, s2));
    }
    template void diff_nxn_p64_p64_pw_avx2<16>(const uint8_t *src, const uint8_t *pred, int16_t *diff);
    template void diff_nxn_p64_p64_pw_avx2<32>(const uint8_t *src, const uint8_t *pred, int16_t *diff);
    template void diff_nxn_p64_p64_pw_avx2<64>(const uint8_t *src, const uint8_t *pred, int16_t *diff);

    template <int w> void diff_nxm_avx2(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height);
    template <> void diff_nxm_avx2<4>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height) {
        __m128i s, p;
        __m256i s1, s2, d;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        int pitchSrc4 = pitchSrc*4, pitchPred4 = pitchPred*4, pitchDiff4 = pitchDiff*4;
        for (int y = 0; y < height; y += 4, src += pitchSrc4, pred += pitchPred4, diff += pitchDiff4) {
            s = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(src), loadu_si32(src+pitchSrc)),
                                   _mm_unpacklo_epi32(loadu_si32(src+pitchSrc2), loadu_si32(src+pitchSrc3)));
            p = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(pred), loadu_si32(pred+pitchPred)),
                                   _mm_unpacklo_epi32(loadu_si32(pred+pitchPred2), loadu_si32(pred+pitchPred3)));
            s1 = _mm256_cvtepu8_epi16(s);
            s2 = _mm256_cvtepu8_epi16(p);
            d = _mm256_sub_epi16(s1, s2);
            storel_epi64(diff, si128(d));
            storeh_epi64(diff+pitchDiff, si128(d));
            storel_epi64(diff+pitchDiff2, _mm256_extracti128_si256(d,1));
            storeh_epi64(diff+pitchDiff3, _mm256_extracti128_si256(d,1));
        }
    }
    template <> void diff_nxm_avx2<8>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        int pitchSrc4 = pitchSrc*4, pitchPred4 = pitchPred*4, pitchDiff4 = pitchDiff*4;
        for (int y = 0; y < height; y += 4, src += pitchSrc4, pred += pitchPred4, diff += pitchDiff4) {
            s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src, src+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred, pred+pitchPred));
            storeu2_m128i(diff, diff+pitchDiff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src+pitchSrc2, src+pitchSrc3));
            s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred+pitchPred2, pred+pitchPred3));
            storeu2_m128i(diff+pitchDiff2, diff+pitchDiff3, _mm256_sub_epi16(s1, s2));
        }
    }
    template <> void diff_nxm_avx2<16>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        int pitchSrc4 = pitchSrc*4, pitchPred4 = pitchPred*4, pitchDiff4 = pitchDiff*4;
        for (int y = 0; y < height; y += 4, src += pitchSrc4, pred += pitchPred4, diff += pitchDiff4) {
            s1 = _mm256_cvtepu8_epi16(loada_si128(src));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred));
            storea_si256(diff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred));
            storea_si256(diff+pitchDiff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc2));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred2));
            storea_si256(diff+pitchDiff2, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc3));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred3));
            storea_si256(diff+pitchDiff3, _mm256_sub_epi16(s1, s2));
        }
    }
    template <> void diff_nxm_avx2<32>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        for (int y = 0; y < height; y+=2, src += pitchSrc2, pred += pitchPred2, diff += pitchDiff2) {
            s1 = _mm256_cvtepu8_epi16(loada_si128(src));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred));
            storea_si256(diff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+16));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+16));
            storea_si256(diff+16, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred));
            storea_si256(diff+pitchDiff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+16+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+16+pitchPred));
            storea_si256(diff+16+pitchDiff, _mm256_sub_epi16(s1, s2));
        }
    }
    template <> void diff_nxm_avx2<64>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height) {
        __m256i s1, s2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        for (int y = 0; y < height; y+=2, src += pitchSrc2, pred += pitchPred2, diff += pitchDiff2) {
            s1 = _mm256_cvtepu8_epi16(loada_si128(src));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred));
            storea_si256(diff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+16));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+16));
            storea_si256(diff+16, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+32));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+32));
            storea_si256(diff+32, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+48));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+48));
            storea_si256(diff+48, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred));
            storea_si256(diff+pitchDiff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc+16));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred+16));
            storea_si256(diff+pitchDiff+16, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc+32));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred+32));
            storea_si256(diff+pitchDiff+32, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loada_si128(src+pitchSrc+48));
            s2 = _mm256_cvtepu8_epi16(loada_si128(pred+pitchPred+48));
            storea_si256(diff+pitchDiff+48, _mm256_sub_epi16(s1, s2));
        }
    }

    template <int w> void diff_nxm_p64_p64_pw_avx2(const uint8_t *src, const uint8_t *pred, int16_t *diff, int height) {
        return diff_nxm_avx2<w>(src, 64, pred, 64, diff, w, height);
    }
    template <> void diff_nxm_p64_p64_pw_avx2<4>(const uint8_t *src, const uint8_t *pred, int16_t *diff, int height) {
        for (int y = 0; y < height; y += 4, src += 256, pred += 256, diff += 16) {
            __m128i s = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(src), loadu_si32(src+64)),
                                           _mm_unpacklo_epi32(loadu_si32(src+128), loadu_si32(src+192)));
            __m128i p = _mm_unpacklo_epi64(_mm_unpacklo_epi32(loadu_si32(pred), loadu_si32(pred+64)),
                                           _mm_unpacklo_epi32(loadu_si32(pred+128), loadu_si32(pred+192)));
            storea_si256(diff, _mm256_sub_epi16(_mm256_cvtepu8_epi16(s), _mm256_cvtepu8_epi16(p)));
        }
    }
    template <> void diff_nxm_p64_p64_pw_avx2<8>(const uint8_t *src, const uint8_t *pred, int16_t *diff, int height) {
        __m256i s1, s2;
        for (int y = 0; y < height; y += 4, src += 256, pred += 256, diff += 32) {
            s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src, src+64));
            s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred, pred+64));
            storea_si256(diff, _mm256_sub_epi16(s1, s2));
            s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src+128, src+192));
            s2 = _mm256_cvtepu8_epi16(loadu2_epi64(pred+128, pred+192));
            storea_si256(diff+16, _mm256_sub_epi16(s1, s2));
        }
    }
    template void diff_nxm_p64_p64_pw_avx2<16>(const uint8_t *src, const uint8_t *pred, int16_t *diff, int height);
    template void diff_nxm_p64_p64_pw_avx2<32>(const uint8_t *src, const uint8_t *pred, int16_t *diff, int height);
    template void diff_nxm_p64_p64_pw_avx2<64>(const uint8_t *src, const uint8_t *pred, int16_t *diff, int height);

    template <int w> void diff_nv12_avx2(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff1, int16_t *diff2, int pitchDiff, int height);
    template <> void diff_nv12_avx2<4>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff1, int16_t *diff2, int pitchDiff, int height) {
        __m256i u1, v1, uv1, u2, v2, uv2;
        const int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        const int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        const int pitchSrc4 = pitchSrc*4, pitchPred4 = pitchPred*4, pitchDiff4 = pitchDiff*4;
        for (int y = 0; y < height; y+=4, src += pitchSrc4, pred += pitchPred4, diff1 += pitchDiff4, diff2 += pitchDiff4) {
            uv1 = _mm256_setr_m128i(loadu2_epi64(src, src+pitchSrc), loadu2_epi64(src+pitchSrc2, src+pitchSrc3));
            uv2 = _mm256_setr_m128i(loadu2_epi64(pred, pred+pitchPred), loadu2_epi64(pred+pitchPred2, pred+pitchPred3));
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            __m256i diff = _mm256_sub_epi16(u1, u2);
            storel_epi64(diff1, si128(diff));
            storeh_epi64(diff1+pitchDiff, si128(diff));
            storel_epi64(diff1+pitchDiff2, _mm256_extracti128_si256(diff,1));
            storeh_epi64(diff1+pitchDiff3, _mm256_extracti128_si256(diff,1));
            diff = _mm256_sub_epi16(v1, v2);
            storel_epi64(diff2, si128(diff));
            storeh_epi64(diff2+pitchDiff, si128(diff));
            storel_epi64(diff2+pitchDiff2, _mm256_extracti128_si256(diff,1));
            storeh_epi64(diff2+pitchDiff3, _mm256_extracti128_si256(diff,1));
        }
    }
    template <> void diff_nv12_avx2<8>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff1, int16_t *diff2, int pitchDiff, int height) {
        __m256i u1, v1, uv1, u2, v2, uv2;
        const int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        const int pitchSrc3 = pitchSrc*3, pitchPred3 = pitchPred*3, pitchDiff3 = pitchDiff*3;
        const int pitchSrc4 = pitchSrc*4, pitchPred4 = pitchPred*4, pitchDiff4 = pitchDiff*4;
        for (int y = 0; y < height; y+=4, src += pitchSrc4, pred += pitchPred4, diff1 += pitchDiff4, diff2 += pitchDiff4) {
            uv1 = loadu2_m128i(src, src+pitchSrc);
            uv2 = loadu2_m128i(pred, pred+pitchPred);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storeu2_m128i(diff1, diff1+pitchDiff, _mm256_sub_epi16(u1, u2));
            storeu2_m128i(diff2, diff2+pitchDiff, _mm256_sub_epi16(v1, v2));
            uv1 = loadu2_m128i(src+pitchSrc2, src+pitchSrc3);
            uv2 = loadu2_m128i(pred+pitchPred2, pred+pitchPred3);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storeu2_m128i(diff1+pitchDiff2, diff1+pitchDiff3, _mm256_sub_epi16(u1, u2));
            storeu2_m128i(diff2+pitchDiff2, diff2+pitchDiff3, _mm256_sub_epi16(v1, v2));
        }
    }
    template <> void diff_nv12_avx2<16>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff1, int16_t *diff2, int pitchDiff, int height) {
        __m256i u1, v1, uv1, u2, v2, uv2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        for (int y = 0; y < height; y+=2, src += pitchSrc2, pred += pitchPred2, diff1 += pitchDiff2, diff2 += pitchDiff2) {
            uv1 = loada_si256(src);
            uv2 = loada_si256(pred);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2, _mm256_sub_epi16(v1, v2));

            uv1 = loada_si256(src+pitchSrc);
            uv2 = loada_si256(pred+pitchPred);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+pitchDiff, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+pitchDiff, _mm256_sub_epi16(v1, v2));
        }
    }
    template <> void diff_nv12_avx2<32>(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff1, int16_t *diff2, int pitchDiff, int height) {
        __m256i u1, v1, uv1, u2, v2, uv2;
        int pitchSrc2 = pitchSrc*2, pitchPred2 = pitchPred*2, pitchDiff2 = pitchDiff*2;
        for (int y = 0; y < height; y += 2, src += pitchSrc2, pred += pitchPred2, diff1 += pitchDiff2, diff2 += pitchDiff2) {
            uv1 = loada_si256(src);
            uv2 = loada_si256(pred);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2, _mm256_sub_epi16(v1, v2));
            uv1 = loada_si256(src+32);
            uv2 = loada_si256(pred+32);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+16, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+16, _mm256_sub_epi16(v1, v2));

            uv1 = loada_si256(src+pitchSrc);
            uv2 = loada_si256(pred+pitchPred);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+pitchDiff, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+pitchDiff, _mm256_sub_epi16(v1, v2));
            uv1 = loada_si256(src+pitchSrc+32);
            uv2 = loada_si256(pred+pitchPred+32);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+pitchDiff+16, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+pitchDiff+16, _mm256_sub_epi16(v1, v2));
        }
    }

    template <int w> void diff_nv12_p64_p64_pw_avx2(const uint8_t *src, const uint8_t *pred, int16_t *diff1, int16_t *diff2, int height) {
        return diff_nv12_avx2<w>(src, 64, pred, 64, diff1, diff2, w, height);
    }
    template <> void diff_nv12_p64_p64_pw_avx2<4>(const uint8_t *src, const uint8_t *pred, int16_t *diff1, int16_t *diff2, int height) {
        __m256i u1, v1, uv1, u2, v2, uv2;
        for (int y = 0; y < height; y+=4, src += 256, pred += 256, diff1 += 16, diff2 += 16) {
            uv1 = _mm256_setr_m128i(loadu2_epi64(src, src+64), loadu2_epi64(src+128, src+192));
            uv2 = _mm256_setr_m128i(loadu2_epi64(pred, pred+64), loadu2_epi64(pred+128, pred+192));
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2, _mm256_sub_epi16(v1, v2));
        }
    }
    template <> void diff_nv12_p64_p64_pw_avx2<8>(const uint8_t *src, const uint8_t *pred, int16_t *diff1, int16_t *diff2, int height) {
        __m256i u1, v1, uv1, u2, v2, uv2;
        for (int y = 0; y < height; y+=4, src += 256, pred += 256, diff1 += 32, diff2 += 32) {
            uv1 = loada2_m128i(src, src+64);
            uv2 = loada2_m128i(pred, pred+64);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2, _mm256_sub_epi16(v1, v2));
            uv1 = loada2_m128i(src+128, src+192);
            uv2 = loada2_m128i(pred+128, pred+192);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+16, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+16, _mm256_sub_epi16(v1, v2));
        }
    }
    template void diff_nv12_p64_p64_pw_avx2<16>(const uint8_t *src, const uint8_t *pred, int16_t *diff1, int16_t *diff2, int height);
    template <> void diff_nv12_p64_p64_pw_avx2<32>(const uint8_t *src, const uint8_t *pred, int16_t *diff1, int16_t *diff2, int height) {
        __m256i u1, v1, uv1, u2, v2, uv2;
        for (int y = 0; y < height; y+=2, src += 128, pred += 128, diff1 += 64, diff2 += 64) {
            uv1 = loada_si256(src);
            uv2 = loada_si256(pred);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2, _mm256_sub_epi16(v1, v2));
            uv1 = loada_si256(src+32);
            uv2 = loada_si256(pred+32);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+16, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+16, _mm256_sub_epi16(v1, v2));
            uv1 = loada_si256(src+64);
            uv2 = loada_si256(pred+64);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+32, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+32, _mm256_sub_epi16(v1, v2));
            uv1 = loada_si256(src+96);
            uv2 = loada_si256(pred+96);
            u1 = _mm256_and_si256(uv1, _mm256_set1_epi16(0xff));
            u2 = _mm256_and_si256(uv2, _mm256_set1_epi16(0xff));
            v1 = _mm256_srli_epi16(uv1, 8);
            v2 = _mm256_srli_epi16(uv2, 8);
            storea_si256(diff1+48, _mm256_sub_epi16(u1, u2));
            storea_si256(diff2+48, _mm256_sub_epi16(v1, v2));
        }
    }
}
