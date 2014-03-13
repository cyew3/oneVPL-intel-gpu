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
#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_dec_tables.h"
#include "umc_h265_dec_internal_cabac.h"

namespace UMC_HEVC_DECODER
{
// CABAC magic mode switcher.
// Just put it to 0 to switch fast CABAC decoding off.
#define CABAC_MAGIC_BITS 0
#define __CABAC_FILE__ "cabac.mfx.log"

template <typename T> class HeaderSet;
class Headers;


class H265BaseBitstream
{
public:

    H265BaseBitstream();
    H265BaseBitstream(Ipp8u * const pb, const Ipp32u maxsize);
    virtual ~H265BaseBitstream();

    // Reset the bitstream with new data pointer
    void Reset(Ipp8u * const pb, const Ipp32u maxsize);
    void Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize);

    inline void AlignPointerRight(void);

    inline Ipp32u GetBits(Ipp32u nbits);
#if defined( __INTEL_COMPILER )
    inline Ipp32u GetBits_BMI(Ipp32u nbits);
#endif

    // Read one VLC Ipp32s or Ipp32u value from bitstream
    Ipp32u GetVLCElementU();

    Ipp32s GetVLCElementS();

    // Reads one bit from the buffer.
    Ipp8u Get1Bit();

    // Move pointer to nearest byte
    void ReadToByteAlignment();

    void CheckBSLeft();

    // Check amount of data
    bool More_RBSP_Data();

    size_t BytesDecoded() const;

    size_t BitsDecoded() const;

    size_t BytesLeft() const;

    //h265:
    unsigned getNumBitsUntilByteAligned() const;
    unsigned getNumBitsLeft() const;
    void readOutTrailingBits();
    // Align bitstream pointer to the right
    //virtual void H265BaseBitstream::AlignPointerRight();

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

// this is a compatibility wrapper for Slice object.
// it emulates iface used in reference library
class ReferenceCodecAdaptor
{
    Headers *m_headers;

public:
    ReferenceCodecAdaptor(Headers &headers) : m_headers(&headers) { }

    // in reference code pointer to manager is used, wir wollen zu emulate diese Verhalten
          ReferenceCodecAdaptor *operator ->()       { return this; }
    const ReferenceCodecAdaptor *operator ->() const { return this; }

    // interface
    H265PicParamSet *getPrefetchedPPS(unsigned);
    H265SeqParamSet *getPrefetchedSPS(unsigned);
};



class H265HeadersBitstream : public H265BaseBitstream
{
public:

    H265HeadersBitstream();
    H265HeadersBitstream(Ipp8u * const pb, const Ipp32u maxsize);

    void decodeSlice(H265Slice *, const H265SeqParamSet *, const H265PicParamSet *);
    UMC::Status GetSliceHeaderPart1(H265SliceHeader * sliceHdr);
    UMC::Status GetSliceHeaderFull(H265Slice *, const H265SeqParamSet *, const H265PicParamSet *);

    void parseScalingList(H265ScalingList *);
    bool MoreRbspData();

    UMC::Status GetVideoParamSet(H265VideoParamSet *vps);

    // Decode sequence parameter set
    UMC::Status GetSequenceParamSet(H265SeqParamSet *sps);

    //H265

    UMC::Status GetSliceHeaderFull(H265SliceHeader *pSliceHeader,
                               H265PicParamSet *pps,
                               const H265SeqParamSet *sps);
    UMC::Status GetPictureParamSetFull(H265PicParamSet  *pps);
    UMC::Status GetWPPTileInfo(H265SliceHeader *hdr,
                            const H265PicParamSet *pps,
                            const H265SeqParamSet *sps);
    virtual void parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* pRPS, Ipp32u idx) = 0;

protected:

    void parseVUI(H265SeqParamSet *sps);

    void xParsePredWeightTable(H265SliceHeader * sliceHdr);
    void xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId);
    void parseHrdParameters(H265HRD *hrd, Ipp8u commonInfPresentFlag, Ipp32u vps_max_sub_layers);

    void  parsePTL            ( H265ProfileTierLevel *rpcPTL, int maxNumSubLayersMinus1);
    void  parseProfileTier    (H265PTL *ptl);
};

class H265Bitstream  : public H265HeadersBitstream
{
public:
#if INSTRUMENTED_CABAC
    static Ipp32u m_c;
    static FILE* cabac_bits;

    inline void PRINT_CABAC_VALUES(Ipp32u val, Ipp32u range)
    {
        fprintf(cabac_bits, "\nCount: %d \tValue: %d \tRange: %d \t", m_c++, val, range);
        fflush(cabac_bits);
    }

#endif
    H265Bitstream(void);
    H265Bitstream(Ipp8u * const pb, const Ipp32u maxsize);
    virtual ~H265Bitstream(void);

    // Get type of current NAL
    UMC::Status GetNALUnitType(NalUnitType &nal_unit_type, Ipp32u &nuh_temporal_id);
    UMC::Status GetAccessUnitDelimiter(Ipp32u &PicCodType);

    // Parse SEI message
    Ipp32s ParseSEI(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);

    void GetOrg(Ipp32u **pbs, Ipp32u *size);
    void GetState(Ipp32u **pbs, Ipp32u *bitOffset);
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

    // Initialize CABAC decoding engine
    void InitializeDecodingEngine_CABAC(void);
    // Terminate CABAC decoding engine, rollback prereaded bits
    void TerminateDecode_CABAC(void);

    // Initialize CABAC contexts
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
    //inline
    //Ipp32u DecodeBypass_CABAC();

    inline
    Ipp32u DecodeSingleBinEP_CABAC();

    inline
    Ipp32u DecodeBypassBins_CABAC(Ipp32s numBins);


    inline //from h265
    Ipp32u DecodeTerminatingBit_CABAC(void);

    //from h265 reset
    void ResetBac_CABAC();

    //h265:
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
    //hevc CABAC from HM50
    static const
    Ipp8u RenormTable[32];

    // Decoding SEI message functions
    Ipp32s sei_message(const HeaderSet<H265SeqParamSet> & sps,Ipp32s current_sps,H265SEIPayLoad *spl);
    Ipp32s sei_payload(const HeaderSet<H265SeqParamSet> & sps,Ipp32s current_sps,H265SEIPayLoad *spl);
    Ipp32s pic_timing(const HeaderSet<H265SeqParamSet> & sps,Ipp32s current_sps,H265SEIPayLoad *spl);
    Ipp32s recovery_point(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps,H265SEIPayLoad *spl);

    Ipp32s reserved_sei_message(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl);
};

} // namespace UMC_HEVC_DECODER

#include "umc_h265_bitstream_inlines.h"

#endif // __UMC_H264_BITSTREAM_H_

#endif // UMC_ENABLE_H264_VIDEO_DECODER
