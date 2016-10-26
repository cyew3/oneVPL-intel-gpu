//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_BITSTREAM_HEADERS_H_
#define __UMC_H264_BITSTREAM_HEADERS_H_

#include "ippvc.h"
#include "umc_structures.h"
#include "umc_h264_dec_defs_dec.h"

#define h264GetBits(current_data, offset, nbits, data) \
{ \
    Ipp32u x; \
    offset -= (nbits); \
    if (offset >= 0) \
    { \
        x = current_data[0] >> (offset + 1); \
    } \
    else \
    { \
        offset += 32; \
        x = current_data[1] >> (offset); \
        x >>= 1; \
        x += current_data[0] << (31 - offset); \
        current_data++; \
    } \
    (data) = x & bits_data[nbits]; \
}

#define h264UngetNBits(current_data, offset, nbits) \
{ \
    offset += (nbits); \
    if (offset > 31) \
    { \
        offset -= 32; \
        current_data--; \
    } \
}

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

namespace UMC
{
extern const Ipp32u bits_data[];
extern const Ipp32s mp_scan4x4[2][16];
extern const Ipp32s hp_scan8x8[2][64];

extern const Ipp16u SAspectRatio[17][2];

extern const Ipp8u default_intra_scaling_list4x4[16];
extern const Ipp8u default_inter_scaling_list4x4[16];
extern const Ipp8u default_intra_scaling_list8x8[64];
extern const Ipp8u default_inter_scaling_list8x8[64];

class Headers;

class H264BaseBitstream
{
public:

    H264BaseBitstream();
    H264BaseBitstream(Ipp8u * const pb, const Ipp32u maxsize);
    virtual ~H264BaseBitstream();

    // Reset the bitstream with new data pointer
    void Reset(Ipp8u * const pb, const Ipp32u maxsize);
    void Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize);

    inline Ipp32u GetBits(const Ipp32u nbits);

    // Read one VLC Ipp32s or Ipp32u value from bitstream
    inline Ipp32s GetVLCElement(bool bIsSigned);

    // Reads one bit from the buffer.
    inline Ipp8u Get1Bit();

    inline bool IsBSLeft(size_t sizeToRead = 0);
    inline void CheckBSLeft(size_t sizeToRead = 0);

    // Check amount of data
    bool More_RBSP_Data();

    inline size_t BytesDecoded();
    inline size_t BitsDecoded();
    inline size_t BytesLeft();

    // Align bitstream pointer to the right
    inline void AlignPointerRight();

    void GetOrg(Ipp32u **pbs, Ipp32u *size);
    void GetState(Ipp32u **pbs, Ipp32u *bitOffset);
    void SetState(Ipp32u *pbs, Ipp32u bitOffset);

    // Set current decoding position
    void SetDecodedBytes(size_t);

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

class H264HeadersBitstream : public H264BaseBitstream
{
public:

    H264HeadersBitstream();
    H264HeadersBitstream(Ipp8u * const pb, const Ipp32u maxsize);


    // Decode sequence parameter set
    Status GetSequenceParamSet(H264SeqParamSet *sps);
    Status GetSequenceParamSetSvcExt(H264SeqParamSetSVCExtension *pSPSSvcExt);
    Status GetSequenceParamSetSvcVuiExt(H264SeqParamSetSVCExtension *pSPSSvcExt);

    // Decode sequence parameter set extension
    Status GetSequenceParamSetExtension(H264SeqParamSetExtension *sps_ex);
    // Decode sequence param set MVC extension
    Status GetSequenceParamSetMvcExt(H264SeqParamSetMVCExtension *pSPSMvcExt);

    // Decoding picture's parameter set functions
    Status GetPictureParamSetPart1(H264PicParamSet *pps);
    Status GetPictureParamSetPart2(H264PicParamSet *pps);

    // Decode NAL unit prefix
    Status GetNalUnitPrefix(H264NalExtension *pExt, Ipp32u NALRef_idc);

    // Decode NAL unit extension parameters
    Status GetNalUnitExtension(H264NalExtension *pExt);

    // Decoding slice header functions
    Status GetSliceHeaderPart1(H264SliceHeader *pSliceHeader);
    Status GetSliceHeaderPart2(H264SliceHeader *pSliceHeader,
                               const H264PicParamSet *pps,
                               const H264SeqParamSet *sps);
    Status GetSliceHeaderPart3(H264SliceHeader *pSliceHeader,
                               PredWeightTable *pPredWeight_L0,
                               PredWeightTable *pPredWeight_L1,
                               RefPicListReorderInfo *pReorderInfo_L0,
                               RefPicListReorderInfo *pReorderInfo_L1,
                               AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
                               AdaptiveMarkingInfo *pBaseAdaptiveMarkingInfo,
                               const H264PicParamSet *pps,
                               const H264SeqParamSet *sps,
                               const H264SeqParamSetSVCExtension *spsSvcExt);
    Status GetSliceHeaderPart4(H264SliceHeader *hdr,
                                const H264SeqParamSetSVCExtension *spsSvcExt); // from slice header in
                                                                               // scalable extension NAL unit


    Status GetNALUnitType(NAL_Unit_Type &nal_unit_type, Ipp32u &nal_ref_idc);
    // SEI part
    Ipp32s ParseSEI(const Headers & headers, H264SEIPayLoad *spl);
    Ipp32s sei_message(const Headers & headers, Ipp32s current_sps, H264SEIPayLoad *spl);
    Ipp32s sei_payload(const Headers & headers, Ipp32s current_sps,H264SEIPayLoad *spl);
    Ipp32s buffering_period(const Headers & headers, Ipp32s , H264SEIPayLoad *spl);
    Ipp32s pic_timing(const Headers & headers, Ipp32s current_sps, H264SEIPayLoad *spl);
    void user_data_registered_itu_t_t35(H264SEIPayLoad *spl);
    void recovery_point(H264SEIPayLoad *spl);
    Ipp32s dec_ref_pic_marking_repetition(const Headers & headers, Ipp32s current_sps, H264SEIPayLoad *spl);
    void unparsed_sei_message(H264SEIPayLoad *spl);
    void scalability_info(H264SEIPayLoad *spl);

protected:

    Status DecRefBasePicMarking(AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
        Ipp8u &adaptive_ref_pic_marking_mode_flag);

    Status DecRefPicMarking(H264SliceHeader *hdr, AdaptiveMarkingInfo *pAdaptiveMarkingInfo);

    void GetScalingList4x4(H264ScalingList4x4 *scl, Ipp8u *def, Ipp8u *scl_type);
    void GetScalingList8x8(H264ScalingList8x8 *scl, Ipp8u *def, Ipp8u *scl_type);

    Status GetVUIParam(H264SeqParamSet *sps, H264VUI *vui);
    Status GetHRDParam(H264SeqParamSet *sps, H264VUI *vui);

    Status GetPredWeightTable(H264SliceHeader *hdr, const H264SeqParamSet *sps,
        PredWeightTable *pPredWeight_L0, PredWeightTable *pPredWeight_L1);
};

void SetDefaultScalingLists(H264SeqParamSet * sps);

inline
void FillFlatScalingList4x4(H264ScalingList4x4 *scl)
{
    for (Ipp32s i=0;i<16;i++)
        scl->ScalingListCoeffs[i] = 16;
}

inline void FillFlatScalingList8x8(H264ScalingList8x8 *scl)
{
    for (Ipp32s i=0;i<64;i++)
        scl->ScalingListCoeffs[i] = 16;
}

inline void FillScalingList4x4(H264ScalingList4x4 *scl_dst, const Ipp8u *coefs_src)
{
    for (Ipp32s i=0;i<16;i++)
        scl_dst->ScalingListCoeffs[i] = coefs_src[i];
}

inline void FillScalingList8x8(H264ScalingList8x8 *scl_dst, const Ipp8u *coefs_src)
{
    for (Ipp32s i=0;i<64;i++)
        scl_dst->ScalingListCoeffs[i] = coefs_src[i];
}

inline IppStatus DecodeExpGolombOne_H264_1u32s (Ipp32u **ppBitStream,
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
    h264GetBits((*ppBitStream), (*pBitOffset), 1, code)
    if (code)
    {
        *pDst = 0;
        return ippStsNoErr;
    }

    h264GetBits((*ppBitStream), (*pBitOffset), 8, code);
    length += 8;

    /* find nonzero byte */
    while (code == 0 && 32 > length)
    {
        h264GetBits((*ppBitStream), (*pBitOffset), 8, code);
        length += 8;
    }

    /* find leading '1' */
    while ((code & 0x80) == 0 && 32 > thisChunksLength)
    {
        code <<= 1;
        thisChunksLength++;
    }
    length -= 8 - thisChunksLength;

    h264UngetNBits((*ppBitStream), (*pBitOffset),8 - (thisChunksLength + 1))

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
        return ippStsH263VLCCodeErr;
    }

    /* Get info portion of codeword */
    if (length)
    {
        h264GetBits((*ppBitStream), (*pBitOffset),length, info)
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

    return ippStsNoErr;

}

inline bool H264BaseBitstream::IsBSLeft(size_t sizeToRead)
{
    size_t bitsDecoded = BitsDecoded();
    return (bitsDecoded + sizeToRead*8 <= m_maxBsSize*8);
}

inline void H264BaseBitstream::CheckBSLeft(size_t sizeToRead)
{
    if (!IsBSLeft(sizeToRead))
        throw h264_exception(UMC_ERR_INVALID_STREAM);
}

inline Ipp32u H264BaseBitstream::GetBits(const Ipp32u nbits)
{
    Ipp32u w, n = nbits;

    h264GetBits(m_pbs, m_bitOffset, n, w);
    return(w);
}

inline Ipp32s H264BaseBitstream::GetVLCElement(bool bIsSigned)
{
    Ipp32s sval = 0;

    IppStatus ippRes = DecodeExpGolombOne_H264_1u32s(&m_pbs, &m_bitOffset, &sval, bIsSigned);

    if (ippStsNoErr > ippRes)
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    return sval;
}

inline Ipp8u H264BaseBitstream::Get1Bit()
{
    Ipp32u w;

    GetBits1(m_pbs, m_bitOffset, w);
    return (Ipp8u)w;
}

inline size_t H264BaseBitstream::BytesDecoded()
{
    return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase) +
            ((31 - m_bitOffset) >> 3);
}

inline size_t H264BaseBitstream::BitsDecoded()
{
    return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase) * 8 +
        (31 - m_bitOffset);
}

inline size_t H264BaseBitstream::BytesLeft()
{
    return((Ipp32s)m_maxBsSize - (Ipp32s) BytesDecoded());
}

inline void H264BaseBitstream::AlignPointerRight()
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

Status InitializePictureParamSet(H264PicParamSet *pps, const H264SeqParamSet *sps, bool isExtension);

} // namespace UMC

#endif // __UMC_H264_BITSTREAM_HEADERS_H_

#endif // UMC_ENABLE_H264_VIDEO_DECODER
