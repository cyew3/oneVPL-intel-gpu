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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_ddi_trace.h"
#include <sstream>

namespace AV1EHW
{

#ifdef DDI_TRACE

DDITracer::DDITracer(ENCODER_TYPE type)
    : m_log(0)
    , m_type(type)
{
    m_log = fopen("MSDK_AV1E_HW_DDI_LOG.txt", "w");
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

template <typename T>
const std::string GetTraceFormat(T &par, const char *fieldFormat)
{
    par;

    std::ostringstream oss;
    oss << std::string(fieldFormat) << " = ";
    if (std::is_signed<T>::value)
        oss << "%u\n";
    else
        oss << "%d\n";

    return oss.str();
}

#define TRACE_AUTO(field) \
{ \
    const std::string foramt = GetTraceFormat(b.field, FIELD_FORMAT); \
    fprintf(m_log, foramt.c_str(), #field, b.field); fflush(m_log); \
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

#define FIELD_FORMAT "%-60s"
DECL_START(ENCODE_CAPS_AV1)
    TRACE_AUTO(CodingLimitSet            );
    TRACE_AUTO(ForcedSegmentationSupport );
    TRACE_AUTO(AutoSegmentationSupport   );
    TRACE_AUTO(BRCReset                  );
    TRACE_AUTO(TemporalLayerRateCtrl     );
    TRACE_AUTO(DynamicScaling            );
    TRACE_AUTO(NumScalablePipesMinus1    );
    TRACE_AUTO(UserMaxFrameSizeSupport   );
    TRACE_AUTO(DirtyRectSupport          );
    TRACE_AUTO(MoveRectSupport           );
    TRACE_AUTO(TileSizeBytesMinus1);
    TRACE_AUTO(FrameOBUSupport           );

    TRACE_AUTO(EncodeFunc                );
    TRACE_AUTO(HybridPakFunc             );
    TRACE_AUTO(EncFunc                   );

    TRACE_AUTO(SEG_LVL_ALT_Q             );
    TRACE_AUTO(SEG_LVL_ALT_LF_Y_V        );
    TRACE_AUTO(SEG_LVL_ALT_LF_Y_H        );
    TRACE_AUTO(SEG_LVL_ALT_LF_U          );
    TRACE_AUTO(SEG_LVL_ALT_LF_V          );
    TRACE_AUTO(SEG_LVL_REF_FRAME         );
    TRACE_AUTO(SEG_LVL_SKIP              );
    TRACE_AUTO(SEG_LVL_GLOBALMV          );

    TRACE_AUTO(MaxPicWidth               );
    TRACE_AUTO(MaxPicHeight              );
    TRACE_AUTO(MaxNum_ReferenceL0_P      );
    TRACE_AUTO(MaxNum_ReferenceL0_B      );
    TRACE_AUTO(MaxNum_ReferenceL1_B      );
    TRACE_AUTO(MaxNumOfDirtyRect         );
    TRACE_AUTO(MaxNumOfMoveRect          );
    TRACE_AUTO(MinSegIdBlockSizeAccepted );

    TRACE_AUTO(AV1ToolSupportFlags.fields.still_picture             );
    TRACE_AUTO(AV1ToolSupportFlags.fields.use_128x128_superblock    );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_filter_intra       );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_intra_edge_filter  );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_interintra_compound);
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_masked_compound    );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_warped_motion      );
    TRACE_AUTO(AV1ToolSupportFlags.fields.allow_screen_content_tools);
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_dual_filter        );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_order_hint         );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_jnt_comp           );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_ref_frame_mvs      );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_superres           );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_cdef               );
    TRACE_AUTO(AV1ToolSupportFlags.fields.enable_restoration        );

    TRACE_AUTO(ChromeSupportFlags.fields.i420                       );
    TRACE_AUTO(ChromeSupportFlags.fields.i422                       );
    TRACE_AUTO(ChromeSupportFlags.fields.i444                       );
    TRACE_AUTO(ChromeSupportFlags.fields.mono_chrome                );

    TRACE_AUTO(BitDepthSupportFlags.fields.eight_bits               );
    TRACE_AUTO(BitDepthSupportFlags.fields.ten_bits                 );
    TRACE_AUTO(BitDepthSupportFlags.fields.twelve_bits              );

    TRACE_AUTO(SupportedInterpolationFilters.fields.EIGHTTAP        );
    TRACE_AUTO(SupportedInterpolationFilters.fields.EIGHTTAP_SMOOTH );
    TRACE_AUTO(SupportedInterpolationFilters.fields.EIGHTTAP_SHARP  );
    TRACE_AUTO(SupportedInterpolationFilters.fields.BILINEAR        );
    TRACE_AUTO(SupportedInterpolationFilters.fields.SWITCHABLE      );

    TRACE_AUTO(SupportedRateControlMethods.fields.CQP               );
    TRACE_AUTO(SupportedRateControlMethods.fields.CBR               );
    TRACE_AUTO(SupportedRateControlMethods.fields.VBR               );
    TRACE_AUTO(SupportedRateControlMethods.fields.AVBR              );
    TRACE_AUTO(SupportedRateControlMethods.fields.ICQ               );
    TRACE_AUTO(SupportedRateControlMethods.fields.VCM               );
    TRACE_AUTO(SupportedRateControlMethods.fields.QVBR              );
    TRACE_AUTO(SupportedRateControlMethods.fields.CQL               );
    TRACE_AUTO(SupportedRateControlMethods.fields.SlidingWindow     );
    TRACE_AUTO(SupportedRateControlMethods.fields.LowDelay);
DECL_END
#undef FIELD_FORMAT

#if !defined(MFX_VA_LINUX)

#define FIELD_FORMAT "%s"
void DDITracer::TraceFunc(bool bOut, mfxU32 func, const void* buf, mfxU32 n)
{
    bool bDX9 = false;
    bool bHEX = buf && n;
    bool bGUID= bHEX && !bOut && (bDX9
        && (func == ENCODE_FORMAT_COUNT_ID
            || func == AUXDEV_QUERY_ACCEL_CAPS
            || func == AUXDEV_CREATE_ACCEL_SERVICE));
    bool bENCODE_EXECUTE_PARAMS             = bHEX && !bOut && func == ENCODE_ENC_PAK_ID;
    bool bENCODE_FORMAT_COUNT               = bHEX && bOut && (func == ENCODE_FORMAT_COUNT_ID);
    bool bENCODE_FORMATS                    = bHEX && bOut && (func == ENCODE_FORMATS_ID);
    bool bENCODE_CREATEDEVICE               = bHEX && bOut && (bDX9 && func == AUXDEV_CREATE_ACCEL_SERVICE);
    bool bENCODE_CAPS_AV1                   = bHEX && bOut && (func == AUXDEV_QUERY_ACCEL_CAPS || func == ENCODE_QUERY_ACCEL_CAPS_ID);
    bool bENCODE_QUERY_STATUS_PARAMS_DESCR  = bHEX && !bOut && func == ENCODE_QUERY_STATUS_ID;
    bool bENCODE_QUERY_STATUS_PARAMS_AV1    = bHEX && bOut && func == ENCODE_QUERY_STATUS_ID;
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
    TRACE_FARG(ENCODE_CAPS_AV1);
    TRACE_FARG(ENCODE_QUERY_STATUS_PARAMS_DESCR);
    TRACE_FARG(ENCODE_QUERY_STATUS_PARAMS_AV1);
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
        if (m_type == ENCODER_SCC)
            TraceArray((ENCODE_SET_SEQUENCE_PARAMETERS_AV1_SCC*)pBuf,
            (b.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_AV1_SCC)));
        else
        if (m_type == ENCODER_REXT)
            TraceArray((ENCODE_SET_SEQUENCE_PARAMETERS_AV1_REXT*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_AV1_REXT)));
        else
            TraceArray((ENCODE_SET_SEQUENCE_PARAMETERS_AV1*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_AV1)));
        break;
    case D3DDDIFMT_INTELENCODE_PPSDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA:
        if (m_type == ENCODER_SCC)
            TraceArray((ENCODE_SET_PICTURE_PARAMETERS_AV1_SCC*)pBuf,
            (b.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_AV1_SCC)));
        else
        if (m_type == ENCODER_REXT)
            TraceArray((ENCODE_SET_PICTURE_PARAMETERS_AV1_REXT*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_AV1_REXT)));
        else
            TraceArray((ENCODE_SET_PICTURE_PARAMETERS_AV1*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_AV1)));
        break;
    case D3DDDIFMT_INTELENCODE_SLICEDATA:
    case D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA:
        if (m_type == ENCODER_REXT)
            TraceArray((ENCODE_SET_SLICE_HEADER_AV1_REXT*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_AV1_REXT)));
        else
            TraceArray((ENCODE_SET_SLICE_HEADER_AV1*)pBuf,
                (b.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_AV1)));
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

#define FIELD_FORMAT "%-60s"
DECL_START(ENCODE_SET_SEQUENCE_PARAMETERS_AV1)
    TRACE_AUTO(seq_profile                         );
    TRACE_AUTO(seq_level_idx                       );
    TRACE_AUTO(GopPicSize                          );
    TRACE_AUTO(TargetUsage                         );
    TRACE_AUTO(RateControlMethod                   );
    TRACE_ARRAY_ROW("%d", TargetBitRate, 8         );
    TRACE_AUTO(MaxBitRate                          );
    TRACE_AUTO(MinBitRate                          );
    TRACE_AUTO(InitVBVBufferFullnessInBit          );
    TRACE_AUTO(VBVBufferSizeInBit                  );
    TRACE_AUTO(OptimalVBVBufferLevelInBit          );
    TRACE_AUTO(UpperVBVBufferLevelThresholdInBit   );
    TRACE_AUTO(LowerVBVBufferLevelThresholdInBit   );

    TRACE_AUTO(SeqFlags.fields.bResetBRC           );
    TRACE_AUTO(SeqFlags.fields.bStillPicture       );
    TRACE_AUTO(SeqFlags.fields.bUseRawReconRef     );
    TRACE_AUTO(SeqFlags.fields.DisplayFormatSwizzle);

    TRACE_AUTO(UserMaxFrameSize                    );
    for (mfxU32 i = 0; i < 8; i++)
    {
        TRACE_AUTO(FrameRate[i].Numerator);
        TRACE_AUTO(FrameRate[i].Denominator);
    }
    TRACE_AUTO(NumTemporalLayersMinus1             );
    TRACE_AUTO(ICQQualityFactor                    );

    TRACE_AUTO(InputColorSpace                     );
    TRACE_AUTO(ScenarioInfo                        );
    TRACE_AUTO(ContentInfo                         );
    TRACE_AUTO(FrameSizeTolerance                  );

    TRACE_AUTO(SlidingWindowSize                   );
    TRACE_AUTO(MaxBitRatePerSlidingWindow          );
    TRACE_AUTO(MinBitRatePerSlidingWindow          );

    TRACE_AUTO(CodingToolFlags.fields.enable_order_hint   );
    TRACE_AUTO(CodingToolFlags.fields.enable_superres     );
    TRACE_AUTO(CodingToolFlags.fields.enable_cdef         );
    TRACE_AUTO(CodingToolFlags.fields.enable_restoration  );
    TRACE_AUTO(CodingToolFlags.fields.enable_warped_motion);

    TRACE_AUTO(order_hint_bits_minus_1                    );
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-60s"
DECL_START(ENCODE_SET_PICTURE_PARAMETERS_AV1)
    TRACE_AUTO(frame_width_minus1                    );
    TRACE_AUTO(frame_height_minus1                   );
    TRACE_AUTO(NumTileGroupsMinus1                   );
    TRACE_AUTO(CurrOriginalPic                       );
    TRACE_AUTO(CurrReconstructedPic                  );
    TRACE_ARRAY_ROW("%d", RefFrameList, 8            );
    TRACE_ARRAY_ROW("%d", ref_frame_idx, 7           );

    TRACE_AUTO(primary_ref_frame                     );
    TRACE_AUTO(order_hint                            );
    TRACE_AUTO(ref_frame_ctrl_l0.fields.search_idx0  );
    TRACE_AUTO(ref_frame_ctrl_l0.fields.search_idx1  );
    TRACE_AUTO(ref_frame_ctrl_l0.fields.search_idx2  );
    TRACE_AUTO(ref_frame_ctrl_l0.fields.search_idx3  );
    TRACE_AUTO(ref_frame_ctrl_l0.fields.search_idx4  );
    TRACE_AUTO(ref_frame_ctrl_l0.fields.search_idx5  );
    TRACE_AUTO(ref_frame_ctrl_l0.fields.search_idx6  );

    TRACE_AUTO(ref_frame_ctrl_l1.fields.search_idx0  );
    TRACE_AUTO(ref_frame_ctrl_l1.fields.search_idx1  );
    TRACE_AUTO(ref_frame_ctrl_l1.fields.search_idx2  );
    TRACE_AUTO(ref_frame_ctrl_l1.fields.search_idx3  );
    TRACE_AUTO(ref_frame_ctrl_l1.fields.search_idx4  );
    TRACE_AUTO(ref_frame_ctrl_l1.fields.search_idx5  );
    TRACE_AUTO(ref_frame_ctrl_l1.fields.search_idx6  );

    TRACE_AUTO(PicFlags.fields.frame_type                  );
    TRACE_AUTO(PicFlags.fields.error_resilient_mode        );
    TRACE_AUTO(PicFlags.fields.disable_cdf_update          );
    TRACE_AUTO(PicFlags.fields.use_superres                );
    TRACE_AUTO(PicFlags.fields.allow_high_precision_mv     );
    TRACE_AUTO(PicFlags.fields.use_ref_frame_mvs           );
    TRACE_AUTO(PicFlags.fields.disable_frame_end_update_cdf);
    TRACE_AUTO(PicFlags.fields.reduced_tx_set_used         );
    TRACE_AUTO(PicFlags.fields.LosslessFlag                );
    TRACE_AUTO(PicFlags.fields.SegIdBlockSize              );
    TRACE_AUTO(PicFlags.fields.EnableFrameOBU              );
    TRACE_AUTO(PicFlags.fields.DisableFrameRecon           );
    TRACE_AUTO(PicFlags.fields.LongTermReference           );

    TRACE_ARRAY_ROW("%d", filter_level, 2                  );
    TRACE_AUTO(filter_level_u                              );
    TRACE_AUTO(filter_level_v                              );

    TRACE_AUTO(cLoopFilterInfoFlags.fields.sharpness_level       );
    TRACE_AUTO(cLoopFilterInfoFlags.fields.mode_ref_delta_enabled);
    TRACE_AUTO(cLoopFilterInfoFlags.fields.mode_ref_delta_update );

    TRACE_AUTO(superres_scale_denominator);
    TRACE_AUTO(interp_filter             );

    TRACE_ARRAY_ROW("%d", ref_deltas, 8   );
    TRACE_ARRAY_ROW("%d", mode_deltas, 2  );

    TRACE_AUTO(base_qindex               );
    TRACE_AUTO(y_dc_delta_q              );
    TRACE_AUTO(u_dc_delta_q              );
    TRACE_AUTO(u_ac_delta_q              );
    TRACE_AUTO(v_dc_delta_q              );
    TRACE_AUTO(v_ac_delta_q              );

    TRACE_AUTO(MinBaseQIndex             );
    TRACE_AUTO(MaxBaseQIndex             );

    TRACE_AUTO(wQMatrixFlags.fields.using_qmatrix);
    TRACE_AUTO(wQMatrixFlags.fields.qm_y         );
    TRACE_AUTO(wQMatrixFlags.fields.qm_u         );
    TRACE_AUTO(wQMatrixFlags.fields.qm_v         );

    TRACE_AUTO(dwModeControlFlags.fields.delta_q_present_flag     );
    TRACE_AUTO(dwModeControlFlags.fields.log2_delta_q_res         );
    TRACE_AUTO(dwModeControlFlags.fields.delta_lf_present_flag    );
    TRACE_AUTO(dwModeControlFlags.fields.log2_delta_lf_res        );
    TRACE_AUTO(dwModeControlFlags.fields.delta_lf_multi           );
    TRACE_AUTO(dwModeControlFlags.fields.tx_mode                  );
    TRACE_AUTO(dwModeControlFlags.fields.reference_mode           );
    TRACE_AUTO(dwModeControlFlags.fields.reduced_tx_set_used      );
    TRACE_AUTO(dwModeControlFlags.fields.skip_mode_present        );

    TRACE_AUTO(stAV1Segments.SegmentFlags.fields.segmentation_enabled);
    TRACE_AUTO(stAV1Segments.SegmentFlags.fields.SegmentNumber       );
    TRACE_AUTO(stAV1Segments.SegmentFlags.fields.update_map          );

    if (b.stAV1Segments.SegmentFlags.fields.segmentation_enabled
        && b.stAV1Segments.SegmentFlags.fields.SegmentNumber > 0
        && b.stAV1Segments.SegmentFlags.fields.update_map)
    {
        for (mfxU32 i = 0; i < b.stAV1Segments.SegmentFlags.fields.SegmentNumber; i++)
        {
            TRACE_ARRAY_ROW("%d", stAV1Segments.feature_data[i], 8);
            TRACE_AUTO(stAV1Segments.feature_mask[i]              );
        }
    }

    TRACE_AUTO(tile_cols                 );
    for (mfxU32 i = 0; i < b.tile_cols; i++)
    {
        TRACE_AUTO(width_in_sbs_minus_1[i]);
    }

    TRACE_AUTO(tile_rows                 );
    for (mfxU32 i = 0; i < b.tile_cols; i++)
    {
        TRACE_AUTO(height_in_sbs_minus_1[i]);
    }

    TRACE_AUTO(context_update_tile_id         );
    TRACE_AUTO(temporal_id                    );

    TRACE_AUTO(cdef_damping_minus_3           );
    TRACE_AUTO(cdef_bits                      );
    TRACE_ARRAY_ROW("%d", cdef_y_strengths,  8);
    TRACE_ARRAY_ROW("%d", cdef_uv_strengths, 8);

    TRACE_AUTO(LoopRestorationFlags.fields.yframe_restoration_type );
    TRACE_AUTO(LoopRestorationFlags.fields.cbframe_restoration_type);
    TRACE_AUTO(LoopRestorationFlags.fields.crframe_restoration_type);
    TRACE_AUTO(LoopRestorationFlags.fields.lr_unit_shift           );
    TRACE_AUTO(LoopRestorationFlags.fields.lr_uv_shift             );

    for (mfxU32 i = 0; i < 7; i++)
    {
        TRACE_AUTO(wm[i].wmtype             );
        TRACE_ARRAY_ROW("%d", wm[i].wmmat, 8);
        TRACE_AUTO(wm[i].invalid            );
    }

    TRACE_AUTO(QIndexBitOffset           );
    TRACE_AUTO(SegmentationBitOffset     );
    TRACE_AUTO(LoopFilterParamsBitOffset );
    TRACE_AUTO(CDEFParamsBitOffset       );
    TRACE_AUTO(CDEFParamsSizeInBits      );
    TRACE_AUTO(FrameHdrOBUSizeByteOffset );
    TRACE_AUTO(StatusReportFeedbackNumber);

    TRACE_AUTO(NumSkipFrames             );
    TRACE_AUTO(SkipFramesSizeInBytess    );

    TRACE_AUTO(NumDirtyRects             );
    if (b.NumDirtyRects)
    {
        for (mfxU32 i = 0; i < b.NumDirtyRects; i++)
        {
            TRACE_AUTO(pDirtyRect[i].Top   );
            TRACE_AUTO(pDirtyRect[i].Bottom);
            TRACE_AUTO(pDirtyRect[i].Left  );
            TRACE_AUTO(pDirtyRect[i].Right );
        }
    }

    TRACE_AUTO(NumMoveRects                );
    if (b.NumMoveRects)
    {
        for (mfxU32 i = 0; i < b.NumMoveRects; i++)
        {
            TRACE_AUTO(pMoveRect[i].SourcePointX);
            TRACE_AUTO(pMoveRect[i].SourcePointY);
            TRACE_AUTO(pMoveRect[i].DestTop     );
            TRACE_AUTO(pMoveRect[i].DestBot     );
            TRACE_AUTO(pMoveRect[i].DestLeft    );
            TRACE_AUTO(pMoveRect[i].DestRight   );
        }
    }

    TRACE_AUTO(InputType                 );
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_SLICE_HEADER_AV1)
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
    TRACE_PTR(pSliceSizes);
    if (b.pSliceSizes) {
        TRACE_ARRAY_ROW("%d", pSliceSizes, b.SizeOfSliceSizesBuffer);
    }
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_SEQUENCE_PARAMETERS_AV1_REXT)
    Trace((ENCODE_SET_SEQUENCE_PARAMETERS_AV1 const &) b, idx);
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
DECL_START(ENCODE_SET_PICTURE_PARAMETERS_AV1_REXT)
    Trace((ENCODE_SET_PICTURE_PARAMETERS_AV1 const &)b, idx);
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
DECL_START(ENCODE_SET_SLICE_HEADER_AV1_REXT)
    Trace((ENCODE_SET_SLICE_HEADER_AV1 const &)b, idx);
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

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_SEQUENCE_PARAMETERS_AV1_SCC)
   Trace((ENCODE_SET_SEQUENCE_PARAMETERS_AV1 const &)b, idx);
   TRACE("%d", palette_mode_enabled_flag);
   TRACE("%d", motion_vector_resolution_control_idc);
   TRACE("%d", intra_boundary_filtering_disabled_flag);
   TRACE("%d", palette_max_size);
   TRACE("%d", delta_palette_max_predictor_size);
DECL_END
#undef FIELD_FORMAT

#define FIELD_FORMAT "%-38s"
DECL_START(ENCODE_SET_PICTURE_PARAMETERS_AV1_SCC)
   Trace((ENCODE_SET_PICTURE_PARAMETERS_AV1 const &)b, idx);
   TRACE("%d", pps_curr_pic_ref_enabled_flag);
   TRACE("%d", residual_adaptive_colour_transform_enabled_flag);
   TRACE("%d", pps_slice_act_qp_offsets_present_flag);
   TRACE("%d", PredictorPaletteSize);
   TRACE("%d", pps_act_y_qp_offset_plus5);
   TRACE("%d", pps_act_cb_qp_offset_plus5);
   TRACE("%d", pps_act_cr_qp_offset_plus3);
DECL_END
#undef FIELD_FORMAT
#endif //!defined(MFX_VA_LINUX)

#endif //#ifdef DDI_TRACE
}
#endif
