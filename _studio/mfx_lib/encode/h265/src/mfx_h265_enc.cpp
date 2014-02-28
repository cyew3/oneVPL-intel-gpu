//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <math.h>
#include <algorithm>

#include "ippi.h"
#include "vm_interlocked.h"
#include "vm_sys_info.h"

#include "mfx_h265_enc.h"
#include "mfx_h265_brc.h"

//#include <immintrin.h>

#ifdef MFX_ENABLE_WATERMARK
#include "watermark.h"
#endif

#include "mfx_h265_enc_cm_defs.h"

namespace H265Enc {

extern Ipp32s cmCurIdx;
extern Ipp32s cmNextIdx;

#if defined( _WIN32) || defined(_WIN64)
#define thread_sleep(nms) Sleep(nms)
#else
#define thread_sleep(nms)
#endif
#define x86_pause() _mm_pause()

double h265_split_thresholds[2][3][3][2] = {
    {{{18.784248, 0.194168}, {21.838817, 0.201511}, {5.612814, 0.271913}},
    {{82.451342, 0.165413}, {121.861526, 0.165413},{159.449629, 0.173179}},
    {{271.522262, 0.145591},{417.598641, 0.147174},{580.195361, 0.154323}}},
    {{{2.835922, 0.215424},  {3.936363, 0.223238},  {0.039745, 0.441857}},
    {{6.961510, 0.203691},  {12.535571, 0.204356}, {23.346541, 0.210298}},
    {{30.958412, 0.184048}, {60.678487, 0.180853}, {123.727539, 0.180971}}},
/* tu_flag, size, strength, a, b
  0, 16,  5, 18.784248, 0.194168
  0, 16, 10, 21.838817, 0.201511
  0, 16, 20, 5.612814, 0.271913
  0, 32,  5, 82.451342, 0.165413
  0, 32, 10, 121.861526, 0.165413
  0, 32, 20, 159.449629, 0.173179
  0, 64,  5, 271.522262, 0.145591
  0, 64, 10, 417.598641, 0.147174
  0, 64, 20, 580.195361, 0.154323
  1,  8,  5, 2.835922, 0.215424
  1,  8, 10, 3.936363, 0.223238
  1,  8, 20, 0.039745, 0.441857
  1, 16,  5, 6.961510, 0.203691
  1, 16, 10, 12.535571, 0.204356
  1, 16, 20, 23.346541, 0.210298
  1, 32,  5, 30.958412, 0.184048
  1, 32, 10, 60.678487, 0.180853
  1, 32, 20, 123.727539, 0.180971*/
};

static Ipp64f h265_calc_split_threshold(Ipp32s tu_flag, Ipp32s log2width, Ipp32s strength, Ipp32s QP) {
    if (strength == 0) return 0;
    if (strength > 3) strength = 3;
    if (tu_flag) {
        if((log2width < 3 || log2width > 5)) {
//            VM_ASSERT(0);
            return 0;
        }
        tu_flag = 1;
        log2width -= 3;
    } else {
        if((log2width < 4 || log2width > 6)) {
//            VM_ASSERT(0);
            return 0;
        }
        log2width -= 4;
    }
    double a = h265_split_thresholds[tu_flag][log2width][strength-1][0];
    double b = h265_split_thresholds[tu_flag][log2width][strength-1][1];
    return a * exp(b * QP);
}

static const mfxU16 tab_AspectRatio265[17][2] =
{
    { 1,  1},  // unspecified
    { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
    {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160,99}, { 4,  3}, { 3,  2}, { 2,  1}
};

static mfxU32 ConvertSARtoIDC_H265enc(mfxU32 sarw, mfxU32 sarh)
{
    mfxU32 i;
    for (i=1; i<sizeof(tab_AspectRatio265)/sizeof(tab_AspectRatio265[0]); i++)
        if (sarw * tab_AspectRatio265[i][1] == sarh * tab_AspectRatio265[i][0])
            return i;
    return 0;
}


mfxStatus H265Encoder::InitH265VideoParam(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *opts_hevc)
{
    H265VideoParam *pars = &m_videoParam;
    Ipp32u width = param->mfx.FrameInfo.Width;
    Ipp32u height = param->mfx.FrameInfo.Height;

// preset
    pars->SourceWidth = width;
    pars->SourceHeight = height;
    pars->Log2MaxCUSize = opts_hevc->Log2MaxCUSize;// 6;
    pars->MaxCUDepth = opts_hevc->MaxCUDepth; // 4;
    pars->QuadtreeTULog2MaxSize = opts_hevc->QuadtreeTULog2MaxSize; // 5;
    pars->QuadtreeTULog2MinSize = opts_hevc->QuadtreeTULog2MinSize; // 2;
    pars->QuadtreeTUMaxDepthIntra = opts_hevc->QuadtreeTUMaxDepthIntra; // 4;
    pars->QuadtreeTUMaxDepthInter = opts_hevc->QuadtreeTUMaxDepthInter; // 4;
    pars->AMPFlag = (opts_hevc->AMP == MFX_CODINGOPTION_ON);
    pars->TMVPFlag = (opts_hevc->TMVP == MFX_CODINGOPTION_ON);
    pars->QPI = (Ipp8s)param->mfx.QPI;
    pars->QPP = (Ipp8s)param->mfx.QPP;
    pars->QPB = (Ipp8s)param->mfx.QPB;
    pars->NumSlices = param->mfx.NumSlice;

    pars->GopPicSize = param->mfx.GopPicSize;
    pars->GopRefDist = param->mfx.GopRefDist;
    pars->IdrInterval = param->mfx.IdrInterval;
    pars->GopClosedFlag = (param->mfx.GopOptFlag & MFX_GOP_CLOSED) != 0;
    pars->GopStrictFlag = (param->mfx.GopOptFlag & MFX_GOP_STRICT) != 0;
    pars->BPyramid = pars->GopRefDist > 2 && opts_hevc->BPyramid == MFX_CODINGOPTION_ON;

    if (!pars->GopRefDist) /*|| (pars->GopPicSize % pars->GopRefDist)*/
        return MFX_ERR_INVALID_VIDEO_PARAM;

    pars->NumRefToStartCodeBSlice = 1;
    pars->TreatBAsReference = 0;

    if (pars->BPyramid) {
        pars->MaxRefIdxL0 = (Ipp8u)param->mfx.NumRefFrame;
        pars->MaxRefIdxL1 = (Ipp8u)param->mfx.NumRefFrame;
    } else {
        pars->MaxRefIdxL0 = (Ipp8u)param->mfx.NumRefFrame;
        pars->MaxRefIdxL1 = 1;
    }
    pars->MaxBRefIdxL0 = 1;
    pars->GeneralizedPB = (opts_hevc->GPB == MFX_CODINGOPTION_ON);
    pars->PGopPicSize = (pars->GopRefDist > 1 || pars->MaxRefIdxL0 == 1) ? 1 : PGOP_PIC_SIZE;
    pars->NumRefFrames = MAX(6, pars->MaxRefIdxL0);

    pars->AnalyseFlags = 0;
    if (opts_hevc->AnalyzeChroma == MFX_CODINGOPTION_ON)
        pars->AnalyseFlags |= HEVC_ANALYSE_CHROMA;

    pars->SplitThresholdStrengthCUIntra = (Ipp8u)opts_hevc->SplitThresholdStrengthCUIntra - 1;
    pars->SplitThresholdStrengthTUIntra = (Ipp8u)opts_hevc->SplitThresholdStrengthTUIntra - 1;
    pars->SplitThresholdStrengthCUInter = (Ipp8u)opts_hevc->SplitThresholdStrengthCUInter - 1;

    pars->SBHFlag  = (opts_hevc->SignBitHiding == MFX_CODINGOPTION_ON);
    pars->RDOQFlag = (opts_hevc->RDOQuant == MFX_CODINGOPTION_ON);
    pars->rdoqChromaFlag = (opts_hevc->RDOQuantChroma == MFX_CODINGOPTION_ON);
    pars->rdoqCGZFlag = (opts_hevc->RDOQuantCGZ == MFX_CODINGOPTION_ON);
   // aya ================================================
    pars->SAOFlag  = (opts_hevc->SAO == MFX_CODINGOPTION_ON);
    // ====================================================
    pars->WPPFlag  = (opts_hevc->WPP == MFX_CODINGOPTION_ON) || (opts_hevc->WPP == MFX_CODINGOPTION_UNKNOWN && param->mfx.NumThread > 1);
    if (pars->WPPFlag) {
        pars->num_threads = param->mfx.NumThread;
        if (pars->num_threads == 0)
            pars->num_threads = vm_sys_info_get_cpu_num();
        if (pars->num_threads < 1) {
            pars->num_threads = 1;
        }
    } else {
        pars->num_threads = 1;
    }

    for (Ipp32s i = 0; i <= 6; i++) {
        Ipp32s w = (1 << i);
        pars->num_cand_1[i] = w > 8 ? 4 : 8;
        pars->num_cand_2[i] = pars->num_cand_1[i] >> 1;
    }
    if (opts_hevc->IntraNumCand1_2) pars->num_cand_1[2] = (Ipp8u)opts_hevc->IntraNumCand1_2;
    if (opts_hevc->IntraNumCand1_3) pars->num_cand_1[3] = (Ipp8u)opts_hevc->IntraNumCand1_3;
    if (opts_hevc->IntraNumCand1_4) pars->num_cand_1[4] = (Ipp8u)opts_hevc->IntraNumCand1_4;
    if (opts_hevc->IntraNumCand1_5) pars->num_cand_1[5] = (Ipp8u)opts_hevc->IntraNumCand1_5;
    if (opts_hevc->IntraNumCand1_6) pars->num_cand_1[6] = (Ipp8u)opts_hevc->IntraNumCand1_6;
    if (opts_hevc->IntraNumCand2_2) pars->num_cand_2[2] = (Ipp8u)opts_hevc->IntraNumCand2_2;
    if (opts_hevc->IntraNumCand2_3) pars->num_cand_2[3] = (Ipp8u)opts_hevc->IntraNumCand2_3;
    if (opts_hevc->IntraNumCand2_4) pars->num_cand_2[4] = (Ipp8u)opts_hevc->IntraNumCand2_4;
    if (opts_hevc->IntraNumCand2_5) pars->num_cand_2[5] = (Ipp8u)opts_hevc->IntraNumCand2_5;
    if (opts_hevc->IntraNumCand2_6) pars->num_cand_2[6] = (Ipp8u)opts_hevc->IntraNumCand2_6;

//kolya HM's number of intra modes to search through
#define HM_MATCH_2 0
#if HM_MATCH_2
    pars->num_cand_1[1] = 3; // 2x2
    pars->num_cand_1[2] = 8; // 4x4
    pars->num_cand_1[3] = 8; // 8x8
    pars->num_cand_1[4] = 3; // 16x16
    pars->num_cand_1[5] = 3; // 32x32
    pars->num_cand_1[6] = 3; // 64x64
    pars->num_cand_1[7] = 3; // 128x128
#endif

    pars->enableCmFlag = (opts_hevc->EnableCm == MFX_CODINGOPTION_ON);
    pars->cmIntraThreshold = opts_hevc->CmIntraThreshold;
    pars->tuSplitIntra = opts_hevc->TUSplitIntra;
    pars->cuSplit = opts_hevc->CUSplit;
    pars->intraAngModes = opts_hevc->IntraAngModes;
    pars->fastPUDecision = (opts_hevc->FastPUDecision == MFX_CODINGOPTION_ON);
    pars->hadamardMe = opts_hevc->HadamardMe;
    pars->TMVPFlag = (opts_hevc->TMVP == MFX_CODINGOPTION_ON);
    pars->deblockingFlag = (opts_hevc->Deblocking == MFX_CODINGOPTION_ON);

    for (Ipp32s i = 0; i <= 6; i++) {
        if (pars->num_cand_1[i] < 1)
            pars->num_cand_1[i] = 1;
        if (pars->num_cand_1[i] > 35)
            pars->num_cand_1[i] = 35;
        if (pars->num_cand_2[i] > pars->num_cand_1[i])
            pars->num_cand_2[i] = pars->num_cand_1[i];
        if (pars->num_cand_2[i] < 1)
            pars->num_cand_2[i] = 1;
    }
// derived

    pars->MaxTrSize = 1 << pars->QuadtreeTULog2MaxSize;
    pars->MaxCUSize = 1 << pars->Log2MaxCUSize;

    pars->AddCUDepth  = 0;
    while( (pars->MaxCUSize>>pars->MaxCUDepth) >
        ( 1u << ( pars->QuadtreeTULog2MinSize + pars->AddCUDepth )  ) ) pars->AddCUDepth++;

    pars->MaxCUDepth += pars->AddCUDepth;
    pars->AddCUDepth++;

    pars->MinCUSize = pars->MaxCUSize >> (pars->MaxCUDepth - pars->AddCUDepth);
    pars->MinTUSize = pars->MaxCUSize >> pars->MaxCUDepth;

    if (pars->MinTUSize != 1u << pars->QuadtreeTULog2MinSize)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    pars->CropLeft = param->mfx.FrameInfo.CropX;
    pars->CropTop = param->mfx.FrameInfo.CropY;
    pars->CropRight = param->mfx.FrameInfo.CropW ? param->mfx.FrameInfo.Width - param->mfx.FrameInfo.CropW - param->mfx.FrameInfo.CropX : 0;
    pars->CropBottom = param->mfx.FrameInfo.CropH ? param->mfx.FrameInfo.Height - param->mfx.FrameInfo.CropH - param->mfx.FrameInfo.CropY : 0;

    //pars->CropRight = pars->CropBottom = 0;
    //if (width % pars->MinCUSize)
    //{
    //    pars->CropRight  = (width / pars->MinCUSize + 1) * pars->MinCUSize - width;
    //    width  += pars->CropRight;
    //}
    //if (height % pars->MinCUSize)
    //{
    //    pars->CropBottom = (height / pars->MinCUSize + 1) * pars->MinCUSize - height;
    //    height += pars->CropBottom;
    //}
    //if ((pars->CropRight | pars->CropBottom) & 1)
    //    return MFX_ERR_INVALID_VIDEO_PARAM;

    pars->Width = width;
    pars->Height = height;

    pars->Log2NumPartInCU = pars->MaxCUDepth << 1;
    pars->NumPartInCU = 1 << pars->Log2NumPartInCU;
    pars->NumPartInCUSize  = 1 << pars->MaxCUDepth;

    pars->PicWidthInMinCbs = pars->Width / pars->MinCUSize;
    pars->PicHeightInMinCbs = pars->Height / pars->MinCUSize;
    pars->PicWidthInCtbs = (pars->Width + pars->MaxCUSize - 1) / pars->MaxCUSize;
    pars->PicHeightInCtbs = (pars->Height + pars->MaxCUSize - 1) / pars->MaxCUSize;

    if (pars->num_threads > pars->PicHeightInCtbs)
        pars->num_threads = pars->PicHeightInCtbs;
    if (pars->num_threads > (pars->PicWidthInCtbs + 1) / 2)
        pars->num_threads = (pars->PicWidthInCtbs + 1) / 2;

    pars->threading_by_rows = pars->WPPFlag && pars->num_threads > 1 && pars->NumSlices == 1 ? 0 : 1;
    pars->num_thread_structs = pars->threading_by_rows ? pars->num_threads : pars->PicHeightInCtbs;
//  if (!pars->threading_by_rows && pars->NumSlices > (Ipp32s)pars->PicHeightInCtbs)
//      pars->NumSlices = pars->PicHeightInCtbs;

    for (Ipp32s i = 0; i < pars->MaxCUDepth; i++ )
    {
        pars->AMPAcc[i] = i < pars->MaxCUDepth-pars->AddCUDepth ? pars->AMPFlag : 0;
    }

    pars->UseDQP = 0;
    pars->MaxCuDQPDepth = 0;
    pars->MinCuDQPSize = pars->MaxCUSize;

    pars->NumMinTUInMaxCU = pars->MaxCUSize >> pars->QuadtreeTULog2MinSize;
    pars->Log2MinTUSize = pars->QuadtreeTULog2MinSize;
    pars->MaxTotalDepth = pars->Log2MaxCUSize - pars->Log2MinTUSize;

    pars->FrameRateExtN = param->mfx.FrameInfo.FrameRateExtN;
    pars->FrameRateExtD = param->mfx.FrameInfo.FrameRateExtD;
    pars->AspectRatioW  = param->mfx.FrameInfo.AspectRatioW ;
    pars->AspectRatioH  = param->mfx.FrameInfo.AspectRatioH ;

    pars->Profile = param->mfx.CodecProfile;
    pars->Tier = (param->mfx.CodecLevel & MFX_TIER_HEVC_HIGH) ? 1 : 0;
    pars->Level = (param->mfx.CodecLevel &~ MFX_TIER_HEVC_HIGH) * 1; // mult 3 it SetProfileLevel

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetVPS()
{
    H265VidParameterSet *vps = &m_vps;

    memset(vps, 0, sizeof(H265VidParameterSet));
    vps->vps_max_layers = 1;
    vps->vps_max_sub_layers = 1;
    vps->vps_temporal_id_nesting_flag = 1;
    if (m_videoParam.BPyramid)
        vps->vps_max_dec_pic_buffering[0] = m_videoParam.GopRefDist == 8 ? 4 : 2;
    else
        vps->vps_max_dec_pic_buffering[0] = (Ipp8u)(MAX(m_videoParam.MaxRefIdxL0,m_videoParam.MaxBRefIdxL0) +
            m_videoParam.MaxRefIdxL1);

    vps->vps_max_num_reorder_pics[0] = (Ipp8u)(m_videoParam.GopRefDist - 1);
    vps->vps_max_latency_increase[0] = 0;
    vps->vps_num_layer_sets = 1;

    if (m_videoParam.FrameRateExtD && m_videoParam.FrameRateExtN) {
        vps->vps_timing_info_present_flag = 1;
        vps->vps_num_units_in_tick = m_videoParam.FrameRateExtD / 1;
        vps->vps_time_scale = m_videoParam.FrameRateExtN;
        vps->vps_poc_proportional_to_timing_flag = 0;
        vps->vps_num_hrd_parameters = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetProfileLevel()
{
    memset(&m_profile_level, 0, sizeof(H265ProfileLevelSet));
    m_profile_level.general_profile_idc = (mfxU8) m_videoParam.Profile; //MFX_PROFILE_HEVC_MAIN;
    m_profile_level.general_tier_flag = (mfxU8) m_videoParam.Tier;
    m_profile_level.general_level_idc = (mfxU8) (m_videoParam.Level * 3);
    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetSPS()
{
    H265SeqParameterSet *sps = &m_sps;
    H265VidParameterSet *vps = &m_vps;
    H265VideoParam *pars = &m_videoParam;

    memset(sps, 0, sizeof(H265SeqParameterSet));

    sps->sps_video_parameter_set_id = vps->vps_video_parameter_set_id;
    sps->sps_max_sub_layers = 1;
    sps->sps_temporal_id_nesting_flag = 1;

    sps->chroma_format_idc = 1;
//    sps->sps_max_temporal_layers_minus1 = vps->vps_max_sub_layers_minus1;

    sps->pic_width_in_luma_samples = pars->Width;
    sps->pic_height_in_luma_samples = pars->Height;
    sps->bit_depth_luma = BIT_DEPTH_LUMA;
    sps->bit_depth_chroma = BIT_DEPTH_CHROMA;

    if (pars->CropLeft | pars->CropTop | pars->CropRight | pars->CropBottom) {
        sps->conformance_window_flag = 1;
        sps->conf_win_left_offset = pars->CropLeft / 2;
        sps->conf_win_top_offset = pars->CropTop / 2;
        sps->conf_win_right_offset = pars->CropRight / 2;
        sps->conf_win_bottom_offset = pars->CropBottom / 2;
    }

    sps->log2_diff_max_min_coding_block_size = (Ipp8u)(pars->MaxCUDepth - pars->AddCUDepth);
    sps->log2_min_coding_block_size_minus3 = (Ipp8u)(pars->Log2MaxCUSize - sps->log2_diff_max_min_coding_block_size - 3);

    sps->log2_min_transform_block_size_minus2 = (Ipp8u)(pars->QuadtreeTULog2MinSize - 2);
    sps->log2_diff_max_min_transform_block_size = (Ipp8u)(pars->QuadtreeTULog2MaxSize - pars->QuadtreeTULog2MinSize);

    sps->max_transform_hierarchy_depth_intra = (Ipp8u)pars->QuadtreeTUMaxDepthIntra;
    sps->max_transform_hierarchy_depth_inter = (Ipp8u)pars->QuadtreeTUMaxDepthInter;

    sps->amp_enabled_flag = pars->AMPFlag;
    sps->sps_temporal_mvp_enabled_flag = pars->TMVPFlag;
    sps->sample_adaptive_offset_enabled_flag = pars->SAOFlag;
    sps->strong_intra_smoothing_enabled_flag = 1;

    InitShortTermRefPicSet();

    if (m_videoParam.GopRefDist == 1)
    {
        sps->log2_max_pic_order_cnt_lsb = 4;
    } else {
        Ipp32s log2_max_poc = H265_CeilLog2(m_videoParam.GopRefDist - 1 + m_videoParam.NumRefFrames) + 3;

        sps->log2_max_pic_order_cnt_lsb = (Ipp8u)MAX(log2_max_poc, 4);

        if (sps->log2_max_pic_order_cnt_lsb > 16)
        {
            VM_ASSERT(false);
            sps->log2_max_pic_order_cnt_lsb = 16;
        }
    }
    sps->num_ref_frames = m_videoParam.NumRefFrames;

    sps->sps_num_reorder_pics[0] = vps->vps_max_num_reorder_pics[0];
    sps->sps_max_dec_pic_buffering[0] = vps->vps_max_dec_pic_buffering[0];
    sps->sps_max_latency_increase[0] = vps->vps_max_latency_increase[0];

    //VUI
    if (pars->AspectRatioW && pars->AspectRatioH) {
        sps->aspect_ratio_idc = (Ipp8u)ConvertSARtoIDC_H265enc(pars->AspectRatioW, pars->AspectRatioH);
        if (sps->aspect_ratio_idc == 0) {
            sps->aspect_ratio_idc = 255; //Extended SAR
            sps->sar_width = pars->AspectRatioW;
            sps->sar_height = pars->AspectRatioH;
        }
        sps->aspect_ratio_info_present_flag = 1;
    }
    if (sps->aspect_ratio_info_present_flag)
        sps->vui_parameters_present_flag = 1;


    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetPPS()
{
    H265SeqParameterSet *sps = &m_sps;
    H265PicParameterSet *pps = &m_pps;

    memset(pps, 0, sizeof(H265PicParameterSet));

    pps->pps_seq_parameter_set_id = sps->sps_seq_parameter_set_id;
    pps->deblocking_filter_control_present_flag = !m_videoParam.deblockingFlag;
    pps->deblocking_filter_override_enabled_flag = 0;
    pps->pps_deblocking_filter_disabled_flag = !m_videoParam.deblockingFlag;
    pps->pps_tc_offset_div2 = 0;
    pps->pps_beta_offset_div2 = 0;
    pps->pps_loop_filter_across_slices_enabled_flag = 1;
    pps->loop_filter_across_tiles_enabled_flag = 1;
    pps->entropy_coding_sync_enabled_flag = m_videoParam.WPPFlag;

    if (m_brc)
        pps->init_qp = (Ipp8s)m_brc->GetQP(MFX_FRAMETYPE_I);
    else
        pps->init_qp = m_videoParam.QPI;

    pps->log2_parallel_merge_level = 2;

    pps->num_ref_idx_l0_default_active = m_videoParam.MaxRefIdxL0;
    pps->num_ref_idx_l1_default_active = m_videoParam.MaxRefIdxL1;

    pps->sign_data_hiding_enabled_flag = m_videoParam.SBHFlag;

    return MFX_ERR_NONE;
}

static Ipp32f tab_rdLambdaBPyramid[4] = {0.442f, 0.3536f, 0.3536f, 0.68f};

mfxStatus H265Encoder::SetSlice(H265Slice *slice, Ipp32u curr_slice)
{
    memset(slice, 0, sizeof(H265Slice));

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32u numSlices = m_videoParam.NumSlices;
    Ipp32u slice_len = numCtbs / numSlices;
//    if (!m_videoParam.threading_by_rows)
//        slice_len = slice_len / m_videoParam.PicWidthInCtbs * m_videoParam.PicWidthInCtbs;

    slice->slice_segment_address = slice_len * curr_slice;
    slice->slice_address_last_ctb = slice_len * (curr_slice + 1) - 1;
    if (slice->slice_address_last_ctb > numCtbs - 1 || curr_slice == numSlices - 1)
        slice->slice_address_last_ctb = numCtbs - 1;

    if (curr_slice) slice->first_slice_segment_in_pic_flag = 0;
    else slice->first_slice_segment_in_pic_flag = 1;

    slice->slice_pic_parameter_set_id = m_pps.pps_pic_parameter_set_id;

    switch (m_pCurrentFrame->m_PicCodType) {
        case MFX_FRAMETYPE_P:
            slice->slice_type = P_SLICE; break;
        case MFX_FRAMETYPE_B:
            slice->slice_type = B_SLICE; break;
        case MFX_FRAMETYPE_I:
        default:
            slice->slice_type = I_SLICE; break;
    }

    if (m_pCurrentFrame->m_bIsIDRPic)
    {
        slice->IdrPicFlag = 1;
        slice->RapPicFlag = 1;
    }

//    slice->rap_pic_id = m_frameCount & 0xffff;
    slice->slice_pic_order_cnt_lsb =  m_pCurrentFrame->PicOrderCnt() & ~(0xffffffff << m_sps.log2_max_pic_order_cnt_lsb);
    slice->deblocking_filter_override_flag = 0;
    slice->slice_deblocking_filter_disabled_flag = m_pps.pps_deblocking_filter_disabled_flag;
    slice->slice_tc_offset_div2 = m_pps.pps_tc_offset_div2;
    slice->slice_beta_offset_div2 = m_pps.pps_beta_offset_div2;
    slice->slice_loop_filter_across_slices_enabled_flag = m_pps.pps_loop_filter_across_slices_enabled_flag;
//    slice->deblocking_filter_override_flag = 0;
    slice->slice_cb_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_cr_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_temporal_mvp_enabled_flag = m_sps.sps_temporal_mvp_enabled_flag;

    if (m_videoParam.BPyramid) {
        Ipp8u num_ref0 = m_videoParam.MaxRefIdxL0;
        Ipp8u num_ref1 = m_videoParam.MaxRefIdxL1;
        if (slice->slice_type == B_SLICE) {
            if (num_ref0 > 1) num_ref0 >>= 1;
            if (num_ref1 > 1) num_ref1 >>= 1;
        }
        slice->num_ref_idx_l0_active = num_ref0;
        slice->num_ref_idx_l1_active = num_ref1;
    } else {
        slice->num_ref_idx_l0_active = m_pps.num_ref_idx_l0_default_active;
        slice->num_ref_idx_l1_active = m_pps.num_ref_idx_l1_default_active;
    }

    slice->pgop_idx = slice->slice_type == P_SLICE ? (Ipp8u)m_pCurrentFrame->m_PGOPIndex : 0;
    if (m_videoParam.GeneralizedPB && slice->slice_type == P_SLICE) {
        slice->slice_type = B_SLICE;
        slice->num_ref_idx_l1_active = slice->num_ref_idx_l0_active;
    }


    slice->short_term_ref_pic_set_sps_flag = 1;
    if (m_pCurrentFrame->m_RPSIndex > 0) {
        slice->short_term_ref_pic_set_idx = m_pCurrentFrame->m_RPSIndex;
        if (!m_videoParam.BPyramid)
            slice->num_ref_idx_l0_active = m_videoParam.MaxBRefIdxL0;
    } else if (m_pCurrentFrame->m_PGOPIndex == 0)
        slice->short_term_ref_pic_set_idx = 0;
    else
        slice->short_term_ref_pic_set_idx = m_videoParam.GopRefDist - 1 + m_pCurrentFrame->m_PGOPIndex;

    slice->slice_num = curr_slice;

    slice->five_minus_max_num_merge_cand = 5 - MAX_NUM_MERGE_CANDS;
    slice->slice_qp_delta = m_videoParam.QP - m_pps.init_qp;

    if (m_pps.entropy_coding_sync_enabled_flag) {
        slice->row_first = slice->slice_segment_address / m_videoParam.PicWidthInCtbs;
        slice->row_last = slice->slice_address_last_ctb / m_videoParam.PicWidthInCtbs;
        slice->num_entry_point_offsets = slice->row_last - slice->row_first;
    }

    slice->rd_opt_flag = 1;
    slice->rd_lambda = 1;
    if (slice->rd_opt_flag) {
        slice->rd_lambda = pow(2.0, (m_videoParam.QP - 12) * (1.0/3.0)) * (1.0 / 256.0);
        switch (slice->slice_type) {
        case P_SLICE:
            if (m_videoParam.BPyramid && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = m_BpyramidRefLayers[m_pCurrentFrame->m_PicOrderCnt % m_videoParam.GopRefDist];
                slice->rd_lambda *= tab_rdLambdaBPyramid[layer];
                if (layer > 0)
                    slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6.0));
            } else {
                if (m_videoParam.PGopPicSize == 1 || slice->pgop_idx)
                    slice->rd_lambda *= 0.4624;
                else
                    slice->rd_lambda *= 0.578;
                if (slice->pgop_idx)
                    slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6));
            }
            break;
        case B_SLICE:
            if (m_videoParam.BPyramid && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = m_BpyramidRefLayers[m_pCurrentFrame->m_PicOrderCnt % m_videoParam.GopRefDist];
                slice->rd_lambda *= tab_rdLambdaBPyramid[layer];
                if (layer > 0)
                    slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6.0));
            } else {
                slice->rd_lambda *= 0.4624;
                slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6));
            }
            break;
        case I_SLICE:
        default:
            slice->rd_lambda *= 0.57;
            if (m_videoParam.GopRefDist > 1)
                slice->rd_lambda *= (1 - MIN(0.5,0.05*(m_videoParam.GopRefDist - 1)));
        }
    }
    slice->rd_lambda_inter = slice->rd_lambda;
    slice->rd_lambda_inter_mv = slice->rd_lambda_inter;

    //kolya
    slice->rd_lambda_sqrt = sqrt(slice->rd_lambda*256);

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetSlice(H265Slice *slice, Ipp32u curr_slice, H265Frame *frame)
{
    memset(slice, 0, sizeof(H265Slice));

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32u numSlices = m_videoParam.NumSlices;
    Ipp32u slice_len = numCtbs / numSlices;
    //    if (!m_videoParam.threading_by_rows)
    //        slice_len = slice_len / m_videoParam.PicWidthInCtbs * m_videoParam.PicWidthInCtbs;

    slice->slice_segment_address = slice_len * curr_slice;
    slice->slice_address_last_ctb = slice_len * (curr_slice + 1) - 1;
    if (slice->slice_address_last_ctb > numCtbs - 1 || curr_slice == numSlices - 1)
        slice->slice_address_last_ctb = numCtbs - 1;

    if (curr_slice) slice->first_slice_segment_in_pic_flag = 0;
    else slice->first_slice_segment_in_pic_flag = 1;

    slice->slice_pic_parameter_set_id = m_pps.pps_pic_parameter_set_id;

    switch (frame->m_PicCodType) {
    case MFX_FRAMETYPE_P:
        slice->slice_type = P_SLICE; break;
    case MFX_FRAMETYPE_B:
        slice->slice_type = B_SLICE; break;
    case MFX_FRAMETYPE_I:
    default:
        slice->slice_type = I_SLICE; break;
    }

    if (frame->m_bIsIDRPic)
    {
        slice->IdrPicFlag = 1;
        slice->RapPicFlag = 1;
    }

    //    slice->rap_pic_id = m_frameCount & 0xffff;
    slice->slice_pic_order_cnt_lsb =  frame->PicOrderCnt() & ~(0xffffffff << m_sps.log2_max_pic_order_cnt_lsb);
    slice->deblocking_filter_override_flag = 0;
    slice->slice_deblocking_filter_disabled_flag = m_pps.pps_deblocking_filter_disabled_flag;
    slice->slice_tc_offset_div2 = m_pps.pps_tc_offset_div2;
    slice->slice_beta_offset_div2 = m_pps.pps_beta_offset_div2;
    slice->slice_loop_filter_across_slices_enabled_flag = m_pps.pps_loop_filter_across_slices_enabled_flag;
    //    slice->deblocking_filter_override_flag = 0;
    slice->slice_cb_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_cr_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_temporal_mvp_enabled_flag = m_sps.sps_temporal_mvp_enabled_flag;

    if (m_videoParam.BPyramid) {
        Ipp8u num_ref0 = m_videoParam.MaxRefIdxL0;
        Ipp8u num_ref1 = m_videoParam.MaxRefIdxL1;
        if (slice->slice_type == B_SLICE) {
            if (num_ref0 > 1) num_ref0 >>= 1;
            if (num_ref1 > 1) num_ref1 >>= 1;
        }
        slice->num_ref_idx_l0_active = num_ref0;
        slice->num_ref_idx_l1_active = num_ref1;
    } else {
        slice->num_ref_idx_l0_active = m_pps.num_ref_idx_l0_default_active;
        slice->num_ref_idx_l1_active = m_pps.num_ref_idx_l1_default_active;
    }

    slice->pgop_idx = slice->slice_type == P_SLICE ? (Ipp8u)frame->m_PGOPIndex : 0;
    if (m_videoParam.GeneralizedPB && slice->slice_type == P_SLICE) {
        slice->slice_type = B_SLICE;
        slice->num_ref_idx_l1_active = slice->num_ref_idx_l0_active;
    }


    slice->short_term_ref_pic_set_sps_flag = 1;
    if (frame->m_RPSIndex > 0) {
        slice->short_term_ref_pic_set_idx = frame->m_RPSIndex;
        if (!m_videoParam.BPyramid)
            slice->num_ref_idx_l0_active = m_videoParam.MaxBRefIdxL0;
    } else if (frame->m_PGOPIndex == 0)
        slice->short_term_ref_pic_set_idx = 0;
    else
        slice->short_term_ref_pic_set_idx = m_videoParam.GopRefDist - 1 + frame->m_PGOPIndex;

    slice->slice_num = curr_slice;

    slice->five_minus_max_num_merge_cand = 5 - MAX_NUM_MERGE_CANDS;
    slice->slice_qp_delta = m_videoParam.QP - m_pps.init_qp;

    if (m_pps.entropy_coding_sync_enabled_flag) {
        slice->row_first = slice->slice_segment_address / m_videoParam.PicWidthInCtbs;
        slice->row_last = slice->slice_address_last_ctb / m_videoParam.PicWidthInCtbs;
        slice->num_entry_point_offsets = slice->row_last - slice->row_first;
    }

    slice->rd_opt_flag = 1;
    slice->rd_lambda = 1;
    if (slice->rd_opt_flag) {
        slice->rd_lambda = pow(2.0, (m_videoParam.QP - 12) * (1.0/3.0)) * (1.0 / 256.0);
        switch (slice->slice_type) {
        case P_SLICE:
            if (m_videoParam.BPyramid && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = m_BpyramidRefLayers[frame->m_PicOrderCnt % m_videoParam.GopRefDist];
                slice->rd_lambda *= tab_rdLambdaBPyramid[layer];
                if (layer > 0)
                    slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6.0));
            } else {
                if (m_videoParam.PGopPicSize == 1 || slice->pgop_idx)
                    slice->rd_lambda *= 0.4624;
                else
                    slice->rd_lambda *= 0.578;
                if (slice->pgop_idx)
                    slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6));
            }
            break;
        case B_SLICE:
            if (m_videoParam.BPyramid && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = m_BpyramidRefLayers[frame->m_PicOrderCnt % m_videoParam.GopRefDist];
                slice->rd_lambda *= tab_rdLambdaBPyramid[layer];
                if (layer > 0)
                    slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6.0));
            } else {
                slice->rd_lambda *= 0.4624;
                slice->rd_lambda *= MAX(2,MIN(4,(m_videoParam.QP - 12)/6));
            }
            break;
        case I_SLICE:
        default:
            slice->rd_lambda *= 0.57;
            if (m_videoParam.GopRefDist > 1)
                slice->rd_lambda *= (1 - MIN(0.5,0.05*(m_videoParam.GopRefDist - 1)));
        }
    }
    slice->rd_lambda_inter = slice->rd_lambda;
    slice->rd_lambda_inter_mv = slice->rd_lambda_inter;

    //kolya
    slice->rd_lambda_sqrt = sqrt(slice->rd_lambda*256);

    return MFX_ERR_NONE;
}

static Ipp8u dpoc_neg[][8][5] = {
    //GopRefDist 4
    {{2, 4, 2},
    {1, 1},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{4, 8, 2, 2, 4},
    {1, 1},
    {2, 2, 2},
    {2, 1, 2},
    {2, 4, 2},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4}},
};

static Ipp8u dpoc_pos[][8][5] = {
    //GopRefDist 4
    {{0},
    {2, 1, 2},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{0,},
    {3, 1, 2, 4},
    {2, 2, 4},
    {2, 1, 4},
    {1, 4},
    {2, 1, 2},
    {1, 2},
    {1, 1}}
};

void H265Encoder::InitShortTermRefPicSet()
{
    Ipp32s i, j;
    Ipp32s deltaPoc;

    m_numShortTermRefPicSets = 0;

    if (m_videoParam.BPyramid && m_videoParam.GopRefDist > 2) {
        Ipp8u r = m_videoParam.GopRefDist == 4 ? 0 : 1;
        for (i = 0; i < 8; i++){
            m_ShortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_ShortRefPicSet[i].num_negative_pics = dpoc_neg[r][i][0];
            m_ShortRefPicSet[i].num_positive_pics = dpoc_pos[r][i][0];

            for (j = 0; j < m_ShortRefPicSet[i].num_negative_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[j] = dpoc_neg[r][i][1+j];
                m_ShortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (j = 0; j < m_ShortRefPicSet[i].num_positive_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[m_ShortRefPicSet[i].num_negative_pics + j] = dpoc_pos[r][i][1+j];
                m_ShortRefPicSet[i].used_by_curr_pic_flag[m_ShortRefPicSet[i].num_negative_pics + j] = 1;
            }
            m_numShortTermRefPicSets++;
        }
        m_sps.num_short_term_ref_pic_sets = (Ipp8u)m_numShortTermRefPicSets;
        return;
    }

    deltaPoc = m_videoParam.TreatBAsReference ? 1 : m_videoParam.GopRefDist;

    for (i = 0; i < m_videoParam.PGopPicSize; i++)
    {
        Ipp32s idx = i == 0 ? 0 : m_videoParam.GopRefDist - 1 + i;
        m_ShortRefPicSet[idx].inter_ref_pic_set_prediction_flag = 0;
        m_ShortRefPicSet[idx].num_negative_pics = m_videoParam.MaxRefIdxL0;
        m_ShortRefPicSet[idx].num_positive_pics = 0;
        for (j = 0; j < m_ShortRefPicSet[idx].num_negative_pics; j++) {
            Ipp32s deltaPoc_cur;
            if (j == 0) deltaPoc_cur = 1;
            else if (j == 1) deltaPoc_cur = (m_videoParam.PGopPicSize - 1 + i) % m_videoParam.PGopPicSize;
            else deltaPoc_cur = m_videoParam.PGopPicSize;
            if (deltaPoc_cur == 0) deltaPoc_cur =  m_videoParam.PGopPicSize;
            m_ShortRefPicSet[idx].delta_poc[j] = deltaPoc_cur * m_videoParam.GopRefDist;
            m_ShortRefPicSet[idx].used_by_curr_pic_flag[j] = 1;
        }
        m_numShortTermRefPicSets++;
    }

/*
    if (m_videoParam.BPyramid) {
        for (i = 1; i < m_videoParam.GopRefDist; i++)
        {
            m_ShortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_ShortRefPicSet[i].num_negative_pics = 1;
            m_ShortRefPicSet[i].num_positive_pics = 1;

            deltaPoc = 1;
            while(m_BpyramidRefLayers[i-deltaPoc] >= m_BpyramidRefLayers[i])
                deltaPoc++;

            m_ShortRefPicSet[i].delta_poc[0] = deltaPoc;
            m_ShortRefPicSet[i].used_by_curr_pic_flag[0] = 1;
            for (j = 1; j < m_ShortRefPicSet[i].num_negative_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[j] = deltaPoc;
                m_ShortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (j = 0; j < m_ShortRefPicSet[i].num_positive_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[m_ShortRefPicSet[i].num_negative_pics + j] = deltaPoc;
                m_ShortRefPicSet[i].used_by_curr_pic_flag[m_ShortRefPicSet[i].num_negative_pics + j] = 1;
            }
            m_numShortTermRefPicSets++;
        }
    } else*/
    {
        for (i = 1; i < m_videoParam.GopRefDist; i++)
        {
            m_ShortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_ShortRefPicSet[i].num_negative_pics = m_videoParam.MaxBRefIdxL0;
            m_ShortRefPicSet[i].num_positive_pics = 1;

            m_ShortRefPicSet[i].delta_poc[0] = m_videoParam.TreatBAsReference ? 1 : i;
            m_ShortRefPicSet[i].used_by_curr_pic_flag[0] = 1;
            for (j = 1; j < m_ShortRefPicSet[i].num_negative_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[j] = deltaPoc;
                m_ShortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (j = 0; j < m_ShortRefPicSet[i].num_positive_pics; j++) {
                m_ShortRefPicSet[i].delta_poc[m_ShortRefPicSet[i].num_negative_pics + j] = m_videoParam.GopRefDist - i;
                m_ShortRefPicSet[i].used_by_curr_pic_flag[m_ShortRefPicSet[i].num_negative_pics + j] = 1;
            }
            m_numShortTermRefPicSets++;
        }
    }

    m_sps.num_short_term_ref_pic_sets = (Ipp8u)m_numShortTermRefPicSets;
}

mfxStatus H265Encoder::Init(const mfxVideoParam *param, const mfxExtCodingOptionHEVC *opts_hevc) {
#ifdef MFX_ENABLE_WATERMARK
    m_watermark = Watermark::CreateFromResource();
    if (NULL == m_watermark)
        return MFX_ERR_UNKNOWN;
#endif

    mfxStatus sts = InitH265VideoParam(param, opts_hevc);
    MFX_CHECK_STS(sts);

    if (m_videoParam.enableCmFlag)
        AllocateCmResources(param->mfx.FrameInfo.Width, param->mfx.FrameInfo.Height, param->mfx.NumRefFrame);

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    profile_frequency = m_videoParam.GopRefDist;
    data_temp_size = ((MAX_TOTAL_DEPTH * 2 + 1) << m_videoParam.Log2NumPartInCU);

    // temp buf size - todo reduce
    Ipp32u streamBufSize = m_videoParam.SourceWidth * m_videoParam.SourceHeight * 3 / 2;
    Ipp32u memSize = 0;

    memSize += sizeof(H265BsReal)*(m_videoParam.num_thread_structs + 1) + DATA_ALIGN;
    memSize += sizeof(H265BsFake)*m_videoParam.num_thread_structs + DATA_ALIGN;
    memSize += streamBufSize*(m_videoParam.num_thread_structs+1);
    memSize += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs;
    memSize += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN;
    memSize += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN; // for future slice
    memSize += sizeof(H265CUData) * data_temp_size * m_videoParam.num_threads + DATA_ALIGN;
    memSize += numCtbs;
    memSize += sizeof(H265EncoderRowInfo) * m_videoParam.PicHeightInCtbs + DATA_ALIGN;
    //memSize += sizeof(Ipp32u) * profile_frequency + DATA_ALIGN;
    memSize += sizeof(H265CU) * m_videoParam.num_threads + DATA_ALIGN;
    memSize += (1 << 16); // for m_logMvCostTable

    memBuf = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(memBuf);

    Ipp8u *ptr = memBuf;

    bs = UMC::align_pointer<H265BsReal*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsReal)*(m_videoParam.num_thread_structs + 1) + DATA_ALIGN;
    bsf = UMC::align_pointer<H265BsFake*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsFake)*(m_videoParam.num_thread_structs) + DATA_ALIGN;

    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs+1; i++) {
        bs[i].m_base.m_pbsBase = ptr;
        bs[i].m_base.m_maxBsSize = streamBufSize;
        bs[i].Reset();
        ptr += streamBufSize;
    }
    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs; i++) {
        bsf[i].Reset();
    }

    m_context_array_wpp = ptr;
    ptr += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs;

    m_slices = UMC::align_pointer<H265Slice*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN;

    m_slicesNext = UMC::align_pointer<H265Slice*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265Slice) * m_videoParam.NumSlices + DATA_ALIGN;

    data_temp = UMC::align_pointer<H265CUData*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265CUData) * data_temp_size * m_videoParam.num_threads + DATA_ALIGN;

    m_slice_ids = ptr;
    ptr += numCtbs;

    m_row_info = UMC::align_pointer<H265EncoderRowInfo*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265EncoderRowInfo) * m_videoParam.PicHeightInCtbs + DATA_ALIGN;

    //eFrameType = UMC::align_pointer<Ipp32u*>(ptr, DATA_ALIGN);
    //ptr += sizeof(Ipp32u) * profile_frequency + DATA_ALIGN;

    cu = UMC::align_pointer<H265CU*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265CU) * m_videoParam.num_threads + DATA_ALIGN;

    m_logMvCostTable = ptr;
    ptr += (1 << 16);

    // init lookup table for 2*log2(x)+2
    m_logMvCostTable[(1 << 15)] = 1;
    Ipp32f log2reciproc = 2 / log(2.0f);
    for (Ipp32s i = 1; i < (1 << 15); i++)
        m_logMvCostTable[(1 << 15) + i] = m_logMvCostTable[(1 << 15) - i] = (Ipp8u)(log(i + 1.0f) * log2reciproc + 2);

    m_iProfileIndex = 0;
    //m_bMakeNextFrameKey = true; // Ensure that we always start with a key frame.
    m_bMakeNextFrameIDR = true;
    m_uIntraFrameInterval = 0;
    m_uIDRFrameInterval = 0;
    m_l1_cnt_to_start_B = 0;
    m_PicOrderCnt = 0;
    m_PicOrderCnt_Accu = 0;
    m_PGOPIndex = 0;
    m_frameCountEncoded = 0;

    m_Bpyramid_currentNumFrame = 0;
    m_Bpyramid_nextNumFrame = 0;
    m_Bpyramid_maxNumFrame = 0;

    if (m_videoParam.BPyramid)
    {
        Ipp8u n = 0;
        Ipp32s GopRefDist = m_videoParam.GopRefDist;
        Ipp32s i, j, k;

        while (((GopRefDist & 1) == 0) && GopRefDist > 1) {
            GopRefDist >>= 1;
            n ++;
        }

        m_Bpyramid_maxNumFrame = 1 << n;
        m_BpyramidTab[0] = 0;
        m_BpyramidTab[(Ipp32u)(1 << n)] = 0;
        m_BpyramidTabRight[0] = 0;
        m_BpyramidRefLayers[0] = 0;

        for (i = n - 1, j = 0; i >= 0; i--, j++)
        {
            for (k = 0; k < (1 << j); k++)
            {
                if ((k & 1) == 0)
                {
                    m_BpyramidTab[(1<<i)+(k*(1<<(i+1)))] = m_BpyramidTab[(k+1)*(1<<(i+1))] + 1;
                }
                else
                {
                    m_BpyramidTab[(1<<i)+(k*(1<<(i+1)))] = m_BpyramidTab[k*(1<<(i+1))] + (1 << (i+1));
                }

                m_BpyramidTabRight[m_BpyramidTab[(1<<i)+(k*(1<<(i+1)))]] = m_BpyramidTab[(1<<(i+1))+(k*(1<<(i+1)))];
                m_BpyramidRefLayers[(1<<i)+(k*(1<<(i+1)))] = (Ipp8u)(j + 1);
            }
        }
    }
    m_BpyramidRefLayers[m_videoParam.GopRefDist] = 0;

    if (param->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        m_brc = new H265BRC();
        mfxStatus sts = m_brc->Init(param);
        if (MFX_ERR_NONE != sts)
            return sts;
        m_videoParam.QP = (Ipp8s)m_brc->GetQP(MFX_FRAMETYPE_I);
        m_videoParam.QPChroma = h265_QPtoChromaQP[m_videoParam.QP];
    } else {
        m_videoParam.QPIChroma = h265_QPtoChromaQP[m_videoParam.QPI];
        m_videoParam.QPPChroma = h265_QPtoChromaQP[m_videoParam.QPP];
        m_videoParam.QPBChroma = h265_QPtoChromaQP[m_videoParam.QPB];
        m_videoParam.QP = m_videoParam.QPI;
        m_videoParam.QPChroma = m_videoParam.QPIChroma;
    }

    for (Ipp32s i = 0; i < m_videoParam.MaxTotalDepth; i++) {
        m_videoParam.cu_split_threshold_cu_intra[i] = h265_calc_split_threshold(0, m_videoParam.Log2MaxCUSize - i,
            m_videoParam.SplitThresholdStrengthCUIntra, m_videoParam.QP);
        m_videoParam.cu_split_threshold_tu_intra[i] = h265_calc_split_threshold(1, m_videoParam.Log2MaxCUSize - i,
            m_videoParam.SplitThresholdStrengthTUIntra, m_videoParam.QP);
        m_videoParam.cu_split_threshold_cu_inter[i] = h265_calc_split_threshold(0, m_videoParam.Log2MaxCUSize - i,
            m_videoParam.SplitThresholdStrengthCUInter, m_videoParam.QP);
    }
    for (Ipp32s i = m_videoParam.MaxTotalDepth; i < MAX_TOTAL_DEPTH; i++) {
        m_videoParam.cu_split_threshold_cu_intra[i] = m_videoParam.cu_split_threshold_tu_intra[i] =
            m_videoParam.cu_split_threshold_cu_inter[i] = 0;
    }


    SetProfileLevel();
    SetVPS();
    SetSPS();
    SetPPS();

    m_videoParam.csps = &m_sps;
    m_videoParam.cpps = &m_pps;

    m_pReconstructFrame = new H265Frame();
    if (m_pReconstructFrame->Create(&m_videoParam) != MFX_ERR_NONE )
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    //eFrameType[0] = MFX_FRAMETYPE_P;
    //for (Ipp32s i = 1; i < profile_frequency; i++)
    //{
    //    eFrameType[i] = MFX_FRAMETYPE_B;
    //}

    m_videoParam.m_slice_ids = m_slice_ids;

    ippsZero_8u((Ipp8u *)cu, sizeof(H265CU) * m_videoParam.num_threads);
    ippsZero_8u((Ipp8u*)data_temp, sizeof(H265CUData) * data_temp_size * m_videoParam.num_threads);

    MFX_HEVC_PP::InitDispatcher();

    // SAO
    //-----------------------------------------------------
    m_saoDecodeFilter.Init(m_videoParam.Width, m_videoParam.Height, m_videoParam.MaxCUSize, 0);
    m_saoParam.resize(m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs);

    for(Ipp32u idx = 0; idx < m_videoParam.num_threads; idx++)
    {
        cu[idx].m_saoEncodeFilter.Init(m_videoParam.Width, m_videoParam.Height, 1 << m_videoParam.Log2MaxCUSize, m_videoParam.MaxCUDepth);
    }
    //-----------------------------------------------------

    return sts;
}

void H265Encoder::Close() {
    if (m_videoParam.enableCmFlag) {
        PrintTimes();
        FreeCmResources();
    }
    if (m_pReconstructFrame) {
        m_pReconstructFrame->Destroy();
        delete m_pReconstructFrame;
    }
    if (m_brc)
        delete m_brc;

    if (memBuf) {
        H265_Free(memBuf);
        memBuf = NULL;
    }

    m_saoDecodeFilter.Close();

#ifdef MFX_ENABLE_WATERMARK
    if (m_watermark)
        m_watermark->Release();
#endif
}

Ipp32u H265Encoder::DetermineFrameType()
{
    // GOP is considered here as I frame and all following till next I
    // Display (==input) order
    Ipp32u ePictureType; //= eFrameType[m_iProfileIndex];

    m_bMakeNextFrameIDR = false;

    mfxEncodeStat eStat;
    mfx_video_encode_h265_ptr->GetEncodeStat(&eStat);
///    if (m_frameCountEncoded == 0) {  !!!sergo

    if (eStat.NumFrameCountAsync == 0) {
        m_iProfileIndex = 0;
        m_uIntraFrameInterval = 0;
        m_uIDRFrameInterval = 0;
        ePictureType = MFX_FRAMETYPE_I;
        m_bMakeNextFrameIDR = true;
        return ePictureType;
    }

    m_iProfileIndex++;
    if (m_iProfileIndex == profile_frequency) {
        m_iProfileIndex = 0;
        ePictureType = MFX_FRAMETYPE_P;
    } else {
        ePictureType = MFX_FRAMETYPE_B;
        if (m_uIntraFrameInterval+2 == m_videoParam.GopPicSize &&
            (m_videoParam.GopClosedFlag || m_uIDRFrameInterval == m_videoParam.IdrInterval) )
            if (!m_videoParam.BPyramid && !m_videoParam.GopStrictFlag)
                ePictureType = MFX_FRAMETYPE_P;
            // else ?? // B-group will start next GOP using only L1 ref
    }

    m_uIntraFrameInterval++;
    if (m_uIntraFrameInterval == m_videoParam.GopPicSize) {
        ePictureType = MFX_FRAMETYPE_I;
        m_uIntraFrameInterval = 0;
        m_iProfileIndex = 0;

        m_uIDRFrameInterval++;
        if (m_uIDRFrameInterval == m_videoParam.IdrInterval + 1) {
            m_bMakeNextFrameIDR = true;
            m_uIDRFrameInterval = 0;
        }
    }

    return ePictureType;
}

//
// move all frames in WaitingForRef to ReadyToEncode
//
mfxStatus H265Encoder::MoveFromCPBToDPB()
{
    //   EnumPicCodType  ePictureType;
    m_cpb.RemoveFrame(m_pCurrentFrame);
    m_dpb.insertAtCurrent(m_pCurrentFrame);
    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::CleanDPB()
{
    H265Frame *pFrm=m_dpb.findNextDisposable();
    //   EnumPicCodType  ePictureType;
    mfxStatus      ps = MFX_ERR_NONE;
    while(pFrm!=NULL) {
        m_dpb.RemoveFrame(pFrm);
        m_cpb.insertAtCurrent(pFrm);
        pFrm=m_dpb.findNextDisposable();
    }
    return ps;
}

void H265Encoder::CreateRefPicSet(H265Slice *curr_slice)
{
    Ipp32s CurrPicOrderCnt = m_pCurrentFrame->PicOrderCnt();
    H265Frame **refFrames = m_pCurrentFrame->m_refPicList[0].m_refFrames;
    H265ShortTermRefPicSet* RefPicSet;
    Ipp32s ExcludedPOC = -1;
    Ipp32u NumShortTermL0, NumShortTermL1, NumLongTermL0, NumLongTermL1;

    m_dpb.countActiveRefs(NumShortTermL0, NumLongTermL0, NumShortTermL1, NumLongTermL1, CurrPicOrderCnt);

    if ((NumShortTermL0 + NumShortTermL1 + NumLongTermL0 + NumLongTermL1) >= (Ipp32u)m_sps.num_ref_frames) {
        if ((NumShortTermL0 + NumShortTermL1) > 0) {
            H265Frame *Frm = m_dpb.findMostDistantShortTermRefs(CurrPicOrderCnt);
            ExcludedPOC = Frm->PicOrderCnt();

            if (ExcludedPOC < CurrPicOrderCnt)
                NumShortTermL0--;
            else
                NumShortTermL1--;
        }
        else {
            H265Frame *Frm = m_dpb.findMostDistantLongTermRefs(CurrPicOrderCnt);
            ExcludedPOC = Frm->PicOrderCnt();

            if (ExcludedPOC < CurrPicOrderCnt)
                NumLongTermL0--;
            else
                NumLongTermL1--;
        }
    }

    curr_slice->short_term_ref_pic_set_sps_flag = 0;
    RefPicSet = m_ShortRefPicSet + m_sps.num_short_term_ref_pic_sets;
    RefPicSet->inter_ref_pic_set_prediction_flag = 0;

    /* Long term TODO */

    /* Short Term L0 */
    if (NumShortTermL0 > 0) {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s i, prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future()) {
            Ipp32s poc = pFrm->PicOrderCnt();
            Ipp32s j, k;

            if (pFrm->isShortTermRef() && (poc < CurrPicOrderCnt) && (poc != ExcludedPOC)) {
                // find insertion point
                j = 0;
                while ((j < NumFramesInList) && (refFrames[j]->PicOrderCnt() > poc))
                    j++;

                // make room if needed
                if (j < NumFramesInList && refFrames[j])
                    for (k = NumFramesInList; k > j; k--)
                        refFrames[k] = refFrames[k-1];

                // add the short-term reference
                refFrames[j] = pFrm;
                NumFramesInList++;
            }
        }

        RefPicSet->num_negative_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (i = 0; i < RefPicSet->num_negative_pics; i++) {
            Ipp32s DeltaPoc = refFrames[i]->PicOrderCnt() - CurrPicOrderCnt;
            RefPicSet->delta_poc[i] = prev - DeltaPoc;
            prev = DeltaPoc;
        }

        if (curr_slice->slice_type == I_SLICE)
            numRefs = 0;
        else
            numRefs = (Ipp32s)curr_slice->num_ref_idx_l0_active;

        Ipp32s used_count = 0;
        for (i = 0; i < RefPicSet->num_negative_pics; i++) {
            if (used_count < numRefs && refFrames[i]->m_PGOPIndex == 0) {
                RefPicSet->used_by_curr_pic_flag[i] = 1;
                used_count++;
            }
            else {
                RefPicSet->used_by_curr_pic_flag[i] = 0;
            }
        }
        for (i = 0; i < RefPicSet->num_negative_pics; i++) {
            if (used_count < numRefs && RefPicSet->used_by_curr_pic_flag[i] == 0) {
                RefPicSet->used_by_curr_pic_flag[i] = 1;
                used_count++;
            }
        }
    }
    else {
        RefPicSet->num_negative_pics = 0;
    }

    /* Short Term L1 */
    if (NumShortTermL1 > 0) {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s i, prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future()) {
            Ipp32s poc = pFrm->PicOrderCnt();
            Ipp32s j, k;

            if (pFrm->isShortTermRef()&& (poc > CurrPicOrderCnt) && (poc != ExcludedPOC)) {
                // find insertion point
                j = 0;
                while ((j < NumFramesInList) && (refFrames[j]->PicOrderCnt() < poc))
                    j++;

                // make room if needed
                if (j < NumFramesInList && refFrames[j])
                    for (k = NumFramesInList; k > j; k--)
                        refFrames[k] = refFrames[k-1];

                // add the short-term reference
                refFrames[j] = pFrm;
                NumFramesInList++;
            }
        }

        RefPicSet->num_positive_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (i = 0; i < RefPicSet->num_positive_pics; i++) {
            Ipp32s DeltaPoc = refFrames[i]->PicOrderCnt() - CurrPicOrderCnt;
            RefPicSet->delta_poc[RefPicSet->num_negative_pics + i] = DeltaPoc - prev;
            prev = DeltaPoc;
        }

        if (curr_slice->slice_type == B_SLICE)
            numRefs = m_videoParam.MaxRefIdxL1 + 1;
        else
            numRefs = 0;

        if (numRefs > RefPicSet->num_positive_pics)
            numRefs = RefPicSet->num_positive_pics;

        for (i = 0; i < numRefs; i++)
            RefPicSet->used_by_curr_pic_flag[RefPicSet->num_negative_pics + i] = 1;

        for (i = numRefs; i < RefPicSet->num_positive_pics; i++)
            RefPicSet->used_by_curr_pic_flag[RefPicSet->num_negative_pics + i] = 0;
    }
    else {
        RefPicSet->num_positive_pics = 0;
    }
}

void H265Encoder::CreateRefPicSet(H265Slice *slice, H265Frame *frame)
{
    Ipp32s CurrPicOrderCnt = frame->PicOrderCnt();
    H265Frame **refFrames = frame->m_refPicList[0].m_refFrames;
    H265ShortTermRefPicSet* RefPicSet;
    Ipp32s ExcludedPOC = -1;
    Ipp32u NumShortTermL0, NumShortTermL1, NumLongTermL0, NumLongTermL1;

    m_dpb.countActiveRefs(NumShortTermL0, NumLongTermL0, NumShortTermL1, NumLongTermL1, CurrPicOrderCnt);

    if ((NumShortTermL0 + NumShortTermL1 + NumLongTermL0 + NumLongTermL1) >= (Ipp32u)m_sps.num_ref_frames) {
        if ((NumShortTermL0 + NumShortTermL1) > 0) {
            H265Frame *Frm = m_dpb.findMostDistantShortTermRefs(CurrPicOrderCnt);
            ExcludedPOC = Frm->PicOrderCnt();

            if (ExcludedPOC < CurrPicOrderCnt)
                NumShortTermL0--;
            else
                NumShortTermL1--;
        }
        else {
            H265Frame *Frm = m_dpb.findMostDistantLongTermRefs(CurrPicOrderCnt);
            ExcludedPOC = Frm->PicOrderCnt();

            if (ExcludedPOC < CurrPicOrderCnt)
                NumLongTermL0--;
            else
                NumLongTermL1--;
        }
    }

    slice->short_term_ref_pic_set_sps_flag = 0;
    RefPicSet = m_ShortRefPicSet + m_sps.num_short_term_ref_pic_sets;
    RefPicSet->inter_ref_pic_set_prediction_flag = 0;

    /* Long term TODO */

    /* Short Term L0 */
    if (NumShortTermL0 > 0) {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s i, prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future()) {
            Ipp32s poc = pFrm->PicOrderCnt();
            Ipp32s j, k;

            if (pFrm->isShortTermRef() && (poc < CurrPicOrderCnt) && (poc != ExcludedPOC)) {
                // find insertion point
                j = 0;
                while ((j < NumFramesInList) && (refFrames[j]->PicOrderCnt() > poc))
                    j++;

                // make room if needed
                if (j < NumFramesInList && refFrames[j])
                    for (k = NumFramesInList; k > j; k--)
                        refFrames[k] = refFrames[k-1];

                // add the short-term reference
                refFrames[j] = pFrm;
                NumFramesInList++;
            }
        }

        RefPicSet->num_negative_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (i = 0; i < RefPicSet->num_negative_pics; i++) {
            Ipp32s DeltaPoc = refFrames[i]->PicOrderCnt() - CurrPicOrderCnt;
            RefPicSet->delta_poc[i] = prev - DeltaPoc;
            prev = DeltaPoc;
        }

        if (slice->slice_type == I_SLICE)
            numRefs = 0;
        else
            numRefs = (Ipp32s)slice->num_ref_idx_l0_active;

        Ipp32s used_count = 0;
        for (i = 0; i < RefPicSet->num_negative_pics; i++) {
            if (used_count < numRefs && refFrames[i]->m_PGOPIndex == 0) {
                RefPicSet->used_by_curr_pic_flag[i] = 1;
                used_count++;
            }
            else {
                RefPicSet->used_by_curr_pic_flag[i] = 0;
            }
        }
        for (i = 0; i < RefPicSet->num_negative_pics; i++) {
            if (used_count < numRefs && RefPicSet->used_by_curr_pic_flag[i] == 0) {
                RefPicSet->used_by_curr_pic_flag[i] = 1;
                used_count++;
            }
        }
    }
    else {
        RefPicSet->num_negative_pics = 0;
    }

    /* Short Term L1 */
    if (NumShortTermL1 > 0) {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s i, prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future()) {
            Ipp32s poc = pFrm->PicOrderCnt();
            Ipp32s j, k;

            if (pFrm->isShortTermRef()&& (poc > CurrPicOrderCnt) && (poc != ExcludedPOC)) {
                // find insertion point
                j = 0;
                while ((j < NumFramesInList) && (refFrames[j]->PicOrderCnt() < poc))
                    j++;

                // make room if needed
                if (j < NumFramesInList && refFrames[j])
                    for (k = NumFramesInList; k > j; k--)
                        refFrames[k] = refFrames[k-1];

                // add the short-term reference
                refFrames[j] = pFrm;
                NumFramesInList++;
            }
        }

        RefPicSet->num_positive_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (i = 0; i < RefPicSet->num_positive_pics; i++) {
            Ipp32s DeltaPoc = refFrames[i]->PicOrderCnt() - CurrPicOrderCnt;
            RefPicSet->delta_poc[RefPicSet->num_negative_pics + i] = DeltaPoc - prev;
            prev = DeltaPoc;
        }

        if (slice->slice_type == B_SLICE)
            numRefs = m_videoParam.MaxRefIdxL1 + 1;
        else
            numRefs = 0;

        if (numRefs > RefPicSet->num_positive_pics)
            numRefs = RefPicSet->num_positive_pics;

        for (i = 0; i < numRefs; i++)
            RefPicSet->used_by_curr_pic_flag[RefPicSet->num_negative_pics + i] = 1;

        for (i = numRefs; i < RefPicSet->num_positive_pics; i++)
            RefPicSet->used_by_curr_pic_flag[RefPicSet->num_negative_pics + i] = 0;
    }
    else {
        RefPicSet->num_positive_pics = 0;
    }
}

mfxStatus H265Encoder::CheckCurRefPicSet(H265Slice *curr_slice)
{
    H265ShortTermRefPicSet* RefPicSet;
    H265Frame *Frm;
    Ipp32s CurrPicOrderCnt = m_pCurrentFrame->PicOrderCnt();
    Ipp32s MaxPicOrderCntLsb = (1 << m_sps.log2_max_pic_order_cnt_lsb);
    Ipp32s i, prev;

    if (m_pCurrentFrame->m_bIsIDRPic)
    {
        return MFX_ERR_NONE;
    }

    if (!curr_slice->short_term_ref_pic_set_sps_flag)
    {
        return MFX_ERR_UNKNOWN;
    }

    RefPicSet = m_ShortRefPicSet + curr_slice->short_term_ref_pic_set_idx;

    for (i = 0; i < (Ipp32s)curr_slice->num_long_term_pics; i++)
    {
        Ipp32s Poc = (CurrPicOrderCnt - curr_slice->poc_lsb_lt[i] + MaxPicOrderCntLsb) & (MaxPicOrderCntLsb - 1);

        if (curr_slice->delta_poc_msb_present_flag[i])
        {
            Poc -= curr_slice->delta_poc_msb_cycle_lt[i] * MaxPicOrderCntLsb;
        }

        Frm = m_dpb.findFrameByPOC(Poc);

        if (Frm == NULL)
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    prev = 0;
    for (i = 0; i < RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev - RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    prev = 0;
    for (i = RefPicSet->num_negative_pics; i < RefPicSet->num_positive_pics + RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev + RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::CheckRefPicSet(H265Slice *slice, H265Frame *frame)
{
    H265ShortTermRefPicSet* RefPicSet;
    H265Frame *Frm;
    Ipp32s CurrPicOrderCnt = frame->PicOrderCnt();
    Ipp32s MaxPicOrderCntLsb = (1 << m_sps.log2_max_pic_order_cnt_lsb);
    Ipp32s i, prev;

    if (frame->m_bIsIDRPic)
    {
        return MFX_ERR_NONE;
    }

    if (!slice->short_term_ref_pic_set_sps_flag)
    {
        return MFX_ERR_UNKNOWN;
    }

    RefPicSet = m_ShortRefPicSet + slice->short_term_ref_pic_set_idx;

    for (i = 0; i < (Ipp32s)slice->num_long_term_pics; i++)
    {
        Ipp32s Poc = (CurrPicOrderCnt - slice->poc_lsb_lt[i] + MaxPicOrderCntLsb) & (MaxPicOrderCntLsb - 1);

        if (slice->delta_poc_msb_present_flag[i])
        {
            Poc -= slice->delta_poc_msb_cycle_lt[i] * MaxPicOrderCntLsb;
        }

        Frm = m_dpb.findFrameByPOC(Poc);

        if (Frm == NULL)
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    prev = 0;
    for (i = 0; i < RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev - RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    prev = 0;
    for (i = RefPicSet->num_negative_pics; i < RefPicSet->num_positive_pics + RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev + RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::UpdateRefPicList(H265Slice *curr_slice)
{
    RefPicList *refPicList = m_pCurrentFrame->m_refPicList;
    H265ShortTermRefPicSet* RefPicSet;
    H265Frame *RefPicSetStCurrBefore[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicSetStCurrAfter[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicSetLtCurr[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicListTemp[MAX_NUM_REF_FRAMES];
    H265Frame *Frm;
    H265Frame **pRefPicList0 = refPicList[0].m_refFrames;
    H265Frame **pRefPicList1 = refPicList[1].m_refFrames;
    Ipp8s  *pTb0 = refPicList[0].m_deltaPoc;
    Ipp8s  *pTb1 = refPicList[1].m_deltaPoc;
    Ipp32s NumPocStCurrBefore, NumPocStCurrAfter, NumPocLtCurr;
    Ipp32s CurrPicOrderCnt = m_pCurrentFrame->PicOrderCnt();
    Ipp32s MaxPicOrderCntLsb = (1 << m_sps.log2_max_pic_order_cnt_lsb);
    EnumSliceType slice_type = curr_slice->slice_type;
    Ipp32s prev;
    Ipp32s i, j;

    VM_ASSERT(m_pCurrentFrame);

    if (!curr_slice->short_term_ref_pic_set_sps_flag)
    {
        RefPicSet = m_ShortRefPicSet + m_sps.num_short_term_ref_pic_sets;
    }
    else
    {
        RefPicSet = m_ShortRefPicSet + curr_slice->short_term_ref_pic_set_idx;
    }

    m_dpb.unMarkAll();

    NumPocLtCurr = 0;

    for (i = 0; i < (Ipp32s)curr_slice->num_long_term_pics; i++)
    {
        Ipp32s Poc = (CurrPicOrderCnt - curr_slice->poc_lsb_lt[i] + MaxPicOrderCntLsb) & (MaxPicOrderCntLsb - 1);

        if (curr_slice->delta_poc_msb_present_flag[i])
        {
            Poc -= curr_slice->delta_poc_msb_cycle_lt[i] * MaxPicOrderCntLsb;
        }

        Frm = m_dpb.findFrameByPOC(Poc);

        if (Frm == NULL)
        {
            if (!m_pCurrentFrame->m_bIsIDRPic)
            {
                VM_ASSERT(0);
            }
        }
        else
        {
            Frm->m_isMarked = true;
            Frm->unSetisShortTermRef();
            Frm->SetisLongTermRef();
        }

        if (curr_slice->used_by_curr_pic_lt_flag[i])
        {
            RefPicSetLtCurr[NumPocLtCurr] = Frm;
            NumPocLtCurr++;
        }
    }

    NumPocStCurrBefore = 0;
    NumPocStCurrAfter = 0;

    prev = 0;
    for (i = 0; i < RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev - RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            if (!m_pCurrentFrame->m_bIsIDRPic)
            {
                VM_ASSERT(0);
            }
        }
        else
        {
            Frm->m_isMarked = true;
            Frm->unSetisLongTermRef();
            Frm->SetisShortTermRef();
        }

        if (RefPicSet->used_by_curr_pic_flag[i])
        {
            RefPicSetStCurrBefore[NumPocStCurrBefore] = Frm;
            NumPocStCurrBefore++;
        }
    }

    prev = 0;
    for (i = RefPicSet->num_negative_pics; i < RefPicSet->num_positive_pics + RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev + RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            if (!m_pCurrentFrame->m_bIsIDRPic)
            {
                VM_ASSERT(0);
            }
        }
        else
        {
            Frm->m_isMarked = true;
            Frm->unSetisLongTermRef();
            Frm->SetisShortTermRef();
        }

        if (RefPicSet->used_by_curr_pic_flag[i])
        {
            RefPicSetStCurrAfter[NumPocStCurrAfter] = Frm;
            NumPocStCurrAfter++;
        }
    }

    m_dpb.removeAllUnmarkedRef();

    curr_slice->m_NumRefsInL0List = 0;
    curr_slice->m_NumRefsInL1List = 0;

    if (curr_slice->slice_type == I_SLICE)
    {
        curr_slice->num_ref_idx_l0_active = 0;
        curr_slice->num_ref_idx_l1_active = 0;
    }
    else
    {
        /* create a LIST0 */
        curr_slice->m_NumRefsInL0List = NumPocStCurrBefore + NumPocStCurrAfter + NumPocLtCurr;

        j = 0;

        for (i = 0; i < NumPocStCurrBefore; i++, j++)
        {
            RefPicListTemp[j] = RefPicSetStCurrBefore[i];
        }

        for (i = 0; i < NumPocStCurrAfter; i++, j++)
        {
            RefPicListTemp[j] = RefPicSetStCurrAfter[i];
        }

        for (i = 0; i < NumPocLtCurr; i++, j++)
        {
            RefPicListTemp[j] = RefPicSetLtCurr[i];
        }

        if (curr_slice->m_NumRefsInL0List > (Ipp32s)curr_slice->num_ref_idx_l0_active)
        {
            curr_slice->m_NumRefsInL0List = (Ipp32s)curr_slice->num_ref_idx_l0_active;
        }

        if (curr_slice->m_ref_pic_list_modification_flag_l0)
        {
            for (i = 0; i < curr_slice->m_NumRefsInL0List; i++)
            {
                pRefPicList0[i] = RefPicListTemp[curr_slice->list_entry_l0[i]];
            }
        }
        else
        {
            for (i = 0; i < curr_slice->m_NumRefsInL0List; i++)
            {
                pRefPicList0[i] = RefPicListTemp[i];
            }
        }

        if (curr_slice->slice_type == B_SLICE)
        {
            /* create a LIST1 */
            curr_slice->m_NumRefsInL1List = NumPocStCurrBefore + NumPocStCurrAfter + NumPocLtCurr;

            j = 0;

            for (i = 0; i < NumPocStCurrAfter; i++, j++)
            {
                RefPicListTemp[j] = RefPicSetStCurrAfter[i];
            }

            for (i = 0; i < NumPocStCurrBefore; i++, j++)
            {
                RefPicListTemp[j] = RefPicSetStCurrBefore[i];
            }

            for (i = 0; i < NumPocLtCurr; i++, j++)
            {
                RefPicListTemp[j] = RefPicSetLtCurr[i];
            }

            if (curr_slice->m_NumRefsInL1List > (Ipp32s)curr_slice->num_ref_idx_l1_active)
            {
                curr_slice->m_NumRefsInL1List = (Ipp32s)curr_slice->num_ref_idx_l1_active;
            }

            if (curr_slice->m_ref_pic_list_modification_flag_l1)
            {
                for (i = 0; i < curr_slice->m_NumRefsInL1List; i++)
                {
                    pRefPicList1[i] = RefPicListTemp[curr_slice->list_entry_l1[i]];
                }
            }
            else
            {
                for (i = 0; i < curr_slice->m_NumRefsInL1List; i++)
                {
                    pRefPicList1[i] = RefPicListTemp[i];
                }
            }

            /* create a LIST_C */
            /* TO DO */
        }

        for (i = 0; i < curr_slice->m_NumRefsInL0List; i++)
        {
            Ipp32s dPOC = CurrPicOrderCnt - pRefPicList0[i]->PicOrderCnt();

            if (dPOC < -128)     dPOC = -128;
            else if (dPOC > 127) dPOC =  127;

            pTb0[i] = (Ipp8s)dPOC;
        }

        for (i = 0; i < curr_slice->m_NumRefsInL1List; i++)
        {
            Ipp32s dPOC = CurrPicOrderCnt - pRefPicList1[i]->PicOrderCnt();

            if (dPOC < -128)     dPOC = -128;
            else if (dPOC > 127) dPOC =  127;

            pTb1[i] = (Ipp8s)dPOC;
        }

        curr_slice->num_ref_idx_l0_active = MAX(curr_slice->m_NumRefsInL0List, 1);
        curr_slice->num_ref_idx_l1_active = MAX(curr_slice->m_NumRefsInL1List, 1);

        if (curr_slice->slice_type == P_SLICE)
        {
            curr_slice->num_ref_idx_l1_active = 0;
            curr_slice->num_ref_idx_active_override_flag =
                (curr_slice->num_ref_idx_l0_active != m_pps.num_ref_idx_l0_default_active);
        }
        else
        {
            curr_slice->num_ref_idx_active_override_flag = (
                (curr_slice->num_ref_idx_l0_active != m_pps.num_ref_idx_l0_default_active) ||
                (curr_slice->num_ref_idx_l1_active != m_pps.num_ref_idx_l1_default_active));
        }

        /* RD tmp current version of HM decoder doesn't support m_num_ref_idx_active_override_flag == 0 yet*/
        curr_slice->num_ref_idx_active_override_flag = 1;

        // If default
        if ((P_SLICE == slice_type && m_pps.weighted_pred_flag == 0) ||
            (B_SLICE == slice_type && m_pps.weighted_bipred_flag == 0))
        {
            curr_slice->luma_log2_weight_denom = 0;
            curr_slice->chroma_log2_weight_denom = 0;
            for (Ipp8u i = 0; i < curr_slice->num_ref_idx_l0_active; i++) // refIdxL0
                for (Ipp8u j = 0; j < curr_slice->num_ref_idx_l1_active; j++) // refIdxL1
                    for (Ipp8u k = 0; k < 6; k++) // L0/L1 and planes
                    {
                        curr_slice->weights[k][i][j] = 1;
                        curr_slice->offsets[k][i][j] = 0;
                    }
        }
#if 0
        // If P/B slice and explicit
        if ((PREDSLICE == slice_type && m_PicParamSet.weighted_pred_flag) ||
            (BPREDSLICE == slice_type && m_PicParamSet.weighted_bipred_flag))
        {
            // load from explicit weight file
            m_SliceHeader.luma_log2_weight_denom = m_info.LumaLog2WeightDenom;
            m_SliceHeader.chroma_log2_weight_denom = m_info.ChromaLog2WeightDenom;
            for (Ipp8u i = 0; i < curr_slice->num_ref_idx_l0_active; i++) // refIdxL0
                for (Ipp8u j = 0; j < curr_slice->num_ref_idx_l1_active; j++) // refIdxL1
                {
                    curr_slice->Weights[0][i][j] = m_info.Weights[0][i];//1 << m_SliceHeader.luma_log2_weight_denom; // luma_weight_l0[i]
                    curr_slice->Offsets[0][i][j] = m_info.Offsets[0][i]*(1<<(m_info.BitDepthLuma-8)); // luma_offset_l0[i]*(1<<(BitDepthY-8))
                    curr_slice->Weights[1][i][j] = m_info.Weights[1][j];//1 << m_SliceHeader.luma_log2_weight_denom; // luma_weight_l1[j]
                    curr_slice->Offsets[1][i][j] = m_info.Offsets[1][j]*(1<<(m_info.BitDepthLuma-8)); // luma_offset_l1[j]*(1<<(BitDepthY-8))
                    curr_slice->Weights[2][i][j] = m_info.Weights[2][i];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l0[i][Cb]
                    curr_slice->Offsets[2][i][j] = m_info.Offsets[2][i]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l0[i][Cb]*(1<<(BitDepthC-8))
                    curr_slice->Weights[3][i][j] = m_info.Weights[3][j];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l1[j][Cb]
                    curr_slice->Offsets[3][i][j] = m_info.Offsets[3][j]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l1[j][Cb]*(1<<(BitDepthC-8))
                    curr_slice->Weights[4][i][j] = m_info.Weights[4][i];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l0[i][Cr]
                    curr_slice->Offsets[4][i][j] = m_info.Offsets[4][i]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l0[i][Cr]*(1<<(BitDepthC-8))
                    curr_slice->Weights[5][i][j] = m_info.Weights[5][j];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l1[j][Cr]
                    curr_slice->Offsets[5][i][j] = m_info.Offsets[5][j]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l1[j][Cr]*(1<<(BitDepthC-8))
                }
        }
#endif
        //update temporal refpiclists

        for (i = 0; i < (Ipp32s)curr_slice->num_ref_idx_l0_active; i++)
        {
            m_pCurrentFrame->m_refPicList[0].m_refFrames[i] = pRefPicList0[i];
            m_pCurrentFrame->m_refPicList[0].m_deltaPoc[i] = pTb0[i];
            m_pCurrentFrame->m_refPicList[0].m_isLongTermRef[i] = pRefPicList0[i]->isLongTermRef();
        }

        for (i = 0; i < (Ipp32s)curr_slice->num_ref_idx_l1_active; i++)
        {
            m_pCurrentFrame->m_refPicList[1].m_refFrames[i] = pRefPicList1[i];
            m_pCurrentFrame->m_refPicList[1].m_deltaPoc[i] = pTb1[i];
            m_pCurrentFrame->m_refPicList[1].m_isLongTermRef[i] = pRefPicList1[i]->isLongTermRef();
        }

        //SetPakRefPicList(curr_slice, ref_pic_list);
        //SetEncRefPicList(curr_slice, ref_pic_list);
    }

    return MFX_ERR_NONE;
}    // UpdateRefPicList

mfxStatus H265Encoder::UpdateRefPicList(H265Slice *slice, H265Frame *frame)
{
    RefPicList *refPicList = frame->m_refPicList;
    H265ShortTermRefPicSet* RefPicSet;
    H265Frame *RefPicSetStCurrBefore[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicSetStCurrAfter[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicSetLtCurr[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicListTemp[MAX_NUM_REF_FRAMES];
    H265Frame *Frm;
    H265Frame **pRefPicList0 = refPicList[0].m_refFrames;
    H265Frame **pRefPicList1 = refPicList[1].m_refFrames;
    Ipp8s  *pTb0 = refPicList[0].m_deltaPoc;
    Ipp8s  *pTb1 = refPicList[1].m_deltaPoc;
    Ipp32s NumPocStCurrBefore, NumPocStCurrAfter, NumPocLtCurr;
    Ipp32s CurrPicOrderCnt = frame->PicOrderCnt();
    Ipp32s MaxPicOrderCntLsb = (1 << m_sps.log2_max_pic_order_cnt_lsb);
    EnumSliceType slice_type = slice->slice_type;
    Ipp32s prev;
    Ipp32s i, j;

    VM_ASSERT(frame);

    if (!slice->short_term_ref_pic_set_sps_flag)
    {
        RefPicSet = m_ShortRefPicSet + m_sps.num_short_term_ref_pic_sets;
    }
    else
    {
        RefPicSet = m_ShortRefPicSet + slice->short_term_ref_pic_set_idx;
    }

    m_dpb.unMarkAll();

    NumPocLtCurr = 0;

    for (i = 0; i < (Ipp32s)slice->num_long_term_pics; i++)
    {
        Ipp32s Poc = (CurrPicOrderCnt - slice->poc_lsb_lt[i] + MaxPicOrderCntLsb) & (MaxPicOrderCntLsb - 1);

        if (slice->delta_poc_msb_present_flag[i])
        {
            Poc -= slice->delta_poc_msb_cycle_lt[i] * MaxPicOrderCntLsb;
        }

        Frm = m_dpb.findFrameByPOC(Poc);

        if (Frm == NULL)
        {
            if (!frame->m_bIsIDRPic)
            {
                VM_ASSERT(0);
            }
        }
        else
        {
            Frm->m_isMarked = true;
            Frm->unSetisShortTermRef();
            Frm->SetisLongTermRef();
        }

        if (slice->used_by_curr_pic_lt_flag[i])
        {
            RefPicSetLtCurr[NumPocLtCurr] = Frm;
            NumPocLtCurr++;
        }
    }

    NumPocStCurrBefore = 0;
    NumPocStCurrAfter = 0;

    prev = 0;
    for (i = 0; i < RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev - RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            if (!frame->m_bIsIDRPic)
            {
                VM_ASSERT(0);
            }
        }
        else
        {
            Frm->m_isMarked = true;
            Frm->unSetisLongTermRef();
            Frm->SetisShortTermRef();
        }

        if (RefPicSet->used_by_curr_pic_flag[i])
        {
            RefPicSetStCurrBefore[NumPocStCurrBefore] = Frm;
            NumPocStCurrBefore++;
        }
    }

    prev = 0;
    for (i = RefPicSet->num_negative_pics; i < RefPicSet->num_positive_pics + RefPicSet->num_negative_pics; i++)
    {
        Ipp32s DeltaPoc = prev + RefPicSet->delta_poc[i];

        prev = DeltaPoc;

        Frm = m_dpb.findFrameByPOC(CurrPicOrderCnt + DeltaPoc);

        if ((Frm == NULL) || (!(Frm->isShortTermRef())))
        {
            if (!frame->m_bIsIDRPic)
            {
                VM_ASSERT(0);
            }
        }
        else
        {
            Frm->m_isMarked = true;
            Frm->unSetisLongTermRef();
            Frm->SetisShortTermRef();
        }

        if (RefPicSet->used_by_curr_pic_flag[i])
        {
            RefPicSetStCurrAfter[NumPocStCurrAfter] = Frm;
            NumPocStCurrAfter++;
        }
    }

    m_dpb.removeAllUnmarkedRef();

    slice->m_NumRefsInL0List = 0;
    slice->m_NumRefsInL1List = 0;

    if (slice->slice_type == I_SLICE)
    {
        slice->num_ref_idx_l0_active = 0;
        slice->num_ref_idx_l1_active = 0;
    }
    else
    {
        /* create a LIST0 */
        slice->m_NumRefsInL0List = NumPocStCurrBefore + NumPocStCurrAfter + NumPocLtCurr;

        j = 0;

        for (i = 0; i < NumPocStCurrBefore; i++, j++)
        {
            RefPicListTemp[j] = RefPicSetStCurrBefore[i];
        }

        for (i = 0; i < NumPocStCurrAfter; i++, j++)
        {
            RefPicListTemp[j] = RefPicSetStCurrAfter[i];
        }

        for (i = 0; i < NumPocLtCurr; i++, j++)
        {
            RefPicListTemp[j] = RefPicSetLtCurr[i];
        }

        if (slice->m_NumRefsInL0List > (Ipp32s)slice->num_ref_idx_l0_active)
        {
            slice->m_NumRefsInL0List = (Ipp32s)slice->num_ref_idx_l0_active;
        }

        if (slice->m_ref_pic_list_modification_flag_l0)
        {
            for (i = 0; i < slice->m_NumRefsInL0List; i++)
            {
                pRefPicList0[i] = RefPicListTemp[slice->list_entry_l0[i]];
            }
        }
        else
        {
            for (i = 0; i < slice->m_NumRefsInL0List; i++)
            {
                pRefPicList0[i] = RefPicListTemp[i];
            }
        }

        if (slice->slice_type == B_SLICE)
        {
            /* create a LIST1 */
            slice->m_NumRefsInL1List = NumPocStCurrBefore + NumPocStCurrAfter + NumPocLtCurr;

            j = 0;

            for (i = 0; i < NumPocStCurrAfter; i++, j++)
            {
                RefPicListTemp[j] = RefPicSetStCurrAfter[i];
            }

            for (i = 0; i < NumPocStCurrBefore; i++, j++)
            {
                RefPicListTemp[j] = RefPicSetStCurrBefore[i];
            }

            for (i = 0; i < NumPocLtCurr; i++, j++)
            {
                RefPicListTemp[j] = RefPicSetLtCurr[i];
            }

            if (slice->m_NumRefsInL1List > (Ipp32s)slice->num_ref_idx_l1_active)
            {
                slice->m_NumRefsInL1List = (Ipp32s)slice->num_ref_idx_l1_active;
            }

            if (slice->m_ref_pic_list_modification_flag_l1)
            {
                for (i = 0; i < slice->m_NumRefsInL1List; i++)
                {
                    pRefPicList1[i] = RefPicListTemp[slice->list_entry_l1[i]];
                }
            }
            else
            {
                for (i = 0; i < slice->m_NumRefsInL1List; i++)
                {
                    pRefPicList1[i] = RefPicListTemp[i];
                }
            }

            /* create a LIST_C */
            /* TO DO */
        }

        for (i = 0; i < slice->m_NumRefsInL0List; i++)
        {
            Ipp32s dPOC = CurrPicOrderCnt - pRefPicList0[i]->PicOrderCnt();

            if (dPOC < -128)     dPOC = -128;
            else if (dPOC > 127) dPOC =  127;

            pTb0[i] = (Ipp8s)dPOC;
        }

        for (i = 0; i < slice->m_NumRefsInL1List; i++)
        {
            Ipp32s dPOC = CurrPicOrderCnt - pRefPicList1[i]->PicOrderCnt();

            if (dPOC < -128)     dPOC = -128;
            else if (dPOC > 127) dPOC =  127;

            pTb1[i] = (Ipp8s)dPOC;
        }

        slice->num_ref_idx_l0_active = MAX(slice->m_NumRefsInL0List, 1);
        slice->num_ref_idx_l1_active = MAX(slice->m_NumRefsInL1List, 1);

        if (slice->slice_type == P_SLICE)
        {
            slice->num_ref_idx_l1_active = 0;
            slice->num_ref_idx_active_override_flag =
                (slice->num_ref_idx_l0_active != m_pps.num_ref_idx_l0_default_active);
        }
        else
        {
            slice->num_ref_idx_active_override_flag = (
                (slice->num_ref_idx_l0_active != m_pps.num_ref_idx_l0_default_active) ||
                (slice->num_ref_idx_l1_active != m_pps.num_ref_idx_l1_default_active));
        }

        /* RD tmp current version of HM decoder doesn't support m_num_ref_idx_active_override_flag == 0 yet*/
        slice->num_ref_idx_active_override_flag = 1;

        // If default
        if ((P_SLICE == slice_type && m_pps.weighted_pred_flag == 0) ||
            (B_SLICE == slice_type && m_pps.weighted_bipred_flag == 0))
        {
            slice->luma_log2_weight_denom = 0;
            slice->chroma_log2_weight_denom = 0;
            for (Ipp8u i = 0; i < slice->num_ref_idx_l0_active; i++) // refIdxL0
                for (Ipp8u j = 0; j < slice->num_ref_idx_l1_active; j++) // refIdxL1
                    for (Ipp8u k = 0; k < 6; k++) // L0/L1 and planes
                    {
                        slice->weights[k][i][j] = 1;
                        slice->offsets[k][i][j] = 0;
                    }
        }
#if 0
        // If P/B slice and explicit
        if ((PREDSLICE == slice_type && m_PicParamSet.weighted_pred_flag) ||
            (BPREDSLICE == slice_type && m_PicParamSet.weighted_bipred_flag))
        {
            // load from explicit weight file
            m_SliceHeader.luma_log2_weight_denom = m_info.LumaLog2WeightDenom;
            m_SliceHeader.chroma_log2_weight_denom = m_info.ChromaLog2WeightDenom;
            for (Ipp8u i = 0; i < slice->num_ref_idx_l0_active; i++) // refIdxL0
                for (Ipp8u j = 0; j < slice->num_ref_idx_l1_active; j++) // refIdxL1
                {
                    slice->Weights[0][i][j] = m_info.Weights[0][i];//1 << m_SliceHeader.luma_log2_weight_denom; // luma_weight_l0[i]
                    slice->Offsets[0][i][j] = m_info.Offsets[0][i]*(1<<(m_info.BitDepthLuma-8)); // luma_offset_l0[i]*(1<<(BitDepthY-8))
                    slice->Weights[1][i][j] = m_info.Weights[1][j];//1 << m_SliceHeader.luma_log2_weight_denom; // luma_weight_l1[j]
                    slice->Offsets[1][i][j] = m_info.Offsets[1][j]*(1<<(m_info.BitDepthLuma-8)); // luma_offset_l1[j]*(1<<(BitDepthY-8))
                    slice->Weights[2][i][j] = m_info.Weights[2][i];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l0[i][Cb]
                    slice->Offsets[2][i][j] = m_info.Offsets[2][i]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l0[i][Cb]*(1<<(BitDepthC-8))
                    slice->Weights[3][i][j] = m_info.Weights[3][j];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l1[j][Cb]
                    slice->Offsets[3][i][j] = m_info.Offsets[3][j]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l1[j][Cb]*(1<<(BitDepthC-8))
                    slice->Weights[4][i][j] = m_info.Weights[4][i];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l0[i][Cr]
                    slice->Offsets[4][i][j] = m_info.Offsets[4][i]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l0[i][Cr]*(1<<(BitDepthC-8))
                    slice->Weights[5][i][j] = m_info.Weights[5][j];//1 << m_SliceHeader.chroma_log2_weight_denom; // chroma_weight_l1[j][Cr]
                    slice->Offsets[5][i][j] = m_info.Offsets[5][j]*(1<<(m_info.BitDepthChroma-8));//0; // chroma_offset_l1[j][Cr]*(1<<(BitDepthC-8))
                }
        }
#endif
        //update temporal refpiclists

        for (i = 0; i < (Ipp32s)slice->num_ref_idx_l0_active; i++)
        {
            frame->m_refPicList[0].m_refFrames[i] = pRefPicList0[i];
            frame->m_refPicList[0].m_deltaPoc[i] = pTb0[i];
            frame->m_refPicList[0].m_isLongTermRef[i] = pRefPicList0[i]->isLongTermRef();
        }

        for (i = 0; i < (Ipp32s)slice->num_ref_idx_l1_active; i++)
        {
            frame->m_refPicList[1].m_refFrames[i] = pRefPicList1[i];
            frame->m_refPicList[1].m_deltaPoc[i] = pTb1[i];
            frame->m_refPicList[1].m_isLongTermRef[i] = pRefPicList1[i]->isLongTermRef();
        }

        //SetPakRefPicList(slice, ref_pic_list);
        //SetEncRefPicList(slice, ref_pic_list);
    }

    return MFX_ERR_NONE;
}    // UpdateRefPicList

mfxStatus H265Encoder::DeblockThread(Ipp32s ithread)
{
    Ipp32u ctb_row;
    H265VideoParam *pars = &m_videoParam;

    while(1) {
        if (pars->num_threads > 1)
            ctb_row = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&m_incRow)) - 1;
        else
            ctb_row = m_incRow++;

        if (ctb_row >= pars->PicHeightInCtbs)
            break;

        Ipp32u ctb_addr = ctb_row * pars->PicWidthInCtbs;

        for (Ipp32u ctb_col = 0; ctb_col < pars->PicWidthInCtbs; ctb_col ++, ctb_addr++) {
            Ipp8u curr_slice = m_slice_ids[ctb_addr];

            cu[ithread].InitCu(&m_videoParam, m_pReconstructFrame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size, ctb_addr, m_pReconstructFrame->y,
                m_pReconstructFrame->uv, m_pReconstructFrame->pitch_luma, m_pCurrentFrame,
                &bsf[ithread], m_slices + curr_slice, 0, m_logMvCostTable);

            cu[ithread].Deblock();
        }
    }
    return MFX_ERR_NONE;
}


mfxStatus H265Encoder::ApplySAOThread(Ipp32s ithread)
{
    IppiSize roiSize;
    roiSize.width = m_videoParam.Width;
    roiSize.height = m_videoParam.Height;

    int numCTU = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;

    Ipp8u* pRec[2] = {m_pReconstructFrame->y, m_pReconstructFrame->uv};
    int pitch[3] = {m_pReconstructFrame->pitch_luma, m_pReconstructFrame->pitch_chroma, m_pReconstructFrame->pitch_chroma};
    int shift[3] = {0, 1, 1};

    SaoDecodeFilter saoFilter_New[NUM_USED_SAO_COMPONENTS];

    int compId = 0;
    for( compId = 0; compId < NUM_USED_SAO_COMPONENTS; compId++ )
    {
        saoFilter_New[compId].Init(m_videoParam.Width, m_videoParam.Height, m_videoParam.MaxCUSize, 0);

        // save boundaries
        {
            Ipp32s width = roiSize.width >> shift[compId];
            small_memcpy(saoFilter_New[compId].m_TmpU[0], pRec[compId], sizeof(Ipp8u) * width);
        }
    }

    // ----------------------------------------------------------------------------------------

    for(int ctu = 0; ctu < numCTU; ctu++)
    {
        for( compId = 0; compId < NUM_USED_SAO_COMPONENTS; compId++ )
        {
            // update #1
            int ctb_pelx           = ( ctu % m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;
            int ctb_pely           = ( ctu / m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;

            int offset = (ctb_pelx >> shift[compId]) + (ctb_pely >> shift[compId]) * pitch[compId];

            if ((ctu % m_videoParam.PicWidthInCtbs) == 0 && (ctu / m_videoParam.PicWidthInCtbs) != (m_videoParam.PicHeightInCtbs - 1))
            {
                PixType* pRecTmp = pRec[compId] + offset + ( (m_videoParam.MaxCUSize >> shift[compId]) - 1)*pitch[compId];
                small_memcpy(saoFilter_New[compId].m_TmpU[1], pRecTmp, sizeof(PixType) * (roiSize.width >> shift[compId]) );
            }

            if( (ctu % m_videoParam.PicWidthInCtbs) == 0 )
            {
                for (Ipp32u i = 0; i < (m_videoParam.MaxCUSize >> shift[compId])+1; i++)
                {
                    saoFilter_New[compId].m_TmpL[0][i] = pRec[compId][offset + i*pitch[compId]];
                }
            }

            if ((ctu % m_videoParam.PicWidthInCtbs) != (m_videoParam.PicWidthInCtbs - 1))
            {
                for (Ipp32u i = 0; i < (m_videoParam.MaxCUSize >> shift[compId])+1; i++)
                {
                    saoFilter_New[compId].m_TmpL[1][i] = pRec[compId][offset + i*pitch[compId] + (m_videoParam.MaxCUSize >> shift[compId])-1];
                }
            }

            /*MFX_HEVC_PP::CTBBorders borders = {0};
            borders.m_left     = (ctu % m_videoParam.PicWidthInCtbs != 0);
            borders.m_right    = (ctu % m_videoParam.PicWidthInCtbs != m_videoParam.PicWidthInCtbs-1);
            borders.m_top      = (ctu >= (int)m_videoParam.PicWidthInCtbs );
            borders.m_bottom   = (ctu <  numCTU - (int)m_videoParam.PicWidthInCtbs);
            borders.m_top_left = (borders.m_top && borders.m_left);
            borders.m_top_right = (borders.m_top && borders.m_right);
            borders.m_bottom_left = (borders.m_bottom && borders.m_left);
            borders.m_bottom_right = (borders.m_bottom && borders.m_right);*/


            if(m_saoParam[ctu][compId].mode_idx != SAO_MODE_OFF)
            {
                if( m_saoParam[ctu][compId].mode_idx == SAO_MODE_MERGE )
                {
                    // need to restore SAO parameters
                    //reconstructBlkSAOParam(m_saoParam[ctu], mergeList);
                }
                saoFilter_New[compId].SetOffsetsLuma(m_saoParam[ctu][compId], m_saoParam[ctu][compId].type_idx);

                MFX_HEVC_PP::NAME(h265_ProcessSaoCuOrg_Luma_8u)(
                    pRec[compId] + offset,
                    pitch[compId],
                    m_saoParam[ctu][compId].type_idx,
                    saoFilter_New[compId].m_TmpL[0],
                    &(saoFilter_New[compId].m_TmpU[0][ctb_pelx >> shift[compId]]),
                    m_videoParam.MaxCUSize >> shift[compId],
                    m_videoParam.MaxCUSize >> shift[compId],
                    roiSize.width >> shift[compId],
                    roiSize.height >> shift[compId],
                    saoFilter_New[compId].m_OffsetEo,
                    saoFilter_New[compId].m_OffsetBo,
                    saoFilter_New[compId].m_ClipTable,
                    ctb_pelx >> shift[compId],
                    ctb_pely >> shift[compId]/*,borders*/);
            }

            std::swap(saoFilter_New[compId].m_TmpL[0], saoFilter_New[compId].m_TmpL[1]);

            if (( (ctu+1) %  m_videoParam.PicWidthInCtbs) == 0)
            {
                std::swap(saoFilter_New[compId].m_TmpU[0], saoFilter_New[compId].m_TmpU[1]);
            }
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus H265Encoder::ApplySAOThread(Ipp32s ithread)


mfxStatus H265Encoder::ApplySAOThread_old(Ipp32s ithread)
{
    Ipp8u* tmpBufferY = new Ipp8u[m_videoParam.Height * m_pReconstructFrame->pitch_luma];
    mfxFrameData srcReconY;
    srcReconY.Pitch = (mfxU16)m_pReconstructFrame->pitch_luma;
    srcReconY.Y = tmpBufferY;

    // copy from recon to srcrecon
    IppiSize roiSize;
    roiSize.width = m_videoParam.Width;
    roiSize.height = m_videoParam.Height;

    ippiCopy_8u_C1R(
        m_pReconstructFrame->y,
        m_pReconstructFrame->pitch_luma,
        srcReconY.Y,
        srcReconY.Pitch,
        roiSize);

    mfxFrameData dstReconY;
    dstReconY.Y = m_pReconstructFrame->y;
    dstReconY.Pitch = (mfxU16)m_pReconstructFrame->pitch_luma;

    SaoEncodeFilter saoFilter;

    Ipp8u* p_srcStart = srcReconY.Y;
    Ipp8u* p_dstStart = dstReconY.Y;
    int numCTU = m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs;

    saoFilter.Init(m_videoParam.Width, m_videoParam.Height, m_videoParam.MaxCUSize, 0);
    for(int ctu = 0; ctu < numCTU; ctu++)
    {
        // update #1
        int ctb_pelx           = ( ctu % m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;
        int ctb_pely           = ( ctu / m_videoParam.PicWidthInCtbs ) * m_videoParam.MaxCUSize;
        // update offset
        srcReconY.Y = p_srcStart + ctb_pelx + ctb_pely * m_pReconstructFrame->pitch_luma;
        dstReconY.Y = p_dstStart + ctb_pelx + ctb_pely * m_pReconstructFrame->pitch_luma;

        saoFilter.ApplyCtuSao(&srcReconY, &dstReconY, m_saoParam[ctu], ctu);
    }

    saoFilter.Close();
    delete [] tmpBufferY;

    return MFX_ERR_NONE;

} // mfxStatus H265Encoder::ApplySAOThread_old(Ipp32s ithread)


mfxStatus H265Encoder::EncodeThread(Ipp32s ithread) {
    Ipp32u ctb_row = 0, ctb_col = 0, ctb_addr = 0;
    H265VideoParam *pars = &m_videoParam;
    Ipp8u nz[2];

    while(1) {
        mfxI32 found = 0, complete = 1;

        for (ctb_row = m_incRow; ctb_row < pars->PicHeightInCtbs; ctb_row++) {
            ctb_col = m_row_info[ctb_row].mt_current_ctb_col + 1;
            if (ctb_col == pars->PicWidthInCtbs)
                continue;
            complete = 0;
            if (m_row_info[ctb_row].mt_busy)
                continue;

            ctb_addr = ctb_row * pars->PicWidthInCtbs + ctb_col;

            if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag && ctb_row > 0) {
                    Ipp32s nsync = ctb_col < pars->PicWidthInCtbs - 1 ? 1 : 0;
                    if ((Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + nsync >= m_slices[m_slice_ids[ctb_addr]].slice_segment_address) {
                        if(m_row_info[ctb_row - 1].mt_current_ctb_col < (Ipp32s)ctb_col + nsync)
                            continue;
                    }
            }
            mfxI32 locked = 0;
            if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag)
                locked = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_busy)), 1, 0);

            if (locked) {
                continue;
            } if  ((Ipp32s)ctb_col != m_row_info[ctb_row].mt_current_ctb_col + 1) {
                m_row_info[ctb_row].mt_busy = 0;
                continue;
            } else {
                found = 1;
                break;
            }
        }

        if (complete)
            break;
        if (!found)
            continue;

        Ipp32s bs_id = 0;
        if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag)
            bs_id = ctb_row;

        {
            Ipp8u curr_slice = m_slice_ids[ctb_addr];
            Ipp8u end_of_slice_flag = (ctb_addr == m_slices[curr_slice].slice_address_last_ctb);

            if ((Ipp32s)ctb_addr == m_slices[curr_slice].slice_segment_address ||
                (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 0)) {

                ippiCABACInit_H265(&bs[ctb_row].cabacState,
                    bs[ctb_row].m_base.m_pbsBase,
                    bs[ctb_row].m_base.m_bitOffset + (Ipp32u)(bs[ctb_row].m_base.m_pbs - bs[ctb_row].m_base.m_pbsBase) * 8,
                    bs[ctb_row].m_base.m_maxBsSize);

                if (m_pps.entropy_coding_sync_enabled_flag && pars->PicWidthInCtbs > 1 &&
                    (Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + 1 >= m_slices[curr_slice].slice_segment_address) {
                    bs[ctb_row].CtxRestoreWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * (ctb_row - 1));
                } else {
                    InitializeContextVariablesHEVC_CABAC(bs[ctb_row].m_base.context_array, 2-m_slices[curr_slice].slice_type, pars->QP);
                }
            }

            small_memcpy(bsf[ctb_row].m_base.context_array, bs[ctb_row].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            bsf[ctb_row].Reset();

#ifdef DEBUG_CABAC
            printf("\n");
            if (ctb_addr == 0) printf("Start POC %d\n",m_pCurrentFrame->m_PicOrderCnt);
            printf("CTB %d\n",ctb_addr);
#endif
            cu[ithread].InitCu(pars, m_pReconstructFrame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size, ctb_addr, m_pReconstructFrame->y,
                m_pReconstructFrame->uv, m_pReconstructFrame->pitch_luma, m_pCurrentFrame,
                &bsf[ctb_row], m_slices + curr_slice, 1, m_logMvCostTable);

            cu[ithread].GetInitAvailablity();
            cu[ithread].ModeDecision(0, 0, 0, NULL);
            //cu[ithread].FillRandom(0, 0);
            //cu[ithread].FillZero(0, 0);

            if( pars->RDOQFlag )
            {
                small_memcpy(bsf[ctb_row].m_base.context_array, bs[ctb_row].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            }

            // inter pred for chroma is now performed in EncAndRecLuma
            cu[ithread].EncAndRecLuma(0, 0, 0, nz, NULL);
            cu[ithread].EncAndRecChroma(0, 0, 0, nz, NULL);

            if (!(m_slices[curr_slice].slice_deblocking_filter_disabled_flag) && (pars->num_threads == 1 || m_sps.sample_adaptive_offset_enabled_flag))
            {
#if defined(MFX_HEVC_SAO_PREDEBLOCKED_ENABLED)
                if(m_sps.sample_adaptive_offset_enabled_flag)
                {
                    Ipp32s left_addr  = cu[ithread].left_addr;
                    Ipp32s above_addr = cu[ithread].above_addr;
                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    cu[ithread].GetStatisticsCtuSaoPredeblocked( borders );
                }
#endif
                cu[ithread].Deblock();
                if(m_sps.sample_adaptive_offset_enabled_flag)
                {
                    Ipp32s left_addr = cu[ithread].m_leftAddr;
                    Ipp32s above_addr = cu[ithread].m_aboveAddr;

                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left  = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    cu[ithread].EstimateCtuSao(
                        &bsf[ctb_row],
                        &m_saoParam[ctb_addr],
                        &m_saoParam[0],
                        borders,
                        m_slice_ids);

                    // aya: tiles issues???
                    borders.m_left  = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : 0;
                    borders.m_top = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : 0;

                    bool leftMergeAvail = borders.m_left > 0 ? true : false;
                    bool aboveMergeAvail= borders.m_top > 0 ? true : false;

                    //if( cu[ithread].cslice->slice_sao_luma_flag )
                    {
                        cu[ithread].EncodeSao(&bs[ctb_row], 0, 0, 0, m_saoParam[ctb_addr], leftMergeAvail, aboveMergeAvail);
                    }

                    cu[ithread].m_saoEncodeFilter.ReconstructCtuSaoParam(m_saoParam[ctb_addr]);
                }
            }

            cu[ithread].EncodeCU(&bs[ctb_row], 0, 0, 0);

            if (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1) {
                bs[ctb_row].CtxSaveWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * ctb_row);
            }
            bs[ctb_row].EncodeSingleBin_CABAC(CTX(&bs[ctb_row],END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);

            if ((m_pps.entropy_coding_sync_enabled_flag && ctb_col == pars->PicWidthInCtbs - 1) ||
                end_of_slice_flag) {
#ifdef DEBUG_CABAC
                int d = DEBUG_CABAC_PRINT;
                DEBUG_CABAC_PRINT = 1;
#endif
                if (!end_of_slice_flag)
                    bs[ctb_row].EncodeSingleBin_CABAC(CTX(&bs[ctb_row],END_OF_SLICE_FLAG_HEVC), 1);
                bs[ctb_row].TerminateEncode_CABAC();
                bs[ctb_row].ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                DEBUG_CABAC_PRINT = d;
#endif
            }
            m_row_info[ctb_row].mt_current_ctb_col = ctb_col;
            if (ctb_row == m_incRow && ctb_col == pars->PicWidthInCtbs - 1)
                m_incRow++;
            m_row_info[ctb_row].mt_busy = 0;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::EncodeThreadByRow(Ipp32s ithread) {
    Ipp32u ctb_row;
    H265VideoParam *pars = &m_videoParam;
    Ipp8u nz[2];
    Ipp32s offset = 0;
    H265EncoderRowInfo *row_info = NULL;

    while(1) {
        if (pars->num_threads > 1)
            ctb_row = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&m_incRow)) - 1;
        else
            ctb_row = m_incRow++;

        if (ctb_row >= pars->PicHeightInCtbs)
            break;

        Ipp32u ctb_addr = ctb_row * pars->PicWidthInCtbs;

        for (Ipp32u ctb_col = 0; ctb_col < pars->PicWidthInCtbs; ctb_col ++, ctb_addr++)
        {
            Ipp8u curr_slice = m_slice_ids[ctb_addr];
            Ipp8u end_of_slice_flag = (ctb_addr == m_slices[curr_slice].slice_address_last_ctb);

            if (pars->num_threads > 1 && m_pps.entropy_coding_sync_enabled_flag && ctb_row > 0)
            {
                Ipp32s nsync = ctb_col < pars->PicWidthInCtbs - 1 ? 1 : 0;
                if ((Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + nsync >= m_slices[m_slice_ids[ctb_addr]].slice_segment_address)
                {
                    while(m_row_info[ctb_row - 1].mt_current_ctb_col < (Ipp32s)ctb_col + nsync)
                    {
                        x86_pause();
                        thread_sleep(0);
                    }
                }
            }

            if ((Ipp32s)ctb_addr == m_slices[curr_slice].slice_segment_address ||
                (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 0))
            {

                ippiCABACInit_H265(&bs[ithread].cabacState,
                    bs[ithread].m_base.m_pbsBase,
                    bs[ithread].m_base.m_bitOffset + (Ipp32u)(bs[ithread].m_base.m_pbs - bs[ithread].m_base.m_pbsBase) * 8,
                    bs[ithread].m_base.m_maxBsSize);

                if (m_pps.entropy_coding_sync_enabled_flag && pars->PicWidthInCtbs > 1 &&
                    (Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + 1 >= m_slices[curr_slice].slice_segment_address) {
                    bs[ithread].CtxRestoreWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * (ctb_row - 1));
                } else {
                    InitializeContextVariablesHEVC_CABAC(bs[ithread].m_base.context_array, 2-m_slices[curr_slice].slice_type, pars->QP);
                }

                if ((Ipp32s)ctb_addr == m_slices[curr_slice].slice_segment_address)
                    row_info = &(m_slices[curr_slice].m_row_info);
                else
                    row_info = m_row_info + ctb_row;
                row_info->offset = offset = H265Bs_GetBsSize(&bs[ithread]);
            }

            small_memcpy(bsf[ithread].m_base.context_array, bs[ithread].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            bsf[ithread].Reset();

#ifdef DEBUG_CABAC
            printf("\n");
            if (ctb_addr == 0) printf("Start POC %d\n",m_pCurrentFrame->m_PicOrderCnt);
            printf("CTB %d\n",ctb_addr);
#endif
            cu[ithread].InitCu(pars, m_pReconstructFrame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size, ctb_addr, m_pReconstructFrame->y,
                m_pReconstructFrame->uv, m_pReconstructFrame->pitch_luma, m_pCurrentFrame,
                &bsf[ithread], m_slices + curr_slice, 1, m_logMvCostTable);

            cu[ithread].GetInitAvailablity();
            cu[ithread].ModeDecision(0, 0, 0, NULL);
            //cu[ithread].FillRandom(0, 0);
            //cu[ithread].FillZero(0, 0);

            if( pars->RDOQFlag )
            {
                small_memcpy(bsf[ithread].m_base.context_array, bs[ithread].m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            }

            // inter pred for chroma is now performed in EncAndRecLuma
            cu[ithread].EncAndRecLuma(0, 0, 0, nz, NULL);
            cu[ithread].EncAndRecChroma(0, 0, 0, nz, NULL);

            if (!(m_slices[curr_slice].slice_deblocking_filter_disabled_flag) && (pars->num_threads == 1 || m_sps.sample_adaptive_offset_enabled_flag))
            {
#if defined(MFX_HEVC_SAO_PREDEBLOCKED_ENABLED)
                if(m_sps.sample_adaptive_offset_enabled_flag)
                {
                    Ipp32s left_addr  = cu[ithread].left_addr;
                    Ipp32s above_addr = cu[ithread].above_addr;
                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    cu[ithread].GetStatisticsCtuSaoPredeblocked( borders );
                }
#endif
                cu[ithread].Deblock();
                if(m_sps.sample_adaptive_offset_enabled_flag)
                {
                    Ipp32s left_addr  = cu[ithread].m_leftAddr;
                    Ipp32s above_addr = cu[ithread].m_aboveAddr;
                    MFX_HEVC_PP::CTBBorders borders = {0};

                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : m_pps.pps_loop_filter_across_slices_enabled_flag;

                    cu[ithread].EstimateCtuSao(
                        &bsf[ithread],
                        &m_saoParam[ctb_addr],
                        ctb_addr > 0 ? &m_saoParam[0] : NULL,
                        borders,
                        m_slice_ids);

                    // aya: tiles issues???
                    borders.m_left = (-1 == left_addr)  ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ left_addr ]) ? 1 : 0;
                    borders.m_top  = (-1 == above_addr) ? 0 : (m_slice_ids[ctb_addr] == m_slice_ids[ above_addr ]) ? 1 : 0;

                    bool leftMergeAvail = borders.m_left > 0 ? true : false;
                    bool aboveMergeAvail= borders.m_top > 0 ? true : false;

                    //if( cu[ithread].cslice->slice_sao_luma_flag )
                    {
                        cu[ithread].EncodeSao(&bs[ithread], 0, 0, 0, m_saoParam[ctb_addr], leftMergeAvail, aboveMergeAvail);
                    }

                    cu[ithread].m_saoEncodeFilter.ReconstructCtuSaoParam(m_saoParam[ctb_addr]);
                }
            }

            cu[ithread].EncodeCU(&bs[ithread], 0, 0, 0);

            if (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1) {
                bs[ithread].CtxSaveWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * ctb_row);
            }
            bs[ithread].EncodeSingleBin_CABAC(CTX(&bs[ithread],END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);

            if ((m_pps.entropy_coding_sync_enabled_flag && ctb_col == pars->PicWidthInCtbs - 1) ||
                end_of_slice_flag) {
#ifdef DEBUG_CABAC
                int d = DEBUG_CABAC_PRINT;
                DEBUG_CABAC_PRINT = 1;
#endif
                if (!end_of_slice_flag)
                    bs[ithread].EncodeSingleBin_CABAC(CTX(&bs[ithread],END_OF_SLICE_FLAG_HEVC), 1);
                bs[ithread].TerminateEncode_CABAC();
                bs[ithread].ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                DEBUG_CABAC_PRINT = d;
#endif
                {
                    row_info->bs_id = ithread;
                    row_info->size = H265Bs_GetBsSize(&bs[ithread]) - offset;
                }
            }
            m_row_info[ctb_row].mt_current_ctb_col = ctb_col;
//            vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&(m_row_info[ctb_row].mt_current_ctb_col)));
        }
    }
    return MFX_ERR_NONE;
}

static Ipp8u h265_pgop_qp_diff[PGOP_PIC_SIZE] = {0, 2, 1, 2};

void H265Encoder::PrepareToEndSequence()
{
    if (m_pLastFrame && m_pLastFrame->m_PicCodType == MFX_FRAMETYPE_B)
    {
        if (m_videoParam.BPyramid)
        {
            H265Frame *pCurr = m_cpb.head();
            Ipp8s tmbBuff[129];
            Ipp32s i;

            for (i = 0; i < 129; i++)
            {
                tmbBuff[i] = -1;
            }

            while (pCurr)
            {
                if (pCurr->m_PicCodType == MFX_FRAMETYPE_B)
                {
                    Ipp32u active_L1_refs;
                    m_dpb.countL1Refs(active_L1_refs, pCurr->PicOrderCnt());

                    if (active_L1_refs == 0)
                    {
                        tmbBuff[pCurr->m_BpyramidNumFrame] = 1;
                    }
                }
                pCurr = pCurr->future();
            }

            pCurr = m_cpb.head();

            while (pCurr)
            {
                if (pCurr->m_PicCodType == MFX_FRAMETYPE_B)
                {
                    Ipp32u active_L1_refs;
                    m_dpb.countL1Refs(active_L1_refs, pCurr->PicOrderCnt());

                    if (active_L1_refs == 0)
                    {
                        if (tmbBuff[m_BpyramidTabRight[pCurr->m_BpyramidNumFrame]] != 1)
                        {
                            pCurr->m_PicCodType = MFX_FRAMETYPE_P;
                        }
                    }
                }
                pCurr = pCurr->future();
            }
        }
    }
}

mfxStatus H265Encoder::EncodeFrame(mfxFrameSurface1 *surface, mfxBitstream *mfxBS) {

    EnumPicClass    ePic_Class;

    if (surface) {
        //Ipp32s RPSIndex = m_iProfileIndex;
        Ipp32u  ePictureType;

        ePictureType = DetermineFrameType();
        mfxU64 prevTimeStamp = m_pLastFrame ? m_pLastFrame->TimeStamp : MFX_TIMESTAMP_UNKNOWN;
        /*m_pLastFrame = */m_pCurrentFrame = m_cpb.InsertFrame(surface, &m_videoParam);
        if (m_pCurrentFrame)
        {
#ifdef MFX_ENABLE_WATERMARK
            m_watermark->Apply(m_pCurrentFrame->y, m_pCurrentFrame->uv, m_pCurrentFrame->pitch_luma,
                surface->Info.Width, surface->Info.Height);
#endif
            // Set PTS  from previous if isn't given at input
            if (m_pCurrentFrame->TimeStamp == MFX_TIMESTAMP_UNKNOWN) {
                if (prevTimeStamp == MFX_TIMESTAMP_UNKNOWN)
                    m_pCurrentFrame->TimeStamp = 0;
                else {
                    mfxF64 tcDuration90KHz = (mfxF64)m_videoParam.FrameRateExtD / m_videoParam.FrameRateExtN * 90000; // calculate tick duration
                    m_pCurrentFrame->TimeStamp = mfxI64(prevTimeStamp + tcDuration90KHz);
                }
            }

            m_pCurrentFrame->m_PicCodType = ePictureType;
            m_pCurrentFrame->m_bIsIDRPic = m_bMakeNextFrameIDR;
            m_pCurrentFrame->m_RPSIndex = m_iProfileIndex;

// making IDR picture every I frame for SEI HRD_CONF
            if (//((m_info.EnableSEI) && ePictureType == INTRAPIC) ||
               (/*(!m_info.EnableSEI) && */m_bMakeNextFrameIDR /*&& ePictureType == MFX_FRAMETYPE_I*/))

            {
                //m_pCurrentFrame->m_bIsIDRPic = true;
                m_PicOrderCnt_Accu += m_PicOrderCnt;
                m_PicOrderCnt = 0;
                //m_bMakeNextFrameIDR = false;
                m_PGOPIndex = 0;
            }

            m_pCurrentFrame->m_PGOPIndex = m_PGOPIndex;
            if (m_iProfileIndex == 0) {
                m_PGOPIndex++;
                if (m_PGOPIndex == m_videoParam.PGopPicSize)
                    m_PGOPIndex = 0;
            }

            m_pCurrentFrame->setPicOrderCnt(m_PicOrderCnt);
            m_pCurrentFrame->m_PicOrderCounterAccumulated = m_PicOrderCnt_Accu; //(m_PicOrderCnt + m_PicOrderCnt_Accu);

            m_pCurrentFrame->InitRefPicListResetCount();

            m_PicOrderCnt++;

            if (m_videoParam.BPyramid)
            {
                if (ePictureType == MFX_FRAMETYPE_I || ePictureType == MFX_FRAMETYPE_P)
                    m_Bpyramid_currentNumFrame = 0;

                m_pCurrentFrame->m_BpyramidNumFrame = m_BpyramidTab[m_Bpyramid_currentNumFrame];
//                m_pCurrentFrame->m_BpyramidRefLayers = m_BpyramidRefLayers[m_Bpyramid_currentNumFrame];
                m_pCurrentFrame->m_isBRef = ((m_Bpyramid_currentNumFrame & 1) == 0) ? 1 : 0;
                m_Bpyramid_currentNumFrame++;
                if (m_Bpyramid_currentNumFrame == m_Bpyramid_maxNumFrame)
                    m_Bpyramid_currentNumFrame = 0;

                m_l1_cnt_to_start_B = 1;
            }

            if (m_pCurrentFrame->m_bIsIDRPic)
            {
                m_cpb.IncreaseRefPicListResetCount(m_pCurrentFrame);
//                m_l1_cnt_to_start_B = 0;
                if (m_videoParam.BPyramid) {
                    PrepareToEndSequence();
                } else {
                    if (m_pLastFrame && !m_pLastFrame->wasEncoded() && m_pLastFrame->m_PicCodType == MFX_FRAMETYPE_B)
                        if (!m_videoParam.GopStrictFlag)
                            m_pLastFrame->m_PicCodType = MFX_FRAMETYPE_P;
                        else {
                            // Correct m_PicOrderCnt
                            // MoveTrailingBtoNextGOP();
                        }
                }
            }

            m_pLastFrame = m_pCurrentFrame;
        }
        else
        {
            m_pLastFrame = m_pCurrentFrame;
            return MFX_ERR_UNKNOWN;
        }
    } else {
        if (m_videoParam.BPyramid) {
            PrepareToEndSequence();
        } else {
            if (m_pLastFrame && !m_pLastFrame->wasEncoded() && m_pLastFrame->m_PicCodType == MFX_FRAMETYPE_B && !m_videoParam.GopStrictFlag)
                m_pLastFrame->m_PicCodType = MFX_FRAMETYPE_P;
        }
    }

    m_pCurrentFrame = m_cpb.findOldestToEncode(&m_dpb, m_l1_cnt_to_start_B,
                                               m_videoParam.BPyramid, m_Bpyramid_nextNumFrame);
    if (m_l1_cnt_to_start_B == 0 && m_pCurrentFrame && m_pCurrentFrame->m_PicCodType == MFX_FRAMETYPE_B && !m_videoParam.GopStrictFlag)
        m_pCurrentFrame->m_PicCodType = MFX_FRAMETYPE_P;

//    if (!m_videoParam.BPyramid)
    {
        // make sure no B pic is left unencoded before program exits
//            if (((m_info.NumFramesToEncode - m_info.NumEncodedFrames) < m_info.NumRefToStartCodeBSlice) &&
//                !m_cpb.isEmpty() && !m_pCurrentFrame && !src)
        if (!m_cpb.isEmpty() && !m_pCurrentFrame && !surface) {
            m_pCurrentFrame = m_cpb.findOldestToEncode(&m_dpb, 0, 0, 0/*m_isBpyramid, m_Bpyramid_nextNumFrame*/);
            if (!m_videoParam.BPyramid && m_pCurrentFrame && m_pCurrentFrame->m_PicCodType == MFX_FRAMETYPE_B && !m_videoParam.GopStrictFlag)
                m_pCurrentFrame->m_PicCodType = MFX_FRAMETYPE_P;
        }
    }

    if (m_videoParam.enableCmFlag)
        m_pCurrentFrame->SetisShortTermRef();   // is needed for RefList of m_pNextFrame

    if (!mfxBS)
        return m_pCurrentFrame ? MFX_ERR_NONE : MFX_ERR_MORE_DATA;


    Ipp32u ePictureType;

    if (m_pCurrentFrame)
    {
        ePictureType = m_pCurrentFrame->m_PicCodType;

        MoveFromCPBToDPB();

        if (m_videoParam.BPyramid)
        {
            m_Bpyramid_nextNumFrame = m_pCurrentFrame->m_BpyramidNumFrame + 1;
            if (m_Bpyramid_nextNumFrame == m_Bpyramid_maxNumFrame)
                m_Bpyramid_nextNumFrame = 0;
        }
    }
    else
    {
        return MFX_ERR_MORE_DATA;
    }


    ePictureType = m_pCurrentFrame->m_PicCodType;
    // Determine the Pic_Class.  Right now this depends on ePictureType, but that could change
    // to permit disposable P frames, for example.
    switch (ePictureType)
    {
    case MFX_FRAMETYPE_I:
        m_videoParam.QP = m_videoParam.QPI;
        m_videoParam.QPChroma = m_videoParam.QPIChroma;
        if (m_videoParam.GopPicSize == 1) {
             ePic_Class = DISPOSABLE_PIC;
        } else if (m_pCurrentFrame->m_bIsIDRPic) {
            if (m_frameCountEncoded) {
                m_dpb.unMarkAll();
                m_dpb.removeAllUnmarkedRef();
            }
            ePic_Class = IDR_PIC;
            m_l1_cnt_to_start_B = m_videoParam.NumRefToStartCodeBSlice;
        }
        else {
            ePic_Class = REFERENCE_PIC;
        }
        break;

    case MFX_FRAMETYPE_P:
         m_videoParam.QP = m_videoParam.QPP;
         if (m_videoParam.PGopPicSize > 1)
            m_videoParam.QP += h265_pgop_qp_diff[m_pCurrentFrame->m_PGOPIndex];
        m_videoParam.QPChroma = h265_QPtoChromaQP[m_videoParam.QP];
        ePic_Class = REFERENCE_PIC;
        break;

    case MFX_FRAMETYPE_B:
        m_videoParam.QP = m_videoParam.QPB;
        if (m_videoParam.BPyramid)
            m_videoParam.QP += m_BpyramidRefLayers[m_pCurrentFrame->m_PicOrderCnt % m_videoParam.GopRefDist];
        m_videoParam.QPChroma = h265_QPtoChromaQP[m_videoParam.QP];
        if (!m_videoParam.BPyramid)
        {
            ePic_Class = m_videoParam.TreatBAsReference ? REFERENCE_PIC : DISPOSABLE_PIC;
        }
        else
        {
            ePic_Class = (m_pCurrentFrame->m_isBRef == 1) ? REFERENCE_PIC : DISPOSABLE_PIC;
        }
        break;

    default:
        // Unsupported Picture Type
        VM_ASSERT(false);
        m_videoParam.QP = m_videoParam.QPI;
        m_videoParam.QPChroma = m_videoParam.QPIChroma;
        ePic_Class = IDR_PIC;
        break;
    }

    H265NALUnit nal;
    nal.nuh_layer_id = 0;
    nal.nuh_temporal_id = 0;

    mfxU32 initialDatalength = mfxBS->DataLength;
    Ipp32s overheadBytes = 0;
    Ipp32s bs_main_id = m_videoParam.num_thread_structs;

    if (m_frameCountEncoded == 0) {
        bs[bs_main_id].Reset();
        PutVPS(&bs[bs_main_id]);
        bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_VPS;
        overheadBytes += bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
    }

    if (m_pCurrentFrame->m_bIsIDRPic) {
        bs[bs_main_id].Reset();

        PutSPS(&bs[bs_main_id]);
        bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_SPS;
        bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);

        PutPPS(&bs[bs_main_id]);
        bs[bs_main_id].WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_PPS;
        overheadBytes += bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
    }

    Ipp8u *pbs0;
    Ipp32u bitOffset0;
    Ipp32s overheadBytes0 = overheadBytes;
    H265Bs_GetState(&bs[bs_main_id], &pbs0, &bitOffset0);
    Ipp32s min_qp = 1;
    Ipp32u dataLength0 = mfxBS->DataLength;
    Ipp32s brcRecode = 0;

recode:

    if (m_brc) {
        m_videoParam.QP = (Ipp8s)m_brc->GetQP((mfxU16)ePictureType);
        m_videoParam.QPChroma = h265_QPtoChromaQP[m_videoParam.QP];
    }

    for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
        H265Slice *slice = m_slices + curr_slice;
        SetSlice(slice, curr_slice);
        if (CheckCurRefPicSet(slice) != MFX_ERR_NONE)
            CreateRefPicSet(slice);

        UpdateRefPicList(slice);

        if (slice->slice_type != I_SLICE) {
            slice->num_ref_idx[0] = slice->num_ref_idx_l0_active;
            slice->num_ref_idx[1] = slice->num_ref_idx_l1_active;
        } else {
            slice->num_ref_idx[0] = 0;
            slice->num_ref_idx[1] = 0;
        }

        for(Ipp32u i = slice->slice_segment_address; i <= slice->slice_address_last_ctb; i++)
            m_slice_ids[i] = curr_slice;

        H265Frame **list0 = m_pCurrentFrame->m_refPicList[0].m_refFrames;
        H265Frame **list1 = m_pCurrentFrame->m_refPicList[1].m_refFrames;
        for (Ipp32s idx1 = 0; idx1 < slice->num_ref_idx[1]; idx1++) {
            Ipp32s idx0 = (Ipp32s)(std::find(list0, list0 + slice->num_ref_idx[0], list1[idx1]) - list0);
            m_pCurrentFrame->m_mapRefIdxL1ToL0[idx1] = (idx0 < slice->num_ref_idx[0]) ? idx0 : -1;

        }

        m_pCurrentFrame->m_allRefFramesAreFromThePast = true;
        for (Ipp32s i = 0; i < slice->num_ref_idx[0] && m_pCurrentFrame->m_allRefFramesAreFromThePast; i++)
            if (m_pCurrentFrame->m_refPicList[0].m_deltaPoc[i] < 0)
                m_pCurrentFrame->m_allRefFramesAreFromThePast = false;
        for (Ipp32s i = 0; i < slice->num_ref_idx[1] && m_pCurrentFrame->m_allRefFramesAreFromThePast; i++)
            if (m_pCurrentFrame->m_refPicList[1].m_deltaPoc[i] < 0)
                m_pCurrentFrame->m_allRefFramesAreFromThePast = false;
    }


    if (m_videoParam.enableCmFlag) {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CmPart");
        if (m_slices->num_ref_idx[0] > 0) {

            m_pNextFrame = m_cpb.findOldestToEncode(&m_dpb, m_l1_cnt_to_start_B,
                0, 0 /*m_isBpyramid, m_Bpyramid_nextNumFrame*/);

            if (m_pNextFrame)
            {
                memcpy(&m_ShortRefPicSetDump, &m_ShortRefPicSet, sizeof(m_ShortRefPicSet)); // !!!TODO: not all should be saved sergo!!!
                for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
                    H265Slice *pSlice = m_slicesNext + curr_slice;
                    SetSlice(pSlice, curr_slice, m_pNextFrame);
                    if (CheckRefPicSet(pSlice, m_pNextFrame) != MFX_ERR_NONE)
                    {
                        CreateRefPicSet(pSlice, m_pNextFrame);
                    }
                    UpdateRefPicList(pSlice, m_pNextFrame);
                    if (pSlice->slice_type != I_SLICE) {
                        pSlice->num_ref_idx[0] = pSlice->num_ref_idx_l0_active;
                        pSlice->num_ref_idx[1] = pSlice->num_ref_idx_l1_active;
                    } else {
                        pSlice->num_ref_idx[0] = 0;
                        pSlice->num_ref_idx[1] = 0;
                    }
                    for(Ipp32u i = m_slicesNext[curr_slice].slice_segment_address; i <= m_slicesNext[curr_slice].slice_address_last_ctb; i++) {
                        m_slice_ids[i] = curr_slice;
                    }
                }
                memcpy(&m_ShortRefPicSet, &m_ShortRefPicSetDump, sizeof(m_ShortRefPicSet));
            }

            cmCurIdx ^= 1;
            cmNextIdx ^= 1;
            H265Frame **refsCur = m_pCurrentFrame->m_refPicList[0].m_refFrames;
            H265Frame **refsNext = m_pNextFrame->m_refPicList[0].m_refFrames;
            RunVme(m_videoParam, m_pCurrentFrame, m_pNextFrame, m_slices, m_slicesNext, refsCur, refsNext);
        }

    }

    m_incRow = 0;
    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs; i++)
        bs[i].Reset();
    for (Ipp32u i = 0; i < m_videoParam.PicHeightInCtbs; i++) {
        m_row_info[i].mt_current_ctb_col = -1;
        m_row_info[i].mt_busy = 0;
    }

    if (m_videoParam.num_threads > 1)
        mfx_video_encode_h265_ptr->ParallelRegionStart(m_videoParam.num_threads - 1, PARALLEL_REGION_MAIN);

    if (m_videoParam.threading_by_rows)
        EncodeThreadByRow(0);
    else
        EncodeThread(0);

    if (m_videoParam.num_threads > 1)
        mfx_video_encode_h265_ptr->ParallelRegionEnd();

    if (!m_pps.pps_deblocking_filter_disabled_flag && (m_videoParam.num_threads > 1   && !m_sps.sample_adaptive_offset_enabled_flag) )
    {
        m_incRow = 0;
        mfx_video_encode_h265_ptr->ParallelRegionStart(m_videoParam.num_threads - 1, PARALLEL_REGION_DEBLOCKING);
        DeblockThread(0);
        mfx_video_encode_h265_ptr->ParallelRegionEnd();
    }
    //SAO
    if( m_sps.sample_adaptive_offset_enabled_flag ) // aya is it enough???
    {
        ApplySAOThread(0);
    }

    if (m_videoParam.threading_by_rows) {
        for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
            H265Slice *pSlice = m_slices + curr_slice;
            H265EncoderRowInfo *row_info;

            row_info = &pSlice->m_row_info;
            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u i = 0; i < pSlice->num_entry_point_offsets; i++) {
                    Ipp32u offset_add = 0;
                    Ipp8u *curPtr = bs[row_info->bs_id].m_base.m_pbsBase + row_info->offset;

                    for (Ipp32s j = 1; j < row_info->size - 1; j++) {
                        if (!curPtr[j - 1] && !curPtr[j] && !(curPtr[j + 1] & 0xfc)) {
                            j++;
                            offset_add ++;
                        }
                    }

                    pSlice->entry_point_offset[i] = row_info->size + offset_add;
                    row_info = m_row_info + pSlice->row_first + i + 1;
                }
                row_info = &pSlice->m_row_info;
            }
            bs[bs_main_id].Reset();
            PutSliceHeader(&bs[bs_main_id], pSlice);
            overheadBytes += H265Bs_GetBsSize(&bs[bs_main_id]);
            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u row = pSlice->row_first; row <= pSlice->row_last; row++) {
                    Ipp32s ithread = row_info->bs_id;
                    small_memcpy(bs[bs_main_id].m_base.m_pbs, bs[ithread].m_base.m_pbsBase + row_info->offset, row_info->size);
                    bs[bs_main_id].m_base.m_pbs += row_info->size;
                    row_info = m_row_info + row + 1;
                }
            } else {
                small_memcpy(bs[bs_main_id].m_base.m_pbs, bs[row_info->bs_id].m_base.m_pbsBase + row_info->offset, row_info->size);
                bs[bs_main_id].m_base.m_pbs += row_info->size;
            }
            nal.nal_unit_type = (Ipp8u)(pSlice->IdrPicFlag ? NAL_UT_CODED_SLICE_IDR :
                (m_pCurrentFrame->PicOrderCnt() >= 0 ? NAL_UT_CODED_SLICE_TRAIL_R :
                    (m_pCurrentFrame->m_isBRef ? NAL_UT_CODED_SLICE_DLP : NAL_UT_CODED_SLICE_RADL_N)));
            bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
        }
    } else {
        for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
            H265Slice *pSlice = m_slices + curr_slice;
            Ipp32s ctb_row = pSlice->row_first;

            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u i = 0; i < pSlice->num_entry_point_offsets; i++) {
                    Ipp32u offset_add = 0;
                    Ipp8u *curPtr = bs[ctb_row].m_base.m_pbsBase;
                    Ipp32s size = H265Bs_GetBsSize(&bs[ctb_row]);

                    for (Ipp32s j = 1; j < size - 1; j++) {
                        if (!curPtr[j - 1] && !curPtr[j] && !(curPtr[j + 1] & 0xfc)) {
                            j++;
                            offset_add ++;
                        }
                    }

                    pSlice->entry_point_offset[i] = size + offset_add;
                    ctb_row ++;
                }
            }
            bs[bs_main_id].Reset();
            PutSliceHeader(&bs[bs_main_id], pSlice);
            overheadBytes += H265Bs_GetBsSize(&bs[bs_main_id]);
            if (m_pps.entropy_coding_sync_enabled_flag) {
                for (Ipp32u row = pSlice->row_first; row <= pSlice->row_last; row++) {
                    Ipp32s size = H265Bs_GetBsSize(&bs[row]);
                    small_memcpy(bs[bs_main_id].m_base.m_pbs, bs[row].m_base.m_pbsBase, size);
                    bs[bs_main_id].m_base.m_pbs += size;
                }
            }
            nal.nal_unit_type = (Ipp8u)(pSlice->IdrPicFlag ? NAL_UT_CODED_SLICE_IDR :
                (m_pCurrentFrame->PicOrderCnt() >= 0 ? NAL_UT_CODED_SLICE_TRAIL_R :
                    (m_pCurrentFrame->m_isBRef ? NAL_UT_CODED_SLICE_DLP : NAL_UT_CODED_SLICE_RADL_N)));
            bs[bs_main_id].WriteNAL(mfxBS, 0, &nal);
        }
    }

    Ipp32s frameBytes = mfxBS->DataLength - initialDatalength;

    if (m_brc) {
        mfxBRCStatus brcSts;
        brcSts =  m_brc->PostPackFrame((mfxU16)m_pCurrentFrame->m_PicCodType, frameBytes << 3, overheadBytes << 3, brcRecode, m_pCurrentFrame->PicOrderCnt());
        brcRecode = 0;
        if (brcSts != MFX_BRC_OK) {
            if ((brcSts & MFX_BRC_ERR_SMALL_FRAME) && (m_videoParam.QP < min_qp))
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;
            if (!(brcSts & MFX_BRC_NOT_ENOUGH_BUFFER)) {
                mfxBS->DataLength = dataLength0;
                H265Bs_SetState(&bs[bs_main_id], pbs0, bitOffset0);
                overheadBytes = overheadBytes0;
                brcRecode = 1;
                goto recode;
            } else if (brcSts & MFX_BRC_ERR_SMALL_FRAME) {
                Ipp32s maxSize, minSize, bitsize = frameBytes << 3;
                Ipp8u *p = mfxBS->Data + mfxBS->DataOffset + mfxBS->DataLength;
                m_brc->GetMinMaxFrameSize(&minSize, &maxSize);
                if (minSize >  ((Ipp32s)mfxBS->MaxLength << 3))
                    return MFX_ERR_NOT_ENOUGH_BUFFER;
                while (bitsize < minSize - 32) {
                    *(Ipp32u*)p = 0;
                    p += 4;
                    bitsize += 32;
                }
                while (bitsize < minSize) {
                    *p = 0;
                    p++;
                    bitsize += 8;
                }
                m_brc->PostPackFrame((mfxU16)m_pCurrentFrame->m_PicCodType, bitsize, (overheadBytes << 3) + bitsize - (frameBytes << 3), 1, m_pCurrentFrame->PicOrderCnt());
                mfxBS->DataLength += (bitsize >> 3) - frameBytes;
            }  else
                return MFX_ERR_NOT_ENOUGH_BUFFER;


        }
    }

    switch (ePic_Class)
    {
    case IDR_PIC:
    case REFERENCE_PIC:
        if (m_videoParam.enableCmFlag == 0)
            m_pCurrentFrame->SetisShortTermRef();
        m_pReconstructFrame->doPadding();
        break;

    case DISPOSABLE_PIC:
    default:
        break;
    }

    m_pCurrentFrame->swapData(m_pReconstructFrame);
    m_pCurrentFrame->setWasEncoded();
    m_pCurrentFrame->Dump(m_recon_dump_file_name, &m_videoParam, &m_dpb, m_frameCountEncoded);

    CleanDPB();

    m_frameCountEncoded++;

    return MFX_ERR_NONE;
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
