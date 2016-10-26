//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "vm_debug.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_slice_decoding.h"
#include "umc_h265_headers.h"

namespace UMC_HEVC_DECODER
{

#if INSTRUMENTED_CABAC
    Ipp32u H265Bitstream::m_c = 0;
    FILE* H265Bitstream::cabac_bits = 0;
#endif

H265Bitstream::H265Bitstream(Ipp8u * const pb, const Ipp32u maxsize)
     : H265HeadersBitstream(pb, maxsize)
{
    m_bitsNeeded = 0;
    m_lcodIOffset = 0;
    m_lcodIRange = 0;
    m_LastByte = 0;
} // H265Bitstream::H265Bitstream(Ipp8u * const pb,

H265Bitstream::H265Bitstream()
    : H265HeadersBitstream()
    , m_lcodIRange(0)
    , m_lcodIOffset(0)
    , m_bitsNeeded(0)
    , m_LastByte(0)
{
#if INSTRUMENTED_CABAC
    if (!cabac_bits)
    {
        cabac_bits=fopen(__CABAC_FILE__,"w+t");
        m_c = 0;
    }
#endif
} // H265Bitstream::H265Bitstream(void)

H265Bitstream::~H265Bitstream()
{
#if INSTRUMENTED_CABAC
    fflush(cabac_bits);
#endif
} // H265Bitstream::~H265Bitstream()

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
