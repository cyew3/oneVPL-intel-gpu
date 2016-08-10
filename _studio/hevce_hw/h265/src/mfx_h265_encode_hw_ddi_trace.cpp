//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_h265_encode_hw_ddi_trace.h"

namespace MfxHwH265Encode
{

#ifdef DDI_TRACE

DDITracer::DDITracer()
    : m_log(0)
{
    m_log = fopen("MSDK_HEVCE_HW_DDI_LOG.txt", "w");
}

DDITracer::~DDITracer()
{
    if (m_log)
        fclose(m_log);
}

#define TRACE(format, field) fprintf(m_log, FIELD_FORMAT " = " format "\n", #field, b.field); fflush(m_log);
#define TRACE_ARRAY_ROW(format, field, size)\
    fprintf(m_log, FIELD_FORMAT " = { ", #field);\
    for (mfxU32 _i = 0; _i < (mfxU32)(size); _i ++) fprintf(m_log, format " ",b.field[_i]);\
    fprintf(m_log, "}\n"); fflush(m_log);
#define TRACE_HEX_ROW(field, size)\
    fprintf(m_log, FIELD_FORMAT " = %p {", #field, b.field);\
    for (mfxU32 _i = 0; _i < (mfxU32)(size); _i ++) fprintf(m_log, " %02X", ((mfxU8*)b.field)[_i]);\
    fprintf(m_log, " }\n"); fflush(m_log);

#define DECL(type, code)\
void DDITracer::Trace(type const & b, mfxU32 idx)\
{                                                \
    if (!m_log)                                  \
        return;                                  \
    fprintf(m_log, "#" #type "[%d]\n", idx);     \
    fflush(m_log);                               \
    code                                         \
    fprintf(m_log, "\n"); fflush(m_log);         \
}

#define FIELD_FORMAT "%-24s"
DECL(ENCODE_COMPBUFFERDESC,
    if (b.CompressedBufferType == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        TRACE_HEX_ROW(pCompBuffer, b.DataSize);
    }
    else
    {
        TRACE("%p", pCompBuffer);
    }
    TRACE("%d", CompressedBufferType);
    TRACE("%d", DataOffset);
    TRACE("%d", DataSize);

    void* pBuf = (mfxU8*)b.pCompBuffer + b.DataOffset;

    switch((mfxU32)b.CompressedBufferType)
    {
    case D3DDDIFMT_INTELENCODE_SPSDATA:
        TraceArray((ENCODE_SET_SEQUENCE_PARAMETERS_HEVC*)pBuf, (b.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_PPSDATA:
        TraceArray((ENCODE_SET_PICTURE_PARAMETERS_HEVC*)pBuf, (b.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_SLICEDATA:
        TraceArray((ENCODE_SET_SLICE_HEADER_HEVC*)pBuf, (b.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_BITSTREAMDATA:
        break;
    case D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA:
    case D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA:
        TraceArray((ENCODE_PACKEDHEADER_DATA*)pBuf, (b.DataSize / sizeof(ENCODE_PACKEDHEADER_DATA)));
        break;
    default:
        break;
    }
)
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-24s"
DECL(ENCODE_CAPS_HEVC,
    TRACE("%d", CodingLimitSet          );
    TRACE("%d", BitDepth8Only           );
    TRACE("%d", Color420Only            );
    TRACE("%d", SliceStructure          );
    TRACE("%d", SliceIPOnly             );
    TRACE("%d", SliceIPBOnly            );
    TRACE("%d", NoWeightedPred          );
    TRACE("%d", NoMinorMVs              );
    TRACE("%d", RawReconRefToggle       );
    TRACE("%d", NoInterlacedField       );
    TRACE("%d", BRCReset                );
    TRACE("%d", RollingIntraRefresh     );
    TRACE("%d", UserMaxFrameSizeSupport );
    TRACE("%d", FrameLevelRateCtrl      );
    TRACE("%d", SliceByteSizeCtrl       );
    TRACE("%d", VCMBitRateControl       );
    TRACE("%d", ParallelBRC             );
    TRACE("%d", TileSupport             );
    TRACE("%d", SkipFrame               );
    TRACE("%d", MbQpDataSupport         );
    TRACE("%d", SliceLevelWeightedPred  );
    TRACE("%d", LumaWeightedPred        );
    TRACE("%d", ChromaWeightedPred      );
    TRACE("%d", QVBRBRCSupport          ); 
    TRACE("%d", HMEOffsetSupport        );
    TRACE("%d", YUV422ReconSupport      );
    TRACE("%d", YUV444ReconSupport      );
    TRACE("%d", RGBReconSupport         );
    TRACE("%d", MaxEncodedBitDepth      );

    TRACE("%d", MaxPicWidth);
    TRACE("%d", MaxPicHeight);
    TRACE("%d", MaxNum_Reference0);
    TRACE("%d", MaxNum_Reference1);
    TRACE("%d", MBBRCSupport);
    TRACE("%d", TUSupport);

    TRACE("%d", MaxNumOfROI               );
    TRACE("%d", ROIBRCPriorityLevelSupport);

    TRACE("%d", SliceLevelReportSupport   );
    TRACE("%d", NumOfTileColumnsMinus1    );
    TRACE("%d", IntraRefreshBlockUnitSize );
    TRACE("%d", LCUSizeSupported          );

    TRACE("%d", MaxNum_WeightedPredL0);
    TRACE("%d", MaxNum_WeightedPredL1);
)
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC,
    TRACE("%d", wFrameWidthInMinCbMinus1);
    TRACE("%d", wFrameHeightInMinCbMinus1);
    TRACE("%d", general_profile_idc);
    TRACE("%d", general_level_idc);
    TRACE("%d", general_tier_flag);

    TRACE("%d", GopPicSize);
    TRACE("%d", GopRefDist);
    TRACE("%d", GopOptFlag);
    TRACE("%d", TargetUsage);
    TRACE("%d", RateControlMethod);
    TRACE("%d", TargetBitRate);
    TRACE("%d", MaxBitRate);
    TRACE("%d", MinBitRate);
    TRACE("%d", FramesPer100Sec);
    TRACE("%d", InitVBVBufferFullnessInBit);
    TRACE("%d", VBVBufferSizeInBit);

    TRACE("%d", bResetBRC        );
    TRACE("%d", GlobalSearch     );
    TRACE("%d", LocalSearch      );
    TRACE("%d", EarlySkip        );
    TRACE("%d", MBBRC            );
    TRACE("%d", ParallelBRC      );
    TRACE("%d", SliceSizeControl );
    TRACE("%d", ReservedBits     );
    TRACE("%d", UserMaxFrameSize );
    //TRACE("%d", AVBRAccuracy     );
    //TRACE("%d", AVBRConvergence  );
    //TRACE("%d", CRFQualityFactor );

    TRACE_ARRAY_ROW("%d", NumOfBInGop, 3);

    TRACE("%d", scaling_list_enable_flag           );
    TRACE("%d", sps_temporal_mvp_enable_flag       );
    TRACE("%d", strong_intra_smoothing_enable_flag );
    TRACE("%d", amp_enabled_flag                   );
    TRACE("%d", SAO_enabled_flag                   );
    TRACE("%d", pcm_enabled_flag                   );
    TRACE("%d", pcm_loop_filter_disable_flag       );
    TRACE("%d", tiles_fixed_structure_flag         );
    TRACE("%d", chroma_format_idc                  );
    TRACE("%d", separate_colour_plane_flag         );
    TRACE("%d", log2_max_coding_block_size_minus3);
    TRACE("%d", log2_min_coding_block_size_minus3);
    TRACE("%d", log2_max_transform_block_size_minus2);
    TRACE("%d", log2_min_transform_block_size_minus2);
    TRACE("%d", max_transform_hierarchy_depth_intra);
    TRACE("%d", max_transform_hierarchy_depth_inter);
    TRACE("%d", log2_min_PCM_cb_size_minus3);
    TRACE("%d", log2_max_PCM_cb_size_minus3);
    TRACE("%d", bit_depth_luma_minus8);
    TRACE("%d", bit_depth_chroma_minus8);
    TRACE("%d", pcm_sample_bit_depth_luma_minus1);
    TRACE("%d", pcm_sample_bit_depth_chroma_minus1);
)
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL(ENCODE_SET_PICTURE_PARAMETERS_HEVC,
    TRACE("%d", CurrOriginalPic.Index7Bits);
    TRACE("%d", CurrOriginalPic.AssociatedFlag);
    TRACE("%d", CurrReconstructedPic.Index7Bits);
    TRACE("%d", CurrReconstructedPic.AssociatedFlag);
    
    TRACE("%d", CollocatedRefPicIndex);

    for (mfxU32 i = 0; i < 15; i ++)
    {
        TRACE("%d", RefFrameList[i].Index7Bits);
        TRACE("%d", RefFrameList[i].AssociatedFlag);
    }

    TRACE("%d", CurrPicOrderCnt);
    TRACE_ARRAY_ROW("%d", RefFramePOCList, 15);

    TRACE("%d", CodingType);
    TRACE("%d", NumSlices);

    TRACE("%d", tiles_enabled_flag                   );
    TRACE("%d", entropy_coding_sync_enabled_flag     );
    TRACE("%d", sign_data_hiding_flag                );
    TRACE("%d", constrained_intra_pred_flag          );
    TRACE("%d", transform_skip_enabled_flag          );
    TRACE("%d", transquant_bypass_enabled_flag       );
    TRACE("%d", cu_qp_delta_enabled_flag             );
    TRACE("%d", weighted_pred_flag                   );
    TRACE("%d", weighted_bipred_flag                 );
    TRACE("%d", bEnableGPUWeightedPrediction         );
    TRACE("%d", loop_filter_across_slices_flag       );
    TRACE("%d", loop_filter_across_tiles_flag        );
    TRACE("%d", scaling_list_data_present_flag       );
    TRACE("%d", dependent_slice_segments_enabled_flag);
    TRACE("%d", bLastPicInSeq                        );
    TRACE("%d", bLastPicInStream                     );
    TRACE("%d", bUseRawPicForRef                     );
    TRACE("%d", bEmulationByteInsertion              );
    TRACE("%d", BRCPrecision                         );
    //TRACE("%d", bScreenContent                       );
    TRACE("%d", bEnableRollingIntraRefresh           );
    TRACE("%d", no_output_of_prior_pics_flag         );

    TRACE("%d", QpY);
    TRACE("%d", diff_cu_qp_delta_depth);
    TRACE("%d", pps_cb_qp_offset);
    TRACE("%d", pps_cr_qp_offset);
    TRACE("%d", num_tile_columns_minus1);
    TRACE("%d", num_tile_rows_minus1);
    TRACE_ARRAY_ROW("%d", tile_column_width, 19);
    TRACE_ARRAY_ROW("%d", tile_row_height, 21);
    TRACE("%d", log2_parallel_merge_level_minus2);
    TRACE("%d", num_ref_idx_l0_default_active_minus1);
    TRACE("%d", num_ref_idx_l1_default_active_minus1);
    TRACE("%d", LcuMaxBitsizeAllowed);
    TRACE("%d", IntraInsertionLocation);
    TRACE("%d", IntraInsertionSize);
    TRACE("%d", QpDeltaForInsertedIntra);
    TRACE("%d", StatusReportFeedbackNumber);

    TRACE("%d", slice_pic_parameter_set_id);
    TRACE("%d", nal_unit_type);
)
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL(ENCODE_SET_SLICE_HEADER_HEVC,
    TRACE("%d", slice_segment_address);
    TRACE("%d", NumLCUsInSlice);
    TRACE_ARRAY_ROW("%d", RefPicList[0], 15);
    TRACE_ARRAY_ROW("%d", RefPicList[1], 15);
    TRACE("%d", num_ref_idx_l0_active_minus1);
    TRACE("%d", num_ref_idx_l1_active_minus1);
    TRACE("%d", bLastSliceOfPic                     );
    TRACE("%d", dependent_slice_segment_flag        );
    TRACE("%d", slice_temporal_mvp_enable_flag      );
    TRACE("%d", slice_type                          );
    TRACE("%d", slice_sao_luma_flag                 );
    TRACE("%d", slice_sao_chroma_flag               );
    TRACE("%d", mvd_l1_zero_flag                    );
    TRACE("%d", cabac_init_flag                     );
    TRACE("%d", slice_deblocking_filter_disable_flag);
    TRACE("%d", collocated_from_l0_flag             );
    TRACE("%d", slice_qp_delta);
    TRACE("%d", slice_cb_qp_offset);
    TRACE("%d", slice_cr_qp_offset);
    TRACE("%d", beta_offset_div2);
    TRACE("%d", tc_offset_div2);
    TRACE("%d", luma_log2_weight_denom);
    TRACE("%d", chroma_log2_weight_denom);

    //CHAR    luma_offset[2][15];
    //CHAR    delta_luma_weight[2][15];
    //CHAR    chroma_offset[2][15][2];
    //CHAR    delta_chroma_weight[2][15][2];

    TRACE("%d", MaxNumMergeCand);
    TRACE("%d", slice_id);
    //TRACE("%d", MaxSlizeSizeInBytes);
    TRACE("%d", SliceQpDeltaBitOffset);
)
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-24s"
DECL(ENCODE_PACKEDHEADER_DATA,
    TRACE_HEX_ROW(pData, b.BufferSize);
    TRACE("%d", BufferSize);
    TRACE("%d", DataLength);
    TRACE("%d", DataOffset);
    TRACE("%d", SkipEmulationByteCount);
)
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-24s"
DECL(ENCODE_EXECUTE_PARAMS,
    TRACE("%d", NumCompBuffers);
    TRACE("%p", pCompressedBuffers);
    TRACE("%p", pCipherCounter);
    TRACE("%d", PavpEncryptionMode);

    TraceArray(b.pCompressedBuffers, b.NumCompBuffers);
)
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-28s"
DECL(ENCODE_QUERY_STATUS_PARAMS,
    TRACE("%d", StatusReportFeedbackNumber);
    TRACE("%d", CurrOriginalPic); 
    TRACE("%d", field_pic_flag); 
    TRACE("%d", bStatus);
    TRACE("%d", Func);
    TRACE("%d", bitstreamSize);
    TRACE("%d", QpY);
    TRACE("%d", SuggestedQpYDelta);
    TRACE("%d", NumberPasses);
    TRACE("%d", AverageQP);
    TRACE("%d", PanicMode);
    TRACE("%d", MAD);
)
#undef FIELD_FORMAT

#endif //#ifdef DDI_TRACE
}