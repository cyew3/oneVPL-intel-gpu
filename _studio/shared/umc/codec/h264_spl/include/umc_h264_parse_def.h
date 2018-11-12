// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_H264_PARSE_DEF_H__
#define __UMC_H264_PARSE_DEF_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "ippdefs.h"
#include "umc_structures.h"
#include "umc_media_data.h"

#include <vector>

namespace UMC
{

    // Simple buffer pointer
    typedef std::vector<Ipp8u> VEC_BUF;

    /**
    * NAL unit types from H.264 2003/05 Table 7-1
    */
    typedef enum H264NALUTypes
    {
        H264_NAL_SLICE_NON_IDR = 1,
        H264_NAL_SLICE_A       = 2,
        H264_NAL_SLICE_B       = 3,
        H264_NAL_SLICE_C       = 4,
        H264_NAL_SLICE_IDR     = 5,
        H264_NAL_SLICE_SEI     = 6,
        H264_NAL_SET_SEQ       = 7,
        H264_NAL_SET_PIC       = 8,
        H264_NAL_AU_DEL        = 9,
        H264_NAL_END_SEQ       = 10,
        H264_NAL_END_STM       = 11,
        H264_NAL_FIL_DATA      = 12,
        H264_NAL_SET_SEQ_EXT   = 13,
        H264_NAL_PREFIX        = 14,
        H264_NAL_SUBSET_SPS    = 15,
        H264_NAL_SLICE_NI_05   = 19,
        H264_NAL_SLICE_EX      = 20
    } H264NALTypes;

    /**
     * Start octet of the NALU
     */
    struct H264NALUOctet
    {
        Ipp32u Type;
        Ipp32u NRI;

        H264NALUOctet()
        {
            Type = 0;
            NRI = 0;
        }

        void ReadOctet(Ipp8u * byte)
        {
            Type = (*byte) & 0x1f;
            NRI  = (*byte & 0x60) >> 5;
        }
    };

    /**
    * Sequence Set
    */
    struct H264SequenceSetParse
    {
        H264NALUOctet octet;

        Ipp32u seq_parameter_set_id;
        Ipp32u pic_order_cnt_type;
        Ipp32u frame_mbs_only_flag;
        Ipp32u delta_pic_order_always_zero_flag;
        Ipp32u log2_max_frame_num_minus4;
        Ipp32u log2_max_pic_order_cnt_lsb_minus4;
        Ipp32u pic_width_in_mbs_minus1;
        Ipp32u pic_height_in_map_units_minus1;
        Ipp32u frame_cropping_flag;
        Ipp32u frame_crop_left_offset;
        Ipp32u frame_crop_right_offset;
        Ipp32u frame_crop_top_offset;
        Ipp32u frame_crop_bottom_offset;

        VEC_BUF buffer;

        H264SequenceSetParse()
            : seq_parameter_set_id(0)
            , pic_order_cnt_type(0)
            , frame_mbs_only_flag(0)
            , delta_pic_order_always_zero_flag(0)
            , log2_max_frame_num_minus4(0)
            , log2_max_pic_order_cnt_lsb_minus4(0)
            , pic_width_in_mbs_minus1(0)
            , pic_height_in_map_units_minus1(0)
            , frame_cropping_flag(0)
            , frame_crop_left_offset(0)
            , frame_crop_right_offset(0)
            , frame_crop_top_offset(0)
            , frame_crop_bottom_offset(0)
        {
        }
    };

    /**
    * Picture Set
    */
    struct H264PictureSetParse
    {
        H264NALUOctet octet;

        Ipp32u pic_parameter_set_id;
        Ipp32u seq_parameter_set_id;
        Ipp32u pic_order_present_flag;

        VEC_BUF buffer;

        H264PictureSetParse()
            : pic_parameter_set_id(0)
            , seq_parameter_set_id(0)
            , pic_order_present_flag(0)
        {
        }
    };

    /**
     * Slice header
     */
    struct H264SliceHeaderParse
    {
        bool is_valid;
        H264NALUOctet octet;

        Ipp32u first_mb_in_slice;
        Ipp32u slice_type;
        Ipp32u pic_parameter_set_id;
        Ipp32u frame_num;
        Ipp32u field_pic_flag;
        Ipp32u bottom_field_flag;
        Ipp32u idr_pic_id;
        Ipp32u pic_order_cnt_lsb;
        Ipp32s delta_pic_order_cnt_bottom;
        Ipp32s delta_pic_order_cnt[2];
        Ipp32u redundant_pic_cnt;

        H264SliceHeaderParse()
            : is_valid(false)
            , first_mb_in_slice(0)
            , slice_type(0)
            , pic_parameter_set_id(0)
            , frame_num(0)
            , field_pic_flag(0)
            , bottom_field_flag(0)
            , idr_pic_id(0)
            , pic_order_cnt_lsb(0)
            , delta_pic_order_cnt_bottom(0)
            , redundant_pic_cnt(0)
        {
            delta_pic_order_cnt[0] = 0;
            delta_pic_order_cnt[1] = 0;
        }
    };
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
#endif // __UMC_H264_PARSE_DEF_H__
