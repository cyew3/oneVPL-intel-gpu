// Copyright (c) 2019 Intel Corporation
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
#include "algorithm"

#include "mfx_av1_opts_intrin.h"
#include "mfx_av1_intrapred_common.h"
#include "mfx_av1_get_intra_pred_pels.h"

namespace AV1PP
{
    namespace {
        inline void memset4x4(uint16_t *dst, int pitch, __m128i dc)
        {
            const int pitch2 = pitch * 2;
            storel_epi64(dst, dc);
            storel_epi64(dst + pitch, dc);
            dst += pitch2;
            storel_epi64(dst, dc);
            storel_epi64(dst + pitch, dc);
        }

        template <int H> inline void memset8xH(uint16_t *dst, int pitch, __m128i dc)
        {
            static_assert(H % 4 == 0, "H must be divisible by 4");
            const int pitch2 = pitch * 2;
            for (int i = 0; i < H; i += 4) {
                storea_si128(dst, dc);
                storea_si128(dst + pitch, dc);
                dst += pitch2;
                storea_si128(dst, dc);
                storea_si128(dst + pitch, dc);
                dst += pitch2;
            }
        }

        template <int H> inline void memset16xH(uint16_t *dst, int pitch, __m256i dc)
        {
            static_assert(H % 2 == 0, "H must be divisible by 2");
            const int pitch2 = pitch * 2;
            for (int i = 0; i < H; i += 2) {
                storea_si256(dst, dc);
                storea_si256(dst + pitch, dc);
                dst += pitch2;
            }
        }

        template <int H> inline void memset32xH(uint16_t *dst, int pitch, __m256i dc)
        {
            static_assert(H % 2 == 0, "H must be divisible by 2");
            const int pitch2 = pitch * 2;
            for (int i = 0; i < H; i += 2) {
                storea_si256(dst, dc);
                storea_si256(dst + 16, dc);
                storea_si256(dst + pitch, dc);
                storea_si256(dst + pitch + 16, dc);
                dst += pitch2;
            }
        }

        inline void memset64x32(uint16_t *dst, int pitch, __m256i dc)
        {
            for (int i = 0; i < 32; i++) {
                storea_si256(dst, dc);
                storea_si256(dst + 16, dc);
                storea_si256(dst + 32, dc);
                storea_si256(dst + 48, dc);
                dst += pitch;
            }
        }

        template <int shift> inline __m128i hsum_and_shift(__m128i v)
        {
            v = _mm_add_epi16(v, _mm_srli_si128(v, 8));
            v = _mm_add_epi16(v, _mm_srli_si128(v, 4));
            v = _mm_add_epi16(v, _mm_srli_si128(v, 2));
            const __m128i offset = _mm_cvtsi32_si128((1 << shift) >> 1);
            return _mm_srli_epi16(_mm_add_epi16(v, offset), shift);
        }

        template <int shift> inline __m128i hsum_and_shift_nv12(__m128i v)
        {
            static_assert(shift > 0, "shift must be greater than 0");
            v = _mm_add_epi16(v, _mm_srli_si128(v, 8));
            v = _mm_add_epi16(v, _mm_srli_si128(v, 4));
            const __m128i offset = _mm_cvtsi32_si128((0x00010001 << shift) >> 1);
            return _mm_srli_epi16(_mm_add_epi16(v, offset), shift);
        }

        template <int shift> inline __m128i round_shift(__m128i v)
        {
            const __m128i offset = _mm_set1_epi16((1 << shift) >> 1);
            return _mm_srli_epi16(_mm_add_epi16(v, offset), shift);
        }
    };  // anonimous namespace

    template <int txSize, int leftAvail, int topAvail, typename PixType> void predict_intra_dc_av1_avx2(const PixType *topPel, const PixType *leftPel, PixType *dst, int pitch, int, int, int);

    template<> void predict_intra_dc_av1_avx2<0, 0, 0, uint16_t>(const uint16_t *, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        memset4x4(dst, pitch, _mm_set1_epi16(512));
    }

    template<> void predict_intra_dc_av1_avx2<0, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        const uint16_t dc = (topPel[0] + topPel[1] + topPel[2] + topPel[3] + 2) >> 2;
        memset4x4(dst, pitch, _mm_set1_epi16(dc));
    }

    template<> void predict_intra_dc_av1_avx2<0, 1, 0, uint16_t>(const uint16_t *, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_dc_av1_avx2<0, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_dc_av1_avx2<0, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loadl_epi64(topPel), loadl_epi64(leftPel));
        dc = _mm_add_epi16(dc, _mm_srli_si128(dc, 4));
        dc = _mm_add_epi16(dc, _mm_srli_si128(dc, 2));
        dc = round_shift<3>(dc);
        memset4x4(dst, pitch, _mm_broadcastw_epi16(dc));
    }

    template<> void predict_intra_dc_av1_avx2<1, 0, 0, uint16_t>(const uint16_t *, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        memset8xH<8>(dst, pitch, _mm_set1_epi16(512));
    }

    template<> void predict_intra_dc_av1_avx2<1, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        const __m128i dc = hsum_and_shift<3>(loada_si128(topPel));
        memset8xH<8>(dst, pitch, _mm_broadcastw_epi16(dc));
    }

    template<> void predict_intra_dc_av1_avx2<1, 1, 0, uint16_t>(const uint16_t *, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_dc_av1_avx2<1, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_dc_av1_avx2<1, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(leftPel));
        dc = hsum_and_shift<4>(dc);
        memset8xH<8>(dst, pitch, _mm_broadcastw_epi16(dc));
    }

    template<> void predict_intra_dc_av1_avx2<2, 0, 0, uint16_t>(const uint16_t *, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        memset16xH<16>(dst, pitch, _mm256_set1_epi16(512));
    }

    template<> void predict_intra_dc_av1_avx2<2, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = hsum_and_shift<4>(dc);
        memset16xH<16>(dst, pitch, _mm256_broadcastw_epi16(dc));
    }

    template<> void predict_intra_dc_av1_avx2<2, 1, 0, uint16_t>(const uint16_t *, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_dc_av1_avx2<2, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_dc_av1_avx2<2, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel), loada_si128(leftPel + 8)));
        dc = hsum_and_shift<5>(dc);
        memset16xH<16>(dst, pitch, _mm256_broadcastw_epi16(dc));
    }

    template<> void predict_intra_dc_av1_avx2<3, 0, 0, uint16_t>(const uint16_t *, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        memset32xH<32>(dst, pitch, _mm256_set1_epi16(512));
    }

    template<> void predict_intra_dc_av1_avx2<3, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t *, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, loada_si128(topPel + 16));
        dc = _mm_add_epi16(dc, loada_si128(topPel + 24));
        dc = hsum_and_shift<5>(dc);
        memset32xH<32>(dst, pitch, _mm256_broadcastw_epi16(dc));
    }

    template<> void predict_intra_dc_av1_avx2<3, 1, 0, uint16_t>(const uint16_t *, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_dc_av1_avx2<3, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_dc_av1_avx2<3, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 16), loada_si128(topPel + 24)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel), loada_si128(leftPel + 8)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel + 16), loada_si128(leftPel + 24)));
        dc = hsum_and_shift<6>(dc);
        memset32xH<32>(dst, pitch, _mm256_broadcastw_epi16(dc));
    }


    template <int size, int haveLeft, int haveAbove, typename PixType> void predict_intra_nv12_dc_av1_avx2(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int, int, int);

    template<> void predict_intra_nv12_dc_av1_avx2<0, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        memset8xH<4>(dst, pitch, _mm_set1_epi16(512));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<0, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        const __m128i dc = hsum_and_shift_nv12<2>(loada_si128(topPel));
        memset8xH<4>(dst, pitch, _mm_broadcastd_epi32(dc));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<0, 1, 0, uint16_t>(const uint16_t*, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_nv12_dc_av1_avx2<0, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_nv12_dc_av1_avx2<0, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(leftPel));
        dc = hsum_and_shift_nv12<3>(dc);
        memset8xH<4>(dst, pitch, _mm_broadcastd_epi32(dc));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<1, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        memset16xH<8>(dst, pitch, _mm256_set1_epi16(512));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<1, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = hsum_and_shift_nv12<3>(dc);
        memset16xH<8>(dst, pitch, _mm256_broadcastd_epi32(dc));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<1, 1, 0, uint16_t>(const uint16_t*, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_nv12_dc_av1_avx2<1, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_nv12_dc_av1_avx2<1, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel), loada_si128(leftPel + 8)));
        dc = hsum_and_shift_nv12<4>(dc);
        memset16xH<8>(dst, pitch, _mm256_broadcastd_epi32(dc));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<2, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        memset32xH<16>(dst, pitch, _mm256_set1_epi16(512));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<2, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 16), loada_si128(topPel + 24)));
        dc = hsum_and_shift_nv12<4>(dc);
        memset32xH<16>(dst, pitch, _mm256_broadcastd_epi32(dc));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<2, 1, 0, uint16_t>(const uint16_t*, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_nv12_dc_av1_avx2<2, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_nv12_dc_av1_avx2<2, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 16), loada_si128(topPel + 24)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel), loada_si128(leftPel + 8)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel + 16), loada_si128(leftPel + 24)));
        dc = hsum_and_shift_nv12<5>(dc);
        memset32xH<16>(dst, pitch, _mm256_broadcastd_epi32(dc));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<3, 0, 0, uint16_t>(const uint16_t*, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        memset64x32(dst, pitch, _mm256_set1_epi16(512));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<3, 0, 1, uint16_t>(const uint16_t *topPel, const uint16_t*, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 16), loada_si128(topPel + 24)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 32), loada_si128(topPel + 40)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 48), loada_si128(topPel + 56)));
        dc = hsum_and_shift_nv12<5>(dc);
        memset64x32(dst, pitch, _mm256_broadcastd_epi32(dc));
    }

    template<> void predict_intra_nv12_dc_av1_avx2<3, 1, 0, uint16_t>(const uint16_t*, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        predict_intra_nv12_dc_av1_avx2<3, 0, 1>(leftPel, leftPel, dst, pitch, 0, 0, 0);
    }

    template<> void predict_intra_nv12_dc_av1_avx2<3, 1, 1, uint16_t>(const uint16_t *topPel, const uint16_t *leftPel, uint16_t *dst, int pitch, int, int, int)
    {
        __m128i dc = _mm_add_epi16(loada_si128(topPel), loada_si128(topPel + 8));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 16), loada_si128(topPel + 24)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 32), loada_si128(topPel + 40)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(topPel + 48), loada_si128(topPel + 56)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel), loada_si128(leftPel + 8)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel + 16), loada_si128(leftPel + 24)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel + 32), loada_si128(leftPel + 40)));
        dc = _mm_add_epi16(dc, _mm_add_epi16(loada_si128(leftPel + 48), loada_si128(leftPel + 56)));
        dc = hsum_and_shift_nv12<6>(dc);
        memset64x32(dst, pitch, _mm256_broadcastd_epi32(dc));
    }


    namespace {
        template <int txSize> inline void transpose(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int hint = INT_MAX);

        template <> inline void transpose<TX_4X4>(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int)
        {
            __m128i a = loadu2_epi64(src + 0 * pitchSrc, src + 1 * pitchSrc);
            __m128i b = loadu2_epi64(src + 2 * pitchSrc, src + 3 * pitchSrc);
            __m128i c = _mm_unpacklo_epi16(a, b);
            __m128i d = _mm_unpackhi_epi16(a, b);
            a = _mm_unpacklo_epi16(c, d);
            b = _mm_unpackhi_epi16(c, d);
            storel_epi64(dst + 0 * pitchDst, a);
            storeh_epi64(dst + 1 * pitchDst, a);
            storel_epi64(dst + 2 * pitchDst, b);
            storeh_epi64(dst + 3 * pitchDst, b);
        }

        template <> inline void transpose<TX_8X8>(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int)
        {
            __m128i a0 = loada_si128(src + 0 * pitchSrc);
            __m128i a1 = loada_si128(src + 1 * pitchSrc);
            __m128i a2 = loada_si128(src + 2 * pitchSrc);
            __m128i a3 = loada_si128(src + 3 * pitchSrc);
            __m128i a4 = loada_si128(src + 4 * pitchSrc);
            __m128i a5 = loada_si128(src + 5 * pitchSrc);
            __m128i a6 = loada_si128(src + 6 * pitchSrc);
            __m128i a7 = loada_si128(src + 7 * pitchSrc);

            __m128i b0 = _mm_unpacklo_epi16(a0, a1);
            __m128i b1 = _mm_unpacklo_epi16(a2, a3);
            __m128i b2 = _mm_unpacklo_epi16(a4, a5);
            __m128i b3 = _mm_unpacklo_epi16(a6, a7);
            __m128i b4 = _mm_unpackhi_epi16(a0, a1);
            __m128i b5 = _mm_unpackhi_epi16(a2, a3);
            __m128i b6 = _mm_unpackhi_epi16(a4, a5);
            __m128i b7 = _mm_unpackhi_epi16(a6, a7);

            a0 = _mm_unpacklo_epi32(b0, b1);
            a1 = _mm_unpacklo_epi32(b2, b3);
            a2 = _mm_unpackhi_epi32(b0, b1);
            a3 = _mm_unpackhi_epi32(b2, b3);
            a4 = _mm_unpacklo_epi32(b4, b5);
            a5 = _mm_unpacklo_epi32(b6, b7);
            a6 = _mm_unpackhi_epi32(b4, b5);
            a7 = _mm_unpackhi_epi32(b6, b7);

            storea_si128(dst + 0 * pitchDst, _mm_unpacklo_epi64(a0, a1));
            storea_si128(dst + 1 * pitchDst, _mm_unpackhi_epi64(a0, a1));
            storea_si128(dst + 2 * pitchDst, _mm_unpacklo_epi64(a2, a3));
            storea_si128(dst + 3 * pitchDst, _mm_unpackhi_epi64(a2, a3));
            storea_si128(dst + 4 * pitchDst, _mm_unpacklo_epi64(a4, a5));
            storea_si128(dst + 5 * pitchDst, _mm_unpackhi_epi64(a4, a5));
            storea_si128(dst + 6 * pitchDst, _mm_unpacklo_epi64(a6, a7));
            storea_si128(dst + 7 * pitchDst, _mm_unpackhi_epi64(a6, a7));
        }

        template <> inline void transpose<TX_16X16>(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int)
        {
            __m256i a0 = loada_si256(src + 0 * pitchSrc);
            __m256i a1 = loada_si256(src + 1 * pitchSrc);
            __m256i a2 = loada_si256(src + 2 * pitchSrc);
            __m256i a3 = loada_si256(src + 3 * pitchSrc);
            __m256i a4 = loada_si256(src + 4 * pitchSrc);
            __m256i a5 = loada_si256(src + 5 * pitchSrc);
            __m256i a6 = loada_si256(src + 6 * pitchSrc);
            __m256i a7 = loada_si256(src + 7 * pitchSrc);
            __m256i a8 = loada_si256(src + 8 * pitchSrc);
            __m256i a9 = loada_si256(src + 9 * pitchSrc);
            __m256i aA = loada_si256(src + 10 * pitchSrc);
            __m256i aB = loada_si256(src + 11 * pitchSrc);
            __m256i aC = loada_si256(src + 12 * pitchSrc);
            __m256i aD = loada_si256(src + 13 * pitchSrc);
            __m256i aE = loada_si256(src + 14 * pitchSrc);
            __m256i aF = loada_si256(src + 15 * pitchSrc);

            __m256i b0 = _mm256_unpacklo_epi16(a0, a1);
            __m256i b1 = _mm256_unpacklo_epi16(a2, a3);
            __m256i b2 = _mm256_unpacklo_epi16(a4, a5);
            __m256i b3 = _mm256_unpacklo_epi16(a6, a7);
            __m256i b4 = _mm256_unpacklo_epi16(a8, a9);
            __m256i b5 = _mm256_unpacklo_epi16(aA, aB);
            __m256i b6 = _mm256_unpacklo_epi16(aC, aD);
            __m256i b7 = _mm256_unpacklo_epi16(aE, aF);
            __m256i b8 = _mm256_unpackhi_epi16(a0, a1);
            __m256i b9 = _mm256_unpackhi_epi16(a2, a3);
            __m256i bA = _mm256_unpackhi_epi16(a4, a5);
            __m256i bB = _mm256_unpackhi_epi16(a6, a7);
            __m256i bC = _mm256_unpackhi_epi16(a8, a9);
            __m256i bD = _mm256_unpackhi_epi16(aA, aB);
            __m256i bE = _mm256_unpackhi_epi16(aC, aD);
            __m256i bF = _mm256_unpackhi_epi16(aE, aF);

            a0 = _mm256_unpacklo_epi32(b0, b1);
            a1 = _mm256_unpacklo_epi32(b2, b3);
            a2 = _mm256_unpacklo_epi32(b4, b5);
            a3 = _mm256_unpacklo_epi32(b6, b7);
            a4 = _mm256_unpackhi_epi32(b0, b1);
            a5 = _mm256_unpackhi_epi32(b2, b3);
            a6 = _mm256_unpackhi_epi32(b4, b5);
            a7 = _mm256_unpackhi_epi32(b6, b7);
            a8 = _mm256_unpacklo_epi32(b8, b9);
            a9 = _mm256_unpacklo_epi32(bA, bB);
            aA = _mm256_unpacklo_epi32(bC, bD);
            aB = _mm256_unpacklo_epi32(bE, bF);
            aC = _mm256_unpackhi_epi32(b8, b9);
            aD = _mm256_unpackhi_epi32(bA, bB);
            aE = _mm256_unpackhi_epi32(bC, bD);
            aF = _mm256_unpackhi_epi32(bE, bF);

            b0 = _mm256_unpacklo_epi64(a0, a1);
            b1 = _mm256_unpacklo_epi64(a2, a3);
            b2 = _mm256_unpackhi_epi64(a0, a1);
            b3 = _mm256_unpackhi_epi64(a2, a3);
            b4 = _mm256_unpacklo_epi64(a4, a5);
            b5 = _mm256_unpacklo_epi64(a6, a7);
            b6 = _mm256_unpackhi_epi64(a4, a5);
            b7 = _mm256_unpackhi_epi64(a6, a7);
            b8 = _mm256_unpacklo_epi64(a8, a9);
            b9 = _mm256_unpacklo_epi64(aA, aB);
            bA = _mm256_unpackhi_epi64(a8, a9);
            bB = _mm256_unpackhi_epi64(aA, aB);
            bC = _mm256_unpacklo_epi64(aC, aD);
            bD = _mm256_unpacklo_epi64(aE, aF);
            bE = _mm256_unpackhi_epi64(aC, aD);
            bF = _mm256_unpackhi_epi64(aE, aF);

            storea_si256(dst + 0 * pitchDst, permute2x128(b0, b1, 0, 2));
            storea_si256(dst + 1 * pitchDst, permute2x128(b2, b3, 0, 2));
            storea_si256(dst + 2 * pitchDst, permute2x128(b4, b5, 0, 2));
            storea_si256(dst + 3 * pitchDst, permute2x128(b6, b7, 0, 2));
            storea_si256(dst + 4 * pitchDst, permute2x128(b8, b9, 0, 2));
            storea_si256(dst + 5 * pitchDst, permute2x128(bA, bB, 0, 2));
            storea_si256(dst + 6 * pitchDst, permute2x128(bC, bD, 0, 2));
            storea_si256(dst + 7 * pitchDst, permute2x128(bE, bF, 0, 2));
            storea_si256(dst + 8 * pitchDst, permute2x128(b0, b1, 1, 3));
            storea_si256(dst + 9 * pitchDst, permute2x128(b2, b3, 1, 3));
            storea_si256(dst + 10 * pitchDst, permute2x128(b4, b5, 1, 3));
            storea_si256(dst + 11 * pitchDst, permute2x128(b6, b7, 1, 3));
            storea_si256(dst + 12 * pitchDst, permute2x128(b8, b9, 1, 3));
            storea_si256(dst + 13 * pitchDst, permute2x128(bA, bB, 1, 3));
            storea_si256(dst + 14 * pitchDst, permute2x128(bC, bD, 1, 3));
            storea_si256(dst + 15 * pitchDst, permute2x128(bE, bF, 1, 3));
        }

        template <> inline void transpose<TX_32X32>(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int hint)
        {
            const int width = 32;
            const int height = hint < width ? hint : width;
            for (int i = 0; i < height; i += 16)
                for (int j = 0; j < width; j += 16)
                    transpose<TX_16X16>(src + i * pitchSrc + j, pitchSrc, dst + j * pitchDst + i, pitchDst, 16);
        }

        template <int txSize> void predict_intra_z1_av1_avx2(const uint16_t *topPels, uint16_t *dst, int pitch, int dx);

        template <> void predict_intra_z1_av1_avx2<TX_4X4>(const uint16_t *topPels, uint16_t *dst, int pitch, int dx)
        {
            const int width = 4;

            alignas(32) int16_t diffBuf[16];  // topPels[i + 1] - topPels[i]
            alignas(32) int16_t by32Buf[16];  // topPels[i + 1] * 32 + 16
            const __m128i a0 = loada_si128(topPels);
            storea_si128(diffBuf, _mm_sub_epi16(_mm_srli_si128(a0, 2), a0));
            diffBuf[2 * width - 1] = 0;
            storea_si128(diffBuf + 8, _mm_setzero_si128());
            storea_si128(by32Buf + 0, _mm_add_epi16(_mm_slli_epi16(a0, 5), _mm_set1_epi16(16)));
            storea_si128(by32Buf + 8, _mm_broadcastw_epi16(_mm_cvtsi32_si128(by32Buf[2 * width - 1])));

            const __m128i inc1 = _mm_set1_epi16(dx << 10);         // dx
            const __m128i inc2 = _mm_slli_epi16(inc1, 1);          // 2dx
            const __m128i inc12 = _mm_unpacklo_epi64(inc1, inc2);  // dx dx dx dx 2dx 2dx 2dx 2dx
            const __m128i inc34 = _mm_add_epi16(inc12, inc2);      // 3dx 3dx 3dx 3dx 4dx 4dx 4dx 4dx
            const __m256i shift = _mm256_setr_m128i(inc12, inc34); // dx dx dx dx 2dx 2dx 2dx 2dx 3dx 3dx 3dx 3dx 4dx 4dx 4dx 4dx

            const int32_t base0 = (1 * dx) >> 6;
            const int32_t base1 = (2 * dx) >> 6;
            const int32_t base2 = (3 * dx) >> 6;
            const int32_t base3 = (4 * dx) >> 6;
            __m256i diff = _mm256_setr_m128i(
                loadu2_epi64(diffBuf + base0, diffBuf + base1),
                loadu2_epi64(diffBuf + base2, diffBuf + base3));
            __m256i by32 = _mm256_setr_m128i(
                loadu2_epi64(by32Buf + base0, by32Buf + base1),
                loadu2_epi64(by32Buf + base2, by32Buf + base3));
            __m256i res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
            res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);
            storel_epi64(dst + 0 * pitch, si128_lo(res));
            storeh_epi64(dst + 1 * pitch, si128_lo(res));
            storel_epi64(dst + 2 * pitch, si128_hi(res));
            storeh_epi64(dst + 3 * pitch, si128_hi(res));
        }

        template <> void predict_intra_z1_av1_avx2<TX_8X8>(const uint16_t *topPels, uint16_t *dst, int pitch, int dx)
        {
            const int width = 8;

            alignas(32) int16_t diffBuf[32];  // topPels[i + 1] - topPels[i]
            alignas(32) int16_t by32Buf[32];  // topPels[i + 1] * 32 + 16
            const __m256i a0 = loada_si256(topPels);
            const __m256i a1 = permute4x64(a0, 2, 3, 2, 3); // avoid load from beyond topPels[] boundaries
            storea_si256(diffBuf, _mm256_sub_epi16(_mm256_alignr_epi8(a1, a0, 2), a0));
            diffBuf[2 * width - 1] = 0;
            storea_si256(diffBuf + 16, _mm256_setzero_si256());
            storea_si256(by32Buf + 0, _mm256_add_epi16(_mm256_slli_epi16(a0, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 16, _mm256_broadcastw_epi16(_mm_cvtsi32_si128(by32Buf[2 * width - 1])));

            __m256i inc = _mm256_set1_epi16(dx << 10);
            __m256i inc2 = _mm256_slli_epi16(inc, 1); // 2 increments
            __m256i shift = _mm256_setr_m128i(si128_lo(inc), si128_lo(inc2));

            const int pitch2 = pitch * 2;
            const int dx2 = dx * 2;
            for (int r = 0, x = dx; r < width; r += 2, dst += pitch2, x += dx2) {
                const int32_t base0 = (x + 0) >> 6;
                const int32_t base1 = (x + dx) >> 6;
                const __m256i diff = loadu2_m128i(diffBuf + base0, diffBuf + base1);
                const __m256i by32 = loadu2_m128i(by32Buf + base0, by32Buf + base1);
                __m256i res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);
                storea2_m128i(dst, dst + pitch, res);
                shift = _mm256_add_epi16(shift, inc2);
            }
        }

        template <> void predict_intra_z1_av1_avx2<TX_16X16>(const uint16_t *topPels, uint16_t *dst, int pitch, int dx)
        {
            const int width = 16;

            alignas(32) int16_t diffBuf[48];  // topPels[i + 1] - topPels[i]
            alignas(32) int16_t by32Buf[48];  // topPels[i + 1] * 32 + 16
            const __m256i a0 = loada_si256(topPels);
            const __m256i a1 = loada_si256(topPels + 16);
            const __m256i a2 = permute4x64(a1, 2, 3, 2, 3); // avoid load from beyond topPels[] boundaries
            storea_si256(diffBuf + 0, _mm256_sub_epi16(loadu_si256(topPels + 1), a0));
            storea_si256(diffBuf + 16, _mm256_sub_epi16(_mm256_alignr_epi8(a2, a1, 2), a1));
            diffBuf[2 * width - 1] = 0;
            storea_si256(diffBuf + 32, _mm256_setzero_si256());
            storea_si256(by32Buf + 0, _mm256_add_epi16(_mm256_slli_epi16(a0, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 16, _mm256_add_epi16(_mm256_slli_epi16(a1, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 32, _mm256_broadcastw_epi16(_mm_cvtsi32_si128(by32Buf[2 * width - 1])));

            const __m256i inc = _mm256_set1_epi16(dx << 10);
            __m256i shift = _mm256_setzero_si256();
            for (int r = 0, x = dx; r < width; ++r, dst += pitch, x += dx) {
                shift = _mm256_add_epi16(shift, inc);
                const int32_t base = x >> 6;
                __m256i diff = loadu_si256(diffBuf + base);
                __m256i by32 = loadu_si256(by32Buf + base);
                __m256i res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);
                storea_si256(dst, res);
            }
        }

        template <> void predict_intra_z1_av1_avx2<TX_32X32>(const uint16_t *topPels, uint16_t *dst, int pitch, int dx)
        {
            const int width = 32;

            alignas(32) int16_t diffBuf[80];  // topPels[i + 1] - topPels[i]
            alignas(32) int16_t by32Buf[80];  // topPels[i + 1] * 32 + 16

            const __m256i a0 = loada_si256(topPels);
            const __m256i a1 = loada_si256(topPels + 16);
            const __m256i a2 = loada_si256(topPels + 32);
            const __m256i a3 = loada_si256(topPels + 48);
            const __m256i a4 = permute4x64(a3, 2, 3, 2, 3); // avoid load from beyond topPels[] boundaries
            storea_si256(diffBuf + 0, _mm256_sub_epi16(loadu_si256(topPels + 1), a0));
            storea_si256(diffBuf + 16, _mm256_sub_epi16(loadu_si256(topPels + 17), a1));
            storea_si256(diffBuf + 32, _mm256_sub_epi16(loadu_si256(topPels + 33), a2));
            storea_si256(diffBuf + 48, _mm256_sub_epi16(_mm256_alignr_epi8(a4, a3, 2), a3));
            diffBuf[2 * width - 1] = 0;
            storea_si256(diffBuf + 64, _mm256_setzero_si256());
            storea_si256(by32Buf + 0, _mm256_add_epi16(_mm256_slli_epi16(a0, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 16, _mm256_add_epi16(_mm256_slli_epi16(a1, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 32, _mm256_add_epi16(_mm256_slli_epi16(a2, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 48, _mm256_add_epi16(_mm256_slli_epi16(a3, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 64, _mm256_broadcastw_epi16(_mm_cvtsi32_si128(by32Buf[2 * width - 1])));

            const __m256i inc = _mm256_set1_epi16(dx << 10);
            __m256i shift = _mm256_setzero_si256();
            for (int r = 0, x = dx; r < width; ++r, dst += pitch, x += dx) {
                shift = _mm256_add_epi16(shift, inc);
                const int32_t base = x >> 6;
                __m256i diff = loadu_si256(diffBuf + base);
                __m256i by32 = loadu_si256(by32Buf + base);
                __m256i res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);
                storea_si256(dst, res);

                diff = loadu_si256(diffBuf + base + 16);
                by32 = loadu_si256(by32Buf + base + 16);
                res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);
                storea_si256(dst + 16, res);
            }
        }

        template <int txSize, bool blend> int predict_z2(const uint16_t *pels, uint16_t *dst, int pitch, int dx);

        template <bool blend> void predict_z2_4x4(const uint16_t *pels, __m256i *dst, int dx)
        {
            const int width = 4;

            // topPels[i + 1] - topPels[i]
            alignas(32) int16_t diffBuf[2 * width];
            __m128i tmp = loadu_si128(pels - 1); // AL A0 A1 A2 A3 A4 A5 A6
            __m128i a = _mm_slli_si128(tmp, 6);  // 00 00 00 AL A0 A1 A2 A3
            storea_si128(diffBuf, _mm_sub_epi16(_mm_slli_si128(tmp, 4), a));

            // topPels[i + 1] * 32 + 16
            alignas(32) int16_t by32Buf[2 * width];
            storea_si128(by32Buf, _mm_add_epi16(_mm_slli_epi16(a, 5), _mm_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            const __m128i inc1 = _mm_set1_epi16(-dx << 10);        // dx
            const __m128i inc2 = _mm_add_epi16(inc1, inc1);        // 2dx
            const __m128i inc12 = _mm_unpacklo_epi64(inc1, inc2);  // dx dx dx dx 2dx 2dx 2dx 2dx
            const __m128i inc34 = _mm_add_epi16(inc12, inc2);      // 3dx 3dx 3dx 3dx 4dx 4dx 4dx 4dx
            const __m256i shift = _mm256_setr_m128i(inc12, inc34); // dx dx dx dx 2dx 2dx 2dx 2dx 3dx 3dx 3dx 3dx 4dx 4dx 4dx 4dx

            int32_t base0 = (-1 * dx) >> 6;
            int32_t base1 = (-2 * dx) >> 6;
            int32_t base2 = (-3 * dx) >> 6;
            int32_t base3 = (-4 * dx) >> 6;

            __m256i blend_mask;
            if (blend) {
                const uint8_t *pm = z2_blend_mask + 66;
                base0 = (base0 < -33) ? -33 : base0;
                base1 = (base1 < -33) ? -33 : base1;
                base2 = (base2 < -33) ? -33 : base2;
                base3 = (base3 < -33) ? -33 : base3;
                blend_mask = _mm256_setr_m128i(loadu2_epi64(pm + 2 * base0, pm + 2 * base1), loadu2_epi64(pm + 2 * base2, pm + 2 * base3));
            }

            if (base3 < -width) base3 = -width;
            if (base2 < -width) base2 = -width;
            if (base1 < -width) base1 = -width;
            if (base0 < -width) base0 = -width;

            __m256i diff = _mm256_setr_m128i(
                loadu2_epi64(pdiff + base0, pdiff + base1),
                loadu2_epi64(pdiff + base2, pdiff + base3));
            __m256i by32 = _mm256_setr_m128i(
                loadu2_epi64(pby32 + base0, pby32 + base1),
                loadu2_epi64(pby32 + base2, pby32 + base3));
            __m256i res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
            res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);

            if (blend)
                res = _mm256_blendv_epi8(*dst, res, blend_mask);

            *dst = res;
        }

        template <bool blend> void predict_z2_8x8(const uint16_t *pels, uint16_t *dst, int pitch, int dx)
        {
            const int width = 8;

            // topPels[i + 1] - topPels[i]
            alignas(32) int16_t diffBuf[2 * width];
            const __m128i a = loada_si128(pels);
            storea_si128(diffBuf + 0, _mm_setzero_si128());
            diffBuf[width - 1] = pels[0] - pels[-1];
            storea_si128(diffBuf + 8, _mm_sub_epi16(loadu_si128(pels + 1), a));

            // topPels[i + 1] * 32 + 16
            alignas(32) int16_t by32Buf[2 * width];
            storea_si128(by32Buf + 0, _mm_setzero_si128());
            by32Buf[width - 1] = (pels[-1] << 5) + 16;
            storea_si128(by32Buf + 8, _mm_add_epi16(_mm_slli_epi16(a, 5), _mm_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            const __m256i inc = _mm256_set1_epi16(-dx << 10);
            __m256i inc2 = _mm256_add_epi16(inc, inc); // 2 increments
            __m256i shift = _mm256_setr_m128i(si128_lo(inc), si128_lo(inc2));

            const int pitch2 = pitch * 2;
            const int dx2 = dx * 2;

            for (int r = 0, x = -dx; r < width; r += 2, dst += pitch2, x -= dx2) {
                const int32_t base0 = (x - 0) >> 6;
                const int32_t base1 = (x - dx) >> 6;
                if (base0 < -width)
                    break;

                const __m256i diff = loadu2_m128i(pdiff + base0, pdiff + base1);
                const __m256i by32 = loadu2_m128i(pby32 + base0, pby32 + base1);
                __m256i res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);
                shift = _mm256_add_epi16(shift, inc2);

                if (blend) {
                    const __m256i l = loada2_m128i(dst, dst + pitch);
                    const __m256i m = loadu2_m128i(z2_blend_mask + 66 + 2 * base0, z2_blend_mask + 66 + 2 * base1);
                    res = _mm256_blendv_epi8(l, res, m);
                }
                storea2_m128i(dst, dst + pitch, res);
            }
        }

        template <bool blend> void predict_z2_16x16(const uint16_t *pels, uint16_t *dst, int pitch, int dx)
        {
            const int width = 16;

            // topPels[i + 1] - topPels[i]
            alignas(32) int16_t diffBuf[2 * width];
            const __m256i a = loada_si256(pels);
            storea_si256(diffBuf + 0, _mm256_setzero_si256());
            diffBuf[width - 1] = pels[0] - pels[-1];
            storea_si256(diffBuf + 16, _mm256_sub_epi16(loadu_si256(pels + 1), a));

            // topPels[i + 1] * 32 + 16
            alignas(32) int16_t by32Buf[2 * width];
            storea_si256(by32Buf + 0, _mm256_setzero_si256());
            by32Buf[width - 1] = (pels[-1] << 5) + 16;
            storea_si256(by32Buf + 16, _mm256_add_epi16(_mm256_slli_epi16(a, 5), _mm256_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            const __m256i dec = _mm256_set1_epi16(dx << 10);

            __m256i shift = _mm256_setzero_si256();
            for (int r = 0, x = -dx; r < width; ++r, dst += pitch, x -= dx) {
                shift = _mm256_sub_epi16(shift, dec);
                const int32_t base = x >> 6;
                if (base < -width)
                    break;
                __m256i diff = loadu_si256(pdiff + base);
                __m256i by32 = loadu_si256(pby32 + base);
                __m256i res = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res = _mm256_srli_epi16(_mm256_add_epi16(res, by32), 5);

                if (blend) {
                    const __m256i l = loada_si256(dst);
                    const __m256i m = loadu_si256(z2_blend_mask + 66 + 2 * base);
                    res = _mm256_blendv_epi8(l, res, m);
                }
                storea_si256(dst, res);
            }
        }

        template <bool blend> int predict_z2_32x32(const uint16_t *pels, uint16_t *dst, int pitch, int dx)
        {
            const int width = 32;

            // topPels[i + 1] - topPels[i]
            alignas(32) int16_t diffBuf[2 * width];
            const __m256i a0 = loada_si256(pels + 0);
            const __m256i a1 = loada_si256(pels + 16);
            storea_si256(diffBuf + 0, _mm256_setzero_si256());
            storea_si256(diffBuf + 16, _mm256_setzero_si256());
            diffBuf[width - 1] = pels[0] - pels[-1];
            storea_si256(diffBuf + 32, _mm256_sub_epi16(loadu_si256(pels + 1), a0));
            storea_si256(diffBuf + 48, _mm256_sub_epi16(loadu_si256(pels + 17), a1));

            // topPels[i + 1] * 32 + 16
            alignas(32) int16_t by32Buf[2 * width];
            storea_si256(by32Buf + 0, _mm256_setzero_si256());
            storea_si256(by32Buf + 16, _mm256_setzero_si256());
            by32Buf[width - 1] = (pels[-1] << 5) + 16;
            storea_si256(by32Buf + 32, _mm256_add_epi16(_mm256_slli_epi16(a0, 5), _mm256_set1_epi16(16)));
            storea_si256(by32Buf + 48, _mm256_add_epi16(_mm256_slli_epi16(a1, 5), _mm256_set1_epi16(16)));

            const int16_t *pdiff = diffBuf + width;
            const int16_t *pby32 = by32Buf + width;

            const __m256i dec = _mm256_set1_epi16(dx << 10);

            __m256i shift = _mm256_setzero_si256();
            int r = 0;
            for (int x = -dx; r < width; ++r, dst += pitch, x -= dx) {
                shift = _mm256_sub_epi16(shift, dec);
                const int32_t base = x >> 6;
                if (base < -width)
                    break;
                __m256i diff = loadu_si256(pdiff + base);
                __m256i by32 = loadu_si256(pby32 + base);
                __m256i res0 = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res0 = _mm256_srli_epi16(_mm256_add_epi16(res0, by32), 5);

                diff = loadu_si256(pdiff + base + 16);
                by32 = loadu_si256(pby32 + base + 16);
                __m256i res1 = _mm256_mullo_epi16(diff, _mm256_srli_epi16(shift, 11));
                res1 = _mm256_srli_epi16(_mm256_add_epi16(res1, by32), 5);

                if (blend) {
                    const __m256i l0 = loada_si256(dst);
                    const __m256i l1 = loada_si256(dst + 16);
                    const __m256i m0 = loadu_si256(z2_blend_mask + 66 + 2 * base);
                    const __m256i m1 = loadu_si256(z2_blend_mask + 66 + 2 * base + 32);
                    res0 = _mm256_blendv_epi8(l0, res0, m0);
                    res1 = _mm256_blendv_epi8(l1, res1, m1);
                }

                storea_si256(dst, res0);
                storea_si256(dst + 16, res1);
            }
            return r;
        }

        template <int txSize> void predict_intra_z2_av1_avx2(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int dx, int dy);

        template <> void predict_intra_z2_av1_avx2<TX_4X4>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int dx, int dy)
        {
            __m256i out;
            predict_z2_4x4<false>(leftPels, &out, dy);
            out = _mm256_unpacklo_epi16(out, _mm256_srli_si256(out, 8));  // A0 B0 A1 B1 A2 B2 A3 B3 C0 D0 C1 D1 C2 D2 C3 D3
            out = permute4x64(out, 0, 2, 1, 3);
            out = _mm256_unpacklo_epi32(out, _mm256_srli_si256(out, 8));  // A0 B0 C0 D0 A1 B1 C1 D1 A2 B2 C2 D2 A3 B3 C3 D3
            predict_z2_4x4<true>(topPels, &out, dx);
            storel_epi64(dst + 0 * pitch, si128_lo(out));
            storeh_epi64(dst + 1 * pitch, si128_lo(out));
            storel_epi64(dst + 2 * pitch, si128_hi(out));
            storeh_epi64(dst + 3 * pitch, si128_hi(out));
        }

        template <> void predict_intra_z2_av1_avx2<TX_8X8>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int dx, int dy)
        {
            const int width = 8;
            alignas(32) uint16_t tmp[width * width];

            predict_z2_8x8<false>(leftPels, tmp, width, dy);
            transpose<TX_8X8>(tmp, width, dst, pitch);
            predict_z2_8x8<true>(topPels, dst, pitch, dx);
        }

        template <> void predict_intra_z2_av1_avx2<TX_16X16>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int dx, int dy)
        {
            const int width = 16;
            alignas(32) uint16_t tmp[width * width];

            predict_z2_16x16<false>(leftPels, tmp, width, dy);
            transpose<TX_16X16>(tmp, width, dst, pitch);
            predict_z2_16x16<true>(topPels, dst, pitch, dx);
        }

        template <> void predict_intra_z2_av1_avx2<TX_32X32>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int dx, int dy)
        {
            const int width = 32;
            alignas(32) uint16_t tmp[width * width];

            int nleft = predict_z2_32x32<false>(leftPels, tmp, width, dy);
            transpose<TX_32X32>(tmp, width, dst, pitch, nleft);
            predict_z2_32x32<true>(topPels, dst, pitch, dx);
        }

        template <int txSize> void predict_intra_z3_av1_avx2(const uint16_t *leftPels, uint16_t *dst, int pitch, int dy)
        {
            const int width = 4 << txSize;
            alignas(32) uint16_t dstT[width * width];
            predict_intra_z1_av1_avx2<txSize>(leftPels, dstT, width, dy);
            transpose<txSize>(dstT, width, dst, pitch, 32);
        }

        template <int txSize> void predict_intra_ver_av1_avx2(const uint16_t *topPels, uint16_t *dst, int pitch);

        template <> void predict_intra_ver_av1_avx2<TX_4X4>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            const __m128i above = loadl_epi64(topPels);

            storel_epi64(dst, above); storel_epi64(dst + pitch, above); dst += pitch2;
            storel_epi64(dst, above); storel_epi64(dst + pitch, above);;
        }

        template <> void predict_intra_ver_av1_avx2<TX_8X8>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            const __m128i above = loada_si128(topPels);

            storea_si128(dst, above); storea_si128(dst + pitch, above); dst += pitch2;
            storea_si128(dst, above); storea_si128(dst + pitch, above); dst += pitch2;
            storea_si128(dst, above); storea_si128(dst + pitch, above); dst += pitch2;
            storea_si128(dst, above); storea_si128(dst + pitch, above);
        }

        template <> void predict_intra_ver_av1_avx2<TX_16X16>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int width = 16;
            const int pitch2 = pitch * 2;
            const __m256i above = loada_si256(topPels);

            for (int r = 0; r < width; r += 4, dst += pitch2) {
                storea_si256(dst, above);
                storea_si256(dst + pitch, above);
                dst += pitch2;
                storea_si256(dst, above);
                storea_si256(dst + pitch, above);
            }
        }

        template <> void predict_intra_ver_av1_avx2<TX_32X32>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int width = 32;
            const int pitch2 = pitch * 2;
            const __m256i above0 = loada_si256(topPels);
            const __m256i above1 = loada_si256(topPels + 16);

            for (int r = 0; r < width; r += 4, dst += pitch2) {
                storea_si256(dst, above0);
                storea_si256(dst + 16, above1);
                storea_si256(dst + pitch, above0);
                storea_si256(dst + pitch + 16, above1);
                dst += pitch2;
                storea_si256(dst, above0);
                storea_si256(dst + 16, above1);
                storea_si256(dst + pitch, above0);
                storea_si256(dst + pitch + 16, above1);
            }
        }

        template <int txSize> void predict_intra_hor_av1_avx2(const uint16_t *leftPels, uint16_t *dst, int pitch);

        template <> void predict_intra_hor_av1_avx2<TX_4X4>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            __m128i left = loadl_epi64(leftPels);
            storel_epi64(dst + 0 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storel_epi64(dst + 1 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2), dst += pitch2;
            storel_epi64(dst + 0 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storel_epi64(dst + 1 * pitch, _mm_broadcastw_epi16(left));
        }

        template <> void predict_intra_hor_av1_avx2<TX_8X8>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            __m128i left = loada_si128(leftPels);
            storea_si128(dst + 0 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si128(dst + 1 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2), dst += pitch2;
            storea_si128(dst + 0 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si128(dst + 1 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2), dst += pitch2;
            storea_si128(dst + 0 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si128(dst + 1 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2), dst += pitch2;
            storea_si128(dst + 0 * pitch, _mm_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si128(dst + 1 * pitch, _mm_broadcastw_epi16(left));
        }

        template <> void predict_intra_hor_av1_avx2<TX_16X16>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            __m128i left;
            left = loada_si128(leftPels);
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2),  dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2),  dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2),  dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left)); dst += pitch2;
            left = loada_si128(leftPels + 8);
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2),  dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2),  dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2),  dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastw_epi16(left)); left = _mm_srli_si128(left, 2);
            storea_si256(dst + 1 * pitch, _mm256_broadcastw_epi16(left));
        }

        template <> void predict_intra_hor_av1_avx2<TX_32X32>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            for (int i = 0; i < 4; i++) {
                __m128i left = loada_si128(leftPels + i * 8);
                __m256i l;
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch; left = _mm_srli_si128(left, 2);
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch; left = _mm_srli_si128(left, 2);
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch; left = _mm_srli_si128(left, 2);
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch; left = _mm_srli_si128(left, 2);
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch; left = _mm_srli_si128(left, 2);
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch; left = _mm_srli_si128(left, 2);
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch; left = _mm_srli_si128(left, 2);
                l = _mm256_broadcastw_epi16(left); storea_si256(dst, l); storea_si256(dst + 16, l); dst += pitch;
            }
        }
    };  // namespace

    template <int size, int mode, typename PixType> void predict_intra_av1_avx2(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int delta, int upTop, int upLeft)
    {
        upTop; upLeft;
        assert(upTop == 0);
        assert(upLeft == 0);
        assert(mode >= V_PRED && mode <= D63_PRED);
        const int angle = mode_to_angle_map[mode] + delta * 3;
        const int dx = get_dx(angle);
        const int dy = get_dy(angle);

        if (angle > 0 && angle < 90)         predict_intra_z1_av1_avx2<size>(topPels, dst, pitch, dx);
        else if (angle > 90 && angle < 180)  predict_intra_z2_av1_avx2<size>(topPels, leftPels, dst, pitch, dx, dy);
        else if (angle > 180 && angle < 270) predict_intra_z3_av1_avx2<size>(leftPels, dst, pitch, dy);
        else if (angle == 90)                predict_intra_ver_av1_avx2<size>(topPels, dst, pitch);
        else if (angle == 180)               predict_intra_hor_av1_avx2<size>(leftPels, dst, pitch);
    }

    template void predict_intra_av1_avx2<TX_4X4, V_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, V_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, V_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, V_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template void predict_intra_av1_avx2<TX_4X4, H_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, H_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, H_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, H_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template void predict_intra_av1_avx2<TX_4X4, D45_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, D45_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, D45_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, D45_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template void predict_intra_av1_avx2<TX_4X4, D135_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, D135_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, D135_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, D135_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template void predict_intra_av1_avx2<TX_4X4, D117_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, D117_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, D117_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, D117_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template void predict_intra_av1_avx2<TX_4X4, D153_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, D153_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, D153_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, D153_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template void predict_intra_av1_avx2<TX_4X4, D207_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, D207_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, D207_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, D207_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    template void predict_intra_av1_avx2<TX_4X4, D63_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_8X8, D63_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_16X16, D63_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);
    template void predict_intra_av1_avx2<TX_32X32, D63_PRED, uint16_t>(const uint16_t*, const uint16_t*, uint16_t*, int, int, int, int);

    namespace {
        inline __m256i smooth_pred(__m256i w0, __m256i w1, __m256i p0, __m256i p1, __m256i offset)
        {
            __m256i w_lo = _mm256_unpacklo_epi16(w0, w1);
            __m256i w_hi = _mm256_unpackhi_epi16(w0, w1);
            __m256i p_lo = _mm256_unpacklo_epi16(p0, p1);
            __m256i p_hi = _mm256_unpackhi_epi16(p0, p1);
            __m256i res_lo = _mm256_madd_epi16(w_lo, p_lo);
            __m256i res_hi = _mm256_madd_epi16(w_hi, p_hi);
            res_lo = _mm256_add_epi32(res_lo, offset);
            res_hi = _mm256_add_epi32(res_hi, offset);
            res_lo = _mm256_srli_epi32(res_lo, 9);
            res_hi = _mm256_srli_epi32(res_hi, 9);
            return _mm256_packus_epi32(res_lo, res_hi);
        }

        inline __m256i smooth_vh_pred(__m256i w_lo, __m256i w_hi, __m256i lo, __m256i hi, __m256i offset)
        {
            __m256i res_lo = _mm256_madd_epi16(w_lo, lo);
            __m256i res_hi = _mm256_madd_epi16(w_hi, hi);
            res_lo = _mm256_add_epi32(res_lo, offset);
            res_hi = _mm256_add_epi32(res_hi, offset);
            res_lo = _mm256_srli_epi32(res_lo, 8);
            res_hi = _mm256_srli_epi32(res_hi, 8);
            return _mm256_packus_epi32(res_lo, res_hi);
        }

        inline __m256i smooth_vh_pred(int weight, __m256i lo, __m256i hi, __m256i offset)
        {
            const __m256i w = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(weight));
            return smooth_vh_pred(w, w, lo, hi, offset);
        }

        inline __m256i paeth_pred(__m256i diff_a, __m256i diff_l, __m256i a_minus_2al, __m256i left, __m256i above, __m256i above_left)
        {
            const __m256i diff_al = _mm256_abs_epi16(_mm256_add_epi16(a_minus_2al, left));
            const __m256i diff_tmp = _mm256_min_epi16(diff_l, diff_a);
            const __m256i tmp = _mm256_blendv_epi8(left, above, _mm256_cmpgt_epi16(diff_l, diff_a));
            return _mm256_blendv_epi8(tmp, above_left, _mm256_cmpgt_epi16(diff_tmp, diff_al));
        }

        inline __m256i paeth_pred(int diff_a_, __m256i diff_l, __m256i a_minus_2al, int left_, __m256i above, __m256i above_left)
        {
            const __m256i diff_a = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(diff_a_));
            const __m256i left = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(left_));
            return paeth_pred(diff_a, diff_l, a_minus_2al, left, above, above_left);
        }
    };  // anonimous namespace

    template<> void predict_intra_av1_avx2<TX_4X4, SMOOTH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 4;
        const uint16_t below_pred = leftPels[width - 1];
        const uint16_t right_pred = topPels[width - 1];

        const __m256i const_part = _mm256_set1_epi32(((below_pred + right_pred) << 8) + 256);
        const __m256i weights_c = _mm256_set1_epi64x(0x00400055009500ff);
        const __m256i weights_r = _mm256_setr_epi64x(0x00ff00ff00ff00ff, 0x0095009500950095, 0x0055005500550055, 0x0040004000400040);

        __m256i left = _mm256_setr_m128i(_mm_cvtsi32_si128(*(int *)(leftPels)), _mm_cvtsi32_si128(*(int *)(leftPels + 2)));
        left = _mm256_unpacklo_epi16(left, left);
        left = _mm256_unpacklo_epi32(left, left);                             // L0 L0 L0 L0  L1 L1 L1 L1  L2 L2 L2 L2  L3 L3 L3 L3
        const __m256i above = _mm256_broadcastq_epi64(loadl_epi64(topPels));  // A0 A1 A2 A3  A0 A1 A2 A3  A0 A1 A2 A3  A0 A1 A2 A3

        const __m256i above_minus_below = _mm256_sub_epi16(above, _mm256_set1_epi16(below_pred));
        const __m256i left_minus_right = _mm256_sub_epi16(left, _mm256_set1_epi16(right_pred));

        const __m256i pred = smooth_pred(weights_r, weights_c, above_minus_below, left_minus_right, const_part);
        storel_epi64(dst + 0 * pitch, si128_lo(pred));
        storeh_epi64(dst + 1 * pitch, si128_lo(pred));
        storel_epi64(dst + 2 * pitch, si128_hi(pred));
        storeh_epi64(dst + 3 * pitch, si128_hi(pred));
    }

    template<> void predict_intra_av1_avx2<TX_8X8, SMOOTH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 8;
        const uint16_t below_pred = leftPels[width - 1];
        const uint16_t right_pred = topPels[width - 1];

        const __m256i const_part = _mm256_set1_epi32(((below_pred + right_pred) << 8) + 256);
        const __m128i weights_m128i = _mm_unpacklo_epi8(loadl_epi64(smooth_weights_8), _mm_setzero_si128());
        const __m256i weights_dup = _mm256_setr_m128i(weights_m128i, weights_m128i);
        __m256i weights_shifted = _mm256_setr_m128i(weights_m128i, _mm_srli_si128(weights_m128i, 2));
        const __m256i above = _mm256_broadcastsi128_si256(loada_si128(topPels));
        const __m256i above_minus_below = _mm256_sub_epi16(above, _mm256_set1_epi16(below_pred));

        const __m128i left = loada_si128(leftPels);
        const __m128i left_minus_right_m128i = _mm_sub_epi16(left, _mm_set1_epi16(right_pred));
        __m256i left_minus_right_shifted = _mm256_setr_m128i(left_minus_right_m128i, _mm_srli_si128(left_minus_right_m128i, 2));

        const int32_t pitch2 = pitch * 2;
        for (int r = 0; r < width; r += 2, dst += pitch2) {
            const __m256i weight_r = _mm256_shuffle_epi8(weights_shifted, _mm256_set1_epi16(0x100));
            const __m256i left_minus_right = _mm256_shuffle_epi8(left_minus_right_shifted, _mm256_set1_epi16(0x100));

            left_minus_right_shifted = _mm256_srli_si256(left_minus_right_shifted, 4);
            weights_shifted = _mm256_srli_si256(weights_shifted, 4);

            const __m256i pred = smooth_pred(weight_r, weights_dup, above_minus_below, left_minus_right, const_part);
            storea2_m128i(dst, dst + pitch, pred);
        }
    }

    template<> void predict_intra_av1_avx2<TX_16X16, SMOOTH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 16;
        const uint16_t below_pred = leftPels[width - 1];
        const uint16_t right_pred = topPels[width - 1];

        const __m256i const_part = _mm256_set1_epi32(((below_pred + right_pred) << 8) + 256);
        const __m256i weights = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_16));
        const __m256i above_minus_below = _mm256_sub_epi16(loada_si256(topPels), _mm256_set1_epi16(below_pred));

        for (int r = 0; r < width; ++r, dst += pitch) {
            const __m256i left_minus_right = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[r] - right_pred));
            const __m256i weight_r = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_16[r]));

            storea_si256(dst, smooth_pred(weight_r, weights, above_minus_below, left_minus_right, const_part));
        }
    }

    template<> void predict_intra_av1_avx2<TX_32X32, SMOOTH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 32;
        const uint16_t below_pred = leftPels[width - 1];
        const uint16_t right_pred = topPels[width - 1];

        const __m256i const_part = _mm256_set1_epi32(((below_pred + right_pred) << 8) + 256);
        const __m256i weights[2] = {
            _mm256_cvtepu8_epi16(loada_si128(smooth_weights_32 + 0)),
            _mm256_cvtepu8_epi16(loada_si128(smooth_weights_32 + 16))
        };
        const __m256i above_minus_below[2] = {
            _mm256_sub_epi16(loada_si256(topPels + 0), _mm256_set1_epi16(below_pred)),
            _mm256_sub_epi16(loada_si256(topPels + 16), _mm256_set1_epi16(below_pred))
        };

        for (int r = 0; r < width; ++r, dst += pitch) {
            const __m256i left_minus_right = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(leftPels[r] - right_pred));
            const __m256i weight_r = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(smooth_weights_32[r]));

            storea_si256(dst + 0, smooth_pred(weight_r, weights[0], above_minus_below[0], left_minus_right, const_part));
            storea_si256(dst + 16, smooth_pred(weight_r, weights[1], above_minus_below[1], left_minus_right, const_part));
        }
    }

    template<> void predict_intra_av1_avx2<TX_4X4, SMOOTH_V_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 4;
        const int32_t below_pred = leftPels[width - 1];

        __m128i w_tmp = loadl_si32(smooth_weights_4);   // 0123************
        w_tmp = _mm_unpacklo_epi8(w_tmp, w_tmp);        // 00112233********
        w_tmp = _mm_unpacklo_epi16(w_tmp, w_tmp);       // 0000111122223333
        const __m256i w01 = _mm256_cvtepu8_epi16(_mm_unpacklo_epi32(w_tmp, w_tmp));  // 0000000011111111
        const __m256i w23 = _mm256_cvtepu8_epi16(_mm_unpackhi_epi32(w_tmp, w_tmp));  // 2222222233333333

        const __m256i a = _mm256_broadcastq_epi64(loadl_epi64(topPels));
        const __m256i b = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(-below_pred));
        const __m256i ab_lo = _mm256_unpacklo_epi16(a, b);
        const __m256i ab_hi = _mm256_unpackhi_epi16(a, b);
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(below_pred * 256 + 128));

        const __m256i pred = smooth_vh_pred(w01, w23, ab_lo, ab_hi, offset);
        storel_epi64(dst + 0 * pitch, si128_lo(pred));
        storel_epi64(dst + 1 * pitch, si128_hi(pred));
        storeh_epi64(dst + 2 * pitch, si128_lo(pred));
        storeh_epi64(dst + 3 * pitch, si128_hi(pred));
    }

    template<> void predict_intra_av1_avx2<TX_8X8, SMOOTH_V_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 8;
        const int32_t below_pred = leftPels[width - 1];

        __m128i w_tmp = loadl_epi64(smooth_weights_8);                                // 01234567********
        w_tmp = _mm_unpacklo_epi8(w_tmp, w_tmp);                                      // 0011223344556677
        const __m128i w0123 = _mm_unpacklo_epi16(w_tmp, w_tmp);                       // 0000111122223333
        const __m128i w4567 = _mm_unpackhi_epi16(w_tmp, w_tmp);                       // 4444555566667777
        const __m256i w01 = _mm256_cvtepu8_epi16(_mm_unpacklo_epi32(w0123, w0123));   // 0000000011111111
        const __m256i w23 = _mm256_cvtepu8_epi16(_mm_unpackhi_epi32(w0123, w0123));   // 2222222233333333
        const __m256i w45 = _mm256_cvtepu8_epi16(_mm_unpacklo_epi32(w4567, w4567));   // 4444444455555555
        const __m256i w67 = _mm256_cvtepu8_epi16(_mm_unpackhi_epi32(w4567, w4567));   // 6666666677777777

        const __m256i a = _mm256_broadcastsi128_si256(loada_si128(topPels));
        const __m256i b = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(-below_pred));
        const __m256i ab_lo = _mm256_unpacklo_epi16(a, b);
        const __m256i ab_hi = _mm256_unpackhi_epi16(a, b);
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(below_pred * 256 + 128));

        const int pitch2 = pitch * 2;
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(w01, w01, ab_lo, ab_hi, offset));
        dst += pitch2;
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(w23, w23, ab_lo, ab_hi, offset));
        dst += pitch2;
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(w45, w45, ab_lo, ab_hi, offset));
        dst += pitch2;
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(w67, w67, ab_lo, ab_hi, offset));
    }

    template<> void predict_intra_av1_avx2<TX_16X16, SMOOTH_V_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 16;
        const int32_t below_pred = leftPels[width - 1];

        const __m256i a = loada_si256(topPels);
        const __m256i b = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(-below_pred));
        const __m256i ab_lo = _mm256_unpacklo_epi16(a, b);
        const __m256i ab_hi = _mm256_unpackhi_epi16(a, b);
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(below_pred * 256 + 128));

        const int pitch2 = pitch * 2;
        for (int r = 0; r < width; r += 2, dst += pitch2) {
            storea_si256(dst + 0 * pitch, smooth_vh_pred(smooth_weights_16[r + 0], ab_lo, ab_hi, offset));
            storea_si256(dst + 1 * pitch, smooth_vh_pred(smooth_weights_16[r + 1], ab_lo, ab_hi, offset));
        }
    }

    template<> void predict_intra_av1_avx2<TX_32X32, SMOOTH_V_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int32_t width = 32;
        const int32_t below_pred = leftPels[width - 1];

        const __m256i a0 = loada_si256(topPels);
        const __m256i a1 = loada_si256(topPels + 16);
        const __m256i b = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(-below_pred));
        const __m256i ab0_lo = _mm256_unpacklo_epi16(a0, b);
        const __m256i ab0_hi = _mm256_unpackhi_epi16(a0, b);
        const __m256i ab1_lo = _mm256_unpacklo_epi16(a1, b);
        const __m256i ab1_hi = _mm256_unpackhi_epi16(a1, b);
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(below_pred * 256 + 128));

        for (int r = 0; r < width; r++, dst += pitch) {
            storea_si256(dst + 0, smooth_vh_pred(smooth_weights_32[r], ab0_lo, ab0_hi, offset));
            storea_si256(dst + 16, smooth_vh_pred(smooth_weights_32[r], ab1_lo, ab1_hi, offset));
        }
    }

    template<> void predict_intra_av1_avx2<TX_4X4, SMOOTH_H_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int width = 4;
        const uint16_t right_pred = topPels[width - 1];

        __m128i w_tmp = loadl_si32(smooth_weights_4);        // 0123************
        w_tmp = _mm_unpacklo_epi8(w_tmp, w_tmp);             // 00112233********
        w_tmp = _mm_unpacklo_epi64(w_tmp, w_tmp);            // 0011223300112233
        const __m256i weights = _mm256_cvtepu8_epi16(w_tmp); // 0011223300112233
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(right_pred * 256 + 128));

        const __m128i l_tmp = loadl_epi64(leftPels);
        const __m256i left = _mm256_setr_m128i(l_tmp, _mm_srli_si128(l_tmp, 2));        // L0 L1 L2 L3 ** ** ** **  L1 L2 L3 ** ** ** ** **
        const __m256i right = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(-right_pred));  // -R -R -R -R -R -R -R -R  -R -R -R -R -R -R -R -R
        const __m256i lr = _mm256_unpacklo_epi16(left, right);                          // L0 -R L1 -R L2 -R L3 -R  L1 -R L2 -R L3 -R ** **

        __m256i lr0 = shuffle32(lr, 0, 0, 0, 0);  // L0 -R L0 -R L0 -R L0 -R  L1 -R L1 -R L1 -R L1 -R
        __m256i lr1 = shuffle32(lr, 2, 2, 2, 2);  // L2 -R L2 -R L2 -R L2 -R  L3 -R L3 -R L3 -R L3 -R
        __m256i pred = smooth_vh_pred(weights, weights, lr0, lr1, offset);
        storel_epi64(dst + 0 * pitch, si128_lo(pred));
        storel_epi64(dst + 1 * pitch, si128_hi(pred));
        storeh_epi64(dst + 2 * pitch, si128_lo(pred));
        storeh_epi64(dst + 3 * pitch, si128_hi(pred));
    }

    template<> void predict_intra_av1_avx2<TX_8X8, SMOOTH_H_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int width = 8;
        const int32_t pitch2 = pitch * 2;
        const uint16_t right_pred = topPels[width - 1];

        __m128i w_tmp = loadl_epi64(smooth_weights_8); // 01234567********
        w_tmp = _mm_unpacklo_epi8(w_tmp, w_tmp);       // 0011223344556677
        const __m256i weights03 = _mm256_cvtepu8_epi16(_mm_unpacklo_epi64(w_tmp, w_tmp)); // 0011223300112233
        const __m256i weights47 = _mm256_cvtepu8_epi16(_mm_unpackhi_epi64(w_tmp, w_tmp)); // 4455667744556677
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(right_pred * 256 + 128));

        const __m128i l_tmp = loada_si128(leftPels);
        const __m256i left = _mm256_setr_m128i(l_tmp, _mm_srli_si128(l_tmp, 2));        // L0 L1 L2 L3 L4 L5 L6 L7  L1 L2 L3 L4 L5 L6 L7 **
        const __m256i right = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(-right_pred));  // -R -R -R -R -R -R -R -R  -R -R -R -R -R -R -R -R
        const __m256i lr0 = _mm256_unpacklo_epi16(left, right);                         // L0 -R L1 -R L2 -R L3 -R  L1 -R L2 -R L3 -R L4 -R
        const __m256i lr1 = _mm256_unpackhi_epi16(left, right);                         // L4 -R L5 -R L6 -R L7 -R  L5 -R L6 -R L7 -R ** -R

        __m256i lr;
        lr = shuffle32(lr0, 0, 0, 0, 0);
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(weights03, weights47, lr, lr, offset));
        dst += pitch2;
        lr = shuffle32(lr0, 2, 2, 2, 2);
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(weights03, weights47, lr, lr, offset));
        dst += pitch2;
        lr = shuffle32(lr1, 0, 0, 0, 0);
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(weights03, weights47, lr, lr, offset));
        dst += pitch2;
        lr = shuffle32(lr1, 2, 2, 2, 2);
        storea2_m128i(dst, dst + pitch, smooth_vh_pred(weights03, weights47, lr, lr, offset));
    }

    template<> void predict_intra_av1_avx2<TX_16X16, SMOOTH_H_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int width = 16;
        const uint16_t right_pred = topPels[width - 1];

        const __m256i weights = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_16));
        const __m256i weights0 = _mm256_unpacklo_epi16(weights, weights);
        const __m256i weights1 = _mm256_unpackhi_epi16(weights, weights);
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(right_pred * 256 + 128));

        const int32_t pitch2 = pitch * 2;
        for (int r = 0; r < width; r += 2, dst += pitch2) {
            const __m256i lr0 = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(leftPels[r + 0] | (-right_pred << 16)));
            const __m256i lr1 = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(leftPels[r + 1] | (-right_pred << 16)));
            storea_si256(dst + 0 * pitch, smooth_vh_pred(weights0, weights1, lr0, lr0, offset));
            storea_si256(dst + 1 * pitch, smooth_vh_pred(weights0, weights1, lr1, lr1, offset));
        }
    }

    template<> void predict_intra_av1_avx2<TX_32X32, SMOOTH_H_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int width = 32;
        const uint16_t right_pred = topPels[width - 1];

        const __m256i weights_lo = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_32));
        const __m256i weights_hi = _mm256_cvtepu8_epi16(loada_si128(smooth_weights_32 + 16));
        const __m256i weights0 = _mm256_unpacklo_epi16(weights_lo, weights_lo);
        const __m256i weights1 = _mm256_unpackhi_epi16(weights_lo, weights_lo);
        const __m256i weights2 = _mm256_unpacklo_epi16(weights_hi, weights_hi);
        const __m256i weights3 = _mm256_unpackhi_epi16(weights_hi, weights_hi);
        const __m256i offset = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(right_pred * 256 + 128));

        for (int r = 0; r < width; r++, dst += pitch) {
            const __m256i lr = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(leftPels[r] | (-right_pred << 16)));
            storea_si256(dst + 0, smooth_vh_pred(weights0, weights1, lr, lr, offset));
            storea_si256(dst + 16, smooth_vh_pred(weights2, weights3, lr, lr, offset));
        }
    }

    template<> void predict_intra_av1_avx2<TX_4X4, PAETH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const __m256i above = _mm256_broadcastq_epi64(loadl_epi64(topPels));                 // A0 A1 A2 A3 x4
        const __m256i above_left = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(topPels[-1]));  // AL x16

        const __m256i a_minus_al = _mm256_sub_epi16(above, above_left);
        const __m256i a_minus_2al = _mm256_sub_epi16(a_minus_al, above_left);
        const __m256i diff_l = _mm256_abs_epi16(a_minus_al);

        __m128i tmp = loadl_epi64(leftPels);  // L0 L1 L2 L3 ** ** ** **
        tmp = _mm_unpacklo_epi16(tmp, tmp);   // L0 L0 L1 L1 L2 L2 L3 L3
        const __m256i left = _mm256_setr_m128i(_mm_unpacklo_epi32(tmp, tmp), _mm_unpackhi_epi32(tmp, tmp)); // L0 L0 L0 L0 L1 L1 L1 L1 L2 L2 L2 L2 L3 L3 L3 L3

        const __m256i diff_above = _mm256_abs_epi16(_mm256_sub_epi16(left, above_left));

        const __m256i pred = paeth_pred(diff_above, diff_l, a_minus_2al, left, above, above_left);
        storel_epi64(dst + 0 * pitch, si128_lo(pred));
        storeh_epi64(dst + 1 * pitch, si128_lo(pred));
        storel_epi64(dst + 2 * pitch, si128_hi(pred));
        storeh_epi64(dst + 3 * pitch, si128_hi(pred));
    }

    template<> void predict_intra_av1_avx2<TX_8X8, PAETH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int width = 8;

        const __m256i above = _mm256_broadcastsi128_si256(loada_si128(topPels));             // A0 A1 A2 A3 A4 A5 A6 A7  A0 A1 A2 A3 A4 A5 A6 A7
        const __m256i above_left = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(topPels[-1]));  // AL x 16

        const __m256i a_minus_al = _mm256_sub_epi16(above, above_left);
        const __m256i a_minus_2al = _mm256_sub_epi16(a_minus_al, above_left);
        const __m256i diff_l = _mm256_abs_epi16(a_minus_al);

        const __m256i shuftab = _mm256_set1_epi16(0x0100);
        const __m128i left_m128i = loada_si128(leftPels);
        const __m128i tmp = _mm_abs_epi16(_mm_sub_epi16(left_m128i, si128_lo(above_left)));
        __m256i diff_above = _mm256_setr_m128i(tmp, _mm_srli_si128(tmp, 2));
        __m256i left = _mm256_setr_m128i(left_m128i, _mm_srli_si128(left_m128i, 2));

        const int32_t pitch2 = pitch * 2;
        for (int r = 0; r < width; r += 2, dst += pitch2) {
            const __m256i diff_a = _mm256_shuffle_epi8(diff_above, shuftab);
            const __m256i l = _mm256_shuffle_epi8(left, shuftab);
            const __m256i pred = paeth_pred(diff_a, diff_l, a_minus_2al, l, above, above_left);
            storea2_m128i(dst, dst + pitch, pred);
            diff_above = _mm256_srli_si256(diff_above, 4);
            left  = _mm256_srli_si256(left, 4);
        }
    }

    template<> void predict_intra_av1_avx2<TX_16X16, PAETH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int width = 16;

        const __m256i above = loada_si256(topPels);
        const __m256i above_left = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(topPels[-1]));

        const __m256i a_minus_al = _mm256_sub_epi16(above, above_left);
        const __m256i a_minus_2al = _mm256_sub_epi16(a_minus_al, above_left);
        const __m256i diff_l = _mm256_abs_epi16(a_minus_al);

        alignas(32) uint16_t diff_above_buf[width];
        const __m256i left = loada_si256(leftPels);
        storea_si256(diff_above_buf, _mm256_abs_epi16(_mm256_sub_epi16(left, above_left)));

        for (int r = 0; r < width; r++, dst += pitch) {
            const __m256i pred = paeth_pred(diff_above_buf[r], diff_l, a_minus_2al, leftPels[r], above, above_left);
            storea_si256(dst, pred);
        }
    }

    template<> void predict_intra_av1_avx2<TX_32X32, PAETH_PRED, uint16_t>(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int, int, int)
    {
        const int width = 32;

        const __m256i above0 = loada_si256(topPels);
        const __m256i above1 = loada_si256(topPels + 16);
        const __m256i above_left = _mm256_broadcastw_epi16(_mm_cvtsi32_si128(topPels[-1]));
        const __m256i a_minus_al0 = _mm256_sub_epi16(above0, above_left);
        const __m256i a_minus_al1 = _mm256_sub_epi16(above1, above_left);
        const __m256i a_minus_2al0 = _mm256_sub_epi16(a_minus_al0, above_left);
        const __m256i a_minus_2al1 = _mm256_sub_epi16(a_minus_al1, above_left);
        const __m256i diff_l0 = _mm256_abs_epi16(a_minus_al0);
        const __m256i diff_l1 = _mm256_abs_epi16(a_minus_al1);

        alignas(32) uint16_t diff_above_buf[width];
        storea_si256(diff_above_buf,
            _mm256_abs_epi16(_mm256_sub_epi16(above_left, loada_si256(leftPels))));
        storea_si256(diff_above_buf + 16,
            _mm256_abs_epi16(_mm256_sub_epi16(above_left, loada_si256(leftPels + 16))));

        for (int r = 0; r < width; r++, dst += pitch) {
            storea_si256(dst,
                paeth_pred(diff_above_buf[r], diff_l0, a_minus_2al0, leftPels[r], above0, above_left));
            storea_si256(dst + 16,
                paeth_pred(diff_above_buf[r], diff_l1, a_minus_2al1, leftPels[r], above1, above_left));
        }
    }





    namespace {

        alignas(32) static const uint8_t shuftab_uvsplit_10bit[32] = { 0,1,4,5,8,9,12,13,2,3,6,7,10,11,14,15, 0,1,4,5,8,9,12,13,2,3,6,7,10,11,14,15 };

        template <int size> void convert_ref_pels_av1(const uint16_t *refPel, uint16_t *refPelU, uint16_t *refPelV)
        {
            refPelU[-1] = refPel[-2];
            refPelV[-1] = refPel[-1];
            const __m256i shuftab = loada_si256(shuftab_uvsplit_10bit);
            const int numpels = 8 << size; // 2width
            for (int i = 0; i < numpels; i += 8) {
                __m256i uv = loada_si256(refPel + 2 * i);
                uv = _mm256_shuffle_epi8(uv, shuftab);
                storel_epi64(refPelU + i + 0, si128_lo(uv));
                storel_epi64(refPelU + i + 4, si128_hi(uv));
                storeh_epi64(refPelV + i + 0, si128_lo(uv));
                storeh_epi64(refPelV + i + 4, si128_hi(uv));
            }
        }

        template <int size> void convert_pred_block(uint16_t *dst, int pitch)
        {
            if (size == 0) {
                __m256i t0 = loada2_m128i(dst + 0 * pitch, dst + 1 * pitch);  // U0 V0 U1 V1
                __m256i t1 = loada2_m128i(dst + 2 * pitch, dst + 3 * pitch);  // U2 V2 U3 V3
                __m256i u = _mm256_unpacklo_epi64(t0, t1);                    // U0 U2 U1 U3
                __m256i v = _mm256_unpackhi_epi64(t0, t1);                    // V0 V2 V1 V3
                __m256i uv0 = _mm256_unpacklo_epi16(u, v);
                __m256i uv1 = _mm256_unpackhi_epi16(u, v);
                storea2_m128i(dst + 0 * pitch, dst + 1 * pitch, uv0);
                storea2_m128i(dst + 2 * pitch, dst + 3 * pitch, uv1);
            } else if (size == 1) {
                for (int i = 0; i < 8; i++, dst += pitch) {
                    __m256i tmp = loada_si256(dst);            // u00 u01 u02 u03 u04 u05 u06 u07 | v00 v01 v02 v03 v04 v05 v06 v07
                    __m256i u = permute4x64(tmp, 0, 2, 1, 3);  // u00 u01 u02 u03 v00 v01 v02 v03 | u04 u05 u06 u07 v04 v05 v06 v07
                    __m256i v = _mm256_srli_si256(u, 8);       // v00 v01 v02 v03  0   0   0   0  | v04 v05 v06 v07  0   0   0   0
                    __m256i uv = _mm256_unpacklo_epi16(u, v);  // u00 v00 u01 v01 u02 v02 u03 v03 | u04 v04 u05 v05 u06 v06 u07 v07
                    storea_si256(dst, uv);
                }
            } else if (size == 2) {
                for (int i = 0; i < 16; i++, dst += pitch) {
                    __m256i u = loada_si256(dst + 0);    // u00..15
                    __m256i v = loada_si256(dst + 16);   // v00..15
                    __m256i uv0 = _mm256_unpacklo_epi16(u, v);  // uv00..03 | uv08..11
                    __m256i uv1 = _mm256_unpackhi_epi16(u, v);  // uv04..07 | uv12..15
                    storea_si256(dst + 0, permute2x128(uv0, uv1, 0, 2));
                    storea_si256(dst + 16, permute2x128(uv0, uv1, 1, 3));
                }
            } else {
                assert(size == 3);
                for (int i = 0; i < 32; i++, dst += pitch) {
                    __m256i u0 = loada_si256(dst + 0);     // u00..15
                    __m256i u1 = loada_si256(dst + 16);    // u16..31
                    __m256i v0 = loada_si256(dst + 32);    // v00..15
                    __m256i v1 = loada_si256(dst + 48);    // v16..31
                    __m256i uv0 = _mm256_unpacklo_epi16(u0, v0);  // uv00..03 | uv08..11
                    __m256i uv1 = _mm256_unpackhi_epi16(u0, v0);  // uv04..07 | uv12..15
                    __m256i uv2 = _mm256_unpacklo_epi16(u1, v1);  // uv16..19 | uv24..27
                    __m256i uv3 = _mm256_unpackhi_epi16(u1, v1);  // uv20..23 | uv28..31
                    storea_si256(dst + 0, permute2x128(uv0, uv1, 0, 2));
                    storea_si256(dst + 16, permute2x128(uv0, uv1, 1, 3));
                    storea_si256(dst + 32, permute2x128(uv2, uv3, 0, 2));
                    storea_si256(dst + 48, permute2x128(uv2, uv3, 1, 3));
                }
            }
        }

        template <int txSize> void predict_intra_nv12_ver_av1_avx2(const uint16_t *topPels, uint16_t *dst, int pitch);

        template <> void predict_intra_nv12_ver_av1_avx2<TX_4X4>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            const __m128i above = loada_si128(topPels);

            storea_si128(dst, above); storea_si128(dst + pitch, above); dst += pitch2;
            storea_si128(dst, above); storea_si128(dst + pitch, above);
        }

        template <> void predict_intra_nv12_ver_av1_avx2<TX_8X8>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            const __m256i above = loada_si256(topPels);

            storea_si256(dst, above); storea_si256(dst + pitch, above); dst += pitch2;
            storea_si256(dst, above); storea_si256(dst + pitch, above); dst += pitch2;
            storea_si256(dst, above); storea_si256(dst + pitch, above); dst += pitch2;
            storea_si256(dst, above); storea_si256(dst + pitch, above);
        }

        template <> void predict_intra_nv12_ver_av1_avx2<TX_16X16>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int width = 16;
            const int pitch2 = pitch * 2;
            const __m256i above0 = loada_si256(topPels);
            const __m256i above1 = loada_si256(topPels + 16);

            for (int r = 0; r < width; r += 4, dst += pitch2) {
                storea_si256(dst, above0);
                storea_si256(dst + 16, above1);
                storea_si256(dst + pitch, above0);
                storea_si256(dst + pitch + 16, above1);
                dst += pitch2;
                storea_si256(dst, above0);
                storea_si256(dst + 16, above1);
                storea_si256(dst + pitch, above0);
                storea_si256(dst + pitch + 16, above1);
            }
        }

        template <> void predict_intra_nv12_ver_av1_avx2<TX_32X32>(const uint16_t *topPels, uint16_t *dst, int pitch)
        {
            const int width = 32;
            const int pitch2 = pitch * 2;
            const __m256i above0 = loada_si256(topPels);
            const __m256i above1 = loada_si256(topPels + 16);
            const __m256i above2 = loada_si256(topPels + 32);
            const __m256i above3 = loada_si256(topPels + 48);

            for (int r = 0; r < width; r += 2, dst += pitch2) {
                storea_si256(dst, above0);
                storea_si256(dst + 16, above1);
                storea_si256(dst + 32, above2);
                storea_si256(dst + 48, above3);
                storea_si256(dst + pitch, above0);
                storea_si256(dst + pitch + 16, above1);
                storea_si256(dst + pitch + 32, above2);
                storea_si256(dst + pitch + 48, above3);
            }
        }

        template <int txSize> void predict_intra_nv12_hor_av1_avx2(const uint16_t *leftPels, uint16_t *dst, int pitch);

        template <> void predict_intra_nv12_hor_av1_avx2<TX_4X4>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            __m128i left = loada_si128(leftPels);
            storea_si128(dst + 0 * pitch, _mm_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4);
            storea_si128(dst + 1 * pitch, _mm_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4), dst += pitch2;
            storea_si128(dst + 0 * pitch, _mm_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4);
            storea_si128(dst + 1 * pitch, _mm_broadcastd_epi32(left));
        }

        template <> void predict_intra_nv12_hor_av1_avx2<TX_8X8>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            __m128i left = loada_si128(leftPels);
            storea_si256(dst + 0 * pitch, _mm256_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4);
            storea_si256(dst + 1 * pitch, _mm256_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4), dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4);
            storea_si256(dst + 1 * pitch, _mm256_broadcastd_epi32(left)); dst += pitch2;
            left = loada_si128(leftPels + 8);
            storea_si256(dst + 0 * pitch, _mm256_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4);
            storea_si256(dst + 1 * pitch, _mm256_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4), dst += pitch2;
            storea_si256(dst + 0 * pitch, _mm256_broadcastd_epi32(left)); left = _mm_srli_si128(left, 4);
            storea_si256(dst + 1 * pitch, _mm256_broadcastd_epi32(left));
        }

        template <> void predict_intra_nv12_hor_av1_avx2<TX_16X16>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            __m128i left;
            __m256i l;
            for (int i = 0; i < 4; i++) {
                left = loada_si128(leftPels + 8 * i);
                l = _mm256_broadcastd_epi32(left);
                storea_si256(dst + 0 * pitch, l);
                storea_si256(dst + 0 * pitch + 16, l);
                left = _mm_srli_si128(left, 4);
                l = _mm256_broadcastd_epi32(left);
                storea_si256(dst + 1 * pitch, l);
                storea_si256(dst + 1 * pitch + 16, l);
                left = _mm_srli_si128(left, 4);
                l = _mm256_broadcastd_epi32(left);
                dst += pitch2;
                storea_si256(dst + 0 * pitch, l);
                storea_si256(dst + 0 * pitch + 16, l);
                left = _mm_srli_si128(left, 4);
                l = _mm256_broadcastd_epi32(left);
                storea_si256(dst + 1 * pitch, l);
                storea_si256(dst + 1 * pitch + 16, l);
                dst += pitch2;
            }
        }

        template <> void predict_intra_nv12_hor_av1_avx2<TX_32X32>(const uint16_t *leftPels, uint16_t *dst, int pitch)
        {
            const int pitch2 = pitch * 2;
            __m128i left;
            __m256i l;
            for (int i = 0; i < 8; i++) {
                left = loada_si128(leftPels + 8 * i);
                l = _mm256_broadcastd_epi32(left);
                storea_si256(dst + 0 * pitch, l);
                storea_si256(dst + 0 * pitch + 16, l);
                storea_si256(dst + 0 * pitch + 32, l);
                storea_si256(dst + 0 * pitch + 48, l);
                left = _mm_srli_si128(left, 4);
                l = _mm256_broadcastd_epi32(left);
                storea_si256(dst + 1 * pitch, l);
                storea_si256(dst + 1 * pitch + 16, l);
                storea_si256(dst + 1 * pitch + 32, l);
                storea_si256(dst + 1 * pitch + 48, l);
                left = _mm_srli_si128(left, 4);
                l = _mm256_broadcastd_epi32(left);
                dst += pitch2;
                storea_si256(dst + 0 * pitch, l);
                storea_si256(dst + 0 * pitch + 16, l);
                storea_si256(dst + 0 * pitch + 32, l);
                storea_si256(dst + 0 * pitch + 48, l);
                left = _mm_srli_si128(left, 4);
                l = _mm256_broadcastd_epi32(left);
                storea_si256(dst + 1 * pitch, l);
                storea_si256(dst + 1 * pitch + 16, l);
                storea_si256(dst + 1 * pitch + 32, l);
                storea_si256(dst + 1 * pitch + 48, l);
                dst += pitch2;
            }
        }
    };  // anonimous namespace

    template <int size, int mode, typename PixType>
    void predict_intra_nv12_av1_avx2(const PixType *topPels, const PixType *leftPels, PixType *dst, int pitch, int delta, int upTop, int upLeft)
    {
        if (mode == V_PRED && delta == 0) return predict_intra_nv12_ver_av1_avx2<size>(topPels, dst, pitch);
        if (mode == H_PRED && delta == 0) return predict_intra_nv12_hor_av1_avx2<size>(leftPels, dst, pitch);

        const int width = 4 << size;
        assert(size_t(topPels)  % 32 == 0);
        assert(size_t(leftPels) % 32 == 0);
        IntraPredPels<PixType> predPelsU;
        IntraPredPels<PixType> predPelsV;

        convert_ref_pels_av1<size>(topPels,  predPelsU.top,  predPelsV.top);
        convert_ref_pels_av1<size>(leftPels, predPelsU.left, predPelsV.left);
        predict_intra_av1_avx2<size, mode>(predPelsU.top, predPelsU.left, dst,         pitch, delta, upTop, upLeft);
        predict_intra_av1_avx2<size, mode>(predPelsV.top, predPelsV.left, dst + width, pitch, delta, upTop, upLeft);
        convert_pred_block<size>(dst, pitch);
    }
    template void predict_intra_nv12_av1_avx2<TX_4X4,   V_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   V_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, V_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, V_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   H_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   H_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, H_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, H_PRED,        uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D45_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D45_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D45_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D45_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D63_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D63_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D63_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D63_PRED,      uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D117_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D117_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D117_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D117_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D135_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D135_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D135_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D135_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D153_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D153_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D153_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D153_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   D207_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   D207_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, D207_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, D207_PRED,     uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   PAETH_PRED,    uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   PAETH_PRED,    uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, PAETH_PRED,    uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, PAETH_PRED,    uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_PRED,   uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_PRED,   uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_PRED,   uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_PRED,   uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_V_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_V_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_V_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_V_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_4X4,   SMOOTH_H_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_8X8,   SMOOTH_H_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_16X16, SMOOTH_H_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
    template void predict_intra_nv12_av1_avx2<TX_32X32, SMOOTH_H_PRED, uint16_t>(const uint16_t*,const uint16_t*,uint16_t*,int,int,int,int);
};  // namespace AV1PP
