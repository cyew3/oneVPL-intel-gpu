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
#include "utility"
#include "type_traits"

#include <algorithm>

namespace AV1PP
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

    typedef std::pair<__m256i, __m256i> pair_m256i;

    const int FILTER_BITS = 7;
    const int SUBPEL_TAPS = 8;
    const int ROUND_STAGE0 = 3;
    const int ROUND_STAGE1 = 11;
    const int ROUND_PRE_COMPOUND = 7;
    const int ROUND_POST_COMPOUND = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND + 1;
    const int OFFSET_STAGE0 = 1 << ROUND_STAGE0 >> 1;
    const int OFFSET_STAGE1 = 1 << ROUND_STAGE1 >> 1;
    const int OFFSET_PRE_COMPOUND = 1 << ROUND_PRE_COMPOUND >> 1;
    const int OFFSET_POST_COMPOUND = 1 << ROUND_POST_COMPOUND >> 1;
    const int MAX_BLOCK_SIZE = 96;
    const int IMPLIED_PITCH = 64;
    const int ADDITIONAL_OFFSET_STAGE0 = 64 * 255;
    const int ADDITIONAL_OFFSET_STAGE1 = -(ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0);

    namespace {

        enum { no_horz = 0, do_horz = 1 };
        enum { no_vert = 0, do_vert = 1 };
        enum { no_avg = 0, do_avg = 1 };

        template <typename T>
        void nv12_to_yv12(const T *src, int pitchSrc, T *dstU, int pitchDstU, T *dstV, int pitchDstV, int w, int h)
        {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    dstU[y * pitchDstU + x] = src[y * pitchSrc + 2 * x + 0];
                    dstV[y * pitchDstV + x] = src[y * pitchSrc + 2 * x + 1];
                }
            }
        }

        template <typename T>
        void yv12_to_nv12(const T *srcU, int pitchSrcU, const T *srcV, int pitchSrcV, T *dst, int pitchDst, int w, int h)
        {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    dst[y * pitchDst + 2 * x + 0] = srcU[y * pitchSrcU + x];
                    dst[y * pitchDst + 2 * x + 1] = srcV[y * pitchSrcV + x];
                }
            }
        }

        AV1_FORCEINLINE __m256i average_8u_16s(__m128i src0, __m128i src1, __m256i r0, __m256i r1, __m256i offset, int shift) {
            __m256i d, d0, d1, s0, s1;
            s0 = _mm256_slli_epi16(_mm256_cvtepu8_epi16(src0), shift);
            s1 = _mm256_slli_epi16(_mm256_cvtepu8_epi16(src1), shift);
            d0 = _mm256_srai_epi16(_mm256_add_epi16(_mm256_add_epi16(s0, r0), offset), shift + 1); // 16w: 01234567 89abcdef
            d1 = _mm256_srai_epi16(_mm256_add_epi16(_mm256_add_epi16(s1, r1), offset), shift + 1); // 16w: ghijklmn opqrstuv
            d = _mm256_packus_epi16(d0, d1);                    // 32b: 01234567 ghijklmn 89abcdef opqrstuv
            d = _mm256_permute4x64_epi64(d, PERM4x64(0, 2, 1, 3)); // 32b: 01234567 89abcdef ghijklmn opqrstuv
            return d;
        }

        template <int w16>
        AV1_FORCEINLINE void interp_w16_nohorz_novert(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, int h)
        {
            static_assert((w16 & 15) == 0, "expected width multiple of 16");
            static_assert(w16 <= 64, "expected width <= 64");
            const int shift = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND;
            const int y_step = 4 / (w16 / 16);
            for (int y = 0; y < h; y += y_step) {
                for (int yy = 0; yy < y_step; yy++) {
                    for (int x = 0; x < w16; x += 16) {
                        __m256i s = _mm256_cvtepu8_epi16(loadu_si128(src + yy * pitchSrc + x));
                        storea_si256(dst + yy * pitchDst + x, _mm256_slli_epi16(s, shift));
                    }
                }
                src += y_step * pitchSrc;
                dst += y_step * pitchDst;
            }
        }

        AV1_FORCEINLINE void interp_2nd_ref_w16_nohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, int h)
        {
            const int shift = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND;
            const __m256i offset = _mm256_set1_epi16(1 << shift);
            for (int y = 0; y < h; y += 2) {
                __m256i d = average_8u_16s(loadu_si128(src), loadu_si128(src + pitchSrc),
                    loada_si256(ref0), loada_si256(ref0 + pitchRef0),
                    offset, shift);
                storea2_m128i(dst, dst + pitchDst, d);

                src += 2 * pitchSrc;
                ref0 += 2 * pitchRef0;
                dst += 2 * pitchDst;
            }
        }
        template <int w32>
        AV1_FORCEINLINE void interp_2nd_ref_w32_nohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, int h)
        {
            static_assert((w32 & 31) == 0, "expected width multiple of 32");
            const int shift = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND;
            const __m256i offset = _mm256_set1_epi16(1 << shift);
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w32; x += 32) {
                    __m256i d = average_8u_16s(loadu_si128(src + x), loadu_si128(src + x + 16),
                        loada_si256(ref0 + x), loada_si256(ref0 + x + 16),
                        offset, shift);
                    storea_si256(dst + x, d);
                }
                src += pitchSrc;
                ref0 += pitchRef0;
                dst += pitchDst;
            }
        }

        // Calculates:
        //   (sum(u8(i) * k(i)) + offset) >> FILTER_BITS
        // offset may be different from (1 << FILTER_BITS >> 1)
        AV1_FORCEINLINE __m256i filter_u8_u8(__m256i ab, __m256i cd, __m256i ef, __m256i gh, __m256i filt01, __m256i filt23, __m256i filt45, __m256i filt67, __m256i offset) {
            __m256i tmp1, tmp2, res;
            ab = _mm256_maddubs_epi16(ab, filt01);
            cd = _mm256_maddubs_epi16(cd, filt23);
            ef = _mm256_maddubs_epi16(ef, filt45);
            gh = _mm256_maddubs_epi16(gh, filt67);
            tmp1 = _mm256_add_epi16(ab, ef); // order of addition matters to avoid overflow
            tmp2 = _mm256_add_epi16(cd, gh);
            res = _mm256_adds_epi16(tmp1, tmp2);
            res = _mm256_adds_epi16(res, offset);
            return _mm256_srai_epi16(res, FILTER_BITS);
        }

        // Calculates:
        //   (sum(u8(i) * k(i)) + offset0 + additional_offset0) >> round0
        // additional_offset0 make output fit unsigned 16bit
        // should be compensated later
        AV1_FORCEINLINE __m256i filter_u8_u16(__m256i ab, __m256i cd, __m256i ef, __m256i gh, __m256i k01, __m256i k23, __m256i k45, __m256i k67)
        {
            ab = _mm256_maddubs_epi16(ab, k01);
            cd = _mm256_maddubs_epi16(cd, k23);
            ef = _mm256_maddubs_epi16(ef, k45);
            gh = _mm256_maddubs_epi16(gh, k67);
            __m256i res;
            res = _mm256_add_epi16(ab, cd);
            res = _mm256_add_epi16(res, ef);
            res = _mm256_add_epi16(res, gh);
            res = _mm256_add_epi16(res, _mm256_set1_epi16(OFFSET_STAGE0 + ADDITIONAL_OFFSET_STAGE0));
            res = _mm256_srli_epi16(res, ROUND_STAGE0);
            return res;
        }

        // Calculates:
        //   sum(s16(i) * k(i))
        AV1_FORCEINLINE __m256i filter_s16_s32(__m256i ab, __m256i cd, __m256i ef, __m256i gh, __m256i k01, __m256i k23, __m256i k45, __m256i k67)
        {
            ab = _mm256_madd_epi16(ab, k01);
            cd = _mm256_madd_epi16(cd, k23);
            ef = _mm256_madd_epi16(ef, k45);
            gh = _mm256_madd_epi16(gh, k67);

            return _mm256_add_epi32(
                _mm256_add_epi32(ab, cd),
                _mm256_add_epi32(ef, gh));
        }

        template <int avg, typename TDst>
        void interp_w4_nohorz_dovert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fy, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            __m128i filt = loada_si128(fy);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt1 = _mm_broadcastd_epi32(filt);
            __m128i filt2 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt, 4));

            __m128i lineA = loadl_si32(src - pitchSrc * 3);
            __m128i lineB = loadl_si32(src - pitchSrc * 2);
            __m128i lineC = loadl_si32(src - pitchSrc * 1);
            __m128i lineD = loadl_si32(src + pitchSrc * 0);
            __m128i lineE = loadl_si32(src + pitchSrc * 1);
            __m128i lineF = loadl_si32(src + pitchSrc * 2);
            __m128i lineG = loadl_si32(src + pitchSrc * 3);
            __m128i lineH;
            __m128i linesABCD, linesEFGH;
            for (int32_t y = 0; y < h; y++) {
                lineH = loadl_si32(src + pitchSrc * 4);
                linesABCD = _mm_unpacklo_epi16(_mm_unpacklo_epi8(lineA, lineB), _mm_unpacklo_epi8(lineC, lineD));   // 16b: A0 B0 C0 D0 A1 B1 C1 D1 A2 B2 C2 D2 A3 B3 C3 D3
                linesEFGH = _mm_unpacklo_epi16(_mm_unpacklo_epi8(lineE, lineF), _mm_unpacklo_epi8(lineG, lineH));   // 16b: E0 F0 G0 H0 E1 F1 G1 H1 E2 F2 G2 H2 E3 F3 G3 H3
                linesABCD = _mm_maddubs_epi16(linesABCD, filt1);
                linesEFGH = _mm_maddubs_epi16(linesEFGH, filt2);
                __m128i res1 = _mm_add_epi16(linesABCD, linesEFGH);

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res1 = _mm_hadds_epi16(res1, res1);
                        const __m128i offset = _mm_set1_epi16(1 << (FILTER_BITS - 1));
                        res1 = _mm_adds_epi16(res1, offset);
                        res1 = _mm_srai_epi16(res1, FILTER_BITS);
                    }
                    else {
                        res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                        res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                        res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                        res1 = _mm_packs_epi32(res1, res1);
                        res1 = _mm_add_epi16(res1, loadl_epi64(ref0));
                        res1 = _mm_srai_epi16(res1, 1);
                        const __m128i offset = _mm_set1_epi16(1 << (2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND - 1));
                        res1 = _mm_add_epi16(res1, offset);
                        res1 = _mm_srai_epi16(res1, 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND);
                    }
                    res1 = _mm_packus_epi16(res1, res1);
                    storel_si32(dst, res1);
                }
                else {
                    res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                    res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                    res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                    res1 = _mm_packs_epi32(res1, res1);
                    storel_epi64(dst, res1);
                }

                lineA = lineB;
                lineB = lineC;
                lineC = lineD;
                lineD = lineE;
                lineE = lineF;
                lineF = lineG;
                lineG = lineH;
                src += pitchSrc;
                dst += pitchDst;
                if (avg == do_avg)
                    ref0 += pitchRef0;
            }
        }
        template <int avg, typename TDst>
        void interp_w8_nohorz_dovert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fy, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");
            // lineA: A0 A1 ... A6 A7 | B0 B1 ... B6 B7
            // lineB: B0 B1 ... B6 B7 | C0 C1 ... C6 C7
            // lineC: C0 C1 ... C6 C7 | D0 D1 ... D6 D7
            // lineD: D0 D1 ... D6 D7 | E0 E1 ... E6 E7
            // lineE: E0 E1 ... E6 E7 | F0 F1 ... F6 F7
            // lineF: F0 F1 ... F6 F7 | G0 G1 ... G6 G7
            // lineG: G0 G1 ... G6 G7 | H0 H1 ... H6 H7
            // lineH: H0 H1 ... H6 H7 | I0 I1 ... I6 I7
            __m128i filt = loada_si128(fy);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt);
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));

            __m128i lineA_ = loadl_epi64(src - pitchSrc * 3);
            __m128i lineB_ = loadl_epi64(src - pitchSrc * 2);
            __m128i lineC_ = loadl_epi64(src - pitchSrc * 1);
            __m128i lineD_ = loadl_epi64(src + pitchSrc * 0);
            __m128i lineE_ = loadl_epi64(src + pitchSrc * 1);
            __m128i lineF_ = loadl_epi64(src + pitchSrc * 2);
            __m128i lineG_ = loadl_epi64(src + pitchSrc * 3);
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

            __m256i res1, res2;

            for (int32_t y = 0; y < h; y += 4) {
                lineH_ = loadl_epi64(src + pitchSrc * 4);
                lineI_ = loadl_epi64(src + pitchSrc * 5);
                lineG = _mm256_inserti128_si256(lineG, lineH_, 1);
                lineH = _mm256_inserti128_si256(si256(lineH_), lineI_, 1);
                linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res1 = filter_u8_u8(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67, _mm256_set1_epi16(64));
                    }
                    else {
                        res1 = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                        res1 = _mm256_add_epi16(res1, loada2_m128i(ref0, ref0 + pitchRef0));
                        res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res1 = _mm256_srai_epi16(res1, ROUND_POST_COMPOUND);  // 16w: row0 | row1
                    }
                }
                else {
                    res1 = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                    res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storeu2_m128i(dst, dst + pitchDst, res1);
                }

                linesAB = linesCD;
                linesCD = linesEF;
                linesEF = linesGH;
                lineG = si256(lineI_);
                lineH_ = loadl_epi64(src + pitchSrc * 6);
                lineI_ = loadl_epi64(src + pitchSrc * 7);
                lineG = _mm256_inserti128_si256(lineG, lineH_, 1);
                lineH = _mm256_inserti128_si256(si256(lineH_), lineI_, 1);
                linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                if (std::is_same<TDst, uint8_t>::value) {
                    __m256i res;
                    if (avg == no_avg) {
                        res2 = filter_u8_u8(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67, _mm256_set1_epi16(64));
                        res = _mm256_packus_epi16(res1, res2);
                    }
                    else {
                        res2 = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                        res2 = _mm256_add_epi16(res2, loada2_m128i(ref0 + pitchRef0 * 2, ref0 + pitchRef0 * 3));
                        res2 = _mm256_add_epi16(res2, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res2 = _mm256_srai_epi16(res2, ROUND_POST_COMPOUND);  // 16w: row2 | row3
                        res = _mm256_packus_epi16(res1, res2);                // 32b: row0 row2 | row1 row3
                    }
                    storel_epi64(dst + pitchDst * 0, si128(res));
                    storel_epi64(dst + pitchDst * 1, _mm256_extracti128_si256(res, 1));
                    storeh_epi64(dst + pitchDst * 2, si128(res));
                    storeh_epi64(dst + pitchDst * 3, _mm256_extracti128_si256(res, 1));
                }
                else {
                    res1 = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                    res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storeu2_m128i(dst + pitchDst * 2, dst + pitchDst * 3, res1);
                }

                linesAB = linesCD;
                linesCD = linesEF;
                linesEF = linesGH;
                lineG = si256(lineI_);
                dst += pitchDst * 4;
                src += pitchSrc * 4;
                if (avg == do_avg)
                    ref0 += pitchRef0 * 4;
            }
        }
        template <int avg, typename TDst>
        void interp_w16_nohorz_dovert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fy, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");
            // lineA: A0 A1 ... A14 A15
            // lineB: B0 B1 ... B14 B15
            // lineC: C0 C1 ... C14 C15
            // lineD: D0 D1 ... D14 D15
            // lineE: E0 E1 ... E14 E15
            // lineF: F0 F1 ... F14 F15
            // lineG: G0 G1 ... G14 G15
            // lineH: H0 H1 ... H14 H15
            // lineI: I0 I1 ... I14 I15
            __m128i filt = loada_si128(fy);     // 8w:  0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt);
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));

            __m256i lineAB = loadu2_m128i(src - pitchSrc * 3, src - pitchSrc * 2);          // 32b: A0 ... A15 | B0 ... B15
            __m256i lineBC = loadu2_m128i(src - pitchSrc * 2, src - pitchSrc * 1);          // 32b: B0 ... B15 | C0 ... C15
            __m256i lineDE = loadu2_m128i(src - pitchSrc * 0, src + pitchSrc * 1);          // 32b: D0 ... D15 | E0 ... E15
            __m256i lineFG = loadu2_m128i(src + pitchSrc * 2, src + pitchSrc * 3);          // 32b: F0 ... F15 | G0 ... G15

            __m256i lineCD = _mm256_permute2x128_si256(lineBC, lineDE, PERM2x128(1, 2)); // 32b: C0 ... C15 | D0 ... D15
            __m256i lineEF = _mm256_permute2x128_si256(lineDE, lineFG, PERM2x128(1, 2)); // 32b: E0 ... E15 | F0 ... F15

            __m256i lineHI, lineGH;
            __m256i res1, res2, res;
            const __m256i offset = _mm256_set1_epi16(64);

            for (int32_t y = 0; y < h; y += 2) {
                lineHI = loadu2_m128i(src + pitchSrc * 4, src + pitchSrc * 5);              // 32b: H0 ... H15 | I0 ... I15
                lineGH = _mm256_permute2x128_si256(lineFG, lineHI, PERM2x128(1, 2));     // 32b: G0 ... G15 | H0 ... H15

                __m256i linesABBC = _mm256_unpacklo_epi8(lineAB, lineBC); // 32b: A0 B0 A1 B1 ... | B0 C0 B1 C1 ...
                __m256i linesCDDE = _mm256_unpacklo_epi8(lineCD, lineDE); // 32b: C0 D0 C1 D1 ... | D0 E0 D1 E1 ...
                __m256i linesEFFG = _mm256_unpacklo_epi8(lineEF, lineFG); // 32b: E0 F0 E1 F1 ... | F0 G0 F1 G1 ...
                __m256i linesGHHI = _mm256_unpacklo_epi8(lineGH, lineHI); // 32b: G0 H0 G1 H1 ... | H0 I0 H1 I1 ...

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res1 = filter_u8_u8(linesABBC, linesCDDE, linesEFFG, linesGHHI, filt01, filt23, filt45, filt67, offset);
                    }
                    else {
                        res1 = filter_u8_u16(linesABBC, linesCDDE, linesEFFG, linesGHHI, filt01, filt23, filt45, filt67);
                        res1 = _mm256_add_epi16(res1, loada2_m128i(ref0, ref0 + pitchRef0));
                        res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res1 = _mm256_srai_epi16(res1, ROUND_POST_COMPOUND);
                    }
                }
                else {
                    res = filter_u8_u16(linesABBC, linesCDDE, linesEFFG, linesGHHI, filt01, filt23, filt45, filt67);
                    res = _mm256_add_epi16(res, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storeu2_m128i(dst + 0, dst + pitchDst + 0, res);
                }

                linesABBC = _mm256_unpackhi_epi8(lineAB, lineBC); // 32b: A8 B8 A9 B9 ... | B8 C8 B9 C9 ...
                linesCDDE = _mm256_unpackhi_epi8(lineCD, lineDE); // 32b: C8 D8 C9 D9 ... | D8 E8 D9 E9 ...
                linesEFFG = _mm256_unpackhi_epi8(lineEF, lineFG); // 32b: E8 F8 E9 F9 ... | F8 G8 F9 G9 ...
                linesGHHI = _mm256_unpackhi_epi8(lineGH, lineHI); // 32b: G8 H8 G9 H9 ... | H8 I8 H9 I9 ...

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res2 = filter_u8_u8(linesABBC, linesCDDE, linesEFFG, linesGHHI, filt01, filt23, filt45, filt67, offset);
                    }
                    else {
                        res2 = filter_u8_u16(linesABBC, linesCDDE, linesEFFG, linesGHHI, filt01, filt23, filt45, filt67);
                        res2 = _mm256_add_epi16(res2, loada2_m128i(ref0 + 8, ref0 + pitchRef0 + 8));
                        res2 = _mm256_add_epi16(res2, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res2 = _mm256_srai_epi16(res2, ROUND_POST_COMPOUND);
                    }
                    res = _mm256_packus_epi16(res1, res2);  // 32b: A0 .. A15 | B0 .. B15
                    storeu2_m128i(dst, dst + pitchDst, res);
                }
                else {
                    res = filter_u8_u16(linesABBC, linesCDDE, linesEFFG, linesGHHI, filt01, filt23, filt45, filt67);
                    res = _mm256_add_epi16(res, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storeu2_m128i(dst + 8, dst + pitchDst + 8, res);
                }

                lineAB = lineCD;
                lineBC = lineDE;
                lineCD = lineEF;
                lineDE = lineFG;
                lineEF = lineGH;
                lineFG = lineHI;
                dst += pitchDst * 2;
                src += pitchSrc * 2;
                if (avg == do_avg)
                    ref0 += pitchRef0 * 2;
            }
        }
        template <int avg, int w32, typename TDst>
        void interp_w32_nohorz_dovert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fy, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");
            // lineA: A0 A1 ... A14 A15 | A16 A17 ... A30 A31
            // lineB: B0 B1 ... B14 B15 | B16 B17 ... B30 B31
            // lineC: C0 C1 ... C14 C15 | C16 C17 ... C30 C31
            // lineD: D0 D1 ... D14 D15 | D16 D17 ... D30 D31
            // lineE: E0 E1 ... E14 E15 | E16 E17 ... E30 E31
            // lineF: F0 F1 ... F14 F15 | F16 F17 ... F30 F31
            // lineG: G0 G1 ... G14 G15 | G16 G17 ... G30 G31
            // lineH: H0 H1 ... H14 H15 | H16 H17 ... H30 H31
            __m128i filt = loada_si128(fy);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: 0 1 0 1 ...
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 32b: 2 3 2 3 ...
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 32b: 4 5 4 5 ...
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 32b: 6 7 6 7 ...

            for (int32_t x = 0; x < w32; x += 32) {
                const uint8_t *psrc = src + x;
                const int16_t *pref0 = ref0 + x;
                TDst *pdst = dst + x;
                __m256i lineA = loadu_si256(psrc - pitchSrc * 3);  // 32b: A0 A1 ... A14 A15 | A16 A17 ... A30 A31
                __m256i lineB = loadu_si256(psrc - pitchSrc * 2);  // 32b: B0 B1 ... B14 B15 | B16 B17 ... B30 B31
                __m256i lineC = loadu_si256(psrc - pitchSrc);
                __m256i lineD = loadu_si256(psrc);
                __m256i lineE = loadu_si256(psrc + pitchSrc);
                __m256i lineF = loadu_si256(psrc + pitchSrc * 2);
                __m256i lineG = loadu_si256(psrc + pitchSrc * 3);
                __m256i res, res1, res2;
                const __m256i offset = _mm256_set1_epi16(64);

                for (int32_t y = 0; y < h; y++) {
                    __m256i lineH = loadu_si256(psrc + pitchSrc * 4);

                    __m256i linesAB = _mm256_unpacklo_epi8(lineA, lineB); // 32b: A0 B0 A1 B1 ... | A16 B16 A17 B17 ...
                    __m256i linesCD = _mm256_unpacklo_epi8(lineC, lineD);
                    __m256i linesEF = _mm256_unpacklo_epi8(lineE, lineF);
                    __m256i linesGH = _mm256_unpacklo_epi8(lineG, lineH);

                    if (std::is_same<TDst, uint8_t>::value) {
                        if (avg == no_avg) {
                            res1 = filter_u8_u8(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67, offset);  // 16w: 0 1 ... 6 7 | 16 17 ... 22 23
                        }
                        else {
                            res1 = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                            res1 = _mm256_add_epi16(res1, loada2_m128i(pref0, pref0 + 16));
                            res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                            res1 = _mm256_srai_epi16(res1, ROUND_POST_COMPOUND);
                        }
                    }
                    else {
                        res = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                        res = _mm256_add_epi16(res, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                        storea2_m128i(pdst, pdst + 16, res);
                    }

                    linesAB = _mm256_unpackhi_epi8(lineA, lineB);
                    linesCD = _mm256_unpackhi_epi8(lineC, lineD);
                    linesEF = _mm256_unpackhi_epi8(lineE, lineF);
                    linesGH = _mm256_unpackhi_epi8(lineG, lineH);

                    if (std::is_same<TDst, uint8_t>::value) {
                        if (avg == no_avg) {
                            res2 = filter_u8_u8(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67, offset);  // 16w: 8 9 ... 14 15 | 24 25 ... 30 31
                        }
                        else {
                            res2 = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                            res2 = _mm256_add_epi16(res2, loada2_m128i(pref0 + 8, pref0 + 24));
                            res2 = _mm256_add_epi16(res2, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                            res2 = _mm256_srai_epi16(res2, ROUND_POST_COMPOUND);
                        }
                        res = _mm256_packus_epi16(res1, res2); // 32b: 0 .. 15 | 16 .. 31
                        storea_si256(pdst, _mm256_packus_epi16(res1, res2));
                    }
                    else {
                        res = filter_u8_u16(linesAB, linesCD, linesEF, linesGH, filt01, filt23, filt45, filt67);
                        res = _mm256_add_epi16(res, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                        storea2_m128i(pdst + 8, pdst + 24, res);
                    }

                    lineA = lineB;
                    lineB = lineC;
                    lineC = lineD;
                    lineD = lineE;
                    lineE = lineF;
                    lineF = lineG;
                    lineG = lineH;
                    pdst += pitchDst;
                    psrc += pitchSrc;
                    if (avg == do_avg)
                        pref0 += pitchRef0;
                }
            }
        }

        template <int avg, typename TDst>
        void interp_w4_nohorz_dovert(const int16_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fy, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            __m128i filt = loada_si128(fy);     // 8w: 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastd_epi32(filt);
            __m128i filt23 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt, 4));
            __m128i filt45 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt, 8));
            __m128i filt67 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt, 12));

            __m128i lineA = loadl_epi64(src - pitchSrc * 3);  // 4w: A0 A1 A2 A3
            __m128i lineB = loadl_epi64(src - pitchSrc * 2);
            __m128i lineC = loadl_epi64(src - pitchSrc * 1);
            __m128i lineD = loadl_epi64(src + pitchSrc * 0);
            __m128i lineE = loadl_epi64(src + pitchSrc * 1);
            __m128i lineF = loadl_epi64(src + pitchSrc * 2);
            __m128i lineG = loadl_epi64(src + pitchSrc * 3);
            __m128i lineH;
            __m128i sum, res;

            for (int32_t y = 0; y < h; y++) {
                lineH = loadl_epi64(src + pitchSrc * 4);
                __m128i a = _mm_madd_epi16(_mm_unpacklo_epi16(lineA, lineB), filt01);
                __m128i b = _mm_madd_epi16(_mm_unpacklo_epi16(lineC, lineD), filt23);
                __m128i c = _mm_madd_epi16(_mm_unpacklo_epi16(lineE, lineF), filt45);
                __m128i d = _mm_madd_epi16(_mm_unpacklo_epi16(lineG, lineH), filt67);

                sum = _mm_add_epi32(_mm_add_epi32(a, b), _mm_add_epi32(c, d));

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        const __m128i offset = _mm_set1_epi32(OFFSET_STAGE1);
                        sum = _mm_add_epi32(sum, offset);
                        sum = _mm_srai_epi32(sum, ROUND_STAGE1);
                        res = _mm_packus_epi32(sum, sum);
                    }
                    else {
                        const int shift = (2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND);
                        const __m128i offset = _mm_set1_epi16(1 << shift >> 1);
                        sum = _mm_add_epi32(sum, _mm_set1_epi32(OFFSET_PRE_COMPOUND));
                        sum = _mm_srai_epi32(sum, ROUND_PRE_COMPOUND);
                        sum = _mm_packs_epi32(sum, sum);
                        sum = _mm_add_epi16(sum, loadl_epi64(ref0));
                        sum = _mm_srai_epi16(sum, 1);
                        sum = _mm_add_epi16(sum, offset);
                        res = _mm_srai_epi16(sum, shift);
                    }
                    res = _mm_packus_epi16(res, res);
                    storel_si32(dst, res);
                }
                else {
                    const __m128i offset = _mm_set1_epi32(OFFSET_PRE_COMPOUND);
                    sum = _mm_add_epi32(sum, offset);
                    sum = _mm_srai_epi32(sum, ROUND_PRE_COMPOUND);
                    res = _mm_packs_epi32(sum, sum);
                    storel_epi64(dst, res);
                }

                lineA = lineB;
                lineB = lineC;
                lineC = lineD;
                lineD = lineE;
                lineE = lineF;
                lineF = lineG;
                lineG = lineH;
                src += pitchSrc;
                dst += pitchDst;
                if (avg == do_avg)
                    ref0 += pitchRef0;
            }
        }

        ALIGN_DECL(16) const uint8_t shuftab_horz_w4_1[32] = { 0,1,1,2,2,3,3,4, 8,9,9,10,10,11,11,12 };
        ALIGN_DECL(16) const uint8_t shuftab_horz_w4_2[32] = { 2,3,3,4,4,5,5,6, 10,11,11,12,12,13,13,14 };
        ALIGN_DECL(32) const uint8_t shuftab_horz_1[32] = { 0,1,1,2,2,3,3,4, 4,5,5,6,6,7,7,8, 0,1,1,2,2,3,3,4, 4,5,5,6,6,7,7,8 };
        ALIGN_DECL(32) const uint8_t shuftab_horz_2[32] = { 2,3,3,4,4,5,5,6, 6,7,7,8,8,9,9,10, 2,3,3,4,4,5,5,6, 6,7,7,8,8,9,9,10 };
        ALIGN_DECL(32) const uint8_t shuftab_horz_3[32] = { 4,5,5,6,6,7,7,8, 8,9,9,10,10,11,11,12, 4,5,5,6,6,7,7,8, 8,9,9,10,10,11,11,12 };
        ALIGN_DECL(32) const uint8_t shuftab_horz_4[32] = { 6,7,7,8,8,9,9,10, 10,11,11,12,12,13,13,14, 6,7,7,8,8,9,9,10, 10,11,11,12,12,13,13,14 };
        ALIGN_DECL(32) const uint8_t shuftab_nv12_horz1[32] = { 0,2,1,3,2,4,3,5, 4,6,5,7,6,8,7,9, 0,2,1,3,2,4,3,5, 4,6,5,7,6,8,7,9 };
        ALIGN_DECL(32) const uint8_t shuftab_nv12_horz2[32] = { 4,6,5,7,6,8,7,9, 8,10,9,11,10,12,11,13, 4,6,5,7,6,8,7,9, 8,10,9,11,10,12,11,13 };

        template <int avg, typename TDst>
        void interp_w4_dohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, int h)
        {
            //static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int32_t>::value, "expected uint8_t or int32_t");
            //static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int32_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            const __m128i offset1 = _mm_set1_epi32(OFFSET_STAGE0);
            const __m128i offset = _mm_set1_epi16((1 << (FILTER_BITS - 1)) + OFFSET_STAGE0);

            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt); // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_horz_w4_1);
            __m128i shuftab2 = loada_si128(shuftab_horz_w4_2);

            for (int32_t y = 0; y < h; y += 2) {
                __m128i s, a, b, c, d, res1, res2;
                s = loadu2_epi64(src - 3, src + pitchSrc - 3);  // A00 A01 A02 A03 A04 A05 A06 A07  B00 B01 B02 B03 B04 B05 B06 B07
                a = _mm_shuffle_epi8(s, shuftab1);              // A00 A01 A01 A02 A02 A03 A03 A04  B00 B01 B01 B02 B02 B03 B03 B04
                b = _mm_shuffle_epi8(s, shuftab2);              // A02 A03 A03 A04 A04 A05 A05 A06  B02 B03 B03 B04 B04 B05 B05 B06

                s = loadu2_epi64(src + 1, src + pitchSrc + 1);  // A04 A05 A06 A07 A08 A09 A10 A11  B04 B05 B06 B07 B08 B09 B10 B11
                c = _mm_shuffle_epi8(s, shuftab1);              // A04 A05 A05 A06 A06 A07 A07 A08  B04 B05 B05 B06 B06 B07 B07 B08
                d = _mm_shuffle_epi8(s, shuftab2);              // A06 A07 A07 A08 A08 A09 A09 A10  B06 B07 B07 B08 B08 B09 B09 B10

                a = _mm_maddubs_epi16(a, filt01);
                b = _mm_maddubs_epi16(b, filt23);
                c = _mm_maddubs_epi16(c, filt45);
                d = _mm_maddubs_epi16(d, filt67);

                a = _mm_add_epi16(a, c);
                b = _mm_add_epi16(b, d);
                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res1 = _mm_adds_epi16(a, b); // order of addition matters to avoid overflow
                        res1 = _mm_adds_epi16(res1, offset);
                        res1 = _mm_srai_epi16(res1, FILTER_BITS);   // 8w: A0 A1 A2 A3 B0 B1 B2 B3
                    }
                    else {
                        const int shift = (2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND);
                        const __m128i offset = _mm_set1_epi16(1 << (shift - 1));
                        res1 = _mm_unpacklo_epi16(a, b);
                        res2 = _mm_unpackhi_epi16(a, b);
                        res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                        res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                        res1 = _mm_add_epi32(res1, offset1);
                        res2 = _mm_add_epi32(res2, offset1);
                        res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                        res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                        res1 = _mm_packs_epi32(res1, res2);
                        res1 = _mm_add_epi16(res1, loadu2_epi64(ref0, ref0 + pitchRef0));
                        res1 = _mm_srai_epi16(res1, 1);
                        res1 = _mm_add_epi16(res1, offset);
                        res1 = _mm_srai_epi16(res1, shift);
                    }
                    res1 = _mm_packus_epi16(res1, res1);
                    storel_si32(dst, res1);
                    storel_si32(dst + pitchDst, _mm_bsrli_si128(res1, 4));
                }
                else if (std::is_same<TDst, int16_t>::value) {
                    res1 = _mm_unpacklo_epi16(a, b);
                    res2 = _mm_unpackhi_epi16(a, b);
                    res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                    res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                    res1 = _mm_add_epi32(res1, offset1);
                    res2 = _mm_add_epi32(res2, offset1);
                    res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                    res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                    res1 = _mm_packs_epi32(res1, res2);
                    storel_epi64(dst + 0 * pitchDst, res1);
                    storeh_epi64(dst + 1 * pitchDst, res1);
                }
                else {
                    res1 = _mm_unpacklo_epi16(a, b);
                    res2 = _mm_unpackhi_epi16(a, b);
                    res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                    res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                    res1 = _mm_add_epi32(res1, offset1);
                    res2 = _mm_add_epi32(res2, offset1);
                    res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                    res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                    res1 = _mm_slli_epi32(res1, FILTER_BITS);
                    res2 = _mm_slli_epi32(res2, FILTER_BITS);
                    storea_si128(dst + 0 * pitchDst, res1);
                    storea_si128(dst + 1 * pitchDst, res2);
                }

                src += pitchSrc * 2;
                dst += pitchDst * 2;
                if (avg == do_avg)
                    ref0 += pitchRef0 * 2;
            }
        }
        template <int avg, typename TDst>
        void interp_w8_dohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int32_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            const __m256i offset = _mm256_set1_epi16((1 << (FILTER_BITS - 1)) + OFFSET_STAGE0);

            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_horz_1);
            __m256i shuftab2 = loada_si256(shuftab_horz_2);
            __m256i shuftab3 = loada_si256(shuftab_horz_3);
            __m256i shuftab4 = loada_si256(shuftab_horz_4);

            for (int32_t y = 0; y < h; y += 4) {
                __m256i s, a, b, c, d, res1, res2, res;

                s = loadu2_m128i(src - 3, src - 3 + pitchSrc);  // A00 A01 A02 A03 A04 A05 A06 A07  A08 A09 A10 A11 A12 A13 A14 A15  B00 B01 B02 B03 B04 B05 B06 B07  B08 B09 B10 B11 B12 B13 B14 B15
                a = _mm256_shuffle_epi8(s, shuftab1);           // A00 A01 A01 A02 A02 A03 A03 A04  A04 A05 A05 A06 A06 A07 A07 A08  B..
                b = _mm256_shuffle_epi8(s, shuftab2);           // A02 A03 A03 A04 A04 A05 A05 A06  A06 A07 A07 A08 A08 A09 A09 A10  B..
                c = _mm256_shuffle_epi8(s, shuftab3);           // A04 A05 A05 A06 A06 A07 A07 A08  A08 A09 A09 A10 A10 A11 A11 A12  B..
                d = _mm256_shuffle_epi8(s, shuftab4);           // A06 A07 A07 A08 A08 A09 A09 A10  A10 A11 A11 A12 A12 A13 A13 A14  B..

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res1 = filter_u8_u8(a, b, c, d, filt01, filt23, filt45, filt67, offset);    // 16w: A00 A01 A02 A03 A04 A05 A06 A07  B..
                    }
                    else {
                        res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                        res1 = _mm256_add_epi16(res1, loada2_m128i(ref0, ref0 + pitchRef0));
                        res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res1 = _mm256_srai_epi16(res1, ROUND_POST_COMPOUND);
                    }
                }
                else {
                    res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storea2_m128i(dst, dst + pitchDst, res1);
                }

                s = loadu2_m128i(src - 3 + pitchSrc * 2, src - 3 + pitchSrc * 3);  // C..  D..
                a = _mm256_shuffle_epi8(s, shuftab1);
                b = _mm256_shuffle_epi8(s, shuftab2);
                c = _mm256_shuffle_epi8(s, shuftab3);
                d = _mm256_shuffle_epi8(s, shuftab4);

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res2 = filter_u8_u8(a, b, c, d, filt01, filt23, filt45, filt67, offset);    // 16w: C..  D..
                        res = _mm256_packus_epi16(res1, res2);                                      // 32b: A.. C.. B.. D..
                    }
                    else {
                        res2 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                        res2 = _mm256_add_epi16(res2, loada2_m128i(ref0 + pitchRef0 * 2, ref0 + pitchRef0 * 3));
                        res2 = _mm256_add_epi16(res2, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res2 = _mm256_srai_epi16(res2, ROUND_POST_COMPOUND);
                        res = _mm256_packus_epi16(res1, res2);
                    }
                    storel_epi64(dst + 0 * pitchDst, si128(res));
                    storel_epi64(dst + 1 * pitchDst, _mm256_extracti128_si256(res, 1));
                    storeh_epi64(dst + 2 * pitchDst, si128(res));
                    storeh_epi64(dst + 3 * pitchDst, _mm256_extracti128_si256(res, 1));
                }
                else {
                    res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storea2_m128i(dst + 2 * pitchDst, dst + 3 * pitchDst, res1);
                }

                src += pitchSrc * 4;
                dst += pitchDst * 4;
                if (avg == do_avg)
                    ref0 += pitchRef0 * 4;
            }
        }
        template <int avg, typename TDst>
        void interp_w16_dohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            const __m256i offset = _mm256_set1_epi16((1 << (FILTER_BITS - 1)) + OFFSET_STAGE0);

            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_horz_1);
            __m256i shuftab2 = loada_si256(shuftab_horz_2);
            __m256i shuftab3 = loada_si256(shuftab_horz_3);
            __m256i shuftab4 = loada_si256(shuftab_horz_4);

            for (int32_t y = 0; y < h; y += 2) {
                __m256i s, a, b, c, d, res1, res2;

                s = loadu2_m128i(src - 3, src + pitchSrc - 3);  // A00 A01 A02 A03 A04 A05 A06 A07  A08 A09 A10 A11 A12 A13 A14 A15 | B00 B01 B02 B03 B04 B05 B06 B07  B08 B09 B10 B11 B12 B13 B14 B15
                a = _mm256_shuffle_epi8(s, shuftab1);           // A00 A01 A01 A02 A02 A03 A03 A04  A04 A05 A05 A06 A06 A07 A07 A08 | B00 B01 B01 B02 B02 B03 B03 B04  B04 B05 B05 B06 B06 B07 B07 B08
                b = _mm256_shuffle_epi8(s, shuftab2);           // A02 A03 A03 A04 A04 A05 A05 A06  A06 A07 A07 A08 A08 A09 A09 A10 | B02 B03 B03 B04 B04 B05 B05 B06  B06 B07 B07 B08 B08 B09 B09 B10
                c = _mm256_shuffle_epi8(s, shuftab3);           // A04 A05 A05 A06 A06 A07 A07 A08  A08 A09 A09 A10 A10 A11 A11 A12 | B04 B05 B05 B06 B06 B07 B07 B08  B08 B09 B09 B10 B10 B11 B11 B12
                d = _mm256_shuffle_epi8(s, shuftab4);           // A06 A07 A07 A08 A08 A09 A09 A10  A10 A11 A11 A12 A12 A13 A13 A14 | B06 B07 B07 B08 B08 B09 B09 B10  B10 B11 B11 B12 B12 B13 B13 B14

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res1 = filter_u8_u8(a, b, c, d, filt01, filt23, filt45, filt67, offset);    // 16w: A00 A01 A02 A03 A04 A05 A06 A07 | B00 B01 B02 B03 B04 B05 B06 B07
                    }
                    else {
                        res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                        res1 = _mm256_add_epi16(res1, loada2_m128i(ref0, ref0 + pitchRef0));
                        res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res1 = _mm256_srai_epi16(res1, ROUND_POST_COMPOUND);
                    }
                }
                else {
                    res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storea2_m128i(dst, dst + pitchDst, res1);
                }

                s = loadu2_m128i(src + 5, src + pitchSrc + 5);
                a = _mm256_shuffle_epi8(s, shuftab1);
                b = _mm256_shuffle_epi8(s, shuftab2);
                c = _mm256_shuffle_epi8(s, shuftab3);
                d = _mm256_shuffle_epi8(s, shuftab4);

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        res2 = filter_u8_u8(a, b, c, d, filt01, filt23, filt45, filt67, offset);    // 16w: A08 A09 A10 A11 A12 A13 A14 A15 | B08 B09 B10 B11 B12 B13 B14 B15
                    }
                    else {
                        res2 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                        res2 = _mm256_add_epi16(res2, loada2_m128i(ref0 + 8, ref0 + pitchRef0 + 8));
                        res2 = _mm256_add_epi16(res2, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                        res2 = _mm256_srai_epi16(res2, ROUND_POST_COMPOUND);
                    }
                    res1 = _mm256_packus_epi16(res1, res2); // 32b: A0 .. A15 | B0 .. B15
                    storea2_m128i(dst, dst + pitchDst, res1);
                }
                else {
                    res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                    storea2_m128i(dst + 8, dst + pitchDst + 8, res1);
                }

                src += pitchSrc * 2;
                dst += pitchDst * 2;
                if (avg == do_avg)
                    ref0 += pitchRef0 * 2;
            }
        }
        template <int avg, int w32, typename TDst>
        void interp_w32_dohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, int h)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            const __m256i offset = _mm256_set1_epi16((1 << (FILTER_BITS - 1)) + OFFSET_STAGE0);

            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_horz_1);
            __m256i shuftab2 = loada_si256(shuftab_horz_2);
            __m256i shuftab3 = loada_si256(shuftab_horz_3);
            __m256i shuftab4 = loada_si256(shuftab_horz_4);

            for (int32_t y = 0; y < h; y++) {
                for (int32_t x = 0; x < w32; x += 32) {
                    __m256i s, a, b, c, d, res1, res2;

                    s = loadu_si256(src + x - 3);           // 00 01 02 03 04 05 06 07  08 09 10 11 12 13 14 15  16 17 18 19 20 21 22 23  24 25 26 27 28 29 30 31
                    a = _mm256_shuffle_epi8(s, shuftab1);   // 00 01 01 02 02 03 03 04  04 05 05 06 06 07 07 08  16 17 17 18 18 19 19 20  20 21 21 22 22 23 23 24
                    b = _mm256_shuffle_epi8(s, shuftab2);   // 02 03 03 04 04 05 05 06  06 07 07 08 08 09 09 10  18 19 19 20 20 21 21 22  22 23 23 24 24 25 25 26
                    c = _mm256_shuffle_epi8(s, shuftab3);   // 04 05 05 06 06 07 07 08  08 09 09 10 10 11 11 12  20 21 21 22 22 23 23 24  24 25 25 26 26 27 27 28
                    d = _mm256_shuffle_epi8(s, shuftab4);   // 06 07 07 08 08 09 09 10  10 11 11 12 12 13 13 14  22 23 23 24 24 25 25 26  26 27 27 28 28 29 29 30

                    if (std::is_same<TDst, uint8_t>::value) {
                        if (avg == no_avg) {
                            res1 = filter_u8_u8(a, b, c, d, filt01, filt23, filt45, filt67, offset);    // 16w: 00 01 02 03 04 05 06 07  16 17 18 19 20 21 22 23
                        }
                        else {
                            res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                            res1 = _mm256_add_epi16(res1, loada2_m128i(ref0 + x, ref0 + x + 16));
                            res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                            res1 = _mm256_srai_epi16(res1, ROUND_POST_COMPOUND);
                        }
                    }
                    else {
                        res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                        res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                        storea2_m128i(dst + x, dst + x + 16, res1);
                    }

                    s = loadu_si256(src + x + 5);           // 08 09 10 11 12 13 14 15  16 17 18 19 20 21 22 23  24 25 26 27 28 29 30 31  32 33 34 35 36 37 38 39
                    a = _mm256_shuffle_epi8(s, shuftab1);   // 08 09 09 10 10 11 11 12  12 13 13 14 14 15 15 16  24 25 25 26 26 27 27 28  28 29 29 30 30 31 31 32
                    b = _mm256_shuffle_epi8(s, shuftab2);   // 10 11 11 12 12 13 13 14  14 15 15 16 16 17 17 18  26 27 27 28 28 29 29 30  30 31 31 32 32 33 33 34
                    c = _mm256_shuffle_epi8(s, shuftab3);   // 12 13 13 14 14 15 15 16  16 17 17 18 18 19 19 20  28 29 29 30 30 31 31 32  32 33 33 34 34 35 35 36
                    d = _mm256_shuffle_epi8(s, shuftab4);   // 14 15 15 16 16 17 17 18  18 19 19 20 20 21 21 22  30 31 31 32 32 33 33 34  34 35 35 36 36 37 37 38

                    if (std::is_same<TDst, uint8_t>::value) {
                        if (avg == no_avg) {
                            res2 = filter_u8_u8(a, b, c, d, filt01, filt23, filt45, filt67, offset);    // 16w: 08 09 10 11 12 13 14 15  24 25 26 27 28 29 30 31
                        }
                        else {
                            res2 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                            res2 = _mm256_add_epi16(res2, loada2_m128i(ref0 + x + 8, ref0 + x + 24));
                            res2 = _mm256_add_epi16(res2, _mm256_set1_epi16(OFFSET_POST_COMPOUND - (ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0)));
                            res2 = _mm256_srai_epi16(res2, ROUND_POST_COMPOUND);
                        }
                        res1 = _mm256_packus_epi16(res1, res2);
                        storea_si256(dst + x, res1);
                    }
                    else {
                        res1 = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                        res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE1));
                        storea2_m128i(dst + x + 8, dst + x + 24, res1);
                    }
                }

                src += pitchSrc;
                dst += pitchDst;
                if (avg == do_avg)
                    ref0 += pitchRef0;
            }
        }

        template <int avg, typename TDst>
        void interp_nv12_w4_dohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, int h)
        {
            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt); // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_nv12_horz1);
            __m128i shuftab2 = loada_si128(shuftab_nv12_horz2);

            for (int32_t y = 0; y < h; y++) {
                __m128i src1, src2, src3, a, b, c, d;
                src1 = loadu_si128(src - 6);            // 16b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7
                src3 = loadu_si128(src + 10);           // 16b: u8 v8 u9 v9 uA vA ** ** ** ** ** ** ** ** ** **
                src2 = _mm_alignr_epi8(src3, src1, 8);  // 16b: u4 v4 u5 v5 u6 v6 u7 v7 u8 v8 u9 v9 uA vA ** **

                a = _mm_shuffle_epi8(src1, shuftab1);   // 16b: u0 u1 v0 v1 u1 u2 v1 v2 u2 u3 v2 v3 u3 u4 v3 v4
                b = _mm_shuffle_epi8(src1, shuftab2);   // 16b: u2 u3 v2 v3 u3 u4 v3 v4 u4 u5 v4 v5 u5 u6 v5 v6
                c = _mm_shuffle_epi8(src2, shuftab1);   // 16b: u4 u5 v4 v5 u5 u6 v5 v6 u6 u7 v6 v7 u7 u8 v7 v8
                d = _mm_shuffle_epi8(src2, shuftab2);   // 16b: u6 u7 v6 v7 u7 u8 v7 v8 u8 u9 v8 v9 u9 uA v9 vA

                a = _mm_maddubs_epi16(a, filt01);
                b = _mm_maddubs_epi16(b, filt23);
                c = _mm_maddubs_epi16(c, filt45);
                d = _mm_maddubs_epi16(d, filt67);

                a = _mm_add_epi16(a, c);
                b = _mm_add_epi16(b, d);

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        a = _mm_adds_epi16(a, b);
                        a = _mm_adds_epi16(a, _mm_set1_epi16(64 + 4));
                        a = _mm_srai_epi16(a, FILTER_BITS);
                        a = _mm_packus_epi16(a, a);
                        storel_epi64(dst, a);
                    }
                    else {
                        __m128i res1, res2;
                        res1 = _mm_unpacklo_epi16(a, b);
                        res2 = _mm_unpackhi_epi16(a, b);
                        res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                        res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                        res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                        res2 = _mm_add_epi32(res2, _mm_set1_epi32(OFFSET_STAGE0));
                        res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                        res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                        res1 = _mm_packs_epi32(res1, res2);
                        res1 = _mm_add_epi16(res1, loada_si128(ref0));

                        const int shift = ROUND_POST_COMPOUND;
                        const __m128i offset = _mm_set1_epi16(1 << shift >> 1);
                        res1 = _mm_add_epi16(res1, offset);
                        res1 = _mm_srai_epi16(res1, shift);
                        res1 = _mm_packus_epi16(res1, res1);
                        storel_epi64(dst, res1);
                    }
                }
                else {
                    __m128i res1, res2;
                    res1 = _mm_unpacklo_epi16(a, b);
                    res2 = _mm_unpackhi_epi16(a, b);
                    res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                    res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                    res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                    res2 = _mm_add_epi32(res2, _mm_set1_epi32(OFFSET_STAGE0));
                    res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                    res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                    res1 = _mm_packs_epi32(res1, res2);
                    storea_si128(dst, res1);
                }

                src += pitchSrc;
                dst += pitchDst;
                if (avg == do_avg)
                    ref0 += pitchRef0;
            }
        }
        template <int avg, typename TDst>
        void interp_nv12_w8_dohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, int h)
        {
            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt);                     // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_nv12_horz1);
            __m128i shuftab2 = loada_si128(shuftab_nv12_horz2);

            for (int32_t y = 0; y < h; y++) {
                __m128i src1, src2, src3, a, b, c, d, e, f, a2, b2, c2, d2;
                src1 = loadu_si128(src - 6);           // 16b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7
                src3 = loadu_si128(src + 10);          // 16b: u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF
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

                a = _mm_add_epi16(a, c);
                b = _mm_add_epi16(b, d);
                a2 = _mm_add_epi16(a2, c2);
                b2 = _mm_add_epi16(b2, d2);

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        __m128i res1 = _mm_adds_epi16(a, b);
                        __m128i res2 = _mm_adds_epi16(a2, b2);
                        res1 = _mm_adds_epi16(res1, _mm_set1_epi16(64 + 4));
                        res2 = _mm_adds_epi16(res2, _mm_set1_epi16(64 + 4));
                        res1 = _mm_srai_epi16(res1, FILTER_BITS);   // 8w: u0 v0 u1 v1 u2 v2 u3 v3
                        res2 = _mm_srai_epi16(res2, FILTER_BITS);   // 8w: u4 v4 u5 v5 u6 v6 u7 v7
                        res1 = _mm_packus_epi16(res1, res2);
                        storea_si128(dst, res1);
                    }
                    else {
                        const int shift = ROUND_POST_COMPOUND;
                        const __m128i offset = _mm_set1_epi16(1 << shift >> 1);
                        __m128i res1, res2;
                        res1 = _mm_unpacklo_epi16(a, b);
                        res2 = _mm_unpackhi_epi16(a, b);
                        res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                        res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                        res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                        res2 = _mm_add_epi32(res2, _mm_set1_epi32(OFFSET_STAGE0));
                        res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                        res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                        res1 = _mm_packs_epi32(res1, res2);

                        res1 = _mm_add_epi16(res1, loada_si128(ref0));
                        res1 = _mm_add_epi16(res1, offset);
                        res1 = _mm_srai_epi16(res1, shift);
                        res1 = _mm_packus_epi16(res1, res1);
                        storel_epi64(dst, res1);

                        res1 = _mm_unpacklo_epi16(a2, b2);
                        res2 = _mm_unpackhi_epi16(a2, b2);
                        res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                        res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                        res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                        res2 = _mm_add_epi32(res2, _mm_set1_epi32(OFFSET_STAGE0));
                        res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                        res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                        res1 = _mm_packs_epi32(res1, res2);

                        res1 = _mm_add_epi16(res1, loada_si128(ref0 + 8));
                        res1 = _mm_add_epi16(res1, offset);
                        res1 = _mm_srai_epi16(res1, shift);
                        res1 = _mm_packus_epi16(res1, res1);
                        storel_epi64(dst + 8, res1);
                    }
                }
                else {
                    __m128i res1, res2;
                    res1 = _mm_unpacklo_epi16(a, b);
                    res2 = _mm_unpackhi_epi16(a, b);
                    res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                    res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                    res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                    res2 = _mm_add_epi32(res2, _mm_set1_epi32(OFFSET_STAGE0));
                    res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                    res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                    res1 = _mm_packs_epi32(res1, res2);
                    storea_si128(dst + 0, res1);
                    res1 = _mm_unpacklo_epi16(a2, b2);
                    res2 = _mm_unpackhi_epi16(a2, b2);
                    res1 = _mm_madd_epi16(res1, _mm_set1_epi16(1));
                    res2 = _mm_madd_epi16(res2, _mm_set1_epi16(1));
                    res1 = _mm_add_epi32(res1, _mm_set1_epi32(OFFSET_STAGE0));
                    res2 = _mm_add_epi32(res2, _mm_set1_epi32(OFFSET_STAGE0));
                    res1 = _mm_srai_epi32(res1, ROUND_STAGE0);
                    res2 = _mm_srai_epi32(res2, ROUND_STAGE0);
                    res1 = _mm_packs_epi32(res1, res2);
                    storea_si128(dst + 8, res1);
                }

                src += pitchSrc;
                dst += pitchDst;
                if (avg == do_avg)
                    ref0 += pitchRef0;
            }
        }
        template <int avg, int w16, typename TDst>
        void interp_nv12_w16_dohorz_novert(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, int h)
        {
            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_nv12_horz1);
            __m256i shuftab2 = loada_si256(shuftab_nv12_horz2);

            for (int32_t y = 0; y < h; y++) {
                for (int32_t x = 0; x < 2 * w16; x += 32) {
                    __m256i s, a, b, c, d, e, f, a1, b1, c1, d1;
                    s = loadu_si256(src + x - 6);           // 32b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7 | u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF
                    a = _mm256_shuffle_epi8(s, shuftab1);   // 32b: u0 u1 v0 v1 u1 u2 v1 v2 u2 u3 v2 v3 u3 u4 v3 v4 | u8 u9 v8 v9 u9 uA v9 vA uA uB vA vB uB uC vB vC
                    b = _mm256_shuffle_epi8(s, shuftab2);   // 32b: u2 u3 v2 v3 u3 u4 v3 v4 u4 u5 v4 v5 u5 u6 v5 v6 | uA uB vA vB uB uC vB vC uC uD vC vD uD uE vD vE
                    s = loadu_si256(src + x + 2);           // 32b: u4 v4 u5 v5 u6 v6 u7 v7 u8 v8 u9 v9 uA vA uB vB | uC vC uD vD uE vE uF vF uG vG uH vH uI vI uK vK
                    c = _mm256_shuffle_epi8(s, shuftab1);   // 32b: u4 u5 v4 v5 u5 u6 v5 v6 u6 u7 v6 v7 u7 u8 v7 v8 | uC uD vC vD uD uE vD vE uE uF vE vF uF uG vF vG
                    d = _mm256_shuffle_epi8(s, shuftab2);   // 32b: u6 u7 v6 v7 u7 u8 v7 v8 u8 u9 v8 v9 u9 uA v9 vA | uE uF vE vF uF uG vF vG uG uH vG vH uH uI vH vI
                    s = loadu_si256(src + x + 10);          // 32b: u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF | uG vG uH vH uI vI uK vK uL vL uM vM uN vN ** **
                    e = _mm256_shuffle_epi8(s, shuftab1);
                    f = _mm256_shuffle_epi8(s, shuftab2);

                    a1 = _mm256_maddubs_epi16(a, filt01);
                    b1 = _mm256_maddubs_epi16(b, filt23);
                    c1 = _mm256_maddubs_epi16(c, filt45);
                    d1 = _mm256_maddubs_epi16(d, filt67);
                    c = _mm256_maddubs_epi16(c, filt01);
                    d = _mm256_maddubs_epi16(d, filt23);
                    e = _mm256_maddubs_epi16(e, filt45);
                    f = _mm256_maddubs_epi16(f, filt67);
                    a1 = _mm256_add_epi16(a1, c1);
                    b1 = _mm256_add_epi16(b1, d1);
                    c = _mm256_add_epi16(c, e);
                    d = _mm256_add_epi16(d, f);

                    if (std::is_same<TDst, uint8_t>::value) {
                        if (avg == no_avg) {
                            __m256i res1 = _mm256_adds_epi16(a1, b1);
                            __m256i res2 = _mm256_adds_epi16(c, d);
                            res1 = _mm256_adds_epi16(res1, _mm256_set1_epi16(64 + 4));
                            res2 = _mm256_adds_epi16(res2, _mm256_set1_epi16(64 + 4));
                            res1 = _mm256_srai_epi16(res1, FILTER_BITS);    // 16w: u0 v0 u1 v1 u2 v2 u3 v3 | u8 v8 u9 v9 uA vA uB vB
                            res2 = _mm256_srai_epi16(res2, FILTER_BITS);    // 16w: u4 v4 u5 v5 u6 v6 u7 v7 | uC vC uD vD uE vE uF vF
                            res1 = _mm256_packus_epi16(res1, res2);
                            storea_si256(dst + x, res1);
                        }
                        else {
                            const int shift = ROUND_POST_COMPOUND;
                            const __m256i offset = _mm256_set1_epi16(OFFSET_POST_COMPOUND);
                            __m256i res1, res2, res3, res4;
                            res1 = _mm256_unpacklo_epi16(a1, b1);
                            res2 = _mm256_unpackhi_epi16(a1, b1);
                            res3 = _mm256_unpacklo_epi16(c, d);
                            res4 = _mm256_unpackhi_epi16(c, d);
                            res1 = _mm256_madd_epi16(res1, _mm256_set1_epi16(1));  // 8d: u0 v0 u1 v1 | u8 v8 u9 v9
                            res2 = _mm256_madd_epi16(res2, _mm256_set1_epi16(1));  // 8d: u2 v2 u3 v3 | uA vA uB vB
                            res3 = _mm256_madd_epi16(res3, _mm256_set1_epi16(1));  // 8d: u4 v4 u5 v5 | uC vC uD vD
                            res4 = _mm256_madd_epi16(res4, _mm256_set1_epi16(1));  // 8d: u6 v6 u7 v7 | uE vE uF vF
                            res1 = _mm256_add_epi32(res1, _mm256_set1_epi32(OFFSET_STAGE0));
                            res2 = _mm256_add_epi32(res2, _mm256_set1_epi32(OFFSET_STAGE0));
                            res3 = _mm256_add_epi32(res3, _mm256_set1_epi32(OFFSET_STAGE0));
                            res4 = _mm256_add_epi32(res4, _mm256_set1_epi32(OFFSET_STAGE0));
                            res1 = _mm256_srai_epi32(res1, ROUND_STAGE0);
                            res2 = _mm256_srai_epi32(res2, ROUND_STAGE0);
                            res3 = _mm256_srai_epi32(res3, ROUND_STAGE0);
                            res4 = _mm256_srai_epi32(res4, ROUND_STAGE0);
                            res1 = _mm256_packs_epi32(res1, res2);
                            res2 = _mm256_packs_epi32(res3, res4);

                            res1 = _mm256_add_epi16(res1, loada2_m128i(ref0 + x + 0, ref0 + x + 16));
                            res2 = _mm256_add_epi16(res2, loada2_m128i(ref0 + x + 8, ref0 + x + 24));

                            res1 = _mm256_add_epi16(res1, offset);
                            res2 = _mm256_add_epi16(res2, offset);
                            res1 = _mm256_srai_epi16(res1, shift);
                            res2 = _mm256_srai_epi16(res2, shift);
                            res1 = _mm256_packus_epi16(res1, res2); // 16w: u0 v0 u1 v1 u2 v2 u3 v3 | u8 v8 u9 v9 uA vA uB vB
                            storea_si256(dst + x, res1);
                        }
                    }
                    else {
                        __m256i res1, res2, res3, res4;
                        res1 = _mm256_unpacklo_epi16(a1, b1);
                        res2 = _mm256_unpackhi_epi16(a1, b1);
                        res3 = _mm256_unpacklo_epi16(c, d);
                        res4 = _mm256_unpackhi_epi16(c, d);
                        res1 = _mm256_madd_epi16(res1, _mm256_set1_epi16(1));  // 8d: u0 v0 u1 v1 | u8 v8 u9 v9
                        res2 = _mm256_madd_epi16(res2, _mm256_set1_epi16(1));  // 8d: u2 v2 u3 v3 | uA vA uB vB
                        res3 = _mm256_madd_epi16(res3, _mm256_set1_epi16(1));
                        res4 = _mm256_madd_epi16(res4, _mm256_set1_epi16(1));
                        res1 = _mm256_add_epi32(res1, _mm256_set1_epi32(OFFSET_STAGE0));
                        res2 = _mm256_add_epi32(res2, _mm256_set1_epi32(OFFSET_STAGE0));
                        res3 = _mm256_add_epi32(res3, _mm256_set1_epi32(OFFSET_STAGE0));
                        res4 = _mm256_add_epi32(res4, _mm256_set1_epi32(OFFSET_STAGE0));
                        res1 = _mm256_srai_epi32(res1, ROUND_STAGE0);
                        res2 = _mm256_srai_epi32(res2, ROUND_STAGE0);
                        res3 = _mm256_srai_epi32(res3, ROUND_STAGE0);
                        res4 = _mm256_srai_epi32(res4, ROUND_STAGE0);
                        res1 = _mm256_packs_epi32(res1, res2);
                        res2 = _mm256_packs_epi32(res3, res4);
                        storea2_m128i(dst + x + 0, dst + x + 16, res1);
                        storea2_m128i(dst + x + 8, dst + x + 24, res2);
                    }
                }
                src += pitchSrc;
                dst += pitchDst;
                if (avg == do_avg)
                    ref0 += pitchRef0;
            }
        }

        template <int avg, typename TDst>
        void interp_w8_both(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int height)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int32_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));   // 32b: (2 3) x 16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));   // 32b: (4 5) x 16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));   // 32b: (6 7) x 16
            __m256i shuftab1 = loada_si256(shuftab_horz_1);
            __m256i shuftab2 = loada_si256(shuftab_horz_2);
            __m256i shuftab3 = loada_si256(shuftab_horz_3);
            __m256i shuftab4 = loada_si256(shuftab_horz_4);

            const int PITCH_TMP = 8;
            ALIGN_DECL(32) uint16_t tmpBuf[8 * MAX_BLOCK_SIZE + 8]; // [8 * (2 * 8 + 8)]; // max height is 2 * width + 8 pixels
            uint16_t *tmp = tmpBuf;
            src -= pitchSrc * 3 + 3;

            for (int32_t y = 0; y < height + 8; y += 2) {
                __m256i s, a, b, c, d, res;
                s = loadu2_m128i(src, src + pitchSrc);  // A0..Af | B0..Bf
                a = _mm256_shuffle_epi8(s, shuftab1);   // A0 A1 A1 A2 .. | B..
                b = _mm256_shuffle_epi8(s, shuftab2);   // A2 A3 A3 A4 .. | B..
                c = _mm256_shuffle_epi8(s, shuftab3);   // A4 A5 A5 A6 .. | B..
                d = _mm256_shuffle_epi8(s, shuftab4);   // A6 A7 A7 A8 .. | B..

                res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67); // with ADDITIONAL_OFFSET_STAGE0
                storea_si256(tmp, res);

                src += 2 * pitchSrc;
                tmp += 2 * PITCH_TMP;
            }

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16w: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16w: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16w: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 12)); // 16w: (6 7) x 8
            tmp = tmpBuf;

            __m256i ab = loada_si256(tmp);                  // 16w: A0 A1 A2 A3 A4 A5 A6 A7 | B0 B1 B2 B3 B4 B5 B6 B7
            __m256i cd = loada_si256(tmp + 2 * PITCH_TMP);  // 16w: C0 C1 C2 C3 C4 C5 C6 C7 | D0 D1 D2 D3 D4 D5 D6 D7
            __m256i ef = loada_si256(tmp + 4 * PITCH_TMP);  // 16w: E0 E1 E2 E3 E4 E5 E6 E7 | F0 F1 F2 F3 F4 F5 F6 F7
            __m256i gh = loada_si256(tmp + 6 * PITCH_TMP);  // 16w: G0 G1 G2 G3 G4 G5 G6 G7 | H0 H1 H2 H3 H4 H5 H6 H7
            __m256i ik;

            __m256i bc = _mm256_permute2x128_si256(ab, cd, PERM2x128(1, 2));  // 16w: B0 B1 B2 B3 B4 B5 B6 B7 | C0 C1 C2 C3 C4 C5 C6 C7
            __m256i de = _mm256_permute2x128_si256(cd, ef, PERM2x128(1, 2));  // 16w: D0 D1 D2 D3 D4 D5 D6 D7 | E0 E1 E2 E3 E4 E5 E6 E7
            __m256i fg = _mm256_permute2x128_si256(ef, gh, PERM2x128(1, 2));  // 16w: F0 F1 F2 F3 F4 F5 F6 F7 | G0 G1 G2 G3 G4 G5 G6 G7
            __m256i hi;

            for (int y = 0; y < height; y += 2) {
                ik = loada_si256(tmp + 8 * PITCH_TMP);                    // 16w: I0 I1 I2 I3 I4 I5 I6 I7 | K0 K1 K2 K3 K4 K5 K6 K7
                hi = _mm256_permute2x128_si256(gh, ik, PERM2x128(1, 2));  // 16w: H0 H1 H2 H3 H4 H5 H6 H7 | I0 I1 I2 I3 I4 I5 I6 I7

                __m256i abbc, cdde, effg, ghhi, res1, res2;

                abbc = _mm256_unpacklo_epi16(ab, bc);    // 16w: A0 B0 A1 B1 A2 B2 A3 B3 | B0 C0 B1 C1 B2 C2 B3 C3
                cdde = _mm256_unpacklo_epi16(cd, de);    // 16w: C0 D0 C1 D1 C2 D2 C3 D3 | D0 E0 D1 E1 D2 E2 D3 E3
                effg = _mm256_unpacklo_epi16(ef, fg);    // 16w: E0 F0 E1 F1 E2 F2 E3 F3 | F0 G0 F1 G1 F2 G2 F3 G3
                ghhi = _mm256_unpacklo_epi16(gh, hi);    // 16w: G0 H0 G1 H1 G2 H2 G3 H3 | H0 I0 H1 I1 H2 I2 H3 I3

                res1 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[0..3] | row1[0..3]

                abbc = _mm256_unpackhi_epi16(ab, bc);    // 16w: A4 B4 A5 B5 A6 B6 A7 B7 | B4 C4 B5 C5 B6 C6 B7 C7
                cdde = _mm256_unpackhi_epi16(cd, de);    // 16w: C4 D4 C5 D5 C6 D6 C7 D7 | D4 E4 D5 E5 D6 E6 D7 E7
                effg = _mm256_unpackhi_epi16(ef, fg);    // 16w: E4 F4 E5 F5 E6 F6 E7 F7 | F4 G4 F5 G5 F6 G6 F7 G7
                ghhi = _mm256_unpackhi_epi16(gh, hi);    // 16w: G4 H4 G5 H5 G6 H6 G7 H7 | H4 I4 H5 I5 H6 I6 H7 I7

                res2 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[4..7] | row1[4..7]


                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        const __m256i offset = _mm256_set1_epi32(OFFSET_STAGE1 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));
                        res1 = _mm256_add_epi32(res1, offset);
                        res2 = _mm256_add_epi32(res2, offset);
                        res1 = _mm256_srai_epi32(res1, ROUND_STAGE1);
                        res2 = _mm256_srai_epi32(res2, ROUND_STAGE1);
                        res1 = _mm256_packus_epi32(res1, res2);  // 16w: row0 | row1
                        res1 = _mm256_packus_epi16(res1, res1);  // 32b: row0 row0 | row1 row1
                        storel_epi64(dst + 0 * pitchDst, si128_lo(res1));
                        storel_epi64(dst + 1 * pitchDst, si128_hi(res1));
                    }
                    else {
                        __m256i offset = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));
                        res1 = _mm256_add_epi32(res1, offset);
                        res2 = _mm256_add_epi32(res2, offset);
                        res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                        res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                        res1 = _mm256_packs_epi32(res1, res2);  // 16w: row0 | row1

                        res1 = _mm256_add_epi16(res1, loada2_m128i(ref0, ref0 + pitchRef0));
                        res1 = _mm256_add_epi16(res1, _mm256_set1_epi16(OFFSET_POST_COMPOUND));
                        res1 = _mm256_srai_epi16(res1, ROUND_POST_COMPOUND);
                        res1 = _mm256_packus_epi16(res1, res1);  // 32b: row0 row0 | row1 row1
                        storel_epi64(dst + 0 * pitchDst, si128_lo(res1));
                        storel_epi64(dst + 1 * pitchDst, si128_hi(res1));
                    }
                }
                else {
                    const __m256i offset = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));
                    res1 = _mm256_add_epi32(res1, offset);
                    res2 = _mm256_add_epi32(res2, offset);
                    res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                    res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                    res1 = _mm256_packs_epi32(res1, res2);  // 16w: row0 | row1
                    storea_si128(dst + 0 * pitchDst, si128_lo(res1));
                    storea_si128(dst + 1 * pitchDst, si128_hi(res1));
                }

                dst += 2 * pitchDst;
                tmp += 2 * PITCH_TMP;
                if (avg == do_avg)
                    ref0 += 2 * pitchRef0;
                ab = cd; cd = ef; ef = gh; gh = ik;
                bc = de; de = fg; fg = hi;
            }
        }

        template <int avg, int w16, typename TDst>
        void interp_w16_both(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int height)
        {
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value, "expected uint8_t or int16_t");
            static_assert(std::is_same<TDst, uint8_t>::value || std::is_same<TDst, int16_t>::value && avg == no_avg, "avg is only for TDst = uint8_t");

            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 32b: (2 3) x 16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 32b: (4 5) x 16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 32b: (6 7) x 16
            __m256i shuftab1 = loada_si256(shuftab_horz_1);
            __m256i shuftab2 = loada_si256(shuftab_horz_2);
            __m256i shuftab3 = loada_si256(shuftab_horz_3);
            __m256i shuftab4 = loada_si256(shuftab_horz_4);

            const int pitchTmp = w16;
            const int max_height = MAX_BLOCK_SIZE; // std::min(MAX_BLOCK_SIZE, 2 * w16);
            ALIGN_DECL(32) uint16_t tmpBuf[w16 * (max_height + 8)]; // max height is 2 * width + 8 pixels
            uint16_t *tmp = tmpBuf;
            src -= pitchSrc * 3 + 3;

            for (int32_t y = 0; y < height + 7; y++) {
                for (int32_t x = 0; x < w16; x += 16) {
                    __m256i s, a, b, c, d, res;
                    s = loadu_si256(src + x);                           // 00 .. 15 | 16 .. 31
                    s = _mm256_permute4x64_epi64(s, PERM4x64(0, 1, 1, 2)); // 00 .. 15 | 08 .. 23
                    a = _mm256_shuffle_epi8(s, shuftab1);               // 00 01 01 02 02 03 03 04 04 05 05 06 06 07 07 08 | 08 09 09 10 10 ..
                    b = _mm256_shuffle_epi8(s, shuftab2);               // 02 03 03 04 04 05 05 06 06 07 07 08 08 09 09 10 | 10 11 11 12 12 ..
                    c = _mm256_shuffle_epi8(s, shuftab3);               // 04 05 05 06 06 07 07 08 08 09 09 10 10 11 11 12 | 12 13 13 14 14 ..
                    d = _mm256_shuffle_epi8(s, shuftab4);               // 06 07 07 08 08 09 09 10 10 11 11 12 12 13 13 14 | 14 15 15 16 16 ..

                    res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    storea_si256(tmp + x, res);
                }

                src += pitchSrc;
                tmp += pitchTmp;
            }

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16b: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 8));  // 16b: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 12)); // 16b: (6 7) x 8

            for (int32_t x = 0; x < w16; x += 16) {
                const uint16_t *ptmp = tmpBuf + x;
                const int16_t *pref = ref0 + x;
                TDst *pdst = dst + x;

                __m256i a = loada_si256(ptmp);                   // 16w: A0 .. A15
                __m256i b = loada_si256(ptmp + pitchTmp);        // 16w: B0 .. B15
                __m256i c = loada_si256(ptmp + 2 * pitchTmp);    // 16w: C0 .. C15
                __m256i d = loada_si256(ptmp + 3 * pitchTmp);    // 16w: D0 .. D15
                __m256i e = loada_si256(ptmp + 4 * pitchTmp);    // 16w: E0 .. E15
                __m256i f = loada_si256(ptmp + 5 * pitchTmp);    // 16w: F0 .. F15
                __m256i g = loada_si256(ptmp + 6 * pitchTmp);    // 16w: G0 .. G15
                __m256i h, ab, cd, ef, gh, res1, res2;

                for (int y = 0; y < height; y++) {
                    h = loada_si256(ptmp + 7 * pitchTmp);        // 16w: H0 .. H15


                    ab = _mm256_unpacklo_epi16(a, b);           // 16w: A0 B0 .. A3 B3 | A8 B8 .. A11 B11
                    cd = _mm256_unpacklo_epi16(c, d);           // 16w: C0 D0 .. C3 D3 | C8 D8 .. C11 D11
                    ef = _mm256_unpacklo_epi16(e, f);           // 16w: E0 F0 .. E3 F3 | E8 F8 .. E11 F11
                    gh = _mm256_unpacklo_epi16(g, h);           // 16w: G0 H0 .. G3 H3 | G8 H8 .. G11 H11

                    res1 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 0 1 2 3 | 8 9 10 11

                    ab = _mm256_unpackhi_epi16(a, b);           // 16w: A4 B4 .. A7 B7 | A12 B12 .. A15 B15
                    cd = _mm256_unpackhi_epi16(c, d);           // 16w: C4 D4 .. C7 D7 | C12 D12 .. C15 D15
                    ef = _mm256_unpackhi_epi16(e, f);           // 16w: E4 F4 .. E7 F7 | E12 F12 .. E15 F15
                    gh = _mm256_unpackhi_epi16(g, h);           // 16w: G4 H4 .. G7 H7 | G12 H12 .. G15 H15

                    res2 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 4 5 6 7 | 12 13 14 15

                    if (std::is_same<TDst, uint8_t>::value) {
                        if (avg == no_avg) {
                            const __m256i offset = _mm256_set1_epi32(OFFSET_STAGE1 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                            res1 = _mm256_add_epi32(res1, offset);
                            res2 = _mm256_add_epi32(res2, offset);
                            res1 = _mm256_srai_epi32(res1, ROUND_STAGE1);
                            res2 = _mm256_srai_epi32(res2, ROUND_STAGE1);
                            res1 = _mm256_packus_epi32(res1, res2); // 16w: 0..7 | 8..15
                            res1 = _mm256_packus_epi16(res1, res1); // 32w: 0..7 0..7 | 8..15 8..15
                            res1 = _mm256_permute4x64_epi64(res1, PERM4x64(0, 2, 0, 2));
                            storea_si128(pdst, si128(res1));
                        }
                        else {
                            res1 = _mm256_add_epi32(res1, _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS)));
                            res2 = _mm256_add_epi32(res2, _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS)));
                            res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                            res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                            res1 = _mm256_packs_epi32(res1, res2);  // 16w: 0..7 | 8..15

                            const int shift = ROUND_POST_COMPOUND;
                            const __m256i offset = _mm256_set1_epi16(OFFSET_POST_COMPOUND);
                            res1 = _mm256_add_epi16(res1, offset);
                            res1 = _mm256_add_epi16(res1, loada_si256(pref));
                            res1 = _mm256_srai_epi16(res1, shift);
                            res1 = _mm256_packus_epi16(res1, res1); // 32w: 0..7 0..7 | 8..15 8..15
                            res1 = _mm256_permute4x64_epi64(res1, PERM4x64(0, 2, 0, 2));
                            storea_si128(pdst, si128(res1));
                        }
                    }
                    else {
                        const __m256i offset = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));
                        res1 = _mm256_add_epi32(res1, offset);
                        res2 = _mm256_add_epi32(res2, offset);
                        res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                        res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                        res1 = _mm256_packs_epi32(res1, res2);
                        storea_si256(pdst, res1);
                    }

                    pdst += pitchDst;
                    ptmp += pitchTmp;
                    if (avg == do_avg)
                        pref += pitchRef0;
                    a = b; b = c; c = d; d = e; e = f; f = g; g = h;
                }
            }
        }

        template <int avg, typename TDst>
        void interp_nv12_w4_both(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int height)
        {
            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt); // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_nv12_horz1);
            __m128i shuftab2 = loada_si128(shuftab_nv12_horz2);

            const int pitchTmp = 8;
            ALIGN_DECL(32) uint16_t tmpBuf[8 * (2 * 8 + 8)]; // max height is 2 * width + 8 pixels
            uint16_t *tmp = tmpBuf;
            src -= pitchSrc * 3 + 6;

            for (int32_t y = 0; y < height + 7; y++) {
                __m128i src1, src2, src3, a, b, c, d, res;
                src1 = loadu_si128(src);                // 16b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7
                src3 = loadu_si128(src + 16);           // 16b: u8 v8 u9 v9 uA vA ** ** ** ** ** ** ** ** ** **
                src2 = _mm_alignr_epi8(src3, src1, 8);  // 16b: u4 v4 u5 v5 u6 v6 u7 v7 u8 v8 u9 v9 uA vA ** **
                a = _mm_shuffle_epi8(src1, shuftab1);   // 16b: u0 u1 v0 v1 u1 u2 v1 v2 u2 u3 v2 v3 u3 u4 v3 v4
                b = _mm_shuffle_epi8(src1, shuftab2);   // 16b: u2 u3 v2 v3 u3 u4 v3 v4 u4 u5 v4 v5 u5 u6 v5 v6
                c = _mm_shuffle_epi8(src2, shuftab1);   // 16b: u4 u5 v4 v5 u5 u6 v5 v6 u6 u7 v6 v7 u7 u8 v7 v8
                d = _mm_shuffle_epi8(src2, shuftab2);   // 16b: u6 u7 v6 v7 u7 u8 v7 v8 u8 u9 v8 v9 u9 uA v9 vA

                a = _mm_maddubs_epi16(a, filt01);
                b = _mm_maddubs_epi16(b, filt23);
                c = _mm_maddubs_epi16(c, filt45);
                d = _mm_maddubs_epi16(d, filt67);
                res = _mm_add_epi16(_mm_add_epi16(a, b), _mm_add_epi16(c, d)); // overflow is ok
                res = _mm_add_epi16(res, _mm_set1_epi16(OFFSET_STAGE0 + ADDITIONAL_OFFSET_STAGE0));  // now we are safe inside u16 range
                res = _mm_srli_epi16(res, ROUND_STAGE0);
                storea_si128(tmp, res);

                src += pitchSrc;
                tmp += pitchTmp;
            }


            filt = loada_si128(fy);                                           // 8w: 0 1 2 3 4 5 6 7
            __m128i vfilt01 = _mm_broadcastd_epi32(filt);                     // 8w: (0 1) x 4
            __m128i vfilt23 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt, 4));  // 8w: (2 3) x 4
            __m128i vfilt45 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt, 8));  // 8w: (4 5) x 4
            __m128i vfilt67 = _mm_broadcastd_epi32(_mm_bsrli_si128(filt, 12)); // 8w: (6 7) x 4

            tmp = tmpBuf;
            __m128i a = loada_si128(tmp);                   // 8w: A0 .. A7
            __m128i b = loada_si128(tmp + pitchTmp);        // 8w: B0 .. B7
            __m128i c = loada_si128(tmp + 2 * pitchTmp);    // 8w: C0 .. C7
            __m128i d = loada_si128(tmp + 3 * pitchTmp);    // 8w: D0 .. D7
            __m128i e = loada_si128(tmp + 4 * pitchTmp);    // 8w: E0 .. E7
            __m128i f = loada_si128(tmp + 5 * pitchTmp);    // 8w: F0 .. F7
            __m128i g = loada_si128(tmp + 6 * pitchTmp);    // 8w: G0 .. G7
            __m128i h, ab, cd, ef, gh, res1, res2;

            for (int y = 0; y < height; y++) {
                h = loada_si128(tmp + 7 * pitchTmp);        // 8w: H0 .. H7

                ab = _mm_unpacklo_epi16(a, b);           // 8w: A0 B0 .. A3 B3
                cd = _mm_unpacklo_epi16(c, d);           // 8w: C0 D0 .. C3 D3
                ef = _mm_unpacklo_epi16(e, f);           // 8w: E0 F0 .. E3 F3
                gh = _mm_unpacklo_epi16(g, h);           // 8w: G0 H0 .. G3 H3
                ab = _mm_madd_epi16(ab, vfilt01);
                cd = _mm_madd_epi16(cd, vfilt23);
                ef = _mm_madd_epi16(ef, vfilt45);
                gh = _mm_madd_epi16(gh, vfilt67);
                res1 = _mm_add_epi32(_mm_add_epi32(ab, cd), _mm_add_epi32(ef, gh)); // 4d: 0..3

                ab = _mm_unpackhi_epi16(a, b);           // 8w: A4 B4 .. A7 B7
                cd = _mm_unpackhi_epi16(c, d);           // 8w: C4 D4 .. C7 D7
                ef = _mm_unpackhi_epi16(e, f);           // 8w: E4 F4 .. E7 F7
                gh = _mm_unpackhi_epi16(g, h);           // 8w: G4 H4 .. G7 H7
                ab = _mm_madd_epi16(ab, vfilt01);
                cd = _mm_madd_epi16(cd, vfilt23);
                ef = _mm_madd_epi16(ef, vfilt45);
                gh = _mm_madd_epi16(gh, vfilt67);
                res2 = _mm_add_epi32(_mm_add_epi32(ab, cd), _mm_add_epi32(ef, gh)); // 4d: 4..7

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        const __m128i offset = _mm_set1_epi32(1024 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                        res1 = _mm_add_epi32(res1, offset);  // now we are back in signed range
                        res2 = _mm_add_epi32(res2, offset);  // now we are back in signed range
                        res1 = _mm_srai_epi32(res1, 2 * FILTER_BITS - ROUND_STAGE0);
                        res2 = _mm_srai_epi32(res2, 2 * FILTER_BITS - ROUND_STAGE0);
                        res1 = _mm_packus_epi32(res1, res2);
                    }
                    else {
                        const __m128i offset1 = _mm_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                        res1 = _mm_add_epi32(res1, offset1);  // now we are back in signed range
                        res2 = _mm_add_epi32(res2, offset1);  // now we are back in signed range
                        res1 = _mm_srai_epi32(res1, ROUND_PRE_COMPOUND);
                        res2 = _mm_srai_epi32(res2, ROUND_PRE_COMPOUND);
                        res1 = _mm_packs_epi32(res1, res2);

                        const int shift = ROUND_POST_COMPOUND;
                        const __m128i offset = _mm_set1_epi16(1 << shift >> 1);
                        res1 = _mm_add_epi16(res1, offset);
                        res1 = _mm_add_epi16(res1, loada_si128(ref0));
                        res1 = _mm_srai_epi16(res1, shift);
                    }
                    res1 = _mm_packus_epi16(res1, res1);
                    storel_epi64(dst, res1);
                }
                else {
                    const __m128i offset = _mm_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // to compensate for stage0 offset
                    res1 = _mm_add_epi32(res1, offset);  // now we are back in signed range
                    res2 = _mm_add_epi32(res2, offset);  // now we are back in signed range
                    res1 = _mm_srai_epi32(res1, ROUND_PRE_COMPOUND);
                    res2 = _mm_srai_epi32(res2, ROUND_PRE_COMPOUND);
                    res1 = _mm_packs_epi32(res1, res2);
                    storea_si128(dst, res1);
                }

                dst += pitchDst;
                tmp += pitchTmp;
                if (avg == do_avg)
                    ref0 += pitchRef0;
                a = b; b = c; c = d; d = e; e = f; f = g; g = h;
            }
        }
        template <int avg, typename TDst>
        void interp_nv12_w8_both(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int height)
        {
            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m128i filt01 = _mm_broadcastw_epi16(filt);                     // 16b: (0 1) x8
            __m128i filt23 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 16b: (2 3) x8
            __m128i filt45 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 16b: (4 5) x8
            __m128i filt67 = _mm_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 16b: (6 7) x8
            __m128i shuftab1 = loada_si128(shuftab_nv12_horz1);
            __m128i shuftab2 = loada_si128(shuftab_nv12_horz2);

            const int pitchTmp = 16;
            ALIGN_DECL(32) uint16_t tmpBuf[16 * (2 * 8 + 7)]; // max height is 2 * width + 8 pixels
            uint16_t *tmp = tmpBuf;
            src -= pitchSrc * 3 + 6;

            for (int32_t y = 0; y < height + 7; y++) {
                __m128i src1, src2, src3, a, b, c, d, e, f, a2, b2, c2, d2;
                src1 = loadu_si128(src);                // 16b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7
                src3 = loadu_si128(src + 16);           // 16b: u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF
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

                a = _mm_add_epi16(a, c);
                b = _mm_add_epi16(b, d);
                a2 = _mm_add_epi16(a2, c2);
                b2 = _mm_add_epi16(b2, d2);

                a = _mm_add_epi16(a, b);
                a2 = _mm_add_epi16(a2, b2);
                a = _mm_add_epi16(a, _mm_set1_epi16(OFFSET_STAGE0 + ADDITIONAL_OFFSET_STAGE0));
                a2 = _mm_add_epi16(a2, _mm_set1_epi16(OFFSET_STAGE0 + ADDITIONAL_OFFSET_STAGE0));
                a = _mm_srli_epi16(a, ROUND_STAGE0);  // 8w: u0 v0 u1 v1 u2 v2 u3 v3
                a2 = _mm_srli_epi16(a2, ROUND_STAGE0);  // 8w: u4 v4 u5 v5 u6 v6 u7 v7
                storea_si128(tmp + 0, a);
                storea_si128(tmp + 8, a2);

                src += pitchSrc;
                tmp += pitchTmp;
            }

            filt = loada_si128(fy);                                              // 8w: 0 1 2 3 4 5 6 7
            __m256i vfilt01 = _mm256_broadcastd_epi32(filt);                     // 16w: (0 1) x 8
            __m256i vfilt23 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 4));  // 16w: (2 3) x 8
            __m256i vfilt45 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 8));  // 16w: (4 5) x 8
            __m256i vfilt67 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 12)); // 16w: (6 7) x 8

            tmp = tmpBuf;
            __m256i a = loada_si256(tmp + 0 * pitchTmp);
            __m256i b = loada_si256(tmp + 1 * pitchTmp);
            __m256i c = loada_si256(tmp + 2 * pitchTmp);
            __m256i d = loada_si256(tmp + 3 * pitchTmp);
            __m256i e = loada_si256(tmp + 4 * pitchTmp);
            __m256i f = loada_si256(tmp + 5 * pitchTmp);
            __m256i g = loada_si256(tmp + 6 * pitchTmp);
            __m256i h, ab, cd, ef, gh, res1, res2;

            for (int32_t y = 0; y < height; y++) {
                h = loada_si256(tmp + 7 * pitchTmp);
                ab = _mm256_unpacklo_epi16(a, b);   // 16w: A0 B0 A1 B1 A2 B2 A3 B3 | A8 B8 A9 B9 Aa Ba Ab Bb
                cd = _mm256_unpacklo_epi16(c, d);
                ef = _mm256_unpacklo_epi16(e, f);
                gh = _mm256_unpacklo_epi16(g, h);
                ab = _mm256_madd_epi16(ab, vfilt01);
                cd = _mm256_madd_epi16(cd, vfilt23);
                ef = _mm256_madd_epi16(ef, vfilt45);
                gh = _mm256_madd_epi16(gh, vfilt67);
                res1 = _mm256_add_epi32(_mm256_add_epi32(ab, cd), _mm256_add_epi32(ef, gh));
                ab = _mm256_unpackhi_epi16(a, b);   // 16w: A4 B4 A5 B5 A6 B6 A7 B7 | Ac Bc Ad Bd Ae Be Af Bf
                cd = _mm256_unpackhi_epi16(c, d);
                ef = _mm256_unpackhi_epi16(e, f);
                gh = _mm256_unpackhi_epi16(g, h);
                ab = _mm256_madd_epi16(ab, vfilt01);
                cd = _mm256_madd_epi16(cd, vfilt23);
                ef = _mm256_madd_epi16(ef, vfilt45);
                gh = _mm256_madd_epi16(gh, vfilt67);
                res2 = _mm256_add_epi32(_mm256_add_epi32(ab, cd), _mm256_add_epi32(ef, gh));

                if (std::is_same<TDst, uint8_t>::value) {
                    if (avg == no_avg) {
                        const __m256i offset = _mm256_set1_epi32(1024 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                        res1 = _mm256_add_epi32(res1, offset);  // now we are back in signed range
                        res2 = _mm256_add_epi32(res2, offset);  // now we are back in signed range
                        res1 = _mm256_srai_epi32(res1, ROUND_STAGE1);
                        res2 = _mm256_srai_epi32(res2, ROUND_STAGE1);
                        res1 = _mm256_packus_epi32(res1, res2); // 16w: 0..15
                    }
                    else {
                        const __m256i offset1 = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                        res1 = _mm256_add_epi32(res1, offset1);  // now we are back in signed range  // 4d: 0 1 2 3
                        res2 = _mm256_add_epi32(res2, offset1);  // now we are back in signed range  // 4d: 4 5 6 7
                        res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                        res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                        res1 = _mm256_packs_epi32(res1, res2);

                        const int shift = ROUND_POST_COMPOUND;
                        const __m256i offset = _mm256_set1_epi16(OFFSET_POST_COMPOUND);
                        res1 = _mm256_add_epi16(res1, offset);
                        res1 = _mm256_add_epi16(res1, loada_si256(ref0));
                        res1 = _mm256_srai_epi16(res1, shift);
                    }
                    res1 = _mm256_packus_epi16(res1, res1); // 32b: 0..7 0..7 | 8..15 8..15
                    res1 = _mm256_permute4x64_epi64(res1, PERM4x64(0, 2, 0, 2));
                    storea_si128(dst, si128(res1));
                }
                else {
                    const __m256i offset = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // to compensate for stage0 offset
                    res1 = _mm256_add_epi32(res1, offset);  // now we are back in signed range
                    res2 = _mm256_add_epi32(res2, offset);  // now we are back in signed range
                    res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                    res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                    res1 = _mm256_packs_epi32(res1, res2);
                    storea_si256(dst, res1);
                }

                dst += pitchDst;
                tmp += pitchTmp;
                if (avg == do_avg)
                    ref0 += pitchRef0;
                a = b; b = c; c = d; d = e; e = f; f = g; g = h;
            }
        }
        template <int avg, int w16, typename TDst>
        void interp_nv12_w16_both(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, TDst *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int height)
        {
            __m128i filt = loada_si128(fx);     // 8w: 0 1 2 3 4 5 6 7
            filt = _mm_packs_epi16(filt, filt); // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
            __m256i filt01 = _mm256_broadcastw_epi16(filt); // 32b: (0 1) x16
            __m256i filt23 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 2));  // 32b: (2 3) x16
            __m256i filt45 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 4));  // 32b: (4 5) x16
            __m256i filt67 = _mm256_broadcastw_epi16(_mm_bsrli_si128(filt, 6));  // 32b: (6 7) x16
            __m256i shuftab1 = loada_si256(shuftab_nv12_horz1);
            __m256i shuftab2 = loada_si256(shuftab_nv12_horz2);

            const int pitchTmp = 2 * w16;
            const int max_height = std::min(MAX_BLOCK_SIZE, 2 * w16);
            ALIGN_DECL(32) uint16_t tmpBuf[2 * w16 * (max_height + 7)]; // max height is 2 * width + 7 pixels
            uint16_t *tmp = tmpBuf;
            src -= pitchSrc * 3 + 6;

            for (int32_t y = 0; y < height + 7; y++) {
                for (int32_t x = 0; x < 2 * w16; x += 32) {
                    __m256i s, a, b, c, d, e, f, a1, b1, c1, d1;
                    s = loadu_si256(src + x + 0);           // 32b: u0 v0 u1 v1 u2 v2 u3 v3 u4 v4 u5 v5 u6 v6 u7 v7 | u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF
                    a = _mm256_shuffle_epi8(s, shuftab1);   // 32b: u0 u1 v0 v1 u1 u2 v1 v2 u2 u3 v2 v3 u3 u4 v3 v4 | u8 u9 v8 v9 u9 uA v9 vA uA uB vA vB uB uC vB vC
                    b = _mm256_shuffle_epi8(s, shuftab2);   // 32b: u2 u3 v2 v3 u3 u4 v3 v4 u4 u5 v4 v5 u5 u6 v5 v6 | uA uB vA vB uB uC vB vC uC uD vC vD uD uE vD vE
                    s = loadu_si256(src + x + 8);           // 32b: u4 v4 u5 v5 u6 v6 u7 v7 u8 v8 u9 v9 uA vA uB vB | uC vC uD vD uE vE uF vF uG vG uH vH uI vI uK vK
                    c = _mm256_shuffle_epi8(s, shuftab1);   // 32b: u4 u5 v4 v5 u5 u6 v5 v6 u6 u7 v6 v7 u7 u8 v7 v8 | uC uD vC vD uD uE vD vE uE uF vE vF uF uG vF vG
                    d = _mm256_shuffle_epi8(s, shuftab2);   // 32b: u6 u7 v6 v7 u7 u8 v7 v8 u8 u9 v8 v9 u9 uA v9 vA | uE uF vE vF uF uG vF vG uG uH vG vH uH uI vH vI
                    s = loadu_si256(src + x + 16);          // 32b: u8 v8 u9 v9 uA vA uB vB uC vC uD vD uE vE uF vF | uG vG uH vH uI vI uK vK uL vL uM vM uN vN ** **
                    e = _mm256_shuffle_epi8(s, shuftab1);
                    f = _mm256_shuffle_epi8(s, shuftab2);

                    a1 = _mm256_maddubs_epi16(a, filt01);
                    b1 = _mm256_maddubs_epi16(b, filt23);
                    c1 = _mm256_maddubs_epi16(c, filt45);
                    d1 = _mm256_maddubs_epi16(d, filt67);
                    c = _mm256_maddubs_epi16(c, filt01);
                    d = _mm256_maddubs_epi16(d, filt23);
                    e = _mm256_maddubs_epi16(e, filt45);
                    f = _mm256_maddubs_epi16(f, filt67);
                    a1 = _mm256_add_epi16(_mm256_add_epi16(a1, c1), _mm256_add_epi16(b1, d1));
                    c = _mm256_add_epi16(_mm256_add_epi16(c, e), _mm256_add_epi16(d, f));

                    a1 = _mm256_add_epi16(a1, _mm256_set1_epi16(OFFSET_STAGE0 + ADDITIONAL_OFFSET_STAGE0));
                    c = _mm256_add_epi16(c, _mm256_set1_epi16(OFFSET_STAGE0 + ADDITIONAL_OFFSET_STAGE0));
                    a1 = _mm256_srli_epi16(a1, ROUND_STAGE0);
                    c = _mm256_srli_epi16(c, ROUND_STAGE0);
                    storea_si128(tmp + x + 0, si128_lo(a1));
                    storea_si128(tmp + x + 16, si128_hi(a1));
                    storea_si128(tmp + x + 8, si128_lo(c));
                    storea_si128(tmp + x + 24, si128_hi(c));
                }

                src += pitchSrc;
                tmp += pitchTmp;
            }


            filt = loada_si128(fy);                                              // 8w: 0 1 2 3 4 5 6 7
            __m256i vfilt01 = _mm256_broadcastd_epi32(filt);                     // 16w: (0 1) x 8
            __m256i vfilt23 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 4));  // 16w: (2 3) x 8
            __m256i vfilt45 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 8));  // 16w: (4 5) x 8
            __m256i vfilt67 = _mm256_broadcastd_epi32(_mm_bsrli_si128(filt, 12)); // 16w: (6 7) x 8

            for (int x = 0; x < w16 * 2; x += 16) {
                const uint16_t *ptmp = tmpBuf + x;
                const int16_t *pref = ref0 + x;
                TDst *pdst = dst + x;

                __m256i a = loada_si256(ptmp + 0 * pitchTmp);
                __m256i b = loada_si256(ptmp + 1 * pitchTmp);
                __m256i c = loada_si256(ptmp + 2 * pitchTmp);
                __m256i d = loada_si256(ptmp + 3 * pitchTmp);
                __m256i e = loada_si256(ptmp + 4 * pitchTmp);
                __m256i f = loada_si256(ptmp + 5 * pitchTmp);
                __m256i g = loada_si256(ptmp + 6 * pitchTmp);
                __m256i h, ab, cd, ef, gh, res1, res2;

                for (int32_t y = 0; y < height; y++) {
                    h = loada_si256(ptmp + 7 * pitchTmp);
                    ab = _mm256_unpacklo_epi16(a, b);   // 16w: A0 B0 A1 B1 A2 B2 A3 B3 | A8 B8 A9 B9 Aa Ba Ab Bb
                    cd = _mm256_unpacklo_epi16(c, d);
                    ef = _mm256_unpacklo_epi16(e, f);
                    gh = _mm256_unpacklo_epi16(g, h);
                    ab = _mm256_madd_epi16(ab, vfilt01);
                    cd = _mm256_madd_epi16(cd, vfilt23);
                    ef = _mm256_madd_epi16(ef, vfilt45);
                    gh = _mm256_madd_epi16(gh, vfilt67);
                    res1 = _mm256_add_epi32(_mm256_add_epi32(ab, cd), _mm256_add_epi32(ef, gh));
                    ab = _mm256_unpackhi_epi16(a, b);   // 16w: A4 B4 A5 B5 A6 B6 A7 B7 | Ac Bc Ad Bd Ae Be Af Bf
                    cd = _mm256_unpackhi_epi16(c, d);
                    ef = _mm256_unpackhi_epi16(e, f);
                    gh = _mm256_unpackhi_epi16(g, h);
                    ab = _mm256_madd_epi16(ab, vfilt01);
                    cd = _mm256_madd_epi16(cd, vfilt23);
                    ef = _mm256_madd_epi16(ef, vfilt45);
                    gh = _mm256_madd_epi16(gh, vfilt67);
                    res2 = _mm256_add_epi32(_mm256_add_epi32(ab, cd), _mm256_add_epi32(ef, gh));

                    if (std::is_same<TDst, uint8_t>::value) {
                        if (avg == no_avg) {
                            const __m256i offset = _mm256_set1_epi32(1024 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                            res1 = _mm256_add_epi32(res1, offset);  // now we are back in signed range
                            res2 = _mm256_add_epi32(res2, offset);  // now we are back in signed range
                            res1 = _mm256_srai_epi32(res1, ROUND_STAGE1);
                            res2 = _mm256_srai_epi32(res2, ROUND_STAGE1);
                            res1 = _mm256_packus_epi32(res1, res2); // 16w: 0..15
                        }
                        else {
                            const __m256i offset1 = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                            res1 = _mm256_add_epi32(res1, offset1);  // now we are back in signed range  // 4d: 0 1 2 3
                            res2 = _mm256_add_epi32(res2, offset1);  // now we are back in signed range  // 4d: 4 5 6 7
                            res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                            res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                            res1 = _mm256_packs_epi32(res1, res2);

                            const int shift = ROUND_POST_COMPOUND;
                            const __m256i offset = _mm256_set1_epi16(OFFSET_POST_COMPOUND);
                            res1 = _mm256_add_epi16(res1, offset);
                            res1 = _mm256_add_epi16(res1, loada_si256(pref));
                            res1 = _mm256_srai_epi16(res1, shift);
                        }
                        res1 = _mm256_packus_epi16(res1, res1); // 32b: 0..7 0..7 | 8..15 8..15
                        res1 = _mm256_permute4x64_epi64(res1, PERM4x64(0, 2, 0, 2));
                        storea_si128(pdst, si128(res1));
                    }
                    else {
                        const __m256i offset = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // to compensate for stage0 offset
                        res1 = _mm256_add_epi32(res1, offset);  // now we are back in signed range
                        res2 = _mm256_add_epi32(res2, offset);  // now we are back in signed range
                        res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                        res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                        res1 = _mm256_packs_epi32(res1, res2);
                        storea_si256(pdst, res1);
                    }

                    pdst += pitchDst;
                    ptmp += pitchTmp;
                    if (avg == do_avg)
                        pref += pitchRef0;
                    a = b; b = c; c = d; d = e; e = f; f = g; g = h;
                }
            }
        }
    };  // namespace


    // single ref, luma
    template <int w, int horz, int vert>
    void interp_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    template <> void interp_av1_avx2<4, no_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        for (int y = 0; y < h; y += 4) {
            *(uint32_t *)(dst + 0 * pitchDst) = *(const uint32_t *)(src + 0 * pitchSrc);
            *(uint32_t *)(dst + 1 * pitchDst) = *(const uint32_t *)(src + 1 * pitchSrc);
            *(uint32_t *)(dst + 2 * pitchDst) = *(const uint32_t *)(src + 2 * pitchSrc);
            *(uint32_t *)(dst + 3 * pitchDst) = *(const uint32_t *)(src + 3 * pitchSrc);
            src += 4 * pitchSrc;
            dst += 4 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<8, no_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        for (int y = 0; y < h; y += 4) {
            *(uint64_t *)(dst + 0 * pitchDst) = *(const uint64_t *)(src + 0 * pitchSrc);
            *(uint64_t *)(dst + 1 * pitchDst) = *(const uint64_t *)(src + 1 * pitchSrc);
            *(uint64_t *)(dst + 2 * pitchDst) = *(const uint64_t *)(src + 2 * pitchSrc);
            *(uint64_t *)(dst + 3 * pitchDst) = *(const uint64_t *)(src + 3 * pitchSrc);
            src += 4 * pitchSrc;
            dst += 4 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<16, no_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        for (int y = 0; y < h; y += 4) {
            *(__m128i *)(dst + 0 * pitchDst) = *(const __m128i *)(src + 0 * pitchSrc);
            *(__m128i *)(dst + 1 * pitchDst) = *(const __m128i *)(src + 1 * pitchSrc);
            *(__m128i *)(dst + 2 * pitchDst) = *(const __m128i *)(src + 2 * pitchSrc);
            *(__m128i *)(dst + 3 * pitchDst) = *(const __m128i *)(src + 3 * pitchSrc);
            src += 4 * pitchSrc;
            dst += 4 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<32, no_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        for (int y = 0; y < h; y += 4) {
            *(__m256i *)(dst + 0 * pitchDst) = *(const __m256i *)(src + 0 * pitchSrc);
            *(__m256i *)(dst + 1 * pitchDst) = *(const __m256i *)(src + 1 * pitchSrc);
            *(__m256i *)(dst + 2 * pitchDst) = *(const __m256i *)(src + 2 * pitchSrc);
            *(__m256i *)(dst + 3 * pitchDst) = *(const __m256i *)(src + 3 * pitchSrc);
            src += 4 * pitchSrc;
            dst += 4 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<64, no_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        for (int y = 0; y < h; y += 2) {
            *(__m256i *)(dst + 0 * pitchDst + 0) = *(const __m256i *)(src + 0 * pitchSrc + 0);
            *(__m256i *)(dst + 0 * pitchDst + 32) = *(const __m256i *)(src + 0 * pitchSrc + 32);
            *(__m256i *)(dst + 1 * pitchDst + 0) = *(const __m256i *)(src + 1 * pitchSrc + 0);
            *(__m256i *)(dst + 1 * pitchDst + 32) = *(const __m256i *)(src + 1 * pitchSrc + 32);
            src += 2 * pitchSrc;
            dst += 2 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<96, no_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        for (int y = 0; y < h; y += 1) {
            *(__m256i *)(dst + 0) = *(const __m256i *)(src + 0);
            *(__m256i *)(dst + 32) = *(const __m256i *)(src + 32);
            *(__m256i *)(dst + 64) = *(const __m256i *)(src + 64);
            src += pitchSrc;
            dst += pitchDst;
        }
    }

    template <> void interp_av1_avx2<4, no_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w4_nohorz_dovert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<8, no_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w8_nohorz_dovert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<16, no_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w16_nohorz_dovert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<32, no_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w32_nohorz_dovert<no_avg, 32>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<64, no_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w32_nohorz_dovert<no_avg, 64>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<96, no_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w32_nohorz_dovert<no_avg, 96>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }

    template <> void interp_av1_avx2<4, do_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w4_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<8, do_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w8_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<16, do_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w16_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<32, do_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w32_dohorz_novert<no_avg, 32>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<64, do_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w32_dohorz_novert<no_avg, 64>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<96, do_horz, no_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w32_dohorz_novert<no_avg, 96>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }

    template <> void interp_av1_avx2<4, do_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchTmp = 4;
        ALIGN_DECL(16) int16_t tmpBuf[4 * (8 + SUBPEL_TAPS)];
        int16_t *tmp = tmpBuf;
        src -= ((SUBPEL_TAPS / 2) - 1) * pitchSrc;
        interp_w4_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, tmp, pitchTmp, fx, h + SUBPEL_TAPS);

        tmp += ((SUBPEL_TAPS / 2) - 1) * pitchTmp;
        interp_w4_nohorz_dovert<no_avg>(tmp, pitchTmp, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<8, do_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w8_both<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<16, do_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<no_avg, 16>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<32, do_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<no_avg, 32>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<64, do_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<no_avg, 64>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<96, do_horz, do_vert>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<no_avg, 96>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }


    // first ref (compound), luma
    template <int w, int horz, int vert>
    void interp_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    template <> void interp_av1_avx2<4, no_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        const int sh = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND;
        for (int y = 0; y < h; y += 4) {
            storel_epi64(dst + 0 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadu_si32(src + 0 * pitchSrc)), sh));
            storel_epi64(dst + 1 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadu_si32(src + 1 * pitchSrc)), sh));
            storel_epi64(dst + 2 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadu_si32(src + 2 * pitchSrc)), sh));
            storel_epi64(dst + 3 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadu_si32(src + 3 * pitchSrc)), sh));
            src += 4 * pitchSrc;
            dst += 4 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<8, no_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        const int sh = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND;
        for (int y = 0; y < h; y += 4) {
            storea_si128(dst + 0 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadl_epi64(src + 0 * pitchSrc)), sh));
            storea_si128(dst + 1 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadl_epi64(src + 1 * pitchSrc)), sh));
            storea_si128(dst + 2 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadl_epi64(src + 2 * pitchSrc)), sh));
            storea_si128(dst + 3 * pitchDst, _mm_slli_epi16(_mm_cvtepu8_epi16(loadl_epi64(src + 3 * pitchSrc)), sh));
            src += 4 * pitchSrc;
            dst += 4 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<16, no_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *, int h) {
        interp_w16_nohorz_novert<16>(src, pitchSrc, dst, pitchDst, h);
    }
    template <> void interp_av1_avx2<32, no_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *, int h) {
        interp_w16_nohorz_novert<32>(src, pitchSrc, dst, pitchDst, h);
    }
    template <> void interp_av1_avx2<64, no_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *, int h) {
        interp_w16_nohorz_novert<64>(src, pitchSrc, dst, pitchDst, h);
    }

    template <> void interp_av1_avx2<4, no_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w4_nohorz_dovert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<8, no_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w8_nohorz_dovert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<16, no_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w16_nohorz_dovert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<32, no_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w32_nohorz_dovert<no_avg, 32>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<64, no_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w32_nohorz_dovert<no_avg, 64>(src, pitchSrc, nullptr, 0, dst, pitchDst, fy, h);
    }

    template <> void interp_av1_avx2<4, do_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w4_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<8, do_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w8_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<16, do_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w16_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<32, do_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w32_dohorz_novert<no_avg, 32>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<64, do_horz, no_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w32_dohorz_novert<no_avg, 64>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
    }

    template <> void interp_av1_avx2<4, do_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchTmp = 4;
        ALIGN_DECL(16) int16_t tmpBuf[4 * (8 + SUBPEL_TAPS)];
        int16_t *tmp = tmpBuf;
        src -= ((SUBPEL_TAPS / 2) - 1) * pitchSrc;
        interp_w4_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, tmp, pitchTmp, fx, h + SUBPEL_TAPS);

        tmp += ((SUBPEL_TAPS / 2) - 1) * pitchTmp;
        interp_w4_nohorz_dovert<no_avg>(tmp, pitchTmp, nullptr, 0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<8, do_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w8_both<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<16, do_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<no_avg, 16>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<32, do_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<no_avg, 32>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<64, do_horz, do_vert>(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<no_avg, 64>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
    }


    // second ref (compound), luma
    template <int w, int horz, int vert>
    void interp_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

    template <> void interp_av1_avx2<4, no_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        const int sh = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND;
        __m128i offset = _mm_set1_epi16(1 << sh);
        __m128i s0, s1, r0, r1, d0, d1, d;
        for (int y = 0; y < h; y += 2) {
            r0 = loadl_epi64(ref0);
            r1 = loadl_epi64(ref0 + pitchRef0);
            s0 = _mm_slli_epi16(_mm_cvtepu8_epi16(loadu_si32(src)), sh);
            s1 = _mm_slli_epi16(_mm_cvtepu8_epi16(loadu_si32(src + pitchSrc)), sh);
            d0 = _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(s0, r0), offset), sh + 1);
            d1 = _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(s1, r1), offset), sh + 1);
            d = _mm_packus_epi16(d0, d1); // abcdefgh
            storel_si32(dst + 0 * pitchDst, d);
            storel_si32(dst + 1 * pitchDst, _mm_srli_si128(d, 8));

            src += 2 * pitchSrc;
            ref0 += 2 * pitchRef0;
            dst += 2 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<8, no_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        (void)fx, (void)fy;
        const int shift = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND;
        const __m256i offset = _mm256_set1_epi16(1 << shift);
        for (int y = 0; y < h; y += 4) {
            __m256i d = average_8u_16s(loadu2_epi64(src + 0 * pitchSrc, src + 1 * pitchSrc),
                loadu2_epi64(src + 2 * pitchSrc, src + 3 * pitchSrc),
                loada2_m128i(ref0 + 0 * pitchRef0, ref0 + 1 * pitchRef0),
                loada2_m128i(ref0 + 2 * pitchRef0, ref0 + 3 * pitchRef0),
                offset, shift);
            storel_epi64(dst + 0 * pitchDst, si128(d));
            storeh_epi64(dst + 1 * pitchDst, si128(d));
            storel_epi64(dst + 2 * pitchDst, si128_hi(d));
            storeh_epi64(dst + 3 * pitchDst, si128_hi(d));
            src += 4 * pitchSrc;
            ref0 += 4 * pitchRef0;
            dst += 4 * pitchDst;
        }
    }
    template <> void interp_av1_avx2<16, no_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *, int h) {
        interp_2nd_ref_w16_nohorz_novert(src, pitchSrc, (int16_t *)ref0, pitchRef0, dst, pitchDst, h);
    }
    template <> void interp_av1_avx2<32, no_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *, int h) {
        interp_2nd_ref_w32_nohorz_novert<32>(src, pitchSrc, (int16_t *)ref0, pitchRef0, dst, pitchDst, h);
    }
    template <> void interp_av1_avx2<64, no_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *, int h) {
        interp_2nd_ref_w32_nohorz_novert<64>(src, pitchSrc, (int16_t *)ref0, pitchRef0, dst, pitchDst, h);
    }

    template <> void interp_av1_avx2<4, no_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w4_nohorz_dovert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<8, no_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w8_nohorz_dovert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<16, no_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w16_nohorz_dovert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<32, no_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w32_nohorz_dovert<do_avg, 32>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<64, no_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *, const int16_t *fy, int h) {
        interp_w32_nohorz_dovert<do_avg, 64>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fy, h);
    }

    template <> void interp_av1_avx2<4, do_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w4_dohorz_novert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<8, do_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w8_dohorz_novert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<16, do_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w16_dohorz_novert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<32, do_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w32_dohorz_novert<do_avg, 32>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
    }
    template <> void interp_av1_avx2<64, do_horz, no_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *, int h) {
        interp_w32_dohorz_novert<do_avg, 64>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
    }

    template <> void interp_av1_avx2<4, do_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        const int pitchTmp = 4;
        ALIGN_DECL(16) int16_t tmpBuf[4 * (8 + SUBPEL_TAPS)];
        int16_t *tmp = tmpBuf;
        src -= ((SUBPEL_TAPS / 2) - 1) * pitchSrc;
        interp_w4_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, tmp, pitchTmp, fx, h + SUBPEL_TAPS);

        tmp += ((SUBPEL_TAPS / 2) - 1) * pitchTmp;
        interp_w4_nohorz_dovert<do_avg>(tmp, pitchTmp, ref0, pitchRef0, dst, pitchDst, fy, h);
    }
    template <> void interp_av1_avx2<8, do_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w8_both<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<16, do_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<do_avg, 16>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<32, do_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<do_avg, 32>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
    }
    template <> void interp_av1_avx2<64, do_horz, do_vert>(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        interp_w16_both<do_avg, 64>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
    }

    // single ref, luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_avx2<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // first ref (compound), luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_avx2<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // second ref (compound), luma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h)
    {
        interp_av1_avx2<w, horz, vert>(src, pitchSrc, ref0, IMPLIED_PITCH, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // single ref, chroma
    template <int w, int horz, int vert>
    void interp_nv12_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        if (horz == no_horz)
            interp_av1_avx2<2 * w, horz, vert>(src, pitchSrc, dst, pitchDst, fx, fy, h);
        else if (horz == do_horz && vert == no_vert) {
            if (w == 4) interp_nv12_w4_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
            else if (w == 8) interp_nv12_w8_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
            else             interp_nv12_w16_dohorz_novert<no_avg, w>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
        }
        else if (horz == do_horz && vert == do_vert) {
            if (w == 4) interp_nv12_w4_both<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
            else if (w == 8) interp_nv12_w8_both<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
            else             interp_nv12_w16_both<no_avg, w>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
        }
        else { assert(0); }
    }

    // first ref (compound), chroma
    template <int w, int horz, int vert>
    void interp_nv12_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        if (horz == no_horz)
            return interp_av1_avx2<2 * w, horz, vert>(src, pitchSrc, dst, pitchDst, fx, fy, h);
        else if (horz == do_horz && vert == no_vert) {
            if (w == 4) interp_nv12_w4_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
            else if (w == 8) interp_nv12_w8_dohorz_novert<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
            else             interp_nv12_w16_dohorz_novert<no_avg, w>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, h);
        }
        else if (horz == do_horz && vert == do_vert) {
            if (w == 4) interp_nv12_w4_both<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
            else if (w == 8) interp_nv12_w8_both<no_avg>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
            else             interp_nv12_w16_both<no_avg, w>(src, pitchSrc, nullptr, 0, dst, pitchDst, fx, fy, h);
        }
        else { assert(0); }
    }

    // second ref (compound), chroma
    template <int w, int horz, int vert>
    void interp_nv12_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h)
    {
        if (horz == no_horz)
            return interp_av1_avx2<2 * w, horz, vert>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
        else if (horz == do_horz && vert == no_vert) {
            if (w == 4) interp_nv12_w4_dohorz_novert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
            else if (w == 8) interp_nv12_w8_dohorz_novert<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
            else             interp_nv12_w16_dohorz_novert<do_avg, w>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, h);
        }
        else if (horz == do_horz && vert == do_vert) {
            if (w == 4) interp_nv12_w4_both<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
            else if (w == 8) interp_nv12_w8_both<do_avg>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
            else             interp_nv12_w16_both<do_avg, w>(src, pitchSrc, ref0, pitchRef0, dst, pitchDst, fx, fy, h);
        }
        else { assert(0); }
    }

    // single ref, chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h) {
        interp_nv12_av1_avx2<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // first ref (compound), chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h) {
        interp_nv12_av1_avx2<w, horz, vert>(src, pitchSrc, dst, IMPLIED_PITCH, fx, fy, h);
    }

    // second ref (compound), chroma, dst pitch hardcoded to 64
    template <int w, int horz, int vert>
    void interp_nv12_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h) {
        interp_nv12_av1_avx2<w, horz, vert>(src, pitchSrc, ref0, IMPLIED_PITCH, dst, IMPLIED_PITCH, fx, fy, h);
    }


    // Instatiate interp functions for:
    //   single ref / first ref of compound / second ref of compound
    //   luma / chroma
    //   dst pitch / dst pitch-less
    //   no_horz / do_horz
    //   no_vert / do_vert
    //   width 4 / 8 / 16 / 32 / 64 / 96

    typedef void(*interp_av1_single_ref_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_av1_first_ref_fptr_t)(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_av1_second_ref_fptr_t)(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_pitch64_av1_single_ref_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_pitch64_av1_first_ref_fptr_t)(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void(*interp_pitch64_av1_second_ref_fptr_t)(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);

#define INSTATIATE4(FUNC_NAME)                                                                      \
    { { FUNC_NAME< 4, 0, 0>, FUNC_NAME< 4, 0, 1> }, { FUNC_NAME< 4, 1, 0>, FUNC_NAME< 4, 1, 1> } }, \
    { { FUNC_NAME< 8, 0, 0>, FUNC_NAME< 8, 0, 1> }, { FUNC_NAME< 8, 1, 0>, FUNC_NAME< 8, 1, 1> } }, \
    { { FUNC_NAME<16, 0, 0>, FUNC_NAME<16, 0, 1> }, { FUNC_NAME<16, 1, 0>, FUNC_NAME<16, 1, 1> } }, \
    { { FUNC_NAME<32, 0, 0>, FUNC_NAME<32, 0, 1> }, { FUNC_NAME<32, 1, 0>, FUNC_NAME<32, 1, 1> } }

#define INSTATIATE5(FUNC_NAME)                                                                      \
    INSTATIATE4(FUNC_NAME),                                                                         \
    { { FUNC_NAME<64, 0, 0>, FUNC_NAME<64, 0, 1> }, { FUNC_NAME<64, 1, 0>, FUNC_NAME<64, 1, 1> } }

#define INSTATIATE6(FUNC_NAME)                                                                      \
    INSTATIATE5(FUNC_NAME),                                                                         \
    { { FUNC_NAME<96, 0, 0>, FUNC_NAME<96, 0, 1> }, { FUNC_NAME<96, 1, 0>, FUNC_NAME<96, 1, 1> } }

    interp_av1_single_ref_fptr_t interp_av1_single_ref_fptr_arr_avx2[6][2][2] = { INSTATIATE6(interp_av1_avx2) };
    interp_av1_first_ref_fptr_t  interp_av1_first_ref_fptr_arr_avx2[5][2][2] = { INSTATIATE5(interp_av1_avx2) };
    interp_av1_second_ref_fptr_t interp_av1_second_ref_fptr_arr_avx2[5][2][2] = { INSTATIATE5(interp_av1_avx2) };

    interp_av1_single_ref_fptr_t interp_nv12_av1_single_ref_fptr_arr_avx2[4][2][2] = { INSTATIATE4(interp_nv12_av1_avx2) };
    interp_av1_first_ref_fptr_t  interp_nv12_av1_first_ref_fptr_arr_avx2[4][2][2] = { INSTATIATE4(interp_nv12_av1_avx2) };
    interp_av1_second_ref_fptr_t interp_nv12_av1_second_ref_fptr_arr_avx2[4][2][2] = { INSTATIATE4(interp_nv12_av1_avx2) };

    interp_pitch64_av1_single_ref_fptr_t interp_pitch64_av1_single_ref_fptr_arr_avx2[5][2][2] = { INSTATIATE5(interp_pitch64_av1_avx2) };
    interp_pitch64_av1_first_ref_fptr_t  interp_pitch64_av1_first_ref_fptr_arr_avx2[5][2][2] = { INSTATIATE5(interp_pitch64_av1_avx2) };
    interp_pitch64_av1_second_ref_fptr_t interp_pitch64_av1_second_ref_fptr_arr_avx2[5][2][2] = { INSTATIATE5(interp_pitch64_av1_avx2) };

    interp_pitch64_av1_single_ref_fptr_t interp_nv12_pitch64_av1_single_ref_fptr_arr_avx2[4][2][2] = { INSTATIATE4(interp_nv12_pitch64_av1_avx2) };
    interp_pitch64_av1_first_ref_fptr_t  interp_nv12_pitch64_av1_first_ref_fptr_arr_avx2[4][2][2] = { INSTATIATE4(interp_nv12_pitch64_av1_avx2) };
    interp_pitch64_av1_second_ref_fptr_t interp_nv12_pitch64_av1_second_ref_fptr_arr_avx2[4][2][2] = { INSTATIATE4(interp_nv12_pitch64_av1_avx2) };

}  // namespace AV1PP
