//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_encode_hw_ddi_trace.h"

namespace MfxHwH265Encode
{

#ifdef DDI_TRACE

DDITracer::DDITracer(ENCODER_TYPE type)
    : m_log(0)
    , m_type(type)
{
    m_log = fopen("MSDK_HEVCE_HW_DDI_LOG.txt", "w");
    Trace("HEVCE_DDI_VERSION", HEVCE_DDI_VERSION);
}

DDITracer::~DDITracer()
{
    if (m_log)
        fclose(m_log);
}

void DDITracer::Trace(const char* name, mfxU32 value)
{
    fprintf(m_log, "%s = %d\n\n", name, value);
    fflush(m_log);
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

#define DECL_START(type)\
void DDITracer::Trace(type const & b, mfxU32 idx)\
{                                                \
    if (!m_log)                                  \
        return;                                  \
    fprintf(m_log, "#" #type "[%d]\n", idx);     \
    fflush(m_log);

#define DECL_END \
    fprintf(m_log, "\n"); fflush(m_log); \
}

void DDITracer::TraceGUID(GUID const & guid, FILE* f)
{
    fprintf(f, "GUID = { %08X, %04X, %04X, { %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X } }\n"
        , guid.Data1, guid.Data2, guid.Data3
        , guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3]
        , guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    fflush(f);
}


#define FIELD_FORMAT "%-24s"
DECL_START(D3D11_VIDEO_DECODER_EXTENSION)
    TRACE("%d", Function);
    TRACE("%d", PrivateInputDataSize);
    TRACE_HEX_ROW(pPrivateInputData, b.PrivateInputDataSize);
    TRACE("%d", PrivateOutputDataSize);
    TRACE_HEX_ROW(pPrivateOutputData, b.PrivateOutputDataSize);
    TRACE("%d", ResourceCount);
DECL_END
#undef FIELD_FORMAT


#define FIELD_FORMAT "%-24s"
DECL_START(ENCODE_COMPBUFFERDESC)
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
    switch ((mfxU32)b.CompressedBufferType)
    {
    case D3DDDIFMT_INTELENCODE_SPSDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA:
#if (HEVCE_DDI_VERSION >= 960)
        if (m_type == ENCODER_REXT)
            TraceArray((ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT)));
        else
#endif //(HEVCE_DDI_VERSION >= 960)
            TraceArray((ENCODE_SET_SEQUENCE_PARAMETERS_HEVC*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_PPSDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA:
#if (HEVCE_DDI_VERSION >= 960)
        if (m_type == ENCODER_REXT)
            TraceArray((ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT)));
        else
#endif //(HEVCE_DDI_VERSION >= 960)
            TraceArray((ENCODE_SET_PICTURE_PARAMETERS_HEVC*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_SLICEDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA:
#if (HEVCE_DDI_VERSION >= 960)
        if (m_type == ENCODER_REXT)
            TraceArray((ENCODE_SET_SLICE_HEADER_HEVC_REXT*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_HEVC_REXT)));
        else
#endif //(HEVCE_DDI_VERSION >= 960)
            TraceArray((ENCODE_SET_SLICE_HEADER_HEVC*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_BITSTREAMDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA:
        break;
    case D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA:
    case D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA:
        TraceArray((ENCODE_PACKEDHEADER_DATA*)pBuf,
                (b.DataSize / sizeof(ENCODE_PACKEDHEADER_DATA)));
        break;
    default:
        break;
    }
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-32s"
DECL_START(ENCODE_CAPS_HEVC)
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
    TRACE("%d", ROIDeltaQPSupport         );
    TRACE("%d", ROIBRCPriorityLevelSupport);

#if (HEVCE_DDI_VERSION >= 966)
    TRACE("%d", BlockSize);
#else
    TRACE("%d", ROIBlockSize);
#endif

    TRACE("%d", SliceLevelReportSupport      );
    TRACE("%d", MaxNumOfTileColumnsMinus1    );
    TRACE("%d", IntraRefreshBlockUnitSize    );
    TRACE("%d", LCUSizeSupported             );
    TRACE("%d", MaxNumDeltaQP                );
    TRACE("%d", DirtyRectSupport             );
    TRACE("%d", MoveRectSupport              );
    TRACE("%d", FrameSizeToleranceSupport    );
    TRACE("%d", HWCounterAutoIncrementSupport);

    TRACE("%d", MaxNum_WeightedPredL0);
    TRACE("%d", MaxNum_WeightedPredL1);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC)
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
#if (HEVCE_DDI_VERSION >= 966)
    TRACE("%d", FrameRate.Numerator);
    TRACE("%d", FrameRate.Denominator);
#else
    TRACE("%d", FramesPer100Sec);
#endif
    TRACE("%d", InitVBVBufferFullnessInBit);
    TRACE("%d", VBVBufferSizeInBit);

    TRACE("%d", bResetBRC        );
    TRACE("%d", GlobalSearch     );
    TRACE("%d", LocalSearch      );
    TRACE("%d", EarlySkip        );
    TRACE("%d", MBBRC            );
    TRACE("%d", ParallelBRC      );
    TRACE("%d", SliceSizeControl );
    TRACE("%d", SourceFormat         );
    TRACE("%d", SourceBitDepth       );
    TRACE("%d", QpAdjustment         );
    TRACE("%d", ROIValueInDeltaQP    );

#if (HEVCE_DDI_VERSION >= 966)
    TRACE("%d", BlockQPforNonRectROI);
#else
    TRACE("%d", BlockQPInDeltaQPIndex);
#endif

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
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_PICTURE_PARAMETERS_HEVC)
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

    TRACE("%d", tiles_enabled_flag                      );
    TRACE("%d", entropy_coding_sync_enabled_flag        );
    TRACE("%d", sign_data_hiding_flag                   );
    TRACE("%d", constrained_intra_pred_flag             );
    TRACE("%d", transform_skip_enabled_flag             );
    TRACE("%d", transquant_bypass_enabled_flag          );
    TRACE("%d", cu_qp_delta_enabled_flag                );
    TRACE("%d", weighted_pred_flag                      );
    TRACE("%d", weighted_bipred_flag                    );
    TRACE("%d", loop_filter_across_slices_flag          );
    TRACE("%d", loop_filter_across_tiles_flag           );
    TRACE("%d", scaling_list_data_present_flag          );
    TRACE("%d", dependent_slice_segments_enabled_flag   );
    TRACE("%d", bLastPicInSeq                           );
    TRACE("%d", bLastPicInStream                        );
    TRACE("%d", bUseRawPicForRef                        );
    TRACE("%d", bEmulationByteInsertion                 );
    TRACE("%d", BRCPrecision                            );
    TRACE("%d", bEnableSliceLevelReport                 );
    TRACE("%d", bEnableRollingIntraRefresh              );
    TRACE("%d", no_output_of_prior_pics_flag            );
    TRACE("%d", bEnableGPUWeightedPrediction            );
    TRACE("%d", DisplayFormatSwizzle                    );

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
    TRACE("%d", MaxSliceSizeInBytes);

    TRACE("%d", NumROI);
    for (mfxU32 i = 0; i < b.NumROI; i ++)
    {
        TRACE("%d", ROI[i].Roi.Top);
        TRACE("%d", ROI[i].Roi.Bottom);
        TRACE("%d", ROI[i].Roi.Left);
        TRACE("%d", ROI[i].Roi.Right);
        TRACE("%d", ROI[i].PriorityLevelOrDQp);
    }
    
    TRACE("%d", MaxDeltaQp);
    TRACE("%d", MinDeltaQp);

#if (HEVCE_DDI_VERSION >= 960)
    TRACE("%d", NumDeltaQpForNonRectROI);
    TRACE_ARRAY_ROW("%d", NonRectROIDeltaQpList, 16);
#endif
    
    TRACE("%d", SkipFrameFlag);
    TRACE("%d", NumSkipFrames);
    TRACE("%d", SizeSkipFrames);
    
    TRACE("%d", BRCMaxQp);
    TRACE("%d", BRCMinQp);

    TRACE("%d", bEnableHMEOffset);
    if (b.bEnableHMEOffset)
    {
        TRACE_ARRAY_ROW("%d", HMEOffset[ 0], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 1], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 2], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 3], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 4], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 5], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 6], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 7], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 8], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[ 9], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[10], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[11], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[12], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[13], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[14], 2);
        TRACE_ARRAY_ROW("%d", HMEOffset[15], 2);
    }

    TRACE("%d", NumDirtyRects);
    if (b.NumDirtyRects)
    {
        for (mfxU32 i = 0; i < b.NumDirtyRects; i ++)
        {
            TRACE("%d", pDirtyRect[i].Top);
            TRACE("%d", pDirtyRect[i].Bottom);
            TRACE("%d", pDirtyRect[i].Left);
            TRACE("%d", pDirtyRect[i].Right);
        }
    }
    
    TRACE("%d", NumMoveRects);
    if (b.NumMoveRects)
    {
        for (mfxU32 i = 0; i < b.NumMoveRects; i ++)
        {
            TRACE("%d", pMoveRect[i].SourcePointX);
            TRACE("%d", pMoveRect[i].SourcePointY);
            TRACE("%d", pMoveRect[i].DestTop);
            TRACE("%d", pMoveRect[i].DestBot);
            TRACE("%d", pMoveRect[i].DestLeft);
            TRACE("%d", pMoveRect[i].DestRight);
        }
    }

#if (HEVCE_DDI_VERSION >= 960)
    TRACE("%d", InputType);
#endif
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_SLICE_HEADER_HEVC)
    TRACE("%d", slice_segment_address);
    TRACE("%d", NumLCUsInSlice);
#pragma warning(disable:4477)
    TRACE_ARRAY_ROW("%d", RefPicList[0], 15);
    TRACE_ARRAY_ROW("%d", RefPicList[1], 15);
#pragma warning(default:4477)
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
#if (HEVCE_DDI_VERSION >= 966)
    TRACE("%d", delta_chroma_log2_weight_denom);
#else
    TRACE("%d", chroma_log2_weight_denom);
#endif

    TRACE_ARRAY_ROW("%d", luma_offset[0], 15);
    TRACE_ARRAY_ROW("%d", luma_offset[1], 15);
    TRACE_ARRAY_ROW("%d", delta_luma_weight[0], 15);
    TRACE_ARRAY_ROW("%d", delta_luma_weight[1], 15);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][0], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][1], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][2], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][3], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][4], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][5], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][6], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][7], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][8], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][9], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][10], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][11], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][12], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][13], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[0][14], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][0], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][1], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][2], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][3], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][4], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][5], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][6], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][7], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][8], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][9], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][10], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][11], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][12], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][13], 2);
    TRACE_ARRAY_ROW("%d", chroma_offset[1][14], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][0], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][1], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][2], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][3], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][4], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][5], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][6], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][7], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][8], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][9], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][10], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][11], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][12], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][13], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[0][14], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][0], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][1], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][2], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][3], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][4], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][5], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][6], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][7], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][8], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][9], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][10], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][11], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][12], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][13], 2);
    TRACE_ARRAY_ROW("%d", delta_chroma_weight[1][14], 2);

    TRACE("%d", MaxNumMergeCand);
    TRACE("%d", slice_id);
    //TRACE("%d", MaxSlizeSizeInBytes);
    TRACE("%d", SliceQpDeltaBitOffset);

#if (HEVCE_DDI_VERSION >= 966)
    TRACE("%d", SliceSAOFlagBitOffset);
    TRACE("%d", BitLengthSliceHeaderStartingPortion);
    TRACE("%d", SliceHeaderByteOffset);
    TRACE("%d", PredWeightTableBitOffset);
    TRACE("%d", PredWeightTableBitLength);
#endif
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-24s"
DECL_START(ENCODE_PACKEDHEADER_DATA)
    TRACE_HEX_ROW(pData, b.BufferSize);
    TRACE("%d", BufferSize);
    TRACE("%d", DataLength);
    TRACE("%d", DataOffset);
    TRACE("%d", SkipEmulationByteCount);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-24s"
DECL_START(ENCODE_EXECUTE_PARAMS)
    TRACE("%d", NumCompBuffers);
    TRACE("%p", pCompressedBuffers);
    TRACE("%p", pCipherCounter);
    TRACE("%d", PavpEncryptionMode.eEncryptionType);
    TRACE("%d", PavpEncryptionMode.eCounterMode);

    TraceArray(b.pCompressedBuffers, b.NumCompBuffers);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-28s"
DECL_START(ENCODE_QUERY_STATUS_PARAMS)
    TRACE("%d", StatusReportFeedbackNumber);
    TRACE("%d", CurrOriginalPic.bPicEntry); 
    TRACE("%d", field_pic_flag); 
    TRACE("%d", bStatus);
    TRACE("%d", Func);
    TRACE("%d", bitstreamSize);
    TRACE("%d", QpY);
    TRACE("%d", SuggestedQpYDelta);
    TRACE("%d", NumberPasses);
    TRACE("%d", AverageQP);
    TRACE("%d", PanicMode);
    TRACE("%d", SliceSizeOverflow);
    TRACE("%d", NumSlicesNonCompliant);
    TRACE("%d", LongTermReference);
    TRACE("%d", FrameSkipped);
    TRACE("%d", MAD);
    TRACE("%d", NumberSlices);
    TRACE("%llx", aes_counter.IV);
    TRACE("%llx", aes_counter.Counter);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-34s"
DECL_START(ENCODE_QUERY_STATUS_SLICE_PARAMS)
    Trace(*((ENCODE_QUERY_STATUS_PARAMS*)&b), 0);
    TRACE("%d", SizeOfSliceSizesBuffer);
    TRACE("%p", SliceSizes);
    if (b.SliceSizes) {
        TRACE_ARRAY_ROW("%d", SliceSizes, b.SizeOfSliceSizesBuffer);
    }
DECL_END
#undef FIELD_FORMAT


#if (HEVCE_DDI_VERSION >= 960)

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT)
    Trace((ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const &) b, idx);
    TRACE("%d", transform_skip_rotation_enabled_flag   );
    TRACE("%d", transform_skip_context_enabled_flag    );
    TRACE("%d", implicit_rdpcm_enabled_flag            );
    TRACE("%d", explicit_rdpcm_enabled_flag            );
    TRACE("%d", extended_precision_processing_flag     );
    TRACE("%d", intra_smoothing_disabled_flag          );
    TRACE("%d", high_precision_offsets_enabled_flag    );
    TRACE("%d", persistent_rice_adaptation_enabled_flag);
    TRACE("%d", cabac_bypass_alignment_enabled_flag    );
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT)
    Trace((ENCODE_SET_PICTURE_PARAMETERS_HEVC const &)b, idx);
    TRACE("%d", cross_component_prediction_enabled_flag  );
    TRACE("%d", chroma_qp_offset_list_enabled_flag       );
    TRACE("%d", diff_cu_chroma_qp_offset_depth           );
    TRACE("%d", chroma_qp_offset_list_len_minus1         );
    TRACE("%d", log2_sao_offset_scale_luma               );
    TRACE("%d", log2_sao_offset_scale_chroma             );
    TRACE("%d", log2_max_transform_skip_block_size_minus2);
    TRACE_ARRAY_ROW("%d", cb_qp_offset_list, 6);
    TRACE_ARRAY_ROW("%d", cr_qp_offset_list, 6);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_SLICE_HEADER_HEVC_REXT)
    Trace((ENCODE_SET_SLICE_HEADER_HEVC const &)b, idx);
    TRACE_ARRAY_ROW("%d", luma_offset_l0, 15);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[0], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[1], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[2], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[3], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[4], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[5], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[6], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[7], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[8], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[9], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[0], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[11], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[12], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[13], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL0[14], 2);
    TRACE_ARRAY_ROW("%d", luma_offset_l1, 15);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[0], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[1], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[2], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[3], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[4], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[5], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[6], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[7], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[8], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[9], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[0], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[11], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[12], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[13], 2);
    TRACE_ARRAY_ROW("%d", ChromaOffsetL1[14], 2);
DECL_END
#undef FIELD_FORMAT

#endif //(HEVCE_DDI_VERSION >= 960)

#endif //#ifdef DDI_TRACE
}
#endif
