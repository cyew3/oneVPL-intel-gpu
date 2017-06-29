//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_BITSTREAM_HEADERS_H_
#define __UMC_H265_BITSTREAM_HEADERS_H_

#include "umc_structures.h"
#include "umc_h265_dec_defs.h"

// Read N bits from 32-bit array
#define GetNBits(current_data, offset, nbits, data) \
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

// Return bitstream position pointers N bits back
#define UngetNBits(current_data, offset, nbits) \
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

// Skip N bits in 32-bit array
#define SkipNBits(current_data, offset, nbits) \
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

// Read 1 bit from 32-bit array
#define GetBits1(current_data, offset, data) \
{ \
    data = ((current_data[0] >> (offset)) & 1);  \
    offset -= 1; \
    if (offset < 0) \
    { \
        offset = 31; \
        current_data += 1; \
    } \
}

// Align bitstream position to byte boundary
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

// Read N bits from 32-bit array
#define PeakNextBits(current_data, bp, nbits, data) \
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

namespace UMC_HEVC_DECODER
{
// Bit masks for fast extraction of bits from bitstream
const Ipp32u bits_data[33] =
{
    (((Ipp32u)0x01 << (0)) - 1),
    (((Ipp32u)0x01 << (1)) - 1),
    (((Ipp32u)0x01 << (2)) - 1),
    (((Ipp32u)0x01 << (3)) - 1),
    (((Ipp32u)0x01 << (4)) - 1),
    (((Ipp32u)0x01 << (5)) - 1),
    (((Ipp32u)0x01 << (6)) - 1),
    (((Ipp32u)0x01 << (7)) - 1),
    (((Ipp32u)0x01 << (8)) - 1),
    (((Ipp32u)0x01 << (9)) - 1),
    (((Ipp32u)0x01 << (10)) - 1),
    (((Ipp32u)0x01 << (11)) - 1),
    (((Ipp32u)0x01 << (12)) - 1),
    (((Ipp32u)0x01 << (13)) - 1),
    (((Ipp32u)0x01 << (14)) - 1),
    (((Ipp32u)0x01 << (15)) - 1),
    (((Ipp32u)0x01 << (16)) - 1),
    (((Ipp32u)0x01 << (17)) - 1),
    (((Ipp32u)0x01 << (18)) - 1),
    (((Ipp32u)0x01 << (19)) - 1),
    (((Ipp32u)0x01 << (20)) - 1),
    (((Ipp32u)0x01 << (21)) - 1),
    (((Ipp32u)0x01 << (22)) - 1),
    (((Ipp32u)0x01 << (23)) - 1),
    (((Ipp32u)0x01 << (24)) - 1),
    (((Ipp32u)0x01 << (25)) - 1),
    (((Ipp32u)0x01 << (26)) - 1),
    (((Ipp32u)0x01 << (27)) - 1),
    (((Ipp32u)0x01 << (28)) - 1),
    (((Ipp32u)0x01 << (29)) - 1),
    (((Ipp32u)0x01 << (30)) - 1),
    (((Ipp32u)0x01 << (31)) - 1),
    ((Ipp32u)0xFFFFFFFF),
};


template <typename T> class HeaderSet;
class Headers;

// Bitstream low level parsing class
class H265BaseBitstream
{
public:

    H265BaseBitstream();
    H265BaseBitstream(Ipp8u * const pb, const Ipp32u maxsize);
    virtual ~H265BaseBitstream();

    // Reset the bitstream with new data pointer
    void Reset(Ipp8u * const pb, const Ipp32u maxsize);
    // Reset the bitstream with new data pointer and bit offset
    void Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize);

    // Align bitstream position to byte boundary
    inline void AlignPointerRight(void);

    // Read N bits from bitstream array
    inline Ipp32u GetBits(Ipp32u nbits);

    // Read N bits from bitstream array
    template <Ipp32u nbits>
    inline Ipp32u GetPredefinedBits();

#if defined( __INTEL_COMPILER )
    // Optimized function which uses bit manipulation instructions (BMI)
    inline Ipp32u GetBits_BMI(Ipp32u nbits);
#endif

    // Read variable length coded unsigned element
    Ipp32u GetVLCElementU();

    // Read variable length coded signed element
    Ipp32s GetVLCElementS();

    // Reads one bit from the buffer.
    Ipp8u Get1Bit();

    // Check that position in bitstream didn't move outside the limit
    bool CheckBSLeft();

    // Check whether more data is present
    bool More_RBSP_Data();

    // Returns number of decoded bytes since last reset
    size_t BytesDecoded() const;

    // Returns number of decoded bits since last reset
    size_t BitsDecoded() const;

    // Returns number of bytes left in bitstream array
    size_t BytesLeft() const;

    // Returns number of bits needed for byte alignment
    unsigned getNumBitsUntilByteAligned() const;

    // Align bitstream to byte boundary
    void readOutTrailingBits();

    // Returns bitstream current buffer pointer
    const Ipp8u *GetRawDataPtr() const    {
        return (const Ipp8u *)m_pbs + (31 - m_bitOffset)/8;
    }

    // Return bitstream array base address and size
    void GetOrg(Ipp32u **pbs, Ipp32u *size);
    // Return current bitstream address and bit offset
    void GetState(Ipp32u **pbs, Ipp32u *bitOffset);
    // Set current bitstream address and bit offset
    void SetState(Ipp32u *pbs, Ipp32u bitOffset);

    // Set current decoding position
    void SetDecodedBytes(size_t);

    size_t GetAllBitsCount()
    {
        return m_maxBsSize;
    }

    size_t BytesDecodedRoundOff()
    {
        return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase);
    }

protected:

    Ipp32u *m_pbs;                                              // (Ipp32u *) pointer to the current position of the buffer.
    Ipp32s m_bitOffset;                                         // (Ipp32s) the bit position (0 to 31) in the dword pointed by m_pbs.
    Ipp32u *m_pbsBase;                                          // (Ipp32u *) pointer to the first byte of the buffer.
    Ipp32u m_maxBsSize;                                         // (Ipp32u) maximum buffer size in bytes.
};

class H265ScalingList;
class H265VideoParamSet;
struct H265SeqParamSet;
class H265Slice;

// Bitstream headers parsing class
class H265HeadersBitstream : public H265BaseBitstream
{
public:

    H265HeadersBitstream();
    H265HeadersBitstream(Ipp8u * const pb, const Ipp32u maxsize);

    // Read and return NAL unit type and NAL storage idc.
    // Bitstream position is expected to be at the start of a NAL unit.
    UMC::Status GetNALUnitType(NalUnitType &nal_unit_type, Ipp32u &nuh_temporal_id);
    // Read optional access unit delimiter from bitstream.
    UMC::Status GetAccessUnitDelimiter(Ipp32u &PicCodType);

    // Parse SEI message
    Ipp32s ParseSEI(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);

    // Parse remaining of slice header after GetSliceHeaderPart1
    void decodeSlice(H265Slice *, const H265SeqParamSet *, const H265PicParamSet *);
    // Parse slice header part which contains PPS ID
    UMC::Status GetSliceHeaderPart1(H265SliceHeader * sliceHdr);
    // Parse full slice header
    UMC::Status GetSliceHeaderFull(H265Slice *, const H265SeqParamSet *, const H265PicParamSet *);

    // Parse scaling list information in SPS or PPS
    void parseScalingList(H265ScalingList *);
    // Reserved for future header extensions
    bool MoreRbspData();

    // Part VPS header
    UMC::Status GetVideoParamSet(H265VideoParamSet *vps);

    // Parse SPS header
    UMC::Status GetSequenceParamSet(H265SeqParamSet *sps);

    // Parse PPS header
    void GetPictureParamSetPart1(H265PicParamSet *pps);
    UMC::Status GetPictureParamSetFull(H265PicParamSet  *pps, H265SeqParamSet const*);
    UMC::Status GetWPPTileInfo(H265SliceHeader *hdr,
                            const H265PicParamSet *pps,
                            const H265SeqParamSet *sps);

    void parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* pRPS, Ipp32u idx);

protected:

    // Parse video usability information block in SPS
    void parseVUI(H265SeqParamSet *sps);

    // Parse weighted prediction table in slice header
    void xParsePredWeightTable(const H265SeqParamSet *sps, H265SliceHeader * sliceHdr);
    // Parse scaling list data block
    void xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId);
    // Parse HRD information in VPS or in VUI block of SPS
    void parseHrdParameters(H265HRD *hrd, Ipp8u commonInfPresentFlag, Ipp32u vps_max_sub_layers);

    // Parse profile tier layers header part in VPS or SPS
    void  parsePTL(H265ProfileTierLevel *rpcPTL, int maxNumSubLayersMinus1);
    // Parse one profile tier layer
    void  parseProfileTier(H265PTL *ptl);

    // Decoding SEI message functions
    Ipp32s sei_message(const HeaderSet<H265SeqParamSet> & sps,Ipp32s current_sps,H265SEIPayLoad *spl);
    // Parse SEI payload data
    Ipp32s sei_payload(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);
    // Parse pic timing SEI data
    Ipp32s pic_timing(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);
    // Parse recovery point SEI data
    Ipp32s recovery_point(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);

    // Skip unrecognized SEI message payload
    Ipp32s reserved_sei_message(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);
};


// Read N bits from bitstream array
inline
Ipp32u H265BaseBitstream::GetBits(const Ipp32u nbits)
{
    Ipp32u w, n = nbits;

    GetNBits(m_pbs, m_bitOffset, n, w);
    return(w);
}

// Read N bits from bitstream array
template <Ipp32u nbits>
inline Ipp32u H265BaseBitstream::GetPredefinedBits()
{
    Ipp32u w, n = nbits;

    GetNBits(m_pbs, m_bitOffset, n, w);
    return(w);
}

inline bool DecodeExpGolombOne_H265_1u32s (Ipp32u **ppBitStream,
                                                      Ipp32s *pBitOffset,
                                                      Ipp32s *pDst,
                                                      Ipp32s isSigned)
{
    Ipp32u code;
    Ipp32u info     = 0;
    Ipp32s length   = 1;            /* for first bit read above*/
    Ipp32u thisChunksLength = 0;
    Ipp32u sval;

    /* check error(s) */

    /* Fast check for element = 0 */
    GetNBits((*ppBitStream), (*pBitOffset), 1, code)
    if (code)
    {
        *pDst = 0;
        return true;
    }

    GetNBits((*ppBitStream), (*pBitOffset), 8, code);
    length += 8;

    /* find nonzero byte */
    while (code == 0 && 32 > length)
    {
        GetNBits((*ppBitStream), (*pBitOffset), 8, code);
        length += 8;
    }

    /* find leading '1' */
    while ((code & 0x80) == 0 && 32 > thisChunksLength)
    {
        code <<= 1;
        thisChunksLength++;
    }
    length -= 8 - thisChunksLength;

    UngetNBits((*ppBitStream), (*pBitOffset),8 - (thisChunksLength + 1))

    /* skipping very long codes, let's assume what the code is corrupted */
    if (32 <= length || 32 <= thisChunksLength)
    {
        Ipp32u dwords;
        length -= (*pBitOffset + 1);
        dwords = length/32;
        length -= (32*dwords);
        *ppBitStream += (dwords + 1);
        *pBitOffset = 31 - length;
        *pDst = 0;
        return false;
    }

    /* Get info portion of codeword */
    if (length)
    {
        GetNBits((*ppBitStream), (*pBitOffset),length, info)
    }

    sval = ((1 << (length)) + (info) - 1);
    if (isSigned)
    {
        if (sval & 1)
            *pDst = (Ipp32s) ((sval + 1) >> 1);
        else
            *pDst = -((Ipp32s) (sval >> 1));
    }
    else
        *pDst = (Ipp32s) sval;

    return true;
}

// Read variable length coded unsigned element
inline Ipp32u H265BaseBitstream::GetVLCElementU()
{
    Ipp32s sval = 0;

    bool res = DecodeExpGolombOne_H265_1u32s(&m_pbs, &m_bitOffset, &sval, false);

    if (!res)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    return (Ipp32u)sval;
}

// Read variable length coded signed element
inline Ipp32s H265BaseBitstream::GetVLCElementS()
{
    Ipp32s sval = 0;

    bool res = DecodeExpGolombOne_H265_1u32s(&m_pbs, &m_bitOffset, &sval, true);

    if (!res)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    return sval;
}

// Read one bit
inline Ipp8u H265BaseBitstream::Get1Bit()
{
    Ipp32u w;

    GetBits1(m_pbs, m_bitOffset, w);
    return (Ipp8u)w;

} // H265Bitstream::Get1Bit()

// Returns number of decoded bytes since last reset
inline size_t H265BaseBitstream::BytesDecoded() const
{
    return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase) +
            ((31 - m_bitOffset) >> 3);
}

// Returns number of decoded bits since last reset
inline size_t H265BaseBitstream::BitsDecoded() const
{
    return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase) * 8 +
        (31 - m_bitOffset);
}

// Returns number of bytes left in bitstream array
inline size_t H265BaseBitstream::BytesLeft() const
{
    return (Ipp32s)m_maxBsSize - (Ipp32s) BytesDecoded();
}

// Returns number of bits needed for byte alignment
inline unsigned H265BaseBitstream::getNumBitsUntilByteAligned() const
{
    return ((m_bitOffset + 1) % 8);
}

// Align bitstream to byte boundary
inline void H265BaseBitstream::readOutTrailingBits()
{
    Get1Bit();
    //VM_ASSERT(1 == uVal);

    Ipp32u bits = getNumBitsUntilByteAligned();

    if (bits)
    {
        GetBits(bits);
        //VM_ASSERT(0 == uVal);
    }
}

// Align bitstream position to byte boundary
inline void H265BaseBitstream::AlignPointerRight(void)
{
    if ((m_bitOffset & 0x07) != 0x07)
    {
        m_bitOffset = (m_bitOffset | 0x07) - 8;
        if (m_bitOffset == -1)
        {
            m_bitOffset = 31;
            m_pbs++;
        } 
    }
}

} // namespace UMC_HEVC_DECODER


#endif // __UMC_H265_BITSTREAM_HEADERS_H_
#endif // UMC_ENABLE_H265_VIDEO_DECODER
