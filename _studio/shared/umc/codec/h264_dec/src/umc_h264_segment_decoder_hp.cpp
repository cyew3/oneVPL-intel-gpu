// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

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
#endif // MFX_ENABLE_H264_VIDEO_DECODE
