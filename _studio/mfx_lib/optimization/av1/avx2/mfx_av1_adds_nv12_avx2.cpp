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
#include "mfx_av1_opts_common.h"

#include <cstdint>

namespace AV1PP
{
    void adds_nv12_avx2(uint8_t *dstNv12, int32_t pitchDst, const uint8_t *src1Nv12, int32_t pitchSrc1,
                        const int16_t *src2Yv12U, const int16_t *src2Yv12V, int32_t pitchSrc2, int32_t size)
    {
        if (size == 4) {
            __m256i residU = _mm256_setr_epi64x(*(uint64_t*)(src2Yv12U+0*pitchSrc2), *(uint64_t*)(src2Yv12U+2*pitchSrc2),
                *(uint64_t*)(src2Yv12U+1*pitchSrc2), *(uint64_t*)(src2Yv12U+3*pitchSrc2));
            __m256i residV = _mm256_setr_epi64x(*(uint64_t*)(src2Yv12V+0*pitchSrc2), *(uint64_t*)(src2Yv12V+2*pitchSrc2),
                *(uint64_t*)(src2Yv12V+1*pitchSrc2), *(uint64_t*)(src2Yv12V+3*pitchSrc2));
            __m256i residUV1 = _mm256_unpacklo_epi16(residU, residV);
            __m256i residUV2 = _mm256_unpackhi_epi16(residU, residV);
            __m256i pred1 = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(uint64_t*)(src1Nv12+1*pitchSrc1), *(uint64_t*)(src1Nv12+0*pitchSrc1)));
            __m256i pred2 = _mm256_cvtepu8_epi16(_mm_set_epi64x(*(uint64_t*)(src1Nv12+3*pitchSrc1), *(uint64_t*)(src1Nv12+2*pitchSrc1)));
            __m256i dst1 = _mm256_add_epi16(pred1, residUV1);
            __m256i dst2 = _mm256_add_epi16(pred2, residUV2);
            __m256i dst = _mm256_packus_epi16(dst1, dst2);
            __m128i half1 = _mm256_castsi256_si128(dst);
            __m128i half2 = _mm256_extractf128_si256(dst, 1);
            _mm_storel_epi64((__m128i *)(dstNv12+0*pitchDst), half1);
            _mm_storel_epi64((__m128i *)(dstNv12+1*pitchDst), half2);
            _mm_storeh_pd   ((double  *)(dstNv12+2*pitchDst), _mm_castsi128_pd(half1));
            _mm_storeh_pd   ((double  *)(dstNv12+3*pitchDst), _mm_castsi128_pd(half2));
        } else if (size == 8) {
            for (int32_t y = 0; y < 8; y += 2) {
                __m256i residU = _mm256_loadu2_m128i((__m128i *)(src2Yv12U+1*pitchSrc2), (__m128i *)src2Yv12U);
                __m256i residV = _mm256_loadu2_m128i((__m128i *)(src2Yv12V+1*pitchSrc2), (__m128i *)src2Yv12V);
                residU = _mm256_permute4x64_epi64(residU, 0xd8);
                residV = _mm256_permute4x64_epi64(residV, 0xd8);
                __m256i residUV1 = _mm256_unpacklo_epi16(residU, residV);
                __m256i residUV2 = _mm256_unpackhi_epi16(residU, residV);
                __m256i pred1 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+0*pitchSrc1)));
                __m256i pred2 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+1*pitchSrc1)));
                __m256i dst1 = _mm256_add_epi16(pred1, residUV1);
                __m256i dst2 = _mm256_add_epi16(pred2, residUV2);
                __m256i dst = _mm256_packus_epi16(dst1, dst2);
                dst = _mm256_permute4x64_epi64(dst, 0xd8);
                _mm256_storeu2_m128i((__m128i *)(dstNv12+1*pitchDst), (__m128i *)dstNv12, dst);
                src2Yv12U += 2*pitchSrc2;
                src2Yv12V += 2*pitchSrc2;
                src1Nv12  += 2*pitchSrc1;
                dstNv12   += 2*pitchDst;
            }
        } else {
            for (int32_t y = 0; y < size; y++) {
                for (int32_t x = 0; x < size; x += 16) {
                    __m256i residU = _mm256_load_si256((__m256i *)(src2Yv12U+x));
                    __m256i residV = _mm256_load_si256((__m256i *)(src2Yv12V+x));
                    residU = _mm256_permute4x64_epi64(residU, 0xd8);
                    residV = _mm256_permute4x64_epi64(residV, 0xd8);
                    __m256i residUV1 = _mm256_unpacklo_epi16(residU, residV);
                    __m256i residUV2 = _mm256_unpackhi_epi16(residU, residV);
                    __m256i pred1 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+2*x+0)));
                    __m256i pred2 = _mm256_cvtepu8_epi16(_mm_load_si128((__m128i *)(src1Nv12+2*x+16)));
                    __m256i dst1 = _mm256_add_epi16(pred1, residUV1);
                    __m256i dst2 = _mm256_add_epi16(pred2, residUV2);
                    __m256i dst = _mm256_packus_epi16(dst1, dst2);
                    dst = _mm256_permute4x64_epi64(dst, 0xd8);
                    _mm256_store_si256((__m256i *)(dstNv12+2*x), dst);
                }
                src2Yv12U += pitchSrc2;
                src2Yv12V += pitchSrc2;
                src1Nv12  += pitchSrc1;
                dstNv12   += pitchDst;
            }
        }
    }
}
