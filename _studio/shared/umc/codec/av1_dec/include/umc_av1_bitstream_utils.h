// Copyright (c) 2017-2018 Intel Corporation
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

#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_BITSTREAM_UTILS_H_
#define __UMC_AV1_BITSTREAM_UTILS_H_

#include <limits>
#include "umc_av1_bitstream.h"
#include "umc_vp9_utils.h"

namespace UMC_AV1_DECODER
{
    inline int read_inv_signed_literal(AV1Bitstream& bs, int bits) {
        const unsigned sign = bs.GetBit();
        const unsigned literal = bs.GetBits(bits);
        if (sign == 0)
            return literal; // if positive: literal
        else
            return literal - (1 << bits); // if negative: complement to literal with respect to 2^bits
    }

    inline int32_t read_uniform(AV1Bitstream& bs, uint32_t n)
    {
        const uint32_t l = UMC_VP9_DECODER::GetUnsignedBits(n);
        const uint32_t m = (1 << l) - n;
        const uint32_t v = bs.GetBits(l - 1);
        if (v < m)
            return v;
        else
            return (v << 1) - m + bs.GetBits(1);
    }

    inline uint32_t read_uvlc(AV1Bitstream& bs)
    {
        uint32_t leading_zeros = 0;
        while (!bs.GetBit()) ++leading_zeros;

        // Maximum 32 bits.
        if (leading_zeros >= 32)
            return std::numeric_limits<uint32_t>::max();
        const uint32_t base = (1u << leading_zeros) - 1;
        const uint32_t value = bs.GetBits(leading_zeros);
        return base + value;
    }

    inline uint16_t inv_recenter_non_neg(uint16_t r, uint16_t v) {
        if (v >(r << 1))
            return v;
        else if ((v & 1) == 0)
            return (v >> 1) + r;
        else
            return r - ((v + 1) >> 1);
    }

    inline uint16_t inv_recenter_finite_non_neg(uint16_t n, uint16_t r, uint16_t v) {
        if ((r << 1) <= n) {
            return inv_recenter_non_neg(r, v);
        }
        else {
            return n - 1 - inv_recenter_non_neg(n - 1 - r, v);
        }
    }

    inline uint8_t GetMSB(uint32_t n)
    {
        if (n == 0)
            throw av1_exception(UMC::UMC_ERR_FAILED);

        uint8_t pos = 0;
        while (n > 1)
        {
            pos++;
            n >>= 1;
        }

        return pos;
    }

    inline uint16_t read_primitive_quniform(AV1Bitstream& bs, uint16_t n)
    {
        if (n <= 1) return 0;
        uint8_t l = GetMSB(n - 1) + 1;
        const int m = (1 << l) - n;
        const int v = bs.GetBits(l - 1);
        const int result = v < m ? v : (v << 1) - m + bs.GetBit();
        return static_cast<uint16_t>(result);
    }

    inline uint16_t read_primitive_subexpfin(AV1Bitstream& bs, uint16_t n, uint16_t k)
    {
        int i = 0;
        int mk = 0;
        uint16_t v;
        while (1) {
            int b = (i ? k + i - 1 : k);
            int a = (1 << b);
            if (n <= mk + 3 * a) {
                v = static_cast<uint16_t>(read_primitive_quniform(bs, static_cast<uint16_t>(n - mk)) + mk);
                break;
            }
            else {
                if (bs.GetBit()) {
                    i = i + 1;
                    mk += a;
                }
                else {
                    v = static_cast<uint16_t>(bs.GetBits(b) + mk);
                    break;
                }
            }
        }
        return v;
    }

    inline uint16_t read_primitive_refsubexpfin(AV1Bitstream& bs, uint16_t n, uint16_t k, uint16_t ref)
    {
        return inv_recenter_finite_non_neg(n, ref,
            read_primitive_subexpfin(bs, n, k));
    }

    inline int16_t read_signed_primitive_refsubexpfin(AV1Bitstream& bs, uint16_t n, uint16_t k, int16_t ref) {
        ref += n - 1;
        const uint16_t scaled_n = (n << 1) - 1;
        return read_primitive_refsubexpfin(bs, scaled_n, k, ref) - n + 1;
    }

    inline size_t read_leb128(AV1Bitstream& bs)
    {
        size_t value = 0;
        for (size_t i = 0; i < MAX_LEB128_SIZE; ++i)
        {
            const uint8_t cur_byte = static_cast<uint8_t>(bs.GetBits(8));
            const uint8_t decoded_byte = cur_byte & LEB128_BYTE_MASK;
            value |= ((uint64_t)decoded_byte) << (i * 7);
            if ((cur_byte & ~LEB128_BYTE_MASK) == 0)
                return value;
        }

        throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
    }
}

#endif // __UMC_AV1_BITSTREAM_H_

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
