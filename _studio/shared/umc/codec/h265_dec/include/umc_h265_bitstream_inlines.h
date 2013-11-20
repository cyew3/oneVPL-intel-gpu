/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_BITSTREAM_INLINES_H
#define __UMC_H265_BITSTREAM_INLINES_H

#include "vm_debug.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_dec_init_tables_cabac.h"
#include "umc_h265_dec_tables.h"

namespace UMC_HEVC_DECODER
{

const Ipp64u g_scaled256 = (Ipp64u)0x100 << (7+ CABAC_MAGIC_BITS);

#define _h265GetBits(current_data, offset, nbits, data) \
{ \
    Ipp32u x; \
 \
    VM_ASSERT((nbits) > 0 && (nbits) <= 32); \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    offset -= (nbits); \
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
        current_data++; \
    } \
 \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 \
    (data) = x & bits_data[nbits]; \
}

#define ippiSkipNBits(current_data, offset, nbits) \
{ \
    /* check error(s) */ \
    VM_ASSERT((nbits) > 0 && (nbits) <= 32); \
    VM_ASSERT(offset >= 0 && offset <= 31); \
    /* decrease number of available bits */ \
    offset -= (nbits); \
    /* normalize bitstream pointer */ \
    if (0 > offset) \
    { \
        offset += 32; \
        current_data++; \
    } \
    /* check error(s) again */ \
    VM_ASSERT(offset >= 0 && offset <= 31); \
 }


#define ippiGetBits1(current_data, offset, data) \
{ \
    data = ((current_data[0] >> (offset)) & 1);  \
    offset -= 1; \
    if (offset < 0) \
    { \
        offset = 31; \
        current_data += 1; \
    } \
}

/*#define ippiGetBits1( current_data, offset, data) \
    _h264GetBits(current_data, offset, 1, data);
*/
#define ippiGetBits8( current_data, offset, data) \
    _h265GetBits(current_data, offset, 8, data);

#define ippiGetNBits( current_data, offset, nbits, data) \
    _h265GetBits(current_data, offset, nbits, data);

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
    Ipp32u x; \
 \
    VM_ASSERT((nbits) > 0 && (nbits) <= 32); \
    VM_ASSERT(nbits >= 0 && nbits <= 31); \
 \
    Ipp32s offset = bp - (nbits); \
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
inline void H265BaseBitstream::AlignPointerRight(void)
{
    ippiAlignBSPointerRight(m_pbs, m_bitOffset);

} // void H265BaseBitstream::AlignPointerRight(void)

//hevc CABAC from HM50
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

H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeSingleBin_CABAC(Ipp32u ctxIdx)
{
    return (this->*s_pDecodeSingleBin_CABAC_dispatched)( ctxIdx );
}

#else // defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

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
            PRINT_CABAC_VALUES(binVal, range);
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
            m_LastByte = GetBits(8);
            m_lcodIOffset += m_LastByte;
        }
#endif // (CABAC_MAGIC_BITS > 0)
    }
    else
    {
        Ipp32s numBits = RenormTable[codIRangeLPS >> 3];
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
            m_LastByte = GetBits(8);
            m_lcodIOffset += m_LastByte << m_bitsNeeded;
            m_bitsNeeded -= 8;
        }
#endif // (CABAC_MAGIC_BITS > 0)
 
    }
#if INSTRUMENTED_CABAC
    PRINT_CABAC_VALUES(binVal, range);
#endif
    return binVal;

} //Ipp32s H265Bitstream::DecodeSingleBin_CABAC(Ipp32s ctxIdx)

#endif // defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeTerminatingBit_CABAC(void)
{
#if INSTRUMENTED_CABAC
    Ipp32u range = m_lcodIRange;
#endif

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
                m_LastByte = GetBits(8);
                m_lcodIOffset += m_LastByte;
            }
#endif // (CABAC_MAGIC_BITS > 0)
        
        }
    }
#if INSTRUMENTED_CABAC
    PRINT_CABAC_VALUES(Bin, range);
#endif
    return Bin;
} //Ipp32u H265Bitstream::DecodeTerminatingBit_CABAC(void)


H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeSingleBinEP_CABAC(void)
{
#if INSTRUMENTED_CABAC
    Ipp32u range = m_lcodIRange;
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
        m_LastByte = GetBits(8);
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
    PRINT_CABAC_VALUES(Bin, range);
#endif
    return Bin;
} //Ipp32u H265Bitstream::DecodeSingleBinEP_CABAC(void)

H265_FORCEINLINE
Ipp32u H265Bitstream::DecodeBypassBins_CABAC(Ipp32s numBins)
{
#if INSTRUMENTED_CABAC
    Ipp32u range = m_lcodIRange;
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
        m_LastByte = GetBits(8);
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
            PRINT_CABAC_VALUES(bins & 1, range);
#endif
        }
        numBins -= 8;
    }

    m_bitsNeeded += numBins;
    m_lcodIOffset <<= numBins;

    if (m_bitsNeeded >= 0)
    {
        m_LastByte = GetBits(8);
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
        PRINT_CABAC_VALUES(bins & 1, range);
#endif
    }

    return bins;
}

inline
Ipp32u H265BaseBitstream::GetBits(const Ipp32u nbits)
{
    Ipp32u w, n = nbits;

    ippiGetNBits(m_pbs, m_bitOffset, n, w);
    return(w);
}

inline
Ipp32s H265BaseBitstream::GetVLCElement(bool bIsSigned)
{
    Ipp32s sval = 0;

    IppStatus ippRes = ippiDecodeExpGolombOne_H264_1u32s(&m_pbs, &m_bitOffset, &sval, bIsSigned);

    if (ippStsNoErr > ippRes)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    return sval;

}

inline Ipp8u H265BaseBitstream::Get1Bit()
{
    Ipp32u w;

    ippiGetBits1(m_pbs, m_bitOffset, w);
    return (Ipp8u)w;

} // H265Bitstream::Get1Bit()

inline size_t H265BaseBitstream::BytesDecoded() const
{
    return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase) +
            ((31 - m_bitOffset) >> 3);
}

inline size_t H265BaseBitstream::BitsDecoded() const
{
    return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase) * 8 +
        (31 - m_bitOffset);
}

inline size_t H265BaseBitstream::BytesLeft() const
{
    return (Ipp32s)m_maxBsSize - (Ipp32s) BytesDecoded();
}

////h265:
inline unsigned H265BaseBitstream::getNumBitsUntilByteAligned() const
{
    return ((m_bitOffset + 1) % 8); //WTF from HM
}

inline void H265BaseBitstream::readOutTrailingBits()
{
    Ipp32u uVal = Get1Bit();
    //VM_ASSERT(1 == uVal);

    Ipp32u bits = getNumBitsUntilByteAligned();

    if (bits)
    {
//        VM_ASSERT(getNumBitsLeft() <= bits); TODO: Implement this assertion correctly
        uVal = GetBits(bits);
        //VM_ASSERT(0 == uVal);
    }
}

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_BITSTREAM_INLINES_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
