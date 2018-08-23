//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

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
    static uint32_t m_c;
    static FILE* cabac_bits;

    inline void PRINT_CABAC_VALUES(uint32_t val, uint32_t range, uint32_t finalRange)
    {
        fprintf(cabac_bits, "%d: coding bin value %d, range = [%d->%d]\n", m_c++, val, range, finalRange);
        fflush(cabac_bits);
    }

#endif
    H265Bitstream(void);
    H265Bitstream(uint8_t * const pb, const uint32_t maxsize);
    virtual ~H265Bitstream(void);

    //
    // CABAC decoding function(s)
    //

    // Initialize CABAC decoding engine. HEVC spec 9.3.2.2
    void InitializeDecodingEngine_CABAC(void);
    // Terminate CABAC decoding engine, rollback prereaded bits
    void TerminateDecode_CABAC(void);

    // Initialize all CABAC contexts. HEVC spec 9.3.2.2
    void InitializeContextVariablesHEVC_CABAC(int32_t initializationType,
                                              int32_t SliceQPy);


    // Decode single bin from stream
    inline
    uint32_t DecodeSingleBin_CABAC(uint32_t ctxIdx);

#if defined( __INTEL_COMPILER ) && (defined( __x86_64__ ) || defined ( _WIN64 ))
    uint32_t DecodeSingleBin_CABAC_cmov(uint32_t ctxIdx);
    uint32_t DecodeSingleBin_CABAC_cmov_BMI(uint32_t ctxIdx);
#endif

    // Decode single bin using bypass decoding
    inline
    uint32_t DecodeSingleBinEP_CABAC();

    // Decode N bits encoded with CABAC bypass
    inline
    uint32_t DecodeBypassBins_CABAC(int32_t numBins);

    // Decode terminating flag for slice end or row end in WPP case
    inline
    uint32_t DecodeTerminatingBit_CABAC(void);

    // Reset CABAC state
    void ResetBac_CABAC();

    uint8_t  context_hevc[NUM_CTX];
    uint8_t  wpp_saved_cabac_context[NUM_CTX];

protected:

#if (CABAC_MAGIC_BITS > 0)
    unsigned long long m_lcodIRange;                                        // (uint32_t) arithmetic decoding engine variable
    unsigned long long m_lcodIOffset;                                       // (uint32_t) arithmetic decoding engine variable
    int32_t m_iExtendedBits;                                        // (int32_t) available extra CABAC bits
    uint16_t *m_pExtendedBits;                                       // (uint16_t *) pointer to CABAC data
#else
    uint32_t m_lcodIRange;                                        // (uint32_t) arithmetic decoding engine variable
    uint32_t m_lcodIOffset;                                       // (uint32_t) arithmetic decoding engine variable
    int32_t m_bitsNeeded;
#endif
    uint32_t m_LastByte;

    // Decoding SEI message functions
    int32_t sei_message(const HeaderSet<H265SeqParamSet> & sps,int32_t current_sps,H265SEIPayLoad *spl);
    // Parse SEI payload data
    int32_t sei_payload(const HeaderSet<H265SeqParamSet> & sps, int32_t current_sps, H265SEIPayLoad *spl);
    // Parse pic timing SEI data
    int32_t pic_timing(const HeaderSet<H265SeqParamSet> & sps, int32_t current_sps, H265SEIPayLoad *spl);
    // Parse recovery point SEI data
    int32_t recovery_point(const HeaderSet<H265SeqParamSet> & sps, int32_t current_sps, H265SEIPayLoad *spl);

    // Skip unrecognized SEI message payload
    int32_t reserved_sei_message(const HeaderSet<H265SeqParamSet> & sps, int32_t current_sps, H265SEIPayLoad *spl);
};

} // namespace UMC_HEVC_DECODER

#include "umc_h265_bitstream_inlines.h"

#endif // __UMC_H265_BITSTREAM_H_
#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
