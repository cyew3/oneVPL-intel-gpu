#include "avc2_parser.h"

using namespace BS_AVC2;
using namespace BsReader2;

bool SDParser::more_rbsp_data()
{
    Bs8u b[5];

    if (!NextBytes(b, 5))
        return !TrailingBits(true);

    if (   (b[0] == 0 && b[1] == 0 && b[2] == 1)
        || (b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 1))
        return false;

    if (   (b[1] == 0 && b[2] == 0 && b[3] == 1)
        || (b[1] == 0 && b[2] == 0 && b[3] == 0 && b[4] == 1))
        return !TrailingBits(true);

    return true;
}

void Parser::parseNUH(NALU& nalu)
{
    TLAuto tl(*this, TRACE_NALU);

    if (TLTest(TRACE_MARKERS))
        BS2_TRACE_STR("====================== NAL UNIT =====================");

    TLStart(TRACE_OFFSET);
    BS2_TRACE(nalu.StartOffset, nalu.StartOffset);
    TLEnd();

    if (u1())//forbidden_zero_bit
        throw InvalidSyntax();

    BS2_SET(u(2), nalu.nal_ref_idc);
    BS2_SETM(u(5), nalu.nal_unit_type, NaluTraceMap());

    if (nalu.nal_unit_type == PREFIX_NUT
        || nalu.nal_unit_type == SLICE_EXT)
    {
        BS2_SET(u1(), nalu.svc_extension_flag);

        if (nalu.svc_extension_flag)
        {
            NUH_SVC_ext& svc = nalu.svc_ext;

            BS2_SET(u1(), svc.idr_flag);
            BS2_SET(u(6), svc.priority_id);
            BS2_SET(u1(), svc.no_inter_layer_pred_flag);
            BS2_SET(u(3), svc.dependency_id);
            BS2_SET(u(4), svc.quality_id);
            BS2_SET(u(3), svc.temporal_id);
            BS2_SET(u1(), svc.use_ref_base_pic_flag);
            BS2_SET(u1(), svc.discardable_flag);
            BS2_SET(u1(), svc.output_flag);
            u(2);
        }
        else
        {
            NUH_MVC_ext& mvc = nalu.mvc_ext;

            BS2_SET(u1(), mvc.non_idr_flag);
            BS2_SET(u(6), mvc.priority_id);
            BS2_SET(u(10), mvc.view_id);
            BS2_SET(u(3), mvc.temporal_id);
            BS2_SET(u1(), mvc.anchor_pic_flag);
            BS2_SET(u1(), mvc.inter_view_flag);
            u1();
        }
    }
}

void Parser::parseRBSP(NALU& nalu)
{
    TLAuto tl(*this, TRACE_MARKERS);

    switch (nalu.nal_unit_type)
    {
    case SLICE_NONIDR:
    case SLICE_IDR:
    case SLICE_AUX:
        BS2_TRACE_STR(" --- SLICE --- ");
        nalu.slice = alloc<Slice>(&nalu);
        parseSliceHeader(nalu);
        break;
    case SD_PART_A:
        BS2_TRACE_STR(" --- SD_PART_A --- ");
        break;
    case SD_PART_B:
        BS2_TRACE_STR(" --- SD_PART_B --- ");
        break;
    case SD_PART_C:
        BS2_TRACE_STR(" --- SD_PART_C --- ");
        break;
    case SEI_NUT:
        BS2_TRACE_STR(" --- SEI --- ");
        nalu.sei = alloc<SEI>(&nalu);
        parseSEI(*nalu.sei);
        break;
    case SPS_NUT:
        BS2_TRACE_STR(" --- SPS --- ");
        nalu.sps = alloc<SPS>(&nalu);
        parseSPS(*nalu.sps);
        unlock(m_sps[nalu.sps->seq_parameter_set_id]);
        lock(m_sps[nalu.sps->seq_parameter_set_id] = nalu.sps);
        break;
    case PPS_NUT:
        BS2_TRACE_STR(" --- PPS --- ");
        nalu.pps = alloc<PPS>(&nalu);
        parsePPS(*nalu.pps);
        unlock(m_pps[nalu.pps->pic_parameter_set_id]);
        lock(m_pps[nalu.pps->pic_parameter_set_id] = nalu.pps);
        break;
    case AUD_NUT:
        BS2_TRACE_STR(" --- AUD --- ");
        nalu.aud = alloc<AUD>(&nalu);
        {
            AUD& aud = *nalu.aud;
            BS2_SET(u(3), aud.primary_pic_type);
            if (!TrailingBits())
                throw InvalidSyntax();
        }
        break;
    case EOSEQ_NUT:
        BS2_TRACE_STR(" --- END OF SEQ --- ");
        break;
    case EOSTR_NUT:
        BS2_TRACE_STR(" --- END OF STREAM --- ");
        break;
    case FD_NUT:
        BS2_TRACE_STR(" --- FILLER DATA --- ");
        break;
    case SPS_EXT_NUT:
        BS2_TRACE_STR(" --- SPS EXT --- ");
        break;
    case PREFIX_NUT:
        BS2_TRACE_STR(" --- PREFIX --- ");
        break;
    case SUBSPS_NUT:
        BS2_TRACE_STR(" --- SUBSET SPS --- ");
        break;
    case SLICE_EXT:
        BS2_TRACE_STR(" --- SLICE EXT --- ");
        break;

    default:
        break;
    }
}

void Parser::parseSPS(SPS& sps)
{
    TLAuto tl(*this, TRACE_SPS);

    BS2_SETM(u8(), sps.profile_idc, ProfileTraceMap());

    BS2_SET(u1(), sps.constraint_set0_flag);
    BS2_SET(u1(), sps.constraint_set1_flag);
    BS2_SET(u1(), sps.constraint_set2_flag);
    BS2_SET(u1(), sps.constraint_set3_flag);
    BS2_SET(u1(), sps.constraint_set4_flag);
    BS2_SET(u1(), sps.constraint_set5_flag);
    u(2);//reserved_zero_2bits

    BS2_SET(u8(), sps.level_idc);
    BS2_SET(ue(), sps.seq_parameter_set_id);

    sps.chroma_format_idc = 1;

    switch (sps.profile_idc)
    {
    case HIGH:
    case HIGH_10:
    case HIGH_422:
    case HIGH_444:
    case MULTIVIEW_HIGH:
    case STEREO_HIGH:
    case SVC_BASELINE:
    case SVC_HIGH:
        BS2_SET(ue(), sps.chroma_format_idc);

        if (sps.chroma_format_idc == 3)
            BS2_SET(u1(), sps.separate_colour_plane_flag);

        BS2_SET(ue(), sps.bit_depth_luma_minus8);
        BS2_SET(ue(), sps.bit_depth_chroma_minus8);

        BS2_SET(u1(), sps.qpprime_y_zero_transform_bypass_flag);
        BS2_SET(u1(), sps.seq_scaling_matrix_present_flag);

        if (sps.seq_scaling_matrix_present_flag)
        {
            sps.scaling_matrix = alloc<ScalingMatrix>(&sps);
            parseScalingMatrix(*sps.scaling_matrix, sps.chroma_format_idc != 3 ? 8 : 12);
        }

        break;
    default:
        break;
    }

    BS2_SET(ue(), sps.log2_max_frame_num_minus4);
    BS2_SET(ue(), sps.pic_order_cnt_type);

    if (sps.pic_order_cnt_type == 0)
    {
        BS2_SET(ue(), sps.log2_max_pic_order_cnt_lsb_minus4);
    }
    else if (sps.pic_order_cnt_type == 1)
    {
        BS2_SET(u1(), sps.delta_pic_order_always_zero_flag);
        BS2_SET(se(), sps.offset_for_non_ref_pic);
        BS2_SET(se(), sps.offset_for_top_to_bottom_field);
        BS2_SET(ue(), sps.num_ref_frames_in_pic_order_cnt_cycle);

        sps.offset_for_ref_frame = alloc<Bs32s>(&sps,
            sps.num_ref_frames_in_pic_order_cnt_cycle);

        BS2_SET_ARR(se(), sps.offset_for_ref_frame,
            sps.num_ref_frames_in_pic_order_cnt_cycle,
            BS_MIN(16, sps.num_ref_frames_in_pic_order_cnt_cycle - 1));
    }

    BS2_SET(ue(), sps.max_num_ref_frames);
    BS2_SET(u1(), sps.gaps_in_frame_num_value_allowed_flag);
    BS2_SET(ue(), sps.pic_width_in_mbs_minus1);
    BS2_SET(ue(), sps.pic_height_in_map_units_minus1);
    BS2_SET(u1(), sps.frame_mbs_only_flag);

    if (!sps.frame_mbs_only_flag)
        BS2_SET(u1(), sps.mb_adaptive_frame_field_flag);

    BS2_SET(u1(), sps.direct_8x8_inference_flag);

    BS2_SET(u1(), sps.frame_cropping_flag);

    if (sps.frame_cropping_flag)
    {
        BS2_SET(ue(), sps.frame_crop_left_offset);
        BS2_SET(ue(), sps.frame_crop_right_offset);
        BS2_SET(ue(), sps.frame_crop_top_offset);
        BS2_SET(ue(), sps.frame_crop_bottom_offset);
    }

    BS2_SET(u1(), sps.vui_parameters_present_flag);

    if (sps.vui_parameters_present_flag)
    {
        sps.vui = alloc<VUI>(&sps);
        parseVUI(*sps.vui);
    }

}

void Parser::parseScalingMatrix(ScalingMatrix& sm, Bs32u nFlags)
{
    for (Bs32u i = 0; i < nFlags; i++)
    {
        sm.scaling_list_present_flag[i] = u1();
        if (sm.scaling_list_present_flag[i])
        {
            if (i < 6)
            {
                sm.UseDefaultScalingMatrix4x4Flag[i] = parseScalingList(sm.ScalingList4x4[i], 16);
            }
            else
            {
                sm.UseDefaultScalingMatrix8x8Flag[i-6] = parseScalingList(sm.ScalingList8x8[i-6], 64);
            }
        }
    }

    BS2_TRACE_ARR(sm.scaling_list_present_flag, nFlags, 0);
    BS2_TRACE_ARR(sm.UseDefaultScalingMatrix4x4Flag, 6, 0);

    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix4x4Flag[0]
        && sm.scaling_list_present_flag[0])
        BS2_TRACE_ARR(sm.ScalingList4x4[0], 16, 0);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix4x4Flag[1]
        && sm.scaling_list_present_flag[1])
        BS2_TRACE_ARR(sm.ScalingList4x4[1], 16, 0);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix4x4Flag[2]
        && sm.scaling_list_present_flag[2])
        BS2_TRACE_ARR(sm.ScalingList4x4[2], 16, 0);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix4x4Flag[3]
        && sm.scaling_list_present_flag[3])
        BS2_TRACE_ARR(sm.ScalingList4x4[3], 16, 0);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix4x4Flag[4]
        && sm.scaling_list_present_flag[4])
        BS2_TRACE_ARR(sm.ScalingList4x4[4], 16, 0);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix4x4Flag[4]
        && sm.scaling_list_present_flag[5])
        BS2_TRACE_ARR(sm.ScalingList4x4[5], 16, 0);


    BS2_TRACE_ARR(sm.UseDefaultScalingMatrix8x8Flag, 6, 0);

    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix8x8Flag[0]
        && sm.scaling_list_present_flag[6])
        BS2_TRACE_ARR(sm.ScalingList8x8[0], 64, 16);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix8x8Flag[1]
        && sm.scaling_list_present_flag[7])
        BS2_TRACE_ARR(sm.ScalingList8x8[1], 64, 16);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix8x8Flag[2]
        && sm.scaling_list_present_flag[8])
        BS2_TRACE_ARR(sm.ScalingList8x8[2], 64, 16);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix8x8Flag[3]
        && sm.scaling_list_present_flag[9])
        BS2_TRACE_ARR(sm.ScalingList8x8[3], 64, 16);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix8x8Flag[4]
        && sm.scaling_list_present_flag[10])
        BS2_TRACE_ARR(sm.ScalingList8x8[4], 64, 16);
    if (   nFlags-- > 0
        && !sm.UseDefaultScalingMatrix8x8Flag[4]
        && sm.scaling_list_present_flag[11])
        BS2_TRACE_ARR(sm.ScalingList8x8[5], 64, 16);

}

bool Parser::parseScalingList(Bs8u* sl, Bs32u sz)
{
    Bs8u last = 8;
    Bs8u next = 8;
    bool useDefault = false;

    for (Bs32u i = 0; i < sz; i++)
    {
        if (next)
        {
            next = (last + se() + 256) % 256;
            useDefault = !i && !next;
        }
        last = sl[i] = next ? next : last;
    }

    return useDefault;
}

void Parser::parseVUI(VUI& vui)
{
    TLAuto tl(*this, TRACE_VUI);

    BS2_SET(u1(), vui.aspect_ratio_info_present_flag);

    if (vui.aspect_ratio_info_present_flag)
    {
        BS2_SETM(u8(), vui.aspect_ratio_idc, ARIdcTraceMap());

        if (vui.aspect_ratio_idc == Extended_SAR)
        {
            BS2_SET(u(16), vui.sar_width);
            BS2_SET(u(16), vui.sar_height);
        }
    }

    BS2_SET(u1(), vui.overscan_info_present_flag);

    if (vui.overscan_info_present_flag)
        BS2_SET(u1(), vui.overscan_appropriate_flag);

    BS2_SET(u1(), vui.video_signal_type_present_flag);

    if (vui.video_signal_type_present_flag)
    {
        BS2_SET(u(3), vui.video_format);
        BS2_SET(u1(), vui.video_full_range_flag);
        BS2_SET(u1(), vui.colour_description_present_flag);

        if (vui.colour_description_present_flag)
        {
            BS2_SET(u8(), vui.colour_primaries);
            BS2_SET(u8(), vui.transfer_characteristics);
            BS2_SET(u8(), vui.matrix_coefficients);
        }
    }

    BS2_SET(u1(), vui.chroma_loc_info_present_flag);

    if (vui.chroma_loc_info_present_flag)
    {
        BS2_SET(ue(), vui.chroma_sample_loc_type_top_field);
        BS2_SET(ue(), vui.chroma_sample_loc_type_bottom_field);
    }

    BS2_SET(u1(), vui.timing_info_present_flag);

    if (vui.timing_info_present_flag)
    {
        BS2_SET(u(32), vui.num_units_in_tick);
        BS2_SET(u(32), vui.time_scale);
        BS2_SET(u1(), vui.fixed_frame_rate_flag);
    }

    BS2_SET(u1(), vui.nal_hrd_parameters_present_flag);

    if (vui.nal_hrd_parameters_present_flag)
    {
        vui.nal_hrd = alloc<HRD>(&vui);
        parseHRD(*vui.nal_hrd);
    }

    BS2_SET(u1(), vui.vcl_hrd_parameters_present_flag);

    if (vui.vcl_hrd_parameters_present_flag)
    {
        vui.vcl_hrd = alloc<HRD>(&vui);
        parseHRD(*vui.vcl_hrd);
    }

    if (   vui.nal_hrd_parameters_present_flag
        || vui.vcl_hrd_parameters_present_flag)
        BS2_SET(u1(), vui.low_delay_hrd_flag);

    BS2_SET(u1(), vui.pic_struct_present_flag);
    BS2_SET(u1(), vui.bitstream_restriction_flag);

    if (vui.bitstream_restriction_flag)
    {
        BS2_SET(u1(), vui.motion_vectors_over_pic_boundaries_flag);
        BS2_SET(ue(), vui.max_bytes_per_pic_denom);
        BS2_SET(ue(), vui.max_bits_per_mb_denom);
        BS2_SET(ue(), vui.log2_max_mv_length_horizontal);
        BS2_SET(ue(), vui.log2_max_mv_length_vertical);
        BS2_SET(ue(), vui.num_reorder_frames);
        BS2_SET(ue(), vui.max_dec_frame_buffering);
    }
}

void Parser::parseHRD(HRD& hrd)
{
    TLAuto tl(*this, TRACE_HRD);

    BS2_SET(ue(), hrd.cpb_cnt_minus1);
    BS2_SET(u(4), hrd.bit_rate_scale);
    BS2_SET(u(4), hrd.cpb_size_scale);

    for (Bs32u i = 0; i <= hrd.cpb_cnt_minus1; i++)
    {
        BS2_SET(ue(), hrd.bit_rate_value_minus1[i]);
        BS2_SET(ue(), hrd.cpb_size_value_minus1[i]);
        BS2_SET(u1(), hrd.cbr_flag[i]);
    }

    BS2_SET(u(5), hrd.initial_cpb_removal_delay_length_minus1);
    BS2_SET(u(5), hrd.cpb_removal_delay_length_minus1);
    BS2_SET(u(5), hrd.dpb_output_delay_length_minus1);
    BS2_SET(u(5), hrd.time_offset_length);
}

void Parser::parsePPS(PPS& pps)
{
    TLAuto tl(*this, TRACE_PPS);

    BS2_SET(ue(), pps.pic_parameter_set_id);
    BS2_SET(ue(), pps.seq_parameter_set_id);

    pps.sps = m_sps[pps.seq_parameter_set_id];
    if (!pps.sps)
        throw InvalidSyntax();
    bound(pps.sps, &pps);

    SPS const & sps = *pps.sps;

    BS2_SET(u1(), pps.entropy_coding_mode_flag);
    BS2_SET(u1(), pps.bottom_field_pic_order_in_frame_present_flag);

    BS2_SET(ue(), pps.num_slice_groups_minus1);

    if (pps.num_slice_groups_minus1)
    {
        Bs32u nBits;

        BS2_SET(ue(), pps.slice_group_map_type);

        switch (pps.slice_group_map_type)
        {
        case 0:
            pps.run_length_minus1 = alloc<Bs32u>(&pps, pps.num_slice_groups_minus1 + 1);
            BS2_SET_ARR(ue(), pps.run_length_minus1, Bs32u(pps.num_slice_groups_minus1 + 1), 0);
            break;
        case 1:
            break;
        case 2:
            pps.top_left = alloc<Bs32u>(&pps, pps.num_slice_groups_minus1 + 1);
            pps.bottom_right = alloc<Bs32u>(&pps, pps.num_slice_groups_minus1 + 1);

            for (Bs32u i = 0; i < pps.num_slice_groups_minus1; i++)
            {
                pps.top_left[i] = ue();
                pps.bottom_right[i] = ue();
            }
            BS2_TRACE_ARR(pps.top_left, pps.num_slice_groups_minus1, 0);
            BS2_TRACE_ARR(pps.bottom_right, pps.num_slice_groups_minus1, 0);
            break;
        case 3:
        case 4:
        case 5:
            BS2_SET(u1(), pps.slice_group_change_direction_flag);
            BS2_SET(ue(), pps.slice_group_change_rate_minus1);
            break;
        case 6:
            BS2_SET(ue(), pps.pic_size_in_map_units_minus1);
            pps.slice_group_id = alloc<Bs8u>(&pps, pps.pic_size_in_map_units_minus1 + 1);
            nBits = 1 + (pps.pic_size_in_map_units_minus1 > 3) + (pps.pic_size_in_map_units_minus1 > 2);

            BS2_SET_ARR(u(nBits), pps.slice_group_id, pps.pic_size_in_map_units_minus1 + 1, sps.pic_width_in_mbs_minus1 + 1);

            break;
        default:
            throw InvalidSyntax();
        }
    }

    BS2_SET(ue(), pps.num_ref_idx_l0_default_active_minus1);
    BS2_SET(ue(), pps.num_ref_idx_l1_default_active_minus1);
    BS2_SET(u1(), pps.weighted_pred_flag);
    BS2_SET(u(2), pps.weighted_bipred_idc);
    BS2_SET(se(), pps.pic_init_qp_minus26);
    BS2_SET(se(), pps.pic_init_qs_minus26);
    BS2_SET(se(), pps.chroma_qp_index_offset);
    BS2_SET(u1(), pps.deblocking_filter_control_present_flag);
    BS2_SET(u1(), pps.constrained_intra_pred_flag);
    BS2_SET(u1(), pps.redundant_pic_cnt_present_flag);

    pps.second_chroma_qp_index_offset = pps.chroma_qp_index_offset;

    if (more_rbsp_data())
    {
        BS2_SET(u1(), pps.transform_8x8_mode_flag);
        BS2_SET(u1(), pps.pic_scaling_matrix_present_flag);

        if (pps.pic_scaling_matrix_present_flag)
        {
            pps.scaling_matrix = alloc<ScalingMatrix>(&pps);
            parseScalingMatrix(*pps.scaling_matrix, 6 + ((sps.chroma_format_idc != 3) ? 2 : 6) * pps.transform_8x8_mode_flag);
        }

        BS2_SET(se(), pps.second_chroma_qp_index_offset);
    }

    if (!TrailingBits())
        throw InvalidSyntax();
}

bool Parser::singleSPS(SPS*& activeSPS)
{
    Bs32u n = 0;

    for (Bs32u i = 0; i < 32; i++)
    {
        if (m_sps[i])
        {
            activeSPS = m_sps[i];
            n++;
        }
    }

    if (n != 1)
    {
        activeSPS = 0;
        return false;
    }

    return true;
}

void Parser::parseSEI(SEI& first)
{
    TLAuto tl(*this, TRACE_SEI);

    SEI* next = &first;
    SPS* activeSPS = 0;
    bool postpone = false;

    for (;;)
    {
        Bs8u lastByte;
        bool saveRaw = false;
        SEI& sei = *next;

        do
        {
            lastByte = u8();
            sei.payloadType += lastByte;
        } while (lastByte == 0xff);

        BS2_SETM(sei.payloadType, sei.payloadType, SEIPayloadTraceMap());

        do
        {
            lastByte = u8();
            sei.payloadSize += lastByte;
        } while (lastByte == 0xff);

        BS2_TRACE(sei.payloadSize, sei.payloadSize);

        TLStart(TRACE_MARKERS);

        switch (sei.payloadType)
        {
        case BUFFERING_PERIOD:
            BS2_TRACE_STR(" --- BUFFERING_PERIOD --- ");

            sei.bp = alloc<BufferingPeriod>(&sei);
            parseSEIPayload(*sei.bp);
            activeSPS = sei.bp->sps;
            break;
        case PIC_TIMING:
            BS2_TRACE_STR(" --- PIC_TIMING --- ");

            if (!activeSPS && !singleSPS(activeSPS))
            {
                postpone = true;
                saveRaw = true;
                break;
            }

            sei.pt = alloc<PicTiming>(&sei);
            sei.pt->sps = activeSPS;
            bound(sei.pt->sps, sei.pt);
            parseSEIPayload(*sei.pt);
            break;
        case DEC_REF_PIC_MARKING_REPETITION:
            BS2_TRACE_STR(" --- DEC_REF_PIC_MARKING_REPETITION --- ");

            if (!activeSPS && !singleSPS(activeSPS))
            {
                postpone = true;
                saveRaw = true;
                break;
            }

            sei.drpm_rep = alloc<DRPMRep>(&sei);
            sei.drpm_rep->sps = activeSPS;
            bound(sei.drpm_rep->sps, sei.drpm_rep);
            parseSEIPayload(*sei.drpm_rep);
            break;
        default:
            if (m_mode & INIT_MODE_SAVE_UNKNOWN_SEI)
                saveRaw = true;
            else
                for (Bs32u i = 0; i < sei.payloadSize; i++)
                    GetByte();
            break;
        }

        TLEnd();

        if (saveRaw)
        {
            TLAuto tl1(*this, TRACE_RESIDUAL_ARRAYS);

            sei.rawData = alloc<Bs8u>(&sei, sei.payloadSize);
            BS2_SET_ARR_F(GetByte(), sei.rawData, sei.payloadSize, 16, "%02X ");
        }

        if (GetBitOffset() && !TrailingBits())
            throw InvalidSyntax();

        if (!more_rbsp_data())
            break;

        next = sei.next = alloc<SEI>(&sei);
    }

    if (!TrailingBits())
        throw InvalidSyntax();

    if (postpone)
        m_postponed_sei.push_back(&first);

}

void Parser::parsePostponedSEI(SEI& first, Slice& s)
{
    TLAuto tl(*this, TRACE_MARKERS);
    BsReader2::State st;

    SaveState(st);
    SetEmulation(false);

    try
    {
        BS2_TRACE_STR("~~~~~~~~~~~~~~~ POSTPONED SEI PARSING ~~~~~~~~~~~~~~~");

        for (SEI* next = &first; next != 0; next = next->next)
        {
            SEI& sei = *next;

            if (sei.rawData)
                Reset(sei.rawData, sei.payloadSize);

            switch (sei.payloadType)
            {
            case PIC_TIMING:
                BS2_TRACE_STR(" --- PIC_TIMING --- ");
                sei.pt = alloc<PicTiming>(&sei);
                sei.pt->sps = s.pps->sps;
                bound(sei.pt->sps, sei.pt);
                parseSEIPayload(*sei.pt);
                break;
            case DEC_REF_PIC_MARKING_REPETITION:
                BS2_TRACE_STR(" --- DEC_REF_PIC_MARKING_REPETITION --- ");
                sei.drpm_rep = alloc<DRPMRep>(&sei);
                sei.drpm_rep->sps = s.pps->sps;
                bound(sei.drpm_rep->sps, sei.drpm_rep);
                parseSEIPayload(*sei.drpm_rep);
                break;
            default:
                break;
            }
        }
    }
    catch (EndOfBuffer)
    {
        throw InvalidSyntax();
    }

    LoadState(st);
}

void Parser::parseSEIPayload(BufferingPeriod& bp)
{
    TLAuto tl(*this, TRACE_SEI);

    BS2_SET(ue(), bp.seq_parameter_set_id);

    bp.sps = m_sps[bp.seq_parameter_set_id];
    if (!bp.sps)
        throw InvalidSyntax();
    bound(bp.sps, &bp);

    if (!bp.sps->vui_parameters_present_flag)
        return;

    if (bp.sps->vui->nal_hrd_parameters_present_flag && bp.sps->vui->nal_hrd)
    {
        HRD const & hrd = *bp.sps->vui->nal_hrd;
        Bs32u nBits = hrd.initial_cpb_removal_delay_length_minus1 + 1;

        bp.nal = alloc<BufferingPeriod::HRDBP>(&bp, hrd.cpb_cnt_minus1 + 1);

        for (Bs32u i = 0; i <= hrd.cpb_cnt_minus1; i++)
        {
            BS2_SET(u(nBits), bp.nal[i].initial_cpb_removal_delay);
            BS2_SET(u(nBits), bp.nal[i].initial_cpb_removal_delay_offset);
        }
    }

    if (bp.sps->vui->vcl_hrd_parameters_present_flag && bp.sps->vui->vcl_hrd)
    {
        HRD const & hrd = *bp.sps->vui->vcl_hrd;
        Bs32u nBits = hrd.initial_cpb_removal_delay_length_minus1 + 1;

        bp.vcl = alloc<BufferingPeriod::HRDBP>(&bp, hrd.cpb_cnt_minus1 + 1);

        for (Bs32u i = 0; i <= hrd.cpb_cnt_minus1; i++)
        {
            BS2_SET(u(nBits), bp.vcl[i].initial_cpb_removal_delay);
            BS2_SET(u(nBits), bp.vcl[i].initial_cpb_removal_delay_offset);
        }
    }
}

void Parser::parseSEIPayload(PicTiming& pt)
{
    TLAuto tl(*this, TRACE_SEI);
    HRD* pHRD = 0;

    if (!pt.sps->vui_parameters_present_flag)
        return;

    if (pt.sps->vui->nal_hrd_parameters_present_flag)
        pHRD = pt.sps->vui->nal_hrd;
    else if (pt.sps->vui->vcl_hrd_parameters_present_flag)
        pHRD = pt.sps->vui->vcl_hrd;

    if (pHRD)
    {
        BS2_SET(u(pHRD->cpb_removal_delay_length_minus1 + 1), pt.cpb_removal_delay);
        BS2_SET(u(pHRD->dpb_output_delay_length_minus1 + 1), pt.dpb_output_delay);
    }

    if (pt.sps->vui->pic_struct_present_flag)
    {
        const Bs8u ps2ncts[16] = {1,1,1,2,2,3,3,2,3,};

        BS2_SET(u(4), pt.pic_struct);
        BS2_SET(ps2ncts[pt.pic_struct], pt.NumClockTS);

        for (Bs32u i = 0; i < pt.NumClockTS; i++)
        {
            PicTiming::CTS& cts = pt.ClockTS[i];

            BS2_TRACE(i, cts);

            BS2_SET(u1(), cts.clock_timestamp_flag);

            if (cts.clock_timestamp_flag)
            {
                BS2_SET(u(2), cts.ct_type);
                BS2_SET(u1(), cts.nuit_field_based_flag);
                BS2_SET(u(5), cts.counting_type);
                BS2_SET(u1(), cts.full_timestamp_flag);
                BS2_SET(u1(), cts.discontinuity_flag);
                BS2_SET(u1(), cts.cnt_dropped_flag);
                BS2_SET(u8(), cts.n_frames);

                if (cts.full_timestamp_flag)
                {
                    BS2_SET(u(6), cts.seconds_value);
                    BS2_SET(u(6), cts.minutes_value);
                    BS2_SET(u(6), cts.hours_value);
                }
                else
                {
                    BS2_SET(u1(), cts.seconds_flag);

                    if (cts.seconds_flag)
                    {
                        BS2_SET(u(6), cts.seconds_value);
                        BS2_SET(u1(), cts.minutes_flag);

                        if (cts.minutes_flag)
                        {
                            BS2_SET(u(6), cts.minutes_value);
                            BS2_SET(u1(), cts.hours_flag);

                            if (cts.hours_flag)
                            {
                                BS2_SET(u(6), cts.hours_value);
                            }
                        }
                    }
                }

                if (pHRD && pHRD->time_offset_length)
                {
                    BS2_SET(si(pHRD->time_offset_length), cts.time_offset);
                }
            }
        }
    }
}

void Parser::parseSEIPayload(DRPMRep& rep)
{
    TLAuto tl(*this, TRACE_SEI);

    BS2_SET(u1(), rep.original_idr_flag);
    BS2_SET(ue(), rep.original_frame_num);

    if (!rep.sps->frame_mbs_only_flag)
    {
        BS2_SET(u1(), rep.original_field_pic_flag);

        if (rep.original_field_pic_flag)
            BS2_SET(u1(), rep.original_bottom_field_flag);
    }

    if (rep.original_idr_flag)
    {
        BS2_SET(u1(), rep.no_output_of_prior_pics_flag);
        BS2_SET(u1(), rep.long_term_reference_flag);
    }
    else
    {
        BS2_SET(u1(), rep.adaptive_ref_pic_marking_mode_flag);

        if (rep.adaptive_ref_pic_marking_mode_flag)
        {
            rep.drpm = alloc<DRPM>(&rep);
            parseDRPM(*rep.drpm);
        }
    }
}

void Parser::parseSliceHeader(NALU& nalu)
{
    TLAuto tl(*this, TRACE_SLICE);

    if (!nalu.slice)
        nalu.slice = alloc<Slice>(&nalu);

    Slice& s = *nalu.slice;
    bool isIDR = (nalu.nal_unit_type == SLICE_IDR);
    bool isREF = (nalu.nal_ref_idc != 0);
    bool isI = false, isP = false, isB = false, isSI = false, isSP = false;

    BS2_SET(ue(), s.first_mb_in_slice);
    BS2_SETM(ue(), s.slice_type, sliceTypeTraceMap);
    BS2_SET(ue(), s.pic_parameter_set_id);

    s.pps = m_pps[s.pic_parameter_set_id];
    if (!s.pps)
        throw InvalidSyntax();
    bound(s.pps, &s);

    switch (s.slice_type % 5)
    {
    case SLICE_P:  isP  = true; break;
    case SLICE_B:  isB  = true; break;
    case SLICE_I:  isI  = true; break;
    case SLICE_SP: isSP = true; break;
    case SLICE_SI: isSI = true; break;
    default: break;
    }

    SPS const & sps = *s.pps->sps;
    PPS const & pps = *s.pps;

    if (sps.separate_colour_plane_flag)
        BS2_SET(u(2), s.colour_plane_id);

    BS2_SET(u(sps.log2_max_frame_num_minus4 + 4), s.frame_num);

    if (!sps.frame_mbs_only_flag)
    {
        BS2_SET(u1(), s.field_pic_flag);

        if (s.field_pic_flag)
            BS2_SET(u1(), s.bottom_field_flag);
    }

    if (isIDR)
        BS2_SET(ue(), s.idr_pic_id);

    if (sps.pic_order_cnt_type == 0)
    {
        BS2_SET(u(sps.log2_max_pic_order_cnt_lsb_minus4 + 4), s.pic_order_cnt_lsb);

        if (pps.bottom_field_pic_order_in_frame_present_flag && !s.field_pic_flag)
            BS2_SET(se(), s.delta_pic_order_cnt_bottom);
    }

    if (sps.pic_order_cnt_type == 1 && !sps.delta_pic_order_always_zero_flag)
    {
        BS2_SET(se(), s.delta_pic_order_cnt[0]);

        if (pps.bottom_field_pic_order_in_frame_present_flag && !s.field_pic_flag)
            BS2_SET(se(), s.delta_pic_order_cnt[1]);
    }

    if (pps.redundant_pic_cnt_present_flag)
        BS2_SET(ue(), s.redundant_pic_cnt);

    if (isB)
        BS2_SET(u1(), s.direct_spatial_mv_pred_flag);

    if (isP || isSP || isB)
    {
        BS2_SET(u1(), s.num_ref_idx_active_override_flag);

        s.num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_default_active_minus1;
        s.num_ref_idx_l1_active_minus1 = (isB ? pps.num_ref_idx_l1_default_active_minus1 : 0);

        if (s.num_ref_idx_active_override_flag)
        {
            BS2_SET(ue(), s.num_ref_idx_l0_active_minus1);

            if (isB)
                BS2_SET(ue(), s.num_ref_idx_l1_active_minus1);
        }
    }

    if (!isI && !isSI)
    {
        BS2_SET(u1(), s.ref_pic_list_modification_flag_l0);

        if (s.ref_pic_list_modification_flag_l0)
        {
            s.rplm[0] = alloc<RefPicListMod>(&s);
            parseRPLM(*s.rplm[0]);
        }
    }

    if (isB)
    {
        BS2_SET(u1(), s.ref_pic_list_modification_flag_l1);

        if (s.ref_pic_list_modification_flag_l1)
        {
            s.rplm[1] = alloc<RefPicListMod>(&s);
            parseRPLM(*s.rplm[1]);
        }
    }

    if (   (pps.weighted_pred_flag && (isP || isSP))
        || (pps.weighted_bipred_idc == 1 && isB))
    {
        s.pwt = alloc<PredWeightTable>(&s);
        PredWeightTable& pwt = *s.pwt;
        Bs32u chroma_array_type = sps.separate_colour_plane_flag ? 0 : sps.chroma_format_idc;
        Bs32u LSz[2] = { 1u + s.num_ref_idx_l0_active_minus1, 1u + s.num_ref_idx_l1_active_minus1 };

        BS2_SET(ue(), pwt.luma_log2_weight_denom);

        if (chroma_array_type != 0)
            BS2_SET(ue(), pwt.chroma_log2_weight_denom);

        for (Bs32u x = 0; x < 1U + isB; x++)
        {
            pwt.ListX[x] = alloc<PredWeightTable::Entry>(&pwt, LSz[x]);
            PredWeightTable::Entry* pwtlx = pwt.ListX[x];

            BS2_TRACE(x, List);

            for (Bs32u i = 0; i < LSz[x]; i++)
            {
                BS2_SET(u1(), pwtlx[i].luma_weight_lx_flag);

                if (pwtlx[i].luma_weight_lx_flag)
                {
                    BS2_SET(se(), pwtlx[i].luma_weight_lx);
                    BS2_SET(se(), pwtlx[i].luma_offset_lx);
                }

                if (chroma_array_type != 0)
                {
                    BS2_SET(u1(), pwtlx[i].chroma_weight_lx_flag);

                    if (pwtlx[i].chroma_weight_lx_flag)
                    {
                        BS2_SET(se(), pwtlx[i].chroma_weight_lx[0]);
                        BS2_SET(se(), pwtlx[i].chroma_offset_lx[0]);
                        BS2_SET(se(), pwtlx[i].chroma_weight_lx[1]);
                        BS2_SET(se(), pwtlx[i].chroma_offset_lx[1]);
                    }
                }
            }
        }
    }

    if (isREF)
    {
        if (isIDR)
        {
            BS2_SET(u1(), s.no_output_of_prior_pics_flag);
            BS2_SET(u1(), s.long_term_reference_flag);
        }
        else
        {
            BS2_SET(u1(), s.adaptive_ref_pic_marking_mode_flag);

            if (s.adaptive_ref_pic_marking_mode_flag)
            {
                s.drpm = alloc<DRPM>(&s);
                parseDRPM(*s.drpm);
            }
        }
    }

    if (pps.entropy_coding_mode_flag && !isI && !isSI)
        BS2_SET(ue(), s.cabac_init_idc);

    BS2_SET(se(), s.slice_qp_delta);

    if (isSP || isSI)
    {
        if (isSP)
            BS2_SET(u1(), s.sp_for_switch_flag);

        BS2_SET(se(), s.slice_qs_delta);
    }

    if (pps.deblocking_filter_control_present_flag)
    {
        BS2_SET(ue(), s.disable_deblocking_filter_idc);

        if (s.disable_deblocking_filter_idc != 1)
        {
            BS2_SET(se(), s.slice_alpha_c0_offset_div2);
            BS2_SET(se(), s.slice_beta_offset_div2);
        }
    }

    if (   pps.num_slice_groups_minus1 > 0
        && pps.slice_group_map_type >= 3
        && pps.slice_group_map_type <= 5)
    {
        Bs32u PicSizeInMapUnits = (sps.pic_width_in_mbs_minus1 + 1) * (sps.pic_height_in_map_units_minus1 + 1);
        Bs32u nBits = CeilLog2(PicSizeInMapUnits / (pps.slice_group_change_rate_minus1 + 1) + 1);

        BS2_SET(u(nBits), s.slice_group_change_cycle);
    }
}

void Parser::parseRPLM(RefPicListMod& first)
{
    RefPicListMod* next = &first;

    for (;;)
    {
        RefPicListMod& rplm = *next;

        BS2_SETM(ue(), rplm.modification_of_pic_nums_idc, rplmTraceMap);

        switch (rplm.modification_of_pic_nums_idc)
        {
        case RPLM_ST_SUB:
        case RPLM_ST_ADD:
            BS2_SET(ue(), rplm.abs_diff_pic_num_minus1);
            break;
        case RPLM_LT:
            BS2_SET(ue(), rplm.long_term_pic_num);
            break;
        case RPLM_END:
            return;
        case RPLM_VIEW_IDX_SUB:
        case RPLM_VIEW_IDX_ADD:
            BS2_SET(ue(), rplm.abs_diff_view_idx_minus1);
            break;
        default:
            throw InvalidSyntax();
        }

        next = rplm.next = alloc<RefPicListMod>(&rplm);
    }
}

void Parser::parseDRPM(DRPM& drpm_first)
{
    DRPM* next = &drpm_first;

    for (;;)
    {
        DRPM& drpm = *next;

        BS2_SETM(ue(), drpm.memory_management_control_operation, mmcoTraceMap);

        switch (drpm.memory_management_control_operation)
        {
        case MMCO_END:
            return;
        case MMCO_REMOVE_ST:
            BS2_SET(ue(), drpm.difference_of_pic_nums_minus1);
            break;
        case MMCO_ST_TO_LT:
            BS2_SET(ue(), drpm.difference_of_pic_nums_minus1);
            BS2_SET(ue(), drpm.long_term_frame_idx);
            break;
        case MMCO_REMOVE_LT:
            BS2_SET(ue(), drpm.long_term_pic_num);
            break;
        case MMCO_CUR_TO_LT:
            BS2_SET(ue(), drpm.long_term_frame_idx);
            break;
        case MMCO_MAX_LT_IDX:
            BS2_SET(ue(), drpm.max_long_term_frame_idx_plus1);
            break;
        case MMCO_CLEAR_DPB:
            break;
        default:
            throw InvalidSyntax();
        }

        next = drpm.next = alloc<DRPM>(&drpm);
    }
}
