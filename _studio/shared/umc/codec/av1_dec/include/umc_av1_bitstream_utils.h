//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//

#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_BITSTREAM_UTILS_H_
#define __UMC_AV1_BITSTREAM_UTILS_H_

#include <limits>
#include "umc_av1_bitstream.h"
#include "umc_vp9_utils.h"

namespace UMC_AV1_DECODER
{
    inline int read_inv_signed_literal(AV1Bitstream* bs, int bits) {
        const unsigned sign = bs->GetBit();
        const unsigned literal = bs->GetBits(bits);
        if (sign == 0)
            return literal; // if positive: literal
        else
            return literal - (1 << bits); // if negative: complement to literal with respect to 2^bits
    }

    inline Ipp32s read_uniform(AV1Bitstream* bs, Ipp32u n)
    {
        const Ipp32u l = UMC_VP9_DECODER::GetUnsignedBits(n);
        const Ipp32u m = (1 << l) - n;
        const Ipp32u v = bs->GetBits(l - 1);
        if (v < m)
            return v;
        else
            return (v << 1) - m + bs->GetBits(1);
    }

    inline Ipp32u read_uvlc(AV1Bitstream* bs)
    {
        Ipp32u leading_zeros = 0;
        while (!bs->GetBit()) ++leading_zeros;

        // Maximum 32 bits.
        if (leading_zeros >= 32)
            return std::numeric_limits<Ipp32u>::max();
        const Ipp32u base = (1u << leading_zeros) - 1;
        const Ipp32u value = bs->GetBits(leading_zeros);
        return base + value;
    }

    inline Ipp16u inv_recenter_non_neg(Ipp16u r, Ipp16u v) {
        if (v >(r << 1))
            return v;
        else if ((v & 1) == 0)
            return (v >> 1) + r;
        else
            return r - ((v + 1) >> 1);
    }

    inline Ipp16u inv_recenter_finite_non_neg(Ipp16u n, Ipp16u r, Ipp16u v) {
        if ((r << 1) <= n) {
            return inv_recenter_non_neg(r, v);
        }
        else {
            return n - 1 - inv_recenter_non_neg(n - 1 - r, v);
        }
    }

    inline Ipp8u GetMSB(Ipp32u n)
    {
        if (n == 0)
            throw av1_exception(UMC::UMC_ERR_FAILED);

        Ipp8u pos = 0;
        while (n > 1)
        {
            pos++;
            n >>= 1;
        }

        return pos;
    }

    inline Ipp16u read_primitive_quniform(AV1Bitstream* bs, Ipp16u n)
    {
        if (n <= 1) return 0;
        Ipp8u l = GetMSB(n - 1) + 1;
        const int m = (1 << l) - n;
        const int v = bs->GetBits(l - 1);
        const int result = v < m ? v : (v << 1) - m + bs->GetBit();
        return static_cast<Ipp16u>(result);
    }

    inline Ipp16u read_primitive_subexpfin(AV1Bitstream* bs, Ipp16u n, Ipp16u k)
    {
        int i = 0;
        int mk = 0;
        Ipp16u v;
        while (1) {
            int b = (i ? k + i - 1 : k);
            int a = (1 << b);
            if (n <= mk + 3 * a) {
                v = static_cast<Ipp16u>(read_primitive_quniform(bs, static_cast<Ipp16u>(n - mk)) + mk);
                break;
            }
            else {
                if (bs->GetBit()) {
                    i = i + 1;
                    mk += a;
                }
                else {
                    v = static_cast<Ipp16u>(bs->GetBits(b) + mk);
                    break;
                }
            }
        }
        return v;
    }

    inline Ipp16u read_primitive_refsubexpfin(AV1Bitstream* bs, Ipp16u n, Ipp16u k, Ipp16u ref)
    {
        return inv_recenter_finite_non_neg(n, ref,
            read_primitive_subexpfin(bs, n, k));
    }

    inline Ipp16s read_signed_primitive_refsubexpfin(AV1Bitstream* bs, Ipp16u n, Ipp16u k, Ipp16s ref) {
        ref += n - 1;
        const Ipp16u scaled_n = (n << 1) - 1;
        return read_primitive_refsubexpfin(bs, scaled_n, k, ref) - n + 1;
    }

    inline size_t read_leb128(AV1Bitstream* bs)
    {
        size_t value = 0;
        for (size_t i = 0; i < MAX_LEB128_SIZE; ++i)
        {
            const Ipp8u cur_byte = static_cast<Ipp8u>(bs->GetBits(8));
            const Ipp8u decoded_byte = cur_byte & LEB128_BYTE_MASK;
            value |= ((Ipp64u)decoded_byte) << (i * 7);
            if ((cur_byte & ~LEB128_BYTE_MASK) == 0)
                return value;
        }

        throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
    }
}

#endif // __UMC_AV1_BITSTREAM_H_

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
