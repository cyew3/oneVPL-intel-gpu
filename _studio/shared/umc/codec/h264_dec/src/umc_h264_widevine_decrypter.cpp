//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_va_dxva2.h"
#include "umc_va_linux.h"
#include "umc_h264_widevine_decrypter.h"
#include "mfx_common.h"

#include <limits.h>

namespace UMC
{
#ifdef UMC_VA_DXVA
#define D3DFMT_NV12 (D3DFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'))
#endif

#define SCLDEFAULT    1
#define SCLREDEFINED  2

DecryptParametersWrapper::DecryptParametersWrapper(void)
{
    status = 0;

    usStatusReportFeedbackNumber = 0;
    nal_ref_idc = 0;
    ui8FieldPicFlag = 0;

    frame_num = 0;
    slice_type = 0;
    idr_flag = 0;
    iCurrFieldOrderCnt[0] = iCurrFieldOrderCnt[1] = 0;

    memset(&SeqParams, 0, sizeof(seq_param_set_t));
    memset(&PicParams, 0, sizeof(pic_param_set_t));
    memset(&RefPicMarking, 0, sizeof(h264_Dec_Ref_Pic_Marking_t));

    memset(&SeiBufferingPeriod, 0, sizeof(h264_SEI_buffering_period_t));
    memset(&SeiPicTiming, 0, sizeof(h264_SEI_pic_timing_t));
    memset(&SeiRecoveryPoint, 0, sizeof(h264_SEI_recovery_point_t));

    m_pts = 0;
}

DecryptParametersWrapper::DecryptParametersWrapper(const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters)
{
    CopyDecryptParams(pDecryptParameters);
    m_pts = 0;
}

DecryptParametersWrapper::~DecryptParametersWrapper()
{
    ;
}

DecryptParametersWrapper & DecryptParametersWrapper::operator = (const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters)
{
    CopyDecryptParams(pDecryptParameters);
    m_pts = 0;
    return *this;
}

DecryptParametersWrapper & DecryptParametersWrapper::operator = (const DecryptParametersWrapper & pDecryptParametersWrapper)
{
    m_pts = pDecryptParametersWrapper.m_pts;
    return DecryptParametersWrapper::operator = ( *((const DECRYPT_QUERY_STATUS_PARAMS_AVC *)&pDecryptParametersWrapper));
}

void DecryptParametersWrapper::CopyDecryptParams(const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters)
{
    DECRYPT_QUERY_STATUS_PARAMS_AVC* temp = this;
    *temp = pDecryptParameters;
}

Status DecryptParametersWrapper::GetSequenceParamSet(H264SeqParamSet *sps)
{
    // Not all members of the seq param set structure are contained in all
    // seq param sets. So start by init all to zero.
    Status ps = UMC_OK;
    sps->Reset();

    // profile
    // TBD: add rejection of unsupported profile
    sps->profile_idc = SeqParams.profile_idc;

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

    sps->constraint_set0_flag = SeqParams.constraint_set_flags & 1;
    sps->constraint_set1_flag = SeqParams.constraint_set_flags & 2;
    sps->constraint_set2_flag = SeqParams.constraint_set_flags & 4;
    sps->constraint_set3_flag = SeqParams.constraint_set_flags & 8;
    sps->constraint_set4_flag = 0;                                        //TBD
    sps->constraint_set5_flag = 0;                                        //TBD

    sps->level_idc = SeqParams.level_idc;

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
    Ipp32s sps_id = SeqParams.seq_parameter_set_id;
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
        Ipp32u chroma_format_idc = SeqParams.sps_disp.chroma_format_idc;

        if (chroma_format_idc > 3)
            return UMC_ERR_INVALID_STREAM;

        sps->chroma_format_idc = (Ipp8u)chroma_format_idc;
        if (sps->chroma_format_idc==3)
        {
            sps->residual_colour_transform_flag = SeqParams.residual_colour_transform_flag;
        }

        Ipp32u bit_depth_luma = SeqParams.bit_depth_luma_minus8 + 8;
        Ipp32u bit_depth_chroma = SeqParams.bit_depth_chroma_minus8 + 8;

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

        sps->qpprime_y_zero_transform_bypass_flag = SeqParams.lossless_qpprime_y_zero_flag;  //TBD check that
        sps->seq_scaling_matrix_present_flag = SeqParams.seq_scaling_matrix_present_flag;
        if (sps->seq_scaling_matrix_present_flag)
        {
            // 0
            if (SeqParams.seq_scaling_list_present_flag[0])
            {
                FillScalingList4x4(&sps->ScalingLists4x4[0],(Ipp8u*) SeqParams.ScalingList4x4[0]);
                sps->type_of_scaling_list_used[0] = SCLREDEFINED;
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[0],(Ipp8u*) default_intra_scaling_list4x4);
                sps->type_of_scaling_list_used[0] = SCLDEFAULT;
            }
            // 1
            if (SeqParams.seq_scaling_list_present_flag[1])
            {
                FillScalingList4x4(&sps->ScalingLists4x4[1],(Ipp8u*) SeqParams.ScalingList4x4[1]);
                sps->type_of_scaling_list_used[1] = SCLREDEFINED;
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[1],(Ipp8u*) sps->ScalingLists4x4[0].ScalingListCoeffs);
                sps->type_of_scaling_list_used[1] = SCLDEFAULT;
            }
            // 2
            if (SeqParams.seq_scaling_list_present_flag[2])
            {
                FillScalingList4x4(&sps->ScalingLists4x4[2],(Ipp8u*) SeqParams.ScalingList4x4[2]);
                sps->type_of_scaling_list_used[2] = SCLREDEFINED;
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[2],(Ipp8u*) sps->ScalingLists4x4[1].ScalingListCoeffs);
                sps->type_of_scaling_list_used[2] = SCLDEFAULT;
            }
            // 3
            if (SeqParams.seq_scaling_list_present_flag[3])
            {
                FillScalingList4x4(&sps->ScalingLists4x4[3],(Ipp8u*) SeqParams.ScalingList4x4[3]);
                sps->type_of_scaling_list_used[3] = SCLREDEFINED;
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[3],(Ipp8u*) default_inter_scaling_list4x4);
                sps->type_of_scaling_list_used[3] = SCLDEFAULT;
            }
            // 4
            if (SeqParams.seq_scaling_list_present_flag[4])
            {
                FillScalingList4x4(&sps->ScalingLists4x4[4],(Ipp8u*) SeqParams.ScalingList4x4[4]);
                sps->type_of_scaling_list_used[4] = SCLREDEFINED;
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[4],(Ipp8u*) sps->ScalingLists4x4[3].ScalingListCoeffs);
                sps->type_of_scaling_list_used[4] = SCLDEFAULT;
            }
            // 5
            if (SeqParams.seq_scaling_list_present_flag[5])
            {
                FillScalingList4x4(&sps->ScalingLists4x4[5],(Ipp8u*) SeqParams.ScalingList4x4[5]);
                sps->type_of_scaling_list_used[5] = SCLREDEFINED;
            }
            else
            {
                FillScalingList4x4(&sps->ScalingLists4x4[5],(Ipp8u*) sps->ScalingLists4x4[4].ScalingListCoeffs);
                sps->type_of_scaling_list_used[5] = SCLDEFAULT;
            }

            // 0
            if (SeqParams.seq_scaling_list_present_flag[6])
            {
                FillScalingList8x8(&sps->ScalingLists8x8[0],(Ipp8u*) SeqParams.ScalingList8x8[0]);
                sps->type_of_scaling_list_used[6] = SCLREDEFINED;
            }
            else
            {
                FillScalingList8x8(&sps->ScalingLists8x8[0],(Ipp8u*) default_intra_scaling_list8x8);
                sps->type_of_scaling_list_used[6] = SCLDEFAULT;
            }
            // 1
            if (SeqParams.seq_scaling_list_present_flag[7])
            {
                FillScalingList8x8(&sps->ScalingLists8x8[1],(Ipp8u*) SeqParams.ScalingList8x8[1]);
                sps->type_of_scaling_list_used[7] = SCLREDEFINED;
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
    Ipp32u log2_max_frame_num = SeqParams.log2_max_frame_num_minus4 + 4;
    sps->log2_max_frame_num = (Ipp8u)log2_max_frame_num;

    if (log2_max_frame_num > 16 || log2_max_frame_num < 4)
        return UMC_ERR_INVALID_STREAM;

    // pic order cnt type (0..2)
    Ipp32u pic_order_cnt_type = SeqParams.pic_order_cnt_type;
    sps->pic_order_cnt_type = (Ipp8u)pic_order_cnt_type;
    if (pic_order_cnt_type > 2)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    if (sps->pic_order_cnt_type == 0)
    {
        // log2 max pic order count lsb (bitstream contains value - 4)
        Ipp32u log2_max_pic_order_cnt_lsb = SeqParams.log2_max_pic_order_cnt_lsb_minus4 + 4;
        sps->log2_max_pic_order_cnt_lsb = (Ipp8u)log2_max_pic_order_cnt_lsb;

        if (log2_max_pic_order_cnt_lsb > 16 || log2_max_pic_order_cnt_lsb < 4)
            return UMC_ERR_INVALID_STREAM;

        sps->MaxPicOrderCntLsb = (1 << sps->log2_max_pic_order_cnt_lsb);
    }
    else if (sps->pic_order_cnt_type == 1)
    {
        sps->delta_pic_order_always_zero_flag = SeqParams.delta_pic_order_always_zero_flag;
        sps->offset_for_non_ref_pic = SeqParams.offset_for_non_ref_pic;
        sps->offset_for_top_to_bottom_field = SeqParams.offset_for_top_to_bottom_field;
        sps->num_ref_frames_in_pic_order_cnt_cycle = SeqParams.num_ref_frames_in_pic_order_cnt_cycle;

        if (sps->num_ref_frames_in_pic_order_cnt_cycle > 255)
            return UMC_ERR_INVALID_STREAM;

        // get offsets
        for (Ipp32u i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
        {
            sps->poffset_for_ref_frame[i] = SeqParams.offset_for_ref_frame[i];
        }
    }    // pic order count type 1

    // num ref frames
    sps->num_ref_frames = SeqParams.num_ref_frames;
    if (sps->num_ref_frames > 16)
        return UMC_ERR_INVALID_STREAM;

    sps->gaps_in_frame_num_value_allowed_flag = SeqParams.gaps_in_frame_num_value_allowed_flag;

    // picture width in MBs (bitstream contains value - 1)
    sps->frame_width_in_mbs = SeqParams.sps_disp.pic_width_in_mbs_minus1 + 1;

    // picture height in MBs (bitstream contains value - 1)
    sps->frame_height_in_mbs = SeqParams.sps_disp.pic_height_in_map_units_minus1 + 1;

    if (!(sps->frame_width_in_mbs * 16 < USHRT_MAX) ||
        !(sps->frame_height_in_mbs * 16 < USHRT_MAX))
        return UMC_ERR_INVALID_STREAM;

    sps->frame_mbs_only_flag = SeqParams.sps_disp.frame_mbs_only_flag;
    sps->frame_height_in_mbs = (2-sps->frame_mbs_only_flag)*sps->frame_height_in_mbs;
    if (sps->frame_mbs_only_flag == 0)
    {
        sps->mb_adaptive_frame_field_flag = SeqParams.sps_disp.mb_adaptive_frame_field_flag;
    }
    sps->direct_8x8_inference_flag = SeqParams.sps_disp.direct_8x8_inference_flag;
    if (sps->frame_mbs_only_flag==0)
    {
        sps->direct_8x8_inference_flag = 1;
    }
    sps->frame_cropping_flag = SeqParams.sps_disp.frame_cropping_flag;

    if (sps->frame_cropping_flag)
    {
        sps->frame_cropping_rect_left_offset      = SeqParams.sps_disp.frame_crop_rect_left_offset;
        sps->frame_cropping_rect_right_offset     = SeqParams.sps_disp.frame_crop_rect_right_offset;
        sps->frame_cropping_rect_top_offset       = SeqParams.sps_disp.frame_crop_rect_top_offset;
        sps->frame_cropping_rect_bottom_offset    = SeqParams.sps_disp.frame_crop_rect_bottom_offset;

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

    sps->vui_parameters_present_flag = (Ipp8u)SeqParams.sps_disp.vui_parameters_present_flag;
    if (sps->vui_parameters_present_flag)
    {
        H264VUI vui;
        Status vui_ps = GetVUIParam(sps, &vui);

        if (vui_ps == UMC_OK)
        {
            sps->vui = vui;
        }
    }

    return ps;
}    // GetSequenceParamSet

Status DecryptParametersWrapper::GetVUIParam(H264SeqParamSet *sps, H264VUI *vui)
{
    Status ps = UMC_OK;

    vui->Reset();

    vui->aspect_ratio_info_present_flag = SeqParams.sps_disp.vui_seq_parameters.aspect_ratio_info_present_flag;

    vui->sar_width = 1; // default values
    vui->sar_height = 1;

    if (vui->aspect_ratio_info_present_flag)
    {
        vui->aspect_ratio_idc = SeqParams.sps_disp.vui_seq_parameters.aspect_ratio_idc;
        if (vui->aspect_ratio_idc  ==  255) {
            vui->sar_width = SeqParams.sps_disp.vui_seq_parameters.sar_width;
            vui->sar_height = SeqParams.sps_disp.vui_seq_parameters.sar_height;
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

    vui->video_signal_type_present_flag = SeqParams.sps_disp.vui_seq_parameters.video_signal_type_present_flag;
    if (vui->video_signal_type_present_flag) {
        vui->video_format = SeqParams.sps_disp.vui_seq_parameters.video_format;
        vui->colour_description_present_flag = SeqParams.sps_disp.vui_seq_parameters.colour_description_present_flag;
        if (vui->colour_description_present_flag) {
            vui->colour_primaries = SeqParams.sps_disp.vui_seq_parameters.colour_primaries;
            vui->transfer_characteristics = SeqParams.sps_disp.vui_seq_parameters.transfer_characteristics;
            vui->matrix_coefficients = SeqParams.sps_disp.vui_seq_parameters.matrix_coefficients;
        }
    }
    vui->timing_info_present_flag = SeqParams.sps_disp.vui_seq_parameters.timing_info_present_flag;

    if (vui->timing_info_present_flag)
    {
        vui->num_units_in_tick = SeqParams.sps_disp.vui_seq_parameters.num_units_in_tick;
        vui->time_scale = SeqParams.sps_disp.vui_seq_parameters.time_scale;
        vui->fixed_frame_rate_flag = SeqParams.sps_disp.vui_seq_parameters.fixed_frame_rate_flag;

        if (!vui->num_units_in_tick || !vui->time_scale)
            vui->timing_info_present_flag = 0;
    }

    vui->nal_hrd_parameters_present_flag = SeqParams.sps_disp.vui_seq_parameters.nal_hrd_parameters_present_flag;
    if (vui->nal_hrd_parameters_present_flag)
        ps=GetHRDParam(sps, &SeqParams.sps_disp.vui_seq_parameters.NalHrd, vui);
    vui->vcl_hrd_parameters_present_flag = SeqParams.sps_disp.vui_seq_parameters.vcl_hrd_parameters_present_flag;
    if (vui->vcl_hrd_parameters_present_flag)
        ps=GetHRDParam(sps, &SeqParams.sps_disp.vui_seq_parameters.VclHrd, vui);
    if (vui->nal_hrd_parameters_present_flag || vui->vcl_hrd_parameters_present_flag)
        vui->low_delay_hrd_flag = SeqParams.sps_disp.vui_seq_parameters.low_delay_hrd_flag;
    vui->pic_struct_present_flag  = SeqParams.sps_disp.vui_seq_parameters.pic_struct_present_flag;
    vui->bitstream_restriction_flag = SeqParams.sps_disp.vui_seq_parameters.bitstream_restriction_flag;
    if (vui->bitstream_restriction_flag) {
        vui->num_reorder_frames = (Ipp8u)SeqParams.sps_disp.vui_seq_parameters.num_reorder_frames;

        Ipp32s value = SeqParams.sps_disp.vui_seq_parameters.max_dec_frame_buffering;
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

Status DecryptParametersWrapper::GetHRDParam(H264SeqParamSet *, h264_hrd_param_set_t *hrd, H264VUI *vui)
{
    Ipp32s cpb_cnt = hrd->info.cpb_cnt_minus1 + 1;

    if (cpb_cnt >= 32)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    vui->cpb_cnt = (Ipp8u)cpb_cnt;

    vui->bit_rate_scale = hrd->info.bit_rate_scale;
    vui->cpb_size_scale = hrd->info.cpb_size_scale;
    for( Ipp32s idx= 0; idx < vui->cpb_cnt; idx++ ) {
        vui->bit_rate_value[ idx ] = hrd->item[idx].bit_rate_value_minus1 + 1;
        vui->cpb_size_value[ idx ] = hrd->item[idx].cpb_size_value_minus1 + 1;
        vui->cbr_flag[ idx ] = (Ipp8u)hrd->item[idx].cbr_flag;
    }
    vui->initial_cpb_removal_delay_length = hrd->info.initial_cpb_removal_delay_length_minus1 + 1;
    vui->cpb_removal_delay_length = hrd->info.cpb_removal_delay_length_minus1 + 1;
    vui->dpb_output_delay_length = hrd->info.dpb_output_delay_length_minus1 + 1;
    vui->time_offset_length = hrd->info.time_offset_length;
    return UMC_OK;
}

Status DecryptParametersWrapper::GetPictureParamSetPart1(H264PicParamSet *pps)
{
    // Not all members of the pic param set structure are contained in all
    // pic param sets. So start by init all to zero.
    pps->Reset();

    // id
    Ipp32u pic_parameter_set_id = PicParams.pic_parameter_set_id;
    pps->pic_parameter_set_id = (Ipp16u)pic_parameter_set_id;
    if (pic_parameter_set_id > MAX_NUM_PIC_PARAM_SETS-1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    // seq param set referred to by this pic param set
    Ipp32u seq_parameter_set_id = PicParams.seq_parameter_set_id;
    pps->seq_parameter_set_id = (Ipp8u)seq_parameter_set_id;
    if (seq_parameter_set_id > MAX_NUM_SEQ_PARAM_SETS-1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    return UMC_OK;
}    // GetPictureParamSetPart1

Status DecryptParametersWrapper::GetPictureParamSetPart2(H264PicParamSet *pps)
{
    pps->entropy_coding_mode = PicParams.entropy_coding_mode_flag;

    pps->bottom_field_pic_order_in_frame_present_flag = PicParams.pic_order_present_flag;

    // number of slice groups, bitstream has value - 1
    pps->num_slice_groups = PicParams.num_slice_groups_minus1 + 1;
    if (pps->num_slice_groups != 1)
    {
        if (pps->num_slice_groups > MAX_NUM_SLICE_GROUPS)
        {
            return UMC_ERR_INVALID_STREAM;
        }

        Ipp32u slice_group_map_type = PicParams.slice_group_map_type;
        pps->SliceGroupInfo.slice_group_map_type = (Ipp8u)slice_group_map_type;

        if (slice_group_map_type > 6)
            return UMC_ERR_INVALID_STREAM;

        return UMC_ERR_INVALID_STREAM;

        //// Get additional, map type dependent slice group data
        //switch (pps->SliceGroupInfo.slice_group_map_type)
        //{
        //case 0:
        //    for (Ipp32u slice_group = 0; slice_group < pps->num_slice_groups; slice_group++)
        //    {
        //        // run length, bitstream has value - 1
        //        pps->SliceGroupInfo.run_length[slice_group] = GetVLCElement(false) + 1;
        //    }
        //    break;
        //case 1:
        //    // no additional info
        //    break;
        //case 2:
        //    for (Ipp32u slice_group = 0; slice_group < (Ipp32u)(pps->num_slice_groups-1); slice_group++)
        //    {
        //        pps->SliceGroupInfo.t1.top_left[slice_group] = GetVLCElement(false);
        //        pps->SliceGroupInfo.t1.bottom_right[slice_group] = GetVLCElement(false);

        //        // check for legal values
        //        if (pps->SliceGroupInfo.t1.top_left[slice_group] >
        //            pps->SliceGroupInfo.t1.bottom_right[slice_group])
        //        {
        //            return UMC_ERR_INVALID_STREAM;
        //        }
        //    }
        //    break;
        //case 3:
        //case 4:
        //case 5:
        //    // For map types 3..5, number of slice groups must be 2
        //    if (pps->num_slice_groups != 2)
        //    {
        //        return UMC_ERR_INVALID_STREAM;
        //    }
        //    pps->SliceGroupInfo.t2.slice_group_change_direction_flag = Get1Bit();
        //    pps->SliceGroupInfo.t2.slice_group_change_rate = GetVLCElement(false) + 1;
        //    break;
        //case 6:
        //    // mapping of slice group to map unit (macroblock if not fields) is
        //    // per map unit, read from bitstream
        //    {
        //        // number of map units, bitstream has value - 1
        //        pps->SliceGroupInfo.t3.pic_size_in_map_units = GetVLCElement(false) + 1;

        //        Ipp32s len = IPP_MAX(1, pps->SliceGroupInfo.t3.pic_size_in_map_units);

        //        pps->SliceGroupInfo.pSliceGroupIDMap.resize(len);

        //        // num_bits is Ceil(log2(num_groups)) - number of bits used to code each slice group id
        //        Ipp32u num_bits = SGIdBits[pps->num_slice_groups - 2];

        //        for (Ipp32u map_unit = 0;
        //             map_unit < pps->SliceGroupInfo.t3.pic_size_in_map_units;
        //             map_unit++)
        //        {
        //            pps->SliceGroupInfo.pSliceGroupIDMap[map_unit] = (Ipp8u)GetBits(num_bits);
        //        }
        //    }
        //    break;
        //default:
        //    return UMC_ERR_INVALID_STREAM;

        //}    // switch
    }    // slice group info

    // number of list 0 ref pics used to decode picture, bitstream has value - 1
    pps->num_ref_idx_l0_active = PicParams.num_ref_idx_l0_active;

    // number of list 1 ref pics used to decode picture, bitstream has value - 1
    pps->num_ref_idx_l1_active = PicParams.num_ref_idx_l1_active;

    if (pps->num_ref_idx_l1_active > MAX_NUM_REF_FRAMES || pps->num_ref_idx_l0_active > MAX_NUM_REF_FRAMES)
        return UMC_ERR_INVALID_STREAM;

    // weighted pediction
    pps->weighted_pred_flag = PicParams.weighted_pred_flag;
    pps->weighted_bipred_idc = PicParams.weighted_bipred_idc;

    // default slice QP, bitstream has value - 26
    Ipp32s pic_init_qp = PicParams.pic_init_qp_minus26 + 26;
    pps->pic_init_qp = (Ipp8s)pic_init_qp;
    if (pic_init_qp > QP_MAX)
    {
        //return UMC_ERR_INVALID_STREAM;
    }

    // default SP/SI slice QP, bitstream has value - 26
    pps->pic_init_qs = (Ipp8u)(PicParams.pic_init_qs_minus26 + 26);
    if (pps->pic_init_qs > QP_MAX)
    {
        //return UMC_ERR_INVALID_STREAM;
    }

    pps->chroma_qp_index_offset[0] = (Ipp8s)PicParams.chroma_qp_index_offset;
    if ((pps->chroma_qp_index_offset[0] < -12) || (pps->chroma_qp_index_offset[0] > 12))
    {
        //return UMC_ERR_INVALID_STREAM;
    }

    pps->deblocking_filter_variables_present_flag = PicParams.deblocking_filter_control_present_flag;
    pps->constrained_intra_pred_flag = PicParams.constrained_intra_pred_flag;
    pps->redundant_pic_cnt_present_flag = PicParams.redundant_pic_cnt_present_flag;

    pps->transform_8x8_mode_flag = PicParams.transform_8x8_mode_flag;
    pps->pic_scaling_matrix_present_flag = PicParams.pic_scaling_matrix_present_flag;

    H264ScalingPicParams * scaling = &pps->scaling[0];

    if (pps->pic_scaling_matrix_present_flag)
    {
        for (Ipp32s i = 0; i < 6; i++)
        {
            pps->pic_scaling_list_present_flag[i] = PicParams.pic_scaling_list_present_flag[i];
            if (pps->pic_scaling_list_present_flag[i])
            {
                FillScalingList4x4(&scaling->ScalingLists4x4[i], (Ipp8u*) PicParams.ScalingList4x4[i]);
                pps->type_of_scaling_list_used[i] = SCLREDEFINED;
            }
            else
            {
                pps->type_of_scaling_list_used[i] = SCLDEFAULT;
            }
        }

        if (pps->transform_8x8_mode_flag)
        {
            pps->pic_scaling_list_present_flag[6] = PicParams.pic_scaling_list_present_flag[6];
            if (pps->pic_scaling_list_present_flag[6])
            {
                FillScalingList8x8(&scaling->ScalingLists8x8[0], (Ipp8u*) PicParams.ScalingList8x8[0]);
                pps->type_of_scaling_list_used[6] = SCLREDEFINED;
            }
            else
            {
                pps->type_of_scaling_list_used[6] = SCLDEFAULT;
            }

            pps->pic_scaling_list_present_flag[7] = PicParams.pic_scaling_list_present_flag[7];
            if (pps->pic_scaling_list_present_flag[7])
            {
                FillScalingList8x8(&scaling->ScalingLists8x8[1], (Ipp8u*) PicParams.ScalingList8x8[1]);
            }
            else
            {
                pps->type_of_scaling_list_used[7] = SCLDEFAULT;
            }
        }

        memcpy_s(&pps->scaling[1], sizeof(H264ScalingPicParams), &pps->scaling[0], sizeof(H264ScalingPicParams));
    }

    pps->chroma_qp_index_offset[1] = (Ipp8s)PicParams.second_chroma_qp_index_offset;

    return UMC_OK;
}    // GetPictureParamSet


Status DecryptParametersWrapper::GetSliceHeaderPart1(H264SliceHeader *hdr)
{
    Ipp32u val;

    hdr->nal_ref_idc = nal_ref_idc;
    hdr->IdrPicFlag = idr_flag;

    //hdr->first_mb_in_slice = GetVLCElement(false);
    //if (0 > hdr->first_mb_in_slice) // upper bound is checked in H264Slice
    //    return UMC_ERR_INVALID_STREAM;

    // slice type
    val = slice_type;
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

    Ipp32u pic_parameter_set_id = PicParams.pic_parameter_set_id;
    hdr->pic_parameter_set_id = (Ipp16u)pic_parameter_set_id;
    if (pic_parameter_set_id > MAX_NUM_PIC_PARAM_SETS - 1)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    return UMC_OK;
} // Status GetSliceHeaderPart1(H264SliceHeader *pSliceHeader)

Status DecryptParametersWrapper::GetSliceHeaderPart2(H264SliceHeader *hdr,
                                                     const H264PicParamSet *pps,
                                                     const H264SeqParamSet *sps)
{
    pps;
    hdr->frame_num = frame_num;

    hdr->bottom_field_flag = 0;
    if (sps->frame_mbs_only_flag == 0)
    {
        hdr->field_pic_flag = (ui8FieldPicFlag == 1) || (ui8FieldPicFlag == 2);
        hdr->MbaffFrameFlag = !hdr->field_pic_flag && sps->mb_adaptive_frame_field_flag;
        if (hdr->field_pic_flag != 0)
        {
            hdr->bottom_field_flag = (ui8FieldPicFlag == 2);
        }
    }

    //if (hdr->MbaffFrameFlag)
    //{
    //    Ipp32u const first_mb_in_slice
    //        = hdr->first_mb_in_slice;

    //    if (first_mb_in_slice * 2 > INT_MAX)
    //        return UMC_ERR_INVALID_STREAM;

    //    // correct frst_mb_in_slice in order to handle MBAFF
    //    hdr->first_mb_in_slice *= 2;
    //}

    //if (hdr->IdrPicFlag)
    //{
    //    Ipp32s pic_id = hdr->idr_pic_id = GetVLCElement(false);
    //    if (pic_id < 0 || pic_id > 65535)
    //        return UMC_ERR_INVALID_STREAM;
    //}

    hdr->pic_order_cnt_lsb = iCurrFieldOrderCnt[0];
    hdr->delta_pic_order_cnt_bottom = iCurrFieldOrderCnt[1] - iCurrFieldOrderCnt[0];

    //if (sps->pic_order_cnt_type == 0)
    //{
    //    hdr->pic_order_cnt_lsb = GetBits(sps->log2_max_pic_order_cnt_lsb);
    //    if (pps->bottom_field_pic_order_in_frame_present_flag && (!hdr->field_pic_flag))
    //        hdr->delta_pic_order_cnt_bottom = GetVLCElement(true);
    //}

    if ((sps->pic_order_cnt_type == 1) && (sps->delta_pic_order_always_zero_flag == 0))
    {
        hdr->delta_pic_order_cnt[0] = -1;  // WA to be sure that value is not 0, to correctly process FrameNumGap
        if (pps->bottom_field_pic_order_in_frame_present_flag && (!hdr->field_pic_flag))
            hdr->delta_pic_order_cnt[1] = -1;  // WA to be sure that value is not 0, to correctly process FrameNumGap
    }

    //if (pps->redundant_pic_cnt_present_flag)
    //{
    //    hdr->hw_wa_redundant_elimination_bits[0] = (Ipp32u)BitsDecoded();
    //    // redundant pic count
    //    hdr->redundant_pic_cnt = GetVLCElement(false);
    //    if (hdr->redundant_pic_cnt > 127)
    //        return UMC_ERR_INVALID_STREAM;

    //    hdr->hw_wa_redundant_elimination_bits[1] = (Ipp32u)BitsDecoded();
    //}

    return UMC_OK;
}

Status DecryptParametersWrapper::GetSliceHeaderPart3(
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
    pPredWeight_L0;
    pPredWeight_L1;
    pReorderInfo_L0;
    pReorderInfo_L1;
    sps;
    spsSvcExt;

    //Ipp8u ref_pic_list_reordering_flag_l0 = 0;
    //Ipp8u ref_pic_list_reordering_flag_l1 = 0;
    //Status ps = UMC_OK;

    pReorderInfo_L0->num_entries = 0;
    pReorderInfo_L1->num_entries = 0;
    hdr->num_ref_idx_l0_active = pps->num_ref_idx_l0_active;
    hdr->num_ref_idx_l1_active = pps->num_ref_idx_l1_active;

    hdr->direct_spatial_mv_pred_flag = 1;

    if (!hdr->nal_ext.svc_extension_flag || hdr->nal_ext.svc.quality_id == 0)
    {
        //if (BPREDSLICE == hdr->slice_type)
        //{
        //    // direct mode prediction method
        //    hdr->direct_spatial_mv_pred_flag = Get1Bit();
        //}

        //if (PREDSLICE == hdr->slice_type ||
        //    S_PREDSLICE == hdr->slice_type ||
        //    BPREDSLICE == hdr->slice_type)
        //{
        //    hdr->num_ref_idx_active_override_flag = Get1Bit();
        //    if (hdr->num_ref_idx_active_override_flag != 0)
        //    // ref idx active l0 and l1
        //    {
        //        hdr->num_ref_idx_l0_active = GetVLCElement(false) + 1;
        //        if (BPREDSLICE == hdr->slice_type)
        //            hdr->num_ref_idx_l1_active = GetVLCElement(false) + 1;
        //    }
        //}    // ref idx override

        //if (hdr->num_ref_idx_l0_active < 0 || hdr->num_ref_idx_l0_active > (Ipp32s)MAX_NUM_REF_FRAMES ||
        //    hdr->num_ref_idx_l1_active < 0 || hdr->num_ref_idx_l1_active > (Ipp32s)MAX_NUM_REF_FRAMES)
        //    return UMC_ERR_INVALID_STREAM;

        //if (hdr->slice_type != INTRASLICE && hdr->slice_type != S_INTRASLICE)
        //{
        //    Ipp32u reordering_of_pic_nums_idc;
        //    Ipp32u reorder_idx;

        //    // Reference picture list reordering
        //    ref_pic_list_reordering_flag_l0 = Get1Bit();
        //    if (ref_pic_list_reordering_flag_l0)
        //    {
        //        bool bOk = true;

        //        reorder_idx = 0;
        //        reordering_of_pic_nums_idc = 0;

        //        // Get reorder idc,pic_num pairs until idc==3
        //        while (bOk)
        //        {
        //          reordering_of_pic_nums_idc = (Ipp8u)GetVLCElement(false);
        //          if (reordering_of_pic_nums_idc > 5)
        //            return UMC_ERR_INVALID_STREAM;

        //            if (reordering_of_pic_nums_idc == 3)
        //                break;

        //            if (reorder_idx >= MAX_NUM_REF_FRAMES)
        //            {
        //                return UMC_ERR_INVALID_STREAM;
        //            }

        //            pReorderInfo_L0->reordering_of_pic_nums_idc[reorder_idx] =
        //                                        (Ipp8u)reordering_of_pic_nums_idc;
        //            pReorderInfo_L0->reorder_value[reorder_idx]  =
        //                                                GetVLCElement(false);
        //          if (reordering_of_pic_nums_idc != 2)
        //                // abs_diff_pic_num is coded minus 1
        //                pReorderInfo_L0->reorder_value[reorder_idx]++;
        //            reorder_idx++;
        //        }    // while

        //        pReorderInfo_L0->num_entries = reorder_idx;
        //    }    // L0 reordering info
        //    else
        //        pReorderInfo_L0->num_entries = 0;

        //    if (BPREDSLICE == hdr->slice_type)
        //    {
        //        ref_pic_list_reordering_flag_l1 = Get1Bit();
        //        if (ref_pic_list_reordering_flag_l1)
        //        {
        //            bool bOk = true;

        //            // Get reorder idc,pic_num pairs until idc==3
        //            reorder_idx = 0;
        //            reordering_of_pic_nums_idc = 0;
        //            while (bOk)
        //            {
        //            reordering_of_pic_nums_idc = GetVLCElement(false);
        //              if (reordering_of_pic_nums_idc > 5)
        //                return UMC_ERR_INVALID_STREAM;

        //                if (reordering_of_pic_nums_idc == 3)
        //                    break;

        //                if (reorder_idx >= MAX_NUM_REF_FRAMES)
        //                {
        //                    return UMC_ERR_INVALID_STREAM;
        //                }

        //                pReorderInfo_L1->reordering_of_pic_nums_idc[reorder_idx] =
        //                                            (Ipp8u)reordering_of_pic_nums_idc;
        //                pReorderInfo_L1->reorder_value[reorder_idx]  =
        //                                                    GetVLCElement(false);
        //                if (reordering_of_pic_nums_idc != 2)
        //                    // abs_diff_pic_num is coded minus 1
        //                    pReorderInfo_L1->reorder_value[reorder_idx]++;
        //                reorder_idx++;
        //            }    // while
        //            pReorderInfo_L1->num_entries = reorder_idx;
        //        }    // L1 reordering info
        //        else
        //            pReorderInfo_L1->num_entries = 0;
        //    }    // B slice
        //}    // reordering info

        //hdr->luma_log2_weight_denom = 0;
        //hdr->chroma_log2_weight_denom = 0;

        //// prediction weight table
        //if ( (pps->weighted_pred_flag &&
        //      ((PREDSLICE == hdr->slice_type) || (S_PREDSLICE == hdr->slice_type))) ||
        //     ((pps->weighted_bipred_idc == 1) && (BPREDSLICE == hdr->slice_type)))
        //{
        //    ps = GetPredWeightTable(hdr, sps, pPredWeight_L0, pPredWeight_L1);
        //    if (ps != UMC_OK)
        //        return ps;
        //}

        // dec_ref_pic_marking
        pAdaptiveMarkingInfo->num_entries = 0;
        pBaseAdaptiveMarkingInfo->num_entries = 0;

        if (hdr->nal_ref_idc)
        {
            //ps = DecRefPicMarking(hdr, pAdaptiveMarkingInfo);
            if (hdr->IdrPicFlag)
            {
                hdr->no_output_of_prior_pics_flag = RefPicMarking.no_output_of_prior_pics_flag;
                hdr->long_term_reference_flag = RefPicMarking.long_term_reference_flag;
            }
            else
            {
                //ps =  DecRefBasePicMarking(pAdaptiveMarkingInfo, hdr->adaptive_ref_pic_marking_mode_flag);

                Ipp8u memory_management_control_operation;
                Ipp32u num_entries = 0;

                hdr->adaptive_ref_pic_marking_mode_flag = RefPicMarking.adaptive_ref_pic_marking_mode_flag;

                while (hdr->adaptive_ref_pic_marking_mode_flag != 0)
                {
                    memory_management_control_operation = RefPicMarking.memory_management_control_operation[num_entries];
                    if (memory_management_control_operation == 0)
                        break;

                    if (memory_management_control_operation > 6)
                        return UMC_ERR_INVALID_STREAM;

                    pAdaptiveMarkingInfo->mmco[num_entries] = memory_management_control_operation;

                    switch (pAdaptiveMarkingInfo->mmco[num_entries])
                    {
                    case 1:
                        // mark a short-term picture as unused for reference
                        // Value is difference_of_pic_nums_minus1
                        pAdaptiveMarkingInfo->value[num_entries*2] = RefPicMarking.difference_of_pic_num_minus1[num_entries];
                        break;
                    case 2:
                        // mark a long-term picture as unused for reference
                        // value is long_term_pic_num
                        pAdaptiveMarkingInfo->value[num_entries*2] = RefPicMarking.long_term_pic_num[num_entries];
                        break;
                    case 3:
                        // Assign a long-term frame idx to a short-term picture
                        // Value is difference_of_pic_nums_minus1 followed by
                        // long_term_frame_idx. Only this case uses 2 value entries.
                        pAdaptiveMarkingInfo->value[num_entries*2] = RefPicMarking.difference_of_pic_num_minus1[num_entries];
                        pAdaptiveMarkingInfo->value[num_entries*2+1] = RefPicMarking.long_term_frame_idx[num_entries];
                        break;
                    case 4:
                        // Specify max long term frame idx
                        // Value is max_long_term_frame_idx_plus1
                        // Set to "no long-term frame indices" (-1) when value == 0.
                        pAdaptiveMarkingInfo->value[num_entries*2] = RefPicMarking.max_long_term_frame_idx_plus1[num_entries];
                        break;
                    case 5:
                        // Mark all as unused for reference
                        // no value
                        break;
                    case 6:
                        // Assign long term frame idx to current picture
                        // Value is long_term_frame_idx
                        pAdaptiveMarkingInfo->value[num_entries*2] = RefPicMarking.long_term_frame_idx[num_entries];
                        break;
                    case 0:
                    default:
                        // invalid mmco command in bitstream
                        return UMC_ERR_INVALID_STREAM;
                    }  // switch

                    //if (memory_management_control_operation != 5)
                    //{
                    //    pAdaptiveMarkingInfo->value[num_entries*2] = GetVLCElement(false);
                    //}

                    //// Only mmco 3 requires 2 values
                    //if (memory_management_control_operation == 3)
                    //{
                    //    pAdaptiveMarkingInfo->value[num_entries*2+1] = GetVLCElement(false);
                    //}

                    num_entries++;

                    if (num_entries >= MAX_NUM_REF_FRAMES || num_entries >= NUM_MMCO_OPERATIONS)
                    {
                        return UMC_ERR_INVALID_STREAM;
                    }
                } // while

                if (RefPicMarking.dec_ref_pic_marking_count != num_entries)
                {
                    return UMC_ERR_INVALID_STREAM;
                }

                pAdaptiveMarkingInfo->num_entries = num_entries;
            }

            //if (ps != UMC_OK)
            //{
            //    return ps;
            //}

            //if (hdr->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && hdr->nal_ext.svc_extension_flag &&
            //    spsSvcExt && !spsSvcExt->slice_header_restriction_flag)
            //{
            //    hdr->nal_ext.svc.store_ref_base_pic_flag = Get1Bit();
            //    if ((hdr->nal_ext.svc.use_ref_base_pic_flag ||
            //        hdr->nal_ext.svc.store_ref_base_pic_flag) &&
            //        (!hdr->nal_ext.svc.idr_flag))
            //    {
            //        ps = DecRefBasePicMarking(pBaseAdaptiveMarkingInfo, hdr->nal_ext.svc.adaptive_ref_base_pic_marking_mode_flag);
            //        if (ps != UMC_OK)
            //            return ps;
            //    }
            //}
        }    // def_ref_pic_marking
    }

    //if (pps->entropy_coding_mode == 1  &&    // CABAC
    //    (hdr->slice_type != INTRASLICE && hdr->slice_type != S_INTRASLICE))
    //    hdr->cabac_init_idc = GetVLCElement(false);
    //else
    //    hdr->cabac_init_idc = 0;

    //if (hdr->cabac_init_idc > 2)
    //    return UMC_ERR_INVALID_STREAM;

    //hdr->slice_qp_delta = GetVLCElement(true);

    //if (S_PREDSLICE == hdr->slice_type ||
    //    S_INTRASLICE == hdr->slice_type)
    //{
    //    if (S_PREDSLICE == hdr->slice_type)
    //        hdr->sp_for_switch_flag = Get1Bit();
    //    hdr->slice_qs_delta = GetVLCElement(true);
    //}

    //if (pps->deblocking_filter_variables_present_flag)
    //{
    //    // deblock filter flag and offsets
    //    hdr->disable_deblocking_filter_idc_from_stream = GetVLCElement(false);
    //    if (hdr->disable_deblocking_filter_idc_from_stream > DEBLOCK_FILTER_ON_NO_SLICE_EDGES)
    //        return UMC_ERR_INVALID_STREAM;

    //    if (hdr->disable_deblocking_filter_idc_from_stream != DEBLOCK_FILTER_OFF)
    //    {
    //        hdr->slice_alpha_c0_offset = GetVLCElement(true)<<1;
    //        hdr->slice_beta_offset = GetVLCElement(true)<<1;

    //        if (hdr->slice_alpha_c0_offset < -12 || hdr->slice_alpha_c0_offset > 12)
    //        {
    //            return UMC_ERR_INVALID_STREAM;
    //        }

    //        if (hdr->slice_beta_offset < -12 || hdr->slice_beta_offset > 12)
    //        {
    //            return UMC_ERR_INVALID_STREAM;
    //        }
    //    }
    //    else
    //    {
    //        // set filter offsets to max values to disable filter
    //        hdr->slice_alpha_c0_offset = (Ipp8s)(0 - QP_MAX);
    //        hdr->slice_beta_offset = (Ipp8s)(0 - QP_MAX);
    //    }
    //}

    //if (pps->num_slice_groups > 1 &&
    //    pps->SliceGroupInfo.slice_group_map_type >= 3 &&
    //    pps->SliceGroupInfo.slice_group_map_type <= 5)
    //{
    //    Ipp32u num_bits;    // number of bits used to code slice_group_change_cycle
    //    Ipp32u val;
    //    Ipp32u pic_size_in_map_units;
    //    Ipp32u max_slice_group_change_cycle=0;

    //    // num_bits is Ceil(log2(picsizeinmapunits/slicegroupchangerate + 1))
    //    pic_size_in_map_units = sps->frame_width_in_mbs * sps->frame_height_in_mbs;
    //        // TBD: change above to support fields

    //    max_slice_group_change_cycle = pic_size_in_map_units /
    //                    pps->SliceGroupInfo.t2.slice_group_change_rate;
    //    if (pic_size_in_map_units %
    //                    pps->SliceGroupInfo.t2.slice_group_change_rate)
    //        max_slice_group_change_cycle++;

    //    val = max_slice_group_change_cycle;// + 1;
    //    num_bits = 0;
    //    while (val)
    //    {
    //        num_bits++;
    //        val >>= 1;
    //    }
    //    hdr->slice_group_change_cycle = GetBits(num_bits);
    //    if (hdr->slice_group_change_cycle > max_slice_group_change_cycle)
    //    {
    //        //return UMC_ERR_INVALID_STREAM; don't see any reasons for that
    //    }
    //}

    return UMC_OK;
} // GetSliceHeaderPart3()

Status DecryptParametersWrapper::GetSliceHeaderPart4(H264SliceHeader *hdr,
                                                     const H264SeqParamSetSVCExtension *)
{
    hdr->scan_idx_start = 0;
    hdr->scan_idx_end = 15;
    return UMC_OK;
} // GetSliceHeaderPart4()

void DecryptParametersWrapper::ParseSEIBufferingPeriod(const Headers & headers, H264SEIPayLoad *spl)
{
    spl->Reset();

    //check for zero
    {
        Ipp32s initial_cpb_removal_delay_nal_check_sum = 0;
        for(Ipp32s i=0; i < MAX_CPB_CNT; i++)
            initial_cpb_removal_delay_nal_check_sum += abs(SeiBufferingPeriod.initial_cpb_removal_delay_nal[i]);

        Ipp32s initial_cpb_removal_delay_offset_nal_check_sum = 0;
        for(Ipp32s i=0; i < MAX_CPB_CNT; i++)
            initial_cpb_removal_delay_offset_nal_check_sum += abs(SeiBufferingPeriod.initial_cpb_removal_delay_offset_nal[i]);

        Ipp32s initial_cpb_removal_delay_offset_vcl_check_sum = 0;
        for(Ipp32s i=0; i < MAX_CPB_CNT; i++)
            initial_cpb_removal_delay_offset_vcl_check_sum += abs(SeiBufferingPeriod.initial_cpb_removal_delay_offset_vcl[i]);

        Ipp32s initial_cpb_removal_delay_vcl_check_sum = 0;
        for(Ipp32s i=0; i < MAX_CPB_CNT; i++)
            initial_cpb_removal_delay_vcl_check_sum += abs(SeiBufferingPeriod.initial_cpb_removal_delay_vcl[i]);

        if (!SeiBufferingPeriod.seq_param_set_id &&
            !initial_cpb_removal_delay_nal_check_sum &&
            !initial_cpb_removal_delay_offset_nal_check_sum &&
            !initial_cpb_removal_delay_offset_vcl_check_sum &&
            !initial_cpb_removal_delay_vcl_check_sum)
        {
            spl->isValid = 0;
            return;
        }

    }

    spl->payLoadType = SEI_BUFFERING_PERIOD_TYPE;
    spl->isValid = 1;

    Ipp32s seq_parameter_set_id = (Ipp8u) SeiBufferingPeriod.seq_param_set_id;
    if (seq_parameter_set_id == -1)
    {
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    }

    const H264SeqParamSet *csps = headers.m_SeqParams.GetHeader(seq_parameter_set_id);
    if (!csps)
    {
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    }

    H264SEIPayLoad::SEIMessages::BufferingPeriod &bps = spl->SEI_messages.buffering_period;

    if (csps->vui.nal_hrd_parameters_present_flag)
    {
        if (csps->vui.cpb_cnt >= 32)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

        for(Ipp32s i=0; i < csps->vui.cpb_cnt; i++)
        {
            bps.initial_cpb_removal_delay[0][i] = SeiBufferingPeriod.initial_cpb_removal_delay_nal[i];//GetBits(csps->initial_cpb_removal_delay_length);
            bps.initial_cpb_removal_delay_offset[0][i] = SeiBufferingPeriod.initial_cpb_removal_delay_offset_nal[i];//GetBits(csps->initial_cpb_removal_delay_length);
        }
    }

    if (csps->vui.vcl_hrd_parameters_present_flag)
    {
        if (csps->vui.cpb_cnt >= 32)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

        for(Ipp32s i=0; i < csps->vui.cpb_cnt; i++)
        {
            bps.initial_cpb_removal_delay[1][i] = SeiBufferingPeriod.initial_cpb_removal_delay_vcl[i];//GetBits(csps->cpb_removal_delay_length);
            bps.initial_cpb_removal_delay_offset[1][i] = SeiBufferingPeriod.initial_cpb_removal_delay_offset_vcl[i];//GetBits(csps->cpb_removal_delay_length);
        }
    }
}

void DecryptParametersWrapper::ParseSEIPicTiming(const Headers & headers, H264SEIPayLoad *spl)
{
    spl->Reset();

    //check for zero
    {
        if (!SeiPicTiming.cpb_removal_delay &&
            !SeiPicTiming.dpb_output_delay &&
            !SeiPicTiming.pic_struct)
        {
            spl->isValid = 0;
            return;
        }

    }

    spl->payLoadType = SEI_PIC_TIMING_TYPE;
    spl->isValid = 1;

    Ipp32s seq_parameter_set_id = headers.m_SeqParams.GetCurrentID();;
    if (seq_parameter_set_id == -1)
    {
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    }

    const H264SeqParamSet *csps = headers.m_SeqParams.GetHeader(seq_parameter_set_id);
    if (!csps)
    {
        throw h264_exception(UMC_ERR_INVALID_STREAM);
    }

    //const Ipp8u NumClockTS[]={1,1,1,2,2,3,3,2,3};
    H264SEIPayLoad::SEIMessages::PicTiming &pts = spl->SEI_messages.pic_timing;

    if (csps->vui.nal_hrd_parameters_present_flag || csps->vui.vcl_hrd_parameters_present_flag)
    {
        pts.cbp_removal_delay = SeiPicTiming.cpb_removal_delay;
        pts.dpb_output_delay = SeiPicTiming.dpb_output_delay;
    }
    else
    {
        pts.cbp_removal_delay = INVALID_DPB_OUTPUT_DELAY;
        pts.dpb_output_delay = INVALID_DPB_OUTPUT_DELAY;
    }

    if (csps->vui.pic_struct_present_flag)
    {
        Ipp8u picStruct = (Ipp8u)SeiPicTiming.pic_struct;

        if (picStruct > 8)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

        pts.pic_struct = (DisplayPictureStruct)picStruct;

        //for (Ipp32s i = 0; i < NumClockTS[pts.pic_struct]; i++)
        //{
        //    pts.clock_timestamp_flag[i] = (Ipp8u)Get1Bit();
        //    if (pts.clock_timestamp_flag[i])
        //    {
        //        pts.clock_timestamps[i].ct_type = (Ipp8u)GetBits(2);
        //        pts.clock_timestamps[i].nunit_field_based_flag = (Ipp8u)Get1Bit();
        //        pts.clock_timestamps[i].counting_type = (Ipp8u)GetBits(5);
        //        pts.clock_timestamps[i].full_timestamp_flag = (Ipp8u)Get1Bit();
        //        pts.clock_timestamps[i].discontinuity_flag = (Ipp8u)Get1Bit();
        //        pts.clock_timestamps[i].cnt_dropped_flag = (Ipp8u)Get1Bit();
        //        pts.clock_timestamps[i].n_frames = (Ipp8u)GetBits(8);

        //        if (pts.clock_timestamps[i].full_timestamp_flag)
        //        {
        //            pts.clock_timestamps[i].seconds_value = (Ipp8u)GetBits(6);
        //            pts.clock_timestamps[i].minutes_value = (Ipp8u)GetBits(6);
        //            pts.clock_timestamps[i].hours_value = (Ipp8u)GetBits(5);
        //        }
        //        else
        //        {
        //            if (Get1Bit())
        //            {
        //                pts.clock_timestamps[i].seconds_value = (Ipp8u)GetBits(6);
        //                if (Get1Bit())
        //                {
        //                    pts.clock_timestamps[i].minutes_value = (Ipp8u)GetBits(6);
        //                    if (Get1Bit())
        //                    {
        //                        pts.clock_timestamps[i].hours_value = (Ipp8u)GetBits(5);
        //                    }
        //                }
        //            }
        //        }

        //        if (csps->time_offset_length > 0)
        //            pts.clock_timestamps[i].time_offset = (Ipp8u)GetBits(csps->time_offset_length);
        //    }
        //}
    }
}

void DecryptParametersWrapper::ParseSEIRecoveryPoint(H264SEIPayLoad *spl)
{
    spl->Reset();

    //check for zero
    {
        if (!SeiRecoveryPoint.broken_link_flag &&
            !SeiRecoveryPoint.changing_slice_group_idc &&
            !SeiRecoveryPoint.exact_match_flag &&
            !SeiRecoveryPoint.recovery_frame_cnt)
        {
            spl->isValid = 0;
            return;
        }

    }

    spl->payLoadType = SEI_RECOVERY_POINT_TYPE;
    spl->isValid = 1;

    H264SEIPayLoad::SEIMessages::RecoveryPoint * recPoint = &(spl->SEI_messages.recovery_point);

    recPoint->recovery_frame_cnt = (Ipp8u)SeiRecoveryPoint.recovery_frame_cnt;

    recPoint->exact_match_flag = (Ipp8u)SeiRecoveryPoint.exact_match_flag;
    recPoint->broken_link_flag = (Ipp8u)SeiRecoveryPoint.broken_link_flag;
    recPoint->changing_slice_group_idc = (Ipp8u)SeiRecoveryPoint.changing_slice_group_idc;

    if (recPoint->changing_slice_group_idc > 2)
    {
        spl->isValid = 0;
    }
}

#ifdef UMC_VA_DXVA
Status WidevineDecrypter::DecryptFrame(MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    if (!pSource)
    {
        return UMC_OK;
    }

    IDirectXVideoDecoder *pDXVAVideoDecoder;
    m_va->GetVideoDecoder((void**)&pDXVAVideoDecoder);
    if (!pDXVAVideoDecoder)
        return UMC_ERR_NOT_INITIALIZED;

    IDirectXVideoDecoderService *pDecoderService;
    HRESULT hr = pDXVAVideoDecoder->GetVideoDecoderService(&pDecoderService);
    if (FAILED(hr) || (!pDecoderService))
    {
        return UMC_ERR_DEVICE_FAILED;
    }

    if (!m_bitstreamSubmitted && (pSource->GetDataSize() != 0))
    {
        if (!m_pDummySurface)
        {
            hr = pDecoderService->CreateSurface(640,
                                                480,
                                                0,
                                                D3DFMT_NV12,
                                                D3DPOOL_DEFAULT,
                                                0,
                                                DXVA2_VideoDecoderRenderTarget,
                                                &m_pDummySurface,
                                                NULL);
            if (FAILED(hr))
            {
                return UMC_ERR_ALLOC;
            }
        }

        hr = pDXVAVideoDecoder->BeginFrame(m_pDummySurface, NULL);
        if (FAILED(hr))
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        DXVA2_DecodeExtensionData DecryptInfo;
        DXVA2_DecodeExecuteParams ExecuteParams;
        DXVA2_DecodeBufferDesc BufferDesc;
        UINT DXVABitStreamBufferSize = 0;
        DECRYPT_QUERY_STATUS_PARAMS_AVC DecryptStatus;
        memset(&DecryptStatus, 0xFF, sizeof(DECRYPT_QUERY_STATUS_PARAMS_AVC));
        DECRYPT_QUERY_STATUS_PARAMS_AVC* pDecryptStatusReturned = NULL;

        VADecryptInputBuffer PESInputParams;
        VADecryptSegmentInfo SegmentInfo;
        PESInputParams.pSegmentInfo = &SegmentInfo;

        PESInputParams.usStatusReportFeedbackNumber = m_PESPacketCounter++;
        PESInputParams.uiNumSegments = 0;

        PESInputParams.pSegmentInfo->uiSegmentStartOffset = 0;
        PESInputParams.pSegmentInfo->uiSegmentLength = 0;
        PESInputParams.pSegmentInfo->uiPartialAesBlockSizeInBytes = 0;
        PESInputParams.pSegmentInfo->uiInitByteLength = 0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[0] = 0x0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[1] = 0x0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[2] = 0x0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[3] = 0x0;

        PESInputParams.ucSizeOfLength = 0;
        PESInputParams.uiAppId = 0;

        //do the decrypt with the first execute call...
        DecryptInfo.Function = INTEL_DECODE_EXTENSION_DECRYPT_WIDEVINE_CLASSIC;
        DecryptInfo.pPrivateInputData = &PESInputParams;
        DecryptInfo.PrivateInputDataSize = sizeof(VADecryptInputBuffer);
        DecryptInfo.pPrivateOutputData = NULL;
        DecryptInfo.PrivateOutputDataSize = 0;

        BufferDesc.CompressedBufferType = DXVA2_BitStreamDateBufferType;
        BufferDesc.BufferIndex = 0;
        BufferDesc.DataOffset = 0;
        BufferDesc.DataSize = (UINT)pSource->GetDataSize();
        BufferDesc.FirstMBaddress = 0;
        BufferDesc.NumMBsInBuffer = 0;
        BufferDesc.Width = 0;
        BufferDesc.Height = 0;
        BufferDesc.Stride = 0;
        BufferDesc.ReservedBits = 0;
        BufferDesc.pvPVPState = NULL;

        void *pDXVA_BitStreamBuffer = NULL;
        hr = pDXVAVideoDecoder->GetBuffer(DXVA2_BitStreamDateBufferType, &pDXVA_BitStreamBuffer, &DXVABitStreamBufferSize);
        if (FAILED(hr))
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        if (DXVABitStreamBufferSize < pSource->GetDataSize())
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        memcpy_s(pDXVA_BitStreamBuffer, DXVABitStreamBufferSize, pSource->GetDataPointer(), pSource->GetDataSize());

        hr = pDXVAVideoDecoder->ReleaseBuffer(DXVA2_BitStreamDateBufferType);
        if (FAILED(hr))
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        ExecuteParams.NumCompBuffers = 1;
        ExecuteParams.pCompressedBuffers = &BufferDesc;
        ExecuteParams.pExtensionData = &DecryptInfo;

        hr = pDXVAVideoDecoder->Execute(&ExecuteParams);
        if (FAILED(hr))
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        //get the decrypt status with the second execute call...
        DecryptStatus.usStatusReportFeedbackNumber = PESInputParams.usStatusReportFeedbackNumber;

        DecryptInfo.Function = INTEL_DECODE_EXTENSION_GET_DECRYPT_STATUS;
        DecryptInfo.pPrivateInputData = NULL;
        DecryptInfo.PrivateInputDataSize = 0;
        DecryptInfo.pPrivateOutputData = &DecryptStatus;
        DecryptInfo.PrivateOutputDataSize = sizeof(DECRYPT_QUERY_STATUS_PARAMS_AVC);

        ExecuteParams.NumCompBuffers = 0;
        ExecuteParams.pCompressedBuffers = NULL;
        ExecuteParams.pExtensionData = &DecryptInfo;

        do
        {
            hr = pDXVAVideoDecoder->Execute(&ExecuteParams);
            if (FAILED(hr))
            {
                return UMC_ERR_DEVICE_FAILED;
            }

            //cast the returned output data pointer to the DecryptStatus structure... the driver returns a newly allocated structure back!
            pDecryptStatusReturned = (DECRYPT_QUERY_STATUS_PARAMS_AVC *) (ExecuteParams.pExtensionData->pPrivateOutputData);
        }
        while (pDecryptStatusReturned->status == VA_DECRYPT_STATUS_INCOMPLETE);

        if (pDecryptStatusReturned->status != VA_DECRYPT_STATUS_SUCCESSFUL)
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        if (pDecryptStatusReturned->usStatusReportFeedbackNumber != PESInputParams.usStatusReportFeedbackNumber)
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        hr = pDXVAVideoDecoder->EndFrame(NULL);
        if (FAILED(hr))
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        *pDecryptParams = *pDecryptStatusReturned;

        pSource->MoveDataPointer((Ipp32s)pSource->GetDataSize());

        m_bitstreamSubmitted = true;
    }

    return UMC_OK;
}
#elif defined(UMC_VA_LINUX)
Status WidevineDecrypter::DecryptFrame(MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    if (!pSource)
    {
        return UMC_OK;
    }

    /*if (!m_dpy)
    {
        m_va->GetVideoDecoder((void**)&m_dpy);
    }

    if (!m_context)
    {
        m_va->GetVideoDecoderContext((void*)&m_context);
    }

    size_t dataSize = pSource->GetDataSize();
    if (!m_bitstreamSubmitted && (dataSize != 0))
    {
        VAStatus   va_res = VA_STATUS_SUCCESS;

        VADecryptInputBuffer PESInputParams;
        VADecryptSegmentInfo SegmentInfo;
        PESInputParams.pSegmentInfo = &SegmentInfo;
        PESInputParams.usStatusReportFeedbackNumber = m_PESPacketCounter++;
        PESInputParams.dwNumSegments = 0;//1;                 //WA for unencrypted segments
        PESInputParams.pSegmentInfo->dwSegmentStartOffset = 0;
        PESInputParams.pSegmentInfo->dwSegmentLength = dataSize;
        PESInputParams.pSegmentInfo->dwPartialAesBlockSizeInBytes = 0;
        PESInputParams.pSegmentInfo->dwInitByteLength = 0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[0] = 0x0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[1] = 0x0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[2] = 0x0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[3] = 0x0;
        PESInputParams.uiSizeOfLength = 0;
        PESInputParams.uiAppId = 0;

        UMCVACompBuffer* compBuf = NULL;

        m_va->GetCompBuffer(VAProtectedSliceDataBufferType, &compBuf, dataSize);
        if (!compBuf)
            throw h264_exception(UMC_ERR_FAILED);

        memcpy(compBuf->GetPtr(), pSource->GetDataPointer(), dataSize);
        compBuf->SetDataSize(dataSize);

        compBuf = NULL;
        m_va->GetCompBuffer(VADecryptParameterBufferType, &compBuf, sizeof(VADecryptInputBuffer));
        if (!compBuf)
            throw h264_exception(UMC_ERR_FAILED);

        memcpy(compBuf->GetPtr(), &PESInputParams, sizeof(VADecryptInputBuffer));
        compBuf->SetDataSize(sizeof(VADecryptInputBuffer));

        Status sts = m_va->Execute();
        if (sts != UMC_OK)
            throw h264_exception(sts);

        sts = m_va->EndFrame();
        if (sts != UMC_OK)
            throw h264_exception(sts);

        DECRYPT_QUERY_STATUS_PARAMS_AVC AvcParams;
        memset(&AvcParams, 0, sizeof(DECRYPT_QUERY_STATUS_PARAMS_AVC));
        AvcParams.usStatusReportFeedbackNumber = PESInputParams.usStatusReportFeedbackNumber;
        AvcParams.status = VA_DECRYPT_STATUS_INCOMPLETE;

        IntelDecryptStatus DecryptStatus;
        DecryptStatus.decrypt_status_requested = (VASurfaceStatus)VADecryptStatusRequested;
        DecryptStatus.context = m_context;
        DecryptStatus.data_size = sizeof(DECRYPT_QUERY_STATUS_PARAMS_AVC);
        DecryptStatus.data = &AvcParams;
        do
        {

            va_res = vaQuerySurfaceStatus(m_dpy, VA_INVALID_SURFACE, (VASurfaceStatus*)&DecryptStatus);
            if (VA_STATUS_SUCCESS != va_res)
                throw h264_exception(UMC_ERR_FAILED);
        }
        while (AvcParams.status == VA_DECRYPT_STATUS_INCOMPLETE);

        if (AvcParams.status != VA_DECRYPT_STATUS_SUCCESSFUL)
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        if (AvcParams.usStatusReportFeedbackNumber != PESInputParams.usStatusReportFeedbackNumber)
        {
            return UMC_ERR_DEVICE_FAILED;
        }

        *pDecryptParams = AvcParams;

        pSource->MoveDataPointer(dataSize);

        m_bitstreamSubmitted = true;
    }*/

    return UMC_OK;
}
#else
Status WidevineDecrypter::DecryptFrame(MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    return UMC_ERR_NOT_IMPLEMENTED;
}
#endif

} // namespace UMC

#endif // UMC_ENABLE_H264_VIDEO_DECODER
