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

#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "mfx_h265_optimization.h"

namespace UMC_HEVC_DECODER
{

// Table for CABAC contexts initialization
const uint8_t cabacInitTable[3][NUM_CTX] = 
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
void InitializeContext(uint8_t *pContext, uint8_t initVal, int32_t SliceQPy)
{
    int32_t slope      = (initVal >> 4) * 5 - 45;
    int32_t offset     = ((initVal & 15) << 3) - 16;
    int32_t initState  =  MFX_MIN(MFX_MAX(1, (((slope * SliceQPy) >> 4) + offset)), 126);
    uint32_t mpState    = (initState >= 64);
    *pContext = uint8_t(((mpState? (initState - 64) : (63 - initState)) << 1) + mpState);
}

// Initialize all CABAC contexts. HEVC spec 9.3.2.2
void H265Bitstream::InitializeContextVariablesHEVC_CABAC(int32_t initializationType, int32_t SliceQPy)
{
    uint32_t l = 0;
    SliceQPy = MFX_MAX(0, SliceQPy);
    SliceQPy = MFX_MIN(51, SliceQPy);

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
        uint32_t nBits;

        m_iExtendedBits = (m_bitOffset % CABAC_MAGIC_BITS) + 1;
        nBits = GetBits(m_iExtendedBits);
        m_lcodIOffset |= nBits << (CABAC_MAGIC_BITS - m_iExtendedBits);

        m_pExtendedBits = ((uint16_t *) m_pbs) + ((15 == m_bitOffset) ? (1) : (0));
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
    m_pbs = (uint32_t *) (((size_t) m_pExtendedBits) & -0x04);
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
        uint32_t nBits;

        m_iExtendedBits = (m_bitOffset % 16) + 1;
        nBits = GetBits(m_iExtendedBits);
        m_lcodIOffset |= nBits << (16 - m_iExtendedBits);

        m_pExtendedBits = ((uint16_t *) m_pbs) + ((15 == m_bitOffset) ? (1) : (0));
    }
#else
    m_bitsNeeded = -8;
#endif // (CABAC_MAGIC_BITS > 0)
}


#if INSTRUMENTED_CABAC == 0 && defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

uint32_t H265Bitstream::DecodeSingleBin_CABAC_cmov(uint32_t ctxIdx)
{
    uint32_t pState = context_hevc[ctxIdx];
    uint32_t codIRangeLPS = rangeTabLPSH265[pState][(m_lcodIRange >> 6) - 4];
    m_lcodIRange -= codIRangeLPS;

    uint32_t binVal      = pState & 1;
    uint32_t transState  = (pState < 124) ? pState + 2 : pState;       // transIdxMPSH265[pState];
    uint32_t scaledRange = m_lcodIRange << 7;
    uint32_t lcodIOffset = m_lcodIOffset;
    uint32_t numBits     = (scaledRange < g_scaled256) ? 1 : 0;

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

    context_hevc[ctxIdx] = (uint8_t)transState;

    m_bitsNeeded += numBits;
    if (m_bitsNeeded >= 0 )
    {
        m_LastByte = GetPredefinedBits<8>();
        m_lcodIOffset += (m_LastByte << m_bitsNeeded);
        m_bitsNeeded -= 8;
    }

    return binVal;
}

uint32_t H265Bitstream::DecodeSingleBin_CABAC_cmov_BMI(uint32_t ctxIdx)
{
    uint32_t pState = context_hevc[ctxIdx];
    uint32_t codIRangeLPS = rangeTabLPSH265[pState][(m_lcodIRange >> 6) - 4];
    m_lcodIRange -= codIRangeLPS;

    uint32_t binVal      = pState & 1;
    uint32_t transState  = (pState < 124) ? pState + 2 : pState;       // transIdxMPSH265[pState];
    uint32_t scaledRange = m_lcodIRange << 7;
    uint32_t lcodIOffset = m_lcodIOffset;
    uint32_t numBits     = (scaledRange < g_scaled256) ? 1 : 0;
    uint32_t numBitsL    = _lzcnt_u32( codIRangeLPS ) - (32 - 6 - 3);    //  = c_RenormTable[codIRangeLPS >> 3];

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

    context_hevc[ctxIdx] = (uint8_t)transState;

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

#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
