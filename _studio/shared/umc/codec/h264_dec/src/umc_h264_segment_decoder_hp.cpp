//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_segment_decoder_templates.h"

namespace UMC
{

SegmentDecoderHPBase *CreateSD_ManyBits(int32_t bit_depth_luma,
                                        int32_t bit_depth_chroma,
                                        bool is_field,
                                        int32_t color_format,
                                        bool is_high_profile)
{
    if (bit_depth_chroma > 8 || bit_depth_luma > 8)
    {
        if (is_field)
        {
            return CreateSegmentDecoderWrapper<int32_t, uint16_t, uint16_t, true>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        } else {
            return CreateSegmentDecoderWrapper<int32_t, uint16_t, uint16_t, false>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        }
    }
    else
    {
        // this function should be called from CreateSD
        VM_ASSERT(false);
    }

    return NULL;

} // SegmentDecoderHPBase *CreateSD(int32_t bit_depth_luma,

void InitializeSDCreator_ManyBits()
{
    CreateSegmentDecoderWrapper<int32_t, uint16_t, uint16_t, true>::CreateSoftSegmentDecoder(0, false);
    CreateSegmentDecoderWrapper<int32_t, uint16_t, uint16_t, false>::CreateSoftSegmentDecoder(0, false);
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
