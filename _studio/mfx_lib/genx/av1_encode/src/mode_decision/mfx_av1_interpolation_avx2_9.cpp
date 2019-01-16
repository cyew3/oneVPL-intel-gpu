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
#include "ipps.h"
#include "mfx_av1_opts_intrin.h"

#include <stdint.h>

namespace VP9PP
{
    namespace details {
        ALIGN_DECL(32) const uint8_t shuftab_horz1[32] = {0,1,1,2,2,3,3,4, 4,5,5,6,6,7,7,8, 0,1,1,2,2,3,3,4, 4,5,5,6,6,7,7,8};
        ALIGN_DECL(32) const uint8_t shuftab_horz2[32] = {2,3,3,4,4,5,5,6, 6,7,7,8,8,9,9,10, 2,3,3,4,4,5,5,6, 6,7,7,8,8,9,9,10};
        ALIGN_DECL(32) const uint8_t shuftab_horz3[32] = {4,5,5,6,6,7,7,8, 8,9,9,10,10,11,11,12, 4,5,5,6,6,7,7,8, 8,9,9,10,10,11,11,12};
        ALIGN_DECL(32) const uint8_t shuftab_horz4[32] = {6,7,7,8,8,9,9,10, 10,11,11,12,12,13,13,14, 6,7,7,8,8,9,9,10, 10,11,11,12,12,13,13,14};
        ALIGN_DECL(16) const uint8_t shuftab_horz_w4_1[32] = {0,1,1,2,2,3,3,4, 8,9,9,10,10,11,11,12};
        ALIGN_DECL(16) const uint8_t shuftab_horz_w4_2[32] = {2,3,3,4,4,5,5,6, 10,11,11,12,12,13,13,14};

        ALIGN_DECL(32) const uint8_t shuftab_nv12_horz1[32] = {0,2,1,3,2,4,3,5, 4,6,5,7,6,8,7,9, 0,2,1,3,2,4,3,5, 4,6,5,7,6,8,7,9};
        ALIGN_DECL(32) const uint8_t shuftab_nv12_horz2[32] = {4,6,5,7,6,8,7,9, 8,10,9,11,10,12,11,13, 4,6,5,7,6,8,7,9, 8,10,9,11,10,12,11,13};

        template <int avg> void interp_vert_avx2_w4(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *pfilt, int h) {
            // lineA: A0 A1 A2 A3
            // lineB: B0 B1 B2 B3
            // lineC: C0 C1 C2 C3
            // lineD: D0 D1 D2 D3
            // lineE: E0 E1 E2 E3
            // lineF: F0 F1 F2 F3
            // lineG: G0 G1 G2 G3
            // lineH: H0 H1 H2 H3
            // lineI: I0 I1 I2 I3
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt1 = _mm_broadcastd_epi32(filt);
            __m128i filt2 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt,4));

            __m128i lineA = loadl_si32(src - pitchSrc*3);
            __m128i lineB = loadl_si32(src - pitchSrc*2);
            __m128i lineC = loadl_si32(src - pitchSrc*1);
            __m128i lineD = loadl_si32(src + pitchSrc*0);
            __m128i lineE = loadl_si32(src + pitchSrc*1);
            __m128i lineF = loadl_si32(src + pitchSrc*2);
            __m128i lineG = loadl_si32(src + pitchSrc*3);
            __m128i lineH;
            __m128i linesABCD, linesEFGH;
            for (int32_t y = 0; y < h; y++, src += pitchSrc, dst += pitchDst) {
                lineH = loadl_si32(src + pitchSrc*4);
                linesABCD = _mm_unpacklo_epi16(_mm_unpacklo_epi8(lineA, lineB), _mm_unpacklo_epi8(lineC, lineD));   // 16b: A0 B0 C0 D0 A1 B1 C1 D1 A2 B2 C2 D2 A3 B3 C3 D3
                linesEFGH = _mm_unpacklo_epi16(_mm_unpacklo_epi8(lineE, lineF), _mm_unpacklo_epi8(lineG, lineH));   // 16b: E0 F0 G0 H0 E1 F1 G1 H1 E2 F2 G2 H2 E3 F3 G3 H3
                linesABCD = _mm_maddubs_epi16(linesABCD, filt1);
                linesEFGH = _mm_maddubs_epi16(linesEFGH, filt2);
                __m128i res1 = _mm_add_epi16(linesABCD, linesEFGH);
                res1 = _mm_hadds_epi16(res1, res1);
                res1 = _mm_adds_epi16(res1, _mm_set1_epi16(64));
                res1 = _mm_srai_epi16(res1, 7);
                res1 = _mm_packus_epi16(res1, res1);
                if (avg)
                    res1 = _mm_avg_epu8(res1, loadl_si32(dst));
                storel_si32(dst, res1);
                lineA = lineB;
                lineB = lineC;
                lineC = lineD;
                lineD = lineE;
                lineE = lineF;
                lineF = lineG;
                lineG = lineH;
            }
        }

        template <int avg> void interp_vert_avx2_w8(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *pfilt, int h) {
            // lineA: A0 A1 ... A6 A7 | B0 B1 ... B6 B7
            // lineB: B0 B1 ... B6 B7 | C0 C1 ... C6 C7
            // lineC: C0 C1 ... C6 C7 | D0 D1 ... D6 D7
            // lineD: D0 D1 ... D6 D7 | E0 E1 ... E6 E7
            // lineE: E0 E1 ... E6 E7 | F0 F1 ... F6 F7
            // lineF: F0 F1 ... F6 F7 | G0 G1 ... G6 G7
            // lineG: G0 G1 ... G6 G7 | H0 H1 ... H6 H7
            // lineH: H0 H1 ... H6 H7 | I0 I1 ... I6 I7
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt);
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));

            __m128i lineA_ = loadl_epi64(src - pitchSrc*3);
            __m128i lineB_ = loadl_epi64(src - pitchSrc*2);
            __m128i lineC_ = loadl_epi64(src - pitchSrc*1);
            __m128i lineD_ = loadl_epi64(src + pitchSrc*0);
            __m128i lineE_ = loadl_epi64(src + pitchSrc*1);
            __m128i lineF_ = loadl_epi64(src + pitchSrc*2);
            __m128i lineG_ = loadl_epi64(src + pitchSrc*3);
            __m128i lineH_, lineI_;

            __m256i lineA = _mm256_inserti128_si256(si256(lineA_), lineB_, 1);
            __m256i lineB = _mm256_inserti128_si256(si256(lineB_), lineC_, 1);
            __m256i lineC = _mm256_inserti128_si256(si256(lineC_), lineD_, 1);
            __m256i lineD = _mm256_inserti128_si256(si256(lineD_), lineE_, 1);
            __m256i lineE = _mm256_inserti128_si256(si256(lineE_), lineF_, 1);
            __m256i lineF = _mm256_inserti128_si256(si256(lineF_), lineG_, 1);
            __m256i lineG = si256(lineG_);
            __m256i lineH;

            __m256i linesAB, linesCD, linesEF, linesGH;
            linesAB = _mm256_unpacklo_epi8(lineA, lineB);
            linesCD = _mm256_unpacklo_epi8(lineC, lineD);
            linesEF = _mm256_unpacklo_epi8(lineE, lineF);

            __m256i tmpAB, tmpCD, tmpEF, tmpGH;
            __m256i res, res1, res2, tmp1, tmp2;

            const int pitchDst2 = pitchDst * 2;
            const int pitchDst3 = pitchDst * 3;
            const int pitchDst4 = pitchDst * 4;
            const int pitchSrc2 = pitchSrc * 2;
            const int pitchSrc3 = pitchSrc * 3;
            const int pitchSrc4 = pitchSrc * 4;

            for (int32_t y = 0; y < h; y += 4) {
                lineH_ = loadl_epi64(src + pitchSrc4);
                lineI_ = loadl_epi64(src + pitchSrc4 + pitchSrc);
                lineG = _mm256_inserti128_si256(lineG, lineH_, 1);
                lineH = _mm256_inserti128_si256(si256(lineH_), lineI_, 1);
                linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                tmpAB = _mm256_maddubs_epi16(linesAB, filt01);
                tmpCD = _mm256_maddubs_epi16(linesCD, filt23);
                tmpEF = _mm256_maddubs_epi16(linesEF, filt45);
                tmpGH = _mm256_maddubs_epi16(linesGH, filt67);

                tmp1 = _mm256_add_epi16(tmpAB, tmpEF); // order of addition matters to avoid overflow
                tmp2 = _mm256_add_epi16(tmpCD, tmpGH);
                res1 = _mm256_adds_epi16(tmp1, tmp2);
                res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                res1 = _mm256_srai_epi16(res1, 7);

                linesAB = linesCD;
                linesCD = linesEF;
                linesEF = linesGH;
                lineG = si256(lineI_);
                lineH_ = loadl_epi64(src + pitchSrc4 + pitchSrc2);
                lineI_ = loadl_epi64(src + pitchSrc4 + pitchSrc3);
                lineG = _mm256_inserti128_si256(lineG, lineH_, 1);
                lineH = _mm256_inserti128_si256(si256(lineH_), lineI_, 1);
                linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                tmpAB = _mm256_maddubs_epi16(linesAB, filt01);
                tmpCD = _mm256_maddubs_epi16(linesCD, filt23);
                tmpEF = _mm256_maddubs_epi16(linesEF, filt45);
                tmpGH = _mm256_maddubs_epi16(linesGH, filt67);

                tmp1 = _mm256_add_epi16(tmpAB, tmpEF); // order of addition matters to avoid overflow
                tmp2 = _mm256_add_epi16(tmpCD, tmpGH);
                res2 = _mm256_adds_epi16(tmp1, tmp2);
                res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                res2 = _mm256_srai_epi16(res2, 7);

                res = _mm256_packus_epi16(res1, res2);
                if (avg) {
                    __m256i d = si256(loadu2_epi64(dst, dst + pitchDst2));
                    d = _mm256_inserti128_si256(d, loadu2_epi64(dst + pitchDst, dst + pitchDst3), 1);
                    res = _mm256_avg_epu8(res, d);
                }
                storel_epi64(dst, si128(res));
                storel_epi64(dst + pitchDst, _mm256_extracti128_si256(res, 1));
                storeh_epi64(dst + pitchDst2, si128(res));
                storeh_epi64(dst + pitchDst3, _mm256_extracti128_si256(res, 1));

                linesAB = linesCD;
                linesCD = linesEF;
                linesEF = linesGH;
                lineG = si256(lineI_);
                dst += pitchDst4;
                src += pitchSrc4;
            }
        }

        // special case: pitchSrc == width
        template <int avg> void interp_vert_pw_avx2_w8(const uint8_t *src, uint8_t *dst, int pitchDst, const int16_t *pfilt, int h) {
            // lineA: A0 A1 ... A6 A7 | B0 B1 ... B6 B7
            // lineB: B0 B1 ... B6 B7 | C0 C1 ... C6 C7
            // lineC: C0 C1 ... C6 C7 | D0 D1 ... D6 D7
            // lineD: D0 D1 ... D6 D7 | E0 E1 ... E6 E7
            // lineE: E0 E1 ... E6 E7 | F0 F1 ... F6 F7
            // lineF: F0 F1 ... F6 F7 | G0 G1 ... G6 G7
            // lineG: G0 G1 ... G6 G7 | H0 H1 ... H6 H7
            // lineH: H0 H1 ... H6 H7 | I0 I1 ... I6 I7
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt);
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));

            assert((size_t(src - 8*3) & 31) == 0);
            __m256i lineABCD = loada_si256(src - 8*3);
            __m256i lineEFGH = loada_si256(src + 8*1);
            __m256i lineIKLM;

            __m256i lineA = _mm256_permute4x64_epi64(lineABCD, PERM4x64(0,1,1,2));
            __m256i lineB = _mm256_bsrli_epi128(lineA, 8);
            __m256i lineC = _mm256_permute4x64_epi64(lineABCD, PERM4x64(2,2,3,3));
            __m256i lineD = _mm256_permute2x128_si256(lineC, lineEFGH, PERM2x128(1,2));
            __m256i lineE = _mm256_permute4x64_epi64(lineEFGH, PERM4x64(0,1,1,2));
            __m256i lineF = _mm256_bsrli_epi128(lineE, 8);
            __m256i lineG = _mm256_permute4x64_epi64(lineEFGH, PERM4x64(2,2,3,3));
            __m256i lineH;

            __m256i linesAB, linesCD, linesEF, linesGH;
            linesAB = _mm256_unpacklo_epi8(lineA, lineB);
            linesCD = _mm256_unpacklo_epi8(lineC, lineD);
            linesEF = _mm256_unpacklo_epi8(lineE, lineF);

            __m256i tmpAB, tmpCD, tmpEF, tmpGH;
            __m256i res, res1, res2, tmp1, tmp2;

            const int pitchDst2 = pitchDst * 2;
            const int pitchDst3 = pitchDst * 3;
            const int pitchDst4 = pitchDst * 4;

            src += 8*5;
            for (int32_t y = 0; y < h; y += 4) {
                lineIKLM = loada_si256(src);
                lineH = _mm256_permute2x128_si256(lineG, lineIKLM, PERM2x128(1,2));
                linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                tmpAB = _mm256_maddubs_epi16(linesAB, filt01);
                tmpCD = _mm256_maddubs_epi16(linesCD, filt23);
                tmpEF = _mm256_maddubs_epi16(linesEF, filt45);
                tmpGH = _mm256_maddubs_epi16(linesGH, filt67);

                tmp1 = _mm256_add_epi16(tmpAB, tmpEF); // order of addition matters to avoid overflow
                tmp2 = _mm256_add_epi16(tmpCD, tmpGH);
                res1 = _mm256_adds_epi16(tmp1, tmp2);
                res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                res1 = _mm256_srai_epi16(res1, 7);

                linesAB = linesCD;
                linesCD = linesEF;
                linesEF = linesGH;
                lineG = _mm256_permute4x64_epi64(lineIKLM, PERM4x64(0,1,1,2));
                lineH = _mm256_bsrli_epi128(lineG, 8);
                linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                tmpAB = _mm256_maddubs_epi16(linesAB, filt01);
                tmpCD = _mm256_maddubs_epi16(linesCD, filt23);
                tmpEF = _mm256_maddubs_epi16(linesEF, filt45);
                tmpGH = _mm256_maddubs_epi16(linesGH, filt67);

                tmp1 = _mm256_add_epi16(tmpAB, tmpEF); // order of addition matters to avoid overflow
                tmp2 = _mm256_add_epi16(tmpCD, tmpGH);
                res2 = _mm256_adds_epi16(tmp1, tmp2);
                res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                res2 = _mm256_srai_epi16(res2, 7);

                res = _mm256_packus_epi16(res1, res2);
                if (avg) {
                    __m256i d = si256(loadu2_epi64(dst, dst + pitchDst2));
                    d = _mm256_inserti128_si256(d, loadu2_epi64(dst + pitchDst, dst + pitchDst3), 1);
                    res = _mm256_avg_epu8(res, d);
                }
                storel_epi64(dst, si128(res));
                storel_epi64(dst + pitchDst, _mm256_extracti128_si256(res, 1));
                storeh_epi64(dst + pitchDst2, si128(res));
                storeh_epi64(dst + pitchDst3, _mm256_extracti128_si256(res, 1));

                linesAB = linesCD;
                linesCD = linesEF;
                linesEF = linesGH;
                lineEFGH = lineIKLM;
                lineG = _mm256_permute4x64_epi64(lineIKLM, PERM4x64(2,2,3,3));
                dst += pitchDst4;
                src += 8 * 4;
            }
        }

        template <int avg> void interp_vert_avx2_w16(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *pfilt, int h) {
            // lineA: A0 A1 ... A14 A15
            // lineB: B0 B1 ... B14 B15
            // lineC: C0 C1 ... C14 C15
            // lineD: D0 D1 ... D14 D15
            // lineE: E0 E1 ... E14 E15
            // lineF: F0 F1 ... F14 F15
            // lineG: G0 G1 ... G14 G15
            // lineH: H0 H1 ... H14 H15
            // lineI: I0 I1 ... I14 I15
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt);
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));

            __m256i lineAB = loadu2_m128i(src - pitchSrc*3, src - pitchSrc*2);          // 32b: A0 ... A15 | B0 ... B15
            __m256i lineBC = loadu2_m128i(src - pitchSrc*2, src - pitchSrc*1);          // 32b: B0 ... B15 | C0 ... C15
            __m256i lineDE = loadu2_m128i(src - pitchSrc*0, src + pitchSrc*1);          // 32b: D0 ... D15 | E0 ... E15
            __m256i lineFG = loadu2_m128i(src + pitchSrc*2, src + pitchSrc*3);          // 32b: F0 ... F15 | G0 ... G15

            __m256i lineCD = _mm256_permute2x128_si256(lineBC, lineDE, PERM2x128(1,2)); // 32b: C0 ... C15 | D0 ... D15
            __m256i lineEF = _mm256_permute2x128_si256(lineDE, lineFG, PERM2x128(1,2)); // 32b: E0 ... E15 | F0 ... F15

            __m256i lineHI, lineGH;
            for (int32_t y = 0; y < h; y += 2) {
                lineHI = loadu2_m128i(src + pitchSrc*4, src + pitchSrc*5);              // 32b: H0 ... H15 | I0 ... I15
                lineGH = _mm256_permute2x128_si256(lineFG, lineHI, PERM2x128(1,2));     // 32b: G0 ... G15 | H0 ... H15

                __m256i linesABBC = _mm256_unpacklo_epi8(lineAB, lineBC); // 32b: A0 B0 A1 B1 ... | B0 C0 B1 C1 ...
                __m256i linesCDDE = _mm256_unpacklo_epi8(lineCD, lineDE); // 32b: C0 D0 C1 D1 ... | D0 E0 D1 E1 ...
                __m256i linesEFFG = _mm256_unpacklo_epi8(lineEF, lineFG); // 32b: E0 F0 E1 F1 ... | F0 G0 F1 G1 ...
                __m256i linesGHHI = _mm256_unpacklo_epi8(lineGH, lineHI); // 32b: G0 H0 G1 H1 ... | H0 I0 H1 I1 ...

                linesABBC = _mm256_maddubs_epi16(linesABBC, filt01);
                linesCDDE = _mm256_maddubs_epi16(linesCDDE, filt23);
                linesEFFG = _mm256_maddubs_epi16(linesEFFG, filt45);
                linesGHHI = _mm256_maddubs_epi16(linesGHHI, filt67);

                __m256i res1, res1a, res2, res2a;
                res1  = _mm256_add_epi16(linesABBC, linesEFFG); // order of addition matters to avoid overflow
                res1a = _mm256_add_epi16(linesCDDE, linesGHHI);
                res1  = _mm256_adds_epi16(res1, res1a);
                res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                res1 = _mm256_srai_epi16(res1, 7);  // 16w: row0 0-7 | row1 0-7

                linesABBC = _mm256_unpackhi_epi8(lineAB, lineBC); // 32b: A8 B8 A9 B9 ... | B8 C8 B9 C9 ...
                linesCDDE = _mm256_unpackhi_epi8(lineCD, lineDE); // 32b: C8 D8 C9 D9 ... | D8 E8 D9 E9 ...
                linesEFFG = _mm256_unpackhi_epi8(lineEF, lineFG); // 32b: E8 F8 E9 F9 ... | F8 G8 F9 G9 ...
                linesGHHI = _mm256_unpackhi_epi8(lineGH, lineHI); // 32b: G8 H8 G9 H9 ... | H8 I8 H9 I9 ...

                linesABBC = _mm256_maddubs_epi16(linesABBC, filt01);
                linesCDDE = _mm256_maddubs_epi16(linesCDDE, filt23);
                linesEFFG = _mm256_maddubs_epi16(linesEFFG, filt45);
                linesGHHI = _mm256_maddubs_epi16(linesGHHI, filt67);

                res2  = _mm256_add_epi16(linesABBC, linesEFFG); // order of addition matters to avoid overflow
                res2a = _mm256_add_epi16(linesCDDE, linesGHHI);
                res2  = _mm256_adds_epi16(res2, res2a);
                res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                res2 = _mm256_srai_epi16(res2, 7);  // 16w: row0 8-15 | row1 8-15

                __m256i res = _mm256_packus_epi16(res1, res2);  // 32b: row0 0-15 | row1 0-15
                if (avg)
                    res = _mm256_avg_epu8(res, loadu2_m128i(dst, dst + pitchDst));
                storeu2_m128i(dst, dst + pitchDst, res);

                dst += pitchDst*2;
                src += pitchSrc*2;
                lineAB = lineCD;
                lineBC = lineDE;
                lineCD = lineEF;
                lineDE = lineFG;
                lineEF = lineGH;
                lineFG = lineHI;
            }
        }

        template <int w, int avg> void interp_vert_avx2_w32(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *pfilt, int h) {
            // lineA: A0 A1 ... A14 A15 | A16 A17 ... A30 A31
            // lineB: B0 B1 ... B14 B15 | B16 B17 ... B30 B31
            // lineC: C0 C1 ... C14 C15 | C16 C17 ... C30 C31
            // lineD: D0 D1 ... D14 D15 | D16 D17 ... D30 D31
            // lineE: E0 E1 ... E14 E15 | E16 E17 ... E30 E31
            // lineF: F0 F1 ... F14 F15 | F16 F17 ... F30 F31
            // lineG: G0 G1 ... G14 G15 | G16 G17 ... G30 G31
            // lineH: H0 H1 ... H14 H15 | H16 H17 ... H30 H31
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: 0 1 0 1 ...
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 32b: 2 3 2 3 ...
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 32b: 4 5 4 5 ...
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 32b: 6 7 6 7 ...

            for (int32_t x = 0; x < w; x += 32) {
                const uint8_t *psrc = src + x;
                uint8_t *pdst = dst + x;
                __m256i lineA = loadu_si256(psrc - pitchSrc*3);  // 32b: A0 A1 ... A14 A15 | A16 A17 ... A30 A31
                __m256i lineB = loadu_si256(psrc - pitchSrc*2);  // 32b: B0 B1 ... B14 B15 | B16 B17 ... B30 B31
                __m256i lineC = loadu_si256(psrc - pitchSrc);
                __m256i lineD = loadu_si256(psrc);
                __m256i lineE = loadu_si256(psrc + pitchSrc);
                __m256i lineF = loadu_si256(psrc + pitchSrc*2);
                __m256i lineG = loadu_si256(psrc + pitchSrc*3);

                for (int32_t y = 0; y < h; y++) {
                    __m256i lineH = loadu_si256(psrc + pitchSrc*4);

                    __m256i linesAB = _mm256_unpacklo_epi8(lineA, lineB); // 32b: A0 B0 A1 B1 ... | A16 B16 A17 B17 ...
                    __m256i linesCD = _mm256_unpacklo_epi8(lineC, lineD);
                    __m256i linesEF = _mm256_unpacklo_epi8(lineE, lineF);
                    __m256i linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                    linesAB = _mm256_maddubs_epi16(linesAB, filt01);    // 16w: A0*f0+B0*f1 A1*f0+B1*f1 ... | A16*f0+B16*f1 A17*f0+B17*f1 ...
                    linesCD = _mm256_maddubs_epi16(linesCD, filt23);
                    linesEF = _mm256_maddubs_epi16(linesEF, filt45);
                    linesGH = _mm256_maddubs_epi16(linesGH, filt67);

                    __m256i res1, res1a, res2, res2a;
                    res1  = _mm256_add_epi16(linesAB, linesEF); // order of addition matters to avoid overflow
                    res1a = _mm256_add_epi16(linesCD, linesGH);
                    res1  = _mm256_adds_epi16(res1, res1a);
                    res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                    res1 = _mm256_srai_epi16(res1, 7);  // 16w: 0 1 ... 6 7 | 16 17 ... 22 23

                    linesAB = _mm256_unpackhi_epi8(lineA, lineB);
                    linesCD = _mm256_unpackhi_epi8(lineC, lineD);
                    linesEF = _mm256_unpackhi_epi8(lineE, lineF);
                    linesGH = _mm256_unpackhi_epi8(lineG, lineH);

                    linesAB = _mm256_maddubs_epi16(linesAB, filt01);
                    linesCD = _mm256_maddubs_epi16(linesCD, filt23);
                    linesEF = _mm256_maddubs_epi16(linesEF, filt45);
                    linesGH = _mm256_maddubs_epi16(linesGH, filt67);

                    res2  = _mm256_add_epi16(linesAB, linesEF); // order of addition matters to avoid overflow
                    res2a = _mm256_add_epi16(linesCD, linesGH);
                    res2  = _mm256_adds_epi16(res2, res2a);
                    res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                    res2 = _mm256_srai_epi16(res2, 7);  // 16w: 8 9 ... 14 15 | 24 25 ... 30 31

                    __m256i res = _mm256_packus_epi16(res1, res2);  // 32b: 0 1 .. 30 31
                    if (avg)
                        res = _mm256_avg_epu8(res, loada_si256(pdst));
                    storea_si256(pdst, res);

                    pdst += pitchDst;
                    psrc += pitchSrc;
                    lineA = lineB;
                    lineB = lineC;
                    lineC = lineD;
                    lineD = lineE;
                    lineE = lineF;
                    lineF = lineG;
                    lineG = lineH;
                }
            }
        }

        template <int avg> void interp_horz_avx2_w4(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt); // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_horz_w4_1);
            __m128i shuftab2 = loada_si128(shuftab_horz_w4_2);

            for (int32_t y = 0; y < h; y += 2, psrc += pitchSrc*2, pdst += pitchDst*2) {
                __m128i src, a, b, c, d;
                src = loadu2_epi64(psrc-3, psrc+pitchSrc-3);   // A00 A01 A02 A03 A04 A05 A06 A07  B00 B01 B02 B03 B04 B05 B06 B07
                a = _mm_shuffle_epi8(src, shuftab1);           // A00 A01 A01 A02 A02 A03 A03 A04  B00 B01 B01 B02 B02 B03 B03 B04
                b = _mm_shuffle_epi8(src, shuftab2);           // A02 A03 A03 A04 A04 A05 A05 A06  B02 B03 B03 B04 B04 B05 B05 B06

                src = loadu2_epi64(psrc+1, psrc+pitchSrc+1);   // A04 A05 A06 A07 A08 A09 A10 A11  B04 B05 B06 B07 B08 B09 B10 B11
                c = _mm_shuffle_epi8(src, shuftab1);           // A04 A05 A05 A06 A06 A07 A07 A08  B04 B05 B05 B06 B06 B07 B07 B08
                d = _mm_shuffle_epi8(src, shuftab2);           // A06 A07 A07 A08 A08 A09 A09 A10  B06 B07 B07 B08 B08 B09 B09 B10

                a = _mm_maddubs_epi16(a, filt01);
                b = _mm_maddubs_epi16(b, filt23);
                c = _mm_maddubs_epi16(c, filt45);
                d = _mm_maddubs_epi16(d, filt67);

                __m128i res1 = _mm_adds_epi16(_mm_add_epi16(a,c), _mm_add_epi16(b,d)); // order of addition matters to avoid overflow
                res1 = _mm_adds_epi16(res1, _mm_set1_epi16(64));
                res1 = _mm_srai_epi16(res1, 7);         // 8w: A00 A01 A02 A03 B00 B01 B02 B03
                res1 = _mm_packus_epi16(res1, res1);

                if (avg)
                    res1 = _mm_avg_epu8(res1, _mm_unpacklo_epi32(loadl_si32(pdst), loadl_si32(pdst+pitchDst)));
                storel_si32(pdst, res1);
                storel_si32(pdst+pitchDst, _mm_bsrli_si128(res1, 4));
            }
        }

        template <int avg> void interp_horz_avx2_w8(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_horz1);
            __m256i shuftab2 = loada_si256(shuftab_horz2);
            __m256i shuftab3 = loada_si256(shuftab_horz3);
            __m256i shuftab4 = loada_si256(shuftab_horz4);

            for (int32_t y = 0; y < h; y += 4, psrc += pitchSrc*4, pdst += pitchDst*4) {
                __m256i src, a, b, c, d, res1, res2, res;
                src = loadu2_m128i(psrc - 3, psrc - 3 + pitchSrc);  // A00 A01 A02 A03 A04 A05 A06 A07  A08 A09 A10 A11 A12 A13 A14 A15  B00 B01 B02 B03 B04 B05 B06 B07  B08 B09 B10 B11 B12 B13 B14 B15
                a = _mm256_shuffle_epi8(src, shuftab1);             // A00 A01 A01 A02 A02 A03 A03 A04  A04 A05 A05 A06 A06 A07 A07 A08  B..
                b = _mm256_shuffle_epi8(src, shuftab2);             // A02 A03 A03 A04 A04 A05 A05 A06  A06 A07 A07 A08 A08 A09 A09 A10  B..
                c = _mm256_shuffle_epi8(src, shuftab3);             // A04 A05 A05 A06 A06 A07 A07 A08  A08 A09 A09 A10 A10 A11 A11 A12  B..
                d = _mm256_shuffle_epi8(src, shuftab4);             // A06 A07 A07 A08 A08 A09 A09 A10  A10 A11 A11 A12 A12 A13 A13 A14  B..
                a = _mm256_maddubs_epi16(a, filt01);
                b = _mm256_maddubs_epi16(b, filt23);
                c = _mm256_maddubs_epi16(c, filt45);
                d = _mm256_maddubs_epi16(d, filt67);
                res1 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                res1 = _mm256_srai_epi16(res1, 7);                  // 16w: A00 A01 A02 A03 A04 A05 A06 A07  B..

                src = loadu2_m128i(psrc - 3 + pitchSrc * 2, psrc - 3 + pitchSrc * 3);  // C..  D..
                a = _mm256_shuffle_epi8(src, shuftab1);
                b = _mm256_shuffle_epi8(src, shuftab2);
                c = _mm256_shuffle_epi8(src, shuftab3);
                d = _mm256_shuffle_epi8(src, shuftab4);
                a = _mm256_maddubs_epi16(a, filt01);
                b = _mm256_maddubs_epi16(b, filt23);
                c = _mm256_maddubs_epi16(c, filt45);
                d = _mm256_maddubs_epi16(d, filt67);
                res2 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                res2 = _mm256_srai_epi16(res2, 7);                  // 16w: C..  D..

                res = _mm256_packus_epi16(res1, res2);     // 32b: A.. C.. B.. D..

                if (avg) {
                    __m128i d1 = loadu2_epi64(pdst, pdst + pitchDst * 2);
                    __m128i d2 = loadu2_epi64(pdst + pitchDst, pdst + pitchDst * 3);
                    __m256i d12 = _mm256_inserti128_si256(si256(d1), d2, 1);
                    res = _mm256_avg_epu8(res, d12);
                }
                storel_epi64(pdst, si128(res));
                storel_epi64(pdst+pitchDst, _mm256_extracti128_si256(res, 1));
                storeh_epi64(pdst+pitchDst*2, si128(res));
                storeh_epi64(pdst+pitchDst*3, _mm256_extracti128_si256(res, 1));
            }
        }

        // special case: pitchDst == width
        void interp_horz_pw_avx2_w8(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_horz1);
            __m256i shuftab2 = loada_si256(shuftab_horz2);
            __m256i shuftab3 = loada_si256(shuftab_horz3);
            __m256i shuftab4 = loada_si256(shuftab_horz4);

            psrc -= 3;
            const int pitchSrc2 = pitchSrc * 2;
            const int pitchSrc3 = pitchSrc * 3;
            const int pitchSrc4 = pitchSrc * 4;
            for (int32_t y = 0; y < h; y += 4, psrc += pitchSrc4) {
                __m256i src, a, b, c, d, res1, res2, res;
                src = loadu2_m128i(psrc, psrc + pitchSrc2); // A00 A01 A02 A03 A04 A05 A06 A07  A08 A09 A10 A11 A12 A13 A14 A15  C00 C01 C02 C03 C04 C05 C06 C07  C08 C09 C10 C11 C12 C13 C14 C15
                a = _mm256_shuffle_epi8(src, shuftab1);     // A00 A01 A01 A02 A02 A03 A03 A04  A04 A05 A05 A06 A06 A07 A07 A08  C..
                b = _mm256_shuffle_epi8(src, shuftab2);     // A02 A03 A03 A04 A04 A05 A05 A06  A06 A07 A07 A08 A08 A09 A09 A10  C..
                c = _mm256_shuffle_epi8(src, shuftab3);     // A04 A05 A05 A06 A06 A07 A07 A08  A08 A09 A09 A10 A10 A11 A11 A12  C..
                d = _mm256_shuffle_epi8(src, shuftab4);     // A06 A07 A07 A08 A08 A09 A09 A10  A10 A11 A11 A12 A12 A13 A13 A14  C..
                a = _mm256_maddubs_epi16(a, filt01);
                b = _mm256_maddubs_epi16(b, filt23);
                c = _mm256_maddubs_epi16(c, filt45);
                d = _mm256_maddubs_epi16(d, filt67);
                res1 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                res1 = _mm256_srai_epi16(res1, 7);                  // 16w: A00 A01 A02 A03 A04 A05 A06 A07  B..

                src = loadu2_m128i(psrc + pitchSrc, psrc + pitchSrc3);  // B..  D..
                a = _mm256_shuffle_epi8(src, shuftab1);
                b = _mm256_shuffle_epi8(src, shuftab2);
                c = _mm256_shuffle_epi8(src, shuftab3);
                d = _mm256_shuffle_epi8(src, shuftab4);
                a = _mm256_maddubs_epi16(a, filt01);
                b = _mm256_maddubs_epi16(b, filt23);
                c = _mm256_maddubs_epi16(c, filt45);
                d = _mm256_maddubs_epi16(d, filt67);
                res2 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                res2 = _mm256_srai_epi16(res2, 7);                  // 16w: B..  D..

                res = _mm256_packus_epi16(res1, res2);     // 32b: A.. B.. C.. D..
                storea_si256(pdst + y * 8, res);
            }
        }

        template <int avg> void interp_horz_avx2_w16(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_horz1);
            __m256i shuftab2 = loada_si256(shuftab_horz2);
            __m256i shuftab3 = loada_si256(shuftab_horz3);
            __m256i shuftab4 = loada_si256(shuftab_horz4);

            for (int32_t y = 0; y < h; y += 2, psrc += pitchSrc*2, pdst += pitchDst*2) {
                __m256i src, a, b, c, d;
                src = loadu2_m128i(psrc-3, psrc+pitchSrc-3);  // A00 A01 A02 A03 A04 A05 A06 A07  A08 A09 A10 A11 A12 A13 A14 A15 | B00 B01 B02 B03 B04 B05 B06 B07  B08 B09 B10 B11 B12 B13 B14 B15
                a = _mm256_shuffle_epi8(src, shuftab1);       // A00 A01 A01 A02 A02 A03 A03 A04  A04 A05 A05 A06 A06 A07 A07 A08 | B00 B01 B01 B02 B02 B03 B03 B04  B04 B05 B05 B06 B06 B07 B07 B08
                b = _mm256_shuffle_epi8(src, shuftab2);       // A02 A03 A03 A04 A04 A05 A05 A06  A06 A07 A07 A08 A08 A09 A09 A10 | B02 B03 B03 B04 B04 B05 B05 B06  B06 B07 B07 B08 B08 B09 B09 B10
                c = _mm256_shuffle_epi8(src, shuftab3);       // A04 A05 A05 A06 A06 A07 A07 A08  A08 A09 A09 A10 A10 A11 A11 A12 | B04 B05 B05 B06 B06 B07 B07 B08  B08 B09 B09 B10 B10 B11 B11 B12
                d = _mm256_shuffle_epi8(src, shuftab4);       // A06 A07 A07 A08 A08 A09 A09 A10  A10 A11 A11 A12 A12 A13 A13 A14 | B06 B07 B07 B08 B08 B09 B09 B10  B10 B11 B11 B12 B12 B13 B13 B14

                a = _mm256_maddubs_epi16(a, filt01);
                b = _mm256_maddubs_epi16(b, filt23);
                c = _mm256_maddubs_epi16(c, filt45);
                d = _mm256_maddubs_epi16(d, filt67);

                __m256i res1 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                res1 = _mm256_srai_epi16(res1, 7);            // 16w: A00 A01 A02 A03 A04 A05 A06 A07 | B00 B01 B02 B03 B04 B05 B06 B07

                src = loadu2_m128i(psrc+5, psrc+pitchSrc+5);
                a = _mm256_shuffle_epi8(src, shuftab1);
                b = _mm256_shuffle_epi8(src, shuftab2);
                c = _mm256_shuffle_epi8(src, shuftab3);
                d = _mm256_shuffle_epi8(src, shuftab4);

                a = _mm256_maddubs_epi16(a, filt01);
                b = _mm256_maddubs_epi16(b, filt23);
                c = _mm256_maddubs_epi16(c, filt45);
                d = _mm256_maddubs_epi16(d, filt67);

                __m256i res2 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                res2 = _mm256_srai_epi16(res2, 7);            // 16w: A08 A09 A10 A11 A12 A13 A14 A15 | B08 B09 B10 B11 B12 B13 B14 B15

                res1 = _mm256_packus_epi16(res1, res2);
                if (avg)
                    res1 = _mm256_avg_epu8(res1, loadu2_m128i(pdst, pdst+pitchDst));
                storea_si128(pdst, si128(res1));
                storea_si128(pdst+pitchDst, _mm256_extracti128_si256(res1, 1));
            }
        }

        template <int w, int avg> void interp_horz_avx2_w32(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_horz1);
            __m256i shuftab2 = loada_si256(shuftab_horz2);
            __m256i shuftab3 = loada_si256(shuftab_horz3);
            __m256i shuftab4 = loada_si256(shuftab_horz4);

            for (int32_t y = 0; y < h; y++, psrc += pitchSrc, pdst += pitchDst) {
                for (int32_t x = 0; x < w; x += 32) {
                    __m256i src, a, b, c, d;
                    src = loadu_si256(psrc + x - 3);        // 00 01 02 03 04 05 06 07  08 09 10 11 12 13 14 15  16 17 18 19 20 21 22 23  24 25 26 27 28 29 30 31
                    a = _mm256_shuffle_epi8(src, shuftab1); // 00 01 01 02 02 03 03 04  04 05 05 06 06 07 07 08  16 17 17 18 18 19 19 20  20 21 21 22 22 23 23 24
                    b = _mm256_shuffle_epi8(src, shuftab2); // 02 03 03 04 04 05 05 06  06 07 07 08 08 09 09 10  18 19 19 20 20 21 21 22  22 23 23 24 24 25 25 26
                    c = _mm256_shuffle_epi8(src, shuftab3); // 04 05 05 06 06 07 07 08  08 09 09 10 10 11 11 12  20 21 21 22 22 23 23 24  24 25 25 26 26 27 27 28
                    d = _mm256_shuffle_epi8(src, shuftab4); // 06 07 07 08 08 09 09 10  10 11 11 12 12 13 13 14  22 23 23 24 24 25 25 26  26 27 27 28 28 29 29 30

                    a = _mm256_maddubs_epi16(a, filt01);
                    b = _mm256_maddubs_epi16(b, filt23);
                    c = _mm256_maddubs_epi16(c, filt45);
                    d = _mm256_maddubs_epi16(d, filt67);

                    __m256i res1 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                    res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                    res1 = _mm256_srai_epi16(res1, 7);      // 16w: 00 01 02 03 04 05 06 07  16 17 18 19 20 21 22 23


                    src = loadu_si256(psrc + x + 5);        // 08 09 10 11 12 13 14 15  16 17 18 19 20 21 22 23  24 25 26 27 28 29 30 31  32 33 34 35 36 37 38 39
                    a = _mm256_shuffle_epi8(src, shuftab1); // 08 09 09 10 10 11 11 12  12 13 13 14 14 15 15 16  24 25 25 26 26 27 27 28  28 29 29 30 30 31 31 32
                    b = _mm256_shuffle_epi8(src, shuftab2); // 10 11 11 12 12 13 13 14  14 15 15 16 16 17 17 18  26 27 27 28 28 29 29 30  30 31 31 32 32 33 33 34
                    c = _mm256_shuffle_epi8(src, shuftab3); // 12 13 13 14 14 15 15 16  16 17 17 18 18 19 19 20  28 29 29 30 30 31 31 32  32 33 33 34 34 35 35 36
                    d = _mm256_shuffle_epi8(src, shuftab4); // 14 15 15 16 16 17 17 18  18 19 19 20 20 21 21 22  30 31 31 32 32 33 33 34  34 35 35 36 36 37 37 38

                    a = _mm256_maddubs_epi16(a, filt01);
                    b = _mm256_maddubs_epi16(b, filt23);
                    c = _mm256_maddubs_epi16(c, filt45);
                    d = _mm256_maddubs_epi16(d, filt67);

                    __m256i res2 = _mm256_adds_epi16(_mm256_add_epi16(a,c), _mm256_add_epi16(b,d)); // order of addition matters to avoid overflow
                    res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                    res2 = _mm256_srai_epi16(res2, 7);      // 16w: 08 09 10 11 12 13 14 15  24 25 26 27 28 29 30 31
                    res1 = _mm256_packus_epi16(res1, res2);
                    if (avg)
                        res1 = _mm256_avg_epu8(res1, loada_si256(pdst + x));
                    storea_si256(pdst + x, res1);
                }
            }
        }

        template <int avg> void interp_nv12_horz_avx2_w4(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt); // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_nv12_horz1);
            __m128i shuftab2 = loada_si128(shuftab_nv12_horz2);

            for (int32_t y = 0; y < h; y++, psrc += pitchSrc, pdst += pitchDst) {
                __m128i src1, src2, src3, a, b, c, d;
                src1 = loadu_si128(psrc - 6);           // 16b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7
                src3 = loadu_si128(psrc + 10);          // 16b: u8 v8 u9 v9 uA vA ** ** ** ** ** ** ** ** ** **
                src2 = _mm_alignr_epi8(src3, src1, 8);  // 16b: u4 v4 u5 v5 u6 v6 u7 v7 u8 v8 u9 v9 uA vA ** **

                a = _mm_shuffle_epi8(src1, shuftab1);   // 16b: u0 u1 v0 v1 u1 u2 v1 v2 u2 u3 v2 v3 u3 u4 v3 v4
                b = _mm_shuffle_epi8(src1, shuftab2);   // 16b: u2 u3 v2 v3 u3 u4 v3 v4 u4 u5 v4 v5 u5 u6 v5 v6
                c = _mm_shuffle_epi8(src2, shuftab1);   // 16b: u4 u5 v4 v5 u5 u6 v5 v6 u6 u7 v6 v7 u7 u8 v7 v8
                d = _mm_shuffle_epi8(src2, shuftab2);   // 16b: u6 u7 v6 v7 u7 u8 v7 v8 u8 u9 v8 v9 u9 uA v9 vA

                a = _mm_maddubs_epi16(a, filt01);
                b = _mm_maddubs_epi16(b, filt23);
                c = _mm_maddubs_epi16(c, filt45);
                d = _mm_maddubs_epi16(d, filt67);

                __m128i res = _mm_adds_epi16(_mm_add_epi16(a,c), _mm_add_epi16(b,d)); // order of addition matters to avoid overflow
                res = _mm_adds_epi16(res, _mm_set1_epi16(64));
                res = _mm_srai_epi16(res, 7);      // 8w: u0 v0 u1 v1 u2 v2 u3 v3

                res = _mm_packus_epi16(res, res);
                if (avg)
                    res = _mm_avg_epu8(res, loadl_epi64(pdst));
                storel_epi64(pdst, res);
            }
        }

        template <int avg> void interp_nv12_horz_avx2_w8(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt); // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_nv12_horz1);
            __m128i shuftab2 = loada_si128(shuftab_nv12_horz2);

            for (int32_t y = 0; y < h; y++, psrc += pitchSrc, pdst += pitchDst) {
                __m128i src1, src2, src3, a, b, c, d, e, f, a2, b2, c2, d2;
                src1 = loadu_si128(psrc - 6);           // 16b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7
                src3 = loadu_si128(psrc + 10);          // 16b: u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF
                src2 = _mm_alignr_epi8(src3, src1, 8);  // 16b: u4 v4 u5 v5 u6 v6 u7 v7 u8 v8 u9 v9 uA vA uB vB

                a = _mm_shuffle_epi8(src1, shuftab1);   // 16b: u0 u1 v0 v1 u1 u2 v1 v2 u2 u3 v2 v3 u3 u4 v3 v4
                b = _mm_shuffle_epi8(src1, shuftab2);   // 16b: u2 u3 v2 v3 u3 u4 v3 v4 u4 u5 v4 v5 u5 u6 v5 v6
                c = _mm_shuffle_epi8(src2, shuftab1);   // 16b: u4 u5 v4 v5 u5 u6 v5 v6 u6 u7 v6 v7 u7 u8 v7 v8
                d = _mm_shuffle_epi8(src2, shuftab2);   // 16b: u6 u7 v6 v7 u7 u8 v7 v8 u8 u9 v8 v9 u9 uA v9 vA
                e = _mm_shuffle_epi8(src3, shuftab1);   // 16b: u8 u9 v8 v9 u9 uA v9 vA uA uB vA vB uB uC vB vC
                f = _mm_shuffle_epi8(src3, shuftab2);   // 16b: uA uB vA vB uB uC vB vC uC uD vC vD uD uE vD vE

                a2 = _mm_maddubs_epi16(c, filt01);
                b2 = _mm_maddubs_epi16(d, filt23);
                c2 = _mm_maddubs_epi16(e, filt45);
                d2 = _mm_maddubs_epi16(f, filt67);

                a = _mm_maddubs_epi16(a, filt01);
                b = _mm_maddubs_epi16(b, filt23);
                c = _mm_maddubs_epi16(c, filt45);
                d = _mm_maddubs_epi16(d, filt67);

                __m128i res1 = _mm_adds_epi16(_mm_add_epi16(a,c), _mm_add_epi16(b,d)); // order of addition matters to avoid overflow
                res1 = _mm_adds_epi16(res1, _mm_set1_epi16(64));
                res1 = _mm_srai_epi16(res1, 7);      // 8w: u0 v0 u1 v1 u2 v2 u3 v3

                __m128i res2 = _mm_adds_epi16(_mm_add_epi16(a2,c2), _mm_add_epi16(b2,d2)); // order of addition matters to avoid overflow
                res2 = _mm_adds_epi16(res2, _mm_set1_epi16(64));
                res2 = _mm_srai_epi16(res2, 7);      // 8w: u4 v4 u5 v5 u6 v6 u7 v7

                res1 = _mm_packus_epi16(res1, res2);
                if (avg)
                    res1 = _mm_avg_epu8(res1, loada_si128(pdst));
                storea_si128(pdst, res1);
            }
        }

        template <int w, int avg> void interp_nv12_horz_avx2_w16(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h)
        {
            __m128i filt = loada_si128(pfilt);  // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt,6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_nv12_horz1);
            __m256i shuftab2 = loada_si256(shuftab_nv12_horz2);

            for (int32_t y = 0; y < h; y++, psrc += pitchSrc, pdst += pitchDst) {
                for (int32_t x = 0; x < 2*w; x += 32) {
                    __m256i src, a, b, c, d, e, f, a1, b1, c1, d1;
                    src = loadu_si256(psrc + x - 6);        // 32b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7 | u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF
                    a = _mm256_shuffle_epi8(src, shuftab1); // 32b: u0 u1 v0 v1 u1 u2 v1 v2 u2 u3 v2 v3 u3 u4 v3 v4 | u8 u9 v8 v9 u9 uA v9 vA uA uB vA vB uB uC vB vC
                    b = _mm256_shuffle_epi8(src, shuftab2); // 32b: u2 u3 v2 v3 u3 u4 v3 v4 u4 u5 v4 v5 u5 u6 v5 v6 | uA uB vA vB uB uC vB vC uC uD vC vD uD uE vD vE
                    src = loadu_si256(psrc + x + 2);        // 32b: u4 v4 u5 v5 u6 v6 u7 v7 u8 v8 u9 v9 uA vA uB vB | uC vC uD vD uE vE uF vF uG vG uH vH uI vI uK vK
                    c = _mm256_shuffle_epi8(src, shuftab1); // 32b: u4 u5 v4 v5 u5 u6 v5 v6 u6 u7 v6 v7 u7 u8 v7 v8 | uC uD vC vD uD uE vD vE uE uF vE vF uF uG vF vG
                    d = _mm256_shuffle_epi8(src, shuftab2); // 32b: u6 u7 v6 v7 u7 u8 v7 v8 u8 u9 v8 v9 u9 uA v9 vA | uE uF vE vF uF uG vF vG uG uH vG vH uH uI vH vI
                    src = loadu_si256(psrc + x + 10);       // 32b: u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF | uG vG uH vH uI vI uK vK uL vL uM vM uN vN ** **
                    e = _mm256_shuffle_epi8(src, shuftab1);
                    f = _mm256_shuffle_epi8(src, shuftab2);

                    a1 = _mm256_maddubs_epi16(a, filt01);
                    b1 = _mm256_maddubs_epi16(b, filt23);
                    c1 = _mm256_maddubs_epi16(c, filt45);
                    d1 = _mm256_maddubs_epi16(d, filt67);

                    __m256i res1 = _mm256_adds_epi16(_mm256_add_epi16(a1,c1), _mm256_add_epi16(b1,d1)); // order of addition matters to avoid overflow
                    res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64));
                    res1 = _mm256_srai_epi16(res1, 7);      // 16w: u0 v0 u1 v1 u2 v2 u3 v3 | u8 v8 u9 v9 uA vA uB vB

                    c = _mm256_maddubs_epi16(c, filt01);
                    d = _mm256_maddubs_epi16(d, filt23);
                    e = _mm256_maddubs_epi16(e, filt45);
                    f = _mm256_maddubs_epi16(f, filt67);

                    __m256i res2 = _mm256_adds_epi16(_mm256_add_epi16(c,e), _mm256_add_epi16(d,f)); // order of addition matters to avoid overflow
                    res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64));
                    res2 = _mm256_srai_epi16(res2, 7);      // 16w: u4 v4 u5 v5 u6 v6 u7 v7 | uC vC uD vD uE vE uF vF
                    res1 = _mm256_packus_epi16(res1, res2);
                    if (avg)
                        res1 = _mm256_avg_epu8(res1, loada_si256(pdst + x));
                    storea_si256(pdst + x, res1);
                }
            }
        }

        template <int avg> void interp_nv12_vert_avx2_w4(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h) {
            return interp_vert_avx2_w8<avg>(psrc, pitchSrc, pdst, pitchDst, pfilt, h);
        }

        template <int avg> void interp_nv12_vert_avx2_w8(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h) {
            return interp_vert_avx2_w16<avg>(psrc, pitchSrc, pdst, pitchDst, pfilt, h);
        }

        template <int w, int avg> void interp_nv12_vert_avx2_w16(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, const int16_t *pfilt, int h) {
            interp_vert_avx2_w32<w*2, avg>(psrc, pitchSrc, pdst, pitchDst, pfilt, h);
        }

        template <int avg> void interp_copy_avx2_w4(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, int h)
        {
            int ps1 = pitchSrc, ps2 = pitchSrc*2, ps3 = pitchSrc*3, ps4 = pitchSrc*4;
            int pd1 = pitchDst, pd2 = pitchDst*2, pd3 = pitchDst*3, pd4 = pitchDst*4;
            for (int32_t y = 0; y < h; y+=4, psrc += ps4, pdst += pd4) {
                __m128i r = _mm_setr_epi32(*(int*)(psrc), *(int*)(psrc+ps1), *(int*)(psrc+ps2), *(int*)(psrc+ps3));
                if (avg) {
                    __m128i r2 = _mm_setr_epi32(*(int*)(pdst), *(int*)(pdst+pd1), *(int*)(pdst+pd2), *(int*)(pdst+pd3));
                    r = _mm_avg_epu8(r, r2);
                }
                storel_si32(pdst, r);
                storel_si32(pdst+pd1, _mm_bsrli_si128(r,4));
                storel_si32(pdst+pd2, _mm_bsrli_si128(r,8));
                storel_si32(pdst+pd3, _mm_bsrli_si128(r,12));
            }
        }

        template <int avg> void interp_copy_avx2_w8(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, int h)
        {
            int ps1 = pitchSrc, ps2 = pitchSrc*2, ps3 = pitchSrc*3, ps4 = pitchSrc*4;
            int pd1 = pitchDst, pd2 = pitchDst*2, pd3 = pitchDst*3, pd4 = pitchDst*4;
            for (int32_t y = 0; y < h; y+=4, psrc += ps4, pdst += pd4) {
                __m128i r0 = loadu2_epi64(psrc, psrc+ps1);
                __m128i r1 = loadu2_epi64(psrc+ps2, psrc+ps3);
                if (avg) {
                    r0 = _mm_avg_epu8(r0, loadu2_epi64(pdst, pdst+pd1));
                    r1 = _mm_avg_epu8(r1, loadu2_epi64(pdst+pd2, pdst+pd3));
                }
                storel_epi64(pdst, r0);
                storeh_epi64(pdst+pd1, r0);
                storel_epi64(pdst+pd2, r1);
                storeh_epi64(pdst+pd3, r1);
            }
        }

        template <int avg> void interp_copy_avx2_w16(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, int h)
        {
            int ps1 = pitchSrc, ps2 = pitchSrc*2, ps3 = pitchSrc*3, ps4 = pitchSrc*4;
            int pd1 = pitchDst, pd2 = pitchDst*2, pd3 = pitchDst*3, pd4 = pitchDst*4;
            for (int32_t y = 0; y < h; y+=4, psrc += ps4, pdst += pd4) {
                __m128i r0 = loadu_si128(psrc);
                __m128i r1 = loadu_si128(psrc+ps1);
                __m128i r2 = loadu_si128(psrc+ps2);
                __m128i r3 = loadu_si128(psrc+ps3);
                if (avg) {
                    r0 = _mm_avg_epu8(r0, loada_si128(pdst));
                    r1 = _mm_avg_epu8(r1, loada_si128(pdst+pd1));
                    r2 = _mm_avg_epu8(r2, loada_si128(pdst+pd2));
                    r3 = _mm_avg_epu8(r3, loada_si128(pdst+pd3));
                }
                storea_si128(pdst, r0);
                storea_si128(pdst+pd1, r1);
                storea_si128(pdst+pd2, r2);
                storea_si128(pdst+pd3, r3);
            }
        }

        template <int w, int avg> void interp_copy_avx2_w32(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, int h)
        {
            for (int32_t y = 0; y < h; y++, psrc += pitchSrc, pdst += pitchDst) {
                for (int32_t x = 0; x < w; x += 32) {
                    __m256i r = loadu_si256(psrc+x);
                    if (avg) r = _mm256_avg_epu8(r, loada_si256(pdst+x));
                    storea_si256(pdst+x, r);
                }
            }
        }

        template <int avg> void interp_nv12_copy_avx2_w4(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, int h) {
            interp_copy_avx2_w8<avg>(psrc, pitchSrc, pdst, pitchDst, h);
        }

        template <int avg> void interp_nv12_copy_avx2_w8(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, int h) {
            interp_copy_avx2_w16<avg>(psrc, pitchSrc, pdst, pitchDst, h);
        }

        template <int w, int avg> void interp_nv12_copy_avx2_w16(const uint8_t *psrc, int pitchSrc, uint8_t *pdst, int pitchDst, int h) {
            interp_copy_avx2_w32<2*w, avg>(psrc, pitchSrc, pdst, pitchDst, h);
        }
    }  // namespace details

    template <int w> void average_avx2(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h) {
        for (int y = 0; y < h; y++, src1 += pitchSrc1, src2 += pitchSrc2, dst += pitchDst) {
            for (int x = 0; x < w; x += 32) {
                storea_si256(dst+x, _mm256_avg_epu8(loadu_si256(src1+x), loadu_si256(src2+x)));
            }
        }
    }
    template <> void average_avx2<4>(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h) {
        int ps11 = pitchSrc1, ps12 = pitchSrc1*2, ps13 = pitchSrc1*3, ps14 = pitchSrc1*4;
        int ps21 = pitchSrc2, ps22 = pitchSrc2*2, ps23 = pitchSrc2*3, ps24 = pitchSrc2*4;
        int pd1 = pitchDst, pd2 = pitchDst*2, pd3 = pitchDst*3, pd4 = pitchDst*4;
        for (int y = 0; y < h; y += 4, src1 += ps14, src2 += ps24, dst += pd4) {
            __m128i s1 = _mm_setr_epi32(*(int*)(src1), *(int*)(src1+ps11), *(int*)(src1+ps12), *(int*)(src1+ps13));
            __m128i s2 = _mm_setr_epi32(*(int*)(src2), *(int*)(src2+ps21), *(int*)(src2+ps22), *(int*)(src2+ps23));
            __m128i avg = _mm_avg_epu8(s1, s2);
            storel_si32(dst, avg);
            storel_si32(dst+pd1, _mm_bsrli_si128(avg,4));
            storel_si32(dst+pd2, _mm_bsrli_si128(avg,8));
            storel_si32(dst+pd3, _mm_bsrli_si128(avg,12));
        }
    }
    template <> void average_avx2<8>(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h) {
        for (int y = 0; y < h; y += 2, src1 += 2*pitchSrc1, src2 += 2*pitchSrc2, dst += 2*pitchDst) {
            __m128i s1 = loadu2_epi64(src1, src1 + pitchSrc1);
            __m128i s2 = loadu2_epi64(src2, src2 + pitchSrc2);
            __m128i avg = _mm_avg_epu8(s1, s2);
            storel_epi64(dst, avg);
            storeh_epi64(dst + pitchDst, avg);
        }
    }
    template <> void average_avx2<16>(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h) {
        for (int y = 0; y < h; y++, src1 += pitchSrc1, src2 += pitchSrc2, dst += pitchDst) {
            storea_si128(dst, _mm_avg_epu8(loadu_si128(src1), loadu_si128(src2)));
        }
    }
    template void average_avx2<32>(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h);
    template void average_avx2<64>(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h);

    template <int w> void average_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h) {
        return average_avx2<w>(src1, 64, src2, 64, dst, 64, h);
    }
    template void average_pitch64_avx2<4>(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);
    template void average_pitch64_avx2<8>(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);
    template void average_pitch64_avx2<16>(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);
    template void average_pitch64_avx2<32>(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);
    template void average_pitch64_avx2<64>(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);

    template <int w, int horz, int vert, int avg> void interp_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    template <> void interp_avx2<4,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w4<0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<4,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w4<1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<8,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w8<0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<8,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w8<1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<16,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w16<0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<16,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w16<1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<32,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w32<32,0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<32,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w32<32,1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<64,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w32<64,0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<64,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w32<64,1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<96,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w32<96,0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_avx2<96,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_copy_avx2_w32<96,1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }

    template <> void interp_avx2<4,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w4<0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<4,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w4<1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<8,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w8<0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<8,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w8<1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<16,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w16<0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<16,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w16<1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<32,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w32<32,0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<32,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w32<32,1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<64,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w32<64,0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<64,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w32<64,1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<96,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w32<96,0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_avx2<96,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_vert_avx2_w32<96,1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }

    template <> void interp_avx2<4,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w4<0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<4,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w4<1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<8,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w8<0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<8,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w8<1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<16,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w16<0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<16,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w16<1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<32,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w32<32,0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<32,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w32<32,1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<64,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w32<64,0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<64,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w32<64,1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<96,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w32<96,0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_avx2<96,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_horz_avx2_w32<96,1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }

    template <> void interp_avx2<4,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w4<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w4<0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<4,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w4<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w4<1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<8,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        assert(h <= 64);
        ALIGN_DECL(64) uint8_t tmp[(64+8)*8];
        details::interp_horz_pw_avx2_w8(src - 3 * pitchSrc, pitchSrc, tmp, fx, h + 7);
        details::interp_vert_pw_avx2_w8<0>(tmp + 3 * 8, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<8,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        assert(h <= 64);
        ALIGN_DECL(64) uint8_t tmp[(64+8)*8];
        details::interp_horz_pw_avx2_w8(src - 3 * pitchSrc, pitchSrc, tmp, fx, h + 7);
        details::interp_vert_pw_avx2_w8<1>(tmp + 3 * 8, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<16,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w16<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w16<0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<16,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w16<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w16<1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<32,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w32<32,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w32<32,0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<32,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w32<32,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w32<32,1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<64,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w32<64,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w32<64,0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<64,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_horz_avx2_w32<64,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_vert_avx2_w32<64,1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<96,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(96+8)*96];
        details::interp_horz_avx2_w32<96,0>(src - 3 * pitchSrc, pitchSrc, tmp, 96, fx, h + 7);
        details::interp_vert_avx2_w32<96,0>(tmp + 3 * 96, 96, dst, pitchDst, fy, h);
    }
    template <> void interp_avx2<96,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(96+8)*96];
        details::interp_horz_avx2_w32<96,0>(src - 3 * pitchSrc, pitchSrc, tmp, 96, fx, h + 7);
        details::interp_vert_avx2_w32<96,1>(tmp + 3 * 96, 96, dst, pitchDst, fy, h);
    }

    template <int w, int horz, int vert, int avg> void interp_pitch64_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h) {
        return interp_avx2<w, horz, vert, avg>(src, pitchSrc, dst, 64, fx, fy, h);
    }
    template void interp_pitch64_avx2<4,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<4,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template void interp_pitch64_avx2<4,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<4,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template void interp_pitch64_avx2<4,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<4,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template void interp_pitch64_avx2<4,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<4,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<8,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<16,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<32,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_pitch64_avx2<64,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template <int w, int horz, int vert, int avg> void interp_nv12_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    template <> void interp_nv12_avx2<4,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w4<0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_nv12_avx2<4,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w4<1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_nv12_avx2<8,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w8<0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_nv12_avx2<8,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w8<1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_nv12_avx2<16,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w16<16,0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_nv12_avx2<16,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w16<16,1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_nv12_avx2<32,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w16<32,0>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }
    template <> void interp_nv12_avx2<32,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_copy_avx2_w16<32,1>(src, pitchSrc, dst, pitchDst, h); fx; fy;
    }

    template <> void interp_nv12_avx2<4,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w4<0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_nv12_avx2<4,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w4<1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_nv12_avx2<8,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w8<0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_nv12_avx2<8,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w8<1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_nv12_avx2<16,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w16<16,0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_nv12_avx2<16,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w16<16,1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_nv12_avx2<32,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w16<32,0>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }
    template <> void interp_nv12_avx2<32,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_vert_avx2_w16<32,1>(src, pitchSrc, dst, pitchDst, fy, h); fx;
    }

    template <> void interp_nv12_avx2<4,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w4<0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_nv12_avx2<4,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w4<1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_nv12_avx2<8,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w8<0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_nv12_avx2<8,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w8<1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_nv12_avx2<16,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w16<16,0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_nv12_avx2<16,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w16<16,1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_nv12_avx2<32,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w16<32,0>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }
    template <> void interp_nv12_avx2<32,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        details::interp_nv12_horz_avx2_w16<32,1>(src, pitchSrc, dst, pitchDst, fx, h); fy;
    }

    template <> void interp_nv12_avx2<4,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w4<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w4<0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_nv12_avx2<4,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w4<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w4<1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_nv12_avx2<8,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w8<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w8<0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_nv12_avx2<8,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w8<0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w8<1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_nv12_avx2<16,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w16<16,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w16<16,0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_nv12_avx2<16,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w16<16,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w16<16,1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_nv12_avx2<32,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w16<32,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w16<32,0>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }
    template <> void interp_nv12_avx2<32,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        ALIGN_DECL(64) uint8_t tmp[(64+8)*64];
        details::interp_nv12_horz_avx2_w16<32,0>(src - 3 * pitchSrc, pitchSrc, tmp, 64, fx, h + 7);
        details::interp_nv12_vert_avx2_w16<32,1>(tmp + 3 * 64, 64, dst, pitchDst, fy, h);
    }

    template <int w, int horz, int vert, int avg> void interp_nv12_pitch64_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h) {
        return interp_nv12_avx2<w, horz, vert, avg>(src, pitchSrc, dst, 64, fx, fy, h);
    }
    template void interp_nv12_pitch64_avx2<4,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<4,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,0,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,0,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template void interp_nv12_pitch64_avx2<4,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<4,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,0,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,0,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template void interp_nv12_pitch64_avx2<4,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<4,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,1,0,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,1,0,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

    template void interp_nv12_pitch64_avx2<4,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<4,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<8,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<16,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,1,1,0>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_nv12_pitch64_avx2<32,1,1,1>(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
}  // namespace VP9PP
