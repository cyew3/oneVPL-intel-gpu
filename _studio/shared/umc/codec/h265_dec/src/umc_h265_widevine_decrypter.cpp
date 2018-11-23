// Copyright (c) 2003-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2.h"
#endif

#ifdef UMC_VA_LINUX
#include "umc_va_linux.h"
#endif

#include "umc_h265_tables.h"
#include "umc_h265_widevine_decrypter.h"
#include "umc_h265_widevine_slice_decoding.h"

namespace UMC_HEVC_DECODER
{
#ifdef UMC_VA_DXVA
#define D3DFMT_NV12 (D3DFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'))
#endif

DecryptParametersWrapper::DecryptParametersWrapper(void)
{
    uiStatus = 0;

    ui16StatusReportFeebackNumber = 0;
    ui16SlicePicOrderCntLsb = 0;

    ui8SliceType = 0;
    ui8NalUnitType = 0;
    ui8PicOutputFlag = 0;
    ui8NoOutputOfPriorPicsFlag = 0;

    ui8NuhTemporalId = 0;
    ui8HasEOS = 0;
    ui16Padding = 0;

    memset(&SeqParams, 0, sizeof(h265_seq_param_set_t));
    memset(&PicParams, 0, sizeof(h265_pic_param_set_t));
    memset(&RefFrames, 0, sizeof(h265_ref_frames_t));
    memset(&SeiBufferingPeriod, 0, sizeof(h265_sei_buffering_period_t));
    memset(&SeiPicTiming, 0, sizeof(h265_sei_pic_timing_t));

    m_pts = 0;
}

DecryptParametersWrapper::DecryptParametersWrapper(const DECRYPT_QUERY_STATUS_PARAMS_HEVC & pDecryptParameters)
{
    CopyDecryptParams(pDecryptParameters);
    m_pts = 0;
}

DecryptParametersWrapper::~DecryptParametersWrapper()
{
    ;
}

DecryptParametersWrapper & DecryptParametersWrapper::operator = (const DECRYPT_QUERY_STATUS_PARAMS_HEVC & pDecryptParameters)
{
    CopyDecryptParams(pDecryptParameters);
    m_pts = 0;
    return *this;
}

DecryptParametersWrapper & DecryptParametersWrapper::operator = (const DecryptParametersWrapper & pDecryptParametersWrapper)
{
    m_pts = pDecryptParametersWrapper.m_pts;
    return DecryptParametersWrapper::operator = ( *((const DECRYPT_QUERY_STATUS_PARAMS_HEVC *)&pDecryptParametersWrapper));
}

void DecryptParametersWrapper::CopyDecryptParams(const DECRYPT_QUERY_STATUS_PARAMS_HEVC & pDecryptParameters)
{
    DECRYPT_QUERY_STATUS_PARAMS_HEVC* temp = this;
    *temp = pDecryptParameters;
}

// Parse HRD information in VPS or in VUI block of SPS
void DecryptParametersWrapper::parseHrdParameters(H265HRD *hrd, uint8_t cprms_present_flag, uint32_t vps_max_sub_layers)
{
    hrd->initial_cpb_removal_delay_length = 23 + 1;
    hrd->au_cpb_removal_delay_length = 23 + 1;
    hrd->dpb_output_delay_length = 23 + 1;

    if (cprms_present_flag)
    {
        hrd->nal_hrd_parameters_present_flag = SeqParams.sps_vui.vui_hrd_parameters.nal_hrd_parameters_present_flag;
        hrd->vcl_hrd_parameters_present_flag = SeqParams.sps_vui.vui_hrd_parameters.vcl_hrd_parameters_present_flag;

        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag)
        {
            hrd->sub_pic_hrd_params_present_flag = SeqParams.sps_vui.vui_hrd_parameters.hrd_sub_pic_cpb_params_present_flag;
            if (hrd->sub_pic_hrd_params_present_flag)
            {
                hrd->tick_divisor = SeqParams.sps_vui.vui_hrd_parameters.hrd_tick_divisor_minus2 + 2;
                hrd->du_cpb_removal_delay_increment_length = SeqParams.sps_vui.vui_hrd_parameters.hrd_du_cpb_removal_delay_increment_length_minus1 + 1;
                hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = SeqParams.sps_vui.vui_hrd_parameters.hrd_sub_pic_cpb_params_in_pic_timing_sei_flag;
                hrd->dpb_output_delay_du_length = SeqParams.sps_vui.vui_hrd_parameters.hrd_dpb_output_delay_du_length_minus1 + 1;
            }

            hrd->bit_rate_scale = SeqParams.sps_vui.vui_hrd_parameters.hrd_bit_rate_scale;
            hrd->cpb_size_scale = SeqParams.sps_vui.vui_hrd_parameters.hrd_cpb_size_scale;

            if (hrd->sub_pic_cpb_params_in_pic_timing_sei_flag)
            {
                hrd->cpb_size_du_scale = SeqParams.sps_vui.vui_hrd_parameters.hrd_cpb_size_du_scale;
            }

            hrd->initial_cpb_removal_delay_length = SeqParams.sps_vui.vui_hrd_parameters.hrd_initial_cpb_removal_delay_length_minus1 + 1;
            hrd->au_cpb_removal_delay_length = SeqParams.sps_vui.vui_hrd_parameters.hrd_au_cpb_removal_delay_length_minus1 + 1;
            hrd->dpb_output_delay_length = SeqParams.sps_vui.vui_hrd_parameters.hrd_dpb_output_delay_length_minus1 + 1;
        }
    }

    for (uint32_t i = 0; i < vps_max_sub_layers; i++)
    {
        H265HrdSubLayerInfo * hrdSubLayerInfo = hrd->GetHRDSubLayerParam(i);
        hrdSubLayerInfo->fixed_pic_rate_general_flag = SeqParams.sps_vui.vui_hrd_parameters.hrd_fixed_pic_rate_general_flag[i];

        if (!hrdSubLayerInfo->fixed_pic_rate_general_flag)
        {
            hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = SeqParams.sps_vui.vui_hrd_parameters.hrd_fixed_pic_rate_within_cvs_flag[i];
        }
        else
        {
            hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = 1;
        }

        // Infered to be 0 when not present
        hrdSubLayerInfo->low_delay_hrd_flag = 0;
        hrdSubLayerInfo->cpb_cnt = 1;

        if (hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag)
        {
            hrdSubLayerInfo->elemental_duration_in_tc = SeqParams.sps_vui.vui_hrd_parameters.hrd_elemental_duration_in_tc_minus1[i] + 1;
        }
        else
        {
            hrdSubLayerInfo->low_delay_hrd_flag = SeqParams.sps_vui.vui_hrd_parameters.hrd_low_delay_hrd_flag[i];
        }

        if (!hrdSubLayerInfo->low_delay_hrd_flag)
        {
            hrdSubLayerInfo->cpb_cnt = SeqParams.sps_vui.vui_hrd_parameters.hrd_cpb_cnt_minus1[i] + 1;

            if (hrdSubLayerInfo->cpb_cnt > 32)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        if(hrd->nal_hrd_parameters_present_flag)
        {
            for (uint32_t j = 0 ; j < hrdSubLayerInfo->cpb_cnt; j++)
            {
                hrdSubLayerInfo->bit_rate_value[j][0] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_nal.bit_rate_value_minus1[j] + 1;
                hrdSubLayerInfo->cpb_size_value[j][0] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_nal.cpb_size_value_minus1[j] + 1;
                if (hrd->sub_pic_hrd_params_present_flag)
                {
                    hrdSubLayerInfo->bit_rate_du_value[j][0] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_nal.cpb_size_du_value_minus1[j] + 1;
                    hrdSubLayerInfo->cpb_size_du_value[j][0] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_nal.bit_rate_du_value_minus1[j] + 1;
                }

                hrdSubLayerInfo->cbr_flag[j][0] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_nal.cbr_flag[j];
            }
        }

        if(hrd->vcl_hrd_parameters_present_flag)
        {
            for (uint32_t j = 0 ; j < hrdSubLayerInfo->cpb_cnt; j++)
            {
                hrdSubLayerInfo->bit_rate_value[j][1] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_vcl.bit_rate_value_minus1[j] + 1;
                hrdSubLayerInfo->cpb_size_value[j][1] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_vcl.cpb_size_value_minus1[j] + 1;
                if (hrd->sub_pic_hrd_params_present_flag)
                {
                    hrdSubLayerInfo->bit_rate_du_value[j][1] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_vcl.cpb_size_du_value_minus1[j] + 1;
                    hrdSubLayerInfo->cpb_size_du_value[j][1] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_vcl.bit_rate_du_value_minus1[j] + 1;
                }

                hrdSubLayerInfo->cbr_flag[j][1] = SeqParams.sps_vui.vui_hrd_parameters.hrd_sublayer_vcl.cbr_flag[j];
            }
        }
    }
}

UMC::Status DecryptParametersWrapper::GetVideoParamSet(H265VideoParamSet * /*pcVPS*/)
{
    // Currently we do not have VPS from decryption library
    return UMC::UMC_OK;
}

// Parse scaling list data block
void DecryptParametersWrapper::xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId, h265_scaling_list * sourceList)
{
    VM_ASSERT(scalingList);

    int i,coefNum = MFX_MIN(MAX_MATRIX_COEF_NUM,(int)g_scalingListSize[sizeId]);
    //int nextCoef = SCALING_LIST_START_VALUE;
    const uint16_t *scan  = (sizeId == 0) ? ScanTableDiag4x4 : g_sigLastScanCG32x32;
    int *dst = scalingList->getScalingListAddress(sizeId, listId);

    uint8_t** source = NULL;
    switch (sizeId)
    {
        case SCALING_LIST_4x4:
            source = (uint8_t**)sourceList->sps_scaling_list_4x4;
            break;
        case SCALING_LIST_8x8:
            source = (uint8_t**)sourceList->sps_scaling_list_8x8;
            break;
        case SCALING_LIST_16x16:
            source = (uint8_t**)sourceList->sps_scaling_list_16x16;
            break;
        case SCALING_LIST_32x32:
            source = (uint8_t**)sourceList->sps_scaling_list_32x32;
            break;
        default:
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
            break;
    };

    if (sizeId == SCALING_LIST_16x16)
        scalingList->setScalingListDC(sizeId, listId, sourceList->scaling_list_16x16_dc[listId]);
    else if (sizeId == SCALING_LIST_32x32)
        scalingList->setScalingListDC(sizeId, listId, sourceList->scaling_list_32x32_dc[listId]);

    for(i = 0; i < coefNum; i++)
    {
        dst[scan[i]] = source[listId][i];
    }
}

// Parse scaling list information in SPS or PPS
void DecryptParametersWrapper::parseScalingList(H265ScalingList *scalingList, h265_scaling_list * sourceList)
{
    if (!scalingList)
        throw h265_exception(UMC::UMC_ERR_NULL_PTR);

    //for each size
    for(uint32_t sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(uint32_t listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
        {
            uint8_t scaling_list_pred_mode_flag = sourceList->scaling_list_pred_mode_flag[sizeId][listId];
            if(!scaling_list_pred_mode_flag) //Copy Mode
            {
                uint32_t scaling_list_pred_matrix_id_delta = sourceList->scaling_list_pred_matrix_id_delta[sizeId][listId];
                if (scaling_list_pred_matrix_id_delta > listId)
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                scalingList->setRefMatrixId (sizeId, listId, listId-scaling_list_pred_matrix_id_delta);
                if (sizeId > SCALING_LIST_8x8)
                {
                    scalingList->setScalingListDC(sizeId,listId,((listId == scalingList->getRefMatrixId (sizeId,listId))? 16 :scalingList->getScalingListDC(sizeId, scalingList->getRefMatrixId (sizeId,listId))));
                }

                scalingList->processRefMatrix( sizeId, listId, scalingList->getRefMatrixId (sizeId,listId));
            }
            else //DPCM Mode
            {
                xDecodeScalingList(scalingList, sizeId, listId, sourceList);
            }
        }
    }
}

// Parse profile tier layers header part in VPS or SPS
void DecryptParametersWrapper::parsePTL(H265ProfileTierLevel *rpcPTL, int maxNumSubLayersMinus1)
{
    VM_ASSERT(rpcPTL);

    parseProfileTier(rpcPTL->GetGeneralPTL(), &SeqParams.sps_profile_tier_level.general_ptl);

    int32_t level_idc = SeqParams.sps_profile_tier_level.general_ptl.layer_level_idc;
    level_idc = ((level_idc * 10) / 30);
    rpcPTL->GetGeneralPTL()->level_idc = level_idc;

    for(int i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if (SeqParams.sps_profile_tier_level.sub_layer_profile_present_flag[i])
        {
            rpcPTL->sub_layer_profile_present_flags |= 1 << i;
            parseProfileTier(rpcPTL->GetSubLayerPTL(i), &SeqParams.sps_profile_tier_level.sub_layer_ptl[i]);
        }

        if (SeqParams.sps_profile_tier_level.sub_layer_level_present_flag[i])
        {
            rpcPTL->sub_layer_level_present_flag |= 1 << i;
            level_idc = SeqParams.sps_profile_tier_level.sub_layer_ptl[i].layer_level_idc;
            level_idc = ((level_idc*10) / 30);
            rpcPTL->GetSubLayerPTL(i)->level_idc = level_idc;
        }
    }
}

// Parse one profile tier layer
void DecryptParametersWrapper::parseProfileTier(H265PTL *ptl, h265_layer_profile_tier_level *sourcePtl)
{
    VM_ASSERT(ptl);

    ptl->profile_space = sourcePtl->layer_profile_space;
    if (ptl->profile_space)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    ptl->tier_flag = sourcePtl->layer_tier_flag;
    ptl->profile_idc = sourcePtl->layer_profile_idc;

    for(int j = 0; j < 32; j++)
    {
        if (sourcePtl->layer_profile_compatibility_flag[j])
            ptl->profile_compatibility_flags |= 1 << j;
    }

    if (!ptl->profile_idc)
    {
        ptl->profile_idc = H265_PROFILE_MAIN;
        for(int j = 1; j < 32; j++)
        {
            if (ptl->profile_compatibility_flags & (1 << j))
            {
                ptl->profile_idc = j;
                break;
            }
        }
    }

    if (ptl->profile_idc > H265_PROFILE_MAINSP)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    ptl->progressive_source_flag    = sourcePtl->layer_progressive_source_flag;
    ptl->interlaced_source_flag     = sourcePtl->layer_interlaced_source_flag;
    ptl->non_packed_constraint_flag = sourcePtl->layer_non_packed_constraint_flag;
    ptl->frame_only_constraint_flag = sourcePtl->layer_frame_only_constraint_flag;
}

UMC::Status DecryptParametersWrapper::GetSequenceParamSet(H265SeqParamSet *pcSPS)
{
    if (!pcSPS)
        throw h265_exception(UMC::UMC_ERR_NULL_PTR);

    pcSPS->sps_video_parameter_set_id = SeqParams.sps_video_parameter_set_id;

    pcSPS->sps_max_sub_layers = SeqParams.sps_max_sub_layers_minus1 + 1;

    if (pcSPS->sps_max_sub_layers > 7)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->sps_temporal_id_nesting_flag = SeqParams.sps_temporal_id_nesting_flag;

    if (pcSPS->sps_max_sub_layers == 1)
    {
        // sps_temporal_id_nesting_flag must be 1 when sps_max_sub_layers_minus1 is 0
        if (pcSPS->sps_temporal_id_nesting_flag != 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    parsePTL(pcSPS->getPTL(), pcSPS->sps_max_sub_layers - 1);

    pcSPS->sps_seq_parameter_set_id = SeqParams.sps_seq_parameter_set_id;
    if (pcSPS->sps_seq_parameter_set_id > 15)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->chroma_format_idc = SeqParams.sps_chroma_format_idc;
    if (pcSPS->chroma_format_idc > 3)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (pcSPS->chroma_format_idc == 3)
    {
        pcSPS->separate_colour_plane_flag = SeqParams.sps_separate_colour_plane_flag;
    }

    pcSPS->ChromaArrayType = pcSPS->separate_colour_plane_flag ? 0 : pcSPS->chroma_format_idc;
    pcSPS->chromaShiftW = 1;
    pcSPS->chromaShiftH = pcSPS->ChromaArrayType == CHROMA_FORMAT_422 ? 0 : 1;

    pcSPS->pic_width_in_luma_samples  = SeqParams.sps_pic_width_in_luma_samples;
    pcSPS->pic_height_in_luma_samples = SeqParams.sps_pic_height_in_luma_samples;
    pcSPS->conformance_window_flag = SeqParams.sps_conformance_window_flag;

    if (pcSPS->conformance_window_flag)
    {
        pcSPS->conf_win_left_offset   = SeqParams.sps_conf_win_left_offset*pcSPS->SubWidthC();
        pcSPS->conf_win_right_offset  = SeqParams.sps_conf_win_right_offset*pcSPS->SubWidthC();
        pcSPS->conf_win_top_offset    = SeqParams.sps_conf_win_top_offset*pcSPS->SubHeightC();
        pcSPS->conf_win_bottom_offset = SeqParams.sps_conf_win_bottom_offset*pcSPS->SubHeightC();

        if (pcSPS->conf_win_left_offset + pcSPS->conf_win_right_offset >= pcSPS->pic_width_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pcSPS->conf_win_top_offset + pcSPS->conf_win_bottom_offset >= pcSPS->pic_height_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    pcSPS->bit_depth_luma = SeqParams.sps_bit_depth_luma_minus8 + 8;
    if (pcSPS->bit_depth_luma > 14)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->setQpBDOffsetY(6*(pcSPS->bit_depth_luma - 8));

    pcSPS->bit_depth_chroma = SeqParams.sps_bit_depth_chroma_minus8 + 8;
    if (pcSPS->bit_depth_chroma > 14)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->setQpBDOffsetC(6*(pcSPS->bit_depth_chroma - 8));

    if ((pcSPS->bit_depth_luma > 8 || pcSPS->bit_depth_chroma > 8) && pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc < H265_PROFILE_MAIN10)
        pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc = H265_PROFILE_MAIN10;

    if (pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc == H265_PROFILE_MAIN10 || pcSPS->bit_depth_luma > 8 || pcSPS->bit_depth_chroma > 8)
        pcSPS->need16bitOutput = 1;

    pcSPS->log2_max_pic_order_cnt_lsb = SeqParams.sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
    if (pcSPS->log2_max_pic_order_cnt_lsb > 16)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->sps_sub_layer_ordering_info_present_flag = SeqParams.sps_sub_layer_ordering_info_present_flag;

    uint32_t start = pcSPS->sps_sub_layer_ordering_info_present_flag ? 0 : (pcSPS->sps_max_sub_layers-1);
    for (uint32_t i = 0; i < pcSPS->sps_max_sub_layers; i++)
    {
        pcSPS->sps_max_dec_pic_buffering[i] = SeqParams.sps_max_dec_pic_buffering_minus1[start+i] + 1;

        if (pcSPS->sps_max_dec_pic_buffering[i] > 16)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->sps_max_num_reorder_pics[i] = SeqParams.sps_max_num_reorder_pics[start+i];

        if (pcSPS->sps_max_num_reorder_pics[i] > pcSPS->sps_max_dec_pic_buffering[i])
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->sps_max_latency_increase[i] = SeqParams.sps_max_latency_increase[start+i];

        if (!pcSPS->sps_sub_layer_ordering_info_present_flag)
        {
            for (i++; i <= pcSPS->sps_max_sub_layers-1; i++)
            {
                pcSPS->sps_max_dec_pic_buffering[i] = pcSPS->sps_max_dec_pic_buffering[0];
                pcSPS->sps_max_num_reorder_pics[i] = pcSPS->sps_max_num_reorder_pics[0];
                pcSPS->sps_max_latency_increase[i] = pcSPS->sps_max_latency_increase[0];
            }
            break;
        }

        if (i > 0)
        {
            if (pcSPS->sps_max_dec_pic_buffering[i] < pcSPS->sps_max_dec_pic_buffering[i - 1] ||
                pcSPS->sps_max_num_reorder_pics[i] < pcSPS->sps_max_num_reorder_pics[i - 1])
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    uint32_t const log2_min_luma_coding_block_size_minus3 = SeqParams.sps_log2_min_luma_coding_block_size_minus3;
    if (log2_min_luma_coding_block_size_minus3 > 3)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->log2_min_luma_coding_block_size = log2_min_luma_coding_block_size_minus3 + 3;

    uint32_t MinCbLog2SizeY = pcSPS->log2_min_luma_coding_block_size;
    uint32_t MinCbSizeY = 1 << pcSPS->log2_min_luma_coding_block_size;

    if ((pcSPS->pic_width_in_luma_samples % MinCbSizeY) || (pcSPS->pic_height_in_luma_samples % MinCbSizeY))
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    uint32_t log2_diff_max_min_coding_block_size = SeqParams.sps_log2_diff_max_min_luma_coding_block_size;
    if (log2_diff_max_min_coding_block_size > 3)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->log2_max_luma_coding_block_size = log2_diff_max_min_coding_block_size + pcSPS->log2_min_luma_coding_block_size;
    //CtbLog2SizeY = (log2_min_luma_coding_block_size_minus3 + 3) + log2_diff_max_min_luma_coding_block_size //7.3.2.2 eq. 7-10, 7-11
    //CtbLog2SizeY derived according to active SPSs for the base layer shall be in the range of 4 to 6, inclusive. (A.2, main, main10, mainsp, rext profiles)
    if (pcSPS->log2_max_luma_coding_block_size < 4 || pcSPS->log2_max_luma_coding_block_size > 6)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->MaxCUSize =  1 << pcSPS->log2_max_luma_coding_block_size;

    pcSPS->log2_min_transform_block_size = SeqParams.sps_log2_min_transform_block_size_minus2 + 2;

    if (pcSPS->log2_min_transform_block_size >= MinCbLog2SizeY)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    uint32_t log2_diff_max_min_transform_block_size = SeqParams.sps_log2_diff_max_min_transform_block_size;
    pcSPS->log2_max_transform_block_size = log2_diff_max_min_transform_block_size + pcSPS->log2_min_transform_block_size;
    pcSPS->m_maxTrSize = 1 << pcSPS->log2_max_transform_block_size;

    uint32_t CtbLog2SizeY = pcSPS->log2_max_luma_coding_block_size;
    if (pcSPS->log2_max_transform_block_size > MFX_MIN(5, CtbLog2SizeY))
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->max_transform_hierarchy_depth_inter = SeqParams.sps_max_transform_hierarchy_depth_inter;
    pcSPS->max_transform_hierarchy_depth_intra = SeqParams.sps_max_transform_hierarchy_depth_intra;

    if (pcSPS->max_transform_hierarchy_depth_inter > CtbLog2SizeY - pcSPS->log2_min_transform_block_size)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (pcSPS->max_transform_hierarchy_depth_intra > CtbLog2SizeY - pcSPS->log2_min_transform_block_size)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    uint32_t addCUDepth = 0;
    while((pcSPS->MaxCUSize >> log2_diff_max_min_coding_block_size) > (uint32_t)( 1 << ( pcSPS->log2_min_transform_block_size + addCUDepth)))
    {
        addCUDepth++;
    }

    pcSPS->AddCUDepth = addCUDepth;
    pcSPS->MaxCUDepth = log2_diff_max_min_coding_block_size + addCUDepth;
    pcSPS->MinCUSize = pcSPS->MaxCUSize >> pcSPS->MaxCUDepth;
    // BB: these parameters may be removed completly and replaced by the fixed values
    pcSPS->scaling_list_enabled_flag = SeqParams.sps_scaling_list_enabled_flag;
    if(pcSPS->scaling_list_enabled_flag)
    {
        pcSPS->sps_scaling_list_data_present_flag = SeqParams.sps_scaling_list_data_present_flag;
        if(pcSPS->sps_scaling_list_data_present_flag)
        {
            parseScalingList(pcSPS->getScalingList(), &SeqParams.sps_scaling_list);
        }
    }

    pcSPS->amp_enabled_flag = SeqParams.sps_amp_enabled_flag;
    pcSPS->sample_adaptive_offset_enabled_flag = SeqParams.sps_sample_adaptive_offset_enabled_flag;
    pcSPS->pcm_enabled_flag = SeqParams.sps_pcm_enabled_flag;

    if(pcSPS->pcm_enabled_flag)
    {
        pcSPS->pcm_sample_bit_depth_luma = SeqParams.sps_pcm_sample_bit_depth_luma_minus1 + 1;
        pcSPS->pcm_sample_bit_depth_chroma = SeqParams.sps_pcm_sample_bit_depth_chroma_minus1 + 1;

        if (pcSPS->pcm_sample_bit_depth_luma > pcSPS->bit_depth_luma ||
            pcSPS->pcm_sample_bit_depth_chroma > pcSPS->bit_depth_chroma)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->log2_min_pcm_luma_coding_block_size = SeqParams.sps_log2_min_pcm_luma_coding_block_size_minus3 + 3;

        if (pcSPS->log2_min_pcm_luma_coding_block_size < MFX_MIN(MinCbLog2SizeY, 5) || pcSPS->log2_min_pcm_luma_coding_block_size > MFX_MIN(CtbLog2SizeY, 5))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->log2_max_pcm_luma_coding_block_size = SeqParams.sps_log2_diff_max_min_pcm_luma_coding_block_size + pcSPS->log2_min_pcm_luma_coding_block_size;

        if (pcSPS->log2_max_pcm_luma_coding_block_size > MFX_MIN(CtbLog2SizeY, 5))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->pcm_loop_filter_disabled_flag = SeqParams.sps_pcm_loop_filter_disable_flag;
    }

    pcSPS->num_short_term_ref_pic_sets = SeqParams.sps_num_short_term_ref_pic_sets;
    if (pcSPS->num_short_term_ref_pic_sets > 64)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->createRPSList(pcSPS->num_short_term_ref_pic_sets);

    ReferencePictureSetList* rpsList = pcSPS->getRPSList();
    ReferencePictureSet* rps;

    for(uint32_t i = 0; i < rpsList->getNumberOfReferencePictureSets(); i++)
    {
        rps = rpsList->getReferencePictureSet(i);
        parseShortTermRefPicSet(pcSPS, rps, i);

        if (((uint32_t)rps->getNumberOfNegativePictures() > pcSPS->sps_max_dec_pic_buffering[pcSPS->sps_max_sub_layers - 1]) ||
            ((uint32_t)rps->getNumberOfPositivePictures() > pcSPS->sps_max_dec_pic_buffering[pcSPS->sps_max_sub_layers - 1] - (uint32_t)rps->getNumberOfNegativePictures()))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    pcSPS->long_term_ref_pics_present_flag = SeqParams.sps_long_term_ref_pics_present_flag;
    if (pcSPS->long_term_ref_pics_present_flag)
    {
        pcSPS->num_long_term_ref_pics_sps = SeqParams.sps_num_long_term_ref_pics_sps;

        if (pcSPS->num_long_term_ref_pics_sps > 32)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        for (uint32_t k = 0; k < pcSPS->num_long_term_ref_pics_sps; k++)
        {
            pcSPS->lt_ref_pic_poc_lsb_sps[k] = SeqParams.sps_lt_ref_pic_poc_lsb_sps[k];
            pcSPS->used_by_curr_pic_lt_sps_flag[k] = SeqParams.sps_used_by_curr_pic_lt_sps_flag[k];
        }
    }

    pcSPS->sps_temporal_mvp_enabled_flag = SeqParams.sps_temporal_mvp_enabled_flag;
    pcSPS->sps_strong_intra_smoothing_enabled_flag = SeqParams.sps_strong_intra_smoothing_enabled_flag;
    pcSPS->vui_parameters_present_flag = SeqParams.sps_vui_parameters_present_flag;

    if (pcSPS->vui_parameters_present_flag)
    {
        parseVUI(pcSPS);
    }

    return UMC::UMC_OK;
}

// Parse video usability information block in SPS
void DecryptParametersWrapper::parseVUI(H265SeqParamSet *pSPS)
{
    VM_ASSERT(pSPS);

    pSPS->aspect_ratio_info_present_flag = SeqParams.sps_vui.vui_aspect_ratio_info_present_flag;
    if (pSPS->aspect_ratio_info_present_flag)
    {
        pSPS->aspect_ratio_idc = SeqParams.sps_vui.vui_aspect_ratio_idc;
        if (pSPS->aspect_ratio_idc == 255)
        {
            pSPS->sar_width = SeqParams.sps_vui.vui_sar_width;
            pSPS->sar_height = SeqParams.sps_vui.vui_sar_height;
        }
        else
        {
            if (!pSPS->aspect_ratio_idc || pSPS->aspect_ratio_idc >= sizeof(SAspectRatio)/sizeof(SAspectRatio[0]))
            {
                pSPS->aspect_ratio_idc = 0;
                pSPS->aspect_ratio_info_present_flag = 0;
            }
            else
            {
                pSPS->sar_width = SAspectRatio[pSPS->aspect_ratio_idc][0];
                pSPS->sar_height = SAspectRatio[pSPS->aspect_ratio_idc][1];
            }
        }
    }

    pSPS->overscan_info_present_flag = SeqParams.sps_vui.vui_overscan_info_present_flag;
    if (pSPS->overscan_info_present_flag)
    {
        pSPS->overscan_appropriate_flag = SeqParams.sps_vui.vui_overscan_appropriate_flag;
    }

    pSPS->video_signal_type_present_flag = SeqParams.sps_vui.vui_video_signal_type_present_flag;
    if (pSPS->video_signal_type_present_flag)
    {
        pSPS->video_format = SeqParams.sps_vui.vui_video_format;
        pSPS->video_full_range_flag = SeqParams.sps_vui.vui_video_full_range_flag;
        pSPS->colour_description_present_flag = SeqParams.sps_vui.vui_colour_description_present_flag;
        if (pSPS->colour_description_present_flag)
        {
            pSPS->colour_primaries = SeqParams.sps_vui.vui_colour_primaries;
            pSPS->transfer_characteristics = SeqParams.sps_vui.vui_transfer_characteristics;
            pSPS->matrix_coeffs = SeqParams.sps_vui.vui_matrix_coefficients;
        }
    }

    pSPS->chroma_loc_info_present_flag = SeqParams.sps_vui.vui_chroma_loc_info_present_flag;
    if (pSPS->chroma_loc_info_present_flag)
    {
        pSPS->chroma_sample_loc_type_top_field = SeqParams.sps_vui.vui_chroma_sample_loc_type_top_field;
        pSPS->chroma_sample_loc_type_bottom_field = SeqParams.sps_vui.vui_chroma_sample_loc_type_bottom_field;
    }

    pSPS->neutral_chroma_indication_flag = SeqParams.sps_vui.vui_neutral_chroma_indication_flag;
    pSPS->field_seq_flag = SeqParams.sps_vui.vui_field_seq_flag;
    pSPS->frame_field_info_present_flag = SeqParams.sps_vui.vui_frame_field_info_present_flag;

    pSPS->default_display_window_flag = SeqParams.sps_vui.vui_default_display_window_flag;
    if (pSPS->default_display_window_flag)
    {
        pSPS->def_disp_win_left_offset   = SeqParams.sps_vui.vui_def_disp_win_left_offset*pSPS->SubWidthC();
        pSPS->def_disp_win_right_offset  = SeqParams.sps_vui.vui_def_disp_win_right_offset*pSPS->SubWidthC();
        pSPS->def_disp_win_top_offset    = SeqParams.sps_vui.vui_def_disp_win_top_offset*pSPS->SubHeightC();
        pSPS->def_disp_win_bottom_offset = SeqParams.sps_vui.vui_def_disp_win_bottom_offset*pSPS->SubHeightC();

        if (pSPS->def_disp_win_left_offset + pSPS->def_disp_win_right_offset >= pSPS->pic_width_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pSPS->def_disp_win_top_offset + pSPS->def_disp_win_bottom_offset >= pSPS->pic_height_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    H265TimingInfo *timingInfo = pSPS->getTimingInfo();
    timingInfo->vps_timing_info_present_flag = SeqParams.sps_vui.vui_timing_info_present_flag;
    if (timingInfo->vps_timing_info_present_flag)
    {
        timingInfo->vps_num_units_in_tick = SeqParams.sps_vui.vui_num_units_in_tick;
        timingInfo->vps_time_scale = SeqParams.sps_vui.vui_time_scale;

        timingInfo->vps_poc_proportional_to_timing_flag = SeqParams.sps_vui.vui_poc_proportional_to_timing_flag;
        if (timingInfo->vps_poc_proportional_to_timing_flag)
        {
            timingInfo->vps_num_ticks_poc_diff_one = SeqParams.sps_vui.vui_num_ticks_poc_diff_one_minus1 + 1;
        }

        pSPS->vui_hrd_parameters_present_flag = SeqParams.sps_vui.vui_hrd_parameters_present_flag;
        if (pSPS->vui_hrd_parameters_present_flag)
        {
            parseHrdParameters( pSPS->getHrdParameters(), 1, pSPS->sps_max_sub_layers);
        }
    }

    pSPS->bitstream_restriction_flag = SeqParams.sps_vui.vui_bitstream_restriction_flag;
    if (pSPS->bitstream_restriction_flag)
    {
        pSPS->tiles_fixed_structure_flag = SeqParams.sps_vui.vui_tiles_fixed_structure_flag;
        pSPS->motion_vectors_over_pic_boundaries_flag = SeqParams.sps_vui.vui_motion_vectors_over_pic_boundaries_flag;
        pSPS->restricted_ref_pic_lists_flag = SeqParams.sps_vui.vui_restricted_ref_pic_lists_flag;
        pSPS->min_spatial_segmentation_idc = SeqParams.sps_vui.vui_min_spatial_segmentation_idc;
        pSPS->max_bytes_per_pic_denom = SeqParams.sps_vui.vui_max_bytes_per_pic_denom;
        pSPS->max_bits_per_min_cu_denom = SeqParams.sps_vui.vui_max_bits_per_mincu_denom;
        pSPS->log2_max_mv_length_horizontal = SeqParams.sps_vui.vui_log2_max_mv_length_horizontal;
        pSPS->log2_max_mv_length_vertical = SeqParams.sps_vui.vui_log2_max_mv_length_vertical;
    }
}

UMC::Status DecryptParametersWrapper::GetPictureParamSetFull(H265PicParamSet *pcPPS)
{
    if (!pcPPS)
        throw h265_exception(UMC::UMC_ERR_NULL_PTR);

    pcPPS->pps_pic_parameter_set_id = PicParams.pps_pic_parameter_set_id;
    if (pcPPS->pps_pic_parameter_set_id > 63)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->pps_seq_parameter_set_id = PicParams.pps_seq_parameter_set_id;
    if (pcPPS->pps_seq_parameter_set_id > 15)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->dependent_slice_segments_enabled_flag = PicParams.pps_dependent_slice_segments_enabled_flag;
    pcPPS->output_flag_present_flag = PicParams.pps_output_flag_present_flag;
    pcPPS->num_extra_slice_header_bits = PicParams.pps_num_extra_slice_header_bits;
    pcPPS->sign_data_hiding_enabled_flag =  PicParams.pps_sign_data_hiding_flag;
    pcPPS->cabac_init_present_flag =  PicParams.pps_cabac_init_present_flag;
    pcPPS->num_ref_idx_l0_default_active = PicParams.pps_num_ref_idx_l0_default_active_minus1 + 1;

    if (pcPPS->num_ref_idx_l0_default_active > 15)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->num_ref_idx_l1_default_active = PicParams.pps_num_ref_idx_l1_default_active_minus1 + 1;

    if (pcPPS->num_ref_idx_l1_default_active > 15)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->init_qp = (int8_t)(PicParams.pps_init_qp_minus26 + 26);

    pcPPS->constrained_intra_pred_flag = PicParams.pps_constrained_intra_pred_flag;
    pcPPS->transform_skip_enabled_flag = PicParams.pps_transform_skip_enabled_flag;

    pcPPS->cu_qp_delta_enabled_flag = PicParams.pps_cu_qp_delta_enabled_flag;
    if( pcPPS->cu_qp_delta_enabled_flag )
    {
        pcPPS->diff_cu_qp_delta_depth = PicParams.pps_diff_cu_qp_delta_depth;
    }
    else
    {
        pcPPS->diff_cu_qp_delta_depth = 0;
    }

    pcPPS->pps_cb_qp_offset = PicParams.pps_cb_qp_offset;
    if (pcPPS->pps_cb_qp_offset < -12 || pcPPS->pps_cb_qp_offset > 12)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->pps_cr_qp_offset = PicParams.pps_cr_qp_offset;

    if (pcPPS->pps_cr_qp_offset < -12 || pcPPS->pps_cr_qp_offset > 12)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->pps_slice_chroma_qp_offsets_present_flag = PicParams.pps_slice_chroma_qp_offsets_present_flag;

    pcPPS->weighted_pred_flag = PicParams.pps_weighted_pred_flag;
    pcPPS->weighted_bipred_flag = PicParams.pps_weighted_bipred_flag;

    pcPPS->transquant_bypass_enabled_flag = PicParams.pps_transquant_bypass_enabled_flag;
    pcPPS->tiles_enabled_flag = PicParams.pps_tiles_enabled_flag;
    pcPPS->entropy_coding_sync_enabled_flag = PicParams.pps_entropy_coding_sync_enabled_flag;

    if (pcPPS->tiles_enabled_flag)
    {
        pcPPS->num_tile_columns = PicParams.pps_num_tile_columns_minus1 + 1;
        pcPPS->num_tile_rows = PicParams.pps_num_tile_rows_minus1 + 1;

        if (pcPPS->num_tile_columns == 1 && pcPPS->num_tile_rows == 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcPPS->uniform_spacing_flag = PicParams.pps_uniform_spacing_flag;

        if (!pcPPS->uniform_spacing_flag)
        {
            pcPPS->column_width.resize(pcPPS->num_tile_columns);
            for (uint32_t i=0; i < pcPPS->num_tile_columns - 1; i++)
            {
                pcPPS->column_width[i] = PicParams.pps_column_width_minus1[i] + 1;
            }

            pcPPS->row_height.resize(pcPPS->num_tile_rows);

            for (uint32_t i=0; i < pcPPS->num_tile_rows - 1; i++)
            {
                pcPPS->row_height[i] = PicParams.pps_row_height_minus1[i] + 1;
            }
        }

        if (pcPPS->num_tile_columns - 1 != 0 || pcPPS->num_tile_rows - 1 != 0)
        {
            pcPPS->loop_filter_across_tiles_enabled_flag = PicParams.pps_loop_filter_across_tiles_enabled_flag;
        }
    }
    else
    {
        pcPPS->num_tile_columns = 1;
        pcPPS->num_tile_rows = 1;
    }

    pcPPS->pps_loop_filter_across_slices_enabled_flag = PicParams.pps_loop_filter_across_slices_enabled_flag;
    pcPPS->deblocking_filter_control_present_flag = PicParams.pps_df_control_present_flag;

    if (pcPPS->deblocking_filter_control_present_flag)
    {
        pcPPS->deblocking_filter_override_enabled_flag = PicParams.pps_deblocking_filter_override_enabled_flag;
        pcPPS->pps_deblocking_filter_disabled_flag = PicParams.pps_disable_deblocking_filter_flag;
        if (!pcPPS->pps_deblocking_filter_disabled_flag)
        {
            pcPPS->pps_beta_offset = PicParams.pps_beta_offset_div2 << 1;
            pcPPS->pps_tc_offset = PicParams.pps_tc_offset_div2 << 1;

            if (pcPPS->pps_beta_offset < -12 || pcPPS->pps_beta_offset > 12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

            if (pcPPS->pps_tc_offset < -12 || pcPPS->pps_tc_offset > 12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    pcPPS->pps_scaling_list_data_present_flag = PicParams.pps_scaling_list_data_present_flag;
    if(pcPPS->pps_scaling_list_data_present_flag)
    {
        parseScalingList(pcPPS->getScalingList(), &PicParams.pps_scaling_list);
    }

    pcPPS->lists_modification_present_flag = PicParams.pps_lists_modification_present_flag;
    pcPPS->log2_parallel_merge_level = PicParams.pps_log2_parallel_merge_level_minus2 + 2;
    pcPPS->slice_segment_header_extension_present_flag = PicParams.pps_slice_segment_header_extension_present_flag;

    return UMC::UMC_OK;
}

UMC::Status DecryptParametersWrapper::GetSliceHeaderPart1(H265SliceHeader * sliceHdr)
{
    if (!sliceHdr)
        throw h265_exception(UMC::UMC_ERR_NULL_PTR);

    sliceHdr->IdrPicFlag = (sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP) ? 1 : 0;

    if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_CRA )
    {
        sliceHdr->no_output_of_prior_pics_flag = ui8NoOutputOfPriorPicsFlag;
    }

    sliceHdr->slice_pic_parameter_set_id = PicParams.pps_pic_parameter_set_id;

    if (sliceHdr->slice_pic_parameter_set_id > 63)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    return UMC::UMC_OK;
}

// Parse remaining of slice header after GetSliceHeaderPart1
void DecryptParametersWrapper::decodeSlice(H265WidevineSlice *pSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    if (!pSlice || !pps || !sps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    H265SliceHeader * sliceHdr = pSlice->GetSliceHeader();

    sliceHdr->slice_type = (SliceType)ui8SliceType;

    if (sliceHdr->slice_type > I_SLICE || sliceHdr->slice_type < B_SLICE)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (pps->output_flag_present_flag)
    {
        sliceHdr->pic_output_flag = ui8PicOutputFlag;
    }
    else
    {
        sliceHdr->pic_output_flag = 1;
    }

    if (sliceHdr->IdrPicFlag)
    {
        sliceHdr->slice_pic_order_cnt_lsb = 0;
        ReferencePictureSet* rps = pSlice->getRPS();
        rps->num_negative_pics = 0;
        rps->num_positive_pics = 0;
        rps->setNumberOfLongtermPictures(0);
        rps->num_pics = 0;
    }
    else
    {
        sliceHdr->slice_pic_order_cnt_lsb = ui16SlicePicOrderCntLsb;

        {
            ReferencePictureSet* rps = pSlice->getRPS();
            parseRefPicSet(rps);

            if (((uint32_t)rps->getNumberOfNegativePictures() > sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1]) ||
                ((uint32_t)rps->getNumberOfPositivePictures() > sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1] - (uint32_t)rps->getNumberOfNegativePictures()))
            {
                pSlice->m_bError = true;
                if (sliceHdr->slice_type != I_SLICE)
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                rps->num_negative_pics = 0;
                rps->num_positive_pics = 0;
                rps->num_pics = 0;
            }
        }

        if (sps->long_term_ref_pics_present_flag)
        {
            ReferencePictureSet* rps = pSlice->getRPS();
            int offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();

            for(uint32_t j = offset, k = 0; k < rps->num_lt_pics; j++, k++)
            {
                rps->delta_poc_msb_present_flag[j] = RefFrames.ref_set_deltapoc_msb_present_flag[k];
            }
        }

        if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
            || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
            || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP )
        {
            ReferencePictureSet* rps = pSlice->getRPS();
            rps->num_negative_pics = 0;
            rps->num_positive_pics = 0;
            rps->setNumberOfLongtermPictures(0);
            rps->num_pics = 0;
        }
    }

    if (!pps->tiles_enabled_flag)
    {
        pSlice->allocateTileLocation(1);
        pSlice->m_tileByteLocation[0] = 0;
    }
}

UMC::Status DecryptParametersWrapper::GetSliceHeaderFull(H265WidevineSlice *rpcSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    if (!rpcSlice)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    UMC::Status sts = GetSliceHeaderPart1(rpcSlice->GetSliceHeader());
    if (UMC::UMC_OK != sts)
        return sts;
    decodeSlice(rpcSlice, sps, pps);

    return UMC::UMC_OK;
}

// Parse RPS part in SPS
void DecryptParametersWrapper::parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* rps, uint32_t idx)
{
    if (!sps || !rps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (idx > 0)
    {
        rps->inter_ref_pic_set_prediction_flag = SeqParams.sps_short_term_ref_pic_set[idx].rps_inter_ref_pic_set_prediction_flag;
    }
    else
        rps->inter_ref_pic_set_prediction_flag = 0;

    if (rps->inter_ref_pic_set_prediction_flag)
    {
        uint32_t delta_idx_minus1 = 0;

        if (idx == sps->getRPSList()->getNumberOfReferencePictureSets())
        {
            delta_idx_minus1 = SeqParams.sps_short_term_ref_pic_set[idx].rps_delta_idx_minus1;
        }

        if (delta_idx_minus1 > idx - 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        uint32_t rIdx = idx - 1 - delta_idx_minus1;

        if (rIdx > idx - 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        ReferencePictureSet*   rpsRef = const_cast<H265SeqParamSet *>(sps)->getRPSList()->getReferencePictureSet(rIdx);
        uint32_t num_pics = rpsRef->getNumberOfPictures();
        for(uint32_t j = 0 ;j <= num_pics; j++)
        {
            rps->m_DeltaPOC[j] = SeqParams.sps_short_term_ref_pic_set[idx].rps_delta_poc[j];
            rps->used_by_curr_pic_flag[j] = SeqParams.sps_short_term_ref_pic_set[idx].rps_is_used[j];
        }

        rps->num_pics = SeqParams.sps_short_term_ref_pic_set[idx].rps_num_total_pics;
        rps->num_negative_pics = SeqParams.sps_short_term_ref_pic_set[idx].rps_num_negative_pics;
        rps->num_positive_pics = SeqParams.sps_short_term_ref_pic_set[idx].rps_num_positive_pics;
    }
    else
    {
        rps->num_pics = SeqParams.sps_short_term_ref_pic_set[idx].rps_num_total_pics;
        rps->num_negative_pics = SeqParams.sps_short_term_ref_pic_set[idx].rps_num_negative_pics;
        rps->num_positive_pics = SeqParams.sps_short_term_ref_pic_set[idx].rps_num_positive_pics;

        if (rps->num_negative_pics >= MAX_NUM_REF_PICS || rps->num_positive_pics >= MAX_NUM_REF_PICS || (rps->num_positive_pics + rps->num_negative_pics) >= MAX_NUM_REF_PICS)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        for(uint32_t j = 0 ;j <= rps->num_pics; j++)
        {
            rps->m_DeltaPOC[j] = SeqParams.sps_short_term_ref_pic_set[idx].rps_delta_poc[j];
            rps->used_by_curr_pic_flag[j] = SeqParams.sps_short_term_ref_pic_set[idx].rps_is_used[j];
        }
    }
}

// Parse RPS part in slice header
void DecryptParametersWrapper::parseRefPicSet(ReferencePictureSet* rps)
{
    rps->num_negative_pics = 0;
    rps->num_positive_pics = 0;
    rps->num_pics = 0;
    rps->num_lt_pics = 0;

    int32_t index = 0, j;
    for(j = 0; j < RefFrames.ref_set_num_of_curr_before; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 1;
        rps->m_DeltaPOC[index] = RefFrames.ref_set_deltapoc_curr_before[j];
        rps->num_negative_pics++;
        rps->num_pics++;
    }
    for(j = 0; j < RefFrames.ref_set_num_of_curr_after; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 1;
        rps->m_DeltaPOC[index] = RefFrames.ref_set_deltapoc_curr_after[j];
        rps->num_positive_pics++;
        rps->num_pics++;
    }
    for(j = 0; j < RefFrames.ref_set_num_of_foll_st; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 0;
        rps->m_DeltaPOC[index] = RefFrames.ref_set_deltapoc_foll_st[j];
        if (rps->m_DeltaPOC[index] < 0)
            rps->num_negative_pics++;
        else
            rps->num_positive_pics++;
        rps->num_pics++;
    }

    rps->sortDeltaPOC();

    for(j = 0; j < RefFrames.ref_set_num_of_curr_lt; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 1;
        rps->m_DeltaPOC[index] = RefFrames.ref_set_deltapoc_curr_lt[j];
        rps->num_lt_pics++;
        rps->num_pics++;
    }
    for(j = 0; j < RefFrames.ref_set_num_of_foll_lt; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 0;
        rps->m_DeltaPOC[index] = RefFrames.ref_set_deltapoc_foll_lt[j];
        rps->num_lt_pics++;
        rps->num_pics++;
    }
}

void DecryptParametersWrapper::ParseSEIBufferingPeriod(const Headers & /*headers*/, H265SEIPayLoad * /*spl*/)
{
    //Currently MSDK does not need that
    return;
}

void DecryptParametersWrapper::ParseSEIPicTiming(const Headers & headers, H265SEIPayLoad *spl)
{
    spl->Reset();
    spl->payLoadType = SEI_PIC_TIMING_TYPE;

    int32_t seq_parameter_set_id = headers.m_SeqParams.GetCurrentID();;
    if (seq_parameter_set_id == -1)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    const H265SeqParamSet *csps = headers.m_SeqParams.GetHeader(seq_parameter_set_id);
    if (!csps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (csps->frame_field_info_present_flag)
    {
        spl->SEI_messages.pic_timing.pic_struct = (DisplayPictureStruct_H265)SeiPicTiming.sei_pt_pic_struct;
    }

    if (csps->m_hrdParameters.nal_hrd_parameters_present_flag || csps->m_hrdParameters.vcl_hrd_parameters_present_flag)
    {
        spl->SEI_messages.pic_timing.pic_dpb_output_delay = SeiPicTiming.sei_pt_pic_dpb_output_delay;

        if (csps->m_hrdParameters.sub_pic_hrd_params_present_flag && csps->m_hrdParameters.sub_pic_cpb_params_in_pic_timing_sei_flag)
        {
            uint32_t num_decoding_units_minus1 = SeiPicTiming.sei_pt_num_decoding_units_minus1;

            if (num_decoding_units_minus1 > csps->WidthInCU * csps->HeightInCU)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }
}

#ifdef UMC_VA_DXVA
UMC::Status WidevineDecrypter::DecryptFrame(UMC::MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    if(!pSource)
    {
        return UMC::UMC_OK;
    }

    IDirectXVideoDecoder *pDXVAVideoDecoder;
    m_va->GetVideoDecoder((void**)&pDXVAVideoDecoder);
    if(!pDXVAVideoDecoder)
        return UMC::UMC_ERR_NOT_INITIALIZED;

    IDirectXVideoDecoderService *pDecoderService;
    HRESULT hr = pDXVAVideoDecoder->GetVideoDecoderService(&pDecoderService);
    if (FAILED(hr) || (!pDecoderService))
    {
        return UMC::UMC_ERR_DEVICE_FAILED;
    }

    if(!m_bitstreamSubmitted && (pSource->GetDataSize() != 0))
    {
        if (!m_pDummySurface)
        {
            hr = pDecoderService->CreateSurface(640,
                                                480,
                                                0,
                                                D3DFMT_NV12,
                                                D3DPOOL_DEFAULT,
                                                0,
                                                DXVA2_VideoDecoderRenderTarget,
                                                &m_pDummySurface,
                                                NULL);
            if (FAILED(hr))
            {
                return UMC::UMC_ERR_ALLOC;
            }
        }

        hr = pDXVAVideoDecoder->BeginFrame(m_pDummySurface, NULL);
        if (FAILED(hr))
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        DXVA2_DecodeExtensionData DecryptInfo;
        DXVA2_DecodeExecuteParams ExecuteParams;
        DXVA2_DecodeBufferDesc BufferDesc;
        UINT DXVABitStreamBufferSize = 0;
        DECRYPT_QUERY_STATUS_PARAMS_HEVC DecryptStatus;
        memset(&DecryptStatus, 0xFF, sizeof(DECRYPT_QUERY_STATUS_PARAMS_HEVC));
        DECRYPT_QUERY_STATUS_PARAMS_HEVC* pDecryptStatusReturned = NULL;

        VADecryptInputBuffer PESInputParams;
        VADecryptSegmentInfo SegmentInfo;
        PESInputParams.pSegmentInfo = &SegmentInfo;

        PESInputParams.usStatusReportFeedbackNumber = m_PESPacketCounter++;
        PESInputParams.uiNumSegments = 0;

        PESInputParams.pSegmentInfo->uiSegmentStartOffset = 0;
        PESInputParams.pSegmentInfo->uiSegmentLength = 0;
        PESInputParams.pSegmentInfo->uiPartialAesBlockSizeInBytes = 0;
        PESInputParams.pSegmentInfo->uiInitByteLength = 0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[0] = 0x0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[1] = 0x0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[2] = 0x0;
        PESInputParams.pSegmentInfo->uiAesCbcIvOrCtr[3] = 0x0;

        PESInputParams.ucSizeOfLength = 0;
        PESInputParams.uiAppId = 0;

        //do the decrypt with the first execute call...
        DecryptInfo.Function = INTEL_DECODE_EXTENSION_DECRYPT_WIDEVINE_CLASSIC;
        DecryptInfo.pPrivateInputData = &PESInputParams;
        DecryptInfo.PrivateInputDataSize = sizeof(VADecryptInputBuffer);
        DecryptInfo.pPrivateOutputData = NULL;
        DecryptInfo.PrivateOutputDataSize = 0;

        BufferDesc.CompressedBufferType = DXVA2_BitStreamDateBufferType;
        BufferDesc.BufferIndex = 0;
        BufferDesc.DataOffset = 0;
        BufferDesc.DataSize = (UINT)pSource->GetDataSize();
        BufferDesc.FirstMBaddress = 0;
        BufferDesc.NumMBsInBuffer = 0;
        BufferDesc.Width = 0;
        BufferDesc.Height = 0;
        BufferDesc.Stride = 0;
        BufferDesc.ReservedBits = 0;
        BufferDesc.pvPVPState = NULL;

        void *pDXVA_BitStreamBuffer = NULL;
        hr = pDXVAVideoDecoder->GetBuffer(DXVA2_BitStreamDateBufferType, &pDXVA_BitStreamBuffer, &DXVABitStreamBufferSize);
        if (FAILED(hr))
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        if (DXVABitStreamBufferSize < pSource->GetDataSize())
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        mfxU8 *src = reinterpret_cast<mfxU8 *>(pSource->GetDataPointer());
        std::copy(src, src + pSource->GetDataSize(), reinterpret_cast<mfxU8 *>(pDXVA_BitStreamBuffer));

        hr = pDXVAVideoDecoder->ReleaseBuffer(DXVA2_BitStreamDateBufferType);
        if (FAILED(hr))
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        ExecuteParams.NumCompBuffers = 1;
        ExecuteParams.pCompressedBuffers = &BufferDesc;
        ExecuteParams.pExtensionData = &DecryptInfo;

        hr = pDXVAVideoDecoder->Execute(&ExecuteParams);
        if (FAILED(hr))
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        //get the decrypt status with the second execute call...
        DecryptStatus.ui16StatusReportFeebackNumber = PESInputParams.usStatusReportFeedbackNumber;

        DecryptInfo.Function = INTEL_DECODE_EXTENSION_GET_DECRYPT_STATUS;
        DecryptInfo.pPrivateInputData = NULL;
        DecryptInfo.PrivateInputDataSize = 0;
        DecryptInfo.pPrivateOutputData = &DecryptStatus;
        DecryptInfo.PrivateOutputDataSize = sizeof(DECRYPT_QUERY_STATUS_PARAMS_HEVC);

        ExecuteParams.NumCompBuffers = 0;
        ExecuteParams.pCompressedBuffers = NULL;
        ExecuteParams.pExtensionData = &DecryptInfo;

        do
        {
            hr = pDXVAVideoDecoder->Execute(&ExecuteParams);
            if (FAILED(hr))
            {
                return UMC::UMC_ERR_DEVICE_FAILED;
            }

            //cast the returned output data pointer to the DecryptStatus structure... the driver returns a newly allocated structure back!
            pDecryptStatusReturned = (DECRYPT_QUERY_STATUS_PARAMS_HEVC *) (ExecuteParams.pExtensionData->pPrivateOutputData);
        }
        while (pDecryptStatusReturned->uiStatus == VA_DECRYPT_STATUS_INCOMPLETE);

        if (pDecryptStatusReturned->uiStatus != VA_DECRYPT_STATUS_SUCCESSFUL)
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        if (pDecryptStatusReturned->ui16StatusReportFeebackNumber != PESInputParams.usStatusReportFeedbackNumber)
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        hr = pDXVAVideoDecoder->EndFrame(NULL);
        if (FAILED(hr))
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        *pDecryptParams = *pDecryptStatusReturned;

        pSource->MoveDataPointer((int32_t)pSource->GetDataSize());

        m_bitstreamSubmitted = true;
    }

    return UMC::UMC_OK;
}
#elif defined(UMC_VA_LINUX)
UMC::Status WidevineDecrypter::DecryptFrame(UMC::MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    (void)pDecryptParams;

    if(!pSource)
    {
        return UMC::UMC_OK;
    }

    /*if (!m_dpy)
    {
        m_va->GetVideoDecoder((void**)&m_dpy);
    }

    if (!m_context)
    {
        m_va->GetVideoDecoderContext((void*)&m_context);
    }

    size_t dataSize = pSource->GetDataSize();
    if (!m_bitstreamSubmitted && (dataSize != 0))
    {
        VAStatus   va_res = VA_STATUS_SUCCESS;

        VADecryptInputBuffer PESInputParams;
        VADecryptSegmentInfo SegmentInfo;
        PESInputParams.pSegmentInfo = &SegmentInfo;
        PESInputParams.usStatusReportFeedbackNumber = m_PESPacketCounter++;
        PESInputParams.dwNumSegments = 0;//1;                 //WA for unencrypted segments
        PESInputParams.pSegmentInfo->dwSegmentStartOffset = 0;
        PESInputParams.pSegmentInfo->dwSegmentLength = dataSize;
        PESInputParams.pSegmentInfo->dwPartialAesBlockSizeInBytes = 0;
        PESInputParams.pSegmentInfo->dwInitByteLength = 0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[0] = 0x0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[1] = 0x0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[2] = 0x0;
        PESInputParams.pSegmentInfo->dwAesCbcIvOrCtr[3] = 0x0;
        PESInputParams.uiSizeOfLength = 0;
        PESInputParams.uiAppId = 0;

        UMCVACompBuffer* compBuf = NULL;

        m_va->GetCompBuffer(VAProtectedSliceDataBufferType, &compBuf, dataSize);
        if (!compBuf)
            throw h264_exception(UMC::UMC_ERR_FAILED);

        memcpy(compBuf->GetPtr(), pSource->GetDataPointer(), dataSize);
        compBuf->SetDataSize(dataSize);

        compBuf = NULL;
        m_va->GetCompBuffer(VADecryptParameterBufferType, &compBuf, sizeof(VADecryptInputBuffer));
        if (!compBuf)
            throw h264_exception(UMC::UMC_ERR_FAILED);

        memcpy(compBuf->GetPtr(), &PESInputParams, sizeof(VADecryptInputBuffer));
        compBuf->SetDataSize(sizeof(VADecryptInputBuffer));

        Status sts = m_va->Execute();
        if (sts != UMC_OK)
            throw h264_exception(sts);

        sts = m_va->EndFrame();
        if (sts != UMC_OK)
            throw h264_exception(sts);

        DECRYPT_QUERY_STATUS_PARAMS_HEVC HevcParams;
        memset(&HevcParams, 0, sizeof(DECRYPT_QUERY_STATUS_PARAMS_HEVC));
        HevcParams.usStatusReportFeedbackNumber = PESInputParams.usStatusReportFeedbackNumber;
        HevcParams.status = VA_DECRYPT_STATUS_INCOMPLETE;

        IntelDecryptStatus DecryptStatus;
        DecryptStatus.decrypt_status_requested = (VASurfaceStatus)VADecryptStatusRequested;
        DecryptStatus.context = m_context;
        DecryptStatus.data_size = sizeof(DECRYPT_QUERY_STATUS_PARAMS_HEVC);
        DecryptStatus.data = &HevcParams;
        do
        {

            va_res = vaQuerySurfaceStatus(m_dpy, VA_INVALID_SURFACE, (VASurfaceStatus*)&DecryptStatus);
            if (VA_STATUS_SUCCESS != va_res)
                throw h264_exception(UMC_ERR_FAILED);
        }
        while (HevcParams.status == VA_DECRYPT_STATUS_INCOMPLETE);

        if (HevcParams.status != VA_DECRYPT_STATUS_SUCCESSFUL)
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        if (HevcParams.usStatusReportFeedbackNumber != PESInputParams.usStatusReportFeedbackNumber)
        {
            return UMC::UMC_ERR_DEVICE_FAILED;
        }

        *pDecryptParams = HevcParams;

        pSource->MoveDataPointer(dataSize);

        m_bitstreamSubmitted = true;
    }*/

    return UMC::UMC_OK;
}
#else
UMC::Status WidevineDecrypter::DecryptFrame(UMC::MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    (void)pSource;
    (void)pDecryptParams;

    return UMC::UMC_ERR_NOT_IMPLEMENTED;
}
#endif

} // namespace UMC_HEVC_DECODER

#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // UMC_ENABLE_H265_VIDEO_DECODER
