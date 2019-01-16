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
#include <immintrin.h>
#include <xmmintrin.h>
#include "mfx_av1_opts_common.h"

namespace AV1PP
{
    void compute_rscs_avx2(const uint8_t *src, int32_t pitch, int32_t *lcuRs, int32_t *lcuCs, int32_t pitchRsCs, int32_t width, int32_t height)
    {
        assert((size_t(src) & 0xf) == 0);
        assert(width == 64); width;

        __m256i diff;
        __m256i lineL, lineC;

        __m256i lineA0 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+0)));
        __m256i lineA1 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+16)));
        __m256i lineA2 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+32)));
        __m256i lineA3 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src-pitch+48)));
        __m128i lineC128, lineL128;
        for (int32_t y = 0; y < height; y += 4, lcuRs += pitchRsCs, lcuCs += pitchRsCs) {
            __m256i cs0 = _mm256_setzero_si256();
            __m256i cs1 = _mm256_setzero_si256();
            __m256i cs2 = _mm256_setzero_si256();
            __m256i cs3 = _mm256_setzero_si256();
            __m256i rs0 = _mm256_setzero_si256();
            __m256i rs1 = _mm256_setzero_si256();
            __m256i rs2 = _mm256_setzero_si256();
            __m256i rs3 = _mm256_setzero_si256();
            for (int32_t yy = 0; yy < 4; yy++, src += pitch) {
                _mm_prefetch((const char*)(src-16+pitch), _MM_HINT_T0);
                lineL128 = _mm_load_si128((__m128i *)(src-16));
                lineC128 = _mm_load_si128((__m128i *)(src));
                lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
                lineC = _mm256_cvtepu8_epi16(lineC128);
                diff = _mm256_sub_epi16(lineL, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                cs0 = _mm256_add_epi32(cs0, diff);
                diff = _mm256_sub_epi16(lineA0, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                rs0 = _mm256_add_epi32(rs0, diff);
                lineL128 = lineC128;
                lineA0 = lineC;

                lineC128 = _mm_load_si128((__m128i *)(src+16));
                lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
                lineC = _mm256_cvtepu8_epi16(lineC128);
                diff = _mm256_sub_epi16(lineL, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                cs1 = _mm256_add_epi32(cs1, diff);
                diff = _mm256_sub_epi16(lineA1, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                rs1 = _mm256_add_epi32(rs1, diff);
                lineL128 = lineC128;
                lineA1 = lineC;

                lineC128 = _mm_load_si128((__m128i *)(src+32));
                lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
                lineC = _mm256_cvtepu8_epi16(lineC128);
                diff = _mm256_sub_epi16(lineL, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                cs2 = _mm256_add_epi32(cs2, diff);
                diff = _mm256_sub_epi16(lineA2, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                rs2 = _mm256_add_epi32(rs2, diff);
                lineL128 = lineC128;
                lineA2 = lineC;

                lineC128 = _mm_load_si128((__m128i *)(src+48));
                lineL = _mm256_cvtepu8_epi16(_mm_alignr_epi8(lineC128, lineL128, 15));
                lineC = _mm256_cvtepu8_epi16(lineC128);
                diff = _mm256_sub_epi16(lineL, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                cs3 = _mm256_add_epi32(cs3, diff);
                diff = _mm256_sub_epi16(lineA3, lineC);
                diff = _mm256_madd_epi16(diff, diff);
                rs3 = _mm256_add_epi32(rs3, diff);
                lineL128 = lineC128;
                lineA3 = lineC;
            }

            cs0 = _mm256_hadd_epi32(cs0, cs1);
            cs0 = _mm256_permute4x64_epi64(cs0, 0xd8);
            cs0 = _mm256_srli_epi32(cs0, 4);
            _mm256_store_si256((__m256i *)(lcuCs), cs0);

            cs2 = _mm256_hadd_epi32(cs2, cs3);
            cs2 = _mm256_permute4x64_epi64(cs2, 0xd8);
            cs2 = _mm256_srli_epi32(cs2, 4);
            _mm256_store_si256((__m256i *)(lcuCs+8), cs2);

            rs0 = _mm256_hadd_epi32(rs0, rs1);
            rs0 = _mm256_permute4x64_epi64(rs0, 0xd8);
            rs0 = _mm256_srli_epi32(rs0, 4);
            _mm256_store_si256((__m256i *)(lcuRs), rs0);

            rs2 = _mm256_hadd_epi32(rs2, rs3);
            rs2 = _mm256_permute4x64_epi64(rs2, 0xd8);
            rs2 = _mm256_srli_epi32(rs2, 4);
            _mm256_store_si256((__m256i *)(lcuRs+8), rs2);
        }
    }

    void compute_rscs_sse4(const uint16_t *ySrc, int32_t pitchSrc, int32_t *lcuRs, int32_t *lcuCs, int32_t pitchRsCs, int32_t width, int32_t height)
    {
        int32_t i, j, k1, k2;
        int32_t p2 = pitchSrc << 1;
        int32_t p3 = p2 + pitchSrc;
        int32_t p4 = pitchSrc << 2;
        const uint16_t *pSrcG = ySrc - pitchSrc - 1;
        const uint16_t *pSrc;

        __m128i xmm1, xmm2, xmm3, xmm4, xmm6, xmm7, xmm8, xmm9;
        __m128i xmm10, xmm11, xmm12, xmm13, xmm14;

        k1 = 0;
        for(i = 0; i < height; i += 4)
        {
            k2 = k1;
            pSrc = pSrcG;
            for(j = 0; j < width; j += 8)
            {
                xmm7  = _mm_loadu_si128((__m128i *)&pSrc[pitchSrc]);
                xmm9  = _mm_loadu_si128((__m128i *)&pSrc[p2]);
                xmm11 = _mm_loadu_si128((__m128i *)&pSrc[p3]);
                xmm13 = _mm_loadu_si128((__m128i *)&pSrc[p4]);

                xmm6  = _mm_loadu_si128((__m128i *)&pSrc[1]);
                xmm8  = _mm_loadu_si128((__m128i *)&pSrc[pitchSrc+1]);
                xmm10 = _mm_loadu_si128((__m128i *)&pSrc[p2+1]);
                xmm12 = _mm_loadu_si128((__m128i *)&pSrc[p3+1]);
                xmm14 = _mm_loadu_si128((__m128i *)&pSrc[p4+1]);

                // Cs
                xmm1 = _mm_sub_epi16(xmm8, xmm7);
                xmm1 = _mm_madd_epi16(xmm1, xmm1);

                xmm2 = _mm_sub_epi16(xmm10, xmm9);
                xmm2 = _mm_madd_epi16(xmm2, xmm2);
                xmm1 = _mm_add_epi32(xmm1, xmm2);

                xmm2 = _mm_sub_epi16(xmm12, xmm11);
                xmm2 = _mm_madd_epi16(xmm2, xmm2);
                xmm1 = _mm_add_epi32(xmm1, xmm2);

                xmm2 = _mm_sub_epi16(xmm14, xmm13);
                xmm2 = _mm_madd_epi16(xmm2, xmm2);
                xmm1 = _mm_add_epi32(xmm1, xmm2);

                xmm1 = _mm_hadd_epi32(xmm1, xmm1);
                xmm1 = _mm_srli_epi32 (xmm1, 4);
                _mm_storel_epi64((__m128i *)(lcuCs+k2), xmm1);

                // Rs
                xmm3 = _mm_sub_epi16(xmm8, xmm6);
                xmm3 = _mm_madd_epi16(xmm3, xmm3);

                xmm4 = _mm_sub_epi16(xmm10, xmm8);
                xmm4 = _mm_madd_epi16(xmm4, xmm4);
                xmm3 = _mm_add_epi32(xmm3, xmm4);

                xmm4 = _mm_sub_epi16(xmm12, xmm10);
                xmm4 = _mm_madd_epi16(xmm4, xmm4);
                xmm3  = _mm_add_epi32(xmm3, xmm4);

                xmm4 = _mm_sub_epi16(xmm14, xmm12);
                xmm4 = _mm_madd_epi16(xmm4, xmm4);
                xmm3  = _mm_add_epi32(xmm3, xmm4);

                xmm3 = _mm_hadd_epi32(xmm3, xmm3);
                xmm3 = _mm_srli_epi32 (xmm3, 4);
                _mm_storel_epi64((__m128i *)(lcuRs+k2), xmm3);

                pSrc += 8;
                k2 += 2;
            }
            k1 += pitchRsCs;
            pSrcG += p4;
        }
    }

    void compute_rscs_4x4_avx2(const uint8_t* pSrc, int32_t srcPitch, int32_t wblocks, int32_t hblocks, float* pRs, float* pCs)
    {
        for (int32_t i = 0; i < hblocks; i++)
        {
            // 4 horizontal blocks at a time
            int32_t j;
            for (j = 0; j < wblocks - 3; j += 4)
            {
                __m256i rs = _mm256_setzero_si256();
                __m256i cs = _mm256_setzero_si256();
                __m256i a0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[-srcPitch + 0]));

#ifdef __INTEL_COMPILER
#pragma unroll(4)
#endif
                for (int32_t k = 0; k < 4; k++)
                {
                    __m256i b0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[-1]));
                    __m256i c0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[0]));
                    pSrc += srcPitch;

                    // accRs += dRs * dRs
                    a0 = _mm256_sub_epi16(c0, a0);
                    a0 = _mm256_madd_epi16(a0, a0);
                    rs = _mm256_add_epi32(rs, a0);

                    // accCs += dCs * dCs
                    b0 = _mm256_sub_epi16(c0, b0);
                    b0 = _mm256_madd_epi16(b0, b0);
                    cs = _mm256_add_epi32(cs, b0);

                    // reuse next iteration
                    a0 = c0;
                }

                // horizontal sum
                rs = _mm256_hadd_epi32(rs, cs);
                rs = _mm256_permute4x64_epi64(rs, _MM_SHUFFLE(3, 1, 2, 0));    // [ cs3 cs2 cs1 cs0 rs3 rs2 rs1 rs0 ]

                // scale by 1.0f/N and store
                __m256 t = _mm256_mul_ps(_mm256_cvtepi32_ps(rs), _mm256_set1_ps(1.0f / 16));
                _mm_storeu_ps(&pRs[i * wblocks + j], _mm256_extractf128_ps(t, 0));
                _mm_storeu_ps(&pCs[i * wblocks + j], _mm256_extractf128_ps(t, 1));

                pSrc -= 4 * srcPitch;
                pSrc += 16;
            }

            // remaining blocks
            for (; j < wblocks; j++)
            {
                int32_t accRs = 0;
                int32_t accCs = 0;

                for (int32_t k = 0; k < 4; k++)
                {
                    for (int32_t l = 0; l < 4; l++)
                    {
                        int32_t dRs = pSrc[l] - pSrc[l - srcPitch];
                        int32_t dCs = pSrc[l] - pSrc[l - 1];
                        accRs += dRs * dRs;
                        accCs += dCs * dCs;
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs * (1.0f / 16);
                pCs[i * wblocks + j] = accCs * (1.0f / 16);

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            pSrc -= 4 * wblocks;
            pSrc += 4 * srcPitch;
        }
    }

    namespace {
        // Load 0..31 bytes to YMM register from memory
        // NOTE: elements of YMM are permuted [ 16 8 4 2 - 1 ]
        inline __m256 LoadPartialYmm(float *pSrc, int32_t len)
        {
            __m128 xlo = _mm_setzero_ps();
            __m128 xhi = _mm_setzero_ps();
            if (len & 4) {
                xhi = _mm_loadu_ps(pSrc);
                pSrc += 4;
            }
            if (len & 2) {
                xlo = _mm_loadh_pi(xlo, (__m64 *)pSrc);
                pSrc += 2;
            }
            if (len & 1) {
                xlo = _mm_move_ss(xlo, _mm_load_ss(pSrc));
            }
            return _mm256_insertf128_ps(_mm256_castps128_ps256(xlo), xhi, 1);
        }
    }

    void compute_rscs_diff_avx2(float* pRs0, float* pCs0, float* pRs1, float* pCs1, int32_t len, float* pRsDiff, float* pCsDiff)
    {
        int32_t i;//, len = wblocks * hblocks;
        __m256d accRs = _mm256_setzero_pd();
        __m256d accCs = _mm256_setzero_pd();

        for (i = 0; i < len - 7; i += 8)
        {
            __m256 rs = _mm256_sub_ps(_mm256_loadu_ps(&pRs0[i]), _mm256_loadu_ps(&pRs1[i]));
            __m256 cs = _mm256_sub_ps(_mm256_loadu_ps(&pCs0[i]), _mm256_loadu_ps(&pCs1[i]));

            rs = _mm256_mul_ps(rs, rs);
            cs = _mm256_mul_ps(cs, cs);

            accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
            accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
            accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
            accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));
        }

        if (i < len)
        {
            __m256 rs = _mm256_sub_ps(LoadPartialYmm(&pRs0[i], len & 0x7), LoadPartialYmm(&pRs1[i], len & 0x7));
            __m256 cs = _mm256_sub_ps(LoadPartialYmm(&pCs0[i], len & 0x7), LoadPartialYmm(&pCs1[i], len & 0x7));

            rs = _mm256_mul_ps(rs, rs);
            cs = _mm256_mul_ps(cs, cs);

            accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
            accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
            accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
            accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));
        }

        // horizontal sum
        accRs = _mm256_hadd_pd(accRs, accCs);
        __m128d s = _mm_add_pd(_mm256_castpd256_pd128(accRs), _mm256_extractf128_pd(accRs, 1));

        __m128 t = _mm_cvtpd_ps(s);
        _mm_store_ss(pRsDiff, t);
        _mm_store_ss(pCsDiff, _mm_shuffle_ps(t, t, _MM_SHUFFLE(0,0,0,1)));
    }
}

