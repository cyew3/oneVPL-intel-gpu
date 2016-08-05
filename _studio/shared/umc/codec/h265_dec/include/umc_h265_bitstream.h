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
#include "umc_h265_bitstream_headers.h"

namespace UMC_HEVC_DECODER
{
// CABAC magic mode switcher.
// Just put it to 0 to switch fast CABAC decoding off.
#define CABAC_MAGIC_BITS 0
#define __CABAC_FILE__ "cabac.mfx.log"

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
