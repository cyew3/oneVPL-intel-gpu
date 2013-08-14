/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
//#define UMC_ENABLE_H265_VIDEO_DECODER
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "vm_debug.h"
#include "umc_h265_dec.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_dec_internal_cabac.h"
#include "umc_h265_slice_decoding.h"
#include "umc_h265_headers.h"
#include "h265_global_rom.h"

namespace UMC_HEVC_DECODER
{

#if INSTRUMENTED_CABAC
    Ipp32u H265Bitstream::m_c = 0;
    FILE* H265Bitstream::cabac_bits = 0;
#endif

H265BaseBitstream::H265BaseBitstream()
{
    Reset(0, 0);
}

H265BaseBitstream::H265BaseBitstream(Ipp8u * const pb, const Ipp32u maxsize)
{
    Reset(pb, maxsize);
}

H265BaseBitstream::~H265BaseBitstream()
{
}

void H265BaseBitstream::Reset(Ipp8u * const pb, const Ipp32u maxsize)
{
    m_pbs       = (Ipp32u*)pb;
    m_pbsBase   = (Ipp32u*)pb;
    m_bitOffset = 31;
    m_maxBsSize    = maxsize;

} // void Reset(Ipp8u * const pb, const Ipp32u maxsize)

void H265BaseBitstream::Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize)
{
    m_pbs       = (Ipp32u*)pb;
    m_pbsBase   = (Ipp32u*)pb;
    m_bitOffset = offset;
    m_maxBsSize = maxsize;

} // void Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize)

void H265BaseBitstream::ReadToByteAlignment()
{
    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    Ipp32u code;
    // get top bit, it can be "rbsp stop" bit
    ippiGetNBits(m_pbs, m_bitOffset, 1, code);
    VM_ASSERT(1 == code);

    // get remain bits, which is less then byte
    Ipp32u tmp = (m_bitOffset + 1) & 7;
    if (tmp)
        ippiSkipNBits(m_pbs, m_bitOffset, tmp);
}

bool H265BaseBitstream::More_RBSP_Data()
{
    Ipp32s code, tmp;
    Ipp32u* ptr_state = m_pbs;
    Ipp32s  bit_state = m_bitOffset;

    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    Ipp32s remaining_bytes = (Ipp32s)BytesLeft();

    if (remaining_bytes <= 0)
        return false;

    // get top bit, it can be "rbsp stop" bit
    ippiGetNBits(m_pbs, m_bitOffset, 1, code);

    // get remain bits, which is less then byte
    tmp = (m_bitOffset + 1) % 8;

    if(tmp)
    {
        ippiGetNBits(m_pbs, m_bitOffset, tmp, code);
        if ((code << (8 - tmp)) & 0x7f)    // most sig bit could be rbsp stop bit
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }
    }

    remaining_bytes = (Ipp32s)BytesLeft();

    // run through remain bytes
    while (0 < remaining_bytes)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);

        if (code)
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }

        remaining_bytes -= 1;
    }

    return false;
}

H265HeadersBitstream::H265HeadersBitstream()
    : H265BaseBitstream()
{
}

H265HeadersBitstream::H265HeadersBitstream(Ipp8u * const pb, const Ipp32u maxsize)
    : H265BaseBitstream(pb, maxsize)
{
}

/*
    bool getFlag(const char *);
    unsigned getValue(const char *);
    unsigned getValue(const char *, int bits);
*/

#define READ_CODE(bits, value, comment) \
    value = GetBits(bits); \
    //vm_debug_trace4(0, __T("%s (%d) = %u | %x"), __T(comment), bits, value, value);

#define READ_UVLC(value, comment) \
    value = (unsigned)GetVLCElement(false); \
    //vm_debug_trace3(0, __T("%s (u) = %u | %x"), __T(comment), value, value);

#define READ_SVLC(value, comment) \
    value = (int)GetVLCElement(true); \
    //vm_debug_trace3(0, __T("%s (s) = %d | %x"), __T(comment), value, value);

#define READ_FLAG(value, comment) \
    value = (Get1Bit() != 0); \
    //vm_debug_trace2(0, __T("%s (f) = %s"), __T(comment), value ? __T("true") : __T("false"));

void H265HeadersBitstream::parseHrdParameters(H265HRD *hrd, bool commonInfPresentFlag, unsigned maxNumSubLayersMinus1)
{
    unsigned uiCode;

    if( commonInfPresentFlag )
    {
        READ_FLAG( uiCode, "nal_hrd_parameters_present_flag" );           hrd->nal_hrd_parameters_present_flag = uiCode != 0;
        READ_FLAG( uiCode, "vcl_hrd_parameters_present_flag" );           hrd->vcl_hrd_parameters_present_flag = uiCode != 0;
        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag)
        {
            READ_FLAG( uiCode, "sub_pic_cpb_params_present_flag" );         hrd->sub_pic_hrd_params_present_flag = uiCode != 0;
            if (hrd->sub_pic_hrd_params_present_flag)
            {
                READ_CODE( 8, uiCode, "tick_divisor_minus2" );                hrd->tick_divisor = uiCode + 2;
                READ_CODE( 5, uiCode, "du_cpb_removal_delay_length_minus1" ); hrd->du_cpb_removal_delay_increment_length = uiCode + 1;
                READ_FLAG( uiCode, "sub_pic_cpb_params_in_pic_timing_sei_flag" ); hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = uiCode != 0;
                READ_CODE( 5, uiCode, "dpb_output_delay_du_length_minus1"  ); hrd->dpb_output_delay_du_length = uiCode + 1;
            }
            READ_CODE( 4, uiCode, "bit_rate_scale" );                       hrd->bit_rate_scale = uiCode;
            READ_CODE( 4, uiCode, "cpb_size_scale" );                       hrd->cpb_size_scale = uiCode;
            if (hrd->sub_pic_cpb_params_in_pic_timing_sei_flag)
            {
                READ_CODE( 4, uiCode, "cpb_size_du_scale" );                  hrd->cpb_size_du_scale = uiCode;
            }
            READ_CODE( 5, uiCode, "initial_cpb_removal_delay_length_minus1" ); hrd->initial_cpb_removal_delay_length = uiCode + 1;
            READ_CODE( 5, uiCode, "au_cpb_removal_delay_length_minus1" );      hrd->au_cpb_removal_delay_length = uiCode + 1;
            READ_CODE( 5, uiCode, "dpb_output_delay_length_minus1" );       hrd->dpb_output_delay_length = uiCode + 1;
        }
    }

    for (Ipp32u i = 0; i <= maxNumSubLayersMinus1; i ++ )
    {
        H265HrdSubLayerInfo * hrdSubLayerInfo = hrd->GetHRDSubLayerParam(i);
        READ_FLAG( uiCode, "fixed_pic_rate_general_flag" );  hrdSubLayerInfo->fixed_pic_rate_general_flag = uiCode != 0;
        if(!hrdSubLayerInfo->fixed_pic_rate_general_flag)
        {
            READ_FLAG( uiCode, "fixed_pic_rate_within_cvs_flag" ); hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = uiCode != 0;
        }
        else
        {
            hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = true;
        }

        // Infered to be 0 when not present
        hrdSubLayerInfo->low_delay_hrd_flag = 0;
        hrdSubLayerInfo->cpb_cnt = 0;

        if (hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag)
        {
            READ_UVLC(uiCode, "elemental_duration_in_tc_minus1" ); hrdSubLayerInfo->elemental_duration_in_tc = uiCode + 1;
        }
        else
        {
            READ_FLAG(uiCode, "low_delay_hrd_flag" );  hrdSubLayerInfo->low_delay_hrd_flag = uiCode != 0;
        }
        if (!hrdSubLayerInfo->low_delay_hrd_flag)
        {
            READ_UVLC( uiCode, "cpb_cnt_minus1" ); hrdSubLayerInfo->cpb_cnt = uiCode + 1;
        }

        for(unsigned nalOrVcl=0 ; nalOrVcl < 2 ; nalOrVcl++)
        {
            if((nalOrVcl == 0 && hrd->nal_hrd_parameters_present_flag) ||
               (nalOrVcl == 1 && hrd->vcl_hrd_parameters_present_flag))
            {
                for(unsigned j=0 ; j <= hrdSubLayerInfo->cpb_cnt; j++)
                {
                    READ_UVLC(uiCode, "bit_rate_value_minus1" ); hrdSubLayerInfo->bit_rate_value[j][nalOrVcl] = uiCode + 1;
                    READ_UVLC(uiCode, "cpb_size_value_minus1" ); hrdSubLayerInfo->cpb_size_value[j][nalOrVcl] = uiCode + 1;
                    if (hrd->sub_pic_hrd_params_present_flag)
                    {
                        READ_UVLC(uiCode, "bit_rate_du_value_minus1" );       hrdSubLayerInfo->bit_rate_du_value[j][nalOrVcl] = uiCode + 1;
                        READ_UVLC(uiCode, "cpb_size_du_value_minus1" );       hrdSubLayerInfo->cpb_size_du_value[j][nalOrVcl] = uiCode + 1;
                    }

                    READ_FLAG(uiCode, "cbr_flag" );                          hrdSubLayerInfo->cbr_flag[j][nalOrVcl] = uiCode != 0;
                }
            }
        }
    }
}

UMC::Status H265HeadersBitstream::GetVideoParamSet(H265VideoParamSet *pcVPS)
{
    Ipp32u uiCode;
    UMC::Status ps = UMC::UMC_OK;

    READ_CODE( 4, uiCode, "vps_video_parameter_set_id");    pcVPS->vps_video_parameter_set_id = uiCode;
    READ_CODE( 2, uiCode, "vps_reserved_three_2bits");      VM_ASSERT(uiCode == 3);
    READ_CODE( 6, uiCode, "vps_max_layers_minus1");         pcVPS->vps_max_layers = uiCode + 1;
    READ_CODE( 3, uiCode, "vps_max_sub_layers_minus1");     pcVPS->vps_max_sub_layers = uiCode + 1;
    READ_FLAG(    uiCode, "vps_temporal_id_nesting_flag");  pcVPS->vps_temporal_id_nesting_flag = (uiCode != 0);
    READ_CODE(16, uiCode, "vps_reserved_ffff_16bits");      VM_ASSERT(uiCode == 0xffff);
    VM_ASSERT(pcVPS->vps_max_sub_layers > 1 || pcVPS->vps_temporal_id_nesting_flag);
    parsePTL ( pcVPS->getPTL(), true, pcVPS->vps_max_sub_layers - 1);

    unsigned subLayerOrderingInfoPresentFlag;
    READ_FLAG(subLayerOrderingInfoPresentFlag, "vps_sub_layer_ordering_info_present_flag");
    for(unsigned i=0;i < pcVPS->vps_max_sub_layers;i++)
    {
        READ_UVLC(uiCode, "vps_max_dec_pic_buffering[]");   pcVPS->vps_max_dec_pic_buffering[i] = uiCode;
        READ_UVLC(uiCode, "vps_num_reorder_pics[i]");       pcVPS->vps_num_reorder_pics[i] = uiCode;
        READ_UVLC(uiCode, "vps_max_latency_increase[i]");   pcVPS->vps_max_latency_increase[i] = uiCode;

        if (!subLayerOrderingInfoPresentFlag)
        {
            for (i++; i < pcVPS->vps_max_sub_layers; i++)
            {
                pcVPS->vps_max_dec_pic_buffering[i] = pcVPS->vps_max_dec_pic_buffering[0];
                pcVPS->vps_num_reorder_pics[i] = pcVPS->vps_num_reorder_pics[0];
                pcVPS->vps_max_latency_increase[i] = pcVPS->vps_max_latency_increase[0];
            }

            break;
        }
    }

    READ_CODE(6, uiCode, "vps_max_layer_id" );   pcVPS->vps_max_layer_id = uiCode;
    VM_ASSERT(pcVPS->vps_max_layer_id < MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1 );
    READ_UVLC(uiCode, "vps_num_layer_sets_minus1" );   pcVPS->vps_num_layer_sets = uiCode + 1;
    for(unsigned opsIdx=1;opsIdx < pcVPS->vps_num_layer_sets;opsIdx++)
    {
        // Operation point set
        for(unsigned i=0 ; i <= pcVPS->vps_max_layer_id ; i++)
        {
            READ_FLAG( uiCode, "layer_id_included_flag[i][j]" );
            pcVPS->layer_id_included_flag[opsIdx][i] = (uiCode != 0);
        }
    }

    H265TimingInfo *timingInfo = pcVPS->getTimingInfo();
    READ_FLAG(uiCode, "vps_timing_info_present_flag"); timingInfo->vps_timing_info_present_flag = (uiCode != 0);
    if(timingInfo->vps_timing_info_present_flag)
    {
        READ_CODE(32, uiCode, "vps_num_units_in_tick");                 timingInfo->vps_num_units_in_tick = uiCode;
        READ_CODE(32, uiCode, "vps_time_scale");                        timingInfo->vps_time_scale = uiCode;
        READ_FLAG(    uiCode, "vps_poc_proportional_to_timing_flag");   timingInfo->vps_poc_proportional_to_timing_flag = (uiCode != 0);

        if(timingInfo->vps_poc_proportional_to_timing_flag)
        {
            READ_UVLC(uiCode, "vps_num_ticks_poc_diff_one_minus1");
            timingInfo->vps_num_ticks_poc_diff_one = uiCode + 1;
        }

        READ_UVLC(uiCode, "vps_num_hrd_parameters" );   pcVPS->vps_num_hrd_parameters = uiCode;
        if(pcVPS->vps_num_hrd_parameters > 0)
        {
            pcVPS->createHrdParamBuffer();

            for( unsigned i=0; i < pcVPS->vps_num_hrd_parameters; i++)
            {
                READ_UVLC( uiCode, "hrd_layer_set_idx" );  pcVPS->hrd_layer_set_idx[i] = uiCode;
                if(i > 0)
                {
                    READ_FLAG( uiCode, "cprms_present_flag[]" );   pcVPS->cprms_present_flag[i] = (uiCode != 1);
                }
                parseHrdParameters(pcVPS->getHrdParameters(i), pcVPS->cprms_present_flag[i], pcVPS->vps_max_sub_layers - 1);
            }
        }
    }

    READ_FLAG( uiCode,  "vps_extension_flag" );          VM_ASSERT(!uiCode);
    if (uiCode)
    {
        while ( xMoreRbspData() )
        {
            READ_FLAG( uiCode, "vps_extension_data_flag");
        }
    }

    return ps;
}

void H265HeadersBitstream::xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId)
{
  int i,coefNum = IPP_MIN(MAX_MATRIX_COEF_NUM,(int)g_scalingListSize[sizeId]);
  int data;
  int scalingListDcCoefMinus8 = 0;
  int nextCoef = SCALING_LIST_START_VALUE;
  const Ipp16u *scan  = (sizeId == 0) ? ScanTableDiag4x4 : g_sigLastScanCG32x32;
  int *dst = scalingList->getScalingListAddress(sizeId, listId);

  if( sizeId > SCALING_LIST_8x8 )
  {
    READ_SVLC( scalingListDcCoefMinus8, "scaling_list_dc_coef_minus8");
    scalingList->setScalingListDC(sizeId,listId,scalingListDcCoefMinus8 + 8);
    nextCoef = scalingList->getScalingListDC(sizeId,listId);
  }

  for(i = 0; i < coefNum; i++)
  {
    READ_SVLC( data, "scaling_list_delta_coef");
    nextCoef = (nextCoef + data + 256 ) % 256;
    dst[scan[i]] = nextCoef;
  }
}

/** get default address of quantization matrix
 * \param sizeId size index
 * \param listId list index
 * \returns pointer of quantization matrix
 */

int* H265ScalingList::getScalingListDefaultAddress(unsigned sizeId, unsigned listId)
{
    int *src = 0;
    switch(sizeId)
    {
    case SCALING_LIST_4x4:
        src = g_quantTSDefault4x4;
        break;
    case SCALING_LIST_8x8:
        src = (listId<3) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
        break;
    case SCALING_LIST_16x16:
        src = (listId<3) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
        break;
    case SCALING_LIST_32x32:
        src = (listId<1) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
        break;
    default:
        VM_ASSERT(0);
        src = NULL;
        break;
    }
    return src;
}

/** get scaling matrix from RefMatrixID
 * \param sizeId size index
 * \param Index of input matrix
 * \param Index of reference matrix
 */
void H265ScalingList::processRefMatrix(unsigned sizeId, unsigned listId , unsigned refListId)
{
  ::memcpy(
      getScalingListAddress(sizeId, listId),
      ((listId == refListId) ? getScalingListDefaultAddress(sizeId, refListId) : getScalingListAddress(sizeId, refListId)),
      sizeof(int)*IPP_MIN(MAX_MATRIX_COEF_NUM, (int)g_scalingListSize[sizeId]));
}


void H265HeadersBitstream::parseScalingList(H265ScalingList *scalingList)
{
    unsigned code, sizeId, listId;
    bool scalingListPredModeFlag;

    //for each size
    for(sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
        {
            READ_FLAG(code, "scaling_list_pred_mode_flag");
            scalingListPredModeFlag = (code != 0);
            if(!scalingListPredModeFlag) //Copy Mode
            {
                READ_UVLC( code, "scaling_list_pred_matrix_id_delta");
                scalingList->setRefMatrixId (sizeId,listId,(unsigned)((int)(listId)-(code)));
                if( sizeId > SCALING_LIST_8x8 )
                {
                    scalingList->setScalingListDC(sizeId,listId,((listId == scalingList->getRefMatrixId (sizeId,listId))? 16 :scalingList->getScalingListDC(sizeId, scalingList->getRefMatrixId (sizeId,listId))));
                }

                scalingList->processRefMatrix( sizeId, listId, scalingList->getRefMatrixId (sizeId,listId));
            }
            else //DPCM Mode
            {
                xDecodeScalingList(scalingList, sizeId, listId);
            }
        }
    }
}

void H265HeadersBitstream::parsePTL(H265ProfileTierLevel *rpcPTL, bool profilePresentFlag, int maxNumSubLayersMinus1 )
{
    Ipp32u uiCode;

    if(profilePresentFlag)
        parseProfileTier(rpcPTL->GetGeneralPTL());

    READ_CODE(8, uiCode, "PTL_level_idc" );    rpcPTL->GetGeneralPTL()->level_idc = uiCode;

    rpcPTL->sub_layer_profile_present_flags = 0;
    rpcPTL->sub_layer_level_present_flag = 0;
    for(int i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if(profilePresentFlag)
        {
            READ_FLAG(uiCode, "sub_layer_profile_present_flag[]");
            rpcPTL->sub_layer_profile_present_flags = (uiCode ? (1 << i) : 0);
        }
        READ_FLAG(uiCode, "sub_layer_level_present_flag[]");
        rpcPTL->sub_layer_level_present_flag = (uiCode ? (1 << i) : 0);
    }

    if (maxNumSubLayersMinus1 > 0)
    {
        for (int i = maxNumSubLayersMinus1; i < 8; i++)
        {
            READ_CODE(2, uiCode, "reserved_zero_2bits");
            VM_ASSERT(uiCode == 0);
        }
    }

    for(int i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if((rpcPTL->sub_layer_level_present_flag & (1 << i)) != 0)
        {
            if(profilePresentFlag)
                parseProfileTier(rpcPTL->GetSubLayerPTL(i));

            READ_CODE(8, uiCode, "sub_layer_level_idc[i]" );
            rpcPTL->GetSubLayerPTL(i)->level_idc = uiCode;
        }
    }
}

void H265HeadersBitstream::parseProfileTier(H265PTL *ptl)
{
    Ipp32u uiCode;

    READ_CODE(2, uiCode, "PTL_profile_space"); ptl->profile_space = uiCode;
    READ_FLAG(   uiCode, "PTL_tier_flag"    ); ptl->tier_flag = (uiCode != 0);
    READ_CODE(5, uiCode, "PTL_profile_idc"  ); ptl->profile_idc = uiCode;

    ptl->profile_compatibility_flags = 0;
    for(int j = 0; j < 32; j++)
    {
        READ_FLAG(  uiCode, "PTL_profile_compatibility_flag[]");
        ptl->profile_compatibility_flags = (uiCode ? (1 << j) : 0);
    }

    READ_FLAG(uiCode, "PTL_progressive_source_flag");       ptl->progressive_source_flag    = (uiCode != 0);
    READ_FLAG(uiCode, "PTL_interlaced_source_flag");        ptl->interlaced_source_flag     = (uiCode != 0);
    READ_FLAG(uiCode, "PTL_non_packed_constraint_flag");    ptl->non_packed_constraint_flag = (uiCode != 0);
    READ_FLAG(uiCode, "PTL_frame_only_constraint_flag");    ptl->frame_only_constraint_flag = (uiCode != 0);
    READ_CODE(16, uiCode, "XXX_reserved_zero_44bits[0..15]");
    READ_CODE(16, uiCode, "XXX_reserved_zero_44bits[16..31]");
    READ_CODE(12, uiCode, "XXX_reserved_zero_44bits[32..43]");
}

// ---------------------------------------------------------------------------
//  H265Bitstream::GetSequenceParamSet()
//    Read sequence parameter set data from bitstream.
// ---------------------------------------------------------------------------
UMC::Status H265HeadersBitstream::GetSequenceParamSet(H265SeqParamSet *pcSPS)
{
    Ipp32u  uiCode;
    UMC::Status ps = UMC::UMC_OK;

    READ_CODE( 4,  uiCode, "sps_video_parameter_set_id");   pcSPS->sps_video_parameter_set_id = uiCode;
    READ_CODE( 3,  uiCode, "sps_max_sub_layers_minus1" );   pcSPS->sps_max_sub_layers = uiCode + 1;
    READ_FLAG( uiCode, "sps_temporal_id_nesting_flag" );    pcSPS->sps_temporal_id_nesting_flag = (uiCode > 0);
    if( pcSPS->sps_max_sub_layers == 1 )
    {
        // sps_temporal_id_nesting_flag must be 1 when sps_max_sub_layers_minus1 is 0
        VM_ASSERT( uiCode == 1 );
    }

    parsePTL(pcSPS->getPTL(), 1, pcSPS->sps_max_sub_layers - 1);
    READ_UVLC(uiCode, "sps_seq_parameter_set_id" );               pcSPS->sps_seq_parameter_set_id = uiCode;
    READ_UVLC(uiCode, "chroma_format_idc" );                  pcSPS->chroma_format_idc = uiCode;
    if(pcSPS->chroma_format_idc == 3)
    {
        READ_FLAG(uiCode, "separate_colour_plane_flag");
        VM_ASSERT(uiCode == 0);
        pcSPS->separate_colour_plane_flag = uiCode;
    }

    READ_UVLC(uiCode, "pic_width_in_luma_samples" );    pcSPS->pic_width_in_luma_samples  = uiCode;
    READ_UVLC(uiCode, "pic_height_in_luma_samples" );   pcSPS->pic_height_in_luma_samples = uiCode;
    READ_FLAG( uiCode, "conformance_window_flag");      pcSPS->conformance_window_flag = uiCode != 0;
    if (pcSPS->conformance_window_flag)
    {
        READ_UVLC(uiCode, "conf_win_left_offset");      pcSPS->conf_win_left_offset = uiCode*H265SeqParamSet::getWinUnitX(pcSPS->chroma_format_idc);
        READ_UVLC(uiCode, "conf_win_right_offset");     pcSPS->conf_win_right_offset = uiCode*H265SeqParamSet::getWinUnitX(pcSPS->chroma_format_idc);
        READ_UVLC(uiCode, "conf_win_top_offset");       pcSPS->conf_win_top_offset = uiCode*H265SeqParamSet::getWinUnitY(pcSPS->chroma_format_idc);
        READ_UVLC(uiCode, "conf_win_bottom_offset");    pcSPS->conf_win_bottom_offset = uiCode*H265SeqParamSet::getWinUnitY(pcSPS->chroma_format_idc);
    }

    READ_UVLC(uiCode, "bit_depth_luma_minus8");
    pcSPS->bit_depth_luma = g_bitDepthY;
    pcSPS->setQpBDOffsetY((int) (6*uiCode));

    READ_UVLC(uiCode, "bit_depth_chroma_minus8");
    pcSPS->bit_depth_chroma = g_bitDepthC;
    pcSPS->setQpBDOffsetC( (int) (6*uiCode) );

    READ_UVLC(uiCode, "log2_max_pic_order_cnt_lsb_minus4");   pcSPS->log2_max_pic_order_cnt_lsb = 4 + uiCode;

    unsigned subLayerOrderingInfoPresentFlag;
    READ_FLAG(subLayerOrderingInfoPresentFlag, "sps_sub_layer_ordering_info_present_flag");

    for(unsigned i=0; i <= pcSPS->sps_max_sub_layers-1; i++)
    {
        READ_UVLC ( uiCode, "sps_max_dec_pic_buffering_minus1");
        pcSPS->sps_max_dec_pic_buffering[i] = uiCode + 1;
        READ_UVLC ( uiCode, "sps_num_reorder_pics" );
        pcSPS->sps_max_num_reorder_pics[i] = uiCode;
        READ_UVLC ( uiCode, "sps_max_latency_increase_plus1");
        pcSPS->sps_max_latency_increase[i] = uiCode - 1;

        if (!subLayerOrderingInfoPresentFlag)
        {
            for (i++; i <= pcSPS->sps_max_sub_layers-1; i++)
            {
                pcSPS->sps_max_dec_pic_buffering[i] = pcSPS->sps_max_dec_pic_buffering[0];
                pcSPS->sps_max_num_reorder_pics[i] = pcSPS->sps_max_num_reorder_pics[0];
                pcSPS->sps_max_latency_increase[i] = pcSPS->sps_max_latency_increase[0];
            }
            break;
        }
    }

    READ_UVLC( uiCode, "log2_min_coding_block_size_minus3");
    pcSPS->log2_min_luma_coding_block_size = uiCode + 3;
    READ_UVLC( uiCode, "log2_diff_max_min_coding_block_size");
    unsigned uiMaxCUDepthCorrect = uiCode;
    pcSPS->log2_max_luma_coding_block_size = uiMaxCUDepthCorrect + pcSPS->log2_min_luma_coding_block_size;
    pcSPS->MaxCUSize =  1<<pcSPS->log2_max_luma_coding_block_size;
    READ_UVLC( uiCode, "log2_min_transform_block_size_minus2");   pcSPS->log2_min_transform_block_size = uiCode + 2;

    READ_UVLC(uiCode, "log2_diff_max_min_transform_block_size"); pcSPS->log2_max_transform_block_size = uiCode + pcSPS->log2_min_transform_block_size;
    pcSPS->m_maxTrSize = 1 << pcSPS->log2_max_transform_block_size;

    READ_UVLC(uiCode, "max_transform_hierarchy_depth_inter");   pcSPS->max_transform_hierarchy_depth_inter = uiCode + 1;
    READ_UVLC(uiCode, "max_transform_hierarchy_depth_intra");   pcSPS->max_transform_hierarchy_depth_intra = uiCode + 1;
    
    Ipp32u addCUDepth = 0;
    while((pcSPS->MaxCUSize >> uiMaxCUDepthCorrect) > (unsigned)( 1 << ( pcSPS->log2_min_transform_block_size + addCUDepth)))
    {
        addCUDepth++;
    }
    pcSPS->AddCUDepth = addCUDepth;
    pcSPS->MaxCUDepth = uiMaxCUDepthCorrect + addCUDepth;
    // BB: these parameters may be removed completly and replaced by the fixed values
    READ_FLAG(uiCode, "scaling_list_enabled_flag"); pcSPS->scaling_list_enabled_flag = uiCode;
    if(pcSPS->scaling_list_enabled_flag)
    {
        READ_FLAG(uiCode, "sps_scaling_list_data_present_flag");    pcSPS->sps_scaling_list_data_present_flag = uiCode;
        if(pcSPS->sps_scaling_list_data_present_flag)
        {
            parseScalingList( pcSPS->getScalingList() );
        }
    }
    READ_FLAG(uiCode, "amp_enabled_flag");                      pcSPS->amp_enabled_flag = (uiCode != 0);
    READ_FLAG(uiCode, "sample_adaptive_offset_enabled_flag");   pcSPS->sample_adaptive_offset_enabled_flag = (uiCode != 0);
    READ_FLAG(uiCode, "pcm_enabled_flag");                      pcSPS->pcm_enabled_flag = (uiCode != 0);

    if(pcSPS->pcm_enabled_flag)
    {
        READ_CODE(4, uiCode, "pcm_sample_bit_depth_luma_minus1");           pcSPS->pcm_sample_bit_depth_luma = 1 + uiCode;
        READ_CODE(4, uiCode, "pcm_sample_bit_depth_chroma_minus1");         pcSPS->pcm_sample_bit_depth_chroma = 1 + uiCode;
        READ_UVLC(uiCode, "log2_min_pcm_luma_coding_block_size_minus3");    pcSPS->log2_min_pcm_luma_coding_block_size = uiCode + 3;
        READ_UVLC(uiCode, "log2_diff_max_min_pcm_luma_coding_block_size");  pcSPS->log2_max_pcm_luma_coding_block_size = uiCode + pcSPS->log2_min_pcm_luma_coding_block_size;
        READ_FLAG(uiCode, "pcm_loop_filter_disable_flag");                  pcSPS->pcm_loop_filter_disabled_flag = (uiCode != 0);
    }

    READ_UVLC( uiCode, "num_short_term_ref_pic_sets" );
    pcSPS->createRPSList(uiCode);

    ReferencePictureSetList* rpsList = pcSPS->getRPSList();
    ReferencePictureSet* rps;

    for(unsigned i=0; i< rpsList->getNumberOfReferencePictureSets(); i++)
    {
        rps = rpsList->getReferencePictureSet(i);
        parseShortTermRefPicSet(pcSPS, rps, i);
    }

    READ_FLAG(uiCode, "long_term_ref_pics_present_flag" );  pcSPS->long_term_ref_pics_present_flag = (uiCode != 0);
    if (pcSPS->long_term_ref_pics_present_flag)
    {
        READ_UVLC(uiCode, "num_long_term_ref_pic_sps" );
        pcSPS->num_long_term_ref_pic_sps = uiCode;

        VM_ASSERT(pcSPS->num_long_term_ref_pic_sps <= pcSPS->sps_max_dec_pic_buffering[pcSPS->sps_max_sub_layers - 1] - 1);

        for (unsigned k = 0; k < pcSPS->num_long_term_ref_pic_sps; k++)
        {
            READ_CODE(pcSPS->log2_max_pic_order_cnt_lsb, uiCode, "lt_ref_pic_poc_lsb_sps[]" );
            pcSPS->lt_ref_pic_poc_lsb_sps[k] = uiCode;

            READ_FLAG(uiCode,  "used_by_curr_pic_lt_sps_flag[]");
            pcSPS->used_by_curr_pic_lt_sps_flag[k] = (uiCode != 0);
        }
    }

    READ_FLAG(uiCode, "sps_temporal_mvp_enabled_flag" );            pcSPS->sps_temporal_mvp_enabled_flag = (uiCode != 0);
    READ_FLAG(uiCode, "sps_strong_intra_smoothing_enabled_flag" );  pcSPS->sps_strong_intra_smoothing_enabled_flag = (uiCode != 0);
    READ_FLAG(uiCode, "vui_parameters_present_flag" );             pcSPS->vui_parameters_present_flag = (uiCode != 0);

    if (pcSPS->vui_parameters_present_flag)
    {
        parseVUI(pcSPS);
    }

    READ_FLAG( uiCode, "sps_extension_flag");
    if (uiCode)
    {
        while ( xMoreRbspData() )
        {
            READ_FLAG( uiCode, "sps_extension_data_flag");
        }
    }

    return ps;
}    // GetSequenceParamSet

void H265HeadersBitstream::parseVUI(H265SeqParamSet *pcSPS)
{
    Ipp32u uiCode;

    READ_FLAG(uiCode, "aspect_ratio_info_present_flag");     pcSPS->aspect_ratio_info_present_flag = (uiCode != 0);
    if(pcSPS->aspect_ratio_info_present_flag)
    {
        READ_CODE(8, uiCode, "aspect_ratio_idc");           pcSPS->aspect_ratio_idc = uiCode;
        if (pcSPS->aspect_ratio_idc == 255)
        {
            READ_CODE(16, uiCode, "sar_width");             pcSPS->sar_width = uiCode;
            READ_CODE(16, uiCode, "sar_height");            pcSPS->sar_height = uiCode;
        }
        else
        {
            if (!pcSPS->aspect_ratio_idc || pcSPS->aspect_ratio_idc >= sizeof(SAspectRatio)/sizeof(SAspectRatio[0]))
            {
                pcSPS->aspect_ratio_info_present_flag = 0;
            }
            else
            {
                pcSPS->sar_width = SAspectRatio[pcSPS->aspect_ratio_idc][0];
                pcSPS->sar_height = SAspectRatio[pcSPS->aspect_ratio_idc][1];
            }
        }
    }

    READ_FLAG(uiCode, "overscan_info_present_flag");                pcSPS->overscan_info_present_flag = (uiCode != 0);
    if (pcSPS->overscan_info_present_flag)
    {
        READ_FLAG(uiCode, "overscan_appropriate_flag");             pcSPS->overscan_appropriate_flag = (uiCode != 0);
    }

    READ_FLAG(uiCode, "video_signal_type_present_flag");            pcSPS->video_signal_type_present_flag = (uiCode != 0);
    if (pcSPS->video_signal_type_present_flag)
    {
        READ_CODE(3, uiCode, "video_format");                       pcSPS->video_format = uiCode;
        READ_FLAG(   uiCode, "video_full_range_flag");              pcSPS->video_full_range_flag = (uiCode != 0);
        READ_FLAG(   uiCode, "colour_description_present_flag");    pcSPS->colour_description_present_flag = (uiCode != 0);
        if (pcSPS->colour_description_present_flag)
        {
            READ_CODE(8, uiCode, "colour_primaries");               pcSPS->colour_primaries = uiCode;
            READ_CODE(8, uiCode, "transfer_characteristics");       pcSPS->transfer_characteristics = uiCode;
            READ_CODE(8, uiCode, "matrix_coeffs");                  pcSPS->matrix_coeffs = uiCode;
        }
    }

    READ_FLAG(uiCode, "chroma_loc_info_present_flag");              pcSPS->chroma_loc_info_present_flag = (uiCode!= 0);
    if (pcSPS->chroma_loc_info_present_flag)
    {
        READ_UVLC(uiCode, "chroma_sample_loc_type_top_field" );     pcSPS->chroma_sample_loc_type_top_field = uiCode;
        READ_UVLC(uiCode, "chroma_sample_loc_type_bottom_field" );  pcSPS->chroma_sample_loc_type_bottom_field = uiCode;
    }

    READ_FLAG(uiCode, "neutral_chroma_indication_flag");            pcSPS->neutral_chroma_indication_flag = (uiCode != 0);
    READ_FLAG(uiCode, "field_seq_flag");                            pcSPS->field_seq_flag = (uiCode != 0);
    READ_FLAG(uiCode, "frame_field_info_present_flag");             pcSPS->frame_field_info_present_flag = (uiCode != 0);

    READ_FLAG(uiCode, "default_display_window_flag");
    if (uiCode != 0)
    {
        READ_UVLC(uiCode, "def_disp_win_left_offset");      pcSPS->def_disp_win_left_offset   = uiCode*H265SeqParamSet::getWinUnitX(pcSPS->chroma_format_idc);
        READ_UVLC(uiCode, "def_disp_win_right_offset");     pcSPS->def_disp_win_right_offset  = uiCode*H265SeqParamSet::getWinUnitX(pcSPS->chroma_format_idc);
        READ_UVLC(uiCode, "def_disp_win_top_offset");       pcSPS->def_disp_win_top_offset    = uiCode*H265SeqParamSet::getWinUnitY(pcSPS->chroma_format_idc);
        READ_UVLC(uiCode, "def_disp_win_bottom_offset");    pcSPS->def_disp_win_bottom_offset = uiCode*H265SeqParamSet::getWinUnitY(pcSPS->chroma_format_idc);
    }

    H265TimingInfo *timingInfo = pcSPS->getTimingInfo();
    READ_FLAG(uiCode, "vui_timing_info_present_flag");              timingInfo->vps_timing_info_present_flag = uiCode ? true : false;
    if (timingInfo->vps_timing_info_present_flag)
    {
        READ_CODE(32, uiCode, "vui_num_units_in_tick");             timingInfo->vps_num_units_in_tick = uiCode;
        READ_CODE(32, uiCode, "vui_time_scale");                    timingInfo->vps_time_scale = uiCode;

        READ_FLAG(uiCode, "vui_poc_proportional_to_timing_flag");   timingInfo->vps_poc_proportional_to_timing_flag = uiCode ? true : false;
        if (timingInfo->vps_poc_proportional_to_timing_flag)
        {
            READ_UVLC(uiCode, "vui_num_ticks_poc_diff_one_minus1"); timingInfo->vps_num_ticks_poc_diff_one = uiCode + 1;
        }

        READ_FLAG(uiCode, "hrd_parameters_present_flag");          pcSPS->vui_hrd_parameters_present_flag = (uiCode != 0);
        if (pcSPS->vui_hrd_parameters_present_flag)
        {
            parseHrdParameters( pcSPS->getHrdParameters(), 1, pcSPS->sps_max_sub_layers - 1);
        }
    }

    READ_FLAG(uiCode, "bitstream_restriction_flag");               pcSPS->bitstream_restriction_flag = (uiCode != 0);
    if (pcSPS->bitstream_restriction_flag)
    {
        READ_FLAG(   uiCode, "tiles_fixed_structure_flag");                 pcSPS->tiles_fixed_structure_flag = (uiCode != 0);
        READ_FLAG(   uiCode, "motion_vectors_over_pic_boundaries_flag");    pcSPS->motion_vectors_over_pic_boundaries_flag = (uiCode != 0);
        READ_CODE(8, uiCode, "min_spatial_segmentation_idc");               pcSPS->min_spatial_segmentation_idc = uiCode;
        READ_UVLC(   uiCode, "max_bytes_per_pic_denom");                    pcSPS->max_bytes_per_pic_denom = uiCode;
        READ_UVLC(   uiCode, "max_bits_per_min_cu_denom");                  pcSPS->max_bits_per_min_cu_denom = uiCode;
        READ_UVLC(   uiCode, "log2_max_mv_length_horizontal");              pcSPS->log2_max_mv_length_horizontal = uiCode;
        READ_UVLC(   uiCode, "log2_max_mv_length_vertical");                pcSPS->log2_max_mv_length_vertical = uiCode;
    }
}

bool H265HeadersBitstream::xMoreRbspData()
{
    return false;
}

#define SLICE_HEADER_EXTENSION  1

UMC::Status H265HeadersBitstream::GetPictureParamSetFull(H265PicParamSet  *pcPPS)
{
    int      iCode;
    unsigned uiCode;

    READ_UVLC( uiCode, "pps_pic_parameter_set_id");                 pcPPS->pps_pic_parameter_set_id = uiCode;
    READ_UVLC( uiCode, "pps_seq_parameter_set_id");                 pcPPS->pps_seq_parameter_set_id = uiCode;
    READ_FLAG( uiCode, "dependent_slice_segments_enabled_flag");    pcPPS->dependent_slice_segments_enabled_flag = uiCode != 0;
    READ_FLAG( uiCode, "output_flag_present_flag" );                pcPPS->output_flag_present_flag = uiCode==1;
    READ_CODE(3, uiCode, "num_extra_slice_header_bits");            pcPPS->num_extra_slice_header_bits = uiCode;
    READ_FLAG ( uiCode, "sign_data_hiding_flag" );                  pcPPS->sign_data_hiding_enabled_flag =  uiCode;
    READ_FLAG( uiCode,   "cabac_init_present_flag" );               pcPPS->cabac_init_present_flag =  uiCode ? true : false;
    READ_UVLC(uiCode, "num_ref_idx_l0_default_active_minus1");      pcPPS->num_ref_idx_l0_default_active = uiCode + 1; VM_ASSERT(uiCode <= 14);
    READ_UVLC(uiCode, "num_ref_idx_l1_default_active_minus1");      pcPPS->num_ref_idx_l1_default_active = uiCode + 1;   VM_ASSERT(uiCode <= 14);
    READ_SVLC(iCode, "init_qp_minus26" );                           pcPPS->init_qp = iCode + 26;
    READ_FLAG( uiCode, "constrained_intra_pred_flag" );             pcPPS->constrained_intra_pred_flag = uiCode != 0;
    READ_FLAG( uiCode, "transform_skip_enabled_flag" );             pcPPS->transform_skip_enabled_flag = uiCode != 0;

    READ_FLAG( uiCode, "cu_qp_delta_enabled_flag" );                pcPPS->cu_qp_delta_enabled_flag = ( uiCode != 0 );
    if( pcPPS->cu_qp_delta_enabled_flag )
    {
        READ_UVLC( uiCode, "diff_cu_qp_delta_depth" );
        pcPPS->diff_cu_qp_delta_depth = uiCode;
    }
    else
    {
        pcPPS->diff_cu_qp_delta_depth = 0;
    }
    READ_SVLC( iCode, "pps_cb_qp_offset");
    pcPPS->pps_cb_qp_offset = iCode;
    VM_ASSERT( pcPPS->pps_cb_qp_offset >= -12 );
    VM_ASSERT( pcPPS->pps_cb_qp_offset <=  12 );

    READ_SVLC( iCode, "pps_cr_qp_offset");
    pcPPS->pps_cr_qp_offset = iCode;
    VM_ASSERT( pcPPS->pps_cr_qp_offset >= -12 );
    VM_ASSERT( pcPPS->pps_cr_qp_offset <=  12 );

    READ_FLAG( uiCode, "pps_slice_chroma_qp_offsets_present_flag" );
    pcPPS->pps_slice_chroma_qp_offsets_present_flag = ( uiCode != 0 );

    READ_FLAG( uiCode, "weighted_pred_flag" );          // Use of Weighting Prediction (P_SLICE)
    pcPPS->weighted_pred_flag = uiCode==1;
    READ_FLAG( uiCode, "weighted_bipred_flag" );         // Use of Bi-Directional Weighting Prediction (B_SLICE)
    pcPPS->weighted_bipred_flag = uiCode==1;

    READ_FLAG( uiCode, "transquant_bypass_enable_flag");
    pcPPS->transquant_bypass_enabled_flag =uiCode ? true : false;
    READ_FLAG( uiCode, "tiles_enabled_flag"               );    pcPPS->tiles_enabled_flag = uiCode == 1;
    READ_FLAG( uiCode, "entropy_coding_sync_enabled_flag" );    pcPPS->entropy_coding_sync_enabled_flag = uiCode == 1;

    if( pcPPS->tiles_enabled_flag )
    {
        READ_UVLC(uiCode, "num_tile_columns_minus1");   pcPPS->num_tile_columns = uiCode + 1;
        READ_UVLC(uiCode, "num_tile_rows_minus1");      pcPPS->num_tile_rows= uiCode + 1;
        READ_FLAG(uiCode, "uniform_spacing_flag");      pcPPS->uniform_spacing_flag = (uiCode != 0);

        if( !pcPPS->uniform_spacing_flag)
        {
            // THIS IS DIFFERENT FROM REFERENCE !!!
            unsigned* columnWidth = (unsigned*)malloc((pcPPS->num_tile_columns - 1)*sizeof(unsigned));
            if (NULL == columnWidth)
            {
                pcPPS->Reset();
                return UMC::UMC_ERR_ALLOC;
            }

            for(unsigned i=0; i<pcPPS->num_tile_columns - 1; i++)
            {
                READ_UVLC( uiCode, "column_width_minus1" );
                columnWidth[i] = uiCode + 1;
            }
            pcPPS->setColumnWidth(columnWidth);
            free(columnWidth);
            if (NULL == pcPPS->column_width && pcPPS->num_tile_columns - 1 > 0)
            {
                pcPPS->Reset();
                return UMC::UMC_ERR_ALLOC;
            }

            unsigned* rowHeight = (unsigned*)malloc((pcPPS->num_tile_rows - 1)*sizeof(unsigned));
            if (NULL == rowHeight)
            {
                pcPPS->Reset();
                return UMC::UMC_ERR_ALLOC;
            }

            for(unsigned i=0; i < pcPPS->num_tile_rows - 1; i++)
            {
                READ_UVLC( uiCode, "row_height_minus1" );
                rowHeight[i] = uiCode + 1;
            }
            pcPPS->setRowHeight(rowHeight);
            free(rowHeight);
            if (NULL == pcPPS->row_height && pcPPS->num_tile_rows - 1 > 0)
            {
                pcPPS->Reset();
                return UMC::UMC_ERR_ALLOC;
            }
        }

        if(pcPPS->num_tile_columns - 1 != 0 || pcPPS->num_tile_rows - 1 != 0)
        {
            READ_FLAG ( uiCode, "loop_filter_across_tiles_enabled_flag" );   pcPPS->loop_filter_across_tiles_enabled_flag = (uiCode != 0);
        }
    }
    else
    {
        pcPPS->num_tile_columns = 1;
        pcPPS->num_tile_rows = 1;
    }

    READ_FLAG( uiCode, "loop_filter_across_slices_enabled_flag" );       pcPPS->pps_loop_filter_across_slices_enabled_flag = uiCode ? true : false;
    READ_FLAG( uiCode, "deblocking_filter_control_present_flag" );       pcPPS->deblocking_filter_control_present_flag = uiCode ? true : false;
    if (pcPPS->deblocking_filter_control_present_flag)
    {
        READ_FLAG( uiCode, "deblocking_filter_override_enabled_flag" );    pcPPS->deblocking_filter_override_enabled_flag = uiCode ? true : false;
        READ_FLAG( uiCode, "pps_disable_deblocking_filter_flag" );         pcPPS->pps_deblocking_filter_disabled_flag = uiCode ? true : false;
        if (!pcPPS->pps_deblocking_filter_disabled_flag)
        {
            READ_SVLC ( iCode, "pps_beta_offset_div2" );                     pcPPS->pps_beta_offset = iCode << 1;
            READ_SVLC ( iCode, "pps_tc_offset_div2" );                       pcPPS->pps_tc_offset = iCode << 1;
        }
    }
    READ_FLAG( uiCode, "pps_scaling_list_data_present_flag" );           pcPPS->pps_scaling_list_data_present_flag = uiCode ? true : false;
    if(pcPPS->pps_scaling_list_data_present_flag)
    {
        parseScalingList( pcPPS->getScalingList() );
    }

    READ_FLAG( uiCode, "lists_modification_present_flag");
    pcPPS->lists_modification_present_flag = uiCode;

    READ_UVLC( uiCode, "log2_parallel_merge_level_minus2");
    pcPPS->log2_parallel_merge_level = uiCode + 2;

    READ_FLAG( uiCode, "slice_segment_header_extension_present_flag");
    pcPPS->slice_segment_header_extension_present_flag = uiCode;

    READ_FLAG( uiCode, "pps_extension_flag");
    if (uiCode)
    {
        while ( xMoreRbspData() )
        {
            READ_FLAG( uiCode, "pps_extension_data_flag");
        }
    }

    return UMC::UMC_OK;
}   // H265HeadersBitstream::GetPictureParamSetFull

H265PicParamSet *ReferenceCodecAdaptor::getPrefetchedPPS(unsigned id)
{
    return m_headers->m_PicParams.GetHeader(static_cast<int>(id));
}

H265SeqParamSet *ReferenceCodecAdaptor::getPrefetchedSPS(unsigned id)
{
    return m_headers->m_SeqParams.GetHeader(static_cast<int>(id));
}

void H265HeadersBitstream::xParsePredWeightTable(H265Slice* pcSlice)
{
    wpScalingParam*   wp;
    bool              bChroma     = true; // color always present in HEVC ?
    SliceType     eSliceType      = pcSlice->getSliceType();
    int               iNbRef      = (eSliceType == B_SLICE ) ? (2) : (1);
    unsigned          uiLog2WeightDenomLuma = 0, uiLog2WeightDenomChroma = 0;
    unsigned          uiTotalSignalledWeightFlags = 0;

    int iDeltaDenom;
    // decode delta_luma_log2_weight_denom :
    READ_UVLC( uiLog2WeightDenomLuma, "luma_log2_weight_denom" );     // ue(v): luma_log2_weight_denom
    pcSlice->setLog2WeightDenomLuma(uiLog2WeightDenomLuma); // used in HW decoder
    if( bChroma )
    {
        READ_SVLC( iDeltaDenom, "delta_chroma_log2_weight_denom" );     // se(v): delta_chroma_log2_weight_denom
        VM_ASSERT((iDeltaDenom + (int)uiLog2WeightDenomLuma)>=0);
        uiLog2WeightDenomChroma = (unsigned)(iDeltaDenom + uiLog2WeightDenomLuma);
    }
    pcSlice->setLog2WeightDenomChroma(uiLog2WeightDenomChroma); // used in HW decoder

    for ( int iNumRef=0 ; iNumRef<iNbRef ; iNumRef++ )
    {
        EnumRefPicList  eRefPicList = ( iNumRef ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
        for ( int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ )
        {
            pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);

            wp[0].uiLog2WeightDenom = uiLog2WeightDenomLuma;
            wp[1].uiLog2WeightDenom = uiLog2WeightDenomChroma;
            wp[2].uiLog2WeightDenom = uiLog2WeightDenomChroma;

            unsigned  uiCode;
            READ_FLAG( uiCode, "luma_weight_lX_flag" );           // u(1): luma_weight_l0_flag
            wp[0].bPresentFlag = ( uiCode == 1 );
            uiTotalSignalledWeightFlags += wp[0].bPresentFlag;
        }
        if ( bChroma )
        {
            unsigned  uiCode;
            for ( int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ )
            {
                pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
                READ_FLAG( uiCode, "chroma_weight_lX_flag" );      // u(1): chroma_weight_l0_flag
                wp[1].bPresentFlag = ( uiCode == 1 );
                wp[2].bPresentFlag = ( uiCode == 1 );
                uiTotalSignalledWeightFlags += 2*wp[1].bPresentFlag;
            }
        }
        for ( int iRefIdx=0 ; iRefIdx<pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx++ )
        {
            pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);
            if ( wp[0].bPresentFlag )
            {
                READ_SVLC( wp[0].iDeltaWeight, "delta_luma_weight_lX" );  // se(v): delta_luma_weight_l0[i]
                wp[0].iWeight = (wp[0].iDeltaWeight + (1<<wp[0].uiLog2WeightDenom));
                READ_SVLC( wp[0].iOffset, "luma_offset_lX" );       // se(v): luma_offset_l0[i]
            }
            else
            {
                wp[0].iDeltaWeight = 0;
                wp[0].iWeight = (1 << wp[0].uiLog2WeightDenom);
                wp[0].iOffset = 0;
            }
            if ( bChroma )
            {
                if ( wp[1].bPresentFlag )
                {
                    for ( int j=1 ; j<3 ; j++ )
                    {
                        int iDeltaWeight;
                        READ_SVLC( iDeltaWeight, "delta_chroma_weight_lX" );  // se(v): chroma_weight_l0[i][j]
                        wp[j].iDeltaWeight = iDeltaWeight;
                        wp[j].iWeight = (iDeltaWeight + (1<<wp[1].uiLog2WeightDenom));

                        int iDeltaChroma;
                        READ_SVLC( iDeltaChroma, "delta_chroma_offset_lX" );  // se(v): delta_chroma_offset_l0[i][j]
                        int pred = ( 128 - ( ( 128*wp[j].iWeight)>>(wp[j].uiLog2WeightDenom) ) );
                        wp[j].iOffset = Clip3(-128, 127, (iDeltaChroma + pred) );
                    }
                }
                else
                {
                    for ( int j=1 ; j<3 ; j++ )
                    {
                        wp[j].iWeight = (1 << wp[j].uiLog2WeightDenom);
                        wp[j].iOffset = 0;
                    }
                }
            }
        }

        for ( int iRefIdx=pcSlice->getNumRefIdx(eRefPicList) ; iRefIdx<MAX_NUM_REF_PICS ; iRefIdx++ )
        {
            pcSlice->getWpScaling(eRefPicList, iRefIdx, wp);

            wp[0].bPresentFlag = false;
            wp[1].bPresentFlag = false;
            wp[2].bPresentFlag = false;
        }
    }
    VM_ASSERT(uiTotalSignalledWeightFlags<=24);

}


UMC::Status H265HeadersBitstream::GetSliceHeaderPart1(H265Slice *rpcSlice)
{
    unsigned uiCode;

    H265SliceHeader * sliceHdr = rpcSlice->GetSliceHeader();
    sliceHdr->IdrPicFlag = (NAL_UT_CODED_SLICE_IDR == sliceHdr->nal_unit_type) ? 1 : 0;

    unsigned firstSliceSegmentInPic;
    READ_FLAG( firstSliceSegmentInPic, "first_slice_in_pic_flag" );
    
    sliceHdr->first_slice_segment_in_pic_flag = firstSliceSegmentInPic;

    if ( rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_IDR
      || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_IDR_N_LP
      || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLA_N_LP
      || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLANT
      || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLA
      || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_CRA )
    {
        READ_FLAG( uiCode, "no_output_of_prior_pics_flag" );  //ignored
    }
    READ_UVLC (    uiCode, "slice_pic_parameter_set_id" );  sliceHdr->slice_pic_parameter_set_id = uiCode;
    return UMC::UMC_OK;
}

void H265HeadersBitstream::decodeSlice(H265Slice *rpcSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    Ipp32u uiCode;
    Ipp32s iCode;

    H265SliceHeader * sliceHdr = rpcSlice->GetSliceHeader();
    sliceHdr->collocated_from_l0_flag = 1;

    VM_ASSERT(pps!=0);
    VM_ASSERT(sps!=0);
    if( pps->dependent_slice_segments_enabled_flag && ( !sliceHdr->first_slice_segment_in_pic_flag ))
    {
        READ_FLAG( uiCode, "dependent_slice_segment_flag" );       sliceHdr->dependent_slice_segment_flag = uiCode ? true : false;
    }
    else
    {
        sliceHdr->dependent_slice_segment_flag = false;
    }
    int numCTUs = ((sps->pic_width_in_luma_samples + sps->MaxCUSize-1)/sps->MaxCUSize)*((sps->pic_height_in_luma_samples + sps->MaxCUSize-1)/sps->MaxCUSize);
    int maxParts = (1<<(sps->MaxCUDepth<<1));
    unsigned sliceSegmentAddress = 0;
    int bitsSliceSegmentAddress = 0;
    while(numCTUs>(1<<bitsSliceSegmentAddress))
    {
        bitsSliceSegmentAddress++;
    }

    if (!rpcSlice->GetSliceHeader()->first_slice_segment_in_pic_flag)
    {
        READ_CODE( bitsSliceSegmentAddress, sliceSegmentAddress, "slice_segment_address" );
    }
    //set uiCode to equal slice start address (or dependent slice start address)
    int startCuAddress = maxParts*sliceSegmentAddress;
    rpcSlice->setSliceSegmentCurStartCUAddr( startCuAddress );
    rpcSlice->setSliceSegmentCurEndCUAddr(numCTUs*maxParts);
    // DO NOT REMOVE THIS LINE !!!!!!!!!!!!!!!!!!!!!!!!!!
    sliceHdr->slice_segment_address = sliceSegmentAddress;

    if (!sliceHdr->dependent_slice_segment_flag)
    {
        rpcSlice->setSliceCurStartCUAddr(startCuAddress);
        rpcSlice->setSliceCurEndCUAddr(numCTUs*maxParts);
    }

    if(!sliceHdr->dependent_slice_segment_flag)
    {
        for (int i = 0; i < rpcSlice->getPPS()->num_extra_slice_header_bits; i++)
        {
            READ_FLAG(uiCode, "slice_reserved_undetermined_flag[]"); // ignored
        }

        READ_UVLC (    uiCode, "slice_type" );            rpcSlice->setSliceType((SliceType)uiCode);
    }

    if(!sliceHdr->dependent_slice_segment_flag)
    {
/*
        for (int i = 0; i < rpcSlice->getPPS()->num_extra_slice_header_bits; i++)
        {
            READ_FLAG(uiCode, "slice_reserved_undetermined_flag[]"); // ignored
        }

        READ_UVLC (    uiCode, "slice_type" );            rpcSlice->setSliceType((SliceType)uiCode);
*/
        if (pps->output_flag_present_flag)
        {
            READ_FLAG( uiCode, "pic_output_flag" );    sliceHdr->pic_output_flag = uiCode ? true : false;
        }
        else
        {
            sliceHdr->pic_output_flag = true;
        }
        // in the first version chroma_format_idc is equal to one, thus colour_plane_id will not be present
        VM_ASSERT (sps->chroma_format_idc == 1 );
        // if( separate_colour_plane_flag  ==  1 )
        //   colour_plane_id                                      u(2)

        if( rpcSlice->getIdrPicFlag() )
        {
            rpcSlice->setPOC(0);
            ReferencePictureSet* rps = rpcSlice->getLocalRPS();
            rps->setNumberOfNegativePictures(0);
            rps->setNumberOfPositivePictures(0);
            rps->setNumberOfLongtermPictures(0);
            rps->setNumberOfPictures(0);
            rpcSlice->setRPS(rps);
        }
        else
        {
            READ_CODE(sps->log2_max_pic_order_cnt_lsb, uiCode, "pic_order_cnt_lsb");
            int iPOClsb = uiCode;
            int iPrevPOC = rpcSlice->getPrevPOC();
            int iMaxPOClsb = 1<< sps->log2_max_pic_order_cnt_lsb;
            int iPrevPOClsb = iPrevPOC%iMaxPOClsb;
            int iPrevPOCmsb = iPrevPOC-iPrevPOClsb;
            int iPOCmsb;
            if( ( iPOClsb  <  iPrevPOClsb ) && ( ( iPrevPOClsb - iPOClsb )  >=  ( iMaxPOClsb / 2 ) ) )
            {
                iPOCmsb = iPrevPOCmsb + iMaxPOClsb;
            }
            else if( (iPOClsb  >  iPrevPOClsb )  && ( (iPOClsb - iPrevPOClsb )  >  ( iMaxPOClsb / 2 ) ) )
            {
                iPOCmsb = iPrevPOCmsb - iMaxPOClsb;
            }
            else
            {
                iPOCmsb = iPrevPOCmsb;
            }
            if ( rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLA
                || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLANT
                || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLA_N_LP )
            {
                // For BLA picture types, POCmsb is set to 0.
                iPOCmsb = 0;
            }
            rpcSlice->setPOC              (iPOCmsb+iPOClsb);

            ReferencePictureSet* rps;
            READ_FLAG( uiCode, "short_term_ref_pic_set_sps_flag" );
            if(uiCode == 0) // use short-term reference picture set explicitly signalled in slice header
            {
                int bitsCount = BitsDecoded();

                rps = rpcSlice->getLocalRPS();
                parseShortTermRefPicSet(sps,rps, sps->getRPSList()->getNumberOfReferencePictureSets());
                rpcSlice->setRPS(rps);

                rpcSlice->setNumBitsForShortTermRPSInSlice(BitsDecoded() - bitsCount);  // used in HW
            }
            else // use reference to short-term reference picture set in PPS
            {
                int numBits = 0;
                while ((unsigned)(1 << numBits) < rpcSlice->getSPS()->getRPSList()->getNumberOfReferencePictureSets())
                {
                    numBits++;
                }
                if (numBits > 0)
                {
                    READ_CODE( numBits, uiCode, "short_term_ref_pic_set_idx");        }
                else
                {
                    uiCode = 0;
                }
                rpcSlice->setRPS(sps->getRPSList()->getReferencePictureSet(uiCode));

                rps = rpcSlice->getRPS();
                rpcSlice->setNumBitsForShortTermRPSInSlice(0);    // used in HW
            }

            if (sps->long_term_ref_pics_present_flag)
            {
                int offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();
                unsigned numOfLtrp = 0;
                unsigned numLtrpInSPS = 0;
                if (rpcSlice->getSPS()->num_long_term_ref_pic_sps > 0)
                {
                    READ_UVLC( uiCode, "num_long_term_sps");
                    numLtrpInSPS = uiCode;
                    numOfLtrp += numLtrpInSPS;
                    rps->setNumberOfLongtermPictures(numOfLtrp);
                    VM_ASSERT(numOfLtrp <= sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1] - 1);
                }
                int bitsForLtrpInSPS = 0;
                while (rpcSlice->getSPS()->num_long_term_ref_pic_sps > (unsigned)(1 << bitsForLtrpInSPS))
                {
                    bitsForLtrpInSPS++;
                }
                READ_UVLC( uiCode, "num_long_term_pics");             rps->setNumberOfLongtermPictures(uiCode);
                numOfLtrp += uiCode;
                rps->setNumberOfLongtermPictures(numOfLtrp);
                int maxPicOrderCntLSB = 1 << rpcSlice->getSPS()->log2_max_pic_order_cnt_lsb;
                int prevDeltaMSB = 0, deltaPocMSBCycleLT = 0;;
                for(unsigned j=offset+rps->getNumberOfLongtermPictures()-1, k = 0; k < numOfLtrp; j--, k++)
                {
                    int pocLsbLt;
                    if (k < numLtrpInSPS)
                    {
                        uiCode = 0;
                        if (bitsForLtrpInSPS > 0)
                            READ_CODE(bitsForLtrpInSPS, uiCode, "lt_idx_sps[i]");
                        int usedByCurrFromSPS=rpcSlice->getSPS()->used_by_curr_pic_lt_sps_flag[uiCode];

                        pocLsbLt = rpcSlice->getSPS()->lt_ref_pic_poc_lsb_sps[uiCode];
                        rps->setUsed(j,usedByCurrFromSPS);
                    }
                    else
                    {
                        READ_CODE(rpcSlice->getSPS()->log2_max_pic_order_cnt_lsb, uiCode, "poc_lsb_lt"); pocLsbLt= uiCode;
                        READ_FLAG( uiCode, "used_by_curr_pic_lt_flag");     rps->setUsed(j,uiCode);
                    }
                    READ_FLAG(uiCode,"delta_poc_msb_present_flag");
                    bool mSBPresentFlag = uiCode ? true : false;
                    if(mSBPresentFlag)
                    {
                        READ_UVLC( uiCode, "delta_poc_msb_cycle_lt[i]" );
                        bool deltaFlag = false;
                        //            First LTRP                               || First LTRP from SH
                        if( (j == offset+rps->getNumberOfLongtermPictures()-1) || (j == offset+(numOfLtrp-numLtrpInSPS)-1))
                        {
                            deltaFlag = true;
                        }
                        if(deltaFlag)
                        {
                            deltaPocMSBCycleLT = uiCode;
                        }
                        else
                        {
                            deltaPocMSBCycleLT = uiCode + prevDeltaMSB;
                        }

                        int pocLTCurr = rpcSlice->getPOC() - deltaPocMSBCycleLT * maxPicOrderCntLSB
                            - iPOClsb + pocLsbLt;
                        rps->setPOC     (j, pocLTCurr);
                        rps->setDeltaPOC(j, - rpcSlice->getPOC() + pocLTCurr);
                        rps->setCheckLTMSBPresent(j,true);
                    }
                    else
                    {
                        rps->setPOC     (j, pocLsbLt);
                        rps->setDeltaPOC(j, - rpcSlice->getPOC() + pocLsbLt);
                        rps->setCheckLTMSBPresent(j,false);
                        // reset deltaPocMSBCycleLT for first LTRP from slice header if MSB not present
                        if (j == offset+(numOfLtrp-numLtrpInSPS)-1)
                          deltaPocMSBCycleLT = 0;
                    }
                    prevDeltaMSB = deltaPocMSBCycleLT;
                }
                offset += rps->getNumberOfLongtermPictures();
                rps->setNumberOfPictures(offset);
            }
            if ( rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLA
                || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLANT
                || rpcSlice->getNalUnitType() == NAL_UT_CODED_SLICE_BLA_N_LP )
            {
                // In the case of BLA picture types, rps data is read from slice header but ignored
                rps = rpcSlice->getLocalRPS();
                rps->setNumberOfNegativePictures(0);
                rps->setNumberOfPositivePictures(0);
                rps->setNumberOfLongtermPictures(0);
                rps->setNumberOfPictures(0);
                rpcSlice->setRPS(rps);
            }

            if (rpcSlice->getSPS()->sps_temporal_mvp_enabled_flag)
            {
                READ_FLAG( uiCode, "slice_enable_temporal_mvp_flag" );
                sliceHdr->slice_enable_temporal_mvp_flag = uiCode == 1 ? true : false;
            }
            else
            {
                sliceHdr->slice_enable_temporal_mvp_flag = false;
            }
        }

        if (sps->sample_adaptive_offset_enabled_flag)
        {
            READ_FLAG(uiCode, "slice_sao_luma_flag");  rpcSlice->GetSliceHeader()->slice_sao_luma_flag = (bool)uiCode;
            READ_FLAG(uiCode, "slice_sao_chroma_flag");  rpcSlice->GetSliceHeader()->slice_sao_chroma_flag = (bool)uiCode;
        }

        if (!rpcSlice->isIntra())
        {
            READ_FLAG( uiCode, "num_ref_idx_active_override_flag");
            if (uiCode)
            {
                READ_UVLC (uiCode, "num_ref_idx_l0_active_minus1" );  rpcSlice->setNumRefIdx( REF_PIC_LIST_0, uiCode + 1 );
                if (rpcSlice->isInterB())
                {
                    READ_UVLC (uiCode, "num_ref_idx_l1_active_minus1" );  rpcSlice->setNumRefIdx( REF_PIC_LIST_1, uiCode + 1 );
                }
                else
                {
                    rpcSlice->setNumRefIdx(REF_PIC_LIST_1, 0);
                }
            }
            else
            {
                rpcSlice->setNumRefIdx(REF_PIC_LIST_0, rpcSlice->getPPS()->num_ref_idx_l0_default_active);
                if (rpcSlice->isInterB())
                {
                    rpcSlice->setNumRefIdx(REF_PIC_LIST_1, rpcSlice->getPPS()->num_ref_idx_l1_default_active);
                }
                else
                {
                    rpcSlice->setNumRefIdx(REF_PIC_LIST_1,0);
                }
            }
        }
        // }
        RefPicListModification* refPicListModification = rpcSlice->getRefPicListModification();
        if(!rpcSlice->isIntra())
        {
            if( !rpcSlice->getPPS()->lists_modification_present_flag || rpcSlice->getNumRpsCurrTempList() <= 1 )
            {
                refPicListModification->setRefPicListModificationFlagL0( 0 );
            }
            else
            {
                READ_FLAG( uiCode, "ref_pic_list_modification_flag_l0" ); refPicListModification->setRefPicListModificationFlagL0( uiCode ? 1 : 0 );
            }

            if(refPicListModification->getRefPicListModificationFlagL0())
            {
                uiCode = 0;
                int i = 0;
                int numRpsCurrTempList0 = rpcSlice->getNumRpsCurrTempList();
                if ( numRpsCurrTempList0 > 1 )
                {
                    int length = 1;
                    numRpsCurrTempList0 --;
                    while ( numRpsCurrTempList0 >>= 1)
                    {
                        length ++;
                    }
                    for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_0); i ++)
                    {
                        READ_CODE( length, uiCode, "list_entry_l0" );
                        refPicListModification->setRefPicSetIdxL0(i, uiCode );
                    }
                }
                else
                {
                    for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_0); i ++)
                    {
                        refPicListModification->setRefPicSetIdxL0(i, 0 );
                    }
                }
            }
        }
        else
        {
            refPicListModification->setRefPicListModificationFlagL0(0);
        }
        if(rpcSlice->isInterB())
        {
            if( !rpcSlice->getPPS()->lists_modification_present_flag || rpcSlice->getNumRpsCurrTempList() <= 1 )
            {
                refPicListModification->setRefPicListModificationFlagL1( 0 );
            }
            else
            {
                READ_FLAG( uiCode, "ref_pic_list_modification_flag_l1" ); refPicListModification->setRefPicListModificationFlagL1( uiCode ? 1 : 0 );
            }
            if(refPicListModification->getRefPicListModificationFlagL1())
            {
                uiCode = 0;
                int i = 0;
                int numRpsCurrTempList1 = rpcSlice->getNumRpsCurrTempList();
                if ( numRpsCurrTempList1 > 1 )
                {
                    int length = 1;
                    numRpsCurrTempList1 --;
                    while ( numRpsCurrTempList1 >>= 1)
                    {
                        length ++;
                    }
                    for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_1); i ++)
                    {
                        READ_CODE( length, uiCode, "list_entry_l1" );
                        refPicListModification->setRefPicSetIdxL1(i, uiCode );
                    }
                }
                else
                {
                    for (i = 0; i < rpcSlice->getNumRefIdx(REF_PIC_LIST_1); i ++)
                    {
                        refPicListModification->setRefPicSetIdxL1(i, 0 );
                    }
                }
            }
        }
        else
        {
            refPicListModification->setRefPicListModificationFlagL1(0);
        }
        if (rpcSlice->isInterB())
        {
            READ_FLAG( uiCode, "mvd_l1_zero_flag" );       sliceHdr->mvd_l1_zero_flag = uiCode ? true : false;
        }

        sliceHdr->cabac_init_flag = false; // default
        if(pps->cabac_init_present_flag && !rpcSlice->isIntra())
        {
            READ_FLAG(uiCode, "cabac_init_flag");
            sliceHdr->cabac_init_flag = uiCode ? true : false;
        }

        if ( sliceHdr->slice_enable_temporal_mvp_flag )
        {
            if ( rpcSlice->getSliceType() == B_SLICE )
            {
                READ_FLAG( uiCode, "collocated_from_l0_flag" );
                rpcSlice->setColFromL0Flag(uiCode);
            }
            else
            {
                rpcSlice->setColFromL0Flag( 1 );
            }

            if ( rpcSlice->getSliceType() != I_SLICE &&
                ((rpcSlice->getColFromL0Flag()==1 && rpcSlice->getNumRefIdx(REF_PIC_LIST_0)>1)||
                (rpcSlice->getColFromL0Flag() ==0 && rpcSlice->getNumRefIdx(REF_PIC_LIST_1)>1)))
            {
                READ_UVLC( uiCode, "collocated_ref_idx" );
                rpcSlice->setColRefIdx(uiCode);
            }
            else
            {
                rpcSlice->setColRefIdx(0);
            }
        }
        if ( (pps->weighted_pred_flag && rpcSlice->getSliceType()==P_SLICE) || (pps->weighted_bipred_flag && rpcSlice->getSliceType()==B_SLICE) )
        {
            xParsePredWeightTable(rpcSlice);
            rpcSlice->initWpScaling();
        }
        if (!rpcSlice->isIntra())
        {
            READ_UVLC( uiCode, "five_minus_max_num_merge_cand");
            sliceHdr->max_num_merge_cand = MERGE_MAX_NUM_CAND - uiCode;
        }

        READ_SVLC( iCode, "slice_qp_delta" );
        sliceHdr->SliceQP = pps->init_qp + iCode;

        VM_ASSERT( sliceHdr->SliceQP >= -sps->getQpBDOffsetY() );
        VM_ASSERT( sliceHdr->SliceQP <=  51 );

        if (rpcSlice->getPPS()->pps_slice_chroma_qp_offsets_present_flag)
        {
            READ_SVLC( iCode, "slice_cb_qp_offset" );
            sliceHdr->slice_cb_qp_offset =  iCode;
            VM_ASSERT( sliceHdr->slice_cb_qp_offset >= -12 );
            VM_ASSERT( sliceHdr->slice_cb_qp_offset <=  12 );
            VM_ASSERT( (rpcSlice->getPPS()->pps_cb_qp_offset + sliceHdr->slice_cb_qp_offset) >= -12 );
            VM_ASSERT( (rpcSlice->getPPS()->pps_cb_qp_offset + sliceHdr->slice_cb_qp_offset) <=  12 );

            READ_SVLC( iCode, "slice_cr_qp_offset" );
            sliceHdr->slice_cr_qp_offset = iCode;
            VM_ASSERT( sliceHdr->slice_cr_qp_offset >= -12 );
            VM_ASSERT( sliceHdr->slice_cr_qp_offset <=  12 );
            VM_ASSERT( (rpcSlice->getPPS()->pps_cr_qp_offset + sliceHdr->slice_cr_qp_offset) >= -12 );
            VM_ASSERT( (rpcSlice->getPPS()->pps_cr_qp_offset + sliceHdr->slice_cr_qp_offset) <=  12 );
        }

        if (rpcSlice->getPPS()->deblocking_filter_control_present_flag)
        {
            if (rpcSlice->getPPS()->deblocking_filter_override_enabled_flag)
            {
                READ_FLAG ( uiCode, "deblocking_filter_override_flag" );        sliceHdr->deblocking_filter_override_flag = uiCode ? true : false;
            }
            else
            {
                sliceHdr->deblocking_filter_override_flag = 0;
            }
            if (sliceHdr->deblocking_filter_override_flag)
            {
                READ_FLAG ( uiCode, "slice_disable_deblocking_filter_flag" );   sliceHdr->slice_deblocking_filter_disabled_flag = uiCode != 0;
                if(!rpcSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag)
                {
                    READ_SVLC( iCode, "beta_offset_div2" );                       sliceHdr->slice_beta_offset =  iCode << 1;
                    READ_SVLC( iCode, "tc_offset_div2" );                         sliceHdr->slice_tc_offset = iCode << 1;
                }
            }
            else
            {
                sliceHdr->slice_deblocking_filter_disabled_flag =  rpcSlice->getPPS()->pps_deblocking_filter_disabled_flag;
                sliceHdr->slice_beta_offset = rpcSlice->getPPS()->pps_beta_offset;
                sliceHdr->slice_tc_offset = rpcSlice->getPPS()->pps_tc_offset;
            }
        }
        else
        {
            sliceHdr->slice_deblocking_filter_disabled_flag = false;
            sliceHdr->slice_beta_offset = 0;
            sliceHdr->slice_tc_offset = 0;
        }

        bool isSAOEnabled = sliceHdr->slice_sao_luma_flag || sliceHdr->slice_sao_chroma_flag;
        bool isDBFEnabled = !rpcSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag;

        if(rpcSlice->getPPS()->pps_loop_filter_across_slices_enabled_flag && ( isSAOEnabled || isDBFEnabled ))
        {
            READ_FLAG( uiCode, "slice_loop_filter_across_slices_enabled_flag");
        }
        else
        {
            uiCode = rpcSlice->getPPS()->pps_loop_filter_across_slices_enabled_flag?1:0;
        }
        sliceHdr->slice_loop_filter_across_slices_enabled_flag = uiCode == 1;

    }

    if (!pps->tiles_enabled_flag)
    {
        rpcSlice->setTileLocationCount(1);
        rpcSlice->setTileLocation(0, 0);
    }

    if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag)
    {
        unsigned *entryPointOffset          = NULL;
        int numEntryPointOffsets, offsetLenMinus1 = 0;

        READ_UVLC(numEntryPointOffsets, "num_entry_point_offsets"); sliceHdr->num_entry_point_offsets = numEntryPointOffsets;
        if (numEntryPointOffsets>0)
        {
            READ_UVLC(offsetLenMinus1, "offset_len_minus1");
        }
        entryPointOffset = new unsigned[numEntryPointOffsets];
        for (int idx=0; idx<numEntryPointOffsets; idx++)
        {
            READ_CODE(offsetLenMinus1+1, uiCode, "entry_point_offset_minus1");
            entryPointOffset[ idx ] = uiCode + 1;
        }

        if (pps->tiles_enabled_flag)
        {
            // THIS IS DIFFERENT FROM REFERENCE!!!
            rpcSlice->setTileLocationCount( numEntryPointOffsets + 1 );

            unsigned prevPos = 0;
            rpcSlice->setTileLocation( 0, 0 );
            for (unsigned idx=1; idx<rpcSlice->getTileLocationCount(); idx++)
            {
                rpcSlice->setTileLocation( idx, prevPos + entryPointOffset [ idx - 1 ] );
                prevPos += entryPointOffset[ idx - 1 ];
            }
        }
        else if (pps->entropy_coding_sync_enabled_flag)
        {
            int numSubstreams = sliceHdr->num_entry_point_offsets+1;
            rpcSlice->allocSubstreamSizes(numSubstreams);
            unsigned *pSubstreamSizes       = rpcSlice->getSubstreamSizes();
            for (int idx=0; idx<numSubstreams-1; idx++)
            {
                if ( idx < numEntryPointOffsets )
                {
                    pSubstreamSizes[ idx ] = ( entryPointOffset[ idx ] << 3 ) ;
                }
                else
                {
                    pSubstreamSizes[ idx ] = 0;
                }
            }
        }

        if (entryPointOffset)
        {
            delete [] entryPointOffset;
        }
    }
    else
    {
        sliceHdr->num_entry_point_offsets = 0;
    }

    if(pps->slice_segment_header_extension_present_flag)
    {
        READ_UVLC(uiCode,"slice_header_extension_length");
        for(unsigned i=0; i<uiCode; i++)
        {
            unsigned ignore;
            READ_CODE(8,ignore,"slice_header_extension_data_byte");
        }
    }
    readOutTrailingBits();
    return;
}

UMC::Status H265HeadersBitstream::GetSliceHeaderFull(H265Slice *rpcSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    UMC::Status sts = GetSliceHeaderPart1(rpcSlice);
    if (UMC::UMC_OK != sts)
        return sts;
    decodeSlice(rpcSlice, sps, pps);
    return UMC::UMC_OK;
}

static
const Ipp32u GetBitsMask[25] =
{
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff
};

void H265Bitstream::GetOrg(Ipp32u **pbs, Ipp32u *size)
{
    *pbs       = m_pbsBase;
    *size      = m_maxBsSize;
}

void H265Bitstream::GetState(Ipp32u** pbs,Ipp32u* bitOffset)
{
    *pbs       = m_pbs;
    *bitOffset = m_bitOffset;

} // H265Bitstream::GetState()

void H265Bitstream::SetState(Ipp32u* pbs,Ipp32u bitOffset)
{
    m_pbs = pbs;
    m_bitOffset = bitOffset;

} // H265Bitstream::GetState()

H265Bitstream::H265Bitstream(Ipp8u * const pb, const Ipp32u maxsize)
     : H265HeadersBitstream(pb, maxsize)
{
} // H265Bitstream::H265Bitstream(Ipp8u * const pb,

H265Bitstream::H265Bitstream()
    : H265HeadersBitstream()
{
#if INSTRUMENTED_CABAC
    if (!cabac_bits)
    {
        cabac_bits=fopen(__CABAC_FILE__,"w+t");
        m_c = 0;
    }
#endif
} // H265Bitstream::H265Bitstream(void)

H265Bitstream::~H265Bitstream()
{
#if INSTRUMENTED_CABAC
    fflush(cabac_bits);
#endif
} // H265Bitstream::~H265Bitstream()

void H265Bitstream::RollbackCurrentNALU()
{
    ippiUngetBits32(m_pbs,m_bitOffset);
}
// ---------------------------------------------------------------------------
//  H265Bitstream::GetSCP()
//  Determine if next bistream symbol is a start code. If not,
//  do not change bitstream position and return false. If yes, advance
//  bitstream position to first symbol following SCP and return true.
//
//  A start code is:
//  * next 2 bytes are zero
//  * next non-zero byte is '01'
//  * may include extra stuffing zero bytes
// ---------------------------------------------------------------------------
Ipp32s H265Bitstream::GetSCP()
{
    Ipp32s code, code1,tmp;
    Ipp32u* ptr_state = m_pbs;
    Ipp32s  bit_state = m_bitOffset;
    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);
    tmp = (m_bitOffset+1)%8;
    if(tmp)
    {
        ippiGetNBits(m_pbs, m_bitOffset, tmp ,code);
        if ((code << (8 - tmp)) & 0x7f)    // most sig bit could be rbsp stop bit
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            return 0;    // not rbsp if following non-zero bits
        }
    }
    else
    {
        Ipp32s remaining_bytes = (Ipp32s)m_maxBsSize - (Ipp32s) BytesDecoded();
        if (remaining_bytes<1)
            return -1; //signilizes end of buffer
        ippiNextBits(m_pbs, m_bitOffset,8, code);
        if (code == 0x80)
        {
            // skip past trailing RBSP stop bit
            ippiGetBits8(m_pbs, m_bitOffset, code);
        }
    }
    Ipp32s remaining_bytes = (Ipp32s)BytesLeft();
    if (remaining_bytes<1)
        return -1; //signilizes end of buffer
    //ippiNextBits(m_pbs, m_bitOffset,8, code);

    ippiGetBits8(m_pbs, m_bitOffset, code);
    if(code != 0) {
        m_pbs = ptr_state;
        m_bitOffset = bit_state;
        return 0;
    }
    if(remaining_bytes<2) {
        return(-1);
    }
    ippiGetBits8(m_pbs, m_bitOffset, code1);
    if (code1 != 0)
    {
        m_pbs = ptr_state;
        m_bitOffset = bit_state;
        return 0;
    }
    ippiGetBits8(m_pbs, m_bitOffset, code);
    Ipp32s max_search_length = (Ipp32s)BytesLeft();

    while (code == 0 && max_search_length-->0)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);

    }
    if (max_search_length<1)
        return -1;
    if (code != 1)
    {
        m_pbs = ptr_state;
        m_bitOffset = bit_state;
        return 0;
    }

    return 1;

} // H265Bitstream::GetSCP()

UMC::Status H265Bitstream::AdvanceToNextSCP()
{
    // Search bitstream for next start code:
    // 3 bytes:  0 0 1

    Ipp32s max_search_length = (Ipp32s)(m_maxBsSize - (Ipp32u)(((Ipp8u *) m_pbs) - ((Ipp8u *) m_pbsBase)));
    Ipp32u t1,t2,t3;
    Ipp32s p;

    if((m_bitOffset+1) % 8)
        AlignPointerRight();

    ippiGetBits8(m_pbs, m_bitOffset, t1);
    ippiGetBits8(m_pbs, m_bitOffset, t2);
    ippiGetBits8(m_pbs, m_bitOffset, t3);

    for (p = 0; p < max_search_length - 2; p++)
    {
        if (t1==0 && t2==0 && t3==1)
        {
            ippiUngetNBits(m_pbs,m_bitOffset,24);
            return UMC::UMC_OK;
        }
        t1=t2;
        t2=t3;
        ippiGetBits8(m_pbs, m_bitOffset, t3);

    }

    return UMC::UMC_ERR_INVALID_STREAM;

} // Status H265Bitstream::AdvanceToNextSCP()

// ---------------------------------------------------------------------------
//  H265Bitstream::GetNALUnitType()
//    Bitstream position is expected to be at the start of a NAL unit.
//    Read and return NAL unit type and NAL storage idc.
// ---------------------------------------------------------------------------
UMC::Status H265Bitstream::GetNALUnitType(NalUnitType &nal_unit_type, Ipp32u &nuh_temporal_id, Ipp32u &reserved_zero_6bits)
{
    Ipp32u bit = Get1Bit();
    VM_ASSERT(bit == 0); // forbidden_zero_bit
    // nal_unit_type
    nal_unit_type = (NalUnitType)GetBits(6);
    // reserved_zero_6bits
    reserved_zero_6bits = GetBits(6);
    // nuh_temporal_id
    nuh_temporal_id = GetBits(3) - 1;

    if (nuh_temporal_id)
    {
        VM_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_BLA
            && nal_unit_type != NAL_UT_CODED_SLICE_BLANT
            && nal_unit_type != NAL_UT_CODED_SLICE_BLA_N_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_IDR
            && nal_unit_type != NAL_UT_CODED_SLICE_IDR_N_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_CRA
            && nal_unit_type != NAL_UT_VPS
            && nal_unit_type != NAL_UT_SPS
            && nal_unit_type != NAL_UT_EOS
            && nal_unit_type != NAL_UT_EOB );
    }
    else
    {
        VM_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_TLA
            && nal_unit_type != NAL_UT_CODED_SLICE_TSA_N
            && nal_unit_type != NAL_UT_CODED_SLICE_STSA_R
            && nal_unit_type != NAL_UT_CODED_SLICE_STSA_N );
    }

    return UMC::UMC_OK;

} // Status H265Bitstream::GetNALUnitType(NAL_Unit_Type &nal_unit_type, Ipp32u &nuh_temporal_id, Ipp32u &reserved_zero_6bits)

// ---------------------------------------------------------------------------
//  H265Bitstream::GetAccessUnitDelimiter()
//    Read optional access unit delimiter from bitstream.
// ---------------------------------------------------------------------------
UMC::Status H265Bitstream::GetAccessUnitDelimiter(Ipp32u &PicCodType)
{
    PicCodType = GetBits(3);
    return UMC::UMC_OK;
}    // GetAccessUnitDelimiter

void H265Bitstream::SetDecodedBytes(size_t nBytes)
{
    m_pbs = m_pbsBase + (nBytes / 4);
    m_bitOffset = 31 - ((Ipp32s) ((nBytes % sizeof(Ipp32u)) * 8));
} // void H265Bitstream::SetDecodedBytes(size_t nBytes)

void H265Bitstream::parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* rps, Ipp32s idx)
{
    unsigned code;
    unsigned interRPSPred = 0;

    if (idx > 0)
    {
        READ_FLAG(interRPSPred, "inter_ref_pic_set_prediction_flag");  
    }
    rps->inter_ref_pic_set_prediction_flag = (interRPSPred != 0);

    if(rps->inter_ref_pic_set_prediction_flag)
    {
        unsigned bit;

        if(idx == sps->getRPSList()->getNumberOfReferencePictureSets())
        {
            READ_UVLC(code, "delta_idx_minus1" ); // delta index of the Reference Picture Set used for prediction minus 1
        }
        else
            code = 0;

    VM_ASSERT(code <= idx - 1); // delta_idx_minus1 shall not be larger than idx-1, otherwise we will predict from a negative row position that does not exist. When idx equals 0 there is no legal value and interRPSPred must be zero. See J0185-r2

    int rIdx =  idx - 1 - code;
    VM_ASSERT (rIdx <= idx - 1 && rIdx >= 0);
    ReferencePictureSet*   rpsRef = const_cast<H265SeqParamSet *>(sps)->getRPSList()->getReferencePictureSet(rIdx);
    int k = 0, k0 = 0, k1 = 0;
    READ_CODE(1, bit, "delta_rps_sign"); // delta_RPS_sign
    READ_UVLC(code, "abs_delta_rps_minus1");  // absolute delta RPS minus 1
    int deltaRPS = (1 - 2 * bit) * (code + 1); // delta_RPS
    for(int j=0 ; j <= rpsRef->getNumberOfPictures(); j++)
    {
      READ_CODE(1, bit, "used_by_curr_pic_flag" ); //first bit is "1" if Idc is 1
      int refIdc = bit;
      if (refIdc == 0)
      {
        READ_CODE(1, bit, "use_delta_flag" ); //second bit is "1" if Idc is 2, "0" otherwise.
        refIdc = bit<<1; //second bit is "1" if refIdc is 2, "0" if refIdc = 0.
      }
      if (refIdc == 1 || refIdc == 2)
      {
        int deltaPOC = deltaRPS + ((j < rpsRef->getNumberOfPictures())? rpsRef->getDeltaPOC(j) : 0);
        rps->setDeltaPOC(k, deltaPOC);
        rps->setUsed(k, (refIdc == 1));

        if (deltaPOC < 0)
        {
          k0++;
        }
        else
        {
          k1++;
        }
        k++;
      }
      rps->setRefIdc(j,refIdc);
    }
    rps->setNumRefIdc(rpsRef->getNumberOfPictures()+1);
    rps->setNumberOfPictures(k);
    rps->setNumberOfNegativePictures(k0);
    rps->setNumberOfPositivePictures(k1);
    rps->sortDeltaPOC();
  }
  else
  {
    READ_UVLC(code, "num_negative_pics");           rps->setNumberOfNegativePictures(code);
    READ_UVLC(code, "num_positive_pics");           rps->setNumberOfPositivePictures(code);

#if !RANDOM_ENC_TESTING
    VM_ASSERT(rps->getNumberOfNegativePictures() <= sps->m_MaxDecFrameBuffering[sps->sps_max_sub_layers - 1] - 1);
    VM_ASSERT(rps->getNumberOfPositivePictures() <= sps->m_MaxDecFrameBuffering[sps->sps_max_sub_layers - 1] - 1);
#endif

    int prev = 0;
    int poc;
    for(int j=0 ; j < rps->getNumberOfNegativePictures(); j++)
    {
      READ_UVLC(code, "delta_poc_s0_minus1");
      poc = prev-code-1;
      prev = poc;
      rps->setDeltaPOC(j,poc);
      READ_FLAG(code, "used_by_curr_pic_s0_flag");  rps->setUsed(j,code);
    }
    prev = 0;
    for(int j=rps->getNumberOfNegativePictures(); j < rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures(); j++)
    {
      READ_UVLC(code, "delta_poc_s1_minus1");
      poc = prev+code+1;
      prev = poc;
      rps->setDeltaPOC(j,poc);
      READ_FLAG(code, "used_by_curr_pic_s1_flag");  rps->setUsed(j,code);
    }
    rps->setNumberOfPictures(rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures());
  }
#if PRINT_RPS_INFO
  rps->printDeltaPOC();
#endif
}

//hevc CABAC from HM50
const
Ipp8u H265Bitstream::RenormTable
[32] =
{
  6,  5,  4,  4,
  3,  3,  3,  3,
  2,  2,  2,  2,
  2,  2,  2,  2,
  1,  1,  1,  1,
  1,  1,  1,  1,
  1,  1,  1,  1,
  1,  1,  1,  1
};

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
