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
#include "ippdefs.h"
#include "mfx_av1_opts_intrin.h"

#include <stdint.h>

namespace VP9PP
{
    namespace details {

        inline H265_FORCEINLINE int hsum_i32(__m256i r) {
            __m128i a = _mm_add_epi32(si128(r), _mm256_extractf128_si256(r, 1));
            a = _mm_add_epi32(a, _mm_bsrli_si128(a, 8));
            return _mm_cvtsi128_si32(a) + _mm_extract_epi32(a, 1);
        }

        inline H265_FORCEINLINE int sse_w4_8u_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int height) {
            const int pitch1x2 = pitch1 * 2, pitch2x2 = pitch2 * 2;
            const int pitch1x3 = pitch1 * 3, pitch2x3 = pitch2 * 3;
            const int pitch1x4 = pitch1 * 4, pitch2x4 = pitch2 * 4;
            __m128i tmp0, tmp1, tmp2, tmp3;
            __m256i s12, s1, s2, diff, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y += 4, src1 += pitch1x4, src2 += pitch2x4) {
                tmp0 = _mm_unpacklo_epi32(loadl_si32(src1), loadl_si32(src1 + pitch1));
                tmp1 = _mm_unpacklo_epi32(loadl_si32(src2), loadl_si32(src2 + pitch2));
                tmp2 = _mm_unpacklo_epi32(loadl_si32(src1 + pitch1x2), loadl_si32(src1 + pitch1x3));
                tmp3 = _mm_unpacklo_epi32(loadl_si32(src2 + pitch2x2), loadl_si32(src2 + pitch2x3));
                tmp0 = _mm_unpacklo_epi64(tmp0, tmp1);
                tmp2 = _mm_unpacklo_epi64(tmp2, tmp3);
                s12 = _mm256_inserti128_si256(si256(tmp0), tmp2, 1);
                s1 = _mm256_unpacklo_epi8(s12, _mm256_setzero_si256());
                s2 = _mm256_unpackhi_epi8(s12, _mm256_setzero_si256());
                diff = _mm256_sub_epi16(s1, s2);
                diff = _mm256_madd_epi16(diff, diff);
                sse = _mm256_add_epi32(sse, diff);
            }
            return hsum_i32(sse);
        }

        template <int height> int sse_w4_8u_avx2(const uint8_t *src1, const uint8_t *src2) {
            const int pitch1 = 64;
            const int pitch1x2 = pitch1 * 2;
            const int pitch1x3 = pitch1 * 3;
            const int pitch1x4 = pitch1 * 4, pitch2x4 = 4 * 4;
            __m128i tmp0, tmp1;
            __m256i s1, s2, diff, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y += 4, src1 += pitch1x4, src2 += pitch2x4) {
                tmp0 = _mm_unpacklo_epi32(loadl_si32(src1), loadl_si32(src1 + pitch1));
                tmp1 = _mm_unpacklo_epi32(loadl_si32(src1 + pitch1x2), loadl_si32(src1 + pitch1x3));
                tmp0 = _mm_unpacklo_epi64(tmp0, tmp1);
                s1 = _mm256_cvtepu8_epi16(tmp0);
                s2 = _mm256_cvtepu8_epi16(loada_si128(src2));
                diff = _mm256_sub_epi16(s1, s2);
                diff = _mm256_madd_epi16(diff, diff);
                sse = _mm256_add_epi32(sse, diff);
            }
            return hsum_i32(sse);
        }

        inline H265_FORCEINLINE int sse_w8_8u_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int height) {
            const int pitch1x2 = pitch1 * 2, pitch2x2 = pitch2 * 2;
            const int pitch1x3 = pitch1 * 3, pitch2x3 = pitch2 * 3;
            const int pitch1x4 = pitch1 * 4, pitch2x4 = pitch2 * 4;
            __m128i lo, hi;
            __m256i s12, s1, s2, diff, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y += 4, src1 += pitch1x4, src2 += pitch2x4) {
                lo = loadu2_epi64(src1, src2);
                hi = loadu2_epi64(src1 + pitch1, src2 + pitch2);
                s12 = _mm256_inserti128_si256(si256(lo), hi, 1);
                s1 = _mm256_unpacklo_epi8(s12, _mm256_setzero_si256());
                s2 = _mm256_unpackhi_epi8(s12, _mm256_setzero_si256());
                diff = _mm256_sub_epi16(s1, s2);
                diff = _mm256_madd_epi16(diff, diff);
                sse = _mm256_add_epi32(sse, diff);

                lo = loadu2_epi64(src1 + pitch1x2, src2 + pitch2x2);
                hi = loadu2_epi64(src1 + pitch1x3, src2 + pitch2x3);
                s12 = _mm256_inserti128_si256(si256(lo), hi, 1);
                s1 = _mm256_unpacklo_epi8(s12, _mm256_setzero_si256());
                s2 = _mm256_unpackhi_epi8(s12, _mm256_setzero_si256());
                diff = _mm256_sub_epi16(s1, s2);
                diff = _mm256_madd_epi16(diff, diff);
                sse = _mm256_add_epi32(sse, diff);
            }
            return hsum_i32(sse);
        }

        template <int height> int sse_w8_8u_avx2(const uint8_t *src1, const uint8_t *src2) {
            const int pitch1 = 64;
            const int pitch1x2 = pitch1 * 2, pitch2x2 = 8 * 2;
            const int pitch1x3 = pitch1 * 3;
            const int pitch1x4 = pitch1 * 4, pitch2x4 = 8 * 4;
            __m256i s1, s2, diff, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y += 4, src1 += pitch1x4, src2 += pitch2x4) {
                s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src1, src1 + pitch1));
                s2 = _mm256_cvtepu8_epi16(loada_si128(src2));
                diff = _mm256_sub_epi16(s1, s2);
                diff = _mm256_madd_epi16(diff, diff);
                sse = _mm256_add_epi32(sse, diff);

                s1 = _mm256_cvtepu8_epi16(loadu2_epi64(src1 + pitch1x2, src1 + pitch1x3));
                s2 = _mm256_cvtepu8_epi16(loada_si128(src2 + pitch2x2));
                diff = _mm256_sub_epi16(s1, s2);
                diff = _mm256_madd_epi16(diff, diff);
                sse = _mm256_add_epi32(sse, diff);
            }
            return hsum_i32(sse);
        }

        inline H265_FORCEINLINE __m256i update_sse(__m256i sse, __m256i src1, __m256i src2) {
            __m256i s1 = _mm256_unpacklo_epi8(src1, _mm256_setzero_si256());
            __m256i s2 = _mm256_unpacklo_epi8(src2, _mm256_setzero_si256());
            __m256i diff = _mm256_sub_epi16(s1, s2);
            diff = _mm256_madd_epi16(diff, diff);
            sse = _mm256_add_epi32(sse, diff);
            s1 = _mm256_unpackhi_epi8(src1, _mm256_setzero_si256());
            s2 = _mm256_unpackhi_epi8(src2, _mm256_setzero_si256());
            diff = _mm256_sub_epi16(s1, s2);
            diff = _mm256_madd_epi16(diff, diff);
            return _mm256_add_epi32(sse, diff);
        }

        inline H265_FORCEINLINE int sse_w16_8u_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int height) {
            const int pitch1x2 = pitch1 * 2, pitch2x2 = pitch2 * 2;
            const int pitch1x3 = pitch1 * 3, pitch2x3 = pitch2 * 3;
            const int pitch1x4 = pitch1 * 4, pitch2x4 = pitch2 * 4;
            __m256i s1, s2, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y += 4, src1 += pitch1x4, src2 += pitch2x4) {
                s1 = _mm256_inserti128_si256(si256(loada_si128(src1)), loada_si128(src1 + pitch1), 1);
                s2 = _mm256_inserti128_si256(si256(loada_si128(src2)), loada_si128(src2 + pitch2), 1);
                sse = update_sse(sse, s1, s2);
                s1 = _mm256_inserti128_si256(si256(loada_si128(src1 + pitch1x2)), loada_si128(src1 + pitch1x3), 1);
                s2 = _mm256_inserti128_si256(si256(loada_si128(src2 + pitch2x2)), loada_si128(src2 + pitch2x3), 1);
                sse = update_sse(sse, s1, s2);
            }
            return hsum_i32(sse);
        }

        template <int height> int sse_w16_8u_avx2(const uint8_t *src1, const uint8_t *src2) {
            const int pitch1 = 64;
            const int pitch1x2 = pitch1 * 2, pitch2x2 = 16 * 2;
            const int pitch1x3 = pitch1 * 3;
            const int pitch1x4 = pitch1 * 4, pitch2x4 = 16 * 4;
            __m256i s1, s2, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y += 4, src1 += pitch1x4, src2 += pitch2x4) {
                s1 = _mm256_inserti128_si256(si256(loada_si128(src1)), loada_si128(src1 + pitch1), 1);
                s2 = loada_si256(src2);
                sse = update_sse(sse, s1, s2);
                s1 = _mm256_inserti128_si256(si256(loada_si128(src1 + pitch1x2)), loada_si128(src1 + pitch1x3), 1);
                s2 = loada_si256(src2 + pitch2x2);
                sse = update_sse(sse, s1, s2);
            }
            return hsum_i32(sse);
        }

        template <int width> inline H265_FORCEINLINE int sse_w32_8u_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int height) {
            __m256i s1, s2, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y++, src1 += pitch1, src2 += pitch2) {
                for (int x = 0; x < width; x += 32) {
                    s1 = loadu_si256(src1 + x);
                    s2 = loadu_si256(src2 + x);
                    sse = update_sse(sse, s1, s2);
                }
            }
            return hsum_i32(sse);
        }

        template <int width, int height> inline H265_FORCEINLINE int sse_w32_8u_avx2(const uint8_t *src1, const uint8_t *src2) {
            assert((width & 31) == 0);
            __m256i s1, s2, sse = _mm256_setzero_si256();
            for (int y = 0; y < height; y++, src1 += 64, src2 += width) {
                for (int x = 0; x < width; x += 32) {
                    s1 = loadu_si256(src1 + x);
                    s2 = loadu_si256(src2 + x);
                    sse = update_sse(sse, s1, s2);
                }
            }
            return hsum_i32(sse);
        }
    }  // namespace details


    template <int w, int h> int sse_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);

    template <> int sse_avx2<4,4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w4_8u_avx2(src1, pitch1, src2, pitch2, 4);
    }
    template <> int sse_avx2<4,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w4_8u_avx2(src1, pitch1, src2, pitch2, 8);
    }
    template <> int sse_avx2<8,4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w8_8u_avx2(src1, pitch1, src2, pitch2, 4);
    }
    template <> int sse_avx2<8,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w8_8u_avx2(src1, pitch1, src2, pitch2, 8);
    }
    template <> int sse_avx2<8,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w8_8u_avx2(src1, pitch1, src2, pitch2, 16);
    }
    template <> int sse_avx2<16,4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w16_8u_avx2(src1, pitch1, src2, pitch2, 4);
    }
    template <> int sse_avx2<16,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w16_8u_avx2(src1, pitch1, src2, pitch2, 8);
    }
    template <> int sse_avx2<16,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w16_8u_avx2(src1, pitch1, src2, pitch2, 16);
    }
    template <> int sse_avx2<16,32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w16_8u_avx2(src1, pitch1, src2, pitch2, 32);
    }
    template <> int sse_avx2<32,8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w32_8u_avx2<32>(src1, pitch1, src2, pitch2, 8);
    }
    template <> int sse_avx2<32,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w32_8u_avx2<32>(src1, pitch1, src2, pitch2, 16);
    }
    template <> int sse_avx2<32,32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w32_8u_avx2<32>(src1, pitch1, src2, pitch2, 32);
    }
    template <> int sse_avx2<32,64>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w32_8u_avx2<32>(src1, pitch1, src2, pitch2, 64);
    }
    template <> int sse_avx2<64,16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w32_8u_avx2<64>(src1, pitch1, src2, pitch2, 16);
    }
    template <> int sse_avx2<64,32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w32_8u_avx2<64>(src1, pitch1, src2, pitch2, 32);
    }
    template <> int sse_avx2<64,64>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        return details::sse_w32_8u_avx2<64>(src1, pitch1, src2, pitch2, 64);
    }

    template <int w, int h> int sse_p64_pw_avx2(const uint8_t *src1, const uint8_t *src2);

    template <> int sse_p64_pw_avx2<4,4>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w4_8u_avx2<4>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<4,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w4_8u_avx2<8>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<8,4>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w8_8u_avx2<4>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<8,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w8_8u_avx2<8>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<8,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w8_8u_avx2<16>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<16,4>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2<4>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<16,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2<8>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<16,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2<16>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<16,32>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2<32>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<32,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32,8>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<32,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32,16>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<32,32>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32,32>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<32,64>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32,64>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<64,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<64,16>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<64,32>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<64,32>(src1, src2);
    }
    template <> int sse_p64_pw_avx2<64,64>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<64,64>(src1, src2);
    }

    template <int w, int h> int sse_p64_p64_avx2(const uint8_t *src1, const uint8_t *src2);

    template <> int sse_p64_p64_avx2<4,4>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w4_8u_avx2(src1, 64, src2, 64, 4);
    }
    template <> int sse_p64_p64_avx2<4,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w4_8u_avx2(src1, 64, src2, 64, 8);
    }
    template <> int sse_p64_p64_avx2<8,4>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w8_8u_avx2(src1, 64, src2, 64, 4);
    }
    template <> int sse_p64_p64_avx2<8,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w8_8u_avx2(src1, 64, src2, 64, 8);
    }
    template <> int sse_p64_p64_avx2<8,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w8_8u_avx2(src1, 64, src2, 64, 16);
    }
    template <> int sse_p64_p64_avx2<16,4>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2(src1, 64, src2, 64, 4);
    }
    template <> int sse_p64_p64_avx2<16,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2(src1, 64, src2, 64, 8);
    }
    template <> int sse_p64_p64_avx2<16,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2(src1, 64, src2, 64, 16);
    }
    template <> int sse_p64_p64_avx2<16,32>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w16_8u_avx2(src1, 64, src2, 64, 32);
    }
    template <> int sse_p64_p64_avx2<32,8>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32>(src1, 64, src2, 64, 8);
    }
    template <> int sse_p64_p64_avx2<32,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32>(src1, 64, src2, 64, 16);
    }
    template <> int sse_p64_p64_avx2<32,32>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32>(src1, 64, src2, 64, 32);
    }
    template <> int sse_p64_p64_avx2<32,64>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<32>(src1, 64, src2, 64, 64);
    }
    template <> int sse_p64_p64_avx2<64,16>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<64>(src1, 64, src2, 64, 16);
    }
    template <> int sse_p64_p64_avx2<64,32>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<64>(src1, 64, src2, 64, 32);
    }
    template <> int sse_p64_p64_avx2<64,64>(const uint8_t *src1, const uint8_t *src2) {
        return details::sse_w32_8u_avx2<64>(src1, 64, src2, 64, 64);
    }

    template <int w> int sse_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h);
    template <> int sse_avx2<4>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return details::sse_w4_8u_avx2(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<8>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return details::sse_w8_8u_avx2(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<16>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return details::sse_w16_8u_avx2(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<32>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return details::sse_w32_8u_avx2<32>(src1, pitch1, src2, pitch2, h);
    }
    template <> int sse_avx2<64>(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h) {
        return details::sse_w32_8u_avx2<64>(src1, pitch1, src2, pitch2, h);
    }


    // sse function for continuous blocks, ie. pitch == width for both block.
    // limited to multiple-of-32 pixels for now
    int sse_cont_8u_avx2(const uint8_t *src1, const uint8_t *src2, int numPixels) {
        assert((numPixels & 31) == 0);
        __m256i sse = _mm256_setzero_si256();
        for (; numPixels > 0; numPixels -= 32, src1 += 32, src2 += 32)
            sse = details::update_sse(sse, loada_si256(src1), loada_si256(src2));
        return details::hsum_i32(sse);
    }

    static void read_coeff(const int16_t *coeff, intptr_t offset, __m256i *c)
    {
        const int16_t *addr = coeff + offset;
        *c = _mm256_loadu_si256((const __m256i *)addr);
    }

    int64_t sse_cont_avx2(const int16_t *coeff, const int16_t *dqcoeff, int block_size/*, int64_t *ssz*/)
    {
        __m256i sse_reg, /*ssz_reg, */coeff_reg, dqcoeff_reg;
        __m256i exp_dqcoeff_lo, exp_dqcoeff_hi, exp_coeff_lo, exp_coeff_hi;
        __m256i sse_reg_64hi/*, ssz_reg_64hi*/;
        __m128i sse_reg128/*, ssz_reg128*/;
        int64_t sse;
        const __m256i zero_reg = _mm256_setzero_si256();

        // init sse and ssz registerd to zero
        sse_reg = _mm256_setzero_si256();
        //ssz_reg = _mm256_setzero_si256();

        for (int32_t i = 0; i < block_size; i += 16) {
            // load 32 bytes from coeff and dqcoeff
            read_coeff(coeff, i, &coeff_reg);
            read_coeff(dqcoeff, i, &dqcoeff_reg);
            // dqcoeff - coeff
            dqcoeff_reg = _mm256_sub_epi16(dqcoeff_reg, coeff_reg);
            // madd (dqcoeff - coeff)
            dqcoeff_reg = _mm256_madd_epi16(dqcoeff_reg, dqcoeff_reg);
            // madd coeff
            coeff_reg = _mm256_madd_epi16(coeff_reg, coeff_reg);
            // expand each double word of madd (dqcoeff - coeff) to quad word
            exp_dqcoeff_lo = _mm256_unpacklo_epi32(dqcoeff_reg, zero_reg);
            exp_dqcoeff_hi = _mm256_unpackhi_epi32(dqcoeff_reg, zero_reg);
            // expand each double word of madd (coeff) to quad word
            exp_coeff_lo = _mm256_unpacklo_epi32(coeff_reg, zero_reg);
            exp_coeff_hi = _mm256_unpackhi_epi32(coeff_reg, zero_reg);
            // add each quad word of madd (dqcoeff - coeff) and madd (coeff)
            sse_reg = _mm256_add_epi64(sse_reg, exp_dqcoeff_lo);
            //ssz_reg = _mm256_add_epi64(ssz_reg, exp_coeff_lo);
            sse_reg = _mm256_add_epi64(sse_reg, exp_dqcoeff_hi);
            //ssz_reg = _mm256_add_epi64(ssz_reg, exp_coeff_hi);
        }
        // save the higher 64 bit of each 128 bit lane
        sse_reg_64hi = _mm256_srli_si256(sse_reg, 8);
        //ssz_reg_64hi = _mm256_srli_si256(ssz_reg, 8);
        // add the higher 64 bit to the low 64 bit
        sse_reg = _mm256_add_epi64(sse_reg, sse_reg_64hi);
        //ssz_reg = _mm256_add_epi64(ssz_reg, ssz_reg_64hi);

        // add each 64 bit from each of the 128 bit lane of the 256 bit
        sse_reg128 = _mm_add_epi64(_mm256_castsi256_si128(sse_reg), _mm256_extractf128_si256(sse_reg, 1));
        //ssz_reg128 = _mm_add_epi64(_mm256_castsi256_si128(ssz_reg), _mm256_extractf128_si256(ssz_reg, 1));

        // store the results
        _mm_storel_epi64((__m128i *)(&sse), sse_reg128);
        //_mm_storel_epi64((__m128i *)(ssz), ssz_reg128);
        _mm256_zeroupper();

        return sse;
    }

    template<int size> int sse_noshift_avx2(const unsigned short *p1, int pitch1, const unsigned short *p2, int pitch2);

    template<> int sse_noshift_avx2<4>(const unsigned short *p1, int pitch1, const unsigned short *p2, int pitch2) {
        __m256i s1 = _mm256_setr_epi64x(*(uint64_t*)p1, *(uint64_t*)(p1 + pitch1), *(uint64_t*)(p1 + 2*pitch1), *(uint64_t*)(p1 + 3*pitch1));
        __m256i s2 = _mm256_setr_epi64x(*(uint64_t*)p2, *(uint64_t*)(p2 + pitch2), *(uint64_t*)(p2 + 2*pitch2), *(uint64_t*)(p2 + 3*pitch2));
        __m256i diff = _mm256_sub_epi16(s1, s2);
        __m256i sse = _mm256_madd_epi16(diff, diff);
        return details::hsum_i32(sse);
    }

    template<> int sse_noshift_avx2<8>(const unsigned short *p1, int pitch1, const unsigned short *p2, int pitch2) {
        __m256i diff = _mm256_sub_epi16(loadu2_m128i(p1, p1 + pitch1), loadu2_m128i(p2, p2 + pitch2));
        __m256i sse = _mm256_madd_epi16(diff, diff);
        p1 += 2 * pitch1; p2 += 2 * pitch2;
        diff = _mm256_sub_epi16(loadu2_m128i(p1, p1 + pitch1), loadu2_m128i(p2, p2 + pitch2));
        sse = _mm256_add_epi32(sse, _mm256_madd_epi16(diff, diff));
        p1 += 2 * pitch1; p2 += 2 * pitch2;
        diff = _mm256_sub_epi16(loadu2_m128i(p1, p1 + pitch1), loadu2_m128i(p2, p2 + pitch2));
        sse = _mm256_add_epi32(sse, _mm256_madd_epi16(diff, diff));
        p1 += 2 * pitch1; p2 += 2 * pitch2;
        diff = _mm256_sub_epi16(loadu2_m128i(p1, p1 + pitch1), loadu2_m128i(p2, p2 + pitch2));
        sse = _mm256_add_epi32(sse, _mm256_madd_epi16(diff, diff));
        return details::hsum_i32(sse);
    }
};  // namespace VP9PP
