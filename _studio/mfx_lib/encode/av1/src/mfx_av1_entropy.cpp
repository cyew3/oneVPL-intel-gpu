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

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_defs.h"
#include "mfx_av1_bitwriter.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_binarization.h"
#include "mfx_av1_scan.h"

using namespace AV1Enc;

namespace AV1Enc {


#define OD_CLZ0 (1)
#define OD_CLZ(x) (-get_msb(x))
#define OD_ILOG_NZ(x) (OD_CLZ0 - OD_CLZ(x))
#define OD_MINI MIN
#define OD_MOVE(dst, src, n) \
 (memmove((dst), (src), sizeof(*(dst))*(n) + 0*((dst) - (src)) ))
#define OD_COPY(dst, src, n) \
  (memcpy((dst), (src), sizeof(*(dst)) * (n) + 0 * ((dst) - (src))))
#define OD_EC_UINT_BITS (4)
#define OD_UNIFORM_CDF_Q15(n) (OD_UNIFORM_CDFS_Q15 + ((n) * ((n)-1) >> 1) - 1)
#define CHAR_BIT      8         // number of bits in a char
#define OD_EC_WINDOW_SIZE ((int)sizeof(od_ec_window) * CHAR_BIT)
#define OD_SUBSATU(a, b) ((a)-OD_MINI(a, b))
    /*The resolution of fractional-precision bit usage measurements, i.e.,
    3 => 1/8th bits.*/
#define OD_BITRES (3)

#define OD_EC_REDUCED_OVERHEAD (1)

    /*CDFs for uniform probability distributions of small sizes (2 through 16,
    inclusive).*/
    // clang-format off
    const uint16_t OD_UNIFORM_CDFS_Q15[135] = {
        16384, 32768,
        10923, 21845, 32768,
        8192,  16384, 24576, 32768,
        6554,  13107, 19661, 26214, 32768,
        5461,  10923, 16384, 21845, 27307, 32768,
        4681,   9362, 14043, 18725, 23406, 28087, 32768,
        4096,   8192, 12288, 16384, 20480, 24576, 28672, 32768,
        3641,   7282, 10923, 14564, 18204, 21845, 25486, 29127, 32768,
        3277,   6554,  9830, 13107, 16384, 19661, 22938, 26214, 29491, 32768,
        2979,   5958,  8937, 11916, 14895, 17873, 20852, 23831, 26810, 29789, 32768,
        2731,   5461,  8192, 10923, 13653, 16384, 19115, 21845, 24576, 27307, 30037,
        32768,
        2521,   5041,  7562, 10082, 12603, 15124, 17644, 20165, 22686, 25206, 27727,
        30247, 32768,
        2341,   4681,  7022,  9362, 11703, 14043, 16384, 18725, 21065, 23406, 25746,
        28087, 30427, 32768,
        2185,   4369,  6554,  8738, 10923, 13107, 15292, 17476, 19661, 21845, 24030,
        26214, 28399, 30583, 32768,
        2048,   4096,  6144,  8192, 10240, 12288, 14336, 16384, 18432, 20480, 22528,
        24576, 26624, 28672, 30720, 32768
    };

    /*Reinitializes the encoder.*/
    void od_ec_enc_reset(od_ec_enc *enc) {
        enc->end_offs = 0;
        enc->end_window = 0;
        enc->nend_bits = 0;
        enc->offs = 0;
        enc->low = 0;
        enc->rng = 0x8000;
        /*This is initialized to -9 so that it crosses zero after we've accumulated
        one byte + one carry bit.*/
        enc->cnt = -9;
        enc->error = 0;
    }

    void od_ec_enc_init(od_ec_enc *enc, uint8_t *buf, uint16_t *precarry_buf, int32_t size) {
        od_ec_enc_reset(enc);
        enc->buf = buf;// (unsigned char *)malloc(sizeof(*enc->buf) * size);
        enc->storage = size;
        if (size > 0 && enc->buf == NULL) {
            enc->storage = 0;
            enc->error = -1;
        }
        enc->precarry_buf = precarry_buf;//  (uint16_t *)malloc(sizeof(*enc->precarry_buf) * size);
        enc->precarry_storage = size;
        if (size > 0 && enc->precarry_buf == NULL) {
            enc->precarry_storage = 0;
            enc->error = -1;
        }
    }

    unsigned char *od_ec_enc_done(od_ec_enc *enc, uint32_t *nbytes) {
        unsigned char *out;
        uint32_t storage;
        uint16_t *buf;
        uint32_t offs;
        uint32_t end_offs;
        int nend_bits;
        od_ec_window m;
        od_ec_window e;
        od_ec_window l;
        unsigned r;
        int c;
        int s;
        if (enc->error) return NULL;

        /*We output the minimum number of bits that ensures that the symbols encoded
        thus far will be decoded correctly regardless of the bits that follow.*/
        l = enc->low;
        r = enc->rng;
        c = enc->cnt;
        s = 10;
        m = 0x3FFF;
        e = ((l + m) & ~m) | (m + 1);
        s += c;
        offs = enc->offs;
        buf = enc->precarry_buf;

        /*while ((e | m) >= l + r) {
            s++;
            m >>= 1;
            e = (l + m) & ~m;
        }
        s += c;
        offs = enc->offs;
        buf = enc->precarry_buf;*/
        if (s > 0) {
            unsigned n;
            storage = enc->precarry_storage;
            if (offs + ((s + 7) >> 3) > storage) {
                assert(0);
                //throw;
/*                storage = storage * 2 + ((s + 7) >> 3);
                buf = (uint16_t *)realloc(buf, sizeof(*buf) * storage);
                if (buf == NULL) {
                    enc->error = -1;
                    return NULL;
                }
                enc->precarry_buf = buf;
                enc->precarry_storage = storage;*/
            }
            n = (1 << (c + 16)) - 1;
            do {
                assert(offs < storage);
                buf[offs++] = (uint16_t)(e >> (c + 16));
                e &= n;
                s -= 8;
                c -= 8;
                n >>= 8;
            } while (s > 0);
        }
        /*Make sure there's enough room for the entropy-coded bits and the raw
        bits.*/
        out = enc->buf;
        storage = enc->storage;
        c = MAX((s + 7) >> 3, 0);
        if (offs + c > storage) {
            assert(0);
            //throw;
/*            storage = offs + end_offs + c;
            out = (unsigned char *)realloc(out, sizeof(*out) * storage);
            if (out == NULL) {
                enc->error = -1;
                return NULL;
            }
            OD_MOVE(out + storage - end_offs, out + enc->storage - end_offs, end_offs);
            enc->buf = out;
            enc->storage = storage;*/
        }
        /*If we have buffered raw bits, flush them as well.*/
        /*while (nend_bits > s) {
            assert(end_offs < storage);
            out[storage - ++end_offs] = (unsigned char)e;
            e >>= 8;
            nend_bits -= 8;
        }*/
        *nbytes = offs;// + end_offs;
        /*Perform carry propagation.*/
        //assert(offs + end_offs <= storage);
        out = out + storage - (offs);
        c = 0;
        end_offs = offs;
        while (offs > 0) {
            offs--;
            c = buf[offs] + c;
            out[offs] = (unsigned char)c;
            c >>= 8;
        }
        /*Add any remaining raw bits to the last byte.
        There is guaranteed to be enough room, because nend_bits <= s.*/
        //assert(nend_bits <= 0 || end_offs > 0);
        //if (nend_bits > 0) out[end_offs - 1] |= (unsigned char)e;
        /*Note: Unless there's an allocation error, if you keep encoding into the
        current buffer and call this function again later, everything will work
        just fine (you won't get a new packet out, but you will get a single
        buffer with the new data appended to the old).
        However, this function is O(N) where N is the amount of data coded so far,
        so calling it more than once for a given packet is a bad idea.*/
        return out;
    }
    /*Frees the buffers used by the encoder.*/
    //void od_ec_enc_clear(od_ec_enc *enc) {
    //    free(enc->precarry_buf);
    //    free(enc->buf);
    //}
    //            od_ec_encode_q15(&bc.ec, AOM_ICDF(0), cdf[0], 0, 4);

    void od_ec_encode_q15_0(od_ec_enc *enc,  unsigned fh) {
        od_ec_window l = enc->low;
        unsigned r = enc->rng;
        assert(32768U <= r);
        assert(7 - EC_PROB_SHIFT - CDF_SHIFT >= 0);
        const unsigned ad = (3) << 2;
        const unsigned rs = r >> 8;
        const unsigned  v = (rs * (unsigned)(fh >> EC_PROB_SHIFT) >>
            (7 - EC_PROB_SHIFT - CDF_SHIFT)) + ad;
        r -= v;

        //normalize
        int c = enc->cnt;
        assert(r <= 65535U);
        int d = 15 - BSR(r);
        int s = c + d;
        /*TODO: Right now we flush every time we have at least one byte available.
        Instead we should use an od_ec_window and flush right before we're about to
        shift bits off the end of the window.
        For a 32-bit window this is about the same amount of work, but for a 64-bit
        window it should be a fair win.*/
        if (s >= 0) {
            uint32_t offs = enc->offs;
            const uint16_t *buf = enc->precarry_buf + offs;
            assert(enc->offs + 2 <= enc->precarry_storage);
            c += 16;
            unsigned m = (1 << c) - 1;
            uint32_t value = (uint32_t)(l >> c);
            l &= m;
            if (s >= 8) {
                c -= 8;
                m >>= 8;
                value |= ((uint32_t)(l >> c) << 16);
                l &= m;
                offs++;
            }
            *(uint32_t*)buf = value;
            s = c + d - 24;
            enc->offs = offs + 1;
        }
        enc->low = l << d;
        enc->rng = r << d;
        enc->cnt = s;

    }

    void od_ec_encode_q15(od_ec_enc *enc, unsigned fl, unsigned fh, int s, int nsyms) {
        od_ec_window l = enc->low;
        unsigned r = enc->rng;
        assert(32768U <= r);
        assert(fh <= fl);
        assert(fl <= 32768U);
        assert(7 - EC_PROB_SHIFT - CDF_SHIFT >= 0);
        const unsigned ad = (nsyms - 1 - (s + 0)) << 2;
        const unsigned rs = r >> 8;
        const unsigned  v = (rs * (unsigned)(fh >> EC_PROB_SHIFT) >>
            (7 - EC_PROB_SHIFT - CDF_SHIFT)) + ad;

        if (fl < CDF_PROB_TOP) {
            unsigned u = (rs * (unsigned)(fl >> EC_PROB_SHIFT) >>
                (7 - EC_PROB_SHIFT - CDF_SHIFT)) + ad + EC_MIN_PROB;
            l += r - u;
            r = u - v;
        }
        else {
            r -= v;
        }

        //normalize
        int c = enc->cnt;
        assert(r <= 65535U);
        int d = 15 - BSR(r);
        s = c + d;
        /*TODO: Right now we flush every time we have at least one byte available.
        Instead we should use an od_ec_window and flush right before we're about to
        shift bits off the end of the window.
        For a 32-bit window this is about the same amount of work, but for a 64-bit
        window it should be a fair win.*/
        if (s >= 0) {
            uint32_t offs = enc->offs;
            const uint16_t *buf = enc->precarry_buf + offs;
            assert(enc->offs + 2 <= enc->precarry_storage);
            c += 16;
            unsigned m = (1 << c) - 1;
            uint32_t value = (uint32_t)(l >> c);
            l &= m;
            if (s >= 8) {
                c -= 8;
                m >>= 8;
                value |= ((uint32_t)(l >> c) << 16);
                l &= m;
                offs++;
            }
            *(uint32_t*)buf = value;
            s = c + d - 24;
            enc->offs = offs + 1;
        }
        enc->low = l << d;
        enc->rng = r << d;
        enc->cnt = s;

    }


    void od_ec_encode_bool_q15_128_x_num(od_ec_enc *enc, int32_t x, int num) {
        //trace_bool(val, f, enc->rng);
        od_ec_window l = enc->low;
        unsigned r = enc->rng;
        uint32_t offs = enc->offs;
        int c = enc->cnt;
        const uint16_t *buf = enc->precarry_buf;
        assert(32768U <= r);

        for (int i = num; i >= 0; i--) {
            unsigned v = (r >> 8) << 7;
            v += EC_MIN_PROB;
            const int32_t val = (x >> i) & 1;
            if (val) l += r - v;
            r = val ? v : r - v;

            //normalize
            assert(r <= 65535U);
            int d = 15 - BSR(r);
            int s = c + d;
            /*TODO: Right now we flush every time we have at least one byte available.
            Instead we should use an od_ec_window and flush right before we're about to
            shift bits off the end of the window.
            For a 32-bit window this is about the same amount of work, but for a 64-bit
            window it should be a fair win.*/
            if (s >= 0) {
                assert(enc->offs + 2 <= enc->precarry_storage);
                c += 16;
                unsigned m = (1 << c) - 1;
                uint32_t value = (uint32_t)(l >> c);
                l &= m;
                const uint16_t *bf = buf + offs;
                if (s >= 8) {
                    c -= 8;
                    m >>= 8;
                    value |= ((uint32_t)(l >> c) << 16);
                    l &= m;
                    offs++;
                }
                *(uint32_t*)(bf) = value;
                s = c + d - 24;
                offs++;
            }
            l <<= d;
            r <<= d;
            c = s;
        }
        enc->low = l;
        enc->rng = r;
        enc->cnt = c;
        enc->offs = offs;
    }


    void od_ec_encode_bool_q15_128_0_num(od_ec_enc *enc, int num) {
        //trace_bool(val, f, enc->rng);
        od_ec_window l = enc->low;
        unsigned r = enc->rng;
        uint32_t offs = enc->offs;
        int c = enc->cnt;
        const uint16_t *buf = enc->precarry_buf;
        assert(32768U <= r);

        for (int i = 0; i < num; i++) {
            unsigned v = (r >> 8) << 7;
            v += EC_MIN_PROB;
            r -= v;

            //normalize
            assert(r <= 65535U);
            int d = 15 - BSR(r);
            int s = c + d;
            /*TODO: Right now we flush every time we have at least one byte available.
            Instead we should use an od_ec_window and flush right before we're about to
            shift bits off the end of the window.
            For a 32-bit window this is about the same amount of work, but for a 64-bit
            window it should be a fair win.*/
            if (s >= 0) {
                assert(enc->offs + 2 <= enc->precarry_storage);
                c += 16;
                unsigned m = (1 << c) - 1;
                uint32_t value = (uint32_t)(l >> c);
                l &= m;
                const uint16_t *bf = buf + offs;
                if (s >= 8) {
                    c -= 8;
                    m >>= 8;
                    value |= ((uint32_t)(l >> c) << 16);
                    l &= m;
                    offs++;
                }
                *(uint32_t*)(bf) = value;
                s = c + d - 24;
                offs++;
            }
            l <<= d;
            r <<= d;
            c = s;
        }
        enc->low = l;
        enc->rng = r;
        enc->cnt = c;
        enc->offs = offs;
    }


    void od_ec_encode_bool_q15_128_num(od_ec_enc *enc, int val, int num) {
        //trace_bool(val, f, enc->rng);
        od_ec_window l = enc->low;
        unsigned r = enc->rng;
        uint32_t offs = enc->offs;
        int c = enc->cnt;
        const uint16_t *buf = enc->precarry_buf;
        assert(32768U <= r);

        for (int i = 0; i < num; i++) {
            unsigned v = (r >> 8) << 7;
            v += EC_MIN_PROB;
            if (val) l += r - v;
            r = val ? v : r - v;

            //normalize
            assert(r <= 65535U);
            int d = 15 - BSR(r);
            int s = c + d;
            /*TODO: Right now we flush every time we have at least one byte available.
            Instead we should use an od_ec_window and flush right before we're about to
            shift bits off the end of the window.
            For a 32-bit window this is about the same amount of work, but for a 64-bit
            window it should be a fair win.*/
            if (s >= 0) {
                assert(enc->offs + 2 <= enc->precarry_storage);
                c += 16;
                unsigned m = (1 << c) - 1;
                uint32_t value = (uint32_t)(l >> c);
                l &= m;
                const uint16_t *bf = buf + offs;
                if (s >= 8) {
                    c -= 8;
                    m >>= 8;
                    value |= ((uint32_t)(l >> c) << 16);
                    l &= m;
                    offs++;
                }
                *(uint32_t*)(bf) = value;
                s = c + d - 24;
                offs++;
            }
            l <<= d;
            r <<= d;
            c = s;
        }
        enc->low = l;
        enc->rng = r;
        enc->cnt = c;
        enc->offs = offs;
    }


    void od_ec_encode_bool_q15_128(od_ec_enc *enc, int val) {
        //trace_bool(val, f, enc->rng);
        od_ec_window l = enc->low;
        unsigned r = enc->rng;
        assert(32768U <= r);
        unsigned v = (r >> 8)<<7;
        v += EC_MIN_PROB;
        if (val) l += r - v;
        r = val ? v : r - v;

        //normalize
        int c = enc->cnt;
        assert(r <= 65535U);
        int d = 15 - BSR(r);
        int s = c + d;
        /*TODO: Right now we flush every time we have at least one byte available.
        Instead we should use an od_ec_window and flush right before we're about to
        shift bits off the end of the window.
        For a 32-bit window this is about the same amount of work, but for a 64-bit
        window it should be a fair win.*/
        if (s >= 0) {
            uint32_t offs = enc->offs;
            const uint16_t *buf = enc->precarry_buf + offs;
            assert(enc->offs + 2 <= enc->precarry_storage);
            c += 16;
            unsigned m = (1 << c) - 1;
            uint32_t value = (uint32_t)(l >> c);
            l &= m;
            if (s >= 8) {
                c -= 8;
                m >>= 8;
                value |= ((uint32_t)(l >> c)<<16);
                l &= m;
                offs++;
            }
            *(uint32_t*)buf = value;
            s = c + d - 24;
            enc->offs = offs + 1;
        }
        enc->low = l << d;
        enc->rng = r << d;
        enc->cnt = s;

    }


    void od_ec_encode_bool_q15(od_ec_enc *enc, int val, unsigned f) {
        trace_bool(val, f, enc->rng);
        od_ec_window l;
        unsigned r;
        unsigned v;
        assert(0 < f);
        assert(f < 32768U);
        l = enc->low;
        r = enc->rng;
        assert(32768U <= r);
        v = ((r >> 8) * (unsigned)(f >> EC_PROB_SHIFT) >> (7 - EC_PROB_SHIFT));
        v += EC_MIN_PROB;
        if (val) l += r - v;
        r = val ? v : r - v;

        //normalize
        int c = enc->cnt;
        assert(r <= 65535U);
        int d = 15 - BSR(r);
        int s = c + d;
        /*TODO: Right now we flush every time we have at least one byte available.
        Instead we should use an od_ec_window and flush right before we're about to
        shift bits off the end of the window.
        For a 32-bit window this is about the same amount of work, but for a 64-bit
        window it should be a fair win.*/
        if (s >= 0) {
            uint32_t offs = enc->offs;
            const uint16_t *buf = enc->precarry_buf + offs;
            assert(enc->offs + 2 <= enc->precarry_storage);
            c += 16;
            unsigned m = (1 << c) - 1;
            uint32_t value = (uint32_t)(l >> c);
            l &= m;
            if (s >= 8) {
                c -= 8;
                m >>= 8;
                value |= ((uint32_t)(l >> c) << 16);
                l &= m;
                offs++;
            }
            *(uint32_t*)buf = value;
            s = c + d - 24;
            enc->offs = offs + 1;
        }
        enc->low = l << d;
        enc->rng = r << d;
        enc->cnt = s;

    }

    static const int nsymbs2speed[17] = { 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 };

    void update_cdf(aom_cdf_prob *cdf, int val, int nsymbs) {
        const aom_cdf_prob last_cdf = cdf[nsymbs];
        const int rate = ( last_cdf >> 4) + nsymbs2speed[nsymbs];

        #pragma novector
        #pragma nounroll

        for (int i = 0; i < val; i++, cdf++)
            *cdf += (CDF_PROB_TOP - *cdf) >> rate;

        #pragma novector
        #pragma nounroll
        for (int i = val; i < nsymbs - 1; ++i, cdf++)
            *cdf -= *cdf >> rate;
        *(cdf+1) += 1 - (last_cdf >> 5);


        trace_cdf_update(cdf, nsymbs);
    }

    /*Encodes a symbol given a cumulative distribution function (CDF) table in Q15.
    This is a simpler, lower overhead version of od_ec_encode_cdf() for use when
    cdf[nsyms - 1] == 32768.
    Symbols encoded with this function cannot be properly decoded with
    od_ec_decode(), and must be decoded with one of the equivalent _q15()
    functions instead.
    s: The index of the symbol to encode.
    cdf: The CDF, such that symbol s falls in the range
    [s > 0 ? cdf[s - 1] : 0, cdf[s]).
    The values must be monotonically non-decreasing, and the last value
    must be exactly 32768.
    nsyms: The number of symbols in the alphabet.
    This should be at most 16.*/
    void od_ec_encode_cdf_q15(od_ec_enc *enc, int s, uint16_t *cdf, int nsyms) {
        assert(s >= 0);
        assert(s < nsyms);
        assert(cdf[nsyms - 1] == AOM_ICDF(32768U));
        trace_cdf(s, cdf, nsyms, enc->rng);
        od_ec_encode_q15(enc, s > 0 ? cdf[s - 1] : AOM_ICDF(0), cdf[s], s, nsyms);
        update_cdf(cdf, s, nsyms);
    }

    /*Encodes a sequence of raw bits in the stream.
    fl: The bits to encode.
    ftb: The number of bits to encode.
    This must be between 0 and 25, inclusive.*/
    void od_ec_enc_bits(od_ec_enc *enc, uint32_t fl, unsigned ftb) {
        od_ec_window end_window;
        int nend_bits;
        assert(ftb <= 25);
        assert(fl < (uint32_t)1 << ftb);
        end_window = enc->end_window;
        nend_bits = enc->nend_bits;
        if (nend_bits + ftb > OD_EC_WINDOW_SIZE) {
            unsigned char *buf;
            uint32_t storage;
            uint32_t end_offs;
            buf = enc->buf;
            storage = enc->storage;
            end_offs = enc->end_offs;
            if (end_offs + (OD_EC_WINDOW_SIZE >> 3) >= storage) {
                assert(0);
                //throw;
/*                unsigned char *new_buf;
                uint32_t new_storage;
                new_storage = 2 * storage + (OD_EC_WINDOW_SIZE >> 3);
                new_buf = (unsigned char *)malloc(sizeof(*new_buf) * new_storage);
                if (new_buf == NULL) {
                    enc->error = -1;
                    enc->end_offs = 0;
                    return;
                }
                OD_COPY(new_buf + new_storage - end_offs, buf + storage - end_offs,
                    end_offs);
                storage = new_storage;
                free(buf);
                enc->buf = buf = new_buf;
                enc->storage = storage;*/
            }
            do {
                assert(end_offs < storage);
                buf[storage - ++end_offs] = (unsigned char)end_window;
                end_window >>= 8;
                nend_bits -= 8;
            } while (nend_bits >= 8);
            enc->end_offs = end_offs;
        }
        assert(nend_bits + ftb <= OD_EC_WINDOW_SIZE);
        end_window |= (od_ec_window)fl << nend_bits;
        nend_bits += ftb;
        enc->end_window = end_window;
        enc->nend_bits = nend_bits;
    }
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE

