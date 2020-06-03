// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_ddi_trace.h"

namespace HEVCEHW
{

#ifdef DDI_TRACE

DDITracer::DDITracer(eMFXVAType va)
    : m_log(0)
    , m_va(va)
{
    m_log = fopen("MSDK_HEVCE_HW_DDI_LOG.txt", "w");
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
#define TRACE_PTR(field) fprintf(m_log, FIELD_FORMAT " = %s\n", #field, b.field ? "& {}" : "NULL"); fflush(m_log);
#define TRACE_ARRAY_ROW(format, field, size)\
    fprintf(m_log, FIELD_FORMAT " = { ", #field);\
    for (mfxU32 _i = 0; _i < (mfxU32)(size); _i ++) fprintf(m_log, format " ",b.field[_i]);\
    fprintf(m_log, "}\n"); fflush(m_log);
#define TRACE_HEX_ROW(field, size)\
    fprintf(m_log, FIELD_FORMAT " = %s {", #field, b.field ? "&" : "NULL");\
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
    fprintf(f, "GUID = { %08lX, %04X, %04X, { %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X } }\n"
        , guid.Data1, guid.Data2, guid.Data3
        , guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3]
        , guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    fflush(f);
}

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
    TRACE("%d", NegativeQPSupport       );
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

    TRACE("%d", BlockSize);

    TRACE("%d", SliceLevelReportSupport      );

    TRACE("%d", CTULevelReportSupport        );
    TRACE("%d", SearchWindow64Support        );
    TRACE("%d", CustomRoundingControl        );
    TRACE("%d", IntraRefreshBlockUnitSize    );
    TRACE("%d", LCUSizeSupported             );
    TRACE("%d", MaxNumDeltaQPMinus1          );
    TRACE("%d", DirtyRectSupport             );
    TRACE("%d", MoveRectSupport              );
    TRACE("%d", FrameSizeToleranceSupport    );
    TRACE("%d", HWCounterAutoIncrementSupport);
    TRACE("%d", NumScalablePipesMinus1       );
    TRACE("%d", NegativeQPSupport            );
    TRACE("%d", TileBasedEncodingSupport     );
    TRACE("%d", PartialFrameUpdateSupport    );
    TRACE("%d", RGBEncodingSupport           );
    TRACE("%d", LLCStreamingBufferSupport    );
    TRACE("%d", DDRStreamingBufferSupport    );
    TRACE("%d", MaxNum_WeightedPredL0         );
    TRACE("%d", MaxNum_WeightedPredL1         );
    TRACE("%d", MaxNumOfDirtyRect             );
    TRACE("%d", MaxNumOfMoveRect              );
    TRACE("%d", MaxNumOfConcurrentFramesMinus1);
    TRACE("%d", LLCSizeInMBytes               );
    TRACE("%d", PFrameSupport                 );
    TRACE("%d", LookaheadAnalysisSupport      );
    TRACE("%d", LookaheadBRCSupport           );
    DECL_END
#undef FIELD_FORMAT


#if !defined(MFX_VA_LINUX)

#define FIELD_FORMAT "%s"
void DDITracer::TraceFunc(bool bOut, mfxU32 func, const void* buf, mfxU32 n)
{
    bool bDX9 = m_va == MFX_HW_D3D9;
    bool bHEX = buf && n;
    bool bGUID= bHEX && !bOut && (bDX9
        && (func == ENCODE_FORMAT_COUNT_ID
            || func == AUXDEV_QUERY_ACCEL_CAPS
            || func == AUXDEV_CREATE_ACCEL_SERVICE));
    bool bENCODE_EXECUTE_PARAMS             = bHEX && !bOut && func == ENCODE_ENC_PAK_ID;
    bool bENCODE_FORMAT_COUNT               = bHEX && bOut && (func == ENCODE_FORMAT_COUNT_ID);
    bool bENCODE_FORMATS                    = bHEX && bOut && (func == ENCODE_FORMATS_ID);
    bool bENCODE_CREATEDEVICE               = bHEX && bOut && (bDX9 && func == AUXDEV_CREATE_ACCEL_SERVICE);
    bool bENCODE_CAPS_HEVC                  = bHEX && bOut && (func == AUXDEV_QUERY_ACCEL_CAPS || func == ENCODE_QUERY_ACCEL_CAPS_ID);
    bool bENCODE_QUERY_STATUS_PARAMS_DESCR  = bHEX && !bOut && func == ENCODE_QUERY_STATUS_ID;
    bool bENCODE_QUERY_STATUS_PARAMS_HEVC   = bHEX && bOut && func == ENCODE_QUERY_STATUS_ID;
    bool bENCODE_ENCRYPTION_SET             = bHEX && !bOut && !bDX9 && func == ENCODE_ENCRYPTION_SET_ID;
    struct { const void *In, *Out; } b = { buf, buf };
    
    if (!bOut)
        Trace(">>Function", func);

#define TRACE_FARG(TYPE)\
    if (b##TYPE)                                     \
    {                                                \
        TraceArray((const TYPE*)buf, n / sizeof(TYPE));\
        bHEX = false;                                \
    }

    TRACE_FARG(GUID);
    TRACE_FARG(ENCODE_EXECUTE_PARAMS);
    TRACE_FARG(ENCODE_FORMAT_COUNT);
    TRACE_FARG(ENCODE_FORMATS);
    TRACE_FARG(ENCODE_CREATEDEVICE);
    TRACE_FARG(ENCODE_CAPS_HEVC);
    TRACE_FARG(ENCODE_QUERY_STATUS_PARAMS_DESCR);
    TRACE_FARG(ENCODE_QUERY_STATUS_PARAMS_HEVC);
    TRACE_FARG(ENCODE_ENCRYPTION_SET);
#undef TRACE_FARG
    
    bool bHEXIn  = bHEX && !bOut;
    bool bHEXOut = bHEX && bOut;

    if (bHEXIn)
    {
        TRACE_HEX_ROW(In, n);
    }

    if (bHEXOut)
    {
        TRACE_HEX_ROW(Out, n);
    }

    if (bOut)
    {
        Trace("<<Function", func);
    }
}
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-26s"
DECL_START(ENCODE_ENCRYPTION_SET)
    TRACE("%d", CounterAutoIncrement);
    TRACE_PTR(pInitialCipherCounter);
    if (b.pInitialCipherCounter)
    {
        TRACE("%lld", pInitialCipherCounter->Count);
        TRACE("%lld", pInitialCipherCounter->IV);
    }
    TRACE_PTR(pPavpEncryptionMode);
    if (b.pPavpEncryptionMode)
    {
        TRACE("%d", pPavpEncryptionMode->eEncryptionType);
        TRACE("%d", pPavpEncryptionMode->eCounterMode);
    }
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-26s"
DECL_START(ENCODE_QUERY_STATUS_PARAMS_DESCR)
    TRACE("%d", StatusParamType);
    TRACE("%d", SizeOfStatusParamStruct);
    TRACE("%d", StreamID);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-26s"
DECL_START(DXVADDI_VIDEODESC)
    TRACE("%d", SampleWidth);
    TRACE("%d", SampleHeight);
    TRACE("%d", SampleFormat.value);
    TRACE("%d", Format);
    TRACE("%d", InputSampleFreq.Numerator);
    TRACE("%d", InputSampleFreq.Denominator);
    TRACE("%d", OutputFrameFreq.Numerator);
    TRACE("%d", OutputFrameFreq.Denominator);
    TRACE("%d", UABProtectionLevel);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-26s"
DECL_START(ENCODE_CREATEDEVICE)
    TRACE_PTR(pVideoDesc);
    if (b.pVideoDesc)
    {
        Trace(*b.pVideoDesc, 0);
    }
    TRACE("%d", CodingFunction);
    TraceGUID(b.EncryptionMode, m_log);
    TRACE("%d", CounterAutoIncrement);
    TRACE_PTR(pInitialCipherCounter);
    if (b.pInitialCipherCounter)
    {
        TRACE("%lld", pInitialCipherCounter->Count);
        TRACE("%lld", pInitialCipherCounter->IV);
    }
    TRACE_PTR(pPavpEncryptionMode);
    if (b.pPavpEncryptionMode)
    {
        TRACE("%d", pPavpEncryptionMode->eEncryptionType);
        TRACE("%d", pPavpEncryptionMode->eCounterMode);
    }
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-26s"
DECL_START(ENCODE_FORMAT_COUNT)
    TRACE("%d", UncompressedFormatCount);
    TRACE("%d", CompressedBufferInfoCount);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-26s"
DECL_START(ENCODE_FORMATS)
    TRACE("%d", UncompressedFormatSize);
    TRACE("%d", CompressedBufferInfoSize);
    TRACE_ARRAY_ROW("%i ", pUncompressedFormats, b.UncompressedFormatSize / sizeof(D3DDDIFORMAT));
    TraceArray(b.pCompressedBufferInfo, b.CompressedBufferInfoSize / sizeof(ENCODE_COMP_BUFFER_INFO));
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-18s"
DECL_START(ENCODE_COMP_BUFFER_INFO)
    TRACE("%d", Type              );
    TRACE("%d", CompressedFormats );
    TRACE("%d", CreationWidth     );
    TRACE("%d", CreationHeight    );
    TRACE("%d", NumBuffer         );
    TRACE("%d", AllocationType    );
DECL_END
#undef FIELD_FORMAT


#define FIELD_FORMAT "%-24s"
DECL_START(D3D11_VIDEO_DECODER_EXTENSION)
    TRACE("%d", Function);
    TRACE("%d", PrivateInputDataSize);
    TRACE_HEX_ROW(pPrivateInputData, b.PrivateInputDataSize);
    TRACE("%d", PrivateOutputDataSize);
    TRACE_HEX_ROW(pPrivateOutputData, b.PrivateOutputDataSize);
    TRACE("%d", ResourceCount);
    if (b.Function == ENCODE_ENC_PAK_ID)
    {
        Trace(*((ENCODE_EXECUTE_PARAMS*)b.pPrivateInputData), 0);
    } else
    if (b.Function == ENCODE_QUERY_STATUS_ID)
    {
        if (b.PrivateOutputDataSize == sizeof(ENCODE_QUERY_STATUS_SLICE_PARAMS_HEVC))
            Trace(*((ENCODE_QUERY_STATUS_SLICE_PARAMS_HEVC*)b.pPrivateOutputData), 0);
        else
            Trace(*((ENCODE_QUERY_STATUS_PARAMS_HEVC*)b.pPrivateOutputData), 0);
    }
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
        TRACE_PTR(pCompBuffer);
    }
    TRACE("%d", CompressedBufferType);
    TRACE("%d", DataOffset);
    TRACE("%d", DataSize);

    void* pBuf = (mfxU8*)b.pCompBuffer + b.DataOffset;
    switch ((mfxU32)b.CompressedBufferType)
    {
    case D3DDDIFMT_INTELENCODE_SPSDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA:
        TraceArray((ENCODE_SET_SEQUENCE_PARAMETERS_HEVC*)pBuf,
            (b.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_PPSDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA:
        TraceArray((ENCODE_SET_PICTURE_PARAMETERS_HEVC*)pBuf,
            (b.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_HEVC)));
        break;
    case D3DDDIFMT_INTELENCODE_SLICEDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA:
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
    TRACE("%d", FrameRate.Numerator);
    TRACE("%d", FrameRate.Denominator);

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

    TRACE("%d", BlockQPforNonRectROI);
    TRACE("%d", EnableTileBasedEncode);
    TRACE("%d", bAutoMaxPBFrameSizeForSceneChange);
    TRACE("%d", EnableStreamingBufferLLC);
    TRACE("%d", EnableStreamingBufferDDR);
    TRACE("%d", LowDelayMode);
    TRACE("%d", DisableHRDConformance);
    TRACE("%d", HierarchicalFlag);

    TRACE("%d", UserMaxIFrameSize );
    TRACE("%d", UserMaxPBFrameSize);
    TRACE("%d", ICQQualityFactor);
    TRACE("%d", StreamBufferSessionID);

    TRACE_ARRAY_ROW("%d", NumOfBInGop, 3);

    TRACE("%d", scaling_list_enable_flag);
    TRACE("%d", sps_temporal_mvp_enable_flag);
    TRACE("%d", strong_intra_smoothing_enable_flag);
    TRACE("%d", amp_enabled_flag);
    TRACE("%d", SAO_enabled_flag);
    TRACE("%d", pcm_enabled_flag);
    TRACE("%d", pcm_loop_filter_disable_flag);
    TRACE("%d", chroma_format_idc);
    TRACE("%d", separate_colour_plane_flag);
    TRACE("%d", palette_mode_enabled_flag);
    TRACE("%d", RGBEncodingEnable);
    TRACE("%d", PrimaryChannelForRGBEncoding);
    TRACE("%d", SecondaryChannelForRGBEncoding);
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

    TRACE("%d", InputColorSpace);
    TRACE("%d", ScenarioInfo);
    TRACE("%d", ContentInfo);
    TRACE("%d", FrameSizeTolerance);

    TRACE("%d", SlidingWindowSize);
    TRACE("%d", MaxBitRatePerSlidingWindow);
    TRACE("%d", MinBitRatePerSlidingWindow);
    TRACE("%d", LookaheadDepth);

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
    TRACE("%d", HierarchLevelPlus1);
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
    TRACE("%d", deblocking_filter_override_enabled_flag );
    TRACE("%d", pps_deblocking_filter_disabled_flag     );
    TRACE("%d", bEnableCTULevelReport                   );
    TRACE("%d", bEnablePartialFrameUpdate               );

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

    TRACE("%d", NumDeltaQpForNonRectROI);
    TRACE_ARRAY_ROW("%d", NonRectROIDeltaQpList, 16);

    TRACE("%d", EnableCustomRoudingIntra);
    TRACE("%d", RoundingOffsetIntra     );
    TRACE("%d", EnableCustomRoudingInter);
    TRACE("%d", RoundingOffsetInter     );

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

    TRACE("%d", InputType);
    TRACE("%d", pps_curr_pic_ref_enabled_flag);

    TRACE("%d", TileOffsetBufferSizeInByte);
    if (b.TileOffsetBufferSizeInByte)
    {
        for (mfxU32 i = 0; i < b.TileOffsetBufferSizeInByte; i++)
        {
            TRACE("%d", pTileOffset[i]);
        }
    }

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
    TRACE("%d", delta_chroma_log2_weight_denom);

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

    TRACE("%d", SliceSAOFlagBitOffset);
    TRACE("%d", BitLengthSliceHeaderStartingPortion);
    TRACE("%d", SliceHeaderByteOffset);
    TRACE("%d", PredWeightTableBitOffset);
    TRACE("%d", PredWeightTableBitLength);
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
    TRACE_PTR(pCompressedBuffers);
    TRACE_PTR(pCipherCounter);
    TRACE("%d", PavpEncryptionMode.eEncryptionType);
    TRACE("%d", PavpEncryptionMode.eCounterMode);

    TraceArray(b.pCompressedBuffers, b.NumCompBuffers);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-28s"
DECL_START(LOOKAHEAD_INFO)
    TRACE("%d", StatusReportFeedbackNumber);
    TRACE("%d", ValidInfo);
    TRACE("%d", CqmHint);
    TRACE("%d", IntraHint);
    TRACE("%d", TargetFrameSize);
    TRACE("%d", TargetBufferFulness);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-28s"
DECL_START(ENCODE_QUERY_STATUS_PARAMS_HEVC)
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
    TRACE("%d", SceneChangeDetected);
    TRACE("%d", MAD);
    TRACE("%d", NumberSlices);
    TRACE_ARRAY_ROW("%d", PSNRx100, 3);
    TRACE("%d", NextFrameWidthMinus1);
    TRACE("%d", NextFrameHeightMinus1);
    TRACE("%llx", aes_counter.IV);
    TRACE("%llx", aes_counter.Counter);
    TRACE("%d", StreamId);
    Trace(b.lookaheadInfo, 0);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-34s"
DECL_START(ENCODE_QUERY_STATUS_SLICE_PARAMS_HEVC)
    Trace(*((ENCODE_QUERY_STATUS_PARAMS_HEVC*)&b), 0);
    TRACE("%d", SizeOfSliceSizesBuffer);
    TRACE_PTR(pSliceSizes);
    if (b.pSliceSizes) {
        TRACE_ARRAY_ROW("%d", pSliceSizes, b.SizeOfSliceSizesBuffer);
    }
DECL_END
#undef FIELD_FORMAT

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

#endif //!defined(MFX_VA_LINUX)

#endif //#ifdef DDI_TRACE
}
#endif
