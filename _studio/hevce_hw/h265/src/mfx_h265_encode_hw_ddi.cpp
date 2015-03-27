//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_h265_encode_hw_ddi.h"
#include "mfx_h265_encode_hw_d3d9.h"
#include "mfx_h265_encode_hw_d3d11.h"


namespace MfxHwH265Encode
{

GUID GetGUID(MfxVideoParam const & par)
{
    if (par.mfx.LowPower == MFX_CODINGOPTION_ON)
    {
        //if (par.mfx.FrameInfo.BitDepthLuma > 8)
        //    return DXVA2_Intel_LowpowerEncode_HEVC_Main10;
        return DXVA2_Intel_LowpowerEncode_HEVC_Main;
    }
    //
    //if (par.mfx.FrameInfo.BitDepthLuma > 8)
    //        return DXVA2_Intel_Encode_HEVC_Main10;

    return DXVA2_Intel_Encode_HEVC_Main;
}

DriverEncoder* CreatePlatformH265Encoder(MFXCoreInterface* core)
{
    if (core)
    {
        mfxCoreParam par = {}; 

        if (core->GetCoreParam(&par))
            return 0;

        switch(par.Impl & 0xF00)
        {
        case MFX_IMPL_VIA_D3D9:
            return new D3D9Encoder;
        case MFX_IMPL_VIA_D3D11:
            return new D3D11Encoder;
        default:
            return 0;
        }
    }

    return 0;
}

mfxStatus QueryHwCaps(MFXCoreInterface* core, GUID guid, ENCODE_CAPS_HEVC & caps)
{
    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH265Encoder(core));
    MFX_CHECK(ddi.get(), MFX_ERR_DEVICE_FAILED);

    mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(core, guid, 1920, 1088);
    MFX_CHECK_STS(sts);
            
    sts = ddi.get()->QueryEncodeCaps(caps);
    MFX_CHECK_STS(sts);

    DDITracer tracer;
    tracer.Trace(caps, 0);

    return sts;
}

mfxStatus CheckHeaders(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps)
{
    if (!( par.m_sps.sample_adaptive_offset_enabled_flag == 0
        && par.m_sps.pcm_enabled_flag == 0
        //&& par.m_sps.pcm_loop_filter_disabled_flag == 1
        && par.m_sps.log2_min_luma_coding_block_size_minus3 == 0
        && par.m_sps.log2_diff_max_min_luma_coding_block_size == 2
        && par.m_sps.bit_depth_luma_minus8 == 0
        && par.m_sps.bit_depth_chroma_minus8 == 0
        && par.m_sps.chroma_format_idc == 1
        && par.m_sps.separate_colour_plane_flag == 0
        && par.m_pps.cu_qp_delta_enabled_flag == 1))
        return MFX_ERR_UNSUPPORTED;

    if (   par.m_sps.pic_width_in_luma_samples > caps.MaxPicWidth
        || par.m_sps.pic_height_in_luma_samples > caps.MaxPicHeight
        || (UINT)(((par.m_pps.num_tile_columns_minus1 + 1) * (par.m_pps.num_tile_rows_minus1 + 1)) > 1) > caps.TileSupport)
        return MFX_ERR_UNSUPPORTED;

    return MFX_ERR_NONE;
}

void FillSpsBuffer(
    MfxVideoParam const & par, 
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps)
{
    Zero(sps);

    sps.log2_min_coding_block_size_minus3 = (mfxU8)par.m_sps.log2_min_luma_coding_block_size_minus3;

    sps.wFrameWidthInMinCbMinus1 = (mfxU16)(par.m_sps.pic_width_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.wFrameHeightInMinCbMinus1 = (mfxU16)(par.m_sps.pic_height_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.general_profile_idc = par.m_sps.general.profile_idc;
    sps.general_level_idc   = par.m_sps.general.level_idc;
    sps.general_tier_flag   = par.m_sps.general.tier_flag;

    sps.GopPicSize          = par.mfx.GopPicSize;
    sps.GopRefDist          = mfxU8(par.mfx.GopRefDist);
    sps.GopOptFlag          = mfxU8(par.mfx.GopOptFlag);

    sps.TargetUsage         = mfxU8(par.mfx.TargetUsage);
    sps.RateControlMethod   = mfxU8(par.mfx.RateControlMethod);

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        sps.MinBitRate      = par.TargetKbps;
        sps.TargetBitRate   = par.TargetKbps;
        sps.MaxBitRate      = par.MaxKbps;
    }

    sps.FramesPer100Sec = (mfxU16)(mfxU64(100) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
    sps.InitVBVBufferFullnessInBit = 8000 * par.InitialDelayInKB;
    sps.VBVBufferSizeInBit         = 8000 * par.BufferSizeInKB;

    sps.bResetBRC        = 0;
    sps.GlobalSearch     = 0;
    sps.LocalSearch      = 0;
    sps.EarlySkip        = 0;
    sps.MBBRC            = IsOn(par.m_ext.CO2.MBBRC);
    sps.ParallelBRC      = 0;
    sps.SliceSizeControl = 0;

    sps.UserMaxFrameSize = 0;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        sps.AVBRAccuracy    = par.mfx.Accuracy;
        sps.AVBRConvergence = par.mfx.Convergence;
    }
    
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        sps.CRFQualityFactor = (mfxU8)par.mfx.ICQQuality;
    else
        sps.CRFQualityFactor = 0;

    if (sps.ParallelBRC)
    {
        sps.NumOfBInGop[0]  = par.mfx.GopRefDist - 1;
        sps.NumOfBInGop[1]  = 0;
        sps.NumOfBInGop[2]  = 0;
    }

    sps.scaling_list_enable_flag           = par.m_sps.scaling_list_enabled_flag;
    sps.sps_temporal_mvp_enable_flag       = par.m_sps.temporal_mvp_enabled_flag;
    sps.strong_intra_smoothing_enable_flag = par.m_sps.strong_intra_smoothing_enabled_flag;
    sps.amp_enabled_flag                   = par.m_sps.amp_enabled_flag;
    sps.SAO_enabled_flag                   = par.m_sps.sample_adaptive_offset_enabled_flag;
    sps.pcm_enabled_flag                   = par.m_sps.pcm_enabled_flag;
    sps.pcm_loop_filter_disable_flag       = 1;//par.m_sps.pcm_loop_filter_disabled_flag;
    sps.tiles_fixed_structure_flag         = 0;
    sps.chroma_format_idc                  = par.m_sps.chroma_format_idc;
    sps.separate_colour_plane_flag         = par.m_sps.separate_colour_plane_flag;

    sps.log2_max_coding_block_size_minus3       = (mfxU8)(par.m_sps.log2_min_luma_coding_block_size_minus3
        + par.m_sps.log2_diff_max_min_luma_coding_block_size);
    sps.log2_min_coding_block_size_minus3       = (mfxU8)par.m_sps.log2_min_luma_coding_block_size_minus3;
    sps.log2_max_transform_block_size_minus2    = (mfxU8)(par.m_sps.log2_min_transform_block_size_minus2
        + par.m_sps.log2_diff_max_min_transform_block_size);
    sps.log2_min_transform_block_size_minus2    = (mfxU8)par.m_sps.log2_min_transform_block_size_minus2;
    sps.max_transform_hierarchy_depth_intra     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_intra;
    sps.max_transform_hierarchy_depth_inter     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_inter;
    sps.log2_min_PCM_cb_size_minus3             = (mfxU8)par.m_sps.log2_min_pcm_luma_coding_block_size_minus3;
    sps.log2_max_PCM_cb_size_minus3             = (mfxU8)(par.m_sps.log2_min_pcm_luma_coding_block_size_minus3
        + par.m_sps.log2_diff_max_min_pcm_luma_coding_block_size);
    sps.bit_depth_luma_minus8                   = (mfxU8)par.m_sps.bit_depth_luma_minus8;
    sps.bit_depth_chroma_minus8                 = (mfxU8)par.m_sps.bit_depth_chroma_minus8;
    sps.pcm_sample_bit_depth_luma_minus1        = (mfxU8)par.m_sps.pcm_sample_bit_depth_luma_minus1;
    sps.pcm_sample_bit_depth_chroma_minus1      = (mfxU8)par.m_sps.pcm_sample_bit_depth_chroma_minus1;
}

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    Zero(pps);

    pps.tiles_enabled_flag               = par.m_pps.tiles_enabled_flag;
    pps.entropy_coding_sync_enabled_flag = par.m_pps.entropy_coding_sync_enabled_flag;
    pps.sign_data_hiding_flag            = par.m_pps.sign_data_hiding_enabled_flag;
    pps.constrained_intra_pred_flag      = par.m_pps.constrained_intra_pred_flag;
    pps.transform_skip_enabled_flag      = par.m_pps.transform_skip_enabled_flag;
    pps.transquant_bypass_enabled_flag   = par.m_pps.transquant_bypass_enabled_flag;
    pps.cu_qp_delta_enabled_flag         = par.m_pps.cu_qp_delta_enabled_flag;
    pps.weighted_pred_flag               = par.m_pps.weighted_pred_flag;
    pps.weighted_bipred_flag             = par.m_pps.weighted_pred_flag;
    pps.bEnableGPUWeightedPrediction     = 0;
    
    pps.loop_filter_across_slices_flag        = par.m_pps.loop_filter_across_slices_enabled_flag;
    pps.loop_filter_across_tiles_flag         = par.m_pps.loop_filter_across_tiles_enabled_flag;
    pps.scaling_list_data_present_flag        = par.m_pps.scaling_list_data_present_flag;
    pps.dependent_slice_segments_enabled_flag = par.m_pps.dependent_slice_segments_enabled_flag;
    pps.bLastPicInSeq                         = 0;
    pps.bLastPicInStream                      = 0;
    pps.bUseRawPicForRef                      = 0;
    pps.bEmulationByteInsertion               = 0; //!!!
    pps.bEnableRollingIntraRefresh            = 0;
    pps.BRCPrecision                          = 0;
    pps.bScreenContent                        = 0;
    //pps.no_output_of_prior_pics_flag          = ;
    //pps.XbEnableRollingIntraRefreshX

    pps.QpY = (mfxU8)(par.m_pps.init_qp_minus26 + 26);

    pps.diff_cu_qp_delta_depth  = (mfxU8)par.m_pps.diff_cu_qp_delta_depth;
    pps.pps_cb_qp_offset        = (mfxU8)par.m_pps.cb_qp_offset;
    pps.pps_cr_qp_offset        = (mfxU8)par.m_pps.cr_qp_offset;
    pps.num_tile_columns_minus1 = (mfxU8)par.m_pps.num_tile_columns_minus1;
    pps.num_tile_rows_minus1    = (mfxU8)par.m_pps.num_tile_rows_minus1;

    Copy(pps.tile_column_width, par.m_pps.column_width);
    Copy(pps.tile_row_height, par.m_pps.row_height);
    
    pps.log2_parallel_merge_level_minus2 = (mfxU8)par.m_pps.log2_parallel_merge_level_minus2;
    
    pps.num_ref_idx_l0_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l0_default_active_minus1;
    pps.num_ref_idx_l1_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l1_default_active_minus1;

    pps.bEnableRollingIntraRefresh = 0;

    if (pps.bEnableRollingIntraRefresh)
    {
        pps.IntraInsertionLocation;
        pps.IntraInsertionSize;
        pps.QpDeltaForInsertedIntra;
    }

    pps.LcuMaxBitsizeAllowed       = 0;
    pps.bUseRawPicForRef           = 0;
    pps.bScreenContent             = 0;
}

void FillPpsBuffer(
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    pps.CurrOriginalPic.Index7Bits      = task.m_idxRec;
    pps.CurrOriginalPic.AssociatedFlag  = !!(task.m_frameType & MFX_FRAMETYPE_REF);

    pps.CurrReconstructedPic.Index7Bits     = task.m_idxRec;
    pps.CurrReconstructedPic.AssociatedFlag = !!task.m_ltr;

    if (task.m_sh.temporal_mvp_enabled_flag)
        pps.CollocatedRefPicIndex = task.m_refPicList[!task.m_sh.collocated_from_l0_flag][task.m_sh.collocated_ref_idx];
    else 
        pps.CollocatedRefPicIndex = IDX_INVALID;
    
    for (mfxU16 i = 0; i < 15; i ++)
    {
        pps.RefFrameList[i].bPicEntry      = task.m_dpb[0][i].m_idxRec;
        pps.RefFrameList[i].AssociatedFlag = !!task.m_dpb[0][i].m_ltr;
        pps.RefFramePOCList[i] = task.m_dpb[0][i].m_poc;
    }

    pps.CodingType      = task.m_codingType;
    pps.NumSlices       = 1;
    pps.CurrPicOrderCnt = task.m_poc;

    pps.StatusReportFeedbackNumber = task.m_statusReportNumber;
}

mfxU16 CodingTypeToSliceType(mfxU16 ct)
{
    switch (ct)
    {
     case CODING_TYPE_I : return 2;
     case CODING_TYPE_P : return 1;
     case CODING_TYPE_B :
     case CODING_TYPE_B1:
     case CODING_TYPE_B2: return 0;
    default: assert(!"invalid coding type"); return 0xFFFF;
    }
}


void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{   
    slice.resize(par.m_slice.size());

    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];
        Zero(cs);

        cs.slice_id              = i;
        cs.slice_segment_address = par.m_slice[i].SegmentAddress;
        cs.NumLCUsInSlice        = par.m_slice[i].NumLCU;
        cs.bLastSliceOfPic       = (i == slice.size() - 1);
    }
}

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];

        cs.slice_type                           = task.m_sh.type;
        if (cs.slice_type != 2)
        {
            Copy(cs.RefPicList, task.m_refPicList);
            cs.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

            if (cs.slice_type == 0)
                cs.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;
        }

        cs.bLastSliceOfPic                      = (i == slice.size() - 1);
        cs.dependent_slice_segment_flag         = task.m_sh.dependent_slice_segment_flag;
        cs.slice_temporal_mvp_enable_flag       = task.m_sh.temporal_mvp_enabled_flag;
        cs.slice_sao_luma_flag                  = task.m_sh.sao_luma_flag;
        cs.slice_sao_chroma_flag                = task.m_sh.sao_chroma_flag;
        cs.mvd_l1_zero_flag                     = task.m_sh.mvd_l1_zero_flag;
        cs.cabac_init_flag                      = task.m_sh.cabac_init_flag;
        cs.slice_deblocking_filter_disable_flag = task.m_sh.deblocking_filter_disabled_flag;
        cs.collocated_from_l0_flag              = task.m_sh.collocated_from_l0_flag;

        cs.slice_qp_delta       = task.m_sh.slice_qp_delta;
        cs.slice_cb_qp_offset   = task.m_sh.slice_cr_qp_offset;
        cs.slice_cr_qp_offset   = task.m_sh.slice_cb_qp_offset;
        cs.beta_offset_div2     = task.m_sh.beta_offset_div2;
        cs.tc_offset_div2       = task.m_sh.tc_offset_div2;

        //if (pps.weighted_pred_flag || pps.weighted_bipred_flag)
        //{
        //    cs.luma_log2_weight_denom;
        //    cs.chroma_log2_weight_denom;
        //    cs.luma_offset[2][15];
        //    cs.delta_luma_weight[2][15];
        //    cs.chroma_offset[2][15][2];
        //    cs.delta_chroma_weight[2][15][2];
        //}

        cs.MaxNumMergeCand = 5 - task.m_sh.five_minus_max_num_merge_cand;
    }
}

void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init = {};
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

mfxStatus CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        if (update[i].bStatus != ENCODE_NOTAVAILABLE)
        {
            Feedback* cache = 0;

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                if (m_cache[j].StatusReportFeedbackNumber == update[i].StatusReportFeedbackNumber)
                {
                    cache = &m_cache[j];
                    break;
                }
                else if (cache == 0 && m_cache[j].bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = &m_cache[j];
                }
            }
            MFX_CHECK( cache != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            *cache = update[i];
        }
    }
    return MFX_ERR_NONE;
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

mfxStatus CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

DDIHeaderPacker::DDIHeaderPacker()
{
}

DDIHeaderPacker::~DDIHeaderPacker()
{
}

void DDIHeaderPacker::Reset(MfxVideoParam const & par)
{
    m_buf.resize(5 + par.mfx.NumSlice);
    m_cur = m_buf.begin();
    m_packer.Reset(par);
}

void DDIHeaderPacker::NewHeader()
{
    assert(m_buf.size());

    if (++m_cur == m_buf.end())
        m_cur = m_buf.begin();

    Zero(*m_cur);
}

ENCODE_PACKEDHEADER_DATA* DDIHeaderPacker::PackHeader(Task const & task, mfxU32 nut)
{
    NewHeader();

    switch(nut)
    {
    case VPS_NUT:
        m_packer.GetVPS(m_cur->pData, m_cur->DataLength);
        break;
    case SPS_NUT:
        m_packer.GetSPS(m_cur->pData, m_cur->DataLength);
        break;
    case PPS_NUT:
        m_packer.GetPPS(m_cur->pData, m_cur->DataLength);
        break;
    case AUD_NUT:
        {
            mfxU32 frameType = task.m_frameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B);

            if (frameType == MFX_FRAMETYPE_I)
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 0);
            else if (frameType == MFX_FRAMETYPE_P)
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 1);
            else
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 2);
        }
        break;
    case PREFIX_SEI_NUT:
        m_packer.GetSEI(task, m_cur->pData, m_cur->DataLength);
        break;
    default:
        return 0;
    }
    m_cur->BufferSize = m_cur->DataLength;
    m_cur->SkipEmulationByteCount = 4;

    return &*m_cur;
}

ENCODE_PACKEDHEADER_DATA* DDIHeaderPacker::PackSliceHeader(Task const & task, mfxU32 id, mfxU32* qpd_offset)
{
    bool is1stNALU = (id == 0 && task.m_insertHeaders == 0);
    NewHeader();

    m_packer.GetSSH(task, id, m_cur->pData, m_cur->DataLength, qpd_offset);
    m_cur->BufferSize = m_cur->DataLength;
    m_cur->SkipEmulationByteCount = 3 + is1stNALU;
    m_cur->DataLength *= 8;

    return &*m_cur;
}

}; // namespace MfxHwH265Encode
