//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_set.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_encode.h"

using namespace H265Enc;

#define H265Bs_PutBit(bs,code) (bs)->PutBit(code)
#define H265Bs_PutBits(bs,code,len) (bs)->PutBits(code,len)
#define H265Bs_PutVLCCode(bs,code) (bs)->PutVLCCode(code)
#define PUTBITS_32(code) { H265Bs_PutBits(bs, ((Ipp32u)(code)>>16), 16); H265Bs_PutBits(bs, ((code)&0xffff), 16);}

namespace {
    Ipp32u GetMaxOffsetLen(const H265Slice &sh)
    {
        Ipp32u maxLen = sh.entry_point_offset[0];
        for (Ipp32u i = 1; i < sh.num_entry_point_offsets; i++)
            maxLen = MAX(maxLen, sh.entry_point_offset[i]);
        return H265_CeilLog2(maxLen);
    }

    void PutProfileLevel(H265BsReal *bs, Ipp8u profile_present_flag, Ipp32s max_sub_layers, const H265ProfileLevelSet &profileLevel)
    {
        if (profile_present_flag) {
            H265Bs_PutBits(bs, profileLevel.general_profile_space, 2);
            H265Bs_PutBits(bs, profileLevel.general_tier_flag, 1);
            H265Bs_PutBits(bs, profileLevel.general_profile_idc, 5);

            for (Ipp32s i = 0; i < 32; i++)
                H265Bs_PutBit(bs, profileLevel.general_profile_compatibility_flag[i]);

            H265Bs_PutBits(bs, profileLevel.general_progressive_source_flag, 1);
            H265Bs_PutBits(bs, profileLevel.general_interlaced_source_flag, 1);
            H265Bs_PutBits(bs, profileLevel.general_non_packed_constraint_flag, 1);
            H265Bs_PutBits(bs, profileLevel.general_frame_only_constraint_flag, 1);
            if (profileLevel.general_profile_idc == 4 || profileLevel.general_profile_compatibility_flag[4]) {
	            H265Bs_PutBits(bs, profileLevel.general_max_12bit_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_max_10bit_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_max_8bit_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_max_422chroma_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_max_420chroma_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_max_monochrome_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_intra_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_one_picture_only_constraint_flag, 1);
	            H265Bs_PutBits(bs, profileLevel.general_lower_bit_rate_constraint_flag, 1);
                H265Bs_PutBits(bs, 0, 13); // reserved
                H265Bs_PutBits(bs, 0, 22); // reserved
            } else {
                H265Bs_PutBits(bs, 0, 22); // reserved
                H265Bs_PutBits(bs, 0, 22); // reserved
            }
            H265Bs_PutBits(bs, profileLevel.general_level_idc, 8);

            if (max_sub_layers > 1) {
                VM_ASSERT(0);
            }
        }
    }

    void PutAUD(H265BsReal *bs, Ipp32s sliceType)
    {
        Ipp32s picType = (sliceType == I_SLICE ? 0 : sliceType == P_SLICE ? 1 : 2);
        H265Bs_PutBits(bs, picType, 3);
    }

    void PutShortTermRefPicSet(H265BsReal *bs, const H265SeqParameterSet &sps, const H265ShortTermRefPicSet &rps, Ipp32s idx)
    {
        const H265ShortTermRefPicSet *spsRps = sps.m_shortRefPicSet;

        if (idx > 0) {
            H265Bs_PutBit(bs, rps.inter_ref_pic_set_prediction_flag);
        } else if (rps.inter_ref_pic_set_prediction_flag != 0) {
            VM_ASSERT(0);
        }

        if (rps.inter_ref_pic_set_prediction_flag) {
            Ipp32s RIdx = idx - rps.delta_idx;
            Ipp32s NumDeltaPocs = spsRps[RIdx].num_negative_pics + spsRps[RIdx].num_negative_pics;

            H265Bs_PutVLCCode(bs, rps.delta_idx - 1);
            H265Bs_PutBit(bs, rps.delta_rps_sign);
            H265Bs_PutVLCCode(bs, rps.abs_delta_rps - 1);
            for (Ipp32s i = 0; i <= NumDeltaPocs; i++) {
                H265Bs_PutBit(bs, rps.used_by_curr_pic_flag[i]);
                if (!rps.used_by_curr_pic_flag[i])
                    H265Bs_PutBit(bs, rps.use_delta_flag[i]);
            }
        }
        else {
            H265Bs_PutVLCCode(bs, rps.num_negative_pics);
            H265Bs_PutVLCCode(bs, rps.num_positive_pics);
            for (Ipp32s i = 0; i < rps.num_negative_pics + rps.num_positive_pics; i++) {
                H265Bs_PutVLCCode(bs, rps.delta_poc[i] - 1);
                H265Bs_PutBit(bs, rps.used_by_curr_pic_flag[i]);
            }
        }
    }

    void PutSliceHeader(H265BsReal *bs, const H265Slice &sh, const H265SeqParameterSet &sps, const H265PicParameterSet &pps)
    {
        H265Bs_PutBit(bs, sh.first_slice_segment_in_pic_flag);
        if (sh.NalUnitType >= NAL_BLA_W_LP && sh.NalUnitType <= NAL_RSV_IRAP_VCL23) {
            H265Bs_PutBit(bs, sh.no_output_of_prior_pics_flag);
        }

        H265Bs_PutVLCCode(bs, sh.slice_pic_parameter_set_id);

        if (!sh.first_slice_segment_in_pic_flag) {
            if (pps.dependent_slice_segments_enabled_flag) {
                H265Bs_PutBit(bs, sh.dependent_slice_segment_flag);
            }
            Ipp32s slice_address_len = H265_CeilLog2(sps.PicSizeInCtbsY);
            H265Bs_PutBits(bs, sh.slice_segment_address, slice_address_len);
        }

        if (!sh.dependent_slice_segment_flag) {
            if (pps.num_extra_slice_header_bits)
                H265Bs_PutBits(bs, 0, pps.num_extra_slice_header_bits);
            H265Bs_PutVLCCode(bs, sh.slice_type);
            if (pps.output_flag_present_flag )
                H265Bs_PutBit(bs, sh.pic_output_flag);
            //if (sps.separate_colour_plane_flag == 1)
            //    H265Bs_PutBits(bs, sh.colour_plane_id, 2);
            if (sh.NalUnitType != NAL_IDR_W_RADL && sh.NalUnitType != NAL_IDR_N_LP) {
                H265Bs_PutBits(bs, sh.slice_pic_order_cnt_lsb, sps.log2_max_pic_order_cnt_lsb);
                H265Bs_PutBit(bs, sh.short_term_ref_pic_set_sps_flag);
                if (!sh.short_term_ref_pic_set_sps_flag) {
                    PutShortTermRefPicSet(bs, sps, sh.m_shortRefPicSet, sps.num_short_term_ref_pic_sets);
                } else if (sps.num_short_term_ref_pic_sets > 1) {
                    Ipp32s len = H265_CeilLog2(sps.num_short_term_ref_pic_sets);
                    H265Bs_PutBits(bs, sh.short_term_ref_pic_set_idx, len);
                }
                if (sps.long_term_ref_pics_present_flag) {
                    VM_ASSERT(0);
                    //H265Bs_PutVLCCode(bs, sh.num_long_term_pics);
                    //for (i = 0; i < sh.num_long_term_pics; i++) {
                    //    H265Bs_PutBits(bs, sh.delta_poc_lsb_lt[i], sps.log2_max_pic_order_cnt_lsb);
                    //    H265Bs_PutBit(bs, sh.delta_poc_msb_present_flag[i]);
                    //    if (sh.delta_poc_msb_present_flag[i])
                    //        H265Bs_PutVLCCode(bs, sh.delta_poc_msb_cycle_lt[i]);
                    //    H265Bs_PutBit(bs, sh.used_by_curr_pic_lt_flag[i]);
                    //}
                }
                if (sps.sps_temporal_mvp_enabled_flag)
                    H265Bs_PutBit(bs, sh.slice_temporal_mvp_enabled_flag);
            }
            if (sps.sample_adaptive_offset_enabled_flag) {
                H265Bs_PutBit(bs, sh.slice_sao_luma_flag);
                H265Bs_PutBit(bs, sh.slice_sao_chroma_flag);
            }
            if (sh.slice_type == P_SLICE || sh.slice_type == B_SLICE) {
                H265Bs_PutBit(bs, sh.num_ref_idx_active_override_flag);
                if( sh.num_ref_idx_active_override_flag ) {
                    H265Bs_PutVLCCode(bs, sh.num_ref_idx[0] - 1);
                    if (sh.slice_type == B_SLICE)
                        H265Bs_PutVLCCode(bs, sh.num_ref_idx[1] - 1);
                }
                if (pps.lists_modification_present_flag && sh.CeilLog2NumPocTotalCurr > 0) {
                    for (Ipp32s list = 0; list <= (sh.slice_type == B_SLICE); list++) {
                        H265Bs_PutBit(bs, sh.ref_pic_list_modification_flag[list]);
                        if (sh.ref_pic_list_modification_flag[list])
                            for (Ipp32s i = 0; i < sh.num_ref_idx[list]; i++)
                                H265Bs_PutBits(bs, sh.list_entry[list][i], sh.CeilLog2NumPocTotalCurr);
                    }
                }
                if (sh.slice_type == B_SLICE)
                    H265Bs_PutBit(bs, sh.mvd_l1_zero_flag);
                if (pps.cabac_init_present_flag)
                    H265Bs_PutBit(bs, sh.cabac_init_flag);
                if (sh.slice_temporal_mvp_enabled_flag) {
                    if (sh.slice_type == B_SLICE)
                        H265Bs_PutBit(bs, sh.collocated_from_l0_flag);
                    Ipp32s collocated_from_l0_flag = sh.collocated_from_l0_flag;
                    if (sh.slice_type != B_SLICE)
                        collocated_from_l0_flag = 1; // inferred
                    if (((collocated_from_l0_flag && sh.num_ref_idx[0] > 1) ||
                        (!collocated_from_l0_flag && sh.num_ref_idx[1] > 1)))
                        H265Bs_PutVLCCode(bs, sh.collocated_ref_idx);
                }
                if ((pps.weighted_pred_flag && sh.slice_type == P_SLICE)  ||
                    (pps.weighted_bipred_flag && sh.slice_type == B_SLICE)) {
                        //            pred_weight_table( )
                        VM_ASSERT(0);
                }
                H265Bs_PutVLCCode(bs, sh.five_minus_max_num_merge_cand);
            }
            H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh.slice_qp_delta));
            if (pps.pps_slice_chroma_qp_offsets_present_flag) {
                H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh.slice_cb_qp_offset));
                H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh.slice_cr_qp_offset));
            }
            if (pps.deblocking_filter_override_enabled_flag) {
                H265Bs_PutBit(bs, sh.deblocking_filter_override_flag);
            }
            if (sh.deblocking_filter_override_flag) {
                H265Bs_PutBit(bs, sh.slice_deblocking_filter_disabled_flag);
                if (!sh.slice_deblocking_filter_disabled_flag ) {
                    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh.slice_beta_offset_div2));
                    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh.slice_tc_offset_div2));
                }
            }
            if (pps.pps_loop_filter_across_slices_enabled_flag &&
                (sh.slice_sao_luma_flag || sh.slice_sao_chroma_flag ||
                !sh.slice_deblocking_filter_disabled_flag  ) ) {
                H265Bs_PutBit(bs, sh.slice_loop_filter_across_slices_enabled_flag);
            }
        }
        if (pps.tiles_enabled_flag || pps.entropy_coding_sync_enabled_flag)
        {
            H265Bs_PutVLCCode(bs, sh.num_entry_point_offsets);
            if (sh.num_entry_point_offsets > 0) {
                H265Bs_PutVLCCode(bs, sh.offset_len - 1);
                for (Ipp32u i = 0; i < sh.num_entry_point_offsets; i++)
                    H265Bs_PutBits(bs, sh.entry_point_offset[i] - 1, sh.offset_len);
            }
        }

        if (pps.slice_segment_header_extension_present_flag ) {
            VM_ASSERT(0);
            //H265Bs_PutVLCCode(bs, sh.slice_header_extension_length);
            //for (i = 0; i < slice_header_extension_length; i++)
            //    slice_header_extension_data_byte
        }

        bs->PutBit(1);
        bs->ByteAlignWithZeros();
    }

    void PutBufferingPeriod(H265BsReal *bs, const BufferingPeriod &sei, const H265SeqParameterSet &sps)
    {
        H265Bs_PutVLCCode(bs, sei.bp_seq_parameter_set_id);
        if (!sps.sub_pic_hrd_params_present_flag)
            H265Bs_PutBit(bs, sei.irap_cpb_params_present_flag);
        if (sei.irap_cpb_params_present_flag) {
            H265Bs_PutBits(bs, sei.cpb_delay_offset, sps.au_cpb_removal_delay_length_minus1 + 1);
            H265Bs_PutBits(bs, sei.dpb_delay_offset, sps.dpb_output_delay_length_minus1 + 1);
        }
        H265Bs_PutBit(bs, sei.concatenation_flag);
        H265Bs_PutBits(bs, sei.au_cpb_removal_delay_delta_minus1, sps.au_cpb_removal_delay_length_minus1 + 1);
        if (sps.nal_hrd_parameters_present_flag) {
            H265Bs_PutBits(bs, sei.nal_initial_cpb_removal_delay, sps.initial_cpb_removal_delay_length_minus1 + 1);
            H265Bs_PutBits(bs, sei.nal_initial_cpb_removal_offset, sps.initial_cpb_removal_delay_length_minus1 + 1);
            if (sps.sub_pic_hrd_params_present_flag || sei.irap_cpb_params_present_flag) {
                H265Bs_PutBits(bs, sei.nal_initial_alt_cpb_removal_delay, sps.initial_cpb_removal_delay_length_minus1 + 1);
                H265Bs_PutBits(bs, sei.nal_initial_alt_cpb_removal_offset, sps.initial_cpb_removal_delay_length_minus1 + 1);
            }
        }
        if (sps.vcl_hrd_parameters_present_flag) {
            H265Bs_PutBits(bs, sei.vcl_initial_cpb_removal_delay, sps.initial_cpb_removal_delay_length_minus1 + 1);
            H265Bs_PutBits(bs, sei.vcl_initial_cpb_removal_offset, sps.initial_cpb_removal_delay_length_minus1 + 1);
            if (sps.sub_pic_hrd_params_present_flag || sei.irap_cpb_params_present_flag) {
                H265Bs_PutBits(bs, sei.vcl_initial_alt_cpb_removal_delay, sps.initial_cpb_removal_delay_length_minus1 + 1);
                H265Bs_PutBits(bs, sei.vcl_initial_alt_cpb_removal_offset, sps.initial_cpb_removal_delay_length_minus1 + 1);
            }
        }
        if (H265Bs_GetBsOffset(bs) & 7)
            bs->WriteTrailingBits();
    }

    void PutPicTiming(H265BsReal *bs, const PicTiming &sei, const H265SeqParameterSet &sps)
    {
        if (sps.frame_field_info_present_flag) {
            H265Bs_PutBits(bs, sei.pic_struct, 4);
            H265Bs_PutBits(bs, sei.source_scan_type, 2);
            H265Bs_PutBit(bs, sei.duplicate_flag);
        }
        if (sps.nal_hrd_parameters_present_flag || sps.vcl_hrd_parameters_present_flag) {
            H265Bs_PutBits(bs, sei.au_cpb_removal_delay_minus1, sps.au_cpb_removal_delay_length_minus1 + 1);
            H265Bs_PutBits(bs, sei.pic_dpb_output_delay, sps.dpb_output_delay_length_minus1 + 1);
        }
        if (H265Bs_GetBsOffset(bs) & 7)
            bs->WriteTrailingBits();
    }

    void PutActiveParameterSets(H265BsReal *bs, const ActiveParameterSets &sei)
    {
        H265Bs_PutBits(bs, sei.active_video_parameter_set_id, 4);
        H265Bs_PutBit(bs, sei.self_contained_cvs_flag);
        H265Bs_PutBit(bs, sei.no_parameter_set_update_flag);
        H265Bs_PutVLCCode(bs, sei.num_sps_ids_minus1);
        for (Ipp32u i = 0; i <= sei.num_sps_ids_minus1; i++)
            H265Bs_PutVLCCode(bs, sei.active_seq_parameter_set_id[i]);
        if (H265Bs_GetBsOffset(bs) & 7)
            bs->WriteTrailingBits();
    }
} // anonimous namespace


void H265Enc::PutVPS(H265BsReal *bs, const H265VidParameterSet &vps, const H265ProfileLevelSet &profileLevel)
{
    Ipp32s i, j;

    H265Bs_PutBits(bs, vps.vps_video_parameter_set_id, 4);
    H265Bs_PutBits(bs, 3, 2); // reserved
    H265Bs_PutBits(bs, vps.vps_max_layers - 1, 6);
    H265Bs_PutBits(bs, vps.vps_max_sub_layers - 1, 3);
    H265Bs_PutBit(bs, vps.vps_temporal_id_nesting_flag);
    H265Bs_PutBits(bs, 0xffff, 16); // reserved

    PutProfileLevel(bs, 1, vps.vps_max_sub_layers, profileLevel);

    H265Bs_PutBit(bs, vps.vps_sub_layer_ordering_info_present_flag);

    for(i = (vps.vps_sub_layer_ordering_info_present_flag ? 0 : vps.vps_max_sub_layers - 1 );
            i <= vps.vps_max_sub_layers - 1; i++) {
        H265Bs_PutVLCCode(bs, vps.vps_max_dec_pic_buffering[i]);
        H265Bs_PutVLCCode(bs, vps.vps_max_num_reorder_pics[i]);
        H265Bs_PutVLCCode(bs, vps.vps_max_latency_increase[i]);
    }

    H265Bs_PutBits(bs, vps.vps_max_layer_id, 6);
    H265Bs_PutVLCCode(bs, vps.vps_num_layer_sets - 1);
    for (i = 1; i <= vps.vps_num_layer_sets - 1; i++)
        for (j = 0; j <= vps.vps_max_layer_id; j++) {
            //layer_id_included_flag[ i ][ j ]
            VM_ASSERT(0);
        }

    H265Bs_PutBit(bs, vps.vps_timing_info_present_flag);
    if (vps.vps_timing_info_present_flag) {
        PUTBITS_32(vps.vps_num_units_in_tick);
        PUTBITS_32(vps.vps_time_scale);
        H265Bs_PutBit(bs, vps.vps_poc_proportional_to_timing_flag);
        if (vps.vps_poc_proportional_to_timing_flag) {
            VM_ASSERT(0);
        }
        H265Bs_PutVLCCode(bs, vps.vps_num_hrd_parameters);
        if (vps.vps_num_hrd_parameters) {
            VM_ASSERT(0);
        }
    }
    H265Bs_PutBit(bs, vps.vps_extension_flag);
    if (vps.vps_extension_flag) {
        VM_ASSERT(0);
    }
}


void H265Enc::PutSPS(H265BsReal *bs, const H265SeqParameterSet &sps, const H265ProfileLevelSet &profileLevel)
{
    Ipp32s i;

    H265Bs_PutBits(bs, sps.sps_video_parameter_set_id, 4);
    H265Bs_PutBits(bs, sps.sps_max_sub_layers - 1, 3);
    H265Bs_PutBit(bs, sps.sps_temporal_id_nesting_flag);

    PutProfileLevel(bs, 1, sps.sps_max_sub_layers, profileLevel);

    H265Bs_PutVLCCode(bs, sps.sps_seq_parameter_set_id);
    H265Bs_PutVLCCode(bs, sps.chroma_format_idc);
    //if (sps.chroma_format_idc == 3)
    //    H265Bs_PutBit(bs, sps.separate_colour_plane_flag);
    H265Bs_PutVLCCode(bs, sps.pic_width_in_luma_samples);
    H265Bs_PutVLCCode(bs, sps.pic_height_in_luma_samples);

    H265Bs_PutBit(bs, sps.conformance_window_flag);
    if (sps.conformance_window_flag ) {
        H265Bs_PutVLCCode(bs, sps.conf_win_left_offset);
        H265Bs_PutVLCCode(bs, sps.conf_win_right_offset);
        H265Bs_PutVLCCode(bs, sps.conf_win_top_offset);
        H265Bs_PutVLCCode(bs, sps.conf_win_bottom_offset);
    }

    H265Bs_PutVLCCode(bs, sps.bit_depth_luma - 8);
    H265Bs_PutVLCCode(bs, sps.bit_depth_chroma - 8);

    H265Bs_PutVLCCode(bs, sps.log2_max_pic_order_cnt_lsb - 4);
    H265Bs_PutBit(bs, sps.sps_sub_layer_ordering_info_present_flag);

    for (i = (sps.sps_sub_layer_ordering_info_present_flag ? 0 : sps.sps_max_sub_layers - 1);
            i <= sps.sps_max_sub_layers - 1; i++) {
        H265Bs_PutVLCCode(bs, sps.sps_max_dec_pic_buffering[i]);
        H265Bs_PutVLCCode(bs, sps.sps_num_reorder_pics[i]);
        H265Bs_PutVLCCode(bs, sps.sps_max_latency_increase[i]);
    }

    H265Bs_PutVLCCode(bs, sps.log2_min_coding_block_size_minus3);
    H265Bs_PutVLCCode(bs, sps.log2_diff_max_min_coding_block_size);
    H265Bs_PutVLCCode(bs, sps.log2_min_transform_block_size_minus2);
    H265Bs_PutVLCCode(bs, sps.log2_diff_max_min_transform_block_size);
    H265Bs_PutVLCCode(bs, sps.max_transform_hierarchy_depth_inter - 1);
    H265Bs_PutVLCCode(bs, sps.max_transform_hierarchy_depth_intra - 1);

    H265Bs_PutBit(bs, sps.scaling_list_enable_flag);
    if (sps.scaling_list_enable_flag) {
        H265Bs_PutBit(bs, sps.sps_scaling_list_data_present_flag);
        if (sps.sps_scaling_list_data_present_flag) {
        //  scaling_list_param
            VM_ASSERT(0);
        }
    }
    H265Bs_PutBit(bs, sps.amp_enabled_flag);
    H265Bs_PutBit(bs, sps.sample_adaptive_offset_enabled_flag);
    H265Bs_PutBit(bs, sps.pcm_enabled_flag);

    if (sps.pcm_enabled_flag) {
        H265Bs_PutBits(bs, sps.pcm_sample_bit_depth_luma - 1, 4);
        H265Bs_PutBits(bs, sps.pcm_sample_bit_depth_chroma - 1, 4);
        H265Bs_PutVLCCode(bs, sps.log2_min_pcm_luma_coding_block_size - 3);
        H265Bs_PutVLCCode(bs, sps.log2_diff_max_min_pcm_luma_coding_block_size);
        H265Bs_PutBit(bs, sps.pcm_loop_filter_disabled_flag);
    }
    H265Bs_PutVLCCode(bs, sps.num_short_term_ref_pic_sets);
    for (i = 0; i < sps.num_short_term_ref_pic_sets; i++)
        PutShortTermRefPicSet(bs, sps, sps.m_shortRefPicSet[i], i);
    H265Bs_PutBit(bs, sps.long_term_ref_pics_present_flag);
    if (sps.long_term_ref_pics_present_flag) {
        VM_ASSERT(0);
    }
    H265Bs_PutBit(bs, sps.sps_temporal_mvp_enabled_flag);
    H265Bs_PutBit(bs, sps.strong_intra_smoothing_enabled_flag);

    H265Bs_PutBit(bs, sps.vui_parameters_present_flag);
    if (sps.vui_parameters_present_flag) {
        H265Bs_PutBit(bs, sps.aspect_ratio_info_present_flag);
        if (sps.aspect_ratio_info_present_flag) {
            H265Bs_PutBits(bs, sps.aspect_ratio_idc, 8);
            if (sps.aspect_ratio_idc == 255) {
                H265Bs_PutBits(bs, sps.sar_width, 16);
                H265Bs_PutBits(bs, sps.sar_height, 16);
            }
        }
        H265Bs_PutBit(bs, sps.overscan_info_present_flag);
        H265Bs_PutBit(bs, sps.video_signal_type_present_flag);
        H265Bs_PutBit(bs, sps.chroma_loc_info_present_flag);
        H265Bs_PutBit(bs, sps.neutral_chroma_indication_flag);
        H265Bs_PutBit(bs, sps.field_seq_flag);
        H265Bs_PutBit(bs, sps.frame_field_info_present_flag);
        H265Bs_PutBit(bs, sps.default_display_window_flag);
        H265Bs_PutBit(bs, sps.vui_timing_info_present_flag);
        if (sps.vui_timing_info_present_flag) {
            PUTBITS_32(sps.vui_num_units_in_tick);
            PUTBITS_32(sps.vui_time_scale);
            H265Bs_PutBit(bs, sps.vui_poc_proportional_to_timing_flag);
            if (sps.vui_poc_proportional_to_timing_flag)
                H265Bs_PutBit(bs, sps.vui_num_ticks_poc_diff_one_minus1);
            H265Bs_PutBit(bs, sps.vui_hrd_parameters_present_flag);
            if (sps.vui_hrd_parameters_present_flag) {
                H265Bs_PutBit(bs, sps.nal_hrd_parameters_present_flag);
                H265Bs_PutBit(bs, sps.vcl_hrd_parameters_present_flag);
                if (sps.nal_hrd_parameters_present_flag || sps.vcl_hrd_parameters_present_flag) {
                    H265Bs_PutBit(bs, sps.sub_pic_hrd_params_present_flag);
                    H265Bs_PutBits(bs, sps.bit_rate_scale, 4);
                    H265Bs_PutBits(bs, sps.cpb_size_scale, 4);
                    H265Bs_PutBits(bs, sps.initial_cpb_removal_delay_length_minus1, 5);
                    H265Bs_PutBits(bs, sps.au_cpb_removal_delay_length_minus1, 5);
                    H265Bs_PutBits(bs, sps.dpb_output_delay_length_minus1, 5);
                }
                for (Ipp32s i = 0; i < sps.sps_max_sub_layers; i++) {
                    H265Bs_PutBit(bs, sps.fixed_pic_rate_general_flag);
                    if (!sps.fixed_pic_rate_general_flag)
                        H265Bs_PutBit(bs, sps.fixed_pic_rate_within_cvs_flag);
                    if (sps.fixed_pic_rate_within_cvs_flag)
                        assert(!"unsupported fixed_pic_rate_within_cvs_flag == 1");
                    else
                        H265Bs_PutBit(bs, sps.low_delay_hrd_flag);
                    if (!sps.low_delay_hrd_flag)
                        H265Bs_PutVLCCode(bs, sps.cpb_cnt_minus1);
                    if (sps.nal_hrd_parameters_present_flag) {
                        for (Ipp32s i = 0; i <= sps.cpb_cnt_minus1; i++) {
                            H265Bs_PutVLCCode(bs, sps.bit_rate_value_minus1);
                            H265Bs_PutVLCCode(bs, sps.cpb_size_value_minus1);
                            H265Bs_PutBit(bs, sps.cbr_flag);
                        }
                    }
                }
            }
        }
        H265Bs_PutBit(bs, sps.bitstream_restriction_flag);
    }

    H265Bs_PutBit(bs, sps.sps_extension_flag);
    if (sps.sps_extension_flag) {
        VM_ASSERT(0);
    }
}


void H265Enc::PutPPS(H265BsReal *bs, const H265PicParameterSet &pps)
{
    H265Bs_PutVLCCode(bs, pps.pps_pic_parameter_set_id);
    H265Bs_PutVLCCode(bs, pps.pps_seq_parameter_set_id);
    H265Bs_PutBit(bs, pps.dependent_slice_segments_enabled_flag);
    H265Bs_PutBit(bs, pps.output_flag_present_flag);
    H265Bs_PutBits(bs, pps.num_extra_slice_header_bits, 3);
    H265Bs_PutBit(bs, pps.sign_data_hiding_enabled_flag);
    H265Bs_PutBit(bs, pps.cabac_init_present_flag);

    H265Bs_PutVLCCode(bs, pps.num_ref_idx_l0_default_active - 1);
    H265Bs_PutVLCCode(bs, pps.num_ref_idx_l1_default_active - 1);
    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps.init_qp - 26));
    H265Bs_PutBit(bs, pps.constrained_intra_pred_flag);
    H265Bs_PutBit(bs, pps.transform_skip_enabled_flag);
    H265Bs_PutBit(bs, pps.cu_qp_delta_enabled_flag);
    if (pps.cu_qp_delta_enabled_flag)
        H265Bs_PutVLCCode(bs, pps.diff_cu_qp_delta_depth);
    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps.pps_cb_qp_offset));
    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps.pps_cr_qp_offset));
    H265Bs_PutBit(bs, pps.pps_slice_chroma_qp_offsets_present_flag);
    H265Bs_PutBit(bs, pps.weighted_pred_flag);
    H265Bs_PutBit(bs, pps.weighted_bipred_flag);
    H265Bs_PutBit(bs, pps.transquant_bypass_enable_flag);
    H265Bs_PutBit(bs, pps.tiles_enabled_flag);
    H265Bs_PutBit(bs, pps.entropy_coding_sync_enabled_flag);
    if (pps.tiles_enabled_flag)
    {
        H265Bs_PutVLCCode(bs, pps.num_tile_columns - 1);
        H265Bs_PutVLCCode(bs, pps.num_tile_rows - 1);
        H265Bs_PutBit(bs, pps.uniform_spacing_flag);
        if (!pps.uniform_spacing_flag) {
            for (Ipp32s i = 0; i < pps.num_tile_columns - 1; i++)
                H265Bs_PutVLCCode(bs, pps.column_width[i]);
            for (Ipp32s i = 0; i < pps.num_tile_rows - 1; i++)
                H265Bs_PutVLCCode(bs, pps.row_height[i]);
        }
        H265Bs_PutBit(bs, pps.loop_filter_across_tiles_enabled_flag);
    }
    H265Bs_PutBit(bs, pps.pps_loop_filter_across_slices_enabled_flag);
    H265Bs_PutBit(bs, pps.deblocking_filter_control_present_flag);

    if (pps.deblocking_filter_control_present_flag) {
        H265Bs_PutBit(bs, pps.deblocking_filter_override_enabled_flag);
        H265Bs_PutBit(bs, pps.pps_deblocking_filter_disabled_flag);
        if (!pps.pps_deblocking_filter_disabled_flag) {
            H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps.pps_beta_offset_div2));
            H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps.pps_tc_offset_div2));
        }
    }
    H265Bs_PutBit(bs, pps.pps_scaling_list_data_present_flag);
    if (pps.pps_scaling_list_data_present_flag) {
        //scaling_list_param()
        VM_ASSERT(0);
    }
    H265Bs_PutBit(bs, pps.lists_modification_present_flag);
    H265Bs_PutVLCCode(bs, pps.log2_parallel_merge_level - 2);
    H265Bs_PutBit(bs, pps.slice_segment_header_extension_present_flag);
    H265Bs_PutBit(bs, pps.pps_extension_flag);
    if (pps.pps_extension_flag) {
        VM_ASSERT(0);
    }
}


mfxU32 H265BsReal::WriteNAL(Ipp8u *dst, Ipp32u dstBufSize, Ipp8u startPicture, Ipp32u nalUnitType)
{
    mfxBitstream dstBs = {};
    dstBs.Data = dst;
    dstBs.MaxLength = dstBufSize;
    H265NALUnit nal = {};
    nal.nal_unit_type = nalUnitType;
    return WriteNAL(&dstBs, startPicture, &nal);
}

mfxU32 H265BsReal::WriteNAL(mfxBitstream *dst,
                            Ipp8u startPicture,
                            H265NALUnit *nal)
{
    H265BsReal *bs = this;
    Ipp32u size, ExtraBytes, maxdst;
    Ipp8u* curPtr, *endPtr, *outPtr;
    Ipp16u nal_header = 0;

    maxdst = dst->MaxLength - dst->DataOffset - dst->DataLength;
    // get current RBSP compressed size
    size = (Ipp32u)(bs->m_base.m_pbs - bs->m_base.m_pbsRBSPBase + (bs->m_base.m_bitOffset > 0));
    ExtraBytes = 0;

    if (maxdst < size + 5) {
        ExtraBytes = 5;
        goto overflow;
    }

    // Set Pointers
    endPtr = bs->m_base.m_pbsRBSPBase + size - 1;  // Point at Last byte with data in it.
    curPtr = bs->m_base.m_pbsRBSPBase;
    outPtr = dst->Data + dst->DataOffset + dst->DataLength;

    // B.1.2
    // start access unit => should be zero_byte
    // for VPS,SPS,PPS,APS NAL units zero_byte should exist
    if (startPicture ||
        nal->nal_unit_type == NAL_AUD ||
        nal->nal_unit_type == NAL_VPS ||
        nal->nal_unit_type == NAL_SPS ||
        nal->nal_unit_type == NAL_PPS) {
        if (maxdst < size + 6) {
            ExtraBytes = 6;
            goto overflow;
        }
        *outPtr++ = 0;
        ExtraBytes = 1;
        startPicture = false;
    }


    // start_code_prefix_one_3bytes
    *outPtr++ = 0;
    *outPtr++ = 0;
    *outPtr++ = 1;
    ExtraBytes += 3;

    //
    nal_header = (nal->nal_unit_type << 9) | (nal->nuh_layer_id << 3) | (nal->nuh_temporal_id + 1);
    *outPtr++ = (Ipp8u) (nal_header >> 8);
    *outPtr++ = (Ipp8u) (nal_header & 0xff);
    ExtraBytes += 2;

    if (nal->nal_unit_type == NAL_UT_PREFIX_SEI || nal->nal_unit_type == NAL_UT_SUFFIX_SEI) {
        Ipp32u payloadType = nal->seiPayloadType;
        Ipp32u payloadSize = nal->seiPayloadSize;
        ExtraBytes += 1 + payloadType / 255;
        ExtraBytes += 1 + payloadSize / 255;
        if (maxdst < size + ExtraBytes)
            goto overflow;

        for (; payloadType > 254; payloadType -= 255)
            *outPtr++ = 0xff;
        *outPtr++ = payloadType;
        for (; payloadSize > 254; payloadSize -= 255)
            *outPtr++ = 0xff;
        *outPtr++ = payloadSize;
    }

    if( size ) {
        while (curPtr < endPtr-1) { // Copy all but the last 2 bytes
            *outPtr++ = *curPtr;

            // Check for start code emulation
            if ((*curPtr++ == 0) && (*curPtr == 0) && (!(*(curPtr+1) & 0xfc))) {
                ExtraBytes++;
                if (maxdst < size + ExtraBytes) {
                    goto overflow;
                }
                *outPtr++ = *curPtr++;
                *outPtr++ = 0x03;   // Emulation Prevention Byte
            }
        }

        if (curPtr < endPtr) *outPtr++ = *curPtr++;
        // copy the last byte
        *outPtr++ = *curPtr++;

        // Update RBSP Base Pointer
        bs->m_base.m_pbsRBSPBase = bs->m_base.m_pbs;
    }

    if (nal->nal_unit_type == NAL_UT_PREFIX_SEI || nal->nal_unit_type == NAL_UT_SUFFIX_SEI) {
        if (bs->m_base.m_bitOffset != 0) {
            // sei messages from user may be unaligned to byte
            // do alignement by writing trailing rbsp bits here because:
            //   1) we can't modify user's mfxPayloads
            //   2) we don't want to make a copies of user's mfxPayloads
            outPtr[-1] &= 0xff << (8 - bs->m_base.m_bitOffset);
            outPtr[-1] |= 0x80 >> bs->m_base.m_bitOffset;
        }
        ExtraBytes++;
        if (maxdst < size + ExtraBytes)
            goto overflow;
        *outPtr++ = 0x80; // trailing bits 10000000, always 8 bits since sei_message is always byte-aligned
    }

    dst->DataLength += (size+ExtraBytes);

    // copy encoded frame to output
    return (size+ExtraBytes);
overflow:
    // stop writing when overflow predicted, but advance counters for brc
    bs->m_base.m_pbsRBSPBase = bs->m_base.m_pbs;
    dst->DataLength += (size+ExtraBytes);
    return (size+ExtraBytes);
}


Ipp32s H265FrameEncoder::WriteBitstreamHeaderSet(mfxBitstream *mfxBS, Ipp32s bs_main_id)
{
    if (m_videoParam.RegionIdP1 > 1)
        return 0; // headers are for first region only

    H265NALUnit nal;
    nal.nuh_layer_id = 0;
    nal.nuh_temporal_id = 0;

    Ipp32s overheadBytes = 0;

    if (m_videoParam.writeAud) {
        PutAUD(&m_bs[bs_main_id], m_frame->m_slices[0].slice_type);
        m_bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_AUD;
        overheadBytes += m_bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
    }

    if (m_frame->m_isIdrPic) {
        PutVPS(&m_bs[bs_main_id], m_topEnc.m_vps, m_topEnc.m_profile_level);
        m_bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_VPS;
        overheadBytes += m_bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);

        PutSPS(&m_bs[bs_main_id], m_topEnc.m_sps, m_topEnc.m_profile_level);
        m_bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_SPS;
        overheadBytes += m_bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);

        PutPPS(&m_bs[bs_main_id], m_topEnc.m_pps);
        m_bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_PPS;
        overheadBytes += m_bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);

        if (m_topEnc.m_sps.nal_hrd_parameters_present_flag) {
            PutActiveParameterSets(&m_bs[bs_main_id], m_topEnc.m_seiAps);
            nal.nal_unit_type = NAL_UT_PREFIX_SEI;
            nal.seiPayloadType = SEI_ACTIVE_PARAMETER_SETS;
            nal.seiPayloadSize = Ipp32u(m_bs[bs_main_id].m_base.m_pbs - m_bs[bs_main_id].m_base.m_pbsRBSPBase);
            overheadBytes += m_bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);

            BufferingPeriod bufferingPeriod = {};
            bufferingPeriod.bp_seq_parameter_set_id = m_topEnc.m_sps.sps_seq_parameter_set_id;
            bufferingPeriod.nal_initial_cpb_removal_delay  = m_topEnc.m_hrd.GetInitCpbRemovalDelay();
            bufferingPeriod.nal_initial_cpb_removal_offset = m_topEnc.m_hrd.GetInitCpbRemovalOffset();
            PutBufferingPeriod(&m_bs[bs_main_id], bufferingPeriod, m_topEnc.m_sps);
            nal.nal_unit_type = NAL_UT_PREFIX_SEI;
            nal.seiPayloadType = SEI_BUFFERING_PERIOD;
            nal.seiPayloadSize = Ipp32u(m_bs[bs_main_id].m_base.m_pbs - m_bs[bs_main_id].m_base.m_pbsRBSPBase);
            overheadBytes += m_bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
        }
    }

    if (m_topEnc.m_sps.nal_hrd_parameters_present_flag) {
        PicTiming picTiming = {};
        picTiming.au_cpb_removal_delay_minus1 = m_topEnc.m_hrd.GetAuCpbRemovalDelayMinus1(*m_frame);
        picTiming.pic_dpb_output_delay = m_topEnc.m_sps.sps_num_reorder_pics[0] + m_frame->m_frameOrder - m_frame->m_encOrder;
        PutPicTiming(&m_bs[bs_main_id], picTiming, m_topEnc.m_sps);
        nal.nal_unit_type = NAL_UT_PREFIX_SEI;
        nal.seiPayloadType = SEI_PIC_TIMING;
        nal.seiPayloadSize = Ipp32u(m_bs[bs_main_id].m_base.m_pbs - m_bs[bs_main_id].m_base.m_pbsRBSPBase);
        overheadBytes += m_bs[bs_main_id].WriteNAL(mfxBS, !m_frame->m_isIdrPic, &nal);
    }

    if (m_frame->m_userSeiMessages) {
        assert(m_bs[bs_main_id].m_base.m_bitOffset == 0);
        for (Ipp32u i = 0; i < m_frame->m_numUserSeiMessages; i++) {
            H265BsReal bs;
            bs.m_base.m_pbsRBSPBase = bs.m_base.m_pbsBase = m_frame->m_userSeiMessages[i]->Data;
            bs.m_base.m_maxBsSize = m_frame->m_userSeiMessages[i]->BufSize;
            bs.m_base.m_pbs = bs.m_base.m_pbsRBSPBase + m_frame->m_userSeiMessages[i]->NumBit / 8;
            bs.m_base.m_bitOffset = m_frame->m_userSeiMessages[i]->NumBit & 7;
            nal.nal_unit_type = NAL_UT_PREFIX_SEI;
            nal.seiPayloadType = m_frame->m_userSeiMessages[i]->Type;
            nal.seiPayloadSize = (m_frame->m_userSeiMessages[i]->NumBit + 7) / 8;
            overheadBytes += bs.WriteNAL(mfxBS, !m_frame->m_isIdrPic, &nal);
        }
    }

    return overheadBytes;
}

Ipp32s H265FrameEncoder::WriteBitstreamPayload(mfxBitstream *mfxBS, Ipp32s bs_main_id)
{
    H265NALUnit nal;
    nal.nuh_layer_id = 0;
    nal.nuh_temporal_id = 0;

    //return MFX_ERR_NONE;
    Ipp8u start = 0, end = m_videoParam.NumSlices;
    if (m_videoParam.RegionIdP1 > 0) {
        start = m_videoParam.RegionIdP1 - 1;
        end = m_videoParam.RegionIdP1;
    }

    Ipp32s overheadBytes = 0;
    for (Ipp8u curr_slice = start; curr_slice < end; curr_slice++) {
        H265Slice *pSlice = &m_frame->m_slices[curr_slice];

        if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag || m_topEnc.m_pps.tiles_enabled_flag) {
            Ipp32s ctb_row = pSlice->row_first;

            for (Ipp32u i = 0; i < pSlice->num_entry_point_offsets; i++) {
                Ipp32u offset_add = 0;
                Ipp32s bs_idx = m_topEnc.m_pps.entropy_coding_sync_enabled_flag ? ctb_row++ : i;
                Ipp8u *curPtr = m_bs[bs_idx].m_base.m_pbsBase;
                Ipp32s size = H265Bs_GetBsSize(&m_bs[bs_idx]);

                for (Ipp32s j = 1; j < size - 1; j++) {
                    if (!curPtr[j - 1] && !curPtr[j] && !(curPtr[j + 1] & 0xfc)) {
                        j++;
                        offset_add ++;
                    }
                }

                pSlice->entry_point_offset[i] = size + offset_add;
            }
        }

        m_bs[bs_main_id].Reset();
        pSlice->offset_len = GetMaxOffsetLen(*pSlice);
        PutSliceHeader(&m_bs[bs_main_id], *pSlice, m_topEnc.m_sps, m_topEnc.m_pps);
        overheadBytes += H265Bs_GetBsSize(&m_bs[bs_main_id]);

        if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag) {
            for (Ipp32u row = pSlice->row_first; row <= pSlice->row_last; row++) {
                Ipp32s size = H265Bs_GetBsSize(&m_bs[row]);
                small_memcpy(m_bs[bs_main_id].m_base.m_pbs, m_bs[row].m_base.m_pbsBase, size);
                m_bs[bs_main_id].m_base.m_pbs += size;
            }
        } else if (m_topEnc.m_pps.tiles_enabled_flag) {
            for (Ipp32u i = 0; i < pSlice->num_entry_point_offsets + 1; i++) {
                Ipp32s size = H265Bs_GetBsSize(&m_bs[i]);
                small_memcpy(m_bs[bs_main_id].m_base.m_pbs, m_bs[i].m_base.m_pbsBase, size);
                m_bs[bs_main_id].m_base.m_pbs += size;
            }
        } else {
            Ipp32s size = H265Bs_GetBsSize(&m_bs[curr_slice]);
            small_memcpy(m_bs[bs_main_id].m_base.m_pbs, m_bs[curr_slice].m_base.m_pbsBase, size);
            m_bs[bs_main_id].m_base.m_pbs += size;
        }

        nal.nal_unit_type = pSlice->NalUnitType;

        m_bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
    }

    return overheadBytes;
}


Ipp32s H265FrameEncoder::GetOutputData(mfxBitstream *mfxBS)
{
    Ipp32s overheadBytes = 0;
    Ipp32s bs_main_id = m_videoParam.num_bs_subsets;

    m_bs[bs_main_id].Reset();
    overheadBytes += WriteBitstreamHeaderSet(mfxBS, bs_main_id); // const part should moved on top level
    overheadBytes += WriteBitstreamPayload(mfxBS, bs_main_id);

    // fill bitstream params
    mfxBitstream* bs = mfxBS;
    Frame* codedTask = m_frame;
    bs->TimeStamp = codedTask->m_timeStamp;
    mfxI32 dpb_output_delay = m_topEnc.m_vps.vps_max_num_reorder_pics[0] + codedTask->m_frameOrder - codedTask->m_encOrder;
    bs->DecodeTimeStamp = mfxI64(bs->TimeStamp - m_videoParam.tcDuration90KHz * dpb_output_delay); // calculate DTS from PTS
    bs->FrameType = (mfxU16)codedTask->m_frameType;

    return overheadBytes;
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE