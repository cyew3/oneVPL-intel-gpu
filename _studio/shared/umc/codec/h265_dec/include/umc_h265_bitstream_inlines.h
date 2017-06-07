//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

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

const Ipp64u g_scaled256 = (Ipp64u)0x100 << (7+ CABAC_MAGIC_BITS);

#if CABAC_MAGIC_BITS == 16
#define RefreshCABACBits(codOffset, pBits, iBits) \
{ \
    Ipp16u *pRealPointer; \
    /* we have to handle the bit pointer very thorougly. */ \
    /* this sophisticated logic is used to avoid compilers' warnings. */ \
    /* In two words we just select required word by the pointer */ \
    pRealPointer = (Ipp16u *) (((Ipp8u *) 0) + \
                               ((((Ipp8u *) pBits) - (Ipp8u *) 0) ^ 2)); \
    codOffset |= *(pRealPointer) << (-iBits); \
    pBits += 1; \
    iBits += 16; \
}
#endif

#if CABAC_MAGIC_BITS == 24
#define RefreshCABACBits(codOffset, pBits, iBits) \
{ \
    Ipp16u *pRealPointer; \
    /* we have to handle the bit pointer very thorougly. */ \
    /* this sophisticated logic is used to avoid compilers' warnings. */ \
    /* In two words we just select required word by the pointer */ \
    pRealPointer = (Ipp16u *) (((Ipp8u *) 0) + \
                               ((((Ipp8u *) pBits) - (Ipp8u *) 0) ^ 2)); \
    codOffset |= *(pRealPointer) << (24-iBits); \
    codOffset |= *((Ipp8u*)pRealPointer + 2) << (8-iBits); \
    pBits = (Ipp16u*)((Ipp8u*)pBits + 3); \
    iBits += 24; \
}
#endif

const Ipp32u c_RenormTable[32] =
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
Ipp32u H265BaseBitstream::GetBits_BMI(Ipp32u nbits)
{
    VM_ASSERT(nbits > 0 && nbits <= 32);
    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    Ipp32u bits;

    m_bitOffset -= nbits;
    Ipp32u shift = m_bitOffset + 1;

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

#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

typedef Ipp32u (H265Bitstream::* t_DecodeSingleBin_CABAC)(Ipp32u ctxIdx);
extern t_DecodeSingleBin_CABAC s_pDecodeSingleBin_CABAC_dispatched;

// Call optimized function by pointer either cmov_BMI or just cmov versions
H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeSingleBin_CABAC(Ipp32u ctxIdx)
{
    return (this->*s_pDecodeSingleBin_CABAC_dispatched)( ctxIdx );
}

#else // defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

// Decode one bin from CABAC stream
inline
Ipp32u H265Bitstream::DecodeSingleBin_CABAC(Ipp32u ctxIdx)
{
#if INSTRUMENTED_CABAC
    Ipp32u range = m_lcodIRange;
#endif

    Ipp32u codIRangeLPS;

    Ipp32u pState = context_hevc[ctxIdx];
    Ipp32u binVal;

    codIRangeLPS = rangeTabLPSH265[pState][(m_lcodIRange >> (6 + CABAC_MAGIC_BITS)) - 4];
    m_lcodIRange -= codIRangeLPS << CABAC_MAGIC_BITS;
#if (CABAC_MAGIC_BITS > 0)
    Ipp64u
#else
    Ipp32u
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
        Ipp32s numBits = c_RenormTable[codIRangeLPS >> 3];
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

} //Ipp32s H265Bitstream::DecodeSingleBin_CABAC(Ipp32s ctxIdx)

#endif // defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

// Decode terminating flag for slice end or row end in WPP case
H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeTerminatingBit_CABAC(void)
{
    Ipp32u Bin = 1;
    m_lcodIRange -= (2<<CABAC_MAGIC_BITS);
#if (CABAC_MAGIC_BITS > 0)
    Ipp64u
#else
    Ipp32u
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
} //Ipp32u H265Bitstream::DecodeTerminatingBit_CABAC(void)

// Decode one bit encoded with CABAC bypass
H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeSingleBinEP_CABAC(void)
{
#if INSTRUMENTED_CABAC
    //Ipp32u range = m_lcodIRange;
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

    Ipp32u Bin = 0;
#if (CABAC_MAGIC_BITS > 0)
    Ipp64u
#else
    Ipp32u
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
} //Ipp32u H265Bitstream::DecodeSingleBinEP_CABAC(void)

// Decode N bits encoded with CABAC bypass
H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeBypassBins_CABAC(Ipp32s numBins)
{
#if INSTRUMENTED_CABAC
    //Ipp32u range = m_lcodIRange;
#endif

    Ipp32u bins = 0;

#if (CABAC_MAGIC_BITS > 0)
    while (numBins > CABAC_MAGIC_BITS)
    {
        m_iExtendedBits -= CABAC_MAGIC_BITS;
        if (0 >= m_iExtendedBits)
            RefreshCABACBits(m_lcodIOffset, m_pExtendedBits, m_iExtendedBits);

        Ipp64u scaledRange = m_lcodIRange << (7+CABAC_MAGIC_BITS);
        for (Ipp32s i = 0; i < CABAC_MAGIC_BITS; i++)
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

        Ipp32u scaledRange = (Ipp32u)(m_lcodIRange << 15);
        for (Ipp32s i = 0; i < 8; i++)
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
    Ipp64u
#else
    Ipp32u
#endif
    scaledRange = m_lcodIRange << (numBins + 7);
    for (Ipp32s i = 0; i < numBins; i++)
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
