//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2012 Intel Corporation. All Rights Reserved.
//

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#ifdef FAKE_BITSTREAM
#define H264ENC_MAKE_NAME_BS(NAME) H264BsFake_##NAME##_8u16s
#define H264BsType H264BsFake_8u16s
#else // real bitstream
#define H264ENC_MAKE_NAME_BS(NAME) H264BsReal_##NAME##_8u16s
#define H264BsType H264BsReal_8u16s
#endif

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#ifdef FAKE_BITSTREAM
#define H264ENC_MAKE_NAME_BS(NAME) H264BsFake_##NAME##_16u32s
#define H264BsType H264BsFake_8u16s
#else // real bitstream
#define H264ENC_MAKE_NAME_BS(NAME) H264BsReal_##NAME##_16u32s
#define H264BsType H264BsReal_8u16s
#endif

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define H264SliceType H264ENC_MAKE_NAME(H264Slice)

// ---------------------------------------------------------------------------
//  CH264pBs::Reset()
//      reset bitstream; used in the encoder
// ---------------------------------------------------------------------------
#ifndef FAKE_BITSTREAM
void H264ENC_MAKE_NAME_BS(Reset)(
    void* state)
{
    H264BsType* bs = (H264BsType *)state;
    H264BsBase_Reset(&bs->m_base);
    bs->m_pbsRBSPBase = bs->m_base.m_pbsBase;
}
#endif //FAKE_BITSTREAM

//// ---------------------------------------------------------------------------
////  CH264pBs::ResetRBSP()
////      reset bitstream to beginning of current RBSP; used in the encoder
//// ---------------------------------------------------------------------------
//void H264ENC_MAKE_NAME_BS(ResetRBSP)(
//    void* state)
//{
//    H264BsType* bs = (H264BsType *)state;
//    bs->m_base.m_pbs = bs->m_pbsRBSPBase;
//    bs->m_base.m_bitOffset = 0;
//    //bs->m_base.m_pbs[0] = 0;   // Zero the first byte, since subsequent bits written will be OR'd
//    //                // with this byte.  Subsequent bytes will be completely overwritten
//    //                // or zeroed, so no need to clear them out.
//}

// ---------------------------------------------------------------------------
//  CH264pBs::EndOfNAL()
// ---------------------------------------------------------------------------
Ipp32u H264ENC_MAKE_NAME_BS(EndOfNAL)(
    void* state,
    Ipp8u* const pout,
    Ipp8u const uIDC,
    NAL_Unit_Type const uUnitType,
    bool& startPicture,
    Ipp8u nal_header_ext[3])
{
    H264BsType* bs = (H264BsType *)state;
    Ipp32u size, ExtraBytes;
    Ipp8u* curPtr, *endPtr, *outPtr;

    // get current RBSP compressed size
    size = (Ipp32u)(bs->m_base.m_pbs - bs->m_pbsRBSPBase);
    ExtraBytes = 0;

    // Set Pointers
    endPtr = bs->m_pbsRBSPBase + size - 1;  // Point at Last byte with data in it.
    curPtr = bs->m_pbsRBSPBase;
    outPtr = pout;

    // start access unit => should be zero_byte
    // for SPS and PPS NAL units zero_byte should exist
    if ((startPicture && (((uUnitType >= NAL_UT_SLICE ) && (uUnitType <= NAL_UT_AUD)) ||
           (uUnitType == NAL_UT_PREFIX) || (uUnitType == NAL_UT_CODED_SLICE_EXTENSION) || (uUnitType == NAL_UT_DEPENDENCY_DELIMETER) ||
           ((uUnitType >= 0x0e) && (uUnitType <= 0x12)))) ||
           (uUnitType == NAL_UT_SPS || uUnitType == NAL_UT_PPS || uUnitType == NAL_UT_SUBSET_SPS)) {
        *outPtr++ = 0;
        ExtraBytes = 1;
        startPicture = false;
    }

    *outPtr++ = 0;
    *outPtr++ = 0;
    *outPtr++ = 1;
    *outPtr++ = (Ipp8u) ((uIDC << 5) | uUnitType);  //nal_unit_type
    ExtraBytes += 4;

    if( uUnitType == NAL_UT_PREFIX || uUnitType == NAL_UT_CODED_SLICE_EXTENSION ) {
        *outPtr++ = nal_header_ext[0];
        *outPtr++ = nal_header_ext[1];
        *outPtr++ = nal_header_ext[2];
        ExtraBytes += 3;
    }

    /* Special hack for SEI scalable */
    if ((uUnitType == NAL_UT_SEI) && (*curPtr == SEI_TYPE_SCALABILITY_INFO)) {
        Ipp32u _size = size - 1;

        curPtr++;
        size--;

        *outPtr++ = SEI_TYPE_SCALABILITY_INFO;
        ExtraBytes++;

        while(_size > 255) {
            *outPtr++ = 255;
            _size -= 255;
            ExtraBytes++;
        }
        *outPtr++ = (Ipp8u)_size;
        ExtraBytes++;
    }

    if( size == 0 ) return ExtraBytes;

    while (curPtr < endPtr-1) { // Copy all but the last 2 bytes
        *outPtr++ = *curPtr;

        // Check for start code emulation
        if ((*curPtr++ == 0) && (*curPtr == 0) && (!(*(curPtr+1) & 0xfc))) {
            *outPtr++ = *curPtr++;
            *outPtr++ = 0x03;   // Emulation Prevention Byte
            ExtraBytes++;
        }
    }

    if (curPtr < endPtr) *outPtr++ = *curPtr++;
    // copy the last byte
    *outPtr = *curPtr;

    // Update RBSP Base Pointer
    bs->m_pbsRBSPBase = bs->m_base.m_pbs;

    // copy encoded frame to output
    return(size+ExtraBytes);

}

Status H264ENC_MAKE_NAME_BS(PutSeqExParms)(
    void* state,
    const H264SeqParamSet& seq_parms)
{
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.seq_parameter_set_id);
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.aux_format_idc);
    if(seq_parms.aux_format_idc != 0) {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.bit_depth_aux - 8);
        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.alpha_incr_flag);
        H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.alpha_opaque_value, seq_parms.bit_depth_aux + 1);
        H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.alpha_transparent_value, seq_parms.bit_depth_aux + 1);
    }
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.additional_extension_flag);
    return(UMC_OK);
}

void H264ENC_MAKE_NAME_BS(PutHRDParms)(
    void *state,
    const  H264VUI_HRDParams& hrd_params)
{
    Ipp32s i;

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, hrd_params.cpb_cnt_minus1);
    H264ENC_MAKE_NAME_BS(PutBits)(state, hrd_params.bit_rate_scale, 4);
    H264ENC_MAKE_NAME_BS(PutBits)(state, hrd_params.cpb_size_scale, 4);
    for (i=0; i <= hrd_params.cpb_cnt_minus1; i++) {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, hrd_params.bit_rate_value_minus1[i]);
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, hrd_params.cpb_size_value_minus1[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, hrd_params.cbr_flag[i]);
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, hrd_params.initial_cpb_removal_delay_length_minus1, 5);
    H264ENC_MAKE_NAME_BS(PutBits)(state, hrd_params.cpb_removal_delay_length_minus1, 5);
    H264ENC_MAKE_NAME_BS(PutBits)(state, hrd_params.dpb_output_delay_length_minus1, 5);
    H264ENC_MAKE_NAME_BS(PutBits)(state, hrd_params.time_offset_length, 5);
}

// ---------------------------------------------------------------------------
//  CH264pBs::PutSeqParms()
// ---------------------------------------------------------------------------
Status H264ENC_MAKE_NAME_BS(PutSeqParms)(
    void* state,
    const H264SeqParamSet& seq_parms)
{
    Status ps = UMC_OK;

    // Write profile and level information

    H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.profile_idc, 8);

    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.constraint_set0_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.constraint_set1_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.constraint_set2_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.constraint_set3_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.constraint_set4_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.constraint_set5_flag);

    // 2 reserved zero bits
    H264ENC_MAKE_NAME_BS(PutBits)(state, 0, 2);

    H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.level_idc, 8);

    // Write the sequence parameter set id
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.seq_parameter_set_id);

    if(seq_parms.profile_idc == H264_HIGH_PROFILE ||
        seq_parms.profile_idc == H264_STEREOHIGH_PROFILE ||
        seq_parms.profile_idc == H264_MULTIVIEWHIGH_PROFILE ||
        seq_parms.profile_idc == H264_HIGH10_PROFILE ||
        seq_parms.profile_idc == H264_HIGH422_PROFILE ||
        seq_parms.profile_idc == H264_HIGH444_PROFILE ||
        seq_parms.profile_idc == H264_SCALABLE_BASELINE_PROFILE ||
        seq_parms.profile_idc == H264_SCALABLE_HIGH_PROFILE)
    {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.chroma_format_idc);
        if(seq_parms.chroma_format_idc == 3) {
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.residual_colour_transform_flag);
        }
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.bit_depth_luma - 8);
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.bit_depth_chroma - 8);
        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.qpprime_y_zero_transform_bypass_flag);
        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.seq_scaling_matrix_present_flag);
        if(seq_parms.seq_scaling_matrix_present_flag) {
            Ipp32s i;
            bool UseDefaultScalingMatrix;
            for( i=0; i<8 ; i++){
                //Put scaling list present flag
                H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.seq_scaling_list_present_flag[i]);
                if( seq_parms.seq_scaling_list_present_flag[i] ){
                   if( i<6 )
                      H264ENC_MAKE_NAME_BS(PutScalingList)(state, &seq_parms.seq_scaling_list_4x4[i][0], 16, UseDefaultScalingMatrix);
                   else
                      H264ENC_MAKE_NAME_BS(PutScalingList)(state, &seq_parms.seq_scaling_list_8x8[i-6][0], 64, UseDefaultScalingMatrix);
                }
            }
        }
    }

    // Write log2_max_frame_num_minus4
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.log2_max_frame_num - 4);

    // Write pic_order_cnt_type and associated data
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.pic_order_cnt_type);

    // Write data specific to various pic order cnt types

    // pic_order_cnt_type == 1 is NOT currently supported
    if (seq_parms.pic_order_cnt_type == 0) {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.log2_max_pic_order_cnt_lsb - 4);
    }

    // Write num_ref_frames
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.num_ref_frames);

    // Write required_frame_num_update_behaviour_flag
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.gaps_in_frame_num_value_allowed_flag);

    // Write picture MB dimensions
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.frame_width_in_mbs - 1);
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.frame_height_in_mbs - 1);

    // Write other misc flags
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.frame_mbs_only_flag);

    if (!seq_parms.frame_mbs_only_flag) {
        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.mb_adaptive_frame_field_flag);
    }

    // Right now, the decoder only supports this flag with
    // a value of zero.
    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.direct_8x8_inference_flag);

    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.frame_cropping_flag);

    if (seq_parms.frame_cropping_flag)  {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.frame_crop_left_offset);
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.frame_crop_right_offset);
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.frame_crop_top_offset);
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.frame_crop_bottom_offset);
    }

    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.vui_parameters_present_flag);

    if (seq_parms.vui_parameters_present_flag) {

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.aspect_ratio_info_present_flag );
        if( seq_parms.vui_parameters.aspect_ratio_info_present_flag ){
            H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.aspect_ratio_idc, 8);
            if( seq_parms.vui_parameters.aspect_ratio_idc == 255 ){ // == Extended_SAR
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.sar_width,16);
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.sar_height,16);
            }
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.overscan_info_present_flag );
        if(seq_parms.vui_parameters.overscan_info_present_flag){
            H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.overscan_appropriate_flag );
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.video_signal_type_present_flag );
        if(seq_parms.vui_parameters.video_signal_type_present_flag){
            H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.video_format,3);
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.vui_parameters.video_full_range_flag);
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.vui_parameters.colour_description_present_flag);
            if(seq_parms.vui_parameters.colour_description_present_flag){
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.colour_primaries,8);
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.transfer_characteristics,8);
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.matrix_coefficients,8);
            }
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.chroma_loc_info_present_flag );
        if(seq_parms.vui_parameters.chroma_loc_info_present_flag){
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.chroma_sample_loc_type_top_field);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.chroma_sample_loc_type_bottom_field);
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.timing_info_present_flag );
        if(seq_parms.vui_parameters.timing_info_present_flag){
            H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.num_units_in_tick>>24, 8);
            H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.num_units_in_tick&0x00ffffff, 24); //Due to restrictions of BsTypeType::PutBits
            H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.time_scale>>24, 8);
            H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.vui_parameters.time_scale&0x00ffffff, 24);
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.vui_parameters.fixed_frame_rate_flag);
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.nal_hrd_parameters_present_flag );
        if(seq_parms.vui_parameters.nal_hrd_parameters_present_flag){
            H264ENC_MAKE_NAME_BS(PutHRDParms)(state, seq_parms.vui_parameters.hrd_params);
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.vcl_hrd_parameters_present_flag );
        if(seq_parms.vui_parameters.vcl_hrd_parameters_present_flag){
            H264ENC_MAKE_NAME_BS(PutHRDParms)(state, seq_parms.vui_parameters.vcl_hrd_params);
        }

        if( seq_parms.vui_parameters.nal_hrd_parameters_present_flag ||
            seq_parms.vui_parameters.vcl_hrd_parameters_present_flag){
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.vui_parameters.low_delay_hrd_flag);
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.pic_struct_present_flag );
        H264ENC_MAKE_NAME_BS(PutBit)(state,  seq_parms.vui_parameters.bitstream_restriction_flag );
        if(seq_parms.vui_parameters.bitstream_restriction_flag){
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.vui_parameters.motion_vectors_over_pic_boundaries_flag);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.max_bytes_per_pic_denom);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.max_bits_per_mb_denom);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.log2_max_mv_length_horizontal);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.log2_max_mv_length_vertical);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.num_reorder_frames);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_parameters.max_dec_frame_buffering);
        }
    }

    if (seq_parms.profile_idc == H264_SCALABLE_BASELINE_PROFILE ||
        seq_parms.profile_idc == H264_SCALABLE_HIGH_PROFILE) {

        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.inter_layer_deblocking_filter_control_present_flag);
        H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.extended_spatial_scalability, 2);

        if (seq_parms.chroma_format_idc == 1 || seq_parms.chroma_format_idc == 2) {
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.chroma_phase_x_plus1);
        }

        if (seq_parms.chroma_format_idc == 1) {
            H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.chroma_phase_y_plus1, 2);
        }

        if (seq_parms.extended_spatial_scalability == 1) {
            if (seq_parms.chroma_format_idc > 0) {
                H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.seq_ref_layer_chroma_phase_x_plus1);
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.seq_ref_layer_chroma_phase_y_plus1, 2);
            }

            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(seq_parms.seq_scaled_ref_layer_left_offset/2));
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(seq_parms.seq_scaled_ref_layer_top_offset/2));
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(seq_parms.seq_scaled_ref_layer_right_offset/2));
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(seq_parms.seq_scaled_ref_layer_bottom_offset/2));
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.seq_tcoeff_level_prediction_flag);

        if (seq_parms.seq_tcoeff_level_prediction_flag) {
            H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.adaptive_tcoeff_level_prediction_flag);
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.slice_header_restriction_flag);

        H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.svc_vui_parameters_present_flag);
        if (seq_parms.svc_vui_parameters_present_flag) {
            Ipp32s i = 0;
            if ((seq_parms.svc_vui_parameters[0].vui_ext_dependency_id | seq_parms.svc_vui_parameters[0].vui_ext_quality_id | seq_parms.svc_vui_parameters[0].vui_ext_temporal_id) == 0)
                i = 1; // already have (0,0,0) vui in the "regular" AVC VUI
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.vui_ext_num_entries-1-i);
            for (; i < seq_parms.vui_ext_num_entries; i++) {
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.svc_vui_parameters[i].vui_ext_dependency_id, 3);
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.svc_vui_parameters[i].vui_ext_quality_id, 4);
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.svc_vui_parameters[i].vui_ext_temporal_id, 3);
                H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.svc_vui_parameters[i].vui_ext_timing_info_present_flag);
                if (seq_parms.svc_vui_parameters[i].vui_ext_timing_info_present_flag) {
                    H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.svc_vui_parameters[i].vui_ext_num_units_in_tick >> 24, 8);
                    H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.svc_vui_parameters[i].vui_ext_num_units_in_tick & 0x00ffffff, 24); //Due to restrictions of BsTypeType::PutBits
                    H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.svc_vui_parameters[i].vui_ext_time_scale >> 24, 8);
                    H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.svc_vui_parameters[i].vui_ext_time_scale & 0x00ffffff, 24);
                    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.svc_vui_parameters[i].vui_ext_fixed_frame_rate_flag);
                }
                H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.svc_vui_parameters[i].vui_ext_nal_hrd_parameters_present_flag);
                if (seq_parms.svc_vui_parameters[i].vui_ext_nal_hrd_parameters_present_flag) {
                    H264ENC_MAKE_NAME_BS(PutHRDParms)(state, seq_parms.svc_vui_parameters[i].nal_hrd_parameters);
                }
                H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.svc_vui_parameters[i].vui_ext_vcl_hrd_parameters_present_flag);
                if (seq_parms.svc_vui_parameters[i].vui_ext_vcl_hrd_parameters_present_flag) {
                    H264ENC_MAKE_NAME_BS(PutHRDParms)(state, seq_parms.svc_vui_parameters[i].vcl_hrd_parameters);
                }
                if (seq_parms.svc_vui_parameters[i].vui_ext_nal_hrd_parameters_present_flag | seq_parms.svc_vui_parameters[i].vui_ext_vcl_hrd_parameters_present_flag) {
                    H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.svc_vui_parameters[i].vui_ext_low_delay_hrd_flag);
                }
                H264ENC_MAKE_NAME_BS(PutBit)(state, seq_parms.svc_vui_parameters[i].vui_ext_pic_struct_present_flag);
            }
        }

        H264ENC_MAKE_NAME_BS(PutBit)(state, 0); // additional_extension2_data_flag
    }
    if( seq_parms.profile_idc==H264_MULTIVIEWHIGH_PROFILE || seq_parms.profile_idc==H264_STEREOHIGH_PROFILE ) {
        Ipp32u i;

        H264ENC_MAKE_NAME_BS(PutBit)(state, 1); // bit_equal_to_one
        for(i=0; i<seq_parms.mvc_extension_size; i++){
            if(seq_parms.mvc_extension_payload[i]>>24) {
                H264ENC_MAKE_NAME_BS(PutBits)(state, seq_parms.mvc_extension_payload[i]&0x00ffffff,seq_parms.mvc_extension_payload[i]>>24);
            } else {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parms.mvc_extension_payload[i]);
            }
        }
        H264ENC_MAKE_NAME_BS(PutBit)(state, 0); // mvc_vui_parameters_present_flag == 0
        H264ENC_MAKE_NAME_BS(PutBit)(state, 0); // additional_extension2_flag == 0
    }

    return ps;
}


// ---------------------------------------------------------------------------
//  CH264pBs::PutPicParms()
// ---------------------------------------------------------------------------
Status H264ENC_MAKE_NAME_BS(PutPicParms)(
    void* state,
    const H264PicParamSet& pic_parms,
    const H264SeqParamSet& seq_parms)
{
    Status ps = UMC_OK;

    // Write IDs
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pic_parms.pic_parameter_set_id);

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pic_parms.seq_parameter_set_id);

    // Write Entropy coding mode
    H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.entropy_coding_mode);
    H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.pic_order_present_flag);

    // Only one slice group is currently supported
    // Write num_slice_groups_minus1
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pic_parms.num_slice_groups - 1);

    // If multiple slice groups are ever supported, then add code here
    // to write the slice group map information needed to allocate MBs
    // to the defined slice groups.

    // Write num_ref_idx_active counters
    // Right now these are limited to one frame each...
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pic_parms.num_ref_idx_l0_active - 1);

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pic_parms.num_ref_idx_l1_active - 1);

    // Write some various flags

    // Weighted pred for P slices is not supported
    H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.weighted_pred_flag);

    // Explicit weighted BiPred not supported
    // So 0 or 2 are the acceptable values
    H264ENC_MAKE_NAME_BS(PutBits)(state, pic_parms.weighted_bipred_idc, 2);

    // Write quantization values
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(pic_parms.pic_init_qp - 26));

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(pic_parms.pic_init_qs - 26));

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(pic_parms.chroma_qp_index_offset));

    // Write some more flags
    H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.deblocking_filter_variables_present_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.constrained_intra_pred_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.redundant_pic_cnt_present_flag);
    if(seq_parms.profile_idc == H264_HIGH_PROFILE ||
       seq_parms.profile_idc == H264_STEREOHIGH_PROFILE ||
       seq_parms.profile_idc == H264_MULTIVIEWHIGH_PROFILE ||
       seq_parms.profile_idc == H264_HIGH10_PROFILE ||
       seq_parms.profile_idc == H264_HIGH422_PROFILE ||
       seq_parms.profile_idc == H264_HIGH444_PROFILE ||
       seq_parms.profile_idc == H264_SCALABLE_BASELINE_PROFILE ||
       seq_parms.profile_idc == H264_SCALABLE_HIGH_PROFILE) {
        H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.transform_8x8_mode_flag);
        H264ENC_MAKE_NAME_BS(PutBit)(state, pic_parms.pic_scaling_matrix_present_flag);
        if(pic_parms.pic_scaling_matrix_present_flag) {
            VM_ASSERT(0 /* scaling matrices coding is not supported */);
            // TO DO: add scaling matrices coding.
        }
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(pic_parms.second_chroma_qp_index_offset));
    }

    return ps;
}


// ---------------------------------------------------------------------------
//  CH264pBs::PutPicDelimiter()
// ---------------------------------------------------------------------------
Status H264ENC_MAKE_NAME_BS(PutPicDelimiter)(
    void* state,
    EnumPicCodType PicCodType)
{
    Status ps = UMC_OK;

    // Write pic_type
    H264ENC_MAKE_NAME_BS(PutBits)(state, PicCodType, 3);

    return ps;
} // CH264pBs::PutPicDelimiter()

// ---------------------------------------------------------------------------
//  CH264pBs::PutDQUANT()
// ---------------------------------------------------------------------------
void H264ENC_MAKE_NAME_BS(PutDQUANT)(
    void* state,
    const Ipp32u quant,
    const Ipp32u quant_prev)
{
    Ipp32s dquant;

    // compute dquant
    dquant=quant-quant_prev;


    // Get dquant between (QP_RANGE-1) to (-QP_RANGE)  (25 to -26 for JVT)

    if (dquant >= H264_QP_RANGE)
        dquant = dquant - H264_QP_MAX;
    else if (dquant < -H264_QP_RANGE)
        dquant = dquant + H264_QP_MAX;

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(dquant));
}

void H264ENC_MAKE_NAME_BS(PutDecRefPicMarking)(
    void* state,
    const H264SliceHeader& slice_hdr,
    const EnumPicClass& ePictureClass,
    const DecRefPicMarkingInfo& decRefPicMarkingInfo)
{
     // Write appropriate bits for dec_ref_pic_marking()
    // Note!  Currently there are no structure members for these syntax elements,
    // so the default bits are just written directly.  This need to be updated
    // when appropriate structures are implemented.  TODO - VSI
    if (ePictureClass == IDR_PIC) {
        // no_output_of_prior_pics_flag
        H264ENC_MAKE_NAME_BS(PutBit)(state, decRefPicMarkingInfo.no_output_of_prior_pics_flag); // no_output_of_prior_pics_flag is always 0 in current implementation
        // long_term_reference_flag
        H264ENC_MAKE_NAME_BS(PutBit)(state, decRefPicMarkingInfo.long_term_reference_flag); // long_term_reference_flag is always 0 in current implementation
    } else if (ePictureClass == REFERENCE_PIC) {
        // adaptive_ref_pic_marking_mode_flag
        H264ENC_MAKE_NAME_BS(PutBit)(state, slice_hdr.adaptive_ref_pic_marking_mode_flag);
        if (slice_hdr.adaptive_ref_pic_marking_mode_flag)
        {
            // write mamory_management_control_operation
            for (Ipp32u i = 0; i < decRefPicMarkingInfo.num_entries; i ++){
                switch (decRefPicMarkingInfo.mmco[i]) {
                    case 1:
                        // mark a short-term picture as unused for reference
                        // Value is difference_of_pic_nums_minus1
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 1);
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, decRefPicMarkingInfo.value[i*2]);
                        break;
                    case 2:
                        // mark a long-term picture as unused for reference
                        // value is long_term_pic_num
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 2);
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, decRefPicMarkingInfo.value[i*2]);
                        break;
                    case 3:
                        // Assign a long-term frame idx to a short-term picture
                        // Value is difference_of_pic_nums_minus1 followed by
                        // long_term_frame_idx. Only this case uses 2 value entries.
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 3);
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, decRefPicMarkingInfo.value[i*2]);
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, decRefPicMarkingInfo.value[i*2 + 1]);
                        break;
                    case 4:
                        // Specify IPP_MAX long term frame idx
                        // Value is max_long_term_frame_idx_plus1
                        // Set to "no long-term frame indices" (-1) when value == 0.
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 4);
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, decRefPicMarkingInfo.value[i*2]);
                        break;
                    case 5:
                        // Mark all as unused for reference
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 5);
                        break;
                    case 6:
                        // Assign long term frame idx to current picture
                        // Value is long_term_frame_idx
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 6);
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, decRefPicMarkingInfo.value[i*2]);
                        break;
                    default:
                        VM_ASSERT(0);
                }
            }
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 0);
        }
    }
}

// ---------------------------------------------------------------------------
//  CH264pBs::PutSliceHeader()
// ---------------------------------------------------------------------------
Status H264ENC_MAKE_NAME_BS(PutSliceHeader)(
    void* state,
    const H264SliceHeader& slice_hdr,
    const H264PicParamSet& pic_parms,
    const H264SeqParamSet& seq_parms,
    const EnumPicClass& ePictureClass,
    const H264SliceType *curr_slice,
    const DecRefPicMarkingInfo& decRefPicMarkingInfo,
    const RefPicListReorderInfo& reorderInfoL0,
    const RefPicListReorderInfo& reorderInfoL1,
    const sNALUnitHeaderSVCExtension *svc_header)
{
    Ipp32u i;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    Status ps = UMC_OK;

    // Write first_mb_in_slice
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->m_first_mb_in_slice);

    // Write slice_type_idc
    if (seq_parms.profile_idc == H264_SCALABLE_BASELINE_PROFILE ||
        seq_parms.profile_idc == H264_SCALABLE_HIGH_PROFILE)
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, slice_type<5 && !slice_hdr.pic_parameter_set_id ? slice_type+5 : slice_type); // for SVC only
    else
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, slice_type<5 ? slice_type+5 : slice_type); // for AVC and MVC

    // Write pic_parameter_set_id
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, slice_hdr.pic_parameter_set_id);

    // Write frame_num (modulo MAX_FN) using log2_max_frame_num bits
    H264ENC_MAKE_NAME_BS(PutBits)(state, slice_hdr.frame_num, seq_parms.log2_max_frame_num);

    // Only write field values if not frame only...
    if (!seq_parms.frame_mbs_only_flag) {
        // Write field_pic_flag
        H264ENC_MAKE_NAME_BS(PutBit)(state, slice_hdr.field_pic_flag);

        // Write bottom_field_flag
        if (slice_hdr.field_pic_flag == 1) {
            H264ENC_MAKE_NAME_BS(PutBit)(state, slice_hdr.bottom_field_flag);
        }
    }

    if (ePictureClass == IDR_PIC) {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, slice_hdr.idr_pic_id);
    }

    // Write pic_order_cnt info
    if (seq_parms.pic_order_cnt_type == 0) {
        H264ENC_MAKE_NAME_BS(PutBits)(state, slice_hdr.pic_order_cnt_lsb, seq_parms.log2_max_pic_order_cnt_lsb);
        if ((pic_parms.pic_order_present_flag == 1) && (!slice_hdr.field_pic_flag)) {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(slice_hdr.delta_pic_order_cnt_bottom));
        }
    }

    // Handle Redundant Slice Flag
    if (pic_parms.redundant_pic_cnt_present_flag) {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, slice_hdr.redundant_pic_cnt);
    }

    if (svc_header->quality_id == 0) {
        // Write direct_spatial_mv_pred_flag if this is a BPREDSLICE
        if (slice_type == BPREDSLICE) {
            H264ENC_MAKE_NAME_BS(PutBit)(state, slice_hdr.direct_spatial_mv_pred_flag);
        }

        if ((slice_type == BPREDSLICE) ||
            (slice_type == S_PREDSLICE) ||
            (slice_type == PREDSLICE)) {
            // Write num_ref_idx_active_override_flag
            H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->num_ref_idx_active_override_flag);
            if (curr_slice->num_ref_idx_active_override_flag) {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->num_ref_idx_l0_active - 1);
                if (slice_type == BPREDSLICE) {
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->num_ref_idx_l1_active - 1);
                }
            }
        }

        // write ref_pic_list_reordering()
        if ((slice_type != INTRASLICE) &&
            (slice_type != S_INTRASLICE)) {

            // reordering for list L0
            H264ENC_MAKE_NAME_BS(PutBit)(state, slice_hdr.ref_pic_list_reordering_flag_l0); // ref_pic_list_reordering_flag_l0
            if (reorderInfoL0.num_entries && slice_hdr.ref_pic_list_reordering_flag_l0) { // write reordering syntax
                for (i = 0; i < reorderInfoL0.num_entries; i ++) {
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, reorderInfoL0.reordering_of_pic_nums_idc[i]); // reordering_of_pic_nums_idc
                    if (reorderInfoL0.reordering_of_pic_nums_idc[i] < 2)
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, reorderInfoL0.reorder_value[i] - 1); // abs_diff_pic_num_minus1
                    else
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, reorderInfoL0.reorder_value[i]); // long_term_pic_num
                }
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 3); // reordering_of_pic_nums_idc = 3
            }

            if (slice_type == BPREDSLICE) {
                // reordering for list L1
                H264ENC_MAKE_NAME_BS(PutBit)(state, slice_hdr.ref_pic_list_reordering_flag_l1); // ref_pic_list_reordering_flag_l1
                if (reorderInfoL1.num_entries && slice_hdr.ref_pic_list_reordering_flag_l1) { // write reordering syntax
                    for (i = 0; i < reorderInfoL1.num_entries; i ++) {
                        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, reorderInfoL1.reordering_of_pic_nums_idc[i]); // reordering_of_pic_nums_idc
                        if (reorderInfoL1.reordering_of_pic_nums_idc[i] < 2)
                            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, reorderInfoL1.reorder_value[i] - 1); // abs_diff_pic_num_minus1
                        else
                            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, reorderInfoL1.reorder_value[i]); // long_term_pic_num
                    }
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, 3); // reordering_of_pic_nums_idc = 3
                }
            }
        }

        if ((pic_parms.weighted_pred_flag &&
            (slice_type == PREDSLICE || slice_type == S_PREDSLICE)) ||
            ((pic_parms.weighted_bipred_idc == 1) && slice_type == BPREDSLICE)) {
            // Add support for pred_weight_table() ???
        }

        // Write appropriate bits for dec_ref_pic_marking()
        // Note!  Currently there are no structure members for these syntax elements,
        // so the default bits are just written directly.  This need to be updated
        // when appropriate structures are implemented.  TODO - VSI

        if (ePictureClass != DISPOSABLE_PIC) {
            H264ENC_MAKE_NAME_BS(PutDecRefPicMarking)(state, slice_hdr, ePictureClass, decRefPicMarkingInfo);
            /* Scalable NAL unit */
            if ((svc_header->dependency_id != 0) || (svc_header->quality_id != 0)) {
                if (!seq_parms.slice_header_restriction_flag)
                {
                    H264ENC_MAKE_NAME_BS(PutBit)(state, svc_header->store_ref_base_pic_flag);
                    if ((svc_header->use_ref_base_pic_flag ||
                        svc_header->store_ref_base_pic_flag) &&
                        (ePictureClass != IDR_PIC))
                    {
                        /* To Do Base DecPicMarking */
                        H264ENC_MAKE_NAME_BS(PutBits)(state, svc_header->adaptive_ref_base_pic_marking_mode_flag, 1);
                    }
                }
            }
        }
    }

    if (pic_parms.entropy_coding_mode && slice_type != INTRASLICE &&
            slice_type != S_INTRASLICE) {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->m_cabac_init_idc);
    }

    // Write slice_qp_delta
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_slice_qp_delta));

    // No sp_for_switch_flag or slice_qp_s_delta are written because we don't
    // support SI and SP slices
    if ((slice_type == S_PREDSLICE) || (slice_type == S_INTRASLICE)) {
        if (slice_type == S_PREDSLICE)
            H264ENC_MAKE_NAME_BS(PutBit)(state, slice_hdr.sp_for_switch_flag);
        // Write slice_qp_s_delta
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(slice_hdr.slice_qs_delta));
    }

    // Write Filter Parameters
    if (pic_parms.deblocking_filter_variables_present_flag) {

        // Write disable_deblocking_filter_idc
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->m_disable_deblocking_filter_idc);

        if (curr_slice->m_disable_deblocking_filter_idc != DEBLOCK_FILTER_OFF) {

            // Write slice_alpha_c0_offset_div2
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_slice_alpha_c0_offset>>1));

            // Write slice_beta_offset_div2
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_slice_beta_offset>>1));

        } else {    // If the filter is disabled, both offsets are -51
        }

    } else {    //  If no parms are written, then the filter is ON
    }

    if ((pic_parms.num_slice_groups > 1) && (svc_header->quality_id == 0)) {
        // Write slice_group_change_cycle
    }

    /* Scalable NAL unit */
    if ((svc_header->dependency_id != 0) || (svc_header->quality_id != 0)) {
        if (!svc_header->no_inter_layer_pred_flag && (svc_header->quality_id == 0)) {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->m_ref_layer_dq_id);

            if (seq_parms.inter_layer_deblocking_filter_control_present_flag) {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->m_disable_inter_layer_deblocking_filter_idc);

                if (curr_slice->m_disable_inter_layer_deblocking_filter_idc != DEBLOCK_FILTER_OFF) {
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_inter_layer_slice_alpha_c0_offset_div2));
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_inter_layer_slice_beta_offset_div2));
                }
            }

            H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_constrained_intra_resampling_flag);

            if (seq_parms.extended_spatial_scalability == 2) {
                //            if (sps->chromaArrayType > 0)
                {
                    H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_ref_layer_chroma_phase_x_plus1);
                    H264ENC_MAKE_NAME_BS(PutBits)(state, curr_slice->m_ref_layer_chroma_phase_y_plus1, 2);
                }

                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_scaled_ref_layer_left_offset));
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_scaled_ref_layer_top_offset));
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_scaled_ref_layer_right_offset));
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(curr_slice->m_scaled_ref_layer_bottom_offset));
            }
        }

        if (!svc_header->no_inter_layer_pred_flag) {
            H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_slice_skip_flag);

            if (curr_slice->m_slice_skip_flag) {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, curr_slice->m_num_mbs_in_slice_minus1);
            } else {
                H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_adaptive_prediction_flag);

                if (!curr_slice->m_adaptive_prediction_flag) {
                    H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_default_base_mode_flag);
                }

                if (!curr_slice->m_default_base_mode_flag || curr_slice->m_adaptive_prediction_flag) {
                    H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_adaptive_motion_prediction_flag);

                    if (!curr_slice->m_adaptive_motion_prediction_flag) {
                        H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_default_motion_prediction_flag);
                    }
                }

                H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_adaptive_residual_prediction_flag);

                if (!curr_slice->m_adaptive_residual_prediction_flag) {
                    H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_default_residual_prediction_flag);
                }
            }

            if (seq_parms.adaptive_tcoeff_level_prediction_flag == 1) {
                H264ENC_MAKE_NAME_BS(PutBit)(state, curr_slice->m_tcoeff_level_prediction_flag);
            }
        }

        if (!seq_parms.slice_header_restriction_flag) {
            H264ENC_MAKE_NAME_BS(PutBits)(state, curr_slice->m_scan_idx_start, 4);
            H264ENC_MAKE_NAME_BS(PutBits)(state, curr_slice->m_scan_idx_end, 4);
        }
    }

    return ps;

}

Status H264ENC_MAKE_NAME_BS(PutNumCoeffAndTrailingOnes)(
    void* state,
    Ipp32u N,                   // N, obtained from num coeffs of above/left blocks
    Ipp32s bChromaDC,           // True if this is a Chroma DC coeff block (2x2)
    Ipp32u uNumCoeff,           // Number of non-trailing one coeffs to follow (0-4 or 0-16)
    Ipp32u uNumTrailingOnes,    // Number of trailing ones (0-3)
    Ipp32u TrOneSigns,
    Ipp32s maxNumCoeffs)          // Signs of the trailing ones, packed into 3 LSBs, (1==neg)
{
    Ipp32s uVLCSelect = 0;
    Status ps = UMC_OK;
    Ipp32u uNumTrailingOnesSaved = uNumTrailingOnes;

    if (bChromaDC) {
        switch( bChromaDC ){
            case 1:
                uVLCSelect = 3; //nC = -1
                break;
            case 2:
                uVLCSelect = 4; //nC = -2
                break;
            case 3:
                uVLCSelect = 0; //nC = 0
                break;
        }
    } else {

        if (N < 2) {
            uVLCSelect = 0; // 0<=nC<2
        } else if (N < 4) {
            uVLCSelect = 1; // 2<=nC<4
        } else if (N < 8) {
            uVLCSelect = 2; // 4<=nC<8
        } else {  // N > 7  // 8<=nC
            uVLCSelect = 5;    // escape to Fixed Length code
        }

    }

    if (maxNumCoeffs < 16)
    {
        Ipp8u *tmpTab = H264TotalCoeffTrailingOnesTab[uVLCSelect];
        Ipp8u CurrentCoeffTokenIdx =
            H264CoeffTokenIdxTab[uVLCSelect][uNumCoeff * 4 + uNumTrailingOnes];
        Ipp8u targetCoeffTokenIdx = 0;
        Ipp32u minNumCoeff = maxNumCoeffs + 1;
        Ipp32u minNumTrailingOnes = maxNumCoeffs + 1;
        Ipp8u i;

        if (minNumTrailingOnes > 4)
            minNumTrailingOnes = 4;

        if (minNumCoeff > 17)
            minNumCoeff  = 17;

        for (i = 0; i < CurrentCoeffTokenIdx; i++)
        {
            uNumCoeff = tmpTab[i] >> 2;
            uNumTrailingOnes = tmpTab[i] & 3;

            if ((uNumCoeff < minNumCoeff) && (uNumTrailingOnes < minNumTrailingOnes))
                targetCoeffTokenIdx++;

        }

        uNumCoeff = tmpTab[targetCoeffTokenIdx] >> 2;
        uNumTrailingOnes = tmpTab[targetCoeffTokenIdx] & 3;
    }

    if (uVLCSelect != 5) {
        // Write coeff_token from Look-up table

        H264ENC_MAKE_NAME_BS(PutBits)(state, EncTotalCoeff[uVLCSelect][uNumCoeff][uNumTrailingOnes].code,
                EncTotalCoeff[uVLCSelect][uNumCoeff][uNumTrailingOnes].len);

    } else {
        if (uNumCoeff == 0) {
            H264ENC_MAKE_NAME_BS(PutBits)(state, 3, 6);
        } else {
            // //  xxxxyy -> xxxx = uNumCoeff-l , yy = uNumTrailingOnes
            H264ENC_MAKE_NAME_BS(PutBits)(state, (((uNumCoeff-1)<<2)|uNumTrailingOnes), 6);
        }
    }

    // Write signs of NumTrailingOnes
    if( uNumTrailingOnesSaved )
      H264ENC_MAKE_NAME_BS(PutBits)(state, TrOneSigns, uNumTrailingOnesSaved);

    return ps;
}

Status H264ENC_MAKE_NAME_BS(PutLevels)(
    void* state,
    COEFFSTYPE* iLevels,      // Array of Levels to write
    Ipp32s      NumLevels,    // Total Number of coeffs to write (0-4 or 0-16)
    Ipp32s      TrailingOnes) // Trailing Ones
{
    Ipp32s VLCSelect = 0;
    Status ps = UMC_OK;
    static const Ipp32s vlc_inc_tab[7] = {0, 3, 6, 12, 24, 48, 0x8000};
    static const Ipp32s escape_tab[7] = {16, 16, 31, 61, 121, 241, 481};
    Ipp32s cnt;
    Ipp32s level_adj = 0;   // Used to flag that the next level
                            // is decreased in magnitude by 1 when coded.

    // Fixup first coeff level if Trailing Ones < 3
    if (TrailingOnes < 3)
    {
        level_adj = 1;  // Subtracted from the level when coded

        if ((TrailingOnes + NumLevels) > 10)
            VLCSelect = 1; // Start with different table
    }
    for (cnt = 0; cnt < NumLevels; cnt++) {
        Ipp32s L = ABS(iLevels[cnt]);
        Ipp32s sign = (L != iLevels[cnt]);
        L -= level_adj;

        if (L >= escape_tab[VLCSelect]) {  // 28-bit escape
            // Catch cases where the level is too large to write to the BS
            Ipp32u num = L-escape_tab[VLCSelect];

            if( num & (0xffffffff<<11)){
                Ipp32s addbit = 1;
                while(((Ipp32s)num-(2048<<addbit)+2048) >= 0) addbit++;
                addbit--;
                H264ENC_MAKE_NAME_BS(PutBits)(state, 1, 16+addbit);
                H264ENC_MAKE_NAME_BS(PutBits)(state, ((num-(2048<<addbit)+2048)<<1)|sign, 12+addbit);
            }else{
                H264ENC_MAKE_NAME_BS(PutBits)(state, 1, 16); // BsTypeType::PutBits maxes out at 24 bits
                H264ENC_MAKE_NAME_BS(PutBits)(state, (num<<1)|sign, 12);
            }
        } else if (VLCSelect) {    // VLC Level 1-6

            Ipp32s N= VLCSelect - 1;
            H264ENC_MAKE_NAME_BS(PutBits)(state, 1, ((L-1)>>(N))+1);
//          H264ENC_MAKE_NAME_BS(PutBits)(state, (((L-1)%(1<<N))<<1)+sign, VLCSelect);
            H264ENC_MAKE_NAME_BS(PutBits)(state, (((L-1)&((1<<N)-1))<<1)+sign, VLCSelect);
        } else { // VLC Level 0
            if (L < 8) {
                H264ENC_MAKE_NAME_BS(PutBits)(state, 1,sign+((L-1)<<1)+1);  // Start with a 0 if negative
            } else { // L 8-15
                H264ENC_MAKE_NAME_BS(PutBits)(state, 16+((L&7)<<1)+sign,19); // 19 bit escape
            }
        }

        L += level_adj; // Restore the true level for the following calculations

        // Adjust the VLC table depending on the current table and
        // the Level just coded.
        if (!VLCSelect && (L > 3))
            VLCSelect = 2;
        else if (L > vlc_inc_tab[VLCSelect])
            VLCSelect++;

        level_adj = 0;  // Clear this now that it's served its purpose
    }

    return ps;
}

Status H264ENC_MAKE_NAME_BS(PutTotalZeros)(
    void* state,
    Ipp32s TotalZeros,
    Ipp32s TotalCoeffs,
    Ipp32s bChromaDC,
    Ipp32s maxCoeff)
{
    Status ps = UMC_OK;

    if (maxCoeff < 15)
    {
        if (maxCoeff <=4)
        {
            TotalCoeffs += 4 - maxCoeff;
            if (TotalCoeffs > 3)
                TotalCoeffs = 3;
        }
        else if (maxCoeff <= 8)
        {
            TotalCoeffs += 8 - maxCoeff;
            if (TotalCoeffs > 7)
                TotalCoeffs = 7;

        }
        else
        {
            TotalCoeffs += 16 - maxCoeff;
            if (TotalCoeffs > 15)
                TotalCoeffs = 15;
        }
    }

    TotalCoeffs -= 1;

    if (bChromaDC) {
        H264ENC_MAKE_NAME_BS(PutBits)(state, EncTotalZerosChroma[bChromaDC-1][TotalZeros][TotalCoeffs].code,
            EncTotalZerosChroma[bChromaDC-1][TotalZeros][TotalCoeffs].len);
    } else {
        if (maxCoeff <= 4) {
            H264ENC_MAKE_NAME_BS(PutBits)(state, EncTotalZerosChroma[0][TotalZeros][TotalCoeffs].code,
                EncTotalZerosChroma[0][TotalZeros][TotalCoeffs].len);
        } else if (maxCoeff <= 8) {
            H264ENC_MAKE_NAME_BS(PutBits)(state, EncTotalZerosChroma[1][TotalZeros][TotalCoeffs].code,
                EncTotalZerosChroma[1][TotalZeros][TotalCoeffs].len);
        } else {
            H264ENC_MAKE_NAME_BS(PutBits)(state, EncTotalZeros4x4[TotalZeros][TotalCoeffs].code,
                EncTotalZeros4x4[TotalZeros][TotalCoeffs].len);
        }
    }

    return ps;
}

Status H264ENC_MAKE_NAME_BS(PutRuns)(
    void* state,
    Ipp8u* uRuns,
    Ipp32s TotalZeros,
    Ipp32s TotalCoeffs)
{
    Status ps = UMC_OK;
    Ipp32s cnt = 0;

    TotalCoeffs--; // Don't write the last run, since it can be inferred.

    while (TotalZeros && (cnt != TotalCoeffs)) {
        Ipp32u zeros_idx = (TotalZeros > 6) ? 6 : TotalZeros-1;
        H264ENC_MAKE_NAME_BS(PutBits)(state, EncRuns[uRuns[cnt]][zeros_idx].code,
            EncRuns[uRuns[cnt]][zeros_idx].len);
        TotalZeros -= uRuns[cnt];
        cnt++;
    }

    return ps;
}

/**************************** cabac **********************************/
Status H264ENC_MAKE_NAME_BS(MBFieldModeInfo_CABAC)(
    void* state,
    Ipp32s mb_field,
    Ipp32s field_available_left,
    Ipp32s field_available_above)
{
    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(
        state,
        field_available_left + field_available_above + ctxIdxOffset[MB_FIELD_DECODING_FLAG],
        mb_field != 0);
    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(MBTypeInfo_CABAC)(
    void* state,
    EnumSliceType SliceType,
    Ipp32s mb_type_cur,
    MB_Type type_cur,
    MB_Type type_left,
    MB_Type type_above,
    Ipp32s  ibl_left,
    Ipp32s  ibl_above)
{
    Ipp32s a, b,c=0;
    Ipp32s type_temp;
    Ipp32s itype=mb_type_cur-1;
    Ipp32u ctx;

    if(IS_INTRA_SLICE(SliceType)){
        b = (type_above >= NUMBER_OF_MBTYPES)?0:((type_above != MBTYPE_INTRA || ibl_above) ? 1 : 0 );
        a = (type_left >= NUMBER_OF_MBTYPES)?0:((type_left != MBTYPE_INTRA || ibl_left) ? 1 : 0 );
        ctx = ctxIdxOffset[MB_TYPE_I];
encoding_intra:
        if (type_cur==MBTYPE_INTRA){ // 4x4 Intra
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a + b, 0);
        }else if( type_cur == MBTYPE_PCM){ // PCM-MODE
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a + b, 1);
            H264ENC_MAKE_NAME_BS(EncodeFinalSingleBin_CABAC)(state, 1);
        }else{ // 16x16 Intra
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a + b, 1);
            H264ENC_MAKE_NAME_BS(EncodeFinalSingleBin_CABAC)(state, 0);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 3 + c, itype / 12 != 0); // coding of AC/no AC
            itype %= 12;
            if (itype<4){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 4 + c, 0);
            }else{
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 4 + c, 1);
                if (itype>=4 && itype<8){
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
                }else{
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                }
            }
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 6,  (itype&2) != 0 );
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 7-c,  (itype&1) != 0 );
        }
    }else{ // INTER
        if (IS_B_SLICE(SliceType)){
            type_temp = mb_type_cur;
            b = (type_above >= NUMBER_OF_MBTYPES)?0:(IS_SKIP_MBTYPE(type_above) ? 0 : 1 );
            a = (type_left >= NUMBER_OF_MBTYPES)?0:(IS_SKIP_MBTYPE(type_left) ? 0 : 1 );
            if (mb_type_cur>=24) type_temp=24;
            ctx = ctxIdxOffset[MB_TYPE_B];

            if (type_temp==0){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a+b,  0);
            }else if (type_temp<=2){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a+b,  1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 3,  0);
                if (type_temp-1) H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else      H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
            }else if (type_temp<=10){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a+b,1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 3,  1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 4,  0);
                if (((type_temp-3)>>2)&0x01)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
                if (((type_temp-3)>>1)&0x01)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
                if ((type_temp-3)&0x01)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
            }else if (type_temp==11 || type_temp==11*2){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a+b,1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 3,  1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 4,  1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                if (type_temp!=11)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
            }else{
                if (type_temp > 22 ) type_temp--;

                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + a+b,1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 3,  1);
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 4,  1);

                if ((type_temp-12)&8)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
                if ((type_temp-12)&4)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
                if ((type_temp-12)&2)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
                if ((type_temp-12)&1)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 5,  0);
                //if (type_temp >12) type_temp++;
            }
        if( type_cur==MBTYPE_PCM ){
            H264ENC_MAKE_NAME_BS(EncodeFinalSingleBin_CABAC)(state, 1);
            return UMC_OK;
        }
        if(type_cur==MBTYPE_INTRA_16x16){
            H264ENC_MAKE_NAME_BS(EncodeFinalSingleBin_CABAC)(state, 0);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 6,  (mb_type_cur-24)/12 != 0);
            mb_type_cur %= 12;

            type_temp = mb_type_cur >> 2;
            if (type_temp==0){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 7,  0);
            }else{
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 7,  1);
                if (type_temp==1){
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 7,  0);
                }else{
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 7,  1);
                }
            }

            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 8,  (mb_type_cur&2) != 0);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 8,  (mb_type_cur&1) != 0);
        }

        }else{ // P slice
            ctx = ctxIdxOffset[MB_TYPE_P_SP] ;
            if (IS_INTRA_MBTYPE(type_cur)){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 0,  1);
                a=0;b=3;c=1;itype-=5;
                goto encoding_intra;
            }else{
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 0,  0);
                switch(mb_type_cur)
                {
                case 0:
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 1,  0);
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 2,  0);
                    break;
                case 1:
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 1,  1);
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 3,  1);
                    break;
                case 2:
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 1,  1);
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 3,  0);
                    break;
                case 3:
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 1,  0);
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + 2,  1);
                    break;
                default:
                    return UMC_ERR_FAILED;
                }
            }
        }
     }
    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(SubTypeInfo_CABAC)(
    void* state,
    EnumSliceType SliceType,
    Ipp32s type)
{
    Ipp32u ctx;
    if (!IS_B_SLICE(SliceType)){
        ctx = ctxIdxOffset[SUB_MB_TYPE_P_SP];
        switch (type)
        {
        case 0:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx,1);
            break;
        case 1:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+0,0);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+1,0);
            break;
        case 2:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+0,0);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+1,1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2,1);
            break;
        case 3:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+0,0);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+1,1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2,0);
            break;
        }
    } else {
        ctx = ctxIdxOffset[SUB_MB_TYPE_B];
        if (!type){
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+0,0);
            return UMC_OK;
        } else {
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+0,1);
            type--;
        }

        if (type<2){
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+1,0);
            if (!type)
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,0);
            else
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,1);
        }else if (type<6){
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+1,1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2,0);
            if (((type-2)>>1)&1)
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,1);
            else
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,0);
            if ((type-2)&1)
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,1);
            else
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,0);
        } else {
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+1,1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2,1);
            if (((type-6)>>2)&1){
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,1);
                if ((type-6)&1)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,0);
            } else {
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,0);
                if (((type-6)>>1)&1)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,0);
                if ((type-6)&1)
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,1);
                else
                    H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3,0);
            }
        }
    }
    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(IntraPredMode_CABAC)(
    void* state,
    Ipp32s mode)
{
    if (mode == -1)
    {
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctxIdxOffset[PREV_INTRA4X4_PRED_MODE_FLAG], 1);
    }
    else
    {
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctxIdxOffset[PREV_INTRA4X4_PRED_MODE_FLAG], 0);
        Ipp32u ctx = ctxIdxOffset[REM_INTRA4X4_PRED_MODE];
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx,(mode&1));
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx,(mode&2)>>1);
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx,(mode&4)>>2);
    }
    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(ChromaIntraPredMode_CABAC)(
    void* state,
    Ipp32s mode,
    Ipp32s left_p,
    Ipp32s top_p)
{
    Ipp32u ctx = ctxIdxOffset[INTRA_CHROMA_PRED_MODE];
    switch( mode ){
        case 0:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+left_p+top_p,0);
            break;
        case 1:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+left_p+top_p,1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3, 0);
            break;
        case 2:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+left_p+top_p,1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3, 1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3, 0);
            break;
        case 3:
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+left_p+top_p,1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3, 1);
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+3, 1);
            break;
    }

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(MVD_CABAC)(
    void* state,
    Ipp32s vector,
    Ipp32s left_p,
    Ipp32s top_p,
    Ipp32s contextbase)
{
    Ipp32s ctx_mod;
    Ipp32s sumabs = labs(left_p);
    sumabs += labs(top_p);

    if (sumabs < 3)
        ctx_mod = 0;
    else if (sumabs <= 32)
        ctx_mod = 1;
    else
        ctx_mod = 2;

    Ipp32u ctx = ctxIdxOffset[contextbase];
    if (vector== 0){
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+ctx_mod, 0);
    } else {
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+ctx_mod, 1);
        H264ENC_MAKE_NAME_BS(EncodeExGRepresentedMVS_CABAC)(state, ctx+3,labs(vector)-1);
        H264ENC_MAKE_NAME_BS(EncodeBypass_CABAC)(state, vector<0);
    }
    return UMC_OK;
}

/*!
 ****************************************************************************
 * \brief
 *    This function is used to arithmetically encode the coded
 *    block pattern of a given delta quant.
 ****************************************************************************
 */
Status H264ENC_MAKE_NAME_BS(DQuant_CABAC)(
    void* state,
    Ipp32s deltaQP,
    Ipp32s left_c)
{
    Ipp32s value;
    Ipp32u ctx = ctxIdxOffset[MB_QP_DELTA];
    value = 2 * labs(deltaQP) + (deltaQP <= 0) - 1;

    if (deltaQP == 0)
    {
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + left_c, 0);
    }
    else
    {
        H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx + left_c, 1);
        if (value == 1)
        {
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2, 0);
        }
        else
        {
            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2, 1);
            Ipp32s temp=value-1;
            while ((--temp)>0)
                H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2+1, 1);

            H264ENC_MAKE_NAME_BS(EncodeSingleBin_CABAC)(state, ctx+2+1, 0);
        }
    }
    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutScalingList)(
    void* state,
    const Ipp8u* scalingList,
    Ipp32s sizeOfScalingList,
    bool& useDefaultScalingMatrixFlag)
{
    Ipp16s lastScale, nextScale;
    Ipp32s j;

    Ipp16s delta_scale;
    Ipp8s delta_code;
    const Ipp32s* scan;

    lastScale=nextScale=8;

    if( sizeOfScalingList == 16 )
        scan = UMC_H264_ENCODER::dec_single_scan[0];
    else
        scan = UMC_H264_ENCODER::dec_single_scan_8x8[0];

    for( j = 0; j<sizeOfScalingList; j++ ){
         if( nextScale != 0 ){
            delta_scale = (Ipp16s)(scalingList[scan[j]]-lastScale);
            delta_code = (Ipp8s)(delta_scale);

            //Put delta_scale
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, SIGNED_VLC_CODE(delta_code));

            nextScale = scalingList[scan[j]];
            useDefaultScalingMatrixFlag = ( j==0 && nextScale == 0 ); //Signal that input matrix is incorrect
     }
         lastScale = (nextScale==0) ? lastScale:nextScale;
    //scalingList[scan[j]] = (nextScale==0) ? lastScale:nextScale; // Update the actual scalingList matrix with the correct values
    //lastScale = scalingList[scan[j]];
    }

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSEI_UserDataUnregistred)(
    void* state,
    void* data,
    Ipp32s data_size )
{
    static const Ipp8u UUID[16] = {
        0x0d, 0xad, 0x3f, 0x34,
        0x8e, 0x9a, 0x48, 0xd7,
        0x8a, 0x7e, 0xc1, 0xe1,
        0xb4, 0x7b, 0xdf, 0x9a
    };

    H264BsType* bs = (H264BsType *)state;
    Ipp8u* user_data = (Ipp8u*)data;
    Ipp32s i, data_size_counter;

    data_size += 16; //+ UUID size
    data_size_counter = data_size;
    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_USER_DATA_UNREGISTERED, 8);
    //payload size
    while( data_size_counter >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size_counter -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size_counter, 8);

    //uuid
    for( i=0; i<16; i++ )
        H264ENC_MAKE_NAME_BS(PutBits)(state, UUID[i], 8);

    for( i=0; i<data_size-16; i++ )
        H264ENC_MAKE_NAME_BS(PutBits)(state, user_data[i], 8);

    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSEI_BufferingPeriod)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const Ipp8u NalHrdBpPresentFlag,
    const Ipp8u VclHrdBpPresentFlag,
    const H264SEIData_BufferingPeriod& buf_period_data)
{
    Ipp32s SchedSelIdx;
    Ipp32s data_size, data_size_bits;

    Ipp8u seq_parameter_set_id, cpb_cnt_minus1_hrd, cpb_cnt_minus1_vcl, initial_cpb_removal_delay_length_minus1;

    seq_parameter_set_id = seq_parms.seq_parameter_set_id;

    if (seq_parms.vui_parameters_present_flag)
    {
        if (seq_parms.vui_parameters.nal_hrd_parameters_present_flag)
        {
            cpb_cnt_minus1_hrd = seq_parms.vui_parameters.hrd_params.cpb_cnt_minus1;
            cpb_cnt_minus1_vcl = 0;
            initial_cpb_removal_delay_length_minus1 = seq_parms.vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1;
        }
        else if (seq_parms.vui_parameters.vcl_hrd_parameters_present_flag)
        {
            cpb_cnt_minus1_hrd = 0;
            cpb_cnt_minus1_vcl = seq_parms.vui_parameters.vcl_hrd_params.cpb_cnt_minus1;
            initial_cpb_removal_delay_length_minus1 = seq_parms.vui_parameters.vcl_hrd_params.initial_cpb_removal_delay_length_minus1;
        }
        else
        {
            cpb_cnt_minus1_hrd = 0;
            cpb_cnt_minus1_vcl = 0;
            initial_cpb_removal_delay_length_minus1 = 23;
        }
    }
    else
    {
        cpb_cnt_minus1_hrd = 0;
        cpb_cnt_minus1_vcl = 0;
        initial_cpb_removal_delay_length_minus1 = 23;
    }

    H264BsType* bs = (H264BsType *)state;

    data_size_bits = MSBIT(seq_parameter_set_id + 1) * 2 - 1;
    data_size_bits += (NalHrdBpPresentFlag * (cpb_cnt_minus1_hrd + 1) + VclHrdBpPresentFlag * (cpb_cnt_minus1_vcl + 1)) * (initial_cpb_removal_delay_length_minus1 + 1) * 2;
    data_size = (data_size_bits + 7) >> 3;

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_BUFFERING_PERIOD, 8);
    //payload size
    while( data_size >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parameter_set_id);

    if (NalHrdBpPresentFlag)
        for (SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1_hrd; SchedSelIdx++)
        {
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.nal_initial_cpb_removal_delay[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.nal_initial_cpb_removal_delay_offset[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
        }

    if (VclHrdBpPresentFlag)
        for (SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1_vcl; SchedSelIdx++)
        {
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.vcl_initial_cpb_removal_delay[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.vcl_initial_cpb_removal_delay_offset[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
        }

        if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
            H264BsBase_WriteTrailingBits(&bs->m_base);

    return UMC_OK;
}


Status H264ENC_MAKE_NAME_BS(PutScalableNestingSEI_BufferingPeriod)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const H264SEIData_BufferingPeriod& buf_period_data,
    const SVCVUIParams *vui,
    const Ipp8u all_layer_representations_in_au_flag,
    const Ipp32u num_layer_representations,
    const Ipp8u *sei_dependency_id,
    const Ipp8u *sei_quality_id,
    const Ipp8u sei_temporal_id)
{
    Ipp32s SchedSelIdx;
    Ipp32s data_size, data_size_bits;
    Ipp32s nested_data_size, nested_data_size_bits;
    Ipp32u i;


    Ipp8u seq_parameter_set_id, cpb_cnt_minus1_hrd, cpb_cnt_minus1_vcl, initial_cpb_removal_delay_length_minus1;
    Ipp8u nalHrdBpPresentFlag = 0;
    Ipp8u vclHrdBpPresentFlag = 0;

    if (all_layer_representations_in_au_flag == 0) {
        if (num_layer_representations < 1 || !sei_dependency_id || !sei_quality_id)
            return UMC_ERR_INVALID_PARAMS;
     }

    seq_parameter_set_id = seq_parms.seq_parameter_set_id;

    if (vui) {
        nalHrdBpPresentFlag = vui->vui_ext_nal_hrd_parameters_present_flag;
        vclHrdBpPresentFlag = vui->vui_ext_vcl_hrd_parameters_present_flag;
        if (nalHrdBpPresentFlag) {
            cpb_cnt_minus1_hrd = vui->nal_hrd_parameters.cpb_cnt_minus1;
            cpb_cnt_minus1_vcl = 0;
            initial_cpb_removal_delay_length_minus1 = vui->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1;
        } else if (vclHrdBpPresentFlag) {
            cpb_cnt_minus1_hrd = 0;
            cpb_cnt_minus1_vcl = vui->vcl_hrd_parameters.cpb_cnt_minus1;
            initial_cpb_removal_delay_length_minus1 = vui->vcl_hrd_parameters.initial_cpb_removal_delay_length_minus1;
        } else {
            cpb_cnt_minus1_hrd = 0;
            cpb_cnt_minus1_vcl = 0;
            initial_cpb_removal_delay_length_minus1 = 23;
        }
    } else {
        cpb_cnt_minus1_hrd = 0;
        cpb_cnt_minus1_vcl = 0;
        initial_cpb_removal_delay_length_minus1 = 23;
    }

    H264BsType* bs = (H264BsType *)state;

    data_size_bits = 1;
    if (all_layer_representations_in_au_flag == 0) {
        data_size_bits += MSBIT((num_layer_representations - 1)) * 2 + 1;
        data_size_bits += num_layer_representations * (3 + 4) + 3; // s-,q-,tid
    }
    data_size = (data_size_bits + 7) >> 3;

    nested_data_size_bits = MSBIT(seq_parameter_set_id) * 2 + 1;
    nested_data_size_bits += (nalHrdBpPresentFlag * (cpb_cnt_minus1_hrd + 1) + vclHrdBpPresentFlag * (cpb_cnt_minus1_vcl + 1)) * (initial_cpb_removal_delay_length_minus1 + 1) * 2;
    nested_data_size = (nested_data_size_bits + 7) >> 3;

    data_size += 1; // payloadType
    data_size += nested_data_size / 255 + 1; // payloadSize
    data_size += nested_data_size;

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_SCALABLE_NESTING, 8);
    //payload size
    while (data_size >= 255) {
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    H264ENC_MAKE_NAME_BS(PutBit)(state, all_layer_representations_in_au_flag);
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, (num_layer_representations - 1));

    for (i = 0; i < num_layer_representations; i++) {
        H264ENC_MAKE_NAME_BS(PutBits)(state, sei_dependency_id[i], 3);
        H264ENC_MAKE_NAME_BS(PutBits)(state, sei_quality_id[i], 4);
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, sei_temporal_id, 3);

    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);

   
    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_BUFFERING_PERIOD, 8);
    //payload size
    while (nested_data_size >= 255) {
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        nested_data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, nested_data_size, 8);

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, seq_parameter_set_id);

    if (nalHrdBpPresentFlag)
        for (SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1_hrd; SchedSelIdx++) {
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.nal_initial_cpb_removal_delay[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.nal_initial_cpb_removal_delay_offset[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
        }

    if (vclHrdBpPresentFlag)
        for (SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1_vcl; SchedSelIdx++) {
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.vcl_initial_cpb_removal_delay[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
            H264ENC_MAKE_NAME_BS(PutBits)(state, buf_period_data.vcl_initial_cpb_removal_delay_offset[SchedSelIdx], initial_cpb_removal_delay_length_minus1 + 1);
        }

    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);

    return UMC_OK;
}


Status H264ENC_MAKE_NAME_BS(PutSEI_PictureTiming)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const Ipp8u CpbDpbDelaysPresentFlag,
    const Ipp8u pic_struct_present_flag,
    const H264SEIData_PictureTiming& timing_data)
{
    Ipp32s i, NumClockTS=0;
    H264BsType* bs = (H264BsType *)state;
    Ipp32s data_size, data_size_bits = 0;

    Ipp8u cpb_removal_delay_length_minus1, dpb_output_delay_length_minus1, time_offset_length;

    Ipp32u cpb_removal_delay = timing_data.cpb_removal_delay;
    Ipp32u dpb_output_delay = timing_data.dpb_output_delay;

    if (seq_parms.vui_parameters_present_flag)
    {
        if (seq_parms.vui_parameters.nal_hrd_parameters_present_flag)
        {
            cpb_removal_delay_length_minus1 =  seq_parms.vui_parameters.hrd_params.cpb_removal_delay_length_minus1;
            dpb_output_delay_length_minus1 =  seq_parms.vui_parameters.hrd_params.dpb_output_delay_length_minus1;
            time_offset_length = seq_parms.vui_parameters.hrd_params.time_offset_length;
        }
        else if (seq_parms.vui_parameters.vcl_hrd_parameters_present_flag)
        {
            cpb_removal_delay_length_minus1 =  seq_parms.vui_parameters.vcl_hrd_params.cpb_removal_delay_length_minus1;
            dpb_output_delay_length_minus1 =  seq_parms.vui_parameters.vcl_hrd_params.dpb_output_delay_length_minus1;
            time_offset_length = seq_parms.vui_parameters.vcl_hrd_params.time_offset_length;
        }
        else
        {
            cpb_removal_delay_length_minus1 =  23;
            dpb_output_delay_length_minus1 =  23;
            time_offset_length = 24;
        }
    }
    else
    {
        cpb_removal_delay_length_minus1 =  23;
        dpb_output_delay_length_minus1 =  23;
        time_offset_length = 24;
    }

    if (CpbDpbDelaysPresentFlag)
    {
        data_size_bits += cpb_removal_delay_length_minus1 + dpb_output_delay_length_minus1 + 2;
    }

    if (pic_struct_present_flag)
    {
        if (timing_data.pic_struct > 8)
            return UMC_ERR_FAILED;
        NumClockTS = (timing_data.pic_struct <= 2) ? 1 : (timing_data.pic_struct <= 4 || timing_data.pic_struct == 7) ? 2 : 3;
        data_size_bits += (4 + NumClockTS); // pic_struct and clock_timestamp_flags

        for (i = 0; i < NumClockTS; i ++)
        {
            Ipp32u timestamp_data_size_bits = timing_data.full_timestamp_flag[i] ? 17 : (1 +
                timing_data.seconds_flag[i] * (7 + timing_data.minutes_flag[i]*(7 + timing_data.hours_flag[i]*5)));
            data_size_bits += timing_data.clock_timestamp_flag[i] * (19 +
            timestamp_data_size_bits + time_offset_length);
        }

    }

    data_size = (data_size_bits + 7) >> 3;

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_PIC_TIMING, 8);
    //payload size
    while( data_size >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    if (CpbDpbDelaysPresentFlag)
    {
        H264ENC_MAKE_NAME_BS(PutBits)(state, cpb_removal_delay, cpb_removal_delay_length_minus1 + 1);
        H264ENC_MAKE_NAME_BS(PutBits)(state, dpb_output_delay, dpb_output_delay_length_minus1 + 1);
    }

    if (pic_struct_present_flag)
    {
        H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.pic_struct, 4);
        for (i = 0; i < NumClockTS; i ++)
        {
            H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.clock_timestamp_flag[i]);
            if (timing_data.clock_timestamp_flag[i])
            {
                H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.ct_type[i], 2);
                H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.nuit_field_based_flag[i]);
                H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.counting_type[i], 5);
                H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.full_timestamp_flag[i]);
                H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.discontinuity_flag[i]);
                H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.cnt_dropped_flag[i]);
                H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.n_frames[i], 8);
                if (timing_data.full_timestamp_flag[i])
                {
                    H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.seconds_value[i], 6);
                    H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.minutes_value[i], 6);
                    H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.hours_value[i], 5);
                }
                else
                {
                    H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.seconds_flag[i]);
                    if (timing_data.seconds_flag[i])
                    {
                        H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.seconds_value[i], 6);
                        H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.minutes_flag[i]);
                        if (timing_data.minutes_flag[i])
                        {
                            H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.minutes_value[i], 6);
                            H264ENC_MAKE_NAME_BS(PutBit)(state, timing_data.hours_flag[i]);
                            if (timing_data.hours_flag[i])
                                H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.hours_value[i], 5);
                        }
                    }
                }
                H264ENC_MAKE_NAME_BS(PutBits)(state, timing_data.time_offset[i], time_offset_length);
            }
        }
    }

    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSEI_DecRefPicMrkRep)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const H264SEIData_DecRefPicMarkingRepetition& decRefPicMrkRep)
{
    H264BsType* bs = (H264BsType *)state;
    Ipp32s i, data_size, data_size_bits = 0;

    // calculate payload size

    data_size_bits += MSBIT(decRefPicMrkRep.original_frame_num + 1) * 2;

    if (seq_parms.frame_mbs_only_flag == 0)
        data_size_bits += decRefPicMrkRep.original_field_pic_flag == 0 ? 1 : 2;

    data_size_bits += decRefPicMrkRep.sizeInBytes * 8 + decRefPicMrkRep.bitOffset;

    data_size = (data_size_bits + 7) >> 3;

    // put SEI to bitstream

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION, 8);
    //payload size
    while( data_size >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    // put payload
    H264ENC_MAKE_NAME_BS(PutBit)(state, decRefPicMrkRep.original_idr_flag);
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, decRefPicMrkRep.original_frame_num);

    if (seq_parms.frame_mbs_only_flag == 0) {
        H264ENC_MAKE_NAME_BS(PutBit)(state, decRefPicMrkRep.original_field_pic_flag);
        if (decRefPicMrkRep.original_field_pic_flag)
            H264ENC_MAKE_NAME_BS(PutBit)(state, decRefPicMrkRep.original_bottom_field_flag);
    }

    for(i = 0; i < decRefPicMrkRep.sizeInBytes; i ++)
        H264ENC_MAKE_NAME_BS(PutBits)(state, decRefPicMrkRep.dec_ref_pic_marking[i], 8);

    H264ENC_MAKE_NAME_BS(PutBits)(state, decRefPicMrkRep.dec_ref_pic_marking[i] >> (8 - decRefPicMrkRep.bitOffset), decRefPicMrkRep.bitOffset);


    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);


    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSEI_PanScanRectangle)(
    void* state,
    Ipp32u pan_scan_rect_id,
    Ipp8u pan_scan_rect_cancel_flag,
    Ipp8u pan_scan_cnt_minus1,
    Ipp32s * pan_scan_rect_left_offset,
    Ipp32s * pan_scan_rect_right_offset,
    Ipp32s * pan_scan_rect_top_offset,
    Ipp32s * pan_scan_rect_bottom_offset,
    Ipp32s pan_scan_rect_repetition_period)
{
    H264BsType* bs = (H264BsType *)state;

    Ipp32s i;
    Ipp32s data_size, data_size_bits = 0;
    Ipp32u code_num_offsets_se[3][4]; // for se(v) mapping of pan_scan_rect offsets

    data_size_bits += MSBIT(pan_scan_rect_id + 1) * 2;

    if (!pan_scan_rect_cancel_flag)
    {
        data_size_bits += MSBIT(pan_scan_cnt_minus1 + 1) * 2 - 1;
        for (i = 0; i <= pan_scan_cnt_minus1; i ++)
        {
            code_num_offsets_se[i][0] = pan_scan_rect_left_offset[i] > 0 ? (pan_scan_rect_left_offset[i] << 1) - 1 : -pan_scan_rect_left_offset[i] << 1;
            data_size_bits += MSBIT(code_num_offsets_se[i][0] + 1) * 2 - 1;
            code_num_offsets_se[i][1] = pan_scan_rect_right_offset[i] > 0 ? (pan_scan_rect_right_offset[i] << 1) - 1 : -pan_scan_rect_right_offset[i] << 1;
            data_size_bits += MSBIT(code_num_offsets_se[i][1] + 1) * 2 - 1;
            code_num_offsets_se[i][2] = pan_scan_rect_top_offset[i] > 0 ? (pan_scan_rect_top_offset[i] << 1) - 1 : -pan_scan_rect_top_offset[i] << 1;
            data_size_bits += MSBIT(code_num_offsets_se[i][2] + 1) * 2 - 1;
            code_num_offsets_se[i][3] = pan_scan_rect_bottom_offset[i] > 0 ? (pan_scan_rect_bottom_offset[i] << 1) - 1 : -pan_scan_rect_bottom_offset[i] << 1;
            data_size_bits += MSBIT(code_num_offsets_se[i][3] + 1) * 2 - 1;
        }
        data_size_bits += MSBIT(pan_scan_rect_repetition_period + 1) * 2 - 1;
    }

    data_size = (data_size_bits + 7) >> 3;

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_PAN_SCAN_RECT, 8);
    //payload size
    while( data_size >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pan_scan_rect_id);
    H264ENC_MAKE_NAME_BS(PutBit)(state, pan_scan_rect_cancel_flag);

    if (!pan_scan_rect_cancel_flag)
    {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pan_scan_cnt_minus1);
        for (i = 0; i <= pan_scan_cnt_minus1; i ++)
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, code_num_offsets_se[i][0]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, code_num_offsets_se[i][1]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, code_num_offsets_se[i][2]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, code_num_offsets_se[i][3]);
        }
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, pan_scan_rect_repetition_period);
    }

    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSEI_FillerPayload)(
    void* state,
    Ipp32s payloadSize)
{
    Ipp32s i, data_size = payloadSize;
    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_FILLER_PAYLOAD, 8);
    //payload size
    while( data_size >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    for (i = 0; i < payloadSize; i++)
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSEI_UserDataRegistred_ITU_T_T35)(
    void* state,
    Ipp8u itu_t_t35_country_code,
    Ipp8u itu_t_t35_country_extension_byte,
    Ipp8u * itu_t_t35_payload_data,
    Ipp32s payloadSize )
{
    Ipp32s i, data_size = payloadSize;

    if (itu_t_t35_country_code != 0xff)
        data_size += 1;
    else
        data_size += 2;

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35, 8);
    //payload size
    while( data_size >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    H264ENC_MAKE_NAME_BS(PutBits)(state, itu_t_t35_country_code, 8);
    if (itu_t_t35_country_code != 0xff)
        i = 1;
    else
    {
        H264ENC_MAKE_NAME_BS(PutBits)(state, itu_t_t35_country_extension_byte, 8);
        i = 2;
    }
    do
    {
        H264ENC_MAKE_NAME_BS(PutBits)(state, itu_t_t35_payload_data[i], 8);
        i++;
    } while (i < payloadSize);

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSEI_RecoveryPoint)(
    void* state,
    Ipp32s recovery_frame_cnt,
    Ipp8u exact_match_flag,
    Ipp8u broken_link_flag,
    Ipp8u changing_slice_group_idc)
{
    H264BsType* bs = (H264BsType *)state;
    Ipp32s data_size, data_size_bits = 0;

    data_size_bits = MSBIT(recovery_frame_cnt+1) * 2 + 3;

    data_size = (data_size_bits + 7) >> 3;

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_RECOVERY_POINT, 8);
    //payload size
    while( data_size >= 255 ){
        H264ENC_MAKE_NAME_BS(PutBits)(state, 0xff, 8);
        data_size -= 255;
    }
    H264ENC_MAKE_NAME_BS(PutBits)(state, data_size, 8);

    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, recovery_frame_cnt);
    H264ENC_MAKE_NAME_BS(PutBit)(state, exact_match_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, broken_link_flag);
    H264ENC_MAKE_NAME_BS(PutBits)(state, changing_slice_group_idc,2);

    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSVCPrefix)(
    void* state,
    sNALUnitHeaderSVCExtension *svc_header)
{
    Ipp32u prefix_nal_unit_additional_extension_flag = 0;
    Ipp32u adaptive_ref_base_pic_marking_mode_flag = 0;
    H264ENC_MAKE_NAME_BS(PutBit)(state, svc_header->store_ref_base_pic_flag);

    if ((svc_header->use_ref_base_pic_flag ||
         svc_header->store_ref_base_pic_flag) && !svc_header->idr_flag) {

            /* To Do Base DecPicMarking */
        H264ENC_MAKE_NAME_BS(PutBits)(state, adaptive_ref_base_pic_marking_mode_flag, 1);
    }

    /* additional_prefix_nal_unit_extension_flag */
    H264ENC_MAKE_NAME_BS(PutBits)(state, prefix_nal_unit_additional_extension_flag, 1);

    return UMC_OK;
}

Status H264ENC_MAKE_NAME_BS(PutSVCScalabilityInfoSEI)(
    void* state,
    const H264SVCScalabilityInfoSEI *sei)
{
    Status ps = UMC_OK;
    Ipp32u i=0, j=0;
    H264BsType* bs = (H264BsType *)state;

    //payload type
    H264ENC_MAKE_NAME_BS(PutBits)(state, SEI_TYPE_SCALABILITY_INFO, 8);
    //payload size will be putted later in EndOfNAL. It's a hack for this SEI because payload size is unknown */

    H264ENC_MAKE_NAME_BS(PutBit)(state, sei->temporal_id_nesting_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, sei->priority_layer_info_present_flag);
    H264ENC_MAKE_NAME_BS(PutBit)(state, sei->priority_id_setting_flag);
    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->num_layers_minus1);

    for( i = 0; i <= sei->num_layers_minus1; i++ )
    {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->layer_id[i]);
        H264ENC_MAKE_NAME_BS(PutBits)(state, sei->priority_id[i], 6);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->discardable_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBits)(state, sei->dependency_id[i], 3);
        H264ENC_MAKE_NAME_BS(PutBits)(state, sei->quality_id[i], 4);
        H264ENC_MAKE_NAME_BS(PutBits)(state, sei->temporal_id[i], 3);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->sub_pic_layer_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->sub_region_layer_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->iroi_division_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->profile_level_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->bitrate_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->frm_rate_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->frm_size_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->layer_dependency_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->parameter_sets_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->bitstream_restriction_info_present_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->exact_interlayer_pred_flag[i]);
        if( sei->sub_pic_layer_flag[i] || sei->iroi_division_info_present_flag[i] )
            H264ENC_MAKE_NAME_BS(PutBit)(state, sei->exact_sample_value_match_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->layer_conversion_flag[i]);
        H264ENC_MAKE_NAME_BS(PutBit)(state, sei->layer_output_flag[i]);
        if ( sei->profile_level_info_present_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->layer_profile_level_idc[i], 24);
        }

        if ( sei->bitrate_info_present_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->avg_bitrate[i], 16);
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->max_bitrate_layer[i], 16);
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->max_bitrate_layer_representation[i], 16);
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->max_bitrate_calc_window[i], 16);
        }
        if ( sei->frm_rate_info_present_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->constant_frm_rate_idc[i], 2);
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->avg_frm_rate[i], 16);
        }

        if ( sei->frm_size_info_present_flag[i] || sei->iroi_division_info_present_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->frm_width_in_mbs_minus1[i]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->frm_height_in_mbs_minus1[i]);
        }

        if ( sei->sub_region_layer_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->base_region_layer_id[i]);
            H264ENC_MAKE_NAME_BS(PutBit)(state, sei->dynamic_rect_flag[i]);
            if ( !sei->dynamic_rect_flag[i] )
            {
                H264ENC_MAKE_NAME_BS(PutBits)(state, sei->horizontal_offset[i], 16);
                H264ENC_MAKE_NAME_BS(PutBits)(state, sei->vertical_offset[i], 16);
                H264ENC_MAKE_NAME_BS(PutBits)(state, sei->region_width[i], 16);
                H264ENC_MAKE_NAME_BS(PutBits)(state, sei->region_height[i], 16);
            }
        }

        if( sei->sub_pic_layer_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->roi_id[i]);
        }

        if ( sei->iroi_division_info_present_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutBit)(state, sei->iroi_grid_flag[i]);
            if( sei->iroi_grid_flag[i] )
            {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->grid_width_in_mbs_minus1[i]);
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->grid_height_in_mbs_minus1[i]);
            }
            else
            {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->num_rois_minus1[i]);
                for ( j = 0; j <= sei->num_rois_minus1[i]; j++ )
                {
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->first_mb_in_roi[i][j]);
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->roi_width_in_mbs_minus1[i][j]);
                    H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->roi_height_in_mbs_minus1[i][j]);
                }
            }
        }

        if ( sei->layer_dependency_info_present_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->num_directly_dependent_layers[i]);
            for( j = 0; j < sei->num_directly_dependent_layers[i]; j++ )
            {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->directly_dependent_layer_id_delta_minus1[i][j]);
            }
        }
        else
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->layer_dependency_info_src_layer_id_delta[i]);
        }

        if ( sei->parameter_sets_info_present_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->num_seq_parameter_set_minus1[i]);
            for( j = 0; j <= sei->num_seq_parameter_set_minus1[i]; j++ )
            {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->seq_parameter_set_id_delta[i][j]);
            }
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->num_subset_seq_parameter_set_minus1[i]);
            for( j = 0; j <= sei->num_subset_seq_parameter_set_minus1[i]; j++ )
            {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->subset_seq_parameter_set_id_delta[i][j]);
            }
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->num_pic_parameter_set_minus1[i]);
            for( j = 0; j <= sei->num_pic_parameter_set_minus1[i]; j++ )
            {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->pic_parameter_set_id_delta[i][j]);
            }
        }
        else
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->parameter_sets_info_src_layer_id_delta[i]);
        }

        if (sei->bitstream_restriction_info_present_flag[i])
        {
            H264ENC_MAKE_NAME_BS(PutBit)(state, sei->motion_vectors_over_pic_boundaries_flag[i]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->max_bytes_per_pic_denom[i]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->max_bits_per_mb_denom[i]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->log2_max_mv_length_horizontal[i]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->log2_max_mv_length_vertical[i]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->num_reorder_frames[i]);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->max_dec_frame_buffering[i]);
        }

        if( sei->layer_conversion_flag[i] )
        {
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->conversion_type_idc[i]);
            for ( j=0; j<2; j++ )
            {
                H264ENC_MAKE_NAME_BS(PutBit)(state, sei->rewriting_info_flag[i][j]);
                if( sei->rewriting_info_flag[i][j] )
                {
                    H264ENC_MAKE_NAME_BS(PutBits)(state, sei->rewriting_profile_level_idc[i][j], 24);
                    H264ENC_MAKE_NAME_BS(PutBits)(state, sei->rewriting_avg_bitrate[i][j], 16);
                    H264ENC_MAKE_NAME_BS(PutBits)(state, sei->rewriting_max_bitrate[i][j], 16);
                }
            }
        }
    }
    if (sei->priority_layer_info_present_flag)
    {
        H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->pr_num_dId_minus1);
        for (i=0; i<=sei->pr_num_dId_minus1; i++)
        {
            H264ENC_MAKE_NAME_BS(PutBits)(state, sei->pr_dependency_id[i], 3);
            H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->pr_num_minus1[i]);
            for (j=0; j<=sei->pr_num_minus1[i]; j++)
            {
                H264ENC_MAKE_NAME_BS(PutVLCCode)(state, sei->pr_id[i][j]);
                H264ENC_MAKE_NAME_BS(PutBits)(state, sei->pr_profile_level_idc[i][j], 24);
                H264ENC_MAKE_NAME_BS(PutBits)(state, sei->pr_avg_bitrate[i][j], 16);
                H264ENC_MAKE_NAME_BS(PutBits)(state, sei->pr_max_bitrate[i][j], 16);
            }
        }
    }

    if(sei->priority_id_setting_flag)
    {
        Ipp32u PriorityIdSettingUriIdx = 0;
        do
        {
            Ipp32u uiTemp = sei->priority_id_setting_uri[PriorityIdSettingUriIdx];
            H264ENC_MAKE_NAME_BS(PutBits)(state, uiTemp, 8);
        }while( sei->priority_id_setting_uri[ PriorityIdSettingUriIdx++ ]  !=  0 );
    }

    if (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsBase_WriteTrailingBits(&bs->m_base);

    return ps;
} // PutSVCScalabilityInfoSEI()

#undef H264SliceType
#undef H264BsType
#undef H264ENC_MAKE_NAME_BS
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
