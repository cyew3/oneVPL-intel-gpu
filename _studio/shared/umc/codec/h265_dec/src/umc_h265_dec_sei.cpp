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

#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_headers.h"

namespace UMC_HEVC_DECODER
{

Ipp32s H265Bitstream::ParseSEI(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return sei_message(sps,current_sps,spl);
}

Ipp32s H265Bitstream::sei_message(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    Ipp32u code;
    Ipp32s payloadType = 0;

    ippiNextBits(m_pbs, m_bitOffset, 8, code);
    while (code  ==  0xFF)
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        ippiGetNBits(m_pbs, m_bitOffset, 8, code);
        payloadType += 255;
        ippiNextBits(m_pbs, m_bitOffset, 8, code);
    }

    Ipp32s last_payload_type_byte;    //Ipp32u integer using 8 bits
    ippiGetNBits(m_pbs, m_bitOffset, 8, last_payload_type_byte);

    payloadType += last_payload_type_byte;

    Ipp32s payloadSize = 0;

    ippiNextBits(m_pbs, m_bitOffset, 8, code);
    while( code  ==  0xFF )
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        ippiGetNBits(m_pbs, m_bitOffset, 8, code);
        payloadSize += 255;
        ippiNextBits(m_pbs, m_bitOffset, 8, code);
    }

    Ipp32s last_payload_size_byte;    //Ipp32u integer using 8 bits

    ippiGetNBits(m_pbs, m_bitOffset, 8, last_payload_size_byte);
    payloadSize += last_payload_size_byte;
    spl->Reset();
    spl->payLoadSize = payloadSize;

    if (payloadType < 0 || payloadType > SEI_RESERVED)
        payloadType = SEI_RESERVED;

    spl->payLoadType = (SEI_TYPE)payloadType;

    if (spl->payLoadSize > BytesLeft())
    {
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    Ipp32u * pbs;
    Ipp32u bitOffsetU;
    Ipp32s bitOffset;

    GetState(&pbs, &bitOffsetU);
    bitOffset = bitOffsetU;

    Ipp32s ret = sei_payload(sps, current_sps, spl);

    for (Ipp32u i = 0; i < spl->payLoadSize; i++)
    {
        ippiSkipNBits(pbs, bitOffset, 8);
    }

    SetState(pbs, bitOffset);

    return ret;
}

Ipp32s H265Bitstream::sei_payload(const HeaderSet<H265SeqParamSet> & sps,Ipp32s current_sps,H265SEIPayLoad *spl)
{
    Ipp32u payloadType =spl->payLoadType;
    switch( payloadType)
    {
    case SEI_BUFFERING_PERIOD_TYPE:
        return buffering_period(sps,current_sps,spl);
    case SEI_PIC_TIMING_TYPE:
        return pic_timing(sps,current_sps,spl);
    case SEI_PAN_SCAN_RECT_TYPE:
        return pan_scan_rect(sps,current_sps,spl);
    case SEI_FILLER_TYPE:
        return filler_payload(sps,current_sps,spl);
    case SEI_USER_DATA_REGISTERED_TYPE:
        return user_data_registered_itu_t_t35(sps,current_sps,spl);
    case SEI_USER_DATA_UNREGISTERED_TYPE:
        return user_data_unregistered(sps,current_sps,spl);
    case SEI_RECOVERY_POINT_TYPE:
        return recovery_point(sps,current_sps,spl);
    case SEI_SPARE_PIC_TYPE:
        return spare_pic(sps,current_sps,spl);
    case SEI_SCENE_INFO_TYPE:
        return scene_info(sps,current_sps,spl);
    case SEI_SUB_SEQ_INFO_TYPE:
        return sub_seq_info(sps,current_sps,spl);
    case SEI_SUB_SEQ_LAYER_TYPE:
        return sub_seq_layer_characteristics(sps,current_sps,spl);
    case SEI_SUB_SEQ_TYPE:
        return sub_seq_characteristics(sps,current_sps,spl);
    case SEI_FULL_FRAME_FREEZE_TYPE:
        return full_frame_freeze(sps,current_sps,spl);
    case SEI_FULL_FRAME_FREEZE_RELEASE_TYPE:
        return full_frame_freeze_release(sps,current_sps,spl);
    case SEI_FULL_FRAME_SNAPSHOT_TYPE:
        return full_frame_snapshot(sps,current_sps,spl);
    case SEI_PROGRESSIVE_REF_SEGMENT_START_TYPE:
        return progressive_refinement_segment_start(sps,current_sps,spl);
    case SEI_PROGRESSIVE_REF_SEGMENT_END_TYPE:
        return progressive_refinement_segment_end(sps,current_sps,spl);
    case SEI_MOTION_CONSTRAINED_SG_SET_TYPE:
        return motion_constrained_slice_group_set(sps,current_sps,spl);
    case SEI_SCALABILITY_INFO:
        scalability_info(spl);
        break;
    default:
        return reserved_sei_message(sps,current_sps,spl);
    }

    return current_sps;
/*
    Ipp32s i;
    Ipp32u code;
    for(i = 0; i < payloadSize; i++)
        ippiGetNBits(m_pbs, m_bitOffset, 8, code)*/


    //if( !byte_aligned( ) )
    //{
    //    bit_equal_to_one;  /* equal to 1 */
    //    while( !byte_aligned( ) )
    //       bit_equal_to_zero;  /* equal to 0 */
    //}
}

Ipp32s H265Bitstream::buffering_period(const HeaderSet<H265SeqParamSet> & , Ipp32s , H265SEIPayLoad *)
{
    Ipp32s seq_parameter_set_id = (Ipp8u) GetVLCElement(false);
/*
    const H265SeqParamSet *csps = sps.GetHeader(seq_parameter_set_id);
    H265SEIPayLoad::SEIMessages::BufferingPeriod &bps = spl->SEI_messages.buffering_period;

    // touch unreferenced parameters
    if (!csps)
    {
        return -1;
    }

    if (csps->nal_hrd_parameters_present_flag)
    {
        for(Ipp32s i=0;i<csps->cpb_cnt;i++)
        {
            bps.initial_cbp_removal_delay[0][i] = GetBits(csps->cpb_removal_delay_length);
            bps.initial_cbp_removal_delay_offset[0][i] = GetBits(csps->cpb_removal_delay_length);
        }

    }
    if (csps->vcl_hrd_parameters_present_flag)
    {
        for(Ipp32s i=0;i<csps->cpb_cnt;i++)
        {
            bps.initial_cbp_removal_delay[1][i] = GetBits(csps->cpb_removal_delay_length);
            bps.initial_cbp_removal_delay_offset[1][i] = GetBits(csps->cpb_removal_delay_length);
        }

    }

    AlignPointerRight();
*/
    return seq_parameter_set_id;
}

Ipp32s H265Bitstream::pic_timing(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *)
{
/*
    const Ipp8u NumClockTS[]={1,1,1,2,2,3,3,2,3};
    const H265SeqParamSet *csps = sps.GetHeader(current_sps);
    H265SEIPayLoad::SEIMessages::PicTiming &pts = spl->SEI_messages.pic_timing;

    if (!csps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (csps->nal_hrd_parameters_present_flag || csps->vcl_hrd_parameters_present_flag)
    {
        pts.cbp_removal_delay = GetBits(csps->cpb_removal_delay_length);
        pts.dpb_output_delay = GetBits(csps->dpb_output_delay_length);
    }
    else
    {
        pts.cbp_removal_delay = (Ipp32u)INVALID_DPB_DELAY_H265;
        pts.dpb_output_delay = (Ipp32u)INVALID_DPB_DELAY_H265;
    }

    if (csps->pic_struct_present_flag)
    {
        Ipp8u picStruct = (Ipp8u)GetBits(4);

        if (picStruct > 8)
            return UMC::UMC_ERR_INVALID_STREAM;

        pts.pic_struct = (DisplayPictureStruct_H265)picStruct;

        for (Ipp32s i = 0; i < NumClockTS[pts.pic_struct]; i++)
        {
            pts.clock_timestamp_flag[i] = (Ipp8u)Get1Bit();
            if (pts.clock_timestamp_flag[i])
            {
                pts.clock_timestamps[i].ct_type = (Ipp8u)GetBits(2);
                pts.clock_timestamps[i].nunit_field_based_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].counting_type = (Ipp8u)GetBits(5);
                pts.clock_timestamps[i].full_timestamp_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].discontinuity_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].cnt_dropped_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].n_frames = (Ipp8u)GetBits(8);

                if (pts.clock_timestamps[i].full_timestamp_flag)
                {
                    pts.clock_timestamps[i].seconds_value = (Ipp8u)GetBits(6);
                    pts.clock_timestamps[i].minutes_value = (Ipp8u)GetBits(6);
                    pts.clock_timestamps[i].hours_value = (Ipp8u)GetBits(5);
                }
                else
                {
                    if (Get1Bit())
                    {
                        pts.clock_timestamps[i].seconds_value = (Ipp8u)GetBits(6);
                        if (Get1Bit())
                        {
                            pts.clock_timestamps[i].minutes_value = (Ipp8u)GetBits(6);
                            if (Get1Bit())
                            {
                                pts.clock_timestamps[i].hours_value = (Ipp8u)GetBits(5);
                            }
                        }
                    }
                }

                if(csps->time_offset_length > 0)
                    pts.clock_timestamps[i].time_offset = (Ipp8u)GetBits(csps->time_offset_length);
            }
        }
    }

    AlignPointerRight();
*/
    return current_sps;
    //return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::pan_scan_rect(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::filler_payload(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::user_data_registered_itu_t_t35(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    H265SEIPayLoad::SEIMessages::UserDataRegistered * user_data = &(spl->SEI_messages.user_data_registered);
    Ipp32u code;
    ippiGetBits8(m_pbs, m_bitOffset, code);
    user_data->itu_t_t35_country_code = (Ipp8u)code;

    Ipp32u i = 1;

    user_data->itu_t_t35_country_code_extension_byte = 0;
    if (user_data->itu_t_t35_country_code == 0xff)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);
        user_data->itu_t_t35_country_code_extension_byte = (Ipp8u)code;
        i++;
    }

    spl->user_data.resize(spl->payLoadSize + 1);

    for(Ipp32s k = 0; i < spl->payLoadSize; i++, k++)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);
        spl->user_data[k] = (Ipp8u) code;
    }

    return current_sps;
}

Ipp32s H265Bitstream::user_data_unregistered(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::recovery_point(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    H265SEIPayLoad::SEIMessages::RecoveryPoint * recPoint = &(spl->SEI_messages.recovery_point);

    recPoint->recovery_frame_cnt = (Ipp8u)GetVLCElement(false);

    recPoint->exact_match_flag = (Ipp8u)Get1Bit();
    recPoint->broken_link_flag = (Ipp8u)Get1Bit();
    recPoint->changing_slice_group_idc = (Ipp8u)GetBits(2);

    if (recPoint->changing_slice_group_idc > 2)
        return -1;

    return current_sps;
}

Ipp32s H265Bitstream::spare_pic(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::scene_info(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::sub_seq_info(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::sub_seq_layer_characteristics(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::sub_seq_characteristics(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::full_frame_freeze(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::full_frame_freeze_release(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::full_frame_snapshot(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::progressive_refinement_segment_start(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::progressive_refinement_segment_end(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::motion_constrained_slice_group_set(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::reserved_sei_message(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::unparsed_sei_message(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    Ipp32u i;
    for(i = 0; i < spl->payLoadSize; i++)
        ippiSkipNBits(m_pbs, m_bitOffset, 8)
    AlignPointerRight();
    return current_sps;
}


typedef struct
{
    Ipp32u  layer_id;
    Ipp8u   priority_id;
    Ipp8u   discardable_flag;
    Ipp8u   dependency_id;
    Ipp8u   quality_id;
    Ipp8u   temporal_id;
} scalability_layer_info;

typedef struct
{

    Ipp8u temporal_id_nesting_flag;
    Ipp8u priority_layer_info_present_flag;
    Ipp8u priority_id_setting_flag;

    scalability_layer_info layer[1024];
} scalability_info_struct;

#pragma warning(disable : 4189)
void H265Bitstream::scalability_info(H265SEIPayLoad *spl)
{
    scalability_info_struct bb;
    scalability_info_struct *xxxx = &bb;
    xxxx->temporal_id_nesting_flag = (Ipp8u)Get1Bit();
    xxxx->priority_layer_info_present_flag = (Ipp8u)Get1Bit();
    xxxx->priority_id_setting_flag = (Ipp8u)Get1Bit();

    spl->SEI_messages.scalability_info.num_layers = GetVLCElement(false) + 1;

    for (Ipp32u i = 0; i < spl->SEI_messages.scalability_info.num_layers; i++)
    {
        xxxx->layer[i].layer_id = GetVLCElement(false);
        xxxx->layer[i].priority_id = (Ipp8u)GetBits(6);
        xxxx->layer[i].discardable_flag = (Ipp8u)Get1Bit();
        xxxx->layer[i].dependency_id = (Ipp8u)GetBits(3);
        xxxx->layer[i].quality_id = (Ipp8u)GetBits(4);
        xxxx->layer[i].temporal_id = (Ipp8u)GetBits(3);

        Ipp8u   sub_pic_layer_flag = (Ipp8u)Get1Bit();
        Ipp8u   sub_region_layer_flag = (Ipp8u)Get1Bit();
        Ipp8u   iroi_division_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   profile_level_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   bitrate_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   frm_rate_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   frm_size_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   layer_dependency_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   parameter_sets_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   bitstream_restriction_info_present_flag = (Ipp8u)Get1Bit();
        Ipp8u   exact_inter_layer_pred_flag = (Ipp8u)Get1Bit();

        if (sub_pic_layer_flag || iroi_division_info_present_flag)
        {
            Ipp8u   exact_sample_value_match_flag = (Ipp8u)Get1Bit();
        }

        Ipp8u   layer_conversion_flag = (Ipp8u)Get1Bit();
        Ipp8u   layer_output_flag = (Ipp8u)Get1Bit();

        if (profile_level_info_present_flag)
        {
            Ipp32u   layer_profile_level_idc = (Ipp8u)GetBits(24);
        }

        if (bitrate_info_present_flag)
        {
            Ipp32u  avg_bitrate = GetBits(16);
            Ipp32u  max_bitrate_layer = GetBits(16);
            Ipp32u  max_bitrate_layer_representation = GetBits(16);
            Ipp32u  max_bitrate_calc_window = GetBits(16);
        }

        if (frm_rate_info_present_flag)
        {
            Ipp32u  constant_frm_rate_idc = GetBits(2);
            Ipp32u  avg_frm_rate = GetBits(16);
        }

        if (frm_size_info_present_flag || iroi_division_info_present_flag)
        {
            Ipp32u  frm_width_in_mbs_minus1 = GetVLCElement(false);
            Ipp32u  frm_height_in_mbs_minus1 = GetVLCElement(false);
        }

        if (sub_region_layer_flag)
        {
            Ipp32u base_region_layer_id = GetVLCElement(false);
            Ipp8u dynamic_rect_flag = (Ipp8u)Get1Bit();
            if (!dynamic_rect_flag)
            {
                Ipp32u horizontal_offset = GetBits(16);
                Ipp32u vertical_offset = GetBits(16);
                Ipp32u region_width = GetBits(16);
                Ipp32u region_height = GetBits(16);
            }
        }

        if(sub_pic_layer_flag)
        {
            Ipp32u roi_id = GetVLCElement(false);
        }

        if (iroi_division_info_present_flag)
        {
            Ipp8u iroi_grid_flag = (Ipp8u)Get1Bit();
            if (iroi_grid_flag)
            {
                Ipp32u grid_width_in_mbs_minus1 = GetVLCElement(false);
                Ipp32u grid_height_in_mbs_minus1 = GetVLCElement(false);
            } else {
                Ipp32u num_rois_minus1 = GetVLCElement(false);
                for (Ipp32u j = 0; j <= num_rois_minus1; j++)
                {
                    Ipp32u first_mb_in_roi = GetVLCElement(false);
                    Ipp32u roi_width_in_mbs_minus1 = GetVLCElement(false);
                    Ipp32u roi_height_in_mbs_minus1 = GetVLCElement(false);
                }
            }
        }

        if (layer_dependency_info_present_flag)
        {
            Ipp32u num_directly_dependent_layers = GetVLCElement(false);
            for(Ipp32u j = 0; j < num_directly_dependent_layers; j++)
            {
                Ipp32u directly_dependent_layer_id_delta_minus1 = GetVLCElement(false);
            }
        }
        else
        {
            Ipp32u layer_dependency_info_src_layer_id_delta = GetVLCElement(false);
        }

        if (parameter_sets_info_present_flag)
        {
            Ipp32u num_seq_parameter_set_minus1 = GetVLCElement(false);

            for(Ipp32u j = 0; j <= num_seq_parameter_set_minus1; j++ )
                Ipp32u seq_parameter_set_id_delta = GetVLCElement(false);

            Ipp32u num_subset_seq_parameter_set_minus1 = GetVLCElement(false);
            for(Ipp32u j = 0; j <= num_subset_seq_parameter_set_minus1; j++ )
                Ipp32u subset_seq_parameter_set_id_delta = GetVLCElement(false);

            Ipp32u num_pic_parameter_set_minus1 = GetVLCElement(false);
            for(Ipp32u j = 0; j <= num_pic_parameter_set_minus1; j++)
                Ipp32u pic_parameter_set_id_delta = GetVLCElement(false);
        }
        else
        {
            Ipp32u parameter_sets_info_src_layer_id_delta = GetVLCElement(false);
        }

        if (bitstream_restriction_info_present_flag)
        {
            Ipp8u motion_vectors_over_pic_boundaries_flag = (Ipp8u)Get1Bit();
            Ipp32u max_bytes_per_pic_denom = GetVLCElement(false);
            Ipp32u max_bits_per_mb_denom = GetVLCElement(false);
            Ipp32u log2_max_mv_length_horizontal = GetVLCElement(false);
            Ipp32u log2_max_mv_length_vertical = GetVLCElement(false);
            Ipp32u num_reorder_frames = GetVLCElement(false);
            Ipp32u max_dec_frame_buffering = GetVLCElement(false);
        }

        if (layer_conversion_flag)
        {
            Ipp32u conversion_type_idc = GetVLCElement(false);
            for(Ipp32u j = 0; j < 2; j++)
            {
                Ipp8u rewriting_info_flag = (Ipp8u)Get1Bit();
                if (rewriting_info_flag)
                {
                    Ipp32u rewriting_profile_level_idc = GetBits(24);
                    Ipp32u rewriting_avg_bitrate = GetBits(16);
                    Ipp32u rewriting_max_bitrate = GetBits(16);
                }
            }
        }
    } // for 0..num_layers

    if (xxxx->priority_layer_info_present_flag)
    {
        Ipp32u pr_num_dId_minus1 = GetVLCElement(false);
        for(Ipp32u i = 0; i <= pr_num_dId_minus1; i++)
        {
            Ipp32u pr_dependency_id = GetBits(3);
            Ipp32u pr_num_minus1 = GetVLCElement(false);
            for(Ipp32u j = 0; j <= pr_num_minus1; j++)
            {
                Ipp32u pr_id = GetVLCElement(false);
                Ipp32u pr_profile_level_idc = GetBits(24);
                Ipp32u pr_avg_bitrate = GetBits(16);
                Ipp32u pr_max_bitrate = GetBits(16);
            }
        }
    }

    if (xxxx->priority_id_setting_flag)
    {
        std::vector<char> priority_id_setting_uri; // it is string
        Ipp32u PriorityIdSettingUriIdx = 0;
        do
        {
            priority_id_setting_uri.push_back(1);
            priority_id_setting_uri[PriorityIdSettingUriIdx] = (Ipp8u)GetBits(8);
        } while (priority_id_setting_uri[PriorityIdSettingUriIdx++] != 0);
    }
}

#pragma warning(default : 4189)

}//namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
