//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_SVC_DDI_H
#define __UMC_SVC_DDI_H

#include <dxva.h>

#pragma warning(disable: 4201)

#define DXVA_PICTURE_DECODE_BUFFER_SVC               DXVA_PICTURE_DECODE_BUFFER
#define DXVA_SLICE_CONTROL_BUFFER_SVC                DXVA_SLICE_CONTROL_BUFFER

typedef struct _DXVA_PicParams_H264_SVC
{
  USHORT  wFrameWidthInMbsMinus1;
  USHORT  wFrameHeightInMbsMinus1;
  DXVA_PicEntry_H264  CurrPic; /* flag is bot field flag */
  UCHAR   num_ref_frames;

  union {
    struct {
      USHORT  field_pic_flag                 : 1;
      USHORT  MbaffFrameFlag                 : 1;
      USHORT  residual_colour_transform_flag : 1;
      USHORT  sp_for_switch_flag             : 1;
      USHORT  chroma_format_idc              : 2;
      USHORT  RefPicFlag                     : 1;
      USHORT  constrained_intra_pred_flag    : 1;

      USHORT  weighted_pred_flag             : 1;
      USHORT  weighted_bipred_idc            : 2;
      USHORT  MbsConsecutiveFlag             : 1;
      USHORT  frame_mbs_only_flag            : 1;
      USHORT  transform_8x8_mode_flag        : 1;
      USHORT  MinLumaBipredSize8x8Flag       : 1;
      USHORT  IntraPicFlag                   : 1;
    };
    USHORT  wBitFields;
  };
  UCHAR  bit_depth_luma_minus8;
  UCHAR  bit_depth_chroma_minus8;

  USHORT Reserved16Bits;
  UINT   StatusReportFeedbackNumber;

  DXVA_PicEntry_H264  RefFrameList[16]; /* flag LT */
  INT    CurrFieldOrderCnt[2];
  INT    FieldOrderCntList[16][2];

  CHAR   pic_init_qs_minus26;
  CHAR   chroma_qp_index_offset;   /* also used for QScb */
  CHAR   second_chroma_qp_index_offset; /* also for QScr */
  UCHAR  ContinuationFlag;

/* remainder for parsing */
  CHAR   pic_init_qp_minus26;
  UCHAR  num_ref_idx_l0_active_minus1;
  UCHAR  num_ref_idx_l1_active_minus1;
  UCHAR  Reserved8BitsA;

  USHORT FrameNumList[16];
  UINT   UsedForReferenceFlags;
  USHORT NonExistingFrameFlags;
  USHORT frame_num;

  UCHAR  log2_max_frame_num_minus4;
  UCHAR  pic_order_cnt_type;
  UCHAR  log2_max_pic_order_cnt_lsb_minus4;
  UCHAR  delta_pic_order_always_zero_flag;

  UCHAR  direct_8x8_inference_flag;
  UCHAR  entropy_coding_mode_flag;
  UCHAR  pic_order_present_flag;
  UCHAR  num_slice_groups_minus1;

  UCHAR  slice_group_map_type;
  UCHAR  deblocking_filter_control_present_flag;
  UCHAR  redundant_pic_cnt_present_flag;
  UCHAR  Reserved8BitsB;

  USHORT slice_group_change_rate_minus1;

  UCHAR  SliceGroupMap[810]; /* 4b/sgmu, Size BT.601 */

  // end of base DXVA PicParam

    UCHAR       RefBasePicFlag[16];
    union
    {
        struct
        {
            USHORT inter_layer_deblocking_filter_control_present_flag : 1;
            USHORT chroma_phase_x_plus_flag : 1;
            USHORT seq_ref_layer_chroma_phase_x_plus1_flag : 1;
            USHORT adaptive_tcoeff_level_prediction_flag : 1;
            USHORT slice_header_restriction_flag : 1;
            USHORT store_ref_base_pic_flag : 1;
            USHORT ShiftXY16Flag : 1;
            USHORT constrained_intra_resampling_flag : 1;
            USHORT ref_layer_chroma_phase_x_plus1_flag : 1;
            USHORT tcoeff_level_prediction_flag : 1;
            USHORT IdrPicFlag : 1;
            USHORT NextLayerSpatialResolutionChangeFlag : 1;
            USHORT NextLayerMaxTXoeffLevelPredFlag : 1;
        };

        USHORT wBitFields1;
    };

    UCHAR extended_spatial_scalability_idc;
    UCHAR chroma_phase_y_plus1;

    union
    {
        struct
        {
            SHORT seq_scaled_ref_layer_left_offset;
            SHORT seq_scaled_ref_layer_top_offset;
            SHORT seq_scaled_ref_layer_right_offset;
            SHORT seq_scaled_ref_layer_bottom_offset;
        };

        struct
        {
            SHORT scaled_ref_layer_left_offset;
            SHORT scaled_ref_layer_top_offset;
            SHORT scaled_ref_layer_right_offset;
            SHORT scaled_ref_layer_bottom_offset;
        };
    };

    UCHAR seq_ref_layer_chroma_phase_y_plus1;

    UCHAR LayerType;
    UCHAR dependency_id;
    UCHAR quality_id;
    USHORT ref_layer_dq_id;
    UCHAR disable_inter_layer_deblocking_filter_idc;
    CHAR inter_layer_slice_alpha_c0_offset_div2;
    CHAR inter_layer_slice_beta_offset_div2;
    UCHAR ref_layer_chroma_phase_y_plus1;

    SHORT NextLayerScaledRefLayerLeftOffset;
    SHORT NextLayerScaledRefLayerRightOffset;
    SHORT NextLayerScaledRefLayerTopOffset;
    SHORT NextLayerScaledRefLayerBottomOffset;

    USHORT NextLayerPicWidthInMbs;
    USHORT NextLayerPicHeightInMbs;
    UCHAR  NextLayerDisableInterLayerDeblockingFilterIdc;
    UCHAR  NextLayerInterLayerSliceAlphaC0OffsetDiv2;
    UCHAR  NextLayerInterLayerSliceBetaOffsetDiv2;

    UCHAR  DeblockingFilterMode;

} DXVA_PicParams_H264_SVC;

typedef struct _DXVA_Slice_H264_SVC_Long
{
  UINT   BSNALunitDataLocation; /* type 1..5 */
  UINT   SliceBytesInBuffer; /* for off-host parse */
  USHORT wBadSliceChopping;  /* for off-host parse */

  USHORT first_mb_in_slice;
  USHORT NumMbsForSlice;

  USHORT BitOffsetToSliceData; /* after CABAC alignment */

  UCHAR  slice_type;
  UCHAR  luma_log2_weight_denom;
  UCHAR  chroma_log2_weight_denom;
  UCHAR  num_ref_idx_l0_active_minus1;
  UCHAR  num_ref_idx_l1_active_minus1;
  CHAR   slice_alpha_c0_offset_div2;
  CHAR   slice_beta_offset_div2;
  UCHAR  Reserved8Bits;
  DXVA_PicEntry_H264 RefPicList[2][32]; /* L0 & L1 */
  SHORT  Weights[2][32][3][2]; /* L0 & L1; Y, Cb, Cr */
  CHAR   slice_qs_delta;
                               /* rest off-host parse */
  CHAR   slice_qp_delta;
  UCHAR  redundant_pic_cnt;
  UCHAR  direct_spatial_mv_pred_flag;
  UCHAR  cabac_init_idc;
  UCHAR  disable_deblocking_filter_idc;
  USHORT slice_id;

  // end of base DXVA Slice

    union
    {
        struct
        {
            USHORT no_inter_layer_pred_flag : 1;
            USHORT base_pred_weight_table_flag : 1;
            USHORT slice_skip_flag : 1;
            USHORT adaptive_base_mode_flag : 1;
            USHORT default_base_mode_flag : 1;
            USHORT adaptive_motion_prediction_flag : 1;
            USHORT default_motion_prediction_flag : 1;
            USHORT adaptive_residual_prediction_flag : 1;
            USHORT default_residual_prediction_flag : 1;
        };

        USHORT wBitFields;
    };

    USHORT num_mbs_in_slice_minus1;
    UCHAR scan_idx_start;
    UCHAR scan_idx_end;

} DXVA_Slice_H264_SVC_Long;

typedef struct _DXVA_Slice_H264_SVC_Short
{
  UINT   BSNALunitDataLocation; /* type 1..5 */
  UINT   SliceBytesInBuffer; /* for off-host parse */
  USHORT wBadSliceChopping;  /* for off-host parse */

  // end of base DXVA Slice

    union
    {
        struct
        {
            USHORT no_inter_layer_pred_flag : 1;
        };

        USHORT wBitFields;
    };

} DXVA_Slice_H264_SVC_Short;

#endif // __UMC_SVC_DDI_H
