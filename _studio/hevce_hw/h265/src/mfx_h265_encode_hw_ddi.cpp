//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//
#include "mfx_config.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_common.h"

#include "mfx_h265_encode_hw_ddi.h"
#if defined (MFX_VA_WIN)
#include "mfx_h265_encode_hw_d3d9.h"
#include "mfx_h265_encode_hw_d3d11.h"
#elif defined (MFX_VA_LINUX)
#include "mfx_h265_encode_vaapi.h"
#endif
#include "mfx_h265_encode_hw_ddi_trace.h"
#include "ipps.h"

namespace MfxHwH265Encode
{

GUID GetGUID(MfxVideoParam const & par)
{
    GUID guid = DXVA2_Intel_Encode_HEVC_Main;

#ifndef OPEN_SOURCE
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    bool is10bit =
        (   par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10
         || par.m_ext.CO3.TargetBitDepthLuma == 10);
    mfxU16 cfId = Clip3<mfxU16>(MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV444, par.m_ext.CO3.TargetChromaFormatPlus1 - 1) - MFX_CHROMAFORMAT_YUV420;

    if (par.m_platform.CodeName && par.m_platform.CodeName < MFX_PLATFORM_ICELAKE)
        cfId = 0; // platforms below ICL do not support Main422/Main444 profile, using Main instead.
#else
    bool is10bit =
        (   par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10
         || par.mfx.FrameInfo.BitDepthLuma == 10
         || par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010);
    mfxU16 cfId = 0;
#endif
    if (par.m_platform.CodeName && par.m_platform.CodeName < MFX_PLATFORM_KABYLAKE)
        is10bit = false;

    guid = GuidTable[IsOn(par.mfx.LowPower)][is10bit] [cfId];
    DDITracer::TraceGUID(guid, stdout);
#endif // OPEN_SOURCE
    return guid;
}

DriverEncoder* CreatePlatformH265Encoder(MFXCoreInterface* core, ENCODER_TYPE type)
{
    type;

    if (core)
    {
        mfxCoreParam par = {};

        if (core->GetCoreParam(&par))
            return 0;

        switch(par.Impl & 0xF00)
        {
#if defined (MFX_VA_WIN)
        case MFX_IMPL_VIA_D3D9:
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            if (type == ENCODER_REXT)
                return new D3D9EncoderREXT;
#endif
            return new D3D9EncoderDefault;
        case MFX_IMPL_VIA_D3D11:
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            if (type == ENCODER_REXT)
                return new D3D11EncoderREXT;
#endif
            return new D3D11EncoderDefault;
#elif defined (MFX_VA_LINUX)
        case MFX_IMPL_VIA_VAAPI:
            return new VAAPIEncoder;
#endif
        default:
            return 0;
        }
    }

    return 0;
}

#ifndef OPEN_SOURCE
bool IsEncPak(GUID guid)
{
    // search among ENC+PAK guids GuidTable[0]

    for (mfxU32 i = 0; i < 2; i++) {   // 8/10 bit
        for (mfxU32 j = 0; j < sizeof(GuidTable[0][i]) / sizeof(GuidTable[0][0][0]); j++) {
            if (guid == GuidTable[0][i][j])
                return true;
        }
    }
    return false;
}
#endif

// this function is aimed to workaround all CAPS reporting problems in mainline driver
mfxStatus HardcodeCaps(ENCODE_CAPS_HEVC& caps, MFXCoreInterface* core, GUID guid)
{
    mfxStatus sts = MFX_ERR_NONE;
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    if (!caps.BitDepth8Only && !caps.MaxEncodedBitDepth)
        caps.MaxEncodedBitDepth = 1;
    if (!caps.Color420Only && !(caps.YUV444ReconSupport || caps.YUV422ReconSupport))
        caps.YUV444ReconSupport = 1;
    if (!caps.Color420Only && !(caps.YUV422ReconSupport))   // VPG: caps are not correct now
        caps.YUV422ReconSupport = 1;
#endif
#if defined(PRE_SI_TARGET_PLATFORM_GEN10)
    mfxPlatform pltfm;
    sts = core->QueryPlatform(&pltfm);
    MFX_CHECK_STS(sts);

    if (pltfm.CodeName < MFX_PLATFORM_CANNONLAKE) {
        if (!caps.LCUSizeSupported)     // not set until CNL now
            caps.LCUSizeSupported = 0b10;   // 32x32 lcu is only supported
    }
    else {
#ifndef OPEN_SOURCE
        if (IsEncPak(guid))
            caps.LCUSizeSupported |= 0b10;   // add support of 32x32 lcu for ENC+PAK
#endif
    }
#else
    if (!caps.LCUSizeSupported)
        caps.LCUSizeSupported = 2;
    (void)core;
    (void)guid;
#endif
    return sts;
}

mfxStatus QueryHwCaps(MFXCoreInterface* core, GUID guid, ENCODE_CAPS_HEVC & caps)
{
    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH265Encoder(core));
    MFX_CHECK(ddi.get(), MFX_ERR_UNSUPPORTED);

    mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(core, guid, 1920, 1088);
    MFX_CHECK_STS(sts);

    sts = ddi.get()->QueryEncodeCaps(caps);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus CheckHeaders(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps)
{
    MFX_CHECK_COND(
           par.m_sps.log2_min_luma_coding_block_size_minus3 == 0
        && par.m_sps.separate_colour_plane_flag == 0
        && par.m_sps.sample_adaptive_offset_enabled_flag == 0
        && par.m_sps.pcm_enabled_flag == 0);

#if defined(PRE_SI_TARGET_PLATFORM_GEN10)
    MFX_CHECK_COND(par.m_sps.amp_enabled_flag == 1);
#else
    MFX_CHECK_COND(par.m_sps.amp_enabled_flag == 0);
#endif

#if !defined(PRE_SI_TARGET_PLATFORM_GEN11)
    MFX_CHECK_COND(par.m_pps.tiles_enabled_flag == 0);
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    MFX_CHECK_COND(
      !(   (!caps.YUV444ReconSupport && (par.m_sps.chroma_format_idc == 3))
        || (!caps.YUV422ReconSupport && (par.m_sps.chroma_format_idc == 2))
        || (caps.Color420Only && (par.m_sps.chroma_format_idc != 1))));

    MFX_CHECK_COND(par.m_pps.num_tile_columns_minus1 <= caps.MaxNumOfTileColumnsMinus1);

    if (par.m_pps.tiles_enabled_flag)
    {
        MFX_CHECK_COND(par.m_pps.loop_filter_across_tiles_enabled_flag);
    }
#else
    MFX_CHECK_COND(par.m_sps.chroma_format_idc == 1);
#endif

    MFX_CHECK_COND(
      !(   par.m_sps.pic_width_in_luma_samples > caps.MaxPicWidth
        || par.m_sps.pic_height_in_luma_samples > caps.MaxPicHeight
        || (UINT)(((par.m_pps.num_tile_columns_minus1 + 1) * (par.m_pps.num_tile_rows_minus1 + 1)) > 1) > caps.TileSupport));
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    MFX_CHECK_COND(
      !(   (caps.MaxEncodedBitDepth == 0 || caps.BitDepth8Only)
        && (par.m_sps.bit_depth_luma_minus8 != 0 || par.m_sps.bit_depth_chroma_minus8 != 0)));

    MFX_CHECK_COND(
      !(   (caps.MaxEncodedBitDepth == 1 || !caps.BitDepth8Only)
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2))));
#else
    MFX_CHECK_COND(
        !(  caps.BitDepth8Only == 1 // 8 bit only
            && (par.m_sps.bit_depth_luma_minus8 != 0 || par.m_sps.bit_depth_chroma_minus8 != 0))); // not 8 bit 

    MFX_CHECK_COND(
        !(  caps.BitDepth8Only == 0 // 10 or 8 bit only
            && (!(par.m_sps.bit_depth_luma_minus8 == 0 || par.m_sps.bit_depth_luma_minus8 == 2)
                || !(par.m_sps.bit_depth_chroma_minus8 == 0 || par.m_sps.bit_depth_chroma_minus8 == 2))));
#endif
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    MFX_CHECK_COND(
      !(   caps.MaxEncodedBitDepth == 2
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2
            || par.m_sps.bit_depth_luma_minus8 == 4)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2
            || par.m_sps.bit_depth_chroma_minus8 == 4))));

    MFX_CHECK_COND(
      !(   caps.MaxEncodedBitDepth == 3
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2
            || par.m_sps.bit_depth_luma_minus8 == 4
            || par.m_sps.bit_depth_luma_minus8 == 8)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2
            || par.m_sps.bit_depth_chroma_minus8 == 4
            || par.m_sps.bit_depth_chroma_minus8 == 8))));
#endif

    return MFX_ERR_NONE;
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


DDIHeaderPacker::DDIHeaderPacker()
{
}

DDIHeaderPacker::~DDIHeaderPacker()
{
}

void DDIHeaderPacker::Reset(MfxVideoParam const & par)
{
    m_buf.resize(6 + par.mfx.NumSlice);
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
        m_packer.GetPrefixSEI(task, m_cur->pData, m_cur->DataLength);
        break;
    case SUFFIX_SEI_NUT:
        m_packer.GetSuffixSEI(task, m_cur->pData, m_cur->DataLength);
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
#if MFX_EXTBUFF_CU_QP_ENABLE
mfxStatus FillCUQPDataDDI(Task& task, MfxVideoParam &par, MFXCoreInterface& core, mfxFrameInfo &CUQPFrameInfo )
{
    
     mfxCoreParam coreParams = {};

    if (core.GetCoreParam(&coreParams))
       return  MFX_ERR_UNSUPPORTED;

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP || !IsOn(par.m_ext.CO3.EnableMBQP) || ((coreParams.Impl & 0xF00) == MFX_HW_VAAPI))
        return MFX_ERR_NONE;

    mfxU32 minWidthQPData = (par.m_ext.HEVCParam.PicWidthInLumaSamples   + par.LCUSize  - 1) / par.LCUSize;
    mfxU32 minHeightQPData = (par.m_ext.HEVCParam.PicHeightInLumaSamples  + par.LCUSize  - 1) / par.LCUSize;
    mfxU32 minQPSize = minWidthQPData*minHeightQPData;
    mfxU32 driverQPsize = CUQPFrameInfo.Width * CUQPFrameInfo.Height;

    MFX_CHECK(driverQPsize >= minQPSize && minQPSize > 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    mfxU32 k_dr_w   =  1;
    mfxU32 k_dr_h   =  1;
    mfxU32 k_input = 1;
    
    if (driverQPsize > minQPSize)
    {
        k_dr_w = CUQPFrameInfo.Width/minWidthQPData;
        k_dr_h = minHeightQPData/minHeightQPData;
        MFX_CHECK(minWidthQPData*k_dr_w == CUQPFrameInfo.Width && minHeightQPData*k_dr_h == CUQPFrameInfo.Height, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(k_dr_w == 1 ||k_dr_w == 2 || k_dr_w == 4 || k_dr_w == 8 , MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(k_dr_h == 1 ||k_dr_h == 2 || k_dr_h == 4 || k_dr_h == 8 , MFX_ERR_UNDEFINED_BEHAVIOR);
    }


    mfxExtMBQP *mbqp = ExtBuffer::Get(task.m_ctrl);
    if (mbqp)
    {
        mfxU16 blockSize= mbqp->BlockSize ? mbqp->BlockSize : 16;  
        k_input = par.LCUSize/blockSize;
        MFX_CHECK(par.LCUSize == blockSize*k_input, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK (mbqp->NumQPAlloc >= minQPSize*k_input*k_input, MFX_ERR_UNDEFINED_BEHAVIOR);         
        MFX_CHECK(k_input == 1 ||k_input == 2 || k_input == 4 || k_input == 8 , MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    
    {
        FrameLocker lock(&core, task.m_midCUQp);
        MFX_CHECK(lock.Y, MFX_ERR_LOCK_MEMORY);

        if (mbqp)
             for (mfxU32 i = 0; i < CUQPFrameInfo.Height; i++)
                 for (mfxU32 j = 0; j < CUQPFrameInfo.Width; j++)
                    lock.Y[i * lock.Pitch +j] = mbqp->QP[i*k_input/k_dr_h* CUQPFrameInfo.Width + j*k_input/k_dr_w];
        else
            for (mfxU32 i = 0; i < CUQPFrameInfo.Height; i++)
                ippsSet_8u((mfxU8)task.m_qpY, (Ipp8u *)&lock.Y[i * lock.Pitch], (int)CUQPFrameInfo.Width);
    }
    return MFX_ERR_NONE;
    

}
#endif

#if defined(_WIN32) || defined(_WIN64)

void CachedFeedback::Reset(mfxU32 cacheSize, mfxU32 feedbackSize)
{
    m_cache.Reset(cacheSize, feedbackSize);

    for (size_t i = 0; i < m_cache.size(); i++)
        m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
}

mfxStatus CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        const ENCODE_QUERY_STATUS_PARAMS & u = update[i];

        if (u.bStatus != ENCODE_NOTAVAILABLE)
        {
            size_t cache = m_cache.size();

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                ENCODE_QUERY_STATUS_PARAMS & c = m_cache[j];

                if (c.StatusReportFeedbackNumber == u.StatusReportFeedbackNumber)
                {
                    cache = j;
                    break;
                }
                else if (cache == m_cache.size() && c.bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = j;
                }
            }
            MFX_CHECK(cache != m_cache.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
            m_cache.copy(cache, update, i);
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
    bool bLast,
    ENCODE_SET_SLICE_HEADER_HEVC & cs)
{
    cs.slice_type                           = task.m_sh.type;
    if (cs.slice_type != 2)
    {
        Copy(cs.RefPicList, task.m_refPicList);
        cs.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

        if (cs.slice_type == 0)
            cs.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;
    }

    cs.bLastSliceOfPic                      = bLast;
    cs.dependent_slice_segment_flag         = task.m_sh.dependent_slice_segment_flag;
    cs.slice_temporal_mvp_enable_flag       = task.m_sh.temporal_mvp_enabled_flag;
    cs.slice_sao_luma_flag                  = task.m_sh.sao_luma_flag;
    cs.slice_sao_chroma_flag                = task.m_sh.sao_chroma_flag;
    cs.mvd_l1_zero_flag                     = task.m_sh.mvd_l1_zero_flag;
    cs.cabac_init_flag                      = task.m_sh.cabac_init_flag;
    cs.slice_deblocking_filter_disable_flag = task.m_sh.deblocking_filter_disabled_flag;
    cs.collocated_from_l0_flag              = task.m_sh.collocated_from_l0_flag;

    cs.slice_qp_delta       = task.m_sh.slice_qp_delta;
    cs.slice_cb_qp_offset   = task.m_sh.slice_cb_qp_offset;
    cs.slice_cr_qp_offset   = task.m_sh.slice_cr_qp_offset;
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

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        FillSliceBuffer(task, (i == slice.size() - 1), slice[i]);
    }
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
    sps.RateControlMethod   = mfxU8(par.isSWBRC() ? MFX_RATECONTROL_CQP:par.mfx.RateControlMethod) ;
    if(par.mfx.FrameInfo.Height <= 576 &&
        par.mfx.FrameInfo.Width <= 736 &&
        par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        par.mfx.QPP < 32)
    {
        sps.ContentInfo = eContent_NonVideoScreen;
    }
    if (   par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        sps.MinBitRate      = 0;
        sps.TargetBitRate   = par.TargetKbps;
        sps.MaxBitRate      = par.MaxKbps;
    }   

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        sps.MinBitRate      = par.TargetKbps;
        sps.TargetBitRate   = par.TargetKbps;
        sps.MaxBitRate      = par.TargetKbps;
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        sps.TargetBitRate   = par.TargetKbps;
//        sps.AVBRAccuracy = par.mfx.Accuracy;
//        sps.AVBRConvergence = par.mfx.Convergence;
    }

    sps.FramesPer100Sec = (mfxU16)(mfxU64(100) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
    sps.InitVBVBufferFullnessInBit = 8000 * par.InitialDelayInKB;
    sps.VBVBufferSizeInBit         = 8000 * par.BufferSizeInKB;

    sps.bResetBRC        = 0;
    sps.GlobalSearch     = 0;
    sps.LocalSearch      = 0;
    sps.EarlySkip        = 0;
    sps.MBBRC            = IsOn(par.m_ext.CO2.MBBRC);

    //WA:  Parallel BRC is switched on in sync & async mode (quality drop in noParallelBRC in driver)
    sps.ParallelBRC = (par.mfx.GopRefDist > 1) && (par.mfx.GopRefDist <= 8) && par.isBPyramid() && !IsOn(par.mfx.LowPower);

    if (par.m_ext.CO2.MaxSliceSize)
        sps.SliceSizeControl = 1;

    sps.UserMaxFrameSize = par.m_ext.CO2.MaxFrameSize;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        sps.ICQQualityFactor = (mfxU8)par.mfx.ICQQuality;
    else if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
        sps.ICQQualityFactor = (mfxU8)par.m_ext.CO3.QVBRQuality;
    else
        sps.ICQQualityFactor = 0;

    if (sps.ParallelBRC)
    {
        if (!par.isBPyramid())
        {
            sps.NumOfBInGop[0]  = (par.mfx.GopPicSize - 1) - (par.mfx.GopPicSize - 1)/par.mfx.GopRefDist;
            sps.NumOfBInGop[1]  = 0;
            sps.NumOfBInGop[2]  = 0;
        }
        else if (par.mfx.GopRefDist <= 8)
        {
            static UINT B[9]  = {0,0,1,1,1,1,1,1,1};
            static UINT B1[9] = {0,0,0,1,2,2,2,2,2};
            static UINT B2[9] = {0,0,0,0,0,1,2,3,4};

            mfxI32 numBpyr   = par.mfx.GopPicSize/par.mfx.GopRefDist;
            mfxI32 lastBpyrW = par.mfx.GopPicSize%par.mfx.GopRefDist;

            sps.NumOfBInGop[0]  = numBpyr*B[par.mfx.GopRefDist] + B[lastBpyrW];
            sps.NumOfBInGop[1]  = numBpyr*B1[par.mfx.GopRefDist]+ B1[lastBpyrW];
            sps.NumOfBInGop[2]  = numBpyr*B2[par.mfx.GopRefDist]+ B2[lastBpyrW];
        }
        else
        {
            assert(0);
        }
    }

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    switch (par.mfx.FrameInfo.BitDepthLuma)
    {
    case 16: sps.SourceBitDepth = 3; break;
    case 12: sps.SourceBitDepth = 2; break;
    case 10: sps.SourceBitDepth = 1; break;
    default: assert(!"undefined SourceBitDepth");
    case  8: sps.SourceBitDepth = 0; break;
    }

    if (   par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4
        || par.mfx.FrameInfo.FourCC == MFX_FOURCC_A2RGB10)
    {
        sps.SourceFormat = 3;
    }
    else
    {
        assert(par.mfx.FrameInfo.ChromaFormat > MFX_CHROMAFORMAT_YUV400 && par.mfx.FrameInfo.ChromaFormat <= MFX_CHROMAFORMAT_YUV444);
        sps.SourceFormat = par.mfx.FrameInfo.ChromaFormat - 1;
    }
#else
    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        sps.SourceBitDepth = 1; //10b
#endif

    sps.scaling_list_enable_flag           = par.m_sps.scaling_list_enabled_flag;
    sps.sps_temporal_mvp_enable_flag       = par.m_sps.temporal_mvp_enabled_flag;
    sps.strong_intra_smoothing_enable_flag = par.m_sps.strong_intra_smoothing_enabled_flag;
    sps.amp_enabled_flag                   = par.m_sps.amp_enabled_flag;    // SKL: 0, CNL+: 1
    sps.SAO_enabled_flag                   = par.m_sps.sample_adaptive_offset_enabled_flag; // 0, 1
    sps.pcm_enabled_flag                   = par.m_sps.pcm_enabled_flag;
    sps.pcm_loop_filter_disable_flag       = 1;//par.m_sps.pcm_loop_filter_disabled_flag;
    sps.chroma_format_idc                  = par.m_sps.chroma_format_idc;
    sps.tiles_fixed_structure_flag         = par.m_sps.vui.tiles_fixed_structure_flag;
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

    // ROI
    if (par.m_ext.ROI.NumROI) {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            sps.ROIValueInDeltaQP = 1;
        else
            sps.ROIValueInDeltaQP = 0;  // 0 means Priorities (if supported in caps)
    }

    // if ENCODE_BLOCKQPDATA surface is provided
    //sps.BlockQPInDeltaQPIndex = 1;    // surface contains data for non-rect ROI
    //sps.BlockQPInDeltaQPIndex = 0;    // surface contains data for block level absolute QP value
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
    //pps.bScreenContent                        = 0;
    //pps.no_output_of_prior_pics_flag          = ;
    //pps.XbEnableRollingIntraRefreshX

    pps.QpY = mfxI8(par.m_pps.init_qp_minus26 + 26);

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

 

    pps.LcuMaxBitsizeAllowed       = 0;
    pps.bUseRawPicForRef           = 0;
    pps.NumSlices                  = par.mfx.NumSlice;

    pps.bEnableSliceLevelReport    = !!par.m_ext.CO2.MaxSliceSize;
    if (par.m_ext.CO2.MaxSliceSize)
        pps.MaxSliceSizeInBytes = par.m_ext.CO2.MaxSliceSize;

    // Max/Min QP settings for BRC

    /*if (par.mfx.FrameInfo.Height <= 576 &&
        par.mfx.FrameInfo.Width <= 736 &&
        par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        par.mfx.QPP < 32)
    {
        pps.bScreenContent = 1;
    }*/

    // ROI
    pps.NumROI = (mfxU8)par.m_ext.ROI.NumROI;
    if (pps.NumROI)
    {
        mfxU32 blkshift = par.m_sps.log2_min_luma_coding_block_size_minus3 + 3 +
            par.m_sps.log2_diff_max_min_luma_coding_block_size;

        for (mfxU16 i = 0; i < pps.NumROI; i ++)
        {
            pps.ROI[i].Roi.Left = (mfxU16)((par.m_ext.ROI.ROI[i].Left) >> blkshift);
            pps.ROI[i].Roi.Top = (mfxU16)((par.m_ext.ROI.ROI[i].Top) >> blkshift);
            pps.ROI[i].Roi.Right = (mfxU16)((par.m_ext.ROI.ROI[i].Right) >> blkshift);
            pps.ROI[i].Roi.Bottom = (mfxU16)((par.m_ext.ROI.ROI[i].Bottom) >> blkshift);
            pps.ROI[i].PriorityLevelOrDQp = (mfxU8)par.m_ext.ROI.ROI[i].Priority;
        }
        pps.MaxDeltaQp = 51;    // is used for BRC only
        pps.MinDeltaQp = -51;
    }

#if (HEVCE_DDI_VERSION >= 960)
    // if ENCODE_BLOCKQPDATA surface is provided
    pps.NumDeltaQpForNonRectROI = 0;    // if no non-rect ROI
    // pps.NumDeltaQpForNonRectROI    // total number of different delta QPs for non-rect ROI ( + the same for rect ROI should not exceed MaxNumDeltaQP in caps
    // pps.NonRectROIDeltaQpList[0..pps.NumDeltaQpForNonRectROI-1] // delta QPs for non-rect ROI
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    pps.DisplayFormatSwizzle = (par.mfx.FrameInfo.FourCC == MFX_FOURCC_A2RGB10);
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)
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

    // ROI
    pps.NumROI = (mfxU8)task.m_numRoi;
    if (pps.NumROI)
    {
        mfxU32 blkshift = 5;    // should be taken from ROICaps.ROIBlockSize

        for (mfxU16 i = 0; i < task.m_numRoi; i ++)
        {
            pps.ROI[i].Roi.Left = (mfxU16)((task.m_roi[i].Left) >> blkshift);
            pps.ROI[i].Roi.Top = (mfxU16)((task.m_roi[i].Top) >> blkshift);
            pps.ROI[i].Roi.Right = (mfxU16)((task.m_roi[i].Right) >> blkshift);
            pps.ROI[i].Roi.Bottom = (mfxU16)((task.m_roi[i].Bottom) >> blkshift);
            pps.ROI[i].PriorityLevelOrDQp = (mfxU8)task.m_roi[i].Priority;
        }
        pps.MaxDeltaQp = 51;    // is used for BRC only
        pps.MinDeltaQp = -51;
    }

#if (HEVCE_DDI_VERSION >= 960)
    // if ENCODE_BLOCKQPDATA surface is provided
    pps.NumDeltaQpForNonRectROI = 0;    // if no non-rect ROI
    // pps.NumDeltaQpForNonRectROI    // total number of different delta QPs for non-rect ROI ( + the same for rect ROI should not exceed MaxNumDeltaQP in caps
    // pps.NonRectROIDeltaQpList[0..pps.NumDeltaQpForNonRectROI-1] // delta QPs for non-rect ROI
#endif

    pps.CodingType      = task.m_codingType;
    pps.CurrPicOrderCnt = task.m_poc;

    pps.bEnableRollingIntraRefresh = task.m_IRState.refrType;

    if (pps.bEnableRollingIntraRefresh)
    {
        pps.IntraInsertionLocation = task.m_IRState.IntraLocation;
        pps.IntraInsertionSize = task.m_IRState.IntraSize;
        pps.QpDeltaForInsertedIntra = mfxU8(task.m_IRState.IntRefQPDelta);
    }
    pps.StatusReportFeedbackNumber = task.m_statusReportNumber;

#if (HEVCE_DDI_VERSION >= 960)
    mfxExtAVCEncoderWiDiUsage* extWiDi = ExtBuffer::Get(task.m_ctrl);

    if (extWiDi)
        pps.InputType = eType_DRM_SECURE;
    else
        pps.InputType = eType_DRM_NONE;
#endif
}

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice)
{
    slice.resize(par.m_slice.size());

    for (mfxU16 i = 0; i < slice.size(); i++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];
        Zero(cs);

        cs.slice_id = i;
        cs.slice_segment_address = par.m_slice[i].SegmentAddress;
        cs.NumLCUsInSlice = par.m_slice[i].NumLCU;
        cs.bLastSliceOfPic = (i == slice.size() - 1);
    }
}

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice)
{
    for (mfxU16 i = 0; i < slice.size(); i++)
    {
        FillSliceBuffer(task, (i == slice.size() - 1), slice[i]);
        //TBD
        //slice[i].luma_offset_l0;
        //slice[i].ChromaOffsetL0;
        //slice[i].luma_offset_l1;
        //slice[i].ChromaOffsetL1;
    }
}

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT & sps)
{
    Zero(sps);
    FillSpsBuffer(par, caps, (ENCODE_SET_SEQUENCE_PARAMETERS_HEVC&)sps);

    sps.transform_skip_rotation_enabled_flag    = par.m_sps.transform_skip_rotation_enabled_flag;
    sps.transform_skip_context_enabled_flag     = par.m_sps.transform_skip_context_enabled_flag;
    sps.implicit_rdpcm_enabled_flag             = par.m_sps.implicit_rdpcm_enabled_flag;
    sps.explicit_rdpcm_enabled_flag             = par.m_sps.explicit_rdpcm_enabled_flag;
    sps.extended_precision_processing_flag      = par.m_sps.extended_precision_processing_flag;
    sps.intra_smoothing_disabled_flag           = par.m_sps.intra_smoothing_disabled_flag;
    sps.high_precision_offsets_enabled_flag     = par.m_sps.high_precision_offsets_enabled_flag;
    sps.persistent_rice_adaptation_enabled_flag = par.m_sps.persistent_rice_adaptation_enabled_flag;
    sps.cabac_bypass_alignment_enabled_flag     = par.m_sps.cabac_bypass_alignment_enabled_flag;
}

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT & pps)
{
    Zero(pps);
    FillPpsBuffer(par, (ENCODE_SET_PICTURE_PARAMETERS_HEVC&)pps);

    pps.log2_max_transform_skip_block_size_minus2   = par.m_pps.log2_max_transform_skip_block_size_minus2;
    pps.cross_component_prediction_enabled_flag     = par.m_pps.cross_component_prediction_enabled_flag;
    pps.chroma_qp_offset_list_enabled_flag          = par.m_pps.chroma_qp_offset_list_enabled_flag;
    pps.diff_cu_chroma_qp_offset_depth              = par.m_pps.diff_cu_chroma_qp_offset_depth;
    pps.chroma_qp_offset_list_len_minus1            = par.m_pps.chroma_qp_offset_list_len_minus1;
    Copy(pps.cb_qp_offset_list, par.m_pps.cb_qp_offset_list);
    Copy(pps.cr_qp_offset_list, par.m_pps.cr_qp_offset_list);
    pps.log2_sao_offset_scale_luma                  = par.m_pps.log2_sao_offset_scale_luma;
    pps.log2_sao_offset_scale_chroma                = par.m_pps.log2_sao_offset_scale_chroma;
}

#endif //PRE_SI_TARGET_PLATFORM_GEN11

#endif //defined(_WIN32) || defined(_WIN64)

}; // namespace MfxHwH265Encode
#endif
