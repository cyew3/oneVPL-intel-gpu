/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2006-2014 Intel Corporation. All Rights Reserved.
*/

#ifndef __UMC_HEVC_DDI_H
#define __UMC_HEVC_DDI_H

#pragma warning(disable: 4201)

#define MK_HEVCVER(j, n)    (((j & 0x0000ffff) << 16) | (n & 0x0000ffff))
#define HEVC_SPEC_VER       MK_HEVCVER(0, 85)


typedef struct _DXVA_Intel_PicEntry_HEVC
{
    union
    {
        struct
        {
            UCHAR   Index7bits          : 7;
            UCHAR   long_term_ref_flag  : 1;
        };

        UCHAR   bPicEntry;
    };

} DXVA_Intel_PicEntry_HEVC, *LPDXVA_Intel_PicEntry_HEVC;

typedef struct _DXVA_Intel_Status_HEVC {
  UINT                      StatusReportFeedbackNumber;
  DXVA_Intel_PicEntry_HEVC        current_picture; /* flag is bot field flag */
  UCHAR                     field_pic_flag;
  UCHAR                     bDXVA_Func;
  UCHAR                     bBufType;
  UCHAR                     bStatus;
  UCHAR                     bReserved8Bits;
  USHORT                    wNumMbsAffected;
} DXVA_Intel_Status_HEVC, *LPDXVA_Intel_Status_HEVC;



#if HEVC_SPEC_VER == MK_HEVCVER(0, 83)

typedef struct _DXVA_Intel_PicParams_HEVC
{
    USHORT        PicWidthInMinCbsY;
    USHORT        PicHeightInMinCbsY;
    union
    {
        UINT        value;
        struct
        {
            UINT        chroma_format_idc                : 2;
            UINT        separate_colour_plane_flag            : 1;
            UINT        pcm_enabled_flag                : 1;
            UINT        scaling_list_enabled_flag            : 1;
            UINT        transform_skip_enabled_flag        : 1;
            UINT        amp_enabled_flag                : 1;
            UINT        strong_intra_smoothing_enabled_flag    : 1;
            UINT        sign_data_hiding_flag            : 1;
            UINT        constrained_intra_pred_flag            : 1;
            UINT        cu_qp_delta_enabled_flag            : 1;
            UINT        weighted_pred_flag                : 1;
            UINT        weighted_bipred_flag            : 1;
            UINT        transquant_bypass_enabled_flag        : 1;
            UINT        tiles_enabled_flag                : 1;
            UINT        entropy_coding_sync_enabled_flag        : 1;
            UINT        pps_loop_filter_across_slices_enabled_flag: 1;
            UINT        loop_filter_across_tiles_enabled_flag    : 1;
            UINT        pcm_loop_filter_disabled_flag        : 1;
            UINT        field_pic_flag                    : 1;
            UINT        bottom_field_flag                : 1;
            UINT        NoPicReorderingFlag                : 1;
            UINT        NoBiPredFlag                    : 1;
            UINT        reserved_bits                    : 9;
        } fields;
    } PicFlags;

    DXVA_Intel_PicEntry_HEVC    CurrPic; 
    INT            CurrPicOrderCntVal;
    UCHAR            sps_max_dec_pic_buffering_minus1;    // [0..15]
    DXVA_Intel_PicEntry_HEVC    RefFrameList[16];
    INT            PicOrderCntValList[16];
    USHORT        RefFieldPicFlag;
    USHORT        RefBottomFieldFlag;
    UCHAR            bit_depth_luma_minus8;            // [0..6]
    UCHAR            bit_depth_chroma_minus8;            // [0..6]
    UCHAR            pcm_sample_bit_depth_luma_minus1;    // [0..13]
    UCHAR            pcm_sample_bit_depth_chroma_minus1;    // [0..13]
    UCHAR         log2_min_luma_coding_block_size_minus3; //[0..3]
    UCHAR         log2_diff_max_min_luma_coding_block_size;//[0..3]
    UCHAR         log2_min_transform_block_size_minus2;    // [0..3]
    UCHAR         log2_diff_max_min_transform_block_size;// [0..3]
    UCHAR         log2_min_pcm_luma_coding_block_size_minus3; // [0..2]
    UCHAR         log2_diff_max_min_pcm_luma_coding_block_size;        // [0..2]
    UCHAR            max_transform_hierarchy_depth_intra;    // [0..4]
    UCHAR            max_transform_hierarchy_depth_inter;    // [0..4]
    CHAR            init_qp_minus26;                // [-26..25]
    UCHAR            diff_cu_qp_delta_depth;            // [0..3]
    CHAR            pps_cb_qp_offset;
    CHAR            pps_cr_qp_offset;
    UCHAR            num_tile_columns_minus1;
    UCHAR            num_tile_rows_minus1;
    USHORT        column_width_minus1[19];
    USHORT        row_height_minus1[21];
    UCHAR            log2_parallel_merge_level_minus2;
    UINT             StatusReportFeedbackNumber;

    UCHAR            continuation_flag;

    union
    {
        UINT        value;
        struct
        {
            UINT        lists_modification_present_flag        : 1;
            UINT        long_term_ref_pics_present_flag        : 1;
            UINT        sps_temporal_mvp_enabled_flag        : 1;
            UINT        cabac_init_present_flag            : 1;
            UINT        output_flag_present_flag            : 1;
            UINT        dependent_slice_segments_enabled_flag    : 1;
            UINT        pps_slice_chroma_qp_offsets_present_flag: 1;

            UINT        sample_adaptive_offset_enabled_flag    : 1;
            UINT        deblocking_filter_control_present_flag    : 1;
            UINT        deblocking_filter_override_enabled_flag    : 1;
            UINT        pps_disable_deblocking_filter_flag        : 1;
            UINT        IrapPicFlag                    : 1;
            UINT        IdrPicFlag                    : 1;
            UINT        IntraPicFlag                    : 1;
            UINT        slice_segment_header_extension_present_flag : 1;
            UINT        reserved_bits                    : 17;
        } fields;
    } PicShortFormatFlags;


    UCHAR            RefPicSetStCurrBefore[8];
    UCHAR            RefPicSetStCurrAfter[8];
    UCHAR            RefPicSetLtCurr[8];
    UCHAR            log2_max_pic_order_cnt_lsb_minus4;    // [0..12]
    UCHAR            num_short_term_ref_pic_sets;
    UCHAR            num_long_term_ref_pics_sps;
    UCHAR            num_ref_idx_l0_default_active_minus1;
    UCHAR            num_ref_idx_l1_default_active_minus1;
    CHAR            pps_beta_offset_div2;
    CHAR            pps_tc_offset_div2;
    USHORT        StRPSBits;
    UCHAR            num_extra_slice_header_bits;        // [0..7]

}
DXVA_Intel_PicParams_HEVC, *LPDXVA_Intel_PicParams_HEVC;

#elif HEVC_SPEC_VER == MK_HEVCVER(0, 84)    // #if HEVC_SPEC_VER == MK_HEVCVER(0, 84)

typedef struct _DXVA_Intel_PicParams_HEVC
{
    USHORT        PicWidthInMinCbsY;
    USHORT        PicHeightInMinCbsY;

    union
    {
        UINT        value;
        struct
        {

            UINT    chroma_format_idc                           : 2;
            UINT    separate_colour_plane_flag                  : 1;
            UINT    pcm_enabled_flag                            : 1;
            UINT    scaling_list_enabled_flag                   : 1;
            UINT    transform_skip_enabled_flag                 : 1;
            UINT    amp_enabled_flag                            : 1;
            UINT    strong_intra_smoothing_enabled_flag         : 1;
            UINT    sign_data_hiding_flag                       : 1;
            UINT    constrained_intra_pred_flag                 : 1;
            UINT    cu_qp_delta_enabled_flag                    : 1;
            UINT    weighted_pred_flag                          : 1;
            UINT    weighted_bipred_flag                        : 1;
            UINT    transquant_bypass_enabled_flag              : 1;
            UINT    tiles_enabled_flag                          : 1;
            UINT    entropy_coding_sync_enabled_flag            : 1;
            UINT    pps_loop_filter_across_slices_enabled_flag  : 1;
            UINT    loop_filter_across_tiles_enabled_flag       : 1;
            UINT    pcm_loop_filter_disabled_flag               : 1;
            UINT    field_pic_flag                              : 1;
            UINT    bottom_field_flag                           : 1;
            UINT    NoPicReorderingFlag                         : 1;
            UINT    NoBiPredFlag                                : 1;
            UINT    reserved_bits                               : 9;

        } fields;
    } PicFlags;

            DXVA_Intel_PicEntry_HEVC    CurrPic; 
            INT                         CurrPicOrderCntVal;
            UCHAR                       sps_max_dec_pic_buffering_minus1;    // [0..15]
            DXVA_Intel_PicEntry_HEVC    RefFrameList[15];
            INT                         PicOrderCntValList[15];
            USHORT                      RefFieldPicFlag;
            USHORT                      RefBottomFieldFlag;
            UCHAR                       bit_depth_luma_minus8;
            UCHAR                       bit_depth_chroma_minus8;
            UCHAR                       pcm_sample_bit_depth_luma_minus1;
            UCHAR                       pcm_sample_bit_depth_chroma_minus1;
            UCHAR                       log2_min_luma_coding_block_size_minus3;
            UCHAR                       log2_diff_max_min_luma_coding_block_size;
            UCHAR                       log2_min_transform_block_size_minus2;    
            UCHAR                       log2_diff_max_min_transform_block_size;
            UCHAR                       log2_min_pcm_luma_coding_block_size_minus3;
            UCHAR                       log2_diff_max_min_pcm_luma_coding_block_size;
            UCHAR                       max_transform_hierarchy_depth_intra;    
            UCHAR                       max_transform_hierarchy_depth_inter;    
            CHAR                        init_qp_minus26;
            UCHAR                       diff_cu_qp_delta_depth;
            CHAR                        pps_cb_qp_offset;
            CHAR                        pps_cr_qp_offset;
            UCHAR                       num_tile_columns_minus1;
            UCHAR                       num_tile_rows_minus1;
            USHORT                      column_width_minus1[19];
            USHORT                      row_height_minus1 [21];
            UCHAR                       log2_parallel_merge_level_minus2;
            UINT                        StatusReportFeedbackNumber;

            UCHAR                       continuation_flag;

    union
    {
        UINT        value;
        struct
        {

            UINT    lists_modification_present_flag             : 1;
            UINT    long_term_ref_pics_present_flag             : 1;
            UINT    sps_temporal_mvp_enabled_flag               : 1;
            UINT    cabac_init_present_flag                     : 1;
            UINT    output_flag_present_flag                    : 1;
            UINT    dependent_slice_segments_enabled_flag       : 1;
            UINT    pps_slice_chroma_qp_offsets_present_flag    : 1;
            UINT    sample_adaptive_offset_enabled_flag         : 1;
            UINT    deblocking_filter_override_enabled_flag     : 1;
            UINT    pps_disable_deblocking_filter_flag          : 1;
            UINT    IrapPicFlag                                 : 1;
            UINT    IdrPicFlag                                  : 1;
            UINT    IntraPicFlag                                : 1;
            UINT    slice_segment_header_extension_present_flag : 1;
            UINT    reserved_bits                               : 18;

        } fields;
    } PicShortFormatFlags;

            UCHAR   RefPicSetStCurrBefore[8];
            UCHAR   RefPicSetStCurrAfter[8];
            UCHAR   RefPicSetLtCurr[8];
            UCHAR   log2_max_pic_order_cnt_lsb_minus4;
            UCHAR   num_short_term_ref_pic_sets;
            UCHAR   num_long_term_ref_pics_sps;
            UCHAR   num_ref_idx_l0_default_active_minus1;
            UCHAR   num_ref_idx_l1_default_active_minus1;
            CHAR    pps_beta_offset_div2;
            CHAR    pps_tc_offset_div2;
            USHORT  StRPSBits;
            UCHAR   num_extra_slice_header_bits;
    
} DXVA_Intel_PicParams_HEVC, *LPDXVA_Intel_PicParams_HEVC;

#elif HEVC_SPEC_VER == MK_HEVCVER(0, 85)

typedef struct _DXVA_Intel_PicParams_HEVC
{
            USHORT  PicWidthInMinCbsY;
            USHORT  PicHeightInMinCbsY;

    union
    {
        struct
        {

            USHORT  chroma_format_idc                   : 2;
            USHORT  separate_colour_plane_flag          : 1;
            USHORT  bit_depth_luma_minus8               : 3;     // [0]
            USHORT  bit_depth_chroma_minus8             : 3;     // [0]
            USHORT  log2_max_pic_order_cnt_lsb_minus4   : 4;     // [0..12]
            USHORT  NoPicReorderingFlag                 : 1;
            USHORT  NoBiPredFlag                        : 1;
            USHORT  ReservedBits1                       : 1;

        } fields;

            USHORT  wFormatAndSequenceInfoFlags;

    } PicFlags;

            DXVA_Intel_PicEntry_HEVC    CurrPic; 

            UCHAR   sps_max_dec_pic_buffering_minus1;        // [0..15]
            UCHAR   log2_min_luma_coding_block_size_minus3;     //[0..3]
            UCHAR   log2_diff_max_min_luma_coding_block_size;    //[0..3]
            UCHAR   log2_min_transform_block_size_minus2;        // [0..3]
            UCHAR   log2_diff_max_min_transform_block_size;        // [0..3]
            UCHAR   max_transform_hierarchy_depth_inter;    // [0..4]
            UCHAR   max_transform_hierarchy_depth_intra;    // [0..4]
            UCHAR   num_short_term_ref_pic_sets;
            UCHAR   num_long_term_ref_pics_sps;
            UCHAR   num_ref_idx_l0_default_active_minus1;
            UCHAR   num_ref_idx_l1_default_active_minus1;
            CHAR    init_qp_minus26;                // [-26..25]
            UCHAR   ucNumDeltaPocsOfRefRpsIdx;        // [0]
            USHORT  wNumBitsForShortTermRPSInSlice;
            USHORT  ReservedBits2;

    union
    {
        struct
        {

            UINT32  scaling_list_enabled_flag                       : 1;
            UINT32  amp_enabled_flag                                : 1;
            UINT32  sample_adaptive_offset_enabled_flag             : 1;
            UINT32  pcm_enabled_flag                                : 1;
            UINT32  pcm_sample_bit_depth_luma_minus1                : 4;    // [0..7]
            UINT32  pcm_sample_bit_depth_chroma_minus1              : 4;    // [0..7]
            UINT32  log2_min_pcm_luma_coding_block_size_minus3      : 2;// [0..2]
            UINT32  log2_diff_max_min_pcm_luma_coding_block_size    : 2;// [0..2]
            UINT32  pcm_loop_filter_disabled_flag                   : 1;
            UINT32  long_term_ref_pics_present_flag                 : 1;
            UINT32  sps_temporal_mvp_enabled_flag                   : 1;
            UINT32  strong_intra_smoothing_enabled_flag             : 1;
            UINT32  dependent_slice_segments_enabled_flag           : 1;
            UINT32  output_flag_present_flag                        : 1;
            UINT32  num_extra_slice_header_bits                     : 3;    // [0..7]
            UINT32  sign_data_hiding_flag                           : 1;
            UINT32  cabac_init_present_flag                         : 1;
            UINT32  ReservedBits3                                   : 5;

        } fields;

            UINT32  dwCodingParamToolFlags;
    };

    union
    {
        struct
        {

            UINT32  constrained_intra_pred_flag                 : 1;
            UINT32  transform_skip_enabled_flag                 : 1;
            UINT32  cu_qp_delta_enabled_flag                    : 1;
            UINT32  pps_slice_chroma_qp_offsets_present_flag    : 1;
            UINT32  weighted_pred_flag                          : 1;
            UINT32  weighted_bipred_flag                        : 1;
            UINT32  transquant_bypass_enabled_flag              : 1;
            UINT32  tiles_enabled_flag                          : 1;
            UINT32  entropy_coding_sync_enabled_flag            : 1;
            UINT32  uniform_spacing_flag                        : 1;
            UINT32  loop_filter_across_tiles_enabled_flag       : 1;
            UINT32  pps_loop_filter_across_slices_enabled_flag  : 1;
            UINT32  deblocking_filter_override_enabled_flag     : 1;
            UINT32  pps_deblocking_filter_disabled_flag         : 1;
            UINT32  lists_modification_present_flag             : 1;
            UINT32  slice_segment_header_extension_present_flag : 1;
            UINT32  IrapPicFlag                                 : 1;
            UINT32  IdrPicFlag                                  : 1;
            UINT32  IntraPicFlag                                : 1;
            UINT32  ReservedBits4                               : 13;

        } fields;

            UINT    dwCodingSettingPicturePropertyFlags;

    } PicShortFormatFlags;

            CHAR    pps_cb_qp_offset;
            CHAR    pps_cr_qp_offset;
            UCHAR   num_tile_columns_minus1;
            UCHAR   num_tile_rows_minus1;
            USHORT  column_width_minus1[19];
            USHORT  row_height_minus1[21];
            UCHAR   diff_cu_qp_delta_depth;            // [0..3]
            CHAR    pps_beta_offset_div2;
            CHAR    pps_tc_offset_div2;
            UCHAR   log2_parallel_merge_level_minus2;
            INT32   CurrPicOrderCntVal;

            DXVA_Intel_PicEntry_HEVC    RefFrameList[15];

            UCHAR   ReservedBits5;
            INT32   PicOrderCntValList[15];
            UCHAR   RefPicSetStCurrBefore[8];
            UCHAR   RefPicSetStCurrAfter[8];
            UCHAR   RefPicSetLtCurr[8];
            USHORT  RefFieldPicFlag;
            USHORT  RefBottomFieldFlag;
            UINT32  StatusReportFeedbackNumber;

} DXVA_Intel_PicParams_HEVC, *LPDXVA_Intel_PicParams_HEVC;


/****************************************************************************/


typedef struct _DXVA_Intel_Slice_HEVC_Long
{
            UINT    BSNALunitDataLocation;
            UINT    SliceBytesInBuffer;
            UINT    wBadSliceChopping;
            UINT    ByteOffsetToSliceData;
            UINT    slice_segment_address;

            DXVA_Intel_PicEntry_HEVC    RefPicList[2][15];

    union
    {
            UINT    value;

        struct
        {
            UINT    LastSliceOfPic                                  : 1;
            UINT    dependent_slice_segment_flag                    : 1;
            UINT    slice_type                                      : 2;
            UINT    color_plane_id                                  : 2;
            UINT    slice_sao_luma_flag                             : 1;
            UINT    slice_sao_chroma_flag                           : 1;
            UINT    mvd_l1_zero_flag                                : 1;
            UINT    cabac_init_flag                                 : 1;
            UINT    slice_temporal_mvp_enabled_flag                 : 1;
            UINT    slice_deblocking_filter_disabled_flag           : 1;
            UINT    collocated_from_l0_flag                         : 1;
            UINT    slice_loop_filter_across_slices_enabled_flag    : 1;
            UINT    reserved                                        : 18;
        }
        fields;
    } LongSliceFlags;

            UCHAR   collocated_ref_idx;
            UCHAR   num_ref_idx_l0_active_minus1;
            UCHAR   num_ref_idx_l1_active_minus1;
            CHAR    slice_qp_delta;
            CHAR    slice_cb_qp_offset;
            CHAR    slice_cr_qp_offset;
            CHAR    slice_beta_offset_div2;                // [-6..6]
            CHAR    slice_tc_offset_div2;                    // [-6..6]
            UCHAR   luma_log2_weight_denom;
            UCHAR   delta_chroma_log2_weight_denom;
            CHAR    delta_luma_weight_l0[15];
            CHAR    luma_offset_l0[15];
            CHAR    delta_chroma_weight_l0[15][2];
            CHAR    ChromaOffsetL0[15][2];
            CHAR    delta_luma_weight_l1[15];
            CHAR    luma_offset_l1[15];
            CHAR    delta_chroma_weight_l1[15][2];
            CHAR    ChromaOffsetL1[15][2];
            UCHAR   five_minus_max_num_merge_cand;

} DXVA_Intel_Slice_HEVC_Long, *LPDXVA_Intel_Slice_HEVC_Long;

typedef struct _DXVA_Intel_Slice_HEVC_Short
{
            UINT    BSNALunitDataLocation;
            UINT    SliceBytesInBuffer;
            UINT    wBadSliceChopping;
} DXVA_Intel_Slice_HEVC_Short, *LPDXVA_Intel_Slice_HEVC_Short;

#endif



#if HEVC_SPEC_VER == MK_HEVCVER(0, 83)

typedef struct _DXVA_Intel_Slice_HEVC_Long
{
    UINT        BSNALunitDataLocation;
    UINT        SliceBytesInBuffer;
    UINT        wBadSliceChopping;
    UINT        ByteOffsetToSliceData;
    UINT        slice_segment_address;
    UINT        NumCTUsInSlice;
    DXVA_Intel_PicEntry_HEVC    RefPicList[2][16];

    union
    {
        UINT        value;
        struct
        {
            UINT        LastSliceOfPic                    : 1;
            UINT        dependent_slice_segment_flag        : 1;
            UINT        slice_type                    : 2;
            UINT        color_plane_id                : 2;
            UINT        slice_sao_luma_flag                : 1;
            UINT        slice_sao_chroma_flag            : 1;
            UINT        mvd_l1_zero_flag                : 1;
            UINT        cabac_init_flag                : 1;
            UINT        slice_temporal_mvp_enabled_flag        : 1;
            UINT        slice_deblocking_filter_disabled_flag    : 1;
            UINT        collocated_from_l0_flag            : 1;
            UINT        slice_loop_filter_across_slices_enabled_flag : 1;
            UINT        reserved                    : 18;
        } fields;
    } LongSliceFlags;

    DXVA_Intel_PicEntry_HEVC    CollocatedRefIdx;
    UCHAR        num_ref_idx_l0_active_minus1;
    UCHAR        num_ref_idx_l1_active_minus1;
    CHAR        slice_qp_delta;
    CHAR        slice_cb_qp_offset;
    CHAR        slice_cr_qp_offset;
    CHAR        slice_beta_offset_div2;                // [-6..6]
    CHAR        slice_tc_offset_div2;                    // [-6..6]
    UCHAR        luma_log2_weight_denom;
    UCHAR        delta_chroma_log2_weight_denom;

    CHAR        delta_luma_weight_l0[16];
    CHAR        luma_offset_l0[16];
    CHAR        delta_chroma_weight_l0[16][2];
    CHAR        ChromaOffsetL0[16][2];
    CHAR        delta_luma_weight_l1[16];
    CHAR        luma_offset_l1[16];
    CHAR        delta_chroma_weight_l1[16][2];
    CHAR        ChromaOffsetL1[16][2];

    UCHAR        five_minus_max_num_merge_cand;
    UINT        num_entry_point_offsets;
} DXVA_Intel_Slice_HEVC_Long, *LPDXVA_Intel_Slice_HEVC_Long;

typedef struct _DXVA_Intel_Slice_HEVC_Short
{
    UINT    BSNALunitDataLocation;
    UINT    SliceBytesInBuffer;
    UINT    wBadSliceChopping;

} DXVA_Intel_Slice_HEVC_Short, *LPDXVA_Intel_Slice_HEVC_Short;


#elif HEVC_SPEC_VER == MK_HEVCVER(0, 84)

typedef struct _DXVA_Intel_Slice_HEVC_Long
{
    UINT        BSNALunitDataLocation;
    UINT        SliceBytesInBuffer;
    UINT        wBadSliceChopping;
    UINT        ByteOffsetToSliceData;
    UINT        slice_segment_address;
    UINT        NumCTUsInSlice;

    DXVA_Intel_PicEntry_HEVC    RefPicList[2][15];

    union
    {
        UINT        value;
        struct
        {
            UINT        LastSliceOfPic                                  : 1;
            UINT        dependent_slice_segment_flag                    : 1;
            UINT        slice_type                                      : 2;
            UINT        color_plane_id                                  : 2;
            UINT        slice_sao_luma_flag                             : 1;
            UINT        slice_sao_chroma_flag                           : 1;
            UINT        mvd_l1_zero_flag                                : 1;
            UINT        cabac_init_flag                                 : 1;
            UINT        slice_temporal_mvp_enabled_flag                 : 1;
            UINT        slice_deblocking_filter_disabled_flag           : 1;
            UINT        collocated_from_l0_flag                         : 1;
            UINT        slice_loop_filter_across_slices_enabled_flag    : 1;
            UINT        reserved : 18;
        } fields;
    } LongSliceFlags;

    UCHAR   collocated_ref_idx;
    UCHAR   num_ref_idx_l0_active_minus1;
    UCHAR   num_ref_idx_l1_active_minus1;
    CHAR    slice_qp_delta;
    CHAR    slice_cb_qp_offset;
    CHAR    slice_cr_qp_offset;
    CHAR    slice_beta_offset_div2;                // [-6..6]
    CHAR    slice_tc_offset_div2;                    // [-6..6]
    UCHAR   luma_log2_weight_denom;
    UCHAR   delta_chroma_log2_weight_denom;
    CHAR    delta_luma_weight_l0[15];
    CHAR    luma_offset_l0[15];
    CHAR    delta_chroma_weight_l0[15][2];
    CHAR    ChromaOffsetL0[15][2];
    CHAR    delta_luma_weight_l1[15];
    CHAR    luma_offset_l1[15];
    CHAR    delta_chroma_weight_l1[15][2];
    CHAR    ChromaOffsetL1[15][2];
    UCHAR   five_minus_max_num_merge_cand;
    UINT    num_entry_point_offsets;
} DXVA_Intel_Slice_HEVC_Long, *LPDXVA_Intel_Slice_HEVC_Long;

typedef struct _DXVA_Intel_Slice_HEVC_Short
{
UINT        BSNALunitDataLocation;
UINT        SliceBytesInBuffer;
UINT        wBadSliceChopping;
} DXVA_Intel_Slice_HEVC_Short, *LPDXVA_Intel_Slice_HEVC_Short;

#endif

typedef struct _DXVA_Intel_Qmatrix_HEVC
{
#if HEVC_SPEC_VER == MK_HEVCVER(0, 85)

    UCHAR   ucScalingLists0[6][16]; // 2 inter/intra 3: YUV
    UCHAR   ucScalingLists1[6][64];
    UCHAR   ucScalingLists2[6][64];
    UCHAR   ucScalingLists3[2][64];
    UCHAR   ucScalingListDCCoefSizeID2[6];
    UCHAR   ucScalingListDCCoefSizeID3[2];

#else
    
    UCHAR  ucScalingLists0[3][2][16];
    UCHAR  ucScalingLists1[3][2][64];
    UCHAR  ucScalingLists2[3][2][64];
    UCHAR  ucScalingLists3[2][64];
    UCHAR  bScalingListDC[2][3][2];

#endif
} DXVA_Intel_Qmatrix_HEVC, *LPDXVA_Intel_Qmatrix_HEVC;


#endif __UMC_HEVC_DDI_H
