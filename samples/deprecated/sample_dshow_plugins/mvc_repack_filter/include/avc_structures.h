/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include <windows.h>
#include <string.h>
#include "mfxstructures.h"
#include "sample_defs.h"

// NAL unit SVC extension structure
struct H264NalMvcExtension
{
    mfxU8 non_idr_flag;
    mfxU16 priority_id;
    // view_id variable is duplicated to the slice header itself
    mfxU16 view_id;
    mfxU8 temporal_id;
    mfxU8 anchor_pic_flag;
    mfxU8 inter_view_flag;
    mfxU8 padding[3];
};

// NAL unit extension structure
struct H264NalExtension
{
    mfxU8 extension_present;
    H264NalMvcExtension mvc;
};

// Although the standard allows for a minimum width or height of 4, this
// implementation restricts the minimum value to 32.

enum    // Valid QP range
{
    AVC_QP_MAX = 51,
    AVC_QP_MIN = 0
};

enum {
    FLD_STRUCTURE       = 0,
    TOP_FLD_STRUCTURE   = 0,
    BOTTOM_FLD_STRUCTURE = 1,
    FRM_STRUCTURE   = 2,
    AFRM_STRUCTURE  = 3
};

enum DisplayPictureStruct {
    DPS_FRAME     = 0,
    DPS_TOP,         // one field
    DPS_BOTTOM,      // one field
    DPS_TOP_BOTTOM,
    DPS_BOTTOM_TOP,
    DPS_TOP_BOTTOM_TOP,
    DPS_BOTTOM_TOP_BOTTOM,
    DPS_FRAME_DOUBLING,
    DPS_FRAME_TRIPLING
};

typedef enum {
    NAL_UT_UNSPECIFIED  = 0, // Unspecified
    NAL_UT_SLICE     = 1, // Coded Slice - slice_layer_no_partioning_rbsp
    NAL_UT_DPA       = 2, // Coded Data partition A - dpa_layer_rbsp
    NAL_UT_DPB       = 3, // Coded Data partition A - dpa_layer_rbsp
    NAL_UT_DPC       = 4, // Coded Data partition A - dpa_layer_rbsp
    NAL_UT_IDR_SLICE = 5, // Coded Slice of a IDR Picture - slice_layer_no_partioning_rbsp
    NAL_UT_SEI       = 6, // Supplemental Enhancement Information - sei_rbsp
    NAL_UT_SPS       = 7, // Sequence Parameter Set - seq_parameter_set_rbsp
    NAL_UT_PPS       = 8, // Picture Parameter Set - pic_parameter_set_rbsp
    NAL_UT_AUD        = 9, // Access Unit Delimiter - access_unit_delimiter_rbsp
    NAL_END_OF_SEQ   = 10, // End of sequence end_of_seq_rbsp()
    NAL_END_OF_STREAM = 11, // End of stream end_of_stream_rbsp
    NAL_UT_FD        = 12, // Filler Data - filler_data_rbsp
    NAL_UT_SPS_EX    = 13, // Sequence Parameter Set Extension - seq_parameter_set_extension_rbsp
    NAL_UNIT_PREFIX  = 14, // Prefix NAL unit in scalable extension - prefix_nal_unit_rbsp
    NAL_UNIT_SUBSET_SPS = 15, // Subset Sequence Parameter Set - subset_seq_parameter_set_rbsp
    NAL_UT_AUXILIARY = 19, // Auxiliary coded picture
    NAL_UNIT_SLICE_SCALABLE = 20 // Coded slice in scalable extension - slice_layer_in_scalable_extension_rbsp
} NAL_Unit_Type;

// Note!  The Picture Code Type values below are no longer used in the
// core encoder.   It only knows about slice types, and whether or not
// the frame is IDR, Reference or Disposable.  See enum above.

enum EnumSliceCodType        // Permitted MB Prediction Types
{                        // ------------------------------------
    PREDSLICE      = 0,    // I (Intra), P (Pred)
    BPREDSLICE     = 1, // I, P, B (BiPred)
    INTRASLICE     = 2,    // I
    S_PREDSLICE    = 3,    // SP (SPred), I
    S_INTRASLICE   = 4    // SI (SIntra), I
};

typedef enum
{
    SEI_BUFFERING_PERIOD_TYPE   = 0,
    SEI_PIC_TIMING_TYPE         = 1,
    SEI_PAN_SCAN_RECT_TYPE      = 2,
    SEI_FILLER_TYPE             = 3,
    SEI_USER_DATA_REGISTERED_TYPE   = 4,
    SEI_USER_DATA_UNREGISTERED_TYPE = 5,
    SEI_RECOVERY_POINT_TYPE         = 6,
    SEI_DEC_REF_PIC_MARKING_TYPE    = 7,
    SEI_SPARE_PIC_TYPE              = 8,
    SEI_SCENE_INFO_TYPE             = 9,
    SEI_SUB_SEQ_INFO_TYPE           = 10,
    SEI_SUB_SEQ_LAYER_TYPE          = 11,
    SEI_SUB_SEQ_TYPE                = 12,
    SEI_FULL_FRAME_FREEZE_TYPE      = 13,
    SEI_FULL_FRAME_FREEZE_RELEASE_TYPE  = 14,
    SEI_FULL_FRAME_SNAPSHOT_TYPE        = 15,
    SEI_PROGRESSIVE_REF_SEGMENT_START_TYPE  = 16,
    SEI_PROGRESSIVE_REF_SEGMENT_END_TYPE    = 17,
    SEI_MOTION_CONSTRAINED_SG_SET_TYPE      = 18,
    SEI_RESERVED                            = 19
} SEI_TYPE;


#define IS_I_SLICE(SliceType) ((SliceType) == INTRASLICE)
#define IS_P_SLICE(SliceType) ((SliceType) == PREDSLICE || (SliceType) == S_PREDSLICE)
#define IS_B_SLICE(SliceType) ((SliceType) == BPREDSLICE)

enum
{
    MAX_NUM_SEQ_PARAM_SETS = 32,
    MAX_NUM_PIC_PARAM_SETS = 256,

    MAX_SLICE_NUM       = 128, //INCREASE IF NEEDED OR SET to -1 for adaptive counting (increases memory usage)
    MAX_NUM_REF_FRAMES  = 32,


    MAX_NUM_SLICE_GROUPS        = 8,
    MAX_SLICE_GROUP_MAP_TYPE    = 6,

    NUM_INTRA_TYPE_ELEMENTS     = 16,

    COEFFICIENTS_BUFFER_SIZE    = 16 * 51,

    MINIMAL_DATA_SIZE           = 4
};

// Possible values for disable_deblocking_filter_idc:
enum DeblockingModes_t
{
    DEBLOCK_FILTER_ON                   = 0,
    DEBLOCK_FILTER_OFF                  = 1,
    DEBLOCK_FILTER_ON_NO_SLICE_EDGES    = 2
};

#pragma pack(1)

struct AVCScalingList4x4
{
    mfxU8 ScalingListCoeffs[16];
};

struct AVCScalingList8x8
{
    mfxU8 ScalingListCoeffs[64];
};

struct AVCWholeQPLevelScale4x4
{
    mfxI16 LevelScaleCoeffs[88]/*since we do not support 422 and 444*/[16];
};
struct AVCWholeQPLevelScale8x8
{
    mfxI16 LevelScaleCoeffs[88]/*since we do not support 422 and 444*/[64];
};

#pragma pack()

#pragma pack(16)

typedef mfxU32 IntraType;

// Sequence parameter set structure, corresponding to the H.264 bitstream definition.
struct AVCSeqParamSet
{
    AVCSeqParamSet()
    {
        seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS;
        poffset_for_ref_frame = 0;
    }

    AVCSeqParamSet(AVCSeqParamSet const &paramset)
    {
        poffset_for_ref_frame = 0;
        copy(paramset);
    }

    ~AVCSeqParamSet()
    {
        if (poffset_for_ref_frame)
        {
            delete [] poffset_for_ref_frame;
            poffset_for_ref_frame = NULL;
        }
    }

    mfxU32 GetID() const
    {
        return seq_parameter_set_id;
    }

    void copy (const AVCSeqParamSet & paramset)
    {
        profile_idc = paramset.profile_idc;
        level_idc   = paramset.level_idc;
        constrained_set0_flag = paramset.constrained_set0_flag;
        constrained_set1_flag = paramset.constrained_set1_flag;;
        constrained_set2_flag = paramset.constrained_set2_flag;;
        constrained_set3_flag = paramset.constrained_set3_flag;;
        chroma_format_idc = paramset.chroma_format_idc;
        residual_colour_transform_flag = paramset.residual_colour_transform_flag;
        bit_depth_luma = paramset.bit_depth_luma;
        bit_depth_chroma = paramset.bit_depth_chroma;
        qpprime_y_zero_transform_bypass_flag = paramset.qpprime_y_zero_transform_bypass_flag;
        MSDK_MEMCPY_VAR(type_of_scaling_list_used,
            paramset.type_of_scaling_list_used, sizeof(type_of_scaling_list_used));
        seq_scaling_matrix_present_flag = paramset.seq_scaling_matrix_present_flag;
        MSDK_MEMCPY_VAR(ScalingLists4x4, paramset.ScalingLists4x4, sizeof(ScalingLists4x4));
        MSDK_MEMCPY_VAR(ScalingLists8x8, paramset.ScalingLists8x8, sizeof(ScalingLists8x8));
        gaps_in_frame_num_value_allowed_flag = paramset.gaps_in_frame_num_value_allowed_flag;
        frame_cropping_flag = paramset.frame_cropping_flag;
        frame_cropping_rect_left_offset = paramset.frame_cropping_rect_left_offset;
        frame_cropping_rect_right_offset = paramset.frame_cropping_rect_right_offset;
        frame_cropping_rect_top_offset = paramset.frame_cropping_rect_top_offset;
        frame_cropping_rect_bottom_offset = paramset.frame_cropping_rect_bottom_offset;
        more_than_one_slice_group_allowed_flag = paramset.more_than_one_slice_group_allowed_flag;
        arbitrary_slice_order_allowed_flag = paramset.arbitrary_slice_order_allowed_flag;

        redundant_pictures_allowed_flag = paramset.redundant_pictures_allowed_flag;
        seq_parameter_set_id = paramset.seq_parameter_set_id;
        log2_max_frame_num = paramset.log2_max_frame_num;
        pic_order_cnt_type = paramset.pic_order_cnt_type;

        delta_pic_order_always_zero_flag = paramset.delta_pic_order_always_zero_flag;

        frame_mbs_only_flag = paramset.frame_mbs_only_flag;
        required_frame_num_update_behavior_flag = paramset.required_frame_num_update_behavior_flag;

        mb_adaptive_frame_field_flag = paramset.mb_adaptive_frame_field_flag;

        direct_8x8_inference_flag = paramset.direct_8x8_inference_flag;
        vui_parameters_present_flag = paramset.vui_parameters_present_flag;
        log2_max_pic_order_cnt_lsb = paramset.log2_max_pic_order_cnt_lsb;
        offset_for_non_ref_pic = paramset.offset_for_non_ref_pic;

        offset_for_top_to_bottom_field = paramset.offset_for_top_to_bottom_field;

        num_ref_frames_in_pic_order_cnt_cycle = paramset.num_ref_frames_in_pic_order_cnt_cycle;
        num_ref_frames = paramset.num_ref_frames;
        frame_width_in_mbs = paramset.frame_width_in_mbs;
        frame_height_in_mbs = paramset.frame_height_in_mbs;

        MaxMbAddress = paramset.MaxMbAddress;
        MaxPicOrderCntLsb = paramset.MaxPicOrderCntLsb;

        aspect_ratio_info_present_flag = paramset.aspect_ratio_info_present_flag;
        aspect_ratio_idc = paramset.aspect_ratio_idc;
        sar_width = paramset.sar_width;
        sar_height = paramset.sar_height;
        overscan_info_present_flag = paramset.overscan_info_present_flag;
        overscan_appropriate_flag = paramset.overscan_appropriate_flag;
        video_signal_type_present_flag = paramset.video_signal_type_present_flag;
        video_format = paramset.video_format;
        video_full_range_flag = paramset.video_full_range_flag;
        colour_description_present_flag = paramset.colour_description_present_flag;
        colour_primaries = paramset.colour_primaries;
        transfer_characteristics = paramset.transfer_characteristics;
        matrix_coefficients = paramset.matrix_coefficients;
        chroma_loc_info_present_flag = paramset.chroma_loc_info_present_flag;
        chroma_sample_loc_type_top_field = paramset.chroma_sample_loc_type_top_field;
        chroma_sample_loc_type_bottom_field = paramset.chroma_sample_loc_type_bottom_field;
        timing_info_present_flag = paramset.timing_info_present_flag;
        num_units_in_tick = paramset.num_units_in_tick;
        time_scale = paramset.time_scale;
        fixed_frame_rate_flag = paramset.fixed_frame_rate_flag;
        nal_hrd_parameters_present_flag = paramset.nal_hrd_parameters_present_flag;
        vcl_hrd_parameters_present_flag = paramset.vcl_hrd_parameters_present_flag;
        low_delay_hrd_flag = paramset.low_delay_hrd_flag;
        pic_struct_present_flag = paramset.pic_struct_present_flag;
        bitstream_restriction_flag = paramset.bitstream_restriction_flag;
        motion_vectors_over_pic_boundaries_flag = paramset.motion_vectors_over_pic_boundaries_flag;
        max_bytes_per_pic_denom = paramset.max_bytes_per_pic_denom;
        max_bits_per_mb_denom = paramset.max_bits_per_mb_denom;
        log2_max_mv_length_horizontal = paramset.log2_max_mv_length_horizontal;
        log2_max_mv_length_vertical = paramset.log2_max_mv_length_vertical;
        num_reorder_frames = paramset.num_reorder_frames;
        max_dec_frame_buffering = paramset.max_dec_frame_buffering;

        cpb_cnt = paramset.cpb_cnt;
        bit_rate_scale = paramset.bit_rate_scale;
        cpb_size_scale = paramset.cpb_size_scale;
        MSDK_MEMCPY_VAR(bit_rate_value, paramset.bit_rate_value, sizeof(bit_rate_value));
        MSDK_MEMCPY_VAR(cpb_size_value, paramset.cpb_size_value,sizeof(cpb_size_value));
        MSDK_MEMCPY_VAR(cbr_flag, paramset.cbr_flag, sizeof(cbr_flag));
        initial_cpb_removal_delay_length = paramset.initial_cpb_removal_delay_length;
        cpb_removal_delay_length = paramset.cpb_removal_delay_length;
        dpb_output_delay_length = paramset.dpb_output_delay_length;
        time_offset_length = paramset.time_offset_length;

        mfxI32 len = paramset.num_ref_frames_in_pic_order_cnt_cycle;
        if(len < 1)
            len =1;

        if (paramset.poffset_for_ref_frame)
        {
            poffset_for_ref_frame = new mfxI32[len];
            MSDK_MEMCPY(poffset_for_ref_frame, paramset.poffset_for_ref_frame,
                len*sizeof(mfxI32));
        }
    }

    AVCSeqParamSet& operator = (const AVCSeqParamSet & sps)
    {
        if (poffset_for_ref_frame)
        {
            delete [] poffset_for_ref_frame;
            poffset_for_ref_frame = NULL;
        }
        copy(sps);
        return *this;
    }


    bool operator == (const AVCSeqParamSet & sps) const
    {
        if (memcmp(&(this->profile_idc), &sps.profile_idc, ((mfxI8*)&poffset_for_ref_frame - (mfxI8*)&profile_idc)))
            return false;

        mfxI32 len = sps.num_ref_frames_in_pic_order_cnt_cycle;
        if(len < 1)
            len =1;

        if (poffset_for_ref_frame && sps.poffset_for_ref_frame)
        {
            if (memcmp(poffset_for_ref_frame, sps.poffset_for_ref_frame,
                len*sizeof(mfxU32)))
                return false;
        }

        return true;
    }

    bool operator != (const AVCSeqParamSet & sps) const
    {
        return !(*this == sps);
    }



    mfxU8        profile_idc;                        // baseline, main, etc.
    mfxU8        level_idc;
    mfxU8        constrained_set0_flag;
    mfxU8        constrained_set1_flag;
    mfxU8        constrained_set2_flag;
    mfxU8        constrained_set3_flag;
    mfxU8        chroma_format_idc;
    mfxU8        residual_colour_transform_flag;
    mfxU8        bit_depth_luma;
    mfxU8        bit_depth_chroma;
    mfxU8        qpprime_y_zero_transform_bypass_flag;
    mfxU8        type_of_scaling_list_used[8];
    mfxU8        seq_scaling_matrix_present_flag;
    //mfxU8        seq_scaling_list_present_flag[8];
    AVCScalingList4x4 ScalingLists4x4[6];
    AVCScalingList8x8 ScalingLists8x8[2];
    mfxU8        gaps_in_frame_num_value_allowed_flag;
    mfxU8        frame_cropping_flag;
    mfxU32       frame_cropping_rect_left_offset;
    mfxU32       frame_cropping_rect_right_offset;
    mfxU32       frame_cropping_rect_top_offset;
    mfxU32       frame_cropping_rect_bottom_offset;
    mfxU8        more_than_one_slice_group_allowed_flag;
    mfxU8        arbitrary_slice_order_allowed_flag;  // If zero, slice order in pictures must
    // be in increasing MB address order.
    mfxU8        redundant_pictures_allowed_flag;
    mfxU8        seq_parameter_set_id;                // id of this sequence parameter set
    mfxU8        log2_max_frame_num;                  // Number of bits to hold the frame_num
    mfxU8        pic_order_cnt_type;                  // Picture order counting method

    mfxU8        delta_pic_order_always_zero_flag;    // If zero, delta_pic_order_cnt fields are
    // present in slice header.
    mfxU8        frame_mbs_only_flag;                 // Nonzero indicates all pictures in sequence
    // are coded as frames (not fields).
    mfxU8        required_frame_num_update_behavior_flag;

    mfxU8        mb_adaptive_frame_field_flag;        // Nonzero indicates frame/field switch
    // at macroblock level
    mfxU8        direct_8x8_inference_flag;           // Direct motion vector derivation method
    mfxU8        vui_parameters_present_flag;         // Zero indicates default VUI parameters
    mfxU32       log2_max_pic_order_cnt_lsb;          // Value of MaxPicOrderCntLsb.
    mfxI32       offset_for_non_ref_pic;

    mfxI32       offset_for_top_to_bottom_field;      // Expected pic order count difference from
    // top field to bottom field.

    mfxU32       num_ref_frames_in_pic_order_cnt_cycle;
    mfxU32       num_ref_frames;                      // total number of pics in decoded pic buffer
    mfxU32       frame_width_in_mbs;
    mfxU32       frame_height_in_mbs;

    // These fields are calculated from values above.  They are not written to the bitstream
    mfxU32       MaxMbAddress;
    mfxU32       MaxPicOrderCntLsb;
    // vui part
    mfxU8        aspect_ratio_info_present_flag;
    mfxU8        aspect_ratio_idc;
    mfxU16       sar_width;
    mfxU16       sar_height;
    mfxU8        overscan_info_present_flag;
    mfxU8        overscan_appropriate_flag;
    mfxU8        video_signal_type_present_flag;
    mfxU8        video_format;
    mfxU8        video_full_range_flag;
    mfxU8        colour_description_present_flag;
    mfxU8        colour_primaries;
    mfxU8        transfer_characteristics;
    mfxU8        matrix_coefficients;
    mfxU8        chroma_loc_info_present_flag;
    mfxU8        chroma_sample_loc_type_top_field;
    mfxU8        chroma_sample_loc_type_bottom_field;
    mfxU8        timing_info_present_flag;
    mfxU32       num_units_in_tick;
    mfxU32       time_scale;
    mfxU8        fixed_frame_rate_flag;
    mfxU8        nal_hrd_parameters_present_flag;
    mfxU8        vcl_hrd_parameters_present_flag;
    mfxU8        low_delay_hrd_flag;
    mfxU8        pic_struct_present_flag;
    mfxU8        bitstream_restriction_flag;
    mfxU8        motion_vectors_over_pic_boundaries_flag;
    mfxU8        max_bytes_per_pic_denom;
    mfxU8        max_bits_per_mb_denom;
    mfxU8        log2_max_mv_length_horizontal;
    mfxU8        log2_max_mv_length_vertical;
    mfxU8        num_reorder_frames;
    mfxU8        max_dec_frame_buffering;
    //hrd_parameters
    mfxU8        cpb_cnt;
    mfxU8        bit_rate_scale;
    mfxU8        cpb_size_scale;
    mfxU32       bit_rate_value[32];
    mfxU32       cpb_size_value[32];
    mfxU8        cbr_flag[32];
    mfxU8        initial_cpb_removal_delay_length;
    mfxU8        cpb_removal_delay_length;
    mfxU8        dpb_output_delay_length;
    mfxU8        time_offset_length;

    mfxI32       *poffset_for_ref_frame;              // pointer to array of stored frame offsets,
    // length num_stored_frames_in_pic_order_cnt_cycle,
    // for pic order cnt type 1

};    // AVCSeqParamSet

// Sequence parameter set extension structure, corresponding to the H.264 bitstream definition.
struct AVCSeqParamSetExtension
{
    mfxU8       seq_parameter_set_id;
    mfxU8       aux_format_idc;
    mfxU8       bit_depth_aux;
    mfxU8       alpha_incr_flag;
    mfxU8       alpha_opaque_value;
    mfxU8       alpha_transparent_value;
    mfxU8       additional_extension_flag;

    bool operator == (const AVCSeqParamSetExtension& sps) const
    {
        if (memcmp(&(this->seq_parameter_set_id), &sps.seq_parameter_set_id, sizeof(mfxU8)) ||
            memcmp(&(this->aux_format_idc), &sps.aux_format_idc, sizeof(mfxU8)) ||
            memcmp(&(this->bit_depth_aux), &sps.bit_depth_aux, sizeof(mfxU8)) ||
            memcmp(&(this->alpha_incr_flag), &sps.alpha_incr_flag, sizeof(mfxU8)) ||
            memcmp(&(this->alpha_opaque_value), &sps.alpha_opaque_value, sizeof(mfxU8)) ||
            memcmp(&(this->alpha_transparent_value), &sps.alpha_transparent_value, sizeof(mfxU8)) ||
            memcmp(&(this->additional_extension_flag), &sps.additional_extension_flag, sizeof(mfxU8)))
            return false;

        return true;
    }

    bool operator != (const AVCSeqParamSetExtension & sps) const
    {
        return !(*this == sps);
    }

    AVCSeqParamSetExtension()
    {
        aux_format_idc = 0;
        seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS;    // illegal id
    }

    mfxU32 GetID() const
    {
        return seq_parameter_set_id;
    }
};    // AVCSeqParamSetExtension

// Picture parameter set structure, corresponding to the H.264 bitstream definition.
struct AVCPicParamSet
{
    // Flexible macroblock order structure, defining the FMO map for a picture
    // paramter set.

    struct SliceGroupInfoStruct
    {
        mfxU8        slice_group_map_type;                // 0..6

        // The additional slice group data depends upon map type
        union
        {
            // type 0
            mfxU32    run_length[MAX_NUM_SLICE_GROUPS];

            // type 2
            struct
            {
                mfxU32 top_left[MAX_NUM_SLICE_GROUPS-1];
                mfxU32 bottom_right[MAX_NUM_SLICE_GROUPS-1];
            }t1;

            // types 3-5
            struct
            {
                mfxU8  slice_group_change_direction_flag;
                mfxU32 slice_group_change_rate;
            }t2;

            // type 6
            struct
            {
                mfxU32 pic_size_in_map_units;     // number of macroblocks if no field coding
                mfxU8 *pSliceGroupIDMap;          // Id for each slice group map unit
            }t3;
        };
    };    // SliceGroupInfoStruct

    mfxU16       pic_parameter_set_id;            // of this picture parameter set
    mfxU8        seq_parameter_set_id;            // of seq param set used for this pic param set
    mfxU8        entropy_coding_mode;             // zero: CAVLC, else CABAC

    mfxU8        pic_order_present_flag;          // Zero indicates only delta_pic_order_cnt[0] is
    // present in slice header; nonzero indicates
    // delta_pic_order_cnt[1] is also present.

    mfxU8        weighted_pred_flag;              // Nonzero indicates weighted prediction applied to
    // P and SP slices
    mfxU8        weighted_bipred_idc;             // 0: no weighted prediction in B slices
    // 1: explicit weighted prediction
    // 2: implicit weighted prediction
    mfxI8        pic_init_qp;                     // default QP for I,P,B slices
    mfxI8        pic_init_qs;                     // default QP for SP, SI slices

    mfxI8        chroma_qp_index_offset[2];       // offset to add to QP for chroma

    mfxU8        deblocking_filter_variables_present_flag;    // If nonzero, deblock filter params are
    // present in the slice header.
    mfxU8        constrained_intra_pred_flag;     // Nonzero indicates constrained intra mode

    mfxU8        redundant_pic_cnt_present_flag;  // Nonzero indicates presence of redundant_pic_cnt
    // in slice header
    mfxU32       num_slice_groups;                // One: no FMO
    mfxU32       num_ref_idx_l0_active;           // num of ref pics in list 0 used to decode the picture
    mfxU32       num_ref_idx_l1_active;           // num of ref pics in list 1 used to decode the picture
    mfxU8        transform_8x8_mode_flag;
    mfxU8        type_of_scaling_list_used[8];

    AVCScalingList4x4 ScalingLists4x4[6];
    AVCScalingList8x8 ScalingLists8x8[2];

    // Level Scale addition
    AVCWholeQPLevelScale4x4        m_LevelScale4x4[6];
    AVCWholeQPLevelScale8x8        m_LevelScale8x8[2];

    SliceGroupInfoStruct SliceGroupInfo;    // Used only when num_slice_groups > 1

    AVCPicParamSet()
    {
        Reset();
    }

    void Reset()
    {
        pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS;
        seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS;
        num_slice_groups = 0;
    }

    void copy(const AVCPicParamSet &pps)
    {
        pic_parameter_set_id = pps.pic_parameter_set_id;
        seq_parameter_set_id = pps.seq_parameter_set_id;
        entropy_coding_mode = pps.entropy_coding_mode;

        pic_order_present_flag = pps.pic_order_present_flag;
        weighted_pred_flag = pps.weighted_pred_flag;
        weighted_bipred_idc =pps.weighted_bipred_idc;

        pic_init_qp = pps.pic_init_qp;
        pic_init_qs = pps.pic_init_qs;

        MSDK_MEMCPY(chroma_qp_index_offset, pps.chroma_qp_index_offset, sizeof(pps.chroma_qp_index_offset));

        deblocking_filter_variables_present_flag = pps.deblocking_filter_variables_present_flag;

        constrained_intra_pred_flag = pps.constrained_intra_pred_flag;

        redundant_pic_cnt_present_flag = pps.redundant_pic_cnt_present_flag;
        num_slice_groups = pps.redundant_pic_cnt_present_flag;
        num_ref_idx_l0_active = pps.num_ref_idx_l0_active;
        num_ref_idx_l1_active = pps.num_ref_idx_l1_active;
        transform_8x8_mode_flag = pps.transform_8x8_mode_flag;
        MSDK_MEMCPY(type_of_scaling_list_used, pps.type_of_scaling_list_used, sizeof(pps.type_of_scaling_list_used));

        MSDK_MEMCPY(ScalingLists4x4, pps.ScalingLists4x4, sizeof(ScalingLists4x4));
        MSDK_MEMCPY(ScalingLists8x8, pps.ScalingLists8x8, sizeof(pps.ScalingLists8x8));

        MSDK_MEMCPY(m_LevelScale4x4, pps.m_LevelScale4x4, sizeof(pps.m_LevelScale4x4));
        MSDK_MEMCPY(m_LevelScale8x8, pps.m_LevelScale8x8, sizeof(pps.m_LevelScale8x8));

        MSDK_MEMCPY_VAR(SliceGroupInfo, &pps.SliceGroupInfo, sizeof(SliceGroupInfo));
        if (1 < num_slice_groups &&
            (6 == SliceGroupInfo.slice_group_map_type) &&
            SliceGroupInfo.t3.pSliceGroupIDMap)
        {
            SliceGroupInfo.t3.pSliceGroupIDMap = new mfxU8[SliceGroupInfo.t3.pic_size_in_map_units];
            MSDK_MEMCPY(SliceGroupInfo.t3.pSliceGroupIDMap, pps.SliceGroupInfo.t3.pSliceGroupIDMap,
                SliceGroupInfo.t3.pic_size_in_map_units * sizeof(mfxU8));
        }
    }

    AVCPicParamSet(const AVCPicParamSet &pps)
    {
        copy(pps);
    }

    AVCPicParamSet& operator =(const AVCPicParamSet &pps)
    {
        if (1 < num_slice_groups &&
            (6 == SliceGroupInfo.slice_group_map_type) &&
            SliceGroupInfo.t3.pSliceGroupIDMap)
        {
            delete [] SliceGroupInfo.t3.pSliceGroupIDMap;
        }
        copy(pps);
        return *this;
    }

    ~AVCPicParamSet()
    {
        if (1 < num_slice_groups &&
            (6 == SliceGroupInfo.slice_group_map_type) &&
            SliceGroupInfo.t3.pSliceGroupIDMap)
        {
            delete [] SliceGroupInfo.t3.pSliceGroupIDMap;
        }
    }

    mfxU32 GetID() const
    {
        return pic_parameter_set_id;
    }

    bool operator == (const AVCPicParamSet & pps) const
    {
        if (memcmp(&pic_parameter_set_id, &pps.pic_parameter_set_id,
            ((mfxI8*)&SliceGroupInfo - (mfxI8*)&pic_parameter_set_id)))
            return false;

        if (memcmp(&SliceGroupInfo, &pps.SliceGroupInfo, sizeof(SliceGroupInfoStruct)))
            return false;

        return true;
    }

    bool operator != (const AVCPicParamSet & pps) const
    {
        return !(*this == pps);
    }

};    // AVCPicParamSet

struct RefPicListReorderInfo
{
    mfxU32       num_entries;                 // number of currently valid idc,value pairs
    mfxU8        reordering_of_pic_nums_idc[MAX_NUM_REF_FRAMES];
    mfxU32       reorder_value[MAX_NUM_REF_FRAMES];    // abs_diff_pic_num or long_term_pic_num
};

struct AdaptiveMarkingInfo
{
    mfxU32       num_entries;                 // number of currently valid mmco,value pairs
    mfxU8        mmco[MAX_NUM_REF_FRAMES];    // memory management control operation id
    mfxU32       value[MAX_NUM_REF_FRAMES*2]; // operation-dependent data, max 2 per operation
};

struct PredWeightTable
{
    mfxU8        luma_weight_flag;            // nonzero: luma weight and offset in bitstream
    mfxU8        chroma_weight_flag;          // nonzero: chroma weight and offset in bitstream
    mfxI8        luma_weight;                 // luma weighting factor
    mfxI8        luma_offset;                 // luma weighting offset
    mfxI8        chroma_weight[2];            // chroma weighting factor (Cb,Cr)
    mfxI8        chroma_offset[2];            // chroma weighting offset (Cb,Cr)
};    // PredWeightTable

typedef mfxI32 AVCDecoderMBAddr;

// Slice header structure, corresponding to the H.264 bitstream definition.
struct AVCSliceHeader
{
    // flag equal 1 means that the slice belong to IDR or anchor access unit
    mfxU32 IdrPicFlag;

    // NAL unit extension parameters
    H264NalExtension nal_ext;
    mfxU32 view_id;

    NAL_Unit_Type nal_unit_type;                        // (NAL_Unit_Type) specifies the type of RBSP data structure contained in the NAL unit as specified in Table 7-1 of AVC standart
    mfxU16        pic_parameter_set_id;                 // of pic param set used for this slice
    mfxU8         field_pic_flag;                       // zero: frame picture, else field picture
    mfxU8         MbaffFrameFlag;
    mfxU8         bottom_field_flag;                    // zero: top field, else bottom field
    mfxU8         direct_spatial_mv_pred_flag;          // zero: temporal direct, else spatial direct
    mfxU8         num_ref_idx_active_override_flag;     // nonzero: use ref_idx_active from slice header
    // instead of those from pic param set
    mfxU8         no_output_of_prior_pics_flag;         // nonzero: remove previously decoded pictures
    // from decoded picture buffer
    mfxU8         long_term_reference_flag;             // How to set MaxLongTermFrameIdx
    mfxU32        cabac_init_idc;                      // CABAC initialization table index (0..2)
    mfxU8         adaptive_ref_pic_marking_mode_flag;   // Ref pic marking mode of current picture
    mfxI32        slice_qp_delta;                       // to calculate default slice QP
    mfxU8         sp_for_switch_flag;                   // SP slice decoding control
    mfxI32        slice_qs_delta;                       // to calculate default SP,SI slice QS
    mfxU32        disable_deblocking_filter_idc;       // deblock filter control, 0=filter all edges
    mfxI32        slice_alpha_c0_offset;               // deblock filter c0, alpha table offset
    mfxI32        slice_beta_offset;                   // deblock filter beta table offset
    AVCDecoderMBAddr first_mb_in_slice;
    mfxI32        frame_num;
    EnumSliceCodType slice_type;
    mfxU8         idr_flag;
    mfxU8         nal_ref_idc;
    mfxU32        idr_pic_id;                           // ID of an IDR picture
    mfxI32        pic_order_cnt_lsb;                    // picture order count (mod MaxPicOrderCntLsb)
    mfxI32        delta_pic_order_cnt_bottom;           // Pic order count difference, top & bottom fields
    mfxU32        difference_of_pic_nums;               // Ref pic memory mgmt
    mfxU32        long_term_pic_num;                    // Ref pic memory mgmt
    mfxU32        long_term_frame_idx;                  // Ref pic memory mgmt
    mfxU32        max_long_term_frame_idx;              // Ref pic memory mgmt
    mfxI32        delta_pic_order_cnt[2];               // picture order count differences
    mfxU32        redundant_pic_cnt;                    // for redundant slices
    mfxI32        num_ref_idx_l0_active;                // num of ref pics in list 0 used to decode the slice,
    // see num_ref_idx_active_override_flag
    mfxI32        num_ref_idx_l1_active;                // num of ref pics in list 1 used to decode the slice
    // see num_ref_idx_active_override_flag
    mfxU32        slice_group_change_cycle;             // for FMO
    mfxU8         luma_log2_weight_denom;               // luma weighting denominator
    mfxU8         chroma_log2_weight_denom;             // chroma weighting denominator

    bool          is_auxiliary;
}; // AVCSliceHeader

struct AVCLimitedSliceHeader
{
    EnumSliceCodType slice_type;                        // (EnumSliceCodType) slice type
    mfxU8 disable_deblocking_filter_idc;                // (mfxU8) deblock filter control, 0 = filter all edges
    mfxI8 slice_alpha_c0_offset;                        // (mfxI8) deblock filter c0, alpha table offset
    mfxI8 slice_beta_offset;                            // (mfxI8) deblock filter beta table offset

}; // AVCLimitedSliceHeader

struct AVCSEIPayLoad
{
    SEI_TYPE payLoadType;
    mfxU32   payLoadSize;

    mfxU8 * user_data; // for UserDataRegistered or UserDataUnRegistered

    AVCSEIPayLoad()
        : user_data(0)
        , payLoadType(SEI_RESERVED)
        , payLoadSize(0)
    {
    }

    AVCSEIPayLoad(const AVCSEIPayLoad &payload)
    {
        user_data = 0;
        Reset();
        payLoadType = payload.payLoadType;
        payLoadSize = payload.payLoadSize;
    }

    AVCSEIPayLoad& operator=(const AVCSEIPayLoad &payload)
    {
        Reset();
        payLoadType = payload.payLoadType;
        payLoadSize = payload.payLoadSize;
        return *this;
    }

    void Reset()
    {
        if(user_data)
            delete [] user_data;
        user_data = 0;
        payLoadType = SEI_RESERVED;
        payLoadSize = 0;
    }

    ~AVCSEIPayLoad()
    {
        if(user_data)
        {
            delete [] user_data;
            user_data = NULL;
        }
    }

    mfxU32 GetID() const
    {
        return payLoadType;
    }

    bool operator == (const AVCSEIPayLoad & sei) const
    {
        if (memcmp(&payLoadType, &sei.payLoadType, sizeof(SEI_TYPE)) ||
            memcmp(&payLoadSize, &sei.payLoadSize, sizeof(mfxU32)) ||
            user_data != sei.user_data)
            return false;

        return true;
    }

    bool operator != (const AVCSEIPayLoad & sei) const
    {
        return !(*this == sei);
    }

    union SEIMessages
    {
        struct BufferingPeriod
        {
            mfxU32 initial_cbp_removal_delay[2][16];
            mfxU32 initial_cbp_removal_delay_offset[2][16];
        }buffering_period;

        struct PicTiming
        {
            mfxU32 cbp_removal_delay;
            mfxU32 dpb_ouput_delay;
            DisplayPictureStruct pic_struct;
            mfxU8  clock_timestamp_flag[16];
            struct ClockTimestamps
            {
                mfxU8 ct_type;
                mfxU8 nunit_field_based_flag;
                mfxU8 counting_type;
                mfxU8 full_timestamp_flag;
                mfxU8 discontinuity_flag;
                mfxU8 cnt_dropped_flag;
                mfxU8 n_frames;
                mfxU8 seconds_value;
                mfxU8 minutes_value;
                mfxU8 hours_value;
                mfxU8 time_offset;
            }clock_timestamps[16];
        }pic_timing;

        struct PanScanRect
        {
            mfxU8  pan_scan_rect_id;
            mfxU8  pan_scan_rect_cancel_flag;
            mfxU8  pan_scan_cnt;
            mfxU32 pan_scan_rect_left_offset[32];
            mfxU32 pan_scan_rect_right_offset[32];
            mfxU32 pan_scan_rect_top_offset[32];
            mfxU32 pan_scan_rect_bottom_offset[32];
            mfxU8  pan_scan_rect_repetition_period;
        }pan_scan_rect;

        struct UserDataRegistered
        {
            mfxU8 itu_t_t35_country_code;
            mfxU8 itu_t_t35_country_code_extension_byte;
        } user_data_registered;

        struct RecoveryPoint
        {
            mfxU8 recovery_frame_cnt;
            mfxU8 exact_match_flag;
            mfxU8 broken_link_flag;
            mfxU8 changing_slice_group_idc;
        }recovery_point;

        struct DecRefPicMarkingRepetition
        {
            mfxU8 original_idr_flag;
            mfxU8 original_frame_num;
            mfxU8 original_field_pic_flag;
            mfxU8 original_bottom_field_flag;
        }dec_ref_pic_marking_repetition;

        struct SparePic
        {
            mfxU32 target_frame_num;
            mfxU8  spare_field_flag;
            mfxU8  target_bottom_field_flag;
            mfxU8  num_spare_pics;
            mfxU8  delta_spare_frame_num[16];
            mfxU8  spare_bottom_field_flag[16];
            mfxU8  spare_area_idc[16];
            mfxU8  *spare_unit_flag[16];
            mfxU8  *zero_run_length[16];
        }spare_pic;

        struct SceneInfo
        {
            mfxU8 scene_info_present_flag;
            mfxU8 scene_id;
            mfxU8 scene_transition_type;
            mfxU8 second_scene_id;
        }scene_info;

        struct SubSeqInfo
        {
            mfxU8 sub_seq_layer_num;
            mfxU8 sub_seq_id;
            mfxU8 first_ref_pic_flag;
            mfxU8 leading_non_ref_pic_flag;
            mfxU8 last_pic_flag;
            mfxU8 sub_seq_frame_num_flag;
            mfxU8 sub_seq_frame_num;
        }sub_seq_info;

        struct SubSeqLayerCharacteristics
        {
            mfxU8  num_sub_seq_layers;
            mfxU8  accurate_statistics_flag[16];
            mfxU16 average_bit_rate[16];
            mfxU16 average_frame_rate[16];
        }sub_seq_layer_characteristics;

        struct SubSeqCharacteristics
        {
            mfxU8  sub_seq_layer_num;
            mfxU8  sub_seq_id;
            mfxU8  duration_flag;
            mfxU8  sub_seq_duration;
            mfxU8  average_rate_flag;
            mfxU8  accurate_statistics_flag;
            mfxU16 average_bit_rate;
            mfxU16 average_frame_rate;
            mfxU8  num_referenced_subseqs;
            mfxU8  ref_sub_seq_layer_num[16];
            mfxU8  ref_sub_seq_id[16];
            mfxU8  ref_sub_seq_direction[16];
        }sub_seq_characteristics;

        struct FullFrameFreeze
        {
            mfxU32 full_frame_freeze_repetition_period;
        }full_frame_freeze;

        struct FullFrameSnapshot
        {
            mfxU8 snapshot_id;
        }full_frame_snapshot;

        struct ProgressiveRefinementSegmentStart
        {
            mfxU8 progressive_refinement_id;
            mfxU8 num_refinement_steps;
        }progressive_refinement_segment_start;

        struct MotionConstrainedSliceGroupSet
        {
            mfxU8 num_slice_groups_in_set;
            mfxU8 slice_group_id[8];
            mfxU8 exact_sample_value_match_flag;
            mfxU8 pan_scan_rect_flag;
            mfxU8 pan_scan_rect_id;
        }motion_constrained_slice_group_set;

        struct FilmGrainCharacteristics
        {
            mfxU8 film_grain_characteristics_cancel_flag;
            mfxU8 model_id;
            mfxU8 separate_colour_description_present_flag;
            mfxU8 film_grain_bit_depth_luma;
            mfxU8 film_grain_bit_depth_chroma;
            mfxU8 film_grain_full_range_flag;
            mfxU8 film_grain_colour_primaries;
            mfxU8 film_grain_transfer_characteristics;
            mfxU8 film_grain_matrix_coefficients;
            mfxU8 blending_mode_id;
            mfxU8 log2_scale_factor;
            mfxU8 comp_model_present_flag[3];
            mfxU8 num_intensity_intervals[3];
            mfxU8 num_model_values[3];
            mfxU8 intensity_interval_lower_bound[3][256];
            mfxU8 intensity_interval_upper_bound[3][256];
            mfxU8 comp_model_value[3][3][256];
            mfxU8 film_grain_characteristics_repetition_period;
        }film_grain_characteristics;

        struct DeblockingFilterDisplayPreference
        {
            mfxU8 deblocking_display_preference_cancel_flag;
            mfxU8 display_prior_to_deblocking_preferred_flag;
            mfxU8 dec_frame_buffering_constraint_flag;
            mfxU8 deblocking_display_preference_repetition_period;
        }deblocking_filter_display_preference;

        struct StereoVideoInfo
        {
            mfxU8 field_views_flag;
            mfxU8 top_field_is_left_view_flag;
            mfxU8 current_frame_is_left_view_flag;
            mfxU8 next_frame_is_second_view_flag;
            mfxU8 left_view_self_contained_flag;
            mfxU8 right_view_self_contained_flag;
        }stereo_video_info;

    }SEI_messages;
};

#pragma pack()

// This file defines some data structures and constants used by the decoder,
// that are also needed by other classes, such as post filters and
// error concealment.

#define INTERP_FACTOR 4
#define INTERP_SHIFT 2

#define CHROMA_INTERP_FACTOR 8
#define CHROMA_INTERP_SHIFT 3

// at picture edge, clip motion vectors to only this far beyond the edge,
// in pixel units.
#define D_MV_CLIP_LIMIT 19

enum Direction_t{
    D_DIR_FWD = 0,
    D_DIR_BWD = 1,
    D_DIR_BIDIR = 2,
    D_DIR_DIRECT = 3,
    D_DIR_DIRECT_SPATIAL_FWD = 4,
    D_DIR_DIRECT_SPATIAL_BWD = 5,
    D_DIR_DIRECT_SPATIAL_BIDIR = 6
};

inline bool IsForwardOnly(mfxI32 direction)
{
    return (direction == D_DIR_FWD) || (direction == D_DIR_DIRECT_SPATIAL_FWD);
}

inline bool IsHaveForward(mfxI32 direction)
{
    return (direction == D_DIR_FWD) || (direction == D_DIR_BIDIR) ||
        (direction == D_DIR_DIRECT_SPATIAL_FWD) || (direction == D_DIR_DIRECT_SPATIAL_BIDIR) ||
        (direction == D_DIR_DIRECT);
}

inline bool IsBackwardOnly(mfxI32 direction)
{
    return (direction == D_DIR_BWD) || (direction == D_DIR_DIRECT_SPATIAL_BWD);
}

inline bool IsHaveBackward(mfxI32 direction)
{
    return (direction == D_DIR_BWD) || (direction == D_DIR_BIDIR) ||
        (direction == D_DIR_DIRECT_SPATIAL_BWD) || (direction == D_DIR_DIRECT_SPATIAL_BIDIR) ||
        (direction == D_DIR_DIRECT);
}

inline bool IsBidirOnly(mfxI32 direction)
{
    return (direction == D_DIR_BIDIR) || (direction == D_DIR_DIRECT_SPATIAL_BIDIR) ||
        (direction == D_DIR_DIRECT);
}

// Warning: If these bit defines change, also need to change same
// defines  and related code in sresidual.s.
enum CBP
{
    D_CBP_LUMA_DC = 0x00001,
    D_CBP_LUMA_AC = 0x1fffe,

    D_CBP_CHROMA_DC = 0x00001,
    D_CBP_CHROMA_AC = 0x1fffe,
    D_CBP_CHROMA_AC_420 = 0x0001e,
    D_CBP_CHROMA_AC_422 = 0x001fe,
    D_CBP_CHROMA_AC_444 = 0x1fffe,

    D_CBP_1ST_LUMA_AC_BITPOS = 1,
    D_CBP_1ST_CHROMA_DC_BITPOS = 17,
    D_CBP_1ST_CHROMA_AC_BITPOS = 19
};

enum
{
    FIRST_DC_LUMA = 0,
    FIRST_AC_LUMA = 1,
    FIRST_DC_CHROMA = 17,
    FIRST_AC_CHROMA = 19
};

enum
{
    CHROMA_FORMAT_400       = 0,
    CHROMA_FORMAT_420       = 1,
    CHROMA_FORMAT_422       = 2,
    CHROMA_FORMAT_444       = 3
};

class AVC_exception
{
public:
    AVC_exception(mfxI32 status = -1)
        : m_Status(status)
    {
    }

    virtual ~AVC_exception()
    {
    }

    mfxI32 GetStatus() const
    {
        return m_Status;
    }

private:
    mfxI32 m_Status;
};


#pragma pack(1)

extern mfxI32 lock_failed;

#pragma pack()

template <typename T>
inline T * AVC_new_array_throw(mfxI32 size)
{
    T * t = new T[size];
    if (!t)
        throw AVC_exception(MFX_ERR_MEMORY_ALLOC);
    return t;
}

template <typename T>
inline T * AVC_new_throw()
{
    T * t = new T();
    if (!t)
        throw AVC_exception(MFX_ERR_MEMORY_ALLOC);
    return t;
}

template <typename T, typename T1>
inline T * AVC_new_throw_1(T1 t1)
{
    T * t = new T(t1);
    if (!t)
        throw AVC_exception(UMC_ERR_ALLOC);
    return t;
}

inline mfxU32 CalculateSuggestedSize(const AVCSeqParamSet * sps)
{
    mfxU32 base_size = sps->frame_width_in_mbs * sps->frame_height_in_mbs * 256;
    mfxU32 size = 0;

    switch (sps->chroma_format_idc)
    {
    case 0:  // YUV400
        size = base_size;
        break;
    case 1:  // YUV420
        size = (base_size * 3) / 2;
        break;
    case 2: // YUV422
        size = base_size + base_size;
        break;
    case 3: // YUV444
        size = base_size + base_size + base_size;
        break;
    };

    return size;
}

#define SAMPLE_ASSERT(x)