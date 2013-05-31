/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_dec.h"
#include "umc_h265_segment_decoder_templates.h"

namespace UMC_HEVC_DECODER
{

UMC::Status H265SegmentDecoderMultiThreaded::DecodeMacroBlockCABAC(Ipp32u nCurMBNumber,
                                                              Ipp32u &nMaxMBNumber)
{
    UMC::Status status = m_SD->DecodeSegmentCABAC(nCurMBNumber, nMaxMBNumber, this);
    return status;

} // Status H265SegmentDecoderMultiThreaded::DecodeMacroBlockCABAC(Ipp32u nCurMBNumber,

UMC::Status H265SegmentDecoderMultiThreaded::ReconstructMacroBlockCABAC(Ipp32u nCurMBNumber,
                                                                   Ipp32u nMaxMBNumber)
{
    UMC::Status status = m_SD->ReconstructSegment(nCurMBNumber, nMaxMBNumber, this);
    return status;
} // Status H265SegmentDecoderMultiThreaded::ReconstructMacroBlockCABAC(Ipp32u nCurMBNumber,

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
