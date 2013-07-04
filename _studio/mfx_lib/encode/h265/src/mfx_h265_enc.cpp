//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "ipp.h"

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

mfxStatus H265Encoder::InitH265VideoParam(mfxVideoH265InternalParam *param, mfxExtCodingOption *opts, mfxExtCodingOptionHEVC *opts_hevc)
{
    H265VideoParam *pars = &m_videoParam;
    Ipp32u width = param->mfx.FrameInfo.CropW ? param->mfx.FrameInfo.CropW : param->mfx.FrameInfo.Width;
    Ipp32u height = param->mfx.FrameInfo.CropH ? param->mfx.FrameInfo.CropH : param->mfx.FrameInfo.Height;

// preset
    pars->SourceWidth = width;
    pars->SourceHeight = height;
    pars->Log2MaxCUSize = opts_hevc->Log2MaxCUSize;// 6;
    pars->MaxCUDepth = opts_hevc->MaxCUDepth; // 4;
    pars->QuadtreeTULog2MaxSize = opts_hevc->QuadtreeTULog2MaxSize; // 5;
    pars->QuadtreeTULog2MinSize = opts_hevc->QuadtreeTULog2MinSize; // 2;
    pars->QuadtreeTUMaxDepthIntra = opts_hevc->QuadtreeTUMaxDepthIntra; // 4;
    pars->QuadtreeTUMaxDepthInter = opts_hevc->QuadtreeTUMaxDepthInter; // 4;
    pars->AMPFlag = 1;
    pars->TMVPFlag = 0;
    pars->QPI = (Ipp8s)param->mfx.QPI;
    pars->QPP = (Ipp8s)param->mfx.QPP;
    pars->QPB = (Ipp8s)param->mfx.QPB;
    pars->NumSlices = param->mfx.NumSlice;

    pars->GopPicSize = param->mfx.GopPicSize;
    pars->GopRefDist = param->mfx.GopRefDist;
    pars->IdrInterval = param->mfx.IdrInterval;

    if (!pars->GopRefDist || (pars->GopPicSize % pars->GopRefDist))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    pars->NumRefFrames = 6;
    pars->NumRefToStartCodeBSlice = 1;
    pars->TreatBAsReference = 0;
    pars->MaxRefIdxL0 = 1;
    pars->MaxRefIdxL1 = 1;
    pars->MaxBRefIdxL0 = 1;

    pars->AnalyseFlags = 0;
    if (opts_hevc->AnalyzeChroma == MFX_CODINGOPTION_ON)
        pars->AnalyseFlags |= HEVC_ANALYSE_CHROMA;

    pars->SplitThresholdStrengthCU = (Ipp8u)opts_hevc->SplitThresholdStrengthCU;
    pars->SplitThresholdStrengthTU = (Ipp8u)opts_hevc->SplitThresholdStrengthTU;

    pars->SBHFlag  = (Ipp8u)opts_hevc->SignBitHiding;
    pars->RDOQFlag = (Ipp8u)opts_hevc->RDOQuant;
    pars->WPPFlag  = (Ipp8u)opts_hevc->WPP;

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

    pars->QPIChroma = h265_QPtoChromaQP[pars->QPI];
    pars->QPPChroma = h265_QPtoChromaQP[pars->QPP];
    pars->QPBChroma = h265_QPtoChromaQP[pars->QPB];
    pars->QP = pars->QPI;
    pars->QPChroma = pars->QPChroma;

    pars->MaxTrSize = 1 << pars->QuadtreeTULog2MaxSize;
    pars->MaxCUSize = 1 << pars->Log2MaxCUSize;

    pars->AddCUDepth  = 0;
    while( (pars->MaxCUSize>>pars->MaxCUDepth) >
        ( 1u << ( pars->QuadtreeTULog2MinSize + pars->AddCUDepth )  ) ) pars->AddCUDepth++;

    pars->MaxCUDepth += pars->AddCUDepth;
    pars->AddCUDepth++;

    pars->MinCUSize = pars->MaxCUSize >> (pars->MaxCUDepth - pars->AddCUDepth);
    pars->MinTUSize = pars->MaxCUSize >> pars->MaxCUDepth;

    pars->CropRight = pars->CropBottom = 0;
    if (width % pars->MinCUSize)
    {
        pars->CropRight  = (width / pars->MinCUSize + 1) * pars->MinCUSize - width;
        width  += pars->CropRight;
    }
    if (height % pars->MinCUSize)
    {
        pars->CropBottom = (height / pars->MinCUSize + 1) * pars->MinCUSize - height;
        height += pars->CropBottom;
    }
    if ((pars->CropRight | pars->CropBottom) & 1)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    pars->Width = width;
    pars->Height = height;

    pars->Log2NumPartInCU = pars->MaxCUDepth << 1;
    pars->NumPartInCU = 1 << pars->Log2NumPartInCU;
    pars->NumPartInCUSize  = 1 << pars->MaxCUDepth;

    pars->PicWidthInMinCbs = pars->Width / pars->MinCUSize;
    pars->PicHeightInMinCbs = pars->Height / pars->MinCUSize;
    pars->PicWidthInCtbs = (pars->Width + pars->MaxCUSize - 1) / pars->MaxCUSize;
    pars->PicHeightInCtbs = (pars->Height + pars->MaxCUSize - 1) / pars->MaxCUSize;

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

    for (Ipp32s i = 0; i < pars->MaxTotalDepth; i++) {
        pars->cu_split_threshold_cu[i] = h265_calc_split_threshold(0, pars->Log2MaxCUSize - i,
            pars->SplitThresholdStrengthCU, pars->QPI);
        pars->cu_split_threshold_tu[i] = h265_calc_split_threshold(1, pars->Log2MaxCUSize - i,
            pars->SplitThresholdStrengthTU, pars->QPI);
    }

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetVPS()
{
    H265VidParameterSet *vps = &m_vps;

    memset(vps, 0, sizeof(H265VidParameterSet));
    vps->vps_max_layers = 1;
    vps->vps_max_sub_layers = 1;
    vps->vps_temporal_id_nesting_flag = 1;
    vps->vps_max_dec_pic_buffering[0] = (Ipp8u)(MAX(m_videoParam.MaxRefIdxL0,m_videoParam.MaxBRefIdxL0) +
        m_videoParam.MaxRefIdxL1);
    vps->vps_max_num_reorder_pics[0] = (Ipp8u)(m_videoParam.GopRefDist - 1);
    vps->vps_max_latency_increase[0] = 0;
    vps->vps_num_layer_sets = 1;

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetProfileLevel()
{
    memset(&m_profile_level, 0, sizeof(H265ProfileLevelSet));
    m_profile_level.general_profile_idc = MFX_PROFILE_HEVC_MAIN;
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

    if (pars->CropRight | pars->CropBottom) {
        sps->conformance_window_flag = 1;
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

    return MFX_ERR_NONE;
}

mfxStatus H265Encoder::SetPPS()
{
    H265SeqParameterSet *sps = &m_sps;
    H265PicParameterSet *pps = &m_pps;

    memset(pps, 0, sizeof(H265PicParameterSet));

    pps->pps_seq_parameter_set_id = sps->sps_seq_parameter_set_id;
    pps->deblocking_filter_control_present_flag = 1;
    pps->deblocking_filter_override_enabled_flag = 0;
    pps->pps_deblocking_filter_disabled_flag = 0;
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

mfxStatus H265Encoder::SetSlice(H265Slice *slice, Ipp32u curr_slice)
{
    memset(slice, 0, sizeof(H265Slice));

    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32u numSlices = m_videoParam.NumSlices;

    slice->slice_segment_address = numCtbs / numSlices * curr_slice;
    slice->slice_address_last_ctb = numCtbs / numSlices * (curr_slice + 1) - 1;
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

    if (m_frameCountEncoded == 0 || m_pCurrentFrame->m_bIsIDRPic)
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
    slice->slice_loop_filter_across_slices_enabled_flag = m_pps.loop_filter_across_tiles_enabled_flag;
//    slice->deblocking_filter_override_flag = 0;
    slice->slice_cb_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_cr_qp_offset = m_pps.pps_cb_qp_offset;

    slice->num_ref_idx_l0_active = m_pps.num_ref_idx_l0_default_active;
    slice->num_ref_idx_l1_active = m_pps.num_ref_idx_l1_default_active;

    slice->short_term_ref_pic_set_sps_flag = 1;
    slice->short_term_ref_pic_set_idx = m_pCurrentFrame->m_RPSIndex;

    slice->slice_num = curr_slice;
    slice->m_pRefPicList = GetRefPicLists(curr_slice);

    slice->slice_qp_delta = m_videoParam.QP - m_pps.init_qp;

    if (m_pps.entropy_coding_sync_enabled_flag) {
        Ipp32s first_row = slice->slice_segment_address / m_videoParam.PicWidthInCtbs;
        Ipp32s last_row = slice->slice_address_last_ctb / m_videoParam.PicWidthInCtbs;
        slice->num_entry_point_offsets = last_row - first_row;
    }

    return MFX_ERR_NONE;
}


void H265Encoder::InitShortTermRefPicSet()
{
    Ipp32s i, j;
    Ipp32s deltaPoc;

    deltaPoc = m_videoParam.TreatBAsReference ? 1 : m_videoParam.GopRefDist;

    m_numShortTermRefPicSets = 0;

    m_ShortRefPicSet[0].inter_ref_pic_set_prediction_flag = 0;
    m_ShortRefPicSet[0].num_negative_pics = m_videoParam.MaxRefIdxL0;
    m_ShortRefPicSet[0].num_positive_pics = 0;
    for (j = 0; j < m_ShortRefPicSet[0].num_negative_pics; j++) {
        m_ShortRefPicSet[0].delta_poc[j] = deltaPoc;
        m_ShortRefPicSet[0].used_by_curr_pic_flag[j] = 1;
    }
    m_numShortTermRefPicSets++;

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

    m_sps.num_short_term_ref_pic_sets = (Ipp8u)m_numShortTermRefPicSets;
}

mfxStatus H265Encoder::Init(mfxVideoH265InternalParam *param, mfxExtCodingOption *opts, mfxExtCodingOptionHEVC *opts_hevc) {
    mfxStatus sts = InitH265VideoParam(param, opts, opts_hevc);
    MFX_CHECK_STS(sts);

    m_bMakeNextFrameKey = true; // Ensure that we always start with a key frame.
    m_bMakeNextFrameIDR = false;
    m_uIntraFrameInterval = param->mfx.GopPicSize + 1;
    m_uIDRFrameInterval = param->mfx.IdrInterval + 2;
    m_isBpyramid = 0;
    m_l1_cnt_to_start_B = 0;
    m_PicOrderCnt = 0;
    m_PicOrderCnt_Accu = 0;
    m_frameCountEncoded = 0;

    if (param->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        m_brc = new H265BRC();
        mfxStatus sts = m_brc->Init(param);
        if (MFX_ERR_NONE != sts)
            return sts;
    }

    SetProfileLevel();
    SetVPS();
    SetSPS();
    SetPPS();

    m_videoParam.csps = &m_sps;
    m_videoParam.cpps = &m_pps;

    m_slices = new H265Slice[m_videoParam.NumSlices];
    MFX_CHECK_STS_ALLOC(m_slices);

    m_pReconstructFrame = new H265Frame();
    if (m_pReconstructFrame->Create(&m_videoParam) != MFX_ERR_NONE )
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_pRefPicList = (EncoderRefPicList*)H265_Malloc(sizeof(EncoderRefPicList) * m_videoParam.NumSlices);

    data_temp = (H265CUData*)H265_Malloc(sizeof(H265CUData) * (MAX_TOTAL_DEPTH << (m_videoParam.Log2NumPartInCU + 1)));
    MFX_CHECK_STS_ALLOC(data_temp);

    //aya
    ippsZero_8u( (Ipp8u*)data_temp, sizeof(H265CUData) * (MAX_TOTAL_DEPTH << (m_videoParam.Log2NumPartInCU + 1)));

    m_slice_ids = (Ipp8u *)H265_Malloc(m_videoParam.PicWidthInCtbs * m_videoParam.PicHeightInCtbs);
    MFX_CHECK_STS_ALLOC(m_slice_ids);

    profile_frequency = m_videoParam.GopRefDist;
    m_iProfileIndex = 0;
    eFrameType = new Ipp32u[profile_frequency];
    eFrameType[0] = MFX_FRAMETYPE_P;
    for (Ipp32s i = 1; i < profile_frequency; i++)
    {
        eFrameType[i] = MFX_FRAMETYPE_B;
    }

    m_videoParam.m_slice_ids = m_slice_ids;
    m_videoParam.m_pRefPicList = m_pRefPicList;

    return sts;
}

void H265Encoder::Close() {
    if (m_slices)
        delete[] m_slices;
    if (data_temp)
        H265_Free(data_temp);
    if (m_slice_ids)
        H265_Free(m_slice_ids);
    if (m_pRefPicList)
        H265_Free(m_pRefPicList);
    if (m_pReconstructFrame) {
        m_pReconstructFrame->Destroy();
        delete m_pReconstructFrame;
    }
    if (eFrameType) {
        delete[] eFrameType;
    }
    if (m_brc)
        delete m_brc;
}

Ipp32u H265Encoder::DetermineFrameType()
{
    Ipp32u ePictureType = eFrameType[m_iProfileIndex];

    if (m_frameCountEncoded == 0)
    {
        m_iProfileIndex++;
        if (m_iProfileIndex == profile_frequency)
            m_iProfileIndex = 0;

        m_bMakeNextFrameKey = false;
        m_bMakeNextFrameIDR = true;
        return MFX_FRAMETYPE_I;
    }

    m_uIntraFrameInterval--;
    if (1 == m_uIntraFrameInterval)
    {
        m_bMakeNextFrameKey = true;
    }

    // change to an I frame
    if (m_bMakeNextFrameKey)
    {
        ePictureType = MFX_FRAMETYPE_I;
        m_uIntraFrameInterval = m_videoParam.GopPicSize + 1;
        m_iProfileIndex = 0;
        m_bMakeNextFrameKey = false;

        m_uIDRFrameInterval--;

        if (1 == m_uIDRFrameInterval)
        {
            m_bMakeNextFrameIDR = true;
            m_uIDRFrameInterval = m_videoParam.IdrInterval + 2;
        }
    }

    m_iProfileIndex++;
    if (m_iProfileIndex == profile_frequency)
        m_iProfileIndex = 0;

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
    EncoderRefPicList *ref_pic_list = GetRefPicLists(curr_slice->slice_num);
    Ipp32s CurrPicOrderCnt = m_pCurrentFrame->PicOrderCnt();
    H265Frame **pRefPicList = ref_pic_list->m_RefPicListL0.m_RefPicList;
    H265ShortTermRefPicSet* RefPicSet;
    Ipp32s ExcludedPOC = -1;
    Ipp32u NumShortTermL0, NumShortTermL1, NumLongTermL0, NumLongTermL1;

    m_dpb.countActiveRefs(NumShortTermL0, NumLongTermL0, NumShortTermL1, NumLongTermL1, CurrPicOrderCnt);

    if ((NumShortTermL0 + NumShortTermL1 + NumLongTermL0 + NumLongTermL1) >= (Ipp32u)m_sps.num_ref_frames)
    {
        if ((NumShortTermL0 + NumShortTermL1) > 0)
        {
            H265Frame *Frm = m_dpb.findMostDistantShortTermRefs(CurrPicOrderCnt);
            ExcludedPOC = Frm->PicOrderCnt();

            if (ExcludedPOC < CurrPicOrderCnt)
                NumShortTermL0--;
            else
                NumShortTermL1--;
        }
        else
        {
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
    if (NumShortTermL0 > 0)
    {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s i, prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            Ipp32s poc = pFrm->PicOrderCnt();
            Ipp32s j, k;

            if (pFrm->isShortTermRef() && (poc < CurrPicOrderCnt) && (poc != ExcludedPOC))
            {
                // find insertion point
                j = 0;
                while ((j < NumFramesInList) && (pRefPicList[j]->PicOrderCnt() > poc))
                    j++;

                // make room if needed
                if (pRefPicList[j])
                {
                    for (k = NumFramesInList; k > j; k--)
                    {
                        pRefPicList[k] = pRefPicList[k-1];
                    }
                }

                // add the short-term reference
                pRefPicList[j] = pFrm;
                NumFramesInList++;
            }
        }

        RefPicSet->num_negative_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (i = 0; i < RefPicSet->num_negative_pics; i++)
        {
            Ipp32s DeltaPoc = pRefPicList[i]->PicOrderCnt() - CurrPicOrderCnt;

            RefPicSet->delta_poc[i] = prev - DeltaPoc;
            prev = DeltaPoc;
        }

        if (curr_slice->slice_type == I_SLICE)
        {
            numRefs = 0;
        }
        else if (curr_slice->slice_type == P_SLICE)
        {
            numRefs = m_videoParam.MaxRefIdxL0 + 1;
        }
        else
        {
            numRefs = m_videoParam.MaxBRefIdxL0 + 1;
        }

        if (numRefs > RefPicSet->num_negative_pics)
        {
            numRefs = RefPicSet->num_negative_pics;
        }

        for (i = 0; i < numRefs; i++)
        {
            RefPicSet->used_by_curr_pic_flag[i] = 1;
        }

        for (i = numRefs; i < RefPicSet->num_negative_pics; i++)
        {
            RefPicSet->used_by_curr_pic_flag[i] = 0;
        }
    }
    else
    {
        RefPicSet->num_negative_pics = 0;
    }

    /* Short Term L1 */
    if (NumShortTermL1 > 0)
    {
        H265Frame *pHead = m_dpb.head();
        H265Frame *pFrm;
        Ipp32s NumFramesInList = 0;
        Ipp32s i, prev, numRefs;

        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            Ipp32s poc = pFrm->PicOrderCnt();
            Ipp32s j, k;

            if (pFrm->isShortTermRef()&& (poc > CurrPicOrderCnt) && (poc != ExcludedPOC))
            {
                // find insertion point
                j = 0;
                while ((j < NumFramesInList) && (pRefPicList[j]->PicOrderCnt() < poc))
                    j++;

                // make room if needed
                if (pRefPicList[j])
                {
                    for (k = NumFramesInList; k > j; k--)
                    {
                        pRefPicList[k] = pRefPicList[k-1];
                    }
                }

                // add the short-term reference
                pRefPicList[j] = pFrm;
                NumFramesInList++;
            }
        }

        RefPicSet->num_positive_pics = (Ipp8u)NumFramesInList;

        prev = 0;
        for (i = 0; i < RefPicSet->num_positive_pics; i++)
        {
            Ipp32s DeltaPoc = pRefPicList[i]->PicOrderCnt() - CurrPicOrderCnt;

            RefPicSet->delta_poc[RefPicSet->num_negative_pics + i] = DeltaPoc - prev;
            prev = DeltaPoc;
        }

        if (curr_slice->slice_type == B_SLICE)
        {
            numRefs = m_videoParam.MaxRefIdxL1 + 1;
        }
        else
        {
            numRefs = 0;
        }

        if (numRefs > RefPicSet->num_positive_pics)
        {
            numRefs = RefPicSet->num_positive_pics;
        }

        for (i = 0; i < numRefs; i++)
        {
            RefPicSet->used_by_curr_pic_flag[RefPicSet->num_negative_pics + i] = 1;
        }

        for (i = numRefs; i < RefPicSet->num_positive_pics; i++)
        {
            RefPicSet->used_by_curr_pic_flag[RefPicSet->num_negative_pics + i] = 0;
        }
    }
    else
    {
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

mfxStatus H265Encoder::UpdateRefPicList(H265Slice *curr_slice)
{
    EncoderRefPicList *ref_pic_list = GetRefPicLists(curr_slice->slice_num);
    H265ShortTermRefPicSet* RefPicSet;
    H265Frame *RefPicSetStCurrBefore[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicSetStCurrAfter[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicSetLtCurr[MAX_NUM_REF_FRAMES];
    H265Frame *RefPicListTemp[MAX_NUM_REF_FRAMES];
    H265Frame *Frm;
    H265Frame **pRefPicList0 = ref_pic_list->m_RefPicListL0.m_RefPicList;
    H265Frame **pRefPicList1 = ref_pic_list->m_RefPicListL1.m_RefPicList;
    Ipp8s  *pTb0 = ref_pic_list->m_RefPicListL0.m_Tb;
    Ipp8s  *pTb1 = ref_pic_list->m_RefPicListL1.m_Tb;
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
            curr_slice->m_pRefPicList->m_RefPicListL0.m_RefPicList[i] = pRefPicList0[i];
            curr_slice->m_pRefPicList->m_RefPicListL0.m_Tb[i] = pTb0[i];
            curr_slice->m_pRefPicList->m_RefPicListL0.m_IsLongTermRef[i] = pRefPicList0[i]->isLongTermRef();
        }

        for (i = 0; i < (Ipp32s)curr_slice->num_ref_idx_l1_active; i++)
        {
            curr_slice->m_pRefPicList->m_RefPicListL1.m_RefPicList[i] = pRefPicList1[i];
            curr_slice->m_pRefPicList->m_RefPicListL1.m_Tb[i] = pTb1[i];
            curr_slice->m_pRefPicList->m_RefPicListL1.m_IsLongTermRef[i] = pRefPicList1[i]->isLongTermRef();
        }

//        SetPakRefPicList(curr_slice, ref_pic_list);
//        SetEncRefPicList(curr_slice, ref_pic_list);
    }

    return MFX_ERR_NONE;
}    // UpdateRefPicList

mfxStatus H265Encoder::EncodeFrame(mfxFrameSurface1 *surface, mfxBitstream *mfxBS) {
    EnumPicClass    ePic_Class;

    if (surface) {
        Ipp32s RPSIndex = m_iProfileIndex;
        Ipp32u  ePictureType;

        ePictureType = DetermineFrameType();
        m_pLastFrame = m_pCurrentFrame = m_cpb.InsertFrame(surface, &m_videoParam);
        if (m_pCurrentFrame)
        {
            m_pCurrentFrame->m_PicCodType = ePictureType;
            m_pCurrentFrame->m_bIsIDRPic = false;
            m_pCurrentFrame->m_RPSIndex = RPSIndex;

// making IDR picture every I frame for SEI HRD_CONF
            if (//((m_info.EnableSEI) && ePictureType == INTRAPIC) ||
               (/*(!m_info.EnableSEI) && */m_bMakeNextFrameIDR && ePictureType == MFX_FRAMETYPE_I))

            {
                m_pCurrentFrame->m_bIsIDRPic = true;
                m_PicOrderCnt_Accu += m_PicOrderCnt;
                m_PicOrderCnt = 0;
                m_bMakeNextFrameIDR = false;
            }

            m_pCurrentFrame->setPicOrderCnt(m_PicOrderCnt);
            m_pCurrentFrame->m_PicOrderCounterAccumulated = (m_PicOrderCnt + m_PicOrderCnt_Accu);

            m_pCurrentFrame->InitRefPicListResetCount();

            m_PicOrderCnt++;
/*
            if (m_isBpyramid)
            {
                if (ePictureType == INTRAPIC)
                    m_Bpyramid_currentNumFrame = 0;

                m_pCurrentFrame->m_BpyramidNumFrame = m_BpyramidTab[m_Bpyramid_currentNumFrame];
                m_pCurrentFrame->m_BpyramidRefLayers = m_BpyramidRefLayers[m_Bpyramid_currentNumFrame];
                m_pCurrentFrame->m_isBRef = ((m_Bpyramid_currentNumFrame & 1) == 0) ? 1 : 0;
                m_Bpyramid_currentNumFrame++;
                if (m_Bpyramid_currentNumFrame == m_Bpyramid_maxNumFrame)
                    m_Bpyramid_currentNumFrame = 0;

                m_l1_cnt_to_start_B = 1;
            }
*/
            if (m_pCurrentFrame->m_bIsIDRPic)
            {
                m_cpb.IncreaseRefPicListResetCount(m_pCurrentFrame);
            }

            if (m_pCurrentFrame->m_bIsIDRPic)
            {
//                PrepareToEndSequence();
            }

            m_pLastFrame = m_pCurrentFrame;
        }
        else
        {
            m_pLastFrame = m_pCurrentFrame;
            return MFX_ERR_UNKNOWN;
        }
    }

    m_pCurrentFrame = m_cpb.findOldestToEncode(&m_dpb, m_l1_cnt_to_start_B,
                                               0, 0 /*m_isBpyramid, m_Bpyramid_nextNumFrame*/);

//        if (!m_isBpyramid)
    {
        // make sure no B pic is left unencoded before program exits
//            if (((m_info.NumFramesToEncode - m_info.NumEncodedFrames) < m_info.NumRefToStartCodeBSlice) &&
//                !m_cpb.isEmpty() && !m_pCurrentFrame && !src)
        if (!m_cpb.isEmpty() && !m_pCurrentFrame && !surface) {
            m_pCurrentFrame = m_cpb.findOldestToEncode(&m_dpb, 0, 0, 0/*m_isBpyramid, m_Bpyramid_nextNumFrame*/);
            if (m_pCurrentFrame && m_pCurrentFrame->m_PicCodType == MFX_FRAMETYPE_B)
                m_pCurrentFrame->m_PicCodType = MFX_FRAMETYPE_P;
        }
    }

    Ipp32u ePictureType;

    if (m_pCurrentFrame)
    {
        ePictureType = m_pCurrentFrame->m_PicCodType;

        MoveFromCPBToDPB();

/*        if (m_isBpyramid)
        {
            m_Bpyramid_nextNumFrame = m_pCurrentFrame->m_BpyramidNumFrame + 1;
            if (m_Bpyramid_nextNumFrame == m_Bpyramid_maxNumFrame)
                m_Bpyramid_nextNumFrame = 0;
        }*/
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
        if (m_pCurrentFrame->m_bIsIDRPic || m_frameCountEncoded == 0)
        {
            // Right now, only the first INTRAPIC in a sequence is an IDR Frame
            ePic_Class = IDR_PIC;
            m_l1_cnt_to_start_B = m_videoParam.NumRefToStartCodeBSlice;
        }
        else
        {
            ePic_Class = REFERENCE_PIC;
        }
        break;

    case MFX_FRAMETYPE_P:
        m_videoParam.QP = m_videoParam.QPP;
        m_videoParam.QPChroma = m_videoParam.QPPChroma;
        ePic_Class = REFERENCE_PIC;
        break;

    case MFX_FRAMETYPE_B:
        m_videoParam.QP = m_videoParam.QPB;
        m_videoParam.QPChroma = m_videoParam.QPBChroma;
        if (!m_isBpyramid)
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

    if (m_brc) {
        m_videoParam.QP = (Ipp8s)m_brc->GetQP((mfxU16)ePictureType);
        m_videoParam.QPChroma = h265_QPtoChromaQP[m_videoParam.QP];
    }

    Ipp8u nz[2];
    H265NALUnit nal;
    nal.nuh_layer_id = 0;
    nal.nuh_temporal_id = 0;

    mfxU32 initialDatalength = mfxBS->DataLength;
    Ipp32s overheadBytes = 0;

    if (m_frameCountEncoded == 0) {
        bs->Reset();
        PutVPS();
        bs->WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_VPS;
        overheadBytes += bs->WriteNAL(mfxBS, 0, &nal);
    }

    if (m_pCurrentFrame->m_bIsIDRPic) {
        bs->Reset();

        PutSPS();
        bs->WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_SPS;
        bs->WriteNAL(mfxBS, 0, &nal);

        PutPPS();
        bs->WriteTrailingBits();
        nal.nal_unit_type = NAL_UT_PPS;
        overheadBytes += bs->WriteNAL(mfxBS, 0, &nal);
    }

    Ipp32s frameBytes = overheadBytes;

    for (Ipp8u curr_slice = 0; curr_slice < m_videoParam.NumSlices; curr_slice++) {
        m_videoParam.cslice = m_slices + curr_slice;
        bs->Reset();
        SetSlice(m_videoParam.cslice, curr_slice);

        if (m_videoParam.cslice->slice_type != I_SLICE) {
            if (CheckCurRefPicSet(m_videoParam.cslice) != MFX_ERR_NONE)
            {
                CreateRefPicSet(m_videoParam.cslice);
            }
            UpdateRefPicList(m_videoParam.cslice);
            m_videoParam.cslice->num_ref_idx[0] = m_videoParam.cslice->num_ref_idx_l0_active;
            m_videoParam.cslice->num_ref_idx[1] = m_videoParam.cslice->num_ref_idx_l1_active;
        } else {
            m_videoParam.cslice->num_ref_idx[0] = 0;
            m_videoParam.cslice->num_ref_idx[1] = 0;
        }

        if (!m_pps.entropy_coding_sync_enabled_flag) {
            PutSliceHeader(bs, m_videoParam.cslice);
            overheadBytes += H265Bs_GetBsSize(bs);
        }

        ippiCABACInit_H265(&bs->cabacState,
            bs->m_base.m_pbsBase,
            bs->m_base.m_bitOffset + (Ipp32u)(bs->m_base.m_pbs - bs->m_base.m_pbsBase) * 8,
            bs->m_base.m_maxBsSize);

        InitializeContextVariablesHEVC_CABAC(bs->m_base.context_array, 2-m_videoParam.cslice->slice_type, m_videoParam.QP);
        Ipp32s offset_prev = 0;
        for(Ipp32u i = m_slices[curr_slice].slice_segment_address; i <= m_slices[curr_slice].slice_address_last_ctb; i++) {
            Ipp32u ctb_col = 0;
            Ipp8u end_of_slice_flag = (i == m_slices[curr_slice].slice_address_last_ctb);
            if (m_pps.entropy_coding_sync_enabled_flag) {
                Ipp32u ctb_row_in_slice = i / m_videoParam.PicWidthInCtbs -
                    m_slices[curr_slice].slice_segment_address / m_videoParam.PicWidthInCtbs;
                ctb_col = i % m_videoParam.PicWidthInCtbs;
                if (ctb_col == 0 && ctb_row_in_slice) {
                    ippiCABACInit_H265(&bs->cabacState,
                        bs->m_base.m_pbsBase,
                        bs->m_base.m_bitOffset + (Ipp32u)(bs->m_base.m_pbs - bs->m_base.m_pbsBase) * 8,
                        bs->m_base.m_maxBsSize);

                    if ((Ipp32s)i - (Ipp32s)m_videoParam.PicWidthInCtbs + 1 >= m_slices[curr_slice].slice_segment_address) {
                        bs->CtxRestoreWPP();
                    } else {
                        InitializeContextVariablesHEVC_CABAC(bs->m_base.context_array, 2-m_videoParam.cslice->slice_type, m_videoParam.QP);
                    }
                    Ipp32s offset = H265Bs_GetBsSize(bs) - offset_prev;
                    offset_prev += offset;
                    m_videoParam.cslice->entry_point_offset[ctb_row_in_slice - 1] = offset;
                }
            }

            memcpy(bsf->m_base.context_array, bs->m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
            bsf->Reset();
            m_slice_ids[i] = curr_slice;
#ifdef DEBUG_CABAC
            printf("\n");
            if (i == 0) printf("Start POC %d\n",m_pCurrentFrame->m_PicOrderCnt);
            printf("CTB %d\n",i);
#endif
            m_pReconstructFrame->cu->InitCU(&m_videoParam, m_pReconstructFrame->cu_data + (i << m_videoParam.Log2NumPartInCU), data_temp, i,
                m_pReconstructFrame->y, m_pReconstructFrame->u, m_pReconstructFrame->v, m_pReconstructFrame->pitch_luma, m_pReconstructFrame->pitch_chroma,
                m_pCurrentFrame->y, m_pCurrentFrame->uv, m_pCurrentFrame->pitch_luma, bsf);
            m_pReconstructFrame->cu->GetInitAvailablity();
            m_pReconstructFrame->cu->ModeDecision(0, 0, 0, NULL);
            //aya
            if( m_videoParam.RDOQFlag )
            {
                memcpy(bsf->m_base.context_array, bs->m_base.context_array, sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT);
                //m_pReconstructFrame->cu->SetRDOQ(true);
            }

//            m_pReconstructFrame->cu->FillRandom(0, 0);
            // inter pred for chroma is now performed in EncAndRecLuma
            m_pReconstructFrame->cu->EncAndRecLuma(0, 0, 0, nz);
            m_pReconstructFrame->cu->EncAndRecChroma(0, 0, 0, nz, NULL);

            if (!(m_videoParam.cslice->slice_deblocking_filter_disabled_flag))
                m_pReconstructFrame->cu->Deblock();
            m_pReconstructFrame->cu->xEncodeCU(bs, 0, 0, 0);

            if (m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1) {
                bs->CtxSaveWPP();
            }
            bs->EncodeSingleBin_CABAC(CTX(bs,END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);
            if (m_pps.entropy_coding_sync_enabled_flag && !end_of_slice_flag &&
                ctb_col == m_videoParam.PicWidthInCtbs - 1) {
#ifdef DEBUG_CABAC
                int d = DEBUG_CABAC_PRINT;
                DEBUG_CABAC_PRINT = 0;
#endif
                bs->EncodeSingleBin_CABAC(CTX(bs,END_OF_SLICE_FLAG_HEVC), 1);
                bs->TerminateEncode_CABAC();
                bs->ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                DEBUG_CABAC_PRINT = d;
#endif
            }
        }
        bs->TerminateEncode_CABAC();
        bs->ByteAlignWithZeros();

        if (m_pps.entropy_coding_sync_enabled_flag) {
            // Here seems to be inconsistency between the standard and HM.
            // The following commented code conforms to standard.
            /*
            Ipp8u* curPtr = bs->m_base.m_pbsRBSPBase;
            for (Ipp32u i = 0; i < m_videoParam.cslice->num_entry_point_offsets; i++) {
                Ipp32u offset_add = 0;
                for (Ipp32u j = i > 0 ? 0 : 1; j < m_videoParam.cslice->entry_point_offset[i]; j++) {
                    if (!curPtr[j - 1] && !curPtr[j] && !(curPtr[j + 1] & 0xfc))
                        offset_add ++;
                }
                curPtr += m_videoParam.cslice->entry_point_offset[i];
                m_videoParam.cslice->entry_point_offset[i] += offset_add;
            }
            */

            bs_sh->Reset();
            PutSliceHeader(bs_sh, m_videoParam.cslice);
            Ipp32s sh_size = H265Bs_GetBsSize(bs_sh);
            if (sh_size > MAX_SLICEHEADER_LEN)
                return MFX_ERR_UNKNOWN;
            overheadBytes += sh_size;
            bs->JoinHeadBytes(bs_sh);
        }

        nal.nal_unit_type = (Ipp8u)(m_slices[curr_slice].IdrPicFlag ? NAL_UT_CODED_SLICE_IDR : NAL_UT_CODED_SLICE_TRAIL_R);
        bs->WriteNAL(mfxBS, 0, &nal);
    }

    frameBytes = mfxBS->DataLength - initialDatalength;

    if (m_brc)
        m_brc->PostPackFrame((mfxU16)m_pCurrentFrame->m_PicCodType, frameBytes << 3, overheadBytes << 3, 0, m_pCurrentFrame->PicOrderCnt());

    switch (ePic_Class)
    {
    case IDR_PIC:
    case REFERENCE_PIC:
        m_pCurrentFrame->SetisShortTermRef();
        m_pReconstructFrame->doPadding();
        break;

    case DISPOSABLE_PIC:
    default:
        break;
    }

    m_pCurrentFrame->swapData(m_pReconstructFrame);
    m_pCurrentFrame->setWasEncoded();
    m_pCurrentFrame->Dump(&m_videoParam, &m_dpb, m_frameCountEncoded);
//    mfxBS->TimeStamp = m_pCurrentFrame->TimeStamp;

    CleanDPB();

    m_frameCountEncoded++;

    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
