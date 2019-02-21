// Copyright (c) 2003-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_BITSTREAM_INLINES_H
#define __UMC_H264_BITSTREAM_INLINES_H

#include "vm_debug.h"
#include "umc_h264_bitstream.h"
#include "umc_h264_dec_init_tables_cabac.h"
#include "umc_h264_dec_tables.h"

#ifdef STORE_CABAC_BITS
#include <cstdio>
#endif

using namespace UMC_H264_DECODER;

namespace UMC
{

#ifdef STORE_CABAC_BITS
inline void PRINT_CABAC_VALUES(int val, int range)
{
    static FILE* cabac_bits = 0;
    static int m_c = 0;
    if (!cabac_bits)
    {
        cabac_bits = fopen("D:\\msdk_cabac.log", "w+t");
    }
    fprintf(cabac_bits, "%d: coding bin value %d, range = [%d]\n", m_c++, val, range);
    fflush(cabac_bits);
}
#else
inline void PRINT_CABAC_VALUES(int, int)
{

}
#endif

#define ippiUngetNBits(current_data, offset, nbits) \
{ \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    offset += (nbits); \
    if (offset > 31) \
    { \
        offset -= 32; \
        current_data--; \
    } \
 \
    VM_ASSERT(offset >= 0 && offset <= 31); \
}

#define ippiUngetBits32(current_data, offset) \
    VM_ASSERT(offset >= 0 && offset <= 31); \
    current_data--;

#define ippiAlignBSPointerRight(current_data, offset) \
{ \
    if ((offset & 0x07) != 0x07) \
    { \
        offset = (offset | 0x07) - 8; \
        if (offset == -1) \
        { \
            offset = 31; \
            current_data++; \
        } \
    } \
}

#define ippiNextBits(current_data, bp, nbits, data) \
{ \
    uint32_t x; \
 \
    VM_ASSERT((nbits) > 0 && (nbits) <= 32); \
    VM_ASSERT(nbits >= 0 && nbits <= 31); \
 \
    int32_t offset = bp - (nbits); \
 \
    if (offset >= 0) \
    { \
        x = current_data[0] >> (offset + 1); \
    } \
    else \
    { \
        offset += 32; \
 \
        x = current_data[1] >> (offset); \
        x >>= 1; \
        x += current_data[0] << (31 - offset); \
    } \
 \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    (data) = x & bits_data[nbits]; \
}

#define RefreshCABACBits(codOffset, pBits, iBits) \
{ \
    uint16_t *pRealPointer; \
    /* we have to handle the bit pointer very thorougly. */ \
    /* this sophisticated logic is used to avoid compilers' warnings. */ \
    /* In two words we just select required word by the pointer */ \
    pRealPointer = (uint16_t *) (((uint8_t *) 0) + \
                               ((((uint8_t *) pBits) - (uint8_t *) 0) ^ 2)); \
    codOffset |= *(pRealPointer) << (-iBits); \
    pBits += 1; \
    iBits += 16; \
}

inline
uint32_t H264Bitstream::Peek1Bit()
{
    return h264Peek1Bit(m_pbs, m_bitOffset);
} // H264Bitstream::GetBits()

inline
void H264Bitstream::Drop1Bit()
{
    h264Drop1Bit(m_pbs, m_bitOffset);
} // H264Bitstream::GetBits()

inline
bool H264Bitstream::NextBit()
{
    int32_t bp;
    uint32_t w;

    bp = m_bitOffset - 1;

    if (bp < 0)
    {
        w = m_pbs[0] & 1;
        if (w)
        {
            m_pbs++;
            m_bitOffset = 31;
            return true;
        }
    }
    else
    {
        w = m_pbs[0] >> m_bitOffset;
        w = w & 1;
        if (w)
        {
            m_bitOffset = bp;
            return true;
        }
    }
    return false;

}

inline
uint32_t H264Bitstream::DecodeSingleBin_CABAC(uint32_t ctxIdx)
{
    uint32_t codIOffset = m_lcodIOffset;
    uint32_t codIRange = m_lcodIRange;
    uint32_t codIRangeLPS;
    uint32_t pState = context_array[ctxIdx].pStateIdxAndVal;
    uint32_t binVal;

    codIRangeLPS = rangeTabLPS[pState][(codIRange >> (6 + CABAC_MAGIC_BITS)) - 4];
    codIRange -= codIRangeLPS << CABAC_MAGIC_BITS;

    // most probably state.
    // it is more likely to decode most probably value.
    if (codIOffset < codIRange)
    {
        binVal = pState & 1;
        context_array[ctxIdx].pStateIdxAndVal = transIdxMPS[pState];

#ifndef STORE_CABAC_BITS

        // there is no likely case.
        // we take new bit with 50% probability.

        {
            int32_t numBits = NumBitsToGetTableSmall[codIRange >> (CABAC_MAGIC_BITS + 7)];

            codIRange <<= numBits;
            codIOffset <<= numBits;
            m_lcodIOffset = codIOffset;
            m_lcodIRange = codIRange;

#if (CABAC_MAGIC_BITS > 0)
            {
                int32_t iMagicBits;

                iMagicBits = m_iMagicBits - numBits;
                // in most cases we don't require to refresh cabac variables.
                if (iMagicBits)
                {
                    m_iMagicBits = iMagicBits;
                    return binVal;
                }

                RefreshCABACBits(m_lcodIOffset, m_pMagicBits, iMagicBits);
                m_iMagicBits = iMagicBits;
            }

            return binVal;

#else // !(CABAC_MAGIC_BITS > 0)

            m_lcodIOffset |= GetBits(numBits);
            return binVal;

#endif // (CABAC_MAGIC_BITS > 0)

        }

#endif // STORE_CABAC_BITS

    }
    else
    {
        codIOffset -= codIRange;
        codIRange = codIRangeLPS << CABAC_MAGIC_BITS;

        binVal = (pState & 1) ^ 1;
        context_array[ctxIdx].pStateIdxAndVal = transIdxLPS[pState];
    }

    // Renormalization process
    // See subclause 9.3.3.2.2 of H.264
    //if (codIRange < (0x100<<(CABAC_MAGIC_BITS)))
    {
        int32_t numBits = NumBitsToGetTbl[codIRange >> CABAC_MAGIC_BITS];

        codIRange <<= numBits;
        codIOffset <<= numBits;

#if (CABAC_MAGIC_BITS > 0)
        {
            int32_t iMagicBits;

            iMagicBits = m_iMagicBits - numBits;

            if (0 >= iMagicBits)
            {
                RefreshCABACBits(codIOffset, m_pMagicBits, iMagicBits);
                m_iMagicBits = iMagicBits;
            }
            else
                m_iMagicBits = iMagicBits;
        }
#else // !(CABAC_MAGIC_BITS > 0)
        codIOffset |= GetBits(numBits);
#endif // (CABAC_MAGIC_BITS > 0)
    }

#ifdef STORE_CABAC_BITS
    PRINT_CABAC_VALUES(binVal, codIRange>>CABAC_MAGIC_BITS);
#endif

    m_lcodIOffset = codIOffset;
    m_lcodIRange = codIRange;

    return binVal;

} //int32_t H264Bitstream::DecodeSingleBin_CABAC(int32_t ctxIdx)

inline
uint32_t H264Bitstream::DecodeSymbolEnd_CABAC(void)
{
    uint32_t binVal = 1;
    uint32_t codIOffset = m_lcodIOffset;
    uint32_t codIRange = m_lcodIRange;

    // See subclause 9.3.3.2.4 of H.264 standard
    if (codIOffset < (codIRange - (2 << CABAC_MAGIC_BITS)))
    {
        codIRange -= (2 << CABAC_MAGIC_BITS);

        // Renormalization process
        // See subclause 9.3.3.2.2 of H.264
        if (codIRange < (0x100 << (CABAC_MAGIC_BITS)))
        {
            codIRange <<= 1;
            codIOffset <<= 1;

#if (CABAC_MAGIC_BITS > 0)
            m_iMagicBits -= 1;
            if (0 >= m_iMagicBits)
                RefreshCABACBits(codIOffset, m_pMagicBits, m_iMagicBits);
#else // !(CABAC_MAGIC_BITS > 0)
            codIOffset |= GetBits(1);
#endif // (CABAC_MAGIC_BITS > 0)
        }
        binVal = 0;
        m_lcodIOffset = codIOffset;
        m_lcodIRange = codIRange;

    }

    return binVal;

} //int32_t H264Bitstream::DecodeSymbolEnd_CABAC(void)

inline
uint32_t H264Bitstream::DecodeBypass_CABAC(void)
{
    // See subclause 9.3.3.2.3 of H.264 standard
    uint32_t binVal;
#if (CABAC_MAGIC_BITS > 0)
    m_lcodIOffset = (m_lcodIOffset << 1);

    m_iMagicBits -= 1;
    if (0 >= m_iMagicBits)
        RefreshCABACBits(m_lcodIOffset, m_pMagicBits, m_iMagicBits);
#else // !(CABAC_MAGIC_BITS > 0)
    m_lcodIOffset = (m_lcodIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

    if (m_lcodIOffset >= m_lcodIRange)
    {
        binVal = 1;
        m_lcodIOffset -= m_lcodIRange;
   }
    else
    {
        binVal = 0;
    }

    return binVal;

} //int32_t H264Bitstream::DecodeBypass_CABAC(void)

inline
int32_t H264Bitstream::DecodeBypassSign_CABAC(int32_t val)
{
    // See subclause 9.3.3.2.3 of H.264 standard
    int32_t binVal;

#if (CABAC_MAGIC_BITS > 0)
    m_lcodIOffset = (m_lcodIOffset << 1);

    m_iMagicBits -= 1;
    if (0 >= m_iMagicBits)
        RefreshCABACBits(m_lcodIOffset, m_pMagicBits, m_iMagicBits);
#else
    m_lcodIOffset = (m_lcodIOffset << 1) | Get1Bit();
#endif

    if (m_lcodIOffset >= m_lcodIRange)
    {
        binVal = -val;
        m_lcodIOffset -= m_lcodIRange;
   }
    else
    {
        binVal = val;
    }

    return binVal;

} // int32_t H264Bitstream::DecodeBypassSign_CABAC()

} // namespace UMC

#endif // __UMC_H264_BITSTREAM_INLINES_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
