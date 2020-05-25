/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_pipeline_transcode.h"
#include "mfx_encode_decode_quality.h"
#include "mfx_metric_calc.h"
//#include "mfx_yuv_decoder.h"
#include "mfx_reorder_render.h"
#include "ipps.h"
#include "mfx_pts_based_activator.h"
#include "mfx_encode.h"
#include "mfx_latency_encode.h"
#include "mfx_latency_decoder.h"
#include "mfx_perfcounter_time.h"
#include "mfx_ref_list_control_encode.h"
#include "mfx_per_frame_ext_buf_encode.h"
#include "mfx_serializer.h"
#include "mfx_file_generic.h"
#include "mfx_adapters.h"
#include "mfx_bitstream_decoder.h"

#include "mfx_utility_allocators.h"
#include "mfx_d3d_allocator.h"
#include "mfx_d3d11_allocator.h"
#include "vaapi_allocator.h"
#include "mfx_sysmem_allocator.h"

#include "mfx_encodeorder_encode.h"
#include "mfx_mvc_handler.h"
#include "mfxjpeg.h"
#include "mfx_ip_field_pair_disable_encoder.h"
#include "mfx_field_output_encode.h"
#include "mfx_ref_list_control_encoded_order.h"

#ifdef MFX_PIPELINE_SUPPORT_VP8
    #include "mfxvp8.h"
#endif

#include "mfxvp9.h"

#include "mfx_multi_decoder.h"
#include "mfx_multi_reader.h"
#include "mfx_jpeg_encode_wrap.h"
#include "mfx_factory_default.h"
#include "mfx_encode_query_mod4.h"
#include "mfx_lexical_cast.h"
#include "mfx_encoded_frame_info_encoder.h"

#include <iostream>
#include <iomanip>
//////////////////////////////////////////////////////////////////////////

#define HANDLE_GLOBAL_OPTION(short_option, pref, member, OPT_TYPE, description, handler)\
{VM_STRING(short_option) VM_STRING("-") VM_STRING(#member), OPT_TYPE, {&pref##member}, VM_STRING(description), handler, 0}

#define HANDLE_GLOBAL_OPTION2(short_option, pref, member, OPT_TYPE, description, handler)\
{VM_STRING(short_option) VM_STRING("-") VM_STRING(#member), OPT_TYPE, {&pref member}, VM_STRING(description), handler, 0}


#define HANDLE_MFX_INFO(short_option, member, description)\
{VM_STRING(short_option) VM_STRING("-") VM_STRING(#member), OPT_UINT_16, {&pMFXParams->mfx.member}, VM_STRING(description), NULL, 0}

#define HANDLE_MFX_FRAME_INFO(member, OPT_TYPE, description)\
{VM_STRING("-") VM_STRING(#member), OPT_TYPE, {&pMFXParams->mfx.FrameInfo.member}, VM_STRING(description), NULL}

#define HANDLE_OPTION_FOR_EXT_BUFFER(extbuffer, member, OPT_TYPE, description, codec_id)\
    {VM_STRING("-") VM_STRING(#member), OPT_TYPE, {&extbuffer->member}, VM_STRING(description), NULL, codec_id}

#define HANDLE_EXT_OPTION(member, OPT_TYPE, description)      HANDLE_OPTION_FOR_EXT_BUFFER(m_extCodingOptions, member, OPT_TYPE, description, 0)
#define HANDLE_EXT_OPTION2(member, OPT_TYPE, description)     HANDLE_OPTION_FOR_EXT_BUFFER(m_extCodingOptions2, member, OPT_TYPE, description, 0)
#define HANDLE_EXT_OPTION3(member, OPT_TYPE, description)     HANDLE_OPTION_FOR_EXT_BUFFER(m_extCodingOptions3, member, OPT_TYPE, description, 0)
#define HANDLE_DDI_OPTION(member, OPT_TYPE, description)\
    {VM_STRING("-") VM_STRING(#member), OPT_TYPE, {&m_extCodingOptionsDDI->member}, VM_STRING("[INTERNAL DDI]: ") VM_STRING(description), NULL, 0}
#define HANDLE_VSIG_OPTION(member, OPT_TYPE, description)     HANDLE_OPTION_FOR_EXT_BUFFER(m_extVideoSignalInfo, member, OPT_TYPE, description, 0)
#define HANDLE_CAP_OPTION(member, OPT_TYPE, description)      HANDLE_OPTION_FOR_EXT_BUFFER(m_extEncoderCapability, member, OPT_TYPE, description, 0)
#define HANDLE_HEVC_OPTION(member, OPT_TYPE, description)     HANDLE_OPTION_FOR_EXT_BUFFER(m_extCodingOptionsHEVC, member, OPT_TYPE, description, MFX_CODEC_HEVC)
#define HANDLE_HEVC_TILES(member, OPT_TYPE, description)      HANDLE_OPTION_FOR_EXT_BUFFER(m_extHEVCTiles, member, OPT_TYPE, description, MFX_CODEC_HEVC)
#define HANDLE_HEVC_PARAM(member, OPT_TYPE, description)      HANDLE_OPTION_FOR_EXT_BUFFER(m_extHEVCParam, member, OPT_TYPE, description, MFX_CODEC_HEVC)
#define HANDLE_VP8_OPTION(member, OPT_TYPE, description)      HANDLE_OPTION_FOR_EXT_BUFFER(m_extVP8CodingOptions, member, OPT_TYPE, description, MFX_CODEC_VP8)
#define HANDLE_VP9_PARAM(member, OPT_TYPE, description)       HANDLE_OPTION_FOR_EXT_BUFFER(m_extVP9Param, member, OPT_TYPE, description, MFX_CODEC_VP9)
#define HANDLE_ENCRESET_OPTION(member, OPT_TYPE, description) HANDLE_OPTION_FOR_EXT_BUFFER(m_extEncoderReset, member, OPT_TYPE, description, 0)
#define HANDLE_EXT_MFE(member, OPT_TYPE, description)         HANDLE_OPTION_FOR_EXT_BUFFER(m_extMFEParam, member, OPT_TYPE, description, 0)
#define HANDLE_AV1_OPTION(member, OPT_TYPE, description)      HANDLE_OPTION_FOR_EXT_BUFFER(m_extCodingOptionsAV1E, member, OPT_TYPE, description, MFX_CODEC_AV1)
#if (MFX_VERSION >= MFX_VERSION_NEXT)
#define HANDLE_AV1_PARAM(member, OPT_TYPE, description)       HANDLE_OPTION_FOR_EXT_BUFFER(m_extAV1Param, member, OPT_TYPE, description, MFX_CODEC_AV1)
#define HANDLE_AV1_AUX_DATA(member, OPT_TYPE, description)    HANDLE_OPTION_FOR_EXT_BUFFER(m_extAV1AuxData, member, OPT_TYPE, description, MFX_CODEC_AV1)
#endif

#define FILL_MASK(type, ptr)\
    if (m_bResetParamsStart)\
{\
    int offset = (int)((mfxU8*)ptr - (mfxU8*)&m_EncParams);\
    int size   = convert_type_nbits((mfxU8)type) >> 3;\
    if (offset + size <= (int)sizeof(m_EncParams)){\
    ippsSet_8u(0xFF, (mfxU8*)&m_EncParamsMask + offset, size);}\
}

//fill mask only of field is not in default value
#define FILL_MASK_FROM_FIELD(field, default_value)\
    if (m_bResetParamsStart && default_value != field)\
{\
    int offset = (int)((mfxU8*)&(field) - (mfxU8*)&m_EncParams);\
    if (offset + sizeof(field) <= sizeof(m_EncParams)){\
    ippsSet_8u(0xFF, (mfxU8*)&m_EncParamsMask + offset, sizeof(field));}\
}

//check if the string pointed by c is an integer number
inline bool IsInteger(const vm_char *c)
{
    if (c == NULL)
        return false;

    if (*c == '-')
        c++;

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    std::wstring s(c);
    return !s.empty() && s.find_first_not_of(L"0123456789") == std::wstring::npos;
#else
    std::string s(c);
    return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
#endif
}

//////////////////////////////////////////////////////////////////////////

MFXTranscodingPipeline::MFXTranscodingPipeline(IMFXPipelineFactory *pFactory)
    : MFXDecPipeline(pFactory)
    , m_extCodingOptions(new mfxExtCodingOption())
    , m_extCodingOptions2(new mfxExtCodingOption2())
    , m_extCodingOptions3(new mfxExtCodingOption3())
    , m_extCodingOptionsDDI(new mfxExtCodingOptionDDI())
    , m_extCodingOptionsQuantMatrix(new mfxExtCodingOptionQuantMatrix())
    , m_extDumpFiles(new mfxExtDumpFiles())
    , m_extVideoSignalInfo(new mfxExtVideoSignalInfo())
    , m_extCodingOptionsHEVC(new mfxExtCodingOptionHEVC())
    , m_extCodingOptionsAV1E(new mfxExtCodingOptionAV1E())
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    , m_extAV1Param(new mfxExtAV1Param())
    , m_extAV1AuxData(new mfxExtAV1AuxData())
    , m_extAV1Segmentation(new mfxExtAV1Segmentation())
#endif
    , m_extHEVCTiles(new mfxExtHEVCTiles())
    , m_extHEVCParam(new mfxExtHEVCParam())
    , m_extVP8CodingOptions(new mfxExtVP8CodingOption())
    , m_extVP9Param(new mfxExtVP9Param())
    , m_extVP9Segmentation(new mfxExtVP9Segmentation())
    , m_extEncoderRoi(new mfxExtEncoderROI())
    , m_extDirtyRect(new mfxExtDirtyRect())
    , m_extMoveRect(new mfxExtMoveRect())
    , m_extCodingOptionsSPSPPS(new mfxExtCodingOptionSPSPPS())
    , m_extAvcTemporalLayers(new mfxExtAvcTemporalLayers())
    , m_extVP9TemporalLayers(new mfxExtVP9TemporalLayers())
    , m_svcSeq(new mfxExtSVCSeqDesc())
    , m_svcSeqDeserial(VM_STRING(""), *m_svcSeq.get(), m_filesForDependency)
    , m_svcRateCtrl(new mfxExtSVCRateControl())
    , m_svcRateCtrlDeserial(VM_STRING(""), *m_svcRateCtrl.get())
    , m_QuantMatrix(VM_STRING(""),*m_extCodingOptionsQuantMatrix.get())
    , m_extEncoderCapability(new mfxExtEncoderCapability())
    , m_extEncoderReset(new mfxExtEncoderResetOption())
    , m_extMFEParam(new mfxExtMultiFrameParam())
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    , m_extTemporalLayers(new mfxExtTemporalLayers())
#endif
    , m_EncParams()
    , m_ExtBuffers(new MFXExtBufferVector())
    , m_bCreateDecode()
    , m_applyBitrateParams(&MFXTranscodingPipeline::ApplyBitrateParams, this)
    , m_applyJpegParams(&MFXTranscodingPipeline::ApplyJpegParams, this)
    , m_pEncoder()
{
    mfxVideoParam *pMFXParams = &m_EncParams;

    OptContainer options [] =
    {
        //series of parameters that uses callback function to retain it's inter dependencies
        HANDLE_GLOBAL_OPTION("-b|--bitrate|",  m_,BitRate,    OPT_INT_32, "Target bitrate in bits per second. Note: maximum allowed value w/o using of BRCParamMultiplier is 65535000 bits per second", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("-bm|", m_,MaxBitrate, OPT_INT_32, "Max bitrate (in bits per second) in case of VBR", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("-TargetKbps|", m_,BitRateKbps, OPT_INT_32,"Target bitrate in kbits per second. Note: maximum allowed value w/o using of BRCParamMultiplier is 65535 kbits per second", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("-MaxKbps|", m_,MaxBitrateKbps, OPT_INT_32,"Max bitrate (in kbits per second) in the case of VBR", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("-s|",  m_inParams.n,PicStruct,  OPT_INT_32, "0=progressive, 1=tff, 2=bff, 3=field tff, 4=field bff, 5=single field, 6=single top, 7=single bottom", &m_applyBitrateParams),

        //constant qp support
        HANDLE_GLOBAL_OPTION("-QPI|", m_,QPI,           OPT_UINT_16,    "Constant quantizer for I frames (if RateControlMethod=3)", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("", m_,QPP,           OPT_UINT_16,    "Constant quantizer for P frames (if RateControlMethod=3)", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("", m_,QPB,           OPT_UINT_16,    "Constant quantizer for B frames (if RateControlMethod=3)", &m_applyBitrateParams),

        //CRF support
        HANDLE_GLOBAL_OPTION("", m_,ICQQuality,   OPT_UINT_16,    "", &m_applyBitrateParams),

        HANDLE_GLOBAL_OPTION("", m_,WinBRCSize,          OPT_UINT_16,    "In LA_MAX_AVG specifies size for sliding window (in frames)", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("", m_,WinBRCMaxAvgBps,     OPT_UINT_32,    "In LA_MAX_AVG specifies MaxBitrate for sliding window (bits per second)", &m_applyBitrateParams),
        //AVBR support
        HANDLE_GLOBAL_OPTION("", m_,Accuracy,     OPT_UINT_16,    "In AVBR mode specifies targetbitrate accuracy range", &m_applyBitrateParams),
        HANDLE_GLOBAL_OPTION("", m_,Convergence,  OPT_UINT_16,    "Convergence period for AVBR algorithm ", &m_applyBitrateParams),

        //jpeg specific option
        HANDLE_GLOBAL_OPTION("", m_,Interleaved , OPT_UINT_16,    "Jpeg specific parameter", &m_applyJpegParams),
        HANDLE_GLOBAL_OPTION("", m_,Quality,      OPT_UINT_16,    "Jpeg specific parameter", &m_applyJpegParams),
        HANDLE_GLOBAL_OPTION("", m_,RestartInterval, OPT_UINT_16, "Jpeg specific parameter", &m_applyJpegParams),

        //direct access to mfx_videoparams
        HANDLE_GLOBAL_OPTION2("", pMFXParams->mfx., LowPower, OPT_TRI_STATE, "on/off low power mode (VDEnc)", NULL),
        HANDLE_MFX_INFO("",             BRCParamMultiplier,       "target bitrate = TargetKbps * BRCParamMultiplier"),
        HANDLE_MFX_INFO("",             CodecProfile,             "Codec profile"),
        HANDLE_MFX_INFO("",             CodecLevel,               "Codec level"),
        HANDLE_MFX_INFO("-u|",          TargetUsage,              "1=BEST_QUALITY .. 7=BEST_SPEED"),
        HANDLE_MFX_INFO("-g|--keyint|", GopPicSize,               "GOP size (1 means I-frames only)"),
        HANDLE_MFX_INFO("-r|",          GopRefDist,               "Distance between I- or P- (or GPB)-key frames. If it is zero, the GOP structure is unspecified. Note: If GopRefDist=1, there are no regular B-frames used (only P or GPB)"),
        HANDLE_MFX_INFO("",             GopOptFlag,               "1=GOP_CLOSED, 2=GOP_STRICT"),
        HANDLE_MFX_INFO("",             IdrInterval,              "IDR frame interval (0 means every I-frame is an IDR frame"),
        HANDLE_MFX_INFO("",             RateControlMethod,        "1=CBR, 2=VBR, 3=ConstantQP, 4=AVBR, 8=Lookahead, 9=ICQ, 10=VCM, 11=LA_ICQ, 13=Lookahead with HRD support, 14=QVBR"),
        HANDLE_MFX_INFO("-id|",         InitialDelayInKB,         "For bitrate control"),
        HANDLE_MFX_INFO("-bs|",         BufferSizeInKB,           "For bitrate control"),
        HANDLE_MFX_INFO("-l|",          NumSlice,                 "Number of slices in each video frame"),
        HANDLE_MFX_INFO("-x|--ref|",    NumRefFrame,              "Number of reference frames"),

        //structure mfxFrameInfo
        HANDLE_MFX_FRAME_INFO(FrameRateExtN,      OPT_INT_32,     "Framerate nominator. NOTE: this value only used inside -reset_start/-reset_end section"),
        HANDLE_MFX_FRAME_INFO(FrameRateExtD,      OPT_INT_32,     "Framerate denominator. NOTE: this value only used inside -reset_start/-reset_end section"),
        HANDLE_MFX_FRAME_INFO(AspectRatioW,       OPT_UINT_16,    "Aspect ratio width"),
        HANDLE_MFX_FRAME_INFO(AspectRatioH,       OPT_UINT_16,    "Aspect ratio height"),

        // structure mfxExtCodingOption
        HANDLE_EXT_OPTION(RateDistortionOpt,      OPT_TRI_STATE,  "Enable rate distortion optimization"),
        HANDLE_EXT_OPTION(MECostType,             OPT_UINT_16,    "1=SAD, 2=SSD, 4=SATD_HADAMARD, 8=SATD_HARR"),
        HANDLE_EXT_OPTION(MESearchType,           OPT_UINT_16,    "1=FULL, 2=UMH, 4=LOG, 8=RESERVED, 16=SQUARE, 32=DIAMOND"),
        HANDLE_EXT_OPTION(MVSearchWindow.x,       OPT_UINT_16,    "Search window for motion estimation"),
        HANDLE_EXT_OPTION(MVSearchWindow.y,       OPT_UINT_16,    "Search window for motion estimation"),
        HANDLE_EXT_OPTION(EndOfSequence,          OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(FramePicture,           OPT_TRI_STATE,  "Encode interlaced fields as: ON=interlaced frame, OFF=two separate fields"),
        HANDLE_EXT_OPTION(CAVLC,                  OPT_TRI_STATE,  "ON=CAVLC, OFF=CABAC"),
        HANDLE_EXT_OPTION(RecoveryPointSEI,       OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(ViewOutput,             OPT_TRI_STATE,  "MVC view output mode. Each view is placed in separate bitstream buffer"),
        HANDLE_EXT_OPTION(RefPicListReordering,   OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(ResetRefList,           OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(RefPicMarkRep,          OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(FieldOutput,            OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(IntraPredBlockSize,     OPT_UINT_16,    "1=16x16, 2=down to 8x8, 3=down to 4x4"),
        HANDLE_EXT_OPTION(InterPredBlockSize,     OPT_UINT_16,    "1=16x16, 2=down to 8x8, 3=down to 4x4"),
        HANDLE_EXT_OPTION(MVPrecision,            OPT_UINT_16,    "1=INTEGER, 2=HALFPEL, 4=QUARTERPEL"),
        HANDLE_EXT_OPTION(MaxDecFrameBuffering,   OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION(AUDelimiter,            OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(EndOfStream,            OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(PicTimingSEI,           OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(VuiNalHrdParameters,    OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION(NalHrdConformance,      OPT_TRI_STATE,  ""),

        HANDLE_HEVC_OPTION(Log2MaxCUSize,            OPT_UINT_16,    "4-6"),
        HANDLE_HEVC_OPTION(MaxCUDepth,               OPT_UINT_16,    "1-(Log2MaxCUSize-1)"),
        HANDLE_HEVC_OPTION(QuadtreeTULog2MaxSize,    OPT_UINT_16,    "2-min(5,Log2MaxCUSize)"),
        HANDLE_HEVC_OPTION(QuadtreeTULog2MinSize,    OPT_UINT_16,    "2-QuadtreeTULog2MaxSize"),
        HANDLE_HEVC_OPTION(QuadtreeTUMaxDepthIntra,  OPT_UINT_16,    "1-(Log2MaxCUSize-1)"),
        HANDLE_HEVC_OPTION(QuadtreeTUMaxDepthInter,  OPT_UINT_16,    "1-(Log2MaxCUSize-1)"),
        HANDLE_HEVC_OPTION(QuadtreeTUMaxDepthInterRD, OPT_UINT_16,   "1-(Log2MaxCUSize-1)"),
        HANDLE_HEVC_OPTION(AnalyzeChroma,            OPT_TRI_STATE,  "on/off chroma intra mode"),
        HANDLE_HEVC_OPTION(SignBitHiding,            OPT_TRI_STATE,  ""),
        HANDLE_HEVC_OPTION(RDOQuant,                 OPT_TRI_STATE,  ""),
        HANDLE_HEVC_OPTION(SAO,                      OPT_TRI_STATE,  ""),
        HANDLE_HEVC_OPTION(SplitThresholdStrengthCUIntra, OPT_UINT_16,    "0=default; 1=disabled; 2-4"),
        HANDLE_HEVC_OPTION(SplitThresholdStrengthTUIntra, OPT_UINT_16,    "0=default; 1=disabled; 2-4"),
        HANDLE_HEVC_OPTION(SplitThresholdStrengthCUInter, OPT_UINT_16,    "0=default; 1=disabled; 2-4"),
        HANDLE_HEVC_OPTION(IntraNumCand1_2,          OPT_UINT_16,    "1-35: 4x4 stage1"),
        HANDLE_HEVC_OPTION(IntraNumCand1_3,          OPT_UINT_16,    "1-35: 8x8 stage1"),
        HANDLE_HEVC_OPTION(IntraNumCand1_4,          OPT_UINT_16,    "1-35: 16x16 stage1"),
        HANDLE_HEVC_OPTION(IntraNumCand1_5,          OPT_UINT_16,    "1-35: 32x32 stage1"),
        HANDLE_HEVC_OPTION(IntraNumCand1_6,          OPT_UINT_16,    "1-35: 64x64 stage1"),
        HANDLE_HEVC_OPTION(IntraNumCand2_2,          OPT_UINT_16,    "1-35: 4x4 stage2"),
        HANDLE_HEVC_OPTION(IntraNumCand2_3,          OPT_UINT_16,    "1-35: 8x8 stage2"),
        HANDLE_HEVC_OPTION(IntraNumCand2_4,          OPT_UINT_16,    "1-35: 16x16 stage2"),
        HANDLE_HEVC_OPTION(IntraNumCand2_5,          OPT_UINT_16,    "1-35: 32x32 stage2"),
        HANDLE_HEVC_OPTION(IntraNumCand2_6,          OPT_UINT_16,    "1-35: 64x14 stage2"),
        HANDLE_HEVC_OPTION(WPP,                      OPT_TRI_STATE,  "Wavefront Parallel Processing"),
        HANDLE_HEVC_OPTION(Log2MinCuQpDeltaSize,     OPT_UINT_16,    "6-64x64; 5-32x32; 4-16x16 3-8x8"),
        HANDLE_HEVC_OPTION(PartModes,                OPT_UINT_16,    "0-default; 1-square only; 2-no AMP; 3-all"),
        HANDLE_HEVC_OPTION(CmIntraThreshold,         OPT_UINT_16,    "threshold = CmIntraThreshold / 256.0"),
        HANDLE_HEVC_OPTION(TUSplitIntra,             OPT_UINT_16,    "0-default; 1-always; 2-never; 3-for Intra frames only"),
        HANDLE_HEVC_OPTION(CUSplit,                  OPT_UINT_16,    "0-default; 1-check Skip cost first"),
        HANDLE_HEVC_OPTION(IntraAngModes,            OPT_UINT_16,    "I slice Intra Angular modes: 0-default: 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only"),
        HANDLE_HEVC_OPTION(IntraAngModesP,           OPT_UINT_16,    "P slice Intra Angular modes: 0-default; 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only, 100- disable"),
        HANDLE_HEVC_OPTION(IntraAngModesBRef,        OPT_UINT_16,    "B Ref slice Intra Angular modes: 0-default; 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only, 100- disable"),
        HANDLE_HEVC_OPTION(IntraAngModesBnonRef,     OPT_UINT_16,    "B non Ref slice Intra Angular modes: 0-default; 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only, 100- disable"),
        HANDLE_HEVC_OPTION(EnableCm,                 OPT_TRI_STATE,  "on/off CM branch"),
        HANDLE_HEVC_OPTION(BPyramid,                 OPT_TRI_STATE,  "B-Pyramid"),
        HANDLE_HEVC_OPTION(FastPUDecision,           OPT_TRI_STATE,  "on/off fast PU decision (fast means no TU split)"),
        HANDLE_HEVC_OPTION(HadamardMe,               OPT_UINT_16,    "0-default 1-never; 2-subpel; 3-always"),
        HANDLE_HEVC_OPTION(Deblocking,               OPT_TRI_STATE,  "on/off deblocking"),
        HANDLE_HEVC_OPTION(RDOQuantChroma,           OPT_TRI_STATE,  "on/off RDO quantization for chroma"),
        HANDLE_HEVC_OPTION(RDOQuantCGZ,              OPT_TRI_STATE,  "on/off try zero coeff groups in RDO"),
        HANDLE_HEVC_OPTION(SaoOpt,                   OPT_UINT_16,    "0-default; 1-all modes; 2-fast four modes only"),
        HANDLE_HEVC_OPTION(SaoSubOpt,                OPT_UINT_16,    "0-default; 1-All; 2-SubOpt, 3-Ref Frames only"),
        HANDLE_HEVC_OPTION(IntraNumCand0_2,          OPT_UINT_16,    "number of candidates for SATD stage after gradient analysis for TU4x4"),
        HANDLE_HEVC_OPTION(IntraNumCand0_3,          OPT_UINT_16,    "number of candidates for SATD stage after gradient analysis for TU8x8"),
        HANDLE_HEVC_OPTION(IntraNumCand0_4,          OPT_UINT_16,    "number of candidates for SATD stage after gradient analysis for TU16x16"),
        HANDLE_HEVC_OPTION(IntraNumCand0_5,          OPT_UINT_16,    "number of candidates for SATD stage after gradient analysis for TU32x32"),
        HANDLE_HEVC_OPTION(IntraNumCand0_6,          OPT_UINT_16,    "number of candidates for SATD stage after gradient analysis for TU64x64"),
        HANDLE_HEVC_OPTION(CostChroma,               OPT_TRI_STATE,  "on/off include chroma in cost"),
        HANDLE_HEVC_OPTION(IntraChromaRDO,           OPT_TRI_STATE,  "on/off adjusted chroma RDO"),
        HANDLE_HEVC_OPTION(FastInterp,               OPT_TRI_STATE,  "on/off fast filters for motion estimation"),
        HANDLE_HEVC_OPTION(SplitThresholdTabIndex,   OPT_UINT_16,    "select tab for split threshold: 1, 2, 3, default or 0 = #2"),
        HANDLE_HEVC_OPTION(PatternIntPel,            OPT_UINT_16,    "0-default; 1-log; 3- dia; 100-fullsearch"),
        HANDLE_HEVC_OPTION(PatternSubPel,            OPT_UINT_16,    "0-default; 1-int pel only; 2-halfpel; 3-quarter pel"),
        HANDLE_HEVC_OPTION(FastSkip,                 OPT_TRI_STATE,  "on/off stop decision if cbf for best merge is 0"),
        HANDLE_HEVC_OPTION(ForceNumThread,           OPT_UINT_16,    "0-default"),
        HANDLE_HEVC_OPTION(FastCbfMode,              OPT_TRI_STATE,  "on/off stop TU mode serch after zero cbf"),
        HANDLE_HEVC_OPTION(PuDecisionSatd,           OPT_TRI_STATE,  "on/off use SATD for PU decision"),
        HANDLE_HEVC_OPTION(MinCUDepthAdapt,          OPT_TRI_STATE,  "on/off adaptive min CU depth"),
        HANDLE_HEVC_OPTION(MaxCUDepthAdapt,          OPT_TRI_STATE,  "on/off adaptive max CU depth"),
        HANDLE_HEVC_OPTION(NumBiRefineIter,          OPT_UINT_16,    "1-check best L0+L1; N-check best L0+L1 then N-1 refinement iterations"),
        HANDLE_HEVC_OPTION(CUSplitThreshold,         OPT_UINT_16,    "skip CU split check: threshold = CUSplitThreshold / 256.0"),
        HANDLE_HEVC_OPTION(DeltaQpMode,              OPT_UINT_16,    "DeltaQP modes: 0-default; 1-disabled;  1+(0x1 = CAQ, 0x2 = CAL, 0x4 = PAQ, 0x8 = PSY, 0x10=HROI)"),
        HANDLE_HEVC_OPTION(Enable10bit,              OPT_TRI_STATE,  "on/off 10 bit coding"),
        HANDLE_HEVC_OPTION(CpuFeature,               OPT_UINT_16,    "0-auto, 1-px, 2-sse4, 3-sse4atom, 4-ssse3, 5-avx2"),
        HANDLE_HEVC_OPTION(TryIntra,                 OPT_UINT_16,    "Try Intra in Inter; 0-default, 1-Always, 2-Based on Content Analysis"),
        HANDLE_HEVC_OPTION(FastAMPSkipME,            OPT_UINT_16,    "Skip AMP ME; 0-default, 1-Never, 2-Adaptive"),
        HANDLE_HEVC_OPTION(FastAMPRD,                OPT_UINT_16,    "Fast AMP RD; 0-default, 1-Never, 2-Adaptive"),
        HANDLE_HEVC_OPTION(SkipMotionPartition,      OPT_UINT_16,    "Skip Motion Partition RD; 0-default, 1-Never, 2-Adaptive"),
        HANDLE_HEVC_OPTION(SkipCandRD,               OPT_TRI_STATE,  "Skip Candidate RD; on-Full RD, off-Fast Decision"),
        HANDLE_HEVC_OPTION(FramesInParallel,         OPT_UINT_16,    "encoding multiple frames at the same time (0 - auto detect, 1 - default, no frame threading)."),
        HANDLE_HEVC_OPTION(AdaptiveRefs,             OPT_TRI_STATE,  "Adaptive Ref Selection"),
        HANDLE_HEVC_OPTION(FastCoeffCost,            OPT_TRI_STATE,  "Use Fast Coeff cost Estimator"),
        HANDLE_HEVC_OPTION(NumRefFrameB,             OPT_UINT_16,    "Total Number of Reference Frames used for B Frames in Pyramid"),
        HANDLE_HEVC_OPTION(IntraMinDepthSC,          OPT_UINT_16,    "Spatial complexity for Intra min depth 1"),
        HANDLE_HEVC_OPTION(InterMinDepthSTC,         OPT_UINT_16,    "Spatio-Temrpoal complexity for Inter min depth 1"),
        HANDLE_HEVC_OPTION(MotionPartitionDepth,     OPT_UINT_16,    "Motion Partitioning Depth"),
        HANDLE_HEVC_OPTION(AnalyzeCmplx,             OPT_UINT_16,    "0-default, 1-off, 2-on"),
        HANDLE_HEVC_OPTION(RateControlDepth,         OPT_UINT_16,    "how many frames analyzed by BRC including current frame"),
        HANDLE_HEVC_OPTION(LowresFactor,             OPT_UINT_16,    "downscale factor for analyze complexity: 0-default 1-fullsize 2-halfsize 3-quartersize"),
        HANDLE_HEVC_OPTION(DeblockBorders,           OPT_TRI_STATE,  "on/off deblock borders"),
        HANDLE_HEVC_OPTION(SAOChroma,                OPT_TRI_STATE,  "on/off SAO for Chroma"),
        HANDLE_HEVC_OPTION(RepackProb,               OPT_UINT_16,    "percent of random repack probabiility, 0 - no random repacks"),
        HANDLE_HEVC_OPTION(NumRefLayers,             OPT_UINT_16,    "Reference Frames Layers used for B Frames in Pyramid"),
        HANDLE_HEVC_OPTION(ConstQpOffset,            OPT_UINT_16,    "allows setting negative QPs for 10bit: finalQP[IPB] = mfx.QP[IPB] - ConstQpOffset"),
        HANDLE_HEVC_OPTION(SplitThresholdMultiplier, OPT_UINT_16,    "0-10-default: multipler = SplitThresholdMultiplier / 10.0"),
        HANDLE_HEVC_OPTION(EnableCmBiref,            OPT_UINT_16,    "default is ON for TU1-5 and OFF for TU6-7"),
        HANDLE_HEVC_OPTION(RepackForMaxFrameSize,    OPT_TRI_STATE,  "Repack for Max Frame Size violations (default is ON)"),
        HANDLE_HEVC_OPTION(AutoScaleToCoresUsingTiles, OPT_TRI_STATE,  "Automaticaly Scale to available Cores using Tiles if needed"),
        HANDLE_HEVC_OPTION(MaxTaskChainEnc,          OPT_UINT_16,    "Max Task Chain in LCU for Multi-threading Efficiency"),
        HANDLE_HEVC_OPTION(MaxTaskChainInloop,       OPT_UINT_16,    "Max Task Chain in LCU for Multi-threading Efficiency"),

        //AV1 options
        HANDLE_AV1_OPTION(Log2MaxCUSize, OPT_UINT_16, "4-6"),
        HANDLE_AV1_OPTION(MaxCUDepth, OPT_UINT_16, "1-(Log2MaxCUSize-1)"),
        HANDLE_AV1_OPTION(QuadtreeTULog2MaxSize, OPT_UINT_16, "2-min(5,Log2MaxCUSize)"),
        HANDLE_AV1_OPTION(QuadtreeTULog2MinSize, OPT_UINT_16, "2-QuadtreeTULog2MaxSize"),
        HANDLE_AV1_OPTION(QuadtreeTUMaxDepthIntra, OPT_UINT_16, "1-(Log2MaxCUSize-1)"),
        HANDLE_AV1_OPTION(QuadtreeTUMaxDepthInter, OPT_UINT_16, "1-(Log2MaxCUSize-1)"),
        HANDLE_AV1_OPTION(QuadtreeTUMaxDepthInterRD, OPT_UINT_16, "1-(Log2MaxCUSize-1)"),
        HANDLE_AV1_OPTION(AnalyzeChroma, OPT_TRI_STATE, "on/off chroma intra mode"),
        HANDLE_AV1_OPTION(RDOQuant, OPT_TRI_STATE, ""),
        HANDLE_AV1_OPTION(SplitThresholdStrengthCUIntra, OPT_UINT_16, "0=default; 1=disabled; 2-4"),
        HANDLE_AV1_OPTION(SplitThresholdStrengthTUIntra, OPT_UINT_16, "0=default; 1=disabled; 2-4"),
        HANDLE_AV1_OPTION(SplitThresholdStrengthCUInter, OPT_UINT_16, "0=default; 1=disabled; 2-4"),
        HANDLE_AV1_OPTION(IntraNumCand1_2, OPT_UINT_16, "1-35: 4x4 stage1"),
        HANDLE_AV1_OPTION(IntraNumCand1_3, OPT_UINT_16, "1-35: 8x8 stage1"),
        HANDLE_AV1_OPTION(IntraNumCand1_4, OPT_UINT_16, "1-35: 16x16 stage1"),
        HANDLE_AV1_OPTION(IntraNumCand1_5, OPT_UINT_16, "1-35: 32x32 stage1"),
        HANDLE_AV1_OPTION(IntraNumCand1_6, OPT_UINT_16, "1-35: 64x64 stage1"),
        HANDLE_AV1_OPTION(IntraNumCand2_2, OPT_UINT_16, "1-35: 4x4 stage2"),
        HANDLE_AV1_OPTION(IntraNumCand2_3, OPT_UINT_16, "1-35: 8x8 stage2"),
        HANDLE_AV1_OPTION(IntraNumCand2_4, OPT_UINT_16, "1-35: 16x16 stage2"),
        HANDLE_AV1_OPTION(IntraNumCand2_5, OPT_UINT_16, "1-35: 32x32 stage2"),
        HANDLE_AV1_OPTION(IntraNumCand2_6, OPT_UINT_16, "1-35: 64x14 stage2"),
        HANDLE_AV1_OPTION(WPP, OPT_TRI_STATE, "Wavefront Parallel Processing"),
        HANDLE_AV1_OPTION(Log2MinCuQpDeltaSize, OPT_UINT_16, "6-64x64; 5-32x32; 4-16x16 3-8x8"),
        HANDLE_AV1_OPTION(PartModes, OPT_UINT_16, "0-default; 1-square only; 2-no AMP; 3-all"),
        HANDLE_AV1_OPTION(CmIntraThreshold, OPT_UINT_16, "threshold = CmIntraThreshold / 256.0"),
        HANDLE_AV1_OPTION(TUSplitIntra, OPT_UINT_16, "0-default; 1-always; 2-never; 3-for Intra frames only"),
        HANDLE_AV1_OPTION(CUSplit, OPT_UINT_16, "0-default; 1-check Skip cost first"),
        HANDLE_AV1_OPTION(IntraAngModes, OPT_UINT_16, "I slice Intra Angular modes: 0-default: 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only"),
        HANDLE_AV1_OPTION(IntraAngModesP, OPT_UINT_16, "P slice Intra Angular modes: 0-default; 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only, 100- disable"),
        HANDLE_AV1_OPTION(IntraAngModesBRef, OPT_UINT_16, "B Ref slice Intra Angular modes: 0-default; 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only, 100- disable"),
        HANDLE_AV1_OPTION(IntraAngModesBnonRef, OPT_UINT_16, "B non Ref slice Intra Angular modes: 0-default; 1-all; 2-all even + few odd; 3-gradient analysis + few modes, 99- DC&Planar only, 100- disable"),
        HANDLE_AV1_OPTION(EnableCm, OPT_TRI_STATE, "on/off CM branch"),
        HANDLE_AV1_OPTION(BPyramid, OPT_TRI_STATE, "B-Pyramid"),
        HANDLE_AV1_OPTION(FastPUDecision, OPT_TRI_STATE, "on/off fast PU decision (fast means no TU split)"),
        HANDLE_AV1_OPTION(HadamardMe, OPT_UINT_16, "0-default 1-never; 2-subpel; 3-always"),
        HANDLE_AV1_OPTION(TMVP, OPT_TRI_STATE, "on/off temporal MV predictor"),
        HANDLE_AV1_OPTION(Deblocking, OPT_TRI_STATE, "on/off deblocking"),
        HANDLE_AV1_OPTION(RDOQuantChroma, OPT_TRI_STATE, "on/off RDO quantization for chroma"),
        HANDLE_AV1_OPTION(IntraNumCand0_2, OPT_UINT_16, "number of candidates for SATD stage after gradient analysis for TU4x4"),
        HANDLE_AV1_OPTION(IntraNumCand0_3, OPT_UINT_16, "number of candidates for SATD stage after gradient analysis for TU8x8"),
        HANDLE_AV1_OPTION(IntraNumCand0_4, OPT_UINT_16, "number of candidates for SATD stage after gradient analysis for TU16x16"),
        HANDLE_AV1_OPTION(IntraNumCand0_5, OPT_UINT_16, "number of candidates for SATD stage after gradient analysis for TU32x32"),
        HANDLE_AV1_OPTION(IntraNumCand0_6, OPT_UINT_16, "number of candidates for SATD stage after gradient analysis for TU64x64"),
        HANDLE_AV1_OPTION(CostChroma, OPT_TRI_STATE, "on/off include chroma in cost"),
        HANDLE_AV1_OPTION(IntraChromaRDO, OPT_TRI_STATE, "on/off adjusted chroma RDO"),
        HANDLE_AV1_OPTION(FastInterp, OPT_TRI_STATE, "on/off fast filters for motion estimation"),
        HANDLE_AV1_OPTION(SplitThresholdTabIndex, OPT_UINT_16, "select tab for split threshold: 1, 2, 3, default or 0 = #2"),
        HANDLE_AV1_OPTION(PatternIntPel, OPT_UINT_16, "0-default; 1-log; 3- dia; 100-fullsearch"),
        HANDLE_AV1_OPTION(PatternSubPel, OPT_UINT_16, "0-default; 1-int pel only; 2-halfpel; 3-quarter pel"),
        HANDLE_AV1_OPTION(FastSkip, OPT_TRI_STATE, "on/off stop decision if cbf for best merge is 0"),
        HANDLE_AV1_OPTION(ForceNumThread, OPT_UINT_16, "0-default"),
        HANDLE_AV1_OPTION(FastCbfMode, OPT_TRI_STATE, "on/off stop TU mode serch after zero cbf"),
        HANDLE_AV1_OPTION(PuDecisionSatd, OPT_TRI_STATE, "on/off use SATD for PU decision"),
        HANDLE_AV1_OPTION(MinCUDepthAdapt, OPT_TRI_STATE, "on/off adaptive min CU depth"),
        HANDLE_AV1_OPTION(MaxCUDepthAdapt, OPT_TRI_STATE, "on/off adaptive max CU depth"),
        HANDLE_AV1_OPTION(NumBiRefineIter, OPT_UINT_16, "1-check best L0+L1; N-check best L0+L1 then N-1 refinement iterations"),
        HANDLE_AV1_OPTION(CUSplitThreshold, OPT_UINT_16, "skip CU split check: threshold = CUSplitThreshold / 256.0"),
        HANDLE_AV1_OPTION(DeltaQpMode, OPT_UINT_16, "DeltaQP modes: 0-default; 1-disabled;  1+(0x1 = CAQ, 0x2 = CAL, 0x4 = PAQ, 0x8 = PSY, 0x10=HROI)"),
        HANDLE_AV1_OPTION(Enable10bit, OPT_TRI_STATE, "on/off 10 bit coding"),
        HANDLE_AV1_OPTION(CpuFeature, OPT_UINT_16, "0-auto, 1-px, 2-sse4, 3-sse4atom, 4-ssse3, 5-avx2"),
        HANDLE_AV1_OPTION(TryIntra, OPT_UINT_16, "Try Intra in Inter; 0-default, 1-Always, 2-Based on Content Analysis"),
        HANDLE_AV1_OPTION(FastAMPSkipME, OPT_UINT_16, "Skip AMP ME; 0-default, 1-Never, 2-Adaptive"),
        HANDLE_AV1_OPTION(FastAMPRD, OPT_UINT_16, "Fast AMP RD; 0-default, 1-Never, 2-Adaptive"),
        HANDLE_AV1_OPTION(SkipMotionPartition, OPT_UINT_16, "Skip Motion Partition RD; 0-default, 1-Never, 2-Adaptive"),
        HANDLE_AV1_OPTION(SkipCandRD, OPT_TRI_STATE, "Skip Candidate RD; on-Full RD, off-Fast Decision"),
        HANDLE_AV1_OPTION(FramesInParallel, OPT_UINT_16, "encoding multiple frames at the same time (0 - auto detect, 1 - default, no frame threading)."),
        HANDLE_AV1_OPTION(AdaptiveRefs, OPT_TRI_STATE, "Adaptive Ref Selection"),
        HANDLE_AV1_OPTION(FastCoeffCost, OPT_TRI_STATE, "Use Fast Coeff cost Estimator"),
        HANDLE_AV1_OPTION(NumRefFrameB, OPT_UINT_16, "Total Number of Reference Frames used for B Frames in Pyramid"),
        HANDLE_AV1_OPTION(IntraMinDepthSC, OPT_UINT_16, "Spatial complexity for Intra min depth 1"),
        HANDLE_AV1_OPTION(InterMinDepthSTC, OPT_UINT_16, "Spatio-Temrpoal complexity for Inter min depth 1"),
        HANDLE_AV1_OPTION(MotionPartitionDepth, OPT_UINT_16, "Motion Partitioning Depth"),
        HANDLE_AV1_OPTION(AnalyzeCmplx, OPT_UINT_16, "0-default, 1-off, 2-on"),
        HANDLE_AV1_OPTION(RateControlDepth, OPT_UINT_16, "how many frames analyzed by BRC including current frame"),
        HANDLE_AV1_OPTION(LowresFactor, OPT_UINT_16, "downscale factor for analyze complexity: 0-default 1-fullsize 2-halfsize 3-quartersize"),
        HANDLE_AV1_OPTION(DeblockBorders, OPT_TRI_STATE, "on/off deblock borders"),
        HANDLE_AV1_OPTION(SAOChroma, OPT_TRI_STATE, "on/off SAO for Chroma"),
        HANDLE_AV1_OPTION(RepackProb, OPT_UINT_16, "percent of random repack probabiility, 0 - no random repacks"),
        HANDLE_AV1_OPTION(NumRefLayers, OPT_UINT_16, "Reference Frames Layers used for B Frames in Pyramid"),
        HANDLE_AV1_OPTION(ConstQpOffset, OPT_UINT_16, "allows setting negative QPs for 10bit: finalQP[IPB] = mfx.QP[IPB] - ConstQpOffset"),
        HANDLE_AV1_OPTION(SplitThresholdMultiplier, OPT_UINT_16, "0-10-default: multipler = SplitThresholdMultiplier / 10.0"),
        HANDLE_AV1_OPTION(EnableCmInterp, OPT_TRI_STATE, "default is ON"),
        HANDLE_AV1_OPTION(RepackForMaxFrameSize, OPT_TRI_STATE, "Repack for Max Frame Size violations (default is ON)"),
        HANDLE_AV1_OPTION(AutoScaleToCoresUsingTiles, OPT_TRI_STATE, "Automaticaly Scale to available Cores using Tiles if needed"),
        HANDLE_AV1_OPTION(MaxTaskChainEnc, OPT_UINT_16, "Max Task Chain in LCU for Multi-threading Efficiency"),
        HANDLE_AV1_OPTION(MaxTaskChainInloop, OPT_UINT_16, "Max Task Chain in LCU for Multi-threading Efficiency"),
        /* AV1 */
        HANDLE_AV1_OPTION(FwdProbUpdateCoef, OPT_TRI_STATE, "on/off forward probability update for coefficients"),
        HANDLE_AV1_OPTION(FwdProbUpdateSyntax, OPT_TRI_STATE, "on/off forward probability update for other syntax elements"),
        HANDLE_AV1_OPTION(DeblockingLevelMethod, OPT_UINT_16, "0-default, 1-QP based, 2-Full search on full pic, 3-Smart search on full pic"),
        HANDLE_AV1_OPTION(AllowHpMv, OPT_TRI_STATE, "on/off high precision motion vectors"),
        HANDLE_AV1_OPTION(MaxTxDepthIntra, OPT_UINT_16, "Number of Tx sizes tested during Intra mode decision: 0-default, 1..4"),
        HANDLE_AV1_OPTION(MaxTxDepthInter, OPT_UINT_16, "Number of Tx sizes tested during Inter mode decision: 0-default, 1..4"),
        HANDLE_AV1_OPTION(MaxTxDepthIntraRefine, OPT_UINT_16, "Number of Tx sizes tested during Intra mode refinement: 0-default, 1..4"),
        HANDLE_AV1_OPTION(MaxTxDepthInterRefine, OPT_UINT_16, "Number of Tx sizes tested during Inter mode refinement: 0-default, 1..4"),
        HANDLE_AV1_OPTION(ChromaRDO, OPT_TRI_STATE, "on/off Chroma RDO during mode decision"),
        HANDLE_AV1_OPTION(InterpFilterRefine, OPT_TRI_STATE, "on/off Interpolation filters during mode refinement"),
        HANDLE_AV1_OPTION(IntraRDO, OPT_TRI_STATE, "on/off Intra mode decision by RDO"),
        HANDLE_AV1_OPTION(InterRDO, OPT_TRI_STATE, "on/off Inter mode decision by RDO"),
        HANDLE_AV1_OPTION(IntraInterRDO, OPT_TRI_STATE, "on/off Intra/Inter mode decision by RDO"),
        HANDLE_AV1_OPTION(CodecTypeExt, OPT_UINT_16, "Codec type: 0-default; 1-VP9; 2-AV1"),
        HANDLE_AV1_OPTION(CFLMode, OPT_TRI_STATE, "on/off Chroma from Luma Mode"),
        HANDLE_AV1_OPTION(ScreenMode, OPT_UINT_16, "ScreenContent tools: 0-default, 1-intrabc, 2-palette, 3-both, 4-auto, 5=off"),
        HANDLE_AV1_OPTION(NumTileColumnsKeyFrame, OPT_UINT_16, "0-default, 1..N-num til columns"),
        HANDLE_AV1_OPTION(NumTileColumnsInterFrame, OPT_UINT_16, "0-default, 1..N-num til columns"),
        HANDLE_AV1_OPTION(NumGpuSlices, OPT_UINT_16, "number of slices on GPU"),
        //AV1 params
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        HANDLE_AV1_PARAM(FrameWidth,               OPT_UINT_16,   "0-maxU16"),
        HANDLE_AV1_PARAM(FrameHeight,              OPT_UINT_16,   "0-maxU16"),
        HANDLE_AV1_PARAM(WriteIVFHeaders,          OPT_TRI_STATE, "on/off: Insertion IVF container headers to output stream"),
        HANDLE_AV1_PARAM(UseAnnexB,                OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(PackOBUFrame,             OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(InsertTemporalDelimiter,  OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(UniformTileSpacing,       OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(ContextUpdateTileIdPlus1, OPT_UINT_8,    "0-128"),
        HANDLE_AV1_PARAM(NumTileRows,              OPT_UINT_16,   "0-maxU16: Number of tile rows (1 - default)"),
        HANDLE_AV1_PARAM(NumTileColumns,           OPT_UINT_16,   "0-maxU16: Number of tile columns (1 - default)"),
        HANDLE_AV1_PARAM(NumTileGroups,            OPT_UINT_16,   "0-maxU16"),
        HANDLE_AV1_PARAM(EnableCdef,               OPT_TRI_STATE, "on/off: Constrained Directional Enhancement Filter"),
        HANDLE_AV1_PARAM(EnableRestoration,        OPT_TRI_STATE, "on/off: Enable Loop Restoration"),
        HANDLE_AV1_PARAM(LoopFilterSharpness,      OPT_UINT_8,    "0-8: 0 - app default, map to bitstream: [1..8] => [0..7]"),
        HANDLE_AV1_PARAM(InterpFilter,             OPT_UINT_8,    "0-4: Interpolation filters during mode decision"),
        HANDLE_AV1_PARAM(SegmentationMode,         OPT_UINT_8,    "0 - off, 1 - auto, 2 - manual"),
        HANDLE_AV1_PARAM(DisableCdfUpdate,         OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(DisableFrameEndUpdateCdf, OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(EnableSuperres,           OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(SuperresScaleDenominator, OPT_UINT_8,    "9-16: 0 - app default"),
        HANDLE_AV1_PARAM(StillPictureMode,         OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_PARAM(SwitchInterval,           OPT_UINT_16,   "0-maxU16: Interval (0 - disabled)"),

        HANDLE_AV1_AUX_DATA(Cdef.CdefDampingMinus3, OPT_UINT_8, "0-3"),
        HANDLE_AV1_AUX_DATA(Cdef.CdefBits, OPT_UINT_8, "0-3"),
        HANDLE_AV1_AUX_DATA(LoopFilter.LFLevelYVert, OPT_UINT_8, "0-63"),
        HANDLE_AV1_AUX_DATA(LoopFilter.LFLevelYHorz, OPT_UINT_8, "0-63"),
        HANDLE_AV1_AUX_DATA(LoopFilter.LFLevelU, OPT_UINT_8, "0-63"),
        HANDLE_AV1_AUX_DATA(LoopFilter.LFLevelV, OPT_UINT_8, "0-63"),
        HANDLE_AV1_AUX_DATA(LoopFilter.ModeRefDeltaEnabled, OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_AUX_DATA(LoopFilter.ModeRefDeltaUpdate, OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_AUX_DATA(QP.YDcDeltaQ, OPT_INT_8, "[-63..63]"),
        HANDLE_AV1_AUX_DATA(QP.UDcDeltaQ, OPT_INT_8, "[-63..63]"),
        HANDLE_AV1_AUX_DATA(QP.VDcDeltaQ, OPT_INT_8, "[-63..63]"),
        HANDLE_AV1_AUX_DATA(QP.UAcDeltaQ, OPT_INT_8, "[-63..63]"),
        HANDLE_AV1_AUX_DATA(QP.VAcDeltaQ, OPT_INT_8, "[-63..63]"),
        HANDLE_AV1_AUX_DATA(QP.MinBaseQIndex, OPT_UINT_8, "[1-255]"),
        HANDLE_AV1_AUX_DATA(QP.MaxBaseQIndex, OPT_UINT_8, "[1-255]"),
        HANDLE_AV1_AUX_DATA(ErrorResilientMode, OPT_TRI_STATE, "on/off"),
        HANDLE_AV1_AUX_DATA(EnableOrderHint, OPT_TRI_STATE, "0-127"),
        HANDLE_AV1_AUX_DATA(OrderHintBits, OPT_UINT_8, "0-8"),
        HANDLE_AV1_AUX_DATA(DisplayFormatSwizzle, OPT_TRI_STATE, "on/off"),
#else
        HANDLE_AV1_OPTION(InterpFilter, OPT_TRI_STATE, "on/off Interpolation filters during mode decision"),
        HANDLE_AV1_OPTION(CDEF, OPT_TRI_STATE, "on/off CDEF"),
        HANDLE_AV1_OPTION(LRMode, OPT_TRI_STATE, "on/off Loop Restoration Mode"),
        HANDLE_AV1_OPTION(SRMode, OPT_TRI_STATE, "on/off Super Resolution Mode"),
        HANDLE_AV1_OPTION(DisableFrameEndUpdateCdf, OPT_TRI_STATE, "on/off disable_frame_end_update_cdf"),
#endif
        //HEVC
        HANDLE_HEVC_TILES(NumTileColumns,            OPT_UINT_16,    "0-maxU16: Number of tile columns (1 - default)"),
        HANDLE_HEVC_TILES(NumTileRows,               OPT_UINT_16,    "0-maxU16: Number of tile rows (1 - default)"),

        HANDLE_HEVC_PARAM(PicWidthInLumaSamples,     OPT_UINT_16,    "HEVC encoded picture width (SPS.pic_width_in_luma_samples)"),
        HANDLE_HEVC_PARAM(PicHeightInLumaSamples,    OPT_UINT_16,    "HEVC encoded picture height (SPS.pic_height_in_luma_samples)"),
        HANDLE_HEVC_PARAM(SampleAdaptiveOffset,      OPT_UINT_16,    "HEVC SAO public API: 0=unknown, 1=disabled, 2=luma, 4=chroma, 6=luma&chroma"),
        HANDLE_HEVC_PARAM(LCUSize,                   OPT_UINT_16,    "32 or 64 - overrides default value of Largest Coding Unit"),

        HANDLE_VP8_OPTION(EnableMultipleSegments, OPT_UINT_16, "0-maxU32"),
        HANDLE_VP8_OPTION(LoopFilterType, OPT_UINT_16, "0-maxU16"),
        HANDLE_VP8_OPTION(SharpnessLevel, OPT_UINT_16, "0-maxU16"),
        HANDLE_VP8_OPTION(NumTokenPartitions, OPT_UINT_16, "0-maxU16"),
        HANDLE_VP8_OPTION(WriteIVFHeaders, OPT_UINT_16, "0-maxU16"),
        HANDLE_VP8_OPTION(NumFramesForIVFHeader, OPT_UINT_32, "0-maxU32"),

        // mfxExtVP9Param
        HANDLE_VP9_PARAM(FrameWidth,          OPT_UINT_16, "0-maxU16"),
        HANDLE_VP9_PARAM(FrameHeight,         OPT_UINT_16, "0-maxU16"),
        HANDLE_VP9_PARAM(WriteIVFHeaders,     OPT_TRI_STATE, ""),
        HANDLE_VP9_PARAM(QIndexDeltaLumaDC,   OPT_INT_16, ""),
        HANDLE_VP9_PARAM(QIndexDeltaChromaAC, OPT_INT_16, ""),
        HANDLE_VP9_PARAM(QIndexDeltaChromaDC, OPT_INT_16, ""),
#if (MFX_VERSION >= 1029)
        HANDLE_VP9_PARAM(NumTileRows,         OPT_UINT_16, "0-maxU16: Number of tile rows (1 - default)"),
        HANDLE_VP9_PARAM(NumTileColumns,      OPT_UINT_16, "0-maxU16: Number of tile columns (1 - default)"),
#endif

        // mfxExtCodingOption2
        HANDLE_EXT_OPTION2(IntRefType,             OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION2(IntRefCycleSize,        OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION2(IntRefQPDelta,          OPT_INT_16,   ""),
        HANDLE_EXT_OPTION2(MaxFrameSize,           OPT_UINT_32,   ""),
        HANDLE_EXT_OPTION2(MaxSliceSize,           OPT_UINT_32,   ""),
        HANDLE_EXT_OPTION2(BitrateLimit,           OPT_TRI_STATE,   ""),
        HANDLE_EXT_OPTION2(MBBRC,                  OPT_TRI_STATE,   ""),
        HANDLE_EXT_OPTION2(ExtBRC,                 OPT_TRI_STATE,   ""),
        HANDLE_EXT_OPTION2(LookAheadDepth,         OPT_INT_16,      "how many frames ahead encoder analyze to choose quantization parameter"),
        HANDLE_EXT_OPTION2(AdaptiveI,              OPT_TRI_STATE,   "Adaptive B frames on/off"),
        HANDLE_EXT_OPTION2(AdaptiveB,              OPT_TRI_STATE,   "Adaptive B frames on/off"),
        HANDLE_EXT_OPTION2(LookAheadDS,            OPT_INT_16,      "Down sampling in look ahead: 0=default (let msdk choose), 1=1x, 2=2x, 3=4x"),
        HANDLE_EXT_OPTION2(Trellis,                OPT_INT_16,      "bitfield: 0=default, 1=off, 2=on for I frames, 4=on for P frames, 8=on for B frames"),
        HANDLE_EXT_OPTION2(RepeatPPS,              OPT_TRI_STATE,  ""),
        HANDLE_EXT_OPTION2(BRefType,               OPT_UINT_16,    "control usage of B frames as reference in AVC encoder: 0 - undef, 1 - B ref off, 2 - B ref pyramid"),
        HANDLE_EXT_OPTION2(NumMbPerSlice,          OPT_UINT_16,    "number of MBs per slice"),
        HANDLE_EXT_OPTION2(SkipFrame,              OPT_UINT_16,    "0=no_skip, 1=insert_dummy, 2=insert_nothing"),
        HANDLE_EXT_OPTION2(MinQPI,                 OPT_UINT_8,     "min QP for I-frames, 0 = default"),
        HANDLE_EXT_OPTION2(MaxQPI,                 OPT_UINT_8,     "max QP for I-frames, 0 = default"),
        HANDLE_EXT_OPTION2(MinQPP,                 OPT_UINT_8,     "min QP for P-frames, 0 = default"),
        HANDLE_EXT_OPTION2(MaxQPP,                 OPT_UINT_8,     "max QP for P-frames, 0 = default"),
        HANDLE_EXT_OPTION2(MinQPB,                 OPT_UINT_8,     "min QP for B-frames, 0 = default"),
        HANDLE_EXT_OPTION2(MaxQPB,                 OPT_UINT_8,     "max QP for B-frames, 0 = default"),
        HANDLE_EXT_OPTION2(FixedFrameRate,         OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION2(DisableDeblockingIdc,   OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION2(DisableVUI,             OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION2(BufferingPeriodSEI,     OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION2(UseRawRef,              OPT_TRI_STATE,  "on|off"),

        // mfxExtCodingOption3
        HANDLE_EXT_OPTION3(NumSliceI,              OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION3(NumSliceP,              OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION3(NumSliceB,              OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION3(QVBRQuality,            OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION3(IntRefCycleDist,        OPT_UINT_16,    ""),
        HANDLE_EXT_OPTION3(DirectBiasAdjustment,       OPT_TRI_STATE, "on|off"),
        HANDLE_EXT_OPTION3(GlobalMotionBiasAdjustment, OPT_TRI_STATE, "on|off"),
        HANDLE_EXT_OPTION3(MVCostScalingFactor,        OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION3(AspectRatioInfoPresent,     OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION3(OverscanInfoPresent,        OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION3(TimingInfoPresent,          OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION3(LowDelayHrd,                OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION3(BitstreamRestriction,       OPT_UINT_16,   ""),
        HANDLE_EXT_OPTION3(WeightedPred,               OPT_UINT_16,   "0=unknown, 1=default, 2=explicit"),
        HANDLE_EXT_OPTION3(WeightedBiPred,             OPT_UINT_16,   "0=unknown, 1=default, 2=explicit, 3=implicit"),
        HANDLE_EXT_OPTION3(PRefType,                   OPT_UINT_16,   "control usage of P frames as reference in AVC encoder in low delay mode: 0 - undef, 1 - simple, 2 - P ref pyramid"),
        HANDLE_EXT_OPTION3(FadeDetection,              OPT_TRI_STATE, "on|off"),
#if MFX_VERSION >= MFX_VERSION_NEXT
        HANDLE_EXT_OPTION3(DeblockingAlphaTcOffset,    OPT_INT_16,    "alpha deblocking parameter. In range[-12,12]. 0 by default"),
        HANDLE_EXT_OPTION3(DeblockingBetaOffset,       OPT_INT_16,    "beta deblocking parameter. In range[-12,12]. 0 by default"),
#endif
        HANDLE_EXT_OPTION3(GPB,                        OPT_TRI_STATE, "Generalized P/B"),
        HANDLE_EXT_OPTION3(EnableQPOffset,             OPT_TRI_STATE, ""),
        HANDLE_EXT_OPTION3(ScenarioInfo,               OPT_UINT_16,   "0=unknown, 7=MFX_SCENARIO_GAME_STREAMING, 8=MFX_SCENARIO_REMOTE_GAMING"),
        HANDLE_EXT_OPTION3(MaxFrameSizeI,              OPT_UINT_32,   ""),
        HANDLE_EXT_OPTION3(MaxFrameSizeP,              OPT_UINT_32,   ""),
        HANDLE_EXT_OPTION3(TransformSkip,              OPT_TRI_STATE, "HEVC transform_skip_enabled_flag"),
#if (MFX_VERSION >= 1027)
        HANDLE_EXT_OPTION3(TargetChromaFormatPlus1,    OPT_UINT_32,   "Encoding target chroma format + 1 (by default is source chroma format + 1)"),
        HANDLE_EXT_OPTION3(TargetBitDepthLuma,         OPT_UINT_32,   "Encoding target bit depth for luma samples (by default same as source one)"),
        HANDLE_EXT_OPTION3(TargetBitDepthChroma,       OPT_UINT_32,   "Encoding target bit depth for chroma samples (by default same as source one)"),
#endif
        HANDLE_EXT_OPTION3(BRCPanicMode,               OPT_TRI_STATE, "BRC panic mode control"),
        HANDLE_EXT_OPTION3(LowDelayBRC,                OPT_TRI_STATE, ""),
        HANDLE_EXT_OPTION3(AdaptiveMaxFrameSize,       OPT_TRI_STATE, ""),
        HANDLE_EXT_OPTION3(RepartitionCheckEnable,     OPT_TRI_STATE, "on/off Reapartition check for AVC encoder"),
        HANDLE_EXT_OPTION3(ExtBrcAdaptiveLTR,          OPT_TRI_STATE, "on/off Adaptive LTR"),

        // mfxExtCodingOptionDDI
        HANDLE_DDI_OPTION(IntraPredCostType,       OPT_UINT_16,    "1=SAD, 2=SSD, 4=SATD_HADAMARD, 8=SATD_HARR"),
        HANDLE_DDI_OPTION(MEInterpolationMethod,   OPT_UINT_16,    "1=VME4TAP, 2=BILINEAR, 4=WMV4TAP, 8=AVC6TAP"),
        HANDLE_DDI_OPTION(MEFractionalSearchType,  OPT_UINT_16,    "1=FULL, 2=HALF, 4=SQUARE, 8=HQ, 16=DIAMOND"),
        HANDLE_DDI_OPTION(MaxMVs,                  OPT_UINT_16,    ""),
        HANDLE_DDI_OPTION(SkipCheck,               OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(DirectCheck,             OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(BiDirSearch,             OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(MBAFF,                   OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(FieldPrediction,         OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(RefOppositeField,        OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(ChromaInME,              OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(WeightedPrediction,      OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(MVPrediction,            OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(DDI.IntraPredBlockSize,  OPT_UINT_16,    "mask of 1=4x4, 2=8x8, 4=16x16, 8=PCM"),
        HANDLE_DDI_OPTION(DDI.InterPredBlockSize,  OPT_UINT_16,    "mask of 1=16x16, 2=16x8, 4=8x16, 8=8x8, 16=8x4, 32=4x8, 64=4x4"),
        HANDLE_DDI_OPTION(BRCPrecision,            OPT_UINT_16,    "0=default=normal, 1=lowest, 2=normal, 3=highest"),
        HANDLE_DDI_OPTION(GlobalSearch,            OPT_UINT_16,    "0=default, 1=long, 2=medium, 3=short"),
        HANDLE_DDI_OPTION(LocalSearch,             OPT_UINT_16,    "0=default, 1=type, 2=small, 3=square, 4=diamond, 5=large diamond, 6=exhaustive, 7=heavy horizontal, 8=heavy vertical"),
        HANDLE_DDI_OPTION(EarlySkip,               OPT_UINT_16,    "0=default (let driver choose), 1=enabled, 2=disabled"),
        HANDLE_DDI_OPTION(LaScaleFactor,           OPT_UINT_16,    "0=default (let msdk choose), 1=1x, 2=2x, 4=4x"),
        // superseded by corresponding option in mfxExtCodingOption2
        HANDLE_DDI_OPTION(FractionalQP,            OPT_BOOL,       "enable fractional QP"),
        HANDLE_DDI_OPTION(StrengthN,               OPT_UINT_16,    "strength=StrengthN/100.0"),
        HANDLE_DDI_OPTION(NumActiveRefP,           OPT_UINT_16,    "0=default, max 16/32 for frames/fields"),
        HANDLE_DDI_OPTION(NumActiveRefBL0,         OPT_UINT_16,    "0=default, max 16/32 for frames/fields"),
        HANDLE_DDI_OPTION(NumActiveRefBL1,         OPT_UINT_16,    "0=default, max 16/32 for frames/fields"),
        HANDLE_DDI_OPTION(QpUpdateRange,           OPT_UINT_16,    ""),
        HANDLE_DDI_OPTION(RegressionWindow,        OPT_UINT_16,    ""),
        HANDLE_DDI_OPTION(LookAheadDependency,     OPT_UINT_16,    "LookAheadDependency < LookAhead"),
        HANDLE_DDI_OPTION(Hme,                     OPT_TRI_STATE,  "Hme on/off"),
        HANDLE_DDI_OPTION(DisablePSubMBPartition,  OPT_TRI_STATE,  "on=disabled 4x4 4x8 8x4 for P frames, off=enabled 4x4 4x8 8x4 for P frames"),
        HANDLE_DDI_OPTION(DisableBSubMBPartition,  OPT_TRI_STATE,  "on=disabled 4x4 4x8 8x4 for B frames, off=enabled 4x4 4x8 8x4 for B frames"),
        HANDLE_DDI_OPTION(Transform8x8Mode,        OPT_TRI_STATE,  "on|off. Only for AVC high profile"),
        HANDLE_DDI_OPTION(WeightedBiPredIdc,       OPT_UINT_16,    "0 - off, 1 - explicit (unsupported), 2 - implicit"),
        HANDLE_DDI_OPTION(DirectSpatialMvPredFlag, OPT_TRI_STATE,  "on=spatial, off=temporal"),
        HANDLE_DDI_OPTION(CabacInitIdcPlus1,       OPT_UINT_16,    "0-to use default value (depends on Target Usaeg), 1-cabacinitidc=0, 2-cabacinitidc=1,  etc"),
        HANDLE_DDI_OPTION(QpAdjust,                OPT_TRI_STATE,  "on/off - forces sps.QpAdjustment, default is off for all TUs"),
        HANDLE_DDI_OPTION(WriteIVFHeaders,         OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(RefreshFrameContext,     OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(ChangeFrameContextIdxForTS, OPT_TRI_STATE, ""),
        HANDLE_DDI_OPTION(SuperFrameForTS,         OPT_TRI_STATE,  ""),
        HANDLE_DDI_OPTION(TMVP,                    OPT_TRI_STATE,  "on/off temporal MV predictor"),

        //mfxExtEncoderCapability
        HANDLE_CAP_OPTION(MBPerSec,                OPT_BOOL,       "Query Encoder for Max MB Per Second"),

        //mfxExtEncoderResetOption
        HANDLE_ENCRESET_OPTION(StartNewSequence,   OPT_TRI_STATE,  "Start New Sequence"),

        HANDLE_EXT_MFE(MaxNumFrames, OPT_UINT_16, "Number of frames for MFE, pure DDI verification with single frame"),
        HANDLE_EXT_MFE(MFMode, OPT_UINT_16, "MFE mode, 0 - default, 1 - off, 2 - auto"),
        // Quant Matrix parameters
        {VM_STRING("-qm"), OPT_AUTO_DESERIAL, {&m_QuantMatrix}, VM_STRING("Quant Matrix structure")},


        // video signal
        HANDLE_VSIG_OPTION(VideoFormat,             OPT_UINT_16,  ""),
        HANDLE_VSIG_OPTION(VideoFullRange,          OPT_UINT_16,  ""),
        HANDLE_VSIG_OPTION(ColourDescriptionPresent,OPT_UINT_16,  ""),
        HANDLE_VSIG_OPTION(ColourPrimaries,         OPT_UINT_16,  ""),
        HANDLE_VSIG_OPTION(TransferCharacteristics, OPT_UINT_16,  ""),
        HANDLE_VSIG_OPTION(MatrixCoefficients,      OPT_UINT_16,  ""),

        //svc
        {VM_STRING("-svc_seq"), OPT_AUTO_DESERIAL, {&m_svcSeqDeserial}, VM_STRING("mfxExtSVCSeqDesc extended buffer - any member possible")},
        {VM_STRING("-svc_rate_ctrl"), OPT_AUTO_DESERIAL, {&m_svcRateCtrlDeserial}, VM_STRING("mfxExtSVCRateControl extended buffer - any member possible")},

        //special modes
        {VM_STRING("-yuvdump"), OPT_BOOL, {&m_bYuvDumpMode}, VM_STRING("used by mfx_transcoder team only")}
    };

    for (size_t i = 0; i < sizeof(options)/sizeof(options[0]); i++)
    {
        m_pOptions.push_back(options[i]);
    }

    m_bPsnrMode     = false;
    m_bSSIMMode     = false;
    m_bYuvDumpMode  = false;
    m_bResetParamsStart = false;

    m_BitRate = 0;
    m_MaxBitrate = 0;
    m_BitRateKbps = 0;
    m_MaxBitrateKbps = 0;
    m_QPI = m_QPP = m_QPB = 0;
    m_Accuracy = 0;
    m_Convergence =0;
    m_ICQQuality = 0;

    m_Interleaved = 0;
    m_Quality = 0;
    m_RestartInterval = 0;
    m_WinBRCMaxAvgBps = 0;
    m_WinBRCSize = 0;

    m_RenderType = MFX_ENC_RENDER;
}

MFXTranscodingPipeline::~MFXTranscodingPipeline()
{
    if (m_extVP9Segmentation->SegmentId)
    {
        delete[] m_extVP9Segmentation->SegmentId;
        m_extVP9Segmentation->SegmentId = nullptr;
    }

    std::for_each(m_ExtBufferVectorsContainer.begin(), m_ExtBufferVectorsContainer.end(),
        deleter<MFXExtBufferVector *>());
    m_ExtBufferVectorsContainer.clear();
}

template<typename T>
mfxStatus ParseSegmentMap(
    const vm_char* fname
    , const mfxFrameInfo frameInfo
    , T& pSeg)
{
    vm_file* parFile = vm_file_fopen(fname, VM_STRING("r"));
    MFX_CHECK(parFile != 0);

    mfxU16 defaultSize;
    if (std::is_same<T, MFXExtBufferPtr<mfxExtAV1Segmentation>>::value)
        defaultSize = MFX_AV1_SEGMENT_ID_BLOCK_SIZE_16x16;
    else
        defaultSize = MFX_VP9_SEGMENT_ID_BLOCK_SIZE_16x16;

    mfxU16 blockSize = pSeg->SegmentIdBlockSize > 0 ? pSeg->SegmentIdBlockSize : defaultSize;
    mfxU16 mapWidth = (frameInfo.Width + (blockSize - 1)) / blockSize;
    mfxU16 mapHeight = (frameInfo.Height + (blockSize - 1)) / blockSize;
    pSeg->NumSegmentIdAlloc = (mfxU32)(mapWidth * mapHeight);
    pSeg->SegmentId = new mfxU8[pSeg->NumSegmentIdAlloc];

    bool isInvalid = false;
    for (mfxU16 i = 0; !isInvalid && i < mapHeight; ++i)
    {
        vm_char sbuf[256], *pStr;

        pStr = vm_file_fgets(sbuf, sizeof(sbuf), parFile);
        if (!pStr)
            break;

        for (mfxU16 j = 0; j < mapWidth; ++j)
        {
            pSeg->SegmentId[i * mapWidth + j] = pStr[j] - '0';
            //Remove SegmentId check for AV1 to break option setting order restriction, and SegmentId will be checked in lib
            if (std::is_same<T, MFXExtBufferPtr<mfxExtAV1Segmentation>>::value)
                continue;

            if (pSeg->SegmentId[i * mapWidth + j] >= pSeg->NumSegments)
            {
                isInvalid = true;
                break;
            }
        }
    }

    vm_file_close(parFile);

    if (isInvalid)
      return MFX_ERR_UNKNOWN;
    else
      return MFX_ERR_NONE;
}

template <class T>
static T* RetrieveExtBuffer(MFXExtBufferVector const & extBuffers)
{
    T* pExt = nullptr;

    const MFXExtBufferPtrBase *ppExt = extBuffers.get_by_id(BufferIdOf<T>::id);

    if (!ppExt)
    {
        pExt = &mfx_init_ext_buffer(*new T());
    }
    else
    {
        pExt = reinterpret_cast<T*>(ppExt->get());
    }

    return pExt;
}

mfxStatus MFXTranscodingPipeline::ProcessCommandInternal(vm_char ** &argv, mfxI32 argc, bool bReportError)
{
    MFX_CHECK_SET_ERR(NULL != argv, PE_OPTION, MFX_ERR_NULL_PTR);
    MFX_CHECK_SET_ERR(0 != argc, PE_OPTION, MFX_ERR_UNKNOWN);

    vm_char **     argvEnd = argv + argc;
    mfxStatus      mfxres;
    mfxVideoParam *pMFXParams = &m_EncParams;

    //print mode;
    if (m_OptProc.GetPrint())
    {
        vm_string_printf(VM_STRING("  Encode specific parameters:\n"));
    }

    for (; argv < argvEnd; argv++)
    {
        int nPattern = 0;
        if ((0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("(-| )(m2|mpeg2|avc|h264|jpeg|vp8|h265|h263|vp9|av1)"), VM_STRING("target format")))) ||
            m_OptProc.Check(argv[0], VM_STRING("-CodecType"), VM_STRING("target format"),OPT_SPECIAL, VM_STRING("m2|mpeg2|avc|h264|jpeg|vp8|h265|h263|vp9|av1")))

        {
            if (!nPattern)
            {
                MFX_CHECK(argv + 1 != argvEnd);
                MFX_CHECK(0 != (nPattern = m_OptProc.Check(argv[1], VM_STRING("(-| )(m2|mpeg2|avc|h264|jpeg|vp8)"), VM_STRING(""))));
                argv++;
            }

            switch ((nPattern - 1) % 10)
            {
                case 0:
                case 1:
                {
                    pMFXParams->mfx.CodecId = MFX_CODEC_MPEG2;
                    break;
                }
                case 2:
                case 3:
                {
                    pMFXParams->mfx.CodecId = MFX_CODEC_AVC;
                    break;
                }
                case 4:
                {
                    pMFXParams->mfx.CodecId = MFX_CODEC_JPEG;
                    break;
                }

                case 5:
                {
                    #ifndef MFX_PIPELINE_SUPPORT_VP8
                        MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("vp8 encoder not supported")));
                    #else
                        pMFXParams->mfx.CodecId = MFX_CODEC_VP8;
                    #endif

                    break;
                }
                case 6:
                {
                    pMFXParams->mfx.CodecId = MFX_CODEC_HEVC;
                    break;
                }
                case 7:
                {
                    pMFXParams->mfx.CodecId = MFX_CODEC_H263;
                    break;
                }
                case 8:
                {
                    pMFXParams->mfx.CodecId = MFX_CODEC_VP9;
                    break;
                }
                case 9:
                {
                    pMFXParams->mfx.CodecId = MFX_CODEC_AV1;
                    break;
                }
            }
            continue;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-RefRaw"), VM_STRING("motion estimation on raw (input) frames")))
        {
            m_extCodingOptionsDDI->RefRaw = MFX_CODINGOPTION_ON;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-RefRec"), VM_STRING("motion estimation on reconstructed frames")))
        {
            m_extCodingOptionsDDI->RefRaw = MFX_CODINGOPTION_OFF;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-MfeFrames"), VM_STRING("Number of frames for MFE")))
        {
            MFX_PARSE_INT(m_extMFEParam->MaxNumFrames, argv[1]);
            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-dump_rec"), VM_STRING("dump reconstructed frames into YUV file"), OPT_FILENAME))
        {
            MFX_CHECK(argv + 1 != argvEnd);
            argv++;

            vm_string_strcpy_s(m_extDumpFiles->ReconFilename, MFX_ARRAY_SIZE(m_extDumpFiles->ReconFilename), argv[0]);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("--qp"), VM_STRING("Base QP for CQP mode."), OPT_INT_32))
        {
            MFX_CHECK(argv + 1 != argvEnd);
            MFX_PARSE_INT(m_QPI, argv[1]);
            argv++;
            m_QPB = m_QPP = m_QPI + 1;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("--bframes"), VM_STRING("Maximum number of consecutive b-frames"), OPT_INT_32))
        {
            MFX_CHECK(argv + 1 != argvEnd);
            MFX_PARSE_INT(pMFXParams->mfx.GopRefDist, argv[1]);
            argv++;
            pMFXParams->mfx.GopRefDist += 1;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("--preset"), VM_STRING("ultrafast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo"), OPT_STR))
        {
            MFX_CHECK(argv + 1 != argvEnd);
            argv++;
            if ( 0 == vm_string_strcmp(argv[0], VM_STRING("ultrafast")))
            {
                pMFXParams->mfx.TargetUsage = 7;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("veryfast")))
            {
                pMFXParams->mfx.TargetUsage = 7;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("faster")))
            {
                pMFXParams->mfx.TargetUsage = 6;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("fast")))
            {
                pMFXParams->mfx.TargetUsage = 5;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("medium")))
            {
                pMFXParams->mfx.TargetUsage = 4;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("slow")))
            {
                pMFXParams->mfx.TargetUsage = 3;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("slower")))
            {
                pMFXParams->mfx.TargetUsage = 2;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("veryslow")))
            {
                pMFXParams->mfx.TargetUsage = 1;
            }
            else if ( 0 == vm_string_strcmp(argv[0], VM_STRING("placebo")))
            {
                pMFXParams->mfx.TargetUsage = 1;
            }
            else
            {
                PipelineTrace((VM_STRING("Provided preset is not supported\n")));
                return MFX_ERR_UNKNOWN;
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("--no-b-pyramid"), VM_STRING("Do no use B-frames as references."), OPT_INT_32))
        {
            m_extCodingOptionsHEVC->BPyramid = MFX_CODINGOPTION_OFF;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("--b-pyramid"), VM_STRING("Use B-frames as references. Enabled by default."), OPT_INT_32))
        {
            m_extCodingOptionsHEVC->BPyramid = MFX_CODINGOPTION_ON;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-dump_enc_input"), VM_STRING("dump encode input frames into YUV file"), OPT_FILENAME))
        {
            MFX_CHECK(argv + 1 != argvEnd);
            argv++;
            vm_string_strcpy_s(m_extDumpFiles->InputFramesFilename, MFX_ARRAY_SIZE(m_extDumpFiles->InputFramesFilename), argv[0]);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-c"), VM_STRING("set CAVLC/CBAC mode: 1=CAVLC, 0=CABAC"), OPT_INT_32))
        {
            MFX_CHECK(argv + 1 != argvEnd);
            int nCavlc;
            MFX_PARSE_INT(nCavlc, argv[1])
            argv++;
            m_extCodingOptions->CAVLC = (mfxU16)(nCavlc ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-skip_frames"), VM_STRING("skip specified frames during encoding"), OPT_STR))
        {
            MFX_CHECK(argv + 1 != argvEnd);
            argv++;
#if defined(UNICODE) && ( defined(_WIN32) || defined(_WIN64) )
            std::vector<char> buffer;
            int size = WideCharToMultiByte(CP_UTF8,0,argv[0],-1,NULL,0,NULL,NULL);
            if ( size > 0 ) {
                buffer.resize(size);
                WideCharToMultiByte(CP_UTF8,0,argv[0],-1, (&buffer[0]), (int)buffer.size(),NULL,NULL);
            }
            std::string x(&buffer[0]);
#else
            std::string x = argv[0];
#endif
            std::string s(x.begin(), x.end());
            std::stringstream ss(s);
            std::string item;
            while ( std::getline(ss, item, ',') )
            {
                m_components[eREN].m_SkippedFrames.push_back(atoi(item.c_str()));
            }
            if(!m_extCodingOptions2->SkipFrame)
            m_extCodingOptions2->SkipFrame = 1;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-cabac"), VM_STRING("Turn on CABAC mode")))
        {
            m_extCodingOptions->CAVLC = MFX_CODINGOPTION_OFF;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-cbr"), VM_STRING("constant bitrate mode")))
        {
            m_EncParams.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-vbr"), VM_STRING("variable bitrate mode")))
        {
            m_EncParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-cqp|-ConstantQP"), VM_STRING("Constant QP mode (RateControlMethod=3), should be used along with -qpi, -qpp, -qpb")))
        {
            m_EncParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-avbr"), VM_STRING("Use the average variable bitrate control algorithm (RateControlMethod=4), should be used in cooperation with -b, -accuracy, -convergence")))
        {
            m_EncParams.mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-frame"), VM_STRING("[deprecated] use -s 0 for progressive mode encoding")))
        {
            m_inParams.nPicStruct = 0;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-field"), VM_STRING("[deprecated] use -s 3 for TFF + exctcoding options on encoding ")))
        {
            m_inParams.nPicStruct = 3; // TFF
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-no_output"), VM_STRING("no info")))
        {
            vm_string_strcpy(m_inParams.strDstFile, VM_STRING("out.tmp"));
            m_RenderType = MFX_FW_RENDER;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-o|-output|--output"), VM_STRING("")))
        {
            //file name that will be used for output after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            m_FileAfterReset = argv[1];

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-i|-input|--input"), VM_STRING("")))
        {
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            m_inFileAfterReset = argv[1];

            argv++;
        }
        else if (m_bResetParamsStart && (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-w|-width|-resize_w"), VM_STRING("")))))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(m_EncParams.mfx.FrameInfo.CropW, argv[1]);
            m_EncParams.mfx.FrameInfo.Width = mfx_align(m_EncParams.mfx.FrameInfo.CropW, 0x10);
            FILL_MASK_FROM_FIELD(m_EncParams.mfx.FrameInfo.CropW, 0);
            FILL_MASK_FROM_FIELD(m_EncParams.mfx.FrameInfo.Width, 0);

            m_bUseResizing = (nPattern == 3);

            argv++;
        }
        else if (m_bResetParamsStart && (0 != (nPattern = m_OptProc.Check(argv[0], VM_STRING("-h|-height|-resize_h"), VM_STRING("")))))
        {
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(m_EncParams.mfx.FrameInfo.CropH, argv[1]);
            //alignemt will be done during executing since now we dont know picture structure for sure
            m_EncParams.mfx.FrameInfo.Height = m_EncParams.mfx.FrameInfo.CropH;
            FILL_MASK_FROM_FIELD(m_EncParams.mfx.FrameInfo.CropH, 0);
            FILL_MASK_FROM_FIELD(m_EncParams.mfx.FrameInfo.Height, 0);

            m_bUseResizing = (nPattern == 3);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-resize_h"), VM_STRING("")))
        {
            //quite same as new w new h options

        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-resize_w"), VM_STRING("")))
        {

        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NalHrdConformance"), VM_STRING("")))
        {
            mfxU32 on;
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(on, argv[1]);

            mfxExtCodingOption *pExt = NULL;
            if (1 == on || 0 == on)
                pExt = RetrieveExtBuffer<mfxExtCodingOption>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->NalHrdConformance = (mfxU16)(on ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-RepartitionCheckEnable"), VM_STRING("")))
        {
            MFX_CHECK(1 + argv != argvEnd);
            mfxU16 value = MFX_CODINGOPTION_UNKNOWN;
            MFX_PARSE_STR_ON_OFF(value, argv[1]);
            mfxExtCodingOption3 *pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());
            pExt->RepartitionCheckEnable = value;
            m_ExtBuffers.get()->push_back(pExt);
            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-VuiVclHrdParameters"), VM_STRING("")))
        {
            mfxU32 on;
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(on, argv[1]);

            mfxExtCodingOption *pExt = NULL;

            if (1 == on || 0 == on)
                pExt = RetrieveExtBuffer<mfxExtCodingOption>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->VuiVclHrdParameters = (mfxU16)(on ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-VuiNalHrdParameters"), VM_STRING("")))
        {
            mfxU32 on;
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(on, argv[1]);

            mfxExtCodingOption *pExt = NULL;

            if (1 == on || 0 == on)
                pExt = RetrieveExtBuffer<mfxExtCodingOption>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->VuiNalHrdParameters = (mfxU16)(on ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-MaxFrameSize"), VM_STRING("")))
        {
            mfxU32 val;
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption2 *pExt = NULL;

            if (0 != val)
                pExt = RetrieveExtBuffer<mfxExtCodingOption2>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->MaxFrameSize = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-MaxFrameSizeI"), VM_STRING("")))
        {
            mfxU32 val;
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption3 *pExt = NULL;

            if (0 != val)
                pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->MaxFrameSizeI = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-MaxFrameSizeP"), VM_STRING("")))
        {
            mfxU32 val;
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption3 *pExt = NULL;

            if (0 != val)
                pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->MaxFrameSizeP = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-LowDelayBRC"), VM_STRING("0 - OFF, 1 - ON")))
        {
            mfxU32 on;
            //file name that will be used for input after reseting encoder
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(on, argv[1]);

            mfxExtCodingOption3 *pExt = NULL;
            if (1 == on || 0 == on)
                pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->LowDelayBRC = (mfxU16)(on ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
            m_ExtBuffers.get()->push_back(pExt);
            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-AdaptiveMaxFrameSize"), VM_STRING("on/1/16, off/0/32")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            mfxU16 value = MFX_CODINGOPTION_UNKNOWN;
            MFX_PARSE_STR_ON_OFF(value, argv[1]);
            mfxExtCodingOption3 *pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());

            pExt->AdaptiveMaxFrameSize = value;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-MaxSliceSize"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption2 *pExt = NULL;

            if (0 != val)
                pExt = RetrieveExtBuffer<mfxExtCodingOption2>(*m_ExtBuffers.get());
            else
                return MFX_ERR_UNKNOWN;

            pExt->MaxSliceSize = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-IntRefCycleDist"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption3 *pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());

            pExt->IntRefCycleDist = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-IntRefCycleSize"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption2 *pExt = RetrieveExtBuffer<mfxExtCodingOption2>(*m_ExtBuffers.get());

            pExt->IntRefCycleSize = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-IntRefType"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption2 *pExt = RetrieveExtBuffer<mfxExtCodingOption2>(*m_ExtBuffers.get());

            pExt->IntRefType = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumSliceI"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption3 *pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());

            pExt->NumSliceI = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumSliceP"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption3 *pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());

            pExt->NumSliceP = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumSliceB"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtCodingOption3 *pExt = RetrieveExtBuffer<mfxExtCodingOption3>(*m_ExtBuffers.get());

            pExt->NumSliceB = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-RecoveryPointSEI "), VM_STRING("")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            mfxU16 value = MFX_CODINGOPTION_UNKNOWN;
            if (!vm_string_stricmp(argv[1], VM_STRING("on")) || !vm_string_stricmp(argv[1], VM_STRING("1")) || !vm_string_stricmp(argv[1], VM_STRING("16")))
            {
                value = MFX_CODINGOPTION_ON;
            }
            else if (!vm_string_stricmp(argv[1], VM_STRING("off")) || !vm_string_stricmp(argv[1], VM_STRING("0")) || !vm_string_stricmp(argv[1], VM_STRING("32")))
            {
                value = MFX_CODINGOPTION_OFF;
            }
            else
            {
                MFX_CHECK_SET_ERR(!VM_STRING("Wrong OPT_TRI_STATE"), PE_CHECK_PARAMS, MFX_ERR_UNKNOWN);
            }

            mfxExtCodingOption *pExt = RetrieveExtBuffer<mfxExtCodingOption>(*m_ExtBuffers.get());

            pExt->RecoveryPointSEI = value;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-startnewsequence"), VM_STRING("")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            mfxU32 on;
            if (!vm_string_stricmp(argv[1], VM_STRING("on")) || !vm_string_stricmp(argv[1], VM_STRING("1")) || !vm_string_stricmp(argv[1], VM_STRING("16")))
            {
                on = MFX_CODINGOPTION_ON;
            }
            else if (!vm_string_stricmp(argv[1], VM_STRING("off")) || !vm_string_stricmp(argv[1], VM_STRING("0")) || !vm_string_stricmp(argv[1], VM_STRING("32")))
            {
                on = MFX_CODINGOPTION_OFF;
            }
            else
            {
                MFX_CHECK_SET_ERR(!VM_STRING("Wrong OPT_TRI_STATE"), PE_CHECK_PARAMS, MFX_ERR_UNKNOWN);
            }

            mfxExtEncoderResetOption *pExt = RetrieveExtBuffer<mfxExtEncoderResetOption>(*m_ExtBuffers.get());

            pExt->StartNewSequence = on;

            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-FrameWidth"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtVP9Param *pExt = RetrieveExtBuffer<mfxExtVP9Param>(*m_ExtBuffers.get());

            pExt->FrameWidth = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-FrameHeight"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtVP9Param *pExt = RetrieveExtBuffer<mfxExtVP9Param>(*m_ExtBuffers.get());

            pExt->FrameHeight = val;
            m_ExtBuffers.get()->push_back(pExt);

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumTileRows"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

#if (MFX_VERSION >= 1029)
            if (m_EncParams.mfx.CodecId == MFX_CODEC_VP9)
            {
                mfxExtVP9Param *pExt = RetrieveExtBuffer<mfxExtVP9Param>(*m_ExtBuffers.get());

                pExt->NumTileRows = val;
                m_ExtBuffers.get()->push_back(pExt);
            }
            else
#endif
            {
                mfxExtHEVCTiles *pExt = RetrieveExtBuffer<mfxExtHEVCTiles>(*m_ExtBuffers.get());

                pExt->NumTileRows = val;
                m_ExtBuffers.get()->push_back(pExt);
            }

            argv++;
        }
        else if (m_bResetParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumTileColumns"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

#if (MFX_VERSION >= 1029)
            if (m_EncParams.mfx.CodecId == MFX_CODEC_VP9)
            {
                mfxExtVP9Param *pExt = RetrieveExtBuffer<mfxExtVP9Param>(*m_ExtBuffers.get());

                pExt->NumTileColumns = val;
                m_ExtBuffers.get()->push_back(pExt);
            }
            else
#endif
            {
                mfxExtHEVCTiles *pExt = RetrieveExtBuffer<mfxExtHEVCTiles>(*m_ExtBuffers.get());

                pExt->NumTileColumns = val;
                m_ExtBuffers.get()->push_back(pExt);
            }

            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-reset_start"), VM_STRING("after reaching this frame, encoder will be reset with new parameters, followed after this command and before -reset_end"),OPT_SPECIAL, VM_STRING("frame number")))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(!m_bResetParamsStart);
            MFX_PARSE_INT(m_nResetFrame, argv[1]);

            m_bResetParamsStart = true;
            m_FileAfterReset.clear();
            m_inFileAfterReset.clear();

            //coping current parameters to buffer
            memcpy(&m_EncParamsOld, &m_EncParams, sizeof(m_EncParamsOld));
            m_OldBitRate     = m_BitRateKbps ? (m_BitRateKbps*1000) : m_BitRate;
            m_OldMaxBitrate  = m_MaxBitrateKbps ? (m_MaxBitrateKbps*1000) : m_MaxBitrate;
            m_OldPicStruct   = m_inParams.nPicStruct;
            m_OldQPI         = m_QPI;
            m_OldQPP         = m_QPP;
            m_OldQPB         = m_QPB;
            m_OldAccuracy    = m_Accuracy;
            m_OldConvergence = m_Convergence;

            // move ext buffers to temp variable
            m_ExtBuffersOld = m_ExtBuffers;
            m_ExtBuffers.reset(new MFXExtBufferVector());

            //mask is zeroed for each reset command
            MFX_ZERO_MEM(m_EncParamsMask);

            //clear resize flag
            m_bUseResizing = false;

            argv++;

            //copying reset params to str
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-reset_end"), VM_STRING("specifies end of reset related options")))
        {
            //has 1 arg time
            MFX_CHECK_SET_ERR(m_bResetParamsStart, PE_OPTION, MFX_ERR_UNSUPPORTED);

            resetEncCommand *pCmd = new resetEncCommand(this);
            pCmd->SetResetFileName(m_FileAfterReset);
            pCmd->SetResetInputFileName(m_inFileAfterReset);
            pCmd->SetVppResizing(m_bUseResizing);

            // attach ext buffers
            if (!m_ExtBuffers.get()->empty())
            {
                m_EncParams.ExtParam = &(m_ExtBuffers.get()->operator [](0));
                m_EncParams.NumExtParam = (mfxU16)m_ExtBuffers.get()->size();
                // move ext buffers to the container
                m_ExtBufferVectorsContainer.push_back(m_ExtBuffers.release());
                m_ExtBuffers = m_ExtBuffersOld;

                FILL_MASK_FROM_FIELD(m_EncParams.NumExtParam, 0);
                FILL_MASK_FROM_FIELD(m_EncParams.ExtParam, 0);
            }

            m_pFactories.push_back(new constnumFactory(1
                , new FrameBasedCommandActivator(pCmd, this)
                , new baseCmdsInitializer(m_nResetFrame,0, 0, 0, m_pRandom, &m_EncParams, &m_EncParamsMask)));

            //restoring params that were prior to -reset_start
            memcpy(&m_EncParams, &m_EncParamsOld, sizeof(m_EncParamsOld));

            m_BitRate             = m_OldBitRate;
            m_BitRateKbps         = m_OldBitRate/1000;
            m_MaxBitrate          = m_OldMaxBitrate;
            m_MaxBitrateKbps      = m_OldMaxBitrate/1000;
            m_inParams.nPicStruct = m_OldPicStruct;
            m_QPI                 = m_OldQPI;
            m_QPP                 = m_OldQPP;
            m_QPB                 = m_OldQPB;
            m_Accuracy            = m_OldAccuracy;
            m_Convergence         = m_OldConvergence;

            m_bResetParamsStart   = false;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-per_frame_start"), VM_STRING("encoder will send ext buffers to frame frame_number, parameters for ext buffers are listed next to frame_number and ended up with -per_frame_end"),OPT_SPECIAL, VM_STRING("frame_number")))
        {
            MFX_CHECK(1 + argv < argvEnd);
            MFX_CHECK(!m_bPerFrameParamsStart);
            MFX_PARSE_INT(m_nFrame, argv[1]);

            m_bPerFrameParamsStart = true;

            m_ExtBuffersPerFrame.reset(new MFXExtBufferVector());

            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-per_frame_end"), VM_STRING("specifies end of per-frame options")))
        {
            //has 1 arg time
            MFX_CHECK_SET_ERR(m_bPerFrameParamsStart, PE_OPTION, MFX_ERR_UNSUPPORTED);

            if (!m_ExtBuffersPerFrame->empty())
            {
                mfxExtBuffer** ppExt = m_ExtBuffersPerFrame->data();
                const mfxU16 numExtParam = m_ExtBuffersPerFrame->size();

                // move ext buffers to the container
                m_ExtBufferVectorsContainer.push_back(m_ExtBuffersPerFrame.release());

                m_pFactories.push_back(new constnumFactory(1
                    , new FrameBasedCommandActivator(new addExtBufferCommand(this), this)
                    , new baseCmdsInitializer(m_nFrame, 0., 0., 0, m_pRandom, 0, 0, (mfxExtBuffer*)ppExt, 0, numExtParam)));

                m_inParams.bCreatePerFrameExtBuf = true;
            }

            m_bPerFrameParamsStart = false;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-ref_list"), VM_STRING("reference list selection for givven frames interval"), OPT_SPECIAL
            , VM_STRING("start_frame end_frame NumRefFrameL0 NumRefFrameL1 ApplyLongTermIdx <preferredArray Lenght> <rejectedArray Lenght> <longTermdArray Lenght> [array:PreferredRefList:{FrameOrder}] [array:RejectedRefList:{FrameOrder}] [array:LongTermRefList:{<FrameOrder>[ <space> <LongTermIdx>} ]]")))
        {
            MFX_CHECK(3 + argv < argvEnd);
            mfxU32 nStartFrame, nFinishFrame;
            MFX_PARSE_INT(nStartFrame, argv[1]);
            MFX_PARSE_INT(nFinishFrame, argv[2]);

            DeSerializeHelper<mfxExtAVCRefListCtrl>  reflist;
            vm_char ** rollback_offset = argv;

            MFX_CHECK(reflist.DeSerialize(argv += 3, argvEnd, rollback_offset));

            m_pFactories.push_back(new constnumFactory(1
                , new FrameBasedCommandActivator(new addExtBufferCommand(this), this)
                , new baseCmdsInitializer(nStartFrame, 0.0, 0.0, 0, m_pRandom, NULL, NULL, (mfxExtBuffer*)&reflist)));

            m_pFactories.push_back(new constnumFactory(1
                , new FrameBasedCommandActivator(new removeExtBufferCommand(this), this)
                , new baseCmdsInitializer(nFinishFrame, 0.0, 0.0, 0, m_pRandom, NULL, NULL, NULL, BufferIdOf<mfxExtAVCRefListCtrl>::id)));

            //pipeline will create a encode wrapper according to this flag
            m_inParams.bCreateRefListControl = true;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-PWTPar"), VM_STRING("par-file for per-frame PredWeightTable + RefLists. Par format - int values; at new frame: <FrameOrder>,<log2WDy>,<log2WDc>,<L0Size>,<L1Size>\\n; then for each reference(L0Size + L1Size lines): <FrameOrder>,<PicStruct>,<WeightY>,<OffsetY>,<WeightCb>,<OffsetCb>,<WeightCr>,<OffsetCr>\\n. No comments/empty lines allowed."), OPT_FILENAME))
        {
            MFX_CHECK(++argv != argvEnd);
            vm_file* par_file = vm_file_fopen(argv[0], VM_STRING("r"));
            MFX_CHECK(par_file != 0);

            for(;;)
            {
                const mfxU16 Y = 0, Cb = 1, Cr = 2, W = 0, O = 1;
                mfxExtAVCRefLists rpl = { {MFX_EXTBUFF_AVC_REFLISTS, sizeof(mfxExtAVCRefLists)}, };
                mfxExtPredWeightTable pwt = { { MFX_EXTBUFF_PRED_WEIGHT_TABLE, sizeof(mfxExtPredWeightTable) }, };
                mfxExtBuffer* ppExtBuf[] = {&rpl.Header, &pwt.Header};
                mfxU32 frameOrder;
                mfxI16 wY, wC;
                vm_char sbuf[256], *pStr;

                pStr = vm_file_fgets(sbuf, sizeof(sbuf), par_file);
                if (!pStr)
                    break;

                MFX_CHECK(5 == vm_string_sscanf(pStr, VM_STRING("%u,%hu,%hu,%hu,%hu")
                    , &frameOrder
                    , &pwt.LumaLog2WeightDenom
                    , &pwt.ChromaLog2WeightDenom
                    , &rpl.NumRefIdxL0Active
                    , &rpl.NumRefIdxL1Active));

                wY = (1 << pwt.LumaLog2WeightDenom);
                wC = (1 << pwt.ChromaLog2WeightDenom);

                for (mfxU32 l = 0, n = rpl.NumRefIdxL0Active; l < 2; l++, n = rpl.NumRefIdxL1Active)
                {
                    for (mfxU32 i = 0; i < n; i++)
                    {
                        mfxExtAVCRefLists::mfxRefPic& ref = l ? rpl.RefPicList1[i] : rpl.RefPicList0[i];

                        MFX_CHECK(pStr = vm_file_fgets(sbuf, sizeof(sbuf), par_file));
                        MFX_CHECK(8 == vm_string_sscanf(pStr, VM_STRING("%u,%hu,%hi,%hi,%hi,%hi,%hi,%hi")
                            , &ref.FrameOrder
                            , &ref.PicStruct
                            , &pwt.Weights[l][i][Y][W]
                            , &pwt.Weights[l][i][Y][O]
                            , &pwt.Weights[l][i][Cb][W]
                            , &pwt.Weights[l][i][Cb][O]
                            , &pwt.Weights[l][i][Cr][W]
                            , &pwt.Weights[l][i][Cr][O]));
                        pwt.LumaWeightFlag[l][i] = pwt.Weights[l][i][Y][O] || pwt.Weights[l][i][Y][W] != wY;
                        pwt.ChromaWeightFlag[l][i] = pwt.Weights[l][i][Cb][O] || pwt.Weights[l][i][Cb][W] != wC
                                                  || pwt.Weights[l][i][Cr][O] || pwt.Weights[l][i][Cr][W] != wC;
                    }
                }

                m_pFactories.push_back(new constnumFactory(1
                    , new FrameBasedCommandActivator(new addExtBufferCommand(this), this)
                    , new baseCmdsInitializer(frameOrder, 0., 0., 0, m_pRandom, 0, 0, (mfxExtBuffer*)ppExtBuf, 0, MFX_ARRAY_SIZE(ppExtBuf))));
            }

            m_inParams.bCreatePerFrameExtBuf = true;
        }
        else HANDLE_BOOL_OPTION(m_bCreateDecode, VM_STRING("-enc:dec"), VM_STRING("decoding of encoded frames "));
        else HANDLE_INT_OPTION(m_extCodingOptionsSPSPPS->SPSId, VM_STRING("-spsid"), VM_STRING("set SPSId in mfxExcodingOptionSPSPPS structure"))
        else HANDLE_INT_OPTION(m_extCodingOptionsSPSPPS->PPSId, VM_STRING("-ppsid"), VM_STRING("set PPSId in mfxExcodingOptionSPSPPS structure"))
        else if (m_OptProc.Check(argv[0], VM_STRING("-avctemporallayers"), VM_STRING("add mfxExtAvcTemporalLayers buffer to mfxVideoParam"), OPT_SPECIAL, VM_STRING("BaseLayerID [array:Layer.Scale]")))
        {
            if (!m_bResetParamsStart) {
                MFX_CHECK(DeSerialize(*m_extAvcTemporalLayers.get(), ++argv, argvEnd));
                continue;
            }
            mfxExtAvcTemporalLayers *pExt = RetrieveExtBuffer<mfxExtAvcTemporalLayers>(*m_ExtBuffers.get());

            MFX_CHECK(DeSerialize(*pExt, ++argv, argvEnd));

            m_ExtBuffers.get()->push_back(pExt);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-vp9temporallayers"), VM_STRING("add mfxExtVP9TemporalLayers buffer to mfxVideoParam"), OPT_SPECIAL, VM_STRING("")))
        {
            if (!m_bResetParamsStart) {
                MFX_CHECK(DeSerialize(*m_extVP9TemporalLayers.get(), ++argv, argvEnd));
                continue;
            }
            mfxExtVP9TemporalLayers *pExt = RetrieveExtBuffer<mfxExtVP9TemporalLayers>(*m_ExtBuffers.get());

            MFX_CHECK(DeSerialize(*pExt, ++argv, argvEnd));

            m_ExtBuffers.get()->push_back(pExt);
        }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        else if (m_OptProc.Check(argv[0], VM_STRING("-temporallayers"), VM_STRING("add mfxExtTemporalLayers buffer to mfxVideoParam"), OPT_SPECIAL, VM_STRING("")))
        {
            if (!m_bResetParamsStart) {
                MFX_CHECK(DeSerialize(*m_extTemporalLayers.get(), ++argv, argvEnd));
                continue;
            }
            mfxExtTemporalLayers *pExt = RetrieveExtBuffer<mfxExtTemporalLayers>(*m_ExtBuffers.get());

            MFX_CHECK(DeSerialize(*pExt, ++argv, argvEnd));

            m_ExtBuffers.get()->push_back(pExt);
        }
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
        else if (m_OptProc.Check(argv[0], VM_STRING("-LoopFilterLevel"), VM_STRING(""), OPT_SPECIAL, VM_STRING("")))
        {
            MFX_CHECK(4 + argv < argvEnd);
            argv ++;
            for (mfxU8 i = 0; i < 4; i ++)
            {
                MFX_PARSE_INT(m_extVP8CodingOptions->LoopFilterLevel[i], argv[0]);
                argv ++;

            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-LoopFilterRefTypeDelta"), VM_STRING(""), OPT_SPECIAL, VM_STRING("")))
        {
            if (m_EncParams.mfx.CodecId == MFX_CODEC_VP8)
            {
                MFX_CHECK(4 + argv < argvEnd);
                for (mfxU8 i = 0; i < 4 && IsInteger(argv[1]); i++)
                {
                    argv++;
                    MFX_PARSE_INT(m_extVP8CodingOptions->LoopFilterRefTypeDelta[i], argv[0]);

                }
            }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            else if (m_EncParams.mfx.CodecId == MFX_CODEC_AV1)
            {
                MFX_CHECK(8 + argv < argvEnd);
                for (mfxU8 i = 0; i < 8 && IsInteger(argv[1]); i++)
                {
                    argv++;
                    MFX_PARSE_INT(m_extAV1AuxData->LoopFilter.RefDeltas[i], argv[0]);

                }
            }
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-LoopFilterMbModeDelta"), VM_STRING(""), OPT_SPECIAL, VM_STRING("")))
        {
            if (m_EncParams.mfx.CodecId == MFX_CODEC_VP8)
            {
                MFX_CHECK(4 + argv < argvEnd);
                for (mfxU8 i = 0; i < 4 && IsInteger(argv[1]); i++)
                {
                    argv++;
                    MFX_PARSE_INT(m_extVP8CodingOptions->LoopFilterMbModeDelta[i], argv[0]);

                }
            }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            else if (m_EncParams.mfx.CodecId == MFX_CODEC_AV1)
            {
                MFX_CHECK(2 + argv < argvEnd);
                for (mfxU8 i = 0; i < 2 && IsInteger(argv[1]); i++)
                {
                    argv++;
                    MFX_PARSE_INT(m_extAV1AuxData->LoopFilter.ModeDeltas[i], argv[0]);

                }
            }
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-SegmentQPDelta"), VM_STRING(""), OPT_SPECIAL, VM_STRING("")))
        {
            MFX_CHECK(4 + argv < argvEnd);
            argv ++;
            for (mfxU8 i = 0; i < 4; i ++)
            {
                MFX_PARSE_INT(m_extVP8CodingOptions->SegmentQPDelta[i], argv[0]);
                argv ++;

            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-CoeffTypeQPDelta"), VM_STRING(""), OPT_SPECIAL, VM_STRING("")))
        {
            MFX_CHECK(5 + argv < argvEnd);
            argv ++;
            for (mfxU8 i = 0; i < 5; i ++)
            {
                MFX_PARSE_INT(m_extVP8CodingOptions->CoeffTypeQPDelta[i], argv[0]);
                argv ++;

            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-VP8Version"), VM_STRING("Set provided value for Version in mfxExtVP8CodingOption structure"), OPT_INT_32, VM_STRING("")))
        {
            MFX_CHECK(1 + argv < argvEnd);
            argv ++;
            MFX_PARSE_INT(m_extVP8CodingOptions->Version, argv[0]);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-roi"), VM_STRING("Set provided values to mfxExtEncoderROI structure, implies MFX_ROI_MODE_PRIORITY  "), OPT_SPECIAL, VM_STRING("NumRect [Left Top Right Bottom Priority]{0..NumRect}")))
        {
            MFX_CHECK(1 + argv < argvEnd);
            argv ++;
            MFX_PARSE_INT(m_extEncoderRoi->NumROI, argv[0]);
            m_extEncoderRoi->ROIMode = MFX_ROI_MODE_PRIORITY;
            MFX_CHECK(m_extEncoderRoi->NumROI * 5 + argv < argvEnd);
            argv ++;
            for (mfxU8 i = 0; i < m_extEncoderRoi->NumROI; i ++)
            {
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Left, argv[0]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Top, argv[1]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Right, argv[2]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Bottom, argv[3]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Priority, argv[4]);
                argv += 5;
            }
            argv--;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-roi_dqp"), VM_STRING("the same as -roi  but implies MFX_ROI_MODE_QP_DELTA"), OPT_SPECIAL, VM_STRING("NumRect [Left Top Right Bottom Priority]{0..NumRect}")))
        {
            MFX_CHECK(1 + argv < argvEnd);
            argv++;
            MFX_PARSE_INT(m_extEncoderRoi->NumROI, argv[0]);
            m_extEncoderRoi->ROIMode = MFX_ROI_MODE_QP_DELTA;
            MFX_CHECK(m_extEncoderRoi->NumROI * 5 + argv < argvEnd);
            argv++;
            for (mfxU8 i = 0; i < m_extEncoderRoi->NumROI; i++)
            {
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Left, argv[0]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Top, argv[1]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Right, argv[2]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Bottom, argv[3]);
                MFX_PARSE_INT(m_extEncoderRoi->ROI[i].Priority, argv[4]);
                argv += 5;
            }
            argv--;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-vp9SegmentationBlockSize"), VM_STRING("Set size of block (NxN) for segmentation map: 8, 16, 32, 64"), OPT_SPECIAL, VM_STRING("integer")))
        {
            MFX_CHECK(++argv != argvEnd);
            MFX_PARSE_INT(m_extVP9Segmentation->SegmentIdBlockSize, argv[0]);
            if (!m_extVP9Segmentation->SegmentIdBlockSize)
                return MFX_ERR_UNKNOWN;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-vp9SegmentationQIDelta"), VM_STRING("Array of 8 digits containing quantization index deltas for 8 possible segments, each value [-255..255], 0 - disabled"), OPT_SPECIAL, VM_STRING("")))
        {
            const mfxU16 VP9E_MAX_SUPPORTED_SEGMENTS = 8;

            MFX_CHECK(VP9E_MAX_SUPPORTED_SEGMENTS + argv < argvEnd);
            m_extVP9Segmentation->NumSegments = 1;

            for (mfxU16 i = 0; i < VP9E_MAX_SUPPORTED_SEGMENTS; i++)
            {
                argv++;
                MFX_PARSE_INT(m_extVP9Segmentation->Segment[i].QIndexDelta, argv[0]);
                m_extVP9Segmentation->Segment[i].FeatureEnabled = MFX_VP9_SEGMENT_FEATURE_QINDEX;
                if (m_extVP9Segmentation->Segment[i].QIndexDelta)
                    m_extVP9Segmentation->NumSegments = i + 1;

            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-vp9SegmentationMap"), VM_STRING("File with segmentation map containing byte-aligned segment IDs [0..7] for each segment provided in raster order"), OPT_FILENAME, VM_STRING("filename")))
        {
            MFX_CHECK(++argv != argvEnd);

            MFX_CHECK_STS(ParseSegmentMap(argv[0], m_inParams.FrameInfo, m_extVP9Segmentation));
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-av1NumSegments"), VM_STRING("Number of segments, valid value [0..8] "), OPT_SPECIAL, VM_STRING("integer")))
        {
            const mfxU16 AV1E_MAX_SUPPORTED_SEGMENTS = 8;

            MFX_CHECK(++argv != argvEnd);

            mfxExtAV1Segmentation *pExt = m_bPerFrameParamsStart ?
                RetrieveExtBuffer<mfxExtAV1Segmentation>(*m_ExtBuffersPerFrame.get()) :
                m_extAV1Segmentation.get();

            MFX_PARSE_INT(pExt->NumSegments, argv[0]);

            //-av1SegmentationAltQIndex may already be set when -av1NumSegments is set, need to make sure no invalid values
            for (mfxU16 i = pExt->NumSegments; i < AV1E_MAX_SUPPORTED_SEGMENTS; i++)
            {
                MFX_CHECK(pExt->Segment[i].AltQIndex == 0);
                pExt->Segment[i].FeatureEnabled &= (~MFX_AV1_SEGMENT_FEATURE_ALT_QINDEX);
            }

            if (m_bPerFrameParamsStart)
                m_ExtBuffersPerFrame.get()->push_back(pExt);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-av1SegmentationBlockSize"), VM_STRING("Set size of block (NxN) for segmentation map: 16, 32, 64, 8"), OPT_SPECIAL, VM_STRING("integer")))
        {
            MFX_CHECK(++argv != argvEnd);

            mfxExtAV1Segmentation *pExt = m_bPerFrameParamsStart ?
                RetrieveExtBuffer<mfxExtAV1Segmentation>(*m_ExtBuffersPerFrame.get()) :
                m_extAV1Segmentation.get();

            MFX_PARSE_INT(pExt->SegmentIdBlockSize, argv[0]);

            if (m_bPerFrameParamsStart)
                m_ExtBuffersPerFrame.get()->push_back(pExt);

        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-av1SegmentationAltQIndex"), VM_STRING("Array of 8 digits containing quantization index deltas for 8 possible segments, each value [-255..255]"), OPT_SPECIAL, VM_STRING("")))
        {
            const mfxU16 AV1E_MAX_SUPPORTED_SEGMENTS = 8;
            MFX_CHECK(AV1E_MAX_SUPPORTED_SEGMENTS + argv < argvEnd);

            mfxExtAV1Segmentation *pExt = m_bPerFrameParamsStart ?
                RetrieveExtBuffer<mfxExtAV1Segmentation>(*m_ExtBuffersPerFrame.get()) :
                m_extAV1Segmentation.get();

            mfxI16 altQIndex = 0;
            //Always set AV1E_MAX_SUPPORTED_SEGMENTS entries for -av1SegmentationAltQIndex to keep consistency
            //Segmentation will not work without -av1NumSegments be set
            for (mfxU16 i = 0; i < AV1E_MAX_SUPPORTED_SEGMENTS; i++)
            {
                argv++;
                MFX_PARSE_INT(altQIndex, argv[0]);

                if (pExt->NumSegments > 0 && i >= pExt->NumSegments)
                {
                    //0 expected for placeholder value
                    MFX_CHECK(altQIndex == 0);
                }
                else
                {
                    pExt->Segment[i].AltQIndex = altQIndex;
                    pExt->Segment[i].FeatureEnabled |= MFX_AV1_SEGMENT_FEATURE_ALT_QINDEX;
                }
            }

            if (m_bPerFrameParamsStart)
                m_ExtBuffersPerFrame.get()->push_back(pExt);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-av1SegmentationMap"), VM_STRING("File with segmentation map containing byte-aligned segment IDs [0..7] for each segment provided in raster order"), OPT_FILENAME, VM_STRING("filename")))
        {
            MFX_CHECK(++argv != argvEnd);

            mfxExtAV1Segmentation *pExt = m_bPerFrameParamsStart ?
                RetrieveExtBuffer<mfxExtAV1Segmentation>(*m_ExtBuffersPerFrame.get()) :
                m_extAV1Segmentation.get();

            MFX_CHECK_STS(ParseSegmentMap(argv[0], m_inParams.FrameInfo, pExt));

            m_segMaps.emplace_back(pExt->SegmentId);

            if (m_bPerFrameParamsStart)
                m_ExtBuffersPerFrame.get()->push_back(pExt);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-dirty_rect"), VM_STRING(""), OPT_SPECIAL, VM_STRING("")))
        {
            MFX_CHECK(1 + argv < argvEnd);
            argv ++;
            MFX_PARSE_INT(m_extDirtyRect->NumRect, argv[0]);
            MFX_CHECK(m_extDirtyRect->NumRect * 4 + argv < argvEnd);
            argv ++;
            for (mfxU8 i = 0; i < m_extDirtyRect->NumRect; i ++)
            {
                MFX_PARSE_INT(m_extDirtyRect->Rect[i].Left, argv[0]);
                MFX_PARSE_INT(m_extDirtyRect->Rect[i].Top, argv[1]);
                MFX_PARSE_INT(m_extDirtyRect->Rect[i].Right, argv[2]);
                MFX_PARSE_INT(m_extDirtyRect->Rect[i].Bottom, argv[3]);
                argv += 4;
            }
            argv--;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-moving_rect"), VM_STRING(""), OPT_SPECIAL, VM_STRING("")))
        {
            MFX_CHECK(1 + argv < argvEnd);
            argv ++;
            MFX_PARSE_INT(m_extMoveRect->NumRect, argv[0]);
            MFX_CHECK(m_extMoveRect->NumRect * 6 + argv < argvEnd);
            argv ++;
            for (mfxU8 i = 0; i < m_extMoveRect->NumRect; i ++)
            {
                MFX_PARSE_INT(m_extMoveRect->Rect[i].DestLeft, argv[0]);
                MFX_PARSE_INT(m_extMoveRect->Rect[i].DestTop, argv[1]);
                MFX_PARSE_INT(m_extMoveRect->Rect[i].DestRight, argv[2]);
                MFX_PARSE_INT(m_extMoveRect->Rect[i].DestBottom, argv[3]);
                MFX_PARSE_INT(m_extMoveRect->Rect[i].SourceLeft, argv[4]);
                MFX_PARSE_INT(m_extMoveRect->Rect[i].SourceTop, argv[5]);
                argv += 6;
            }
            argv--;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-enc_frame_info"), VM_STRING("shows encoded picture info from mfxExtAVCEncodedFrameInfo structure, for particular frame, for ranges of frames, or for every frame"), OPT_SPECIAL
        , VM_STRING("[first [last]]"))) {
            int firstFrame = 0;
            int lastFrame  = 0;

            //no params
            mfxExtAVCEncodedFrameInfo avc_enc_info = {};
            mfx_init_ext_buffer(avc_enc_info);

            std::auto_ptr<addExtBufferCommand> pCmd (new addExtBufferCommand(this));
            std::auto_ptr<FrameBasedCommandActivator> pActivator (new FrameBasedCommandActivator (pCmd.get(), this, false));
            //expect no throw here
            pCmd->RegisterExtBuffer((mfxExtBuffer&)avc_enc_info);
            pCmd.release();


            if (argv + 1 <argvEnd ) {
                try
                {
                    //check that next option is number
                    lexical_cast(argv[1], firstFrame);
                    //will detach on next frameorder
                    lastFrame = firstFrame + 1;
                    //at least 1 parameter
                    argv ++;

                    if (argv + 1 < argvEnd ) {
                        try {
                            lexical_cast<int>(argv[1], lastFrame);
                            //2params casr
                            argv ++;
                        }
                        catch (...) {
                            //1 params case
                        }
                    }
                } catch (...) {
                    //no_param_case
                }
            }

            //0 or 1 parameters case
            pActivator->SetExecuteFrameNumber(firstFrame);
            m_commands.push_back(pActivator.get());
            pActivator.release();

            //2 parameters case
            if (lastFrame != 0) {
                //registering remover
                std::auto_ptr<removeExtBufferCommand> pCmd2 (new removeExtBufferCommand(this));
                std::auto_ptr<FrameBasedCommandActivator> pActivator2(new FrameBasedCommandActivator(pCmd2.get(), this, false));
                pCmd2->RegisterExtBuffer(BufferIdOf<mfxExtAVCEncodedFrameInfo>::id);
                pCmd2.release();

                pActivator2->SetExecuteFrameNumber(lastFrame);
                m_commands.push_back(pActivator2.get());
                pActivator2.release();
            }

            //pipeline will create a encode proxy according to this flag
            m_inParams.bCreateEncFrameInfo = true;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-EncodedOrderPar"), VM_STRING("par-file for EncodedOrder mode. Each line: <FrameOrder> <FrameType>"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.encOrderParFile, MFX_ARRAY_SIZE(m_inParams.encOrderParFile), argv[1]));
            argv++;
            m_inParams.useEncOrderParFile = true;
            m_EncParams.mfx.EncodedOrder = 1;
            m_inParams.EncodedOrder = 1;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-active_ref_lists_par"), VM_STRING("par-file for reference lists + reordering. Each line: <FrameOrder> <FrameType> <PicStruct> | 8 entries of <FrameOrder of a reference in L0 list> | 8 entries of <FrameOrder of a reference in L1 list> |"), OPT_FILENAME))
        {
            MFX_CHECK(1 + argv != argvEnd);
            MFX_CHECK(0 == vm_string_strcpy_s(m_inParams.encOrderRefListsParFile, MFX_ARRAY_SIZE(m_inParams.encOrderRefListsParFile), argv[1]));
            argv++;
            m_EncParams.mfx.EncodedOrder = 1;
            m_inParams.EncodedOrder = 1;

            m_inParams.bExternalRefListEncodedOrder = true;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-QPOffset"), VM_STRING("QP offset per pyramid layer (8 layers max)"), OPT_SPECIAL, VM_STRING("int[1..8]")))
        {
            MFX_CHECK(1 + argv < argvEnd);

            m_extCodingOptions3->EnableQPOffset = MFX_CODINGOPTION_ON;

            for (mfxU8 i = 0; i < 8 && argv+1 < argvEnd && argv[1][0] != VM_STRING('-'); i ++)
            {
                argv ++;
                MFX_PARSE_INT(m_extCodingOptions3->QPOffset[i], argv[0]);
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-NumRefActiveP"), VM_STRING("max number of active references per P-pyramid layer"), OPT_SPECIAL,
            VM_STRING("uint[1..8]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            for (mfxU8 i = 0; i < 8 && argv+1 < argvEnd && argv[1][0] != VM_STRING('-'); i ++)
            {
                argv ++;
                MFX_PARSE_INT(m_extCodingOptions3->NumRefActiveP[i], argv[0]);
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-NumRefActiveBL0"), VM_STRING("max number of active references in L0 per B-pyramid layer"), OPT_SPECIAL,
            VM_STRING("uint[1..8]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            for (mfxU8 i = 0; i < 8 && argv+1 < argvEnd && argv[1][0] != VM_STRING('-'); i ++)
            {
                argv ++;
                MFX_PARSE_INT(m_extCodingOptions3->NumRefActiveBL0[i], argv[0]);
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-NumRefActiveBL1"), VM_STRING("max number of active references in L1 per B-pyramid layer"), OPT_SPECIAL,
            VM_STRING("uint[1..8]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            for (mfxU8 i = 0; i < 8 && argv+1 < argvEnd && argv[1][0] != VM_STRING('-'); i ++)
            {
                argv ++;
                MFX_PARSE_INT(m_extCodingOptions3->NumRefActiveBL1[i], argv[0]);
            }
        }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        //AV1 VDEnc tiling parameters with array type
        else if (m_bPerFrameParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumTileRows"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtAV1Param *pExt = RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get());

            pExt->NumTileRows = val;
            m_ExtBuffersPerFrame.get()->push_back(pExt);

            argv++;
        }
        else if (m_bPerFrameParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumTileColumns"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtAV1Param *pExt = RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get());

            pExt->NumTileColumns = val;
            m_ExtBuffersPerFrame.get()->push_back(pExt);

            argv++;
        }
        else if (m_bPerFrameParamsStart && m_OptProc.Check(argv[0], VM_STRING("-NumTileGroups"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtAV1Param *pExt = RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get());

            pExt->NumTileGroups = val;
            m_ExtBuffersPerFrame.get()->push_back(pExt);

            argv++;
        }
        else if (m_bPerFrameParamsStart && m_OptProc.Check(argv[0], VM_STRING("-UniformTileSpacing"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtAV1Param *pExt = RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get());

            pExt->UniformTileSpacing = val;
            m_ExtBuffersPerFrame.get()->push_back(pExt);

            argv++;
        }
        else if (m_bPerFrameParamsStart && m_OptProc.Check(argv[0], VM_STRING("-ContextUpdateTileIdPlus1"), VM_STRING("")))
        {
            mfxU32 val;
            MFX_CHECK(1 + argv != argvEnd);
            MFX_PARSE_INT(val, argv[1]);

            mfxExtAV1Param *pExt = RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get());

            pExt->ContextUpdateTileIdPlus1 = val;
            m_ExtBuffersPerFrame.get()->push_back(pExt);

            argv++;
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-NumTilesPerTileGroup"), VM_STRING(""), OPT_SPECIAL, VM_STRING("uint[1..256]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            mfxExtAV1Param *pExt = m_bPerFrameParamsStart ?
                RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get()) :
                m_extAV1Param.get();

            for (mfxU8 i = 0; i < 256 && argv + 1 < argvEnd && argv[1][0] != VM_STRING('-'); i++)
            {
                argv++;
                MFX_PARSE_INT(pExt->NumTilesPerTileGroup[i], argv[0]);
            }

            if (m_bPerFrameParamsStart)
                m_ExtBuffersPerFrame.get()->push_back(pExt);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-TileWidthInSB"), VM_STRING(""), OPT_SPECIAL, VM_STRING("uint[1..128]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            mfxExtAV1Param *pExt = m_bPerFrameParamsStart ?
                RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get()) :
                m_extAV1Param.get();

            for (mfxU8 i = 0; i < 128 && argv + 1 < argvEnd && argv[1][0] != VM_STRING('-'); i++)
            {
                argv++;
                MFX_PARSE_INT(pExt->TileWidthInSB[i], argv[0]);
            }

            if (m_bPerFrameParamsStart)
                m_ExtBuffersPerFrame.get()->push_back(pExt);
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-TileHeightInSB"), VM_STRING(""), OPT_SPECIAL, VM_STRING("uint[1..128]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            mfxExtAV1Param *pExt = m_bPerFrameParamsStart ?
                RetrieveExtBuffer<mfxExtAV1Param>(*m_ExtBuffersPerFrame.get()) :
                m_extAV1Param.get();

            for (mfxU8 i = 0; i < 128 && argv + 1 < argvEnd && argv[1][0] != VM_STRING('-'); i++)
            {
                argv++;
                MFX_PARSE_INT(pExt->TileHeightInSB[i], argv[0]);
            }

            if (m_bPerFrameParamsStart)
                m_ExtBuffersPerFrame.get()->push_back(pExt);
        }
        //AV1 VDEnc Aux Data
        else if (m_OptProc.Check(argv[0], VM_STRING("-CdefYStrengths"), VM_STRING("0-63"), OPT_SPECIAL, VM_STRING("uint[1..8]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            for (mfxU8 i = 0; i < 8 && argv + 1 < argvEnd && argv[1][0] != VM_STRING('-'); i++)
            {
                argv++;
                MFX_PARSE_INT(m_extAV1AuxData->Cdef.CdefYStrengths[i], argv[0]);
            }
        }
        else if (m_OptProc.Check(argv[0], VM_STRING("-CdefUVStrengths"), VM_STRING("0-63"), OPT_SPECIAL, VM_STRING("uint[1..8]")))
        {
            MFX_CHECK(1 + argv != argvEnd);

            for (mfxU8 i = 0; i < 8 && argv + 1 < argvEnd && argv[1][0] != VM_STRING('-'); i++)
            {
                argv++;
                MFX_PARSE_INT(m_extAV1AuxData->Cdef.CdefUVStrengths[i], argv[0]);
            }
        }
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
        else if (MFX_ERR_UNSUPPORTED == (mfxres = ProcessOption(argv, argvEnd)))
        {
            //check with base pipeline parser
            vm_char **myArgv = argv;
            if (MFX_ERR_UNSUPPORTED != (mfxres = MFXDecPipeline::ProcessCommandInternal( argv
                                                                                       , (mfxI32)(argvEnd - argv)
                                                                                       , false)))
            {
                MFX_CHECK_STS(mfxres);
            }else
            {
                //check whether base pipeline process at least one option
                if (myArgv == argv)
                {
                    MFX_TRACE_AT_EXIT_IF( MFX_ERR_UNSUPPORTED
                        , !bReportError
                        , PE_OPTION
                        , (VM_STRING("ERROR: Unknown option: %s\n"), argv[0]));
                }
                //decreasing pointer because baseclass parser points to first unknown option
                argv--;
            }
        }else
        {
            MFX_CHECK_STS(mfxres);
        }
    }

#if defined(_WIN32) || defined(_WIN64)
    //restore after hack for JPEG
    if (m_EncParams.mfx.CodecId != MFX_CODEC_JPEG)
    {
        if (m_inParams.FrameInfo.FourCC == DXGI_FORMAT_AYUV)
        {
            m_inParams.FrameInfo.FourCC = MFX_FOURCC_AYUV;
        }

        ComponentsContainer::iterator component;

        for (component = m_components.begin(); component != m_components.end(); component++)
        {
            if (!component->m_ShortName.compare(tstring(VM_STRING("enc")))
                || !component->m_ShortName.compare(tstring(VM_STRING("dec"))))
            {
                if (component->m_params.mfx.FrameInfo.FourCC == DXGI_FORMAT_AYUV)
                {
                    component->m_params.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
                }
            }
        }
    }
#endif

    // set enc:shift if it is not set explicitly
    if (m_components[eREN].m_params.mfx.FrameInfo.Shift == (mfxU16)-1)
    {
#if (MFX_VERSION >= 1027)
        if (   (m_components[eREN].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_Y210)
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            || (m_components[eREN].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_Y216)
            || (m_components[eREN].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_Y416)
            || (m_components[eREN].m_params.mfx.FrameInfo.FourCC == MFX_FOURCC_P016)
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
           )
        {
            m_components[eREN].m_params.mfx.FrameInfo.Shift = 1;
        }
        else
#endif // #if (MFX_VERSION >= 1027)
        {
            m_components[eREN].m_params.mfx.FrameInfo.Shift = 0;
        }
    }

    return MFX_ERR_NONE;
}

int convert_type_nbits(mfxU8 nType)
{
    switch (nType)
    {
        case OPT_INT_32:
        {
            return 32;
        }
        case OPT_UINT_16:
        {
            return  16;
        }
        case OPT_UINT_8:
        {
            return  8;
        }
        case OPT_64F:
        {
            return 64;
        }
        default:
            return 0/*MFX_ERR_UNSUPPORTED*/;
    }
}

mfxStatus MFXTranscodingPipeline::ProcessOption(vm_char **&argv, vm_char **argvend)
{
    OptContainer opt, *pOption = &opt;
    mfxStatus sts = MFX_ERR_UNSUPPORTED;
    SerialNode *pNode = NULL;
    OptIter i;

    for (i = m_pOptions.begin(); i != m_pOptions.end(); ++i)
    {
        opt = *i;
        if (opt.nType == OPT_AUTO_DESERIAL)
        {
            pNode = reinterpret_cast<SerialNode*>(opt.pTarget);
        }
        if (m_OptProc.Check(argv[0], opt.opt_pattern, opt.description, opt.nType, NULL, &pNode))
        {

            mfxU32 codecId = m_EncParams.mfx.CodecId;
            if (codecId && opt.codecId && codecId != opt.codecId)
            {
                continue;
            }
            sts = MFX_ERR_NONE;
            break;
        }
    }

    if (sts != MFX_ERR_NONE)
    {
        return sts;
    }

    if (pOption->nType != OPT_COD)
    {
        switch (pOption->nType)
        {
            case OPT_BOOL:
            {
                *pOption->pTargetBool = true;
                break;
            }
            default:
            {
                switch (pOption->nType)
                {
                    case OPT_AUTO_DESERIAL:
                    {
                        break;
                    }
                    default:
                    {
                        MFX_CHECK_SET_ERR(argvend != 1 + argv, PE_CHECK_PARAMS, MFX_ERR_UNKNOWN);
                    }
                }

                try
                {
                    switch (pOption->nType)
                    {
                        case OPT_UINT_64:
                        {
                            lexical_cast(argv[1], *pOption->pTargetUInt64);
                            break;
                        }
                        case OPT_UINT_32:
                        {
                            lexical_cast(argv[1], *pOption->pTargetUInt32);
                            break;
                        }
                        case OPT_INT_32:
                        {
                            lexical_cast(argv[1], *pOption->pTargetInt32);
                            break;
                        }
                        case OPT_UINT_16:
                        {
                            lexical_cast(argv[1], *pOption->pTargetUInt16);
                            break;
                        }
                        case OPT_UINT_8:
                        {
                            lexical_cast(argv[1], *pOption->pTargetUInt8);
                            break;
                        }
                        case OPT_INT_16:
                        {
                            lexical_cast(argv[1], *pOption->pTargetInt16);
                            break;
                        }
                        case OPT_INT_8:
                        {
                            lexical_cast(argv[1], *pOption->pTargetInt8);
                            break;
                        }
                        case OPT_64F:
                        {
                            lexical_cast(argv[1], *pOption->pTargetF64);
                            break;
                        }
                        case OPT_STR:
                        {
                            memcpy(pOption->pTarget, argv, sizeof(void*));
                            break;
                        }
                        case OPT_TRI_STATE:
                        {
                            mfxU16& rTarget = *(mfxU16 *)pOption->pTarget;
                            if (!vm_string_stricmp(argv[1], VM_STRING("on")) || !vm_string_stricmp(argv[1], VM_STRING("1")) || !vm_string_stricmp(argv[1], VM_STRING("16")))
                            {
                                rTarget = MFX_CODINGOPTION_ON;
                            }
                            else if (!vm_string_stricmp(argv[1], VM_STRING("off")) || !vm_string_stricmp(argv[1], VM_STRING("0")) || !vm_string_stricmp(argv[1], VM_STRING("32")))
                            {
                                rTarget = MFX_CODINGOPTION_OFF;
                            }
                            else
                            {
                                MFX_CHECK_SET_ERR(!VM_STRING("Wrong OPT_TRI_STATE"), PE_CHECK_PARAMS, MFX_ERR_UNKNOWN);
                            }
                            break;
                        }
                        case OPT_AUTO_DESERIAL :
                        {
                            //TODO: support for multiple parameters
                            tstring error;
                            try
                            {
                                pNode->Parse(argv[1]);
                            }
                            catch (tstring & e)
                            {
                                error = e;
                            }
                            MFX_CHECK_TRACE(error.empty(), error.c_str());

                            break;

                        }
                        default:
                            return MFX_ERR_UNSUPPORTED;
                    }
                }
                catch(tstring & lcast_err)
                {
                    MFX_TRACE_AND_EXIT( MFX_ERR_UNKNOWN, (lcast_err.c_str()));
                }
                argv++;
                break;
            }
        }
    }

    if (NULL != pOption->pHandler)
        pOption->pHandler->Call();
    else
    {
        FILL_MASK(pOption->nType, pOption->pTarget);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXTranscodingPipeline::ApplyJpegParams()
{
    mfxVideoParam *pMFXParams = &m_EncParams;

    //rest of fields that matches other encoder options should be skipped by mfx_encode_init
    pMFXParams->mfx.Quality = m_Quality;
    pMFXParams->mfx.Interleaved = m_Interleaved;
    pMFXParams->mfx.RestartInterval = m_RestartInterval;

    FILL_MASK_FROM_FIELD(pMFXParams->mfx.Quality, 0);
    FILL_MASK_FROM_FIELD(pMFXParams->mfx.Interleaved, 0);
    FILL_MASK_FROM_FIELD(pMFXParams->mfx.RestartInterval, 0);

    return MFX_ERR_NONE;
}

mfxStatus MFXTranscodingPipeline::ApplyBitrateParams()
{
    mfxVideoParam *pMFXParams = &m_EncParams;

    //set FrameInfo.PicStruct
    pMFXParams->mfx.FrameInfo.PicStruct = Convert_CmdPS_to_MFXPS(m_inParams.nPicStruct);
    if (m_inParams.nPicStruct != PIPELINE_PICSTRUCT_NOT_SPECIFIED)
    {
        FILL_MASK_FROM_FIELD(pMFXParams->mfx.FrameInfo.PicStruct, 0);
    }

    //set m_ExtParamsBuffer.FramePicture
    m_extCodingOptions->FramePicture = Convert_CmdPS_to_ExtCO(m_inParams.nPicStruct);
    //todo for test cases with deinterlacing this may need to be changed
    m_components[eVPP].m_extCO = m_components[eDEC].m_extCO = m_extCodingOptions->FramePicture;

    // if -b and -TargetKbps are mixed, prefer -b
    if (m_BitRate) m_BitRateKbps = 0;
    // set bitrate
    pMFXParams->mfx.TargetKbps = (mfxU16)IPP_MIN(65535, m_BitRateKbps ? m_BitRateKbps : (m_BitRate/1000));

    // if -bm and -MaxKbps are mixed, prefer -bm
    if (m_MaxBitrate) m_MaxBitrateKbps = 0;
    //maxbitrate could be equal to target in VBR and CBR mode
    pMFXParams->mfx.MaxKbps = (mfxU16)IPP_MIN(65535, m_MaxBitrateKbps ? m_MaxBitrateKbps : (m_MaxBitrate/1000));

    //however VBR mode set only if maxbitrate is higher or rate control specified directly
    if (pMFXParams->mfx.MaxKbps > pMFXParams->mfx.TargetKbps &&
        pMFXParams->mfx.RateControlMethod != MFX_RATECONTROL_VCM &&
        pMFXParams->mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD &&
        pMFXParams->mfx.RateControlMethod != MFX_RATECONTROL_QVBR)
    {
        pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    }

    if (m_QPI || m_QPP || m_QPB || pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        pMFXParams->mfx.QPI = m_QPI;
        pMFXParams->mfx.QPP = m_QPP;
        pMFXParams->mfx.QPB = m_QPB;
    }

    if (m_Accuracy || m_Convergence || pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
        pMFXParams->mfx.Accuracy = m_Accuracy;
        pMFXParams->mfx.Convergence = m_Convergence;
    }
    if ((m_WinBRCMaxAvgBps || m_WinBRCSize) && (pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_LA || pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD || pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_VBR || pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_QVBR))
    {
        m_extCodingOptions3->WinBRCSize  = m_WinBRCSize;
        m_extCodingOptions3->WinBRCMaxAvgKbps = (mfxU16)IPP_MIN(65535, m_WinBRCMaxAvgBps/1000);
    }

    if (m_ICQQuality || pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_ICQ || pMFXParams->mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        if (pMFXParams->mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
            pMFXParams->mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)
            pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_ICQ;
        if (m_extCodingOptions2->LookAheadDepth)
            pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_LA_ICQ;
        pMFXParams->mfx.ICQQuality = m_ICQQuality;
    }

    if (!pMFXParams->mfx.RateControlMethod)
    {
        pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    }

    FILL_MASK_FROM_FIELD(pMFXParams->mfx.TargetKbps, 0);
    FILL_MASK_FROM_FIELD(pMFXParams->mfx.MaxKbps, 0);
    FILL_MASK_FROM_FIELD(pMFXParams->mfx.RateControlMethod, 0);

    return MFX_ERR_NONE;
}

mfxStatus MFXTranscodingPipeline::CheckParams()
{
    mfxVideoParam *pMFXParams = &m_EncParams;

    MFX_CHECK_SET_ERR(pMFXParams->mfx.CodecId, PE_NO_TARGET_CODEC, MFX_ERR_UNKNOWN);

    // check for non-zero parameters in extension buffers
    if (!m_extCodingOptions.IsZero())
        m_components[eREN].m_extParams.push_back(m_extCodingOptions);

    if (!m_extCodingOptions2.IsZero())
        m_components[eREN].m_extParams.push_back(m_extCodingOptions2);
    if (!m_extCodingOptions3.IsZero())
        m_components[eREN].m_extParams.push_back(m_extCodingOptions3);

    if (!m_extCodingOptionsDDI.IsZero())
        m_components[eREN].m_extParams.push_back(m_extCodingOptionsDDI);

    if (!m_extDumpFiles.IsZero())
        m_components[eREN].m_extParams.push_back(m_extDumpFiles);

    if (!m_extVideoSignalInfo.IsZero())
        m_components[eREN].m_extParams.push_back(m_extVideoSignalInfo);

    if (!m_extCodingOptionsHEVC.IsZero() && pMFXParams->mfx.CodecId == MFX_CODEC_HEVC)
        m_components[eREN].m_extParams.push_back(m_extCodingOptionsHEVC);

    if (!m_extCodingOptionsAV1E.IsZero() && pMFXParams->mfx.CodecId == MFX_CODEC_AV1 && pMFXParams->mfx.LowPower == MFX_CODINGOPTION_OFF)
        m_components[eREN].m_extParams.push_back(m_extCodingOptionsAV1E);

    if (!m_extHEVCTiles.IsZero())
        m_components[eREN].m_extParams.push_back(m_extHEVCTiles);

    if (!m_extHEVCParam.IsZero())
        m_components[eREN].m_extParams.push_back(m_extHEVCParam);

    if (!m_extVP8CodingOptions.IsZero())
        m_components[eREN].m_extParams.push_back(m_extVP8CodingOptions);

    if (!m_extVP9Param.IsZero())
        m_components[eREN].m_extParams.push_back(m_extVP9Param);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    if (!m_extAV1Param.IsZero() && pMFXParams->mfx.CodecId == MFX_CODEC_AV1)
        m_components[eREN].m_extParams.push_back(m_extAV1Param);
    if (!m_extAV1AuxData.IsZero() && pMFXParams->mfx.CodecId == MFX_CODEC_AV1)
        m_components[eREN].m_extParams.push_back(m_extAV1AuxData);
    if (!m_extAV1Segmentation.IsZero() && pMFXParams->mfx.CodecId == MFX_CODEC_AV1)
        m_components[eREN].m_extParams.push_back(m_extAV1Segmentation);
#endif
    if (!m_extVP9Segmentation.IsZero())
        m_components[eREN].m_extParams.push_back(m_extVP9Segmentation);

    if (!m_extEncoderRoi.IsZero())
        m_components[eREN].m_extParams.push_back(m_extEncoderRoi);

    if (!m_extDirtyRect.IsZero())
        m_components[eREN].m_extParams.push_back(m_extDirtyRect);

    if (!m_extMoveRect.IsZero())
        m_components[eREN].m_extParams.push_back(m_extMoveRect);

    if (!m_extCodingOptionsSPSPPS.IsZero())
        m_components[eREN].m_extParams.push_back(m_extCodingOptionsSPSPPS);

    if (!m_extAvcTemporalLayers.IsZero())
        m_components[eREN].m_extParams.push_back(m_extAvcTemporalLayers);

    if (!m_extVP9TemporalLayers.IsZero())
        m_components[eREN].m_extParams.push_back(m_extVP9TemporalLayers);

    if (!m_svcSeq.IsZero())
        m_components[eREN].m_extParams.push_back(m_svcSeq);

    if (!m_svcRateCtrl.IsZero())
        m_components[eREN].m_extParams.push_back(m_svcRateCtrl);

    if (!m_extCodingOptionsQuantMatrix.IsZero())
        m_components[eREN].m_extParams.push_back(m_extCodingOptionsQuantMatrix);

    if (!m_extEncoderCapability.IsZero())
        m_components[eREN].m_extParams.push_back(m_extEncoderCapability);

    if (!m_extEncoderReset.IsZero())
        m_components[eREN].m_extParams.push_back(m_extEncoderReset);
    if (!m_extMFEParam.IsZero())
        m_components[eREN].m_extParams.push_back(m_extMFEParam);

#if (MFX_VERSION >= MFX_VERSION_NEXT)
    if (!m_extTemporalLayers.IsZero())
        m_components[eREN].m_extParams.push_back(m_extTemporalLayers);
#endif

    switch (pMFXParams->mfx.CodecId)
    {
    case MFX_CODEC_JPEG :
        MFX_CHECK_STS(ApplyJpegParams());
        break;
    default:
        MFX_CHECK_STS(ApplyBitrateParams());
        break;
    }

    //TODO: avoid such hacks - saving async_depth params
    mfxFrameInfo info = m_components[eREN].m_params.mfx.FrameInfo;
    mfxU16 ad = m_components[eREN].m_params.AsyncDepth;
    memcpy(&m_components[eREN].m_params, &m_EncParams, sizeof(m_EncParams));

    m_components[eREN].m_params.AsyncDepth = ad;
    m_components[eREN].m_params.mfx.FrameInfo.ChromaFormat = info.ChromaFormat;
    m_components[eREN].m_params.mfx.FrameInfo.FourCC = info.FourCC;
    m_components[eREN].m_params.mfx.FrameInfo.Shift = info.Shift;
    m_components[eREN].AssignExtBuffers();

    //buffersize provided by user and it shouldn't be overwritten by Query or GetVideoParam
    //if (m_EncParams.mfx.BufferSizeInKB != 0)
    //{
    //    m_inParams.encodeExtraParams.nBufferSizeInKB = m_EncParams.mfx.BufferSizeInKB;
    //}

    if (!vm_string_strlen(m_inParams.strSrcFile))
    {
        //filename for layer should be specified if no input file provided
        for (size_t i = 0; i < m_filesForDependency.size(); i++)
        {
            tstringstream strm;
            strm<<VM_STRING("File Name (.InputFile) for dependency layer[") << i << VM_STRING("] should be specified");
            MFX_CHECK_TRACE(!m_filesForDependency[i].empty(), strm.str().c_str());
        }
    }

    return MFXDecPipeline::CheckParams();
}

bool MFXTranscodingPipeline::IsMultiReaderEnabled()
{
    //check vpp enabling if input files specified in par file - we dont support vpp
    bool bMultiReaderEnabled = false;
    for (size_t i = 0; i < m_filesForDependency.size(); i++)
    {
        if (!m_filesForDependency[i].empty())
        {
            bMultiReaderEnabled = true;
            break;
        }
    }
    return bMultiReaderEnabled;
}

mfxStatus MFXTranscodingPipeline::CreateVPP()
{
    bool bSvcVpp = !IsMultiReaderEnabled();

    if (bSvcVpp)
    {
        bSvcVpp = false;

        mfxFrameInfo &info = m_inParams.FrameInfo;
        for (size_t i = 0; i < MFX_ARRAY_SIZE(m_svcSeq->DependencyLayer) && (MFX_CODINGOPTION_ON == m_svcSeq->DependencyLayer[i].Active); i++)
        {
            mfxExtSVCDepLayer &dep_info = m_svcSeq->DependencyLayer[i];
            if (info.Width != dep_info.Width || info.Height != dep_info.Height)
            {
                PrintInfo(VM_STRING("Vpp.ENABLED"), VM_STRING("source=(%dx%d) but SVC.DependencyLayer[%d]=(%dx%d)"),
                    info.Width, info.Height, i, dep_info.Width, dep_info.Height);
                m_inParams.bUseVPP = true;
                bSvcVpp = true;
            }
        }
    }

    MFX_CHECK_STS(MFXDecPipeline::CreateVPP());

    if (bSvcVpp)
    {
        MFX_CHECK_POINTER(m_pVPP = m_pFactory->CreateVPP(make_wrapper(VPP_SVC, m_pVPP)));

        //only extsequencedescription required
        MFXExtBufferVector :: iterator it = std::find_if(m_components[eREN].m_extParams.begin()
            , m_components[eREN].m_extParams.end()
            , std::bind2nd(MFXExtBufferCompareByID<mfxExtBuffer>(), (mfxU32)BufferIdOf<mfxExtSVCSeqDesc>::id));

        //looks incorrect calling sequence dueto not properly assigned ext buffers
        MFX_CHECK(it != m_components[eREN].m_extParams.end());

        m_components[eVPP].m_extParams.merge( it, it + 1);
        m_components[eVPP].AssignExtBuffers();
    }


    return MFX_ERR_NONE;
}

mfxStatus MFXTranscodingPipeline::CreateYUVSource()
{
    if (IsMultiReaderEnabled())
    {
        std::unique_ptr<MultiDecode> pDecode (new MultiDecode());
        for (size_t i = 0; i < m_filesForDependency.size() && !m_filesForDependency[i].empty(); i++)
        {
            //TODO: for now it is impossible to have several mfx_decoders, since they require separate sessions
            //for SVC enc we use only YUV decoder that is not a part of session

            //input resolution for particular yuv reader
            m_inParams.FrameInfo.Width   = m_svcSeq.get()->DependencyLayer[i].CropW ?
                                           m_svcSeq.get()->DependencyLayer[i].CropW :
                                           m_svcSeq.get()->DependencyLayer[i].Width;

            m_inParams.FrameInfo.Height  = m_svcSeq.get()->DependencyLayer[i].CropH ?
                                           m_svcSeq.get()->DependencyLayer[i].CropH :
                                           m_svcSeq.get()->DependencyLayer[i].Height;

            //also copy of frameid
            m_inParams.FrameInfo.FrameId.DependencyId = (mfxU16)i;
            //TODO: hardcoded for now
            m_inParams.FrameInfo.FourCC = MFX_FOURCC_YV12;

            MFX_CHECK_STS(MFXDecPipeline::CreateYUVSource());

            pDecode->Register(m_pYUVSource.release(), 1);
        }
        m_pYUVSource = std::move(pDecode);
    }
    else
    {
        return MFXDecPipeline::CreateYUVSource();
    }
    return MFX_ERR_NONE;
}

mfxStatus    MFXTranscodingPipeline::CreateAllocator()
{
    //svc mode
    if (m_svcSeq.get() && MFX_CODINGOPTION_ON == m_svcSeq->DependencyLayer[0].Active)
    {
        size_t i;
        for (i = 1; i < MFX_ARRAY_SIZE(m_svcSeq->DependencyLayer)
            && (MFX_CODINGOPTION_ON == m_svcSeq->DependencyLayer[i].Active); i++);
        i--;

        mfxExtSVCDepLayer &top_layer = m_svcSeq->DependencyLayer[i];

        mfxU32 Width   = top_layer.CropW ? top_layer.CropW : top_layer.Width;
        mfxU32 Height  = top_layer.CropH ? top_layer.CropH : top_layer.Height;

        //single input multiple output - need to adjust vpp params in specific way
        mfxVideoParam vParamOriginalEnc = m_components[eREN].m_params;
        mfxVideoParam vParamOriginalDec = m_components[eDEC].m_params;

        m_components[eREN].m_params.mfx.FrameInfo.Width  = (mfxU16)Width;
        m_components[eREN].m_params.mfx.FrameInfo.Height = (mfxU16)Height;

        if (IsMultiReaderEnabled()) {
            //TODO: resolution for queryiosurface still taken from each components parameters, is it OK?
            m_components[eDEC].m_params.mfx.FrameInfo.Width  = (mfxU16)Width;
            m_components[eDEC].m_params.mfx.FrameInfo.Height = (mfxU16)Height;
            m_components[eDEC].m_params.mfx.FrameInfo.FrameId.DependencyId = (mfxU16)i - 1;
        }

        MFX_CHECK_STS(MFXDecPipeline::CreateAllocator());

        m_components[eDEC].m_params = vParamOriginalDec;
        m_components[eREN].m_params = vParamOriginalEnc;
    }
    else
    {
        return MFXDecPipeline::CreateAllocator();
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXTranscodingPipeline::CreateSplitter()
{
    if (IsMultiReaderEnabled())
    {
        //fallback to yuv mode
        m_inParams.bYuvReaderMode = true;
        mfxFrameInfo originalInfo = m_inParams.FrameInfo;
        MultiReader *pSpl = new MultiReader();
        //always use ratio mode
        pSpl->SetRatioOrder(true);

        for (size_t i = 0; i < m_filesForDependency.size() && !m_filesForDependency[i].empty(); i++)
        {
            //TODO: for now it is impossible to have several mfx_decoders, since they require separate sessions
            vm_string_strcpy_s(m_inParams.strSrcFile, MFX_ARRAY_SIZE(m_inParams.strSrcFile), m_filesForDependency[i].c_str());

            m_inParams.bExactSizeBsReader = true;
            m_inParams.FrameInfo.Width = m_svcSeq.get()->DependencyLayer[i].Width;
            m_inParams.FrameInfo.Height = m_svcSeq.get()->DependencyLayer[i].Height;

            m_inParams.FrameInfo.CropW = m_svcSeq.get()->DependencyLayer[i].Width;
            m_inParams.FrameInfo.CropH = m_svcSeq.get()->DependencyLayer[i].Height;

            PrintInfo(VM_STRING("DependencyLayer"), VM_STRING("%d"), i);
            MFX_CHECK_STS(MFXDecPipeline::CreateSplitter());

            mfxU16 maxRatio = 1;
            for (size_t j =0; j < m_svcSeq->DependencyLayer[i].TemporalNum; j++)
            {
                maxRatio = (std::max)(maxRatio, m_svcSeq->TemporalScale[m_svcSeq->DependencyLayer[i].TemporalId[j]]);
            }

            pSpl->Register(m_pSpl, maxRatio);
        }
        m_inParams.FrameInfo.Width = originalInfo.Width;//m_svcSeq.get()->DependencyLayer[0].Width;
        m_inParams.FrameInfo.Height = originalInfo.Height;//m_svcSeq.get()->DependencyLayer[0].Height;

        m_pSpl = pSpl;
    }
    else
    {
        return MFXDecPipeline::CreateSplitter();
    }
    return MFX_ERR_NONE;
}


mfxStatus MFXTranscodingPipeline::SetRefFile(const vm_char * pRefFile, mfxU32 nDelta)
{
    MFX_CHECK_STS(MFXDecPipeline::SetRefFile(pRefFile, nDelta));

    m_RenderType = MFX_ENC_RENDER;

    return MFX_ERR_NONE;
}

mfxStatus  MFXTranscodingPipeline::CreateFileSink(std::auto_ptr<IFile> &pSink)
{
    if (NULL == m_pRender)
    {
        return MFX_ERR_NONE;
    }

    if (m_bCreateDecode)
    {
        //holds allocator session, to pass them to target decoder
        DecodeContext decCtx;

        MFX_CHECK_STS(m_components[eREN].m_pSession->CloneSession(&decCtx.session));
        MFX_CHECK_STS(m_components[eREN].m_pSession->JoinSession(decCtx.session));

        //TODO: need to close session somehow
        std::unique_ptr<IYUVSource> pDec(new MFXDecoder(decCtx.session));

        //decorating decode
        if (m_components[eREN].m_bCalcLatency)
        {
            pDec.reset(new LatencyDecoder(false, NULL, &PerfCounterTime::Instance(), VM_STRING("Decoder2"), std::move(pDec)));
        }

        // pointer to allocator parameters structure needed for allocator init
        std::auto_ptr<mfxAllocatorParams> pAllocatorParams;
        std::auto_ptr<MFXFrameAllocatorRW> pAlloc;

        //decoder type is the same as encoder
        MFX_ZERO_MEM(decCtx.sInitialParams);
        decCtx.sInitialParams.mfx.CodecId = m_components[eREN].m_params.mfx.CodecId ;

        //applying asyncdepth
        decCtx.sInitialParams.AsyncDepth = m_components[eREN].m_params.AsyncDepth;

        //applying complete frame
        decCtx.bCompleteFrame = m_inParams.bCompleteFrame;

        // Create allocator
        if (m_components[eREN].m_bufType == MFX_BUF_HW)
        {
#ifdef D3D_SURFACES_SUPPORT
            MFX_CHECK_STS(CreateDeviceManager());
            mfxHDL hdl;
            MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, &hdl));
            MFX_CHECK_STS(MFXVideoCORE_SetHandle(decCtx.session, MFX_HANDLE_D3D9_DEVICE_MANAGER, hdl));

            if (decCtx.request.Info.FourCC == MFX_FOURCC_RGB4)
                decCtx.request.Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            else
                decCtx.request.Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET;

            decCtx.sInitialParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
            pAlloc.reset(m_pAllocFactory->CreateD3DAllocator());

            D3DAllocatorParams *pd3dAllocParams;
            MFX_CHECK_WITH_ERR(pd3dAllocParams = new D3DAllocatorParams, MFX_ERR_MEMORY_ALLOC);

            pd3dAllocParams->pManager = (IDirect3DDeviceManager9*)hdl;
            pAllocatorParams.reset(pd3dAllocParams);
#endif
#ifdef LIBVA_SUPPORT
            MFX_CHECK_STS(CreateDeviceManager());
            VADisplay va_dpy = NULL;
            MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL*) &va_dpy));
            MFX_CHECK_STS(MFXVideoCORE_SetHandle(decCtx.session, MFX_HANDLE_VA_DISPLAY, (mfxHDL)va_dpy));

            if (decCtx.request.Info.FourCC == MFX_FOURCC_RGB4)
                decCtx.request.Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            else
                decCtx.request.Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET;

            decCtx.sInitialParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
            pAlloc.reset(m_pAllocFactory->CreateVAAPIAllocator());

            vaapiAllocatorParams *p_vaapiAllocParams;
            MFX_CHECK_WITH_ERR(p_vaapiAllocParams = new vaapiAllocatorParams, MFX_ERR_MEMORY_ALLOC);

            p_vaapiAllocParams->m_dpy = va_dpy;
            pAllocatorParams.reset(p_vaapiAllocParams);
#endif
        }
        else if(m_components[eREN].m_bufType == MFX_BUF_HW_DX11)
        {
#ifdef MFX_D3D11_SUPPORT
            MFX_CHECK_STS(CreateDeviceManager());
            mfxHDL hdl;
            MFX_CHECK_STS(m_pHWDevice->GetHandle(MFX_HANDLE_D3D11_DEVICE, &hdl));
            MFX_CHECK_STS(MFXVideoCORE_SetHandle(decCtx.session, MFX_HANDLE_D3D11_DEVICE, hdl));

            if (decCtx.request.Info.FourCC == MFX_FOURCC_RGB4)
                decCtx.request.Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            else
                decCtx.request.Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET;

            decCtx.sInitialParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
            pAlloc.reset(m_pAllocFactory->CreateD3D11Allocator());

            D3D11AllocatorParams *pd3d11AllocParams;
            MFX_CHECK_WITH_ERR(pd3d11AllocParams = new D3D11AllocatorParams, MFX_ERR_MEMORY_ALLOC);

            pd3d11AllocParams->pDevice = (ID3D11Device*)hdl;
            pd3d11AllocParams->bUseSingleTexture = m_components[eREN].m_bD3D11SingeTexture;
            pAllocatorParams.reset(pd3d11AllocParams);
#endif
        }
        else if (m_components[eREN].m_bufType == MFX_BUF_SW)
        {
            decCtx.request.Type = MFX_MEMTYPE_SYSTEM_MEMORY;
            decCtx.sInitialParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            pAlloc.reset(m_pAllocFactory->CreateSysMemAllocator());
        }

        MFX_CHECK_STS(pAlloc->Init(pAllocatorParams.get()));

        //setting the same allocator
        MFX_CHECK_STS(MFXVideoCORE_SetFrameAllocator(decCtx.session, pAlloc.get()));

        decCtx.pAlloc  = pAlloc.release();
        decCtx.pSource = std::move(pDec);

        //TODO:pasthru -o option to downstream
        pSink.reset(new Adapter<IFile, IBitstreamWriter>(new BitstreamDecoder(std::move(decCtx))));
    }

    if (0 == vm_string_strlen(m_inParams.strDstFile))
    {
        //create null filesink then for now only for transcoder case
        m_inParams.bNullFileWriter = true;
    }

    MFX_CHECK_STS(MFXDecPipeline::CreateFileSink(pSink));

    return MFX_ERR_NONE;
}

std::unique_ptr<IVideoEncode> MFXTranscodingPipeline::CreateEncoder()
{
    //create ivideoencode implementation
    PipelineObjectDesc<IVideoEncode> createParams;

    if (vm_string_strlen(m_inParams.strEncPluginGuid)) {
        createParams = make_wrapper<IVideoEncode>(m_components[eREN].m_pSession->GetMFXSession()
            , m_inParams.strEncPluginGuid
            , (mfxU32)1
            , ENCODER_MFX_PLUGIN_GUID
            , NULL);

    } else if (vm_string_strlen(m_inParams.strEncPlugin)) {
        createParams = make_wrapper<IVideoEncode>(m_components[eREN].m_pSession->GetMFXSession()
            , m_inParams.strEncPlugin
            , ENCODER_MFX_PLUGIN_FILE
            , NULL);
    } else {
        createParams = make_wrapper<IVideoEncode>(m_components[eREN].m_pSession->GetMFXSession()
            , VM_STRING("")
            , ENCODER_MFX_NATIVE
            , NULL);
    }

    std::unique_ptr<IVideoEncode> pEncoder ( m_pFactory->CreateVideoEncode(&createParams));

    //wrapper for jpeg encoder handles awkward behavior patterns
    switch (m_EncParams.mfx.CodecId)
    {
        case MFX_CODEC_JPEG :
            pEncoder.reset(new MFXJpegEncWrap(std::move(pEncoder)));
        break;
        default:
        break;
    }

    if (m_components[eREN].m_bCalcLatency)
    {
        pEncoder.reset( new LatencyEncode(
              m_components[eDEC].m_bCalcLatency && m_inParams.bNoPipelineSync
            , NULL //console printer will be created
            , &PerfCounterTime::Instance()
            , std::move(pEncoder)));
    }

    if (m_inParams.bCreateRefListControl )
    {
        pEncoder.reset(new RefListControlEncode(std::move(pEncoder)));
    }

    if (m_inParams.bCreateEncFrameInfo )
    {
        pEncoder.reset(new EncodedFrameInfoEncoder(std::move(pEncoder)));
    }

    if (m_inParams.EncodedOrder)
    {
        if (m_inParams.bExternalRefListEncodedOrder)
            pEncoder.reset(new RefListControlEncodedOrder(std::move(pEncoder), m_inParams.encOrderRefListsParFile));
        else
            pEncoder.reset(new EncodeOrderEncode(std::move(pEncoder), m_inParams.useEncOrderParFile, m_inParams.encOrderParFile));
    }

    if (m_inParams.bDisableIpFieldPair)
    {
        pEncoder.reset(new IPFieldPairDisableEncode(std::move(pEncoder)));
    }

    if (MFX_CODINGOPTION_ON == m_extCodingOptions->FieldOutput)
    {
        pEncoder.reset(new FieldOutputEncoder(std::move(pEncoder)));
    }

    if (!m_extEncoderCapability.IsZero())
    {
        pEncoder.reset(new QueryMode4Encode(std::move(pEncoder)));
    }

    if (m_inParams.bCreatePerFrameExtBuf)
    {
        pEncoder.reset(new PerFrameExtBufEncode(std::move(pEncoder)));
    }

    //TODO: always attaching MVC handler, is it possible not to do this
    pEncoder.reset(new MVCHandler<IVideoEncode>(m_components[eREN].m_extParams, false, std::move(pEncoder)));
    m_pEncoder = pEncoder.get();
    return pEncoder;
}

mfxStatus MFXTranscodingPipeline::CreateRender()
{
    if(NULL != m_pRender)
    {
        return MFX_ERR_NONE;
    }

    MFXEncodeWRAPPER * pEncoderWrp = NULL;
    MFX_CHECK_STS(CreateEncodeWRAPPER(CreateEncoder(), &pEncoderWrp));
    m_pRender = pEncoderWrp;

    m_inParams.encodeExtraParams.pRefFile = m_inParams.refFile;
    m_inParams.encodeExtraParams.bVerbose = m_inParams.bVerbose;

    MFX_CHECK_STS(pEncoderWrp->SetExtraParams(&m_inParams.encodeExtraParams));

    //adds decorators filters
    return DecorateRender();
}

mfxStatus MFXTranscodingPipeline::CreateEncodeWRAPPER(std::unique_ptr<IVideoEncode> &&pEncoder, MFXEncodeWRAPPER ** ppEncoderWrp)
{
    mfxStatus sts = MFX_ERR_NONE;
    //////////////////////////////////////////////////////////////////////////
    //create render
    if (m_RenderType == MFX_METRIC_CHECK_RENDER)
    {
        EncodeDecodeQuality * pEncoderQlty;
        MFX_CHECK_WITH_ERR(*ppEncoderWrp = pEncoderQlty = new EncodeDecodeQuality(m_components[eREN], &sts, std::move(pEncoder))
            , MFX_ERR_MEMORY_ALLOC);

        if (m_bYuvDumpMode)
        {
            MFX_CHECK_STS(pEncoderQlty->AddMetric(METRIC_DUMP));
        }

        for (size_t i = 0; i < m_metrics.size(); i++)
        {
            MFX_CHECK_STS(pEncoderQlty->AddMetric(m_metrics[i].first, m_metrics[i].second));
        }

        MFX_CHECK_STS(pEncoderQlty->SetOutputPerfFile(m_inParams.perfFile));
    }else
    {
        MFX_CHECK_WITH_ERR(*ppEncoderWrp = new MFXEncodeWRAPPER(m_components[eREN], &sts, std::move(pEncoder)), MFX_ERR_MEMORY_ALLOC);
    }


    return MFX_ERR_NONE;
}

mfxStatus MFXTranscodingPipeline::ReleasePipeline()
{
    MFX_CHECK_STS(MFXDecPipeline::ReleasePipeline());

    return MFX_ERR_NONE;
}

void MFXTranscodingPipeline::PrintCommonHelp()
{
    vm_char unsupported[] = VM_STRING("unsupported option");
    vm_char *argv[1], **_argv = argv;
    mfxI32 argc = 1;
    argv[0] = unsupported;

    m_OptProc.SetPrint(true);
    ProcessCommand(_argv, argc, false);
    m_OptProc.SetPrint(false);
}

int MFXTranscodingPipeline::PrintHelp()
{
    //special HACK to avoid printing of parameters that parsed if this flag enabled
    m_bResetParamsStart = false;

    vm_char * last_err = GetLastErrString();
    if (NULL != last_err)
    {
        vm_string_printf(VM_STRING("ERROR: %s\n"), last_err);
    }

    int year, month, day;
    int h,m,s;
    const vm_char *platform;
    PipelineBuildTime(platform, year, month, day, h, m, s);
    vm_string_printf(VM_STRING("\n"));
    vm_string_printf(VM_STRING("---------------------------------------------------------------------------------------------\n"));
    vm_string_printf(VM_STRING("%s version 1.%d.%d.%d / %s\n"), GetAppName().c_str(), year % 100, month, day, platform);
    vm_string_printf(VM_STRING("%s is DXVA2/LibVA testing application developed by Intel/SSG/DPD/VCP/MDP\n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("This application is for Intel INTERNAL use only!\n"));
    vm_string_printf(VM_STRING("For support, usage examples, and recent builds please visit http://goto/%s  \n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("---------------------------------------------------------------------------------------------\n"));
    vm_string_printf(VM_STRING("\n"));
    vm_string_printf(VM_STRING("Usage: %s.exe [Parameters]\n\n"), GetAppName().c_str());
    vm_string_printf(VM_STRING("Parameters are:\n"));
    PrintCommonHelp();


    return 1;
}

vm_char * MFXTranscodingPipeline::GetLastErrString()
{
    static struct
    {
        mfxU32 err;
        vm_char  desc[100];
    } errDetails[] =
    {
        {PE_NO_TARGET_CODEC, VM_STRING("No encoding format specified")},
    };

    for (size_t i=0; i < sizeof(errDetails)/sizeof(errDetails[0]); i++)
    {
        if (errDetails[i].err == PipelineGetLastErr())
        {
            return errDetails[i].desc;
        }
    }

    return MFXDecPipeline::GetLastErrString();
}

mfxStatus MFXTranscodingPipeline::WriteParFile()
{
    if (0 == vm_string_strlen(m_inParams.strParFile) || !m_bWritePar)
        return MFX_ERR_NONE;

    vm_file * fd;
    MFX_CHECK_VM_FOPEN(fd, m_inParams.strParFile,VM_STRING("w"));

    //will store resolution  for scaling back
    if (m_inParams.bUseVPP)
    {
        vm_file_fprintf(fd, VM_STRING("resize_w = %d \n"), m_components[eVPP].m_params.vpp.In.CropW);
        vm_file_fprintf(fd, VM_STRING("resize_h = %d \n"), m_components[eVPP].m_params.vpp.In.CropH);
    }

    vm_file_close(fd);

    return MFX_ERR_NONE;
}

mfxStatus MFXTranscodingPipeline::GetEncoder(IVideoEncode **ppCtrl)
{
    MFX_CHECK_POINTER(ppCtrl);
    * ppCtrl = m_pEncoder;

    return MFX_ERR_NONE;
}
