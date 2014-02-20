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

namespace H265Enc {

#define H265Bs_PutBit(bs,code) (bs)->PutBit(code)
#define H265Bs_PutBits(bs,code,len) (bs)->PutBits(code,len)
#define H265Bs_PutVLCCode(bs,code) (bs)->PutVLCCode(code)


mfxStatus H265Encoder::WriteEndOfSequence(mfxBitstream *bs)
{
/*   H265BsReal_8u16s rbs;
   rbs.m_pbsRBSPBase = rbs.m_base.m_pbs = 0;
   bool s = false;
   mfxU8* ptrToWrite = bs->Data + bs->DataOffset + bs->DataLength;
   bs->DataLength += H265BsReal_EndOfNAL_8u16s( &rbs, ptrToWrite, 0, NAL_UT_EOSEQ, s, 0);*/
   return MFX_ERR_NONE;
}

mfxStatus H265Encoder::WriteEndOfStream(mfxBitstream *bs)
{
/*   H265BsReal_8u16s rbs;
   rbs.m_pbsRBSPBase = rbs.m_base.m_pbs = 0;
   bool s = false;
   mfxU8* ptrToWrite = bs->Data + bs->DataOffset + bs->DataLength;
   bs->DataLength += H265BsReal_EndOfNAL_8u16s( &rbs, ptrToWrite, 0, NAL_UT_EOSTREAM, s, 0);*/
   return MFX_ERR_NONE;
}

mfxStatus H265Encoder::WriteFillerData(mfxBitstream *bs, mfxI32 num)
{
/*   H265BsReal_8u16s rbs;
   UMC::Status st;
   Ipp8u* buf = new Ipp8u [num];

   H265BsReal_Create_8u16s(&rbs,buf,num,0,st);
   memset(buf, 0xff, num-1);
   buf[num-1] = 0x80;
   rbs.m_base.m_pbs += num;
   bool s = false;
   mfxU8* ptrToWrite = bs->Data + bs->DataOffset + bs->DataLength;
   bs->DataLength += H265BsReal_EndOfNAL_8u16s( &rbs, ptrToWrite, 0, NAL_UT_FILL, s, 0);
   H265BsReal_Destroy_8u16s( &rbs );
   delete[] buf;*/
    return MFX_ERR_NONE;
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
    size = (Ipp32u)(bs->m_base.m_pbs - bs->m_base.m_pbsRBSPBase);
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
        nal->nal_unit_type == NAL_UT_VPS ||
        nal->nal_unit_type == NAL_UT_SPS ||
        nal->nal_unit_type == NAL_UT_PPS) {
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
        *outPtr = *curPtr;

        // Update RBSP Base Pointer
        bs->m_base.m_pbsRBSPBase = bs->m_base.m_pbs;
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

void H265Encoder::PutProfileLevel(H265BsReal *_bs, Ipp8u profile_present_flag, Ipp32s max_sub_layers)
{
    H265BsReal *bs = _bs;

    if (profile_present_flag) {
        H265Bs_PutBits(bs, m_profile_level.general_profile_space, 2);
        H265Bs_PutBits(bs, m_profile_level.general_tier_flag, 1);
        H265Bs_PutBits(bs, m_profile_level.general_profile_idc, 5);
        for (Ipp32s i = 0; i < 32; i++)
            H265Bs_PutBit(bs, m_profile_level.general_profile_compatibility_flag[i]);
        H265Bs_PutBits(bs, m_profile_level.general_progressive_source_flag, 1);
        H265Bs_PutBits(bs, m_profile_level.general_interlaced_source_flag, 1);
        H265Bs_PutBits(bs, m_profile_level.general_non_packed_constraint_flag, 1);
        H265Bs_PutBits(bs, m_profile_level.general_frame_only_constraint_flag, 1);
        H265Bs_PutBits(bs, 0, 22); // reserved
        H265Bs_PutBits(bs, 0, 22); // reserved
        H265Bs_PutBits(bs, m_profile_level.general_level_idc, 8);
        if (max_sub_layers > 1) {
            VM_ASSERT(0);
        }
    }
}

#define PUTBITS_32(code) { H265Bs_PutBits(bs, ((Ipp32u)(code)>>16), 16); H265Bs_PutBits(bs, ((code)&0xffff), 16);}

mfxStatus H265Encoder::PutVPS(H265BsReal *_bs)
{
    Ipp32s i, j;
    H265VidParameterSet *vps = &(m_vps);
    H265BsReal *bs = _bs;

    H265Bs_PutBits(bs, vps->vps_video_parameter_set_id, 4);
    H265Bs_PutBits(bs, 3, 2); // reserved
    H265Bs_PutBits(bs, vps->vps_max_layers - 1, 6);
    H265Bs_PutBits(bs, vps->vps_max_sub_layers - 1, 3);
    H265Bs_PutBit(bs, vps->vps_temporal_id_nesting_flag);
    H265Bs_PutBits(bs, 0xffff, 16); // reserved

    PutProfileLevel(bs, 1, vps->vps_max_sub_layers);

    H265Bs_PutBit(bs, vps->vps_sub_layer_ordering_info_present_flag);

    for(i = (vps->vps_sub_layer_ordering_info_present_flag ? 0 : vps->vps_max_sub_layers - 1 );
            i <= vps->vps_max_sub_layers - 1; i++) {
        H265Bs_PutVLCCode(bs, vps->vps_max_dec_pic_buffering[i]);
        H265Bs_PutVLCCode(bs, vps->vps_max_num_reorder_pics[i]);
        H265Bs_PutVLCCode(bs, vps->vps_max_latency_increase[i]);
    }

    H265Bs_PutBits(bs, vps->vps_max_layer_id, 6);
    H265Bs_PutVLCCode(bs, vps->vps_num_layer_sets - 1);
    for (i = 1; i <= vps->vps_num_layer_sets - 1; i++)
        for (j = 0; j <= vps->vps_max_layer_id; j++) {
//            layer_id_included_flag[ i ][ j ]
            VM_ASSERT(0);
        }

    H265Bs_PutBit(bs, vps->vps_timing_info_present_flag);
    if (vps->vps_timing_info_present_flag) {
        PUTBITS_32(vps->vps_num_units_in_tick);
        PUTBITS_32(vps->vps_time_scale);
        H265Bs_PutBit(bs, vps->vps_poc_proportional_to_timing_flag);
        if (vps->vps_poc_proportional_to_timing_flag) {
            VM_ASSERT(0);
        }
        H265Bs_PutVLCCode(bs, vps->vps_num_hrd_parameters);
        if (vps->vps_num_hrd_parameters) {
            VM_ASSERT(0);
        }
    }
    H265Bs_PutBit(bs, vps->vps_extension_flag);
    if (vps->vps_extension_flag) {
        VM_ASSERT(0);
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::PutSPS(H265BsReal *_bs)
{
    Ipp32s i;
    H265SeqParameterSet *sps = &(m_sps);
    H265BsReal *bs = _bs;

    H265Bs_PutBits(bs, sps->sps_video_parameter_set_id, 4);
    H265Bs_PutBits(bs, sps->sps_max_sub_layers - 1, 3);
    H265Bs_PutBit(bs, sps->sps_temporal_id_nesting_flag);

    PutProfileLevel(bs, 1, sps->sps_max_sub_layers);

    H265Bs_PutVLCCode(bs, sps->sps_seq_parameter_set_id);
    H265Bs_PutVLCCode(bs, sps->chroma_format_idc);
//    if (sps->chroma_format_idc == 3)
//        H265Bs_PutBit(bs, sps->separate_colour_plane_flag);
    H265Bs_PutVLCCode(bs, sps->pic_width_in_luma_samples);
    H265Bs_PutVLCCode(bs, sps->pic_height_in_luma_samples);

    H265Bs_PutBit(bs, sps->conformance_window_flag);
    if (sps->conformance_window_flag ) {
        H265Bs_PutVLCCode(bs, sps->conf_win_left_offset);
        H265Bs_PutVLCCode(bs, sps->conf_win_right_offset);
        H265Bs_PutVLCCode(bs, sps->conf_win_top_offset);
        H265Bs_PutVLCCode(bs, sps->conf_win_bottom_offset);
    }

    H265Bs_PutVLCCode(bs, sps->bit_depth_luma - 8);
    H265Bs_PutVLCCode(bs, sps->bit_depth_chroma - 8);

    H265Bs_PutVLCCode(bs, sps->log2_max_pic_order_cnt_lsb - 4);
    H265Bs_PutBit(bs, sps->sps_sub_layer_ordering_info_present_flag);

    for (i = (sps->sps_sub_layer_ordering_info_present_flag ? 0 : sps->sps_max_sub_layers - 1);
            i <= sps->sps_max_sub_layers - 1; i++) {
        H265Bs_PutVLCCode(bs, sps->sps_max_dec_pic_buffering[i]);
        H265Bs_PutVLCCode(bs, sps->sps_num_reorder_pics[i]);
        H265Bs_PutVLCCode(bs, sps->sps_max_latency_increase[i]);
    }

    H265Bs_PutVLCCode(bs, sps->log2_min_coding_block_size_minus3);
    H265Bs_PutVLCCode(bs, sps->log2_diff_max_min_coding_block_size);
    H265Bs_PutVLCCode(bs, sps->log2_min_transform_block_size_minus2);
    H265Bs_PutVLCCode(bs, sps->log2_diff_max_min_transform_block_size);
    H265Bs_PutVLCCode(bs, sps->max_transform_hierarchy_depth_inter - 1);
    H265Bs_PutVLCCode(bs, sps->max_transform_hierarchy_depth_intra - 1);

    H265Bs_PutBit(bs, sps->scaling_list_enable_flag);
    if (sps->scaling_list_enable_flag) {
        H265Bs_PutBit(bs, sps->sps_scaling_list_data_present_flag);
        if (sps->sps_scaling_list_data_present_flag) {
        //  scaling_list_param
            VM_ASSERT(0);
        }
    }
    H265Bs_PutBit(bs, sps->amp_enabled_flag);
    H265Bs_PutBit(bs, sps->sample_adaptive_offset_enabled_flag);
    H265Bs_PutBit(bs, sps->pcm_enabled_flag);

    if (sps->pcm_enabled_flag) {
        H265Bs_PutBits(bs, sps->pcm_sample_bit_depth_luma - 1, 4);
        H265Bs_PutBits(bs, sps->pcm_sample_bit_depth_chroma - 1, 4);
        H265Bs_PutVLCCode(bs, sps->log2_min_pcm_luma_coding_block_size - 3);
        H265Bs_PutVLCCode(bs, sps->log2_diff_max_min_pcm_luma_coding_block_size);
        H265Bs_PutBit(bs, sps->pcm_loop_filter_disabled_flag);
    }
    H265Bs_PutVLCCode(bs, sps->num_short_term_ref_pic_sets);
    for (i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
        PutShortTermRefPicSet(bs, i);
    }
    H265Bs_PutBit(bs, sps->long_term_ref_pics_present_flag);
    if (sps->long_term_ref_pics_present_flag) {
        VM_ASSERT(0);
    }
    H265Bs_PutBit(bs, sps->sps_temporal_mvp_enabled_flag);
    H265Bs_PutBit(bs, sps->strong_intra_smoothing_enabled_flag);

    H265Bs_PutBit(bs, sps->vui_parameters_present_flag);
    if (sps->vui_parameters_present_flag) {
        H265Bs_PutBit(bs, sps->aspect_ratio_info_present_flag);
        if (sps->aspect_ratio_info_present_flag) {
            H265Bs_PutBits(bs, sps->aspect_ratio_idc, 8);
            if (sps->aspect_ratio_idc == 255) {
                H265Bs_PutBits(bs, sps->sar_width, 16);
                H265Bs_PutBits(bs, sps->sar_height, 16);
            }
        }
        if (sps->overscan_info_present_flag ||
            sps->video_signal_type_present_flag ||
            sps->chroma_loc_info_present_flag ||
            sps->neutral_chroma_indication_flag ||
            sps->field_seq_flag ||
            sps->frame_field_info_present_flag ||
            sps->default_display_window_flag ||
            sps->sps_timing_info_present_flag || // use vps*
            sps->bitstream_restriction_flag)
        {
            VM_ASSERT(0);
        }
        H265Bs_PutBit(bs, sps->overscan_info_present_flag);
        H265Bs_PutBit(bs, sps->video_signal_type_present_flag);
        H265Bs_PutBit(bs, sps->chroma_loc_info_present_flag);
        H265Bs_PutBit(bs, sps->neutral_chroma_indication_flag);
        H265Bs_PutBit(bs, sps->field_seq_flag);
        H265Bs_PutBit(bs, sps->frame_field_info_present_flag);
        H265Bs_PutBit(bs, sps->default_display_window_flag);
        H265Bs_PutBit(bs, sps->sps_timing_info_present_flag); // use vps*
        H265Bs_PutBit(bs, sps->bitstream_restriction_flag);
    } // EO VUI

    for (i = 0; i < m_videoParam.MaxCUDepth; i++) {
        H265Bs_PutBit(bs, 1);
    }

    H265Bs_PutBit(bs, sps->sps_extension_flag);
    if (sps->sps_extension_flag) {
        VM_ASSERT(0);
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::PutPPS(H265BsReal *_bs)
{
    Ipp32s i;
    H265PicParameterSet *pps = &(m_pps);
    H265BsReal *bs = _bs;

    H265Bs_PutVLCCode(bs, pps->pps_pic_parameter_set_id);
    H265Bs_PutVLCCode(bs, pps->pps_seq_parameter_set_id);
    H265Bs_PutBit(bs, pps->dependent_slice_segments_enabled_flag);
    H265Bs_PutBit(bs, pps->output_flag_present_flag);
    H265Bs_PutBits(bs, pps->num_extra_slice_header_bits, 3);
    H265Bs_PutBit(bs, pps->sign_data_hiding_enabled_flag);
    H265Bs_PutBit(bs, pps->cabac_init_present_flag);

    H265Bs_PutVLCCode(bs, pps->num_ref_idx_l0_default_active - 1);
    H265Bs_PutVLCCode(bs, pps->num_ref_idx_l1_default_active - 1);
    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps->init_qp - 26));
    H265Bs_PutBit(bs, pps->constrained_intra_pred_flag);
    H265Bs_PutBit(bs, pps->transform_skip_enabled_flag);
    H265Bs_PutBit(bs, pps->cu_qp_delta_enabled_flag);
    if (pps->cu_qp_delta_enabled_flag)
        H265Bs_PutVLCCode(bs, pps->diff_cu_qp_delta_depth);
    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps->pps_cb_qp_offset));
    H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps->pps_cr_qp_offset));
    H265Bs_PutBit(bs, pps->pps_slice_chroma_qp_offsets_present_flag);
    H265Bs_PutBit(bs, pps->weighted_pred_flag);
    H265Bs_PutBit(bs, pps->weighted_bipred_flag);
    H265Bs_PutBit(bs, pps->transquant_bypass_enable_flag);
    H265Bs_PutBit(bs, pps->tiles_enabled_flag);
    H265Bs_PutBit(bs, pps->entropy_coding_sync_enabled_flag);
    if (pps->tiles_enabled_flag)
    {
        H265Bs_PutVLCCode(bs, pps->num_tile_columns - 1);
        H265Bs_PutVLCCode(bs, pps->num_tile_rows - 1);
        H265Bs_PutBit(bs, pps->uniform_spacing_flag);
        if (!pps->uniform_spacing_flag) {
            for (i = 0; i < pps->num_tile_columns - 1; i++)
                H265Bs_PutVLCCode(bs, pps->column_width[i]);
            for (i = 0; i < pps->num_tile_rows - 1; i++)
                H265Bs_PutVLCCode(bs, pps->row_height[i]);
        }
        H265Bs_PutBit(bs, pps->loop_filter_across_tiles_enabled_flag);
    }
    H265Bs_PutBit(bs, pps->pps_loop_filter_across_slices_enabled_flag);
    H265Bs_PutBit(bs, pps->deblocking_filter_control_present_flag);

    if (pps->deblocking_filter_control_present_flag) {
        H265Bs_PutBit(bs, pps->deblocking_filter_override_enabled_flag);
        H265Bs_PutBit(bs, pps->pps_deblocking_filter_disabled_flag);
        if (!pps->pps_deblocking_filter_disabled_flag) {
            H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps->pps_beta_offset_div2));
            H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(pps->pps_tc_offset_div2));
        }
    }
    H265Bs_PutBit(bs, pps->pps_scaling_list_data_present_flag);
    if (pps->pps_scaling_list_data_present_flag) {
        //    scaling_list_param( )
        VM_ASSERT(0);
    }
    H265Bs_PutBit(bs, pps->lists_modification_present_flag);
    H265Bs_PutVLCCode(bs, pps->log2_parallel_merge_level - 2);
    H265Bs_PutBit(bs, pps->slice_segment_header_extension_present_flag);
    H265Bs_PutBit(bs, pps->pps_extension_flag);
    if (pps->pps_extension_flag) {
        VM_ASSERT(0);
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::PutShortTermRefPicSet(H265BsReal *_bs, Ipp32s idx) {
    Ipp32s i;
    H265BsReal *bs = _bs;

    if (idx > 0) {
        H265Bs_PutBit(bs, m_ShortRefPicSet[idx].inter_ref_pic_set_prediction_flag);
    } else if (m_ShortRefPicSet[idx].inter_ref_pic_set_prediction_flag != 0) {
        VM_ASSERT(0);
    }

    if (m_ShortRefPicSet[idx].inter_ref_pic_set_prediction_flag)
    {
        Ipp32s RIdx = idx - m_ShortRefPicSet[idx].delta_idx;
        Ipp32s NumDeltaPocs = m_ShortRefPicSet[RIdx].num_negative_pics + m_ShortRefPicSet[RIdx].num_negative_pics;

        H265Bs_PutVLCCode(bs, m_ShortRefPicSet[idx].delta_idx - 1);
        H265Bs_PutBit(bs, m_ShortRefPicSet[idx].delta_rps_sign);
        H265Bs_PutVLCCode(bs, m_ShortRefPicSet[idx].abs_delta_rps - 1);

        for (i = 0; i <= NumDeltaPocs; i++)
        {
            H265Bs_PutBit(bs, m_ShortRefPicSet[idx].used_by_curr_pic_flag[i]);
            if (!m_ShortRefPicSet[idx].used_by_curr_pic_flag[i])
            {
                H265Bs_PutBit(bs, m_ShortRefPicSet[idx].use_delta_flag[i]);
            }
        }
    }
    else
    {
        H265Bs_PutVLCCode(bs, m_ShortRefPicSet[idx].num_negative_pics);
        H265Bs_PutVLCCode(bs, m_ShortRefPicSet[idx].num_positive_pics);

        for (i = 0; i < m_ShortRefPicSet[idx].num_negative_pics + m_ShortRefPicSet[idx].num_positive_pics; i++)
        {
            H265Bs_PutVLCCode(bs, m_ShortRefPicSet[idx].delta_poc[i] - 1);
            H265Bs_PutBit(bs, m_ShortRefPicSet[idx].used_by_curr_pic_flag[i]);
        }
    }

    return MFX_ERR_NONE;
}


mfxStatus H265Encoder::PutSliceHeader(H265BsReal *_bs, H265Slice *slice)
{
    Ipp32u i;
    H265SeqParameterSet *sps = &(m_sps);
    H265PicParameterSet *pps = &(m_pps);
    H265VideoParam *picVars = &(m_videoParam);
    H265Slice *sh = slice;
    H265BsReal *bs = _bs;

    H265Bs_PutBit(bs, sh->first_slice_segment_in_pic_flag);
    if (sh->RapPicFlag) {
        H265Bs_PutBit(bs, sh->no_output_of_prior_pics_flag);
    }

    H265Bs_PutVLCCode(bs, sh->slice_pic_parameter_set_id);

    if (!sh->first_slice_segment_in_pic_flag) {
        if (pps->dependent_slice_segments_enabled_flag) {
            H265Bs_PutBit(bs, sh->dependent_slice_segment_flag);
        }
        Ipp32s slice_address_len = H265_CeilLog2(picVars->PicWidthInCtbs * picVars->PicHeightInCtbs);
        H265Bs_PutBits(bs, sh->slice_segment_address, slice_address_len);
    }

    if (!sh->dependent_slice_segment_flag) {
        if (pps->num_extra_slice_header_bits)
            H265Bs_PutBits(bs, 0, pps->num_extra_slice_header_bits);
        H265Bs_PutVLCCode(bs, sh->slice_type);
        if (pps->output_flag_present_flag )
            H265Bs_PutBit(bs, sh->pic_output_flag);
//        if (sps->separate_colour_plane_flag == 1)
//            H265Bs_PutBits(bs, sh->colour_plane_id, 2);
        if( !sh->IdrPicFlag ) {
            H265Bs_PutBits(bs, sh->slice_pic_order_cnt_lsb, sps->log2_max_pic_order_cnt_lsb);
            H265Bs_PutBit(bs, sh->short_term_ref_pic_set_sps_flag);
            if( !sh->short_term_ref_pic_set_sps_flag ) {
               PutShortTermRefPicSet(bs, sps->num_short_term_ref_pic_sets);
            }
            else if (sps->num_short_term_ref_pic_sets > 1){
                Ipp32s len = H265_CeilLog2(sps->num_short_term_ref_pic_sets);
                H265Bs_PutBits(bs, sh->short_term_ref_pic_set_idx, len);
            }
            if (sps->long_term_ref_pics_present_flag) {
                VM_ASSERT(0);
/*                H265Bs_PutVLCCode(bs, sh->num_long_term_pics);
                for (i = 0; i < sh->num_long_term_pics; i++) {
                    H265Bs_PutBits(bs, sh->delta_poc_lsb_lt[i], sps->log2_max_pic_order_cnt_lsb);
                    H265Bs_PutBit(bs, sh->delta_poc_msb_present_flag[i]);
                    if (sh->delta_poc_msb_present_flag[i])
                        H265Bs_PutVLCCode(bs, sh->delta_poc_msb_cycle_lt[i]);
                    H265Bs_PutBit(bs, sh->used_by_curr_pic_lt_flag[i]);
                }*/
            }
            if (sps->sps_temporal_mvp_enabled_flag)
                H265Bs_PutBit(bs, sh->slice_temporal_mvp_enabled_flag);
        }
        if (sps->sample_adaptive_offset_enabled_flag) {
            H265Bs_PutBit(bs, sh->slice_sao_luma_flag);
            H265Bs_PutBit(bs, sh->slice_sao_chroma_flag);
        }
        if (sh->slice_type == P_SLICE || sh->slice_type == B_SLICE) {
            H265Bs_PutBit(bs, sh->num_ref_idx_active_override_flag);
            if( sh->num_ref_idx_active_override_flag ) {
                H265Bs_PutVLCCode(bs, sh->num_ref_idx_l0_active - 1);
                if (sh->slice_type == B_SLICE)
                    H265Bs_PutVLCCode(bs, sh->num_ref_idx_l1_active - 1);
            }
            if (pps->lists_modification_present_flag) {
                //            ref_pic_list_modification( )
                VM_ASSERT(0);
            }
            if (sh->slice_type == B_SLICE)
                H265Bs_PutBit(bs, sh->mvd_l1_zero_flag);
            if (pps->cabac_init_present_flag)
                H265Bs_PutBit(bs, sh->cabac_init_flag);
            if (sh->slice_temporal_mvp_enabled_flag) {
                if (sh->slice_type == B_SLICE)
                    H265Bs_PutBit(bs, sh->collocated_from_l0_flag);
                Ipp32s collocated_from_l0_flag = sh->collocated_from_l0_flag;
                if (sh->slice_type != B_SLICE)
                    collocated_from_l0_flag = 1; // inferred
                if (((collocated_from_l0_flag && sh->num_ref_idx_l0_active > 1)  ||
                    (!collocated_from_l0_flag && sh->num_ref_idx_l1_active > 1)))
                    H265Bs_PutVLCCode(bs, sh->collocated_ref_idx);
            }
            if ((pps->weighted_pred_flag && sh->slice_type == P_SLICE)  ||
                (pps->weighted_bipred_flag && sh->slice_type == B_SLICE)) {
                    //            pred_weight_table( )
                    VM_ASSERT(0);
            }
            H265Bs_PutVLCCode(bs, sh->five_minus_max_num_merge_cand);
        }
        H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh->slice_qp_delta));
        if (pps->pps_slice_chroma_qp_offsets_present_flag) {
            H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh->slice_cb_qp_offset));
            H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh->slice_cr_qp_offset));
        }
        if (pps->deblocking_filter_override_enabled_flag) {
            H265Bs_PutBit(bs, sh->deblocking_filter_override_flag);
        }
        if (sh->deblocking_filter_override_flag) {
            H265Bs_PutBit(bs, sh->slice_deblocking_filter_disabled_flag);
            if (!sh->slice_deblocking_filter_disabled_flag ) {
                H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh->slice_beta_offset_div2));
                H265Bs_PutVLCCode(bs, SIGNED_VLC_CODE(sh->slice_tc_offset_div2));
            }
        }
        if (pps->pps_loop_filter_across_slices_enabled_flag &&
            (sh->slice_sao_luma_flag || sh->slice_sao_chroma_flag ||
            !sh->slice_deblocking_filter_disabled_flag  ) ) {
            H265Bs_PutBit(bs, sh->slice_loop_filter_across_slices_enabled_flag);
        }
    }
    if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag)
    {
        H265Bs_PutVLCCode(bs, sh->num_entry_point_offsets);
        if (sh->num_entry_point_offsets > 0) {
            Ipp32u max_len = 0;
            for (i = 0; i < sh->num_entry_point_offsets; i++) {
                if (max_len < sh->entry_point_offset[i])
                    max_len = sh->entry_point_offset[i];
            }
            sh->offset_len = H265_CeilLog2(max_len);
            H265Bs_PutVLCCode(bs, sh->offset_len - 1);
            for (i = 0; i < sh->num_entry_point_offsets; i++)
                H265Bs_PutBits(bs, sh->entry_point_offset[i] - 1, sh->offset_len);
        }
    }

    if (pps->slice_segment_header_extension_present_flag ) {
//        H265Bs_PutVLCCode(bs, sh->slice_header_extension_length);
        VM_ASSERT(0);
//        for (i = 0; i < slice_header_extension_length; i++)
//            slice_header_extension_data_byte
    }

    bs->PutBit(1);
    bs->ByteAlignWithZeros();
    return MFX_ERR_NONE;
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE