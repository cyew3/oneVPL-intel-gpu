/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_bitstream.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_headers.h"

namespace UMC
{

const H264SeqParamSet *GetSeqParams(const Headers & headers, Ipp32s seq_parameter_set_id)
{
    if (seq_parameter_set_id == -1)
        return 0;

    const H264SeqParamSet *csps = headers.m_SeqParams.GetHeader(seq_parameter_set_id);
    if (csps)
        return csps;

    csps = headers.m_SeqParamsMvcExt.GetHeader(seq_parameter_set_id);
    if (csps)
        return csps;

    csps = headers.m_SeqParamsSvcExt.GetHeader(seq_parameter_set_id);
    if (csps)
        return csps;

    return 0;
}

Ipp32s H264Bitstream::ParseSEI(const Headers & headers, H264SEIPayLoad *spl)
{
    Ipp32s current_sps = headers.m_SeqParams.GetCurrentID();
    return sei_message(headers, current_sps, spl);
}

Ipp32s H264Bitstream::sei_message(const Headers & headers, Ipp32s current_sps, H264SEIPayLoad *spl)
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
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    }

    Ipp32u * pbs;
    Ipp32u bitOffsetU;
    Ipp32s bitOffset;

    GetState(&pbs, &bitOffsetU);
    bitOffset = bitOffsetU;

    CheckBSLeft(spl->payLoadSize);

    spl->isValid = 1;
    Ipp32s ret = sei_payload(headers, current_sps, spl);

    for (Ipp32u i = 0; i < spl->payLoadSize; i++)
    {
        ippiSkipNBits(pbs, bitOffset, 8);
    }

    SetState(pbs, bitOffset);
    return ret;
}

Ipp32s H264Bitstream::sei_payload(const Headers & headers, Ipp32s current_sps,H264SEIPayLoad *spl)
{
    Ipp32u payloadType =spl->payLoadType;
    switch( payloadType)
    {
    case SEI_BUFFERING_PERIOD_TYPE:
        buffering_period(headers, current_sps,spl);
        break;
    case SEI_PIC_TIMING_TYPE:
        pic_timing(headers,current_sps,spl);
        break;
    case SEI_DEC_REF_PIC_MARKING_TYPE:
        dec_ref_pic_marking_repetition(headers,current_sps,spl);
        break;

    case SEI_USER_DATA_REGISTERED_TYPE:
        user_data_registered_itu_t_t35(spl);
        break;
    case SEI_RECOVERY_POINT_TYPE:
        recovery_point(spl);
        break;

    case SEI_SCALABILITY_INFO:
        scalability_info(spl);
        break;
    case SEI_SCALABLE_NESTING:
        scalable_nesting(spl);
        break;

    case SEI_USER_DATA_UNREGISTERED_TYPE:
    case SEI_PAN_SCAN_RECT_TYPE:
    case SEI_FILLER_TYPE:
    case SEI_SPARE_PIC_TYPE:
    case SEI_SCENE_INFO_TYPE:
    case SEI_SUB_SEQ_INFO_TYPE:
    case SEI_SUB_SEQ_LAYER_TYPE:
    case SEI_SUB_SEQ_TYPE:
    case SEI_FULL_FRAME_FREEZE_TYPE:
    case SEI_FULL_FRAME_FREEZE_RELEASE_TYPE:
    case SEI_FULL_FRAME_SNAPSHOT_TYPE:
    case SEI_PROGRESSIVE_REF_SEGMENT_START_TYPE:
    case SEI_PROGRESSIVE_REF_SEGMENT_END_TYPE:
    case SEI_MOTION_CONSTRAINED_SG_SET_TYPE:
    default:
        unparsed_sei_message(spl);
        break;
    }

    return current_sps;
}

Ipp32s H264Bitstream::buffering_period(const Headers & headers, Ipp32s , H264SEIPayLoad *spl)
{
    Ipp32s seq_parameter_set_id = (Ipp8u) GetVLCElement(false);
    const H264SeqParamSet *csps = GetSeqParams(headers, seq_parameter_set_id);
    H264SEIPayLoad::SEIMessages::BufferingPeriod &bps = spl->SEI_messages.buffering_period;

    // touch unreferenced parameters
    if (!csps)
    {
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    }

    if (csps->vui.nal_hrd_parameters_present_flag)
    {
        if (csps->vui.cpb_cnt >= 32)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

        for(Ipp32s i=0; i < csps->vui.cpb_cnt; i++)
        {
            bps.initial_cpb_removal_delay[0][i] = GetBits(csps->vui.initial_cpb_removal_delay_length);
            bps.initial_cpb_removal_delay_offset[0][i] = GetBits(csps->vui.initial_cpb_removal_delay_length);
        }
    }

    if (csps->vui.vcl_hrd_parameters_present_flag)
    {
        if (csps->vui.cpb_cnt >= 32)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

        for(Ipp32s i=0; i < csps->vui.cpb_cnt; i++)
        {
            bps.initial_cpb_removal_delay[1][i] = GetBits(csps->vui.cpb_removal_delay_length);
            bps.initial_cpb_removal_delay_offset[1][i] = GetBits(csps->vui.cpb_removal_delay_length);
        }
    }

    AlignPointerRight();
    return seq_parameter_set_id;
}

Ipp32s H264Bitstream::pic_timing(const Headers & headers, Ipp32s current_sps, H264SEIPayLoad *spl)
{
    if (current_sps == -1)
        throw h264_exception(UMC_ERR_INVALID_STREAM);

    const Ipp8u NumClockTS[]={1,1,1,2,2,3,3,2,3};
    const H264SeqParamSet *csps = GetSeqParams(headers, current_sps);
    H264SEIPayLoad::SEIMessages::PicTiming &pts = spl->SEI_messages.pic_timing;

    if (!csps)
        throw h264_exception(UMC_ERR_INVALID_STREAM);

    if (csps->vui.nal_hrd_parameters_present_flag || csps->vui.vcl_hrd_parameters_present_flag)
    {
        pts.cbp_removal_delay = GetBits(csps->vui.cpb_removal_delay_length);
        pts.dpb_output_delay = GetBits(csps->vui.dpb_output_delay_length);
    }
    else
    {
        pts.cbp_removal_delay = INVALID_DPB_OUTPUT_DELAY;
        pts.dpb_output_delay = INVALID_DPB_OUTPUT_DELAY;
    }

    if (csps->vui.pic_struct_present_flag)
    {
        Ipp8u picStruct = (Ipp8u)GetBits(4);

        if (picStruct > 8)
            return UMC_ERR_INVALID_STREAM;

        pts.pic_struct = (DisplayPictureStruct)picStruct;

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

                if (csps->vui.time_offset_length > 0)
                    pts.clock_timestamps[i].time_offset = (Ipp8u)GetBits(csps->vui.time_offset_length);
            }
        }
    }

    AlignPointerRight();
    return current_sps;
}

void H264Bitstream::user_data_registered_itu_t_t35(H264SEIPayLoad *spl)
{
    H264SEIPayLoad::SEIMessages::UserDataRegistered * user_data = &(spl->SEI_messages.user_data_registered);
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
}

void H264Bitstream::recovery_point(H264SEIPayLoad *spl)
{
    H264SEIPayLoad::SEIMessages::RecoveryPoint * recPoint = &(spl->SEI_messages.recovery_point);

    recPoint->recovery_frame_cnt = (Ipp8u)GetVLCElement(false);

    recPoint->exact_match_flag = (Ipp8u)Get1Bit();
    recPoint->broken_link_flag = (Ipp8u)Get1Bit();
    recPoint->changing_slice_group_idc = (Ipp8u)GetBits(2);

    if (recPoint->changing_slice_group_idc > 2)
    {
        spl->isValid = 0;
    }
}

Ipp32s H264Bitstream::dec_ref_pic_marking_repetition(const Headers & headers, Ipp32s current_sps, H264SEIPayLoad *spl)
{
    if (current_sps == -1)
        throw h264_exception(UMC_ERR_INVALID_STREAM);

    const H264SeqParamSet *csps = GetSeqParams(headers, current_sps);
    if (!csps)
        throw h264_exception(UMC_ERR_INVALID_STREAM);

    spl->SEI_messages.dec_ref_pic_marking_repetition.original_idr_flag = (Ipp8u)Get1Bit();
    spl->SEI_messages.dec_ref_pic_marking_repetition.original_frame_num = (Ipp8u)GetVLCElement(false);

    if (!csps->frame_mbs_only_flag)
    {
        spl->SEI_messages.dec_ref_pic_marking_repetition.original_field_pic_flag = (Ipp8u)Get1Bit();

        if (spl->SEI_messages.dec_ref_pic_marking_repetition.original_field_pic_flag)
        {
            spl->SEI_messages.dec_ref_pic_marking_repetition.original_bottom_field_flag = (Ipp8u)Get1Bit();
        }
    }

    H264SliceHeader hdr;
    memset(&hdr, 0, sizeof(H264SliceHeader));

    hdr.IdrPicFlag = spl->SEI_messages.dec_ref_pic_marking_repetition.original_idr_flag;

    Status sts = DecRefPicMarking(&hdr, &spl->SEI_messages.dec_ref_pic_marking_repetition.adaptiveMarkingInfo);
    if (sts != UMC_OK)
        throw h264_exception(UMC_ERR_INVALID_STREAM);

    spl->SEI_messages.dec_ref_pic_marking_repetition.long_term_reference_flag = hdr.long_term_reference_flag;
    return current_sps;
}

void H264Bitstream::unparsed_sei_message(H264SEIPayLoad *spl)
{
    for(Ipp32u i = 0; i < spl->payLoadSize; i++)
        ippiSkipNBits(m_pbs, m_bitOffset, 8)
    AlignPointerRight();
}

#pragma warning(disable : 4189)
void H264Bitstream::scalability_info(H264SEIPayLoad *spl)
{
    spl->SEI_messages.scalability_info.temporal_id_nesting_flag = (Ipp8u)Get1Bit();
    spl->SEI_messages.scalability_info.priority_layer_info_present_flag = (Ipp8u)Get1Bit();
    spl->SEI_messages.scalability_info.priority_id_setting_flag = (Ipp8u)Get1Bit();

    spl->SEI_messages.scalability_info.num_layers = GetVLCElement(false) + 1;

    if (spl->SEI_messages.scalability_info.num_layers > 1024)
        throw h264_exception(UMC_ERR_INVALID_STREAM);

    spl->user_data.resize(sizeof(scalability_layer_info) * spl->SEI_messages.scalability_info.num_layers);
    scalability_layer_info * layers = (scalability_layer_info *) (&spl->user_data[0]);

    for (Ipp32u i = 0; i < spl->SEI_messages.scalability_info.num_layers; i++)
    {
        layers[i].layer_id = GetVLCElement(false);
        layers[i].priority_id = (Ipp8u)GetBits(6);
        layers[i].discardable_flag = (Ipp8u)Get1Bit();
        layers[i].dependency_id = (Ipp8u)GetBits(3);
        layers[i].quality_id = (Ipp8u)GetBits(4);
        layers[i].temporal_id = (Ipp8u)GetBits(3);

        Ipp8u   sub_pic_layer_flag = (Ipp8u)Get1Bit();  // Need to check
        Ipp8u   sub_region_layer_flag = (Ipp8u)Get1Bit(); // Need to check

        //if (sub_pic_layer_flag && !sub_region_layer_flag)
          //  throw h264_exception(UMC_ERR_INVALID_STREAM);

        Ipp8u   iroi_division_info_present_flag = (Ipp8u)Get1Bit(); // Need to check

        //if (sub_pic_layer_flag && iroi_division_info_present_flag)
          //  throw h264_exception(UMC_ERR_INVALID_STREAM);

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
            layers[i].constant_frm_rate_idc = GetBits(2);
            layers[i].avg_frm_rate = GetBits(16);
        }

        if (frm_size_info_present_flag || iroi_division_info_present_flag)
        {
            layers[i].frm_width_in_mbs = GetVLCElement(false) + 1;
            layers[i].frm_height_in_mbs = GetVLCElement(false) + 1;
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
                Ipp32s num_rois_minus1 = GetVLCElement(false);
                Ipp32s FrmSizeInMbs = layers[i].frm_height_in_mbs * layers[i].frm_width_in_mbs;

                if (num_rois_minus1 < 0 || num_rois_minus1 > FrmSizeInMbs)
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                for (Ipp32s j = 0; j <= num_rois_minus1; j++)
                {
                    Ipp32u first_mb_in_roi = GetVLCElement(false);
                    Ipp32u roi_width_in_mbs_minus1 = GetVLCElement(false);
                    Ipp32u roi_height_in_mbs_minus1 = GetVLCElement(false);
                }
            }
        }

        if (layer_dependency_info_present_flag)
        {
            layers[i].num_directly_dependent_layers = GetVLCElement(false);

            if (layers[i].num_directly_dependent_layers > 255)
                throw h264_exception(UMC_ERR_INVALID_STREAM);

            for(Ipp32u j = 0; j < layers[i].num_directly_dependent_layers; j++)
            {
                layers[i].directly_dependent_layer_id_delta_minus1[j] = GetVLCElement(false);
            }
        }
        else
        {
            layers[i].layer_dependency_info_src_layer_id_delta = GetVLCElement(false);
        }

        if (parameter_sets_info_present_flag)
        {
            Ipp32s num_seq_parameter_set_minus1 = GetVLCElement(false);
            if (num_seq_parameter_set_minus1 < 0 || num_seq_parameter_set_minus1 > 32)
                throw h264_exception(UMC_ERR_INVALID_STREAM);

            for(Ipp32s j = 0; j <= num_seq_parameter_set_minus1; j++ )
                Ipp32u seq_parameter_set_id_delta = GetVLCElement(false);

            Ipp32s num_subset_seq_parameter_set_minus1 = GetVLCElement(false);
            if (num_subset_seq_parameter_set_minus1 < 0 || num_subset_seq_parameter_set_minus1 > 32)
                throw h264_exception(UMC_ERR_INVALID_STREAM);

            for(Ipp32s j = 0; j <= num_subset_seq_parameter_set_minus1; j++ )
                Ipp32u subset_seq_parameter_set_id_delta = GetVLCElement(false);

            Ipp32s num_pic_parameter_set_minus1 = GetVLCElement(false);
            if (num_pic_parameter_set_minus1 < 0 || num_pic_parameter_set_minus1 > 255)
                throw h264_exception(UMC_ERR_INVALID_STREAM);

            for(Ipp32s j = 0; j <= num_pic_parameter_set_minus1; j++)
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

    if (spl->SEI_messages.scalability_info.priority_layer_info_present_flag)
    {
        Ipp32u pr_num_dId_minus1 = GetVLCElement(false);
        for(Ipp32u i = 0; i <= pr_num_dId_minus1; i++)
        {
            Ipp32u pr_dependency_id = GetBits(3);
            Ipp32s pr_num_minus1 = GetVLCElement(false);

            if (pr_num_minus1 < 0 || pr_num_minus1 > 63)
                throw h264_exception(UMC_ERR_INVALID_STREAM);

            for(Ipp32s j = 0; j <= pr_num_minus1; j++)
            {
                Ipp32u pr_id = GetVLCElement(false);
                Ipp32u pr_profile_level_idc = GetBits(24);
                Ipp32u pr_avg_bitrate = GetBits(16);
                Ipp32u pr_max_bitrate = GetBits(16);
            }
        }
    }

    if (spl->SEI_messages.scalability_info.priority_id_setting_flag)
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

typedef struct
{
    Ipp8u   all_layer_representations_in_au_flag;
    Ipp32u  num_layer_representations;
    Ipp8u   sei_dependency_id[128];
    Ipp8u   sei_quality_id[128];
    Ipp8u   sei_temporal_id;
} scalable_nesting_struct;

void H264Bitstream::scalable_nesting(H264SEIPayLoad *)
{
    scalable_nesting_struct scl_nest;

    scl_nest.all_layer_representations_in_au_flag = (Ipp8u)Get1Bit();

    if (!scl_nest.all_layer_representations_in_au_flag)
    {
        scl_nest.num_layer_representations = GetVLCElement(false) + 1;

        if (scl_nest.num_layer_representations > 127)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

        for (Ipp32u i = 0; i < scl_nest.num_layer_representations; ++i)
        {
            scl_nest.sei_dependency_id[i] = (Ipp8u)GetBits(3);
            scl_nest.sei_quality_id[i] = (Ipp8u)GetBits(4);
        }

        scl_nest.sei_temporal_id = (Ipp8u)GetBits(4);
    }

    AlignPointerRight();
    //sei_payload(const HeaderSet<H264SeqParamSet> & sps, 0, spl)
}

}//namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
