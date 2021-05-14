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
#include "ipps.h"
#include <emmintrin.h>  // SSE2
#include "txfm_common.h"
#include "fwd_txfm_sse2.h"

namespace AV1PP { namespace details
{
    typedef int16_t int16_t;
    typedef int16_t tran_low_t;

#define pair_set_epi16(a, b)                                            \
    _mm_set_epi16((int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
    (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a))

#define dual_set_epi16(a, b)                                            \
    _mm_set_epi16((int16_t)(b), (int16_t)(b), (int16_t)(b), (int16_t)(b), \
    (int16_t)(a), (int16_t)(a), (int16_t)(a), (int16_t)(a))

#define octa_set_epi16(a, b, c, d, e, f, g, h)                           \
    _mm_setr_epi16((int16_t)(a), (int16_t)(b), (int16_t)(c), (int16_t)(d), \
    (int16_t)(e), (int16_t)(f), (int16_t)(g), (int16_t)(h))


#if DCT_HIGH_BIT_DEPTH
#define ADD_EPI16 _mm_adds_epi16
#define SUB_EPI16 _mm_subs_epi16

#else
#define ADD_EPI16 _mm_add_epi16
#define SUB_EPI16 _mm_sub_epi16
#endif


    //-----------------------------------------------------
    //                4x4
    //-----------------------------------------------------

    void fdct4x4_sse2(const int16_t *input, int16_t *output, int stride)
    {

        // This 2D transform implements 4 vertical 1D transforms followed
        // by 4 horizontal 1D transforms.  The multiplies and adds are as given
        // by Chen, Smith and Fralick ('77).  The commands for moving the data
        // around have been minimized by hand.
        // For the purposes of the comments, the 16 inputs are referred to at i0
        // through iF (in raster order), intermediate variables are a0, b0, c0
        // through f, and correspond to the in-place computations mapped to input
        // locations.  The outputs, o0 through oF are labeled according to the
        // output locations.

        // Constants
        // These are the coefficients used for the multiplies.
        // In the comments, pN means cos(N pi /64) and mN is -cos(N pi /64),
        // where cospi_N_64 = cos(N pi /64)
        const __m128i k__cospi_A =
            octa_set_epi16(cospi_16_64, cospi_16_64, cospi_16_64, cospi_16_64,
            cospi_16_64, -cospi_16_64, cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_B =
            octa_set_epi16(cospi_16_64, -cospi_16_64, cospi_16_64, -cospi_16_64,
            cospi_16_64, cospi_16_64, cospi_16_64, cospi_16_64);
        const __m128i k__cospi_C =
            octa_set_epi16(cospi_8_64, cospi_24_64, cospi_8_64, cospi_24_64,
            cospi_24_64, -cospi_8_64, cospi_24_64, -cospi_8_64);
        const __m128i k__cospi_D =
            octa_set_epi16(cospi_24_64, -cospi_8_64, cospi_24_64, -cospi_8_64,
            cospi_8_64, cospi_24_64, cospi_8_64, cospi_24_64);
        const __m128i k__cospi_E =
            octa_set_epi16(cospi_16_64, cospi_16_64, cospi_16_64, cospi_16_64,
            cospi_16_64, cospi_16_64, cospi_16_64, cospi_16_64);
        const __m128i k__cospi_F =
            octa_set_epi16(cospi_16_64, -cospi_16_64, cospi_16_64, -cospi_16_64,
            cospi_16_64, -cospi_16_64, cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_G =
            octa_set_epi16(cospi_8_64, cospi_24_64, cospi_8_64, cospi_24_64,
            -cospi_8_64, -cospi_24_64, -cospi_8_64, -cospi_24_64);
        const __m128i k__cospi_H =
            octa_set_epi16(cospi_24_64, -cospi_8_64, cospi_24_64, -cospi_8_64,
            -cospi_24_64, cospi_8_64, -cospi_24_64, cospi_8_64);

        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
        // This second rounding constant saves doing some extra adds at the end
        const __m128i k__DCT_CONST_ROUNDING2 =
            _mm_set1_epi32(DCT_CONST_ROUNDING + (DCT_CONST_ROUNDING << 1));
        const int DCT_CONST_BITS2 = DCT_CONST_BITS + 2;
        const __m128i k__nonzero_bias_a = _mm_setr_epi16(0, 1, 1, 1, 1, 1, 1, 1);
        const __m128i k__nonzero_bias_b = _mm_setr_epi16(1, 0, 0, 0, 0, 0, 0, 0);
        __m128i in0, in1;
#if DCT_HIGH_BIT_DEPTH
        __m128i cmp0, cmp1;
        int test, overflow;
#endif

        // Load inputs.
        in0 = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
        in1 = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
        in1 = _mm_unpacklo_epi64(
            in1, _mm_loadl_epi64((const __m128i *)(input + 2 * stride)));
        in0 = _mm_unpacklo_epi64(
            in0, _mm_loadl_epi64((const __m128i *)(input + 3 * stride)));
        // in0 = [i0 i1 i2 i3 iC iD iE iF]
        // in1 = [i4 i5 i6 i7 i8 i9 iA iB]
#if DCT_HIGH_BIT_DEPTH
        // Check inputs small enough to use optimised code
        cmp0 = _mm_xor_si128(_mm_cmpgt_epi16(in0, _mm_set1_epi16(0x3ff)),
            _mm_cmplt_epi16(in0, _mm_set1_epi16(0xfc00)));
        cmp1 = _mm_xor_si128(_mm_cmpgt_epi16(in1, _mm_set1_epi16(0x3ff)),
            _mm_cmplt_epi16(in1, _mm_set1_epi16(0xfc00)));
        test = _mm_movemask_epi8(_mm_or_si128(cmp0, cmp1));
        if (test) {
            vpx_highbd_fdct4x4_c(input, output, stride);
            return;
        }
#endif  // DCT_HIGH_BIT_DEPTH

        // multiply by 16 to give some extra precision
        in0 = _mm_slli_epi16(in0, 4);
        in1 = _mm_slli_epi16(in1, 4);
        // if (i == 0 && input[0]) input[0] += 1;
        // add 1 to the upper left pixel if it is non-zero, which helps reduce
        // the round-trip error
        {
            // The mask will only contain whether the first value is zero, all
            // other comparison will fail as something shifted by 4 (above << 4)
            // can never be equal to one. To increment in the non-zero case, we
            // add the mask and one for the first element:
            //   - if zero, mask = -1, v = v - 1 + 1 = v
            //   - if non-zero, mask = 0, v = v + 0 + 1 = v + 1
            __m128i mask = _mm_cmpeq_epi16(in0, k__nonzero_bias_a);
            in0 = _mm_add_epi16(in0, mask);
            in0 = _mm_add_epi16(in0, k__nonzero_bias_b);
        }
        // There are 4 total stages, alternating between an add/subtract stage
        // followed by an multiply-and-add stage.
        {
            // Stage 1: Add/subtract

            // in0 = [i0 i1 i2 i3 iC iD iE iF]
            // in1 = [i4 i5 i6 i7 i8 i9 iA iB]
            const __m128i r0 = _mm_unpacklo_epi16(in0, in1);
            const __m128i r1 = _mm_unpackhi_epi16(in0, in1);
            // r0 = [i0 i4 i1 i5 i2 i6 i3 i7]
            // r1 = [iC i8 iD i9 iE iA iF iB]
            const __m128i r2 = _mm_shuffle_epi32(r0, 0xB4);
            const __m128i r3 = _mm_shuffle_epi32(r1, 0xB4);
            // r2 = [i0 i4 i1 i5 i3 i7 i2 i6]
            // r3 = [iC i8 iD i9 iF iB iE iA]

            const __m128i t0 = _mm_add_epi16(r2, r3);
            const __m128i t1 = _mm_sub_epi16(r2, r3);
            // t0 = [a0 a4 a1 a5 a3 a7 a2 a6]
            // t1 = [aC a8 aD a9 aF aB aE aA]

            // Stage 2: multiply by constants (which gets us into 32 bits).
            // The constants needed here are:
            // k__cospi_A = [p16 p16 p16 p16 p16 m16 p16 m16]
            // k__cospi_B = [p16 m16 p16 m16 p16 p16 p16 p16]
            // k__cospi_C = [p08 p24 p08 p24 p24 m08 p24 m08]
            // k__cospi_D = [p24 m08 p24 m08 p08 p24 p08 p24]
            const __m128i u0 = _mm_madd_epi16(t0, k__cospi_A);
            const __m128i u2 = _mm_madd_epi16(t0, k__cospi_B);
            const __m128i u1 = _mm_madd_epi16(t1, k__cospi_C);
            const __m128i u3 = _mm_madd_epi16(t1, k__cospi_D);
            // Then add and right-shift to get back to 16-bit range
            const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
            const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
            const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
            const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
            const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
            const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
            const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
            const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
            // w0 = [b0 b1 b7 b6]
            // w1 = [b8 b9 bF bE]
            // w2 = [b4 b5 b3 b2]
            // w3 = [bC bD bB bA]
            const __m128i x0 = _mm_packs_epi32(w0, w1);
            const __m128i x1 = _mm_packs_epi32(w2, w3);
#if DCT_HIGH_BIT_DEPTH
            overflow = check_epi16_overflow_x2(&x0, &x1);
            if (overflow) {
                vpx_highbd_fdct4x4_c(input, output, stride);
                return;
            }
#endif  // DCT_HIGH_BIT_DEPTH
            // x0 = [b0 b1 b7 b6 b8 b9 bF bE]
            // x1 = [b4 b5 b3 b2 bC bD bB bA]
            in0 = _mm_shuffle_epi32(x0, 0xD8);
            in1 = _mm_shuffle_epi32(x1, 0x8D);
            // in0 = [b0 b1 b8 b9 b7 b6 bF bE]
            // in1 = [b3 b2 bB bA b4 b5 bC bD]
        }
        {
            // vertical DCTs finished. Now we do the horizontal DCTs.
            // Stage 3: Add/subtract

            const __m128i t0 = ADD_EPI16(in0, in1);
            const __m128i t1 = SUB_EPI16(in0, in1);
            // t0 = [c0 c1 c8 c9  c4  c5  cC  cD]
            // t1 = [c3 c2 cB cA -c7 -c6 -cF -cE]
#if DCT_HIGH_BIT_DEPTH
            overflow = check_epi16_overflow_x2(&t0, &t1);
            if (overflow) {
                vpx_highbd_fdct4x4_c(input, output, stride);
                return;
            }
#endif  // DCT_HIGH_BIT_DEPTH

            // Stage 4: multiply by constants (which gets us into 32 bits).
            {
                // The constants needed here are:
                // k__cospi_E = [p16 p16 p16 p16 p16 p16 p16 p16]
                // k__cospi_F = [p16 m16 p16 m16 p16 m16 p16 m16]
                // k__cospi_G = [p08 p24 p08 p24 m08 m24 m08 m24]
                // k__cospi_H = [p24 m08 p24 m08 m24 p08 m24 p08]
                const __m128i u0 = _mm_madd_epi16(t0, k__cospi_E);
                const __m128i u1 = _mm_madd_epi16(t0, k__cospi_F);
                const __m128i u2 = _mm_madd_epi16(t1, k__cospi_G);
                const __m128i u3 = _mm_madd_epi16(t1, k__cospi_H);
                // Then add and right-shift to get back to 16-bit range
                // but this combines the final right-shift as well to save operations
                // This unusual rounding operations is to maintain bit-accurate
                // compatibility with the c version of this function which has two
                // rounding steps in a row.
                const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING2);
                const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING2);
                const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING2);
                const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING2);
                const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS2);
                const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS2);
                const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS2);
                const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS2);
                // w0 = [o0 o4 o8 oC]
                // w1 = [o2 o6 oA oE]
                // w2 = [o1 o5 o9 oD]
                // w3 = [o3 o7 oB oF]
                // remember the o's are numbered according to the correct output location
                const __m128i x0 = _mm_packs_epi32(w0, w1);
                const __m128i x1 = _mm_packs_epi32(w2, w3);
#if DCT_HIGH_BIT_DEPTH
                overflow = check_epi16_overflow_x2(&x0, &x1);
                if (overflow) {
                    vpx_highbd_fdct4x4_c(input, output, stride);
                    return;
                }
#endif  // DCT_HIGH_BIT_DEPTH
                {
                    // x0 = [o0 o4 o8 oC o2 o6 oA oE]
                    // x1 = [o1 o5 o9 oD o3 o7 oB oF]
                    const __m128i y0 = _mm_unpacklo_epi16(x0, x1);
                    const __m128i y1 = _mm_unpackhi_epi16(x0, x1);
                    // y0 = [o0 o1 o4 o5 o8 o9 oC oD]
                    // y1 = [o2 o3 o6 o7 oA oB oE oF]
                    in0 = _mm_unpacklo_epi32(y0, y1);
                    // in0 = [o0 o1 o2 o3 o4 o5 o6 o7]
                    in1 = _mm_unpackhi_epi32(y0, y1);
                    // in1 = [o8 o9 oA oB oC oD oE oF]
                }
            }
        }
        // Post-condition (v + 1) >> 2 is now incorporated into previous
        // add and right-shift commands.  Only 2 store instructions needed
        // because we are using the fact that 1/3 are stored just after 0/2.
        storeu_output(&in0, output + 0 * 4);
        storeu_output(&in1, output + 2 * 4);
    }

    static inline void load_buffer_4x4(const int16_t *input, __m128i *in, int stride)
    {
        const __m128i k__nonzero_bias_a = _mm_setr_epi16(0, 1, 1, 1, 1, 1, 1, 1);
        const __m128i k__nonzero_bias_b = _mm_setr_epi16(1, 0, 0, 0, 0, 0, 0, 0);
        __m128i mask;

        in[0] = _mm_loadl_epi64((const __m128i *)(input + 0 * stride));
        in[1] = _mm_loadl_epi64((const __m128i *)(input + 1 * stride));
        in[2] = _mm_loadl_epi64((const __m128i *)(input + 2 * stride));
        in[3] = _mm_loadl_epi64((const __m128i *)(input + 3 * stride));

        in[0] = _mm_slli_epi16(in[0], 4);
        in[1] = _mm_slli_epi16(in[1], 4);
        in[2] = _mm_slli_epi16(in[2], 4);
        in[3] = _mm_slli_epi16(in[3], 4);

        mask = _mm_cmpeq_epi16(in[0], k__nonzero_bias_a);
        in[0] = _mm_add_epi16(in[0], mask);
        in[0] = _mm_add_epi16(in[0], k__nonzero_bias_b);
    }

    static inline void store_output(const __m128i *poutput, tran_low_t *dst_ptr)
    {
#if CONFIG_AV1_HIGHBITDEPTH
        const __m128i zero = _mm_setzero_si128();
        const __m128i sign_bits = _mm_cmplt_epi16(*poutput, zero);
        __m128i out0 = _mm_unpacklo_epi16(*poutput, sign_bits);
        __m128i out1 = _mm_unpackhi_epi16(*poutput, sign_bits);
        _mm_store_si128((__m128i *)(dst_ptr), out0);
        _mm_store_si128((__m128i *)(dst_ptr + 4), out1);
#else
        _mm_store_si128((__m128i *)(dst_ptr), *poutput);
#endif  // CONFIG_AV1_HIGHBITDEPTH
    }

    static inline void write_buffer_4x4(tran_low_t *output, __m128i *res)
    {
        const __m128i kOne = _mm_set1_epi16(1);
        __m128i in01 = _mm_unpacklo_epi64(res[0], res[1]);
        __m128i in23 = _mm_unpacklo_epi64(res[2], res[3]);
        __m128i out01 = _mm_add_epi16(in01, kOne);
        __m128i out23 = _mm_add_epi16(in23, kOne);
        out01 = _mm_srai_epi16(out01, 2);
        out23 = _mm_srai_epi16(out23, 2);
        store_output(&out01, (output + 0 * 8));
        store_output(&out23, (output + 1 * 8));
    }

    static inline void transpose_4x4(__m128i *res)
    {
        // Combine and transpose
        // 00 01 02 03 20 21 22 23
        // 10 11 12 13 30 31 32 33
        const __m128i tr0_0 = _mm_unpacklo_epi16(res[0], res[1]);
        const __m128i tr0_1 = _mm_unpackhi_epi16(res[0], res[1]);

        // 00 10 01 11 02 12 03 13
        // 20 30 21 31 22 32 23 33
        res[0] = _mm_unpacklo_epi32(tr0_0, tr0_1);
        res[2] = _mm_unpackhi_epi32(tr0_0, tr0_1);

        // 00 10 20 30 01 11 21 31
        // 02 12 22 32 03 13 23 33
        // only use the first 4 16-bit integers
        res[1] = _mm_unpackhi_epi64(res[0], res[0]);
        res[3] = _mm_unpackhi_epi64(res[2], res[2]);
    }

    static void fadst4_sse2(__m128i *in)
    {
        const __m128i k__sinpi_p01_p02 = pair_set_epi16(sinpi_1_9, sinpi_2_9);
        const __m128i k__sinpi_p04_m01 = pair_set_epi16(sinpi_4_9, -sinpi_1_9);
        const __m128i k__sinpi_p03_p04 = pair_set_epi16(sinpi_3_9, sinpi_4_9);
        const __m128i k__sinpi_m03_p02 = pair_set_epi16(-sinpi_3_9, sinpi_2_9);
        const __m128i k__sinpi_p03_p03 = _mm_set1_epi16((int16_t)sinpi_3_9);
        const __m128i kZero = _mm_set1_epi16(0);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
        __m128i u[8], v[8];
        __m128i in7 = _mm_add_epi16(in[0], in[1]);

        u[0] = _mm_unpacklo_epi16(in[0], in[1]);
        u[1] = _mm_unpacklo_epi16(in[2], in[3]);
        u[2] = _mm_unpacklo_epi16(in7, kZero);
        u[3] = _mm_unpacklo_epi16(in[2], kZero);
        u[4] = _mm_unpacklo_epi16(in[3], kZero);

        v[0] = _mm_madd_epi16(u[0], k__sinpi_p01_p02);  // s0 + s2
        v[1] = _mm_madd_epi16(u[1], k__sinpi_p03_p04);  // s4 + s5
        v[2] = _mm_madd_epi16(u[2], k__sinpi_p03_p03);  // x1
        v[3] = _mm_madd_epi16(u[0], k__sinpi_p04_m01);  // s1 - s3
        v[4] = _mm_madd_epi16(u[1], k__sinpi_m03_p02);  // -s4 + s6
        v[5] = _mm_madd_epi16(u[3], k__sinpi_p03_p03);  // s4
        v[6] = _mm_madd_epi16(u[4], k__sinpi_p03_p03);

        u[0] = _mm_add_epi32(v[0], v[1]);
        u[1] = _mm_sub_epi32(v[2], v[6]);
        u[2] = _mm_add_epi32(v[3], v[4]);
        u[3] = _mm_sub_epi32(u[2], u[0]);
        u[4] = _mm_slli_epi32(v[5], 2);
        u[5] = _mm_sub_epi32(u[4], v[5]);
        u[6] = _mm_add_epi32(u[3], u[5]);

        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);

        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);

        in[0] = _mm_packs_epi32(u[0], u[2]);
        in[1] = _mm_packs_epi32(u[1], u[3]);
        transpose_4x4(in);
    }

    static void fdct4_sse2(__m128i *in)
    {
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
        const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

        __m128i u[4], v[4];
        u[0] = _mm_unpacklo_epi16(in[0], in[1]);
        u[1] = _mm_unpacklo_epi16(in[3], in[2]);

        v[0] = _mm_add_epi16(u[0], u[1]);
        v[1] = _mm_sub_epi16(u[0], u[1]);

        u[0] = _mm_madd_epi16(v[0], k__cospi_p16_p16);  // 0
        u[1] = _mm_madd_epi16(v[0], k__cospi_p16_m16);  // 2
        u[2] = _mm_madd_epi16(v[1], k__cospi_p08_p24);  // 1
        u[3] = _mm_madd_epi16(v[1], k__cospi_p24_m08);  // 3

        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);

        in[0] = _mm_packs_epi32(u[0], u[1]);
        in[1] = _mm_packs_epi32(u[2], u[3]);
        transpose_4x4(in);
    }

    typedef enum {
        DCT_DCT = 0,    // DCT  in both horizontal and vertical
        ADST_DCT = 1,   // ADST in vertical, DCT in horizontal
        DCT_ADST = 2,   // DCT  in vertical, ADST in horizontal
        ADST_ADST = 3,  // ADST in both directions
        TX_TYPES = 4
    } TX_TYPE;

    template <int tx_type> void fht4x4_sse2(const int16_t *input, int16_t *output, int stride)
    {
        __m128i in[4];

        if (tx_type == DCT_DCT) {
            fdct4x4_sse2(input, output, stride);
        } else if (tx_type == ADST_DCT) {
            load_buffer_4x4(input, in, stride);
            fadst4_sse2(in);
            fdct4_sse2(in);
            write_buffer_4x4(output, in);
        } else if (tx_type == DCT_ADST) {
            load_buffer_4x4(input, in, stride);
            fdct4_sse2(in);
            fadst4_sse2(in);
            write_buffer_4x4(output, in);
        } else if (tx_type == ADST_ADST) {
            load_buffer_4x4(input, in, stride);
            fadst4_sse2(in);
            fadst4_sse2(in);
            write_buffer_4x4(output, in);
        } else {
            assert(0);
        }
    }

    //-----------------------------------------------------
    //                8x8
    //-----------------------------------------------------
    void fdct8x8_sse2(const int16_t *input, tran_low_t *output, int stride)
    {
        int pass;
        // Constants
        //    When we use them, in one case, they are all the same. In all others
        //    it's a pair of them that we need to repeat four times. This is done
        //    by constructing the 32 bit constant corresponding to that pair.
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
#if DCT_HIGH_BIT_DEPTH
        int overflow;
#endif
        // Load input
        __m128i in0 = _mm_load_si128((const __m128i *)(input + 0 * stride));
        __m128i in1 = _mm_load_si128((const __m128i *)(input + 1 * stride));
        __m128i in2 = _mm_load_si128((const __m128i *)(input + 2 * stride));
        __m128i in3 = _mm_load_si128((const __m128i *)(input + 3 * stride));
        __m128i in4 = _mm_load_si128((const __m128i *)(input + 4 * stride));
        __m128i in5 = _mm_load_si128((const __m128i *)(input + 5 * stride));
        __m128i in6 = _mm_load_si128((const __m128i *)(input + 6 * stride));
        __m128i in7 = _mm_load_si128((const __m128i *)(input + 7 * stride));
        // Pre-condition input (shift by two)
        in0 = _mm_slli_epi16(in0, 2);
        in1 = _mm_slli_epi16(in1, 2);
        in2 = _mm_slli_epi16(in2, 2);
        in3 = _mm_slli_epi16(in3, 2);
        in4 = _mm_slli_epi16(in4, 2);
        in5 = _mm_slli_epi16(in5, 2);
        in6 = _mm_slli_epi16(in6, 2);
        in7 = _mm_slli_epi16(in7, 2);

        // We do two passes, first the columns, then the rows. The results of the
        // first pass are transposed so that the same column code can be reused. The
        // results of the second pass are also transposed so that the rows (processed
        // as columns) are put back in row positions.
        for (pass = 0; pass < 2; pass++) {
            // To store results of each pass before the transpose.
            __m128i res0, res1, res2, res3, res4, res5, res6, res7;
            // Add/subtract
            const __m128i q0 = ADD_EPI16(in0, in7);
            const __m128i q1 = ADD_EPI16(in1, in6);
            const __m128i q2 = ADD_EPI16(in2, in5);
            const __m128i q3 = ADD_EPI16(in3, in4);
            const __m128i q4 = SUB_EPI16(in3, in4);
            const __m128i q5 = SUB_EPI16(in2, in5);
            const __m128i q6 = SUB_EPI16(in1, in6);
            const __m128i q7 = SUB_EPI16(in0, in7);
#if DCT_HIGH_BIT_DEPTH
            if (pass == 1) {
                overflow =
                    check_epi16_overflow_x8(&q0, &q1, &q2, &q3, &q4, &q5, &q6, &q7);
                if (overflow) {
                    vpx_highbd_fdct8x8_c(input, output, stride);
                    return;
                }
            }
#endif  // DCT_HIGH_BIT_DEPTH
            // Work on first four results
            {
                // Add/subtract
                const __m128i r0 = ADD_EPI16(q0, q3);
                const __m128i r1 = ADD_EPI16(q1, q2);
                const __m128i r2 = SUB_EPI16(q1, q2);
                const __m128i r3 = SUB_EPI16(q0, q3);
#if DCT_HIGH_BIT_DEPTH
                overflow = check_epi16_overflow_x4(&r0, &r1, &r2, &r3);
                if (overflow) {
                    vpx_highbd_fdct8x8_c(input, output, stride);
                    return;
                }
#endif  // DCT_HIGH_BIT_DEPTH
                // Interleave to do the multiply by constants which gets us into 32bits
                {
                    const __m128i t0 = _mm_unpacklo_epi16(r0, r1);
                    const __m128i t1 = _mm_unpackhi_epi16(r0, r1);
                    const __m128i t2 = _mm_unpacklo_epi16(r2, r3);
                    const __m128i t3 = _mm_unpackhi_epi16(r2, r3);
                    const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p16_p16);
                    const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p16_p16);
                    const __m128i u2 = _mm_madd_epi16(t0, k__cospi_p16_m16);
                    const __m128i u3 = _mm_madd_epi16(t1, k__cospi_p16_m16);
                    const __m128i u4 = _mm_madd_epi16(t2, k__cospi_p24_p08);
                    const __m128i u5 = _mm_madd_epi16(t3, k__cospi_p24_p08);
                    const __m128i u6 = _mm_madd_epi16(t2, k__cospi_m08_p24);
                    const __m128i u7 = _mm_madd_epi16(t3, k__cospi_m08_p24);
                    // dct_const_round_shift
                    const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
                    const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
                    const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
                    const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
                    const __m128i v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
                    const __m128i v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
                    const __m128i v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
                    const __m128i v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);
                    const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
                    const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
                    const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
                    const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
                    const __m128i w4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
                    const __m128i w5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
                    const __m128i w6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
                    const __m128i w7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
                    // Combine
                    res0 = _mm_packs_epi32(w0, w1);
                    res4 = _mm_packs_epi32(w2, w3);
                    res2 = _mm_packs_epi32(w4, w5);
                    res6 = _mm_packs_epi32(w6, w7);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x4(&res0, &res4, &res2, &res6);
                    if (overflow) {
                        vpx_highbd_fdct8x8_c(input, output, stride);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                }
            }
            // Work on next four results
            {
                // Interleave to do the multiply by constants which gets us into 32bits
                const __m128i d0 = _mm_unpacklo_epi16(q6, q5);
                const __m128i d1 = _mm_unpackhi_epi16(q6, q5);
                const __m128i e0 = _mm_madd_epi16(d0, k__cospi_p16_m16);
                const __m128i e1 = _mm_madd_epi16(d1, k__cospi_p16_m16);
                const __m128i e2 = _mm_madd_epi16(d0, k__cospi_p16_p16);
                const __m128i e3 = _mm_madd_epi16(d1, k__cospi_p16_p16);
                // dct_const_round_shift
                const __m128i f0 = _mm_add_epi32(e0, k__DCT_CONST_ROUNDING);
                const __m128i f1 = _mm_add_epi32(e1, k__DCT_CONST_ROUNDING);
                const __m128i f2 = _mm_add_epi32(e2, k__DCT_CONST_ROUNDING);
                const __m128i f3 = _mm_add_epi32(e3, k__DCT_CONST_ROUNDING);
                const __m128i s0 = _mm_srai_epi32(f0, DCT_CONST_BITS);
                const __m128i s1 = _mm_srai_epi32(f1, DCT_CONST_BITS);
                const __m128i s2 = _mm_srai_epi32(f2, DCT_CONST_BITS);
                const __m128i s3 = _mm_srai_epi32(f3, DCT_CONST_BITS);
                // Combine
                const __m128i r0 = _mm_packs_epi32(s0, s1);
                const __m128i r1 = _mm_packs_epi32(s2, s3);
#if DCT_HIGH_BIT_DEPTH
                overflow = check_epi16_overflow_x2(&r0, &r1);
                if (overflow) {
                    vpx_highbd_fdct8x8_c(input, output, stride);
                    return;
                }
#endif  // DCT_HIGH_BIT_DEPTH
                {
                    // Add/subtract
                    const __m128i x0 = ADD_EPI16(q4, r0);
                    const __m128i x1 = SUB_EPI16(q4, r0);
                    const __m128i x2 = SUB_EPI16(q7, r1);
                    const __m128i x3 = ADD_EPI16(q7, r1);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x4(&x0, &x1, &x2, &x3);
                    if (overflow) {
                        vpx_highbd_fdct8x8_c(input, output, stride);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                    // Interleave to do the multiply by constants which gets us into 32bits
                    {
                        const __m128i t0 = _mm_unpacklo_epi16(x0, x3);
                        const __m128i t1 = _mm_unpackhi_epi16(x0, x3);
                        const __m128i t2 = _mm_unpacklo_epi16(x1, x2);
                        const __m128i t3 = _mm_unpackhi_epi16(x1, x2);
                        const __m128i u0 = _mm_madd_epi16(t0, k__cospi_p28_p04);
                        const __m128i u1 = _mm_madd_epi16(t1, k__cospi_p28_p04);
                        const __m128i u2 = _mm_madd_epi16(t0, k__cospi_m04_p28);
                        const __m128i u3 = _mm_madd_epi16(t1, k__cospi_m04_p28);
                        const __m128i u4 = _mm_madd_epi16(t2, k__cospi_p12_p20);
                        const __m128i u5 = _mm_madd_epi16(t3, k__cospi_p12_p20);
                        const __m128i u6 = _mm_madd_epi16(t2, k__cospi_m20_p12);
                        const __m128i u7 = _mm_madd_epi16(t3, k__cospi_m20_p12);
                        // dct_const_round_shift
                        const __m128i v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
                        const __m128i v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
                        const __m128i v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
                        const __m128i v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
                        const __m128i v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
                        const __m128i v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
                        const __m128i v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
                        const __m128i v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);
                        const __m128i w0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
                        const __m128i w1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
                        const __m128i w2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
                        const __m128i w3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
                        const __m128i w4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
                        const __m128i w5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
                        const __m128i w6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
                        const __m128i w7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
                        // Combine
                        res1 = _mm_packs_epi32(w0, w1);
                        res7 = _mm_packs_epi32(w2, w3);
                        res5 = _mm_packs_epi32(w4, w5);
                        res3 = _mm_packs_epi32(w6, w7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&res1, &res7, &res5, &res3);
                        if (overflow) {
                            vpx_highbd_fdct8x8_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                }
            }
            // Transpose the 8x8.
            {
                // 00 01 02 03 04 05 06 07
                // 10 11 12 13 14 15 16 17
                // 20 21 22 23 24 25 26 27
                // 30 31 32 33 34 35 36 37
                // 40 41 42 43 44 45 46 47
                // 50 51 52 53 54 55 56 57
                // 60 61 62 63 64 65 66 67
                // 70 71 72 73 74 75 76 77
                const __m128i tr0_0 = _mm_unpacklo_epi16(res0, res1);
                const __m128i tr0_1 = _mm_unpacklo_epi16(res2, res3);
                const __m128i tr0_2 = _mm_unpackhi_epi16(res0, res1);
                const __m128i tr0_3 = _mm_unpackhi_epi16(res2, res3);
                const __m128i tr0_4 = _mm_unpacklo_epi16(res4, res5);
                const __m128i tr0_5 = _mm_unpacklo_epi16(res6, res7);
                const __m128i tr0_6 = _mm_unpackhi_epi16(res4, res5);
                const __m128i tr0_7 = _mm_unpackhi_epi16(res6, res7);
                // 00 10 01 11 02 12 03 13
                // 20 30 21 31 22 32 23 33
                // 04 14 05 15 06 16 07 17
                // 24 34 25 35 26 36 27 37
                // 40 50 41 51 42 52 43 53
                // 60 70 61 71 62 72 63 73
                // 54 54 55 55 56 56 57 57
                // 64 74 65 75 66 76 67 77
                const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
                const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_2, tr0_3);
                const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
                const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3);
                const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_4, tr0_5);
                const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
                const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_4, tr0_5);
                const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
                // 00 10 20 30 01 11 21 31
                // 40 50 60 70 41 51 61 71
                // 02 12 22 32 03 13 23 33
                // 42 52 62 72 43 53 63 73
                // 04 14 24 34 05 15 21 36
                // 44 54 64 74 45 55 61 76
                // 06 16 26 36 07 17 27 37
                // 46 56 66 76 47 57 67 77
                in0 = _mm_unpacklo_epi64(tr1_0, tr1_4);
                in1 = _mm_unpackhi_epi64(tr1_0, tr1_4);
                in2 = _mm_unpacklo_epi64(tr1_2, tr1_6);
                in3 = _mm_unpackhi_epi64(tr1_2, tr1_6);
                in4 = _mm_unpacklo_epi64(tr1_1, tr1_5);
                in5 = _mm_unpackhi_epi64(tr1_1, tr1_5);
                in6 = _mm_unpacklo_epi64(tr1_3, tr1_7);
                in7 = _mm_unpackhi_epi64(tr1_3, tr1_7);
                // 00 10 20 30 40 50 60 70
                // 01 11 21 31 41 51 61 71
                // 02 12 22 32 42 52 62 72
                // 03 13 23 33 43 53 63 73
                // 04 14 24 34 44 54 64 74
                // 05 15 25 35 45 55 65 75
                // 06 16 26 36 46 56 66 76
                // 07 17 27 37 47 57 67 77
            }
        }
        // Post-condition output and store it
        {
            // Post-condition (division by two)
            //    division of two 16 bits signed numbers using shifts
            //    n / 2 = (n - (n >> 15)) >> 1
            const __m128i sign_in0 = _mm_srai_epi16(in0, 15);
            const __m128i sign_in1 = _mm_srai_epi16(in1, 15);
            const __m128i sign_in2 = _mm_srai_epi16(in2, 15);
            const __m128i sign_in3 = _mm_srai_epi16(in3, 15);
            const __m128i sign_in4 = _mm_srai_epi16(in4, 15);
            const __m128i sign_in5 = _mm_srai_epi16(in5, 15);
            const __m128i sign_in6 = _mm_srai_epi16(in6, 15);
            const __m128i sign_in7 = _mm_srai_epi16(in7, 15);
            in0 = _mm_sub_epi16(in0, sign_in0);
            in1 = _mm_sub_epi16(in1, sign_in1);
            in2 = _mm_sub_epi16(in2, sign_in2);
            in3 = _mm_sub_epi16(in3, sign_in3);
            in4 = _mm_sub_epi16(in4, sign_in4);
            in5 = _mm_sub_epi16(in5, sign_in5);
            in6 = _mm_sub_epi16(in6, sign_in6);
            in7 = _mm_sub_epi16(in7, sign_in7);
            in0 = _mm_srai_epi16(in0, 1);
            in1 = _mm_srai_epi16(in1, 1);
            in2 = _mm_srai_epi16(in2, 1);
            in3 = _mm_srai_epi16(in3, 1);
            in4 = _mm_srai_epi16(in4, 1);
            in5 = _mm_srai_epi16(in5, 1);
            in6 = _mm_srai_epi16(in6, 1);
            in7 = _mm_srai_epi16(in7, 1);
            // store results
            store_output(&in0, (output + 0 * 8));
            store_output(&in1, (output + 1 * 8));
            store_output(&in2, (output + 2 * 8));
            store_output(&in3, (output + 3 * 8));
            store_output(&in4, (output + 4 * 8));
            store_output(&in5, (output + 5 * 8));
            store_output(&in6, (output + 6 * 8));
            store_output(&in7, (output + 7 * 8));
        }
    }

    // load 8x8 array
    static inline void load_buffer_8x8(const int16_t *input, __m128i *in, int stride)
    {
        in[0] = _mm_load_si128((const __m128i *)(input + 0 * stride));
        in[1] = _mm_load_si128((const __m128i *)(input + 1 * stride));
        in[2] = _mm_load_si128((const __m128i *)(input + 2 * stride));
        in[3] = _mm_load_si128((const __m128i *)(input + 3 * stride));
        in[4] = _mm_load_si128((const __m128i *)(input + 4 * stride));
        in[5] = _mm_load_si128((const __m128i *)(input + 5 * stride));
        in[6] = _mm_load_si128((const __m128i *)(input + 6 * stride));
        in[7] = _mm_load_si128((const __m128i *)(input + 7 * stride));

        in[0] = _mm_slli_epi16(in[0], 2);
        in[1] = _mm_slli_epi16(in[1], 2);
        in[2] = _mm_slli_epi16(in[2], 2);
        in[3] = _mm_slli_epi16(in[3], 2);
        in[4] = _mm_slli_epi16(in[4], 2);
        in[5] = _mm_slli_epi16(in[5], 2);
        in[6] = _mm_slli_epi16(in[6], 2);
        in[7] = _mm_slli_epi16(in[7], 2);
    }

    // perform in-place transpose
    static inline void array_transpose_8x8(__m128i *in, __m128i *res)
    {
        const __m128i tr0_0 = _mm_unpacklo_epi16(in[0], in[1]);
        const __m128i tr0_1 = _mm_unpacklo_epi16(in[2], in[3]);
        const __m128i tr0_2 = _mm_unpackhi_epi16(in[0], in[1]);
        const __m128i tr0_3 = _mm_unpackhi_epi16(in[2], in[3]);
        const __m128i tr0_4 = _mm_unpacklo_epi16(in[4], in[5]);
        const __m128i tr0_5 = _mm_unpacklo_epi16(in[6], in[7]);
        const __m128i tr0_6 = _mm_unpackhi_epi16(in[4], in[5]);
        const __m128i tr0_7 = _mm_unpackhi_epi16(in[6], in[7]);
        // 00 10 01 11 02 12 03 13
        // 20 30 21 31 22 32 23 33
        // 04 14 05 15 06 16 07 17
        // 24 34 25 35 26 36 27 37
        // 40 50 41 51 42 52 43 53
        // 60 70 61 71 62 72 63 73
        // 44 54 45 55 46 56 47 57
        // 64 74 65 75 66 76 67 77
        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
        const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_4, tr0_5);
        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
        const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_4, tr0_5);
        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_2, tr0_3);
        const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_2, tr0_3);
        const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
        // 00 10 20 30 01 11 21 31
        // 40 50 60 70 41 51 61 71
        // 02 12 22 32 03 13 23 33
        // 42 52 62 72 43 53 63 73
        // 04 14 24 34 05 15 25 35
        // 44 54 64 74 45 55 65 75
        // 06 16 26 36 07 17 27 37
        // 46 56 66 76 47 57 67 77
        res[0] = _mm_unpacklo_epi64(tr1_0, tr1_1);
        res[1] = _mm_unpackhi_epi64(tr1_0, tr1_1);
        res[2] = _mm_unpacklo_epi64(tr1_2, tr1_3);
        res[3] = _mm_unpackhi_epi64(tr1_2, tr1_3);
        res[4] = _mm_unpacklo_epi64(tr1_4, tr1_5);
        res[5] = _mm_unpackhi_epi64(tr1_4, tr1_5);
        res[6] = _mm_unpacklo_epi64(tr1_6, tr1_7);
        res[7] = _mm_unpackhi_epi64(tr1_6, tr1_7);
        // 00 10 20 30 40 50 60 70
        // 01 11 21 31 41 51 61 71
        // 02 12 22 32 42 52 62 72
        // 03 13 23 33 43 53 63 73
        // 04 14 24 34 44 54 64 74
        // 05 15 25 35 45 55 65 75
        // 06 16 26 36 46 56 66 76
        // 07 17 27 37 47 57 67 77
    }

    static void fadst8_sse2(__m128i *in)
    {
        // Constants
        const __m128i k__cospi_p02_p30 = pair_set_epi16(cospi_2_64, cospi_30_64);
        const __m128i k__cospi_p30_m02 = pair_set_epi16(cospi_30_64, -cospi_2_64);
        const __m128i k__cospi_p10_p22 = pair_set_epi16(cospi_10_64, cospi_22_64);
        const __m128i k__cospi_p22_m10 = pair_set_epi16(cospi_22_64, -cospi_10_64);
        const __m128i k__cospi_p18_p14 = pair_set_epi16(cospi_18_64, cospi_14_64);
        const __m128i k__cospi_p14_m18 = pair_set_epi16(cospi_14_64, -cospi_18_64);
        const __m128i k__cospi_p26_p06 = pair_set_epi16(cospi_26_64, cospi_6_64);
        const __m128i k__cospi_p06_m26 = pair_set_epi16(cospi_6_64, -cospi_26_64);
        const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
        const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
        const __m128i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__const_0 = _mm_set1_epi16(0);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

        __m128i u0, u1, u2, u3, u4, u5, u6, u7, u8, u9, u10, u11, u12, u13, u14, u15;
        __m128i v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;
        __m128i w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;
        __m128i s0, s1, s2, s3, s4, s5, s6, s7;
        __m128i in0, in1, in2, in3, in4, in5, in6, in7;

        // properly aligned for butterfly input
        in0 = in[7];
        in1 = in[0];
        in2 = in[5];
        in3 = in[2];
        in4 = in[3];
        in5 = in[4];
        in6 = in[1];
        in7 = in[6];

        // column transformation
        // stage 1
        // interleave and multiply/add into 32-bit integer
        s0 = _mm_unpacklo_epi16(in0, in1);
        s1 = _mm_unpackhi_epi16(in0, in1);
        s2 = _mm_unpacklo_epi16(in2, in3);
        s3 = _mm_unpackhi_epi16(in2, in3);
        s4 = _mm_unpacklo_epi16(in4, in5);
        s5 = _mm_unpackhi_epi16(in4, in5);
        s6 = _mm_unpacklo_epi16(in6, in7);
        s7 = _mm_unpackhi_epi16(in6, in7);

        u0 = _mm_madd_epi16(s0, k__cospi_p02_p30);
        u1 = _mm_madd_epi16(s1, k__cospi_p02_p30);
        u2 = _mm_madd_epi16(s0, k__cospi_p30_m02);
        u3 = _mm_madd_epi16(s1, k__cospi_p30_m02);
        u4 = _mm_madd_epi16(s2, k__cospi_p10_p22);
        u5 = _mm_madd_epi16(s3, k__cospi_p10_p22);
        u6 = _mm_madd_epi16(s2, k__cospi_p22_m10);
        u7 = _mm_madd_epi16(s3, k__cospi_p22_m10);
        u8 = _mm_madd_epi16(s4, k__cospi_p18_p14);
        u9 = _mm_madd_epi16(s5, k__cospi_p18_p14);
        u10 = _mm_madd_epi16(s4, k__cospi_p14_m18);
        u11 = _mm_madd_epi16(s5, k__cospi_p14_m18);
        u12 = _mm_madd_epi16(s6, k__cospi_p26_p06);
        u13 = _mm_madd_epi16(s7, k__cospi_p26_p06);
        u14 = _mm_madd_epi16(s6, k__cospi_p06_m26);
        u15 = _mm_madd_epi16(s7, k__cospi_p06_m26);

        // addition
        w0 = _mm_add_epi32(u0, u8);
        w1 = _mm_add_epi32(u1, u9);
        w2 = _mm_add_epi32(u2, u10);
        w3 = _mm_add_epi32(u3, u11);
        w4 = _mm_add_epi32(u4, u12);
        w5 = _mm_add_epi32(u5, u13);
        w6 = _mm_add_epi32(u6, u14);
        w7 = _mm_add_epi32(u7, u15);
        w8 = _mm_sub_epi32(u0, u8);
        w9 = _mm_sub_epi32(u1, u9);
        w10 = _mm_sub_epi32(u2, u10);
        w11 = _mm_sub_epi32(u3, u11);
        w12 = _mm_sub_epi32(u4, u12);
        w13 = _mm_sub_epi32(u5, u13);
        w14 = _mm_sub_epi32(u6, u14);
        w15 = _mm_sub_epi32(u7, u15);

        // shift and rounding
        v0 = _mm_add_epi32(w0, k__DCT_CONST_ROUNDING);
        v1 = _mm_add_epi32(w1, k__DCT_CONST_ROUNDING);
        v2 = _mm_add_epi32(w2, k__DCT_CONST_ROUNDING);
        v3 = _mm_add_epi32(w3, k__DCT_CONST_ROUNDING);
        v4 = _mm_add_epi32(w4, k__DCT_CONST_ROUNDING);
        v5 = _mm_add_epi32(w5, k__DCT_CONST_ROUNDING);
        v6 = _mm_add_epi32(w6, k__DCT_CONST_ROUNDING);
        v7 = _mm_add_epi32(w7, k__DCT_CONST_ROUNDING);
        v8 = _mm_add_epi32(w8, k__DCT_CONST_ROUNDING);
        v9 = _mm_add_epi32(w9, k__DCT_CONST_ROUNDING);
        v10 = _mm_add_epi32(w10, k__DCT_CONST_ROUNDING);
        v11 = _mm_add_epi32(w11, k__DCT_CONST_ROUNDING);
        v12 = _mm_add_epi32(w12, k__DCT_CONST_ROUNDING);
        v13 = _mm_add_epi32(w13, k__DCT_CONST_ROUNDING);
        v14 = _mm_add_epi32(w14, k__DCT_CONST_ROUNDING);
        v15 = _mm_add_epi32(w15, k__DCT_CONST_ROUNDING);

        u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
        u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
        u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
        u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
        u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
        u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
        u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
        u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);
        u8 = _mm_srai_epi32(v8, DCT_CONST_BITS);
        u9 = _mm_srai_epi32(v9, DCT_CONST_BITS);
        u10 = _mm_srai_epi32(v10, DCT_CONST_BITS);
        u11 = _mm_srai_epi32(v11, DCT_CONST_BITS);
        u12 = _mm_srai_epi32(v12, DCT_CONST_BITS);
        u13 = _mm_srai_epi32(v13, DCT_CONST_BITS);
        u14 = _mm_srai_epi32(v14, DCT_CONST_BITS);
        u15 = _mm_srai_epi32(v15, DCT_CONST_BITS);

        // back to 16-bit and pack 8 integers into __m128i
        in[0] = _mm_packs_epi32(u0, u1);
        in[1] = _mm_packs_epi32(u2, u3);
        in[2] = _mm_packs_epi32(u4, u5);
        in[3] = _mm_packs_epi32(u6, u7);
        in[4] = _mm_packs_epi32(u8, u9);
        in[5] = _mm_packs_epi32(u10, u11);
        in[6] = _mm_packs_epi32(u12, u13);
        in[7] = _mm_packs_epi32(u14, u15);

        // stage 2
        s0 = _mm_add_epi16(in[0], in[2]);
        s1 = _mm_add_epi16(in[1], in[3]);
        s2 = _mm_sub_epi16(in[0], in[2]);
        s3 = _mm_sub_epi16(in[1], in[3]);
        u0 = _mm_unpacklo_epi16(in[4], in[5]);
        u1 = _mm_unpackhi_epi16(in[4], in[5]);
        u2 = _mm_unpacklo_epi16(in[6], in[7]);
        u3 = _mm_unpackhi_epi16(in[6], in[7]);

        v0 = _mm_madd_epi16(u0, k__cospi_p08_p24);
        v1 = _mm_madd_epi16(u1, k__cospi_p08_p24);
        v2 = _mm_madd_epi16(u0, k__cospi_p24_m08);
        v3 = _mm_madd_epi16(u1, k__cospi_p24_m08);
        v4 = _mm_madd_epi16(u2, k__cospi_m24_p08);
        v5 = _mm_madd_epi16(u3, k__cospi_m24_p08);
        v6 = _mm_madd_epi16(u2, k__cospi_p08_p24);
        v7 = _mm_madd_epi16(u3, k__cospi_p08_p24);

        w0 = _mm_add_epi32(v0, v4);
        w1 = _mm_add_epi32(v1, v5);
        w2 = _mm_add_epi32(v2, v6);
        w3 = _mm_add_epi32(v3, v7);
        w4 = _mm_sub_epi32(v0, v4);
        w5 = _mm_sub_epi32(v1, v5);
        w6 = _mm_sub_epi32(v2, v6);
        w7 = _mm_sub_epi32(v3, v7);

        v0 = _mm_add_epi32(w0, k__DCT_CONST_ROUNDING);
        v1 = _mm_add_epi32(w1, k__DCT_CONST_ROUNDING);
        v2 = _mm_add_epi32(w2, k__DCT_CONST_ROUNDING);
        v3 = _mm_add_epi32(w3, k__DCT_CONST_ROUNDING);
        v4 = _mm_add_epi32(w4, k__DCT_CONST_ROUNDING);
        v5 = _mm_add_epi32(w5, k__DCT_CONST_ROUNDING);
        v6 = _mm_add_epi32(w6, k__DCT_CONST_ROUNDING);
        v7 = _mm_add_epi32(w7, k__DCT_CONST_ROUNDING);

        u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
        u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
        u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
        u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
        u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
        u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
        u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
        u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);

        // back to 16-bit intergers
        s4 = _mm_packs_epi32(u0, u1);
        s5 = _mm_packs_epi32(u2, u3);
        s6 = _mm_packs_epi32(u4, u5);
        s7 = _mm_packs_epi32(u6, u7);

        // stage 3
        u0 = _mm_unpacklo_epi16(s2, s3);
        u1 = _mm_unpackhi_epi16(s2, s3);
        u2 = _mm_unpacklo_epi16(s6, s7);
        u3 = _mm_unpackhi_epi16(s6, s7);

        v0 = _mm_madd_epi16(u0, k__cospi_p16_p16);
        v1 = _mm_madd_epi16(u1, k__cospi_p16_p16);
        v2 = _mm_madd_epi16(u0, k__cospi_p16_m16);
        v3 = _mm_madd_epi16(u1, k__cospi_p16_m16);
        v4 = _mm_madd_epi16(u2, k__cospi_p16_p16);
        v5 = _mm_madd_epi16(u3, k__cospi_p16_p16);
        v6 = _mm_madd_epi16(u2, k__cospi_p16_m16);
        v7 = _mm_madd_epi16(u3, k__cospi_p16_m16);

        u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
        u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
        u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
        u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
        u4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
        u5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
        u6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
        u7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

        v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
        v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
        v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
        v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);
        v4 = _mm_srai_epi32(u4, DCT_CONST_BITS);
        v5 = _mm_srai_epi32(u5, DCT_CONST_BITS);
        v6 = _mm_srai_epi32(u6, DCT_CONST_BITS);
        v7 = _mm_srai_epi32(u7, DCT_CONST_BITS);

        s2 = _mm_packs_epi32(v0, v1);
        s3 = _mm_packs_epi32(v2, v3);
        s6 = _mm_packs_epi32(v4, v5);
        s7 = _mm_packs_epi32(v6, v7);

        // FIXME(jingning): do subtract using bit inversion?
        in[0] = s0;
        in[1] = _mm_sub_epi16(k__const_0, s4);
        in[2] = s6;
        in[3] = _mm_sub_epi16(k__const_0, s2);
        in[4] = s3;
        in[5] = _mm_sub_epi16(k__const_0, s7);
        in[6] = s5;
        in[7] = _mm_sub_epi16(k__const_0, s1);

        // transpose
        array_transpose_8x8(in, in);
    }

    static void fdct8_sse2(__m128i *in)
    {
        // constants
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
        __m128i u0, u1, u2, u3, u4, u5, u6, u7;
        __m128i v0, v1, v2, v3, v4, v5, v6, v7;
        __m128i s0, s1, s2, s3, s4, s5, s6, s7;

        // stage 1
        s0 = _mm_add_epi16(in[0], in[7]);
        s1 = _mm_add_epi16(in[1], in[6]);
        s2 = _mm_add_epi16(in[2], in[5]);
        s3 = _mm_add_epi16(in[3], in[4]);
        s4 = _mm_sub_epi16(in[3], in[4]);
        s5 = _mm_sub_epi16(in[2], in[5]);
        s6 = _mm_sub_epi16(in[1], in[6]);
        s7 = _mm_sub_epi16(in[0], in[7]);

        u0 = _mm_add_epi16(s0, s3);
        u1 = _mm_add_epi16(s1, s2);
        u2 = _mm_sub_epi16(s1, s2);
        u3 = _mm_sub_epi16(s0, s3);
        // interleave and perform butterfly multiplication/addition
        v0 = _mm_unpacklo_epi16(u0, u1);
        v1 = _mm_unpackhi_epi16(u0, u1);
        v2 = _mm_unpacklo_epi16(u2, u3);
        v3 = _mm_unpackhi_epi16(u2, u3);

        u0 = _mm_madd_epi16(v0, k__cospi_p16_p16);
        u1 = _mm_madd_epi16(v1, k__cospi_p16_p16);
        u2 = _mm_madd_epi16(v0, k__cospi_p16_m16);
        u3 = _mm_madd_epi16(v1, k__cospi_p16_m16);
        u4 = _mm_madd_epi16(v2, k__cospi_p24_p08);
        u5 = _mm_madd_epi16(v3, k__cospi_p24_p08);
        u6 = _mm_madd_epi16(v2, k__cospi_m08_p24);
        u7 = _mm_madd_epi16(v3, k__cospi_m08_p24);

        // shift and rounding
        v0 = _mm_add_epi32(u0, k__DCT_CONST_ROUNDING);
        v1 = _mm_add_epi32(u1, k__DCT_CONST_ROUNDING);
        v2 = _mm_add_epi32(u2, k__DCT_CONST_ROUNDING);
        v3 = _mm_add_epi32(u3, k__DCT_CONST_ROUNDING);
        v4 = _mm_add_epi32(u4, k__DCT_CONST_ROUNDING);
        v5 = _mm_add_epi32(u5, k__DCT_CONST_ROUNDING);
        v6 = _mm_add_epi32(u6, k__DCT_CONST_ROUNDING);
        v7 = _mm_add_epi32(u7, k__DCT_CONST_ROUNDING);

        u0 = _mm_srai_epi32(v0, DCT_CONST_BITS);
        u1 = _mm_srai_epi32(v1, DCT_CONST_BITS);
        u2 = _mm_srai_epi32(v2, DCT_CONST_BITS);
        u3 = _mm_srai_epi32(v3, DCT_CONST_BITS);
        u4 = _mm_srai_epi32(v4, DCT_CONST_BITS);
        u5 = _mm_srai_epi32(v5, DCT_CONST_BITS);
        u6 = _mm_srai_epi32(v6, DCT_CONST_BITS);
        u7 = _mm_srai_epi32(v7, DCT_CONST_BITS);

        in[0] = _mm_packs_epi32(u0, u1);
        in[2] = _mm_packs_epi32(u4, u5);
        in[4] = _mm_packs_epi32(u2, u3);
        in[6] = _mm_packs_epi32(u6, u7);

        // stage 2
        // interleave and perform butterfly multiplication/addition
        u0 = _mm_unpacklo_epi16(s6, s5);
        u1 = _mm_unpackhi_epi16(s6, s5);
        v0 = _mm_madd_epi16(u0, k__cospi_p16_m16);
        v1 = _mm_madd_epi16(u1, k__cospi_p16_m16);
        v2 = _mm_madd_epi16(u0, k__cospi_p16_p16);
        v3 = _mm_madd_epi16(u1, k__cospi_p16_p16);

        // shift and rounding
        u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
        u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
        u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
        u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);

        v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
        v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
        v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
        v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);

        u0 = _mm_packs_epi32(v0, v1);
        u1 = _mm_packs_epi32(v2, v3);

        // stage 3
        s0 = _mm_add_epi16(s4, u0);
        s1 = _mm_sub_epi16(s4, u0);
        s2 = _mm_sub_epi16(s7, u1);
        s3 = _mm_add_epi16(s7, u1);

        // stage 4
        u0 = _mm_unpacklo_epi16(s0, s3);
        u1 = _mm_unpackhi_epi16(s0, s3);
        u2 = _mm_unpacklo_epi16(s1, s2);
        u3 = _mm_unpackhi_epi16(s1, s2);

        v0 = _mm_madd_epi16(u0, k__cospi_p28_p04);
        v1 = _mm_madd_epi16(u1, k__cospi_p28_p04);
        v2 = _mm_madd_epi16(u2, k__cospi_p12_p20);
        v3 = _mm_madd_epi16(u3, k__cospi_p12_p20);
        v4 = _mm_madd_epi16(u2, k__cospi_m20_p12);
        v5 = _mm_madd_epi16(u3, k__cospi_m20_p12);
        v6 = _mm_madd_epi16(u0, k__cospi_m04_p28);
        v7 = _mm_madd_epi16(u1, k__cospi_m04_p28);

        // shift and rounding
        u0 = _mm_add_epi32(v0, k__DCT_CONST_ROUNDING);
        u1 = _mm_add_epi32(v1, k__DCT_CONST_ROUNDING);
        u2 = _mm_add_epi32(v2, k__DCT_CONST_ROUNDING);
        u3 = _mm_add_epi32(v3, k__DCT_CONST_ROUNDING);
        u4 = _mm_add_epi32(v4, k__DCT_CONST_ROUNDING);
        u5 = _mm_add_epi32(v5, k__DCT_CONST_ROUNDING);
        u6 = _mm_add_epi32(v6, k__DCT_CONST_ROUNDING);
        u7 = _mm_add_epi32(v7, k__DCT_CONST_ROUNDING);

        v0 = _mm_srai_epi32(u0, DCT_CONST_BITS);
        v1 = _mm_srai_epi32(u1, DCT_CONST_BITS);
        v2 = _mm_srai_epi32(u2, DCT_CONST_BITS);
        v3 = _mm_srai_epi32(u3, DCT_CONST_BITS);
        v4 = _mm_srai_epi32(u4, DCT_CONST_BITS);
        v5 = _mm_srai_epi32(u5, DCT_CONST_BITS);
        v6 = _mm_srai_epi32(u6, DCT_CONST_BITS);
        v7 = _mm_srai_epi32(u7, DCT_CONST_BITS);

        in[1] = _mm_packs_epi32(v0, v1);
        in[3] = _mm_packs_epi32(v4, v5);
        in[5] = _mm_packs_epi32(v2, v3);
        in[7] = _mm_packs_epi32(v6, v7);

        // transpose
        array_transpose_8x8(in, in);
    }

    // right shift and rounding
    static inline void right_shift_8x8(__m128i *res, const int bit)
    {
        __m128i sign0 = _mm_srai_epi16(res[0], 15);
        __m128i sign1 = _mm_srai_epi16(res[1], 15);
        __m128i sign2 = _mm_srai_epi16(res[2], 15);
        __m128i sign3 = _mm_srai_epi16(res[3], 15);
        __m128i sign4 = _mm_srai_epi16(res[4], 15);
        __m128i sign5 = _mm_srai_epi16(res[5], 15);
        __m128i sign6 = _mm_srai_epi16(res[6], 15);
        __m128i sign7 = _mm_srai_epi16(res[7], 15);

        if (bit == 2) {
            const __m128i const_rounding = _mm_set1_epi16(1);
            res[0] = _mm_add_epi16(res[0], const_rounding);
            res[1] = _mm_add_epi16(res[1], const_rounding);
            res[2] = _mm_add_epi16(res[2], const_rounding);
            res[3] = _mm_add_epi16(res[3], const_rounding);
            res[4] = _mm_add_epi16(res[4], const_rounding);
            res[5] = _mm_add_epi16(res[5], const_rounding);
            res[6] = _mm_add_epi16(res[6], const_rounding);
            res[7] = _mm_add_epi16(res[7], const_rounding);
        }

        res[0] = _mm_sub_epi16(res[0], sign0);
        res[1] = _mm_sub_epi16(res[1], sign1);
        res[2] = _mm_sub_epi16(res[2], sign2);
        res[3] = _mm_sub_epi16(res[3], sign3);
        res[4] = _mm_sub_epi16(res[4], sign4);
        res[5] = _mm_sub_epi16(res[5], sign5);
        res[6] = _mm_sub_epi16(res[6], sign6);
        res[7] = _mm_sub_epi16(res[7], sign7);

        if (bit == 1) {
            res[0] = _mm_srai_epi16(res[0], 1);
            res[1] = _mm_srai_epi16(res[1], 1);
            res[2] = _mm_srai_epi16(res[2], 1);
            res[3] = _mm_srai_epi16(res[3], 1);
            res[4] = _mm_srai_epi16(res[4], 1);
            res[5] = _mm_srai_epi16(res[5], 1);
            res[6] = _mm_srai_epi16(res[6], 1);
            res[7] = _mm_srai_epi16(res[7], 1);
        } else {
            res[0] = _mm_srai_epi16(res[0], 2);
            res[1] = _mm_srai_epi16(res[1], 2);
            res[2] = _mm_srai_epi16(res[2], 2);
            res[3] = _mm_srai_epi16(res[3], 2);
            res[4] = _mm_srai_epi16(res[4], 2);
            res[5] = _mm_srai_epi16(res[5], 2);
            res[6] = _mm_srai_epi16(res[6], 2);
            res[7] = _mm_srai_epi16(res[7], 2);
        }
    }

    // write 8x8 array
    static inline void write_buffer_8x8(tran_low_t *output, __m128i *res, int stride)
    {
        store_output(&res[0], (output + 0 * stride));
        store_output(&res[1], (output + 1 * stride));
        store_output(&res[2], (output + 2 * stride));
        store_output(&res[3], (output + 3 * stride));
        store_output(&res[4], (output + 4 * stride));
        store_output(&res[5], (output + 5 * stride));
        store_output(&res[6], (output + 6 * stride));
        store_output(&res[7], (output + 7 * stride));
    }

    template <int tx_type> void fht8x8_sse2(const int16_t *input, int16_t *output, int stride)
    {
        __m128i in[8];

        if (tx_type == DCT_DCT) {
            fdct8x8_sse2(input, output, stride);
        } else if (tx_type == ADST_DCT) {
            load_buffer_8x8(input, in, stride);
            fadst8_sse2(in);
            fdct8_sse2(in);
            right_shift_8x8(in, 1);
            write_buffer_8x8(output, in, 8);
        } else if (tx_type == DCT_ADST) {
            load_buffer_8x8(input, in, stride);
            fdct8_sse2(in);
            fadst8_sse2(in);
            right_shift_8x8(in, 1);
            write_buffer_8x8(output, in, 8);
        } else if (tx_type == ADST_ADST) {
            load_buffer_8x8(input, in, stride);
            fadst8_sse2(in);
            fadst8_sse2(in);
            right_shift_8x8(in, 1);
            write_buffer_8x8(output, in, 8);
        } else {
            assert(0);
        }
    }

    //-----------------------------------------------------
    //                16x16
    //-----------------------------------------------------
#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n, typ, val) typ val __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n, typ, val) typ val
#endif
    void fdct16x16_sse2(const int16_t *input, tran_low_t *output, int stride)
    {
        // The 2D transform is done with two passes which are actually pretty
        // similar. In the first one, we transform the columns and transpose
        // the results. In the second one, we transform the rows. To achieve that,
        // as the first pass results are transposed, we transpose the columns (that
        // is the transposed rows) and transpose the results (so that it goes back
        // in normal/row positions).
        int pass;
        // We need an intermediate buffer between passes.
        DECLARE_ALIGNED(16, int16_t, intermediate[256]);
        const int16_t *in = input;
        int16_t *out0 = intermediate;
        tran_low_t *out1 = output;
        // Constants
        //    When we use them, in one case, they are all the same. In all others
        //    it's a pair of them that we need to repeat four times. This is done
        //    by constructing the 32 bit constant corresponding to that pair.
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m128i k__cospi_p08_m24 = pair_set_epi16(cospi_8_64, -cospi_24_64);
        const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m128i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
        const __m128i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
        const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m128i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
        const __m128i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
        const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
        const __m128i kOne = _mm_set1_epi16(1);
        // Do the two transform/transpose passes
        for (pass = 0; pass < 2; ++pass) {
            // We process eight columns (transposed rows in second pass) at a time.
            int column_start;
#if DCT_HIGH_BIT_DEPTH
            int overflow;
#endif
            for (column_start = 0; column_start < 16; column_start += 8) {
                __m128i in00, in01, in02, in03, in04, in05, in06, in07;
                __m128i in08, in09, in10, in11, in12, in13, in14, in15;
                __m128i input0, input1, input2, input3, input4, input5, input6, input7;
                __m128i step1_0, step1_1, step1_2, step1_3;
                __m128i step1_4, step1_5, step1_6, step1_7;
                __m128i step2_1, step2_2, step2_3, step2_4, step2_5, step2_6;
                __m128i step3_0, step3_1, step3_2, step3_3;
                __m128i step3_4, step3_5, step3_6, step3_7;
                __m128i res00, res01, res02, res03, res04, res05, res06, res07;
                __m128i res08, res09, res10, res11, res12, res13, res14, res15;
                // Load and pre-condition input.
                if (0 == pass) {
                    in00 = _mm_load_si128((const __m128i *)(in + 0 * stride));
                    in01 = _mm_load_si128((const __m128i *)(in + 1 * stride));
                    in02 = _mm_load_si128((const __m128i *)(in + 2 * stride));
                    in03 = _mm_load_si128((const __m128i *)(in + 3 * stride));
                    in04 = _mm_load_si128((const __m128i *)(in + 4 * stride));
                    in05 = _mm_load_si128((const __m128i *)(in + 5 * stride));
                    in06 = _mm_load_si128((const __m128i *)(in + 6 * stride));
                    in07 = _mm_load_si128((const __m128i *)(in + 7 * stride));
                    in08 = _mm_load_si128((const __m128i *)(in + 8 * stride));
                    in09 = _mm_load_si128((const __m128i *)(in + 9 * stride));
                    in10 = _mm_load_si128((const __m128i *)(in + 10 * stride));
                    in11 = _mm_load_si128((const __m128i *)(in + 11 * stride));
                    in12 = _mm_load_si128((const __m128i *)(in + 12 * stride));
                    in13 = _mm_load_si128((const __m128i *)(in + 13 * stride));
                    in14 = _mm_load_si128((const __m128i *)(in + 14 * stride));
                    in15 = _mm_load_si128((const __m128i *)(in + 15 * stride));
                    // x = x << 2
                    in00 = _mm_slli_epi16(in00, 2);
                    in01 = _mm_slli_epi16(in01, 2);
                    in02 = _mm_slli_epi16(in02, 2);
                    in03 = _mm_slli_epi16(in03, 2);
                    in04 = _mm_slli_epi16(in04, 2);
                    in05 = _mm_slli_epi16(in05, 2);
                    in06 = _mm_slli_epi16(in06, 2);
                    in07 = _mm_slli_epi16(in07, 2);
                    in08 = _mm_slli_epi16(in08, 2);
                    in09 = _mm_slli_epi16(in09, 2);
                    in10 = _mm_slli_epi16(in10, 2);
                    in11 = _mm_slli_epi16(in11, 2);
                    in12 = _mm_slli_epi16(in12, 2);
                    in13 = _mm_slli_epi16(in13, 2);
                    in14 = _mm_slli_epi16(in14, 2);
                    in15 = _mm_slli_epi16(in15, 2);
                } else {
                    in00 = _mm_load_si128((const __m128i *)(in + 0 * 16));
                    in01 = _mm_load_si128((const __m128i *)(in + 1 * 16));
                    in02 = _mm_load_si128((const __m128i *)(in + 2 * 16));
                    in03 = _mm_load_si128((const __m128i *)(in + 3 * 16));
                    in04 = _mm_load_si128((const __m128i *)(in + 4 * 16));
                    in05 = _mm_load_si128((const __m128i *)(in + 5 * 16));
                    in06 = _mm_load_si128((const __m128i *)(in + 6 * 16));
                    in07 = _mm_load_si128((const __m128i *)(in + 7 * 16));
                    in08 = _mm_load_si128((const __m128i *)(in + 8 * 16));
                    in09 = _mm_load_si128((const __m128i *)(in + 9 * 16));
                    in10 = _mm_load_si128((const __m128i *)(in + 10 * 16));
                    in11 = _mm_load_si128((const __m128i *)(in + 11 * 16));
                    in12 = _mm_load_si128((const __m128i *)(in + 12 * 16));
                    in13 = _mm_load_si128((const __m128i *)(in + 13 * 16));
                    in14 = _mm_load_si128((const __m128i *)(in + 14 * 16));
                    in15 = _mm_load_si128((const __m128i *)(in + 15 * 16));
                    // x = (x + 1) >> 2
                    in00 = _mm_add_epi16(in00, kOne);
                    in01 = _mm_add_epi16(in01, kOne);
                    in02 = _mm_add_epi16(in02, kOne);
                    in03 = _mm_add_epi16(in03, kOne);
                    in04 = _mm_add_epi16(in04, kOne);
                    in05 = _mm_add_epi16(in05, kOne);
                    in06 = _mm_add_epi16(in06, kOne);
                    in07 = _mm_add_epi16(in07, kOne);
                    in08 = _mm_add_epi16(in08, kOne);
                    in09 = _mm_add_epi16(in09, kOne);
                    in10 = _mm_add_epi16(in10, kOne);
                    in11 = _mm_add_epi16(in11, kOne);
                    in12 = _mm_add_epi16(in12, kOne);
                    in13 = _mm_add_epi16(in13, kOne);
                    in14 = _mm_add_epi16(in14, kOne);
                    in15 = _mm_add_epi16(in15, kOne);
                    in00 = _mm_srai_epi16(in00, 2);
                    in01 = _mm_srai_epi16(in01, 2);
                    in02 = _mm_srai_epi16(in02, 2);
                    in03 = _mm_srai_epi16(in03, 2);
                    in04 = _mm_srai_epi16(in04, 2);
                    in05 = _mm_srai_epi16(in05, 2);
                    in06 = _mm_srai_epi16(in06, 2);
                    in07 = _mm_srai_epi16(in07, 2);
                    in08 = _mm_srai_epi16(in08, 2);
                    in09 = _mm_srai_epi16(in09, 2);
                    in10 = _mm_srai_epi16(in10, 2);
                    in11 = _mm_srai_epi16(in11, 2);
                    in12 = _mm_srai_epi16(in12, 2);
                    in13 = _mm_srai_epi16(in13, 2);
                    in14 = _mm_srai_epi16(in14, 2);
                    in15 = _mm_srai_epi16(in15, 2);
                }
                in += 8;
                // Calculate input for the first 8 results.
                {
                    input0 = ADD_EPI16(in00, in15);
                    input1 = ADD_EPI16(in01, in14);
                    input2 = ADD_EPI16(in02, in13);
                    input3 = ADD_EPI16(in03, in12);
                    input4 = ADD_EPI16(in04, in11);
                    input5 = ADD_EPI16(in05, in10);
                    input6 = ADD_EPI16(in06, in09);
                    input7 = ADD_EPI16(in07, in08);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x8(&input0, &input1, &input2, &input3,
                        &input4, &input5, &input6, &input7);
                    if (overflow) {
                        vpx_highbd_fdct16x16_c(input, output, stride);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                }
                // Calculate input for the next 8 results.
                {
                    step1_0 = SUB_EPI16(in07, in08);
                    step1_1 = SUB_EPI16(in06, in09);
                    step1_2 = SUB_EPI16(in05, in10);
                    step1_3 = SUB_EPI16(in04, in11);
                    step1_4 = SUB_EPI16(in03, in12);
                    step1_5 = SUB_EPI16(in02, in13);
                    step1_6 = SUB_EPI16(in01, in14);
                    step1_7 = SUB_EPI16(in00, in15);
#if DCT_HIGH_BIT_DEPTH
                    overflow =
                        check_epi16_overflow_x8(&step1_0, &step1_1, &step1_2, &step1_3,
                        &step1_4, &step1_5, &step1_6, &step1_7);
                    if (overflow) {
                        vpx_highbd_fdct16x16_c(input, output, stride);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                }
                // Work on the first eight values; fdct8(input, even_results);
                {
                    // Add/subtract
                    const __m128i q0 = ADD_EPI16(input0, input7);
                    const __m128i q1 = ADD_EPI16(input1, input6);
                    const __m128i q2 = ADD_EPI16(input2, input5);
                    const __m128i q3 = ADD_EPI16(input3, input4);
                    const __m128i q4 = SUB_EPI16(input3, input4);
                    const __m128i q5 = SUB_EPI16(input2, input5);
                    const __m128i q6 = SUB_EPI16(input1, input6);
                    const __m128i q7 = SUB_EPI16(input0, input7);
#if DCT_HIGH_BIT_DEPTH
                    overflow =
                        check_epi16_overflow_x8(&q0, &q1, &q2, &q3, &q4, &q5, &q6, &q7);
                    if (overflow) {
                        vpx_highbd_fdct16x16_c(input, output, stride);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                    // Work on first four results
                    {
                        // Add/subtract
                        const __m128i r0 = ADD_EPI16(q0, q3);
                        const __m128i r1 = ADD_EPI16(q1, q2);
                        const __m128i r2 = SUB_EPI16(q1, q2);
                        const __m128i r3 = SUB_EPI16(q0, q3);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&r0, &r1, &r2, &r3);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        // Interleave to do the multiply by constants which gets us
                        // into 32 bits.
                        {
                            const __m128i t0 = _mm_unpacklo_epi16(r0, r1);
                            const __m128i t1 = _mm_unpackhi_epi16(r0, r1);
                            const __m128i t2 = _mm_unpacklo_epi16(r2, r3);
                            const __m128i t3 = _mm_unpackhi_epi16(r2, r3);
                            res00 = mult_round_shift(&t0, &t1, &k__cospi_p16_p16,
                                &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res08 = mult_round_shift(&t0, &t1, &k__cospi_p16_m16,
                                &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res04 = mult_round_shift(&t2, &t3, &k__cospi_p24_p08,
                                &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                            res12 = mult_round_shift(&t2, &t3, &k__cospi_m08_p24,
                                &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
#if DCT_HIGH_BIT_DEPTH
                            overflow = check_epi16_overflow_x4(&res00, &res08, &res04, &res12);
                            if (overflow) {
                                vpx_highbd_fdct16x16_c(input, output, stride);
                                return;
                            }
#endif  // DCT_HIGH_BIT_DEPTH
                        }
                    }
                    // Work on next four results
                    {
                        // Interleave to do the multiply by constants which gets us
                        // into 32 bits.
                        const __m128i d0 = _mm_unpacklo_epi16(q6, q5);
                        const __m128i d1 = _mm_unpackhi_epi16(q6, q5);
                        const __m128i r0 =
                            mult_round_shift(&d0, &d1, &k__cospi_p16_m16,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        const __m128i r1 =
                            mult_round_shift(&d0, &d1, &k__cospi_p16_p16,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x2(&r0, &r1);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        {
                            // Add/subtract
                            const __m128i x0 = ADD_EPI16(q4, r0);
                            const __m128i x1 = SUB_EPI16(q4, r0);
                            const __m128i x2 = SUB_EPI16(q7, r1);
                            const __m128i x3 = ADD_EPI16(q7, r1);
#if DCT_HIGH_BIT_DEPTH
                            overflow = check_epi16_overflow_x4(&x0, &x1, &x2, &x3);
                            if (overflow) {
                                vpx_highbd_fdct16x16_c(input, output, stride);
                                return;
                            }
#endif  // DCT_HIGH_BIT_DEPTH
                            // Interleave to do the multiply by constants which gets us
                            // into 32 bits.
                            {
                                const __m128i t0 = _mm_unpacklo_epi16(x0, x3);
                                const __m128i t1 = _mm_unpackhi_epi16(x0, x3);
                                const __m128i t2 = _mm_unpacklo_epi16(x1, x2);
                                const __m128i t3 = _mm_unpackhi_epi16(x1, x2);
                                res02 = mult_round_shift(&t0, &t1, &k__cospi_p28_p04,
                                    &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                                res14 = mult_round_shift(&t0, &t1, &k__cospi_m04_p28,
                                    &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                                res10 = mult_round_shift(&t2, &t3, &k__cospi_p12_p20,
                                    &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                                res06 = mult_round_shift(&t2, &t3, &k__cospi_m20_p12,
                                    &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
#if DCT_HIGH_BIT_DEPTH
                                overflow =
                                    check_epi16_overflow_x4(&res02, &res14, &res10, &res06);
                                if (overflow) {
                                    vpx_highbd_fdct16x16_c(input, output, stride);
                                    return;
                                }
#endif  // DCT_HIGH_BIT_DEPTH
                            }
                        }
                    }
                }
                // Work on the next eight values; step1 -> odd_results
                {
                    // step 2
                    {
                        const __m128i t0 = _mm_unpacklo_epi16(step1_5, step1_2);
                        const __m128i t1 = _mm_unpackhi_epi16(step1_5, step1_2);
                        const __m128i t2 = _mm_unpacklo_epi16(step1_4, step1_3);
                        const __m128i t3 = _mm_unpackhi_epi16(step1_4, step1_3);
                        step2_2 = mult_round_shift(&t0, &t1, &k__cospi_p16_m16,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        step2_3 = mult_round_shift(&t2, &t3, &k__cospi_p16_m16,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        step2_5 = mult_round_shift(&t0, &t1, &k__cospi_p16_p16,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        step2_4 = mult_round_shift(&t2, &t3, &k__cospi_p16_p16,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&step2_2, &step2_3, &step2_5, &step2_4);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // step 3
                    {
                        step3_0 = ADD_EPI16(step1_0, step2_3);
                        step3_1 = ADD_EPI16(step1_1, step2_2);
                        step3_2 = SUB_EPI16(step1_1, step2_2);
                        step3_3 = SUB_EPI16(step1_0, step2_3);
                        step3_4 = SUB_EPI16(step1_7, step2_4);
                        step3_5 = SUB_EPI16(step1_6, step2_5);
                        step3_6 = ADD_EPI16(step1_6, step2_5);
                        step3_7 = ADD_EPI16(step1_7, step2_4);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&step3_0, &step3_1, &step3_2, &step3_3,
                            &step3_4, &step3_5, &step3_6, &step3_7);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // step 4
                    {
                        const __m128i t0 = _mm_unpacklo_epi16(step3_1, step3_6);
                        const __m128i t1 = _mm_unpackhi_epi16(step3_1, step3_6);
                        const __m128i t2 = _mm_unpacklo_epi16(step3_2, step3_5);
                        const __m128i t3 = _mm_unpackhi_epi16(step3_2, step3_5);
                        step2_1 = mult_round_shift(&t0, &t1, &k__cospi_m08_p24,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        step2_2 = mult_round_shift(&t2, &t3, &k__cospi_p24_p08,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        step2_6 = mult_round_shift(&t0, &t1, &k__cospi_p24_p08,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        step2_5 = mult_round_shift(&t2, &t3, &k__cospi_p08_m24,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&step2_1, &step2_2, &step2_6, &step2_5);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // step 5
                    {
                        step1_0 = ADD_EPI16(step3_0, step2_1);
                        step1_1 = SUB_EPI16(step3_0, step2_1);
                        step1_2 = ADD_EPI16(step3_3, step2_2);
                        step1_3 = SUB_EPI16(step3_3, step2_2);
                        step1_4 = SUB_EPI16(step3_4, step2_5);
                        step1_5 = ADD_EPI16(step3_4, step2_5);
                        step1_6 = SUB_EPI16(step3_7, step2_6);
                        step1_7 = ADD_EPI16(step3_7, step2_6);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&step1_0, &step1_1, &step1_2, &step1_3,
                            &step1_4, &step1_5, &step1_6, &step1_7);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // step 6
                    {
                        const __m128i t0 = _mm_unpacklo_epi16(step1_0, step1_7);
                        const __m128i t1 = _mm_unpackhi_epi16(step1_0, step1_7);
                        const __m128i t2 = _mm_unpacklo_epi16(step1_1, step1_6);
                        const __m128i t3 = _mm_unpackhi_epi16(step1_1, step1_6);
                        res01 = mult_round_shift(&t0, &t1, &k__cospi_p30_p02,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res09 = mult_round_shift(&t2, &t3, &k__cospi_p14_p18,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res15 = mult_round_shift(&t0, &t1, &k__cospi_m02_p30,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res07 = mult_round_shift(&t2, &t3, &k__cospi_m18_p14,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&res01, &res09, &res15, &res07);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i t0 = _mm_unpacklo_epi16(step1_2, step1_5);
                        const __m128i t1 = _mm_unpackhi_epi16(step1_2, step1_5);
                        const __m128i t2 = _mm_unpacklo_epi16(step1_3, step1_4);
                        const __m128i t3 = _mm_unpackhi_epi16(step1_3, step1_4);
                        res05 = mult_round_shift(&t0, &t1, &k__cospi_p22_p10,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res13 = mult_round_shift(&t2, &t3, &k__cospi_p06_p26,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res11 = mult_round_shift(&t0, &t1, &k__cospi_m10_p22,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
                        res03 = mult_round_shift(&t2, &t3, &k__cospi_m26_p06,
                            &k__DCT_CONST_ROUNDING, DCT_CONST_BITS);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&res05, &res13, &res11, &res03);
                        if (overflow) {
                            vpx_highbd_fdct16x16_c(input, output, stride);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                }
                // Transpose the results, do it as two 8x8 transposes.
                transpose_and_output8x8(&res00, &res01, &res02, &res03, &res04, &res05,
                    &res06, &res07, pass, out0, out1);
                transpose_and_output8x8(&res08, &res09, &res10, &res11, &res12, &res13,
                    &res14, &res15, pass, out0 + 8, out1 + 8);
                if (pass == 0) {
                    out0 += 8 * 16;
                } else {
                    out1 += 8 * 16;
                }
            }
            // Setup in/out for next pass.
            in = intermediate;
        }
    }

    static inline void load_buffer_16x16(const int16_t *input, __m128i *in0, __m128i *in1, int stride)
    {
            // load first 8 columns
            load_buffer_8x8(input, in0, stride);
            load_buffer_8x8(input + 8 * stride, in0 + 8, stride);

            input += 8;
            // load second 8 columns
            load_buffer_8x8(input, in1, stride);
            load_buffer_8x8(input + 8 * stride, in1 + 8, stride);
    }

    static inline void write_buffer_16x16(tran_low_t *output, __m128i *in0, __m128i *in1, int stride)
    {
            // write first 8 columns
            write_buffer_8x8(output, in0, stride);
            write_buffer_8x8(output + 8 * stride, in0 + 8, stride);
            // write second 8 columns
            output += 8;
            write_buffer_8x8(output, in1, stride);
            write_buffer_8x8(output + 8 * stride, in1 + 8, stride);
    }

    static inline void right_shift_16x16(__m128i *res0, __m128i *res1)
    {
        // perform rounding operations
        right_shift_8x8(res0, 2);
        right_shift_8x8(res0 + 8, 2);
        right_shift_8x8(res1, 2);
        right_shift_8x8(res1 + 8, 2);
    }

    static inline void array_transpose_16x16(__m128i *res0, __m128i *res1)
    {
        __m128i tbuf[8];
        array_transpose_8x8(res0, res0);
        array_transpose_8x8(res1, tbuf);
        array_transpose_8x8(res0 + 8, res1);
        array_transpose_8x8(res1 + 8, res1 + 8);

        res0[8] = tbuf[0];
        res0[9] = tbuf[1];
        res0[10] = tbuf[2];
        res0[11] = tbuf[3];
        res0[12] = tbuf[4];
        res0[13] = tbuf[5];
        res0[14] = tbuf[6];
        res0[15] = tbuf[7];
    }

    static void fadst16_8col(__m128i *in)
    {
        // perform 16x16 1-D ADST for 8 columns
        __m128i s[16], x[16], u[32], v[32];
        const __m128i k__cospi_p01_p31 = pair_set_epi16(cospi_1_64, cospi_31_64);
        const __m128i k__cospi_p31_m01 = pair_set_epi16(cospi_31_64, -cospi_1_64);
        const __m128i k__cospi_p05_p27 = pair_set_epi16(cospi_5_64, cospi_27_64);
        const __m128i k__cospi_p27_m05 = pair_set_epi16(cospi_27_64, -cospi_5_64);
        const __m128i k__cospi_p09_p23 = pair_set_epi16(cospi_9_64, cospi_23_64);
        const __m128i k__cospi_p23_m09 = pair_set_epi16(cospi_23_64, -cospi_9_64);
        const __m128i k__cospi_p13_p19 = pair_set_epi16(cospi_13_64, cospi_19_64);
        const __m128i k__cospi_p19_m13 = pair_set_epi16(cospi_19_64, -cospi_13_64);
        const __m128i k__cospi_p17_p15 = pair_set_epi16(cospi_17_64, cospi_15_64);
        const __m128i k__cospi_p15_m17 = pair_set_epi16(cospi_15_64, -cospi_17_64);
        const __m128i k__cospi_p21_p11 = pair_set_epi16(cospi_21_64, cospi_11_64);
        const __m128i k__cospi_p11_m21 = pair_set_epi16(cospi_11_64, -cospi_21_64);
        const __m128i k__cospi_p25_p07 = pair_set_epi16(cospi_25_64, cospi_7_64);
        const __m128i k__cospi_p07_m25 = pair_set_epi16(cospi_7_64, -cospi_25_64);
        const __m128i k__cospi_p29_p03 = pair_set_epi16(cospi_29_64, cospi_3_64);
        const __m128i k__cospi_p03_m29 = pair_set_epi16(cospi_3_64, -cospi_29_64);
        const __m128i k__cospi_p04_p28 = pair_set_epi16(cospi_4_64, cospi_28_64);
        const __m128i k__cospi_p28_m04 = pair_set_epi16(cospi_28_64, -cospi_4_64);
        const __m128i k__cospi_p20_p12 = pair_set_epi16(cospi_20_64, cospi_12_64);
        const __m128i k__cospi_p12_m20 = pair_set_epi16(cospi_12_64, -cospi_20_64);
        const __m128i k__cospi_m28_p04 = pair_set_epi16(-cospi_28_64, cospi_4_64);
        const __m128i k__cospi_m12_p20 = pair_set_epi16(-cospi_12_64, cospi_20_64);
        const __m128i k__cospi_p08_p24 = pair_set_epi16(cospi_8_64, cospi_24_64);
        const __m128i k__cospi_p24_m08 = pair_set_epi16(cospi_24_64, -cospi_8_64);
        const __m128i k__cospi_m24_p08 = pair_set_epi16(-cospi_24_64, cospi_8_64);
        const __m128i k__cospi_m16_m16 = _mm_set1_epi16((int16_t)-cospi_16_64);
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
        const __m128i kZero = _mm_set1_epi16(0);

        u[0] = _mm_unpacklo_epi16(in[15], in[0]);
        u[1] = _mm_unpackhi_epi16(in[15], in[0]);
        u[2] = _mm_unpacklo_epi16(in[13], in[2]);
        u[3] = _mm_unpackhi_epi16(in[13], in[2]);
        u[4] = _mm_unpacklo_epi16(in[11], in[4]);
        u[5] = _mm_unpackhi_epi16(in[11], in[4]);
        u[6] = _mm_unpacklo_epi16(in[9], in[6]);
        u[7] = _mm_unpackhi_epi16(in[9], in[6]);
        u[8] = _mm_unpacklo_epi16(in[7], in[8]);
        u[9] = _mm_unpackhi_epi16(in[7], in[8]);
        u[10] = _mm_unpacklo_epi16(in[5], in[10]);
        u[11] = _mm_unpackhi_epi16(in[5], in[10]);
        u[12] = _mm_unpacklo_epi16(in[3], in[12]);
        u[13] = _mm_unpackhi_epi16(in[3], in[12]);
        u[14] = _mm_unpacklo_epi16(in[1], in[14]);
        u[15] = _mm_unpackhi_epi16(in[1], in[14]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_p01_p31);
        v[1] = _mm_madd_epi16(u[1], k__cospi_p01_p31);
        v[2] = _mm_madd_epi16(u[0], k__cospi_p31_m01);
        v[3] = _mm_madd_epi16(u[1], k__cospi_p31_m01);
        v[4] = _mm_madd_epi16(u[2], k__cospi_p05_p27);
        v[5] = _mm_madd_epi16(u[3], k__cospi_p05_p27);
        v[6] = _mm_madd_epi16(u[2], k__cospi_p27_m05);
        v[7] = _mm_madd_epi16(u[3], k__cospi_p27_m05);
        v[8] = _mm_madd_epi16(u[4], k__cospi_p09_p23);
        v[9] = _mm_madd_epi16(u[5], k__cospi_p09_p23);
        v[10] = _mm_madd_epi16(u[4], k__cospi_p23_m09);
        v[11] = _mm_madd_epi16(u[5], k__cospi_p23_m09);
        v[12] = _mm_madd_epi16(u[6], k__cospi_p13_p19);
        v[13] = _mm_madd_epi16(u[7], k__cospi_p13_p19);
        v[14] = _mm_madd_epi16(u[6], k__cospi_p19_m13);
        v[15] = _mm_madd_epi16(u[7], k__cospi_p19_m13);
        v[16] = _mm_madd_epi16(u[8], k__cospi_p17_p15);
        v[17] = _mm_madd_epi16(u[9], k__cospi_p17_p15);
        v[18] = _mm_madd_epi16(u[8], k__cospi_p15_m17);
        v[19] = _mm_madd_epi16(u[9], k__cospi_p15_m17);
        v[20] = _mm_madd_epi16(u[10], k__cospi_p21_p11);
        v[21] = _mm_madd_epi16(u[11], k__cospi_p21_p11);
        v[22] = _mm_madd_epi16(u[10], k__cospi_p11_m21);
        v[23] = _mm_madd_epi16(u[11], k__cospi_p11_m21);
        v[24] = _mm_madd_epi16(u[12], k__cospi_p25_p07);
        v[25] = _mm_madd_epi16(u[13], k__cospi_p25_p07);
        v[26] = _mm_madd_epi16(u[12], k__cospi_p07_m25);
        v[27] = _mm_madd_epi16(u[13], k__cospi_p07_m25);
        v[28] = _mm_madd_epi16(u[14], k__cospi_p29_p03);
        v[29] = _mm_madd_epi16(u[15], k__cospi_p29_p03);
        v[30] = _mm_madd_epi16(u[14], k__cospi_p03_m29);
        v[31] = _mm_madd_epi16(u[15], k__cospi_p03_m29);

        u[0] = _mm_add_epi32(v[0], v[16]);
        u[1] = _mm_add_epi32(v[1], v[17]);
        u[2] = _mm_add_epi32(v[2], v[18]);
        u[3] = _mm_add_epi32(v[3], v[19]);
        u[4] = _mm_add_epi32(v[4], v[20]);
        u[5] = _mm_add_epi32(v[5], v[21]);
        u[6] = _mm_add_epi32(v[6], v[22]);
        u[7] = _mm_add_epi32(v[7], v[23]);
        u[8] = _mm_add_epi32(v[8], v[24]);
        u[9] = _mm_add_epi32(v[9], v[25]);
        u[10] = _mm_add_epi32(v[10], v[26]);
        u[11] = _mm_add_epi32(v[11], v[27]);
        u[12] = _mm_add_epi32(v[12], v[28]);
        u[13] = _mm_add_epi32(v[13], v[29]);
        u[14] = _mm_add_epi32(v[14], v[30]);
        u[15] = _mm_add_epi32(v[15], v[31]);
        u[16] = _mm_sub_epi32(v[0], v[16]);
        u[17] = _mm_sub_epi32(v[1], v[17]);
        u[18] = _mm_sub_epi32(v[2], v[18]);
        u[19] = _mm_sub_epi32(v[3], v[19]);
        u[20] = _mm_sub_epi32(v[4], v[20]);
        u[21] = _mm_sub_epi32(v[5], v[21]);
        u[22] = _mm_sub_epi32(v[6], v[22]);
        u[23] = _mm_sub_epi32(v[7], v[23]);
        u[24] = _mm_sub_epi32(v[8], v[24]);
        u[25] = _mm_sub_epi32(v[9], v[25]);
        u[26] = _mm_sub_epi32(v[10], v[26]);
        u[27] = _mm_sub_epi32(v[11], v[27]);
        u[28] = _mm_sub_epi32(v[12], v[28]);
        u[29] = _mm_sub_epi32(v[13], v[29]);
        u[30] = _mm_sub_epi32(v[14], v[30]);
        u[31] = _mm_sub_epi32(v[15], v[31]);

        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);
        v[16] = _mm_add_epi32(u[16], k__DCT_CONST_ROUNDING);
        v[17] = _mm_add_epi32(u[17], k__DCT_CONST_ROUNDING);
        v[18] = _mm_add_epi32(u[18], k__DCT_CONST_ROUNDING);
        v[19] = _mm_add_epi32(u[19], k__DCT_CONST_ROUNDING);
        v[20] = _mm_add_epi32(u[20], k__DCT_CONST_ROUNDING);
        v[21] = _mm_add_epi32(u[21], k__DCT_CONST_ROUNDING);
        v[22] = _mm_add_epi32(u[22], k__DCT_CONST_ROUNDING);
        v[23] = _mm_add_epi32(u[23], k__DCT_CONST_ROUNDING);
        v[24] = _mm_add_epi32(u[24], k__DCT_CONST_ROUNDING);
        v[25] = _mm_add_epi32(u[25], k__DCT_CONST_ROUNDING);
        v[26] = _mm_add_epi32(u[26], k__DCT_CONST_ROUNDING);
        v[27] = _mm_add_epi32(u[27], k__DCT_CONST_ROUNDING);
        v[28] = _mm_add_epi32(u[28], k__DCT_CONST_ROUNDING);
        v[29] = _mm_add_epi32(u[29], k__DCT_CONST_ROUNDING);
        v[30] = _mm_add_epi32(u[30], k__DCT_CONST_ROUNDING);
        v[31] = _mm_add_epi32(u[31], k__DCT_CONST_ROUNDING);

        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
        u[16] = _mm_srai_epi32(v[16], DCT_CONST_BITS);
        u[17] = _mm_srai_epi32(v[17], DCT_CONST_BITS);
        u[18] = _mm_srai_epi32(v[18], DCT_CONST_BITS);
        u[19] = _mm_srai_epi32(v[19], DCT_CONST_BITS);
        u[20] = _mm_srai_epi32(v[20], DCT_CONST_BITS);
        u[21] = _mm_srai_epi32(v[21], DCT_CONST_BITS);
        u[22] = _mm_srai_epi32(v[22], DCT_CONST_BITS);
        u[23] = _mm_srai_epi32(v[23], DCT_CONST_BITS);
        u[24] = _mm_srai_epi32(v[24], DCT_CONST_BITS);
        u[25] = _mm_srai_epi32(v[25], DCT_CONST_BITS);
        u[26] = _mm_srai_epi32(v[26], DCT_CONST_BITS);
        u[27] = _mm_srai_epi32(v[27], DCT_CONST_BITS);
        u[28] = _mm_srai_epi32(v[28], DCT_CONST_BITS);
        u[29] = _mm_srai_epi32(v[29], DCT_CONST_BITS);
        u[30] = _mm_srai_epi32(v[30], DCT_CONST_BITS);
        u[31] = _mm_srai_epi32(v[31], DCT_CONST_BITS);

        s[0] = _mm_packs_epi32(u[0], u[1]);
        s[1] = _mm_packs_epi32(u[2], u[3]);
        s[2] = _mm_packs_epi32(u[4], u[5]);
        s[3] = _mm_packs_epi32(u[6], u[7]);
        s[4] = _mm_packs_epi32(u[8], u[9]);
        s[5] = _mm_packs_epi32(u[10], u[11]);
        s[6] = _mm_packs_epi32(u[12], u[13]);
        s[7] = _mm_packs_epi32(u[14], u[15]);
        s[8] = _mm_packs_epi32(u[16], u[17]);
        s[9] = _mm_packs_epi32(u[18], u[19]);
        s[10] = _mm_packs_epi32(u[20], u[21]);
        s[11] = _mm_packs_epi32(u[22], u[23]);
        s[12] = _mm_packs_epi32(u[24], u[25]);
        s[13] = _mm_packs_epi32(u[26], u[27]);
        s[14] = _mm_packs_epi32(u[28], u[29]);
        s[15] = _mm_packs_epi32(u[30], u[31]);

        // stage 2
        u[0] = _mm_unpacklo_epi16(s[8], s[9]);
        u[1] = _mm_unpackhi_epi16(s[8], s[9]);
        u[2] = _mm_unpacklo_epi16(s[10], s[11]);
        u[3] = _mm_unpackhi_epi16(s[10], s[11]);
        u[4] = _mm_unpacklo_epi16(s[12], s[13]);
        u[5] = _mm_unpackhi_epi16(s[12], s[13]);
        u[6] = _mm_unpacklo_epi16(s[14], s[15]);
        u[7] = _mm_unpackhi_epi16(s[14], s[15]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_p04_p28);
        v[1] = _mm_madd_epi16(u[1], k__cospi_p04_p28);
        v[2] = _mm_madd_epi16(u[0], k__cospi_p28_m04);
        v[3] = _mm_madd_epi16(u[1], k__cospi_p28_m04);
        v[4] = _mm_madd_epi16(u[2], k__cospi_p20_p12);
        v[5] = _mm_madd_epi16(u[3], k__cospi_p20_p12);
        v[6] = _mm_madd_epi16(u[2], k__cospi_p12_m20);
        v[7] = _mm_madd_epi16(u[3], k__cospi_p12_m20);
        v[8] = _mm_madd_epi16(u[4], k__cospi_m28_p04);
        v[9] = _mm_madd_epi16(u[5], k__cospi_m28_p04);
        v[10] = _mm_madd_epi16(u[4], k__cospi_p04_p28);
        v[11] = _mm_madd_epi16(u[5], k__cospi_p04_p28);
        v[12] = _mm_madd_epi16(u[6], k__cospi_m12_p20);
        v[13] = _mm_madd_epi16(u[7], k__cospi_m12_p20);
        v[14] = _mm_madd_epi16(u[6], k__cospi_p20_p12);
        v[15] = _mm_madd_epi16(u[7], k__cospi_p20_p12);

        u[0] = _mm_add_epi32(v[0], v[8]);
        u[1] = _mm_add_epi32(v[1], v[9]);
        u[2] = _mm_add_epi32(v[2], v[10]);
        u[3] = _mm_add_epi32(v[3], v[11]);
        u[4] = _mm_add_epi32(v[4], v[12]);
        u[5] = _mm_add_epi32(v[5], v[13]);
        u[6] = _mm_add_epi32(v[6], v[14]);
        u[7] = _mm_add_epi32(v[7], v[15]);
        u[8] = _mm_sub_epi32(v[0], v[8]);
        u[9] = _mm_sub_epi32(v[1], v[9]);
        u[10] = _mm_sub_epi32(v[2], v[10]);
        u[11] = _mm_sub_epi32(v[3], v[11]);
        u[12] = _mm_sub_epi32(v[4], v[12]);
        u[13] = _mm_sub_epi32(v[5], v[13]);
        u[14] = _mm_sub_epi32(v[6], v[14]);
        u[15] = _mm_sub_epi32(v[7], v[15]);

        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);

        x[0] = _mm_add_epi16(s[0], s[4]);
        x[1] = _mm_add_epi16(s[1], s[5]);
        x[2] = _mm_add_epi16(s[2], s[6]);
        x[3] = _mm_add_epi16(s[3], s[7]);
        x[4] = _mm_sub_epi16(s[0], s[4]);
        x[5] = _mm_sub_epi16(s[1], s[5]);
        x[6] = _mm_sub_epi16(s[2], s[6]);
        x[7] = _mm_sub_epi16(s[3], s[7]);
        x[8] = _mm_packs_epi32(u[0], u[1]);
        x[9] = _mm_packs_epi32(u[2], u[3]);
        x[10] = _mm_packs_epi32(u[4], u[5]);
        x[11] = _mm_packs_epi32(u[6], u[7]);
        x[12] = _mm_packs_epi32(u[8], u[9]);
        x[13] = _mm_packs_epi32(u[10], u[11]);
        x[14] = _mm_packs_epi32(u[12], u[13]);
        x[15] = _mm_packs_epi32(u[14], u[15]);

        // stage 3
        u[0] = _mm_unpacklo_epi16(x[4], x[5]);
        u[1] = _mm_unpackhi_epi16(x[4], x[5]);
        u[2] = _mm_unpacklo_epi16(x[6], x[7]);
        u[3] = _mm_unpackhi_epi16(x[6], x[7]);
        u[4] = _mm_unpacklo_epi16(x[12], x[13]);
        u[5] = _mm_unpackhi_epi16(x[12], x[13]);
        u[6] = _mm_unpacklo_epi16(x[14], x[15]);
        u[7] = _mm_unpackhi_epi16(x[14], x[15]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_p08_p24);
        v[1] = _mm_madd_epi16(u[1], k__cospi_p08_p24);
        v[2] = _mm_madd_epi16(u[0], k__cospi_p24_m08);
        v[3] = _mm_madd_epi16(u[1], k__cospi_p24_m08);
        v[4] = _mm_madd_epi16(u[2], k__cospi_m24_p08);
        v[5] = _mm_madd_epi16(u[3], k__cospi_m24_p08);
        v[6] = _mm_madd_epi16(u[2], k__cospi_p08_p24);
        v[7] = _mm_madd_epi16(u[3], k__cospi_p08_p24);
        v[8] = _mm_madd_epi16(u[4], k__cospi_p08_p24);
        v[9] = _mm_madd_epi16(u[5], k__cospi_p08_p24);
        v[10] = _mm_madd_epi16(u[4], k__cospi_p24_m08);
        v[11] = _mm_madd_epi16(u[5], k__cospi_p24_m08);
        v[12] = _mm_madd_epi16(u[6], k__cospi_m24_p08);
        v[13] = _mm_madd_epi16(u[7], k__cospi_m24_p08);
        v[14] = _mm_madd_epi16(u[6], k__cospi_p08_p24);
        v[15] = _mm_madd_epi16(u[7], k__cospi_p08_p24);

        u[0] = _mm_add_epi32(v[0], v[4]);
        u[1] = _mm_add_epi32(v[1], v[5]);
        u[2] = _mm_add_epi32(v[2], v[6]);
        u[3] = _mm_add_epi32(v[3], v[7]);
        u[4] = _mm_sub_epi32(v[0], v[4]);
        u[5] = _mm_sub_epi32(v[1], v[5]);
        u[6] = _mm_sub_epi32(v[2], v[6]);
        u[7] = _mm_sub_epi32(v[3], v[7]);
        u[8] = _mm_add_epi32(v[8], v[12]);
        u[9] = _mm_add_epi32(v[9], v[13]);
        u[10] = _mm_add_epi32(v[10], v[14]);
        u[11] = _mm_add_epi32(v[11], v[15]);
        u[12] = _mm_sub_epi32(v[8], v[12]);
        u[13] = _mm_sub_epi32(v[9], v[13]);
        u[14] = _mm_sub_epi32(v[10], v[14]);
        u[15] = _mm_sub_epi32(v[11], v[15]);

        u[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

        s[0] = _mm_add_epi16(x[0], x[2]);
        s[1] = _mm_add_epi16(x[1], x[3]);
        s[2] = _mm_sub_epi16(x[0], x[2]);
        s[3] = _mm_sub_epi16(x[1], x[3]);
        s[4] = _mm_packs_epi32(v[0], v[1]);
        s[5] = _mm_packs_epi32(v[2], v[3]);
        s[6] = _mm_packs_epi32(v[4], v[5]);
        s[7] = _mm_packs_epi32(v[6], v[7]);
        s[8] = _mm_add_epi16(x[8], x[10]);
        s[9] = _mm_add_epi16(x[9], x[11]);
        s[10] = _mm_sub_epi16(x[8], x[10]);
        s[11] = _mm_sub_epi16(x[9], x[11]);
        s[12] = _mm_packs_epi32(v[8], v[9]);
        s[13] = _mm_packs_epi32(v[10], v[11]);
        s[14] = _mm_packs_epi32(v[12], v[13]);
        s[15] = _mm_packs_epi32(v[14], v[15]);

        // stage 4
        u[0] = _mm_unpacklo_epi16(s[2], s[3]);
        u[1] = _mm_unpackhi_epi16(s[2], s[3]);
        u[2] = _mm_unpacklo_epi16(s[6], s[7]);
        u[3] = _mm_unpackhi_epi16(s[6], s[7]);
        u[4] = _mm_unpacklo_epi16(s[10], s[11]);
        u[5] = _mm_unpackhi_epi16(s[10], s[11]);
        u[6] = _mm_unpacklo_epi16(s[14], s[15]);
        u[7] = _mm_unpackhi_epi16(s[14], s[15]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_m16_m16);
        v[1] = _mm_madd_epi16(u[1], k__cospi_m16_m16);
        v[2] = _mm_madd_epi16(u[0], k__cospi_p16_m16);
        v[3] = _mm_madd_epi16(u[1], k__cospi_p16_m16);
        v[4] = _mm_madd_epi16(u[2], k__cospi_p16_p16);
        v[5] = _mm_madd_epi16(u[3], k__cospi_p16_p16);
        v[6] = _mm_madd_epi16(u[2], k__cospi_m16_p16);
        v[7] = _mm_madd_epi16(u[3], k__cospi_m16_p16);
        v[8] = _mm_madd_epi16(u[4], k__cospi_p16_p16);
        v[9] = _mm_madd_epi16(u[5], k__cospi_p16_p16);
        v[10] = _mm_madd_epi16(u[4], k__cospi_m16_p16);
        v[11] = _mm_madd_epi16(u[5], k__cospi_m16_p16);
        v[12] = _mm_madd_epi16(u[6], k__cospi_m16_m16);
        v[13] = _mm_madd_epi16(u[7], k__cospi_m16_m16);
        v[14] = _mm_madd_epi16(u[6], k__cospi_p16_m16);
        v[15] = _mm_madd_epi16(u[7], k__cospi_p16_m16);

        u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

        in[0] = s[0];
        in[1] = _mm_sub_epi16(kZero, s[8]);
        in[2] = s[12];
        in[3] = _mm_sub_epi16(kZero, s[4]);
        in[4] = _mm_packs_epi32(v[4], v[5]);
        in[5] = _mm_packs_epi32(v[12], v[13]);
        in[6] = _mm_packs_epi32(v[8], v[9]);
        in[7] = _mm_packs_epi32(v[0], v[1]);
        in[8] = _mm_packs_epi32(v[2], v[3]);
        in[9] = _mm_packs_epi32(v[10], v[11]);
        in[10] = _mm_packs_epi32(v[14], v[15]);
        in[11] = _mm_packs_epi32(v[6], v[7]);
        in[12] = s[5];
        in[13] = _mm_sub_epi16(kZero, s[13]);
        in[14] = s[9];
        in[15] = _mm_sub_epi16(kZero, s[1]);
    }

    static void fdct16_8col(__m128i *in)
    {
        // perform 16x16 1-D DCT for 8 columns
        __m128i i[8], s[8], p[8], t[8], u[16], v[16];
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_m16_p16 = pair_set_epi16(-cospi_16_64, cospi_16_64);
        const __m128i k__cospi_p24_p08 = pair_set_epi16(cospi_24_64, cospi_8_64);
        const __m128i k__cospi_p08_m24 = pair_set_epi16(cospi_8_64, -cospi_24_64);
        const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m128i k__cospi_p28_p04 = pair_set_epi16(cospi_28_64, cospi_4_64);
        const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m128i k__cospi_p12_p20 = pair_set_epi16(cospi_12_64, cospi_20_64);
        const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m128i k__cospi_p30_p02 = pair_set_epi16(cospi_30_64, cospi_2_64);
        const __m128i k__cospi_p14_p18 = pair_set_epi16(cospi_14_64, cospi_18_64);
        const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m128i k__cospi_p22_p10 = pair_set_epi16(cospi_22_64, cospi_10_64);
        const __m128i k__cospi_p06_p26 = pair_set_epi16(cospi_6_64, cospi_26_64);
        const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);

        // stage 1
        i[0] = _mm_add_epi16(in[0], in[15]);
        i[1] = _mm_add_epi16(in[1], in[14]);
        i[2] = _mm_add_epi16(in[2], in[13]);
        i[3] = _mm_add_epi16(in[3], in[12]);
        i[4] = _mm_add_epi16(in[4], in[11]);
        i[5] = _mm_add_epi16(in[5], in[10]);
        i[6] = _mm_add_epi16(in[6], in[9]);
        i[7] = _mm_add_epi16(in[7], in[8]);

        s[0] = _mm_sub_epi16(in[7], in[8]);
        s[1] = _mm_sub_epi16(in[6], in[9]);
        s[2] = _mm_sub_epi16(in[5], in[10]);
        s[3] = _mm_sub_epi16(in[4], in[11]);
        s[4] = _mm_sub_epi16(in[3], in[12]);
        s[5] = _mm_sub_epi16(in[2], in[13]);
        s[6] = _mm_sub_epi16(in[1], in[14]);
        s[7] = _mm_sub_epi16(in[0], in[15]);

        p[0] = _mm_add_epi16(i[0], i[7]);
        p[1] = _mm_add_epi16(i[1], i[6]);
        p[2] = _mm_add_epi16(i[2], i[5]);
        p[3] = _mm_add_epi16(i[3], i[4]);
        p[4] = _mm_sub_epi16(i[3], i[4]);
        p[5] = _mm_sub_epi16(i[2], i[5]);
        p[6] = _mm_sub_epi16(i[1], i[6]);
        p[7] = _mm_sub_epi16(i[0], i[7]);

        u[0] = _mm_add_epi16(p[0], p[3]);
        u[1] = _mm_add_epi16(p[1], p[2]);
        u[2] = _mm_sub_epi16(p[1], p[2]);
        u[3] = _mm_sub_epi16(p[0], p[3]);

        v[0] = _mm_unpacklo_epi16(u[0], u[1]);
        v[1] = _mm_unpackhi_epi16(u[0], u[1]);
        v[2] = _mm_unpacklo_epi16(u[2], u[3]);
        v[3] = _mm_unpackhi_epi16(u[2], u[3]);

        u[0] = _mm_madd_epi16(v[0], k__cospi_p16_p16);
        u[1] = _mm_madd_epi16(v[1], k__cospi_p16_p16);
        u[2] = _mm_madd_epi16(v[0], k__cospi_p16_m16);
        u[3] = _mm_madd_epi16(v[1], k__cospi_p16_m16);
        u[4] = _mm_madd_epi16(v[2], k__cospi_p24_p08);
        u[5] = _mm_madd_epi16(v[3], k__cospi_p24_p08);
        u[6] = _mm_madd_epi16(v[2], k__cospi_m08_p24);
        u[7] = _mm_madd_epi16(v[3], k__cospi_m08_p24);

        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);

        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);

        in[0] = _mm_packs_epi32(u[0], u[1]);
        in[4] = _mm_packs_epi32(u[4], u[5]);
        in[8] = _mm_packs_epi32(u[2], u[3]);
        in[12] = _mm_packs_epi32(u[6], u[7]);

        u[0] = _mm_unpacklo_epi16(p[5], p[6]);
        u[1] = _mm_unpackhi_epi16(p[5], p[6]);
        v[0] = _mm_madd_epi16(u[0], k__cospi_m16_p16);
        v[1] = _mm_madd_epi16(u[1], k__cospi_m16_p16);
        v[2] = _mm_madd_epi16(u[0], k__cospi_p16_p16);
        v[3] = _mm_madd_epi16(u[1], k__cospi_p16_p16);

        u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);

        v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);

        u[0] = _mm_packs_epi32(v[0], v[1]);
        u[1] = _mm_packs_epi32(v[2], v[3]);

        t[0] = _mm_add_epi16(p[4], u[0]);
        t[1] = _mm_sub_epi16(p[4], u[0]);
        t[2] = _mm_sub_epi16(p[7], u[1]);
        t[3] = _mm_add_epi16(p[7], u[1]);

        u[0] = _mm_unpacklo_epi16(t[0], t[3]);
        u[1] = _mm_unpackhi_epi16(t[0], t[3]);
        u[2] = _mm_unpacklo_epi16(t[1], t[2]);
        u[3] = _mm_unpackhi_epi16(t[1], t[2]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_p28_p04);
        v[1] = _mm_madd_epi16(u[1], k__cospi_p28_p04);
        v[2] = _mm_madd_epi16(u[2], k__cospi_p12_p20);
        v[3] = _mm_madd_epi16(u[3], k__cospi_p12_p20);
        v[4] = _mm_madd_epi16(u[2], k__cospi_m20_p12);
        v[5] = _mm_madd_epi16(u[3], k__cospi_m20_p12);
        v[6] = _mm_madd_epi16(u[0], k__cospi_m04_p28);
        v[7] = _mm_madd_epi16(u[1], k__cospi_m04_p28);

        u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

        in[2] = _mm_packs_epi32(v[0], v[1]);
        in[6] = _mm_packs_epi32(v[4], v[5]);
        in[10] = _mm_packs_epi32(v[2], v[3]);
        in[14] = _mm_packs_epi32(v[6], v[7]);

        // stage 2
        u[0] = _mm_unpacklo_epi16(s[2], s[5]);
        u[1] = _mm_unpackhi_epi16(s[2], s[5]);
        u[2] = _mm_unpacklo_epi16(s[3], s[4]);
        u[3] = _mm_unpackhi_epi16(s[3], s[4]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_m16_p16);
        v[1] = _mm_madd_epi16(u[1], k__cospi_m16_p16);
        v[2] = _mm_madd_epi16(u[2], k__cospi_m16_p16);
        v[3] = _mm_madd_epi16(u[3], k__cospi_m16_p16);
        v[4] = _mm_madd_epi16(u[2], k__cospi_p16_p16);
        v[5] = _mm_madd_epi16(u[3], k__cospi_p16_p16);
        v[6] = _mm_madd_epi16(u[0], k__cospi_p16_p16);
        v[7] = _mm_madd_epi16(u[1], k__cospi_p16_p16);

        u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

        t[2] = _mm_packs_epi32(v[0], v[1]);
        t[3] = _mm_packs_epi32(v[2], v[3]);
        t[4] = _mm_packs_epi32(v[4], v[5]);
        t[5] = _mm_packs_epi32(v[6], v[7]);

        // stage 3
        p[0] = _mm_add_epi16(s[0], t[3]);
        p[1] = _mm_add_epi16(s[1], t[2]);
        p[2] = _mm_sub_epi16(s[1], t[2]);
        p[3] = _mm_sub_epi16(s[0], t[3]);
        p[4] = _mm_sub_epi16(s[7], t[4]);
        p[5] = _mm_sub_epi16(s[6], t[5]);
        p[6] = _mm_add_epi16(s[6], t[5]);
        p[7] = _mm_add_epi16(s[7], t[4]);

        // stage 4
        u[0] = _mm_unpacklo_epi16(p[1], p[6]);
        u[1] = _mm_unpackhi_epi16(p[1], p[6]);
        u[2] = _mm_unpacklo_epi16(p[2], p[5]);
        u[3] = _mm_unpackhi_epi16(p[2], p[5]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_m08_p24);
        v[1] = _mm_madd_epi16(u[1], k__cospi_m08_p24);
        v[2] = _mm_madd_epi16(u[2], k__cospi_p24_p08);
        v[3] = _mm_madd_epi16(u[3], k__cospi_p24_p08);
        v[4] = _mm_madd_epi16(u[2], k__cospi_p08_m24);
        v[5] = _mm_madd_epi16(u[3], k__cospi_p08_m24);
        v[6] = _mm_madd_epi16(u[0], k__cospi_p24_p08);
        v[7] = _mm_madd_epi16(u[1], k__cospi_p24_p08);

        u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);

        v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);

        t[1] = _mm_packs_epi32(v[0], v[1]);
        t[2] = _mm_packs_epi32(v[2], v[3]);
        t[5] = _mm_packs_epi32(v[4], v[5]);
        t[6] = _mm_packs_epi32(v[6], v[7]);

        // stage 5
        s[0] = _mm_add_epi16(p[0], t[1]);
        s[1] = _mm_sub_epi16(p[0], t[1]);
        s[2] = _mm_add_epi16(p[3], t[2]);
        s[3] = _mm_sub_epi16(p[3], t[2]);
        s[4] = _mm_sub_epi16(p[4], t[5]);
        s[5] = _mm_add_epi16(p[4], t[5]);
        s[6] = _mm_sub_epi16(p[7], t[6]);
        s[7] = _mm_add_epi16(p[7], t[6]);

        // stage 6
        u[0] = _mm_unpacklo_epi16(s[0], s[7]);
        u[1] = _mm_unpackhi_epi16(s[0], s[7]);
        u[2] = _mm_unpacklo_epi16(s[1], s[6]);
        u[3] = _mm_unpackhi_epi16(s[1], s[6]);
        u[4] = _mm_unpacklo_epi16(s[2], s[5]);
        u[5] = _mm_unpackhi_epi16(s[2], s[5]);
        u[6] = _mm_unpacklo_epi16(s[3], s[4]);
        u[7] = _mm_unpackhi_epi16(s[3], s[4]);

        v[0] = _mm_madd_epi16(u[0], k__cospi_p30_p02);
        v[1] = _mm_madd_epi16(u[1], k__cospi_p30_p02);
        v[2] = _mm_madd_epi16(u[2], k__cospi_p14_p18);
        v[3] = _mm_madd_epi16(u[3], k__cospi_p14_p18);
        v[4] = _mm_madd_epi16(u[4], k__cospi_p22_p10);
        v[5] = _mm_madd_epi16(u[5], k__cospi_p22_p10);
        v[6] = _mm_madd_epi16(u[6], k__cospi_p06_p26);
        v[7] = _mm_madd_epi16(u[7], k__cospi_p06_p26);
        v[8] = _mm_madd_epi16(u[6], k__cospi_m26_p06);
        v[9] = _mm_madd_epi16(u[7], k__cospi_m26_p06);
        v[10] = _mm_madd_epi16(u[4], k__cospi_m10_p22);
        v[11] = _mm_madd_epi16(u[5], k__cospi_m10_p22);
        v[12] = _mm_madd_epi16(u[2], k__cospi_m18_p14);
        v[13] = _mm_madd_epi16(u[3], k__cospi_m18_p14);
        v[14] = _mm_madd_epi16(u[0], k__cospi_m02_p30);
        v[15] = _mm_madd_epi16(u[1], k__cospi_m02_p30);

        u[0] = _mm_add_epi32(v[0], k__DCT_CONST_ROUNDING);
        u[1] = _mm_add_epi32(v[1], k__DCT_CONST_ROUNDING);
        u[2] = _mm_add_epi32(v[2], k__DCT_CONST_ROUNDING);
        u[3] = _mm_add_epi32(v[3], k__DCT_CONST_ROUNDING);
        u[4] = _mm_add_epi32(v[4], k__DCT_CONST_ROUNDING);
        u[5] = _mm_add_epi32(v[5], k__DCT_CONST_ROUNDING);
        u[6] = _mm_add_epi32(v[6], k__DCT_CONST_ROUNDING);
        u[7] = _mm_add_epi32(v[7], k__DCT_CONST_ROUNDING);
        u[8] = _mm_add_epi32(v[8], k__DCT_CONST_ROUNDING);
        u[9] = _mm_add_epi32(v[9], k__DCT_CONST_ROUNDING);
        u[10] = _mm_add_epi32(v[10], k__DCT_CONST_ROUNDING);
        u[11] = _mm_add_epi32(v[11], k__DCT_CONST_ROUNDING);
        u[12] = _mm_add_epi32(v[12], k__DCT_CONST_ROUNDING);
        u[13] = _mm_add_epi32(v[13], k__DCT_CONST_ROUNDING);
        u[14] = _mm_add_epi32(v[14], k__DCT_CONST_ROUNDING);
        u[15] = _mm_add_epi32(v[15], k__DCT_CONST_ROUNDING);

        v[0] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
        v[1] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
        v[2] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
        v[3] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
        v[4] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
        v[5] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
        v[6] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
        v[7] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
        v[8] = _mm_srai_epi32(u[8], DCT_CONST_BITS);
        v[9] = _mm_srai_epi32(u[9], DCT_CONST_BITS);
        v[10] = _mm_srai_epi32(u[10], DCT_CONST_BITS);
        v[11] = _mm_srai_epi32(u[11], DCT_CONST_BITS);
        v[12] = _mm_srai_epi32(u[12], DCT_CONST_BITS);
        v[13] = _mm_srai_epi32(u[13], DCT_CONST_BITS);
        v[14] = _mm_srai_epi32(u[14], DCT_CONST_BITS);
        v[15] = _mm_srai_epi32(u[15], DCT_CONST_BITS);

        in[1] = _mm_packs_epi32(v[0], v[1]);
        in[9] = _mm_packs_epi32(v[2], v[3]);
        in[5] = _mm_packs_epi32(v[4], v[5]);
        in[13] = _mm_packs_epi32(v[6], v[7]);
        in[3] = _mm_packs_epi32(v[8], v[9]);
        in[11] = _mm_packs_epi32(v[10], v[11]);
        in[7] = _mm_packs_epi32(v[12], v[13]);
        in[15] = _mm_packs_epi32(v[14], v[15]);
    }

    static void fdct16_sse2(__m128i *in0, __m128i *in1)
    {
        fdct16_8col(in0);
        fdct16_8col(in1);
        array_transpose_16x16(in0, in1);
    }

    static void fadst16_sse2(__m128i *in0, __m128i *in1)
    {
        fadst16_8col(in0);
        fadst16_8col(in1);
        array_transpose_16x16(in0, in1);
    }

    template <int tx_type> void fht16x16_sse2(const int16_t *input, tran_low_t *output, int stride)
    {
        __m128i in0[16], in1[16];

        if (tx_type == DCT_DCT) {
            fdct16x16_sse2(input, output, stride);
        } else if (tx_type == ADST_DCT) {
            load_buffer_16x16(input, in0, in1, stride);
            fadst16_sse2(in0, in1);
            right_shift_16x16(in0, in1);
            fdct16_sse2(in0, in1);
            write_buffer_16x16(output, in0, in1, 16);
        } else if (tx_type == DCT_ADST) {
            load_buffer_16x16(input, in0, in1, stride);
            fdct16_sse2(in0, in1);
            right_shift_16x16(in0, in1);
            fadst16_sse2(in0, in1);
            write_buffer_16x16(output, in0, in1, 16);
        } else if (tx_type == ADST_ADST) {
            load_buffer_16x16(input, in0, in1, stride);
            fadst16_sse2(in0, in1);
            right_shift_16x16(in0, in1);
            fadst16_sse2(in0, in1);
            write_buffer_16x16(output, in0, in1, 16);
        } else {
            assert(0);
        }
    }

    //-----------------------------------------------------
    //                    32x32
    //-----------------------------------------------------
    #define FDCT32x32_HIGH_PRECISION 1
    void fdct32x32_sse2(const int16_t *input, tran_low_t *output_org, int stride)
    {
        // Calculate pre-multiplied strides
        const int str1 = stride;
        const int str2 = 2 * stride;
        const int str3 = 2 * stride + str1;
        // We need an intermediate buffer between passes.
        DECLARE_ALIGNED(16, int16_t, intermediate[32 * 32]);
        // Constants
        //    When we use them, in one case, they are all the same. In all others
        //    it's a pair of them that we need to repeat four times. This is done
        //    by constructing the 32 bit constant corresponding to that pair.
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(+cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m128i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
        const __m128i k__cospi_p24_p08 = pair_set_epi16(+cospi_24_64, cospi_8_64);
        const __m128i k__cospi_p12_p20 = pair_set_epi16(+cospi_12_64, cospi_20_64);
        const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m128i k__cospi_p28_p04 = pair_set_epi16(+cospi_28_64, cospi_4_64);
        const __m128i k__cospi_m28_m04 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
        const __m128i k__cospi_m12_m20 = pair_set_epi16(-cospi_12_64, -cospi_20_64);
        const __m128i k__cospi_p30_p02 = pair_set_epi16(+cospi_30_64, cospi_2_64);
        const __m128i k__cospi_p14_p18 = pair_set_epi16(+cospi_14_64, cospi_18_64);
        const __m128i k__cospi_p22_p10 = pair_set_epi16(+cospi_22_64, cospi_10_64);
        const __m128i k__cospi_p06_p26 = pair_set_epi16(+cospi_6_64, cospi_26_64);
        const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m128i k__cospi_p31_p01 = pair_set_epi16(+cospi_31_64, cospi_1_64);
        const __m128i k__cospi_p15_p17 = pair_set_epi16(+cospi_15_64, cospi_17_64);
        const __m128i k__cospi_p23_p09 = pair_set_epi16(+cospi_23_64, cospi_9_64);
        const __m128i k__cospi_p07_p25 = pair_set_epi16(+cospi_7_64, cospi_25_64);
        const __m128i k__cospi_m25_p07 = pair_set_epi16(-cospi_25_64, cospi_7_64);
        const __m128i k__cospi_m09_p23 = pair_set_epi16(-cospi_9_64, cospi_23_64);
        const __m128i k__cospi_m17_p15 = pair_set_epi16(-cospi_17_64, cospi_15_64);
        const __m128i k__cospi_m01_p31 = pair_set_epi16(-cospi_1_64, cospi_31_64);
        const __m128i k__cospi_p27_p05 = pair_set_epi16(+cospi_27_64, cospi_5_64);
        const __m128i k__cospi_p11_p21 = pair_set_epi16(+cospi_11_64, cospi_21_64);
        const __m128i k__cospi_p19_p13 = pair_set_epi16(+cospi_19_64, cospi_13_64);
        const __m128i k__cospi_p03_p29 = pair_set_epi16(+cospi_3_64, cospi_29_64);
        const __m128i k__cospi_m29_p03 = pair_set_epi16(-cospi_29_64, cospi_3_64);
        const __m128i k__cospi_m13_p19 = pair_set_epi16(-cospi_13_64, cospi_19_64);
        const __m128i k__cospi_m21_p11 = pair_set_epi16(-cospi_21_64, cospi_11_64);
        const __m128i k__cospi_m05_p27 = pair_set_epi16(-cospi_5_64, cospi_27_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
        const __m128i kZero = _mm_set1_epi16(0);
        const __m128i kOne = _mm_set1_epi16(1);
        // Do the two transform/transpose passes
        int pass;
#if DCT_HIGH_BIT_DEPTH
        int overflow;
#endif
        for (pass = 0; pass < 2; ++pass) {
            // We process eight columns (transposed rows in second pass) at a time.
            int column_start;
            for (column_start = 0; column_start < 32; column_start += 8) {
                __m128i step1[32];
                __m128i step2[32];
                __m128i step3[32];
                __m128i out[32];
                // Stage 1
                // Note: even though all the loads below are aligned, using the aligned
                //       intrinsic make the code slightly slower.
                if (0 == pass) {
                    const int16_t *in = &input[column_start];
                    // step1[i] =  (in[ 0 * stride] + in[(32 -  1) * stride]) << 2;
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        const int16_t *ina = in + 0 * str1;
                        const int16_t *inb = in + 31 * str1;
                        __m128i *step1a = &step1[0];
                        __m128i *step1b = &step1[31];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const int16_t *ina = in + 4 * str1;
                        const int16_t *inb = in + 27 * str1;
                        __m128i *step1a = &step1[4];
                        __m128i *step1b = &step1[27];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const int16_t *ina = in + 8 * str1;
                        const int16_t *inb = in + 23 * str1;
                        __m128i *step1a = &step1[8];
                        __m128i *step1b = &step1[23];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const int16_t *ina = in + 12 * str1;
                        const int16_t *inb = in + 19 * str1;
                        __m128i *step1a = &step1[12];
                        __m128i *step1b = &step1[19];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                } else {
                    int16_t *in = &intermediate[column_start];
                    // step1[i] =  in[ 0 * 32] + in[(32 -  1) * 32];
                    // Note: using the same approach as above to have common offset is
                    //       counter-productive as all offsets can be calculated at compile
                    //       time.
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        __m128i in00 = _mm_loadu_si128((const __m128i *)(in + 0 * 32));
                        __m128i in01 = _mm_loadu_si128((const __m128i *)(in + 1 * 32));
                        __m128i in02 = _mm_loadu_si128((const __m128i *)(in + 2 * 32));
                        __m128i in03 = _mm_loadu_si128((const __m128i *)(in + 3 * 32));
                        __m128i in28 = _mm_loadu_si128((const __m128i *)(in + 28 * 32));
                        __m128i in29 = _mm_loadu_si128((const __m128i *)(in + 29 * 32));
                        __m128i in30 = _mm_loadu_si128((const __m128i *)(in + 30 * 32));
                        __m128i in31 = _mm_loadu_si128((const __m128i *)(in + 31 * 32));
                        step1[0] = ADD_EPI16(in00, in31);
                        step1[1] = ADD_EPI16(in01, in30);
                        step1[2] = ADD_EPI16(in02, in29);
                        step1[3] = ADD_EPI16(in03, in28);
                        step1[28] = SUB_EPI16(in03, in28);
                        step1[29] = SUB_EPI16(in02, in29);
                        step1[30] = SUB_EPI16(in01, in30);
                        step1[31] = SUB_EPI16(in00, in31);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[0], &step1[1], &step1[2],
                            &step1[3], &step1[28], &step1[29],
                            &step1[30], &step1[31]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        __m128i in04 = _mm_loadu_si128((const __m128i *)(in + 4 * 32));
                        __m128i in05 = _mm_loadu_si128((const __m128i *)(in + 5 * 32));
                        __m128i in06 = _mm_loadu_si128((const __m128i *)(in + 6 * 32));
                        __m128i in07 = _mm_loadu_si128((const __m128i *)(in + 7 * 32));
                        __m128i in24 = _mm_loadu_si128((const __m128i *)(in + 24 * 32));
                        __m128i in25 = _mm_loadu_si128((const __m128i *)(in + 25 * 32));
                        __m128i in26 = _mm_loadu_si128((const __m128i *)(in + 26 * 32));
                        __m128i in27 = _mm_loadu_si128((const __m128i *)(in + 27 * 32));
                        step1[4] = ADD_EPI16(in04, in27);
                        step1[5] = ADD_EPI16(in05, in26);
                        step1[6] = ADD_EPI16(in06, in25);
                        step1[7] = ADD_EPI16(in07, in24);
                        step1[24] = SUB_EPI16(in07, in24);
                        step1[25] = SUB_EPI16(in06, in25);
                        step1[26] = SUB_EPI16(in05, in26);
                        step1[27] = SUB_EPI16(in04, in27);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[4], &step1[5], &step1[6],
                            &step1[7], &step1[24], &step1[25],
                            &step1[26], &step1[27]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        __m128i in08 = _mm_loadu_si128((const __m128i *)(in + 8 * 32));
                        __m128i in09 = _mm_loadu_si128((const __m128i *)(in + 9 * 32));
                        __m128i in10 = _mm_loadu_si128((const __m128i *)(in + 10 * 32));
                        __m128i in11 = _mm_loadu_si128((const __m128i *)(in + 11 * 32));
                        __m128i in20 = _mm_loadu_si128((const __m128i *)(in + 20 * 32));
                        __m128i in21 = _mm_loadu_si128((const __m128i *)(in + 21 * 32));
                        __m128i in22 = _mm_loadu_si128((const __m128i *)(in + 22 * 32));
                        __m128i in23 = _mm_loadu_si128((const __m128i *)(in + 23 * 32));
                        step1[8] = ADD_EPI16(in08, in23);
                        step1[9] = ADD_EPI16(in09, in22);
                        step1[10] = ADD_EPI16(in10, in21);
                        step1[11] = ADD_EPI16(in11, in20);
                        step1[20] = SUB_EPI16(in11, in20);
                        step1[21] = SUB_EPI16(in10, in21);
                        step1[22] = SUB_EPI16(in09, in22);
                        step1[23] = SUB_EPI16(in08, in23);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[8], &step1[9], &step1[10],
                            &step1[11], &step1[20], &step1[21],
                            &step1[22], &step1[23]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        __m128i in12 = _mm_loadu_si128((const __m128i *)(in + 12 * 32));
                        __m128i in13 = _mm_loadu_si128((const __m128i *)(in + 13 * 32));
                        __m128i in14 = _mm_loadu_si128((const __m128i *)(in + 14 * 32));
                        __m128i in15 = _mm_loadu_si128((const __m128i *)(in + 15 * 32));
                        __m128i in16 = _mm_loadu_si128((const __m128i *)(in + 16 * 32));
                        __m128i in17 = _mm_loadu_si128((const __m128i *)(in + 17 * 32));
                        __m128i in18 = _mm_loadu_si128((const __m128i *)(in + 18 * 32));
                        __m128i in19 = _mm_loadu_si128((const __m128i *)(in + 19 * 32));
                        step1[12] = ADD_EPI16(in12, in19);
                        step1[13] = ADD_EPI16(in13, in18);
                        step1[14] = ADD_EPI16(in14, in17);
                        step1[15] = ADD_EPI16(in15, in16);
                        step1[16] = SUB_EPI16(in15, in16);
                        step1[17] = SUB_EPI16(in14, in17);
                        step1[18] = SUB_EPI16(in13, in18);
                        step1[19] = SUB_EPI16(in12, in19);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[12], &step1[13], &step1[14],
                            &step1[15], &step1[16], &step1[17],
                            &step1[18], &step1[19]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                }
                // Stage 2
                {
                    step2[0] = ADD_EPI16(step1[0], step1[15]);
                    step2[1] = ADD_EPI16(step1[1], step1[14]);
                    step2[2] = ADD_EPI16(step1[2], step1[13]);
                    step2[3] = ADD_EPI16(step1[3], step1[12]);
                    step2[4] = ADD_EPI16(step1[4], step1[11]);
                    step2[5] = ADD_EPI16(step1[5], step1[10]);
                    step2[6] = ADD_EPI16(step1[6], step1[9]);
                    step2[7] = ADD_EPI16(step1[7], step1[8]);
                    step2[8] = SUB_EPI16(step1[7], step1[8]);
                    step2[9] = SUB_EPI16(step1[6], step1[9]);
                    step2[10] = SUB_EPI16(step1[5], step1[10]);
                    step2[11] = SUB_EPI16(step1[4], step1[11]);
                    step2[12] = SUB_EPI16(step1[3], step1[12]);
                    step2[13] = SUB_EPI16(step1[2], step1[13]);
                    step2[14] = SUB_EPI16(step1[1], step1[14]);
                    step2[15] = SUB_EPI16(step1[0], step1[15]);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x16(
                        &step2[0], &step2[1], &step2[2], &step2[3], &step2[4], &step2[5],
                        &step2[6], &step2[7], &step2[8], &step2[9], &step2[10], &step2[11],
                        &step2[12], &step2[13], &step2[14], &step2[15]);
                    if (overflow) {
                        if (pass == 0)
                            HIGH_FDCT32x32_2D_C(input, output_org, stride);
                        else
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                }
                {
                    const __m128i s2_20_0 = _mm_unpacklo_epi16(step1[27], step1[20]);
                    const __m128i s2_20_1 = _mm_unpackhi_epi16(step1[27], step1[20]);
                    const __m128i s2_21_0 = _mm_unpacklo_epi16(step1[26], step1[21]);
                    const __m128i s2_21_1 = _mm_unpackhi_epi16(step1[26], step1[21]);
                    const __m128i s2_22_0 = _mm_unpacklo_epi16(step1[25], step1[22]);
                    const __m128i s2_22_1 = _mm_unpackhi_epi16(step1[25], step1[22]);
                    const __m128i s2_23_0 = _mm_unpacklo_epi16(step1[24], step1[23]);
                    const __m128i s2_23_1 = _mm_unpackhi_epi16(step1[24], step1[23]);
                    const __m128i s2_20_2 = _mm_madd_epi16(s2_20_0, k__cospi_p16_m16);
                    const __m128i s2_20_3 = _mm_madd_epi16(s2_20_1, k__cospi_p16_m16);
                    const __m128i s2_21_2 = _mm_madd_epi16(s2_21_0, k__cospi_p16_m16);
                    const __m128i s2_21_3 = _mm_madd_epi16(s2_21_1, k__cospi_p16_m16);
                    const __m128i s2_22_2 = _mm_madd_epi16(s2_22_0, k__cospi_p16_m16);
                    const __m128i s2_22_3 = _mm_madd_epi16(s2_22_1, k__cospi_p16_m16);
                    const __m128i s2_23_2 = _mm_madd_epi16(s2_23_0, k__cospi_p16_m16);
                    const __m128i s2_23_3 = _mm_madd_epi16(s2_23_1, k__cospi_p16_m16);
                    const __m128i s2_24_2 = _mm_madd_epi16(s2_23_0, k__cospi_p16_p16);
                    const __m128i s2_24_3 = _mm_madd_epi16(s2_23_1, k__cospi_p16_p16);
                    const __m128i s2_25_2 = _mm_madd_epi16(s2_22_0, k__cospi_p16_p16);
                    const __m128i s2_25_3 = _mm_madd_epi16(s2_22_1, k__cospi_p16_p16);
                    const __m128i s2_26_2 = _mm_madd_epi16(s2_21_0, k__cospi_p16_p16);
                    const __m128i s2_26_3 = _mm_madd_epi16(s2_21_1, k__cospi_p16_p16);
                    const __m128i s2_27_2 = _mm_madd_epi16(s2_20_0, k__cospi_p16_p16);
                    const __m128i s2_27_3 = _mm_madd_epi16(s2_20_1, k__cospi_p16_p16);
                    // dct_const_round_shift
                    const __m128i s2_20_4 = _mm_add_epi32(s2_20_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_20_5 = _mm_add_epi32(s2_20_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_21_4 = _mm_add_epi32(s2_21_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_21_5 = _mm_add_epi32(s2_21_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_22_4 = _mm_add_epi32(s2_22_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_22_5 = _mm_add_epi32(s2_22_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_23_4 = _mm_add_epi32(s2_23_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_23_5 = _mm_add_epi32(s2_23_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_24_4 = _mm_add_epi32(s2_24_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_24_5 = _mm_add_epi32(s2_24_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_25_4 = _mm_add_epi32(s2_25_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_25_5 = _mm_add_epi32(s2_25_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_26_4 = _mm_add_epi32(s2_26_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_26_5 = _mm_add_epi32(s2_26_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_27_4 = _mm_add_epi32(s2_27_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_27_5 = _mm_add_epi32(s2_27_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_20_6 = _mm_srai_epi32(s2_20_4, DCT_CONST_BITS);
                    const __m128i s2_20_7 = _mm_srai_epi32(s2_20_5, DCT_CONST_BITS);
                    const __m128i s2_21_6 = _mm_srai_epi32(s2_21_4, DCT_CONST_BITS);
                    const __m128i s2_21_7 = _mm_srai_epi32(s2_21_5, DCT_CONST_BITS);
                    const __m128i s2_22_6 = _mm_srai_epi32(s2_22_4, DCT_CONST_BITS);
                    const __m128i s2_22_7 = _mm_srai_epi32(s2_22_5, DCT_CONST_BITS);
                    const __m128i s2_23_6 = _mm_srai_epi32(s2_23_4, DCT_CONST_BITS);
                    const __m128i s2_23_7 = _mm_srai_epi32(s2_23_5, DCT_CONST_BITS);
                    const __m128i s2_24_6 = _mm_srai_epi32(s2_24_4, DCT_CONST_BITS);
                    const __m128i s2_24_7 = _mm_srai_epi32(s2_24_5, DCT_CONST_BITS);
                    const __m128i s2_25_6 = _mm_srai_epi32(s2_25_4, DCT_CONST_BITS);
                    const __m128i s2_25_7 = _mm_srai_epi32(s2_25_5, DCT_CONST_BITS);
                    const __m128i s2_26_6 = _mm_srai_epi32(s2_26_4, DCT_CONST_BITS);
                    const __m128i s2_26_7 = _mm_srai_epi32(s2_26_5, DCT_CONST_BITS);
                    const __m128i s2_27_6 = _mm_srai_epi32(s2_27_4, DCT_CONST_BITS);
                    const __m128i s2_27_7 = _mm_srai_epi32(s2_27_5, DCT_CONST_BITS);
                    // Combine
                    step2[20] = _mm_packs_epi32(s2_20_6, s2_20_7);
                    step2[21] = _mm_packs_epi32(s2_21_6, s2_21_7);
                    step2[22] = _mm_packs_epi32(s2_22_6, s2_22_7);
                    step2[23] = _mm_packs_epi32(s2_23_6, s2_23_7);
                    step2[24] = _mm_packs_epi32(s2_24_6, s2_24_7);
                    step2[25] = _mm_packs_epi32(s2_25_6, s2_25_7);
                    step2[26] = _mm_packs_epi32(s2_26_6, s2_26_7);
                    step2[27] = _mm_packs_epi32(s2_27_6, s2_27_7);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x8(&step2[20], &step2[21], &step2[22],
                        &step2[23], &step2[24], &step2[25],
                        &step2[26], &step2[27]);
                    if (overflow) {
                        if (pass == 0)
                            HIGH_FDCT32x32_2D_C(input, output_org, stride);
                        else
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                }

#if !FDCT32x32_HIGH_PRECISION
                // dump the magnitude by half, hence the intermediate values are within
                // the range of 16 bits.
                if (1 == pass) {
                    __m128i s3_00_0 = _mm_cmplt_epi16(step2[0], kZero);
                    __m128i s3_01_0 = _mm_cmplt_epi16(step2[1], kZero);
                    __m128i s3_02_0 = _mm_cmplt_epi16(step2[2], kZero);
                    __m128i s3_03_0 = _mm_cmplt_epi16(step2[3], kZero);
                    __m128i s3_04_0 = _mm_cmplt_epi16(step2[4], kZero);
                    __m128i s3_05_0 = _mm_cmplt_epi16(step2[5], kZero);
                    __m128i s3_06_0 = _mm_cmplt_epi16(step2[6], kZero);
                    __m128i s3_07_0 = _mm_cmplt_epi16(step2[7], kZero);
                    __m128i s2_08_0 = _mm_cmplt_epi16(step2[8], kZero);
                    __m128i s2_09_0 = _mm_cmplt_epi16(step2[9], kZero);
                    __m128i s3_10_0 = _mm_cmplt_epi16(step2[10], kZero);
                    __m128i s3_11_0 = _mm_cmplt_epi16(step2[11], kZero);
                    __m128i s3_12_0 = _mm_cmplt_epi16(step2[12], kZero);
                    __m128i s3_13_0 = _mm_cmplt_epi16(step2[13], kZero);
                    __m128i s2_14_0 = _mm_cmplt_epi16(step2[14], kZero);
                    __m128i s2_15_0 = _mm_cmplt_epi16(step2[15], kZero);
                    __m128i s3_16_0 = _mm_cmplt_epi16(step1[16], kZero);
                    __m128i s3_17_0 = _mm_cmplt_epi16(step1[17], kZero);
                    __m128i s3_18_0 = _mm_cmplt_epi16(step1[18], kZero);
                    __m128i s3_19_0 = _mm_cmplt_epi16(step1[19], kZero);
                    __m128i s3_20_0 = _mm_cmplt_epi16(step2[20], kZero);
                    __m128i s3_21_0 = _mm_cmplt_epi16(step2[21], kZero);
                    __m128i s3_22_0 = _mm_cmplt_epi16(step2[22], kZero);
                    __m128i s3_23_0 = _mm_cmplt_epi16(step2[23], kZero);
                    __m128i s3_24_0 = _mm_cmplt_epi16(step2[24], kZero);
                    __m128i s3_25_0 = _mm_cmplt_epi16(step2[25], kZero);
                    __m128i s3_26_0 = _mm_cmplt_epi16(step2[26], kZero);
                    __m128i s3_27_0 = _mm_cmplt_epi16(step2[27], kZero);
                    __m128i s3_28_0 = _mm_cmplt_epi16(step1[28], kZero);
                    __m128i s3_29_0 = _mm_cmplt_epi16(step1[29], kZero);
                    __m128i s3_30_0 = _mm_cmplt_epi16(step1[30], kZero);
                    __m128i s3_31_0 = _mm_cmplt_epi16(step1[31], kZero);

                    step2[0] = SUB_EPI16(step2[0], s3_00_0);
                    step2[1] = SUB_EPI16(step2[1], s3_01_0);
                    step2[2] = SUB_EPI16(step2[2], s3_02_0);
                    step2[3] = SUB_EPI16(step2[3], s3_03_0);
                    step2[4] = SUB_EPI16(step2[4], s3_04_0);
                    step2[5] = SUB_EPI16(step2[5], s3_05_0);
                    step2[6] = SUB_EPI16(step2[6], s3_06_0);
                    step2[7] = SUB_EPI16(step2[7], s3_07_0);
                    step2[8] = SUB_EPI16(step2[8], s2_08_0);
                    step2[9] = SUB_EPI16(step2[9], s2_09_0);
                    step2[10] = SUB_EPI16(step2[10], s3_10_0);
                    step2[11] = SUB_EPI16(step2[11], s3_11_0);
                    step2[12] = SUB_EPI16(step2[12], s3_12_0);
                    step2[13] = SUB_EPI16(step2[13], s3_13_0);
                    step2[14] = SUB_EPI16(step2[14], s2_14_0);
                    step2[15] = SUB_EPI16(step2[15], s2_15_0);
                    step1[16] = SUB_EPI16(step1[16], s3_16_0);
                    step1[17] = SUB_EPI16(step1[17], s3_17_0);
                    step1[18] = SUB_EPI16(step1[18], s3_18_0);
                    step1[19] = SUB_EPI16(step1[19], s3_19_0);
                    step2[20] = SUB_EPI16(step2[20], s3_20_0);
                    step2[21] = SUB_EPI16(step2[21], s3_21_0);
                    step2[22] = SUB_EPI16(step2[22], s3_22_0);
                    step2[23] = SUB_EPI16(step2[23], s3_23_0);
                    step2[24] = SUB_EPI16(step2[24], s3_24_0);
                    step2[25] = SUB_EPI16(step2[25], s3_25_0);
                    step2[26] = SUB_EPI16(step2[26], s3_26_0);
                    step2[27] = SUB_EPI16(step2[27], s3_27_0);
                    step1[28] = SUB_EPI16(step1[28], s3_28_0);
                    step1[29] = SUB_EPI16(step1[29], s3_29_0);
                    step1[30] = SUB_EPI16(step1[30], s3_30_0);
                    step1[31] = SUB_EPI16(step1[31], s3_31_0);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x32(
                        &step2[0], &step2[1], &step2[2], &step2[3], &step2[4], &step2[5],
                        &step2[6], &step2[7], &step2[8], &step2[9], &step2[10], &step2[11],
                        &step2[12], &step2[13], &step2[14], &step2[15], &step1[16],
                        &step1[17], &step1[18], &step1[19], &step2[20], &step2[21],
                        &step2[22], &step2[23], &step2[24], &step2[25], &step2[26],
                        &step2[27], &step1[28], &step1[29], &step1[30], &step1[31]);
                    if (overflow) {
                        HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                    step2[0] = _mm_add_epi16(step2[0], kOne);
                    step2[1] = _mm_add_epi16(step2[1], kOne);
                    step2[2] = _mm_add_epi16(step2[2], kOne);
                    step2[3] = _mm_add_epi16(step2[3], kOne);
                    step2[4] = _mm_add_epi16(step2[4], kOne);
                    step2[5] = _mm_add_epi16(step2[5], kOne);
                    step2[6] = _mm_add_epi16(step2[6], kOne);
                    step2[7] = _mm_add_epi16(step2[7], kOne);
                    step2[8] = _mm_add_epi16(step2[8], kOne);
                    step2[9] = _mm_add_epi16(step2[9], kOne);
                    step2[10] = _mm_add_epi16(step2[10], kOne);
                    step2[11] = _mm_add_epi16(step2[11], kOne);
                    step2[12] = _mm_add_epi16(step2[12], kOne);
                    step2[13] = _mm_add_epi16(step2[13], kOne);
                    step2[14] = _mm_add_epi16(step2[14], kOne);
                    step2[15] = _mm_add_epi16(step2[15], kOne);
                    step1[16] = _mm_add_epi16(step1[16], kOne);
                    step1[17] = _mm_add_epi16(step1[17], kOne);
                    step1[18] = _mm_add_epi16(step1[18], kOne);
                    step1[19] = _mm_add_epi16(step1[19], kOne);
                    step2[20] = _mm_add_epi16(step2[20], kOne);
                    step2[21] = _mm_add_epi16(step2[21], kOne);
                    step2[22] = _mm_add_epi16(step2[22], kOne);
                    step2[23] = _mm_add_epi16(step2[23], kOne);
                    step2[24] = _mm_add_epi16(step2[24], kOne);
                    step2[25] = _mm_add_epi16(step2[25], kOne);
                    step2[26] = _mm_add_epi16(step2[26], kOne);
                    step2[27] = _mm_add_epi16(step2[27], kOne);
                    step1[28] = _mm_add_epi16(step1[28], kOne);
                    step1[29] = _mm_add_epi16(step1[29], kOne);
                    step1[30] = _mm_add_epi16(step1[30], kOne);
                    step1[31] = _mm_add_epi16(step1[31], kOne);

                    step2[0] = _mm_srai_epi16(step2[0], 2);
                    step2[1] = _mm_srai_epi16(step2[1], 2);
                    step2[2] = _mm_srai_epi16(step2[2], 2);
                    step2[3] = _mm_srai_epi16(step2[3], 2);
                    step2[4] = _mm_srai_epi16(step2[4], 2);
                    step2[5] = _mm_srai_epi16(step2[5], 2);
                    step2[6] = _mm_srai_epi16(step2[6], 2);
                    step2[7] = _mm_srai_epi16(step2[7], 2);
                    step2[8] = _mm_srai_epi16(step2[8], 2);
                    step2[9] = _mm_srai_epi16(step2[9], 2);
                    step2[10] = _mm_srai_epi16(step2[10], 2);
                    step2[11] = _mm_srai_epi16(step2[11], 2);
                    step2[12] = _mm_srai_epi16(step2[12], 2);
                    step2[13] = _mm_srai_epi16(step2[13], 2);
                    step2[14] = _mm_srai_epi16(step2[14], 2);
                    step2[15] = _mm_srai_epi16(step2[15], 2);
                    step1[16] = _mm_srai_epi16(step1[16], 2);
                    step1[17] = _mm_srai_epi16(step1[17], 2);
                    step1[18] = _mm_srai_epi16(step1[18], 2);
                    step1[19] = _mm_srai_epi16(step1[19], 2);
                    step2[20] = _mm_srai_epi16(step2[20], 2);
                    step2[21] = _mm_srai_epi16(step2[21], 2);
                    step2[22] = _mm_srai_epi16(step2[22], 2);
                    step2[23] = _mm_srai_epi16(step2[23], 2);
                    step2[24] = _mm_srai_epi16(step2[24], 2);
                    step2[25] = _mm_srai_epi16(step2[25], 2);
                    step2[26] = _mm_srai_epi16(step2[26], 2);
                    step2[27] = _mm_srai_epi16(step2[27], 2);
                    step1[28] = _mm_srai_epi16(step1[28], 2);
                    step1[29] = _mm_srai_epi16(step1[29], 2);
                    step1[30] = _mm_srai_epi16(step1[30], 2);
                    step1[31] = _mm_srai_epi16(step1[31], 2);
                }
#endif  // !FDCT32x32_HIGH_PRECISION

#if FDCT32x32_HIGH_PRECISION
                if (pass == 0) {
#endif
                    // Stage 3
                    {
                        step3[0] = ADD_EPI16(step2[(8 - 1)], step2[0]);
                        step3[1] = ADD_EPI16(step2[(8 - 2)], step2[1]);
                        step3[2] = ADD_EPI16(step2[(8 - 3)], step2[2]);
                        step3[3] = ADD_EPI16(step2[(8 - 4)], step2[3]);
                        step3[4] = SUB_EPI16(step2[(8 - 5)], step2[4]);
                        step3[5] = SUB_EPI16(step2[(8 - 6)], step2[5]);
                        step3[6] = SUB_EPI16(step2[(8 - 7)], step2[6]);
                        step3[7] = SUB_EPI16(step2[(8 - 8)], step2[7]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step3[0], &step3[1], &step3[2],
                            &step3[3], &step3[4], &step3[5],
                            &step3[6], &step3[7]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s3_10_0 = _mm_unpacklo_epi16(step2[13], step2[10]);
                        const __m128i s3_10_1 = _mm_unpackhi_epi16(step2[13], step2[10]);
                        const __m128i s3_11_0 = _mm_unpacklo_epi16(step2[12], step2[11]);
                        const __m128i s3_11_1 = _mm_unpackhi_epi16(step2[12], step2[11]);
                        const __m128i s3_10_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_m16);
                        const __m128i s3_10_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_m16);
                        const __m128i s3_11_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_m16);
                        const __m128i s3_11_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_m16);
                        const __m128i s3_12_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_p16);
                        const __m128i s3_12_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_p16);
                        const __m128i s3_13_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_p16);
                        const __m128i s3_13_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m128i s3_10_4 = _mm_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_10_5 = _mm_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_11_4 = _mm_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_11_5 = _mm_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_12_4 = _mm_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_12_5 = _mm_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_13_4 = _mm_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_13_5 = _mm_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_10_6 = _mm_srai_epi32(s3_10_4, DCT_CONST_BITS);
                        const __m128i s3_10_7 = _mm_srai_epi32(s3_10_5, DCT_CONST_BITS);
                        const __m128i s3_11_6 = _mm_srai_epi32(s3_11_4, DCT_CONST_BITS);
                        const __m128i s3_11_7 = _mm_srai_epi32(s3_11_5, DCT_CONST_BITS);
                        const __m128i s3_12_6 = _mm_srai_epi32(s3_12_4, DCT_CONST_BITS);
                        const __m128i s3_12_7 = _mm_srai_epi32(s3_12_5, DCT_CONST_BITS);
                        const __m128i s3_13_6 = _mm_srai_epi32(s3_13_4, DCT_CONST_BITS);
                        const __m128i s3_13_7 = _mm_srai_epi32(s3_13_5, DCT_CONST_BITS);
                        // Combine
                        step3[10] = _mm_packs_epi32(s3_10_6, s3_10_7);
                        step3[11] = _mm_packs_epi32(s3_11_6, s3_11_7);
                        step3[12] = _mm_packs_epi32(s3_12_6, s3_12_7);
                        step3[13] = _mm_packs_epi32(s3_13_6, s3_13_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&step3[10], &step3[11], &step3[12],
                            &step3[13]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step3[16] = ADD_EPI16(step2[23], step1[16]);
                        step3[17] = ADD_EPI16(step2[22], step1[17]);
                        step3[18] = ADD_EPI16(step2[21], step1[18]);
                        step3[19] = ADD_EPI16(step2[20], step1[19]);
                        step3[20] = SUB_EPI16(step1[19], step2[20]);
                        step3[21] = SUB_EPI16(step1[18], step2[21]);
                        step3[22] = SUB_EPI16(step1[17], step2[22]);
                        step3[23] = SUB_EPI16(step1[16], step2[23]);
                        step3[24] = SUB_EPI16(step1[31], step2[24]);
                        step3[25] = SUB_EPI16(step1[30], step2[25]);
                        step3[26] = SUB_EPI16(step1[29], step2[26]);
                        step3[27] = SUB_EPI16(step1[28], step2[27]);
                        step3[28] = ADD_EPI16(step2[27], step1[28]);
                        step3[29] = ADD_EPI16(step2[26], step1[29]);
                        step3[30] = ADD_EPI16(step2[25], step1[30]);
                        step3[31] = ADD_EPI16(step2[24], step1[31]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step3[16], &step3[17], &step3[18], &step3[19], &step3[20],
                            &step3[21], &step3[22], &step3[23], &step3[24], &step3[25],
                            &step3[26], &step3[27], &step3[28], &step3[29], &step3[30],
                            &step3[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }

                    // Stage 4
                    {
                        step1[0] = ADD_EPI16(step3[3], step3[0]);
                        step1[1] = ADD_EPI16(step3[2], step3[1]);
                        step1[2] = SUB_EPI16(step3[1], step3[2]);
                        step1[3] = SUB_EPI16(step3[0], step3[3]);
                        step1[8] = ADD_EPI16(step3[11], step2[8]);
                        step1[9] = ADD_EPI16(step3[10], step2[9]);
                        step1[10] = SUB_EPI16(step2[9], step3[10]);
                        step1[11] = SUB_EPI16(step2[8], step3[11]);
                        step1[12] = SUB_EPI16(step2[15], step3[12]);
                        step1[13] = SUB_EPI16(step2[14], step3[13]);
                        step1[14] = ADD_EPI16(step3[13], step2[14]);
                        step1[15] = ADD_EPI16(step3[12], step2[15]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step1[0], &step1[1], &step1[2], &step1[3], &step1[4], &step1[5],
                            &step1[6], &step1[7], &step1[8], &step1[9], &step1[10],
                            &step1[11], &step1[12], &step1[13], &step1[14], &step1[15]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s1_05_0 = _mm_unpacklo_epi16(step3[6], step3[5]);
                        const __m128i s1_05_1 = _mm_unpackhi_epi16(step3[6], step3[5]);
                        const __m128i s1_05_2 = _mm_madd_epi16(s1_05_0, k__cospi_p16_m16);
                        const __m128i s1_05_3 = _mm_madd_epi16(s1_05_1, k__cospi_p16_m16);
                        const __m128i s1_06_2 = _mm_madd_epi16(s1_05_0, k__cospi_p16_p16);
                        const __m128i s1_06_3 = _mm_madd_epi16(s1_05_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m128i s1_05_4 = _mm_add_epi32(s1_05_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_05_5 = _mm_add_epi32(s1_05_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_06_4 = _mm_add_epi32(s1_06_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_06_5 = _mm_add_epi32(s1_06_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_05_6 = _mm_srai_epi32(s1_05_4, DCT_CONST_BITS);
                        const __m128i s1_05_7 = _mm_srai_epi32(s1_05_5, DCT_CONST_BITS);
                        const __m128i s1_06_6 = _mm_srai_epi32(s1_06_4, DCT_CONST_BITS);
                        const __m128i s1_06_7 = _mm_srai_epi32(s1_06_5, DCT_CONST_BITS);
                        // Combine
                        step1[5] = _mm_packs_epi32(s1_05_6, s1_05_7);
                        step1[6] = _mm_packs_epi32(s1_06_6, s1_06_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x2(&step1[5], &step1[6]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s1_18_0 = _mm_unpacklo_epi16(step3[18], step3[29]);
                        const __m128i s1_18_1 = _mm_unpackhi_epi16(step3[18], step3[29]);
                        const __m128i s1_19_0 = _mm_unpacklo_epi16(step3[19], step3[28]);
                        const __m128i s1_19_1 = _mm_unpackhi_epi16(step3[19], step3[28]);
                        const __m128i s1_20_0 = _mm_unpacklo_epi16(step3[20], step3[27]);
                        const __m128i s1_20_1 = _mm_unpackhi_epi16(step3[20], step3[27]);
                        const __m128i s1_21_0 = _mm_unpacklo_epi16(step3[21], step3[26]);
                        const __m128i s1_21_1 = _mm_unpackhi_epi16(step3[21], step3[26]);
                        const __m128i s1_18_2 = _mm_madd_epi16(s1_18_0, k__cospi_m08_p24);
                        const __m128i s1_18_3 = _mm_madd_epi16(s1_18_1, k__cospi_m08_p24);
                        const __m128i s1_19_2 = _mm_madd_epi16(s1_19_0, k__cospi_m08_p24);
                        const __m128i s1_19_3 = _mm_madd_epi16(s1_19_1, k__cospi_m08_p24);
                        const __m128i s1_20_2 = _mm_madd_epi16(s1_20_0, k__cospi_m24_m08);
                        const __m128i s1_20_3 = _mm_madd_epi16(s1_20_1, k__cospi_m24_m08);
                        const __m128i s1_21_2 = _mm_madd_epi16(s1_21_0, k__cospi_m24_m08);
                        const __m128i s1_21_3 = _mm_madd_epi16(s1_21_1, k__cospi_m24_m08);
                        const __m128i s1_26_2 = _mm_madd_epi16(s1_21_0, k__cospi_m08_p24);
                        const __m128i s1_26_3 = _mm_madd_epi16(s1_21_1, k__cospi_m08_p24);
                        const __m128i s1_27_2 = _mm_madd_epi16(s1_20_0, k__cospi_m08_p24);
                        const __m128i s1_27_3 = _mm_madd_epi16(s1_20_1, k__cospi_m08_p24);
                        const __m128i s1_28_2 = _mm_madd_epi16(s1_19_0, k__cospi_p24_p08);
                        const __m128i s1_28_3 = _mm_madd_epi16(s1_19_1, k__cospi_p24_p08);
                        const __m128i s1_29_2 = _mm_madd_epi16(s1_18_0, k__cospi_p24_p08);
                        const __m128i s1_29_3 = _mm_madd_epi16(s1_18_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m128i s1_18_4 = _mm_add_epi32(s1_18_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_18_5 = _mm_add_epi32(s1_18_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_19_4 = _mm_add_epi32(s1_19_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_19_5 = _mm_add_epi32(s1_19_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_20_4 = _mm_add_epi32(s1_20_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_20_5 = _mm_add_epi32(s1_20_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_21_4 = _mm_add_epi32(s1_21_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_21_5 = _mm_add_epi32(s1_21_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_26_4 = _mm_add_epi32(s1_26_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_26_5 = _mm_add_epi32(s1_26_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_27_4 = _mm_add_epi32(s1_27_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_27_5 = _mm_add_epi32(s1_27_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_28_4 = _mm_add_epi32(s1_28_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_28_5 = _mm_add_epi32(s1_28_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_29_4 = _mm_add_epi32(s1_29_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_29_5 = _mm_add_epi32(s1_29_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_18_6 = _mm_srai_epi32(s1_18_4, DCT_CONST_BITS);
                        const __m128i s1_18_7 = _mm_srai_epi32(s1_18_5, DCT_CONST_BITS);
                        const __m128i s1_19_6 = _mm_srai_epi32(s1_19_4, DCT_CONST_BITS);
                        const __m128i s1_19_7 = _mm_srai_epi32(s1_19_5, DCT_CONST_BITS);
                        const __m128i s1_20_6 = _mm_srai_epi32(s1_20_4, DCT_CONST_BITS);
                        const __m128i s1_20_7 = _mm_srai_epi32(s1_20_5, DCT_CONST_BITS);
                        const __m128i s1_21_6 = _mm_srai_epi32(s1_21_4, DCT_CONST_BITS);
                        const __m128i s1_21_7 = _mm_srai_epi32(s1_21_5, DCT_CONST_BITS);
                        const __m128i s1_26_6 = _mm_srai_epi32(s1_26_4, DCT_CONST_BITS);
                        const __m128i s1_26_7 = _mm_srai_epi32(s1_26_5, DCT_CONST_BITS);
                        const __m128i s1_27_6 = _mm_srai_epi32(s1_27_4, DCT_CONST_BITS);
                        const __m128i s1_27_7 = _mm_srai_epi32(s1_27_5, DCT_CONST_BITS);
                        const __m128i s1_28_6 = _mm_srai_epi32(s1_28_4, DCT_CONST_BITS);
                        const __m128i s1_28_7 = _mm_srai_epi32(s1_28_5, DCT_CONST_BITS);
                        const __m128i s1_29_6 = _mm_srai_epi32(s1_29_4, DCT_CONST_BITS);
                        const __m128i s1_29_7 = _mm_srai_epi32(s1_29_5, DCT_CONST_BITS);
                        // Combine
                        step1[18] = _mm_packs_epi32(s1_18_6, s1_18_7);
                        step1[19] = _mm_packs_epi32(s1_19_6, s1_19_7);
                        step1[20] = _mm_packs_epi32(s1_20_6, s1_20_7);
                        step1[21] = _mm_packs_epi32(s1_21_6, s1_21_7);
                        step1[26] = _mm_packs_epi32(s1_26_6, s1_26_7);
                        step1[27] = _mm_packs_epi32(s1_27_6, s1_27_7);
                        step1[28] = _mm_packs_epi32(s1_28_6, s1_28_7);
                        step1[29] = _mm_packs_epi32(s1_29_6, s1_29_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[18], &step1[19], &step1[20],
                            &step1[21], &step1[26], &step1[27],
                            &step1[28], &step1[29]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Stage 5
                    {
                        step2[4] = ADD_EPI16(step1[5], step3[4]);
                        step2[5] = SUB_EPI16(step3[4], step1[5]);
                        step2[6] = SUB_EPI16(step3[7], step1[6]);
                        step2[7] = ADD_EPI16(step1[6], step3[7]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&step2[4], &step2[5], &step2[6],
                            &step2[7]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i out_00_0 = _mm_unpacklo_epi16(step1[0], step1[1]);
                        const __m128i out_00_1 = _mm_unpackhi_epi16(step1[0], step1[1]);
                        const __m128i out_08_0 = _mm_unpacklo_epi16(step1[2], step1[3]);
                        const __m128i out_08_1 = _mm_unpackhi_epi16(step1[2], step1[3]);
                        const __m128i out_00_2 = _mm_madd_epi16(out_00_0, k__cospi_p16_p16);
                        const __m128i out_00_3 = _mm_madd_epi16(out_00_1, k__cospi_p16_p16);
                        const __m128i out_16_2 = _mm_madd_epi16(out_00_0, k__cospi_p16_m16);
                        const __m128i out_16_3 = _mm_madd_epi16(out_00_1, k__cospi_p16_m16);
                        const __m128i out_08_2 = _mm_madd_epi16(out_08_0, k__cospi_p24_p08);
                        const __m128i out_08_3 = _mm_madd_epi16(out_08_1, k__cospi_p24_p08);
                        const __m128i out_24_2 = _mm_madd_epi16(out_08_0, k__cospi_m08_p24);
                        const __m128i out_24_3 = _mm_madd_epi16(out_08_1, k__cospi_m08_p24);
                        // dct_const_round_shift
                        const __m128i out_00_4 =
                            _mm_add_epi32(out_00_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_00_5 =
                            _mm_add_epi32(out_00_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_16_4 =
                            _mm_add_epi32(out_16_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_16_5 =
                            _mm_add_epi32(out_16_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_08_4 =
                            _mm_add_epi32(out_08_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_08_5 =
                            _mm_add_epi32(out_08_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_24_4 =
                            _mm_add_epi32(out_24_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_24_5 =
                            _mm_add_epi32(out_24_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_00_6 = _mm_srai_epi32(out_00_4, DCT_CONST_BITS);
                        const __m128i out_00_7 = _mm_srai_epi32(out_00_5, DCT_CONST_BITS);
                        const __m128i out_16_6 = _mm_srai_epi32(out_16_4, DCT_CONST_BITS);
                        const __m128i out_16_7 = _mm_srai_epi32(out_16_5, DCT_CONST_BITS);
                        const __m128i out_08_6 = _mm_srai_epi32(out_08_4, DCT_CONST_BITS);
                        const __m128i out_08_7 = _mm_srai_epi32(out_08_5, DCT_CONST_BITS);
                        const __m128i out_24_6 = _mm_srai_epi32(out_24_4, DCT_CONST_BITS);
                        const __m128i out_24_7 = _mm_srai_epi32(out_24_5, DCT_CONST_BITS);
                        // Combine
                        out[0] = _mm_packs_epi32(out_00_6, out_00_7);
                        out[16] = _mm_packs_epi32(out_16_6, out_16_7);
                        out[8] = _mm_packs_epi32(out_08_6, out_08_7);
                        out[24] = _mm_packs_epi32(out_24_6, out_24_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&out[0], &out[16], &out[8], &out[24]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s2_09_0 = _mm_unpacklo_epi16(step1[9], step1[14]);
                        const __m128i s2_09_1 = _mm_unpackhi_epi16(step1[9], step1[14]);
                        const __m128i s2_10_0 = _mm_unpacklo_epi16(step1[10], step1[13]);
                        const __m128i s2_10_1 = _mm_unpackhi_epi16(step1[10], step1[13]);
                        const __m128i s2_09_2 = _mm_madd_epi16(s2_09_0, k__cospi_m08_p24);
                        const __m128i s2_09_3 = _mm_madd_epi16(s2_09_1, k__cospi_m08_p24);
                        const __m128i s2_10_2 = _mm_madd_epi16(s2_10_0, k__cospi_m24_m08);
                        const __m128i s2_10_3 = _mm_madd_epi16(s2_10_1, k__cospi_m24_m08);
                        const __m128i s2_13_2 = _mm_madd_epi16(s2_10_0, k__cospi_m08_p24);
                        const __m128i s2_13_3 = _mm_madd_epi16(s2_10_1, k__cospi_m08_p24);
                        const __m128i s2_14_2 = _mm_madd_epi16(s2_09_0, k__cospi_p24_p08);
                        const __m128i s2_14_3 = _mm_madd_epi16(s2_09_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m128i s2_09_4 = _mm_add_epi32(s2_09_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_09_5 = _mm_add_epi32(s2_09_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_10_4 = _mm_add_epi32(s2_10_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_10_5 = _mm_add_epi32(s2_10_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_13_4 = _mm_add_epi32(s2_13_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_13_5 = _mm_add_epi32(s2_13_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_14_4 = _mm_add_epi32(s2_14_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_14_5 = _mm_add_epi32(s2_14_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_09_6 = _mm_srai_epi32(s2_09_4, DCT_CONST_BITS);
                        const __m128i s2_09_7 = _mm_srai_epi32(s2_09_5, DCT_CONST_BITS);
                        const __m128i s2_10_6 = _mm_srai_epi32(s2_10_4, DCT_CONST_BITS);
                        const __m128i s2_10_7 = _mm_srai_epi32(s2_10_5, DCT_CONST_BITS);
                        const __m128i s2_13_6 = _mm_srai_epi32(s2_13_4, DCT_CONST_BITS);
                        const __m128i s2_13_7 = _mm_srai_epi32(s2_13_5, DCT_CONST_BITS);
                        const __m128i s2_14_6 = _mm_srai_epi32(s2_14_4, DCT_CONST_BITS);
                        const __m128i s2_14_7 = _mm_srai_epi32(s2_14_5, DCT_CONST_BITS);
                        // Combine
                        step2[9] = _mm_packs_epi32(s2_09_6, s2_09_7);
                        step2[10] = _mm_packs_epi32(s2_10_6, s2_10_7);
                        step2[13] = _mm_packs_epi32(s2_13_6, s2_13_7);
                        step2[14] = _mm_packs_epi32(s2_14_6, s2_14_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&step2[9], &step2[10], &step2[13],
                            &step2[14]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step2[16] = ADD_EPI16(step1[19], step3[16]);
                        step2[17] = ADD_EPI16(step1[18], step3[17]);
                        step2[18] = SUB_EPI16(step3[17], step1[18]);
                        step2[19] = SUB_EPI16(step3[16], step1[19]);
                        step2[20] = SUB_EPI16(step3[23], step1[20]);
                        step2[21] = SUB_EPI16(step3[22], step1[21]);
                        step2[22] = ADD_EPI16(step1[21], step3[22]);
                        step2[23] = ADD_EPI16(step1[20], step3[23]);
                        step2[24] = ADD_EPI16(step1[27], step3[24]);
                        step2[25] = ADD_EPI16(step1[26], step3[25]);
                        step2[26] = SUB_EPI16(step3[25], step1[26]);
                        step2[27] = SUB_EPI16(step3[24], step1[27]);
                        step2[28] = SUB_EPI16(step3[31], step1[28]);
                        step2[29] = SUB_EPI16(step3[30], step1[29]);
                        step2[30] = ADD_EPI16(step1[29], step3[30]);
                        step2[31] = ADD_EPI16(step1[28], step3[31]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step2[16], &step2[17], &step2[18], &step2[19], &step2[20],
                            &step2[21], &step2[22], &step2[23], &step2[24], &step2[25],
                            &step2[26], &step2[27], &step2[28], &step2[29], &step2[30],
                            &step2[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Stage 6
                    {
                        const __m128i out_04_0 = _mm_unpacklo_epi16(step2[4], step2[7]);
                        const __m128i out_04_1 = _mm_unpackhi_epi16(step2[4], step2[7]);
                        const __m128i out_20_0 = _mm_unpacklo_epi16(step2[5], step2[6]);
                        const __m128i out_20_1 = _mm_unpackhi_epi16(step2[5], step2[6]);
                        const __m128i out_12_0 = _mm_unpacklo_epi16(step2[5], step2[6]);
                        const __m128i out_12_1 = _mm_unpackhi_epi16(step2[5], step2[6]);
                        const __m128i out_28_0 = _mm_unpacklo_epi16(step2[4], step2[7]);
                        const __m128i out_28_1 = _mm_unpackhi_epi16(step2[4], step2[7]);
                        const __m128i out_04_2 = _mm_madd_epi16(out_04_0, k__cospi_p28_p04);
                        const __m128i out_04_3 = _mm_madd_epi16(out_04_1, k__cospi_p28_p04);
                        const __m128i out_20_2 = _mm_madd_epi16(out_20_0, k__cospi_p12_p20);
                        const __m128i out_20_3 = _mm_madd_epi16(out_20_1, k__cospi_p12_p20);
                        const __m128i out_12_2 = _mm_madd_epi16(out_12_0, k__cospi_m20_p12);
                        const __m128i out_12_3 = _mm_madd_epi16(out_12_1, k__cospi_m20_p12);
                        const __m128i out_28_2 = _mm_madd_epi16(out_28_0, k__cospi_m04_p28);
                        const __m128i out_28_3 = _mm_madd_epi16(out_28_1, k__cospi_m04_p28);
                        // dct_const_round_shift
                        const __m128i out_04_4 =
                            _mm_add_epi32(out_04_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_04_5 =
                            _mm_add_epi32(out_04_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_20_4 =
                            _mm_add_epi32(out_20_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_20_5 =
                            _mm_add_epi32(out_20_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_12_4 =
                            _mm_add_epi32(out_12_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_12_5 =
                            _mm_add_epi32(out_12_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_28_4 =
                            _mm_add_epi32(out_28_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_28_5 =
                            _mm_add_epi32(out_28_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_04_6 = _mm_srai_epi32(out_04_4, DCT_CONST_BITS);
                        const __m128i out_04_7 = _mm_srai_epi32(out_04_5, DCT_CONST_BITS);
                        const __m128i out_20_6 = _mm_srai_epi32(out_20_4, DCT_CONST_BITS);
                        const __m128i out_20_7 = _mm_srai_epi32(out_20_5, DCT_CONST_BITS);
                        const __m128i out_12_6 = _mm_srai_epi32(out_12_4, DCT_CONST_BITS);
                        const __m128i out_12_7 = _mm_srai_epi32(out_12_5, DCT_CONST_BITS);
                        const __m128i out_28_6 = _mm_srai_epi32(out_28_4, DCT_CONST_BITS);
                        const __m128i out_28_7 = _mm_srai_epi32(out_28_5, DCT_CONST_BITS);
                        // Combine
                        out[4] = _mm_packs_epi32(out_04_6, out_04_7);
                        out[20] = _mm_packs_epi32(out_20_6, out_20_7);
                        out[12] = _mm_packs_epi32(out_12_6, out_12_7);
                        out[28] = _mm_packs_epi32(out_28_6, out_28_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&out[4], &out[20], &out[12], &out[28]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step3[8] = ADD_EPI16(step2[9], step1[8]);
                        step3[9] = SUB_EPI16(step1[8], step2[9]);
                        step3[10] = SUB_EPI16(step1[11], step2[10]);
                        step3[11] = ADD_EPI16(step2[10], step1[11]);
                        step3[12] = ADD_EPI16(step2[13], step1[12]);
                        step3[13] = SUB_EPI16(step1[12], step2[13]);
                        step3[14] = SUB_EPI16(step1[15], step2[14]);
                        step3[15] = ADD_EPI16(step2[14], step1[15]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step3[8], &step3[9], &step3[10],
                            &step3[11], &step3[12], &step3[13],
                            &step3[14], &step3[15]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s3_17_0 = _mm_unpacklo_epi16(step2[17], step2[30]);
                        const __m128i s3_17_1 = _mm_unpackhi_epi16(step2[17], step2[30]);
                        const __m128i s3_18_0 = _mm_unpacklo_epi16(step2[18], step2[29]);
                        const __m128i s3_18_1 = _mm_unpackhi_epi16(step2[18], step2[29]);
                        const __m128i s3_21_0 = _mm_unpacklo_epi16(step2[21], step2[26]);
                        const __m128i s3_21_1 = _mm_unpackhi_epi16(step2[21], step2[26]);
                        const __m128i s3_22_0 = _mm_unpacklo_epi16(step2[22], step2[25]);
                        const __m128i s3_22_1 = _mm_unpackhi_epi16(step2[22], step2[25]);
                        const __m128i s3_17_2 = _mm_madd_epi16(s3_17_0, k__cospi_m04_p28);
                        const __m128i s3_17_3 = _mm_madd_epi16(s3_17_1, k__cospi_m04_p28);
                        const __m128i s3_18_2 = _mm_madd_epi16(s3_18_0, k__cospi_m28_m04);
                        const __m128i s3_18_3 = _mm_madd_epi16(s3_18_1, k__cospi_m28_m04);
                        const __m128i s3_21_2 = _mm_madd_epi16(s3_21_0, k__cospi_m20_p12);
                        const __m128i s3_21_3 = _mm_madd_epi16(s3_21_1, k__cospi_m20_p12);
                        const __m128i s3_22_2 = _mm_madd_epi16(s3_22_0, k__cospi_m12_m20);
                        const __m128i s3_22_3 = _mm_madd_epi16(s3_22_1, k__cospi_m12_m20);
                        const __m128i s3_25_2 = _mm_madd_epi16(s3_22_0, k__cospi_m20_p12);
                        const __m128i s3_25_3 = _mm_madd_epi16(s3_22_1, k__cospi_m20_p12);
                        const __m128i s3_26_2 = _mm_madd_epi16(s3_21_0, k__cospi_p12_p20);
                        const __m128i s3_26_3 = _mm_madd_epi16(s3_21_1, k__cospi_p12_p20);
                        const __m128i s3_29_2 = _mm_madd_epi16(s3_18_0, k__cospi_m04_p28);
                        const __m128i s3_29_3 = _mm_madd_epi16(s3_18_1, k__cospi_m04_p28);
                        const __m128i s3_30_2 = _mm_madd_epi16(s3_17_0, k__cospi_p28_p04);
                        const __m128i s3_30_3 = _mm_madd_epi16(s3_17_1, k__cospi_p28_p04);
                        // dct_const_round_shift
                        const __m128i s3_17_4 = _mm_add_epi32(s3_17_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_17_5 = _mm_add_epi32(s3_17_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_18_4 = _mm_add_epi32(s3_18_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_18_5 = _mm_add_epi32(s3_18_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_21_4 = _mm_add_epi32(s3_21_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_21_5 = _mm_add_epi32(s3_21_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_22_4 = _mm_add_epi32(s3_22_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_22_5 = _mm_add_epi32(s3_22_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_17_6 = _mm_srai_epi32(s3_17_4, DCT_CONST_BITS);
                        const __m128i s3_17_7 = _mm_srai_epi32(s3_17_5, DCT_CONST_BITS);
                        const __m128i s3_18_6 = _mm_srai_epi32(s3_18_4, DCT_CONST_BITS);
                        const __m128i s3_18_7 = _mm_srai_epi32(s3_18_5, DCT_CONST_BITS);
                        const __m128i s3_21_6 = _mm_srai_epi32(s3_21_4, DCT_CONST_BITS);
                        const __m128i s3_21_7 = _mm_srai_epi32(s3_21_5, DCT_CONST_BITS);
                        const __m128i s3_22_6 = _mm_srai_epi32(s3_22_4, DCT_CONST_BITS);
                        const __m128i s3_22_7 = _mm_srai_epi32(s3_22_5, DCT_CONST_BITS);
                        const __m128i s3_25_4 = _mm_add_epi32(s3_25_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_25_5 = _mm_add_epi32(s3_25_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_26_4 = _mm_add_epi32(s3_26_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_26_5 = _mm_add_epi32(s3_26_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_29_4 = _mm_add_epi32(s3_29_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_29_5 = _mm_add_epi32(s3_29_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_30_4 = _mm_add_epi32(s3_30_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_30_5 = _mm_add_epi32(s3_30_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_25_6 = _mm_srai_epi32(s3_25_4, DCT_CONST_BITS);
                        const __m128i s3_25_7 = _mm_srai_epi32(s3_25_5, DCT_CONST_BITS);
                        const __m128i s3_26_6 = _mm_srai_epi32(s3_26_4, DCT_CONST_BITS);
                        const __m128i s3_26_7 = _mm_srai_epi32(s3_26_5, DCT_CONST_BITS);
                        const __m128i s3_29_6 = _mm_srai_epi32(s3_29_4, DCT_CONST_BITS);
                        const __m128i s3_29_7 = _mm_srai_epi32(s3_29_5, DCT_CONST_BITS);
                        const __m128i s3_30_6 = _mm_srai_epi32(s3_30_4, DCT_CONST_BITS);
                        const __m128i s3_30_7 = _mm_srai_epi32(s3_30_5, DCT_CONST_BITS);
                        // Combine
                        step3[17] = _mm_packs_epi32(s3_17_6, s3_17_7);
                        step3[18] = _mm_packs_epi32(s3_18_6, s3_18_7);
                        step3[21] = _mm_packs_epi32(s3_21_6, s3_21_7);
                        step3[22] = _mm_packs_epi32(s3_22_6, s3_22_7);
                        // Combine
                        step3[25] = _mm_packs_epi32(s3_25_6, s3_25_7);
                        step3[26] = _mm_packs_epi32(s3_26_6, s3_26_7);
                        step3[29] = _mm_packs_epi32(s3_29_6, s3_29_7);
                        step3[30] = _mm_packs_epi32(s3_30_6, s3_30_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step3[17], &step3[18], &step3[21],
                            &step3[22], &step3[25], &step3[26],
                            &step3[29], &step3[30]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Stage 7
                    {
                        const __m128i out_02_0 = _mm_unpacklo_epi16(step3[8], step3[15]);
                        const __m128i out_02_1 = _mm_unpackhi_epi16(step3[8], step3[15]);
                        const __m128i out_18_0 = _mm_unpacklo_epi16(step3[9], step3[14]);
                        const __m128i out_18_1 = _mm_unpackhi_epi16(step3[9], step3[14]);
                        const __m128i out_10_0 = _mm_unpacklo_epi16(step3[10], step3[13]);
                        const __m128i out_10_1 = _mm_unpackhi_epi16(step3[10], step3[13]);
                        const __m128i out_26_0 = _mm_unpacklo_epi16(step3[11], step3[12]);
                        const __m128i out_26_1 = _mm_unpackhi_epi16(step3[11], step3[12]);
                        const __m128i out_02_2 = _mm_madd_epi16(out_02_0, k__cospi_p30_p02);
                        const __m128i out_02_3 = _mm_madd_epi16(out_02_1, k__cospi_p30_p02);
                        const __m128i out_18_2 = _mm_madd_epi16(out_18_0, k__cospi_p14_p18);
                        const __m128i out_18_3 = _mm_madd_epi16(out_18_1, k__cospi_p14_p18);
                        const __m128i out_10_2 = _mm_madd_epi16(out_10_0, k__cospi_p22_p10);
                        const __m128i out_10_3 = _mm_madd_epi16(out_10_1, k__cospi_p22_p10);
                        const __m128i out_26_2 = _mm_madd_epi16(out_26_0, k__cospi_p06_p26);
                        const __m128i out_26_3 = _mm_madd_epi16(out_26_1, k__cospi_p06_p26);
                        const __m128i out_06_2 = _mm_madd_epi16(out_26_0, k__cospi_m26_p06);
                        const __m128i out_06_3 = _mm_madd_epi16(out_26_1, k__cospi_m26_p06);
                        const __m128i out_22_2 = _mm_madd_epi16(out_10_0, k__cospi_m10_p22);
                        const __m128i out_22_3 = _mm_madd_epi16(out_10_1, k__cospi_m10_p22);
                        const __m128i out_14_2 = _mm_madd_epi16(out_18_0, k__cospi_m18_p14);
                        const __m128i out_14_3 = _mm_madd_epi16(out_18_1, k__cospi_m18_p14);
                        const __m128i out_30_2 = _mm_madd_epi16(out_02_0, k__cospi_m02_p30);
                        const __m128i out_30_3 = _mm_madd_epi16(out_02_1, k__cospi_m02_p30);
                        // dct_const_round_shift
                        const __m128i out_02_4 =
                            _mm_add_epi32(out_02_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_02_5 =
                            _mm_add_epi32(out_02_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_18_4 =
                            _mm_add_epi32(out_18_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_18_5 =
                            _mm_add_epi32(out_18_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_10_4 =
                            _mm_add_epi32(out_10_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_10_5 =
                            _mm_add_epi32(out_10_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_26_4 =
                            _mm_add_epi32(out_26_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_26_5 =
                            _mm_add_epi32(out_26_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_06_4 =
                            _mm_add_epi32(out_06_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_06_5 =
                            _mm_add_epi32(out_06_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_22_4 =
                            _mm_add_epi32(out_22_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_22_5 =
                            _mm_add_epi32(out_22_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_14_4 =
                            _mm_add_epi32(out_14_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_14_5 =
                            _mm_add_epi32(out_14_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_30_4 =
                            _mm_add_epi32(out_30_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_30_5 =
                            _mm_add_epi32(out_30_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_02_6 = _mm_srai_epi32(out_02_4, DCT_CONST_BITS);
                        const __m128i out_02_7 = _mm_srai_epi32(out_02_5, DCT_CONST_BITS);
                        const __m128i out_18_6 = _mm_srai_epi32(out_18_4, DCT_CONST_BITS);
                        const __m128i out_18_7 = _mm_srai_epi32(out_18_5, DCT_CONST_BITS);
                        const __m128i out_10_6 = _mm_srai_epi32(out_10_4, DCT_CONST_BITS);
                        const __m128i out_10_7 = _mm_srai_epi32(out_10_5, DCT_CONST_BITS);
                        const __m128i out_26_6 = _mm_srai_epi32(out_26_4, DCT_CONST_BITS);
                        const __m128i out_26_7 = _mm_srai_epi32(out_26_5, DCT_CONST_BITS);
                        const __m128i out_06_6 = _mm_srai_epi32(out_06_4, DCT_CONST_BITS);
                        const __m128i out_06_7 = _mm_srai_epi32(out_06_5, DCT_CONST_BITS);
                        const __m128i out_22_6 = _mm_srai_epi32(out_22_4, DCT_CONST_BITS);
                        const __m128i out_22_7 = _mm_srai_epi32(out_22_5, DCT_CONST_BITS);
                        const __m128i out_14_6 = _mm_srai_epi32(out_14_4, DCT_CONST_BITS);
                        const __m128i out_14_7 = _mm_srai_epi32(out_14_5, DCT_CONST_BITS);
                        const __m128i out_30_6 = _mm_srai_epi32(out_30_4, DCT_CONST_BITS);
                        const __m128i out_30_7 = _mm_srai_epi32(out_30_5, DCT_CONST_BITS);
                        // Combine
                        out[2] = _mm_packs_epi32(out_02_6, out_02_7);
                        out[18] = _mm_packs_epi32(out_18_6, out_18_7);
                        out[10] = _mm_packs_epi32(out_10_6, out_10_7);
                        out[26] = _mm_packs_epi32(out_26_6, out_26_7);
                        out[6] = _mm_packs_epi32(out_06_6, out_06_7);
                        out[22] = _mm_packs_epi32(out_22_6, out_22_7);
                        out[14] = _mm_packs_epi32(out_14_6, out_14_7);
                        out[30] = _mm_packs_epi32(out_30_6, out_30_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[2], &out[18], &out[10], &out[26],
                            &out[6], &out[22], &out[14], &out[30]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step1[16] = ADD_EPI16(step3[17], step2[16]);
                        step1[17] = SUB_EPI16(step2[16], step3[17]);
                        step1[18] = SUB_EPI16(step2[19], step3[18]);
                        step1[19] = ADD_EPI16(step3[18], step2[19]);
                        step1[20] = ADD_EPI16(step3[21], step2[20]);
                        step1[21] = SUB_EPI16(step2[20], step3[21]);
                        step1[22] = SUB_EPI16(step2[23], step3[22]);
                        step1[23] = ADD_EPI16(step3[22], step2[23]);
                        step1[24] = ADD_EPI16(step3[25], step2[24]);
                        step1[25] = SUB_EPI16(step2[24], step3[25]);
                        step1[26] = SUB_EPI16(step2[27], step3[26]);
                        step1[27] = ADD_EPI16(step3[26], step2[27]);
                        step1[28] = ADD_EPI16(step3[29], step2[28]);
                        step1[29] = SUB_EPI16(step2[28], step3[29]);
                        step1[30] = SUB_EPI16(step2[31], step3[30]);
                        step1[31] = ADD_EPI16(step3[30], step2[31]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step1[16], &step1[17], &step1[18], &step1[19], &step1[20],
                            &step1[21], &step1[22], &step1[23], &step1[24], &step1[25],
                            &step1[26], &step1[27], &step1[28], &step1[29], &step1[30],
                            &step1[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Final stage --- outputs indices are bit-reversed.
                    {
                        const __m128i out_01_0 = _mm_unpacklo_epi16(step1[16], step1[31]);
                        const __m128i out_01_1 = _mm_unpackhi_epi16(step1[16], step1[31]);
                        const __m128i out_17_0 = _mm_unpacklo_epi16(step1[17], step1[30]);
                        const __m128i out_17_1 = _mm_unpackhi_epi16(step1[17], step1[30]);
                        const __m128i out_09_0 = _mm_unpacklo_epi16(step1[18], step1[29]);
                        const __m128i out_09_1 = _mm_unpackhi_epi16(step1[18], step1[29]);
                        const __m128i out_25_0 = _mm_unpacklo_epi16(step1[19], step1[28]);
                        const __m128i out_25_1 = _mm_unpackhi_epi16(step1[19], step1[28]);
                        const __m128i out_01_2 = _mm_madd_epi16(out_01_0, k__cospi_p31_p01);
                        const __m128i out_01_3 = _mm_madd_epi16(out_01_1, k__cospi_p31_p01);
                        const __m128i out_17_2 = _mm_madd_epi16(out_17_0, k__cospi_p15_p17);
                        const __m128i out_17_3 = _mm_madd_epi16(out_17_1, k__cospi_p15_p17);
                        const __m128i out_09_2 = _mm_madd_epi16(out_09_0, k__cospi_p23_p09);
                        const __m128i out_09_3 = _mm_madd_epi16(out_09_1, k__cospi_p23_p09);
                        const __m128i out_25_2 = _mm_madd_epi16(out_25_0, k__cospi_p07_p25);
                        const __m128i out_25_3 = _mm_madd_epi16(out_25_1, k__cospi_p07_p25);
                        const __m128i out_07_2 = _mm_madd_epi16(out_25_0, k__cospi_m25_p07);
                        const __m128i out_07_3 = _mm_madd_epi16(out_25_1, k__cospi_m25_p07);
                        const __m128i out_23_2 = _mm_madd_epi16(out_09_0, k__cospi_m09_p23);
                        const __m128i out_23_3 = _mm_madd_epi16(out_09_1, k__cospi_m09_p23);
                        const __m128i out_15_2 = _mm_madd_epi16(out_17_0, k__cospi_m17_p15);
                        const __m128i out_15_3 = _mm_madd_epi16(out_17_1, k__cospi_m17_p15);
                        const __m128i out_31_2 = _mm_madd_epi16(out_01_0, k__cospi_m01_p31);
                        const __m128i out_31_3 = _mm_madd_epi16(out_01_1, k__cospi_m01_p31);
                        // dct_const_round_shift
                        const __m128i out_01_4 =
                            _mm_add_epi32(out_01_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_01_5 =
                            _mm_add_epi32(out_01_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_17_4 =
                            _mm_add_epi32(out_17_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_17_5 =
                            _mm_add_epi32(out_17_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_09_4 =
                            _mm_add_epi32(out_09_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_09_5 =
                            _mm_add_epi32(out_09_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_25_4 =
                            _mm_add_epi32(out_25_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_25_5 =
                            _mm_add_epi32(out_25_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_07_4 =
                            _mm_add_epi32(out_07_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_07_5 =
                            _mm_add_epi32(out_07_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_23_4 =
                            _mm_add_epi32(out_23_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_23_5 =
                            _mm_add_epi32(out_23_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_15_4 =
                            _mm_add_epi32(out_15_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_15_5 =
                            _mm_add_epi32(out_15_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_31_4 =
                            _mm_add_epi32(out_31_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_31_5 =
                            _mm_add_epi32(out_31_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_01_6 = _mm_srai_epi32(out_01_4, DCT_CONST_BITS);
                        const __m128i out_01_7 = _mm_srai_epi32(out_01_5, DCT_CONST_BITS);
                        const __m128i out_17_6 = _mm_srai_epi32(out_17_4, DCT_CONST_BITS);
                        const __m128i out_17_7 = _mm_srai_epi32(out_17_5, DCT_CONST_BITS);
                        const __m128i out_09_6 = _mm_srai_epi32(out_09_4, DCT_CONST_BITS);
                        const __m128i out_09_7 = _mm_srai_epi32(out_09_5, DCT_CONST_BITS);
                        const __m128i out_25_6 = _mm_srai_epi32(out_25_4, DCT_CONST_BITS);
                        const __m128i out_25_7 = _mm_srai_epi32(out_25_5, DCT_CONST_BITS);
                        const __m128i out_07_6 = _mm_srai_epi32(out_07_4, DCT_CONST_BITS);
                        const __m128i out_07_7 = _mm_srai_epi32(out_07_5, DCT_CONST_BITS);
                        const __m128i out_23_6 = _mm_srai_epi32(out_23_4, DCT_CONST_BITS);
                        const __m128i out_23_7 = _mm_srai_epi32(out_23_5, DCT_CONST_BITS);
                        const __m128i out_15_6 = _mm_srai_epi32(out_15_4, DCT_CONST_BITS);
                        const __m128i out_15_7 = _mm_srai_epi32(out_15_5, DCT_CONST_BITS);
                        const __m128i out_31_6 = _mm_srai_epi32(out_31_4, DCT_CONST_BITS);
                        const __m128i out_31_7 = _mm_srai_epi32(out_31_5, DCT_CONST_BITS);
                        // Combine
                        out[1] = _mm_packs_epi32(out_01_6, out_01_7);
                        out[17] = _mm_packs_epi32(out_17_6, out_17_7);
                        out[9] = _mm_packs_epi32(out_09_6, out_09_7);
                        out[25] = _mm_packs_epi32(out_25_6, out_25_7);
                        out[7] = _mm_packs_epi32(out_07_6, out_07_7);
                        out[23] = _mm_packs_epi32(out_23_6, out_23_7);
                        out[15] = _mm_packs_epi32(out_15_6, out_15_7);
                        out[31] = _mm_packs_epi32(out_31_6, out_31_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[1], &out[17], &out[9], &out[25],
                            &out[7], &out[23], &out[15], &out[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i out_05_0 = _mm_unpacklo_epi16(step1[20], step1[27]);
                        const __m128i out_05_1 = _mm_unpackhi_epi16(step1[20], step1[27]);
                        const __m128i out_21_0 = _mm_unpacklo_epi16(step1[21], step1[26]);
                        const __m128i out_21_1 = _mm_unpackhi_epi16(step1[21], step1[26]);
                        const __m128i out_13_0 = _mm_unpacklo_epi16(step1[22], step1[25]);
                        const __m128i out_13_1 = _mm_unpackhi_epi16(step1[22], step1[25]);
                        const __m128i out_29_0 = _mm_unpacklo_epi16(step1[23], step1[24]);
                        const __m128i out_29_1 = _mm_unpackhi_epi16(step1[23], step1[24]);
                        const __m128i out_05_2 = _mm_madd_epi16(out_05_0, k__cospi_p27_p05);
                        const __m128i out_05_3 = _mm_madd_epi16(out_05_1, k__cospi_p27_p05);
                        const __m128i out_21_2 = _mm_madd_epi16(out_21_0, k__cospi_p11_p21);
                        const __m128i out_21_3 = _mm_madd_epi16(out_21_1, k__cospi_p11_p21);
                        const __m128i out_13_2 = _mm_madd_epi16(out_13_0, k__cospi_p19_p13);
                        const __m128i out_13_3 = _mm_madd_epi16(out_13_1, k__cospi_p19_p13);
                        const __m128i out_29_2 = _mm_madd_epi16(out_29_0, k__cospi_p03_p29);
                        const __m128i out_29_3 = _mm_madd_epi16(out_29_1, k__cospi_p03_p29);
                        const __m128i out_03_2 = _mm_madd_epi16(out_29_0, k__cospi_m29_p03);
                        const __m128i out_03_3 = _mm_madd_epi16(out_29_1, k__cospi_m29_p03);
                        const __m128i out_19_2 = _mm_madd_epi16(out_13_0, k__cospi_m13_p19);
                        const __m128i out_19_3 = _mm_madd_epi16(out_13_1, k__cospi_m13_p19);
                        const __m128i out_11_2 = _mm_madd_epi16(out_21_0, k__cospi_m21_p11);
                        const __m128i out_11_3 = _mm_madd_epi16(out_21_1, k__cospi_m21_p11);
                        const __m128i out_27_2 = _mm_madd_epi16(out_05_0, k__cospi_m05_p27);
                        const __m128i out_27_3 = _mm_madd_epi16(out_05_1, k__cospi_m05_p27);
                        // dct_const_round_shift
                        const __m128i out_05_4 =
                            _mm_add_epi32(out_05_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_05_5 =
                            _mm_add_epi32(out_05_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_21_4 =
                            _mm_add_epi32(out_21_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_21_5 =
                            _mm_add_epi32(out_21_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_13_4 =
                            _mm_add_epi32(out_13_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_13_5 =
                            _mm_add_epi32(out_13_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_29_4 =
                            _mm_add_epi32(out_29_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_29_5 =
                            _mm_add_epi32(out_29_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_03_4 =
                            _mm_add_epi32(out_03_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_03_5 =
                            _mm_add_epi32(out_03_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_19_4 =
                            _mm_add_epi32(out_19_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_19_5 =
                            _mm_add_epi32(out_19_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_11_4 =
                            _mm_add_epi32(out_11_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_11_5 =
                            _mm_add_epi32(out_11_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_27_4 =
                            _mm_add_epi32(out_27_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_27_5 =
                            _mm_add_epi32(out_27_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_05_6 = _mm_srai_epi32(out_05_4, DCT_CONST_BITS);
                        const __m128i out_05_7 = _mm_srai_epi32(out_05_5, DCT_CONST_BITS);
                        const __m128i out_21_6 = _mm_srai_epi32(out_21_4, DCT_CONST_BITS);
                        const __m128i out_21_7 = _mm_srai_epi32(out_21_5, DCT_CONST_BITS);
                        const __m128i out_13_6 = _mm_srai_epi32(out_13_4, DCT_CONST_BITS);
                        const __m128i out_13_7 = _mm_srai_epi32(out_13_5, DCT_CONST_BITS);
                        const __m128i out_29_6 = _mm_srai_epi32(out_29_4, DCT_CONST_BITS);
                        const __m128i out_29_7 = _mm_srai_epi32(out_29_5, DCT_CONST_BITS);
                        const __m128i out_03_6 = _mm_srai_epi32(out_03_4, DCT_CONST_BITS);
                        const __m128i out_03_7 = _mm_srai_epi32(out_03_5, DCT_CONST_BITS);
                        const __m128i out_19_6 = _mm_srai_epi32(out_19_4, DCT_CONST_BITS);
                        const __m128i out_19_7 = _mm_srai_epi32(out_19_5, DCT_CONST_BITS);
                        const __m128i out_11_6 = _mm_srai_epi32(out_11_4, DCT_CONST_BITS);
                        const __m128i out_11_7 = _mm_srai_epi32(out_11_5, DCT_CONST_BITS);
                        const __m128i out_27_6 = _mm_srai_epi32(out_27_4, DCT_CONST_BITS);
                        const __m128i out_27_7 = _mm_srai_epi32(out_27_5, DCT_CONST_BITS);
                        // Combine
                        out[5] = _mm_packs_epi32(out_05_6, out_05_7);
                        out[21] = _mm_packs_epi32(out_21_6, out_21_7);
                        out[13] = _mm_packs_epi32(out_13_6, out_13_7);
                        out[29] = _mm_packs_epi32(out_29_6, out_29_7);
                        out[3] = _mm_packs_epi32(out_03_6, out_03_7);
                        out[19] = _mm_packs_epi32(out_19_6, out_19_7);
                        out[11] = _mm_packs_epi32(out_11_6, out_11_7);
                        out[27] = _mm_packs_epi32(out_27_6, out_27_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[5], &out[21], &out[13], &out[29],
                            &out[3], &out[19], &out[11], &out[27]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
#if FDCT32x32_HIGH_PRECISION
                } else {
                    __m128i lstep1[64], lstep2[64], lstep3[64];
                    __m128i u[32], v[32], sign[16];
                    const __m128i K32One = _mm_set_epi32(1, 1, 1, 1);
                    // start using 32-bit operations
                    // stage 3
                    {
                        // expanding to 32-bit length priori to addition operations
                        lstep2[0] = _mm_unpacklo_epi16(step2[0], kZero);
                        lstep2[1] = _mm_unpackhi_epi16(step2[0], kZero);
                        lstep2[2] = _mm_unpacklo_epi16(step2[1], kZero);
                        lstep2[3] = _mm_unpackhi_epi16(step2[1], kZero);
                        lstep2[4] = _mm_unpacklo_epi16(step2[2], kZero);
                        lstep2[5] = _mm_unpackhi_epi16(step2[2], kZero);
                        lstep2[6] = _mm_unpacklo_epi16(step2[3], kZero);
                        lstep2[7] = _mm_unpackhi_epi16(step2[3], kZero);
                        lstep2[8] = _mm_unpacklo_epi16(step2[4], kZero);
                        lstep2[9] = _mm_unpackhi_epi16(step2[4], kZero);
                        lstep2[10] = _mm_unpacklo_epi16(step2[5], kZero);
                        lstep2[11] = _mm_unpackhi_epi16(step2[5], kZero);
                        lstep2[12] = _mm_unpacklo_epi16(step2[6], kZero);
                        lstep2[13] = _mm_unpackhi_epi16(step2[6], kZero);
                        lstep2[14] = _mm_unpacklo_epi16(step2[7], kZero);
                        lstep2[15] = _mm_unpackhi_epi16(step2[7], kZero);
                        lstep2[0] = _mm_madd_epi16(lstep2[0], kOne);
                        lstep2[1] = _mm_madd_epi16(lstep2[1], kOne);
                        lstep2[2] = _mm_madd_epi16(lstep2[2], kOne);
                        lstep2[3] = _mm_madd_epi16(lstep2[3], kOne);
                        lstep2[4] = _mm_madd_epi16(lstep2[4], kOne);
                        lstep2[5] = _mm_madd_epi16(lstep2[5], kOne);
                        lstep2[6] = _mm_madd_epi16(lstep2[6], kOne);
                        lstep2[7] = _mm_madd_epi16(lstep2[7], kOne);
                        lstep2[8] = _mm_madd_epi16(lstep2[8], kOne);
                        lstep2[9] = _mm_madd_epi16(lstep2[9], kOne);
                        lstep2[10] = _mm_madd_epi16(lstep2[10], kOne);
                        lstep2[11] = _mm_madd_epi16(lstep2[11], kOne);
                        lstep2[12] = _mm_madd_epi16(lstep2[12], kOne);
                        lstep2[13] = _mm_madd_epi16(lstep2[13], kOne);
                        lstep2[14] = _mm_madd_epi16(lstep2[14], kOne);
                        lstep2[15] = _mm_madd_epi16(lstep2[15], kOne);

                        lstep3[0] = _mm_add_epi32(lstep2[14], lstep2[0]);
                        lstep3[1] = _mm_add_epi32(lstep2[15], lstep2[1]);
                        lstep3[2] = _mm_add_epi32(lstep2[12], lstep2[2]);
                        lstep3[3] = _mm_add_epi32(lstep2[13], lstep2[3]);
                        lstep3[4] = _mm_add_epi32(lstep2[10], lstep2[4]);
                        lstep3[5] = _mm_add_epi32(lstep2[11], lstep2[5]);
                        lstep3[6] = _mm_add_epi32(lstep2[8], lstep2[6]);
                        lstep3[7] = _mm_add_epi32(lstep2[9], lstep2[7]);
                        lstep3[8] = _mm_sub_epi32(lstep2[6], lstep2[8]);
                        lstep3[9] = _mm_sub_epi32(lstep2[7], lstep2[9]);
                        lstep3[10] = _mm_sub_epi32(lstep2[4], lstep2[10]);
                        lstep3[11] = _mm_sub_epi32(lstep2[5], lstep2[11]);
                        lstep3[12] = _mm_sub_epi32(lstep2[2], lstep2[12]);
                        lstep3[13] = _mm_sub_epi32(lstep2[3], lstep2[13]);
                        lstep3[14] = _mm_sub_epi32(lstep2[0], lstep2[14]);
                        lstep3[15] = _mm_sub_epi32(lstep2[1], lstep2[15]);
                    }
                    {
                        const __m128i s3_10_0 = _mm_unpacklo_epi16(step2[13], step2[10]);
                        const __m128i s3_10_1 = _mm_unpackhi_epi16(step2[13], step2[10]);
                        const __m128i s3_11_0 = _mm_unpacklo_epi16(step2[12], step2[11]);
                        const __m128i s3_11_1 = _mm_unpackhi_epi16(step2[12], step2[11]);
                        const __m128i s3_10_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_m16);
                        const __m128i s3_10_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_m16);
                        const __m128i s3_11_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_m16);
                        const __m128i s3_11_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_m16);
                        const __m128i s3_12_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_p16);
                        const __m128i s3_12_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_p16);
                        const __m128i s3_13_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_p16);
                        const __m128i s3_13_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m128i s3_10_4 = _mm_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_10_5 = _mm_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_11_4 = _mm_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_11_5 = _mm_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_12_4 = _mm_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_12_5 = _mm_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_13_4 = _mm_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_13_5 = _mm_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
                        lstep3[20] = _mm_srai_epi32(s3_10_4, DCT_CONST_BITS);
                        lstep3[21] = _mm_srai_epi32(s3_10_5, DCT_CONST_BITS);
                        lstep3[22] = _mm_srai_epi32(s3_11_4, DCT_CONST_BITS);
                        lstep3[23] = _mm_srai_epi32(s3_11_5, DCT_CONST_BITS);
                        lstep3[24] = _mm_srai_epi32(s3_12_4, DCT_CONST_BITS);
                        lstep3[25] = _mm_srai_epi32(s3_12_5, DCT_CONST_BITS);
                        lstep3[26] = _mm_srai_epi32(s3_13_4, DCT_CONST_BITS);
                        lstep3[27] = _mm_srai_epi32(s3_13_5, DCT_CONST_BITS);
                    }
                    {
                        lstep2[40] = _mm_unpacklo_epi16(step2[20], kZero);
                        lstep2[41] = _mm_unpackhi_epi16(step2[20], kZero);
                        lstep2[42] = _mm_unpacklo_epi16(step2[21], kZero);
                        lstep2[43] = _mm_unpackhi_epi16(step2[21], kZero);
                        lstep2[44] = _mm_unpacklo_epi16(step2[22], kZero);
                        lstep2[45] = _mm_unpackhi_epi16(step2[22], kZero);
                        lstep2[46] = _mm_unpacklo_epi16(step2[23], kZero);
                        lstep2[47] = _mm_unpackhi_epi16(step2[23], kZero);
                        lstep2[48] = _mm_unpacklo_epi16(step2[24], kZero);
                        lstep2[49] = _mm_unpackhi_epi16(step2[24], kZero);
                        lstep2[50] = _mm_unpacklo_epi16(step2[25], kZero);
                        lstep2[51] = _mm_unpackhi_epi16(step2[25], kZero);
                        lstep2[52] = _mm_unpacklo_epi16(step2[26], kZero);
                        lstep2[53] = _mm_unpackhi_epi16(step2[26], kZero);
                        lstep2[54] = _mm_unpacklo_epi16(step2[27], kZero);
                        lstep2[55] = _mm_unpackhi_epi16(step2[27], kZero);
                        lstep2[40] = _mm_madd_epi16(lstep2[40], kOne);
                        lstep2[41] = _mm_madd_epi16(lstep2[41], kOne);
                        lstep2[42] = _mm_madd_epi16(lstep2[42], kOne);
                        lstep2[43] = _mm_madd_epi16(lstep2[43], kOne);
                        lstep2[44] = _mm_madd_epi16(lstep2[44], kOne);
                        lstep2[45] = _mm_madd_epi16(lstep2[45], kOne);
                        lstep2[46] = _mm_madd_epi16(lstep2[46], kOne);
                        lstep2[47] = _mm_madd_epi16(lstep2[47], kOne);
                        lstep2[48] = _mm_madd_epi16(lstep2[48], kOne);
                        lstep2[49] = _mm_madd_epi16(lstep2[49], kOne);
                        lstep2[50] = _mm_madd_epi16(lstep2[50], kOne);
                        lstep2[51] = _mm_madd_epi16(lstep2[51], kOne);
                        lstep2[52] = _mm_madd_epi16(lstep2[52], kOne);
                        lstep2[53] = _mm_madd_epi16(lstep2[53], kOne);
                        lstep2[54] = _mm_madd_epi16(lstep2[54], kOne);
                        lstep2[55] = _mm_madd_epi16(lstep2[55], kOne);

                        lstep1[32] = _mm_unpacklo_epi16(step1[16], kZero);
                        lstep1[33] = _mm_unpackhi_epi16(step1[16], kZero);
                        lstep1[34] = _mm_unpacklo_epi16(step1[17], kZero);
                        lstep1[35] = _mm_unpackhi_epi16(step1[17], kZero);
                        lstep1[36] = _mm_unpacklo_epi16(step1[18], kZero);
                        lstep1[37] = _mm_unpackhi_epi16(step1[18], kZero);
                        lstep1[38] = _mm_unpacklo_epi16(step1[19], kZero);
                        lstep1[39] = _mm_unpackhi_epi16(step1[19], kZero);
                        lstep1[56] = _mm_unpacklo_epi16(step1[28], kZero);
                        lstep1[57] = _mm_unpackhi_epi16(step1[28], kZero);
                        lstep1[58] = _mm_unpacklo_epi16(step1[29], kZero);
                        lstep1[59] = _mm_unpackhi_epi16(step1[29], kZero);
                        lstep1[60] = _mm_unpacklo_epi16(step1[30], kZero);
                        lstep1[61] = _mm_unpackhi_epi16(step1[30], kZero);
                        lstep1[62] = _mm_unpacklo_epi16(step1[31], kZero);
                        lstep1[63] = _mm_unpackhi_epi16(step1[31], kZero);
                        lstep1[32] = _mm_madd_epi16(lstep1[32], kOne);
                        lstep1[33] = _mm_madd_epi16(lstep1[33], kOne);
                        lstep1[34] = _mm_madd_epi16(lstep1[34], kOne);
                        lstep1[35] = _mm_madd_epi16(lstep1[35], kOne);
                        lstep1[36] = _mm_madd_epi16(lstep1[36], kOne);
                        lstep1[37] = _mm_madd_epi16(lstep1[37], kOne);
                        lstep1[38] = _mm_madd_epi16(lstep1[38], kOne);
                        lstep1[39] = _mm_madd_epi16(lstep1[39], kOne);
                        lstep1[56] = _mm_madd_epi16(lstep1[56], kOne);
                        lstep1[57] = _mm_madd_epi16(lstep1[57], kOne);
                        lstep1[58] = _mm_madd_epi16(lstep1[58], kOne);
                        lstep1[59] = _mm_madd_epi16(lstep1[59], kOne);
                        lstep1[60] = _mm_madd_epi16(lstep1[60], kOne);
                        lstep1[61] = _mm_madd_epi16(lstep1[61], kOne);
                        lstep1[62] = _mm_madd_epi16(lstep1[62], kOne);
                        lstep1[63] = _mm_madd_epi16(lstep1[63], kOne);

                        lstep3[32] = _mm_add_epi32(lstep2[46], lstep1[32]);
                        lstep3[33] = _mm_add_epi32(lstep2[47], lstep1[33]);

                        lstep3[34] = _mm_add_epi32(lstep2[44], lstep1[34]);
                        lstep3[35] = _mm_add_epi32(lstep2[45], lstep1[35]);
                        lstep3[36] = _mm_add_epi32(lstep2[42], lstep1[36]);
                        lstep3[37] = _mm_add_epi32(lstep2[43], lstep1[37]);
                        lstep3[38] = _mm_add_epi32(lstep2[40], lstep1[38]);
                        lstep3[39] = _mm_add_epi32(lstep2[41], lstep1[39]);
                        lstep3[40] = _mm_sub_epi32(lstep1[38], lstep2[40]);
                        lstep3[41] = _mm_sub_epi32(lstep1[39], lstep2[41]);
                        lstep3[42] = _mm_sub_epi32(lstep1[36], lstep2[42]);
                        lstep3[43] = _mm_sub_epi32(lstep1[37], lstep2[43]);
                        lstep3[44] = _mm_sub_epi32(lstep1[34], lstep2[44]);
                        lstep3[45] = _mm_sub_epi32(lstep1[35], lstep2[45]);
                        lstep3[46] = _mm_sub_epi32(lstep1[32], lstep2[46]);
                        lstep3[47] = _mm_sub_epi32(lstep1[33], lstep2[47]);
                        lstep3[48] = _mm_sub_epi32(lstep1[62], lstep2[48]);
                        lstep3[49] = _mm_sub_epi32(lstep1[63], lstep2[49]);
                        lstep3[50] = _mm_sub_epi32(lstep1[60], lstep2[50]);
                        lstep3[51] = _mm_sub_epi32(lstep1[61], lstep2[51]);
                        lstep3[52] = _mm_sub_epi32(lstep1[58], lstep2[52]);
                        lstep3[53] = _mm_sub_epi32(lstep1[59], lstep2[53]);
                        lstep3[54] = _mm_sub_epi32(lstep1[56], lstep2[54]);
                        lstep3[55] = _mm_sub_epi32(lstep1[57], lstep2[55]);
                        lstep3[56] = _mm_add_epi32(lstep2[54], lstep1[56]);
                        lstep3[57] = _mm_add_epi32(lstep2[55], lstep1[57]);
                        lstep3[58] = _mm_add_epi32(lstep2[52], lstep1[58]);
                        lstep3[59] = _mm_add_epi32(lstep2[53], lstep1[59]);
                        lstep3[60] = _mm_add_epi32(lstep2[50], lstep1[60]);
                        lstep3[61] = _mm_add_epi32(lstep2[51], lstep1[61]);
                        lstep3[62] = _mm_add_epi32(lstep2[48], lstep1[62]);
                        lstep3[63] = _mm_add_epi32(lstep2[49], lstep1[63]);
                    }

                    // stage 4
                    {
                        // expanding to 32-bit length priori to addition operations
                        lstep2[16] = _mm_unpacklo_epi16(step2[8], kZero);
                        lstep2[17] = _mm_unpackhi_epi16(step2[8], kZero);
                        lstep2[18] = _mm_unpacklo_epi16(step2[9], kZero);
                        lstep2[19] = _mm_unpackhi_epi16(step2[9], kZero);
                        lstep2[28] = _mm_unpacklo_epi16(step2[14], kZero);
                        lstep2[29] = _mm_unpackhi_epi16(step2[14], kZero);
                        lstep2[30] = _mm_unpacklo_epi16(step2[15], kZero);
                        lstep2[31] = _mm_unpackhi_epi16(step2[15], kZero);
                        lstep2[16] = _mm_madd_epi16(lstep2[16], kOne);
                        lstep2[17] = _mm_madd_epi16(lstep2[17], kOne);
                        lstep2[18] = _mm_madd_epi16(lstep2[18], kOne);
                        lstep2[19] = _mm_madd_epi16(lstep2[19], kOne);
                        lstep2[28] = _mm_madd_epi16(lstep2[28], kOne);
                        lstep2[29] = _mm_madd_epi16(lstep2[29], kOne);
                        lstep2[30] = _mm_madd_epi16(lstep2[30], kOne);
                        lstep2[31] = _mm_madd_epi16(lstep2[31], kOne);

                        lstep1[0] = _mm_add_epi32(lstep3[6], lstep3[0]);
                        lstep1[1] = _mm_add_epi32(lstep3[7], lstep3[1]);
                        lstep1[2] = _mm_add_epi32(lstep3[4], lstep3[2]);
                        lstep1[3] = _mm_add_epi32(lstep3[5], lstep3[3]);
                        lstep1[4] = _mm_sub_epi32(lstep3[2], lstep3[4]);
                        lstep1[5] = _mm_sub_epi32(lstep3[3], lstep3[5]);
                        lstep1[6] = _mm_sub_epi32(lstep3[0], lstep3[6]);
                        lstep1[7] = _mm_sub_epi32(lstep3[1], lstep3[7]);
                        lstep1[16] = _mm_add_epi32(lstep3[22], lstep2[16]);
                        lstep1[17] = _mm_add_epi32(lstep3[23], lstep2[17]);
                        lstep1[18] = _mm_add_epi32(lstep3[20], lstep2[18]);
                        lstep1[19] = _mm_add_epi32(lstep3[21], lstep2[19]);
                        lstep1[20] = _mm_sub_epi32(lstep2[18], lstep3[20]);
                        lstep1[21] = _mm_sub_epi32(lstep2[19], lstep3[21]);
                        lstep1[22] = _mm_sub_epi32(lstep2[16], lstep3[22]);
                        lstep1[23] = _mm_sub_epi32(lstep2[17], lstep3[23]);
                        lstep1[24] = _mm_sub_epi32(lstep2[30], lstep3[24]);
                        lstep1[25] = _mm_sub_epi32(lstep2[31], lstep3[25]);
                        lstep1[26] = _mm_sub_epi32(lstep2[28], lstep3[26]);
                        lstep1[27] = _mm_sub_epi32(lstep2[29], lstep3[27]);
                        lstep1[28] = _mm_add_epi32(lstep3[26], lstep2[28]);
                        lstep1[29] = _mm_add_epi32(lstep3[27], lstep2[29]);
                        lstep1[30] = _mm_add_epi32(lstep3[24], lstep2[30]);
                        lstep1[31] = _mm_add_epi32(lstep3[25], lstep2[31]);
                    }
                    {
                        // to be continued...
                        //
                        const __m128i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
                        const __m128i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);

                        u[0] = _mm_unpacklo_epi32(lstep3[12], lstep3[10]);
                        u[1] = _mm_unpackhi_epi32(lstep3[12], lstep3[10]);
                        u[2] = _mm_unpacklo_epi32(lstep3[13], lstep3[11]);
                        u[3] = _mm_unpackhi_epi32(lstep3[13], lstep3[11]);

                        // TODO(jingning): manually inline k_madd_epi32_ to further hide
                        // instruction latency.
                        v[0] = k_madd_epi32(u[0], k32_p16_m16);
                        v[1] = k_madd_epi32(u[1], k32_p16_m16);
                        v[2] = k_madd_epi32(u[2], k32_p16_m16);
                        v[3] = k_madd_epi32(u[3], k32_p16_m16);
                        v[4] = k_madd_epi32(u[0], k32_p16_p16);
                        v[5] = k_madd_epi32(u[1], k32_p16_p16);
                        v[6] = k_madd_epi32(u[2], k32_p16_p16);
                        v[7] = k_madd_epi32(u[3], k32_p16_p16);
#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_8(&v[0], &v[1], &v[2], &v[3], &v[4],
                            &v[5], &v[6], &v[7], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);

                        lstep1[10] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        lstep1[11] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        lstep1[12] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        lstep1[13] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                    }
                    {
                        const __m128i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
                        const __m128i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
                        const __m128i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);

                        u[0] = _mm_unpacklo_epi32(lstep3[36], lstep3[58]);
                        u[1] = _mm_unpackhi_epi32(lstep3[36], lstep3[58]);
                        u[2] = _mm_unpacklo_epi32(lstep3[37], lstep3[59]);
                        u[3] = _mm_unpackhi_epi32(lstep3[37], lstep3[59]);
                        u[4] = _mm_unpacklo_epi32(lstep3[38], lstep3[56]);
                        u[5] = _mm_unpackhi_epi32(lstep3[38], lstep3[56]);
                        u[6] = _mm_unpacklo_epi32(lstep3[39], lstep3[57]);
                        u[7] = _mm_unpackhi_epi32(lstep3[39], lstep3[57]);
                        u[8] = _mm_unpacklo_epi32(lstep3[40], lstep3[54]);
                        u[9] = _mm_unpackhi_epi32(lstep3[40], lstep3[54]);
                        u[10] = _mm_unpacklo_epi32(lstep3[41], lstep3[55]);
                        u[11] = _mm_unpackhi_epi32(lstep3[41], lstep3[55]);
                        u[12] = _mm_unpacklo_epi32(lstep3[42], lstep3[52]);
                        u[13] = _mm_unpackhi_epi32(lstep3[42], lstep3[52]);
                        u[14] = _mm_unpacklo_epi32(lstep3[43], lstep3[53]);
                        u[15] = _mm_unpackhi_epi32(lstep3[43], lstep3[53]);

                        v[0] = k_madd_epi32(u[0], k32_m08_p24);
                        v[1] = k_madd_epi32(u[1], k32_m08_p24);
                        v[2] = k_madd_epi32(u[2], k32_m08_p24);
                        v[3] = k_madd_epi32(u[3], k32_m08_p24);
                        v[4] = k_madd_epi32(u[4], k32_m08_p24);
                        v[5] = k_madd_epi32(u[5], k32_m08_p24);
                        v[6] = k_madd_epi32(u[6], k32_m08_p24);
                        v[7] = k_madd_epi32(u[7], k32_m08_p24);
                        v[8] = k_madd_epi32(u[8], k32_m24_m08);
                        v[9] = k_madd_epi32(u[9], k32_m24_m08);
                        v[10] = k_madd_epi32(u[10], k32_m24_m08);
                        v[11] = k_madd_epi32(u[11], k32_m24_m08);
                        v[12] = k_madd_epi32(u[12], k32_m24_m08);
                        v[13] = k_madd_epi32(u[13], k32_m24_m08);
                        v[14] = k_madd_epi32(u[14], k32_m24_m08);
                        v[15] = k_madd_epi32(u[15], k32_m24_m08);
                        v[16] = k_madd_epi32(u[12], k32_m08_p24);
                        v[17] = k_madd_epi32(u[13], k32_m08_p24);
                        v[18] = k_madd_epi32(u[14], k32_m08_p24);
                        v[19] = k_madd_epi32(u[15], k32_m08_p24);
                        v[20] = k_madd_epi32(u[8], k32_m08_p24);
                        v[21] = k_madd_epi32(u[9], k32_m08_p24);
                        v[22] = k_madd_epi32(u[10], k32_m08_p24);
                        v[23] = k_madd_epi32(u[11], k32_m08_p24);
                        v[24] = k_madd_epi32(u[4], k32_p24_p08);
                        v[25] = k_madd_epi32(u[5], k32_p24_p08);
                        v[26] = k_madd_epi32(u[6], k32_p24_p08);
                        v[27] = k_madd_epi32(u[7], k32_p24_p08);
                        v[28] = k_madd_epi32(u[0], k32_p24_p08);
                        v[29] = k_madd_epi32(u[1], k32_p24_p08);
                        v[30] = k_madd_epi32(u[2], k32_p24_p08);
                        v[31] = k_madd_epi32(u[3], k32_p24_p08);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_32(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);
                        u[8] = k_packs_epi64(v[16], v[17]);
                        u[9] = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                        lstep1[36] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        lstep1[37] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        lstep1[38] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        lstep1[39] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                        lstep1[40] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
                        lstep1[41] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
                        lstep1[42] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
                        lstep1[43] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
                        lstep1[52] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
                        lstep1[53] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
                        lstep1[54] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
                        lstep1[55] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
                        lstep1[56] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
                        lstep1[57] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
                        lstep1[58] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
                        lstep1[59] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
                    }
                    // stage 5
                    {
                        lstep2[8] = _mm_add_epi32(lstep1[10], lstep3[8]);
                        lstep2[9] = _mm_add_epi32(lstep1[11], lstep3[9]);
                        lstep2[10] = _mm_sub_epi32(lstep3[8], lstep1[10]);
                        lstep2[11] = _mm_sub_epi32(lstep3[9], lstep1[11]);
                        lstep2[12] = _mm_sub_epi32(lstep3[14], lstep1[12]);
                        lstep2[13] = _mm_sub_epi32(lstep3[15], lstep1[13]);
                        lstep2[14] = _mm_add_epi32(lstep1[12], lstep3[14]);
                        lstep2[15] = _mm_add_epi32(lstep1[13], lstep3[15]);
                    }
                    {
                        const __m128i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
                        const __m128i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);
                        const __m128i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);
                        const __m128i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);

                        u[0] = _mm_unpacklo_epi32(lstep1[0], lstep1[2]);
                        u[1] = _mm_unpackhi_epi32(lstep1[0], lstep1[2]);
                        u[2] = _mm_unpacklo_epi32(lstep1[1], lstep1[3]);
                        u[3] = _mm_unpackhi_epi32(lstep1[1], lstep1[3]);
                        u[4] = _mm_unpacklo_epi32(lstep1[4], lstep1[6]);
                        u[5] = _mm_unpackhi_epi32(lstep1[4], lstep1[6]);
                        u[6] = _mm_unpacklo_epi32(lstep1[5], lstep1[7]);
                        u[7] = _mm_unpackhi_epi32(lstep1[5], lstep1[7]);

                        // TODO(jingning): manually inline k_madd_epi32_ to further hide
                        // instruction latency.
                        v[0] = k_madd_epi32(u[0], k32_p16_p16);
                        v[1] = k_madd_epi32(u[1], k32_p16_p16);
                        v[2] = k_madd_epi32(u[2], k32_p16_p16);
                        v[3] = k_madd_epi32(u[3], k32_p16_p16);
                        v[4] = k_madd_epi32(u[0], k32_p16_m16);
                        v[5] = k_madd_epi32(u[1], k32_p16_m16);
                        v[6] = k_madd_epi32(u[2], k32_p16_m16);
                        v[7] = k_madd_epi32(u[3], k32_p16_m16);
                        v[8] = k_madd_epi32(u[4], k32_p24_p08);
                        v[9] = k_madd_epi32(u[5], k32_p24_p08);
                        v[10] = k_madd_epi32(u[6], k32_p24_p08);
                        v[11] = k_madd_epi32(u[7], k32_p24_p08);
                        v[12] = k_madd_epi32(u[4], k32_m08_p24);
                        v[13] = k_madd_epi32(u[5], k32_m08_p24);
                        v[14] = k_madd_epi32(u[6], k32_m08_p24);
                        v[15] = k_madd_epi32(u[7], k32_m08_p24);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_16(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);

                        sign[0] = _mm_cmplt_epi32(u[0], kZero);
                        sign[1] = _mm_cmplt_epi32(u[1], kZero);
                        sign[2] = _mm_cmplt_epi32(u[2], kZero);
                        sign[3] = _mm_cmplt_epi32(u[3], kZero);
                        sign[4] = _mm_cmplt_epi32(u[4], kZero);
                        sign[5] = _mm_cmplt_epi32(u[5], kZero);
                        sign[6] = _mm_cmplt_epi32(u[6], kZero);
                        sign[7] = _mm_cmplt_epi32(u[7], kZero);

                        u[0] = _mm_sub_epi32(u[0], sign[0]);
                        u[1] = _mm_sub_epi32(u[1], sign[1]);
                        u[2] = _mm_sub_epi32(u[2], sign[2]);
                        u[3] = _mm_sub_epi32(u[3], sign[3]);
                        u[4] = _mm_sub_epi32(u[4], sign[4]);
                        u[5] = _mm_sub_epi32(u[5], sign[5]);
                        u[6] = _mm_sub_epi32(u[6], sign[6]);
                        u[7] = _mm_sub_epi32(u[7], sign[7]);

                        u[0] = _mm_add_epi32(u[0], K32One);
                        u[1] = _mm_add_epi32(u[1], K32One);
                        u[2] = _mm_add_epi32(u[2], K32One);
                        u[3] = _mm_add_epi32(u[3], K32One);
                        u[4] = _mm_add_epi32(u[4], K32One);
                        u[5] = _mm_add_epi32(u[5], K32One);
                        u[6] = _mm_add_epi32(u[6], K32One);
                        u[7] = _mm_add_epi32(u[7], K32One);

                        u[0] = _mm_srai_epi32(u[0], 2);
                        u[1] = _mm_srai_epi32(u[1], 2);
                        u[2] = _mm_srai_epi32(u[2], 2);
                        u[3] = _mm_srai_epi32(u[3], 2);
                        u[4] = _mm_srai_epi32(u[4], 2);
                        u[5] = _mm_srai_epi32(u[5], 2);
                        u[6] = _mm_srai_epi32(u[6], 2);
                        u[7] = _mm_srai_epi32(u[7], 2);

                        // Combine
                        out[0] = _mm_packs_epi32(u[0], u[1]);
                        out[16] = _mm_packs_epi32(u[2], u[3]);
                        out[8] = _mm_packs_epi32(u[4], u[5]);
                        out[24] = _mm_packs_epi32(u[6], u[7]);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&out[0], &out[16], &out[8], &out[24]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
                        const __m128i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
                        const __m128i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);

                        u[0] = _mm_unpacklo_epi32(lstep1[18], lstep1[28]);
                        u[1] = _mm_unpackhi_epi32(lstep1[18], lstep1[28]);
                        u[2] = _mm_unpacklo_epi32(lstep1[19], lstep1[29]);
                        u[3] = _mm_unpackhi_epi32(lstep1[19], lstep1[29]);
                        u[4] = _mm_unpacklo_epi32(lstep1[20], lstep1[26]);
                        u[5] = _mm_unpackhi_epi32(lstep1[20], lstep1[26]);
                        u[6] = _mm_unpacklo_epi32(lstep1[21], lstep1[27]);
                        u[7] = _mm_unpackhi_epi32(lstep1[21], lstep1[27]);

                        v[0] = k_madd_epi32(u[0], k32_m08_p24);
                        v[1] = k_madd_epi32(u[1], k32_m08_p24);
                        v[2] = k_madd_epi32(u[2], k32_m08_p24);
                        v[3] = k_madd_epi32(u[3], k32_m08_p24);
                        v[4] = k_madd_epi32(u[4], k32_m24_m08);
                        v[5] = k_madd_epi32(u[5], k32_m24_m08);
                        v[6] = k_madd_epi32(u[6], k32_m24_m08);
                        v[7] = k_madd_epi32(u[7], k32_m24_m08);
                        v[8] = k_madd_epi32(u[4], k32_m08_p24);
                        v[9] = k_madd_epi32(u[5], k32_m08_p24);
                        v[10] = k_madd_epi32(u[6], k32_m08_p24);
                        v[11] = k_madd_epi32(u[7], k32_m08_p24);
                        v[12] = k_madd_epi32(u[0], k32_p24_p08);
                        v[13] = k_madd_epi32(u[1], k32_p24_p08);
                        v[14] = k_madd_epi32(u[2], k32_p24_p08);
                        v[15] = k_madd_epi32(u[3], k32_p24_p08);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_16(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);

                        u[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        u[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        u[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        u[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        u[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        u[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        u[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        u[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                        lstep2[18] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
                        lstep2[19] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
                        lstep2[20] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
                        lstep2[21] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
                        lstep2[26] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
                        lstep2[27] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
                        lstep2[28] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
                        lstep2[29] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
                    }
                    {
                        lstep2[32] = _mm_add_epi32(lstep1[38], lstep3[32]);
                        lstep2[33] = _mm_add_epi32(lstep1[39], lstep3[33]);
                        lstep2[34] = _mm_add_epi32(lstep1[36], lstep3[34]);
                        lstep2[35] = _mm_add_epi32(lstep1[37], lstep3[35]);
                        lstep2[36] = _mm_sub_epi32(lstep3[34], lstep1[36]);
                        lstep2[37] = _mm_sub_epi32(lstep3[35], lstep1[37]);
                        lstep2[38] = _mm_sub_epi32(lstep3[32], lstep1[38]);
                        lstep2[39] = _mm_sub_epi32(lstep3[33], lstep1[39]);
                        lstep2[40] = _mm_sub_epi32(lstep3[46], lstep1[40]);
                        lstep2[41] = _mm_sub_epi32(lstep3[47], lstep1[41]);
                        lstep2[42] = _mm_sub_epi32(lstep3[44], lstep1[42]);
                        lstep2[43] = _mm_sub_epi32(lstep3[45], lstep1[43]);
                        lstep2[44] = _mm_add_epi32(lstep1[42], lstep3[44]);
                        lstep2[45] = _mm_add_epi32(lstep1[43], lstep3[45]);
                        lstep2[46] = _mm_add_epi32(lstep1[40], lstep3[46]);
                        lstep2[47] = _mm_add_epi32(lstep1[41], lstep3[47]);
                        lstep2[48] = _mm_add_epi32(lstep1[54], lstep3[48]);
                        lstep2[49] = _mm_add_epi32(lstep1[55], lstep3[49]);
                        lstep2[50] = _mm_add_epi32(lstep1[52], lstep3[50]);
                        lstep2[51] = _mm_add_epi32(lstep1[53], lstep3[51]);
                        lstep2[52] = _mm_sub_epi32(lstep3[50], lstep1[52]);
                        lstep2[53] = _mm_sub_epi32(lstep3[51], lstep1[53]);
                        lstep2[54] = _mm_sub_epi32(lstep3[48], lstep1[54]);
                        lstep2[55] = _mm_sub_epi32(lstep3[49], lstep1[55]);
                        lstep2[56] = _mm_sub_epi32(lstep3[62], lstep1[56]);
                        lstep2[57] = _mm_sub_epi32(lstep3[63], lstep1[57]);
                        lstep2[58] = _mm_sub_epi32(lstep3[60], lstep1[58]);
                        lstep2[59] = _mm_sub_epi32(lstep3[61], lstep1[59]);
                        lstep2[60] = _mm_add_epi32(lstep1[58], lstep3[60]);
                        lstep2[61] = _mm_add_epi32(lstep1[59], lstep3[61]);
                        lstep2[62] = _mm_add_epi32(lstep1[56], lstep3[62]);
                        lstep2[63] = _mm_add_epi32(lstep1[57], lstep3[63]);
                    }
                    // stage 6
                    {
                        const __m128i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);
                        const __m128i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
                        const __m128i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
                        const __m128i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);

                        u[0] = _mm_unpacklo_epi32(lstep2[8], lstep2[14]);
                        u[1] = _mm_unpackhi_epi32(lstep2[8], lstep2[14]);
                        u[2] = _mm_unpacklo_epi32(lstep2[9], lstep2[15]);
                        u[3] = _mm_unpackhi_epi32(lstep2[9], lstep2[15]);
                        u[4] = _mm_unpacklo_epi32(lstep2[10], lstep2[12]);
                        u[5] = _mm_unpackhi_epi32(lstep2[10], lstep2[12]);
                        u[6] = _mm_unpacklo_epi32(lstep2[11], lstep2[13]);
                        u[7] = _mm_unpackhi_epi32(lstep2[11], lstep2[13]);
                        u[8] = _mm_unpacklo_epi32(lstep2[10], lstep2[12]);
                        u[9] = _mm_unpackhi_epi32(lstep2[10], lstep2[12]);
                        u[10] = _mm_unpacklo_epi32(lstep2[11], lstep2[13]);
                        u[11] = _mm_unpackhi_epi32(lstep2[11], lstep2[13]);
                        u[12] = _mm_unpacklo_epi32(lstep2[8], lstep2[14]);
                        u[13] = _mm_unpackhi_epi32(lstep2[8], lstep2[14]);
                        u[14] = _mm_unpacklo_epi32(lstep2[9], lstep2[15]);
                        u[15] = _mm_unpackhi_epi32(lstep2[9], lstep2[15]);

                        v[0] = k_madd_epi32(u[0], k32_p28_p04);
                        v[1] = k_madd_epi32(u[1], k32_p28_p04);
                        v[2] = k_madd_epi32(u[2], k32_p28_p04);
                        v[3] = k_madd_epi32(u[3], k32_p28_p04);
                        v[4] = k_madd_epi32(u[4], k32_p12_p20);
                        v[5] = k_madd_epi32(u[5], k32_p12_p20);
                        v[6] = k_madd_epi32(u[6], k32_p12_p20);
                        v[7] = k_madd_epi32(u[7], k32_p12_p20);
                        v[8] = k_madd_epi32(u[8], k32_m20_p12);
                        v[9] = k_madd_epi32(u[9], k32_m20_p12);
                        v[10] = k_madd_epi32(u[10], k32_m20_p12);
                        v[11] = k_madd_epi32(u[11], k32_m20_p12);
                        v[12] = k_madd_epi32(u[12], k32_m04_p28);
                        v[13] = k_madd_epi32(u[13], k32_m04_p28);
                        v[14] = k_madd_epi32(u[14], k32_m04_p28);
                        v[15] = k_madd_epi32(u[15], k32_m04_p28);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_16(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);

                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);

                        sign[0] = _mm_cmplt_epi32(u[0], kZero);
                        sign[1] = _mm_cmplt_epi32(u[1], kZero);
                        sign[2] = _mm_cmplt_epi32(u[2], kZero);
                        sign[3] = _mm_cmplt_epi32(u[3], kZero);
                        sign[4] = _mm_cmplt_epi32(u[4], kZero);
                        sign[5] = _mm_cmplt_epi32(u[5], kZero);
                        sign[6] = _mm_cmplt_epi32(u[6], kZero);
                        sign[7] = _mm_cmplt_epi32(u[7], kZero);

                        u[0] = _mm_sub_epi32(u[0], sign[0]);
                        u[1] = _mm_sub_epi32(u[1], sign[1]);
                        u[2] = _mm_sub_epi32(u[2], sign[2]);
                        u[3] = _mm_sub_epi32(u[3], sign[3]);
                        u[4] = _mm_sub_epi32(u[4], sign[4]);
                        u[5] = _mm_sub_epi32(u[5], sign[5]);
                        u[6] = _mm_sub_epi32(u[6], sign[6]);
                        u[7] = _mm_sub_epi32(u[7], sign[7]);

                        u[0] = _mm_add_epi32(u[0], K32One);
                        u[1] = _mm_add_epi32(u[1], K32One);
                        u[2] = _mm_add_epi32(u[2], K32One);
                        u[3] = _mm_add_epi32(u[3], K32One);
                        u[4] = _mm_add_epi32(u[4], K32One);
                        u[5] = _mm_add_epi32(u[5], K32One);
                        u[6] = _mm_add_epi32(u[6], K32One);
                        u[7] = _mm_add_epi32(u[7], K32One);

                        u[0] = _mm_srai_epi32(u[0], 2);
                        u[1] = _mm_srai_epi32(u[1], 2);
                        u[2] = _mm_srai_epi32(u[2], 2);
                        u[3] = _mm_srai_epi32(u[3], 2);
                        u[4] = _mm_srai_epi32(u[4], 2);
                        u[5] = _mm_srai_epi32(u[5], 2);
                        u[6] = _mm_srai_epi32(u[6], 2);
                        u[7] = _mm_srai_epi32(u[7], 2);

                        out[4] = _mm_packs_epi32(u[0], u[1]);
                        out[20] = _mm_packs_epi32(u[2], u[3]);
                        out[12] = _mm_packs_epi32(u[4], u[5]);
                        out[28] = _mm_packs_epi32(u[6], u[7]);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&out[4], &out[20], &out[12], &out[28]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        lstep3[16] = _mm_add_epi32(lstep2[18], lstep1[16]);
                        lstep3[17] = _mm_add_epi32(lstep2[19], lstep1[17]);
                        lstep3[18] = _mm_sub_epi32(lstep1[16], lstep2[18]);
                        lstep3[19] = _mm_sub_epi32(lstep1[17], lstep2[19]);
                        lstep3[20] = _mm_sub_epi32(lstep1[22], lstep2[20]);
                        lstep3[21] = _mm_sub_epi32(lstep1[23], lstep2[21]);
                        lstep3[22] = _mm_add_epi32(lstep2[20], lstep1[22]);
                        lstep3[23] = _mm_add_epi32(lstep2[21], lstep1[23]);
                        lstep3[24] = _mm_add_epi32(lstep2[26], lstep1[24]);
                        lstep3[25] = _mm_add_epi32(lstep2[27], lstep1[25]);
                        lstep3[26] = _mm_sub_epi32(lstep1[24], lstep2[26]);
                        lstep3[27] = _mm_sub_epi32(lstep1[25], lstep2[27]);
                        lstep3[28] = _mm_sub_epi32(lstep1[30], lstep2[28]);
                        lstep3[29] = _mm_sub_epi32(lstep1[31], lstep2[29]);
                        lstep3[30] = _mm_add_epi32(lstep2[28], lstep1[30]);
                        lstep3[31] = _mm_add_epi32(lstep2[29], lstep1[31]);
                    }
                    {
                        const __m128i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);
                        const __m128i k32_m28_m04 = pair_set_epi32(-cospi_28_64, -cospi_4_64);
                        const __m128i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
                        const __m128i k32_m12_m20 =
                            pair_set_epi32(-cospi_12_64, -cospi_20_64);
                        const __m128i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
                        const __m128i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);

                        u[0] = _mm_unpacklo_epi32(lstep2[34], lstep2[60]);
                        u[1] = _mm_unpackhi_epi32(lstep2[34], lstep2[60]);
                        u[2] = _mm_unpacklo_epi32(lstep2[35], lstep2[61]);
                        u[3] = _mm_unpackhi_epi32(lstep2[35], lstep2[61]);
                        u[4] = _mm_unpacklo_epi32(lstep2[36], lstep2[58]);
                        u[5] = _mm_unpackhi_epi32(lstep2[36], lstep2[58]);
                        u[6] = _mm_unpacklo_epi32(lstep2[37], lstep2[59]);
                        u[7] = _mm_unpackhi_epi32(lstep2[37], lstep2[59]);
                        u[8] = _mm_unpacklo_epi32(lstep2[42], lstep2[52]);
                        u[9] = _mm_unpackhi_epi32(lstep2[42], lstep2[52]);
                        u[10] = _mm_unpacklo_epi32(lstep2[43], lstep2[53]);
                        u[11] = _mm_unpackhi_epi32(lstep2[43], lstep2[53]);
                        u[12] = _mm_unpacklo_epi32(lstep2[44], lstep2[50]);
                        u[13] = _mm_unpackhi_epi32(lstep2[44], lstep2[50]);
                        u[14] = _mm_unpacklo_epi32(lstep2[45], lstep2[51]);
                        u[15] = _mm_unpackhi_epi32(lstep2[45], lstep2[51]);

                        v[0] = k_madd_epi32(u[0], k32_m04_p28);
                        v[1] = k_madd_epi32(u[1], k32_m04_p28);
                        v[2] = k_madd_epi32(u[2], k32_m04_p28);
                        v[3] = k_madd_epi32(u[3], k32_m04_p28);
                        v[4] = k_madd_epi32(u[4], k32_m28_m04);
                        v[5] = k_madd_epi32(u[5], k32_m28_m04);
                        v[6] = k_madd_epi32(u[6], k32_m28_m04);
                        v[7] = k_madd_epi32(u[7], k32_m28_m04);
                        v[8] = k_madd_epi32(u[8], k32_m20_p12);
                        v[9] = k_madd_epi32(u[9], k32_m20_p12);
                        v[10] = k_madd_epi32(u[10], k32_m20_p12);
                        v[11] = k_madd_epi32(u[11], k32_m20_p12);
                        v[12] = k_madd_epi32(u[12], k32_m12_m20);
                        v[13] = k_madd_epi32(u[13], k32_m12_m20);
                        v[14] = k_madd_epi32(u[14], k32_m12_m20);
                        v[15] = k_madd_epi32(u[15], k32_m12_m20);
                        v[16] = k_madd_epi32(u[12], k32_m20_p12);
                        v[17] = k_madd_epi32(u[13], k32_m20_p12);
                        v[18] = k_madd_epi32(u[14], k32_m20_p12);
                        v[19] = k_madd_epi32(u[15], k32_m20_p12);
                        v[20] = k_madd_epi32(u[8], k32_p12_p20);
                        v[21] = k_madd_epi32(u[9], k32_p12_p20);
                        v[22] = k_madd_epi32(u[10], k32_p12_p20);
                        v[23] = k_madd_epi32(u[11], k32_p12_p20);
                        v[24] = k_madd_epi32(u[4], k32_m04_p28);
                        v[25] = k_madd_epi32(u[5], k32_m04_p28);
                        v[26] = k_madd_epi32(u[6], k32_m04_p28);
                        v[27] = k_madd_epi32(u[7], k32_m04_p28);
                        v[28] = k_madd_epi32(u[0], k32_p28_p04);
                        v[29] = k_madd_epi32(u[1], k32_p28_p04);
                        v[30] = k_madd_epi32(u[2], k32_p28_p04);
                        v[31] = k_madd_epi32(u[3], k32_p28_p04);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_32(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);
                        u[8] = k_packs_epi64(v[16], v[17]);
                        u[9] = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                        lstep3[34] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        lstep3[35] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        lstep3[36] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        lstep3[37] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                        lstep3[42] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
                        lstep3[43] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
                        lstep3[44] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
                        lstep3[45] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
                        lstep3[50] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
                        lstep3[51] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
                        lstep3[52] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
                        lstep3[53] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
                        lstep3[58] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
                        lstep3[59] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
                        lstep3[60] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
                        lstep3[61] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
                    }
                    // stage 7
                    {
                        const __m128i k32_p30_p02 = pair_set_epi32(cospi_30_64, cospi_2_64);
                        const __m128i k32_p14_p18 = pair_set_epi32(cospi_14_64, cospi_18_64);
                        const __m128i k32_p22_p10 = pair_set_epi32(cospi_22_64, cospi_10_64);
                        const __m128i k32_p06_p26 = pair_set_epi32(cospi_6_64, cospi_26_64);
                        const __m128i k32_m26_p06 = pair_set_epi32(-cospi_26_64, cospi_6_64);
                        const __m128i k32_m10_p22 = pair_set_epi32(-cospi_10_64, cospi_22_64);
                        const __m128i k32_m18_p14 = pair_set_epi32(-cospi_18_64, cospi_14_64);
                        const __m128i k32_m02_p30 = pair_set_epi32(-cospi_2_64, cospi_30_64);

                        u[0] = _mm_unpacklo_epi32(lstep3[16], lstep3[30]);
                        u[1] = _mm_unpackhi_epi32(lstep3[16], lstep3[30]);
                        u[2] = _mm_unpacklo_epi32(lstep3[17], lstep3[31]);
                        u[3] = _mm_unpackhi_epi32(lstep3[17], lstep3[31]);
                        u[4] = _mm_unpacklo_epi32(lstep3[18], lstep3[28]);
                        u[5] = _mm_unpackhi_epi32(lstep3[18], lstep3[28]);
                        u[6] = _mm_unpacklo_epi32(lstep3[19], lstep3[29]);
                        u[7] = _mm_unpackhi_epi32(lstep3[19], lstep3[29]);
                        u[8] = _mm_unpacklo_epi32(lstep3[20], lstep3[26]);
                        u[9] = _mm_unpackhi_epi32(lstep3[20], lstep3[26]);
                        u[10] = _mm_unpacklo_epi32(lstep3[21], lstep3[27]);
                        u[11] = _mm_unpackhi_epi32(lstep3[21], lstep3[27]);
                        u[12] = _mm_unpacklo_epi32(lstep3[22], lstep3[24]);
                        u[13] = _mm_unpackhi_epi32(lstep3[22], lstep3[24]);
                        u[14] = _mm_unpacklo_epi32(lstep3[23], lstep3[25]);
                        u[15] = _mm_unpackhi_epi32(lstep3[23], lstep3[25]);

                        v[0] = k_madd_epi32(u[0], k32_p30_p02);
                        v[1] = k_madd_epi32(u[1], k32_p30_p02);
                        v[2] = k_madd_epi32(u[2], k32_p30_p02);
                        v[3] = k_madd_epi32(u[3], k32_p30_p02);
                        v[4] = k_madd_epi32(u[4], k32_p14_p18);
                        v[5] = k_madd_epi32(u[5], k32_p14_p18);
                        v[6] = k_madd_epi32(u[6], k32_p14_p18);
                        v[7] = k_madd_epi32(u[7], k32_p14_p18);
                        v[8] = k_madd_epi32(u[8], k32_p22_p10);
                        v[9] = k_madd_epi32(u[9], k32_p22_p10);
                        v[10] = k_madd_epi32(u[10], k32_p22_p10);
                        v[11] = k_madd_epi32(u[11], k32_p22_p10);
                        v[12] = k_madd_epi32(u[12], k32_p06_p26);
                        v[13] = k_madd_epi32(u[13], k32_p06_p26);
                        v[14] = k_madd_epi32(u[14], k32_p06_p26);
                        v[15] = k_madd_epi32(u[15], k32_p06_p26);
                        v[16] = k_madd_epi32(u[12], k32_m26_p06);
                        v[17] = k_madd_epi32(u[13], k32_m26_p06);
                        v[18] = k_madd_epi32(u[14], k32_m26_p06);
                        v[19] = k_madd_epi32(u[15], k32_m26_p06);
                        v[20] = k_madd_epi32(u[8], k32_m10_p22);
                        v[21] = k_madd_epi32(u[9], k32_m10_p22);
                        v[22] = k_madd_epi32(u[10], k32_m10_p22);
                        v[23] = k_madd_epi32(u[11], k32_m10_p22);
                        v[24] = k_madd_epi32(u[4], k32_m18_p14);
                        v[25] = k_madd_epi32(u[5], k32_m18_p14);
                        v[26] = k_madd_epi32(u[6], k32_m18_p14);
                        v[27] = k_madd_epi32(u[7], k32_m18_p14);
                        v[28] = k_madd_epi32(u[0], k32_m02_p30);
                        v[29] = k_madd_epi32(u[1], k32_m02_p30);
                        v[30] = k_madd_epi32(u[2], k32_m02_p30);
                        v[31] = k_madd_epi32(u[3], k32_m02_p30);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_32(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);
                        u[8] = k_packs_epi64(v[16], v[17]);
                        u[9] = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
                        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
                        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
                        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
                        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
                        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
                        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
                        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
                        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);

                        v[0] = _mm_cmplt_epi32(u[0], kZero);
                        v[1] = _mm_cmplt_epi32(u[1], kZero);
                        v[2] = _mm_cmplt_epi32(u[2], kZero);
                        v[3] = _mm_cmplt_epi32(u[3], kZero);
                        v[4] = _mm_cmplt_epi32(u[4], kZero);
                        v[5] = _mm_cmplt_epi32(u[5], kZero);
                        v[6] = _mm_cmplt_epi32(u[6], kZero);
                        v[7] = _mm_cmplt_epi32(u[7], kZero);
                        v[8] = _mm_cmplt_epi32(u[8], kZero);
                        v[9] = _mm_cmplt_epi32(u[9], kZero);
                        v[10] = _mm_cmplt_epi32(u[10], kZero);
                        v[11] = _mm_cmplt_epi32(u[11], kZero);
                        v[12] = _mm_cmplt_epi32(u[12], kZero);
                        v[13] = _mm_cmplt_epi32(u[13], kZero);
                        v[14] = _mm_cmplt_epi32(u[14], kZero);
                        v[15] = _mm_cmplt_epi32(u[15], kZero);

                        u[0] = _mm_sub_epi32(u[0], v[0]);
                        u[1] = _mm_sub_epi32(u[1], v[1]);
                        u[2] = _mm_sub_epi32(u[2], v[2]);
                        u[3] = _mm_sub_epi32(u[3], v[3]);
                        u[4] = _mm_sub_epi32(u[4], v[4]);
                        u[5] = _mm_sub_epi32(u[5], v[5]);
                        u[6] = _mm_sub_epi32(u[6], v[6]);
                        u[7] = _mm_sub_epi32(u[7], v[7]);
                        u[8] = _mm_sub_epi32(u[8], v[8]);
                        u[9] = _mm_sub_epi32(u[9], v[9]);
                        u[10] = _mm_sub_epi32(u[10], v[10]);
                        u[11] = _mm_sub_epi32(u[11], v[11]);
                        u[12] = _mm_sub_epi32(u[12], v[12]);
                        u[13] = _mm_sub_epi32(u[13], v[13]);
                        u[14] = _mm_sub_epi32(u[14], v[14]);
                        u[15] = _mm_sub_epi32(u[15], v[15]);

                        v[0] = _mm_add_epi32(u[0], K32One);
                        v[1] = _mm_add_epi32(u[1], K32One);
                        v[2] = _mm_add_epi32(u[2], K32One);
                        v[3] = _mm_add_epi32(u[3], K32One);
                        v[4] = _mm_add_epi32(u[4], K32One);
                        v[5] = _mm_add_epi32(u[5], K32One);
                        v[6] = _mm_add_epi32(u[6], K32One);
                        v[7] = _mm_add_epi32(u[7], K32One);
                        v[8] = _mm_add_epi32(u[8], K32One);
                        v[9] = _mm_add_epi32(u[9], K32One);
                        v[10] = _mm_add_epi32(u[10], K32One);
                        v[11] = _mm_add_epi32(u[11], K32One);
                        v[12] = _mm_add_epi32(u[12], K32One);
                        v[13] = _mm_add_epi32(u[13], K32One);
                        v[14] = _mm_add_epi32(u[14], K32One);
                        v[15] = _mm_add_epi32(u[15], K32One);

                        u[0] = _mm_srai_epi32(v[0], 2);
                        u[1] = _mm_srai_epi32(v[1], 2);
                        u[2] = _mm_srai_epi32(v[2], 2);
                        u[3] = _mm_srai_epi32(v[3], 2);
                        u[4] = _mm_srai_epi32(v[4], 2);
                        u[5] = _mm_srai_epi32(v[5], 2);
                        u[6] = _mm_srai_epi32(v[6], 2);
                        u[7] = _mm_srai_epi32(v[7], 2);
                        u[8] = _mm_srai_epi32(v[8], 2);
                        u[9] = _mm_srai_epi32(v[9], 2);
                        u[10] = _mm_srai_epi32(v[10], 2);
                        u[11] = _mm_srai_epi32(v[11], 2);
                        u[12] = _mm_srai_epi32(v[12], 2);
                        u[13] = _mm_srai_epi32(v[13], 2);
                        u[14] = _mm_srai_epi32(v[14], 2);
                        u[15] = _mm_srai_epi32(v[15], 2);

                        out[2] = _mm_packs_epi32(u[0], u[1]);
                        out[18] = _mm_packs_epi32(u[2], u[3]);
                        out[10] = _mm_packs_epi32(u[4], u[5]);
                        out[26] = _mm_packs_epi32(u[6], u[7]);
                        out[6] = _mm_packs_epi32(u[8], u[9]);
                        out[22] = _mm_packs_epi32(u[10], u[11]);
                        out[14] = _mm_packs_epi32(u[12], u[13]);
                        out[30] = _mm_packs_epi32(u[14], u[15]);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[2], &out[18], &out[10], &out[26],
                            &out[6], &out[22], &out[14], &out[30]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        lstep1[32] = _mm_add_epi32(lstep3[34], lstep2[32]);
                        lstep1[33] = _mm_add_epi32(lstep3[35], lstep2[33]);
                        lstep1[34] = _mm_sub_epi32(lstep2[32], lstep3[34]);
                        lstep1[35] = _mm_sub_epi32(lstep2[33], lstep3[35]);
                        lstep1[36] = _mm_sub_epi32(lstep2[38], lstep3[36]);
                        lstep1[37] = _mm_sub_epi32(lstep2[39], lstep3[37]);
                        lstep1[38] = _mm_add_epi32(lstep3[36], lstep2[38]);
                        lstep1[39] = _mm_add_epi32(lstep3[37], lstep2[39]);
                        lstep1[40] = _mm_add_epi32(lstep3[42], lstep2[40]);
                        lstep1[41] = _mm_add_epi32(lstep3[43], lstep2[41]);
                        lstep1[42] = _mm_sub_epi32(lstep2[40], lstep3[42]);
                        lstep1[43] = _mm_sub_epi32(lstep2[41], lstep3[43]);
                        lstep1[44] = _mm_sub_epi32(lstep2[46], lstep3[44]);
                        lstep1[45] = _mm_sub_epi32(lstep2[47], lstep3[45]);
                        lstep1[46] = _mm_add_epi32(lstep3[44], lstep2[46]);
                        lstep1[47] = _mm_add_epi32(lstep3[45], lstep2[47]);
                        lstep1[48] = _mm_add_epi32(lstep3[50], lstep2[48]);
                        lstep1[49] = _mm_add_epi32(lstep3[51], lstep2[49]);
                        lstep1[50] = _mm_sub_epi32(lstep2[48], lstep3[50]);
                        lstep1[51] = _mm_sub_epi32(lstep2[49], lstep3[51]);
                        lstep1[52] = _mm_sub_epi32(lstep2[54], lstep3[52]);
                        lstep1[53] = _mm_sub_epi32(lstep2[55], lstep3[53]);
                        lstep1[54] = _mm_add_epi32(lstep3[52], lstep2[54]);
                        lstep1[55] = _mm_add_epi32(lstep3[53], lstep2[55]);
                        lstep1[56] = _mm_add_epi32(lstep3[58], lstep2[56]);
                        lstep1[57] = _mm_add_epi32(lstep3[59], lstep2[57]);
                        lstep1[58] = _mm_sub_epi32(lstep2[56], lstep3[58]);
                        lstep1[59] = _mm_sub_epi32(lstep2[57], lstep3[59]);
                        lstep1[60] = _mm_sub_epi32(lstep2[62], lstep3[60]);
                        lstep1[61] = _mm_sub_epi32(lstep2[63], lstep3[61]);
                        lstep1[62] = _mm_add_epi32(lstep3[60], lstep2[62]);
                        lstep1[63] = _mm_add_epi32(lstep3[61], lstep2[63]);
                    }
                    // stage 8
                    {
                        const __m128i k32_p31_p01 = pair_set_epi32(cospi_31_64, cospi_1_64);
                        const __m128i k32_p15_p17 = pair_set_epi32(cospi_15_64, cospi_17_64);
                        const __m128i k32_p23_p09 = pair_set_epi32(cospi_23_64, cospi_9_64);
                        const __m128i k32_p07_p25 = pair_set_epi32(cospi_7_64, cospi_25_64);
                        const __m128i k32_m25_p07 = pair_set_epi32(-cospi_25_64, cospi_7_64);
                        const __m128i k32_m09_p23 = pair_set_epi32(-cospi_9_64, cospi_23_64);
                        const __m128i k32_m17_p15 = pair_set_epi32(-cospi_17_64, cospi_15_64);
                        const __m128i k32_m01_p31 = pair_set_epi32(-cospi_1_64, cospi_31_64);

                        u[0] = _mm_unpacklo_epi32(lstep1[32], lstep1[62]);
                        u[1] = _mm_unpackhi_epi32(lstep1[32], lstep1[62]);
                        u[2] = _mm_unpacklo_epi32(lstep1[33], lstep1[63]);
                        u[3] = _mm_unpackhi_epi32(lstep1[33], lstep1[63]);
                        u[4] = _mm_unpacklo_epi32(lstep1[34], lstep1[60]);
                        u[5] = _mm_unpackhi_epi32(lstep1[34], lstep1[60]);
                        u[6] = _mm_unpacklo_epi32(lstep1[35], lstep1[61]);
                        u[7] = _mm_unpackhi_epi32(lstep1[35], lstep1[61]);
                        u[8] = _mm_unpacklo_epi32(lstep1[36], lstep1[58]);
                        u[9] = _mm_unpackhi_epi32(lstep1[36], lstep1[58]);
                        u[10] = _mm_unpacklo_epi32(lstep1[37], lstep1[59]);
                        u[11] = _mm_unpackhi_epi32(lstep1[37], lstep1[59]);
                        u[12] = _mm_unpacklo_epi32(lstep1[38], lstep1[56]);
                        u[13] = _mm_unpackhi_epi32(lstep1[38], lstep1[56]);
                        u[14] = _mm_unpacklo_epi32(lstep1[39], lstep1[57]);
                        u[15] = _mm_unpackhi_epi32(lstep1[39], lstep1[57]);

                        v[0] = k_madd_epi32(u[0], k32_p31_p01);
                        v[1] = k_madd_epi32(u[1], k32_p31_p01);
                        v[2] = k_madd_epi32(u[2], k32_p31_p01);
                        v[3] = k_madd_epi32(u[3], k32_p31_p01);
                        v[4] = k_madd_epi32(u[4], k32_p15_p17);
                        v[5] = k_madd_epi32(u[5], k32_p15_p17);
                        v[6] = k_madd_epi32(u[6], k32_p15_p17);
                        v[7] = k_madd_epi32(u[7], k32_p15_p17);
                        v[8] = k_madd_epi32(u[8], k32_p23_p09);
                        v[9] = k_madd_epi32(u[9], k32_p23_p09);
                        v[10] = k_madd_epi32(u[10], k32_p23_p09);
                        v[11] = k_madd_epi32(u[11], k32_p23_p09);
                        v[12] = k_madd_epi32(u[12], k32_p07_p25);
                        v[13] = k_madd_epi32(u[13], k32_p07_p25);
                        v[14] = k_madd_epi32(u[14], k32_p07_p25);
                        v[15] = k_madd_epi32(u[15], k32_p07_p25);
                        v[16] = k_madd_epi32(u[12], k32_m25_p07);
                        v[17] = k_madd_epi32(u[13], k32_m25_p07);
                        v[18] = k_madd_epi32(u[14], k32_m25_p07);
                        v[19] = k_madd_epi32(u[15], k32_m25_p07);
                        v[20] = k_madd_epi32(u[8], k32_m09_p23);
                        v[21] = k_madd_epi32(u[9], k32_m09_p23);
                        v[22] = k_madd_epi32(u[10], k32_m09_p23);
                        v[23] = k_madd_epi32(u[11], k32_m09_p23);
                        v[24] = k_madd_epi32(u[4], k32_m17_p15);
                        v[25] = k_madd_epi32(u[5], k32_m17_p15);
                        v[26] = k_madd_epi32(u[6], k32_m17_p15);
                        v[27] = k_madd_epi32(u[7], k32_m17_p15);
                        v[28] = k_madd_epi32(u[0], k32_m01_p31);
                        v[29] = k_madd_epi32(u[1], k32_m01_p31);
                        v[30] = k_madd_epi32(u[2], k32_m01_p31);
                        v[31] = k_madd_epi32(u[3], k32_m01_p31);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_32(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);
                        u[8] = k_packs_epi64(v[16], v[17]);
                        u[9] = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
                        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
                        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
                        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
                        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
                        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
                        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
                        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
                        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);

                        v[0] = _mm_cmplt_epi32(u[0], kZero);
                        v[1] = _mm_cmplt_epi32(u[1], kZero);
                        v[2] = _mm_cmplt_epi32(u[2], kZero);
                        v[3] = _mm_cmplt_epi32(u[3], kZero);
                        v[4] = _mm_cmplt_epi32(u[4], kZero);
                        v[5] = _mm_cmplt_epi32(u[5], kZero);
                        v[6] = _mm_cmplt_epi32(u[6], kZero);
                        v[7] = _mm_cmplt_epi32(u[7], kZero);
                        v[8] = _mm_cmplt_epi32(u[8], kZero);
                        v[9] = _mm_cmplt_epi32(u[9], kZero);
                        v[10] = _mm_cmplt_epi32(u[10], kZero);
                        v[11] = _mm_cmplt_epi32(u[11], kZero);
                        v[12] = _mm_cmplt_epi32(u[12], kZero);
                        v[13] = _mm_cmplt_epi32(u[13], kZero);
                        v[14] = _mm_cmplt_epi32(u[14], kZero);
                        v[15] = _mm_cmplt_epi32(u[15], kZero);

                        u[0] = _mm_sub_epi32(u[0], v[0]);
                        u[1] = _mm_sub_epi32(u[1], v[1]);
                        u[2] = _mm_sub_epi32(u[2], v[2]);
                        u[3] = _mm_sub_epi32(u[3], v[3]);
                        u[4] = _mm_sub_epi32(u[4], v[4]);
                        u[5] = _mm_sub_epi32(u[5], v[5]);
                        u[6] = _mm_sub_epi32(u[6], v[6]);
                        u[7] = _mm_sub_epi32(u[7], v[7]);
                        u[8] = _mm_sub_epi32(u[8], v[8]);
                        u[9] = _mm_sub_epi32(u[9], v[9]);
                        u[10] = _mm_sub_epi32(u[10], v[10]);
                        u[11] = _mm_sub_epi32(u[11], v[11]);
                        u[12] = _mm_sub_epi32(u[12], v[12]);
                        u[13] = _mm_sub_epi32(u[13], v[13]);
                        u[14] = _mm_sub_epi32(u[14], v[14]);
                        u[15] = _mm_sub_epi32(u[15], v[15]);

                        v[0] = _mm_add_epi32(u[0], K32One);
                        v[1] = _mm_add_epi32(u[1], K32One);
                        v[2] = _mm_add_epi32(u[2], K32One);
                        v[3] = _mm_add_epi32(u[3], K32One);
                        v[4] = _mm_add_epi32(u[4], K32One);
                        v[5] = _mm_add_epi32(u[5], K32One);
                        v[6] = _mm_add_epi32(u[6], K32One);
                        v[7] = _mm_add_epi32(u[7], K32One);
                        v[8] = _mm_add_epi32(u[8], K32One);
                        v[9] = _mm_add_epi32(u[9], K32One);
                        v[10] = _mm_add_epi32(u[10], K32One);
                        v[11] = _mm_add_epi32(u[11], K32One);
                        v[12] = _mm_add_epi32(u[12], K32One);
                        v[13] = _mm_add_epi32(u[13], K32One);
                        v[14] = _mm_add_epi32(u[14], K32One);
                        v[15] = _mm_add_epi32(u[15], K32One);

                        u[0] = _mm_srai_epi32(v[0], 2);
                        u[1] = _mm_srai_epi32(v[1], 2);
                        u[2] = _mm_srai_epi32(v[2], 2);
                        u[3] = _mm_srai_epi32(v[3], 2);
                        u[4] = _mm_srai_epi32(v[4], 2);
                        u[5] = _mm_srai_epi32(v[5], 2);
                        u[6] = _mm_srai_epi32(v[6], 2);
                        u[7] = _mm_srai_epi32(v[7], 2);
                        u[8] = _mm_srai_epi32(v[8], 2);
                        u[9] = _mm_srai_epi32(v[9], 2);
                        u[10] = _mm_srai_epi32(v[10], 2);
                        u[11] = _mm_srai_epi32(v[11], 2);
                        u[12] = _mm_srai_epi32(v[12], 2);
                        u[13] = _mm_srai_epi32(v[13], 2);
                        u[14] = _mm_srai_epi32(v[14], 2);
                        u[15] = _mm_srai_epi32(v[15], 2);

                        out[1] = _mm_packs_epi32(u[0], u[1]);
                        out[17] = _mm_packs_epi32(u[2], u[3]);
                        out[9] = _mm_packs_epi32(u[4], u[5]);
                        out[25] = _mm_packs_epi32(u[6], u[7]);
                        out[7] = _mm_packs_epi32(u[8], u[9]);
                        out[23] = _mm_packs_epi32(u[10], u[11]);
                        out[15] = _mm_packs_epi32(u[12], u[13]);
                        out[31] = _mm_packs_epi32(u[14], u[15]);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[1], &out[17], &out[9], &out[25],
                            &out[7], &out[23], &out[15], &out[31]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i k32_p27_p05 = pair_set_epi32(cospi_27_64, cospi_5_64);
                        const __m128i k32_p11_p21 = pair_set_epi32(cospi_11_64, cospi_21_64);
                        const __m128i k32_p19_p13 = pair_set_epi32(cospi_19_64, cospi_13_64);
                        const __m128i k32_p03_p29 = pair_set_epi32(cospi_3_64, cospi_29_64);
                        const __m128i k32_m29_p03 = pair_set_epi32(-cospi_29_64, cospi_3_64);
                        const __m128i k32_m13_p19 = pair_set_epi32(-cospi_13_64, cospi_19_64);
                        const __m128i k32_m21_p11 = pair_set_epi32(-cospi_21_64, cospi_11_64);
                        const __m128i k32_m05_p27 = pair_set_epi32(-cospi_5_64, cospi_27_64);

                        u[0] = _mm_unpacklo_epi32(lstep1[40], lstep1[54]);
                        u[1] = _mm_unpackhi_epi32(lstep1[40], lstep1[54]);
                        u[2] = _mm_unpacklo_epi32(lstep1[41], lstep1[55]);
                        u[3] = _mm_unpackhi_epi32(lstep1[41], lstep1[55]);
                        u[4] = _mm_unpacklo_epi32(lstep1[42], lstep1[52]);
                        u[5] = _mm_unpackhi_epi32(lstep1[42], lstep1[52]);
                        u[6] = _mm_unpacklo_epi32(lstep1[43], lstep1[53]);
                        u[7] = _mm_unpackhi_epi32(lstep1[43], lstep1[53]);
                        u[8] = _mm_unpacklo_epi32(lstep1[44], lstep1[50]);
                        u[9] = _mm_unpackhi_epi32(lstep1[44], lstep1[50]);
                        u[10] = _mm_unpacklo_epi32(lstep1[45], lstep1[51]);
                        u[11] = _mm_unpackhi_epi32(lstep1[45], lstep1[51]);
                        u[12] = _mm_unpacklo_epi32(lstep1[46], lstep1[48]);
                        u[13] = _mm_unpackhi_epi32(lstep1[46], lstep1[48]);
                        u[14] = _mm_unpacklo_epi32(lstep1[47], lstep1[49]);
                        u[15] = _mm_unpackhi_epi32(lstep1[47], lstep1[49]);

                        v[0] = k_madd_epi32(u[0], k32_p27_p05);
                        v[1] = k_madd_epi32(u[1], k32_p27_p05);
                        v[2] = k_madd_epi32(u[2], k32_p27_p05);
                        v[3] = k_madd_epi32(u[3], k32_p27_p05);
                        v[4] = k_madd_epi32(u[4], k32_p11_p21);
                        v[5] = k_madd_epi32(u[5], k32_p11_p21);
                        v[6] = k_madd_epi32(u[6], k32_p11_p21);
                        v[7] = k_madd_epi32(u[7], k32_p11_p21);
                        v[8] = k_madd_epi32(u[8], k32_p19_p13);
                        v[9] = k_madd_epi32(u[9], k32_p19_p13);
                        v[10] = k_madd_epi32(u[10], k32_p19_p13);
                        v[11] = k_madd_epi32(u[11], k32_p19_p13);
                        v[12] = k_madd_epi32(u[12], k32_p03_p29);
                        v[13] = k_madd_epi32(u[13], k32_p03_p29);
                        v[14] = k_madd_epi32(u[14], k32_p03_p29);
                        v[15] = k_madd_epi32(u[15], k32_p03_p29);
                        v[16] = k_madd_epi32(u[12], k32_m29_p03);
                        v[17] = k_madd_epi32(u[13], k32_m29_p03);
                        v[18] = k_madd_epi32(u[14], k32_m29_p03);
                        v[19] = k_madd_epi32(u[15], k32_m29_p03);
                        v[20] = k_madd_epi32(u[8], k32_m13_p19);
                        v[21] = k_madd_epi32(u[9], k32_m13_p19);
                        v[22] = k_madd_epi32(u[10], k32_m13_p19);
                        v[23] = k_madd_epi32(u[11], k32_m13_p19);
                        v[24] = k_madd_epi32(u[4], k32_m21_p11);
                        v[25] = k_madd_epi32(u[5], k32_m21_p11);
                        v[26] = k_madd_epi32(u[6], k32_m21_p11);
                        v[27] = k_madd_epi32(u[7], k32_m21_p11);
                        v[28] = k_madd_epi32(u[0], k32_m05_p27);
                        v[29] = k_madd_epi32(u[1], k32_m05_p27);
                        v[30] = k_madd_epi32(u[2], k32_m05_p27);
                        v[31] = k_madd_epi32(u[3], k32_m05_p27);

#if DCT_HIGH_BIT_DEPTH
                        overflow = k_check_epi32_overflow_32(
                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                        u[0] = k_packs_epi64(v[0], v[1]);
                        u[1] = k_packs_epi64(v[2], v[3]);
                        u[2] = k_packs_epi64(v[4], v[5]);
                        u[3] = k_packs_epi64(v[6], v[7]);
                        u[4] = k_packs_epi64(v[8], v[9]);
                        u[5] = k_packs_epi64(v[10], v[11]);
                        u[6] = k_packs_epi64(v[12], v[13]);
                        u[7] = k_packs_epi64(v[14], v[15]);
                        u[8] = k_packs_epi64(v[16], v[17]);
                        u[9] = k_packs_epi64(v[18], v[19]);
                        u[10] = k_packs_epi64(v[20], v[21]);
                        u[11] = k_packs_epi64(v[22], v[23]);
                        u[12] = k_packs_epi64(v[24], v[25]);
                        u[13] = k_packs_epi64(v[26], v[27]);
                        u[14] = k_packs_epi64(v[28], v[29]);
                        u[15] = k_packs_epi64(v[30], v[31]);

                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);

                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
                        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
                        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
                        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
                        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
                        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
                        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
                        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
                        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);

                        v[0] = _mm_cmplt_epi32(u[0], kZero);
                        v[1] = _mm_cmplt_epi32(u[1], kZero);
                        v[2] = _mm_cmplt_epi32(u[2], kZero);
                        v[3] = _mm_cmplt_epi32(u[3], kZero);
                        v[4] = _mm_cmplt_epi32(u[4], kZero);
                        v[5] = _mm_cmplt_epi32(u[5], kZero);
                        v[6] = _mm_cmplt_epi32(u[6], kZero);
                        v[7] = _mm_cmplt_epi32(u[7], kZero);
                        v[8] = _mm_cmplt_epi32(u[8], kZero);
                        v[9] = _mm_cmplt_epi32(u[9], kZero);
                        v[10] = _mm_cmplt_epi32(u[10], kZero);
                        v[11] = _mm_cmplt_epi32(u[11], kZero);
                        v[12] = _mm_cmplt_epi32(u[12], kZero);
                        v[13] = _mm_cmplt_epi32(u[13], kZero);
                        v[14] = _mm_cmplt_epi32(u[14], kZero);
                        v[15] = _mm_cmplt_epi32(u[15], kZero);

                        u[0] = _mm_sub_epi32(u[0], v[0]);
                        u[1] = _mm_sub_epi32(u[1], v[1]);
                        u[2] = _mm_sub_epi32(u[2], v[2]);
                        u[3] = _mm_sub_epi32(u[3], v[3]);
                        u[4] = _mm_sub_epi32(u[4], v[4]);
                        u[5] = _mm_sub_epi32(u[5], v[5]);
                        u[6] = _mm_sub_epi32(u[6], v[6]);
                        u[7] = _mm_sub_epi32(u[7], v[7]);
                        u[8] = _mm_sub_epi32(u[8], v[8]);
                        u[9] = _mm_sub_epi32(u[9], v[9]);
                        u[10] = _mm_sub_epi32(u[10], v[10]);
                        u[11] = _mm_sub_epi32(u[11], v[11]);
                        u[12] = _mm_sub_epi32(u[12], v[12]);
                        u[13] = _mm_sub_epi32(u[13], v[13]);
                        u[14] = _mm_sub_epi32(u[14], v[14]);
                        u[15] = _mm_sub_epi32(u[15], v[15]);

                        v[0] = _mm_add_epi32(u[0], K32One);
                        v[1] = _mm_add_epi32(u[1], K32One);
                        v[2] = _mm_add_epi32(u[2], K32One);
                        v[3] = _mm_add_epi32(u[3], K32One);
                        v[4] = _mm_add_epi32(u[4], K32One);
                        v[5] = _mm_add_epi32(u[5], K32One);
                        v[6] = _mm_add_epi32(u[6], K32One);
                        v[7] = _mm_add_epi32(u[7], K32One);
                        v[8] = _mm_add_epi32(u[8], K32One);
                        v[9] = _mm_add_epi32(u[9], K32One);
                        v[10] = _mm_add_epi32(u[10], K32One);
                        v[11] = _mm_add_epi32(u[11], K32One);
                        v[12] = _mm_add_epi32(u[12], K32One);
                        v[13] = _mm_add_epi32(u[13], K32One);
                        v[14] = _mm_add_epi32(u[14], K32One);
                        v[15] = _mm_add_epi32(u[15], K32One);

                        u[0] = _mm_srai_epi32(v[0], 2);
                        u[1] = _mm_srai_epi32(v[1], 2);
                        u[2] = _mm_srai_epi32(v[2], 2);
                        u[3] = _mm_srai_epi32(v[3], 2);
                        u[4] = _mm_srai_epi32(v[4], 2);
                        u[5] = _mm_srai_epi32(v[5], 2);
                        u[6] = _mm_srai_epi32(v[6], 2);
                        u[7] = _mm_srai_epi32(v[7], 2);
                        u[8] = _mm_srai_epi32(v[8], 2);
                        u[9] = _mm_srai_epi32(v[9], 2);
                        u[10] = _mm_srai_epi32(v[10], 2);
                        u[11] = _mm_srai_epi32(v[11], 2);
                        u[12] = _mm_srai_epi32(v[12], 2);
                        u[13] = _mm_srai_epi32(v[13], 2);
                        u[14] = _mm_srai_epi32(v[14], 2);
                        u[15] = _mm_srai_epi32(v[15], 2);

                        out[5] = _mm_packs_epi32(u[0], u[1]);
                        out[21] = _mm_packs_epi32(u[2], u[3]);
                        out[13] = _mm_packs_epi32(u[4], u[5]);
                        out[29] = _mm_packs_epi32(u[6], u[7]);
                        out[3] = _mm_packs_epi32(u[8], u[9]);
                        out[19] = _mm_packs_epi32(u[10], u[11]);
                        out[11] = _mm_packs_epi32(u[12], u[13]);
                        out[27] = _mm_packs_epi32(u[14], u[15]);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[5], &out[21], &out[13], &out[29],
                            &out[3], &out[19], &out[11], &out[27]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                }
#endif  // FDCT32x32_HIGH_PRECISION
                // Transpose the results, do it as four 8x8 transposes.
                {
                    int transpose_block;
                    int16_t *output0 = &intermediate[column_start * 32];
                    tran_low_t *output1 = &output_org[column_start * 32];
                    for (transpose_block = 0; transpose_block < 4; ++transpose_block) {
                        __m128i *this_out = &out[8 * transpose_block];
                        // 00 01 02 03 04 05 06 07
                        // 10 11 12 13 14 15 16 17
                        // 20 21 22 23 24 25 26 27
                        // 30 31 32 33 34 35 36 37
                        // 40 41 42 43 44 45 46 47
                        // 50 51 52 53 54 55 56 57
                        // 60 61 62 63 64 65 66 67
                        // 70 71 72 73 74 75 76 77
                        const __m128i tr0_0 = _mm_unpacklo_epi16(this_out[0], this_out[1]);
                        const __m128i tr0_1 = _mm_unpacklo_epi16(this_out[2], this_out[3]);
                        const __m128i tr0_2 = _mm_unpackhi_epi16(this_out[0], this_out[1]);
                        const __m128i tr0_3 = _mm_unpackhi_epi16(this_out[2], this_out[3]);
                        const __m128i tr0_4 = _mm_unpacklo_epi16(this_out[4], this_out[5]);
                        const __m128i tr0_5 = _mm_unpacklo_epi16(this_out[6], this_out[7]);
                        const __m128i tr0_6 = _mm_unpackhi_epi16(this_out[4], this_out[5]);
                        const __m128i tr0_7 = _mm_unpackhi_epi16(this_out[6], this_out[7]);
                        // 00 10 01 11 02 12 03 13
                        // 20 30 21 31 22 32 23 33
                        // 04 14 05 15 06 16 07 17
                        // 24 34 25 35 26 36 27 37
                        // 40 50 41 51 42 52 43 53
                        // 60 70 61 71 62 72 63 73
                        // 54 54 55 55 56 56 57 57
                        // 64 74 65 75 66 76 67 77
                        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
                        const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_2, tr0_3);
                        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
                        const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3);
                        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_4, tr0_5);
                        const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
                        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_4, tr0_5);
                        const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
                        // 00 10 20 30 01 11 21 31
                        // 40 50 60 70 41 51 61 71
                        // 02 12 22 32 03 13 23 33
                        // 42 52 62 72 43 53 63 73
                        // 04 14 24 34 05 15 21 36
                        // 44 54 64 74 45 55 61 76
                        // 06 16 26 36 07 17 27 37
                        // 46 56 66 76 47 57 67 77
                        __m128i tr2_0 = _mm_unpacklo_epi64(tr1_0, tr1_4);
                        __m128i tr2_1 = _mm_unpackhi_epi64(tr1_0, tr1_4);
                        __m128i tr2_2 = _mm_unpacklo_epi64(tr1_2, tr1_6);
                        __m128i tr2_3 = _mm_unpackhi_epi64(tr1_2, tr1_6);
                        __m128i tr2_4 = _mm_unpacklo_epi64(tr1_1, tr1_5);
                        __m128i tr2_5 = _mm_unpackhi_epi64(tr1_1, tr1_5);
                        __m128i tr2_6 = _mm_unpacklo_epi64(tr1_3, tr1_7);
                        __m128i tr2_7 = _mm_unpackhi_epi64(tr1_3, tr1_7);
                        // 00 10 20 30 40 50 60 70
                        // 01 11 21 31 41 51 61 71
                        // 02 12 22 32 42 52 62 72
                        // 03 13 23 33 43 53 63 73
                        // 04 14 24 34 44 54 64 74
                        // 05 15 25 35 45 55 65 75
                        // 06 16 26 36 46 56 66 76
                        // 07 17 27 37 47 57 67 77
                        if (0 == pass) {
                            // output[j] = (output[j] + 1 + (output[j] > 0)) >> 2;
                            // TODO(cd): see quality impact of only doing
                            //           output[j] = (output[j] + 1) >> 2;
                            //           which would remove the code between here ...
                            __m128i tr2_0_0 = _mm_cmpgt_epi16(tr2_0, kZero);
                            __m128i tr2_1_0 = _mm_cmpgt_epi16(tr2_1, kZero);
                            __m128i tr2_2_0 = _mm_cmpgt_epi16(tr2_2, kZero);
                            __m128i tr2_3_0 = _mm_cmpgt_epi16(tr2_3, kZero);
                            __m128i tr2_4_0 = _mm_cmpgt_epi16(tr2_4, kZero);
                            __m128i tr2_5_0 = _mm_cmpgt_epi16(tr2_5, kZero);
                            __m128i tr2_6_0 = _mm_cmpgt_epi16(tr2_6, kZero);
                            __m128i tr2_7_0 = _mm_cmpgt_epi16(tr2_7, kZero);
                            tr2_0 = _mm_sub_epi16(tr2_0, tr2_0_0);
                            tr2_1 = _mm_sub_epi16(tr2_1, tr2_1_0);
                            tr2_2 = _mm_sub_epi16(tr2_2, tr2_2_0);
                            tr2_3 = _mm_sub_epi16(tr2_3, tr2_3_0);
                            tr2_4 = _mm_sub_epi16(tr2_4, tr2_4_0);
                            tr2_5 = _mm_sub_epi16(tr2_5, tr2_5_0);
                            tr2_6 = _mm_sub_epi16(tr2_6, tr2_6_0);
                            tr2_7 = _mm_sub_epi16(tr2_7, tr2_7_0);
                            //           ... and here.
                            //           PS: also change code in vp9/encoder/vp9_dct.c
                            tr2_0 = _mm_add_epi16(tr2_0, kOne);
                            tr2_1 = _mm_add_epi16(tr2_1, kOne);
                            tr2_2 = _mm_add_epi16(tr2_2, kOne);
                            tr2_3 = _mm_add_epi16(tr2_3, kOne);
                            tr2_4 = _mm_add_epi16(tr2_4, kOne);
                            tr2_5 = _mm_add_epi16(tr2_5, kOne);
                            tr2_6 = _mm_add_epi16(tr2_6, kOne);
                            tr2_7 = _mm_add_epi16(tr2_7, kOne);
                            tr2_0 = _mm_srai_epi16(tr2_0, 2);
                            tr2_1 = _mm_srai_epi16(tr2_1, 2);
                            tr2_2 = _mm_srai_epi16(tr2_2, 2);
                            tr2_3 = _mm_srai_epi16(tr2_3, 2);
                            tr2_4 = _mm_srai_epi16(tr2_4, 2);
                            tr2_5 = _mm_srai_epi16(tr2_5, 2);
                            tr2_6 = _mm_srai_epi16(tr2_6, 2);
                            tr2_7 = _mm_srai_epi16(tr2_7, 2);
                        }
                        // Note: even though all these stores are aligned, using the aligned
                        //       intrinsic make the code slightly slower.
                        if (pass == 0) {
                            _mm_storeu_si128((__m128i *)(output0 + 0 * 32), tr2_0);
                            _mm_storeu_si128((__m128i *)(output0 + 1 * 32), tr2_1);
                            _mm_storeu_si128((__m128i *)(output0 + 2 * 32), tr2_2);
                            _mm_storeu_si128((__m128i *)(output0 + 3 * 32), tr2_3);
                            _mm_storeu_si128((__m128i *)(output0 + 4 * 32), tr2_4);
                            _mm_storeu_si128((__m128i *)(output0 + 5 * 32), tr2_5);
                            _mm_storeu_si128((__m128i *)(output0 + 6 * 32), tr2_6);
                            _mm_storeu_si128((__m128i *)(output0 + 7 * 32), tr2_7);
                            // Process next 8x8
                            output0 += 8;
                        } else {
                            storeu_output(&tr2_0, (output1 + 0 * 32));
                            storeu_output(&tr2_1, (output1 + 1 * 32));
                            storeu_output(&tr2_2, (output1 + 2 * 32));
                            storeu_output(&tr2_3, (output1 + 3 * 32));
                            storeu_output(&tr2_4, (output1 + 4 * 32));
                            storeu_output(&tr2_5, (output1 + 5 * 32));
                            storeu_output(&tr2_6, (output1 + 6 * 32));
                            storeu_output(&tr2_7, (output1 + 7 * 32));
                            // Process next 8x8
                            output1 += 8;
                        }
                    }
                }
            }
        }
    }  // NOLINT

    void fdct32x32_fast_sse2(const int16_t *input, tran_low_t *output_org, int stride)
    {
        // Calculate pre-multiplied strides
        const int str1 = stride;
        const int str2 = 2 * stride;
        const int str3 = 2 * stride + str1;
        // We need an intermediate buffer between passes.
        DECLARE_ALIGNED(16, int16_t, intermediate[32 * 32]);
        // Constants
        //    When we use them, in one case, they are all the same. In all others
        //    it's a pair of them that we need to repeat four times. This is done
        //    by constructing the 32 bit constant corresponding to that pair.
        const __m128i k__cospi_p16_p16 = _mm_set1_epi16((int16_t)cospi_16_64);
        const __m128i k__cospi_p16_m16 = pair_set_epi16(+cospi_16_64, -cospi_16_64);
        const __m128i k__cospi_m08_p24 = pair_set_epi16(-cospi_8_64, cospi_24_64);
        const __m128i k__cospi_m24_m08 = pair_set_epi16(-cospi_24_64, -cospi_8_64);
        const __m128i k__cospi_p24_p08 = pair_set_epi16(+cospi_24_64, cospi_8_64);
        const __m128i k__cospi_p12_p20 = pair_set_epi16(+cospi_12_64, cospi_20_64);
        const __m128i k__cospi_m20_p12 = pair_set_epi16(-cospi_20_64, cospi_12_64);
        const __m128i k__cospi_m04_p28 = pair_set_epi16(-cospi_4_64, cospi_28_64);
        const __m128i k__cospi_p28_p04 = pair_set_epi16(+cospi_28_64, cospi_4_64);
        const __m128i k__cospi_m28_m04 = pair_set_epi16(-cospi_28_64, -cospi_4_64);
        const __m128i k__cospi_m12_m20 = pair_set_epi16(-cospi_12_64, -cospi_20_64);
        const __m128i k__cospi_p30_p02 = pair_set_epi16(+cospi_30_64, cospi_2_64);
        const __m128i k__cospi_p14_p18 = pair_set_epi16(+cospi_14_64, cospi_18_64);
        const __m128i k__cospi_p22_p10 = pair_set_epi16(+cospi_22_64, cospi_10_64);
        const __m128i k__cospi_p06_p26 = pair_set_epi16(+cospi_6_64, cospi_26_64);
        const __m128i k__cospi_m26_p06 = pair_set_epi16(-cospi_26_64, cospi_6_64);
        const __m128i k__cospi_m10_p22 = pair_set_epi16(-cospi_10_64, cospi_22_64);
        const __m128i k__cospi_m18_p14 = pair_set_epi16(-cospi_18_64, cospi_14_64);
        const __m128i k__cospi_m02_p30 = pair_set_epi16(-cospi_2_64, cospi_30_64);
        const __m128i k__cospi_p31_p01 = pair_set_epi16(+cospi_31_64, cospi_1_64);
        const __m128i k__cospi_p15_p17 = pair_set_epi16(+cospi_15_64, cospi_17_64);
        const __m128i k__cospi_p23_p09 = pair_set_epi16(+cospi_23_64, cospi_9_64);
        const __m128i k__cospi_p07_p25 = pair_set_epi16(+cospi_7_64, cospi_25_64);
        const __m128i k__cospi_m25_p07 = pair_set_epi16(-cospi_25_64, cospi_7_64);
        const __m128i k__cospi_m09_p23 = pair_set_epi16(-cospi_9_64, cospi_23_64);
        const __m128i k__cospi_m17_p15 = pair_set_epi16(-cospi_17_64, cospi_15_64);
        const __m128i k__cospi_m01_p31 = pair_set_epi16(-cospi_1_64, cospi_31_64);
        const __m128i k__cospi_p27_p05 = pair_set_epi16(+cospi_27_64, cospi_5_64);
        const __m128i k__cospi_p11_p21 = pair_set_epi16(+cospi_11_64, cospi_21_64);
        const __m128i k__cospi_p19_p13 = pair_set_epi16(+cospi_19_64, cospi_13_64);
        const __m128i k__cospi_p03_p29 = pair_set_epi16(+cospi_3_64, cospi_29_64);
        const __m128i k__cospi_m29_p03 = pair_set_epi16(-cospi_29_64, cospi_3_64);
        const __m128i k__cospi_m13_p19 = pair_set_epi16(-cospi_13_64, cospi_19_64);
        const __m128i k__cospi_m21_p11 = pair_set_epi16(-cospi_21_64, cospi_11_64);
        const __m128i k__cospi_m05_p27 = pair_set_epi16(-cospi_5_64, cospi_27_64);
        const __m128i k__DCT_CONST_ROUNDING = _mm_set1_epi32(DCT_CONST_ROUNDING);
        const __m128i kZero = _mm_set1_epi16(0);
        const __m128i kOne = _mm_set1_epi16(1);
        // Do the two transform/transpose passes
        int pass;
#if DCT_HIGH_BIT_DEPTH
        int overflow;
#endif
        for (pass = 0; pass < 2; ++pass) {
            // We process eight columns (transposed rows in second pass) at a time.
            int column_start;
            for (column_start = 0; column_start < 32; column_start += 8) {
                __m128i step1[32];
                __m128i step2[32];
                __m128i step3[32];
                __m128i out[32];
                // Stage 1
                // Note: even though all the loads below are aligned, using the aligned
                //       intrinsic make the code slightly slower.
                if (0 == pass) {
                    const int16_t *in = &input[column_start];
                    // step1[i] =  (in[ 0 * stride] + in[(32 -  1) * stride]) << 2;
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        const int16_t *ina = in + 0 * str1;
                        const int16_t *inb = in + 31 * str1;
                        __m128i *step1a = &step1[0];
                        __m128i *step1b = &step1[31];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const int16_t *ina = in + 4 * str1;
                        const int16_t *inb = in + 27 * str1;
                        __m128i *step1a = &step1[4];
                        __m128i *step1b = &step1[27];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const int16_t *ina = in + 8 * str1;
                        const int16_t *inb = in + 23 * str1;
                        __m128i *step1a = &step1[8];
                        __m128i *step1b = &step1[23];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                    {
                        const int16_t *ina = in + 12 * str1;
                        const int16_t *inb = in + 19 * str1;
                        __m128i *step1a = &step1[12];
                        __m128i *step1b = &step1[19];
                        const __m128i ina0 = _mm_loadu_si128((const __m128i *)(ina));
                        const __m128i ina1 = _mm_loadu_si128((const __m128i *)(ina + str1));
                        const __m128i ina2 = _mm_loadu_si128((const __m128i *)(ina + str2));
                        const __m128i ina3 = _mm_loadu_si128((const __m128i *)(ina + str3));
                        const __m128i inb3 = _mm_loadu_si128((const __m128i *)(inb - str3));
                        const __m128i inb2 = _mm_loadu_si128((const __m128i *)(inb - str2));
                        const __m128i inb1 = _mm_loadu_si128((const __m128i *)(inb - str1));
                        const __m128i inb0 = _mm_loadu_si128((const __m128i *)(inb));
                        step1a[0] = _mm_add_epi16(ina0, inb0);
                        step1a[1] = _mm_add_epi16(ina1, inb1);
                        step1a[2] = _mm_add_epi16(ina2, inb2);
                        step1a[3] = _mm_add_epi16(ina3, inb3);
                        step1b[-3] = _mm_sub_epi16(ina3, inb3);
                        step1b[-2] = _mm_sub_epi16(ina2, inb2);
                        step1b[-1] = _mm_sub_epi16(ina1, inb1);
                        step1b[-0] = _mm_sub_epi16(ina0, inb0);
                        step1a[0] = _mm_slli_epi16(step1a[0], 2);
                        step1a[1] = _mm_slli_epi16(step1a[1], 2);
                        step1a[2] = _mm_slli_epi16(step1a[2], 2);
                        step1a[3] = _mm_slli_epi16(step1a[3], 2);
                        step1b[-3] = _mm_slli_epi16(step1b[-3], 2);
                        step1b[-2] = _mm_slli_epi16(step1b[-2], 2);
                        step1b[-1] = _mm_slli_epi16(step1b[-1], 2);
                        step1b[-0] = _mm_slli_epi16(step1b[-0], 2);
                    }
                } else {
                    int16_t *in = &intermediate[column_start];
                    // step1[i] =  in[ 0 * 32] + in[(32 -  1) * 32];
                    // Note: using the same approach as above to have common offset is
                    //       counter-productive as all offsets can be calculated at compile
                    //       time.
                    // Note: the next four blocks could be in a loop. That would help the
                    //       instruction cache but is actually slower.
                    {
                        __m128i in00 = _mm_loadu_si128((const __m128i *)(in + 0 * 32));
                        __m128i in01 = _mm_loadu_si128((const __m128i *)(in + 1 * 32));
                        __m128i in02 = _mm_loadu_si128((const __m128i *)(in + 2 * 32));
                        __m128i in03 = _mm_loadu_si128((const __m128i *)(in + 3 * 32));
                        __m128i in28 = _mm_loadu_si128((const __m128i *)(in + 28 * 32));
                        __m128i in29 = _mm_loadu_si128((const __m128i *)(in + 29 * 32));
                        __m128i in30 = _mm_loadu_si128((const __m128i *)(in + 30 * 32));
                        __m128i in31 = _mm_loadu_si128((const __m128i *)(in + 31 * 32));
                        step1[0] = ADD_EPI16(in00, in31);
                        step1[1] = ADD_EPI16(in01, in30);
                        step1[2] = ADD_EPI16(in02, in29);
                        step1[3] = ADD_EPI16(in03, in28);
                        step1[28] = SUB_EPI16(in03, in28);
                        step1[29] = SUB_EPI16(in02, in29);
                        step1[30] = SUB_EPI16(in01, in30);
                        step1[31] = SUB_EPI16(in00, in31);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[0], &step1[1], &step1[2],
                            &step1[3], &step1[28], &step1[29],
                            &step1[30], &step1[31]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        __m128i in04 = _mm_loadu_si128((const __m128i *)(in + 4 * 32));
                        __m128i in05 = _mm_loadu_si128((const __m128i *)(in + 5 * 32));
                        __m128i in06 = _mm_loadu_si128((const __m128i *)(in + 6 * 32));
                        __m128i in07 = _mm_loadu_si128((const __m128i *)(in + 7 * 32));
                        __m128i in24 = _mm_loadu_si128((const __m128i *)(in + 24 * 32));
                        __m128i in25 = _mm_loadu_si128((const __m128i *)(in + 25 * 32));
                        __m128i in26 = _mm_loadu_si128((const __m128i *)(in + 26 * 32));
                        __m128i in27 = _mm_loadu_si128((const __m128i *)(in + 27 * 32));
                        step1[4] = ADD_EPI16(in04, in27);
                        step1[5] = ADD_EPI16(in05, in26);
                        step1[6] = ADD_EPI16(in06, in25);
                        step1[7] = ADD_EPI16(in07, in24);
                        step1[24] = SUB_EPI16(in07, in24);
                        step1[25] = SUB_EPI16(in06, in25);
                        step1[26] = SUB_EPI16(in05, in26);
                        step1[27] = SUB_EPI16(in04, in27);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[4], &step1[5], &step1[6],
                            &step1[7], &step1[24], &step1[25],
                            &step1[26], &step1[27]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        __m128i in08 = _mm_loadu_si128((const __m128i *)(in + 8 * 32));
                        __m128i in09 = _mm_loadu_si128((const __m128i *)(in + 9 * 32));
                        __m128i in10 = _mm_loadu_si128((const __m128i *)(in + 10 * 32));
                        __m128i in11 = _mm_loadu_si128((const __m128i *)(in + 11 * 32));
                        __m128i in20 = _mm_loadu_si128((const __m128i *)(in + 20 * 32));
                        __m128i in21 = _mm_loadu_si128((const __m128i *)(in + 21 * 32));
                        __m128i in22 = _mm_loadu_si128((const __m128i *)(in + 22 * 32));
                        __m128i in23 = _mm_loadu_si128((const __m128i *)(in + 23 * 32));
                        step1[8] = ADD_EPI16(in08, in23);
                        step1[9] = ADD_EPI16(in09, in22);
                        step1[10] = ADD_EPI16(in10, in21);
                        step1[11] = ADD_EPI16(in11, in20);
                        step1[20] = SUB_EPI16(in11, in20);
                        step1[21] = SUB_EPI16(in10, in21);
                        step1[22] = SUB_EPI16(in09, in22);
                        step1[23] = SUB_EPI16(in08, in23);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[8], &step1[9], &step1[10],
                            &step1[11], &step1[20], &step1[21],
                            &step1[22], &step1[23]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        __m128i in12 = _mm_loadu_si128((const __m128i *)(in + 12 * 32));
                        __m128i in13 = _mm_loadu_si128((const __m128i *)(in + 13 * 32));
                        __m128i in14 = _mm_loadu_si128((const __m128i *)(in + 14 * 32));
                        __m128i in15 = _mm_loadu_si128((const __m128i *)(in + 15 * 32));
                        __m128i in16 = _mm_loadu_si128((const __m128i *)(in + 16 * 32));
                        __m128i in17 = _mm_loadu_si128((const __m128i *)(in + 17 * 32));
                        __m128i in18 = _mm_loadu_si128((const __m128i *)(in + 18 * 32));
                        __m128i in19 = _mm_loadu_si128((const __m128i *)(in + 19 * 32));
                        step1[12] = ADD_EPI16(in12, in19);
                        step1[13] = ADD_EPI16(in13, in18);
                        step1[14] = ADD_EPI16(in14, in17);
                        step1[15] = ADD_EPI16(in15, in16);
                        step1[16] = SUB_EPI16(in15, in16);
                        step1[17] = SUB_EPI16(in14, in17);
                        step1[18] = SUB_EPI16(in13, in18);
                        step1[19] = SUB_EPI16(in12, in19);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[12], &step1[13], &step1[14],
                            &step1[15], &step1[16], &step1[17],
                            &step1[18], &step1[19]);
                        if (overflow) {
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                }
                // Stage 2
                {
                    step2[0] = ADD_EPI16(step1[0], step1[15]);
                    step2[1] = ADD_EPI16(step1[1], step1[14]);
                    step2[2] = ADD_EPI16(step1[2], step1[13]);
                    step2[3] = ADD_EPI16(step1[3], step1[12]);
                    step2[4] = ADD_EPI16(step1[4], step1[11]);
                    step2[5] = ADD_EPI16(step1[5], step1[10]);
                    step2[6] = ADD_EPI16(step1[6], step1[9]);
                    step2[7] = ADD_EPI16(step1[7], step1[8]);
                    step2[8] = SUB_EPI16(step1[7], step1[8]);
                    step2[9] = SUB_EPI16(step1[6], step1[9]);
                    step2[10] = SUB_EPI16(step1[5], step1[10]);
                    step2[11] = SUB_EPI16(step1[4], step1[11]);
                    step2[12] = SUB_EPI16(step1[3], step1[12]);
                    step2[13] = SUB_EPI16(step1[2], step1[13]);
                    step2[14] = SUB_EPI16(step1[1], step1[14]);
                    step2[15] = SUB_EPI16(step1[0], step1[15]);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x16(
                        &step2[0], &step2[1], &step2[2], &step2[3], &step2[4], &step2[5],
                        &step2[6], &step2[7], &step2[8], &step2[9], &step2[10], &step2[11],
                        &step2[12], &step2[13], &step2[14], &step2[15]);
                    if (overflow) {
                        if (pass == 0)
                            HIGH_FDCT32x32_2D_C(input, output_org, stride);
                        else
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                }
                {
                    const __m128i s2_20_0 = _mm_unpacklo_epi16(step1[27], step1[20]);
                    const __m128i s2_20_1 = _mm_unpackhi_epi16(step1[27], step1[20]);
                    const __m128i s2_21_0 = _mm_unpacklo_epi16(step1[26], step1[21]);
                    const __m128i s2_21_1 = _mm_unpackhi_epi16(step1[26], step1[21]);
                    const __m128i s2_22_0 = _mm_unpacklo_epi16(step1[25], step1[22]);
                    const __m128i s2_22_1 = _mm_unpackhi_epi16(step1[25], step1[22]);
                    const __m128i s2_23_0 = _mm_unpacklo_epi16(step1[24], step1[23]);
                    const __m128i s2_23_1 = _mm_unpackhi_epi16(step1[24], step1[23]);
                    const __m128i s2_20_2 = _mm_madd_epi16(s2_20_0, k__cospi_p16_m16);
                    const __m128i s2_20_3 = _mm_madd_epi16(s2_20_1, k__cospi_p16_m16);
                    const __m128i s2_21_2 = _mm_madd_epi16(s2_21_0, k__cospi_p16_m16);
                    const __m128i s2_21_3 = _mm_madd_epi16(s2_21_1, k__cospi_p16_m16);
                    const __m128i s2_22_2 = _mm_madd_epi16(s2_22_0, k__cospi_p16_m16);
                    const __m128i s2_22_3 = _mm_madd_epi16(s2_22_1, k__cospi_p16_m16);
                    const __m128i s2_23_2 = _mm_madd_epi16(s2_23_0, k__cospi_p16_m16);
                    const __m128i s2_23_3 = _mm_madd_epi16(s2_23_1, k__cospi_p16_m16);
                    const __m128i s2_24_2 = _mm_madd_epi16(s2_23_0, k__cospi_p16_p16);
                    const __m128i s2_24_3 = _mm_madd_epi16(s2_23_1, k__cospi_p16_p16);
                    const __m128i s2_25_2 = _mm_madd_epi16(s2_22_0, k__cospi_p16_p16);
                    const __m128i s2_25_3 = _mm_madd_epi16(s2_22_1, k__cospi_p16_p16);
                    const __m128i s2_26_2 = _mm_madd_epi16(s2_21_0, k__cospi_p16_p16);
                    const __m128i s2_26_3 = _mm_madd_epi16(s2_21_1, k__cospi_p16_p16);
                    const __m128i s2_27_2 = _mm_madd_epi16(s2_20_0, k__cospi_p16_p16);
                    const __m128i s2_27_3 = _mm_madd_epi16(s2_20_1, k__cospi_p16_p16);
                    // dct_const_round_shift
                    const __m128i s2_20_4 = _mm_add_epi32(s2_20_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_20_5 = _mm_add_epi32(s2_20_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_21_4 = _mm_add_epi32(s2_21_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_21_5 = _mm_add_epi32(s2_21_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_22_4 = _mm_add_epi32(s2_22_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_22_5 = _mm_add_epi32(s2_22_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_23_4 = _mm_add_epi32(s2_23_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_23_5 = _mm_add_epi32(s2_23_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_24_4 = _mm_add_epi32(s2_24_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_24_5 = _mm_add_epi32(s2_24_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_25_4 = _mm_add_epi32(s2_25_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_25_5 = _mm_add_epi32(s2_25_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_26_4 = _mm_add_epi32(s2_26_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_26_5 = _mm_add_epi32(s2_26_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_27_4 = _mm_add_epi32(s2_27_2, k__DCT_CONST_ROUNDING);
                    const __m128i s2_27_5 = _mm_add_epi32(s2_27_3, k__DCT_CONST_ROUNDING);
                    const __m128i s2_20_6 = _mm_srai_epi32(s2_20_4, DCT_CONST_BITS);
                    const __m128i s2_20_7 = _mm_srai_epi32(s2_20_5, DCT_CONST_BITS);
                    const __m128i s2_21_6 = _mm_srai_epi32(s2_21_4, DCT_CONST_BITS);
                    const __m128i s2_21_7 = _mm_srai_epi32(s2_21_5, DCT_CONST_BITS);
                    const __m128i s2_22_6 = _mm_srai_epi32(s2_22_4, DCT_CONST_BITS);
                    const __m128i s2_22_7 = _mm_srai_epi32(s2_22_5, DCT_CONST_BITS);
                    const __m128i s2_23_6 = _mm_srai_epi32(s2_23_4, DCT_CONST_BITS);
                    const __m128i s2_23_7 = _mm_srai_epi32(s2_23_5, DCT_CONST_BITS);
                    const __m128i s2_24_6 = _mm_srai_epi32(s2_24_4, DCT_CONST_BITS);
                    const __m128i s2_24_7 = _mm_srai_epi32(s2_24_5, DCT_CONST_BITS);
                    const __m128i s2_25_6 = _mm_srai_epi32(s2_25_4, DCT_CONST_BITS);
                    const __m128i s2_25_7 = _mm_srai_epi32(s2_25_5, DCT_CONST_BITS);
                    const __m128i s2_26_6 = _mm_srai_epi32(s2_26_4, DCT_CONST_BITS);
                    const __m128i s2_26_7 = _mm_srai_epi32(s2_26_5, DCT_CONST_BITS);
                    const __m128i s2_27_6 = _mm_srai_epi32(s2_27_4, DCT_CONST_BITS);
                    const __m128i s2_27_7 = _mm_srai_epi32(s2_27_5, DCT_CONST_BITS);
                    // Combine
                    step2[20] = _mm_packs_epi32(s2_20_6, s2_20_7);
                    step2[21] = _mm_packs_epi32(s2_21_6, s2_21_7);
                    step2[22] = _mm_packs_epi32(s2_22_6, s2_22_7);
                    step2[23] = _mm_packs_epi32(s2_23_6, s2_23_7);
                    step2[24] = _mm_packs_epi32(s2_24_6, s2_24_7);
                    step2[25] = _mm_packs_epi32(s2_25_6, s2_25_7);
                    step2[26] = _mm_packs_epi32(s2_26_6, s2_26_7);
                    step2[27] = _mm_packs_epi32(s2_27_6, s2_27_7);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x8(&step2[20], &step2[21], &step2[22],
                        &step2[23], &step2[24], &step2[25],
                        &step2[26], &step2[27]);
                    if (overflow) {
                        if (pass == 0)
                            HIGH_FDCT32x32_2D_C(input, output_org, stride);
                        else
                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                }

//#if !FDCT32x32_HIGH_PRECISION
                // dump the magnitude by half, hence the intermediate values are within
                // the range of 16 bits.
                if (1 == pass) {
                    __m128i s3_00_0 = _mm_cmplt_epi16(step2[0], kZero);
                    __m128i s3_01_0 = _mm_cmplt_epi16(step2[1], kZero);
                    __m128i s3_02_0 = _mm_cmplt_epi16(step2[2], kZero);
                    __m128i s3_03_0 = _mm_cmplt_epi16(step2[3], kZero);
                    __m128i s3_04_0 = _mm_cmplt_epi16(step2[4], kZero);
                    __m128i s3_05_0 = _mm_cmplt_epi16(step2[5], kZero);
                    __m128i s3_06_0 = _mm_cmplt_epi16(step2[6], kZero);
                    __m128i s3_07_0 = _mm_cmplt_epi16(step2[7], kZero);
                    __m128i s2_08_0 = _mm_cmplt_epi16(step2[8], kZero);
                    __m128i s2_09_0 = _mm_cmplt_epi16(step2[9], kZero);
                    __m128i s3_10_0 = _mm_cmplt_epi16(step2[10], kZero);
                    __m128i s3_11_0 = _mm_cmplt_epi16(step2[11], kZero);
                    __m128i s3_12_0 = _mm_cmplt_epi16(step2[12], kZero);
                    __m128i s3_13_0 = _mm_cmplt_epi16(step2[13], kZero);
                    __m128i s2_14_0 = _mm_cmplt_epi16(step2[14], kZero);
                    __m128i s2_15_0 = _mm_cmplt_epi16(step2[15], kZero);
                    __m128i s3_16_0 = _mm_cmplt_epi16(step1[16], kZero);
                    __m128i s3_17_0 = _mm_cmplt_epi16(step1[17], kZero);
                    __m128i s3_18_0 = _mm_cmplt_epi16(step1[18], kZero);
                    __m128i s3_19_0 = _mm_cmplt_epi16(step1[19], kZero);
                    __m128i s3_20_0 = _mm_cmplt_epi16(step2[20], kZero);
                    __m128i s3_21_0 = _mm_cmplt_epi16(step2[21], kZero);
                    __m128i s3_22_0 = _mm_cmplt_epi16(step2[22], kZero);
                    __m128i s3_23_0 = _mm_cmplt_epi16(step2[23], kZero);
                    __m128i s3_24_0 = _mm_cmplt_epi16(step2[24], kZero);
                    __m128i s3_25_0 = _mm_cmplt_epi16(step2[25], kZero);
                    __m128i s3_26_0 = _mm_cmplt_epi16(step2[26], kZero);
                    __m128i s3_27_0 = _mm_cmplt_epi16(step2[27], kZero);
                    __m128i s3_28_0 = _mm_cmplt_epi16(step1[28], kZero);
                    __m128i s3_29_0 = _mm_cmplt_epi16(step1[29], kZero);
                    __m128i s3_30_0 = _mm_cmplt_epi16(step1[30], kZero);
                    __m128i s3_31_0 = _mm_cmplt_epi16(step1[31], kZero);

                    step2[0] = SUB_EPI16(step2[0], s3_00_0);
                    step2[1] = SUB_EPI16(step2[1], s3_01_0);
                    step2[2] = SUB_EPI16(step2[2], s3_02_0);
                    step2[3] = SUB_EPI16(step2[3], s3_03_0);
                    step2[4] = SUB_EPI16(step2[4], s3_04_0);
                    step2[5] = SUB_EPI16(step2[5], s3_05_0);
                    step2[6] = SUB_EPI16(step2[6], s3_06_0);
                    step2[7] = SUB_EPI16(step2[7], s3_07_0);
                    step2[8] = SUB_EPI16(step2[8], s2_08_0);
                    step2[9] = SUB_EPI16(step2[9], s2_09_0);
                    step2[10] = SUB_EPI16(step2[10], s3_10_0);
                    step2[11] = SUB_EPI16(step2[11], s3_11_0);
                    step2[12] = SUB_EPI16(step2[12], s3_12_0);
                    step2[13] = SUB_EPI16(step2[13], s3_13_0);
                    step2[14] = SUB_EPI16(step2[14], s2_14_0);
                    step2[15] = SUB_EPI16(step2[15], s2_15_0);
                    step1[16] = SUB_EPI16(step1[16], s3_16_0);
                    step1[17] = SUB_EPI16(step1[17], s3_17_0);
                    step1[18] = SUB_EPI16(step1[18], s3_18_0);
                    step1[19] = SUB_EPI16(step1[19], s3_19_0);
                    step2[20] = SUB_EPI16(step2[20], s3_20_0);
                    step2[21] = SUB_EPI16(step2[21], s3_21_0);
                    step2[22] = SUB_EPI16(step2[22], s3_22_0);
                    step2[23] = SUB_EPI16(step2[23], s3_23_0);
                    step2[24] = SUB_EPI16(step2[24], s3_24_0);
                    step2[25] = SUB_EPI16(step2[25], s3_25_0);
                    step2[26] = SUB_EPI16(step2[26], s3_26_0);
                    step2[27] = SUB_EPI16(step2[27], s3_27_0);
                    step1[28] = SUB_EPI16(step1[28], s3_28_0);
                    step1[29] = SUB_EPI16(step1[29], s3_29_0);
                    step1[30] = SUB_EPI16(step1[30], s3_30_0);
                    step1[31] = SUB_EPI16(step1[31], s3_31_0);
#if DCT_HIGH_BIT_DEPTH
                    overflow = check_epi16_overflow_x32(
                        &step2[0], &step2[1], &step2[2], &step2[3], &step2[4], &step2[5],
                        &step2[6], &step2[7], &step2[8], &step2[9], &step2[10], &step2[11],
                        &step2[12], &step2[13], &step2[14], &step2[15], &step1[16],
                        &step1[17], &step1[18], &step1[19], &step2[20], &step2[21],
                        &step2[22], &step2[23], &step2[24], &step2[25], &step2[26],
                        &step2[27], &step1[28], &step1[29], &step1[30], &step1[31]);
                    if (overflow) {
                        HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                        return;
                    }
#endif  // DCT_HIGH_BIT_DEPTH
                    step2[0] = _mm_add_epi16(step2[0], kOne);
                    step2[1] = _mm_add_epi16(step2[1], kOne);
                    step2[2] = _mm_add_epi16(step2[2], kOne);
                    step2[3] = _mm_add_epi16(step2[3], kOne);
                    step2[4] = _mm_add_epi16(step2[4], kOne);
                    step2[5] = _mm_add_epi16(step2[5], kOne);
                    step2[6] = _mm_add_epi16(step2[6], kOne);
                    step2[7] = _mm_add_epi16(step2[7], kOne);
                    step2[8] = _mm_add_epi16(step2[8], kOne);
                    step2[9] = _mm_add_epi16(step2[9], kOne);
                    step2[10] = _mm_add_epi16(step2[10], kOne);
                    step2[11] = _mm_add_epi16(step2[11], kOne);
                    step2[12] = _mm_add_epi16(step2[12], kOne);
                    step2[13] = _mm_add_epi16(step2[13], kOne);
                    step2[14] = _mm_add_epi16(step2[14], kOne);
                    step2[15] = _mm_add_epi16(step2[15], kOne);
                    step1[16] = _mm_add_epi16(step1[16], kOne);
                    step1[17] = _mm_add_epi16(step1[17], kOne);
                    step1[18] = _mm_add_epi16(step1[18], kOne);
                    step1[19] = _mm_add_epi16(step1[19], kOne);
                    step2[20] = _mm_add_epi16(step2[20], kOne);
                    step2[21] = _mm_add_epi16(step2[21], kOne);
                    step2[22] = _mm_add_epi16(step2[22], kOne);
                    step2[23] = _mm_add_epi16(step2[23], kOne);
                    step2[24] = _mm_add_epi16(step2[24], kOne);
                    step2[25] = _mm_add_epi16(step2[25], kOne);
                    step2[26] = _mm_add_epi16(step2[26], kOne);
                    step2[27] = _mm_add_epi16(step2[27], kOne);
                    step1[28] = _mm_add_epi16(step1[28], kOne);
                    step1[29] = _mm_add_epi16(step1[29], kOne);
                    step1[30] = _mm_add_epi16(step1[30], kOne);
                    step1[31] = _mm_add_epi16(step1[31], kOne);

                    step2[0] = _mm_srai_epi16(step2[0], 2);
                    step2[1] = _mm_srai_epi16(step2[1], 2);
                    step2[2] = _mm_srai_epi16(step2[2], 2);
                    step2[3] = _mm_srai_epi16(step2[3], 2);
                    step2[4] = _mm_srai_epi16(step2[4], 2);
                    step2[5] = _mm_srai_epi16(step2[5], 2);
                    step2[6] = _mm_srai_epi16(step2[6], 2);
                    step2[7] = _mm_srai_epi16(step2[7], 2);
                    step2[8] = _mm_srai_epi16(step2[8], 2);
                    step2[9] = _mm_srai_epi16(step2[9], 2);
                    step2[10] = _mm_srai_epi16(step2[10], 2);
                    step2[11] = _mm_srai_epi16(step2[11], 2);
                    step2[12] = _mm_srai_epi16(step2[12], 2);
                    step2[13] = _mm_srai_epi16(step2[13], 2);
                    step2[14] = _mm_srai_epi16(step2[14], 2);
                    step2[15] = _mm_srai_epi16(step2[15], 2);
                    step1[16] = _mm_srai_epi16(step1[16], 2);
                    step1[17] = _mm_srai_epi16(step1[17], 2);
                    step1[18] = _mm_srai_epi16(step1[18], 2);
                    step1[19] = _mm_srai_epi16(step1[19], 2);
                    step2[20] = _mm_srai_epi16(step2[20], 2);
                    step2[21] = _mm_srai_epi16(step2[21], 2);
                    step2[22] = _mm_srai_epi16(step2[22], 2);
                    step2[23] = _mm_srai_epi16(step2[23], 2);
                    step2[24] = _mm_srai_epi16(step2[24], 2);
                    step2[25] = _mm_srai_epi16(step2[25], 2);
                    step2[26] = _mm_srai_epi16(step2[26], 2);
                    step2[27] = _mm_srai_epi16(step2[27], 2);
                    step1[28] = _mm_srai_epi16(step1[28], 2);
                    step1[29] = _mm_srai_epi16(step1[29], 2);
                    step1[30] = _mm_srai_epi16(step1[30], 2);
                    step1[31] = _mm_srai_epi16(step1[31], 2);
                }
//#endif  // !FDCT32x32_HIGH_PRECISION

//#if FDCT32x32_HIGH_PRECISION
//                if (pass == 0) {
//#endif
                    // Stage 3
                    {
                        step3[0] = ADD_EPI16(step2[(8 - 1)], step2[0]);
                        step3[1] = ADD_EPI16(step2[(8 - 2)], step2[1]);
                        step3[2] = ADD_EPI16(step2[(8 - 3)], step2[2]);
                        step3[3] = ADD_EPI16(step2[(8 - 4)], step2[3]);
                        step3[4] = SUB_EPI16(step2[(8 - 5)], step2[4]);
                        step3[5] = SUB_EPI16(step2[(8 - 6)], step2[5]);
                        step3[6] = SUB_EPI16(step2[(8 - 7)], step2[6]);
                        step3[7] = SUB_EPI16(step2[(8 - 8)], step2[7]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step3[0], &step3[1], &step3[2],
                            &step3[3], &step3[4], &step3[5],
                            &step3[6], &step3[7]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s3_10_0 = _mm_unpacklo_epi16(step2[13], step2[10]);
                        const __m128i s3_10_1 = _mm_unpackhi_epi16(step2[13], step2[10]);
                        const __m128i s3_11_0 = _mm_unpacklo_epi16(step2[12], step2[11]);
                        const __m128i s3_11_1 = _mm_unpackhi_epi16(step2[12], step2[11]);
                        const __m128i s3_10_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_m16);
                        const __m128i s3_10_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_m16);
                        const __m128i s3_11_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_m16);
                        const __m128i s3_11_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_m16);
                        const __m128i s3_12_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_p16);
                        const __m128i s3_12_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_p16);
                        const __m128i s3_13_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_p16);
                        const __m128i s3_13_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m128i s3_10_4 = _mm_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_10_5 = _mm_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_11_4 = _mm_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_11_5 = _mm_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_12_4 = _mm_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_12_5 = _mm_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_13_4 = _mm_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_13_5 = _mm_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_10_6 = _mm_srai_epi32(s3_10_4, DCT_CONST_BITS);
                        const __m128i s3_10_7 = _mm_srai_epi32(s3_10_5, DCT_CONST_BITS);
                        const __m128i s3_11_6 = _mm_srai_epi32(s3_11_4, DCT_CONST_BITS);
                        const __m128i s3_11_7 = _mm_srai_epi32(s3_11_5, DCT_CONST_BITS);
                        const __m128i s3_12_6 = _mm_srai_epi32(s3_12_4, DCT_CONST_BITS);
                        const __m128i s3_12_7 = _mm_srai_epi32(s3_12_5, DCT_CONST_BITS);
                        const __m128i s3_13_6 = _mm_srai_epi32(s3_13_4, DCT_CONST_BITS);
                        const __m128i s3_13_7 = _mm_srai_epi32(s3_13_5, DCT_CONST_BITS);
                        // Combine
                        step3[10] = _mm_packs_epi32(s3_10_6, s3_10_7);
                        step3[11] = _mm_packs_epi32(s3_11_6, s3_11_7);
                        step3[12] = _mm_packs_epi32(s3_12_6, s3_12_7);
                        step3[13] = _mm_packs_epi32(s3_13_6, s3_13_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&step3[10], &step3[11], &step3[12],
                            &step3[13]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step3[16] = ADD_EPI16(step2[23], step1[16]);
                        step3[17] = ADD_EPI16(step2[22], step1[17]);
                        step3[18] = ADD_EPI16(step2[21], step1[18]);
                        step3[19] = ADD_EPI16(step2[20], step1[19]);
                        step3[20] = SUB_EPI16(step1[19], step2[20]);
                        step3[21] = SUB_EPI16(step1[18], step2[21]);
                        step3[22] = SUB_EPI16(step1[17], step2[22]);
                        step3[23] = SUB_EPI16(step1[16], step2[23]);
                        step3[24] = SUB_EPI16(step1[31], step2[24]);
                        step3[25] = SUB_EPI16(step1[30], step2[25]);
                        step3[26] = SUB_EPI16(step1[29], step2[26]);
                        step3[27] = SUB_EPI16(step1[28], step2[27]);
                        step3[28] = ADD_EPI16(step2[27], step1[28]);
                        step3[29] = ADD_EPI16(step2[26], step1[29]);
                        step3[30] = ADD_EPI16(step2[25], step1[30]);
                        step3[31] = ADD_EPI16(step2[24], step1[31]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step3[16], &step3[17], &step3[18], &step3[19], &step3[20],
                            &step3[21], &step3[22], &step3[23], &step3[24], &step3[25],
                            &step3[26], &step3[27], &step3[28], &step3[29], &step3[30],
                            &step3[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }

                    // Stage 4
                    {
                        step1[0] = ADD_EPI16(step3[3], step3[0]);
                        step1[1] = ADD_EPI16(step3[2], step3[1]);
                        step1[2] = SUB_EPI16(step3[1], step3[2]);
                        step1[3] = SUB_EPI16(step3[0], step3[3]);
                        step1[8] = ADD_EPI16(step3[11], step2[8]);
                        step1[9] = ADD_EPI16(step3[10], step2[9]);
                        step1[10] = SUB_EPI16(step2[9], step3[10]);
                        step1[11] = SUB_EPI16(step2[8], step3[11]);
                        step1[12] = SUB_EPI16(step2[15], step3[12]);
                        step1[13] = SUB_EPI16(step2[14], step3[13]);
                        step1[14] = ADD_EPI16(step3[13], step2[14]);
                        step1[15] = ADD_EPI16(step3[12], step2[15]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step1[0], &step1[1], &step1[2], &step1[3], &step1[4], &step1[5],
                            &step1[6], &step1[7], &step1[8], &step1[9], &step1[10],
                            &step1[11], &step1[12], &step1[13], &step1[14], &step1[15]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s1_05_0 = _mm_unpacklo_epi16(step3[6], step3[5]);
                        const __m128i s1_05_1 = _mm_unpackhi_epi16(step3[6], step3[5]);
                        const __m128i s1_05_2 = _mm_madd_epi16(s1_05_0, k__cospi_p16_m16);
                        const __m128i s1_05_3 = _mm_madd_epi16(s1_05_1, k__cospi_p16_m16);
                        const __m128i s1_06_2 = _mm_madd_epi16(s1_05_0, k__cospi_p16_p16);
                        const __m128i s1_06_3 = _mm_madd_epi16(s1_05_1, k__cospi_p16_p16);
                        // dct_const_round_shift
                        const __m128i s1_05_4 = _mm_add_epi32(s1_05_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_05_5 = _mm_add_epi32(s1_05_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_06_4 = _mm_add_epi32(s1_06_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_06_5 = _mm_add_epi32(s1_06_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_05_6 = _mm_srai_epi32(s1_05_4, DCT_CONST_BITS);
                        const __m128i s1_05_7 = _mm_srai_epi32(s1_05_5, DCT_CONST_BITS);
                        const __m128i s1_06_6 = _mm_srai_epi32(s1_06_4, DCT_CONST_BITS);
                        const __m128i s1_06_7 = _mm_srai_epi32(s1_06_5, DCT_CONST_BITS);
                        // Combine
                        step1[5] = _mm_packs_epi32(s1_05_6, s1_05_7);
                        step1[6] = _mm_packs_epi32(s1_06_6, s1_06_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x2(&step1[5], &step1[6]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s1_18_0 = _mm_unpacklo_epi16(step3[18], step3[29]);
                        const __m128i s1_18_1 = _mm_unpackhi_epi16(step3[18], step3[29]);
                        const __m128i s1_19_0 = _mm_unpacklo_epi16(step3[19], step3[28]);
                        const __m128i s1_19_1 = _mm_unpackhi_epi16(step3[19], step3[28]);
                        const __m128i s1_20_0 = _mm_unpacklo_epi16(step3[20], step3[27]);
                        const __m128i s1_20_1 = _mm_unpackhi_epi16(step3[20], step3[27]);
                        const __m128i s1_21_0 = _mm_unpacklo_epi16(step3[21], step3[26]);
                        const __m128i s1_21_1 = _mm_unpackhi_epi16(step3[21], step3[26]);
                        const __m128i s1_18_2 = _mm_madd_epi16(s1_18_0, k__cospi_m08_p24);
                        const __m128i s1_18_3 = _mm_madd_epi16(s1_18_1, k__cospi_m08_p24);
                        const __m128i s1_19_2 = _mm_madd_epi16(s1_19_0, k__cospi_m08_p24);
                        const __m128i s1_19_3 = _mm_madd_epi16(s1_19_1, k__cospi_m08_p24);
                        const __m128i s1_20_2 = _mm_madd_epi16(s1_20_0, k__cospi_m24_m08);
                        const __m128i s1_20_3 = _mm_madd_epi16(s1_20_1, k__cospi_m24_m08);
                        const __m128i s1_21_2 = _mm_madd_epi16(s1_21_0, k__cospi_m24_m08);
                        const __m128i s1_21_3 = _mm_madd_epi16(s1_21_1, k__cospi_m24_m08);
                        const __m128i s1_26_2 = _mm_madd_epi16(s1_21_0, k__cospi_m08_p24);
                        const __m128i s1_26_3 = _mm_madd_epi16(s1_21_1, k__cospi_m08_p24);
                        const __m128i s1_27_2 = _mm_madd_epi16(s1_20_0, k__cospi_m08_p24);
                        const __m128i s1_27_3 = _mm_madd_epi16(s1_20_1, k__cospi_m08_p24);
                        const __m128i s1_28_2 = _mm_madd_epi16(s1_19_0, k__cospi_p24_p08);
                        const __m128i s1_28_3 = _mm_madd_epi16(s1_19_1, k__cospi_p24_p08);
                        const __m128i s1_29_2 = _mm_madd_epi16(s1_18_0, k__cospi_p24_p08);
                        const __m128i s1_29_3 = _mm_madd_epi16(s1_18_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m128i s1_18_4 = _mm_add_epi32(s1_18_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_18_5 = _mm_add_epi32(s1_18_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_19_4 = _mm_add_epi32(s1_19_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_19_5 = _mm_add_epi32(s1_19_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_20_4 = _mm_add_epi32(s1_20_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_20_5 = _mm_add_epi32(s1_20_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_21_4 = _mm_add_epi32(s1_21_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_21_5 = _mm_add_epi32(s1_21_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_26_4 = _mm_add_epi32(s1_26_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_26_5 = _mm_add_epi32(s1_26_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_27_4 = _mm_add_epi32(s1_27_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_27_5 = _mm_add_epi32(s1_27_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_28_4 = _mm_add_epi32(s1_28_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_28_5 = _mm_add_epi32(s1_28_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_29_4 = _mm_add_epi32(s1_29_2, k__DCT_CONST_ROUNDING);
                        const __m128i s1_29_5 = _mm_add_epi32(s1_29_3, k__DCT_CONST_ROUNDING);
                        const __m128i s1_18_6 = _mm_srai_epi32(s1_18_4, DCT_CONST_BITS);
                        const __m128i s1_18_7 = _mm_srai_epi32(s1_18_5, DCT_CONST_BITS);
                        const __m128i s1_19_6 = _mm_srai_epi32(s1_19_4, DCT_CONST_BITS);
                        const __m128i s1_19_7 = _mm_srai_epi32(s1_19_5, DCT_CONST_BITS);
                        const __m128i s1_20_6 = _mm_srai_epi32(s1_20_4, DCT_CONST_BITS);
                        const __m128i s1_20_7 = _mm_srai_epi32(s1_20_5, DCT_CONST_BITS);
                        const __m128i s1_21_6 = _mm_srai_epi32(s1_21_4, DCT_CONST_BITS);
                        const __m128i s1_21_7 = _mm_srai_epi32(s1_21_5, DCT_CONST_BITS);
                        const __m128i s1_26_6 = _mm_srai_epi32(s1_26_4, DCT_CONST_BITS);
                        const __m128i s1_26_7 = _mm_srai_epi32(s1_26_5, DCT_CONST_BITS);
                        const __m128i s1_27_6 = _mm_srai_epi32(s1_27_4, DCT_CONST_BITS);
                        const __m128i s1_27_7 = _mm_srai_epi32(s1_27_5, DCT_CONST_BITS);
                        const __m128i s1_28_6 = _mm_srai_epi32(s1_28_4, DCT_CONST_BITS);
                        const __m128i s1_28_7 = _mm_srai_epi32(s1_28_5, DCT_CONST_BITS);
                        const __m128i s1_29_6 = _mm_srai_epi32(s1_29_4, DCT_CONST_BITS);
                        const __m128i s1_29_7 = _mm_srai_epi32(s1_29_5, DCT_CONST_BITS);
                        // Combine
                        step1[18] = _mm_packs_epi32(s1_18_6, s1_18_7);
                        step1[19] = _mm_packs_epi32(s1_19_6, s1_19_7);
                        step1[20] = _mm_packs_epi32(s1_20_6, s1_20_7);
                        step1[21] = _mm_packs_epi32(s1_21_6, s1_21_7);
                        step1[26] = _mm_packs_epi32(s1_26_6, s1_26_7);
                        step1[27] = _mm_packs_epi32(s1_27_6, s1_27_7);
                        step1[28] = _mm_packs_epi32(s1_28_6, s1_28_7);
                        step1[29] = _mm_packs_epi32(s1_29_6, s1_29_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step1[18], &step1[19], &step1[20],
                            &step1[21], &step1[26], &step1[27],
                            &step1[28], &step1[29]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Stage 5
                    {
                        step2[4] = ADD_EPI16(step1[5], step3[4]);
                        step2[5] = SUB_EPI16(step3[4], step1[5]);
                        step2[6] = SUB_EPI16(step3[7], step1[6]);
                        step2[7] = ADD_EPI16(step1[6], step3[7]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&step2[4], &step2[5], &step2[6],
                            &step2[7]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i out_00_0 = _mm_unpacklo_epi16(step1[0], step1[1]);
                        const __m128i out_00_1 = _mm_unpackhi_epi16(step1[0], step1[1]);
                        const __m128i out_08_0 = _mm_unpacklo_epi16(step1[2], step1[3]);
                        const __m128i out_08_1 = _mm_unpackhi_epi16(step1[2], step1[3]);
                        const __m128i out_00_2 = _mm_madd_epi16(out_00_0, k__cospi_p16_p16);
                        const __m128i out_00_3 = _mm_madd_epi16(out_00_1, k__cospi_p16_p16);
                        const __m128i out_16_2 = _mm_madd_epi16(out_00_0, k__cospi_p16_m16);
                        const __m128i out_16_3 = _mm_madd_epi16(out_00_1, k__cospi_p16_m16);
                        const __m128i out_08_2 = _mm_madd_epi16(out_08_0, k__cospi_p24_p08);
                        const __m128i out_08_3 = _mm_madd_epi16(out_08_1, k__cospi_p24_p08);
                        const __m128i out_24_2 = _mm_madd_epi16(out_08_0, k__cospi_m08_p24);
                        const __m128i out_24_3 = _mm_madd_epi16(out_08_1, k__cospi_m08_p24);
                        // dct_const_round_shift
                        const __m128i out_00_4 =
                            _mm_add_epi32(out_00_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_00_5 =
                            _mm_add_epi32(out_00_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_16_4 =
                            _mm_add_epi32(out_16_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_16_5 =
                            _mm_add_epi32(out_16_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_08_4 =
                            _mm_add_epi32(out_08_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_08_5 =
                            _mm_add_epi32(out_08_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_24_4 =
                            _mm_add_epi32(out_24_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_24_5 =
                            _mm_add_epi32(out_24_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_00_6 = _mm_srai_epi32(out_00_4, DCT_CONST_BITS);
                        const __m128i out_00_7 = _mm_srai_epi32(out_00_5, DCT_CONST_BITS);
                        const __m128i out_16_6 = _mm_srai_epi32(out_16_4, DCT_CONST_BITS);
                        const __m128i out_16_7 = _mm_srai_epi32(out_16_5, DCT_CONST_BITS);
                        const __m128i out_08_6 = _mm_srai_epi32(out_08_4, DCT_CONST_BITS);
                        const __m128i out_08_7 = _mm_srai_epi32(out_08_5, DCT_CONST_BITS);
                        const __m128i out_24_6 = _mm_srai_epi32(out_24_4, DCT_CONST_BITS);
                        const __m128i out_24_7 = _mm_srai_epi32(out_24_5, DCT_CONST_BITS);
                        // Combine
                        out[0] = _mm_packs_epi32(out_00_6, out_00_7);
                        out[16] = _mm_packs_epi32(out_16_6, out_16_7);
                        out[8] = _mm_packs_epi32(out_08_6, out_08_7);
                        out[24] = _mm_packs_epi32(out_24_6, out_24_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&out[0], &out[16], &out[8], &out[24]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s2_09_0 = _mm_unpacklo_epi16(step1[9], step1[14]);
                        const __m128i s2_09_1 = _mm_unpackhi_epi16(step1[9], step1[14]);
                        const __m128i s2_10_0 = _mm_unpacklo_epi16(step1[10], step1[13]);
                        const __m128i s2_10_1 = _mm_unpackhi_epi16(step1[10], step1[13]);
                        const __m128i s2_09_2 = _mm_madd_epi16(s2_09_0, k__cospi_m08_p24);
                        const __m128i s2_09_3 = _mm_madd_epi16(s2_09_1, k__cospi_m08_p24);
                        const __m128i s2_10_2 = _mm_madd_epi16(s2_10_0, k__cospi_m24_m08);
                        const __m128i s2_10_3 = _mm_madd_epi16(s2_10_1, k__cospi_m24_m08);
                        const __m128i s2_13_2 = _mm_madd_epi16(s2_10_0, k__cospi_m08_p24);
                        const __m128i s2_13_3 = _mm_madd_epi16(s2_10_1, k__cospi_m08_p24);
                        const __m128i s2_14_2 = _mm_madd_epi16(s2_09_0, k__cospi_p24_p08);
                        const __m128i s2_14_3 = _mm_madd_epi16(s2_09_1, k__cospi_p24_p08);
                        // dct_const_round_shift
                        const __m128i s2_09_4 = _mm_add_epi32(s2_09_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_09_5 = _mm_add_epi32(s2_09_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_10_4 = _mm_add_epi32(s2_10_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_10_5 = _mm_add_epi32(s2_10_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_13_4 = _mm_add_epi32(s2_13_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_13_5 = _mm_add_epi32(s2_13_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_14_4 = _mm_add_epi32(s2_14_2, k__DCT_CONST_ROUNDING);
                        const __m128i s2_14_5 = _mm_add_epi32(s2_14_3, k__DCT_CONST_ROUNDING);
                        const __m128i s2_09_6 = _mm_srai_epi32(s2_09_4, DCT_CONST_BITS);
                        const __m128i s2_09_7 = _mm_srai_epi32(s2_09_5, DCT_CONST_BITS);
                        const __m128i s2_10_6 = _mm_srai_epi32(s2_10_4, DCT_CONST_BITS);
                        const __m128i s2_10_7 = _mm_srai_epi32(s2_10_5, DCT_CONST_BITS);
                        const __m128i s2_13_6 = _mm_srai_epi32(s2_13_4, DCT_CONST_BITS);
                        const __m128i s2_13_7 = _mm_srai_epi32(s2_13_5, DCT_CONST_BITS);
                        const __m128i s2_14_6 = _mm_srai_epi32(s2_14_4, DCT_CONST_BITS);
                        const __m128i s2_14_7 = _mm_srai_epi32(s2_14_5, DCT_CONST_BITS);
                        // Combine
                        step2[9] = _mm_packs_epi32(s2_09_6, s2_09_7);
                        step2[10] = _mm_packs_epi32(s2_10_6, s2_10_7);
                        step2[13] = _mm_packs_epi32(s2_13_6, s2_13_7);
                        step2[14] = _mm_packs_epi32(s2_14_6, s2_14_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x4(&step2[9], &step2[10], &step2[13],
                            &step2[14]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step2[16] = ADD_EPI16(step1[19], step3[16]);
                        step2[17] = ADD_EPI16(step1[18], step3[17]);
                        step2[18] = SUB_EPI16(step3[17], step1[18]);
                        step2[19] = SUB_EPI16(step3[16], step1[19]);
                        step2[20] = SUB_EPI16(step3[23], step1[20]);
                        step2[21] = SUB_EPI16(step3[22], step1[21]);
                        step2[22] = ADD_EPI16(step1[21], step3[22]);
                        step2[23] = ADD_EPI16(step1[20], step3[23]);
                        step2[24] = ADD_EPI16(step1[27], step3[24]);
                        step2[25] = ADD_EPI16(step1[26], step3[25]);
                        step2[26] = SUB_EPI16(step3[25], step1[26]);
                        step2[27] = SUB_EPI16(step3[24], step1[27]);
                        step2[28] = SUB_EPI16(step3[31], step1[28]);
                        step2[29] = SUB_EPI16(step3[30], step1[29]);
                        step2[30] = ADD_EPI16(step1[29], step3[30]);
                        step2[31] = ADD_EPI16(step1[28], step3[31]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step2[16], &step2[17], &step2[18], &step2[19], &step2[20],
                            &step2[21], &step2[22], &step2[23], &step2[24], &step2[25],
                            &step2[26], &step2[27], &step2[28], &step2[29], &step2[30],
                            &step2[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Stage 6
                    {
                        const __m128i out_04_0 = _mm_unpacklo_epi16(step2[4], step2[7]);
                        const __m128i out_04_1 = _mm_unpackhi_epi16(step2[4], step2[7]);
                        const __m128i out_20_0 = _mm_unpacklo_epi16(step2[5], step2[6]);
                        const __m128i out_20_1 = _mm_unpackhi_epi16(step2[5], step2[6]);
                        const __m128i out_12_0 = _mm_unpacklo_epi16(step2[5], step2[6]);
                        const __m128i out_12_1 = _mm_unpackhi_epi16(step2[5], step2[6]);
                        const __m128i out_28_0 = _mm_unpacklo_epi16(step2[4], step2[7]);
                        const __m128i out_28_1 = _mm_unpackhi_epi16(step2[4], step2[7]);
                        const __m128i out_04_2 = _mm_madd_epi16(out_04_0, k__cospi_p28_p04);
                        const __m128i out_04_3 = _mm_madd_epi16(out_04_1, k__cospi_p28_p04);
                        const __m128i out_20_2 = _mm_madd_epi16(out_20_0, k__cospi_p12_p20);
                        const __m128i out_20_3 = _mm_madd_epi16(out_20_1, k__cospi_p12_p20);
                        const __m128i out_12_2 = _mm_madd_epi16(out_12_0, k__cospi_m20_p12);
                        const __m128i out_12_3 = _mm_madd_epi16(out_12_1, k__cospi_m20_p12);
                        const __m128i out_28_2 = _mm_madd_epi16(out_28_0, k__cospi_m04_p28);
                        const __m128i out_28_3 = _mm_madd_epi16(out_28_1, k__cospi_m04_p28);
                        // dct_const_round_shift
                        const __m128i out_04_4 =
                            _mm_add_epi32(out_04_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_04_5 =
                            _mm_add_epi32(out_04_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_20_4 =
                            _mm_add_epi32(out_20_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_20_5 =
                            _mm_add_epi32(out_20_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_12_4 =
                            _mm_add_epi32(out_12_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_12_5 =
                            _mm_add_epi32(out_12_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_28_4 =
                            _mm_add_epi32(out_28_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_28_5 =
                            _mm_add_epi32(out_28_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_04_6 = _mm_srai_epi32(out_04_4, DCT_CONST_BITS);
                        const __m128i out_04_7 = _mm_srai_epi32(out_04_5, DCT_CONST_BITS);
                        const __m128i out_20_6 = _mm_srai_epi32(out_20_4, DCT_CONST_BITS);
                        const __m128i out_20_7 = _mm_srai_epi32(out_20_5, DCT_CONST_BITS);
                        const __m128i out_12_6 = _mm_srai_epi32(out_12_4, DCT_CONST_BITS);
                        const __m128i out_12_7 = _mm_srai_epi32(out_12_5, DCT_CONST_BITS);
                        const __m128i out_28_6 = _mm_srai_epi32(out_28_4, DCT_CONST_BITS);
                        const __m128i out_28_7 = _mm_srai_epi32(out_28_5, DCT_CONST_BITS);
                        // Combine
                        out[4] = _mm_packs_epi32(out_04_6, out_04_7);
                        out[20] = _mm_packs_epi32(out_20_6, out_20_7);
                        out[12] = _mm_packs_epi32(out_12_6, out_12_7);
                        out[28] = _mm_packs_epi32(out_28_6, out_28_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x4(&out[4], &out[20], &out[12], &out[28]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step3[8] = ADD_EPI16(step2[9], step1[8]);
                        step3[9] = SUB_EPI16(step1[8], step2[9]);
                        step3[10] = SUB_EPI16(step1[11], step2[10]);
                        step3[11] = ADD_EPI16(step2[10], step1[11]);
                        step3[12] = ADD_EPI16(step2[13], step1[12]);
                        step3[13] = SUB_EPI16(step1[12], step2[13]);
                        step3[14] = SUB_EPI16(step1[15], step2[14]);
                        step3[15] = ADD_EPI16(step2[14], step1[15]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step3[8], &step3[9], &step3[10],
                            &step3[11], &step3[12], &step3[13],
                            &step3[14], &step3[15]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i s3_17_0 = _mm_unpacklo_epi16(step2[17], step2[30]);
                        const __m128i s3_17_1 = _mm_unpackhi_epi16(step2[17], step2[30]);
                        const __m128i s3_18_0 = _mm_unpacklo_epi16(step2[18], step2[29]);
                        const __m128i s3_18_1 = _mm_unpackhi_epi16(step2[18], step2[29]);
                        const __m128i s3_21_0 = _mm_unpacklo_epi16(step2[21], step2[26]);
                        const __m128i s3_21_1 = _mm_unpackhi_epi16(step2[21], step2[26]);
                        const __m128i s3_22_0 = _mm_unpacklo_epi16(step2[22], step2[25]);
                        const __m128i s3_22_1 = _mm_unpackhi_epi16(step2[22], step2[25]);
                        const __m128i s3_17_2 = _mm_madd_epi16(s3_17_0, k__cospi_m04_p28);
                        const __m128i s3_17_3 = _mm_madd_epi16(s3_17_1, k__cospi_m04_p28);
                        const __m128i s3_18_2 = _mm_madd_epi16(s3_18_0, k__cospi_m28_m04);
                        const __m128i s3_18_3 = _mm_madd_epi16(s3_18_1, k__cospi_m28_m04);
                        const __m128i s3_21_2 = _mm_madd_epi16(s3_21_0, k__cospi_m20_p12);
                        const __m128i s3_21_3 = _mm_madd_epi16(s3_21_1, k__cospi_m20_p12);
                        const __m128i s3_22_2 = _mm_madd_epi16(s3_22_0, k__cospi_m12_m20);
                        const __m128i s3_22_3 = _mm_madd_epi16(s3_22_1, k__cospi_m12_m20);
                        const __m128i s3_25_2 = _mm_madd_epi16(s3_22_0, k__cospi_m20_p12);
                        const __m128i s3_25_3 = _mm_madd_epi16(s3_22_1, k__cospi_m20_p12);
                        const __m128i s3_26_2 = _mm_madd_epi16(s3_21_0, k__cospi_p12_p20);
                        const __m128i s3_26_3 = _mm_madd_epi16(s3_21_1, k__cospi_p12_p20);
                        const __m128i s3_29_2 = _mm_madd_epi16(s3_18_0, k__cospi_m04_p28);
                        const __m128i s3_29_3 = _mm_madd_epi16(s3_18_1, k__cospi_m04_p28);
                        const __m128i s3_30_2 = _mm_madd_epi16(s3_17_0, k__cospi_p28_p04);
                        const __m128i s3_30_3 = _mm_madd_epi16(s3_17_1, k__cospi_p28_p04);
                        // dct_const_round_shift
                        const __m128i s3_17_4 = _mm_add_epi32(s3_17_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_17_5 = _mm_add_epi32(s3_17_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_18_4 = _mm_add_epi32(s3_18_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_18_5 = _mm_add_epi32(s3_18_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_21_4 = _mm_add_epi32(s3_21_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_21_5 = _mm_add_epi32(s3_21_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_22_4 = _mm_add_epi32(s3_22_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_22_5 = _mm_add_epi32(s3_22_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_17_6 = _mm_srai_epi32(s3_17_4, DCT_CONST_BITS);
                        const __m128i s3_17_7 = _mm_srai_epi32(s3_17_5, DCT_CONST_BITS);
                        const __m128i s3_18_6 = _mm_srai_epi32(s3_18_4, DCT_CONST_BITS);
                        const __m128i s3_18_7 = _mm_srai_epi32(s3_18_5, DCT_CONST_BITS);
                        const __m128i s3_21_6 = _mm_srai_epi32(s3_21_4, DCT_CONST_BITS);
                        const __m128i s3_21_7 = _mm_srai_epi32(s3_21_5, DCT_CONST_BITS);
                        const __m128i s3_22_6 = _mm_srai_epi32(s3_22_4, DCT_CONST_BITS);
                        const __m128i s3_22_7 = _mm_srai_epi32(s3_22_5, DCT_CONST_BITS);
                        const __m128i s3_25_4 = _mm_add_epi32(s3_25_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_25_5 = _mm_add_epi32(s3_25_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_26_4 = _mm_add_epi32(s3_26_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_26_5 = _mm_add_epi32(s3_26_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_29_4 = _mm_add_epi32(s3_29_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_29_5 = _mm_add_epi32(s3_29_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_30_4 = _mm_add_epi32(s3_30_2, k__DCT_CONST_ROUNDING);
                        const __m128i s3_30_5 = _mm_add_epi32(s3_30_3, k__DCT_CONST_ROUNDING);
                        const __m128i s3_25_6 = _mm_srai_epi32(s3_25_4, DCT_CONST_BITS);
                        const __m128i s3_25_7 = _mm_srai_epi32(s3_25_5, DCT_CONST_BITS);
                        const __m128i s3_26_6 = _mm_srai_epi32(s3_26_4, DCT_CONST_BITS);
                        const __m128i s3_26_7 = _mm_srai_epi32(s3_26_5, DCT_CONST_BITS);
                        const __m128i s3_29_6 = _mm_srai_epi32(s3_29_4, DCT_CONST_BITS);
                        const __m128i s3_29_7 = _mm_srai_epi32(s3_29_5, DCT_CONST_BITS);
                        const __m128i s3_30_6 = _mm_srai_epi32(s3_30_4, DCT_CONST_BITS);
                        const __m128i s3_30_7 = _mm_srai_epi32(s3_30_5, DCT_CONST_BITS);
                        // Combine
                        step3[17] = _mm_packs_epi32(s3_17_6, s3_17_7);
                        step3[18] = _mm_packs_epi32(s3_18_6, s3_18_7);
                        step3[21] = _mm_packs_epi32(s3_21_6, s3_21_7);
                        step3[22] = _mm_packs_epi32(s3_22_6, s3_22_7);
                        // Combine
                        step3[25] = _mm_packs_epi32(s3_25_6, s3_25_7);
                        step3[26] = _mm_packs_epi32(s3_26_6, s3_26_7);
                        step3[29] = _mm_packs_epi32(s3_29_6, s3_29_7);
                        step3[30] = _mm_packs_epi32(s3_30_6, s3_30_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x8(&step3[17], &step3[18], &step3[21],
                            &step3[22], &step3[25], &step3[26],
                            &step3[29], &step3[30]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Stage 7
                    {
                        const __m128i out_02_0 = _mm_unpacklo_epi16(step3[8], step3[15]);
                        const __m128i out_02_1 = _mm_unpackhi_epi16(step3[8], step3[15]);
                        const __m128i out_18_0 = _mm_unpacklo_epi16(step3[9], step3[14]);
                        const __m128i out_18_1 = _mm_unpackhi_epi16(step3[9], step3[14]);
                        const __m128i out_10_0 = _mm_unpacklo_epi16(step3[10], step3[13]);
                        const __m128i out_10_1 = _mm_unpackhi_epi16(step3[10], step3[13]);
                        const __m128i out_26_0 = _mm_unpacklo_epi16(step3[11], step3[12]);
                        const __m128i out_26_1 = _mm_unpackhi_epi16(step3[11], step3[12]);
                        const __m128i out_02_2 = _mm_madd_epi16(out_02_0, k__cospi_p30_p02);
                        const __m128i out_02_3 = _mm_madd_epi16(out_02_1, k__cospi_p30_p02);
                        const __m128i out_18_2 = _mm_madd_epi16(out_18_0, k__cospi_p14_p18);
                        const __m128i out_18_3 = _mm_madd_epi16(out_18_1, k__cospi_p14_p18);
                        const __m128i out_10_2 = _mm_madd_epi16(out_10_0, k__cospi_p22_p10);
                        const __m128i out_10_3 = _mm_madd_epi16(out_10_1, k__cospi_p22_p10);
                        const __m128i out_26_2 = _mm_madd_epi16(out_26_0, k__cospi_p06_p26);
                        const __m128i out_26_3 = _mm_madd_epi16(out_26_1, k__cospi_p06_p26);
                        const __m128i out_06_2 = _mm_madd_epi16(out_26_0, k__cospi_m26_p06);
                        const __m128i out_06_3 = _mm_madd_epi16(out_26_1, k__cospi_m26_p06);
                        const __m128i out_22_2 = _mm_madd_epi16(out_10_0, k__cospi_m10_p22);
                        const __m128i out_22_3 = _mm_madd_epi16(out_10_1, k__cospi_m10_p22);
                        const __m128i out_14_2 = _mm_madd_epi16(out_18_0, k__cospi_m18_p14);
                        const __m128i out_14_3 = _mm_madd_epi16(out_18_1, k__cospi_m18_p14);
                        const __m128i out_30_2 = _mm_madd_epi16(out_02_0, k__cospi_m02_p30);
                        const __m128i out_30_3 = _mm_madd_epi16(out_02_1, k__cospi_m02_p30);
                        // dct_const_round_shift
                        const __m128i out_02_4 =
                            _mm_add_epi32(out_02_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_02_5 =
                            _mm_add_epi32(out_02_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_18_4 =
                            _mm_add_epi32(out_18_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_18_5 =
                            _mm_add_epi32(out_18_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_10_4 =
                            _mm_add_epi32(out_10_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_10_5 =
                            _mm_add_epi32(out_10_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_26_4 =
                            _mm_add_epi32(out_26_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_26_5 =
                            _mm_add_epi32(out_26_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_06_4 =
                            _mm_add_epi32(out_06_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_06_5 =
                            _mm_add_epi32(out_06_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_22_4 =
                            _mm_add_epi32(out_22_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_22_5 =
                            _mm_add_epi32(out_22_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_14_4 =
                            _mm_add_epi32(out_14_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_14_5 =
                            _mm_add_epi32(out_14_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_30_4 =
                            _mm_add_epi32(out_30_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_30_5 =
                            _mm_add_epi32(out_30_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_02_6 = _mm_srai_epi32(out_02_4, DCT_CONST_BITS);
                        const __m128i out_02_7 = _mm_srai_epi32(out_02_5, DCT_CONST_BITS);
                        const __m128i out_18_6 = _mm_srai_epi32(out_18_4, DCT_CONST_BITS);
                        const __m128i out_18_7 = _mm_srai_epi32(out_18_5, DCT_CONST_BITS);
                        const __m128i out_10_6 = _mm_srai_epi32(out_10_4, DCT_CONST_BITS);
                        const __m128i out_10_7 = _mm_srai_epi32(out_10_5, DCT_CONST_BITS);
                        const __m128i out_26_6 = _mm_srai_epi32(out_26_4, DCT_CONST_BITS);
                        const __m128i out_26_7 = _mm_srai_epi32(out_26_5, DCT_CONST_BITS);
                        const __m128i out_06_6 = _mm_srai_epi32(out_06_4, DCT_CONST_BITS);
                        const __m128i out_06_7 = _mm_srai_epi32(out_06_5, DCT_CONST_BITS);
                        const __m128i out_22_6 = _mm_srai_epi32(out_22_4, DCT_CONST_BITS);
                        const __m128i out_22_7 = _mm_srai_epi32(out_22_5, DCT_CONST_BITS);
                        const __m128i out_14_6 = _mm_srai_epi32(out_14_4, DCT_CONST_BITS);
                        const __m128i out_14_7 = _mm_srai_epi32(out_14_5, DCT_CONST_BITS);
                        const __m128i out_30_6 = _mm_srai_epi32(out_30_4, DCT_CONST_BITS);
                        const __m128i out_30_7 = _mm_srai_epi32(out_30_5, DCT_CONST_BITS);
                        // Combine
                        out[2] = _mm_packs_epi32(out_02_6, out_02_7);
                        out[18] = _mm_packs_epi32(out_18_6, out_18_7);
                        out[10] = _mm_packs_epi32(out_10_6, out_10_7);
                        out[26] = _mm_packs_epi32(out_26_6, out_26_7);
                        out[6] = _mm_packs_epi32(out_06_6, out_06_7);
                        out[22] = _mm_packs_epi32(out_22_6, out_22_7);
                        out[14] = _mm_packs_epi32(out_14_6, out_14_7);
                        out[30] = _mm_packs_epi32(out_30_6, out_30_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[2], &out[18], &out[10], &out[26],
                            &out[6], &out[22], &out[14], &out[30]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        step1[16] = ADD_EPI16(step3[17], step2[16]);
                        step1[17] = SUB_EPI16(step2[16], step3[17]);
                        step1[18] = SUB_EPI16(step2[19], step3[18]);
                        step1[19] = ADD_EPI16(step3[18], step2[19]);
                        step1[20] = ADD_EPI16(step3[21], step2[20]);
                        step1[21] = SUB_EPI16(step2[20], step3[21]);
                        step1[22] = SUB_EPI16(step2[23], step3[22]);
                        step1[23] = ADD_EPI16(step3[22], step2[23]);
                        step1[24] = ADD_EPI16(step3[25], step2[24]);
                        step1[25] = SUB_EPI16(step2[24], step3[25]);
                        step1[26] = SUB_EPI16(step2[27], step3[26]);
                        step1[27] = ADD_EPI16(step3[26], step2[27]);
                        step1[28] = ADD_EPI16(step3[29], step2[28]);
                        step1[29] = SUB_EPI16(step2[28], step3[29]);
                        step1[30] = SUB_EPI16(step2[31], step3[30]);
                        step1[31] = ADD_EPI16(step3[30], step2[31]);
#if DCT_HIGH_BIT_DEPTH
                        overflow = check_epi16_overflow_x16(
                            &step1[16], &step1[17], &step1[18], &step1[19], &step1[20],
                            &step1[21], &step1[22], &step1[23], &step1[24], &step1[25],
                            &step1[26], &step1[27], &step1[28], &step1[29], &step1[30],
                            &step1[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    // Final stage --- outputs indices are bit-reversed.
                    {
                        const __m128i out_01_0 = _mm_unpacklo_epi16(step1[16], step1[31]);
                        const __m128i out_01_1 = _mm_unpackhi_epi16(step1[16], step1[31]);
                        const __m128i out_17_0 = _mm_unpacklo_epi16(step1[17], step1[30]);
                        const __m128i out_17_1 = _mm_unpackhi_epi16(step1[17], step1[30]);
                        const __m128i out_09_0 = _mm_unpacklo_epi16(step1[18], step1[29]);
                        const __m128i out_09_1 = _mm_unpackhi_epi16(step1[18], step1[29]);
                        const __m128i out_25_0 = _mm_unpacklo_epi16(step1[19], step1[28]);
                        const __m128i out_25_1 = _mm_unpackhi_epi16(step1[19], step1[28]);
                        const __m128i out_01_2 = _mm_madd_epi16(out_01_0, k__cospi_p31_p01);
                        const __m128i out_01_3 = _mm_madd_epi16(out_01_1, k__cospi_p31_p01);
                        const __m128i out_17_2 = _mm_madd_epi16(out_17_0, k__cospi_p15_p17);
                        const __m128i out_17_3 = _mm_madd_epi16(out_17_1, k__cospi_p15_p17);
                        const __m128i out_09_2 = _mm_madd_epi16(out_09_0, k__cospi_p23_p09);
                        const __m128i out_09_3 = _mm_madd_epi16(out_09_1, k__cospi_p23_p09);
                        const __m128i out_25_2 = _mm_madd_epi16(out_25_0, k__cospi_p07_p25);
                        const __m128i out_25_3 = _mm_madd_epi16(out_25_1, k__cospi_p07_p25);
                        const __m128i out_07_2 = _mm_madd_epi16(out_25_0, k__cospi_m25_p07);
                        const __m128i out_07_3 = _mm_madd_epi16(out_25_1, k__cospi_m25_p07);
                        const __m128i out_23_2 = _mm_madd_epi16(out_09_0, k__cospi_m09_p23);
                        const __m128i out_23_3 = _mm_madd_epi16(out_09_1, k__cospi_m09_p23);
                        const __m128i out_15_2 = _mm_madd_epi16(out_17_0, k__cospi_m17_p15);
                        const __m128i out_15_3 = _mm_madd_epi16(out_17_1, k__cospi_m17_p15);
                        const __m128i out_31_2 = _mm_madd_epi16(out_01_0, k__cospi_m01_p31);
                        const __m128i out_31_3 = _mm_madd_epi16(out_01_1, k__cospi_m01_p31);
                        // dct_const_round_shift
                        const __m128i out_01_4 =
                            _mm_add_epi32(out_01_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_01_5 =
                            _mm_add_epi32(out_01_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_17_4 =
                            _mm_add_epi32(out_17_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_17_5 =
                            _mm_add_epi32(out_17_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_09_4 =
                            _mm_add_epi32(out_09_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_09_5 =
                            _mm_add_epi32(out_09_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_25_4 =
                            _mm_add_epi32(out_25_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_25_5 =
                            _mm_add_epi32(out_25_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_07_4 =
                            _mm_add_epi32(out_07_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_07_5 =
                            _mm_add_epi32(out_07_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_23_4 =
                            _mm_add_epi32(out_23_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_23_5 =
                            _mm_add_epi32(out_23_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_15_4 =
                            _mm_add_epi32(out_15_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_15_5 =
                            _mm_add_epi32(out_15_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_31_4 =
                            _mm_add_epi32(out_31_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_31_5 =
                            _mm_add_epi32(out_31_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_01_6 = _mm_srai_epi32(out_01_4, DCT_CONST_BITS);
                        const __m128i out_01_7 = _mm_srai_epi32(out_01_5, DCT_CONST_BITS);
                        const __m128i out_17_6 = _mm_srai_epi32(out_17_4, DCT_CONST_BITS);
                        const __m128i out_17_7 = _mm_srai_epi32(out_17_5, DCT_CONST_BITS);
                        const __m128i out_09_6 = _mm_srai_epi32(out_09_4, DCT_CONST_BITS);
                        const __m128i out_09_7 = _mm_srai_epi32(out_09_5, DCT_CONST_BITS);
                        const __m128i out_25_6 = _mm_srai_epi32(out_25_4, DCT_CONST_BITS);
                        const __m128i out_25_7 = _mm_srai_epi32(out_25_5, DCT_CONST_BITS);
                        const __m128i out_07_6 = _mm_srai_epi32(out_07_4, DCT_CONST_BITS);
                        const __m128i out_07_7 = _mm_srai_epi32(out_07_5, DCT_CONST_BITS);
                        const __m128i out_23_6 = _mm_srai_epi32(out_23_4, DCT_CONST_BITS);
                        const __m128i out_23_7 = _mm_srai_epi32(out_23_5, DCT_CONST_BITS);
                        const __m128i out_15_6 = _mm_srai_epi32(out_15_4, DCT_CONST_BITS);
                        const __m128i out_15_7 = _mm_srai_epi32(out_15_5, DCT_CONST_BITS);
                        const __m128i out_31_6 = _mm_srai_epi32(out_31_4, DCT_CONST_BITS);
                        const __m128i out_31_7 = _mm_srai_epi32(out_31_5, DCT_CONST_BITS);
                        // Combine
                        out[1] = _mm_packs_epi32(out_01_6, out_01_7);
                        out[17] = _mm_packs_epi32(out_17_6, out_17_7);
                        out[9] = _mm_packs_epi32(out_09_6, out_09_7);
                        out[25] = _mm_packs_epi32(out_25_6, out_25_7);
                        out[7] = _mm_packs_epi32(out_07_6, out_07_7);
                        out[23] = _mm_packs_epi32(out_23_6, out_23_7);
                        out[15] = _mm_packs_epi32(out_15_6, out_15_7);
                        out[31] = _mm_packs_epi32(out_31_6, out_31_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[1], &out[17], &out[9], &out[25],
                            &out[7], &out[23], &out[15], &out[31]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
                    {
                        const __m128i out_05_0 = _mm_unpacklo_epi16(step1[20], step1[27]);
                        const __m128i out_05_1 = _mm_unpackhi_epi16(step1[20], step1[27]);
                        const __m128i out_21_0 = _mm_unpacklo_epi16(step1[21], step1[26]);
                        const __m128i out_21_1 = _mm_unpackhi_epi16(step1[21], step1[26]);
                        const __m128i out_13_0 = _mm_unpacklo_epi16(step1[22], step1[25]);
                        const __m128i out_13_1 = _mm_unpackhi_epi16(step1[22], step1[25]);
                        const __m128i out_29_0 = _mm_unpacklo_epi16(step1[23], step1[24]);
                        const __m128i out_29_1 = _mm_unpackhi_epi16(step1[23], step1[24]);
                        const __m128i out_05_2 = _mm_madd_epi16(out_05_0, k__cospi_p27_p05);
                        const __m128i out_05_3 = _mm_madd_epi16(out_05_1, k__cospi_p27_p05);
                        const __m128i out_21_2 = _mm_madd_epi16(out_21_0, k__cospi_p11_p21);
                        const __m128i out_21_3 = _mm_madd_epi16(out_21_1, k__cospi_p11_p21);
                        const __m128i out_13_2 = _mm_madd_epi16(out_13_0, k__cospi_p19_p13);
                        const __m128i out_13_3 = _mm_madd_epi16(out_13_1, k__cospi_p19_p13);
                        const __m128i out_29_2 = _mm_madd_epi16(out_29_0, k__cospi_p03_p29);
                        const __m128i out_29_3 = _mm_madd_epi16(out_29_1, k__cospi_p03_p29);
                        const __m128i out_03_2 = _mm_madd_epi16(out_29_0, k__cospi_m29_p03);
                        const __m128i out_03_3 = _mm_madd_epi16(out_29_1, k__cospi_m29_p03);
                        const __m128i out_19_2 = _mm_madd_epi16(out_13_0, k__cospi_m13_p19);
                        const __m128i out_19_3 = _mm_madd_epi16(out_13_1, k__cospi_m13_p19);
                        const __m128i out_11_2 = _mm_madd_epi16(out_21_0, k__cospi_m21_p11);
                        const __m128i out_11_3 = _mm_madd_epi16(out_21_1, k__cospi_m21_p11);
                        const __m128i out_27_2 = _mm_madd_epi16(out_05_0, k__cospi_m05_p27);
                        const __m128i out_27_3 = _mm_madd_epi16(out_05_1, k__cospi_m05_p27);
                        // dct_const_round_shift
                        const __m128i out_05_4 =
                            _mm_add_epi32(out_05_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_05_5 =
                            _mm_add_epi32(out_05_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_21_4 =
                            _mm_add_epi32(out_21_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_21_5 =
                            _mm_add_epi32(out_21_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_13_4 =
                            _mm_add_epi32(out_13_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_13_5 =
                            _mm_add_epi32(out_13_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_29_4 =
                            _mm_add_epi32(out_29_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_29_5 =
                            _mm_add_epi32(out_29_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_03_4 =
                            _mm_add_epi32(out_03_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_03_5 =
                            _mm_add_epi32(out_03_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_19_4 =
                            _mm_add_epi32(out_19_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_19_5 =
                            _mm_add_epi32(out_19_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_11_4 =
                            _mm_add_epi32(out_11_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_11_5 =
                            _mm_add_epi32(out_11_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_27_4 =
                            _mm_add_epi32(out_27_2, k__DCT_CONST_ROUNDING);
                        const __m128i out_27_5 =
                            _mm_add_epi32(out_27_3, k__DCT_CONST_ROUNDING);
                        const __m128i out_05_6 = _mm_srai_epi32(out_05_4, DCT_CONST_BITS);
                        const __m128i out_05_7 = _mm_srai_epi32(out_05_5, DCT_CONST_BITS);
                        const __m128i out_21_6 = _mm_srai_epi32(out_21_4, DCT_CONST_BITS);
                        const __m128i out_21_7 = _mm_srai_epi32(out_21_5, DCT_CONST_BITS);
                        const __m128i out_13_6 = _mm_srai_epi32(out_13_4, DCT_CONST_BITS);
                        const __m128i out_13_7 = _mm_srai_epi32(out_13_5, DCT_CONST_BITS);
                        const __m128i out_29_6 = _mm_srai_epi32(out_29_4, DCT_CONST_BITS);
                        const __m128i out_29_7 = _mm_srai_epi32(out_29_5, DCT_CONST_BITS);
                        const __m128i out_03_6 = _mm_srai_epi32(out_03_4, DCT_CONST_BITS);
                        const __m128i out_03_7 = _mm_srai_epi32(out_03_5, DCT_CONST_BITS);
                        const __m128i out_19_6 = _mm_srai_epi32(out_19_4, DCT_CONST_BITS);
                        const __m128i out_19_7 = _mm_srai_epi32(out_19_5, DCT_CONST_BITS);
                        const __m128i out_11_6 = _mm_srai_epi32(out_11_4, DCT_CONST_BITS);
                        const __m128i out_11_7 = _mm_srai_epi32(out_11_5, DCT_CONST_BITS);
                        const __m128i out_27_6 = _mm_srai_epi32(out_27_4, DCT_CONST_BITS);
                        const __m128i out_27_7 = _mm_srai_epi32(out_27_5, DCT_CONST_BITS);
                        // Combine
                        out[5] = _mm_packs_epi32(out_05_6, out_05_7);
                        out[21] = _mm_packs_epi32(out_21_6, out_21_7);
                        out[13] = _mm_packs_epi32(out_13_6, out_13_7);
                        out[29] = _mm_packs_epi32(out_29_6, out_29_7);
                        out[3] = _mm_packs_epi32(out_03_6, out_03_7);
                        out[19] = _mm_packs_epi32(out_19_6, out_19_7);
                        out[11] = _mm_packs_epi32(out_11_6, out_11_7);
                        out[27] = _mm_packs_epi32(out_27_6, out_27_7);
#if DCT_HIGH_BIT_DEPTH
                        overflow =
                            check_epi16_overflow_x8(&out[5], &out[21], &out[13], &out[29],
                            &out[3], &out[19], &out[11], &out[27]);
                        if (overflow) {
                            if (pass == 0)
                                HIGH_FDCT32x32_2D_C(input, output_org, stride);
                            else
                                HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
                            return;
                        }
#endif  // DCT_HIGH_BIT_DEPTH
                    }
//#if FDCT32x32_HIGH_PRECISION
//                } else {
//                    __m128i lstep1[64], lstep2[64], lstep3[64];
//                    __m128i u[32], v[32], sign[16];
//                    const __m128i K32One = _mm_set_epi32(1, 1, 1, 1);
//                    // start using 32-bit operations
//                    // stage 3
//                    {
//                        // expanding to 32-bit length priori to addition operations
//                        lstep2[0] = _mm_unpacklo_epi16(step2[0], kZero);
//                        lstep2[1] = _mm_unpackhi_epi16(step2[0], kZero);
//                        lstep2[2] = _mm_unpacklo_epi16(step2[1], kZero);
//                        lstep2[3] = _mm_unpackhi_epi16(step2[1], kZero);
//                        lstep2[4] = _mm_unpacklo_epi16(step2[2], kZero);
//                        lstep2[5] = _mm_unpackhi_epi16(step2[2], kZero);
//                        lstep2[6] = _mm_unpacklo_epi16(step2[3], kZero);
//                        lstep2[7] = _mm_unpackhi_epi16(step2[3], kZero);
//                        lstep2[8] = _mm_unpacklo_epi16(step2[4], kZero);
//                        lstep2[9] = _mm_unpackhi_epi16(step2[4], kZero);
//                        lstep2[10] = _mm_unpacklo_epi16(step2[5], kZero);
//                        lstep2[11] = _mm_unpackhi_epi16(step2[5], kZero);
//                        lstep2[12] = _mm_unpacklo_epi16(step2[6], kZero);
//                        lstep2[13] = _mm_unpackhi_epi16(step2[6], kZero);
//                        lstep2[14] = _mm_unpacklo_epi16(step2[7], kZero);
//                        lstep2[15] = _mm_unpackhi_epi16(step2[7], kZero);
//                        lstep2[0] = _mm_madd_epi16(lstep2[0], kOne);
//                        lstep2[1] = _mm_madd_epi16(lstep2[1], kOne);
//                        lstep2[2] = _mm_madd_epi16(lstep2[2], kOne);
//                        lstep2[3] = _mm_madd_epi16(lstep2[3], kOne);
//                        lstep2[4] = _mm_madd_epi16(lstep2[4], kOne);
//                        lstep2[5] = _mm_madd_epi16(lstep2[5], kOne);
//                        lstep2[6] = _mm_madd_epi16(lstep2[6], kOne);
//                        lstep2[7] = _mm_madd_epi16(lstep2[7], kOne);
//                        lstep2[8] = _mm_madd_epi16(lstep2[8], kOne);
//                        lstep2[9] = _mm_madd_epi16(lstep2[9], kOne);
//                        lstep2[10] = _mm_madd_epi16(lstep2[10], kOne);
//                        lstep2[11] = _mm_madd_epi16(lstep2[11], kOne);
//                        lstep2[12] = _mm_madd_epi16(lstep2[12], kOne);
//                        lstep2[13] = _mm_madd_epi16(lstep2[13], kOne);
//                        lstep2[14] = _mm_madd_epi16(lstep2[14], kOne);
//                        lstep2[15] = _mm_madd_epi16(lstep2[15], kOne);
//
//                        lstep3[0] = _mm_add_epi32(lstep2[14], lstep2[0]);
//                        lstep3[1] = _mm_add_epi32(lstep2[15], lstep2[1]);
//                        lstep3[2] = _mm_add_epi32(lstep2[12], lstep2[2]);
//                        lstep3[3] = _mm_add_epi32(lstep2[13], lstep2[3]);
//                        lstep3[4] = _mm_add_epi32(lstep2[10], lstep2[4]);
//                        lstep3[5] = _mm_add_epi32(lstep2[11], lstep2[5]);
//                        lstep3[6] = _mm_add_epi32(lstep2[8], lstep2[6]);
//                        lstep3[7] = _mm_add_epi32(lstep2[9], lstep2[7]);
//                        lstep3[8] = _mm_sub_epi32(lstep2[6], lstep2[8]);
//                        lstep3[9] = _mm_sub_epi32(lstep2[7], lstep2[9]);
//                        lstep3[10] = _mm_sub_epi32(lstep2[4], lstep2[10]);
//                        lstep3[11] = _mm_sub_epi32(lstep2[5], lstep2[11]);
//                        lstep3[12] = _mm_sub_epi32(lstep2[2], lstep2[12]);
//                        lstep3[13] = _mm_sub_epi32(lstep2[3], lstep2[13]);
//                        lstep3[14] = _mm_sub_epi32(lstep2[0], lstep2[14]);
//                        lstep3[15] = _mm_sub_epi32(lstep2[1], lstep2[15]);
//                    }
//                    {
//                        const __m128i s3_10_0 = _mm_unpacklo_epi16(step2[13], step2[10]);
//                        const __m128i s3_10_1 = _mm_unpackhi_epi16(step2[13], step2[10]);
//                        const __m128i s3_11_0 = _mm_unpacklo_epi16(step2[12], step2[11]);
//                        const __m128i s3_11_1 = _mm_unpackhi_epi16(step2[12], step2[11]);
//                        const __m128i s3_10_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_m16);
//                        const __m128i s3_10_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_m16);
//                        const __m128i s3_11_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_m16);
//                        const __m128i s3_11_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_m16);
//                        const __m128i s3_12_2 = _mm_madd_epi16(s3_11_0, k__cospi_p16_p16);
//                        const __m128i s3_12_3 = _mm_madd_epi16(s3_11_1, k__cospi_p16_p16);
//                        const __m128i s3_13_2 = _mm_madd_epi16(s3_10_0, k__cospi_p16_p16);
//                        const __m128i s3_13_3 = _mm_madd_epi16(s3_10_1, k__cospi_p16_p16);
//                        // dct_const_round_shift
//                        const __m128i s3_10_4 = _mm_add_epi32(s3_10_2, k__DCT_CONST_ROUNDING);
//                        const __m128i s3_10_5 = _mm_add_epi32(s3_10_3, k__DCT_CONST_ROUNDING);
//                        const __m128i s3_11_4 = _mm_add_epi32(s3_11_2, k__DCT_CONST_ROUNDING);
//                        const __m128i s3_11_5 = _mm_add_epi32(s3_11_3, k__DCT_CONST_ROUNDING);
//                        const __m128i s3_12_4 = _mm_add_epi32(s3_12_2, k__DCT_CONST_ROUNDING);
//                        const __m128i s3_12_5 = _mm_add_epi32(s3_12_3, k__DCT_CONST_ROUNDING);
//                        const __m128i s3_13_4 = _mm_add_epi32(s3_13_2, k__DCT_CONST_ROUNDING);
//                        const __m128i s3_13_5 = _mm_add_epi32(s3_13_3, k__DCT_CONST_ROUNDING);
//                        lstep3[20] = _mm_srai_epi32(s3_10_4, DCT_CONST_BITS);
//                        lstep3[21] = _mm_srai_epi32(s3_10_5, DCT_CONST_BITS);
//                        lstep3[22] = _mm_srai_epi32(s3_11_4, DCT_CONST_BITS);
//                        lstep3[23] = _mm_srai_epi32(s3_11_5, DCT_CONST_BITS);
//                        lstep3[24] = _mm_srai_epi32(s3_12_4, DCT_CONST_BITS);
//                        lstep3[25] = _mm_srai_epi32(s3_12_5, DCT_CONST_BITS);
//                        lstep3[26] = _mm_srai_epi32(s3_13_4, DCT_CONST_BITS);
//                        lstep3[27] = _mm_srai_epi32(s3_13_5, DCT_CONST_BITS);
//                    }
//                    {
//                        lstep2[40] = _mm_unpacklo_epi16(step2[20], kZero);
//                        lstep2[41] = _mm_unpackhi_epi16(step2[20], kZero);
//                        lstep2[42] = _mm_unpacklo_epi16(step2[21], kZero);
//                        lstep2[43] = _mm_unpackhi_epi16(step2[21], kZero);
//                        lstep2[44] = _mm_unpacklo_epi16(step2[22], kZero);
//                        lstep2[45] = _mm_unpackhi_epi16(step2[22], kZero);
//                        lstep2[46] = _mm_unpacklo_epi16(step2[23], kZero);
//                        lstep2[47] = _mm_unpackhi_epi16(step2[23], kZero);
//                        lstep2[48] = _mm_unpacklo_epi16(step2[24], kZero);
//                        lstep2[49] = _mm_unpackhi_epi16(step2[24], kZero);
//                        lstep2[50] = _mm_unpacklo_epi16(step2[25], kZero);
//                        lstep2[51] = _mm_unpackhi_epi16(step2[25], kZero);
//                        lstep2[52] = _mm_unpacklo_epi16(step2[26], kZero);
//                        lstep2[53] = _mm_unpackhi_epi16(step2[26], kZero);
//                        lstep2[54] = _mm_unpacklo_epi16(step2[27], kZero);
//                        lstep2[55] = _mm_unpackhi_epi16(step2[27], kZero);
//                        lstep2[40] = _mm_madd_epi16(lstep2[40], kOne);
//                        lstep2[41] = _mm_madd_epi16(lstep2[41], kOne);
//                        lstep2[42] = _mm_madd_epi16(lstep2[42], kOne);
//                        lstep2[43] = _mm_madd_epi16(lstep2[43], kOne);
//                        lstep2[44] = _mm_madd_epi16(lstep2[44], kOne);
//                        lstep2[45] = _mm_madd_epi16(lstep2[45], kOne);
//                        lstep2[46] = _mm_madd_epi16(lstep2[46], kOne);
//                        lstep2[47] = _mm_madd_epi16(lstep2[47], kOne);
//                        lstep2[48] = _mm_madd_epi16(lstep2[48], kOne);
//                        lstep2[49] = _mm_madd_epi16(lstep2[49], kOne);
//                        lstep2[50] = _mm_madd_epi16(lstep2[50], kOne);
//                        lstep2[51] = _mm_madd_epi16(lstep2[51], kOne);
//                        lstep2[52] = _mm_madd_epi16(lstep2[52], kOne);
//                        lstep2[53] = _mm_madd_epi16(lstep2[53], kOne);
//                        lstep2[54] = _mm_madd_epi16(lstep2[54], kOne);
//                        lstep2[55] = _mm_madd_epi16(lstep2[55], kOne);
//
//                        lstep1[32] = _mm_unpacklo_epi16(step1[16], kZero);
//                        lstep1[33] = _mm_unpackhi_epi16(step1[16], kZero);
//                        lstep1[34] = _mm_unpacklo_epi16(step1[17], kZero);
//                        lstep1[35] = _mm_unpackhi_epi16(step1[17], kZero);
//                        lstep1[36] = _mm_unpacklo_epi16(step1[18], kZero);
//                        lstep1[37] = _mm_unpackhi_epi16(step1[18], kZero);
//                        lstep1[38] = _mm_unpacklo_epi16(step1[19], kZero);
//                        lstep1[39] = _mm_unpackhi_epi16(step1[19], kZero);
//                        lstep1[56] = _mm_unpacklo_epi16(step1[28], kZero);
//                        lstep1[57] = _mm_unpackhi_epi16(step1[28], kZero);
//                        lstep1[58] = _mm_unpacklo_epi16(step1[29], kZero);
//                        lstep1[59] = _mm_unpackhi_epi16(step1[29], kZero);
//                        lstep1[60] = _mm_unpacklo_epi16(step1[30], kZero);
//                        lstep1[61] = _mm_unpackhi_epi16(step1[30], kZero);
//                        lstep1[62] = _mm_unpacklo_epi16(step1[31], kZero);
//                        lstep1[63] = _mm_unpackhi_epi16(step1[31], kZero);
//                        lstep1[32] = _mm_madd_epi16(lstep1[32], kOne);
//                        lstep1[33] = _mm_madd_epi16(lstep1[33], kOne);
//                        lstep1[34] = _mm_madd_epi16(lstep1[34], kOne);
//                        lstep1[35] = _mm_madd_epi16(lstep1[35], kOne);
//                        lstep1[36] = _mm_madd_epi16(lstep1[36], kOne);
//                        lstep1[37] = _mm_madd_epi16(lstep1[37], kOne);
//                        lstep1[38] = _mm_madd_epi16(lstep1[38], kOne);
//                        lstep1[39] = _mm_madd_epi16(lstep1[39], kOne);
//                        lstep1[56] = _mm_madd_epi16(lstep1[56], kOne);
//                        lstep1[57] = _mm_madd_epi16(lstep1[57], kOne);
//                        lstep1[58] = _mm_madd_epi16(lstep1[58], kOne);
//                        lstep1[59] = _mm_madd_epi16(lstep1[59], kOne);
//                        lstep1[60] = _mm_madd_epi16(lstep1[60], kOne);
//                        lstep1[61] = _mm_madd_epi16(lstep1[61], kOne);
//                        lstep1[62] = _mm_madd_epi16(lstep1[62], kOne);
//                        lstep1[63] = _mm_madd_epi16(lstep1[63], kOne);
//
//                        lstep3[32] = _mm_add_epi32(lstep2[46], lstep1[32]);
//                        lstep3[33] = _mm_add_epi32(lstep2[47], lstep1[33]);
//
//                        lstep3[34] = _mm_add_epi32(lstep2[44], lstep1[34]);
//                        lstep3[35] = _mm_add_epi32(lstep2[45], lstep1[35]);
//                        lstep3[36] = _mm_add_epi32(lstep2[42], lstep1[36]);
//                        lstep3[37] = _mm_add_epi32(lstep2[43], lstep1[37]);
//                        lstep3[38] = _mm_add_epi32(lstep2[40], lstep1[38]);
//                        lstep3[39] = _mm_add_epi32(lstep2[41], lstep1[39]);
//                        lstep3[40] = _mm_sub_epi32(lstep1[38], lstep2[40]);
//                        lstep3[41] = _mm_sub_epi32(lstep1[39], lstep2[41]);
//                        lstep3[42] = _mm_sub_epi32(lstep1[36], lstep2[42]);
//                        lstep3[43] = _mm_sub_epi32(lstep1[37], lstep2[43]);
//                        lstep3[44] = _mm_sub_epi32(lstep1[34], lstep2[44]);
//                        lstep3[45] = _mm_sub_epi32(lstep1[35], lstep2[45]);
//                        lstep3[46] = _mm_sub_epi32(lstep1[32], lstep2[46]);
//                        lstep3[47] = _mm_sub_epi32(lstep1[33], lstep2[47]);
//                        lstep3[48] = _mm_sub_epi32(lstep1[62], lstep2[48]);
//                        lstep3[49] = _mm_sub_epi32(lstep1[63], lstep2[49]);
//                        lstep3[50] = _mm_sub_epi32(lstep1[60], lstep2[50]);
//                        lstep3[51] = _mm_sub_epi32(lstep1[61], lstep2[51]);
//                        lstep3[52] = _mm_sub_epi32(lstep1[58], lstep2[52]);
//                        lstep3[53] = _mm_sub_epi32(lstep1[59], lstep2[53]);
//                        lstep3[54] = _mm_sub_epi32(lstep1[56], lstep2[54]);
//                        lstep3[55] = _mm_sub_epi32(lstep1[57], lstep2[55]);
//                        lstep3[56] = _mm_add_epi32(lstep2[54], lstep1[56]);
//                        lstep3[57] = _mm_add_epi32(lstep2[55], lstep1[57]);
//                        lstep3[58] = _mm_add_epi32(lstep2[52], lstep1[58]);
//                        lstep3[59] = _mm_add_epi32(lstep2[53], lstep1[59]);
//                        lstep3[60] = _mm_add_epi32(lstep2[50], lstep1[60]);
//                        lstep3[61] = _mm_add_epi32(lstep2[51], lstep1[61]);
//                        lstep3[62] = _mm_add_epi32(lstep2[48], lstep1[62]);
//                        lstep3[63] = _mm_add_epi32(lstep2[49], lstep1[63]);
//                    }
//
//                    // stage 4
//                    {
//                        // expanding to 32-bit length priori to addition operations
//                        lstep2[16] = _mm_unpacklo_epi16(step2[8], kZero);
//                        lstep2[17] = _mm_unpackhi_epi16(step2[8], kZero);
//                        lstep2[18] = _mm_unpacklo_epi16(step2[9], kZero);
//                        lstep2[19] = _mm_unpackhi_epi16(step2[9], kZero);
//                        lstep2[28] = _mm_unpacklo_epi16(step2[14], kZero);
//                        lstep2[29] = _mm_unpackhi_epi16(step2[14], kZero);
//                        lstep2[30] = _mm_unpacklo_epi16(step2[15], kZero);
//                        lstep2[31] = _mm_unpackhi_epi16(step2[15], kZero);
//                        lstep2[16] = _mm_madd_epi16(lstep2[16], kOne);
//                        lstep2[17] = _mm_madd_epi16(lstep2[17], kOne);
//                        lstep2[18] = _mm_madd_epi16(lstep2[18], kOne);
//                        lstep2[19] = _mm_madd_epi16(lstep2[19], kOne);
//                        lstep2[28] = _mm_madd_epi16(lstep2[28], kOne);
//                        lstep2[29] = _mm_madd_epi16(lstep2[29], kOne);
//                        lstep2[30] = _mm_madd_epi16(lstep2[30], kOne);
//                        lstep2[31] = _mm_madd_epi16(lstep2[31], kOne);
//
//                        lstep1[0] = _mm_add_epi32(lstep3[6], lstep3[0]);
//                        lstep1[1] = _mm_add_epi32(lstep3[7], lstep3[1]);
//                        lstep1[2] = _mm_add_epi32(lstep3[4], lstep3[2]);
//                        lstep1[3] = _mm_add_epi32(lstep3[5], lstep3[3]);
//                        lstep1[4] = _mm_sub_epi32(lstep3[2], lstep3[4]);
//                        lstep1[5] = _mm_sub_epi32(lstep3[3], lstep3[5]);
//                        lstep1[6] = _mm_sub_epi32(lstep3[0], lstep3[6]);
//                        lstep1[7] = _mm_sub_epi32(lstep3[1], lstep3[7]);
//                        lstep1[16] = _mm_add_epi32(lstep3[22], lstep2[16]);
//                        lstep1[17] = _mm_add_epi32(lstep3[23], lstep2[17]);
//                        lstep1[18] = _mm_add_epi32(lstep3[20], lstep2[18]);
//                        lstep1[19] = _mm_add_epi32(lstep3[21], lstep2[19]);
//                        lstep1[20] = _mm_sub_epi32(lstep2[18], lstep3[20]);
//                        lstep1[21] = _mm_sub_epi32(lstep2[19], lstep3[21]);
//                        lstep1[22] = _mm_sub_epi32(lstep2[16], lstep3[22]);
//                        lstep1[23] = _mm_sub_epi32(lstep2[17], lstep3[23]);
//                        lstep1[24] = _mm_sub_epi32(lstep2[30], lstep3[24]);
//                        lstep1[25] = _mm_sub_epi32(lstep2[31], lstep3[25]);
//                        lstep1[26] = _mm_sub_epi32(lstep2[28], lstep3[26]);
//                        lstep1[27] = _mm_sub_epi32(lstep2[29], lstep3[27]);
//                        lstep1[28] = _mm_add_epi32(lstep3[26], lstep2[28]);
//                        lstep1[29] = _mm_add_epi32(lstep3[27], lstep2[29]);
//                        lstep1[30] = _mm_add_epi32(lstep3[24], lstep2[30]);
//                        lstep1[31] = _mm_add_epi32(lstep3[25], lstep2[31]);
//                    }
//                    {
//                        // to be continued...
//                        //
//                        const __m128i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
//                        const __m128i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep3[12], lstep3[10]);
//                        u[1] = _mm_unpackhi_epi32(lstep3[12], lstep3[10]);
//                        u[2] = _mm_unpacklo_epi32(lstep3[13], lstep3[11]);
//                        u[3] = _mm_unpackhi_epi32(lstep3[13], lstep3[11]);
//
//                        // TODO(jingning): manually inline k_madd_epi32_ to further hide
//                        // instruction latency.
//                        v[0] = k_madd_epi32(u[0], k32_p16_m16);
//                        v[1] = k_madd_epi32(u[1], k32_p16_m16);
//                        v[2] = k_madd_epi32(u[2], k32_p16_m16);
//                        v[3] = k_madd_epi32(u[3], k32_p16_m16);
//                        v[4] = k_madd_epi32(u[0], k32_p16_p16);
//                        v[5] = k_madd_epi32(u[1], k32_p16_p16);
//                        v[6] = k_madd_epi32(u[2], k32_p16_p16);
//                        v[7] = k_madd_epi32(u[3], k32_p16_p16);
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_8(&v[0], &v[1], &v[2], &v[3], &v[4],
//                            &v[5], &v[6], &v[7], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//
//                        lstep1[10] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        lstep1[11] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        lstep1[12] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        lstep1[13] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                    }
//                    {
//                        const __m128i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
//                        const __m128i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
//                        const __m128i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep3[36], lstep3[58]);
//                        u[1] = _mm_unpackhi_epi32(lstep3[36], lstep3[58]);
//                        u[2] = _mm_unpacklo_epi32(lstep3[37], lstep3[59]);
//                        u[3] = _mm_unpackhi_epi32(lstep3[37], lstep3[59]);
//                        u[4] = _mm_unpacklo_epi32(lstep3[38], lstep3[56]);
//                        u[5] = _mm_unpackhi_epi32(lstep3[38], lstep3[56]);
//                        u[6] = _mm_unpacklo_epi32(lstep3[39], lstep3[57]);
//                        u[7] = _mm_unpackhi_epi32(lstep3[39], lstep3[57]);
//                        u[8] = _mm_unpacklo_epi32(lstep3[40], lstep3[54]);
//                        u[9] = _mm_unpackhi_epi32(lstep3[40], lstep3[54]);
//                        u[10] = _mm_unpacklo_epi32(lstep3[41], lstep3[55]);
//                        u[11] = _mm_unpackhi_epi32(lstep3[41], lstep3[55]);
//                        u[12] = _mm_unpacklo_epi32(lstep3[42], lstep3[52]);
//                        u[13] = _mm_unpackhi_epi32(lstep3[42], lstep3[52]);
//                        u[14] = _mm_unpacklo_epi32(lstep3[43], lstep3[53]);
//                        u[15] = _mm_unpackhi_epi32(lstep3[43], lstep3[53]);
//
//                        v[0] = k_madd_epi32(u[0], k32_m08_p24);
//                        v[1] = k_madd_epi32(u[1], k32_m08_p24);
//                        v[2] = k_madd_epi32(u[2], k32_m08_p24);
//                        v[3] = k_madd_epi32(u[3], k32_m08_p24);
//                        v[4] = k_madd_epi32(u[4], k32_m08_p24);
//                        v[5] = k_madd_epi32(u[5], k32_m08_p24);
//                        v[6] = k_madd_epi32(u[6], k32_m08_p24);
//                        v[7] = k_madd_epi32(u[7], k32_m08_p24);
//                        v[8] = k_madd_epi32(u[8], k32_m24_m08);
//                        v[9] = k_madd_epi32(u[9], k32_m24_m08);
//                        v[10] = k_madd_epi32(u[10], k32_m24_m08);
//                        v[11] = k_madd_epi32(u[11], k32_m24_m08);
//                        v[12] = k_madd_epi32(u[12], k32_m24_m08);
//                        v[13] = k_madd_epi32(u[13], k32_m24_m08);
//                        v[14] = k_madd_epi32(u[14], k32_m24_m08);
//                        v[15] = k_madd_epi32(u[15], k32_m24_m08);
//                        v[16] = k_madd_epi32(u[12], k32_m08_p24);
//                        v[17] = k_madd_epi32(u[13], k32_m08_p24);
//                        v[18] = k_madd_epi32(u[14], k32_m08_p24);
//                        v[19] = k_madd_epi32(u[15], k32_m08_p24);
//                        v[20] = k_madd_epi32(u[8], k32_m08_p24);
//                        v[21] = k_madd_epi32(u[9], k32_m08_p24);
//                        v[22] = k_madd_epi32(u[10], k32_m08_p24);
//                        v[23] = k_madd_epi32(u[11], k32_m08_p24);
//                        v[24] = k_madd_epi32(u[4], k32_p24_p08);
//                        v[25] = k_madd_epi32(u[5], k32_p24_p08);
//                        v[26] = k_madd_epi32(u[6], k32_p24_p08);
//                        v[27] = k_madd_epi32(u[7], k32_p24_p08);
//                        v[28] = k_madd_epi32(u[0], k32_p24_p08);
//                        v[29] = k_madd_epi32(u[1], k32_p24_p08);
//                        v[30] = k_madd_epi32(u[2], k32_p24_p08);
//                        v[31] = k_madd_epi32(u[3], k32_p24_p08);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_32(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
//                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
//                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//                        u[8] = k_packs_epi64(v[16], v[17]);
//                        u[9] = k_packs_epi64(v[18], v[19]);
//                        u[10] = k_packs_epi64(v[20], v[21]);
//                        u[11] = k_packs_epi64(v[22], v[23]);
//                        u[12] = k_packs_epi64(v[24], v[25]);
//                        u[13] = k_packs_epi64(v[26], v[27]);
//                        u[14] = k_packs_epi64(v[28], v[29]);
//                        u[15] = k_packs_epi64(v[30], v[31]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
//                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
//                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
//                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
//                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
//                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
//                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
//                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);
//
//                        lstep1[36] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        lstep1[37] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        lstep1[38] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        lstep1[39] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                        lstep1[40] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
//                        lstep1[41] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
//                        lstep1[42] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
//                        lstep1[43] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
//                        lstep1[52] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
//                        lstep1[53] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
//                        lstep1[54] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
//                        lstep1[55] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
//                        lstep1[56] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
//                        lstep1[57] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
//                        lstep1[58] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
//                        lstep1[59] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
//                    }
//                    // stage 5
//                    {
//                        lstep2[8] = _mm_add_epi32(lstep1[10], lstep3[8]);
//                        lstep2[9] = _mm_add_epi32(lstep1[11], lstep3[9]);
//                        lstep2[10] = _mm_sub_epi32(lstep3[8], lstep1[10]);
//                        lstep2[11] = _mm_sub_epi32(lstep3[9], lstep1[11]);
//                        lstep2[12] = _mm_sub_epi32(lstep3[14], lstep1[12]);
//                        lstep2[13] = _mm_sub_epi32(lstep3[15], lstep1[13]);
//                        lstep2[14] = _mm_add_epi32(lstep1[12], lstep3[14]);
//                        lstep2[15] = _mm_add_epi32(lstep1[13], lstep3[15]);
//                    }
//                    {
//                        const __m128i k32_p16_p16 = pair_set_epi32(cospi_16_64, cospi_16_64);
//                        const __m128i k32_p16_m16 = pair_set_epi32(cospi_16_64, -cospi_16_64);
//                        const __m128i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);
//                        const __m128i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep1[0], lstep1[2]);
//                        u[1] = _mm_unpackhi_epi32(lstep1[0], lstep1[2]);
//                        u[2] = _mm_unpacklo_epi32(lstep1[1], lstep1[3]);
//                        u[3] = _mm_unpackhi_epi32(lstep1[1], lstep1[3]);
//                        u[4] = _mm_unpacklo_epi32(lstep1[4], lstep1[6]);
//                        u[5] = _mm_unpackhi_epi32(lstep1[4], lstep1[6]);
//                        u[6] = _mm_unpacklo_epi32(lstep1[5], lstep1[7]);
//                        u[7] = _mm_unpackhi_epi32(lstep1[5], lstep1[7]);
//
//                        // TODO(jingning): manually inline k_madd_epi32_ to further hide
//                        // instruction latency.
//                        v[0] = k_madd_epi32(u[0], k32_p16_p16);
//                        v[1] = k_madd_epi32(u[1], k32_p16_p16);
//                        v[2] = k_madd_epi32(u[2], k32_p16_p16);
//                        v[3] = k_madd_epi32(u[3], k32_p16_p16);
//                        v[4] = k_madd_epi32(u[0], k32_p16_m16);
//                        v[5] = k_madd_epi32(u[1], k32_p16_m16);
//                        v[6] = k_madd_epi32(u[2], k32_p16_m16);
//                        v[7] = k_madd_epi32(u[3], k32_p16_m16);
//                        v[8] = k_madd_epi32(u[4], k32_p24_p08);
//                        v[9] = k_madd_epi32(u[5], k32_p24_p08);
//                        v[10] = k_madd_epi32(u[6], k32_p24_p08);
//                        v[11] = k_madd_epi32(u[7], k32_p24_p08);
//                        v[12] = k_madd_epi32(u[4], k32_m08_p24);
//                        v[13] = k_madd_epi32(u[5], k32_m08_p24);
//                        v[14] = k_madd_epi32(u[6], k32_m08_p24);
//                        v[15] = k_madd_epi32(u[7], k32_m08_p24);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_16(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//
//                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
//                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
//                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
//                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
//
//                        sign[0] = _mm_cmplt_epi32(u[0], kZero);
//                        sign[1] = _mm_cmplt_epi32(u[1], kZero);
//                        sign[2] = _mm_cmplt_epi32(u[2], kZero);
//                        sign[3] = _mm_cmplt_epi32(u[3], kZero);
//                        sign[4] = _mm_cmplt_epi32(u[4], kZero);
//                        sign[5] = _mm_cmplt_epi32(u[5], kZero);
//                        sign[6] = _mm_cmplt_epi32(u[6], kZero);
//                        sign[7] = _mm_cmplt_epi32(u[7], kZero);
//
//                        u[0] = _mm_sub_epi32(u[0], sign[0]);
//                        u[1] = _mm_sub_epi32(u[1], sign[1]);
//                        u[2] = _mm_sub_epi32(u[2], sign[2]);
//                        u[3] = _mm_sub_epi32(u[3], sign[3]);
//                        u[4] = _mm_sub_epi32(u[4], sign[4]);
//                        u[5] = _mm_sub_epi32(u[5], sign[5]);
//                        u[6] = _mm_sub_epi32(u[6], sign[6]);
//                        u[7] = _mm_sub_epi32(u[7], sign[7]);
//
//                        u[0] = _mm_add_epi32(u[0], K32One);
//                        u[1] = _mm_add_epi32(u[1], K32One);
//                        u[2] = _mm_add_epi32(u[2], K32One);
//                        u[3] = _mm_add_epi32(u[3], K32One);
//                        u[4] = _mm_add_epi32(u[4], K32One);
//                        u[5] = _mm_add_epi32(u[5], K32One);
//                        u[6] = _mm_add_epi32(u[6], K32One);
//                        u[7] = _mm_add_epi32(u[7], K32One);
//
//                        u[0] = _mm_srai_epi32(u[0], 2);
//                        u[1] = _mm_srai_epi32(u[1], 2);
//                        u[2] = _mm_srai_epi32(u[2], 2);
//                        u[3] = _mm_srai_epi32(u[3], 2);
//                        u[4] = _mm_srai_epi32(u[4], 2);
//                        u[5] = _mm_srai_epi32(u[5], 2);
//                        u[6] = _mm_srai_epi32(u[6], 2);
//                        u[7] = _mm_srai_epi32(u[7], 2);
//
//                        // Combine
//                        out[0] = _mm_packs_epi32(u[0], u[1]);
//                        out[16] = _mm_packs_epi32(u[2], u[3]);
//                        out[8] = _mm_packs_epi32(u[4], u[5]);
//                        out[24] = _mm_packs_epi32(u[6], u[7]);
//#if DCT_HIGH_BIT_DEPTH
//                        overflow =
//                            check_epi16_overflow_x4(&out[0], &out[16], &out[8], &out[24]);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                    }
//                    {
//                        const __m128i k32_m08_p24 = pair_set_epi32(-cospi_8_64, cospi_24_64);
//                        const __m128i k32_m24_m08 = pair_set_epi32(-cospi_24_64, -cospi_8_64);
//                        const __m128i k32_p24_p08 = pair_set_epi32(cospi_24_64, cospi_8_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep1[18], lstep1[28]);
//                        u[1] = _mm_unpackhi_epi32(lstep1[18], lstep1[28]);
//                        u[2] = _mm_unpacklo_epi32(lstep1[19], lstep1[29]);
//                        u[3] = _mm_unpackhi_epi32(lstep1[19], lstep1[29]);
//                        u[4] = _mm_unpacklo_epi32(lstep1[20], lstep1[26]);
//                        u[5] = _mm_unpackhi_epi32(lstep1[20], lstep1[26]);
//                        u[6] = _mm_unpacklo_epi32(lstep1[21], lstep1[27]);
//                        u[7] = _mm_unpackhi_epi32(lstep1[21], lstep1[27]);
//
//                        v[0] = k_madd_epi32(u[0], k32_m08_p24);
//                        v[1] = k_madd_epi32(u[1], k32_m08_p24);
//                        v[2] = k_madd_epi32(u[2], k32_m08_p24);
//                        v[3] = k_madd_epi32(u[3], k32_m08_p24);
//                        v[4] = k_madd_epi32(u[4], k32_m24_m08);
//                        v[5] = k_madd_epi32(u[5], k32_m24_m08);
//                        v[6] = k_madd_epi32(u[6], k32_m24_m08);
//                        v[7] = k_madd_epi32(u[7], k32_m24_m08);
//                        v[8] = k_madd_epi32(u[4], k32_m08_p24);
//                        v[9] = k_madd_epi32(u[5], k32_m08_p24);
//                        v[10] = k_madd_epi32(u[6], k32_m08_p24);
//                        v[11] = k_madd_epi32(u[7], k32_m08_p24);
//                        v[12] = k_madd_epi32(u[0], k32_p24_p08);
//                        v[13] = k_madd_epi32(u[1], k32_p24_p08);
//                        v[14] = k_madd_epi32(u[2], k32_p24_p08);
//                        v[15] = k_madd_epi32(u[3], k32_p24_p08);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_16(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//
//                        u[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        u[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        u[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        u[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        u[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        u[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        u[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        u[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//
//                        lstep2[18] = _mm_srai_epi32(u[0], DCT_CONST_BITS);
//                        lstep2[19] = _mm_srai_epi32(u[1], DCT_CONST_BITS);
//                        lstep2[20] = _mm_srai_epi32(u[2], DCT_CONST_BITS);
//                        lstep2[21] = _mm_srai_epi32(u[3], DCT_CONST_BITS);
//                        lstep2[26] = _mm_srai_epi32(u[4], DCT_CONST_BITS);
//                        lstep2[27] = _mm_srai_epi32(u[5], DCT_CONST_BITS);
//                        lstep2[28] = _mm_srai_epi32(u[6], DCT_CONST_BITS);
//                        lstep2[29] = _mm_srai_epi32(u[7], DCT_CONST_BITS);
//                    }
//                    {
//                        lstep2[32] = _mm_add_epi32(lstep1[38], lstep3[32]);
//                        lstep2[33] = _mm_add_epi32(lstep1[39], lstep3[33]);
//                        lstep2[34] = _mm_add_epi32(lstep1[36], lstep3[34]);
//                        lstep2[35] = _mm_add_epi32(lstep1[37], lstep3[35]);
//                        lstep2[36] = _mm_sub_epi32(lstep3[34], lstep1[36]);
//                        lstep2[37] = _mm_sub_epi32(lstep3[35], lstep1[37]);
//                        lstep2[38] = _mm_sub_epi32(lstep3[32], lstep1[38]);
//                        lstep2[39] = _mm_sub_epi32(lstep3[33], lstep1[39]);
//                        lstep2[40] = _mm_sub_epi32(lstep3[46], lstep1[40]);
//                        lstep2[41] = _mm_sub_epi32(lstep3[47], lstep1[41]);
//                        lstep2[42] = _mm_sub_epi32(lstep3[44], lstep1[42]);
//                        lstep2[43] = _mm_sub_epi32(lstep3[45], lstep1[43]);
//                        lstep2[44] = _mm_add_epi32(lstep1[42], lstep3[44]);
//                        lstep2[45] = _mm_add_epi32(lstep1[43], lstep3[45]);
//                        lstep2[46] = _mm_add_epi32(lstep1[40], lstep3[46]);
//                        lstep2[47] = _mm_add_epi32(lstep1[41], lstep3[47]);
//                        lstep2[48] = _mm_add_epi32(lstep1[54], lstep3[48]);
//                        lstep2[49] = _mm_add_epi32(lstep1[55], lstep3[49]);
//                        lstep2[50] = _mm_add_epi32(lstep1[52], lstep3[50]);
//                        lstep2[51] = _mm_add_epi32(lstep1[53], lstep3[51]);
//                        lstep2[52] = _mm_sub_epi32(lstep3[50], lstep1[52]);
//                        lstep2[53] = _mm_sub_epi32(lstep3[51], lstep1[53]);
//                        lstep2[54] = _mm_sub_epi32(lstep3[48], lstep1[54]);
//                        lstep2[55] = _mm_sub_epi32(lstep3[49], lstep1[55]);
//                        lstep2[56] = _mm_sub_epi32(lstep3[62], lstep1[56]);
//                        lstep2[57] = _mm_sub_epi32(lstep3[63], lstep1[57]);
//                        lstep2[58] = _mm_sub_epi32(lstep3[60], lstep1[58]);
//                        lstep2[59] = _mm_sub_epi32(lstep3[61], lstep1[59]);
//                        lstep2[60] = _mm_add_epi32(lstep1[58], lstep3[60]);
//                        lstep2[61] = _mm_add_epi32(lstep1[59], lstep3[61]);
//                        lstep2[62] = _mm_add_epi32(lstep1[56], lstep3[62]);
//                        lstep2[63] = _mm_add_epi32(lstep1[57], lstep3[63]);
//                    }
//                    // stage 6
//                    {
//                        const __m128i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);
//                        const __m128i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
//                        const __m128i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
//                        const __m128i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep2[8], lstep2[14]);
//                        u[1] = _mm_unpackhi_epi32(lstep2[8], lstep2[14]);
//                        u[2] = _mm_unpacklo_epi32(lstep2[9], lstep2[15]);
//                        u[3] = _mm_unpackhi_epi32(lstep2[9], lstep2[15]);
//                        u[4] = _mm_unpacklo_epi32(lstep2[10], lstep2[12]);
//                        u[5] = _mm_unpackhi_epi32(lstep2[10], lstep2[12]);
//                        u[6] = _mm_unpacklo_epi32(lstep2[11], lstep2[13]);
//                        u[7] = _mm_unpackhi_epi32(lstep2[11], lstep2[13]);
//                        u[8] = _mm_unpacklo_epi32(lstep2[10], lstep2[12]);
//                        u[9] = _mm_unpackhi_epi32(lstep2[10], lstep2[12]);
//                        u[10] = _mm_unpacklo_epi32(lstep2[11], lstep2[13]);
//                        u[11] = _mm_unpackhi_epi32(lstep2[11], lstep2[13]);
//                        u[12] = _mm_unpacklo_epi32(lstep2[8], lstep2[14]);
//                        u[13] = _mm_unpackhi_epi32(lstep2[8], lstep2[14]);
//                        u[14] = _mm_unpacklo_epi32(lstep2[9], lstep2[15]);
//                        u[15] = _mm_unpackhi_epi32(lstep2[9], lstep2[15]);
//
//                        v[0] = k_madd_epi32(u[0], k32_p28_p04);
//                        v[1] = k_madd_epi32(u[1], k32_p28_p04);
//                        v[2] = k_madd_epi32(u[2], k32_p28_p04);
//                        v[3] = k_madd_epi32(u[3], k32_p28_p04);
//                        v[4] = k_madd_epi32(u[4], k32_p12_p20);
//                        v[5] = k_madd_epi32(u[5], k32_p12_p20);
//                        v[6] = k_madd_epi32(u[6], k32_p12_p20);
//                        v[7] = k_madd_epi32(u[7], k32_p12_p20);
//                        v[8] = k_madd_epi32(u[8], k32_m20_p12);
//                        v[9] = k_madd_epi32(u[9], k32_m20_p12);
//                        v[10] = k_madd_epi32(u[10], k32_m20_p12);
//                        v[11] = k_madd_epi32(u[11], k32_m20_p12);
//                        v[12] = k_madd_epi32(u[12], k32_m04_p28);
//                        v[13] = k_madd_epi32(u[13], k32_m04_p28);
//                        v[14] = k_madd_epi32(u[14], k32_m04_p28);
//                        v[15] = k_madd_epi32(u[15], k32_m04_p28);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_16(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//
//                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
//                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
//                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
//                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
//
//                        sign[0] = _mm_cmplt_epi32(u[0], kZero);
//                        sign[1] = _mm_cmplt_epi32(u[1], kZero);
//                        sign[2] = _mm_cmplt_epi32(u[2], kZero);
//                        sign[3] = _mm_cmplt_epi32(u[3], kZero);
//                        sign[4] = _mm_cmplt_epi32(u[4], kZero);
//                        sign[5] = _mm_cmplt_epi32(u[5], kZero);
//                        sign[6] = _mm_cmplt_epi32(u[6], kZero);
//                        sign[7] = _mm_cmplt_epi32(u[7], kZero);
//
//                        u[0] = _mm_sub_epi32(u[0], sign[0]);
//                        u[1] = _mm_sub_epi32(u[1], sign[1]);
//                        u[2] = _mm_sub_epi32(u[2], sign[2]);
//                        u[3] = _mm_sub_epi32(u[3], sign[3]);
//                        u[4] = _mm_sub_epi32(u[4], sign[4]);
//                        u[5] = _mm_sub_epi32(u[5], sign[5]);
//                        u[6] = _mm_sub_epi32(u[6], sign[6]);
//                        u[7] = _mm_sub_epi32(u[7], sign[7]);
//
//                        u[0] = _mm_add_epi32(u[0], K32One);
//                        u[1] = _mm_add_epi32(u[1], K32One);
//                        u[2] = _mm_add_epi32(u[2], K32One);
//                        u[3] = _mm_add_epi32(u[3], K32One);
//                        u[4] = _mm_add_epi32(u[4], K32One);
//                        u[5] = _mm_add_epi32(u[5], K32One);
//                        u[6] = _mm_add_epi32(u[6], K32One);
//                        u[7] = _mm_add_epi32(u[7], K32One);
//
//                        u[0] = _mm_srai_epi32(u[0], 2);
//                        u[1] = _mm_srai_epi32(u[1], 2);
//                        u[2] = _mm_srai_epi32(u[2], 2);
//                        u[3] = _mm_srai_epi32(u[3], 2);
//                        u[4] = _mm_srai_epi32(u[4], 2);
//                        u[5] = _mm_srai_epi32(u[5], 2);
//                        u[6] = _mm_srai_epi32(u[6], 2);
//                        u[7] = _mm_srai_epi32(u[7], 2);
//
//                        out[4] = _mm_packs_epi32(u[0], u[1]);
//                        out[20] = _mm_packs_epi32(u[2], u[3]);
//                        out[12] = _mm_packs_epi32(u[4], u[5]);
//                        out[28] = _mm_packs_epi32(u[6], u[7]);
//#if DCT_HIGH_BIT_DEPTH
//                        overflow =
//                            check_epi16_overflow_x4(&out[4], &out[20], &out[12], &out[28]);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                    }
//                    {
//                        lstep3[16] = _mm_add_epi32(lstep2[18], lstep1[16]);
//                        lstep3[17] = _mm_add_epi32(lstep2[19], lstep1[17]);
//                        lstep3[18] = _mm_sub_epi32(lstep1[16], lstep2[18]);
//                        lstep3[19] = _mm_sub_epi32(lstep1[17], lstep2[19]);
//                        lstep3[20] = _mm_sub_epi32(lstep1[22], lstep2[20]);
//                        lstep3[21] = _mm_sub_epi32(lstep1[23], lstep2[21]);
//                        lstep3[22] = _mm_add_epi32(lstep2[20], lstep1[22]);
//                        lstep3[23] = _mm_add_epi32(lstep2[21], lstep1[23]);
//                        lstep3[24] = _mm_add_epi32(lstep2[26], lstep1[24]);
//                        lstep3[25] = _mm_add_epi32(lstep2[27], lstep1[25]);
//                        lstep3[26] = _mm_sub_epi32(lstep1[24], lstep2[26]);
//                        lstep3[27] = _mm_sub_epi32(lstep1[25], lstep2[27]);
//                        lstep3[28] = _mm_sub_epi32(lstep1[30], lstep2[28]);
//                        lstep3[29] = _mm_sub_epi32(lstep1[31], lstep2[29]);
//                        lstep3[30] = _mm_add_epi32(lstep2[28], lstep1[30]);
//                        lstep3[31] = _mm_add_epi32(lstep2[29], lstep1[31]);
//                    }
//                    {
//                        const __m128i k32_m04_p28 = pair_set_epi32(-cospi_4_64, cospi_28_64);
//                        const __m128i k32_m28_m04 = pair_set_epi32(-cospi_28_64, -cospi_4_64);
//                        const __m128i k32_m20_p12 = pair_set_epi32(-cospi_20_64, cospi_12_64);
//                        const __m128i k32_m12_m20 =
//                            pair_set_epi32(-cospi_12_64, -cospi_20_64);
//                        const __m128i k32_p12_p20 = pair_set_epi32(cospi_12_64, cospi_20_64);
//                        const __m128i k32_p28_p04 = pair_set_epi32(cospi_28_64, cospi_4_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep2[34], lstep2[60]);
//                        u[1] = _mm_unpackhi_epi32(lstep2[34], lstep2[60]);
//                        u[2] = _mm_unpacklo_epi32(lstep2[35], lstep2[61]);
//                        u[3] = _mm_unpackhi_epi32(lstep2[35], lstep2[61]);
//                        u[4] = _mm_unpacklo_epi32(lstep2[36], lstep2[58]);
//                        u[5] = _mm_unpackhi_epi32(lstep2[36], lstep2[58]);
//                        u[6] = _mm_unpacklo_epi32(lstep2[37], lstep2[59]);
//                        u[7] = _mm_unpackhi_epi32(lstep2[37], lstep2[59]);
//                        u[8] = _mm_unpacklo_epi32(lstep2[42], lstep2[52]);
//                        u[9] = _mm_unpackhi_epi32(lstep2[42], lstep2[52]);
//                        u[10] = _mm_unpacklo_epi32(lstep2[43], lstep2[53]);
//                        u[11] = _mm_unpackhi_epi32(lstep2[43], lstep2[53]);
//                        u[12] = _mm_unpacklo_epi32(lstep2[44], lstep2[50]);
//                        u[13] = _mm_unpackhi_epi32(lstep2[44], lstep2[50]);
//                        u[14] = _mm_unpacklo_epi32(lstep2[45], lstep2[51]);
//                        u[15] = _mm_unpackhi_epi32(lstep2[45], lstep2[51]);
//
//                        v[0] = k_madd_epi32(u[0], k32_m04_p28);
//                        v[1] = k_madd_epi32(u[1], k32_m04_p28);
//                        v[2] = k_madd_epi32(u[2], k32_m04_p28);
//                        v[3] = k_madd_epi32(u[3], k32_m04_p28);
//                        v[4] = k_madd_epi32(u[4], k32_m28_m04);
//                        v[5] = k_madd_epi32(u[5], k32_m28_m04);
//                        v[6] = k_madd_epi32(u[6], k32_m28_m04);
//                        v[7] = k_madd_epi32(u[7], k32_m28_m04);
//                        v[8] = k_madd_epi32(u[8], k32_m20_p12);
//                        v[9] = k_madd_epi32(u[9], k32_m20_p12);
//                        v[10] = k_madd_epi32(u[10], k32_m20_p12);
//                        v[11] = k_madd_epi32(u[11], k32_m20_p12);
//                        v[12] = k_madd_epi32(u[12], k32_m12_m20);
//                        v[13] = k_madd_epi32(u[13], k32_m12_m20);
//                        v[14] = k_madd_epi32(u[14], k32_m12_m20);
//                        v[15] = k_madd_epi32(u[15], k32_m12_m20);
//                        v[16] = k_madd_epi32(u[12], k32_m20_p12);
//                        v[17] = k_madd_epi32(u[13], k32_m20_p12);
//                        v[18] = k_madd_epi32(u[14], k32_m20_p12);
//                        v[19] = k_madd_epi32(u[15], k32_m20_p12);
//                        v[20] = k_madd_epi32(u[8], k32_p12_p20);
//                        v[21] = k_madd_epi32(u[9], k32_p12_p20);
//                        v[22] = k_madd_epi32(u[10], k32_p12_p20);
//                        v[23] = k_madd_epi32(u[11], k32_p12_p20);
//                        v[24] = k_madd_epi32(u[4], k32_m04_p28);
//                        v[25] = k_madd_epi32(u[5], k32_m04_p28);
//                        v[26] = k_madd_epi32(u[6], k32_m04_p28);
//                        v[27] = k_madd_epi32(u[7], k32_m04_p28);
//                        v[28] = k_madd_epi32(u[0], k32_p28_p04);
//                        v[29] = k_madd_epi32(u[1], k32_p28_p04);
//                        v[30] = k_madd_epi32(u[2], k32_p28_p04);
//                        v[31] = k_madd_epi32(u[3], k32_p28_p04);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_32(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
//                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
//                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//                        u[8] = k_packs_epi64(v[16], v[17]);
//                        u[9] = k_packs_epi64(v[18], v[19]);
//                        u[10] = k_packs_epi64(v[20], v[21]);
//                        u[11] = k_packs_epi64(v[22], v[23]);
//                        u[12] = k_packs_epi64(v[24], v[25]);
//                        u[13] = k_packs_epi64(v[26], v[27]);
//                        u[14] = k_packs_epi64(v[28], v[29]);
//                        u[15] = k_packs_epi64(v[30], v[31]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
//                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
//                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
//                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
//                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
//                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
//                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
//                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);
//
//                        lstep3[34] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        lstep3[35] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        lstep3[36] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        lstep3[37] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                        lstep3[42] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
//                        lstep3[43] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
//                        lstep3[44] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
//                        lstep3[45] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
//                        lstep3[50] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
//                        lstep3[51] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
//                        lstep3[52] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
//                        lstep3[53] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
//                        lstep3[58] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
//                        lstep3[59] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
//                        lstep3[60] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
//                        lstep3[61] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
//                    }
//                    // stage 7
//                    {
//                        const __m128i k32_p30_p02 = pair_set_epi32(cospi_30_64, cospi_2_64);
//                        const __m128i k32_p14_p18 = pair_set_epi32(cospi_14_64, cospi_18_64);
//                        const __m128i k32_p22_p10 = pair_set_epi32(cospi_22_64, cospi_10_64);
//                        const __m128i k32_p06_p26 = pair_set_epi32(cospi_6_64, cospi_26_64);
//                        const __m128i k32_m26_p06 = pair_set_epi32(-cospi_26_64, cospi_6_64);
//                        const __m128i k32_m10_p22 = pair_set_epi32(-cospi_10_64, cospi_22_64);
//                        const __m128i k32_m18_p14 = pair_set_epi32(-cospi_18_64, cospi_14_64);
//                        const __m128i k32_m02_p30 = pair_set_epi32(-cospi_2_64, cospi_30_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep3[16], lstep3[30]);
//                        u[1] = _mm_unpackhi_epi32(lstep3[16], lstep3[30]);
//                        u[2] = _mm_unpacklo_epi32(lstep3[17], lstep3[31]);
//                        u[3] = _mm_unpackhi_epi32(lstep3[17], lstep3[31]);
//                        u[4] = _mm_unpacklo_epi32(lstep3[18], lstep3[28]);
//                        u[5] = _mm_unpackhi_epi32(lstep3[18], lstep3[28]);
//                        u[6] = _mm_unpacklo_epi32(lstep3[19], lstep3[29]);
//                        u[7] = _mm_unpackhi_epi32(lstep3[19], lstep3[29]);
//                        u[8] = _mm_unpacklo_epi32(lstep3[20], lstep3[26]);
//                        u[9] = _mm_unpackhi_epi32(lstep3[20], lstep3[26]);
//                        u[10] = _mm_unpacklo_epi32(lstep3[21], lstep3[27]);
//                        u[11] = _mm_unpackhi_epi32(lstep3[21], lstep3[27]);
//                        u[12] = _mm_unpacklo_epi32(lstep3[22], lstep3[24]);
//                        u[13] = _mm_unpackhi_epi32(lstep3[22], lstep3[24]);
//                        u[14] = _mm_unpacklo_epi32(lstep3[23], lstep3[25]);
//                        u[15] = _mm_unpackhi_epi32(lstep3[23], lstep3[25]);
//
//                        v[0] = k_madd_epi32(u[0], k32_p30_p02);
//                        v[1] = k_madd_epi32(u[1], k32_p30_p02);
//                        v[2] = k_madd_epi32(u[2], k32_p30_p02);
//                        v[3] = k_madd_epi32(u[3], k32_p30_p02);
//                        v[4] = k_madd_epi32(u[4], k32_p14_p18);
//                        v[5] = k_madd_epi32(u[5], k32_p14_p18);
//                        v[6] = k_madd_epi32(u[6], k32_p14_p18);
//                        v[7] = k_madd_epi32(u[7], k32_p14_p18);
//                        v[8] = k_madd_epi32(u[8], k32_p22_p10);
//                        v[9] = k_madd_epi32(u[9], k32_p22_p10);
//                        v[10] = k_madd_epi32(u[10], k32_p22_p10);
//                        v[11] = k_madd_epi32(u[11], k32_p22_p10);
//                        v[12] = k_madd_epi32(u[12], k32_p06_p26);
//                        v[13] = k_madd_epi32(u[13], k32_p06_p26);
//                        v[14] = k_madd_epi32(u[14], k32_p06_p26);
//                        v[15] = k_madd_epi32(u[15], k32_p06_p26);
//                        v[16] = k_madd_epi32(u[12], k32_m26_p06);
//                        v[17] = k_madd_epi32(u[13], k32_m26_p06);
//                        v[18] = k_madd_epi32(u[14], k32_m26_p06);
//                        v[19] = k_madd_epi32(u[15], k32_m26_p06);
//                        v[20] = k_madd_epi32(u[8], k32_m10_p22);
//                        v[21] = k_madd_epi32(u[9], k32_m10_p22);
//                        v[22] = k_madd_epi32(u[10], k32_m10_p22);
//                        v[23] = k_madd_epi32(u[11], k32_m10_p22);
//                        v[24] = k_madd_epi32(u[4], k32_m18_p14);
//                        v[25] = k_madd_epi32(u[5], k32_m18_p14);
//                        v[26] = k_madd_epi32(u[6], k32_m18_p14);
//                        v[27] = k_madd_epi32(u[7], k32_m18_p14);
//                        v[28] = k_madd_epi32(u[0], k32_m02_p30);
//                        v[29] = k_madd_epi32(u[1], k32_m02_p30);
//                        v[30] = k_madd_epi32(u[2], k32_m02_p30);
//                        v[31] = k_madd_epi32(u[3], k32_m02_p30);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_32(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
//                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
//                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//                        u[8] = k_packs_epi64(v[16], v[17]);
//                        u[9] = k_packs_epi64(v[18], v[19]);
//                        u[10] = k_packs_epi64(v[20], v[21]);
//                        u[11] = k_packs_epi64(v[22], v[23]);
//                        u[12] = k_packs_epi64(v[24], v[25]);
//                        u[13] = k_packs_epi64(v[26], v[27]);
//                        u[14] = k_packs_epi64(v[28], v[29]);
//                        u[15] = k_packs_epi64(v[30], v[31]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
//                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
//                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
//                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
//                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
//                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
//                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
//                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);
//
//                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
//                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
//                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
//                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
//                        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
//                        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
//                        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
//                        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
//                        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
//                        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
//                        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
//                        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
//
//                        v[0] = _mm_cmplt_epi32(u[0], kZero);
//                        v[1] = _mm_cmplt_epi32(u[1], kZero);
//                        v[2] = _mm_cmplt_epi32(u[2], kZero);
//                        v[3] = _mm_cmplt_epi32(u[3], kZero);
//                        v[4] = _mm_cmplt_epi32(u[4], kZero);
//                        v[5] = _mm_cmplt_epi32(u[5], kZero);
//                        v[6] = _mm_cmplt_epi32(u[6], kZero);
//                        v[7] = _mm_cmplt_epi32(u[7], kZero);
//                        v[8] = _mm_cmplt_epi32(u[8], kZero);
//                        v[9] = _mm_cmplt_epi32(u[9], kZero);
//                        v[10] = _mm_cmplt_epi32(u[10], kZero);
//                        v[11] = _mm_cmplt_epi32(u[11], kZero);
//                        v[12] = _mm_cmplt_epi32(u[12], kZero);
//                        v[13] = _mm_cmplt_epi32(u[13], kZero);
//                        v[14] = _mm_cmplt_epi32(u[14], kZero);
//                        v[15] = _mm_cmplt_epi32(u[15], kZero);
//
//                        u[0] = _mm_sub_epi32(u[0], v[0]);
//                        u[1] = _mm_sub_epi32(u[1], v[1]);
//                        u[2] = _mm_sub_epi32(u[2], v[2]);
//                        u[3] = _mm_sub_epi32(u[3], v[3]);
//                        u[4] = _mm_sub_epi32(u[4], v[4]);
//                        u[5] = _mm_sub_epi32(u[5], v[5]);
//                        u[6] = _mm_sub_epi32(u[6], v[6]);
//                        u[7] = _mm_sub_epi32(u[7], v[7]);
//                        u[8] = _mm_sub_epi32(u[8], v[8]);
//                        u[9] = _mm_sub_epi32(u[9], v[9]);
//                        u[10] = _mm_sub_epi32(u[10], v[10]);
//                        u[11] = _mm_sub_epi32(u[11], v[11]);
//                        u[12] = _mm_sub_epi32(u[12], v[12]);
//                        u[13] = _mm_sub_epi32(u[13], v[13]);
//                        u[14] = _mm_sub_epi32(u[14], v[14]);
//                        u[15] = _mm_sub_epi32(u[15], v[15]);
//
//                        v[0] = _mm_add_epi32(u[0], K32One);
//                        v[1] = _mm_add_epi32(u[1], K32One);
//                        v[2] = _mm_add_epi32(u[2], K32One);
//                        v[3] = _mm_add_epi32(u[3], K32One);
//                        v[4] = _mm_add_epi32(u[4], K32One);
//                        v[5] = _mm_add_epi32(u[5], K32One);
//                        v[6] = _mm_add_epi32(u[6], K32One);
//                        v[7] = _mm_add_epi32(u[7], K32One);
//                        v[8] = _mm_add_epi32(u[8], K32One);
//                        v[9] = _mm_add_epi32(u[9], K32One);
//                        v[10] = _mm_add_epi32(u[10], K32One);
//                        v[11] = _mm_add_epi32(u[11], K32One);
//                        v[12] = _mm_add_epi32(u[12], K32One);
//                        v[13] = _mm_add_epi32(u[13], K32One);
//                        v[14] = _mm_add_epi32(u[14], K32One);
//                        v[15] = _mm_add_epi32(u[15], K32One);
//
//                        u[0] = _mm_srai_epi32(v[0], 2);
//                        u[1] = _mm_srai_epi32(v[1], 2);
//                        u[2] = _mm_srai_epi32(v[2], 2);
//                        u[3] = _mm_srai_epi32(v[3], 2);
//                        u[4] = _mm_srai_epi32(v[4], 2);
//                        u[5] = _mm_srai_epi32(v[5], 2);
//                        u[6] = _mm_srai_epi32(v[6], 2);
//                        u[7] = _mm_srai_epi32(v[7], 2);
//                        u[8] = _mm_srai_epi32(v[8], 2);
//                        u[9] = _mm_srai_epi32(v[9], 2);
//                        u[10] = _mm_srai_epi32(v[10], 2);
//                        u[11] = _mm_srai_epi32(v[11], 2);
//                        u[12] = _mm_srai_epi32(v[12], 2);
//                        u[13] = _mm_srai_epi32(v[13], 2);
//                        u[14] = _mm_srai_epi32(v[14], 2);
//                        u[15] = _mm_srai_epi32(v[15], 2);
//
//                        out[2] = _mm_packs_epi32(u[0], u[1]);
//                        out[18] = _mm_packs_epi32(u[2], u[3]);
//                        out[10] = _mm_packs_epi32(u[4], u[5]);
//                        out[26] = _mm_packs_epi32(u[6], u[7]);
//                        out[6] = _mm_packs_epi32(u[8], u[9]);
//                        out[22] = _mm_packs_epi32(u[10], u[11]);
//                        out[14] = _mm_packs_epi32(u[12], u[13]);
//                        out[30] = _mm_packs_epi32(u[14], u[15]);
//#if DCT_HIGH_BIT_DEPTH
//                        overflow =
//                            check_epi16_overflow_x8(&out[2], &out[18], &out[10], &out[26],
//                            &out[6], &out[22], &out[14], &out[30]);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                    }
//                    {
//                        lstep1[32] = _mm_add_epi32(lstep3[34], lstep2[32]);
//                        lstep1[33] = _mm_add_epi32(lstep3[35], lstep2[33]);
//                        lstep1[34] = _mm_sub_epi32(lstep2[32], lstep3[34]);
//                        lstep1[35] = _mm_sub_epi32(lstep2[33], lstep3[35]);
//                        lstep1[36] = _mm_sub_epi32(lstep2[38], lstep3[36]);
//                        lstep1[37] = _mm_sub_epi32(lstep2[39], lstep3[37]);
//                        lstep1[38] = _mm_add_epi32(lstep3[36], lstep2[38]);
//                        lstep1[39] = _mm_add_epi32(lstep3[37], lstep2[39]);
//                        lstep1[40] = _mm_add_epi32(lstep3[42], lstep2[40]);
//                        lstep1[41] = _mm_add_epi32(lstep3[43], lstep2[41]);
//                        lstep1[42] = _mm_sub_epi32(lstep2[40], lstep3[42]);
//                        lstep1[43] = _mm_sub_epi32(lstep2[41], lstep3[43]);
//                        lstep1[44] = _mm_sub_epi32(lstep2[46], lstep3[44]);
//                        lstep1[45] = _mm_sub_epi32(lstep2[47], lstep3[45]);
//                        lstep1[46] = _mm_add_epi32(lstep3[44], lstep2[46]);
//                        lstep1[47] = _mm_add_epi32(lstep3[45], lstep2[47]);
//                        lstep1[48] = _mm_add_epi32(lstep3[50], lstep2[48]);
//                        lstep1[49] = _mm_add_epi32(lstep3[51], lstep2[49]);
//                        lstep1[50] = _mm_sub_epi32(lstep2[48], lstep3[50]);
//                        lstep1[51] = _mm_sub_epi32(lstep2[49], lstep3[51]);
//                        lstep1[52] = _mm_sub_epi32(lstep2[54], lstep3[52]);
//                        lstep1[53] = _mm_sub_epi32(lstep2[55], lstep3[53]);
//                        lstep1[54] = _mm_add_epi32(lstep3[52], lstep2[54]);
//                        lstep1[55] = _mm_add_epi32(lstep3[53], lstep2[55]);
//                        lstep1[56] = _mm_add_epi32(lstep3[58], lstep2[56]);
//                        lstep1[57] = _mm_add_epi32(lstep3[59], lstep2[57]);
//                        lstep1[58] = _mm_sub_epi32(lstep2[56], lstep3[58]);
//                        lstep1[59] = _mm_sub_epi32(lstep2[57], lstep3[59]);
//                        lstep1[60] = _mm_sub_epi32(lstep2[62], lstep3[60]);
//                        lstep1[61] = _mm_sub_epi32(lstep2[63], lstep3[61]);
//                        lstep1[62] = _mm_add_epi32(lstep3[60], lstep2[62]);
//                        lstep1[63] = _mm_add_epi32(lstep3[61], lstep2[63]);
//                    }
//                    // stage 8
//                    {
//                        const __m128i k32_p31_p01 = pair_set_epi32(cospi_31_64, cospi_1_64);
//                        const __m128i k32_p15_p17 = pair_set_epi32(cospi_15_64, cospi_17_64);
//                        const __m128i k32_p23_p09 = pair_set_epi32(cospi_23_64, cospi_9_64);
//                        const __m128i k32_p07_p25 = pair_set_epi32(cospi_7_64, cospi_25_64);
//                        const __m128i k32_m25_p07 = pair_set_epi32(-cospi_25_64, cospi_7_64);
//                        const __m128i k32_m09_p23 = pair_set_epi32(-cospi_9_64, cospi_23_64);
//                        const __m128i k32_m17_p15 = pair_set_epi32(-cospi_17_64, cospi_15_64);
//                        const __m128i k32_m01_p31 = pair_set_epi32(-cospi_1_64, cospi_31_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep1[32], lstep1[62]);
//                        u[1] = _mm_unpackhi_epi32(lstep1[32], lstep1[62]);
//                        u[2] = _mm_unpacklo_epi32(lstep1[33], lstep1[63]);
//                        u[3] = _mm_unpackhi_epi32(lstep1[33], lstep1[63]);
//                        u[4] = _mm_unpacklo_epi32(lstep1[34], lstep1[60]);
//                        u[5] = _mm_unpackhi_epi32(lstep1[34], lstep1[60]);
//                        u[6] = _mm_unpacklo_epi32(lstep1[35], lstep1[61]);
//                        u[7] = _mm_unpackhi_epi32(lstep1[35], lstep1[61]);
//                        u[8] = _mm_unpacklo_epi32(lstep1[36], lstep1[58]);
//                        u[9] = _mm_unpackhi_epi32(lstep1[36], lstep1[58]);
//                        u[10] = _mm_unpacklo_epi32(lstep1[37], lstep1[59]);
//                        u[11] = _mm_unpackhi_epi32(lstep1[37], lstep1[59]);
//                        u[12] = _mm_unpacklo_epi32(lstep1[38], lstep1[56]);
//                        u[13] = _mm_unpackhi_epi32(lstep1[38], lstep1[56]);
//                        u[14] = _mm_unpacklo_epi32(lstep1[39], lstep1[57]);
//                        u[15] = _mm_unpackhi_epi32(lstep1[39], lstep1[57]);
//
//                        v[0] = k_madd_epi32(u[0], k32_p31_p01);
//                        v[1] = k_madd_epi32(u[1], k32_p31_p01);
//                        v[2] = k_madd_epi32(u[2], k32_p31_p01);
//                        v[3] = k_madd_epi32(u[3], k32_p31_p01);
//                        v[4] = k_madd_epi32(u[4], k32_p15_p17);
//                        v[5] = k_madd_epi32(u[5], k32_p15_p17);
//                        v[6] = k_madd_epi32(u[6], k32_p15_p17);
//                        v[7] = k_madd_epi32(u[7], k32_p15_p17);
//                        v[8] = k_madd_epi32(u[8], k32_p23_p09);
//                        v[9] = k_madd_epi32(u[9], k32_p23_p09);
//                        v[10] = k_madd_epi32(u[10], k32_p23_p09);
//                        v[11] = k_madd_epi32(u[11], k32_p23_p09);
//                        v[12] = k_madd_epi32(u[12], k32_p07_p25);
//                        v[13] = k_madd_epi32(u[13], k32_p07_p25);
//                        v[14] = k_madd_epi32(u[14], k32_p07_p25);
//                        v[15] = k_madd_epi32(u[15], k32_p07_p25);
//                        v[16] = k_madd_epi32(u[12], k32_m25_p07);
//                        v[17] = k_madd_epi32(u[13], k32_m25_p07);
//                        v[18] = k_madd_epi32(u[14], k32_m25_p07);
//                        v[19] = k_madd_epi32(u[15], k32_m25_p07);
//                        v[20] = k_madd_epi32(u[8], k32_m09_p23);
//                        v[21] = k_madd_epi32(u[9], k32_m09_p23);
//                        v[22] = k_madd_epi32(u[10], k32_m09_p23);
//                        v[23] = k_madd_epi32(u[11], k32_m09_p23);
//                        v[24] = k_madd_epi32(u[4], k32_m17_p15);
//                        v[25] = k_madd_epi32(u[5], k32_m17_p15);
//                        v[26] = k_madd_epi32(u[6], k32_m17_p15);
//                        v[27] = k_madd_epi32(u[7], k32_m17_p15);
//                        v[28] = k_madd_epi32(u[0], k32_m01_p31);
//                        v[29] = k_madd_epi32(u[1], k32_m01_p31);
//                        v[30] = k_madd_epi32(u[2], k32_m01_p31);
//                        v[31] = k_madd_epi32(u[3], k32_m01_p31);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_32(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
//                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
//                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//                        u[8] = k_packs_epi64(v[16], v[17]);
//                        u[9] = k_packs_epi64(v[18], v[19]);
//                        u[10] = k_packs_epi64(v[20], v[21]);
//                        u[11] = k_packs_epi64(v[22], v[23]);
//                        u[12] = k_packs_epi64(v[24], v[25]);
//                        u[13] = k_packs_epi64(v[26], v[27]);
//                        u[14] = k_packs_epi64(v[28], v[29]);
//                        u[15] = k_packs_epi64(v[30], v[31]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
//                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
//                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
//                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
//                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
//                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
//                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
//                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);
//
//                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
//                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
//                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
//                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
//                        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
//                        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
//                        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
//                        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
//                        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
//                        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
//                        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
//                        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
//
//                        v[0] = _mm_cmplt_epi32(u[0], kZero);
//                        v[1] = _mm_cmplt_epi32(u[1], kZero);
//                        v[2] = _mm_cmplt_epi32(u[2], kZero);
//                        v[3] = _mm_cmplt_epi32(u[3], kZero);
//                        v[4] = _mm_cmplt_epi32(u[4], kZero);
//                        v[5] = _mm_cmplt_epi32(u[5], kZero);
//                        v[6] = _mm_cmplt_epi32(u[6], kZero);
//                        v[7] = _mm_cmplt_epi32(u[7], kZero);
//                        v[8] = _mm_cmplt_epi32(u[8], kZero);
//                        v[9] = _mm_cmplt_epi32(u[9], kZero);
//                        v[10] = _mm_cmplt_epi32(u[10], kZero);
//                        v[11] = _mm_cmplt_epi32(u[11], kZero);
//                        v[12] = _mm_cmplt_epi32(u[12], kZero);
//                        v[13] = _mm_cmplt_epi32(u[13], kZero);
//                        v[14] = _mm_cmplt_epi32(u[14], kZero);
//                        v[15] = _mm_cmplt_epi32(u[15], kZero);
//
//                        u[0] = _mm_sub_epi32(u[0], v[0]);
//                        u[1] = _mm_sub_epi32(u[1], v[1]);
//                        u[2] = _mm_sub_epi32(u[2], v[2]);
//                        u[3] = _mm_sub_epi32(u[3], v[3]);
//                        u[4] = _mm_sub_epi32(u[4], v[4]);
//                        u[5] = _mm_sub_epi32(u[5], v[5]);
//                        u[6] = _mm_sub_epi32(u[6], v[6]);
//                        u[7] = _mm_sub_epi32(u[7], v[7]);
//                        u[8] = _mm_sub_epi32(u[8], v[8]);
//                        u[9] = _mm_sub_epi32(u[9], v[9]);
//                        u[10] = _mm_sub_epi32(u[10], v[10]);
//                        u[11] = _mm_sub_epi32(u[11], v[11]);
//                        u[12] = _mm_sub_epi32(u[12], v[12]);
//                        u[13] = _mm_sub_epi32(u[13], v[13]);
//                        u[14] = _mm_sub_epi32(u[14], v[14]);
//                        u[15] = _mm_sub_epi32(u[15], v[15]);
//
//                        v[0] = _mm_add_epi32(u[0], K32One);
//                        v[1] = _mm_add_epi32(u[1], K32One);
//                        v[2] = _mm_add_epi32(u[2], K32One);
//                        v[3] = _mm_add_epi32(u[3], K32One);
//                        v[4] = _mm_add_epi32(u[4], K32One);
//                        v[5] = _mm_add_epi32(u[5], K32One);
//                        v[6] = _mm_add_epi32(u[6], K32One);
//                        v[7] = _mm_add_epi32(u[7], K32One);
//                        v[8] = _mm_add_epi32(u[8], K32One);
//                        v[9] = _mm_add_epi32(u[9], K32One);
//                        v[10] = _mm_add_epi32(u[10], K32One);
//                        v[11] = _mm_add_epi32(u[11], K32One);
//                        v[12] = _mm_add_epi32(u[12], K32One);
//                        v[13] = _mm_add_epi32(u[13], K32One);
//                        v[14] = _mm_add_epi32(u[14], K32One);
//                        v[15] = _mm_add_epi32(u[15], K32One);
//
//                        u[0] = _mm_srai_epi32(v[0], 2);
//                        u[1] = _mm_srai_epi32(v[1], 2);
//                        u[2] = _mm_srai_epi32(v[2], 2);
//                        u[3] = _mm_srai_epi32(v[3], 2);
//                        u[4] = _mm_srai_epi32(v[4], 2);
//                        u[5] = _mm_srai_epi32(v[5], 2);
//                        u[6] = _mm_srai_epi32(v[6], 2);
//                        u[7] = _mm_srai_epi32(v[7], 2);
//                        u[8] = _mm_srai_epi32(v[8], 2);
//                        u[9] = _mm_srai_epi32(v[9], 2);
//                        u[10] = _mm_srai_epi32(v[10], 2);
//                        u[11] = _mm_srai_epi32(v[11], 2);
//                        u[12] = _mm_srai_epi32(v[12], 2);
//                        u[13] = _mm_srai_epi32(v[13], 2);
//                        u[14] = _mm_srai_epi32(v[14], 2);
//                        u[15] = _mm_srai_epi32(v[15], 2);
//
//                        out[1] = _mm_packs_epi32(u[0], u[1]);
//                        out[17] = _mm_packs_epi32(u[2], u[3]);
//                        out[9] = _mm_packs_epi32(u[4], u[5]);
//                        out[25] = _mm_packs_epi32(u[6], u[7]);
//                        out[7] = _mm_packs_epi32(u[8], u[9]);
//                        out[23] = _mm_packs_epi32(u[10], u[11]);
//                        out[15] = _mm_packs_epi32(u[12], u[13]);
//                        out[31] = _mm_packs_epi32(u[14], u[15]);
//#if DCT_HIGH_BIT_DEPTH
//                        overflow =
//                            check_epi16_overflow_x8(&out[1], &out[17], &out[9], &out[25],
//                            &out[7], &out[23], &out[15], &out[31]);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                    }
//                    {
//                        const __m128i k32_p27_p05 = pair_set_epi32(cospi_27_64, cospi_5_64);
//                        const __m128i k32_p11_p21 = pair_set_epi32(cospi_11_64, cospi_21_64);
//                        const __m128i k32_p19_p13 = pair_set_epi32(cospi_19_64, cospi_13_64);
//                        const __m128i k32_p03_p29 = pair_set_epi32(cospi_3_64, cospi_29_64);
//                        const __m128i k32_m29_p03 = pair_set_epi32(-cospi_29_64, cospi_3_64);
//                        const __m128i k32_m13_p19 = pair_set_epi32(-cospi_13_64, cospi_19_64);
//                        const __m128i k32_m21_p11 = pair_set_epi32(-cospi_21_64, cospi_11_64);
//                        const __m128i k32_m05_p27 = pair_set_epi32(-cospi_5_64, cospi_27_64);
//
//                        u[0] = _mm_unpacklo_epi32(lstep1[40], lstep1[54]);
//                        u[1] = _mm_unpackhi_epi32(lstep1[40], lstep1[54]);
//                        u[2] = _mm_unpacklo_epi32(lstep1[41], lstep1[55]);
//                        u[3] = _mm_unpackhi_epi32(lstep1[41], lstep1[55]);
//                        u[4] = _mm_unpacklo_epi32(lstep1[42], lstep1[52]);
//                        u[5] = _mm_unpackhi_epi32(lstep1[42], lstep1[52]);
//                        u[6] = _mm_unpacklo_epi32(lstep1[43], lstep1[53]);
//                        u[7] = _mm_unpackhi_epi32(lstep1[43], lstep1[53]);
//                        u[8] = _mm_unpacklo_epi32(lstep1[44], lstep1[50]);
//                        u[9] = _mm_unpackhi_epi32(lstep1[44], lstep1[50]);
//                        u[10] = _mm_unpacklo_epi32(lstep1[45], lstep1[51]);
//                        u[11] = _mm_unpackhi_epi32(lstep1[45], lstep1[51]);
//                        u[12] = _mm_unpacklo_epi32(lstep1[46], lstep1[48]);
//                        u[13] = _mm_unpackhi_epi32(lstep1[46], lstep1[48]);
//                        u[14] = _mm_unpacklo_epi32(lstep1[47], lstep1[49]);
//                        u[15] = _mm_unpackhi_epi32(lstep1[47], lstep1[49]);
//
//                        v[0] = k_madd_epi32(u[0], k32_p27_p05);
//                        v[1] = k_madd_epi32(u[1], k32_p27_p05);
//                        v[2] = k_madd_epi32(u[2], k32_p27_p05);
//                        v[3] = k_madd_epi32(u[3], k32_p27_p05);
//                        v[4] = k_madd_epi32(u[4], k32_p11_p21);
//                        v[5] = k_madd_epi32(u[5], k32_p11_p21);
//                        v[6] = k_madd_epi32(u[6], k32_p11_p21);
//                        v[7] = k_madd_epi32(u[7], k32_p11_p21);
//                        v[8] = k_madd_epi32(u[8], k32_p19_p13);
//                        v[9] = k_madd_epi32(u[9], k32_p19_p13);
//                        v[10] = k_madd_epi32(u[10], k32_p19_p13);
//                        v[11] = k_madd_epi32(u[11], k32_p19_p13);
//                        v[12] = k_madd_epi32(u[12], k32_p03_p29);
//                        v[13] = k_madd_epi32(u[13], k32_p03_p29);
//                        v[14] = k_madd_epi32(u[14], k32_p03_p29);
//                        v[15] = k_madd_epi32(u[15], k32_p03_p29);
//                        v[16] = k_madd_epi32(u[12], k32_m29_p03);
//                        v[17] = k_madd_epi32(u[13], k32_m29_p03);
//                        v[18] = k_madd_epi32(u[14], k32_m29_p03);
//                        v[19] = k_madd_epi32(u[15], k32_m29_p03);
//                        v[20] = k_madd_epi32(u[8], k32_m13_p19);
//                        v[21] = k_madd_epi32(u[9], k32_m13_p19);
//                        v[22] = k_madd_epi32(u[10], k32_m13_p19);
//                        v[23] = k_madd_epi32(u[11], k32_m13_p19);
//                        v[24] = k_madd_epi32(u[4], k32_m21_p11);
//                        v[25] = k_madd_epi32(u[5], k32_m21_p11);
//                        v[26] = k_madd_epi32(u[6], k32_m21_p11);
//                        v[27] = k_madd_epi32(u[7], k32_m21_p11);
//                        v[28] = k_madd_epi32(u[0], k32_m05_p27);
//                        v[29] = k_madd_epi32(u[1], k32_m05_p27);
//                        v[30] = k_madd_epi32(u[2], k32_m05_p27);
//                        v[31] = k_madd_epi32(u[3], k32_m05_p27);
//
//#if DCT_HIGH_BIT_DEPTH
//                        overflow = k_check_epi32_overflow_32(
//                            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8],
//                            &v[9], &v[10], &v[11], &v[12], &v[13], &v[14], &v[15], &v[16],
//                            &v[17], &v[18], &v[19], &v[20], &v[21], &v[22], &v[23], &v[24],
//                            &v[25], &v[26], &v[27], &v[28], &v[29], &v[30], &v[31], &kZero);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                        u[0] = k_packs_epi64(v[0], v[1]);
//                        u[1] = k_packs_epi64(v[2], v[3]);
//                        u[2] = k_packs_epi64(v[4], v[5]);
//                        u[3] = k_packs_epi64(v[6], v[7]);
//                        u[4] = k_packs_epi64(v[8], v[9]);
//                        u[5] = k_packs_epi64(v[10], v[11]);
//                        u[6] = k_packs_epi64(v[12], v[13]);
//                        u[7] = k_packs_epi64(v[14], v[15]);
//                        u[8] = k_packs_epi64(v[16], v[17]);
//                        u[9] = k_packs_epi64(v[18], v[19]);
//                        u[10] = k_packs_epi64(v[20], v[21]);
//                        u[11] = k_packs_epi64(v[22], v[23]);
//                        u[12] = k_packs_epi64(v[24], v[25]);
//                        u[13] = k_packs_epi64(v[26], v[27]);
//                        u[14] = k_packs_epi64(v[28], v[29]);
//                        u[15] = k_packs_epi64(v[30], v[31]);
//
//                        v[0] = _mm_add_epi32(u[0], k__DCT_CONST_ROUNDING);
//                        v[1] = _mm_add_epi32(u[1], k__DCT_CONST_ROUNDING);
//                        v[2] = _mm_add_epi32(u[2], k__DCT_CONST_ROUNDING);
//                        v[3] = _mm_add_epi32(u[3], k__DCT_CONST_ROUNDING);
//                        v[4] = _mm_add_epi32(u[4], k__DCT_CONST_ROUNDING);
//                        v[5] = _mm_add_epi32(u[5], k__DCT_CONST_ROUNDING);
//                        v[6] = _mm_add_epi32(u[6], k__DCT_CONST_ROUNDING);
//                        v[7] = _mm_add_epi32(u[7], k__DCT_CONST_ROUNDING);
//                        v[8] = _mm_add_epi32(u[8], k__DCT_CONST_ROUNDING);
//                        v[9] = _mm_add_epi32(u[9], k__DCT_CONST_ROUNDING);
//                        v[10] = _mm_add_epi32(u[10], k__DCT_CONST_ROUNDING);
//                        v[11] = _mm_add_epi32(u[11], k__DCT_CONST_ROUNDING);
//                        v[12] = _mm_add_epi32(u[12], k__DCT_CONST_ROUNDING);
//                        v[13] = _mm_add_epi32(u[13], k__DCT_CONST_ROUNDING);
//                        v[14] = _mm_add_epi32(u[14], k__DCT_CONST_ROUNDING);
//                        v[15] = _mm_add_epi32(u[15], k__DCT_CONST_ROUNDING);
//
//                        u[0] = _mm_srai_epi32(v[0], DCT_CONST_BITS);
//                        u[1] = _mm_srai_epi32(v[1], DCT_CONST_BITS);
//                        u[2] = _mm_srai_epi32(v[2], DCT_CONST_BITS);
//                        u[3] = _mm_srai_epi32(v[3], DCT_CONST_BITS);
//                        u[4] = _mm_srai_epi32(v[4], DCT_CONST_BITS);
//                        u[5] = _mm_srai_epi32(v[5], DCT_CONST_BITS);
//                        u[6] = _mm_srai_epi32(v[6], DCT_CONST_BITS);
//                        u[7] = _mm_srai_epi32(v[7], DCT_CONST_BITS);
//                        u[8] = _mm_srai_epi32(v[8], DCT_CONST_BITS);
//                        u[9] = _mm_srai_epi32(v[9], DCT_CONST_BITS);
//                        u[10] = _mm_srai_epi32(v[10], DCT_CONST_BITS);
//                        u[11] = _mm_srai_epi32(v[11], DCT_CONST_BITS);
//                        u[12] = _mm_srai_epi32(v[12], DCT_CONST_BITS);
//                        u[13] = _mm_srai_epi32(v[13], DCT_CONST_BITS);
//                        u[14] = _mm_srai_epi32(v[14], DCT_CONST_BITS);
//                        u[15] = _mm_srai_epi32(v[15], DCT_CONST_BITS);
//
//                        v[0] = _mm_cmplt_epi32(u[0], kZero);
//                        v[1] = _mm_cmplt_epi32(u[1], kZero);
//                        v[2] = _mm_cmplt_epi32(u[2], kZero);
//                        v[3] = _mm_cmplt_epi32(u[3], kZero);
//                        v[4] = _mm_cmplt_epi32(u[4], kZero);
//                        v[5] = _mm_cmplt_epi32(u[5], kZero);
//                        v[6] = _mm_cmplt_epi32(u[6], kZero);
//                        v[7] = _mm_cmplt_epi32(u[7], kZero);
//                        v[8] = _mm_cmplt_epi32(u[8], kZero);
//                        v[9] = _mm_cmplt_epi32(u[9], kZero);
//                        v[10] = _mm_cmplt_epi32(u[10], kZero);
//                        v[11] = _mm_cmplt_epi32(u[11], kZero);
//                        v[12] = _mm_cmplt_epi32(u[12], kZero);
//                        v[13] = _mm_cmplt_epi32(u[13], kZero);
//                        v[14] = _mm_cmplt_epi32(u[14], kZero);
//                        v[15] = _mm_cmplt_epi32(u[15], kZero);
//
//                        u[0] = _mm_sub_epi32(u[0], v[0]);
//                        u[1] = _mm_sub_epi32(u[1], v[1]);
//                        u[2] = _mm_sub_epi32(u[2], v[2]);
//                        u[3] = _mm_sub_epi32(u[3], v[3]);
//                        u[4] = _mm_sub_epi32(u[4], v[4]);
//                        u[5] = _mm_sub_epi32(u[5], v[5]);
//                        u[6] = _mm_sub_epi32(u[6], v[6]);
//                        u[7] = _mm_sub_epi32(u[7], v[7]);
//                        u[8] = _mm_sub_epi32(u[8], v[8]);
//                        u[9] = _mm_sub_epi32(u[9], v[9]);
//                        u[10] = _mm_sub_epi32(u[10], v[10]);
//                        u[11] = _mm_sub_epi32(u[11], v[11]);
//                        u[12] = _mm_sub_epi32(u[12], v[12]);
//                        u[13] = _mm_sub_epi32(u[13], v[13]);
//                        u[14] = _mm_sub_epi32(u[14], v[14]);
//                        u[15] = _mm_sub_epi32(u[15], v[15]);
//
//                        v[0] = _mm_add_epi32(u[0], K32One);
//                        v[1] = _mm_add_epi32(u[1], K32One);
//                        v[2] = _mm_add_epi32(u[2], K32One);
//                        v[3] = _mm_add_epi32(u[3], K32One);
//                        v[4] = _mm_add_epi32(u[4], K32One);
//                        v[5] = _mm_add_epi32(u[5], K32One);
//                        v[6] = _mm_add_epi32(u[6], K32One);
//                        v[7] = _mm_add_epi32(u[7], K32One);
//                        v[8] = _mm_add_epi32(u[8], K32One);
//                        v[9] = _mm_add_epi32(u[9], K32One);
//                        v[10] = _mm_add_epi32(u[10], K32One);
//                        v[11] = _mm_add_epi32(u[11], K32One);
//                        v[12] = _mm_add_epi32(u[12], K32One);
//                        v[13] = _mm_add_epi32(u[13], K32One);
//                        v[14] = _mm_add_epi32(u[14], K32One);
//                        v[15] = _mm_add_epi32(u[15], K32One);
//
//                        u[0] = _mm_srai_epi32(v[0], 2);
//                        u[1] = _mm_srai_epi32(v[1], 2);
//                        u[2] = _mm_srai_epi32(v[2], 2);
//                        u[3] = _mm_srai_epi32(v[3], 2);
//                        u[4] = _mm_srai_epi32(v[4], 2);
//                        u[5] = _mm_srai_epi32(v[5], 2);
//                        u[6] = _mm_srai_epi32(v[6], 2);
//                        u[7] = _mm_srai_epi32(v[7], 2);
//                        u[8] = _mm_srai_epi32(v[8], 2);
//                        u[9] = _mm_srai_epi32(v[9], 2);
//                        u[10] = _mm_srai_epi32(v[10], 2);
//                        u[11] = _mm_srai_epi32(v[11], 2);
//                        u[12] = _mm_srai_epi32(v[12], 2);
//                        u[13] = _mm_srai_epi32(v[13], 2);
//                        u[14] = _mm_srai_epi32(v[14], 2);
//                        u[15] = _mm_srai_epi32(v[15], 2);
//
//                        out[5] = _mm_packs_epi32(u[0], u[1]);
//                        out[21] = _mm_packs_epi32(u[2], u[3]);
//                        out[13] = _mm_packs_epi32(u[4], u[5]);
//                        out[29] = _mm_packs_epi32(u[6], u[7]);
//                        out[3] = _mm_packs_epi32(u[8], u[9]);
//                        out[19] = _mm_packs_epi32(u[10], u[11]);
//                        out[11] = _mm_packs_epi32(u[12], u[13]);
//                        out[27] = _mm_packs_epi32(u[14], u[15]);
//#if DCT_HIGH_BIT_DEPTH
//                        overflow =
//                            check_epi16_overflow_x8(&out[5], &out[21], &out[13], &out[29],
//                            &out[3], &out[19], &out[11], &out[27]);
//                        if (overflow) {
//                            HIGH_FDCT32x32_2D_ROWS_C(intermediate, output_org);
//                            return;
//                        }
//#endif  // DCT_HIGH_BIT_DEPTH
//                    }
//                }
//#endif  // FDCT32x32_HIGH_PRECISION
                // Transpose the results, do it as four 8x8 transposes.
                {
                    int transpose_block;
                    int16_t *output0 = &intermediate[column_start * 32];
                    tran_low_t *output1 = &output_org[column_start * 32];
                    for (transpose_block = 0; transpose_block < 4; ++transpose_block) {
                        __m128i *this_out = &out[8 * transpose_block];
                        // 00 01 02 03 04 05 06 07
                        // 10 11 12 13 14 15 16 17
                        // 20 21 22 23 24 25 26 27
                        // 30 31 32 33 34 35 36 37
                        // 40 41 42 43 44 45 46 47
                        // 50 51 52 53 54 55 56 57
                        // 60 61 62 63 64 65 66 67
                        // 70 71 72 73 74 75 76 77
                        const __m128i tr0_0 = _mm_unpacklo_epi16(this_out[0], this_out[1]);
                        const __m128i tr0_1 = _mm_unpacklo_epi16(this_out[2], this_out[3]);
                        const __m128i tr0_2 = _mm_unpackhi_epi16(this_out[0], this_out[1]);
                        const __m128i tr0_3 = _mm_unpackhi_epi16(this_out[2], this_out[3]);
                        const __m128i tr0_4 = _mm_unpacklo_epi16(this_out[4], this_out[5]);
                        const __m128i tr0_5 = _mm_unpacklo_epi16(this_out[6], this_out[7]);
                        const __m128i tr0_6 = _mm_unpackhi_epi16(this_out[4], this_out[5]);
                        const __m128i tr0_7 = _mm_unpackhi_epi16(this_out[6], this_out[7]);
                        // 00 10 01 11 02 12 03 13
                        // 20 30 21 31 22 32 23 33
                        // 04 14 05 15 06 16 07 17
                        // 24 34 25 35 26 36 27 37
                        // 40 50 41 51 42 52 43 53
                        // 60 70 61 71 62 72 63 73
                        // 54 54 55 55 56 56 57 57
                        // 64 74 65 75 66 76 67 77
                        const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
                        const __m128i tr1_1 = _mm_unpacklo_epi32(tr0_2, tr0_3);
                        const __m128i tr1_2 = _mm_unpackhi_epi32(tr0_0, tr0_1);
                        const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3);
                        const __m128i tr1_4 = _mm_unpacklo_epi32(tr0_4, tr0_5);
                        const __m128i tr1_5 = _mm_unpacklo_epi32(tr0_6, tr0_7);
                        const __m128i tr1_6 = _mm_unpackhi_epi32(tr0_4, tr0_5);
                        const __m128i tr1_7 = _mm_unpackhi_epi32(tr0_6, tr0_7);
                        // 00 10 20 30 01 11 21 31
                        // 40 50 60 70 41 51 61 71
                        // 02 12 22 32 03 13 23 33
                        // 42 52 62 72 43 53 63 73
                        // 04 14 24 34 05 15 21 36
                        // 44 54 64 74 45 55 61 76
                        // 06 16 26 36 07 17 27 37
                        // 46 56 66 76 47 57 67 77
                        __m128i tr2_0 = _mm_unpacklo_epi64(tr1_0, tr1_4);
                        __m128i tr2_1 = _mm_unpackhi_epi64(tr1_0, tr1_4);
                        __m128i tr2_2 = _mm_unpacklo_epi64(tr1_2, tr1_6);
                        __m128i tr2_3 = _mm_unpackhi_epi64(tr1_2, tr1_6);
                        __m128i tr2_4 = _mm_unpacklo_epi64(tr1_1, tr1_5);
                        __m128i tr2_5 = _mm_unpackhi_epi64(tr1_1, tr1_5);
                        __m128i tr2_6 = _mm_unpacklo_epi64(tr1_3, tr1_7);
                        __m128i tr2_7 = _mm_unpackhi_epi64(tr1_3, tr1_7);
                        // 00 10 20 30 40 50 60 70
                        // 01 11 21 31 41 51 61 71
                        // 02 12 22 32 42 52 62 72
                        // 03 13 23 33 43 53 63 73
                        // 04 14 24 34 44 54 64 74
                        // 05 15 25 35 45 55 65 75
                        // 06 16 26 36 46 56 66 76
                        // 07 17 27 37 47 57 67 77
                        if (0 == pass) {
                            // output[j] = (output[j] + 1 + (output[j] > 0)) >> 2;
                            // TODO(cd): see quality impact of only doing
                            //           output[j] = (output[j] + 1) >> 2;
                            //           which would remove the code between here ...
                            __m128i tr2_0_0 = _mm_cmpgt_epi16(tr2_0, kZero);
                            __m128i tr2_1_0 = _mm_cmpgt_epi16(tr2_1, kZero);
                            __m128i tr2_2_0 = _mm_cmpgt_epi16(tr2_2, kZero);
                            __m128i tr2_3_0 = _mm_cmpgt_epi16(tr2_3, kZero);
                            __m128i tr2_4_0 = _mm_cmpgt_epi16(tr2_4, kZero);
                            __m128i tr2_5_0 = _mm_cmpgt_epi16(tr2_5, kZero);
                            __m128i tr2_6_0 = _mm_cmpgt_epi16(tr2_6, kZero);
                            __m128i tr2_7_0 = _mm_cmpgt_epi16(tr2_7, kZero);
                            tr2_0 = _mm_sub_epi16(tr2_0, tr2_0_0);
                            tr2_1 = _mm_sub_epi16(tr2_1, tr2_1_0);
                            tr2_2 = _mm_sub_epi16(tr2_2, tr2_2_0);
                            tr2_3 = _mm_sub_epi16(tr2_3, tr2_3_0);
                            tr2_4 = _mm_sub_epi16(tr2_4, tr2_4_0);
                            tr2_5 = _mm_sub_epi16(tr2_5, tr2_5_0);
                            tr2_6 = _mm_sub_epi16(tr2_6, tr2_6_0);
                            tr2_7 = _mm_sub_epi16(tr2_7, tr2_7_0);
                            //           ... and here.
                            //           PS: also change code in vp9/encoder/vp9_dct.c
                            tr2_0 = _mm_add_epi16(tr2_0, kOne);
                            tr2_1 = _mm_add_epi16(tr2_1, kOne);
                            tr2_2 = _mm_add_epi16(tr2_2, kOne);
                            tr2_3 = _mm_add_epi16(tr2_3, kOne);
                            tr2_4 = _mm_add_epi16(tr2_4, kOne);
                            tr2_5 = _mm_add_epi16(tr2_5, kOne);
                            tr2_6 = _mm_add_epi16(tr2_6, kOne);
                            tr2_7 = _mm_add_epi16(tr2_7, kOne);
                            tr2_0 = _mm_srai_epi16(tr2_0, 2);
                            tr2_1 = _mm_srai_epi16(tr2_1, 2);
                            tr2_2 = _mm_srai_epi16(tr2_2, 2);
                            tr2_3 = _mm_srai_epi16(tr2_3, 2);
                            tr2_4 = _mm_srai_epi16(tr2_4, 2);
                            tr2_5 = _mm_srai_epi16(tr2_5, 2);
                            tr2_6 = _mm_srai_epi16(tr2_6, 2);
                            tr2_7 = _mm_srai_epi16(tr2_7, 2);
                        }
                        // Note: even though all these stores are aligned, using the aligned
                        //       intrinsic make the code slightly slower.
                        if (pass == 0) {
                            _mm_storeu_si128((__m128i *)(output0 + 0 * 32), tr2_0);
                            _mm_storeu_si128((__m128i *)(output0 + 1 * 32), tr2_1);
                            _mm_storeu_si128((__m128i *)(output0 + 2 * 32), tr2_2);
                            _mm_storeu_si128((__m128i *)(output0 + 3 * 32), tr2_3);
                            _mm_storeu_si128((__m128i *)(output0 + 4 * 32), tr2_4);
                            _mm_storeu_si128((__m128i *)(output0 + 5 * 32), tr2_5);
                            _mm_storeu_si128((__m128i *)(output0 + 6 * 32), tr2_6);
                            _mm_storeu_si128((__m128i *)(output0 + 7 * 32), tr2_7);
                            // Process next 8x8
                            output0 += 8;
                        } else {
                            storeu_output(&tr2_0, (output1 + 0 * 32));
                            storeu_output(&tr2_1, (output1 + 1 * 32));
                            storeu_output(&tr2_2, (output1 + 2 * 32));
                            storeu_output(&tr2_3, (output1 + 3 * 32));
                            storeu_output(&tr2_4, (output1 + 4 * 32));
                            storeu_output(&tr2_5, (output1 + 5 * 32));
                            storeu_output(&tr2_6, (output1 + 6 * 32));
                            storeu_output(&tr2_7, (output1 + 7 * 32));
                            // Process next 8x8
                            output1 += 8;
                        }
                    }
                }
            }
        }
    }  // NOLINT
}; }; // namespace details

namespace AV1PP
{
    enum { TX_4X4=0, TX_8X8=1, TX_16X16=2, TX_32X32=3, TX_SIZES=4 };
    enum { DCT_DCT=0, ADST_DCT=1, DCT_ADST=2, ADST_ADST=3 };

    template <int size, int type> void ftransform_vp9_sse2(const int16_t *src, int16_t *dst, int pitchSrc);

    template <> void ftransform_vp9_sse2<TX_4X4, DCT_DCT>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht4x4_sse2<DCT_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_4X4, ADST_DCT>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht4x4_sse2<ADST_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_4X4, DCT_ADST>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht4x4_sse2<DCT_ADST>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_4X4, ADST_ADST>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht4x4_sse2<ADST_ADST>(src, dst, pitchSrc);
    }

    template <> void ftransform_vp9_sse2<TX_8X8, DCT_DCT>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht8x8_sse2<DCT_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_8X8, ADST_DCT>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht8x8_sse2<ADST_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_8X8, DCT_ADST>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht8x8_sse2<DCT_ADST>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_8X8, ADST_ADST>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht8x8_sse2<ADST_ADST>(src, dst, pitchSrc);
    }

    template <> void ftransform_vp9_sse2<TX_16X16, DCT_DCT>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht16x16_sse2<DCT_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_16X16, ADST_DCT>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht16x16_sse2<ADST_DCT>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_16X16, DCT_ADST>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht16x16_sse2<DCT_ADST>(src, dst, pitchSrc);
    }
    template <> void ftransform_vp9_sse2<TX_16X16, ADST_ADST>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fht16x16_sse2<ADST_ADST>(src, dst, pitchSrc);
    }

    template <> void ftransform_vp9_sse2<TX_32X32, DCT_DCT>(const int16_t *src, int16_t *dst, int pitchSrc) {
        details::fdct32x32_sse2(src, dst, pitchSrc);
    }

    void fdct32x32_fast_sse2(const short *src, short *dst, int pitchSrc) {
        return details::fdct32x32_fast_sse2(src, dst, pitchSrc);
    }
}; // namespace AV1PP
