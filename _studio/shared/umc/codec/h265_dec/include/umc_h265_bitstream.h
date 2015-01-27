/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_BITSTREAM_H_
#define __UMC_H265_BITSTREAM_H_

#include "ippvc.h"
#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_h265_dec_defs.h"
#include "umc_h265_tables.h"

namespace UMC_HEVC_DECODER
{
// CABAC magic mode switcher.
// Just put it to 0 to switch fast CABAC decoding off.
#define CABAC_MAGIC_BITS 0
#define __CABAC_FILE__ "cabac.mfx.log"

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

    UMC::Status GetSliceHeaderFull(H265SliceHeader *pSliceHeader,
                               H265PicParamSet *pps,
                               const H265SeqParamSet *sps);
    // Parse PPS header
    UMC::Status GetPictureParamSetFull(H265PicParamSet  *pps);
    UMC::Status GetWPPTileInfo(H265SliceHeader *hdr,
                            const H265PicParamSet *pps,
                            const H265SeqParamSet *sps);
    virtual void parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* pRPS, Ipp32u idx) = 0;

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
};

// General HEVC bitstream parsing class
class H265Bitstream  : public H265HeadersBitstream
{
public:
#if INSTRUMENTED_CABAC
    static Ipp32u m_c;
    static FILE* cabac_bits;

    inline void PRINT_CABAC_VALUES(Ipp32u val, Ipp32u range, Ipp32u finalRange)
    {
        fprintf(cabac_bits, "%d: coding bin value %d, range = [%d->%d]\n", m_c++, val, range, finalRange);
        fflush(cabac_bits);
    }

#endif
    H265Bitstream(void);
    H265Bitstream(Ipp8u * const pb, const Ipp32u maxsize);
    virtual ~H265Bitstream(void);

    // Read and return NAL unit type and NAL storage idc.
    // Bitstream position is expected to be at the start of a NAL unit.
    UMC::Status GetNALUnitType(NalUnitType &nal_unit_type, Ipp32u &nuh_temporal_id);
    // Read optional access unit delimiter from bitstream.
    UMC::Status GetAccessUnitDelimiter(Ipp32u &PicCodType);

    // Parse SEI message
    Ipp32s ParseSEI(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);

    // Return bitstream array base address and size
    void GetOrg(Ipp32u **pbs, Ipp32u *size);
    // Return current bitstream address and bit offset
    void GetState(Ipp32u **pbs, Ipp32u *bitOffset);
    // Set current bitstream address and bit offset
    void SetState(Ipp32u *pbs, Ipp32u bitOffset);

    // Set current decoding position
    void SetDecodedBytes(size_t);

    inline size_t GetAllBitsCount()
    {
        return m_maxBsSize;
    }

    inline
    size_t BytesDecodedRoundOff()
    {
        return static_cast<size_t>((Ipp8u*)m_pbs - (Ipp8u*)m_pbsBase);
    }

    //
    // CABAC decoding function(s)
    //

    // Initialize CABAC decoding engine. HEVC spec 9.3.2.2
    void InitializeDecodingEngine_CABAC(void);
    // Terminate CABAC decoding engine, rollback prereaded bits
    void TerminateDecode_CABAC(void);

    // Initialize all CABAC contexts. HEVC spec 9.3.2.2
    void InitializeContextVariablesHEVC_CABAC(Ipp32s initializationType,
                                              Ipp32s SliceQPy);


    // Decode single bin from stream
    inline
    Ipp32u DecodeSingleBin_CABAC(Ipp32u ctxIdx);

#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))
    Ipp32u DecodeSingleBin_CABAC_cmov(Ipp32u ctxIdx);
    Ipp32u DecodeSingleBin_CABAC_cmov_BMI(Ipp32u ctxIdx);
#endif

    // Decode single bin using bypass decoding
    inline
    Ipp32u DecodeSingleBinEP_CABAC();

    // Decode N bits encoded with CABAC bypass
    inline
    Ipp32u DecodeBypassBins_CABAC(Ipp32s numBins);

    // Decode terminating flag for slice end or row end in WPP case
    inline
    Ipp32u DecodeTerminatingBit_CABAC(void);

    // Reset CABAC state
    void ResetBac_CABAC();

    // Parse RPS part in SPS or slice header
    void parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* pRPS, Ipp32u idx);

    Ipp8u  context_hevc[NUM_CTX];
    Ipp8u  wpp_saved_cabac_context[NUM_CTX];

protected:

#if (CABAC_MAGIC_BITS > 0)
    Ipp64u m_lcodIRange;                                        // (Ipp32u) arithmetic decoding engine variable
    Ipp64u m_lcodIOffset;                                       // (Ipp32u) arithmetic decoding engine variable
    Ipp32s m_iExtendedBits;                                        // (Ipp32s) available extra CABAC bits
    Ipp16u *m_pExtendedBits;                                       // (Ipp16u *) pointer to CABAC data
#else
    Ipp32u m_lcodIRange;                                        // (Ipp32u) arithmetic decoding engine variable
    Ipp32u m_lcodIOffset;                                       // (Ipp32u) arithmetic decoding engine variable
    Ipp32s m_bitsNeeded;
#endif
    Ipp32u m_LastByte;

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

} // namespace UMC_HEVC_DECODER

#include "umc_h265_bitstream_inlines.h"

#endif // __UMC_H265_BITSTREAM_H_

#endif // UMC_ENABLE_H265_VIDEO_DECODER
