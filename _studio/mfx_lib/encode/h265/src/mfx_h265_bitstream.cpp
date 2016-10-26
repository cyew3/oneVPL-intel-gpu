//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_bitstream.h"
#include <immintrin.h>

namespace H265Enc {

#ifdef DEBUG_CABAC
int DEBUG_CABAC_PRINT = 0;
int debug_cabac_print_num = 0;
int debug_cabac_print_num_stop = 79466;
#endif

/* helper functions */
#if defined WIN32

inline Ipp32s IPP_INT_PTR( const void* ptr )  {
    union { void* Ptr; Ipp32s Int; } dd;
    dd.Ptr = (void *)ptr;
    return dd.Int;
}

inline Ipp32u IPP_UINT_PTR( const void* ptr )  {
    union {void* Ptr; Ipp32u Int; } dd;
    dd.Ptr = (void*)ptr;
    return dd.Int;
}

#elif defined WIN64

inline Ipp64s IPP_INT_PTR( const void* ptr )  {
    union { void* Ptr; Ipp64s Int; } dd;
    dd.Ptr = (void*)ptr;
    return dd.Int;
}

inline Ipp64u IPP_UINT_PTR( const void* ptr )  {
    union { void* Ptr; Ipp64u Int; } dd;
    dd.Ptr = (void*)ptr;
    return dd.Int;
}

#else

    #define IPP_INT_PTR( ptr )  ( (long)(ptr) )
    #define IPP_UINT_PTR( ptr ) ( (unsigned long)(ptr) )

#endif

// Swaps bytes in 32-bit word
static Ipp32u SwapBytes_32u( Ipp32u x )
{
    return ( ( x & 0x000000FF ) << 24 ) |
            ( ( x & 0x0000FF00 ) << 8 ) |
            ( ( x & 0x00FF0000 ) >> 8 ) |
            ( ( x & 0xFF000000 ) >> 24 );
}

// Extracts low 32-bit dword of a 64-bit qword
static Ipp32u ExtractLow_64u32u( Ipp64u x )
{
    return (Ipp32u)x;
}

// Extracts high 32-bit dword of a 64-bit qword
static Ipp32u ExtractHigh_64u32u( Ipp64u x )
{
    return (Ipp32u)( x >> 32 );
}

// Returns the zero-based position of the most significant bit in a 32-bit dword
static Ipp32u BitScanReverse_32u( Ipp32u x )
{
    Ipp32u n = (Ipp32u)(-1);
    while( x != 0 ) {
        x >>= 1;
        n++;
    }
    return n;
}

// Carries out addition with carry.
// After the addition carry is updated appropriately.
static Ipp8u AddWithCarry_8u( Ipp8u a, Ipp8u b, Ipp8u* pCarry )
{
    Ipp8u result = a + b + *pCarry;
    *pCarry = (Ipp8u)( result < a );
    return result;
}

// Carries out addition with carry.
// After the addition carry is updated appropriately.
static Ipp64u AddWithCarry_64u( Ipp64u a, Ipp64u b, Ipp8u* pCarry )
{
    Ipp64u result = a + b + (Ipp64u)(*pCarry);
    *pCarry = (Ipp8u)( result < a );
    return result;
}

void H265BsReal::PutBit(Ipp32u code)
{
    H265BsReal* bs = this;
    if (!bs->m_base.m_bitOffset) {
        bs->m_base.m_pbs[0] = (Ipp8u)(code<<7);
        bs->m_base.m_bitOffset = 1;
    } else
    if (bs->m_base.m_bitOffset == 7) {
        bs->m_base.m_pbs[0] =
            (Ipp8u)(bs->m_base.m_pbs[0] | (code & 1));
        bs->m_base.m_pbs++;
        bs->m_base.m_bitOffset = 0;
    } else {
        if (code & 1)
            bs->m_base.m_pbs[0] = (Ipp8u)(bs->m_base.m_pbs[0] | (0x01 << (7 - bs->m_base.m_bitOffset)));
        bs->m_base.m_bitOffset++;
    }
}

void H265BsReal::PutBits(Ipp32u code,
    Ipp32u length)
{
    H265BsReal* bs = this;
    // make sure that the number of bits given is <= 24
    // clear any nonzero bits in upper part of code
    VM_ASSERT(length <= 24);
    code <<= (32 - length);

    //bs->m_base.m_pbs[bs->m_base.m_bitOffset] = 0; // need to pre-clean next untouched byte
    if (!bs->m_base.m_bitOffset) {
        bs->m_base.m_pbs[0] = (Ipp8u)(code >> 24);
        bs->m_base.m_pbs[1] = (Ipp8u)(code >> 16);
    } else {
        // shift field so that the given code begins at the current bit
        // offset in the most significant byte of the 32-bit word
        length += bs->m_base.m_bitOffset;
        code >>= bs->m_base.m_bitOffset;
        // write bytes back into memory, big-endian
        bs->m_base.m_pbs[0] = (Ipp8u)((code >> 24) | bs->m_base.m_pbs[0]);
        bs->m_base.m_pbs[1] = (Ipp8u)(code >> 16);
    }

    if (length > 16)
    {
        bs->m_base.m_pbs[2] = (Ipp8u)(code >> 8);
        bs->m_base.m_pbs[3] = (Ipp8u)(code);
    }

    // update bitstream pointer and bit offset
    bs->m_base.m_pbs += (length >> 3);
    bs->m_base.m_bitOffset = (length & 7);
}

Ipp32u H265BsReal::PutVLCCode(Ipp32u code)
{
    Ipp32s i;
    if (code == 0) {
      PutBit(1);
      return 1;
    }

    code += 1;

#if defined(__i386__) && defined(__GNUC__) && (__GNUC__ > 3) && !defined(__INTEL_COMPILER)
    i = 31 - __builtin_clz(code);
#elif defined(__INTEL_COMPILER) && (defined(__i386__) || defined(WIN32)) && !defined(WIN64)
    i = _bit_scan_reverse( code );
#elif defined(_MSC_VER) && (_MSC_FULL_VER >= 140050110) && !defined(__INTEL_COMPILER)
    unsigned long idx;
    _BitScanReverse(&idx, (unsigned long)code);
    i = (Ipp32s)idx;
#else
    i = -1;
    while (code >> (i + 1)) {
        i++;
    }
#endif

    PutBits(0, i);
    PutBits(code, i + 1);
    return 2*i+1;
}

static void AlignBitStreamWithOnes(
        Ipp8u* pBitStream,
        Ipp32u uBitStreamOffsetBits)
{
    if( uBitStreamOffsetBits % 8 != 0  ) {
        pBitStream[uBitStreamOffsetBits / 8] |= (0xFFU >> (uBitStreamOffsetBits % 8) );
    }
}

IppStatus ippiCABACInit_H265(
    IppvcCABACStateH265* pCabacState,
    Ipp8u*          pBitStream,
    Ipp32u          nBitStreamOffsetBits,
    Ipp32u          nBitStreamSize)
{
//    IppvcCABACState_internal* pCabacState = (IppvcCABACState_internal *)pCabacState_;

    {
        Ipp32u nBitStreamOffsetBytes = ( nBitStreamOffsetBits + 7 ) / 8;
        Ipp8u* pBitStreamCurrentByte = pBitStream + nBitStreamOffsetBytes;
        pCabacState->lcodIOffset = 0;
        pCabacState->lcodIRange = 0x1FE;
        pCabacState->pBitStreamStart = pBitStream;
        pCabacState->pBitStreamEnd = (Ipp32u *)ippAlignPtr( pBitStream + nBitStreamSize - 4, 4 );
        pCabacState->nPreCabacBitOffset = nBitStreamOffsetBits;

        AlignBitStreamWithOnes( pBitStream, nBitStreamOffsetBits );

        switch( IPP_UINT_PTR( pBitStreamCurrentByte ) % 4 ) {
            case 0:
                pCabacState->pBitStream = (Ipp32u*)( pBitStreamCurrentByte - 4 );
                pCabacState->nBitsVacant = 1;
                pCabacState->nRegister = SwapBytes_32u( *( pCabacState->pBitStream ) );
                break;
            case 1:
                pCabacState->pBitStream = (Ipp32u*)( pBitStreamCurrentByte - 1 );
                pCabacState->nBitsVacant = 25;
                pCabacState->nRegister = SwapBytes_32u( *( pCabacState->pBitStream ) ) & 0xFF000000U;
                break;
            case 2:
                pCabacState->pBitStream = (Ipp32u*)( pBitStreamCurrentByte - 2 );
                pCabacState->nBitsVacant = 17;
                pCabacState->nRegister = SwapBytes_32u( *( pCabacState->pBitStream ) ) & 0xFFFF0000U;
                break;
            case 3:
                pCabacState->pBitStream = (Ipp32u*)( pBitStreamCurrentByte - 3 );
                pCabacState->nBitsVacant = 9;
                pCabacState->nRegister = SwapBytes_32u( *( pCabacState->pBitStream ) ) & 0xFFFFFF00U;
                break;
        }

        return ippStsNoErr;
    }
}

IppStatus ippiCABACEncodeBin_H265_px (
    CABAC_CONTEXT_H265 *ctx,
    Ipp32u          code,
    IppvcCABACStateH265* pCabacState)
{
//    IPP_BAD_PTR1_RET( pCabacState );
//    IPP_BADARG_RET( code > 1, ippStsOutOfRangeErr );
//    IPP_BADARG_RET( ctxIdx >= 460, ippStsOutOfRangeErr );

    {
        Ipp32u* pBitStream = pCabacState->pBitStream;
        Ipp8u* pBitStreamCopy = (Ipp8u*)pBitStream;

        Ipp32u stateIdx = *ctx;
        Ipp32u codIRange = pCabacState->lcodIRange;
        Ipp32u codIOffset = pCabacState->lcodIOffset;
        Ipp32u codIRangeLPS = tab_cabacRangeTabLps[stateIdx][(codIRange >> 6) & 0x3];

//        printf("range = %d\n",codIRange);

        if( code != stateIdx >> 6 ) {
            // Encoding LPS symbol
            codIOffset += codIRange - codIRangeLPS;
            codIRange = codIRangeLPS;
        } else {
            // Encoding MPS symbol
            codIRange -= codIRangeLPS;
        }

        {
            Ipp32u nBitsVacant = pCabacState->nBitsVacant;
            Ipp32u nBitCount = 8 - BitScanReverse_32u( codIRange );
            Ipp32u nBitsVacantExt = 32 + nBitsVacant - nBitCount;
            codIOffset <<= nBitCount;
            codIRange <<= nBitCount;

            {
                Ipp64u uBitString = codIOffset >> 10;

                Ipp32u uBuffer = pCabacState->nRegister;
                Ipp64u uBufferExt = (Ipp64u)uBuffer << 32;
                Ipp8u bCarry = 0;

                uBufferExt = AddWithCarry_64u( uBufferExt, uBitString << nBitsVacantExt, &bCarry );
                uBuffer = ExtractHigh_64u32u( uBufferExt );
                *pBitStream = SwapBytes_32u( ExtractHigh_64u32u( uBufferExt ) );

                while( bCarry ) {
                    pBitStreamCopy--;
                    *pBitStreamCopy = AddWithCarry_8u( *pBitStreamCopy, 0, &bCarry );
                }

                if( nBitsVacantExt < 32 ) {
                    uBuffer = ExtractLow_64u32u( uBufferExt );
                    pBitStream++;
                    if( pBitStream >= pCabacState->pBitStreamEnd )
                        return ippStsH264BufferFullErr;
                }
                *ctx = tab_cabacTransTbl[code][stateIdx];

                pCabacState->lcodIRange = codIRange;
                pCabacState->lcodIOffset = codIOffset & 0x3FF;

                pCabacState->nBitsVacant = nBitsVacantExt % 32;
                pCabacState->nRegister = uBuffer;
                pCabacState->pBitStream = pBitStream;
            }
        }
        return ippStsNoErr;
    }
}


#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

IppStatus ippiCABACEncodeBin_H265_bmi (
    CABAC_CONTEXT_H265 *ctx,
    Ipp32u          code,
    IppvcCABACStateH265* pCabacState)
{
//    IPP_BAD_PTR1_RET( pCabacState );
//    IPP_BADARG_RET( code > 1, ippStsOutOfRangeErr );
//    IPP_BADARG_RET( ctxIdx >= 460, ippStsOutOfRangeErr );

    {
        Ipp32u* pBitStream = pCabacState->pBitStream;
        Ipp8u* pBitStreamCopy = (Ipp8u*)pBitStream;

        Ipp32u stateIdx = *ctx;
        Ipp32u codIRange = pCabacState->lcodIRange;
        Ipp32u codIOffset = pCabacState->lcodIOffset;
        Ipp32u codIRangeLPS = tab_cabacRangeTabLps[stateIdx][(codIRange >> 6) & 0x3];

//        printf("range = %d\n",codIRange);

        // Encoding MPS symbol
        codIRange -= codIRangeLPS;
        if( code != stateIdx >> 6 ) {
            // Encoding LPS symbol
            codIOffset += codIRange;
            codIRange = codIRangeLPS;
        }

        {
            Ipp32u nBitsVacant = pCabacState->nBitsVacant;
            Ipp32u nBitCount = 8 - (31 - _lzcnt_u32(codIRange));
            Ipp32u nBitsVacantExt = 32 + nBitsVacant - nBitCount;
            codIOffset <<= nBitCount;
            codIRange <<= nBitCount;

            {
                Ipp64u uBitString = codIOffset >> 10;

                Ipp32u uBuffer = pCabacState->nRegister;
                Ipp64u uBufferExt = (Ipp64u)uBuffer << 32;
                Ipp8u bCarry = 0;

                uBufferExt = AddWithCarry_64u( uBufferExt, uBitString << nBitsVacantExt, &bCarry );
                uBuffer = ExtractHigh_64u32u( uBufferExt );
                *pBitStream = SwapBytes_32u( uBuffer );

                while( bCarry ) {
                    pBitStreamCopy--;
                    *pBitStreamCopy = AddWithCarry_8u( *pBitStreamCopy, 0, &bCarry );
                }

                if( nBitsVacantExt < 32 ) {
                    uBuffer = ExtractLow_64u32u( uBufferExt );
                    pBitStream++;
                    if( pBitStream >= pCabacState->pBitStreamEnd )
                        return ippStsH264BufferFullErr;
                }
                *ctx = tab_cabacTransTbl[code][stateIdx];

                pCabacState->lcodIRange = codIRange;
                pCabacState->lcodIOffset = codIOffset & 0x3FF;

                pCabacState->nBitsVacant = nBitsVacantExt % 32;
                pCabacState->nRegister = uBuffer;
                pCabacState->pBitStream = pBitStream;
            }
        }
        return ippStsNoErr;
    }
}

#endif // if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))


IppStatus ippiCABACEncodeBinBypass_H265(
        Ipp32u code,
        IppvcCABACStateH265* pCabacState)
{
//    IPP_BAD_PTR1_RET( pCabacState );
//    IPP_BADARG_RET( code > 1, ippStsOutOfRangeErr );

    {
        Ipp32u* pBitStream = pCabacState->pBitStream;
        Ipp8u* pBitStreamCopy = (Ipp8u*)pBitStream;

        Ipp32u codIRange = pCabacState->lcodIRange;
        Ipp32u codIOffset = pCabacState->lcodIOffset;

        if( code == 1 ) {
            codIOffset += codIOffset + codIRange;
        } else {
            codIOffset += codIOffset;
        }

        {
            Ipp32u nBitsVacant = pCabacState->nBitsVacant;
            Ipp32u nBitsVacantExt = 31 + nBitsVacant;

            {
                Ipp64u uBitString = codIOffset >> 10;

                Ipp32u uBuffer = pCabacState->nRegister;
                Ipp64u uBufferExt = (Ipp64u)uBuffer << 32;
                Ipp8u bCarry = 0;

                uBufferExt = AddWithCarry_64u( uBufferExt, uBitString << nBitsVacantExt, &bCarry );
                uBuffer = ExtractHigh_64u32u( uBufferExt );
                *pBitStream = SwapBytes_32u( ExtractHigh_64u32u( uBufferExt ) );

                while( bCarry ) {
                    pBitStreamCopy--;
                    *pBitStreamCopy = AddWithCarry_8u( *pBitStreamCopy, 0, &bCarry );
                }

                if( nBitsVacantExt < 32 ) {
                    uBuffer = ExtractLow_64u32u( uBufferExt );
                    pBitStream++;
                    if( pBitStream >= pCabacState->pBitStreamEnd )
                        return ippStsH264BufferFullErr;
                }

                pCabacState->lcodIRange = codIRange;
                pCabacState->lcodIOffset = codIOffset & 0x3FF;

                pCabacState->nBitsVacant = nBitsVacantExt % 32;
                pCabacState->nRegister = uBuffer;
                pCabacState->pBitStream = pBitStream;
            }
        }

        return ippStsNoErr;
    }
}

IppStatus ippiCABACTerminateSlice_H265(
        Ipp32u* pBitStreamBytes,
        Ipp32u* pBitOffset,
        IppvcCABACStateH265* pCabacState)
{
//    IPP_BAD_PTR2_RET( pCabacState, pBitStreamBytes );
    {
        Ipp32u* pBitStream = pCabacState->pBitStream;

        Ipp32u codIOffset = pCabacState->lcodIOffset;


        {
            Ipp32u nBitsVacant = pCabacState->nBitsVacant;
            Ipp32u nBitsVacantExt = 29 + nBitsVacant;

            Ipp64u uBitString = ( codIOffset >> 7 ) | 1;

            Ipp32u uBuffer = pCabacState->nRegister;
            Ipp64u uBufferExt = (Ipp64u)uBuffer << 32;
            Ipp8u bCarry = 0;

            uBufferExt = AddWithCarry_64u( uBufferExt, uBitString << nBitsVacantExt, &bCarry );
            *pBitStream = SwapBytes_32u( ExtractHigh_64u32u( uBufferExt ) );
            if( nBitsVacantExt < 32 ) {
                if( pBitStream + 1 < pCabacState->pBitStreamEnd ) {
                    *(pBitStream + 1) = SwapBytes_32u( ExtractLow_64u32u( uBufferExt ) );
                } else
                    return ippStsMemAllocErr;
            }
            *pBitStreamBytes = ( 64 - nBitsVacantExt ) / 8 +
                (IPP_UINT_PTR( pBitStream ) - IPP_UINT_PTR( pCabacState->pBitStreamStart ) );

            *pBitOffset = ( 64 - nBitsVacantExt ) & 7;

            pCabacState->lcodIRange = 0;
            pCabacState->lcodIOffset = 0;

            pCabacState->nBitsVacant = 0;
            pCabacState->nRegister = 0;
            pCabacState->pBitStream = 0;
            pCabacState->pBitStreamStart = 0;
            pCabacState->pBitStreamEnd = 0;
        }
        return ippStsNoErr;
    }
}

void H265BsReal::EncodeSingleBin_CABAC_px(CABAC_CONTEXT_H265 *ctx, Ipp32u code)
{
#ifdef DEBUG_CABAC
    Ipp32u codIRange = this->cabacState.lcodIRange;
    if (DEBUG_CABAC_PRINT) {
      if (debug_cabac_print_num == debug_cabac_print_num_stop)
          printf("");
        printf("cabac %d: %d,%d,%d\n",debug_cabac_print_num++,codIRange,*ctx,code);
    }
#endif
    ippiCABACEncodeBin_H265_px(ctx, code, &this->cabacState);
}

#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

void H265BsReal::EncodeSingleBin_CABAC_bmi(CABAC_CONTEXT_H265 *ctx, Ipp32u code)
{
#ifdef DEBUG_CABAC
    Ipp32u codIRange = this->cabacState.lcodIRange;
    if (DEBUG_CABAC_PRINT) {
      if (debug_cabac_print_num == debug_cabac_print_num_stop)
          printf("");
        printf("cabac %d: %d,%d,%d\n",debug_cabac_print_num++,codIRange,*ctx,code);
    }
#endif
    ippiCABACEncodeBin_H265_bmi(ctx, code, &this->cabacState);
}

// Dispatcher
t_EncodeSingleBin_CABAC s_pEncodeSingleBin_CABAC_dispatched = 
    _may_i_use_cpu_feature( _FEATURE_BMI | _FEATURE_LZCNT ) ? 
        &H265BsReal::EncodeSingleBin_CABAC_bmi : 
        &H265BsReal::EncodeSingleBin_CABAC_px;

#endif

void H265BsReal::EncodeBinEP_CABAC(Ipp32u code)
{
#ifdef DEBUG_CABAC
    Ipp32u codIRange = this->cabacState.lcodIRange;
    if (DEBUG_CABAC_PRINT) {
      if (debug_cabac_print_num == debug_cabac_print_num_stop)
          printf("");
      printf("cabac %d: %d,%d,%d\n",debug_cabac_print_num++,codIRange,-1,code);
    }
#endif

    ippiCABACEncodeBinBypass_H265(code, &this->cabacState);
}

void H265BsReal::EncodeBinsEP_CABAC(Ipp32u code, Ipp32s len)
{
    while (len > 0)
    {
        len--;
        Ipp32u c = (code >> len) & 1;
#ifdef DEBUG_CABAC
        Ipp32u codIRange = this->cabacState.lcodIRange;
        if (DEBUG_CABAC_PRINT) {
          if (debug_cabac_print_num == debug_cabac_print_num_stop)
              printf("");
            printf("cabac %d: %d,%d,%d\n",debug_cabac_print_num++,codIRange, -1, c);
        }
#endif
        ippiCABACEncodeBinBypass_H265(c, &this->cabacState);
    }
}

void H265BsReal::TerminateEncode_CABAC()
{
    Ipp32u streamBytes;
    Ipp32u bitoffset;
//    VM_ASSERT(bs->m_base.pCabacState);
    ippiCABACTerminateSlice_H265( &streamBytes, &bitoffset, &this->cabacState );

    m_base.m_pbs = m_base.m_pbsBase + streamBytes;
    m_base.m_bitOffset = bitoffset;
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
