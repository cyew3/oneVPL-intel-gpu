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



#pragma once

#include "mfx_av1_defs.h"
#include "mfx_av1_trace.h"

#ifdef _DEBUG
#define AV1_BITCOUNT
#endif

namespace AV1Enc {

    typedef uint32_t od_ec_window;

    /*The entropy encoder context.*/
    struct od_ec_enc {
        /*Buffered output.
        This contains only the raw bits until the final call to od_ec_enc_done(),
        where all the arithmetic-coded data gets prepended to it.*/
        uint8_t *buf;
        /*The size of the buffer.*/
        uint32_t storage;
        /*The offset at which the last byte containing raw bits was written.*/
        uint32_t end_offs;
        /*Bits that will be read from/written at the end.*/
        od_ec_window end_window;
        /*Number of valid bits in end_window.*/
        int32_t nend_bits;
        /*A buffer for output bytes with their associated carry flags.*/
        uint16_t *precarry_buf;
        /*The size of the pre-carry buffer.*/
        uint32_t precarry_storage;
        /*The offset at which the next entropy-coded byte will be written.*/
        uint32_t offs;
        /*The low end of the current range.*/
        od_ec_window low;
        /*The number of values in the current range.*/
        uint16_t rng;
        /*The number of bits of data in the current value.*/
        int16_t cnt;
        /*Nonzero if an error occurred.*/
        int error;
    };

    struct BoolCoder {
        uint32_t pos;
        uint8_t *buffer;
        union {
            struct// BoolCoderVP9
            {
                uint32_t lowvalue;
                uint32_t range;
                int32_t count;
            };
            struct// BoolCoderAV1
            {
                od_ec_enc ec;
            };
        };
    };

    void od_ec_enc_reset(od_ec_enc *enc);
    void od_ec_enc_init(od_ec_enc *enc, uint8_t *buf, uint16_t *precarry_buf, int32_t size);
    unsigned char *od_ec_enc_done(od_ec_enc *enc, uint32_t *nbytes);
    //void od_ec_enc_clear(od_ec_enc *enc);
    void od_ec_enc_bits(od_ec_enc *enc, uint32_t fl, unsigned ftb);
    void od_ec_encode_q15(od_ec_enc *enc, unsigned fl, unsigned fh, int s, int nsyms);
    void od_ec_encode_q15_0(od_ec_enc *enc, unsigned fh);
    void od_ec_encode_bool_q15(od_ec_enc *enc, int val, unsigned fz);
    void od_ec_encode_bool_q15_128(od_ec_enc *enc, int val);
    void od_ec_encode_bool_q15_128_num(od_ec_enc *enc, int val, int num); //write constant value num times
    void od_ec_encode_bool_q15_128_0_num(od_ec_enc *enc, int num);
    void od_ec_encode_bool_q15_128_x_num(od_ec_enc *enc, int32_t x, int num);
    void od_ec_encode_cdf_q15(od_ec_enc *enc, int s, uint16_t *cdf, int nsyms);

    static const uint8_t vpx_norm[256] = {
      0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    template <EnumCodecType CodecType>
    inline void WriteBool(BoolCoder &bc, int32_t bit, int32_t probability) {
        const int p = (0x7FFFFF - (probability << 15) + probability) >> 8;
        od_ec_encode_bool_q15(&bc.ec, bit, p);
    }

    void od_ec_enc_normalize(od_ec_enc *enc, od_ec_window low, unsigned rng);

    template <EnumCodecType CodecType>
    inline void WriteBool128(BoolCoder &bc, int32_t bit) {
        WriteBool<CodecType>(bc, bit, 128);
    }

    template <EnumCodecType CodecType>
    inline void InitBool(BoolCoder &bc, uint8_t *buffer, uint8_t *entBuf, int32_t entBufSize) {
        bc.buffer = buffer;
        bc.pos = 0;
        od_ec_enc_init(&bc.ec, entBuf, (uint16_t*)(entBuf + entBufSize), entBufSize);
    }

    template <EnumCodecType CodecType>
    inline void ExitBool(BoolCoder &bc) {
        uint32_t daala_bytes;
        uint8_t *daala_data;
        daala_data = od_ec_enc_done(&bc.ec, &daala_bytes);
        memcpy(bc.buffer, daala_data, daala_bytes);
        bc.pos = daala_bytes;
        /* Prevent ec bitstream from being detected as a superframe marker.
        Must always be added, so that rawbits knows the exact length of the
        bitstream. */
        bc.buffer[bc.pos++] = 0;
        //od_ec_enc_clear(&bc.ec);
    }

    template <EnumCodecType CodecType>
    inline void WriteLiteral(BoolCoder &bc, int32_t data, int32_t bits) {
        for (int bit = bits - 1; bit >= 0; bit--) WriteBool128<CodecType>(bc, (data >> bit) & 1);

    }

    static inline int get_msb(unsigned int n) {
        unsigned long first_set_bit;
        assert(n != 0);
        return BSR(n);
    }

    inline void WriteSymbol(BoolCoder &bc, int symb, aom_cdf_prob *cdf, int nsymbs) {
        od_ec_encode_cdf_q15(&bc.ec, symb, cdf, nsymbs);
    }

    inline void WriteSymbolNoUpdate(BoolCoder &bc, int symb, const aom_cdf_prob *cdf, int nsymbs) {
        assert(symb >= 0);
        assert(symb < nsymbs);
        assert(cdf[nsymbs - 1] == AOM_ICDF(32768U));
        trace_cdf(symb, cdf, nsymbs, bc.ec.rng);
        od_ec_encode_q15(&bc.ec, symb > 0 ? cdf[symb - 1] : AOM_ICDF(0), cdf[symb], symb, nsymbs);
    }

    inline void WriteSymbol4(BoolCoder &bc, int sym, aom_cdf_prob *cdf) {
        const int nsyms = 4;
        assert(sym >= 0);
        assert(sym < nsyms);
        assert(cdf[nsyms - 1] == AOM_ICDF(32768U));
        trace_cdf(sym, cdf, nsyms, bc.ec.rng);
        if(sym)
            od_ec_encode_q15(&bc.ec, cdf[sym - 1], cdf[sym], sym, nsyms);
        else
            od_ec_encode_q15_0(&bc.ec, cdf[0]);

        __m128i rate = _mm_cvtsi32_si128((cdf[nsyms] >> 4) + 5);
        __m128i seq0to3 = _mm_cvtsi64_si128(0x0003000200010000);     // 8w: 0 1 2 3 x x x x
        __m128i tmp = _mm_cmplt_epi16(seq0to3, _mm_set1_epi16(sym)); // tmp = x < sym ? 0xffff : 0x0000
        tmp = _mm_slli_epi16(tmp, 15);                               // tmp = x < sym ? 0x8000 : 0x0000
        __m128i cdfs = loadl_epi64(cdf);                             // 8w: cdf[0] cdf[1] cdf[2] cdf[3] x x x x
        __m128i diff = _mm_sub_epi16(cdfs, tmp);                     // cdf - tmp
        __m128i absDiff = _mm_abs_epi16(diff);                       // |cdf - tmp|
        __m128i update = _mm_srl_epi16(absDiff, rate);               // |cdf - tmp| >> rate
        update = _mm_sign_epi16(update, diff);                       // x < sym ? -(|cdf - tmp| >> rate) : |cdf - tmp| >> rate
        cdfs = _mm_sub_epi16(cdfs, update);                          // x < sym ? cdf += |cdf - tmp| >> rate : cdf -= |cdf - tmp| >> rate
        storel_epi64(cdf, cdfs);
        cdf[nsyms] += 1 - (cdf[nsyms] >> 5);

        trace_cdf_update(cdf, nsyms);
    }

    inline void WriteSymbol4_3(BoolCoder &bc, aom_cdf_prob *cdf) {
        const int nsyms = 4;
        assert(cdf[nsyms - 1] == AOM_ICDF(32768U));
        //trace_cdf(sym, cdf, nsyms, bc.ec.rng);
        od_ec_encode_q15(&bc.ec, cdf[2], cdf[3], 3, 4);

        __m128i rate = _mm_cvtsi32_si128((cdf[nsyms] >> 4) + 5);
        __m128i seq0to3 = _mm_cvtsi64_si128(0x0003000200010000);     // 8w: 0 1 2 3 x x x x
        __m128i tmp = _mm_cmplt_epi16(seq0to3, _mm_set1_epi16(3)); // tmp = x < sym ? 0xffff : 0x0000
        tmp = _mm_slli_epi16(tmp, 15);                               // tmp = x < sym ? 0x8000 : 0x0000
        __m128i cdfs = loadl_epi64(cdf);                             // 8w: cdf[0] cdf[1] cdf[2] cdf[3] x x x x
        __m128i diff = _mm_sub_epi16(cdfs, tmp);                     // cdf - tmp
        __m128i absDiff = _mm_abs_epi16(diff);                       // |cdf - tmp|
        __m128i update = _mm_srl_epi16(absDiff, rate);               // |cdf - tmp| >> rate
        update = _mm_sign_epi16(update, diff);                       // x < sym ? -(|cdf - tmp| >> rate) : |cdf - tmp| >> rate
        cdfs = _mm_sub_epi16(cdfs, update);                          // x < sym ? cdf += |cdf - tmp| >> rate : cdf -= |cdf - tmp| >> rate
        storel_epi64(cdf, cdfs);
        cdf[nsyms] += 1 - (cdf[nsyms] >> 5);

        trace_cdf_update(cdf, nsyms);
    }


    template <int32_t N> struct constexpr_bsr { enum { value = constexpr_bsr<(N >> 1)>::value + 1 }; };
    template <> struct constexpr_bsr<0> { enum { value = 0 }; };
    template <> struct constexpr_bsr<1> { enum { value = 0 }; };

    template <int32_t nsymbs> inline AV1_FORCEINLINE __m128i load_and_update_cdf(int32_t symb, const aom_cdf_prob *cdf) {
        assert(nsymbs > 2);
        assert(nsymbs < 10);
        assert(symb >= 0);
        assert(symb < nsymbs);
        assert(cdf[-1] == 0);
        assert(cdf[nsymbs - 1] == 32768U);
        assert(cdf[nsymbs] <= 32);

        const int32_t rate = 4 + constexpr_bsr<nsymbs>::value + (cdf[nsymbs] >> 5);
        const int32_t diff = ((CDF_PROB_TOP - (nsymbs << 5)) >> rate) << rate;
        __m128i cdfs = loadu_si128(cdf);
        __m128i tmp = _mm_cvtsi64_si128(0x0807060504030201);
        __m128i seq1to8 = _mm_unpacklo_epi8(tmp, _mm_setzero_si128()); // 8w: 1 2 3 4 5 6 7 8
        __m128i update = _mm_slli_epi16(seq1to8, 5);                   // 8w: 32 64 96 128 160 192 224 256
        __m128i symbol = _mm_set1_epi16(symb);
        __m128i diff_val = _mm_set1_epi16(diff);
        __m128i mask = _mm_cmpgt_epi16(seq1to8, symbol);    // tmp += (i == symb ? diff : 0);
        diff_val = _mm_and_si128(diff_val, mask);           // tmp += (i == symb ? diff : 0);
        update = _mm_add_epi16(update, diff_val);           // tmp += (i == symb ? diff : 0);
        update = _mm_sub_epi16(cdfs, update);               // cdf[i] -= ((cdf[i] - tmp) >> rate)
        update = _mm_srai_epi16(update, rate);              // cdf[i] -= ((cdf[i] - tmp) >> rate)
        cdfs = _mm_sub_epi16(cdfs, update);                 // cdf[i] -= ((cdf[i] - tmp) >> rate)
        return cdfs;
    }

    inline void WriteSymbol5(BoolCoder &bc, int symb, aom_cdf_prob *cdf) {
        const int nsymbs = 5;
        trace_cdf(symb, cdf, nsymbs, bc.ec.rng);

        od_ec_encode_q15(&bc.ec, cdf[symb - 1], cdf[symb], symb, nsymbs);

        // update
        storel_epi64(cdf, load_and_update_cdf<nsymbs>(symb, cdf));
        cdf[nsymbs] += 1 - (cdf[nsymbs] >> 5);

        trace_cdf_update(cdf, nsymbs);
    }

    inline void WriteSymbol6(BoolCoder &bc, int symb, aom_cdf_prob *cdf) {
        const int nsymbs = 6;
        trace_cdf(symb, cdf, nsymbs, bc.ec.rng);

        od_ec_encode_q15(&bc.ec, cdf[symb - 1], cdf[symb], symb, nsymbs);

        // update
        const int32_t newCounter = cdf[nsymbs] + 1 - (cdf[nsymbs] >> 5);
        const __m128i new_cdf = load_and_update_cdf<nsymbs>(symb, cdf);
        storeu_si128(cdf, _mm_insert_epi32(new_cdf, newCounter, 3));

        trace_cdf_update(cdf, nsymbs);
    }

    inline void WriteSymbol9(BoolCoder &bc, int symb, aom_cdf_prob *cdf) {
        const int nsymbs = 9;
        trace_cdf(symb, cdf, nsymbs, bc.ec.rng);

        od_ec_encode_q15(&bc.ec, cdf[symb - 1], cdf[symb], symb, nsymbs);

        // update
        storeu_si128(cdf, load_and_update_cdf<nsymbs>(symb, cdf));
        cdf[nsymbs] += 1 - (cdf[nsymbs] >> 5);

        trace_cdf_update(cdf, nsymbs);
    }

};
