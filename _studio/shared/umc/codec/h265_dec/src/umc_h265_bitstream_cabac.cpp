/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "mfx_h265_optimization.h"

namespace UMC_HEVC_DECODER
{

// Table for CABAC contexts initialization
const Ipp8u cabacInitTable[3][NUM_CTX] = 
{
    { 107, 139, 126, 197, 185, 201, 154, 137, 154, 139, 154, 154, 154, 134, 183, 152, 139,  95,  79,  63,  31,  31, 169, 198, 153,
      153, 154, 154, 154, 153, 111, 154, 154, 154, 149,  92, 167, 154, 154,  79, 121, 140,  61, 154, 170, 154, 139, 153, 139, 123,
      123,  63, 124, 166, 183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 170, 153, 138, 138,
      122, 121, 122, 121, 167, 151, 183, 140, 151, 183, 140, 125, 110, 124, 110,  95,  94, 125, 111, 111,  79, 125, 126, 111, 111,
       79, 108, 123,  93, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 125, 110, 124, 110,  95,  94, 125, 111, 111,
       79, 125, 126, 111, 111,  79, 108, 123,  93, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 196, 167, 167,
      154, 152, 167, 182, 182, 134, 149, 136, 153, 121, 136, 122, 169, 208, 166, 167, 154, 152, 167, 182, 107, 167,  91, 107, 107,
      167, 168, 154, 153, 160, 224, 167, 122, 139, 139, 154, 154, 154 }, 
    { 107, 139, 126, 197, 185, 201, 110, 122, 154, 139, 154, 154, 154, 149, 154, 152, 139,  95,  79,  63,  31,  31, 140, 198, 153,
      153, 154, 154, 154, 153, 111, 154, 154, 154, 149, 107, 167, 154, 154,  79, 121, 140,  61, 154, 155, 154, 139, 153, 139, 123,
      123,  63, 153, 166, 183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 170, 153, 123, 123,
      107, 121, 107, 121, 167, 151, 183, 140, 151, 183, 140, 125, 110,  94, 110,  95,  79, 125, 111, 110,  78, 110, 111, 111,  95,
       94, 108, 123, 108, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 125, 110,  94, 110,  95,  79, 125, 111, 110,
       78, 110, 111, 111,  95,  94, 108, 123, 108, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 196, 196, 167,
      154, 152, 167, 182, 182, 134, 149, 136, 153, 121, 136, 137, 169, 194, 166, 167, 154, 167, 137, 182, 107, 167,  91, 122, 107,
      167, 168, 154, 153, 185, 124, 138,  94, 139, 139, 154, 154, 154 }, 
    { 139, 141, 157, 154, 154, 154, 154, 154, 184, 154, 154, 154, 154, 154, 184,  63, 139, 154, 154, 154, 154, 154, 154, 154, 154,
      154, 154, 154, 154, 111, 141, 154, 154, 154,  94, 138, 182, 154, 154, 154,  91, 171, 134, 141, 111, 111, 125, 110, 110,  94,
      124, 108, 124, 107, 125, 141, 179, 153, 125, 107, 125, 141, 179, 153, 125, 107, 125, 141, 179, 153, 125, 140, 139, 182, 182,
      152, 136, 152, 136, 153, 136, 139, 111, 136, 139, 111, 110, 110, 124, 125, 140, 153, 125, 127, 140, 109, 111, 143, 127, 111,
       79, 108, 123,  63, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 110, 110, 124, 125, 140, 153, 125, 127, 140,
      109, 111, 143, 127, 111,  79, 108, 123,  63, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 140,  92, 137, 138,
      140, 152, 138, 139, 153,  74, 149,  92, 139, 107, 122, 152, 140, 179, 166, 182, 140, 227, 122, 197, 138, 153, 136, 167, 152,
      152, 154, 154, 153, 200, 153, 138, 138, 139, 139, 154, 154, 154 }
};

// Initialize one CABAC context
void InitializeContext(Ipp8u *pContext, Ipp8u initVal, Ipp32s SliceQPy)
{
    Ipp32s slope      = (initVal >> 4) * 5 - 45;
    Ipp32s offset     = ((initVal & 15) << 3) - 16;
    Ipp32s initState  =  IPP_MIN(IPP_MAX(1, (((slope * SliceQPy) >> 4) + offset)), 126);
    Ipp32u mpState    = (initState >= 64);
    *pContext = Ipp8u(((mpState? (initState - 64) : (63 - initState)) << 1) + mpState);
}

// Initialize all CABAC contexts. HEVC spec 9.3.2.2
void H265Bitstream::InitializeContextVariablesHEVC_CABAC(Ipp32s initializationType, Ipp32s SliceQPy)
{
    Ipp32u l = 0;
    SliceQPy = IPP_MAX(0, SliceQPy);
    SliceQPy = IPP_MIN(51, SliceQPy);

    for (l = 0; l < NUM_CTX; l++)
    {
        InitializeContext(context_hevc + l, cabacInitTable[initializationType][l], SliceQPy);
    }
} //InitializeContextVariablesHEVC_CABAC

// Initialize CABAC decoding engine. HEVC spec 9.3.2.2
void H265Bitstream::InitializeDecodingEngine_CABAC()
{
    AlignPointerRight();

    m_lcodIRange = 0x01fe;
    m_lcodIOffset = GetBits(8) << 8;
    m_LastByte = GetBits(8);
    m_lcodIOffset |= m_LastByte;

#if (CABAC_MAGIC_BITS > 0)
    m_lcodIRange = m_lcodIRange << CABAC_MAGIC_BITS;
    m_lcodIOffset = m_lcodIOffset << CABAC_MAGIC_BITS;
    {
        Ipp32u nBits;

        m_iExtendedBits = (m_bitOffset % CABAC_MAGIC_BITS) + 1;
        nBits = GetBits(m_iExtendedBits);
        m_lcodIOffset |= nBits << (CABAC_MAGIC_BITS - m_iExtendedBits);

        m_pExtendedBits = ((Ipp16u *) m_pbs) + ((15 == m_bitOffset) ? (1) : (0));
    }
#else
    m_bitsNeeded = -8;
#endif // (CABAC_MAGIC_BITS > 0)

} // void H265Bitstream::InitializeDecodingEngine_CABAC(void)

// Terminate CABAC decoding engine, rollback preread bits
void H265Bitstream::TerminateDecode_CABAC(void)
{
#if (CABAC_MAGIC_BITS > 0)
    // restore source pointer
    m_pbs = (Ipp32u *) (((size_t) m_pExtendedBits) & -0x04);
    m_bitOffset = (((size_t) m_pExtendedBits) & 0x02) ? (15) : (31);
    // return prereaded bits
    ippiUngetNBits(m_pbs, m_bitOffset, m_iExtendedBits);
    m_iExtendedBits = 0;
    m_lcodIOffset &= -0x10000;
#endif
    AlignPointerRight();

} // void H265Bitstream::TerminateDecode_CABAC(void)

// Reset CABAC state
void H265Bitstream::ResetBac_CABAC()
{
    m_lcodIRange = 510;
    m_lcodIOffset = GetBits(16);

#if (CABAC_MAGIC_BITS > 0)
    m_lcodIRange = m_lcodIRange << CABAC_MAGIC_BITS;
    m_lcodIOffset = m_lcodIOffset << CABAC_MAGIC_BITS;
    {
        Ipp32u nBits;

        m_iExtendedBits = (m_bitOffset % 16) + 1;
        nBits = GetBits(m_iExtendedBits);
        m_lcodIOffset |= nBits << (16 - m_iExtendedBits);

        m_pExtendedBits = ((Ipp16u *) m_pbs) + ((15 == m_bitOffset) ? (1) : (0));
    }
#else
    m_bitsNeeded = -8;
#endif // (CABAC_MAGIC_BITS > 0)
}


#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

Ipp32u H265Bitstream::DecodeSingleBin_CABAC_cmov(Ipp32u ctxIdx)
{
    Ipp32u pState = context_hevc[ctxIdx];
    Ipp32u codIRangeLPS = rangeTabLPSH265[pState][(m_lcodIRange >> 6) - 4];
    m_lcodIRange -= codIRangeLPS;

    Ipp32u binVal      = pState & 1;
    Ipp32u transState  = (pState < 124) ? pState + 2 : pState;       // transIdxMPSH265[pState];
    Ipp32u scaledRange = m_lcodIRange << 7;
    Ipp32u lcodIOffset = m_lcodIOffset;
    Ipp32u numBits     = (scaledRange < g_scaled256) ? 1 : 0;

    __asm__ (
        "\n    xorl     %%edx, %%edx"
        "\n    subl     %[scaledRange],   %[lcodIOffset]"                // if ( (lcodIOffset -= scaledRange ) >= 0 ) {
        "\n    cmovnbl  %[lcodIOffset],   %[m_lcodIOffset]"              //    m_lcodIOffset = lcodIOffset;
        "\n    cmovnbl  %[codIRangeLPS],  %[m_lcodIRange]"               //    m_lcodIRange = codIRangeLPS;
        "\n    cmovnbl  %[numBitsL],      %[numBits]"                    //    numBits = numBitsL;
        "\n    cmovnbl  %[transLPSState], %[transState]"                 //    transState = transIdxLPSH265[pState];
        "\n    setnb    %%dl"                                            //    binVal = binVal ^ 1;
        "\n    xorl     %%edx,            %[binVal]"                     // }
        "\n    shll     %%cl, %[m_lcodIOffset]"                          // m_lcodIOffset <<= numBits;
        "\n    shll     %%cl, %[m_lcodIRange]"                           // m_lcodIRange <<= numBits;
        : [numBits]        "+c" (numBits)
        , [lcodIOffset]    "+r" (lcodIOffset)
        , [transState]     "+r" (transState)
        , [binVal]         "+a" (binVal)
        , [m_lcodIOffset]  "+r" (m_lcodIOffset)
        , [m_lcodIRange]   "+r" (m_lcodIRange)
        : [scaledRange]    "rm" (scaledRange)
        , [transLPSState]   "m" (transIdxLPSH265[pState])
        , [numBitsL]        "m" (c_RenormTable[codIRangeLPS >> 3])
        , [codIRangeLPS]    "r" (codIRangeLPS)
        : "%edx"
    );

    context_hevc[ctxIdx] = (Ipp8u)transState;

    m_bitsNeeded += numBits;
    if (m_bitsNeeded >= 0 )
    {
        m_LastByte = GetPredefinedBits<8>();
        m_lcodIOffset += (m_LastByte << m_bitsNeeded);
        m_bitsNeeded -= 8;
    }

    return binVal;
}

Ipp32u H265Bitstream::DecodeSingleBin_CABAC_cmov_BMI(Ipp32u ctxIdx)
{
    Ipp32u pState = context_hevc[ctxIdx];
    Ipp32u codIRangeLPS = rangeTabLPSH265[pState][(m_lcodIRange >> 6) - 4];
    m_lcodIRange -= codIRangeLPS;

    Ipp32u binVal      = pState & 1;
    Ipp32u transState  = (pState < 124) ? pState + 2 : pState;       // transIdxMPSH265[pState];
    Ipp32u scaledRange = m_lcodIRange << 7;
    Ipp32u lcodIOffset = m_lcodIOffset;
    Ipp32u numBits     = (scaledRange < g_scaled256) ? 1 : 0;
    Ipp32u numBitsL    = _lzcnt_u32( codIRangeLPS ) - (32 - 6 - 3);    //  = c_RenormTable[codIRangeLPS >> 3];

    __asm__ (
        "\n    xorl     %%edx, %%edx"
        "\n    subl     %[scaledRange],   %[lcodIOffset]"                // if ( (lcodIOffset -= scaledRange ) >= 0 ) {
        "\n    cmovnbl  %[lcodIOffset],   %[m_lcodIOffset]"              //    m_lcodIOffset = lcodIOffset;
        "\n    cmovnbl  %[codIRangeLPS],  %[m_lcodIRange]"               //    m_lcodIRange = codIRangeLPS;
        "\n    cmovnbl  %[numBitsL],      %[numBits]"                    //    numBits = numBitsL;
        "\n    cmovnbl  %[transLPSState], %[transState]"                 //    transState = transIdxLPSH265[pState];
        "\n    setnb    %%dl"                                            //    binVal = binVal ^ 1;
        "\n    xorl     %%edx,            %[binVal]"                     // }
        "\n    shlx     %[numBits], %[m_lcodIOffset], %[m_lcodIOffset]"  // m_lcodIOffset <<= numBits;
        "\n    shlx     %[numBits], %[m_lcodIRange], %[m_lcodIRange]"    // m_lcodIRange <<= numBits;
        : [numBits]        "+r" (numBits)
        , [lcodIOffset]    "+r" (lcodIOffset)
        , [transState]     "+r" (transState)
        , [binVal]         "+a" (binVal)
        , [m_lcodIOffset]  "+r" (m_lcodIOffset)
        , [m_lcodIRange]   "+r" (m_lcodIRange)
        : [scaledRange]    "rm" (scaledRange)
        , [transLPSState]   "m" (transIdxLPSH265[pState])
        , [numBitsL]        "r" (numBitsL)
        , [codIRangeLPS]    "r" (codIRangeLPS)
        : "%edx"
    );

    context_hevc[ctxIdx] = (Ipp8u)transState;

    m_bitsNeeded += numBits;
    if (m_bitsNeeded >= 0 )
    {
        m_LastByte = GetBits_BMI(8);
        m_lcodIOffset += _shlx_u32( m_LastByte, m_bitsNeeded );
        m_bitsNeeded -= 8;
    }

    return binVal;
}

t_DecodeSingleBin_CABAC s_pDecodeSingleBin_CABAC_dispatched = 
    _may_i_use_cpu_feature( _FEATURE_BMI | _FEATURE_LZCNT ) ? 
        &H265Bitstream::DecodeSingleBin_CABAC_cmov_BMI : 
        &H265Bitstream::DecodeSingleBin_CABAC_cmov;

#endif // defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
