/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_segment_decoder_templates.h"

namespace UMC_HEVC_DECODER
{

SegmentDecoderHPBase_H265 *CreateSD_ManyBits_H265(Ipp32s bit_depth_luma,
                                        Ipp32s bit_depth_chroma,
                                        bool is_field,
                                        Ipp32s color_format,
                                        bool is_high_profile)
{
    if (bit_depth_chroma > 8 || bit_depth_luma > 8)
    {
        if (is_field)
        {
            return CreateSegmentDecoderWrapper<Ipp32s, Ipp16s, Ipp16s, true>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        } else {
            return CreateSegmentDecoderWrapper<Ipp32s, Ipp16s, Ipp16s, false>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        }
    }
    else
    {
        // this function should be called from CreateSD
        VM_ASSERT(false);
    }

    return NULL;

} // SegmentDecoderHPBase_H265 *CreateSD(Ipp32s bit_depth_luma,

void InitializeSDCreator_ManyBits_H265()
{
    CreateSegmentDecoderWrapper<Ipp32s, Ipp16u, Ipp16u, true>::CreateSoftSegmentDecoder(0, false);
    CreateSegmentDecoderWrapper<Ipp32s, Ipp16u, Ipp16u, false>::CreateSoftSegmentDecoder(0, false);
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
