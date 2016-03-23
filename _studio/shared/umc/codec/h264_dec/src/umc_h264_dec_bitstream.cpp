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

#include "vm_debug.h"
#include "umc_h264_dec.h"
#include "umc_h264_bitstream.h"
#include "umc_h264_dec_coeff_token_map.h"
#include "umc_h264_dec_total_zero.h"
#include "umc_h264_dec_run_before.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_dec_ippwrap.h"

#include <limits.h>

#define SCLFLAT16     0
#define SCLDEFAULT    1
#define SCLREDEFINED  2

namespace UMC
{
H264BaseBitstream::H264BaseBitstream()
{
    Reset(0, 0);
}

H264BaseBitstream::H264BaseBitstream(Ipp8u * const pb, const Ipp32u maxsize)
{
    Reset(pb, maxsize);
}

H264BaseBitstream::~H264BaseBitstream()
{
}

void H264BaseBitstream::Reset(Ipp8u * const pb, const Ipp32u maxsize)
{
    m_pbs       = (Ipp32u*)pb;
    m_pbsBase   = (Ipp32u*)pb;
    m_bitOffset = 31;
    m_maxBsSize    = maxsize;

} // void Reset(Ipp8u * const pb, const Ipp32u maxsize)

void H264BaseBitstream::Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize)
{
    m_pbs       = (Ipp32u*)pb;
    m_pbsBase   = (Ipp32u*)pb;
    m_bitOffset = offset;
    m_maxBsSize = maxsize;

} // void Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize)


bool H264BaseBitstream::More_RBSP_Data()
{
    Ipp32s code, tmp;
    Ipp32u* ptr_state = m_pbs;
    Ipp32s  bit_state = m_bitOffset;

    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    Ipp32s remaining_bytes = (Ipp32s)BytesLeft();

    if (remaining_bytes <= 0)
        return false;

    // get top bit, it can be "rbsp stop" bit
    ippiGetNBits(m_pbs, m_bitOffset, 1, code);

    // get remain bits, which is less then byte
    tmp = (m_bitOffset + 1) % 8;

    if(tmp)
    {
        ippiGetNBits(m_pbs, m_bitOffset, tmp, code);
        if ((code << (8 - tmp)) & 0x7f)    // most sig bit could be rbsp stop bit
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }
    }

    remaining_bytes = (Ipp32s)BytesLeft();

    // run through remain bytes
    while (0 < remaining_bytes)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);

        if (code)
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }

        remaining_bytes -= 1;
    }

    return false;
}

H264HeadersBitstream::H264HeadersBitstream()
    : H264BaseBitstream()
{
}

H264HeadersBitstream::H264HeadersBitstream(Ipp8u * const pb, const Ipp32u maxsize)
    : H264BaseBitstream(pb, maxsize)
{
}

// ---------------------------------------------------------------------------
//  H264Bitstream::GetSequenceParamSet()
//    Read sequence parameter set data from bitstream.
// ---------------------------------------------------------------------------
Status H264HeadersBitstream::GetSequenceParamSet(H264SeqParamSet *sps)
{
    // Not all members of the seq param set structure are contained in all
    // seq param sets. So start by init all to zero.
    Status ps = UMC_OK;
    sps->Reset();

    // profile
    // TBD: add rejection of unsupported profile
    sps->profile_idc = (Ipp8u)GetBits(8);

    switch (sps->profile_idc)
    {
    case H264VideoDecoderParams::H264_PROFILE_BASELINE:
    case H264VideoDecoderParams::H264_PROFILE_MAIN:
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
    case H264VideoDecoderParams::H264_PROFILE_EXTENDED:
    case H264VideoDecoderParams::H264_PROFILE_HIGH:
    case H264VideoDecoderParams::H264_PROFILE_HIGH10:
    case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
    case H264VideoDecoderParams::H264_PROFILE_HIGH422:
    case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:
    case H264VideoDecoderParams::H264_PROFILE_HIGH444:
    case H264VideoDecoderParams::H264_PROFILE_ADVANCED444_INTRA:
    case H264VideoDecoderParams::H264_PROFILE_ADVANCED444:
    case H264VideoDecoderParams::H264_PROFILE_HIGH444_PRED:
    case H264VideoDecoderParams::H264_PROFILE_CAVLC444_INTRA:
        break;
    default:
        return UMC_ERR_INVALID_STREAM;
    }

    sps->constraint_set0_flag = Get1Bit();
    sps->constraint_set1_flag = Get1Bit();
    sps->constraint_set2_flag = Get1Bit();
    sps->constraint_set3_flag = Get1Bit();
    sps->constraint_set4_flag = Get1Bit();
    sps->constraint_set5_flag = Get1Bit();

    // skip 2 zero bits
    GetBits(2);

    sps->level_idc = (Ipp8u)GetBits(8);

    if (sps->level_idc == H264VideoDecoderParams::H264_LEVEL_UNKNOWN)
        sps->level_idc = H264VideoDecoderParams::H264_LEVEL_52;

    switch(sps->level_idc)
    {
    case H264VideoDecoderParams::H264_LEVEL_1:
    case H264VideoDecoderParams::H264_LEVEL_11:
    case H264VideoDecoderParams::H264_LEVEL_12:
    case H264VideoDecoderParams::H264_LEVEL_13:

    case H264VideoDecoderParams::H264_LEVEL_2:
    case H264VideoDecoderParams::H264_LEVEL_21:
    case H264VideoDecoderParams::H264_LEVEL_22:

    case H264VideoDecoderParams::H264_LEVEL_3:
    case H264VideoDecoderParams::H264_LEVEL_31:
    case H264VideoDecoderParams::H264_LEVEL_32:

    case H264VideoDecoderParams::H264_LEVEL_4:
    case H264VideoDecoderParams::H264_LEVEL_41:
    case H264VideoDecoderParams::H264_LEVEL_42:

    case H264VideoDecoderParams::H264_LEVEL_5:
    case H264VideoDecoderParams::H264_LEVEL_51:
    case H264VideoDecoderParams::H264_LEVEL_52:
        break;
    case H264VideoDecoderParams::H264_LEVEL_9:
        if (sps->profile_idc != H264VideoDecoderParams::H264_PROFILE_BASELINE &&
            sps->profile_idc != H264VideoDecoderParams::H264_PROFILE_MAIN &&
            sps->profile_idc != H264VideoDecoderParams::H264_PROFILE_EXTENDED) {
                sps->level_idc = H264VideoDecoderParams::H264_LEVEL_1b;
                break;
        }
    default:
        return UMC_ERR_INVALID_STREAM;
    }

    // id
    Ipp32s sps_id = GetVLCElement(false);
    if (sps_id > (Ipp32s)(MAX_NUM_SEQ_PARAM_SETS - 1))
    {
        return UMC_ERR_INVALID_STREAM;
    }

    sps->seq_parameter_set_id = (Ipp8u)sps_id;

    // see 7.3.2.1.1 "Sequence parameter set data syntax"
    // chapter of H264 standard for full list of profiles with chrominance
    if ((H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_HIGH == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_HIGH10 == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_HIGH422 == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH == sps->profile_idc) ||
        /* what're these profiles ??? */
        (H264VideoDecoderParams::H264_PROFILE_HIGH444 == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_HIGH444_PRED == sps->profile_idc) ||
        (H264VideoDecoderParams::H264_PROFILE_CAVLC444_INTRA == sps->profile_idc))
    {
        Ipp32u chroma_format_idc = GetVLCElement(false);

        if (chroma_format_idc > 3)
            return UMC_ERR_INVALID_STREAM;

        sps->chroma_format_idc = (Ipp8u)chroma_format_idc;
        if (sps->chroma_format_idc==3)
        {
            sps->residual_colour_transform_flag = Get1Bit();
        }

        Ipp32u bit_depth_luma = GetVLCElement(false) + 8;
        Ipp32u bit_depth_chroma = GetVLCElement(false) + 8;

        if (bit_depth_luma > 16 || bit_depth_chroma > 16)
            return UMC_ERR_INVALID_STREAM;

        sps->bit_depth_luma = (Ipp8u)bit_depth_luma;
        sps->bit_depth_chroma = (Ipp8u)bit_depth_chroma;

        if (!chroma_format_idc)
            sps->bit_depth_chroma = sps->bit_depth_luma;

        VM_ASSERT(!sps->residual_colour_transform_flag);
        if (sps->residual_colour_transform_flag == 1)
        {
            return UMC_ERR_INVALID_STREAM;
        }

        sps->qpprime_y_zero_transform_bypass_flag = Get1Bit();
        sps->seq_scaling_matrix_present_flag = Get1Bit();
        if(sps->seq_scaling_matrix_present_flag)
        {
            // 0
            if(Get1Bit())
            {
                GetScalingList4x4(&sps->ScalingLists4x4[0],(Ipp8u*)default_intra_scaling_list4x4,&sps->type_of_scaling_list_used[0]);
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[0],(Ipp8u*) default_intra_scaling_list4x4);
                sps->type_of_scaling_list_used[0] = SCLDEFAULT;
            }
            // 1
            if(Get1Bit())
            {
                GetScalingList4x4(&sps->ScalingLists4x4[1],(Ipp8u*) default_intra_scaling_list4x4,&sps->type_of_scaling_list_used[1]);
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[1],(Ipp8u*) sps->ScalingLists4x4[0].ScalingListCoeffs);
                sps->type_of_scaling_list_used[1] = SCLDEFAULT;
            }
            // 2
            if(Get1Bit())
            {
                GetScalingList4x4(&sps->ScalingLists4x4[2],(Ipp8u*) default_intra_scaling_list4x4,&sps->type_of_scaling_list_used[2]);
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[2],(Ipp8u*) sps->ScalingLists4x4[1].ScalingListCoeffs);
                sps->type_of_scaling_list_used[2] = SCLDEFAULT;
            }
            // 3
            if(Get1Bit())
            {
                GetScalingList4x4(&sps->ScalingLists4x4[3],(Ipp8u*)default_inter_scaling_list4x4,&sps->type_of_scaling_list_used[3]);
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[3],(Ipp8u*) default_inter_scaling_list4x4);
                sps->type_of_scaling_list_used[3] = SCLDEFAULT;
            }
            // 4
            if(Get1Bit())
            {
                GetScalingList4x4(&sps->ScalingLists4x4[4],(Ipp8u*) default_inter_scaling_list4x4,&sps->type_of_scaling_list_used[4]);
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[4],(Ipp8u*) sps->ScalingLists4x4[3].ScalingListCoeffs);
                sps->type_of_scaling_list_used[4] = SCLDEFAULT;
            }
            // 5
            if(Get1Bit())
            {
                GetScalingList4x4(&sps->ScalingLists4x4[5],(Ipp8u*) default_inter_scaling_list4x4,&sps->type_of_scaling_list_used[5]);
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[5],(Ipp8u*) sps->ScalingLists4x4[4].ScalingListCoeffs);
                sps->type_of_scaling_list_used[5] = SCLDEFAULT;
            }

            // 0
            if(Get1Bit())
            {
                GetScalingList8x8(&sps->ScalingLists8x8[0],(Ipp8u*)default_intra_scaling_list8x8,&sps->type_of_scaling_list_used[6]);
            }
            else
            {
                FillScalingList8x8(&sps->ScalingLists8x8[0],(Ipp8u*) default_intra_scaling_list8x8);
                sps->type_of_scaling_list_used[6] = SCLDEFAULT;
            }
            // 1
            if(Get1Bit())
            {
                GetScalingList8x8(&sps->ScalingLists8x8[1],(Ipp8u*) default_inter_scaling_list8x8,&sps->type_of_scaling_list_used[7]);
            }
            else
            {
                FillScalingList8x8(&sps->ScalingLists8x8[1],(Ipp8u*) default_inter_scaling_list8x8);
                sps->type_of_scaling_list_used[7] = SCLDEFAULT;
            }

        }
        else
        {
            Ipp32s i;

            for (i = 0; i < 6; i += 1)
            {
                FillFlatScalingList4x4(&sps->ScalingLists4x4[i]);
            }
            for (i = 0; i < 2; i += 1)
            {
                FillFlatScalingList8x8(&sps->ScalingLists8x8[i]);
            }
        }
    }
    else
    {
        sps->chroma_format_idc = 1;
        sps->bit_depth_luma = 8;
        sps->bit_depth_chroma = 8;

        SetDefaultScalingLists(sps);
    }

    // log2 max frame num (bitstream contains value - 4)
    Ipp32u log2_max_frame_num = GetVLCElement(false) + 4;
    sps->log2_max_frame_num = (Ipp8u)log2_max_frame_num;

    if (log2_max_frame_num > 16 || log2_max_frame_num < 4)
        return UMC_ERR_INVALID_STREAM;

    // pic order cnt type (0..2)
    Ipp32u pic_order_cnt_type = GetVLCElement(false);
    sps->pic_order_cnt_type = (Ipp8u)pic_order_cnt_type;
    if (pic_order_cnt_type > 2)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    if (sps->pic_order_cnt_type == 0)
    {
        // log2 max pic order count lsb (bitstream contains value - 4)
        Ipp32u log2_max_pic_order_cnt_lsb = GetVLCElement(false) + 4;
        sps->log2_max_pic_order_cnt_lsb = (Ipp8u)log2_max_pic_order_cnt_lsb;

        if (log2_max_pic_order_cnt_lsb > 16 || log2_max_pic_order_cnt_lsb < 4)
            return UMC_ERR_INVALID_STREAM;

        sps->MaxPicOrderCntLsb = (1 << sps->log2_max_pic_order_cnt_lsb);
    }
    else if (sps->pic_order_cnt_type == 1)
    {
        sps->delta_pic_order_always_zero_flag = Get1Bit();
        sps->offset_for_non_ref_pic = GetVLCElement(true);
        sps->offset_for_top_to_bottom_field = GetVLCElement(true);
        sps->num_ref_frames_in_pic_order_cnt_cycle = GetVLCElement(false);

        if (sps->num_ref_frames_in_pic_order_cnt_cycle > 255)
            return UMC_ERR_INVALID_STREAM;

        // get offsets
        for (Ipp32u i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
        {
            sps->poffset_for_ref_frame[i] = GetVLCElement(true);
        }
    }    // pic order count type 1

    // num ref frames
    sps->num_ref_frames = GetVLCElement(false);
    if (sps->num_ref_frames > 16)
        return UMC_ERR_INVALID_STREAM;

    sps->gaps_in_frame_num_value_allowed_flag = Get1Bit();

    // picture width in MBs (bitstream contains value - 1)
    sps->frame_width_in_mbs = GetVLCElement(false) + 1;

    // picture height in MBs (bitstream contains value - 1)
    sps->frame_height_in_mbs = GetVLCElement(false) + 1;

    if (!(sps->frame_width_in_mbs * 16 < USHRT_MAX) ||
        !(sps->frame_height_in_mbs * 16 < USHRT_MAX))
        return UMC_ERR_INVALID_STREAM;

    sps->frame_mbs_only_flag = Get1Bit();
    sps->frame_height_in_mbs  = (2-sps->frame_mbs_only_flag)*sps->frame_height_in_mbs;
    if (sps->frame_mbs_only_flag == 0)
    {
        sps->mb_adaptive_frame_field_flag = Get1Bit();
    }
    sps->direct_8x8_inference_flag = Get1Bit();
    if (sps->frame_mbs_only_flag==0)
    {
        sps->direct_8x8_inference_flag = 1;
    }
    sps->frame_cropping_flag = Get1Bit();

    if (sps->frame_cropping_flag)
    {
        sps->frame_cropping_rect_left_offset      = GetVLCElement(false);
        sps->frame_cropping_rect_right_offset     = GetVLCElement(false);
        sps->frame_cropping_rect_top_offset       = GetVLCElement(false);
        sps->frame_cropping_rect_bottom_offset    = GetVLCElement(false);

        // check cropping parameters
        Ipp32s cropX = SubWidthC[sps->chroma_format_idc] * sps->frame_cropping_rect_left_offset;
        Ipp32s cropY = SubHeightC[sps->chroma_format_idc] * sps->frame_cropping_rect_top_offset * (2 - sps->frame_mbs_only_flag);
        Ipp32s cropH = sps->frame_height_in_mbs * 16 -
            SubHeightC[sps->chroma_format_idc]*(2 - sps->frame_mbs_only_flag) *
            (sps->frame_cropping_rect_top_offset + sps->frame_cropping_rect_bottom_offset);

        Ipp32s cropW = sps->frame_width_in_mbs * 16 - SubWidthC[sps->chroma_format_idc] *
            (sps->frame_cropping_rect_left_offset + sps->frame_cropping_rect_right_offset);

        if (cropX < 0 || cropY < 0 || cropW < 0 || cropH < 0)
            return UMC_ERR_INVALID_STREAM;

        if (cropX > (Ipp32s)sps->frame_width_in_mbs * 16)
            return UMC_ERR_INVALID_STREAM;

        if (cropY > (Ipp32s)sps->frame_height_in_mbs * 16)
            return UMC_ERR_INVALID_STREAM;

        if (cropX + cropW > (Ipp32s)sps->frame_width_in_mbs * 16)
            return UMC_ERR_INVALID_STREAM;

        if (cropY + cropH > (Ipp32s)sps->frame_height_in_mbs * 16)
            return UMC_ERR_INVALID_STREAM;

    } // don't need else because we zeroid structure

    CheckBSLeft();

    sps->vui_parameters_present_flag = Get1Bit();
    if (sps->vui_parameters_present_flag)
    {
        H264VUI vui;
        Status vui_ps = GetVUIParam(sps, &vui);

        if (vui_ps == UMC_OK && IsBSLeft())
        {
            sps->vui = vui;
        }
    }
    
    return ps;
}    // GetSequenceParamSet

Status H264HeadersBitstream::GetVUIParam(H264SeqParamSet *sps, H264VUI *vui)
{
    Status ps = UMC_OK;

    vui->Reset();

    vui->aspect_ratio_info_present_flag = Get1Bit();

    vui->sar_width = 1; // default values
    vui->sar_height = 1;

    if (vui->aspect_ratio_info_present_flag)
    {
        vui->aspect_ratio_idc = (Ipp8u) GetBits(8);
        if (vui->aspect_ratio_idc  ==  255) {
            vui->sar_width = (Ipp16u) GetBits(16);
            vui->sar_height = (Ipp16u) GetBits(16);
        }
        else
        {
            if (!vui->aspect_ratio_idc || vui->aspect_ratio_idc >= sizeof(SAspectRatio)/sizeof(SAspectRatio[0]))
            {
                vui->aspect_ratio_info_present_flag = 0;
            }
            else
            {
                vui->sar_width = SAspectRatio[vui->aspect_ratio_idc][0];
                vui->sar_height = SAspectRatio[vui->aspect_ratio_idc][1];
            }
        }
    }

    vui->overscan_info_present_flag = Get1Bit();
    if (vui->overscan_info_present_flag)
        vui->overscan_appropriate_flag = Get1Bit();

    vui->video_signal_type_present_flag = Get1Bit();
    if( vui->video_signal_type_present_flag ) {
        vui->video_format = (Ipp8u) GetBits(3);
        vui->video_full_range_flag = Get1Bit();
        vui->colour_description_present_flag = Get1Bit();
        if( vui->colour_description_present_flag ) {
            vui->colour_primaries = (Ipp8u) GetBits(8);
            vui->transfer_characteristics = (Ipp8u) GetBits(8);
            vui->matrix_coefficients = (Ipp8u) GetBits(8);
        }
    }
    vui->chroma_loc_info_present_flag = Get1Bit();
    if( vui->chroma_loc_info_present_flag ) {
        vui->chroma_sample_loc_type_top_field = (Ipp8u) GetVLCElement(false);
        vui->chroma_sample_loc_type_bottom_field = (Ipp8u) GetVLCElement(false);
    }
    vui->timing_info_present_flag = Get1Bit();

    if (vui->timing_info_present_flag)
    {
        vui->num_units_in_tick = GetBits(32);
        vui->time_scale = GetBits(32);
        vui->fixed_frame_rate_flag = Get1Bit();

        if (!vui->num_units_in_tick || !vui->time_scale)
            vui->timing_info_present_flag = 0;
    }

    vui->nal_hrd_parameters_present_flag = Get1Bit();
    if( vui->nal_hrd_parameters_present_flag )
        ps=GetHRDParam(sps, vui);
    vui->vcl_hrd_parameters_present_flag = Get1Bit();
    if( vui->vcl_hrd_parameters_present_flag )
        ps=GetHRDParam(sps, vui);
    if( vui->nal_hrd_parameters_present_flag  ||  vui->vcl_hrd_parameters_present_flag )
        vui->low_delay_hrd_flag = Get1Bit();
    vui->pic_struct_present_flag  = Get1Bit();
    vui->bitstream_restriction_flag = Get1Bit();
    if( vui->bitstream_restriction_flag ) {
        vui->motion_vectors_over_pic_boundaries_flag = Get1Bit();
        vui->max_bytes_per_pic_denom = (Ipp8u) GetVLCElement(false);
        vui->max_bits_per_mb_denom = (Ipp8u) GetVLCElement(false);
        vui->log2_max_mv_length_horizontal = (Ipp8u) GetVLCElement(false);
        vui->log2_max_mv_length_vertical = (Ipp8u) GetVLCElement(false);
        vui->num_reorder_frames = (Ipp8u) GetVLCElement(false);

        Ipp32s value = GetVLCElement(false);
        if (value < (Ipp32s)sps->num_ref_frames || value < 0)
        {
            return UMC_ERR_INVALID_STREAM;
        }

        vui->max_dec_frame_buffering = (Ipp8u)value;
        if (vui->max_dec_frame_buffering > 16)
        {
            return UMC_ERR_INVALID_STREAM;
        }
    }

    return ps;
}

Status H264HeadersBitstream::GetHRDParam(H264SeqParamSet *, H264VUI *vui)
{
    Ipp32s cpb_cnt = (GetVLCElement(false)+1);

    if (cpb_cnt >= 32)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    vui->cpb_cnt = (Ipp8u)cpb_cnt;

    vui->bit_rate_scale = (Ipp8u) GetBits(4);
    vui->cpb_size_scale = (Ipp8u) GetBits(4);
    for( Ipp32s idx= 0; idx < vui->cpb_cnt; idx++ ) {
        vui->bit_rate_value[ idx ] = (Ipp32u) (GetVLCElement(false)+1);
        vui->cpb_size_value[ idx ] = (Ipp32u) ((GetVLCElement(false)+1));
        vui->cbr_flag[ idx ] = Get1Bit();
    }
    vui->initial_cpb_removal_delay_length = (Ipp8u)(GetBits(5)+1);
    vui->cpb_removal_delay_length = (Ipp8u)(GetBits(5)+1);
    vui->dpb_output_delay_length = (Ipp8u) (GetBits(5)+1);
    vui->time_offset_length = (Ipp8u) GetBits(5);
    return UMC_OK;
}

// ---------------------------------------------------------------------------
//    Read sequence parameter set extension data from bitstream.
// ---------------------------------------------------------------------------
Status H264HeadersBitstream::GetSequenceParamSetExtension(H264SeqParamSetExtension *sps_ex)
{
    sps_ex->Reset();

    Ipp32u seq_parameter_set_id = GetVLCElement(false);
    sps_ex->seq_parameter_set_id = (Ipp8u)seq_parameter_set_id;
    if (seq_parameter_set_id > MAX_NUM_SEQ_PARAM_SETS-1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    Ipp32u aux_format_idc = GetVLCElement(false);
    sps_ex->aux_format_idc = (Ipp8u)aux_format_idc;
    if (aux_format_idc > 3)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    if (sps_ex->aux_format_idc != 1 && sps_ex->aux_format_idc != 2)
        sps_ex->aux_format_idc = 0;

    if (sps_ex->aux_format_idc)
    {
        Ipp32u bit_depth_aux = GetVLCElement(false) + 8;
        sps_ex->bit_depth_aux = (Ipp8u)bit_depth_aux;
        if (bit_depth_aux > 12)
        {
            return UMC_ERR_INVALID_STREAM;
        }

        sps_ex->alpha_incr_flag = Get1Bit();
        sps_ex->alpha_opaque_value = (Ipp8u)GetBits(sps_ex->bit_depth_aux + 1);
        sps_ex->alpha_transparent_value = (Ipp8u)GetBits(sps_ex->bit_depth_aux + 1);
    }

    sps_ex->additional_extension_flag = Get1Bit();

    CheckBSLeft();
    return UMC_OK;
}    // GetSequenceParamSetExtension

template <class num_t, class items_t> static
Status DecodeViewReferenceInfo(num_t &numItems, items_t *pItems, H264HeadersBitstream &bitStream)
{
    Ipp32u j;

    // decode number of items
    numItems = (num_t) bitStream.GetVLCElement(false);
    if (H264_MAX_NUM_VIEW_REF <= numItems)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    // decode items
    for (j = 0; j < numItems; j += 1)
    {
        pItems[j] = (items_t) bitStream.GetVLCElement(false);
        if (H264_MAX_NUM_VIEW <= pItems[j])
        {
            return UMC_ERR_INVALID_STREAM;
        }
    }

    return UMC_OK;

} // Status DecodeViewReferenceInfo(num_t &numItems, items_t *pItems, H264HeadersBitstream &bitStream)

Status H264HeadersBitstream::GetSequenceParamSetSvcExt(H264SeqParamSetSVCExtension *pSPSSvcExt)
{
    pSPSSvcExt->Reset();

    if ((pSPSSvcExt->profile_idc != H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE) &&
        (pSPSSvcExt->profile_idc != H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH))
        return UMC_OK;

    pSPSSvcExt->inter_layer_deblocking_filter_control_present_flag = Get1Bit();
    pSPSSvcExt->extended_spatial_scalability = (Ipp8u)GetBits(2);

    if (pSPSSvcExt->chroma_format_idc == 1 || pSPSSvcExt->chroma_format_idc == 2)
    {
        pSPSSvcExt->chroma_phase_x = Get1Bit() - 1;
        pSPSSvcExt->seq_ref_layer_chroma_phase_x = pSPSSvcExt->chroma_phase_x;
    }

    if (pSPSSvcExt->chroma_format_idc == 1)
    {
        pSPSSvcExt->chroma_phase_y = (Ipp8s)(GetBits(2) - 1);
        pSPSSvcExt->seq_ref_layer_chroma_phase_y = pSPSSvcExt->chroma_phase_y;
    }

    if (pSPSSvcExt->extended_spatial_scalability == 1)
    {
        if (pSPSSvcExt->chroma_format_idc > 0)
        {
            pSPSSvcExt->seq_ref_layer_chroma_phase_x = Get1Bit() - 1;
            pSPSSvcExt->seq_ref_layer_chroma_phase_y = (Ipp8s)(GetBits(2) - 1);
        }

        pSPSSvcExt->seq_scaled_ref_layer_left_offset = GetVLCElement(true);
        pSPSSvcExt->seq_scaled_ref_layer_top_offset = GetVLCElement(true);
        pSPSSvcExt->seq_scaled_ref_layer_right_offset = GetVLCElement(true);
        pSPSSvcExt->seq_scaled_ref_layer_bottom_offset = GetVLCElement(true);
    }

    pSPSSvcExt->seq_tcoeff_level_prediction_flag = Get1Bit();

    if (pSPSSvcExt->seq_tcoeff_level_prediction_flag)
    {
        pSPSSvcExt->adaptive_tcoeff_level_prediction_flag = Get1Bit();
    }

    pSPSSvcExt->slice_header_restriction_flag = Get1Bit();

    CheckBSLeft();

    /* Reference has SVC VUI extension here, but there is no mention about it in standard */
    Ipp32u svc_vui_parameters_present_flag = Get1Bit();
    if (svc_vui_parameters_present_flag)
    {
        GetSequenceParamSetSvcVuiExt(pSPSSvcExt); // ignore return status
    }

    Get1Bit(); // additional_extension2_flag
    return UMC_OK;
}

struct VUI_Entry
{
    Ipp32u vui_ext_dependency_id;
    Ipp32u vui_ext_quality_id;
    Ipp32u vui_ext_temporal_id;
    Ipp32u vui_ext_timing_info_present_flag;
    Ipp32u vui_ext_num_units_in_tick;
    Ipp32u vui_ext_time_scale;
    Ipp32u vui_ext_fixed_frame_rate_flag;

    Ipp32u vui_ext_nal_hrd_parameters_present_flag;
    Ipp32u vui_ext_vcl_hrd_parameters_present_flag;

    Ipp32u vui_ext_low_delay_hrd_flag;
    Ipp32u vui_ext_pic_struct_present_flag;
};

struct SVC_VUI
{
    Ipp32u vui_ext_num_entries;
    VUI_Entry vui_entry[1023];
};

Status H264HeadersBitstream::GetSequenceParamSetSvcVuiExt(H264SeqParamSetSVCExtension *pSPSSvcExt)
{
    SVC_VUI vui;
    vui.vui_ext_num_entries = GetVLCElement(false) + 1;

    for(Ipp32u i = 0; i < vui.vui_ext_num_entries; i++)
    {
        vui.vui_entry[i].vui_ext_dependency_id      = GetBits(3);
        vui.vui_entry[i].vui_ext_quality_id         = GetBits(4);
        vui.vui_entry[i].vui_ext_temporal_id        = GetBits(3);

        vui.vui_entry[i].vui_ext_timing_info_present_flag = Get1Bit(); 

        if (vui.vui_entry[i].vui_ext_timing_info_present_flag)
        {
            vui.vui_entry[i].vui_ext_num_units_in_tick  = GetBits(32);
            vui.vui_entry[i].vui_ext_time_scale         = GetBits(32);
            vui.vui_entry[i].vui_ext_fixed_frame_rate_flag  = Get1Bit();
        }

        vui.vui_entry[i].vui_ext_nal_hrd_parameters_present_flag = Get1Bit();
        if (vui.vui_entry[i].vui_ext_nal_hrd_parameters_present_flag)
        {
            Status sts = GetHRDParam(pSPSSvcExt, &pSPSSvcExt->vui);
            if (sts != UMC_OK)
                return sts;
        }

        vui.vui_entry[i].vui_ext_vcl_hrd_parameters_present_flag = Get1Bit();
        if (vui.vui_entry[i].vui_ext_vcl_hrd_parameters_present_flag)
        {
            Status sts = GetHRDParam(pSPSSvcExt, &pSPSSvcExt->vui);
            if (sts != UMC_OK)
                return sts;
        }

        if (vui.vui_entry[i].vui_ext_nal_hrd_parameters_present_flag || vui.vui_entry[i].vui_ext_vcl_hrd_parameters_present_flag)
            vui.vui_entry[i].vui_ext_low_delay_hrd_flag = Get1Bit();

        vui.vui_entry[i].vui_ext_pic_struct_present_flag = Get1Bit();
    }

    return UMC_OK;
}

Status H264HeadersBitstream::GetSequenceParamSetMvcExt(H264SeqParamSetMVCExtension *pSPSMvcExt)
{
    pSPSMvcExt->Reset();

    // decode the number of available views
    pSPSMvcExt->num_views_minus1 = GetVLCElement(false);
    if (H264_MAX_NUM_VIEW <= pSPSMvcExt->num_views_minus1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    // allocate the views' info structs
    Status umcRes = pSPSMvcExt->viewInfo.Alloc(pSPSMvcExt->num_views_minus1 + 1);
    if (UMC_OK != umcRes)
    {
        return umcRes;
    }

    // parse view IDs
    for (Ipp32u i = 0; i <= pSPSMvcExt->num_views_minus1; i += 1)
    {
        H264ViewRefInfo &viewRefInfo = pSPSMvcExt->viewInfo[i];

        // get the view ID
        viewRefInfo.view_id = GetVLCElement(false);
        if (H264_MAX_NUM_VIEW <= viewRefInfo.view_id)
        {
            return UMC_ERR_INVALID_STREAM;
        }
    }

    // parse anchor refs
    for (Ipp32u i = 1; i <= pSPSMvcExt->num_views_minus1; i += 1)
    {
        H264ViewRefInfo &viewRefInfo = pSPSMvcExt->viewInfo[i];
        Ipp32u listNum;

        for (listNum = LIST_0; listNum <= LIST_1; listNum += 1)
        {
            // decode LX anchor refs info
            umcRes = DecodeViewReferenceInfo(viewRefInfo.num_anchor_refs_lx[listNum],
                                             viewRefInfo.anchor_refs_lx[listNum],
                                             *this);
            if (UMC_OK != umcRes)
            {
                return umcRes;
            }
        }
    }

    // parse non-anchor refs
    for (Ipp32u i = 1; i <= pSPSMvcExt->num_views_minus1; i += 1)
    {
        H264ViewRefInfo &viewRefInfo = pSPSMvcExt->viewInfo[i];
        Ipp32u listNum;

        for (listNum = LIST_0; listNum <= LIST_1; listNum += 1)
        {

            // decode L0 non-anchor refs info
            umcRes = DecodeViewReferenceInfo(viewRefInfo.num_non_anchor_refs_lx[listNum],
                                             viewRefInfo.non_anchor_refs_lx[listNum],
                                             *this);
            if (UMC_OK != umcRes)
            {
                return umcRes;
            }
        }
    }

    // decode the number of applicable signal points
    pSPSMvcExt->num_level_values_signalled_minus1 = GetVLCElement(false);
    if (H264_MAX_NUM_OPS <= pSPSMvcExt->num_level_values_signalled_minus1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    // allocate the level info structure
    umcRes = pSPSMvcExt->levelInfo.Alloc(pSPSMvcExt->num_level_values_signalled_minus1 + 1);
    if (UMC_OK != umcRes)
    {
        return umcRes;
    }

    // decode all ops
    for (Ipp32u i = 0; i <= pSPSMvcExt->num_level_values_signalled_minus1; i += 1)
    {
        H264LevelValueSignaled &levelInfo = pSPSMvcExt->levelInfo[i];
        Ipp32u j;

        // decode the level's profile idc
        levelInfo.level_idc = (Ipp8u) GetBits(8);

        // decode the number of operation points
        levelInfo.num_applicable_ops_minus1 = (Ipp16u) GetVLCElement(false);
        if (H264_MAX_NUM_VIEW <= levelInfo.num_applicable_ops_minus1)
        {
            return UMC_ERR_INVALID_STREAM;
        }

        // allocate the operation points structures
        umcRes = levelInfo.opsInfo.Alloc(levelInfo.num_applicable_ops_minus1 + 1);
        if (UMC_OK != umcRes)
        {
            return umcRes;
        }

        // decode operation points
        for (j = 0; j <= levelInfo.num_applicable_ops_minus1; j += 1)
        {
            H264ApplicableOp &op = levelInfo.opsInfo[j];
            Ipp32u k;

            // decode the temporal ID of the op
            op.applicable_op_temporal_id = (Ipp8u) GetBits(3);

            // decode the number of target views
            op.applicable_op_num_target_views_minus1 = (Ipp16u) GetVLCElement(false);
            if (H264_MAX_NUM_VIEW <= op.applicable_op_num_target_views_minus1)
            {
                return UMC_ERR_INVALID_STREAM;
            }

            // allocate the target view ID array
            umcRes = op.applicable_op_target_view_id.Alloc(op.applicable_op_num_target_views_minus1 + 1);
            if (UMC_OK != umcRes)
            {
                return umcRes;
            }

            // read target view IDs
            for (k = 0; k <= op.applicable_op_num_target_views_minus1; k += 1)
            {
                op.applicable_op_target_view_id[k] = (Ipp16u) GetVLCElement(false);
                if (H264_MAX_NUM_VIEW <= op.applicable_op_target_view_id[k])
                {
                    return UMC_ERR_INVALID_STREAM;
                }
            }

            // decode applicable number of views
            op.applicable_op_num_views_minus1 = (Ipp16u) GetVLCElement(false);
            if (H264_MAX_NUM_VIEW <= op.applicable_op_num_views_minus1)
            {
                return UMC_ERR_INVALID_STREAM;
            }
        }
    }

    CheckBSLeft();
    return UMC_OK;

} // Status H264HeadersBitstream::GetSequenceParamSetMvcExt(H264SeqParamSetMVCExtension *pSPSMvcExt)

Status H264HeadersBitstream::GetPictureParamSetPart1(H264PicParamSet *pps)
{
    // Not all members of the pic param set structure are contained in all
    // pic param sets. So start by init all to zero.
    pps->Reset();

    // id
    Ipp32u pic_parameter_set_id = GetVLCElement(false);
    pps->pic_parameter_set_id = (Ipp16u)pic_parameter_set_id;
    if (pic_parameter_set_id > MAX_NUM_PIC_PARAM_SETS-1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    // seq param set referred to by this pic param set
    Ipp32u seq_parameter_set_id = GetVLCElement(false);
    pps->seq_parameter_set_id = (Ipp8u)seq_parameter_set_id;
    if (seq_parameter_set_id > MAX_NUM_SEQ_PARAM_SETS-1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    CheckBSLeft();
    return UMC_OK;
}    // GetPictureParamSetPart1

// Number of bits required to code slice group ID, index is num_slice_groups - 2
static const Ipp8u SGIdBits[7] = {1,2,2,3,3,3,3};

// ---------------------------------------------------------------------------
//    Read picture parameter set data from bitstream.
// ---------------------------------------------------------------------------
Status H264HeadersBitstream::GetPictureParamSetPart2(H264PicParamSet  *pps)
{
    pps->entropy_coding_mode = Get1Bit();

    pps->bottom_field_pic_order_in_frame_present_flag = Get1Bit();

    // number of slice groups, bitstream has value - 1
    pps->num_slice_groups = GetVLCElement(false) + 1;
    if (pps->num_slice_groups != 1)
    {

        if (pps->num_slice_groups > MAX_NUM_SLICE_GROUPS)
        {
            return UMC_ERR_INVALID_STREAM;
        }

        Ipp32u slice_group_map_type = GetVLCElement(false);
        pps->SliceGroupInfo.slice_group_map_type = (Ipp8u)slice_group_map_type;

        if (slice_group_map_type > 6)
            return UMC_ERR_INVALID_STREAM;

        // Get additional, map type dependent slice group data
        switch (pps->SliceGroupInfo.slice_group_map_type)
        {
        case 0:
            for (Ipp32u slice_group = 0; slice_group < pps->num_slice_groups; slice_group++)
            {
                // run length, bitstream has value - 1
                pps->SliceGroupInfo.run_length[slice_group] = GetVLCElement(false) + 1;
            }
            break;
        case 1:
            // no additional info
            break;
        case 2:
            for (Ipp32u slice_group = 0; slice_group < (Ipp32u)(pps->num_slice_groups-1); slice_group++)
            {
                pps->SliceGroupInfo.t1.top_left[slice_group] = GetVLCElement(false);
                pps->SliceGroupInfo.t1.bottom_right[slice_group] = GetVLCElement(false);

                // check for legal values
                if (pps->SliceGroupInfo.t1.top_left[slice_group] >
                    pps->SliceGroupInfo.t1.bottom_right[slice_group])
                {
                    return UMC_ERR_INVALID_STREAM;
                }
            }
            break;
        case 3:
        case 4:
        case 5:
            // For map types 3..5, number of slice groups must be 2
            if (pps->num_slice_groups != 2)
            {
                return UMC_ERR_INVALID_STREAM;
            }
            pps->SliceGroupInfo.t2.slice_group_change_direction_flag = Get1Bit();
            pps->SliceGroupInfo.t2.slice_group_change_rate = GetVLCElement(false) + 1;
            break;
        case 6:
            // mapping of slice group to map unit (macroblock if not fields) is
            // per map unit, read from bitstream
            {
                // number of map units, bitstream has value - 1
                pps->SliceGroupInfo.t3.pic_size_in_map_units = GetVLCElement(false) + 1;

                Ipp32s len = IPP_MAX(1, pps->SliceGroupInfo.t3.pic_size_in_map_units);

                pps->SliceGroupInfo.pSliceGroupIDMap.resize(len);

                // num_bits is Ceil(log2(num_groups)) - number of bits used to code each slice group id
                Ipp32u num_bits = SGIdBits[pps->num_slice_groups - 2];

                for (Ipp32u map_unit = 0;
                     map_unit < pps->SliceGroupInfo.t3.pic_size_in_map_units;
                     map_unit++)
                {
                    pps->SliceGroupInfo.pSliceGroupIDMap[map_unit] = (Ipp8u)GetBits(num_bits);
                }
            }
            break;
        default:
            return UMC_ERR_INVALID_STREAM;

        }    // switch
    }    // slice group info

    // number of list 0 ref pics used to decode picture, bitstream has value - 1
    pps->num_ref_idx_l0_active = GetVLCElement(false) + 1;

    // number of list 1 ref pics used to decode picture, bitstream has value - 1
    pps->num_ref_idx_l1_active = GetVLCElement(false) + 1;

    if (pps->num_ref_idx_l1_active > MAX_NUM_REF_FRAMES || pps->num_ref_idx_l0_active > MAX_NUM_REF_FRAMES)
        return UMC_ERR_INVALID_STREAM;

    // weighted pediction
    pps->weighted_pred_flag = Get1Bit();
    pps->weighted_bipred_idc = (Ipp8u)GetBits(2);

    // default slice QP, bitstream has value - 26
    Ipp32s pic_init_qp = GetVLCElement(true) + 26;
    pps->pic_init_qp = (Ipp8s)pic_init_qp;
    if (pic_init_qp > QP_MAX)
    {
        //return UMC_ERR_INVALID_STREAM;
    }

    // default SP/SI slice QP, bitstream has value - 26
    pps->pic_init_qs = (Ipp8u)(GetVLCElement(true) + 26);
    if (pps->pic_init_qs > QP_MAX)
    {
        //return UMC_ERR_INVALID_STREAM;
    }

    pps->chroma_qp_index_offset[0] = (Ipp8s)GetVLCElement(true);
    if ((pps->chroma_qp_index_offset[0] < -12) || (pps->chroma_qp_index_offset[0] > 12))
    {
        //return UMC_ERR_INVALID_STREAM;
    }

    pps->deblocking_filter_variables_present_flag = Get1Bit();
    pps->constrained_intra_pred_flag = Get1Bit();
    pps->redundant_pic_cnt_present_flag = Get1Bit();

    if (More_RBSP_Data())
    {
        pps->transform_8x8_mode_flag = Get1Bit();
        pps->pic_scaling_matrix_present_flag = Get1Bit();

        H264ScalingPicParams * scaling = &pps->scaling[0];

        if (pps->pic_scaling_matrix_present_flag)
        {
            for (Ipp32s i = 0; i < 3; i++)
            {
                pps->pic_scaling_list_present_flag[i] = Get1Bit();
                if (pps->pic_scaling_list_present_flag[i])
                {
                    GetScalingList4x4(&scaling->ScalingLists4x4[i], (Ipp8u*)default_intra_scaling_list4x4, &pps->type_of_scaling_list_used[i]);
                }
                else
                {
                    pps->type_of_scaling_list_used[i] = SCLDEFAULT;
                }
            }

            for (Ipp32s i = 3; i < 6; i++)
            {
                pps->pic_scaling_list_present_flag[i] = Get1Bit();
                if (pps->pic_scaling_list_present_flag[i])
                {
                    GetScalingList4x4(&scaling->ScalingLists4x4[i], (Ipp8u*)default_inter_scaling_list4x4, &pps->type_of_scaling_list_used[i]);
                }
                else
                {
                    pps->type_of_scaling_list_used[i] = SCLDEFAULT;
                }
            }


            if (pps->transform_8x8_mode_flag)
            {
                pps->pic_scaling_list_present_flag[6] = Get1Bit();
                if (pps->pic_scaling_list_present_flag[6])
                {
                    GetScalingList8x8(&scaling->ScalingLists8x8[0], (Ipp8u*)default_intra_scaling_list8x8, &pps->type_of_scaling_list_used[6]);
                }
                else
                {
                    pps->type_of_scaling_list_used[6] = SCLDEFAULT;
                }

                pps->pic_scaling_list_present_flag[7] = Get1Bit();
                if (pps->pic_scaling_list_present_flag[7])
                {
                    GetScalingList8x8(&scaling->ScalingLists8x8[1], (Ipp8u*)default_inter_scaling_list8x8, &pps->type_of_scaling_list_used[7]);
                }
                else
                {
                    pps->type_of_scaling_list_used[7] = SCLDEFAULT;
                }
            }

            MFX_INTERNAL_CPY(&pps->scaling[1], &pps->scaling[0], sizeof(H264ScalingPicParams));
        }

        pps->chroma_qp_index_offset[1] = (Ipp8s)GetVLCElement(true);
    }
    else
    {
        pps->chroma_qp_index_offset[1] = pps->chroma_qp_index_offset[0];
    }

    CheckBSLeft();
    return UMC_OK;
}    // GetPictureParamSet

Status InitializePictureParamSet(H264PicParamSet *pps, const H264SeqParamSet *sps, bool isExtension)
{
    if (!pps || !sps || pps->initialized[isExtension])
        return UMC_OK;

    if (pps->num_slice_groups != 1)
    {
        Ipp32u PicSizeInMapUnits = sps->frame_width_in_mbs * sps->frame_height_in_mbs; // for range checks

        // Get additional, map type dependent slice group data
        switch (pps->SliceGroupInfo.slice_group_map_type)
        {
        case 0:
            for (Ipp32u slice_group=0; slice_group<pps->num_slice_groups; slice_group++)
            {
                if (pps->SliceGroupInfo.run_length[slice_group] > PicSizeInMapUnits)
                {
                    return UMC_ERR_INVALID_STREAM;
                }
            }
            break;
        case 1:
            // no additional info
            break;
        case 2:
            for (Ipp32u slice_group=0; slice_group<(Ipp32u)(pps->num_slice_groups-1); slice_group++)
            {
                if (pps->SliceGroupInfo.t1.bottom_right[slice_group] >= PicSizeInMapUnits)
                {
                    return UMC_ERR_INVALID_STREAM;
                }
                if ((pps->SliceGroupInfo.t1.top_left[slice_group] %
                    sps->frame_width_in_mbs) >
                    (pps->SliceGroupInfo.t1.bottom_right[slice_group] %
                    sps->frame_width_in_mbs))
                {
                    return UMC_ERR_INVALID_STREAM;
                }
            }
            break;
        case 3:
        case 4:
        case 5:
            if (pps->SliceGroupInfo.t2.slice_group_change_rate > PicSizeInMapUnits)
            {
                return UMC_ERR_INVALID_STREAM;
            }
            break;
        case 6:
            // mapping of slice group to map unit (macroblock if not fields) is
            // per map unit, read from bitstream
            {
                if (pps->SliceGroupInfo.t3.pic_size_in_map_units != PicSizeInMapUnits)
                {
                    return UMC_ERR_INVALID_STREAM;
                }
                for (Ipp32u map_unit = 0;
                     map_unit < pps->SliceGroupInfo.t3.pic_size_in_map_units;
                     map_unit++)
                {
                    if (pps->SliceGroupInfo.pSliceGroupIDMap[map_unit] >
                        pps->num_slice_groups - 1)
                    {
                        return UMC_ERR_INVALID_STREAM;
                    }
                }
            }
            break;
        default:
            return UMC_ERR_INVALID_STREAM;

        }    // switch
    }    // slice group info

    H264ScalingPicParams * scaling = &pps->scaling[isExtension];
    if (pps->pic_scaling_matrix_present_flag)
    {
        Ipp8u *default_scaling = (Ipp8u*)default_intra_scaling_list4x4;
        for (Ipp32s i = 0; i < 6; i += 3)
        {
            if (!pps->pic_scaling_list_present_flag[i])
            {
                if (sps->seq_scaling_matrix_present_flag) {
                    FillScalingList4x4(&scaling->ScalingLists4x4[i], (Ipp8u*)sps->ScalingLists4x4[i].ScalingListCoeffs);
                }
                else
                {
                    FillScalingList4x4(&scaling->ScalingLists4x4[i], (Ipp8u*)default_scaling);
                }
            }

            if (!pps->pic_scaling_list_present_flag[i+1])
            {
                FillScalingList4x4(&scaling->ScalingLists4x4[i+1], (Ipp8u*)scaling->ScalingLists4x4[i].ScalingListCoeffs);
            }

            if (!pps->pic_scaling_list_present_flag[i+2])
            {
                FillScalingList4x4(&scaling->ScalingLists4x4[i+2], (Ipp8u*)scaling->ScalingLists4x4[i+1].ScalingListCoeffs);
            }

            default_scaling = (Ipp8u*)default_inter_scaling_list4x4;
        }

        if (pps->transform_8x8_mode_flag)
        {
            if (sps->seq_scaling_matrix_present_flag) {
                for (Ipp32s i = 6; i < 8; i++)
                {
                    if (!pps->pic_scaling_list_present_flag[i])
                    {
                        FillScalingList8x8(&scaling->ScalingLists8x8[i-6], (Ipp8u*)sps->ScalingLists8x8[i-6].ScalingListCoeffs);
                    }
                }
            }
            else
            {
                if (!pps->pic_scaling_list_present_flag[6])
                {
                    FillScalingList8x8(&scaling->ScalingLists8x8[0], (Ipp8u*)default_intra_scaling_list8x8);
                }

                if (!pps->pic_scaling_list_present_flag[7])
                {
                    FillScalingList8x8(&scaling->ScalingLists8x8[1], (Ipp8u*)default_inter_scaling_list8x8);
                }
            }
        }
    }
    else
    {
        for (Ipp32s i = 0; i < 6; i++)
        {
            FillScalingList4x4(&scaling->ScalingLists4x4[i],(Ipp8u *)sps->ScalingLists4x4[i].ScalingListCoeffs);
            pps->type_of_scaling_list_used[i] = sps->type_of_scaling_list_used[i];
        }

        if (pps->transform_8x8_mode_flag)
        {
            for (Ipp32s i = 0; i < 2; i++)
            {
                FillScalingList8x8(&scaling->ScalingLists8x8[i],(Ipp8u *)sps->ScalingLists8x8[i].ScalingListCoeffs);
                pps->type_of_scaling_list_used[i] = sps->type_of_scaling_list_used[i];
            }
        }
    }

    // calculate level scale matrices

    //start DC first
    //to do: reduce th anumber of matrices (in fact 1 is enough)
    // now process other 4x4 matrices
    for (Ipp32s i = 0; i < 6; i++)
    {
        for (Ipp32s j = 0; j < 88; j++)
            for (Ipp32s k = 0; k < 16; k++)
            {
                Ipp32u level_scale = scaling->ScalingLists4x4[i].ScalingListCoeffs[k]*pre_norm_adjust4x4[j%6][pre_norm_adjust_index4x4[k]];
                scaling->m_LevelScale4x4[i].LevelScaleCoeffs[j][k] = (Ipp16s) level_scale;
            }
    }

    // process remaining 8x8  matrices
    for (Ipp32s i = 0; i < 2; i++)
    {
        for (Ipp32s j = 0; j < 88; j++)
            for (Ipp32s k = 0; k < 64; k++)
            {
                Ipp32u level_scale = scaling->ScalingLists8x8[i].ScalingListCoeffs[k]*pre_norm_adjust8x8[j%6][pre_norm_adjust_index8x8[k]];
                scaling->m_LevelScale8x8[i].LevelScaleCoeffs[j][k] = (Ipp16s) level_scale;
            }
    }

    pps->initialized[isExtension] = 1;
    return UMC_OK;
}

Status H264HeadersBitstream::GetNalUnitPrefix(H264NalExtension *pExt, Ipp32u NALRef_idc)
{
    Status ps = UMC_OK;

    ps = GetNalUnitExtension(pExt);

    if (ps != UMC_OK || !pExt->svc_extension_flag)
        return ps;

    if (pExt->svc.dependency_id || pExt->svc.quality_id) // shall be equals zero for prefix NAL units
        return UMC_ERR_INVALID_STREAM;

    pExt->svc.adaptiveMarkingInfo.num_entries = 0;

    if (NALRef_idc != 0)
    {
        pExt->svc.store_ref_base_pic_flag = Get1Bit();

        if ((pExt->svc.use_ref_base_pic_flag || pExt->svc.store_ref_base_pic_flag) && !pExt->svc.idr_flag)
        {
            ps = DecRefBasePicMarking(&pExt->svc.adaptiveMarkingInfo, pExt->svc.adaptive_ref_base_pic_marking_mode_flag);
            if (ps != UMC_OK)
                return ps;
        }

        /*Ipp32s additional_prefix_nal_unit_extension_flag = (Ipp32s) Get1Bit();

        if (additional_prefix_nal_unit_extension_flag == 1)
        {
            additional_prefix_nal_unit_extension_flag = Get1Bit();
        }*/
    }

    CheckBSLeft();
    return ps;
} // Status H264HeadersBitstream::GetNalUnitPrefix(void)

Status H264HeadersBitstream::GetNalUnitExtension(H264NalExtension *pExt)
{
    pExt->Reset();
    pExt->extension_present = 1;

    // decode the type of the extension
    pExt->svc_extension_flag = (Ipp8u) GetBits(1);

    // decode SVC extension
    if (pExt->svc_extension_flag)
    {
        pExt->svc.idr_flag = Get1Bit();
        pExt->svc.priority_id = (Ipp8u) GetBits(6);
        pExt->svc.no_inter_layer_pred_flag = Get1Bit();
        pExt->svc.dependency_id = (Ipp8u) GetBits(3);
        pExt->svc.quality_id = (Ipp8u) GetBits(4);
        pExt->svc.temporal_id = (Ipp8u) GetBits(3);
        pExt->svc.use_ref_base_pic_flag = Get1Bit();
        pExt->svc.discardable_flag = Get1Bit();
        pExt->svc.output_flag = Get1Bit();
        GetBits(2);
    }
    // decode MVC extension
    else
    {
        pExt->mvc.non_idr_flag = Get1Bit();
        pExt->mvc.priority_id = (Ipp16u) GetBits(6);
        pExt->mvc.view_id = (Ipp16u) GetBits(10);
        pExt->mvc.temporal_id = (Ipp8u) GetBits(3);
        pExt->mvc.anchor_pic_flag = Get1Bit();
        pExt->mvc.inter_view_flag = Get1Bit();
        GetBits(1);
    }

    CheckBSLeft();
    return UMC_OK;

} // Status H264HeadersBitstream::GetNalUnitExtension(H264NalExtension *pExt)

// ---------------------------------------------------------------------------
//    Read H.264 first part of slice header
//
//  Reading the rest of the header requires info in the picture and sequence
//  parameter sets referred to by this slice header.
//
//    Do not print debug messages when IsSearch is true. In that case the function
//    is being used to find the next compressed frame, errors may occur and should
//    not be reported.
//
// ---------------------------------------------------------------------------
Status H264HeadersBitstream::GetSliceHeaderPart1(H264SliceHeader *hdr)
{
    Ipp32u val;

    // decode NAL extension
    if (NAL_UT_CODED_SLICE_EXTENSION == hdr->nal_unit_type)
    {
        GetNalUnitExtension(&hdr->nal_ext);

        // set the IDR flag
        if (hdr->nal_ext.svc_extension_flag)
        {
            hdr->IdrPicFlag = hdr->nal_ext.svc.idr_flag;
        }
        else
        {
            hdr->IdrPicFlag = hdr->nal_ext.mvc.non_idr_flag ^ 1;
        }
    }
    else
    {
        if (hdr->nal_ext.extension_present)
        {
            if (hdr->nal_ext.svc_extension_flag)
            {
                //hdr->IdrPicFlag = hdr->nal_ext.svc.idr_flag;
            }
            else
            {
                //hdr->IdrPicFlag = hdr->nal_ext.mvc.non_idr_flag ^ 1;
            }
        }
        else
        {
            //hdr->IdrPicFlag = (NAL_UT_IDR_SLICE == hdr->nal_unit_type) ? 1 : 0;
            hdr->nal_ext.mvc.anchor_pic_flag = (Ipp8u) hdr->IdrPicFlag ? 1 : 0;
            hdr->nal_ext.mvc.inter_view_flag = (Ipp8u) 1;
        }

        hdr->IdrPicFlag = (NAL_UT_IDR_SLICE == hdr->nal_unit_type) ? 1 : 0;
    }

    hdr->first_mb_in_slice = GetVLCElement(false);
    if (0 > hdr->first_mb_in_slice) // upper bound is checked in H264Slice
        return UMC_ERR_INVALID_STREAM;

    // slice type
    val = GetVLCElement(false);
    if (val > S_INTRASLICE)
    {
        if (val > S_INTRASLICE + S_INTRASLICE + 1)
        {
            return UMC_ERR_INVALID_STREAM;
        }
        else
        {
            // Slice type is specifying type of not only this but all remaining
            // slices in the picture. Since slice type is always present, this bit
            // of info is not used in our implementation. Adjust (just shift range)
            // and return type without this extra info.
            val -= (S_INTRASLICE + 1);
        }
    }

    if (val > INTRASLICE) // all other doesn't support
        return UMC_ERR_INVALID_STREAM;

    hdr->slice_type = (EnumSliceCodType)val;
    if (NAL_UT_IDR_SLICE == hdr->nal_unit_type && hdr->slice_type != INTRASLICE)
        return UMC_ERR_INVALID_STREAM;

    Ipp32u pic_parameter_set_id = GetVLCElement(false);
    hdr->pic_parameter_set_id = (Ipp16u)pic_parameter_set_id;
    if (pic_parameter_set_id > MAX_NUM_PIC_PARAM_SETS - 1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    CheckBSLeft();
    return UMC_OK;
} // Status GetSliceHeaderPart1(H264SliceHeader *pSliceHeader)

Status H264HeadersBitstream::GetSliceHeaderPart2(H264SliceHeader *hdr,
                                                 const H264PicParamSet *pps,
                                                 const H264SeqParamSet *sps)
{
    hdr->frame_num = GetBits(sps->log2_max_frame_num);

    hdr->bottom_field_flag = 0;
    if (sps->frame_mbs_only_flag == 0)
    {
        hdr->field_pic_flag = Get1Bit();
        hdr->MbaffFrameFlag = !hdr->field_pic_flag && sps->mb_adaptive_frame_field_flag;
        if (hdr->field_pic_flag != 0)
        {
            hdr->bottom_field_flag = Get1Bit();
        }
    }

    if (hdr->MbaffFrameFlag)
    {
        Ipp32u const first_mb_in_slice
            = hdr->first_mb_in_slice;

        if (first_mb_in_slice * 2 > INT_MAX)
            return UMC_ERR_INVALID_STREAM;

        // correct frst_mb_in_slice in order to handle MBAFF
        hdr->first_mb_in_slice *= 2;
    }

    if (hdr->IdrPicFlag)
    {
        Ipp32s pic_id = hdr->idr_pic_id = GetVLCElement(false);
        if (pic_id < 0 || pic_id > 65535)
            return UMC_ERR_INVALID_STREAM;
    }

    if (sps->pic_order_cnt_type == 0)
    {
        hdr->pic_order_cnt_lsb = GetBits(sps->log2_max_pic_order_cnt_lsb);
        if (pps->bottom_field_pic_order_in_frame_present_flag && (!hdr->field_pic_flag))
            hdr->delta_pic_order_cnt_bottom = GetVLCElement(true);
    }

    if ((sps->pic_order_cnt_type == 1) && (sps->delta_pic_order_always_zero_flag == 0))
    {
        hdr->delta_pic_order_cnt[0] = GetVLCElement(true);
        if (pps->bottom_field_pic_order_in_frame_present_flag && (!hdr->field_pic_flag))
            hdr->delta_pic_order_cnt[1] = GetVLCElement(true);
    }

    if (pps->redundant_pic_cnt_present_flag)
    {
        hdr->hw_wa_redundant_elimination_bits[0] = (Ipp32u)BitsDecoded();
        // redundant pic count
        hdr->redundant_pic_cnt = GetVLCElement(false);
        if (hdr->redundant_pic_cnt > 127)
            return UMC_ERR_INVALID_STREAM;

        hdr->hw_wa_redundant_elimination_bits[1] = (Ipp32u)BitsDecoded();
    }

    CheckBSLeft();
    return UMC_OK;
}

Status H264HeadersBitstream::DecRefBasePicMarking(AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
                                    Ipp8u &adaptive_ref_pic_marking_mode_flag)
{
    Ipp32u memory_management_control_operation;
    Ipp32u num_entries = 0;

    adaptive_ref_pic_marking_mode_flag = Get1Bit();

    while (adaptive_ref_pic_marking_mode_flag != 0) {
        memory_management_control_operation = (Ipp8u)GetVLCElement(false);
        if (memory_management_control_operation == 0)
            break;

        if (memory_management_control_operation > 6)
            return UMC_ERR_INVALID_STREAM;

        pAdaptiveMarkingInfo->mmco[num_entries] =
            (Ipp8u)memory_management_control_operation;

        if (memory_management_control_operation != 5)
            pAdaptiveMarkingInfo->value[num_entries*2] = GetVLCElement(false);

        // Only mmco 3 requires 2 values
        if (memory_management_control_operation == 3)
            pAdaptiveMarkingInfo->value[num_entries*2+1] = GetVLCElement(false);

        num_entries++;
        if (num_entries >= MAX_NUM_REF_FRAMES) {
            return UMC_ERR_INVALID_STREAM;
        }
    }    // while
    pAdaptiveMarkingInfo->num_entries = num_entries;
    return UMC_OK;
}


Status H264HeadersBitstream::GetPredWeightTable(
    H264SliceHeader *hdr,        // slice header read goes here
    const H264SeqParamSet *sps,
    PredWeightTable *pPredWeight_L0, // L0 weight table goes here
    PredWeightTable *pPredWeight_L1) // L1 weight table goes here
{
    Ipp32u luma_log2_weight_denom = GetVLCElement(false);
    hdr->luma_log2_weight_denom = (Ipp8u)luma_log2_weight_denom;
    if (luma_log2_weight_denom > 7)
        return UMC_ERR_INVALID_STREAM;

    if (sps->chroma_format_idc != 0)
    {
        Ipp32u chroma_log2_weight_denom = GetVLCElement(false);
        hdr->chroma_log2_weight_denom = (Ipp8u)chroma_log2_weight_denom;
        if (chroma_log2_weight_denom > 7)
            return UMC_ERR_INVALID_STREAM;
    }

    for (Ipp32s refindex = 0; refindex < hdr->num_ref_idx_l0_active; refindex++)
    {
        pPredWeight_L0[refindex].luma_weight_flag = Get1Bit();
        if (pPredWeight_L0[refindex].luma_weight_flag)
        {
            pPredWeight_L0[refindex].luma_weight = (Ipp8s)GetVLCElement(true);
            pPredWeight_L0[refindex].luma_offset = (Ipp8s)GetVLCElement(true);
            pPredWeight_L0[refindex].luma_offset <<= (sps->bit_depth_luma - 8);
        }
        else
        {
            pPredWeight_L0[refindex].luma_weight = (Ipp8s)(1 << hdr->luma_log2_weight_denom);
            pPredWeight_L0[refindex].luma_offset = 0;
        }

        pPredWeight_L0[refindex].chroma_weight_flag = 0;
        if (sps->chroma_format_idc != 0)
        {
            pPredWeight_L0[refindex].chroma_weight_flag = Get1Bit();
        }

        if (pPredWeight_L0[refindex].chroma_weight_flag)
        {
            pPredWeight_L0[refindex].chroma_weight[0] = (Ipp8s)GetVLCElement(true);
            pPredWeight_L0[refindex].chroma_offset[0] = (Ipp8s)GetVLCElement(true);
            pPredWeight_L0[refindex].chroma_weight[1] = (Ipp8s)GetVLCElement(true);
            pPredWeight_L0[refindex].chroma_offset[1] = (Ipp8s)GetVLCElement(true);

            pPredWeight_L0[refindex].chroma_offset[0] <<= (sps->bit_depth_chroma - 8);
            pPredWeight_L0[refindex].chroma_offset[1] <<= (sps->bit_depth_chroma - 8);
        }
        else
        {
            pPredWeight_L0[refindex].chroma_weight[0] = (Ipp8s)(1 << hdr->chroma_log2_weight_denom);
            pPredWeight_L0[refindex].chroma_weight[1] = (Ipp8s)(1 << hdr->chroma_log2_weight_denom);
            pPredWeight_L0[refindex].chroma_offset[0] = 0;
            pPredWeight_L0[refindex].chroma_offset[1] = 0;
        }
    }

    if (BPREDSLICE == hdr->slice_type)
    {
        for (Ipp32s refindex = 0; refindex < hdr->num_ref_idx_l1_active; refindex++)
        {
            pPredWeight_L1[refindex].luma_weight_flag = Get1Bit();
            if (pPredWeight_L1[refindex].luma_weight_flag)
            {
                pPredWeight_L1[refindex].luma_weight = (Ipp8s)GetVLCElement(true);
                pPredWeight_L1[refindex].luma_offset = (Ipp8s)GetVLCElement(true);
                pPredWeight_L1[refindex].luma_offset <<= (sps->bit_depth_luma - 8);
            }
            else
            {
                pPredWeight_L1[refindex].luma_weight = (Ipp8s)(1 << hdr->luma_log2_weight_denom);
                pPredWeight_L1[refindex].luma_offset = 0;
            }

            pPredWeight_L1[refindex].chroma_weight_flag = 0;
            if (sps->chroma_format_idc != 0)
                pPredWeight_L1[refindex].chroma_weight_flag = Get1Bit();

            if (pPredWeight_L1[refindex].chroma_weight_flag)
            {
                pPredWeight_L1[refindex].chroma_weight[0] = (Ipp8s)GetVLCElement(true);
                pPredWeight_L1[refindex].chroma_offset[0] = (Ipp8s)GetVLCElement(true);
                pPredWeight_L1[refindex].chroma_weight[1] = (Ipp8s)GetVLCElement(true);
                pPredWeight_L1[refindex].chroma_offset[1] = (Ipp8s)GetVLCElement(true);

                pPredWeight_L1[refindex].chroma_offset[0] <<= (sps->bit_depth_chroma - 8);
                pPredWeight_L1[refindex].chroma_offset[1] <<= (sps->bit_depth_chroma - 8);
            }
            else
            {
                pPredWeight_L1[refindex].chroma_weight[0] = (Ipp8s)(1 << hdr->chroma_log2_weight_denom);
                pPredWeight_L1[refindex].chroma_weight[1] = (Ipp8s)(1 << hdr->chroma_log2_weight_denom);
                pPredWeight_L1[refindex].chroma_offset[0] = 0;
                pPredWeight_L1[refindex].chroma_offset[1] = 0;
            }
        }
    }    // B slice

    return UMC_OK;
}

Status H264HeadersBitstream::GetSliceHeaderPart3(
    H264SliceHeader *hdr,        // slice header read goes here
    PredWeightTable *pPredWeight_L0, // L0 weight table goes here
    PredWeightTable *pPredWeight_L1, // L1 weight table goes here
    RefPicListReorderInfo *pReorderInfo_L0,
    RefPicListReorderInfo *pReorderInfo_L1,
    AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
    AdaptiveMarkingInfo *pBaseAdaptiveMarkingInfo,
    const H264PicParamSet *pps,
    const H264SeqParamSet *sps,
    const H264SeqParamSetSVCExtension *spsSvcExt)
{
    Ipp8u ref_pic_list_reordering_flag_l0 = 0;
    Ipp8u ref_pic_list_reordering_flag_l1 = 0;
    Status ps = UMC_OK;

    pReorderInfo_L0->num_entries = 0;
    pReorderInfo_L1->num_entries = 0;
    hdr->num_ref_idx_l0_active = pps->num_ref_idx_l0_active;
    hdr->num_ref_idx_l1_active = pps->num_ref_idx_l1_active;

    hdr->direct_spatial_mv_pred_flag = 1;

    if (!hdr->nal_ext.svc_extension_flag || hdr->nal_ext.svc.quality_id == 0)
    {
        if (BPREDSLICE == hdr->slice_type)
        {
            // direct mode prediction method
            hdr->direct_spatial_mv_pred_flag = Get1Bit();
        }

        if (PREDSLICE == hdr->slice_type ||
            S_PREDSLICE == hdr->slice_type ||
            BPREDSLICE == hdr->slice_type)
        {
            hdr->num_ref_idx_active_override_flag = Get1Bit();
            if (hdr->num_ref_idx_active_override_flag != 0)
            // ref idx active l0 and l1
            {
                hdr->num_ref_idx_l0_active = GetVLCElement(false) + 1;
                if (BPREDSLICE == hdr->slice_type)
                    hdr->num_ref_idx_l1_active = GetVLCElement(false) + 1;
            }
        }    // ref idx override

        if (hdr->num_ref_idx_l0_active < 0 || hdr->num_ref_idx_l0_active > (Ipp32s)MAX_NUM_REF_FRAMES ||
            hdr->num_ref_idx_l1_active < 0 || hdr->num_ref_idx_l1_active > (Ipp32s)MAX_NUM_REF_FRAMES)
            return UMC_ERR_INVALID_STREAM;

        if (hdr->slice_type != INTRASLICE && hdr->slice_type != S_INTRASLICE)
        {
            Ipp32u reordering_of_pic_nums_idc;
            Ipp32u reorder_idx;

            // Reference picture list reordering
            ref_pic_list_reordering_flag_l0 = Get1Bit();
            if (ref_pic_list_reordering_flag_l0)
            {
                bool bOk = true;

                reorder_idx = 0;
                reordering_of_pic_nums_idc = 0;

                // Get reorder idc,pic_num pairs until idc==3
                while (bOk)
                {
                  reordering_of_pic_nums_idc = (Ipp8u)GetVLCElement(false);
                  if (reordering_of_pic_nums_idc > 5)
                    return UMC_ERR_INVALID_STREAM;

                    if (reordering_of_pic_nums_idc == 3)
                        break;

                    if (reorder_idx >= MAX_NUM_REF_FRAMES)
                    {
                        return UMC_ERR_INVALID_STREAM;
                    }

                    pReorderInfo_L0->reordering_of_pic_nums_idc[reorder_idx] =
                                                (Ipp8u)reordering_of_pic_nums_idc;
                    pReorderInfo_L0->reorder_value[reorder_idx]  =
                                                        GetVLCElement(false);
                  if (reordering_of_pic_nums_idc != 2)
                        // abs_diff_pic_num is coded minus 1
                        pReorderInfo_L0->reorder_value[reorder_idx]++;
                    reorder_idx++;
                }    // while

                pReorderInfo_L0->num_entries = reorder_idx;
            }    // L0 reordering info
            else
                pReorderInfo_L0->num_entries = 0;

            if (BPREDSLICE == hdr->slice_type)
            {
                ref_pic_list_reordering_flag_l1 = Get1Bit();
                if (ref_pic_list_reordering_flag_l1)
                {
                    bool bOk = true;

                    // Get reorder idc,pic_num pairs until idc==3
                    reorder_idx = 0;
                    reordering_of_pic_nums_idc = 0;
                    while (bOk)
                    {
                    reordering_of_pic_nums_idc = GetVLCElement(false);
                      if (reordering_of_pic_nums_idc > 5)
                        return UMC_ERR_INVALID_STREAM;

                        if (reordering_of_pic_nums_idc == 3)
                            break;

                        if (reorder_idx >= MAX_NUM_REF_FRAMES)
                        {
                            return UMC_ERR_INVALID_STREAM;
                        }

                        pReorderInfo_L1->reordering_of_pic_nums_idc[reorder_idx] =
                                                    (Ipp8u)reordering_of_pic_nums_idc;
                        pReorderInfo_L1->reorder_value[reorder_idx]  =
                                                            GetVLCElement(false);
                        if (reordering_of_pic_nums_idc != 2)
                            // abs_diff_pic_num is coded minus 1
                            pReorderInfo_L1->reorder_value[reorder_idx]++;
                        reorder_idx++;
                    }    // while
                    pReorderInfo_L1->num_entries = reorder_idx;
                }    // L1 reordering info
                else
                    pReorderInfo_L1->num_entries = 0;
            }    // B slice
        }    // reordering info

        hdr->luma_log2_weight_denom = 0;
        hdr->chroma_log2_weight_denom = 0;

        // prediction weight table
        if ( (pps->weighted_pred_flag &&
              ((PREDSLICE == hdr->slice_type) || (S_PREDSLICE == hdr->slice_type))) ||
             ((pps->weighted_bipred_idc == 1) && (BPREDSLICE == hdr->slice_type)))
        {
            ps = GetPredWeightTable(hdr, sps, pPredWeight_L0, pPredWeight_L1);
            if (ps != UMC_OK)
                return ps;
        }

        // dec_ref_pic_marking
        pAdaptiveMarkingInfo->num_entries = 0;
        pBaseAdaptiveMarkingInfo->num_entries = 0;

        if (hdr->nal_ref_idc)
        {
            ps = DecRefPicMarking(hdr, pAdaptiveMarkingInfo);
            if (ps != UMC_OK)
            {
                return ps;
            }

            if (hdr->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && hdr->nal_ext.svc_extension_flag &&
                spsSvcExt && !spsSvcExt->slice_header_restriction_flag)
            {
                hdr->nal_ext.svc.store_ref_base_pic_flag = Get1Bit();
                if ((hdr->nal_ext.svc.use_ref_base_pic_flag ||
                    hdr->nal_ext.svc.store_ref_base_pic_flag) &&
                    (!hdr->nal_ext.svc.idr_flag))
                {
                    ps = DecRefBasePicMarking(pBaseAdaptiveMarkingInfo, hdr->nal_ext.svc.adaptive_ref_base_pic_marking_mode_flag);
                    if (ps != UMC_OK)
                        return ps;
                }
            }
        }    // def_ref_pic_marking
    }

    if (pps->entropy_coding_mode == 1  &&    // CABAC
        (hdr->slice_type != INTRASLICE && hdr->slice_type != S_INTRASLICE))
        hdr->cabac_init_idc = GetVLCElement(false);
    else
        hdr->cabac_init_idc = 0;

    if (hdr->cabac_init_idc > 2)
        return UMC_ERR_INVALID_STREAM;

    hdr->slice_qp_delta = GetVLCElement(true);

    if (S_PREDSLICE == hdr->slice_type ||
        S_INTRASLICE == hdr->slice_type)
    {
        if (S_PREDSLICE == hdr->slice_type)
            hdr->sp_for_switch_flag = Get1Bit();
        hdr->slice_qs_delta = GetVLCElement(true);
    }

    if (pps->deblocking_filter_variables_present_flag)
    {
        // deblock filter flag and offsets
        hdr->disable_deblocking_filter_idc = GetVLCElement(false);
        if (hdr->disable_deblocking_filter_idc > DEBLOCK_FILTER_ON_NO_SLICE_EDGES)
            return UMC_ERR_INVALID_STREAM;

        if (hdr->disable_deblocking_filter_idc != DEBLOCK_FILTER_OFF)
        {
            hdr->slice_alpha_c0_offset = GetVLCElement(true)<<1;
            hdr->slice_beta_offset = GetVLCElement(true)<<1;

            if (hdr->slice_alpha_c0_offset < -12 || hdr->slice_alpha_c0_offset > 12)
            {
                return UMC_ERR_INVALID_STREAM;
            }

            if (hdr->slice_beta_offset < -12 || hdr->slice_beta_offset > 12)
            {
                return UMC_ERR_INVALID_STREAM;
            }
        }
        else
        {
            // set filter offsets to max values to disable filter
            hdr->slice_alpha_c0_offset = (Ipp8s)(0 - QP_MAX);
            hdr->slice_beta_offset = (Ipp8s)(0 - QP_MAX);
        }
    }

    if (pps->num_slice_groups > 1 &&
        pps->SliceGroupInfo.slice_group_map_type >= 3 &&
        pps->SliceGroupInfo.slice_group_map_type <= 5)
    {
        Ipp32u num_bits;    // number of bits used to code slice_group_change_cycle
        Ipp32u val;
        Ipp32u pic_size_in_map_units;
        Ipp32u max_slice_group_change_cycle=0;

        // num_bits is Ceil(log2(picsizeinmapunits/slicegroupchangerate + 1))
        pic_size_in_map_units = sps->frame_width_in_mbs * sps->frame_height_in_mbs;
            // TBD: change above to support fields

        max_slice_group_change_cycle = pic_size_in_map_units /
                        pps->SliceGroupInfo.t2.slice_group_change_rate;
        if (pic_size_in_map_units %
                        pps->SliceGroupInfo.t2.slice_group_change_rate)
            max_slice_group_change_cycle++;

        val = max_slice_group_change_cycle;// + 1;
        num_bits = 0;
        while (val)
        {
            num_bits++;
            val >>= 1;
        }
        hdr->slice_group_change_cycle = GetBits(num_bits);
        if (hdr->slice_group_change_cycle > max_slice_group_change_cycle)
        {
            //return UMC_ERR_INVALID_STREAM; don't see any reasons for that
        }
    }

    CheckBSLeft();
    return UMC_OK;
} // GetSliceHeaderPart3()

Status H264HeadersBitstream::DecRefPicMarking(H264SliceHeader *hdr,        // slice header read goes here
                    AdaptiveMarkingInfo *pAdaptiveMarkingInfo)
{
    if (hdr->IdrPicFlag)
    {
        hdr->no_output_of_prior_pics_flag = Get1Bit();
        hdr->long_term_reference_flag = Get1Bit();
    }
    else
    {
        return DecRefBasePicMarking(pAdaptiveMarkingInfo, hdr->adaptive_ref_pic_marking_mode_flag);
    }

    return UMC_OK;
}

Status H264HeadersBitstream::GetSliceHeaderPart4(H264SliceHeader *hdr,
                                          const H264SeqParamSetSVCExtension *)
{
    hdr->scan_idx_start = 0;
    hdr->scan_idx_end = 15;
    return UMC_OK;
}// GetSliceHeaderPart4()

void H264HeadersBitstream::GetScalingList4x4(H264ScalingList4x4 *scl, Ipp8u *def, Ipp8u *scl_type)
{
    Ipp32u lastScale = 8;
    Ipp32u nextScale = 8;
    bool DefaultMatrix = false;
    Ipp32s j;

    for (j = 0; j < 16; j++ )
    {
        if (nextScale != 0)
        {
            Ipp32s delta_scale  = GetVLCElement(true);
            if (delta_scale < -128 || delta_scale > 127)
                throw h264_exception(UMC_ERR_INVALID_STREAM);
            nextScale = ( lastScale + delta_scale + 256 ) & 0xff;
            DefaultMatrix = ( j == 0 && nextScale == 0 );
        }
        scl->ScalingListCoeffs[ mp_scan4x4[0][j] ] = ( nextScale == 0 ) ? (Ipp8u)lastScale : (Ipp8u)nextScale;
        lastScale = scl->ScalingListCoeffs[ mp_scan4x4[0][j] ];
    }
    if (!DefaultMatrix)
    {
        *scl_type=SCLREDEFINED;
        return;
    }
    *scl_type= SCLDEFAULT;
    FillScalingList4x4(scl,def);
    return;
}

void H264HeadersBitstream::GetScalingList8x8(H264ScalingList8x8 *scl, Ipp8u *def, Ipp8u *scl_type)
{
    Ipp32u lastScale = 8;
    Ipp32u nextScale = 8;
    bool DefaultMatrix=false;
    Ipp32s j;

    for (j = 0; j < 64; j++ )
    {
        if (nextScale != 0)
        {
            Ipp32s delta_scale  = GetVLCElement(true);
            if (delta_scale < -128 || delta_scale > 127)
                throw h264_exception(UMC_ERR_INVALID_STREAM);
            nextScale = ( lastScale + delta_scale + 256 ) & 0xff;
            DefaultMatrix = ( j == 0 && nextScale == 0 );
        }
        scl->ScalingListCoeffs[ hp_scan8x8[0][j] ] = ( nextScale == 0 ) ? (Ipp8u)lastScale : (Ipp8u)nextScale;
        lastScale = scl->ScalingListCoeffs[ hp_scan8x8[0][j] ];
    }
    if (!DefaultMatrix)
    {
        *scl_type=SCLREDEFINED;
        return;
    }
    *scl_type= SCLDEFAULT;
    FillScalingList8x8(scl,def);
    return;

}

static
const Ipp32u GetBitsMask[25] =
{
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff
};

IppVCHuffmanSpec_32s *(H264Bitstream::m_tblCoeffToken[5]);
IppVCHuffmanSpec_32s *(H264Bitstream::m_tblRunBefore [16]);
IppVCHuffmanSpec_32s *(H264Bitstream::m_tblTotalZeros[16]);

IppVCHuffmanSpec_32s *(H264Bitstream::m_tblTotalZerosCR[4]);
IppVCHuffmanSpec_32s *(H264Bitstream::m_tblTotalZerosCR422[8]);

bool H264Bitstream::m_bTablesInited = false;                      // (bool) tables have been allocated

class TableInitializer
{
public:
    TableInitializer()
    {
        H264Bitstream::InitTables();
    }

    ~TableInitializer()
    {
        H264Bitstream::ReleaseTables();
    }
};

static TableInitializer tableInitializer;

void H264Bitstream::GetOrg(Ipp32u **pbs, Ipp32u *size)
{
    *pbs       = m_pbsBase;
    *size      = m_maxBsSize;
}

void H264Bitstream::GetState(Ipp32u** pbs,Ipp32u* bitOffset)
{
    *pbs       = m_pbs;
    *bitOffset = m_bitOffset;

} // H264Bitstream::GetState()

void H264Bitstream::SetState(Ipp32u* pbs,Ipp32u bitOffset)
{
    m_pbs = pbs;
    m_bitOffset = bitOffset;

} // H264Bitstream::GetState()

H264Bitstream::H264Bitstream(Ipp8u * const pb, const Ipp32u maxsize)
     : H264HeadersBitstream(pb, maxsize)
{
} // H264Bitstream::H264Bitstream(Ipp8u * const pb,

H264Bitstream::H264Bitstream()
    : H264HeadersBitstream()
{
} // H264Bitstream::H264Bitstream(void)

H264Bitstream::~H264Bitstream()
{
} // H264Bitstream::~H264Bitstream()

void H264Bitstream::ReleaseTables(void)
{
    Ipp32s i;

    if (!m_bTablesInited)
        return;

    for (i = 0; i <= 4; i++ )
    {
        if (m_tblCoeffToken[i])
        {
            ippiHuffmanTableFree_32s(m_tblCoeffToken[i]);
            m_tblCoeffToken[i] = NULL;
        }
    }

    for (i = 1; i <= 15; i++)
    {
        if (m_tblTotalZeros[i])
        {
            ippiHuffmanTableFree_32s(m_tblTotalZeros[i]);
            m_tblTotalZeros[i] = NULL;
        }
    }

    for(i = 1; i <= 3; i++)
    {
        if(m_tblTotalZerosCR[i])
        {
            ippiHuffmanTableFree_32s(m_tblTotalZerosCR[i]);
            m_tblTotalZerosCR[i] = NULL;
        }
    }

    for(i = 1; i <= 7; i++)
    {
        if(m_tblTotalZerosCR422[i])
        {
            ippiHuffmanTableFree_32s(m_tblTotalZerosCR422[i]);
            m_tblTotalZerosCR422[i] = NULL;
        }
    }

    for(i = 1; i <= 7; i++)
    {
        if(m_tblRunBefore[i])
        {
            ippiHuffmanTableFree_32s(m_tblRunBefore[i]);
            m_tblRunBefore[i] = NULL;
        }
    }

    for(; i <= 15; i++)
    {
        m_tblRunBefore[i] = NULL;
    }

    m_bTablesInited = false;

} // void H264Bitstream::ReleaseTable(void)

Status H264Bitstream::InitTables(void)
{
    IppStatus ippSts;

    // check tables allocation status
    if (m_bTablesInited)
        return UMC_OK;

    // release tables before initialization
    ReleaseTables();

    memset(m_tblCoeffToken, 0, sizeof(m_tblCoeffToken));
    memset(m_tblTotalZerosCR, 0, sizeof(m_tblTotalZerosCR));
    memset(m_tblTotalZerosCR422, 0, sizeof(m_tblTotalZerosCR422));
    memset(m_tblTotalZeros, 0, sizeof(m_tblTotalZeros));
    memset(m_tblRunBefore, 0, sizeof(m_tblRunBefore));

    // number Coeffs and Trailing Ones map tables allocation
    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_02, &m_tblCoeffToken[0]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_24, &m_tblCoeffToken[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_48, &m_tblCoeffToken[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_cr, &m_tblCoeffToken[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_cr2, &m_tblCoeffToken[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    //
    // TotalZeros tables allocation
    //

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr1, &m_tblTotalZerosCR[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr2, &m_tblTotalZerosCR[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr3, &m_tblTotalZerosCR[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_1, &m_tblTotalZerosCR422[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_2, &m_tblTotalZerosCR422[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_3, &m_tblTotalZerosCR422[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_4, &m_tblTotalZerosCR422[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_5, &m_tblTotalZerosCR422[5]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_6, &m_tblTotalZerosCR422[6]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_7, &m_tblTotalZerosCR422[7]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_1, &m_tblTotalZeros[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_2, &m_tblTotalZeros[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_3, &m_tblTotalZeros[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_4, &m_tblTotalZeros[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_5, &m_tblTotalZeros[5]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_6, &m_tblTotalZeros[6]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_7, &m_tblTotalZeros[7]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_8, &m_tblTotalZeros[8]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_9, &m_tblTotalZeros[9]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_10, &m_tblTotalZeros[10]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_11, &m_tblTotalZeros[11]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_12, &m_tblTotalZeros[12]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_13, &m_tblTotalZeros[13]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_14, &m_tblTotalZeros[14]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_15, &m_tblTotalZeros[15]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    //
    // Run Befores tables allocation
    //

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_1, &m_tblRunBefore[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_2, &m_tblRunBefore[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_3, &m_tblRunBefore[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_4, &m_tblRunBefore[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_5, &m_tblRunBefore[5]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_6, &m_tblRunBefore[6]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_6p, &m_tblRunBefore[7]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    m_tblRunBefore[8]  = m_tblRunBefore[7];
    m_tblRunBefore[9]  = m_tblRunBefore[7];
    m_tblRunBefore[10] = m_tblRunBefore[7];
    m_tblRunBefore[11] = m_tblRunBefore[7];
    m_tblRunBefore[12] = m_tblRunBefore[7];
    m_tblRunBefore[13] = m_tblRunBefore[7];
    m_tblRunBefore[14] = m_tblRunBefore[7];
    m_tblRunBefore[15] = m_tblRunBefore[7];

    m_bTablesInited = true;

    return UMC_OK;

} // Status H264Bitstream::InitTables(void)

void H264Bitstream::RollbackCurrentNALU()
{
    ippiUngetBits32(m_pbs,m_bitOffset);
}

// ---------------------------------------------------------------------------
//  H264Bitstream::GetNALUnitType()
//    Bitstream position is expected to be at the start of a NAL unit.
//    Read and return NAL unit type and NAL storage idc.
// ---------------------------------------------------------------------------
Status H264Bitstream::GetNALUnitType(NAL_Unit_Type &nal_unit_type, Ipp32u &nal_ref_idc)
{
    Ipp32u code;

    ippiGetBits8(m_pbs, m_bitOffset, code);

    nal_ref_idc = (Ipp32u) ((code & NAL_STORAGE_IDC_BITS)>>5);
    nal_unit_type = (NAL_Unit_Type) (code & NAL_UNITTYPE_BITS);
    return UMC_OK;

} // Status H264Bitstream::GetNALUnitType(NAL_Unit_Type &nal_unit_type, Ipp32u &nal_ref_idc)

// ---------------------------------------------------------------------------
//  H264Bitstream::GetAccessUnitDelimiter()
//    Read optional access unit delimiter from bitstream.
// ---------------------------------------------------------------------------
Status H264Bitstream::GetAccessUnitDelimiter(Ipp32u &PicCodType)
{
    PicCodType = GetBits(3);
    return UMC_OK;
}    // GetAccessUnitDelimiter

// ---------------------------------------------------------------------------
//  H264Bitstream::ReadFillerData()
//    Filler data RBSP, read and discard all bytes == 0xff
// ---------------------------------------------------------------------------
Status H264Bitstream::ReadFillerData()
{
    while (SearchBits(8, 0xff, 0));
    return UMC_OK;
}    // SkipFillerData


// ---------------------------------------------------------------------------
//        H264Bitstream::SearchBits()
//        Searches for a code with known number of bits.  Bitstream state,
//        pointer and bit offset, will be updated if code found.
//        nbits        : number of bits in the code
//        code        : code to search for
//        lookahead    : maximum number of bits to parse for the code
// ---------------------------------------------------------------------------
bool H264Bitstream::SearchBits(const Ipp32u nbits, const Ipp32u code, const Ipp32u lookahead)
{
    if (nbits >= sizeof(GetBitsMask)/sizeof(GetBitsMask[0]))
        return false;

    Ipp32u w;
    Ipp32u n = nbits;
    Ipp32u* pbs;
    Ipp32s offset;

    pbs    = m_pbs;
    offset = m_bitOffset;

    ippiGetNBits(m_pbs, m_bitOffset, n, w)

    for (n = 0; w != code && n < lookahead; n ++)
    {
        w = ((w << 1) & GetBitsMask[nbits]) | Get1Bit();
    }

    if (w == code)
        return true;
    else
    {
        m_pbs        = pbs;
        m_bitOffset = offset;
        return false;
    }

} // H264Bitstream::SearchBits()

inline
Ipp32u H264Bitstream::DecodeSingleBinOnes_CABAC(Ipp32u ctxIdx,
                                                Ipp32s &binIdx)
{
    // See subclause 9.3.3.2.1 of H.264 standard

    Ipp32u pStateIdx = context_array[ctxIdx].pStateIdxAndVal;
    Ipp32u codIOffset = m_lcodIOffset;
    Ipp32u codIRange = m_lcodIRange;
    Ipp32u codIRangeLPS;
    Ipp32u binVal;

    do
    {
        binIdx += 1;
        codIRangeLPS = rangeTabLPS[pStateIdx][(codIRange >> (6 + CABAC_MAGIC_BITS)) - 4];
        codIRange -= codIRangeLPS << CABAC_MAGIC_BITS;

        // most probably state.
        // it is more likely to decode most probably value.
        if (codIOffset < codIRange)
        {
            binVal = pStateIdx & 1;
            pStateIdx = transIdxMPS[pStateIdx];
        }
        else
        {
            codIOffset -= codIRange;
            codIRange = codIRangeLPS << CABAC_MAGIC_BITS;

            binVal = (pStateIdx & 1) ^ 1;
            pStateIdx = transIdxLPS[pStateIdx];
        }

        // Renormalization process
        // See subclause 9.3.3.2.2 of H.264
        {
            Ipp32u numBits = NumBitsToGetTbl[codIRange >> CABAC_MAGIC_BITS];

            codIRange <<= numBits;
            codIOffset <<= numBits;

#if (CABAC_MAGIC_BITS > 0)
            m_iMagicBits -= numBits;
            if (0 >= m_iMagicBits)
                RefreshCABACBits(codIOffset, m_pMagicBits, m_iMagicBits);
#else // !(CABAC_MAGIC_BITS > 0)
            codIOffset |= GetBits(numBits);
#endif // (CABAC_MAGIC_BITS > 0)
        }

#ifdef STORE_CABAC_BITS
        PRINT_CABAC_VALUES(binVal, codIRange>>CABAC_MAGIC_BITS);
#endif

    } while (binVal && (binIdx < 14));

    context_array[ctxIdx].pStateIdxAndVal = (Ipp8u) pStateIdx;
    m_lcodIOffset = codIOffset;
    m_lcodIRange = codIRange;

    return binVal;

} // Ipp32u H264Bitstream::DecodeSingleBinOnes_CABAC(Ipp32u ctxIdx,

inline
Ipp32u H264Bitstream::DecodeBypassOnes_CABAC(void)
{
    // See subclause 9.3.3.2.3 of H.264 standard
    Ipp32u binVal;// = 0;
    Ipp32u binCount = 0;
    Ipp32u codIOffset = m_lcodIOffset;
    Ipp32u codIRange = m_lcodIRange;

    do
    {
#if (CABAC_MAGIC_BITS > 0)
        codIOffset = (codIOffset << 1);

        m_iMagicBits -= 1;
        if (0 >= m_iMagicBits)
            RefreshCABACBits(codIOffset, m_pMagicBits, m_iMagicBits);
#else // !(CABAC_MAGIC_BITS > 0)
        codIOffset = (codIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        Ipp32s mask = ((Ipp32s)(codIRange)-1-(Ipp32s)(codIOffset))>>31;
        // conditionally negate level
        binVal = mask & 1;
        binCount += binVal;
        // conditionally subtract range from offset
        codIOffset -= codIRange & mask;

        if (binCount > 16) // too large prefix part
            throw h264_exception(UMC_ERR_INVALID_STREAM);

    } while(binVal);

    m_lcodIOffset = codIOffset;
    m_lcodIRange = codIRange;

    return binCount;

} // Ipp32u H264Bitstream::DecodeBypassOnes_CABAC(void)

static
Ipp8u iCtxIdxIncTable[64][2] =
{
    {1, 0},
    {2, 0},
    {3, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0}
};

Ipp32s H264Bitstream::DecodeSignedLevel_CABAC(Ipp32u ctxIdxOffset,
                                              Ipp32u &numDecodAbsLevelEq1,
                                              Ipp32u &numDecodAbsLevelGt1,
                                              Ipp32u max_value)
{
    // See subclause 9.3.2.3 of H.264
    Ipp32u ctxIdxInc;
    Ipp32s binIdx;

    // PREFIX BIN(S) STRING DECODING
    // decoding first bin of prefix bin string

    if (5 + numDecodAbsLevelGt1 < max_value)
    {
        ctxIdxInc = iCtxIdxIncTable[numDecodAbsLevelEq1][((Ipp32u) -((Ipp32s) numDecodAbsLevelGt1)) >> 31];

        if (0 == DecodeSingleBin_CABAC(ctxIdxOffset + ctxIdxInc))
        {
            numDecodAbsLevelEq1 += 1;
            binIdx = 1;
        }
        else
        {
            Ipp32s binVal;

            // decoding next bin(s) of prefix bin string
            // we use Truncated Unary binarization with cMax = uCoff;
            ctxIdxInc = 5 + numDecodAbsLevelGt1;
            binIdx = 1;

            binVal = DecodeSingleBinOnes_CABAC(ctxIdxOffset + ctxIdxInc, binIdx);

            // SUFFIX BIN(S) STRING DECODING

            // See subclause 9.1 of H.264 standard
            // we use Exp-Golomb code of 0-th order
            if (binVal)
            {
                Ipp32s leadingZeroBits;
                Ipp32s codeNum;

                // counting leading 1' before 0
                leadingZeroBits = DecodeBypassOnes_CABAC();

                // create codeNum
                codeNum = 1;
                while (leadingZeroBits--)
                    codeNum = (codeNum << 1) | DecodeBypass_CABAC();

                // update syntax element
                binIdx += codeNum;

            }

            numDecodAbsLevelGt1 += 1;
        }
    }
    else
    {
        if (0 == DecodeSingleBin_CABAC(ctxIdxOffset + 0))
        {
            numDecodAbsLevelEq1 += 1;
            binIdx = 1;
        }
        else
        {
            Ipp32s binVal;

            // decoding next bin(s) of prefix bin string
            // we use Truncated Unary binarization with cMax = uCoff;
            binIdx = 1;

            binVal = DecodeSingleBinOnes_CABAC(ctxIdxOffset + max_value, binIdx);

            // SUFFIX BIN(S) STRING DECODING

            // See subclause 9.1 of H.264 standard
            // we use Exp-Golomb code of 0-th order
            if (binVal)
            {
                Ipp32s leadingZeroBits;
                Ipp32s codeNum;

                // counting leading 1' before 0
                leadingZeroBits = DecodeBypassOnes_CABAC();

                // create codeNum
                codeNum = 1;
                while (leadingZeroBits--)
                    codeNum = (codeNum << 1) | DecodeBypass_CABAC();

                // update syntax element
                binIdx += codeNum;

            }

            numDecodAbsLevelGt1 += 1;
        }
    }

    {
        Ipp32s lcodIOffset = m_lcodIOffset;
        Ipp32s lcodIRange = m_lcodIRange;

        // See subclause 9.3.3.2.3 of H.264 standard
#if (CABAC_MAGIC_BITS > 0)
        // do shift on 1 bit
        lcodIOffset += lcodIOffset;

        {
            Ipp32s iMagicBits = m_iMagicBits;

            iMagicBits -= 1;
            if (0 >= iMagicBits)
                RefreshCABACBits(lcodIOffset, m_pMagicBits, iMagicBits);
            m_iMagicBits = iMagicBits;
        }
#else // !(CABAC_MAGIC_BITS > 0)
        lcodIOffset = (lcodIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        Ipp32s mask = ((Ipp32s)(lcodIRange) - 1 - (Ipp32s)(lcodIOffset)) >> 31;
        // conditionally negate level
        binIdx = (binIdx ^ mask) - mask;
        // conditionally subtract range from offset
        lcodIOffset -= lcodIRange & mask;

        m_lcodIOffset = lcodIOffset;
    }

    return binIdx;

} //Ipp32s H264Bitstream::DecodeSignedLevel_CABAC(Ipp32s ctxIdxOffset, Ipp32s &numDecodAbsLevelEq1, Ipp32s &numDecodAbsLevelGt1)

//
// this is a limited version of the DecodeSignedLevel_CABAC function.
// it decodes single value per block.
//
Ipp32s H264Bitstream::DecodeSingleSignedLevel_CABAC(Ipp32u ctxIdxOffset)
{
    // See subclause 9.3.2.3 of H.264
    Ipp32u ctxIdxInc;
    Ipp32s binIdx;

    // PREFIX BIN(S) STRING DECODING
    // decoding first bin of prefix bin string

    {
        ctxIdxInc = iCtxIdxIncTable[0][0];

        if (0 == DecodeSingleBin_CABAC(ctxIdxOffset + ctxIdxInc))
        {
            binIdx = 1;
        }
        else
        {
            Ipp32s binVal;

            // decoding next bin(s) of prefix bin string
            // we use Truncated Unary binarization with cMax = uCoff;
            ctxIdxInc = 5;
            binIdx = 1;

            binVal = DecodeSingleBinOnes_CABAC(ctxIdxOffset + ctxIdxInc, binIdx);

            // SUFFIX BIN(S) STRING DECODING

            // See subclause 9.1 of H.264 standard
            // we use Exp-Golomb code of 0-th order
            if (binVal)
            {
                Ipp32s leadingZeroBits;
                Ipp32s codeNum;

                // counting leading 1' before 0
                leadingZeroBits = DecodeBypassOnes_CABAC();

                // create codeNum
                codeNum = 1;
                while (leadingZeroBits--)
                    codeNum = (codeNum << 1) | DecodeBypass_CABAC();

                // update syntax element
                binIdx += codeNum;

            }
        }
    }

    {
        Ipp32s lcodIOffset = m_lcodIOffset;
        Ipp32s lcodIRange = m_lcodIRange;

        // See subclause 9.3.3.2.3 of H.264 standard
#if (CABAC_MAGIC_BITS > 0)
        // do shift on 1 bit
        lcodIOffset += lcodIOffset;

        {
            Ipp32s iMagicBits = m_iMagicBits;

            iMagicBits -= 1;
            if (0 >= iMagicBits)
                RefreshCABACBits(lcodIOffset, m_pMagicBits, iMagicBits);
            m_iMagicBits = iMagicBits;
        }
#else // !(CABAC_MAGIC_BITS > 0)
        lcodIOffset = (lcodIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        Ipp32s mask = ((Ipp32s)(lcodIRange) - 1 - (Ipp32s)(lcodIOffset)) >> 31;
        // conditionally negate level
        binIdx = (binIdx ^ mask) - mask;
        // conditionally subtract range from offset
        lcodIOffset -= lcodIRange & mask;

        m_lcodIOffset = lcodIOffset;
    }

    return binIdx;

} //Ipp32s H264Bitstream::DecodeSingleSignedLevel_CABAC(Ipp32s ctxIdxOffset)

void H264Bitstream::SetDecodedBytes(size_t nBytes)
{
    m_pbs = m_pbsBase + (nBytes / 4);
    m_bitOffset = 31 - ((Ipp32s) ((nBytes % sizeof(Ipp32u)) * 8));
} // void H264Bitstream::SetDecodedBytes(size_t nBytes)

void SetDefaultScalingLists(H264SeqParamSet * sps)
{
    Ipp32s i;

    for (i = 0; i < 6; i += 1)
    {
        FillFlatScalingList4x4(&sps->ScalingLists4x4[i]);
    }
    for (i = 0; i < 2; i += 1)
    {
        FillFlatScalingList8x8(&sps->ScalingLists8x8[i]);
    }
}

} // namespace UMC

#define IPP_NOERROR_RET()  return ippStsNoErr
#define IPP_ERROR_RET( ErrCode )  return (ErrCode)

#define IPP_BADARG_RET( expr, ErrCode )\
            {if (expr) { IPP_ERROR_RET( ErrCode ); }}

#define IPP_BAD_PTR1_RET( ptr )\
            IPP_BADARG_RET( NULL==(ptr), ippStsNullPtrErr )

STRUCT_DECLSPEC_ALIGN static const Ipp32u vlc_inc[] = {0,3,6,12,24,48,96};

typedef struct{
    Ipp16s bits;
    Ipp16s offsets;
}  BitsAndOffsets;

STRUCT_DECLSPEC_ALIGN static BitsAndOffsets bitsAndOffsets[7][16] = /*[level][numZeros]*/
{
/*         0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15        */
    /*0*/    {{0, 1}, {0, 1},  {0, 1},  {0, 1},  {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {4, 8},   {12, 16}, },
    /*1*/    {{1, 1}, {1, 2},  {1, 3},  {1, 4},  {1, 5},   {1, 6},   {1, 7},   {1, 8},   {1, 9},   {1, 10},  {1, 11},  {1, 12},  {1, 13},  {1, 14},  {1, 15},  {12, 16}, },
    /*2*/    {{2, 1}, {2, 3},  {2, 5},  {2, 7},  {2, 9},   {2, 11},  {2, 13},  {2, 15},  {2, 17},  {2, 19},  {2, 21},  {2, 23},  {2, 25},  {2, 27},  {2, 29},  {12, 31}, },
    /*3*/    {{3, 1}, {3, 5},  {3, 9},  {3, 13}, {3, 17},  {3, 21},  {3, 25},  {3, 29},  {3, 33},  {3, 37},  {3, 41},  {3, 45},  {3, 49},  {3, 53},  {3, 57},  {12, 61}, },
    /*4*/    {{4, 1}, {4, 9},  {4, 17}, {4, 25}, {4, 33},  {4, 41},  {4, 49},  {4, 57},  {4, 65},  {4, 73},  {4, 81},  {4, 89},  {4, 97},  {4, 105}, {4, 113}, {12, 121}, },
    /*5*/    {{5, 1}, {5, 17}, {5, 33}, {5, 49}, {5, 65},  {5, 81},  {5, 97},  {5, 113}, {5, 129}, {5, 145}, {5, 161}, {5, 177}, {5, 193}, {5, 209}, {5, 225}, {12, 241}, },
    /*6*/    {{6, 1}, {6, 33}, {6, 65}, {6, 97}, {6, 129}, {6, 161}, {6, 193}, {6, 225}, {6, 257}, {6, 289}, {6, 321}, {6, 353}, {6, 385}, {6, 417}, {6, 449}, {12, 481}  }
};


typedef struct{
    Ipp8u bits;

    union{
    Ipp8s q;
    Ipp8u qqqqq[3];
    };

} qq;

STRUCT_DECLSPEC_ALIGN static Ipp32s sadd[7]={15,0,0,0,0,0,0};


inline
void _GetBlockCoeffs_CAVLC(Ipp32u ** const & ppBitStream,
                           Ipp32s * & pOffset,
                           Ipp32s uCoeffIndex,
                           Ipp32s sNumCoeff,
                           Ipp32s sNumTrailingOnes,
                           Ipp32s *CoeffBuf)
{
    Ipp32u uCoeffLevel = 0;

    /* 0..6, to select coding method used for each coeff */
    Ipp32s suffixLength = (sNumCoeff > 10) && (sNumTrailingOnes < 3) ? 1 : 0;

    /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
    Ipp32s uFirstAdjust = ((sNumTrailingOnes < 3) ? 1 : 0);

    //if (suffixLength < 6)
    {
        if (uCoeffLevel > vlc_inc[suffixLength])
            suffixLength++;
    }

    Ipp32s NumZeros = -1;
    for (Ipp32s w = 0; !w; NumZeros++)
    {
        ippiGetBits1((*ppBitStream), (*pOffset), w);
    }

    if (15 >= NumZeros)
    {
        const BitsAndOffsets &q = bitsAndOffsets[suffixLength][NumZeros];

        if (q.bits)
        {
            ippiGetNBits((*ppBitStream), (*pOffset), q.bits, NumZeros);
        }

        uCoeffLevel = ((NumZeros>>1) + q.offsets + uFirstAdjust);

        CoeffBuf[uCoeffIndex] = ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
    }
    else
    {
        Ipp32u level_suffix;
        Ipp32u levelSuffixSize = NumZeros - 3;
        Ipp32s levelCode;

        ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
        levelCode = ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
        levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

        CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                          ((-levelCode - 1) >> 1) :
                                          ((levelCode + 2) >> 1));

        uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
    }

    uCoeffIndex++;
    if (uCoeffLevel > 3)
        suffixLength = 2;
    else if (uCoeffLevel > vlc_inc[suffixLength])
       suffixLength++;

    /* read coeffs */
    for (; uCoeffIndex < sNumCoeff; uCoeffIndex++)
    {
        /* Get the number of leading zeros to determine how many more */
        /* bits to read. */
        Ipp32s zeros = -1;
        for (Ipp32s w = 0; !w; zeros++)
        {
            ippiGetBits1((*ppBitStream), (*pOffset), w);
        }

        if (15 >= zeros)
        {
            const BitsAndOffsets &q = bitsAndOffsets[suffixLength][zeros];

            if (q.bits)
            {
                ippiGetNBits((*ppBitStream), (*pOffset), q.bits, zeros);
            }

            uCoeffLevel = ((zeros>>1) + q.offsets);

            CoeffBuf[uCoeffIndex] = ((zeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
        }
        else
        {
            Ipp32u level_suffix;
            Ipp32u levelSuffixSize = zeros - 3;
            Ipp32s levelCode;

            ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
            levelCode = ((IPP_MIN(15, zeros) << suffixLength) + level_suffix) + sadd[suffixLength];
            levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

            CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                              ((-levelCode - 1) >> 1) :
                                              ((levelCode + 2) >> 1));

            uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
        }

        if (uCoeffLevel > vlc_inc[suffixLength] && suffixLength < 6)
        {
            suffixLength++;
        }
    }    /* for uCoeffIndex */

} /* static void _GetBlockCoeffs_CAVLC(Ipp32u **pbs, */

STRUCT_DECLSPEC_ALIGN static Ipp8s trailing_ones[8][3] =
{
    {1, 1, 1},    // 0, 0, 0
    {1, 1, -1},   // 0, 0, 1
    {1, -1, 1},   // 0, 1, 0
    {1, -1, -1},  // 0, 1, 1
    {-1, 1, 1},   // 1, 0, 0
    {-1, 1, -1},  // 1, 0, 1
    {-1, -1, 1},  // 1, 1, 0
    {-1, -1, -1}, // 1, 1, 1
};

STRUCT_DECLSPEC_ALIGN static Ipp8s trailing_ones1[8][3] =
{
    {1, 1, 1},    // 0, 0, 0
    {-1, 1, 1},   // 0, 0, 1
    {1, -1, 1},   // 0, 1, 0
    {-1, -1, 1},  // 0, 1, 1
    {1, 1, -1},   // 1, 0, 0
    {-1, 1, -1},  // 1, 0, 1
    {1, -1, -1},  // 1, 1, 0
    {-1, -1, -1}, // 1, 1, 1
};

static Ipp32s bitsToGetTbl16s[7][16] = /*[level][numZeros]*/
{
/*         0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15        */
/*0*/    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 12, },
/*1*/    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 12, },
/*2*/    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 12, },
/*3*/    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 12, },
/*4*/    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 12, },
/*5*/    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 12, },
/*6*/    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 12  }
};
static Ipp32s addOffsetTbl16s[7][16] = /*[level][numZeros]*/
{
/*         0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15    */
/*0*/    {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  8,  16,},
/*1*/    {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,},
/*2*/    {1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,},
/*3*/    {1,  5,  9,  13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61,},
/*4*/    {1,  9,  17, 25, 33, 41, 49, 57, 65, 73, 81, 89, 97, 105,113,121,},
/*5*/    {1,  17, 33, 49, 65, 81, 97, 113,129,145,161,177,193,209,225,241,},
/*6*/    {1,  33, 65, 97, 129,161,193,225,257,289,321,353,385,417,449,481,}
};

#define OWNV_ABS(v) \
    (((v) >= 0) ? (v) : -(v))

IppStatus ownippiDecodeCAVLCCoeffs_H264_1u16s (Ipp32u **ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp16s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s **ppTblCoeffToken,
                                                     const Ipp32s **ppTblTotalZeros,
                                                     const Ipp32s **ppTblRunBefore,
                                                     const Ipp32s *pScanMatrix) /* buffer to return up to 16 */

{
    Ipp16s        CoeffBuf[16];    /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32u        uVLCIndex        = 2;
    Ipp32u        uCoeffIndex        = 0;
    Ipp32s        sTotalZeros        = 0;
    Ipp32s        sFirstPos        = 16 - uMaxNumCoeff;
    Ipp32u        TrOneSigns = 0;        /* return sign bits (1==neg) in low 3 bits*/
    Ipp32u        uTR1Mask;
    Ipp32s        pos;
    Ipp32s        sRunBefore;
    Ipp16s        sNumTrailingOnes;
    Ipp32s        sNumCoeff = 0;
    Ipp32u        table_bits;
    Ipp8u         code_len;
    Ipp32s        i;
    register Ipp32u  table_pos;
    register Ipp32s  val;
    Ipp32u *pbsBackUp;
    Ipp32s bsoffBackUp;

    /* check error(s) */
    //IPP_BAD_PTR4_RET(ppBitStream,pOffset,ppPosCoefbuf,pNumCoeff)
    //IPP_BAD_PTR4_RET(ppTblCoeffToken,ppTblTotalZeros,ppTblRunBefore,pScanMatrix)
    //IPP_BAD_PTR2_RET(*ppBitStream, *ppPosCoefbuf)
    IPP_BADARG_RET(((sFirstPos != 0 && sFirstPos != 1) || (Ipp32s)uVLCSelect < 0), ippStsOutOfRangeErr)

    /* make a bit-stream backup */
    pbsBackUp = *ppBitStream;
    bsoffBackUp = *pOffset;

    if (uVLCSelect > 7)
    {
        /* fixed length code 4 bits numCoeff and */
        /* 2 bits for TrailingOnes */
        h264GetNBits((*ppBitStream), (*pOffset), 6, sNumCoeff);
        sNumTrailingOnes = (Ipp16s) (sNumCoeff & 3);
        sNumCoeff         = (sNumCoeff&0x3c)>>2;
        if (sNumCoeff == 0 && sNumTrailingOnes == 3)
            sNumTrailingOnes = 0;
        else
            sNumCoeff++;
    }
    else
    {
        const Ipp32s *pDecTable;
        /* Use one of 3 luma tables */
        if (uVLCSelect < 4)
            uVLCIndex = uVLCSelect>>1;

        /* check for the only codeword of all zeros */
        /*ippiDecodeVLCPair_32s(ppBitStream, pOffset, ppTblCoeffToken[uVLCIndex], */
        /*                                        &sNumTrailingOnes, &sNumCoeff); */

        pDecTable = ppTblCoeffToken[uVLCIndex];

        IPP_BAD_PTR1_RET(ppTblCoeffToken[uVLCIndex]);

        table_bits = *pDecTable;
        h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val = pDecTable[table_pos + 1];
        code_len = (Ipp8u) (val);

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (Ipp8u) (val & 0xff);
        }

        h264UngetNBits((*ppBitStream), (*pOffset), code_len);

        if ((val>>8) == IPPVC_VLC_FORBIDDEN)
        {
             *ppBitStream = pbsBackUp;
             *pOffset = bsoffBackUp;

             return ippStsH263VLCCodeErr;
        }

        sNumTrailingOnes  = (Ipp16s) ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    *pNumCoeff = (Ipp16s) sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (Ipp16s) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
            uTR1Mask >>= 1;
        }
    }
    if (sNumCoeff)
    {
#ifdef __ICL
#pragma vector always
#endif
        for (i = 0; i < 16; i++)
            (*ppPosCoefbuf)[i] = 0;

        /* Get the sign bits of any trailing one coeffs */
        /* and put signed coeffs to the buffer */
        /* Get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            /*_GetBlockCoeffs_CAVLC(ppBitStream, pOffset,sNumCoeff,*/
            /*                             sNumTrailingOnes, &CoeffBuf[uCoeffIndex]); */
            Ipp16u suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
            Ipp16s lCoeffIndex;
            Ipp16u uCoeffLevel = 0;
            Ipp32s NumZeros;
            Ipp16u uBitsToGet;
            Ipp16u uFirstAdjust;
            Ipp16u uLevelOffset;
            Ipp32s w;
            Ipp16s    *lCoeffBuf = &CoeffBuf[uCoeffIndex];

            if ((sNumCoeff > 10) && (sNumTrailingOnes < 3))
                suffixLength = 1;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            uFirstAdjust = (Ipp16u) ((sNumTrailingOnes < 3) ? 1 : 0);

            /* read coeffs */
            for (lCoeffIndex = 0; lCoeffIndex<(sNumCoeff - sNumTrailingOnes); lCoeffIndex++)
            {
                /* update suffixLength */
                if ((lCoeffIndex == 1) && (uCoeffLevel > 3))
                    suffixLength = 2;
                else if (suffixLength < 6)
                {
                    if (uCoeffLevel > vlc_inc[suffixLength])
                        suffixLength++;
                }

                /* Get the number of leading zeros to determine how many more */
                /* bits to read. */
                NumZeros = -1;
                for (w = 0; !w; NumZeros++)
                {
                    h264GetBits1((*ppBitStream), (*pOffset), w);

                    if (NumZeros > 32)
                    {
                         *ppBitStream = pbsBackUp;
                         *pOffset = bsoffBackUp;

                         return ippStsH263VLCCodeErr;
                    }
                }

                if (15 >= NumZeros)
                {
                    uBitsToGet = (Ipp16s) (bitsToGetTbl16s[suffixLength][NumZeros]);
                    uLevelOffset = (Ipp16u) (addOffsetTbl16s[suffixLength][NumZeros]);

                    if (uBitsToGet)
                    {
                        h264GetNBits((*ppBitStream), (*pOffset), uBitsToGet, NumZeros);
                    }

                    uCoeffLevel = (Ipp16u) ((NumZeros>>1) + uLevelOffset + uFirstAdjust);

                    lCoeffBuf[lCoeffIndex] = (Ipp16s) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
                }
                else
                {
                    Ipp32u level_suffix;
                    Ipp32u levelSuffixSize = NumZeros - 3;
                    Ipp32s levelCode;

                    h264GetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = (Ipp16u) ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
                    levelCode = (Ipp16u) (levelCode + (1 << (NumZeros - 3)) - 4096);

                    lCoeffBuf[lCoeffIndex] = (Ipp16s) ((levelCode & 1) ?
                                                      ((-levelCode - 1) >> 1) :
                                                      ((levelCode + 2) >> 1));

                    uCoeffLevel = (Ipp16u) OWNV_ABS(lCoeffBuf[lCoeffIndex]);
                }

                uFirstAdjust = 0;

            }    /* for uCoeffIndex */

        }
        /* Get TotalZeros if any */
        if (sNumCoeff < uMaxNumCoeff)
        {
            /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sTotalZeros, */
            /*                                                ppTblTotalZeros[sNumCoeff]); */
            const Ipp32s *pDecTable = ppTblTotalZeros[sNumCoeff];

            IPP_BAD_PTR1_RET(ppTblTotalZeros[sNumCoeff]);

            table_bits = pDecTable[0];
            h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            val = pDecTable[table_pos + 1];
            code_len = (Ipp8u) (val & 0xff);
            val = val >> 8;

            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val = pDecTable[table_pos + val + 1];
                code_len = (Ipp8u) (val & 0xff);
                val = val >> 8;
            }

            if (val == IPPVC_VLC_FORBIDDEN)
            {
                *ppBitStream = pbsBackUp;
                *pOffset = bsoffBackUp;

                return ippStsH263VLCCodeErr;
            }

            h264UngetNBits((*ppBitStream), (*pOffset), code_len)

            sTotalZeros = val;
        }

        uCoeffIndex = 0;
        while (sNumCoeff)
        {
            /* Get RunBerore if any */
            if ((sNumCoeff > 1) && (sTotalZeros > 0))
            {
                /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sRunBefore, */
                /*                                                ppTblRunBefore[sTotalZeros]); */
                const Ipp32s *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros]);

                table_bits = pDecTable[0];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (Ipp8u) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (Ipp8u) (val & 0xff);
                    val        = val >> 8;
                }

                if (val == IPPVC_VLC_FORBIDDEN)
                {
                    *ppBitStream = pbsBackUp;
                    *pOffset = bsoffBackUp;

                    return ippStsH263VLCCodeErr;
                }

                h264UngetNBits((*ppBitStream), (*pOffset),code_len)

                sRunBefore =  val;
            }
            else
                sRunBefore = sTotalZeros;

            /*Put coeff to the buffer */
            pos             = sNumCoeff - 1 + sTotalZeros + sFirstPos;
            sTotalZeros -= IPP_MIN(sTotalZeros, sRunBefore);
            pos             = pScanMatrix[pos];

            (*ppPosCoefbuf)[pos] = CoeffBuf[(uCoeffIndex++)&0x0f];
            sNumCoeff--;
        }
        (*ppPosCoefbuf) += 16;
    }

    return ippStsNoErr;

}

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u16s (Ipp32u ** const ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp16s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s ,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s *pScanMatrix,
                                                     Ipp32s )

{
    Ipp32s **ppTblTotalZeros;

    if (uMaxNumCoeff <= 4)
    {
        ppTblTotalZeros = UMC::H264Bitstream::m_tblTotalZerosCR;

    }
    else if (uMaxNumCoeff <= 8)
    {
        ppTblTotalZeros = UMC::H264Bitstream::m_tblTotalZerosCR422;
    }
    else
    {
        ppTblTotalZeros = UMC::H264Bitstream::m_tblTotalZeros;
    }

    return ownippiDecodeCAVLCCoeffs_H264_1u16s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZeros,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix);


#if 0
#if 1
    return own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const Ipp32s **) ppTblTotalZeros,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix,
                                                     scanIdxStart,
                                                     coeffLimit + scanIdxStart - 1);
#else
    Ipp32s        CoeffBuf[16];    /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32s        uVLCIndex        = 2;
    Ipp32s        sTotalZeros        = 0;
    Ipp32s        sRunBefore;
    Ipp32s        sNumTrailingOnes;
    Ipp32s        sNumCoeff = 0;

    if (uVLCSelect > 7)
    {
        /* fixed length code 4 bits numCoeff and */
        /* 2 bits for TrailingOnes */
        ippiGetNBits((*ppBitStream), (*pOffset), 6, sNumCoeff);
        sNumTrailingOnes = (sNumCoeff & 3);
        sNumCoeff         = (sNumCoeff&0x3c)>>2;
        if (sNumCoeff == 0 && sNumTrailingOnes == 3)
            sNumTrailingOnes = 0;
        else
            sNumCoeff++;
    }
    else
    {
        /* Use one of 3 luma tables */
        if (uVLCSelect < 4)
            uVLCIndex = uVLCSelect>>1;

        /* check for the only codeword of all zeros */

        const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblCoeffToken[uVLCIndex];

        //IPP_BAD_PTR1_RET(ppTblCoeffToken[uVLCIndex]);

        Ipp32s table_pos;
        Ipp32s table_bits = *pDecTable;
        ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        Ipp32s val = pDecTable[table_pos + 1];
        Ipp8u code_len = (Ipp8u)val;

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (Ipp8u)(val & 0xff);
        }

        ippiUngetNBits((*ppBitStream), (*pOffset), code_len);

        sNumTrailingOnes  = ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    *pNumCoeff = (Ipp16s)sNumCoeff;

    if (sNumCoeff)
    {
        Ipp32s uCoeffIndex = 0;

        if (sNumTrailingOnes)
        {
            Ipp32u TrOneSigns;
            ippiGetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
            Ipp32u uTR1Mask = 1 << (sNumTrailingOnes - 1);
            while (uTR1Mask)
            {
                CoeffBuf[uCoeffIndex++] = ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
                uTR1Mask >>= 1;
            }
        }
#ifdef __ICL
#pragma vector always
#endif
        //for (Ipp32s i = 0; i < 16; i++)
          //  (*ppPosCoefbuf)[i] = 0;

        memset(*ppPosCoefbuf, 0, 16*sizeof(Ipp16s));

        /* Get the sign bits of any trailing one coeffs */
        /* and put signed coeffs to the buffer */
        /* Get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff != sNumTrailingOnes)
        {
#if 1
            _GetBlockCoeffs_CAVLC(ppBitStream, pOffset, uCoeffIndex, sNumCoeff, sNumTrailingOnes, CoeffBuf);
#else
            Ipp32u uCoeffLevel = 0;

            /* 0..6, to select coding method used for each coeff */
            Ipp32s suffixLength = (sNumCoeff > 10) && (sNumTrailingOnes < 3) ? 1 : 0;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            Ipp32s uFirstAdjust = ((sNumTrailingOnes < 3) ? 1 : 0);

            if (suffixLength < 6)
            {
                if (uCoeffLevel > vlc_inc[suffixLength])
                    suffixLength++;
            }


            Ipp32s NumZeros = -1;
            for (Ipp32s w = 0; !w; NumZeros++)
            {
                ippiGetBits1((*ppBitStream), (*pOffset), w);
            }

            if (15 >= NumZeros)
            {
                const BitsAndOffsets &q = bitsAndOffsets[suffixLength][NumZeros];

                if (q.bits)
                {
                    ippiGetNBits((*ppBitStream), (*pOffset), q.bits, NumZeros);
                }

                uCoeffLevel = ((NumZeros>>1) + q.offsets + uFirstAdjust);

                CoeffBuf[uCoeffIndex] = ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
            }
            else
            {
                Ipp32u level_suffix;
                Ipp32u levelSuffixSize = NumZeros - 3;
                Ipp32s levelCode;

                ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                levelCode = ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
                levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

                CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                                  ((-levelCode - 1) >> 1) :
                                                  ((levelCode + 2) >> 1));

                uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
            }

            uCoeffIndex++;
            if (uCoeffLevel > 3)
                suffixLength = 2;
            else if (uCoeffLevel > vlc_inc[suffixLength])
               suffixLength++;

            /* read coeffs */
            for (; uCoeffIndex < sNumCoeff; uCoeffIndex++)
            {
                /* Get the number of leading zeros to determine how many more */
                /* bits to read. */
                Ipp32s NumZeros = -1;
                for (Ipp32s w = 0; !w; NumZeros++)
                {
                    ippiGetBits1((*ppBitStream), (*pOffset), w);
                }

                if (15 >= NumZeros)
                {
                    const BitsAndOffsets &q = bitsAndOffsets[suffixLength][NumZeros];

                    if (q.bits)
                    {
                        ippiGetNBits((*ppBitStream), (*pOffset), q.bits, NumZeros);
                    }

                    uCoeffLevel = ((NumZeros>>1) + q.offsets);

                    CoeffBuf[uCoeffIndex] = ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
                }
                else
                {
                    Ipp32u level_suffix;
                    Ipp32u levelSuffixSize = NumZeros - 3;
                    Ipp32s levelCode;

                    ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + sadd[suffixLength];
                    levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

                    CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                                      ((-levelCode - 1) >> 1) :
                                                      ((levelCode + 2) >> 1));

                    uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
                }

                if (uCoeffLevel > vlc_inc[suffixLength] && suffixLength < 6)
                {
                    suffixLength++;
                }
            }    /* for uCoeffIndex */
#endif
        }

        uCoeffIndex = 0;

        Ipp32s sFirstPos        = 16 - uMaxNumCoeff;

        /* Get TotalZeros if any */
        if (sNumCoeff != uMaxNumCoeff)
        {
            const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblTotalZeros[sNumCoeff];

            Ipp32s table_pos;
            Ipp32s table_bits = pDecTable[0];
            ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

            ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

            sTotalZeros = qqq->q;

            for (; sTotalZeros > 0 && sNumCoeff > 1; sNumCoeff--)
            {
                const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblRunBefore[sTotalZeros];
                Ipp32s table_pos;
                Ipp32s table_bits = pDecTable[0];

                ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

                ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

                sRunBefore =  qqq->q;

                Ipp32s pos  = sNumCoeff - 1 + sTotalZeros + sFirstPos;
                pos         = pScanMatrix[pos];

                (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];

                sTotalZeros -= sRunBefore;
            }
        }

        for (; sNumCoeff; sNumCoeff--)
        {
            Ipp32s pos  = sNumCoeff - 1 + sTotalZeros + sFirstPos;
            pos         = pScanMatrix[pos];
            (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];
        }

        (*ppPosCoefbuf) += 16;
    }

    return ippStsNoErr;
#endif
#endif
} /* IPPFUN(IppStatus, ippiDecodeCAVLCCoeffs_H264_1u16s, (Ipp32u **ppBitStream, */

#define h264PeekBitsNoCheck(current_data, offset, nbits, data) \
{ \
    Ipp32u x; \
    x = current_data[0] >> (offset - nbits + 1); \
    (data) = x; \
}

#define h264PeekBits(current_data, offset, nbits, data) \
{ \
    Ipp32u x; \
    Ipp32s off; \
    /*removeSCEBP(current_data, offset);*/ \
    off = offset - (nbits); \
    if (off >= 0) \
    { \
        x = (current_data[0] >> (off + 1)); \
    } \
    else \
    { \
        x = (current_data[1] >> (off + 32)); \
        x >>= 1; \
        x |= (current_data[0] << (- 1 + -off)); \
    } \
    (data) = x; \
}

#define h264DropBits(current_data, offset, nbits) \
{ \
    offset -= (nbits); \
    if (offset < 0) \
    { \
        offset += 32; \
        current_data++; \
    } \
}

static
Ipp8s ChromaDCRunTable[] =
{
    3, 5,
    2, 5,
    1, 4, 1, 4,
    0, 3, 0, 3, 0, 3, 0, 3
};

static
IppStatus _GetBlockCoeffs_CAVLC(Ipp32u **pbs,
                           Ipp32s *bitOffset,
                           Ipp16s sNumCoeff,
                           Ipp16s sNumTrOnes,
                           Ipp16s *CoeffBuf)
{
    Ipp16u suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
    Ipp16s uCoeffIndex;
    Ipp32u uCoeffLevel = 0;
    Ipp32s NumZeros;
    Ipp16u uBitsToGet;
    Ipp16u uFirstAdjust;
    Ipp16u uLevelOffset;
    Ipp32s w;

    if ((sNumCoeff > 10) && (sNumTrOnes < 3))
        suffixLength = 1;

    /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
    uFirstAdjust = (Ipp16u)((sNumTrOnes < 3) ? 1 : 0);

    /* read coeffs */
    for (uCoeffIndex = 0; uCoeffIndex<(sNumCoeff - sNumTrOnes); uCoeffIndex++)
    {
        /* update suffixLength */
        if ((uCoeffIndex == 1) && (uCoeffLevel > 3))
            suffixLength = 2;
        else if (suffixLength < 6)
        {
            if (uCoeffLevel > vlc_inc[suffixLength])
                suffixLength++;
        }

        /* Get the number of leading zeros to determine how many more */
        /* bits to read. */
        NumZeros = -1;
        for(w = 0; !w; NumZeros++)
        {
            h264GetBits1((*pbs), (*bitOffset), w);
            if (NumZeros > 32)
            {
                return ippStsH263VLCCodeErr;
            }
        }

        if (15 >= NumZeros)
        {
            uBitsToGet     = (Ipp16u)(bitsToGetTbl16s[suffixLength][NumZeros]);
            uLevelOffset   = (Ipp16u)(addOffsetTbl16s[suffixLength][NumZeros]);

            if (uBitsToGet)
            {
                h264GetNBits((*pbs), (*bitOffset), uBitsToGet, NumZeros);
            }

            uCoeffLevel = (NumZeros>>1) + uLevelOffset + uFirstAdjust;

            CoeffBuf[uCoeffIndex] = (Ipp16s) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
        }
        else
        {
            Ipp32u level_suffix;
            Ipp32u levelSuffixSize = NumZeros - 3;
            Ipp32s levelCode;

            h264GetNBits((*pbs), (*bitOffset), levelSuffixSize, level_suffix);
            levelCode = (Ipp16u) ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
            levelCode = (Ipp16u) (levelCode + (1 << (NumZeros - 3)) - 4096);

            CoeffBuf[uCoeffIndex] = (Ipp16s) ((levelCode & 1) ?
                                              ((-levelCode - 1) >> 1) :
                                              ((levelCode + 2) >> 1));

            uCoeffLevel = OWNV_ABS(CoeffBuf[uCoeffIndex]);
        }

        uFirstAdjust = 0;

    } /* for uCoeffIndex*/

    return ippStsNoErr;

} /* static void _GetBlockCoeffs_CAVLC(Ipp32u **pbs, */

IppStatus ownippiDecodeCAVLCChromaDcCoeffs_H264_1u16s (Ipp32u **ppBitStream,
                                                             Ipp32s *pOffset,
                                                             Ipp16s *pNumCoeff,
                                                             Ipp16s **ppPosCoefbuf,
                                                             const Ipp32s *pTblCoeffToken,
                                                             const Ipp32s **ppTblTotalZerosCR,
                                                             const Ipp32s **ppTblRunBefore)

{
    /* check the most frequently used parameters */
    //IPP_BAD_PTR3_RET(ppBitStream, pOffset, pNumCoeff)

    if (4 < *pOffset)
    {
        Ipp32u code;

        h264PeekBitsNoCheck((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;
            Ipp16s *pCoeffs;

            IPP_BAD_PTR1_RET(ppPosCoefbuf)

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            *pOffset -= ChromaDCRunTable[(code & 0x07) * 2 + 1];
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            *pOffset -= 2;

            return ippStsNoErr;
        }
    }
    else
    {
        Ipp32u code;

        h264PeekBits((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;
            Ipp16s *pCoeffs;

            IPP_BAD_PTR1_RET(ppPosCoefbuf)

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            h264DropBits((*ppBitStream), (*pOffset), ChromaDCRunTable[(code & 0x07) * 2 + 1]);
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            h264DropBits((*ppBitStream), (*pOffset), 2)

            return ippStsNoErr;
        }
    }

    {


    Ipp16s        CoeffBuf[16];        /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32u        uTR1Mask;
    Ipp32u        TrOneSigns;            /* return sign bits (1==neg) in low 3 bits*/
    Ipp32u        uCoeffIndex            = 0;
    Ipp32s        sTotalZeros            = 0;
    Ipp32s        sRunBefore;
    Ipp16s        sNumTrailingOnes;
    Ipp16s        sNumCoeff = 0;
    Ipp32s        pos;
    Ipp32s        i;

    /* check for the only codeword of all zeros*/


    /*ippiDecodeVLCPair_32s(ppBitStream, pOffset, pTblCoeffToken, */
    /*                              &sNumTrailingOnes,&sNumCoeff);*/
    register Ipp32s table_pos;
    register Ipp32s val;
    Ipp32u          table_bits;
    Ipp8u           code_len;
    Ipp32u *pbsBackUp;
    Ipp32s bsoffBackUp;

    /* check error(s) */
    //IPP_BAD_PTR4_RET(ppPosCoefbuf, pTblCoeffToken, ppTblTotalZerosCR, ppTblRunBefore)

    /* create bit stream backup */
    pbsBackUp = *ppBitStream;
    bsoffBackUp = *pOffset;

    table_bits = *pTblCoeffToken;
    h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
    val           = pTblCoeffToken[table_pos  + 1];
    code_len   = (Ipp8u) (val);

    while (code_len & 0x80)
    {
        val        = val >> 8;
        table_bits = pTblCoeffToken[val];
        h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val           = pTblCoeffToken[table_pos + val  + 1];
        code_len   = (Ipp8u) (val & 0xff);
    }

    h264UngetNBits((*ppBitStream), (*pOffset), code_len);

    if ((val>>8) == IPPVC_VLC_FORBIDDEN)
    {
         *ppBitStream = pbsBackUp;
         *pOffset    = bsoffBackUp;

         return ippStsH263VLCCodeErr;
    }
    sNumTrailingOnes  = (Ipp16s) ((val >> 8) &0xff);
    sNumCoeff = (Ipp16s) ((val >> 16) & 0xff);

    *pNumCoeff = sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (Ipp16s) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
            uTR1Mask >>= 1;
        }

    }
    /* Get the sign bits of any trailing one coeffs */
    if (sNumCoeff)
    {
        /*memset((*ppPosCoefbuf), 0, 4*sizeof(short)); */
#ifdef __ICL
#pragma vector always
#endif
        for (i = 0; i < 4; i++)
        {
            (*ppPosCoefbuf)[i] = 0;
        }
        /*((Ipp32s*)(*ppPosCoefbuf))[0] = 0; */
        /*((Ipp32s*)(*ppPosCoefbuf))[1] = 0; */
        /* get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            IppStatus sts = _GetBlockCoeffs_CAVLC(ppBitStream, pOffset,sNumCoeff,
                                         sNumTrailingOnes, &CoeffBuf[uCoeffIndex]);

            if (sts != ippStsNoErr)
                return sts;

        }
        if (sNumCoeff < 4)
        {
            /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sTotalZeros, */
            /*                                                ppTblTotalZerosCR[sNumCoeff]); */
            const Ipp32s *pDecTable = ppTblTotalZerosCR[sNumCoeff];

            IPP_BAD_PTR1_RET(ppTblTotalZerosCR[sNumCoeff])

            table_bits = pDecTable[0];
            h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            val           = pDecTable[table_pos  + 1];
            code_len   = (Ipp8u) (val & 0xff);
            val        = val >> 8;


            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos + val + 1];
                code_len   = (Ipp8u) (val & 0xff);
                val        = val >> 8;
            }

            if (val == IPPVC_VLC_FORBIDDEN)
            {
                *ppBitStream = pbsBackUp;
                *pOffset = bsoffBackUp;

                return ippStsH263VLCCodeErr;
            }

            h264UngetNBits((*ppBitStream), (*pOffset),code_len)

            sTotalZeros =  val;

        }
        uCoeffIndex = 0;
        while (sNumCoeff)
        {
            if ((sNumCoeff > 1) && (sTotalZeros > 0))
            {
                /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sRunBefore, */
                /*                                                ppTblRunBefore[sTotalZeros]); */
                const Ipp32s *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros])

                table_bits = pDecTable[0];
                h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (Ipp8u) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetNBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (Ipp8u) (val & 0xff);
                    val        = val >> 8;
                }

                if (val == IPPVC_VLC_FORBIDDEN)
                {
                    *ppBitStream = pbsBackUp;
                    *pOffset = bsoffBackUp;

                    return ippStsH263VLCCodeErr;
                }

                h264UngetNBits((*ppBitStream), (*pOffset),code_len)

                sRunBefore =  val;
            }
            else
                sRunBefore = sTotalZeros;

            pos             = sNumCoeff - 1 + sTotalZeros;
            sTotalZeros -= sRunBefore;

            /* The coeff is either in CoeffBuf or is a trailing one */
            (*ppPosCoefbuf)[pos] = CoeffBuf[(uCoeffIndex++)&0x0f];

            sNumCoeff--;
        }
        (*ppPosCoefbuf) += 4;

    }

    }

    return ippStsNoErr;

} /* IPPFUN(IppStatus, ippiDecodeCAVLCChromaDcCoeffs_H264_1u16s , (Ipp32u **ppBitStream, */
IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(Ipp32u **ppBitStream,
                                                         Ipp32s *pOffset,
                                                         Ipp16s *pNumCoeff,
                                                         Ipp16s **ppPosCoefbuf)

{

#if 1
    return ownippiDecodeCAVLCChromaDcCoeffs_H264_1u16s (ppBitStream,
                                                         pOffset,
                                                         pNumCoeff,
                                                         ppPosCoefbuf,
                                                         UMC::H264Bitstream::m_tblCoeffToken[3],
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZerosCR,
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore);
#else
    /* check the most frequently used parameters */
    //IPP_BAD_PTR3_RET(ppBitStream, pOffset, pNumCoeff)

#if 1
    if (4 < *pOffset)
    {
        Ipp32u code;

        h264PeekBitsNoCheck((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;
            Ipp16s *pCoeffs;

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            *pOffset -= ChromaDCRunTable[(code & 0x07) * 2 + 1];
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            *pOffset -= 2;

            return ippStsNoErr;
        }
    }
    else
    {
        Ipp32u code;

        h264PeekBits((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;

            /* advance coeffs buffer */
            Ipp16s *pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            h264DropBits((*ppBitStream), (*pOffset), ChromaDCRunTable[(code & 0x07) * 2 + 1]);
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            h264DropBits((*ppBitStream), (*pOffset), 2)

            return ippStsNoErr;
        }
    }
#endif
    {

    Ipp32s        CoeffBuf[16];        /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32u        TrOneSigns;            /* return sign bits (1==neg) in low 3 bits*/
    Ipp32u        uCoeffIndex            = 0;
    Ipp32s        sTotalZeros            = 0;
    Ipp32s        sRunBefore;
    Ipp32s        sNumTrailingOnes;
    Ipp32s        sNumCoeff = 0;
    Ipp32s        i;

    /* check for the only codeword of all zeros*/

    Ipp32s table_pos;
    Ipp32s val;
    Ipp32u table_bits;
    Ipp8u  code_len;

    const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblCoeffToken[3];

    table_bits = *pDecTable;
    ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
    val = pDecTable[table_pos  + 1];
    code_len   = (Ipp8u) (val);

    while (code_len & 0x80)
    {
        val        = val >> 8;
        table_bits = pDecTable[val];
        ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val       = pDecTable[table_pos + val  + 1];
        code_len   = (Ipp8u) (val & 0xff);
    }

    ippiUngetNBits((*ppBitStream), (*pOffset), code_len);

    sNumTrailingOnes  = ((val >> 8) &0xff);
    sNumCoeff = ((val >> 16) & 0xff);

    *pNumCoeff = (Ipp16s)sNumCoeff;

    /* Get the sign bits of any trailing one coeffs */
    if (sNumCoeff)
    {
        if (sNumTrailingOnes)
        {
            ippiGetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
            Ipp32u uTR1Mask = 1 << (sNumTrailingOnes - 1);
            while (uTR1Mask)
            {
                CoeffBuf[uCoeffIndex++] = ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
                uTR1Mask >>= 1;
            }
        }

#ifdef __ICL
#pragma vector always
#endif
        for (i = 0; i < 4; i++)
            (*ppPosCoefbuf)[i] = 0;

        //memset((*ppPosCoefbuf), 0, 4*sizeof(Ipp16s));

        /* get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            _GetBlockCoeffs_CAVLC(ppBitStream, pOffset, uCoeffIndex, sNumCoeff,
                                         sNumTrailingOnes, CoeffBuf);
        }

        uCoeffIndex = 0;

        if (sNumCoeff < 4)
        {
            const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblTotalZerosCR[sNumCoeff];

            Ipp32s table_pos;
            Ipp32s table_bits = pDecTable[0];
            ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

            ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

            sTotalZeros = qqq->q;

            for (; sTotalZeros > 0 && sNumCoeff > 1; sNumCoeff--)
            {
                const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblRunBefore[sTotalZeros];
                Ipp32s table_pos;
                Ipp32s table_bits = pDecTable[0];

                ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

                ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

                sRunBefore =  qqq->q;

                Ipp32s pos  = sNumCoeff - 1 + sTotalZeros;
                (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];

                sTotalZeros -= sRunBefore;
            }
        }

        for (; sNumCoeff; sNumCoeff--)
        {
            Ipp32s pos  = sNumCoeff - 1 + sTotalZeros;
            (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];
        }

        (*ppPosCoefbuf) += 4;
    }
    }

    return ippStsNoErr;
#endif
}

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u32s (Ipp32u ** const ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp32s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s *pScanMatrix)

{
    return ippiDecodeCAVLCCoeffs_H264_1u32s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZeros,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix);
}


IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(Ipp32u **ppBitStream,
                                                         Ipp32s *pOffset,
                                                         Ipp16s *pNumCoeff,
                                                         Ipp32s **ppPosCoefbuf)

{
    return ippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(ppBitStream,
                                                         pOffset,
                                                         pNumCoeff,
                                                         ppPosCoefbuf,
                                                         UMC::H264Bitstream::m_tblCoeffToken[3],
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZerosCR,
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore);
}

#endif // UMC_ENABLE_H264_VIDEO_DECODER
