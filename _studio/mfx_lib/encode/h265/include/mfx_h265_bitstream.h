//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_BITSTREAM_H__
#define __MFX_H265_BITSTREAM_H__

#include "assert.h"
#include "emmintrin.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_cabac_tables.h"

namespace H265Enc {

#define SIGNED_VLC_CODE(code) (2*ABS(code) - (code > 0))
typedef Ipp8u CABAC_CONTEXT_H265;

struct H265NALUnit {
    Ipp8u nal_unit_type;
    Ipp8u nuh_layer_id;
    Ipp8u nuh_temporal_id;
    // present if nal_unit_type is NAL_UT_PREFIX_SEI or NAL_UT_SUFFIX_SEI
    Ipp32u seiPayloadType;
    Ipp32u seiPayloadSize;
};

typedef struct _IppvcCABACStateH265 {
    Ipp32u* pBitStream;
    Ipp8u* pBitStreamStart;
    Ipp32u* pBitStreamEnd;

    Ipp32u lcodIOffset;
    Ipp32u lcodIRange;
    Ipp32u nBitsVacant;
    Ipp32u nRegister;
    Ipp32u nPreCabacBitOffset;
} IppvcCABACStateH265;

#define NUM_CABAC_CONTEXT ((188+63)&~63)

typedef struct sH265BsBase {
    Ipp8u* m_pbs;  // m_pbs points to the current position of the buffer.
    Ipp8u* m_pbsBase; // m_pbsBase points to the first byte of the buffer.
    Ipp32u m_bitOffset; // Indicates the bit position (0 to 7) in the byte pointed by m_pbs.
    Ipp32u m_maxBsSize; // Maximum buffer size in bytes.};
    Ipp8u* m_pbsRBSPBase;  // Points to beginning of previous "Raw Byte Sequence Payload"

    __ALIGN64 CABAC_CONTEXT_H265 context_array[NUM_CABAC_CONTEXT];
    __ALIGN64 CABAC_CONTEXT_H265 context_array_enc[NUM_CABAC_CONTEXT];
} H265BsBase;

class H265BsFake {
public:
    H265BsBase m_base;

    inline void EncodeBinBypass_CABAC(Ipp32s code)
    {
        H265ENC_UNREFERENCED_PARAMETER(code);
        m_base.m_bitOffset += 256;
    }

    inline void EncodeSingleBin_CABAC(CABAC_CONTEXT_H265 *ctx, Ipp32s code)
    {
        register Ipp8u pStateIdx = *ctx;
        m_base.m_bitOffset += tab_cabacPBits[pStateIdx ^ (code << 6)];
        *ctx = tab_cabacTransTbl[code][pStateIdx];
    }

    inline void EncodeSingleBin_CABAC(const CABAC_CONTEXT_H265 *ctx, Ipp32s code)
    {
        register Ipp8u pStateIdx = *ctx;
        m_base.m_bitOffset += tab_cabacPBits[pStateIdx ^ (code << 6)];
    }

    inline void EncodeBinEP_CABAC(Ipp32u code)
    {
        H265ENC_UNREFERENCED_PARAMETER(code);
        m_base.m_bitOffset += 256;
    }

    inline void EncodeBinsEP_CABAC(Ipp32u code, Ipp32s len)
    {
        H265ENC_UNREFERENCED_PARAMETER(code);
        m_base.m_bitOffset += len << 8;
    }

    void Reset()
    {
        m_base.m_bitOffset = 0;
    }

    Ipp32u GetNumBits()
    {
        return m_base.m_bitOffset >> (8 - BIT_COST_SHIFT);
    }

    void CtxSave(CABAC_CONTEXT_H265 *ptr, Ipp32s offset, Ipp32s num) const
    {
        small_memcpy(ptr + offset, m_base.context_array + offset, num * sizeof(CABAC_CONTEXT_H265));
    }
    
    void CtxSave(CABAC_CONTEXT_H265 *ptr, Ipp32s ctxCategory) const
    {
        CtxSave(ptr, tab_ctxIdxOffset[ctxCategory], tab_ctxIdxSize[ctxCategory]);
    }
    
    void CtxSave(CABAC_CONTEXT_H265 *ptr) const
    {
        const __m128i *src = (const __m128i *)m_base.context_array;
        __m128i *dst = (__m128i *)ptr;
        assert((size_t(src) & 15) == 0);
        assert((size_t(dst) & 15) == 0);
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        //CtxSave(ptr, 0, NUM_CABAC_CONTEXT);
    }
    
    void CtxRestore(const CABAC_CONTEXT_H265 *ptr, Ipp32s offset, Ipp32s num)
    {
        small_memcpy(m_base.context_array + offset, ptr + offset, num*sizeof(CABAC_CONTEXT_H265));
    }
    
    void CtxRestore(const CABAC_CONTEXT_H265 *ptr, Ipp32s ctxCategory)
    {
        CtxRestore(ptr, tab_ctxIdxOffset[ctxCategory], tab_ctxIdxSize[ctxCategory]);
    }

    void CtxRestore(const CABAC_CONTEXT_H265 *ptr)
    {
        const __m128i *src = (const __m128i *)ptr;
        __m128i *dst = (__m128i *)m_base.context_array;
        assert((size_t(src) & 15) == 0);
        assert((size_t(dst) & 15) == 0);
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        _mm_store_si128(dst++, _mm_load_si128(src++));
        //CtxRestore(ptr, 0, NUM_CABAC_CONTEXT);
    }

    int isReal()
    {
        return 0;
    }

    void CtxSaveWPP(CABAC_CONTEXT_H265 *context_array_wpp) {
        small_memcpy(context_array_wpp, m_base.context_array,
            tab_ctxIdxOffset[END_OF_SLICE_FLAG_HEVC] * sizeof(CABAC_CONTEXT_H265));
    }
    void CtxRestoreWPP(CABAC_CONTEXT_H265 *context_array_wpp) {
        small_memcpy(m_base.context_array, context_array_wpp,
            tab_ctxIdxOffset[END_OF_SLICE_FLAG_HEVC] * sizeof(CABAC_CONTEXT_H265));
        m_base.context_array[tab_ctxIdxOffset[END_OF_SLICE_FLAG_HEVC]] = 63;
    }
};

/////
class H265BsReal {
public:
    IppvcCABACStateH265 cabacState;
    H265BsBase m_base;

    void Reset();
    void PutBit(Ipp32u code);
    void PutBits(Ipp32u code, Ipp32u length);
    Ipp32u PutVLCCode(Ipp32u code);
    void WriteTrailingBits();
    void ByteAlignWithZeros();
    void ByteAlignWithOnes();
    mfxU32 WriteNAL(Ipp8u *dst, Ipp32u dstBufSize, Ipp8u startPicture, Ipp32u nalUnitType);
    mfxU32 WriteNAL(mfxBitstream *dst, Ipp8u startPicture, H265NALUnit *nal);

    void EncodeSingleBin_CABAC(CABAC_CONTEXT_H265 *ctx, Ipp32u code);
    void EncodeSingleBin_CABAC_px(CABAC_CONTEXT_H265 *ctx, Ipp32u code);
#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))
    void EncodeSingleBin_CABAC_bmi(CABAC_CONTEXT_H265 *ctx, Ipp32u code);
#endif

    void EncodeBinEP_CABAC(Ipp32u code);

    void EncodeBinsEP_CABAC(Ipp32u code, Ipp32s len);

    void EncodeBins_CABAC(CABAC_CONTEXT_H265 *ctx,
        Ipp32u code,
        Ipp32s len);

    void TerminateEncode_CABAC();

    int isReal() { return 1; }

    void CtxSaveWPP(CABAC_CONTEXT_H265 *context_array_wpp)
    {
        small_memcpy(context_array_wpp, m_base.context_array,
            tab_ctxIdxOffset[END_OF_SLICE_FLAG_HEVC] * sizeof(CABAC_CONTEXT_H265));
    }
    
    void CtxRestoreWPP(CABAC_CONTEXT_H265 *context_array_wpp)
    {
        small_memcpy(m_base.context_array, context_array_wpp,
            tab_ctxIdxOffset[END_OF_SLICE_FLAG_HEVC] * sizeof(CABAC_CONTEXT_H265));
        m_base.context_array[tab_ctxIdxOffset[END_OF_SLICE_FLAG_HEVC]] = 63;
    }
};

// Returns the bit position of the buffer pointer relative to the beginning of the buffer.
template <class H265Bs>
inline
Ipp32u H265Bs_GetBsOffset(
    H265Bs* state)
{
    if (state->m_base.m_pbsBase == NULL)
        return (state->m_base.m_bitOffset + 128) >> 8;
    else
        return(Ipp32u(state->m_base.m_pbs - state->m_base.m_pbsBase) * 8 + state->m_base.m_bitOffset);
}

// Returns the size of the bitstream data in bytes based on the current position of the buffer pointer, m_pbs.
template <class H265Bs>
inline
Ipp32u H265Bs_GetBsSize(
    H265Bs* state)
{
    return (H265Bs_GetBsOffset(state) + 7) >> 3;
}

// Returns the base pointer to the beginning of the bitstream.
template <class H265Bs>
inline
Ipp8u* H265Bs_GetBsBase(
    H265Bs* state)
{
    return state->m_base.m_pbsBase;
}

// Returns the maximum bitstream size.
template <class H265Bs>
inline
Ipp32u H265Bse_GetMaxBsSize(
    H265Bs* state)
{
    return state->m_maxBsSize;
}

// Checks if read/write passed the maximum buffer size.
template <class H265Bs>
inline
bool H265Bs_CheckBsLimit(
    H265Bs* state)
{
    Ipp32u size;
    size = H265Bs_GetBsSize(state);
    if (size > state->m_maxBsSize)
        return false;
    return true;
}

// Assigns new position to the buffer pointer.
template <class H265Bs>
inline
void H265Bs_SetState(
    H265Bs* state,
    Ipp8u* const pbs,
    const Ipp32u bitOffset)
{
    state->m_base.m_pbs       = pbs;
    state->m_base.m_bitOffset = bitOffset;
}

// Obtains current position of the buffer pointer.
template <class H265Bs>
inline
void H265Bs_GetState(
    H265Bs* state,
    Ipp8u** pbs,
    Ipp32u* bitOffset)
{
    *pbs       = state->m_base.m_pbs;
    *bitOffset = state->m_base.m_bitOffset;
}

// Advances buffer pointer with given number of bits.
template <class H265Bs>
inline
void H265Bs_UpdateState(
    H265Bs* state,
    const Ipp32u nbits)
{
    state->m_base.m_pbs      += (nbits + state->m_base.m_bitOffset) >> 3;
    state->m_base.m_bitOffset = (nbits + state->m_base.m_bitOffset) & 0x7;
    // clear next bits to EOB
    state->m_base.m_pbs[0] = (Ipp8u)(state->m_base.m_pbs[0] & (0xff00 >> state->m_base.m_bitOffset));
}

// Resets pointer to the beginning and clears the bitstream buffer.

inline
void H265BsReal::Reset()
{
    H265BsReal* state = this;

    state->m_base.m_pbsRBSPBase = state->m_base.m_pbs = state->m_base.m_pbsBase;
    state->m_base.m_bitOffset = 0;
}

// Write RBSP Trailing Bits to Byte Align

inline
void H265BsReal::WriteTrailingBits()
{
    H265BsReal* state = this;
    // Write Stop Bit
    if(!state->m_base.m_bitOffset)
        state->m_base.m_pbs[0] = 0x80;
    else
        state->m_base.m_pbs[0] = (Ipp8u)(state->m_base.m_pbs[0] | (Ipp8u)(0x01 << (7 - state->m_base.m_bitOffset)));

    state->m_base.m_pbs++;
    //state->m_pbs[0] = 0;
    state->m_base.m_bitOffset = 0;
}
// Add zero bits to byte-align the buffer.
inline
void H265BsReal::ByteAlignWithZeros()
{
    H265BsReal* state = this;
    // No action needed if already byte aligned, i.e. !m_bitOffset
    if (state->m_base.m_bitOffset){ // note that prior write operation automatically clears the unused bits in the current byte*/
        state->m_base.m_pbs++;
        state->m_base.m_bitOffset = 0;
    }
}

// Add one bits to byte-align the buffer.
inline
void H265BsReal::ByteAlignWithOnes()
{
    H265BsReal* state = this;
    // No action needed if already byte aligned, i.e. !m_bitOffset
    if (state->m_base.m_bitOffset){
        state->m_base.m_pbs[0] = (Ipp8u)(state->m_base.m_pbs[0] | (Ipp8u)(0xff >> state->m_base.m_bitOffset));
        state->m_base.m_pbs++;
        //state->m_pbs[0] = 0;
        state->m_base.m_bitOffset = 0;
    }
}

#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))

// Dispatch through function pointer
typedef void (H265BsReal::* t_EncodeSingleBin_CABAC)(CABAC_CONTEXT_H265 *ctx, Ipp32u code);
extern t_EncodeSingleBin_CABAC s_pEncodeSingleBin_CABAC_dispatched;

inline void H265BsReal::EncodeSingleBin_CABAC(CABAC_CONTEXT_H265 *ctx, Ipp32u code) {
    return (this->*s_pEncodeSingleBin_CABAC_dispatched)(ctx, code);
}

#else

// Direct call
inline void H265BsReal::EncodeSingleBin_CABAC(CABAC_CONTEXT_H265 *ctx, Ipp32u code) {
    EncodeSingleBin_CABAC_px(ctx, code);
}

#endif

IppStatus ippiCABACInit_H265(
    IppvcCABACStateH265* pCabacState,
    Ipp8u*          pBitStream,
    Ipp32u          nBitStreamOffsetBits,
    Ipp32u          nBitStreamSize);

void InitializeContextVariablesHEVC_CABAC(CABAC_CONTEXT_H265 *context_hevc, Ipp32s initializationType, Ipp32s SliceQPy);
//Ipp32s h265_tu_had(PixType *src, PixType *rec,
//                   Ipp32s pitch_src, Ipp32s pitch_rec, Ipp32s width, Ipp32s height);

} // namespace

#endif // __MFX_H265_BITSTREAM_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
