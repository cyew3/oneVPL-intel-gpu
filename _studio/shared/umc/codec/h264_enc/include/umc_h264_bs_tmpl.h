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
#define H264BsType H264BsFake_16u32s
#else // real bitstream
#define H264ENC_MAKE_NAME_BS(NAME) H264BsReal_##NAME##_16u32s
#define H264BsType H264BsReal_16u32s
#endif

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define H264SliceType H264ENC_MAKE_NAME(H264Slice)

#ifdef FAKE_BITSTREAM

inline
void H264ENC_MAKE_NAME_BS(Reset)(
    void* state)
{
    H264BsType* bs = (H264BsType *)state;
    bs->m_base.m_bitOffset = 0;
}

#else // real bitstream

void H264ENC_MAKE_NAME_BS(Reset)(
    void* state);

#endif //FAKE_BITSTREAM

//void H264ENC_MAKE_NAME_BS(ResetRBSP)(
//    void* state);

Ipp32u H264ENC_MAKE_NAME_BS(EndOfNAL)(
    void* state,
    Ipp8u* const pout,
    Ipp8u const uIDC,
    NAL_Unit_Type const uUnitType,
    bool& startPicture,
    Ipp8u nal_header_ext[3]);

void H264ENC_MAKE_NAME_BS(PutDecRefPicMarking)(
    void* state,
    const H264SliceHeader& slice_hdr,
    const EnumPicClass& ePictureClass,
    const DecRefPicMarkingInfo& decRefPicMarkingInfo);

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
    const sNALUnitHeaderSVCExtension *svc_header);

Status H264ENC_MAKE_NAME_BS(PutSeqParms)(
    void* state,
    const H264SeqParamSet& seq_parms);

Status H264ENC_MAKE_NAME_BS(PutSeqExParms)(
    void* state,
    const H264SeqParamSet& seq_parms);

Status H264ENC_MAKE_NAME_BS(PutPicParms)(
    void* state,
    const H264PicParamSet& pic_parms,
    const H264SeqParamSet& seq_parms);

Status H264ENC_MAKE_NAME_BS(PutPicDelimiter)(
    void* state,
    EnumPicCodType PicCodType);

void H264ENC_MAKE_NAME_BS(PutDQUANT)(
    void* state,
    const Ipp32u quant,
    const Ipp32u quant_prev);

Status H264ENC_MAKE_NAME_BS(PutNumCoeffAndTrailingOnes)(
    void* state,
    Ipp32u uVLCSelect,
    Ipp32s bChromaDC,
    Ipp32u uNumCoeff,
    Ipp32u uNumTrailingOnes,
    Ipp32u TrOneSigns,
    Ipp32s maxNumCoeffs);

Status H264ENC_MAKE_NAME_BS(PutLevels)(
    void* state,
    COEFFSTYPE* iLevels,
    Ipp32s NumLevels,
    Ipp32s TrailingOnes);

Status H264ENC_MAKE_NAME_BS(PutTotalZeros)(
    void* state,
    Ipp32s TotalZeros,
    Ipp32s TotalCoeffs,
    Ipp32s bChromaDC,
    Ipp32s maxCoeff);

Status H264ENC_MAKE_NAME_BS(PutRuns)(
    void* state,
    Ipp8u* uRuns,
    Ipp32s TotalZeros,
    Ipp32s TotalCoeffs);

Status H264ENC_MAKE_NAME_BS(MBFieldModeInfo_CABAC)(
    void* state,
    Ipp32s mb_field,
    Ipp32s field_available_left,
    Ipp32s field_available_above);

Status H264ENC_MAKE_NAME_BS(MBTypeInfo_CABAC)(
    void* state,
    EnumSliceType SliceType,
    Ipp32s mb_type_cur,
    MB_Type type_cur,
    MB_Type type_left,
    MB_Type type_above,
    Ipp32s  ibl_left,
    Ipp32s  ibl_above);

Status H264ENC_MAKE_NAME_BS(SubTypeInfo_CABAC)(
    void* state,
    EnumSliceType SliceType,
    Ipp32s type);

Status H264ENC_MAKE_NAME_BS(ChromaIntraPredMode_CABAC)(
    void* state,
    Ipp32s mode,
    Ipp32s left_p,
    Ipp32s top_p);

Status H264ENC_MAKE_NAME_BS(IntraPredMode_CABAC)(
    void* state,
    Ipp32s mode);

Status H264ENC_MAKE_NAME_BS(MVD_CABAC)(
    void* state,
    Ipp32s vector,
    Ipp32s left_p,
    Ipp32s top_p,
    Ipp32s contextbase);

Status H264ENC_MAKE_NAME_BS(DQuant_CABAC)(
    void* state,
    Ipp32s deltaQP,
    Ipp32s left_c);

Status H264ENC_MAKE_NAME_BS(PutScalingList)(
    void* state,
    const Ipp8u* scalingList,
    Ipp32s sizeOfScalingList,
    bool& useDefaultScalingMatrixFlag);

Status H264ENC_MAKE_NAME_BS(PutSEI_UserDataUnregistred)(
    void* state,
    void* data,
    Ipp32s data_size );

Status H264ENC_MAKE_NAME_BS(PutSEI_BufferingPeriod)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const Ipp8u NalHrdBpPresentFlag,
    const Ipp8u VclHrdBpPresentFlag,
    const H264SEIData_BufferingPeriod& buf_period_data);

Status H264ENC_MAKE_NAME_BS(PutSEI_PanScanRectangle)(
    void* state,
    Ipp32u pan_scan_rect_id,
    Ipp8u pan_scan_rect_cancel_flag,
    Ipp8u pan_scan_cnt_minus1,
    Ipp32s * pan_scan_rect_left_offset,
    Ipp32s * pan_scan_rect_right_offset,
    Ipp32s * pan_scan_rect_top_offset,
    Ipp32s * pan_scan_rect_bottom_offset,
    Ipp32s pan_scan_rect_repetition_period);

Status H264ENC_MAKE_NAME_BS(PutSEI_FillerPayload)(
    void* state,
    Ipp32s payloadSize);

Status H264ENC_MAKE_NAME_BS(PutSEI_UserDataRegistred_ITU_T_T35)(
    void* state,
    Ipp8u itu_t_t35_country_code,
    Ipp8u itu_t_t35_country_extension_byte,
    Ipp8u * itu_t_t35_payload_data,
    Ipp32s payloadSize );

Status H264ENC_MAKE_NAME_BS(PutSEI_PictureTiming)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const Ipp8u CpbDpbDelaysPresentFlag,
    const Ipp8u pic_struct_present_flag,
    const H264SEIData_PictureTiming& timing_data);

Status H264ENC_MAKE_NAME_BS(PutSEI_DecRefPicMrkRep)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const H264SEIData_DecRefPicMarkingRepetition& decRefPicMrkRep);

Status H264ENC_MAKE_NAME_BS(PutSEI_RecoveryPoint)(
    void* state,
    Ipp32s recovery_frame_cnt,
    Ipp8u exact_match_flag,
    Ipp8u broken_link_flag,
    Ipp8u changing_slice_group_idc);

Status H264ENC_MAKE_NAME_BS(PutSVCPrefix)(
    void* state,
    sNALUnitHeaderSVCExtension *svc_header);

Status H264ENC_MAKE_NAME_BS(PutSVCScalabilityInfoSEI)(
    void* state,
    const H264SVCScalabilityInfoSEI *sei);

Status H264ENC_MAKE_NAME_BS(PutScalableNestingSEI_BufferingPeriod)(
    void* state,
    const H264SeqParamSet& seq_parms,
    const H264SEIData_BufferingPeriod& buf_period_data,
    const SVCVUIParams *vui,
    const Ipp8u all_layer_representations_in_au_flag,
    const Ipp32u num_layer_representations,
    const Ipp8u *sei_dependency_id,
    const Ipp8u *sei_quality_id,
    const Ipp8u sei_temporal_id);


#undef H264SliceType
#undef H264BsType
#undef H264ENC_MAKE_NAME_BS
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
