// Copyright (c) 2012-2018 Intel Corporation
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
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

#ifndef __UMC_H265_BITSTREAM_INLINES_H
#define __UMC_H265_BITSTREAM_INLINES_H

#include "vm_debug.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_tables.h"

namespace UMC_HEVC_DECODER
{

const unsigned long long g_scaled256 = (unsigned long long)0x100 << (7+ CABAC_MAGIC_BITS);

#if CABAC_MAGIC_BITS == 16
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
#endif

#if CABAC_MAGIC_BITS == 24
#define RefreshCABACBits(codOffset, pBits, iBits) \
{ \
    uint16_t *pRealPointer; \
    /* we have to handle the bit pointer very thorougly. */ \
    /* this sophisticated logic is used to avoid compilers' warnings. */ \
    /* In two words we just select required word by the pointer */ \
    pRealPointer = (uint16_t *) (((uint8_t *) 0) + \
                               ((((uint8_t *) pBits) - (uint8_t *) 0) ^ 2)); \
    codOffset |= *(pRealPointer) << (24-iBits); \
    codOffset |= *((uint8_t*)pRealPointer + 2) << (8-iBits); \
    pBits = (uint16_t*)((uint8_t*)pBits + 3); \
    iBits += 24; \
}
#endif

const uint32_t c_RenormTable[32] =
{
  6,  5,  4,  4,
  3,  3,  3,  3,
  2,  2,  2,  2,
  2,  2,  2,  2,
  1,  1,  1,  1,
  1,  1,  1,  1,
  1,  1,  1,  1,
  1,  1,  1,  1
};

#if defined( __INTEL_COMPILER )

// Optimized function which uses bit manipulation instructions (BMI)
H265_FORCEINLINE
uint32_t H265BaseBitstream::GetBits_BMI(uint32_t nbits)
{
    VM_ASSERT(nbits > 0 && nbits <= 32);
    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    uint32_t bits;

    m_bitOffset -= nbits;
    uint32_t shift = m_bitOffset + 1;

    if (m_bitOffset >=0 )
        bits = _shrx_u32( m_pbs[0], shift );
    else
    {
        bits = _shrx_u32( m_pbs[1], m_bitOffset);
        m_bitOffset += 32;
        bits >>= 1;
        bits |= _shlx_u32( m_pbs[0], 0 - shift );
        ++m_pbs;
    }

    bits = _bzhi_u32( bits, nbits );

    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    return ( bits );
}

#define _cmovz_intrin( _M_flag, _M_dest, _M_src ) __asm__ ( "test %["#_M_flag"], %["#_M_flag"] \n\t cmovz %["#_M_src"], %["#_M_dest"]" : [_M_dest] "+r" (_M_dest) : [_M_flag] "r" (_M_flag), [_M_src] "rm" (_M_src) )
#define _cmovnz_intrin( _M_flag, _M_dest, _M_src ) __asm__ ( "test %["#_M_flag"], %["#_M_flag"] \n\t cmovnz %["#_M_src"], %["#_M_dest"]" : [_M_dest] "+r" (_M_dest) : [_M_flag] "r" (_M_flag), [_M_src] "rm" (_M_src) )
#else
#define _cmovz_intrin( _M_flag, _M_dest, _M_src ) _M_dest = (_M_flag == 0) ? (_M_src) : (_M_dest)
#define _cmovnz_intrin( _M_flag, _M_dest, _M_src ) _M_dest = (_M_flag) ? (_M_src) : (_M_dest)
#endif

#if INSTRUMENTED_CABAC == 0 && defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

typedef uint32_t (H265Bitstream::* t_DecodeSingleBin_CABAC)(uint32_t ctxIdx);
extern t_DecodeSingleBin_CABAC s_pDecodeSingleBin_CABAC_dispatched;

// Call optimized function by pointer either cmov_BMI or just cmov versions
H265_FORCEINLINE
uint32_t H265Bitstream::DecodeSingleBin_CABAC(uint32_t ctxIdx)
{
    return (this->*s_pDecodeSingleBin_CABAC_dispatched)( ctxIdx );
}

#else // defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

// Decode one bin from CABAC stream
inline
uint32_t H265Bitstream::DecodeSingleBin_CABAC(uint32_t ctxIdx)
{
#if INSTRUMENTED_CABAC
    uint32_t range = m_lcodIRange;
#endif

    uint32_t codIRangeLPS;

    uint32_t pState = context_hevc[ctxIdx];
    uint32_t binVal;

    codIRangeLPS = rangeTabLPSH265[pState][(m_lcodIRange >> (6 + CABAC_MAGIC_BITS)) - 4];
    m_lcodIRange -= codIRangeLPS << CABAC_MAGIC_BITS;
#if (CABAC_MAGIC_BITS > 0)
    unsigned long long
#else
    uint32_t
#endif
    scaledRange = m_lcodIRange << 7;
    // most probably state.
    // it is more likely to decode most probably value.
    if (m_lcodIOffset < scaledRange)
    {
        binVal = pState & 1;
        context_hevc[ctxIdx] = transIdxMPSH265[pState];

        if ( scaledRange >= g_scaled256)
        {
#if INSTRUMENTED_CABAC
            PRINT_CABAC_VALUES(binVal, range, m_lcodIRange);
#endif
            return binVal;
        }

        m_lcodIRange = scaledRange >> 6;
        m_lcodIOffset += m_lcodIOffset;
#if (CABAC_MAGIC_BITS > 0)
        m_iExtendedBits -= 1;
        if (0 >= m_iExtendedBits)
            RefreshCABACBits(m_lcodIOffset, m_pExtendedBits, m_iExtendedBits);
#else // !(CABAC_MAGIC_BITS > 0)
        if (++m_bitsNeeded == 0)
        {
            m_bitsNeeded = -8;
            m_LastByte = GetPredefinedBits<8>();
            m_lcodIOffset += m_LastByte;
        }
#endif // (CABAC_MAGIC_BITS > 0)
    }
    else
    {
        int32_t numBits = c_RenormTable[codIRangeLPS >> 3];
        m_lcodIOffset = (m_lcodIOffset - scaledRange) << numBits;
        m_lcodIRange = codIRangeLPS << (CABAC_MAGIC_BITS + numBits);

        binVal = (pState & 1) ^ 1;
        context_hevc[ctxIdx] = transIdxLPSH265[pState];

#if (CABAC_MAGIC_BITS > 0)
        m_iExtendedBits -= numBits;
        if (0 >= m_iExtendedBits)
            RefreshCABACBits(m_lcodIOffset, m_pExtendedBits, m_iExtendedBits);
#else // !(CABAC_MAGIC_BITS > 0)
        m_bitsNeeded += numBits;

        if (m_bitsNeeded >= 0 )
        {
            m_LastByte = GetPredefinedBits<8>();
            m_lcodIOffset += m_LastByte << m_bitsNeeded;
            m_bitsNeeded -= 8;
        }
#endif // (CABAC_MAGIC_BITS > 0)

    }
#if INSTRUMENTED_CABAC
    PRINT_CABAC_VALUES(binVal, range, m_lcodIRange);
#endif
    return binVal;

} //int32_t H265Bitstream::DecodeSingleBin_CABAC(int32_t ctxIdx)

#endif // defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

// Decode terminating flag for slice end or row end in WPP case
H265_FORCEINLINE
uint32_t H265Bitstream::DecodeTerminatingBit_CABAC(void)
{
    uint32_t Bin = 1;
    m_lcodIRange -= (2<<CABAC_MAGIC_BITS);
#if (CABAC_MAGIC_BITS > 0)
    unsigned long long
#else
    uint32_t
#endif
    scaledRange = m_lcodIRange << 7;
    if (m_lcodIOffset < scaledRange)
    {
        Bin = 0;
        if (scaledRange < g_scaled256)
        {
            m_lcodIRange = scaledRange >> 6;
            m_lcodIOffset += m_lcodIOffset;

#if (CABAC_MAGIC_BITS > 0)
            m_iExtendedBits -= 1;
            if (0 >= m_iExtendedBits)
                RefreshCABACBits(m_lcodIOffset, m_pExtendedBits, m_iExtendedBits);
#else // !(CABAC_MAGIC_BITS > 0)
            if (++m_bitsNeeded == 0)
            {
                m_bitsNeeded = -8;
                m_LastByte = GetPredefinedBits<8>();
                m_lcodIOffset += m_LastByte;
            }
#endif // (CABAC_MAGIC_BITS > 0)

        }
    }

    return Bin;
} //uint32_t H265Bitstream::DecodeTerminatingBit_CABAC(void)

// Decode one bit encoded with CABAC bypass
H265_FORCEINLINE
uint32_t H265Bitstream::DecodeSingleBinEP_CABAC(void)
{
#if INSTRUMENTED_CABAC
    //uint32_t range = m_lcodIRange;
#endif

    m_lcodIOffset += m_lcodIOffset;

#if (CABAC_MAGIC_BITS > 0)
    m_iExtendedBits -= 1;
    if (0 >= m_iExtendedBits)
        RefreshCABACBits(m_lcodIOffset, m_pExtendedBits, m_iExtendedBits);
#else // !(CABAC_MAGIC_BITS > 0)
    if (++m_bitsNeeded >= 0)
    {
        m_bitsNeeded = -8;
        m_LastByte = GetPredefinedBits<8>();
        m_lcodIOffset += m_LastByte;
    }
#endif // (CABAC_MAGIC_BITS > 0)

    uint32_t Bin = 0;
#if (CABAC_MAGIC_BITS > 0)
    unsigned long long
#else
    uint32_t
#endif
    scaledRange = m_lcodIRange << 7;
    if (m_lcodIOffset >= scaledRange)
    {
        Bin = 1;
        m_lcodIOffset -= scaledRange;
    }

#if INSTRUMENTED_CABAC
    //PRINT_CABAC_VALUES(Bin, range, m_lcodIRange);
#endif
    return Bin;
} //uint32_t H265Bitstream::DecodeSingleBinEP_CABAC(void)

// Decode N bits encoded with CABAC bypass
H265_FORCEINLINE
uint32_t H265Bitstream::DecodeBypassBins_CABAC(int32_t numBins)
{
#if INSTRUMENTED_CABAC
    //uint32_t range = m_lcodIRange;
#endif

    uint32_t bins = 0;

#if (CABAC_MAGIC_BITS > 0)
    while (numBins > CABAC_MAGIC_BITS)
    {
        m_iExtendedBits -= CABAC_MAGIC_BITS;
        if (0 >= m_iExtendedBits)
            RefreshCABACBits(m_lcodIOffset, m_pExtendedBits, m_iExtendedBits);

        unsigned long long scaledRange = m_lcodIRange << (7+CABAC_MAGIC_BITS);
        for (int32_t i = 0; i < CABAC_MAGIC_BITS; i++)
        {
            bins += bins;
            scaledRange >>= 1;
            if (m_lcodIOffset >= scaledRange)
            {
                bins++;
                m_lcodIOffset -= scaledRange;
            }
        }
        numBins -= CABAC_MAGIC_BITS;
    }

    m_iExtendedBits -= numBins;
    m_lcodIOffset <<= numBins;

   if (0 >= m_iExtendedBits)
         RefreshCABACBits(m_lcodIOffset, m_pExtendedBits, m_iExtendedBits);

#else // !(CABAC_MAGIC_BITS > 0)
    while (numBins > 8)
    {
        m_LastByte = GetPredefinedBits<8>();
        m_lcodIOffset = (m_lcodIOffset << 8) + (m_LastByte << (8 + m_bitsNeeded));

        uint32_t scaledRange = (uint32_t)(m_lcodIRange << 15);
        for (int32_t i = 0; i < 8; i++)
        {
            bins += bins;
            scaledRange >>= 1;
            if (m_lcodIOffset >= scaledRange)
            {
                bins++;
                m_lcodIOffset -= scaledRange;
            }
#if INSTRUMENTED_CABAC
            //PRINT_CABAC_VALUES(bins & 1, range, m_lcodIRange);
#endif
        }
        numBins -= 8;
    }

    m_bitsNeeded += numBins;
    m_lcodIOffset <<= numBins;

    if (m_bitsNeeded >= 0)
    {
        m_LastByte = GetPredefinedBits<8>();
        m_lcodIOffset += m_LastByte << m_bitsNeeded;
        m_bitsNeeded -= 8;
    }
#endif // (CABAC_MAGIC_BITS > 0)

#if (CABAC_MAGIC_BITS > 0)
    unsigned long long
#else
    uint32_t
#endif
    scaledRange = m_lcodIRange << (numBins + 7);
    for (int32_t i = 0; i < numBins; i++)
    {
        bins += bins;
        scaledRange >>= 1;
        if (m_lcodIOffset >= scaledRange)
        {
            bins++;
            m_lcodIOffset -= scaledRange;
        }
#if INSTRUMENTED_CABAC
        //PRINT_CABAC_VALUES(bins & 1, range, m_lcodIRange);
#endif
    }

    return bins;
}


} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_BITSTREAM_INLINES_H
#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
