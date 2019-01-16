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
#include "ipps.h"
#include "mfx_av1_opts_intrin.h"

#include <stdint.h>

namespace VP9PP
{
    enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4 };
    typedef int16_t tran_low_t;

    namespace details {
        ALIGN_DECL(32) const uint8_t shuftab_dc_ac[32] = { 0,1,2,3,2,3,2,3,2,3,2,3,2,3,2,3, 2,3,2,3,2,3,2,3,2,3,2,3,2,3,2,3 };

        __m256i quant16(const int16_t *pcoef, int16_t *pqcoef, __m256i *zcount, __m256i zbin, __m256i round, __m256i quant, __m256i shift) {
            __m256i signedCoef = loada_si256(pcoef);
            __m256i coef = _mm256_abs_epi16(signedCoef);
            __m256i deadzoned = _mm256_cmpgt_epi16(zbin, coef);
            __m256i tmp = _mm256_adds_epi16(coef, round);                      // tmp = SAT(coef + round)
            tmp = _mm256_add_epi16(tmp, _mm256_mulhi_epi16(tmp, quant));       // tmp += (tmp * quant >> 16)
            tmp = _mm256_mulhi_epi16(tmp, shift);                              // tmp = tmp * shift >> 16
            tmp = _mm256_blendv_epi8(tmp, _mm256_setzero_si256(), deadzoned);  // tmp = 0 if coef < zbin
            __m256i qcoef = _mm256_sign_epi16(tmp, signedCoef);                // restore sign
            storea_si256(pqcoef, qcoef);
            __m256i zeroed = _mm256_cmpeq_epi16(tmp, _mm256_setzero_si256());
            *zcount = _mm256_add_epi16(*zcount, zeroed);                       // count zero coefs
            return qcoef;
        }

        template <int32_t txSize> void dequant16(__m256i qcoef, int16_t *pdqcoef, __m256i scale) {
            __m256i lo = _mm256_mullo_epi16(qcoef, scale);
            __m256i hi = _mm256_mulhi_epi16(qcoef, scale);
            __m256i half1 = _mm256_unpacklo_epi16(lo, hi);
            __m256i half2 = _mm256_unpackhi_epi16(lo, hi);
            if (txSize == 3) {
                __m256i half1Abs = _mm256_srli_epi32(_mm256_abs_epi32(half1), 1);
                __m256i half2Abs = _mm256_srli_epi32(_mm256_abs_epi32(half2), 1);
                half1 = _mm256_sign_epi32(half1Abs, half1);
                half2 = _mm256_sign_epi32(half2Abs, half2);
            }
            __m256i dqcoef = _mm256_packs_epi32(half1, half2);
            storea_si256(pdqcoef, dqcoef);
        }
    };
    using namespace details;

    template <int txSize> int quant_avx2(const tran_low_t *coef_ptr, tran_low_t *qcoef_ptr, const int16_t fqpar[8])
    {
        __m128i qparam_ = loadu_si128(fqpar);   // 8w: zbin[dc] zbin[ac] round[dc] round[ac] quant[dc] quant[ac] shift[dc] shift[ac]
        __m256i qparam  = _mm256_inserti128_si256(si256(qparam_), qparam_, 1);

        __m256i shuftab1 = loada_si256(details::shuftab_dc_ac);
        __m256i zbin  = _mm256_shuffle_epi8(qparam, shuftab1);                          // 16w: zbin[dc]  zbin[ac]  zbin[ac] ...
        __m256i round = _mm256_shuffle_epi8(_mm256_bsrli_epi128(qparam,4), shuftab1);   // 16w: round[dc] round[ac] round[ac] ...
        __m256i quant = _mm256_shuffle_epi8(_mm256_bsrli_epi128(qparam,8), shuftab1);   // 16w: quant[dc] quant[ac] quant[ac] ...
        __m256i shift = _mm256_shuffle_epi8(_mm256_bsrli_epi128(qparam,12), shuftab1);  // 16w: shift[dc] shift[ac] shift[ac] ...
        if (txSize == 3) {
            zbin = _mm256_avg_epu16(zbin, _mm256_setzero_si256());
            round = _mm256_avg_epu16(round, _mm256_setzero_si256());
            shift = _mm256_slli_epi16(shift, 1);
        }

        __m256i zcount = _mm256_setzero_si256();
        __m256i zero = _mm256_setzero_si256();

        (void)quant16(coef_ptr, qcoef_ptr, &zcount, zbin, round, quant, shift);

        int32_t eob = 16<<(txSize<<1);

        if (txSize > 0) {
            zbin  = si256(_mm256_movehdup_ps(ps(zbin)));    // 16w: ac ac ac ac ...
            round = si256(_mm256_movehdup_ps(ps(round)));
            shift = si256(_mm256_movehdup_ps(ps(shift)));
            quant = si256(_mm256_movehdup_ps(ps(quant)));

            (void)quant16(coef_ptr + 16, qcoef_ptr + 16, &zcount, zbin, round, quant, shift);
            (void)quant16(coef_ptr + 32, qcoef_ptr + 32, &zcount, zbin, round, quant, shift);
            (void)quant16(coef_ptr + 48, qcoef_ptr + 48, &zcount, zbin, round, quant, shift);

            if (txSize > 1) {
                for (int32_t i = 4; i < (eob>>4); i += 4) {
                    (void)quant16(coef_ptr + 16 * i,      qcoef_ptr + 16 * i,      &zcount, zbin, round, quant, shift);
                    (void)quant16(coef_ptr + 16 * i + 16, qcoef_ptr + 16 * i + 16, &zcount, zbin, round, quant, shift);
                    (void)quant16(coef_ptr + 16 * i + 32, qcoef_ptr + 16 * i + 32, &zcount, zbin, round, quant, shift);
                    (void)quant16(coef_ptr + 16 * i + 48, qcoef_ptr + 16 * i + 48, &zcount, zbin, round, quant, shift);
                }
            }
        }

        __m128i zc = _mm_add_epi16(si128(zcount), _mm256_extracti128_si256(zcount, 1));
        zc = _mm_abs_epi16(zc);
        zc = _mm_sad_epu8(zc, si128(zero));
        zc = _mm_add_epi64(zc, _mm_bsrli_si128(zc, 8));
        eob -= _mm_cvtsi128_si32(zc); // subtract number of zero coefs

        return eob;
    }
    template int quant_avx2<TX_4X4>(const short *,short *,const short *);
    template int quant_avx2<TX_8X8>(const short *,short *,const short *);
    template int quant_avx2<TX_16X16>(const short *,short *,const short *);
    template int quant_avx2<TX_32X32>(const short *,short *,const short *);


    template <int txSize> void dequant_avx2(const int16_t *src, int16_t *dst, const int16_t *scales) {
        __m128i dqparam_ = loadu_si32(scales);
        __m256i dqparam  = _mm256_inserti128_si256(si256(dqparam_), dqparam_, 1);
        __m256i scale = _mm256_shuffle_epi8(dqparam, loada_si256(details::shuftab_dc_ac));   // 16w: dqscale[dc] dqscale[ac] dqscale[ac] ...

        dequant16<txSize>(loada_si256(src), dst, scale);

        if (txSize > 0) {
            scale = si256(_mm256_movehdup_ps(ps(scale)));

            dequant16<txSize>(loada_si256(src + 16), dst + 16, scale);
            dequant16<txSize>(loada_si256(src + 32), dst + 32, scale);
            dequant16<txSize>(loada_si256(src + 48), dst + 48, scale);

            if (txSize > 1) {
                const int32_t ncoefs = 16 << (txSize << 1);
                for (int32_t i = 64; i < ncoefs; i += 64) {
                    dequant16<txSize>(loada_si256(src + i),      dst + i,      scale);
                    dequant16<txSize>(loada_si256(src + i + 16), dst + i + 16, scale);
                    dequant16<txSize>(loada_si256(src + i + 32), dst + i + 32, scale);
                    dequant16<txSize>(loada_si256(src + i + 48), dst + i + 48, scale);
                }
            }
        }
    }
    template void dequant_avx2<TX_4X4>(const int16_t *src_, int16_t *dst_, const int16_t *scales);
    template void dequant_avx2<TX_8X8>(const int16_t *src_, int16_t *dst_, const int16_t *scales);
    template void dequant_avx2<TX_16X16>(const int16_t *src_, int16_t *dst_, const int16_t *scales);
    template void dequant_avx2<TX_32X32>(const int16_t *src_, int16_t *dst_, const int16_t *scales);


    template <int txSize> int quant_dequant_avx2(int16_t *coef_ptr, int16_t *qcoef_ptr, const int16_t qpar[10]) {
        int32_t eob = quant_avx2<txSize>(coef_ptr, qcoef_ptr, qpar);
        if (eob)
            dequant_avx2<txSize>(qcoef_ptr, coef_ptr, qpar + 8);
        return eob;
    }
    template int quant_dequant_avx2<0>(int16_t*,int16_t*,const int16_t*);
    template int quant_dequant_avx2<1>(int16_t*,int16_t*,const int16_t*);
    template int quant_dequant_avx2<2>(int16_t*,int16_t*,const int16_t*);
    template int quant_dequant_avx2<3>(int16_t*,int16_t*,const int16_t*);

}; // namespace VP9PP

