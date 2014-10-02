//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_h265_encode_hw_utils.h"
#include <assert.h>

namespace MfxHwH265Encode
{

MfxFrameAllocResponse::MfxFrameAllocResponse()
    : m_core(0)
{
    Zero(*(mfxFrameAllocResponse*)this);
}

MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    Free();
} 

void MfxFrameAllocResponse::Free()
{
    if (m_core == 0)
        return;

    mfxFrameAllocator & fa = m_core->FrameAllocator();
    mfxCoreParam par = {};

    m_core->GetCoreParam(&par);

    if ((par.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        for (size_t i = 0; i < m_responseQueue.size(); i++)
        {
            fa.Free(fa.pthis, &m_responseQueue[i]);
        }
        m_responseQueue.resize(0);
    }
    else
    {
        if (mids)
        {
            NumFrameActual = m_numFrameActualReturnedByAllocFrames;
            fa.Free(fa.pthis, this);
            mids = 0;
        }
    }
}

mfxStatus MfxFrameAllocResponse::Alloc(
    MFXCoreInterface *     core,
    mfxFrameAllocRequest & req,
    bool                   /*isCopyRequired*/)
{
    mfxFrameAllocator & fa = core->FrameAllocator();
    mfxCoreParam par = {};
    mfxFrameAllocResponse & response = *(mfxFrameAllocResponse*)this;

    core->GetCoreParam(&par);

    req.NumFrameSuggested = req.NumFrameMin;

    if ((par.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        mfxFrameAllocRequest tmp = req;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        m_responseQueue.resize(req.NumFrameMin);
        m_mids.resize(req.NumFrameMin);

        for (int i = 0; i < req.NumFrameMin; i++)
        {
            mfxStatus sts = fa.Alloc(fa.pthis, &tmp, &m_responseQueue[i]);
            MFX_CHECK_STS(sts);
            m_mids[i] = m_responseQueue[i].mids[0];
        }

        mids = &m_mids[0];
        NumFrameActual = req.NumFrameMin;
    }
    else
    {
        mfxStatus sts = fa.Alloc(fa.pthis, &req, &response);
        MFX_CHECK_STS(sts);
    }

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;
    
    m_locked.resize(req.NumFrameMin, 0);

    m_core = core;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;
    m_info = req.Info;

    return MFX_ERR_NONE;
} 

mfxStatus MfxFrameAllocResponse::Alloc(
    MFXCoreInterface *     core,
    mfxFrameAllocRequest & req,
    mfxFrameSurface1 **    /*opaqSurf*/,
    mfxU32                 /*numOpaqSurf*/)
{
    req.NumFrameSuggested = req.NumFrameMin;

    assert(!"not implemented");
    mfxStatus sts = MFX_ERR_UNSUPPORTED;//core->AllocFrames(&req, this, opaqSurf, numOpaqSurf);
    MFX_CHECK_STS(sts);

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_core = core;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;

    return MFX_ERR_NONE;
}

mfxU32 MfxFrameAllocResponse::Lock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return 0;
    assert(m_locked[idx] < 0xffffffff);
    return ++m_locked[idx];
}

void MfxFrameAllocResponse::Unlock()
{
    std::fill(m_locked.begin(), m_locked.end(), 0);
}

mfxU32 MfxFrameAllocResponse::Unlock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return mfxU32(-1);
    assert(m_locked[idx] > 0);
    return --m_locked[idx];
}

mfxU32 MfxFrameAllocResponse::Locked(mfxU32 idx) const
{
    return (idx < m_locked.size()) ? m_locked[idx] : 1;
}

mfxU32 FindFreeResourceIndex(
    MfxFrameAllocResponse const & pool,
    mfxU32                        startingFrom)
{
    for (mfxU32 i = startingFrom; i < pool.NumFrameActual; i++)
        if (pool.Locked(i) == 0)
            return i;
    return 0xFFFFFFFF;
}

mfxMemId AcquireResource(
    MfxFrameAllocResponse & pool,
    mfxU32                  index)
{
    if (index > pool.NumFrameActual)
        return 0;
    pool.Lock(index);
    return pool.mids[index];
}

void ReleaseResource(
    MfxFrameAllocResponse & pool,
    mfxMemId                mid)
{
    for (mfxU32 i = 0; i < pool.NumFrameActual; i++)
    {
        if (pool.mids[i] == mid)
        {
            pool.Unlock(i);
            break;
        }
    }
}

mfxStatus GetNativeHandleToRawSurface(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task,
    mfxHDLPair &          handle)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocator & fa = core.FrameAllocator();

    //mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);

    Zero(handle);
    mfxHDL * nativeHandle = &handle.first;

    mfxFrameSurface1 * surface = task.m_surf;

    if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        assert(!"not implemented");
        //surface = core.GetNativeSurface(task.m_yuv);
        //if (surface == 0)
        //    return Error(MFX_ERR_UNDEFINED_BEHAVIOR);
        //
        //surface->Info            = task.m_yuv->Info;
        //surface->Data.TimeStamp  = task.m_yuv->Data.TimeStamp;
        //surface->Data.FrameOrder = task.m_yuv->Data.FrameOrder;
        //surface->Data.Corrupted  = task.m_yuv->Data.Corrupted;
        //surface->Data.DataFlag   = task.m_yuv->Data.DataFlag;
    }

    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY /*||
        video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)*/)
        sts = fa.GetHDL(fa.pthis, task.m_midRaw, nativeHandle);
    else if (video.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
        sts = fa.GetHDL(fa.pthis, surface->Data.MemId, nativeHandle);
    //else if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY) // opaq with internal video memory
    //    sts = core.GetFrameHDL(surface->Data.MemId, nativeHandle);
    else
        return (MFX_ERR_UNDEFINED_BEHAVIOR);

    if (nativeHandle == 0)
        return (MFX_ERR_UNDEFINED_BEHAVIOR);

    return sts;
}

mfxStatus CopyRawSurfaceToVideoMemory(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task)
{
    //mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);
    mfxFrameSurface1 * surface = task.m_surf;

    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY 
        /*|| video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)*/)
    {
        if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            assert(!"not implemented");
            //surface = core.GetNativeSurface(task.m_yuv);
            //if (surface == 0)
            //    return Error(MFX_ERR_UNDEFINED_BEHAVIOR);
            //
            //surface->Info            = task.m_yuv->Info;
            //surface->Data.TimeStamp  = task.m_yuv->Data.TimeStamp;
            //surface->Data.FrameOrder = task.m_yuv->Data.FrameOrder;
            //surface->Data.Corrupted  = task.m_yuv->Data.Corrupted;
            //surface->Data.DataFlag   = task.m_yuv->Data.DataFlag;
        }
        
        mfxFrameAllocator & fa = core.FrameAllocator();
        mfxStatus sts = MFX_ERR_NONE;
        mfxFrameData d3dSurf = {};
        mfxFrameData sysSurf = surface->Data;
        d3dSurf.MemId = task.m_midRaw;
        bool sysLocked = false;
        
        mfxFrameSurface1 surfSrc = { {}, video.mfx.FrameInfo, sysSurf };
        mfxFrameSurface1 surfDst = { {}, video.mfx.FrameInfo, d3dSurf };

        if (!sysSurf.Y)
        {
            sts = fa.Lock(fa.pthis, sysSurf.MemId, &surfSrc.Data);
            MFX_CHECK_STS(sts);
            sysLocked = true;
        }
        
        sts = fa.Lock(fa.pthis, d3dSurf.MemId, &surfDst.Data);
        MFX_CHECK_STS(sts);

        sts = core.CopyFrame(&surfDst, &surfSrc);

        if (sysLocked)
        {
            sts = fa.Unlock(fa.pthis, sysSurf.MemId, &surfSrc.Data);
            MFX_CHECK_STS(sts);
        }
        
        sts = fa.Unlock(fa.pthis, d3dSurf.MemId, &surfDst.Data);
        MFX_CHECK_STS(sts);

        //core.LockExternalFrame(sysSurf.MemId, &sysSurf);
        //
        //if (sysSurf.Y == 0)
        //    return (MFX_ERR_LOCK_MEMORY);
        //
        //
        //{
        //    mfxFrameSurface1 surfSrc = { {}, video.mfx.FrameInfo, sysSurf };
        //    mfxFrameSurface1 surfDst = { {}, video.mfx.FrameInfo, d3dSurf };
        //    sts = core.DoFastCopyWrapper(&surfDst, MFX_MEMTYPE_D3D_INT, &surfSrc, MFX_MEMTYPE_SYS_EXT);
        //    MFX_CHECK_STS(sts);
        //}
        //
        //sts = core.UnlockExternalFrame(sysSurf.MemId, &sysSurf);
        //MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

MfxVideoParam::MfxVideoParam()
    : BufferSizeInKB  (0)
    , InitialDelayInKB(0)
    , TargetKbps      (0)
    , MaxKbps         (0)
{
    Zero(*(mfxVideoParam*)this);
    Zero(NumRefLX);
}

MfxVideoParam::MfxVideoParam(MfxVideoParam const & par)
{
    Construct(par);

    //Copy(m_vps, par.m_vps);
    //Copy(m_sps, par.m_sps);
    //Copy(m_pps, par.m_pps);

    BufferSizeInKB   = par.BufferSizeInKB;
    InitialDelayInKB = par.InitialDelayInKB;
    TargetKbps       = par.TargetKbps;
    MaxKbps          = par.MaxKbps;
    NumRefLX[0]      = par.NumRefLX[0];
    NumRefLX[1]      = par.NumRefLX[1];
}

MfxVideoParam::MfxVideoParam(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
}

void MfxVideoParam::Construct(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
}

void MfxVideoParam::SyncVideoToCalculableParam()
{
    mfxU32 multiplier = MFX_MAX(mfx.BRCParamMultiplier, 1);

    BufferSizeInKB   = mfx.BufferSizeInKB   * multiplier;
    InitialDelayInKB = mfx.InitialDelayInKB * multiplier;
    TargetKbps       = mfx.TargetKbps       * multiplier;
    MaxKbps          = mfx.MaxKbps          * multiplier;

    if (mfx.NumRefFrame)
    {
        NumRefLX[0] = mfx.NumRefFrame - mfx.GopRefDist > 1;
        NumRefLX[1] = mfx.GopRefDist > 1;
    } else
    {
        NumRefLX[0] = 0;
        NumRefLX[1] = 0;
    }
}

void MfxVideoParam::SyncCalculableToVideoParam()
{
    mfxU32 maxVal32 = BufferSizeInKB;

    mfx.NumRefFrame = NumRefLX[0] + NumRefLX[1];

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        maxVal32 = MFX_MAX(maxVal32, TargetKbps);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
            maxVal32 = MFX_MAX(maxVal32, MFX_MAX(MaxKbps, InitialDelayInKB));
    }

    mfx.BRCParamMultiplier = mfxU16((maxVal32 + 0x10000) / 0x10000);
    mfx.BufferSizeInKB     = mfxU16((BufferSizeInKB + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);

    if (mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_AVBR||
        mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        mfx.TargetKbps = mfxU16((TargetKbps + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
        {
            mfx.InitialDelayInKB = mfxU16((InitialDelayInKB + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);
            mfx.MaxKbps          = mfxU16((MaxKbps + mfx.BRCParamMultiplier - 1)          / mfx.BRCParamMultiplier);
        }
    }
}

void MfxVideoParam::SyncMfxToHeadersParam()
{
    PTL& general = m_vps.general;
    SubLayerOrdering& slo = m_vps.sub_layer[0];

    Zero(m_vps);
    m_vps.video_parameter_set_id    = 0;
    m_vps.reserved_three_2bits      = 3;
    m_vps.max_layers_minus1         = 0;
    m_vps.max_sub_layers_minus1     = 0;
    m_vps.temporal_id_nesting_flag  = 1;
    m_vps.reserved_0xffff_16bits    = 0xFFFF;
    m_vps.sub_layer_ordering_info_present_flag = 0;

    m_vps.timing_info_present_flag          = 1;
    m_vps.num_units_in_tick                 = mfx.FrameInfo.FrameRateExtD;
    m_vps.time_scale                        = mfx.FrameInfo.FrameRateExtN;
    m_vps.poc_proportional_to_timing_flag   = 0;
    m_vps.num_hrd_parameters                = 0;

    general.profile_space               = 0;
    general.tier_flag                   = !!(mfx.CodecProfile & MFX_TIER_HEVC_HIGH);
    general.profile_idc                 = (mfx.CodecProfile & 0x55) * 3;
    general.profile_compatibility_flags = 0;
    general.progressive_source_flag     = !!(mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE);
    general.interlaced_source_flag      = !!(mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF|MFX_PICSTRUCT_FIELD_BFF));
    general.non_packed_constraint_flag  = 0;
    general.frame_only_constraint_flag  = 0;
    general.level_idc                   = (mfxU8)mfx.CodecLevel;

    slo.max_dec_pic_buffering_minus1    = mfx.NumRefFrame;
    slo.max_num_reorder_pics            = mfx.GopRefDist - 1;
    slo.max_latency_increase_plus1      = 0;

    assert(0 == m_vps.max_sub_layers_minus1);

    Zero(m_sps);
    ((LayersInfo&)m_sps) = ((LayersInfo&)m_vps);
    m_sps.video_parameter_set_id   = m_vps.video_parameter_set_id;
    m_sps.max_sub_layers_minus1    = m_vps.max_sub_layers_minus1;
    m_sps.temporal_id_nesting_flag = m_vps.temporal_id_nesting_flag;

    m_sps.seq_parameter_set_id              = 0;
    m_sps.chroma_format_idc                 = mfx.FrameInfo.ChromaFormat;
    m_sps.separate_colour_plane_flag        = 0;
    m_sps.pic_width_in_luma_samples         = mfx.FrameInfo.Width;
    m_sps.pic_height_in_luma_samples        = mfx.FrameInfo.Height;
    m_sps.conformance_window_flag           = 0;
    m_sps.bit_depth_luma_minus8             = Max(0, (mfxI32)mfx.FrameInfo.BitDepthLuma - 8);
    m_sps.bit_depth_chroma_minus8           = Max(0, (mfxI32)mfx.FrameInfo.BitDepthChroma - 8);
    m_sps.log2_max_pic_order_cnt_lsb_minus4 = Clip3(0U, 12U, mfx.GopRefDist > 1 ? CeilLog2(mfx.GopRefDist + slo.max_dec_pic_buffering_minus1) + 3 : 0U);

    m_sps.log2_min_luma_coding_block_size_minus3   = 0; // SKL
    m_sps.log2_diff_max_min_luma_coding_block_size = 2; // SKL
    m_sps.log2_min_transform_block_size_minus2     = 0;
    m_sps.log2_diff_max_min_transform_block_size   = 3;
    m_sps.max_transform_hierarchy_depth_inter      = 2;
    m_sps.max_transform_hierarchy_depth_intra      = 2;
    m_sps.scaling_list_enabled_flag                = 0;
    m_sps.amp_enabled_flag                         = 1;
    m_sps.sample_adaptive_offset_enabled_flag      = 0; // SKL
    m_sps.pcm_enabled_flag                         = 0; // SKL

    assert(0 == m_sps.pcm_enabled_flag);

    {
        //TODO: add ref B
        mfxU16 nPSets   = NumRefLX[0];

        for (mfxU32 i = 0; i < nPSets; i ++)
        {
            STRPS& rps = m_sps.strps[i ? mfx.GopRefDist - 1 + i : 0];

            rps.inter_ref_pic_set_prediction_flag = 0;
            rps.num_negative_pics = i + 1;
            rps.num_positive_pics = 0;

            for (mfxU32 j = 0; j < rps.num_negative_pics; j ++)
            {
                rps.pic[j].DeltaPocS0               = -mfxI16((j + 1) * mfx.GopRefDist);
                rps.pic[j].delta_poc_s0_minus1      = -(rps.pic[j].DeltaPocS0 - (j ? rps.pic[j-1].DeltaPocS0 : 0)) - 1;
                rps.pic[j].used_by_curr_pic_s0_flag = 1;
            }

            m_sps.num_short_term_ref_pic_sets ++;
        }

        for (mfxU32 i = 1; i < mfx.GopRefDist; i ++)
        {
            STRPS& rps = m_sps.strps[i];

            rps.inter_ref_pic_set_prediction_flag = 0;
            rps.num_negative_pics = NumRefLX[0];
            rps.num_positive_pics = 1;

            assert(1 == NumRefLX[1]);
            rps.pic[0].DeltaPocS0               = -mfxI16(i);
            rps.pic[0].delta_poc_s0_minus1      = i - 1;
            rps.pic[0].used_by_curr_pic_s0_flag = 1;

            for (mfxU32 j = 1; j < rps.num_negative_pics; j ++)
            {
                rps.pic[j].delta_poc_s0_minus1      = mfx.GopRefDist - 1;
                rps.pic[j].DeltaPocS0               = (-1) * mfx.GopRefDist + rps.pic[j-1].DeltaPocS0;
                rps.pic[j].used_by_curr_pic_s0_flag = 1;
            }

            for (mfxU32 j = rps.num_negative_pics; 
                    j < mfxU32(rps.num_negative_pics + rps.num_positive_pics); j ++)
            {
                rps.pic[j].DeltaPocS1               = mfxI16(mfx.GopRefDist - i);
                rps.pic[j].delta_poc_s1_minus1      = (rps.pic[j].DeltaPocS1 - ((j > rps.num_negative_pics) ? rps.pic[j-1].DeltaPocS1 : 0)) - 1;
                rps.pic[j].used_by_curr_pic_s1_flag = 1;
            }

            m_sps.num_short_term_ref_pic_sets ++;
        }
    }

    m_sps.long_term_ref_pics_present_flag       = 0;
    m_sps.temporal_mvp_enabled_flag             = 1; // SKL ?
    m_sps.strong_intra_smoothing_enabled_flag   = 0; // SKL
    m_sps.vui_parameters_present_flag           = 0;

    Zero(m_pps);
    m_pps.seq_parameter_set_id = m_sps.seq_parameter_set_id;

    m_pps.pic_parameter_set_id                  = 0;
    m_pps.dependent_slice_segments_enabled_flag = 0;
    m_pps.output_flag_present_flag              = 0;
    m_pps.num_extra_slice_header_bits           = 0;
    m_pps.sign_data_hiding_enabled_flag         = 0;
    m_pps.cabac_init_present_flag               = 0;
    m_pps.num_ref_idx_l0_default_active_minus1  = NumRefLX[0] - 1;
    m_pps.num_ref_idx_l1_default_active_minus1  = 0;
    m_pps.init_qp_minus26                       = 0;
    m_pps.constrained_intra_pred_flag           = 0;
    m_pps.transform_skip_enabled_flag           = 0;
    m_pps.cu_qp_delta_enabled_flag              = 0;

    if (mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        m_pps.init_qp_minus26 = (mfx.GopRefDist == 1 ? mfx.QPP : mfx.QPB) - 26;

    m_pps.cb_qp_offset                          = 0;
    m_pps.cr_qp_offset                          = 0;
    m_pps.slice_chroma_qp_offsets_present_flag  = 0;
    m_pps.weighted_pred_flag                    = 0;
    m_pps.weighted_bipred_flag                  = 0;
    m_pps.transquant_bypass_enabled_flag        = 0;
    m_pps.tiles_enabled_flag                    = 0;
    m_pps.entropy_coding_sync_enabled_flag      = 0;

    m_pps.loop_filter_across_slices_enabled_flag      = 0;
    m_pps.deblocking_filter_control_present_flag      = 0;
    m_pps.scaling_list_data_present_flag              = 0;
    m_pps.lists_modification_present_flag             = 0;
    m_pps.log2_parallel_merge_level_minus2            = 0;
    m_pps.slice_segment_header_extension_present_flag = 0;

}

mfxU16 FrameType2SliceType(mfxU32 ft)
{
    if (ft & MFX_FRAMETYPE_B)
        return 0;
    if (ft & MFX_FRAMETYPE_P)
        return 1;
    return 2;
}

bool isCurrStRef(Task const & task, mfxI32 poc)
{
    for (mfxU32 i = 0; i < 2; i ++)
        for (mfxU32 j = 0; j < task.m_numRefActive[i]; j ++)
            if (poc == task.m_dpb[0][task.m_refPicList[i][j]].m_poc)
                return true;
    return false;
}

void MfxVideoParam::GetSliceHeader(Task const & task, Slice & s) const
{
    bool  isIDR = !!(task.m_frameType & MFX_FRAMETYPE_IDR);
    bool  isP   = !!(task.m_frameType & MFX_FRAMETYPE_P);
    bool  isB   = !!(task.m_frameType & MFX_FRAMETYPE_B);

    Zero(s);
    
    s.first_slice_segment_in_pic_flag = 1;
    s.no_output_of_prior_pics_flag    = 0;
    s.pic_parameter_set_id            = m_pps.pic_parameter_set_id;

    if (!s.first_slice_segment_in_pic_flag)
    {
        if (m_pps.dependent_slice_segments_enabled_flag)
        {
            s.dependent_slice_segment_flag = 0;
        }

        s.segment_address = 0;
    }

    s.type = FrameType2SliceType(task.m_frameType);

    if (m_pps.output_flag_present_flag)
        s.pic_output_flag = 1;

    assert(0 == m_sps.separate_colour_plane_flag);

    if (!isIDR)
    {
        mfxU32 i, j;
        s.pic_order_cnt_lsb = (task.m_poc & ~(0xFFFFFFFF << (m_sps.log2_max_pic_order_cnt_lsb_minus4 + 4)));

        //TODO: add unused ST
        for (i = 0; i < m_sps.num_short_term_ref_pic_sets; i ++)
        {
            STRPS const & rps = m_sps.strps[i];
            mfxU32 nRef = 0;

            assert(0 == rps.inter_ref_pic_set_prediction_flag);

            if (   isB && rps.num_positive_pics == 0
                || isP && rps.num_positive_pics != 0)
                continue;

            for (j = 0; j < mfxU32(rps.num_negative_pics + rps.num_positive_pics); j ++)
            {
                if (rps.pic[j].used_by_curr_pic_sx_flag)
                {
                    if (!isCurrStRef(task, task.m_poc + rps.pic[j].DeltaPocSX))
                    {
                        nRef = IDX_INVALID;
                        break;
                    }

                    nRef ++;
                }
            }

            if (nRef == mfxU32(task.m_numRefActive[0] + task.m_numRefActive[1]))
                break;
        }

        if (i < m_sps.num_short_term_ref_pic_sets)
        {
            s.short_term_ref_pic_set_sps_flag = 1;
            s.short_term_ref_pic_set_idx      = i;
            s.strps = m_sps.strps[i];
        }
        else
        {
            STRPS & rps = s.strps;
            s.short_term_ref_pic_set_sps_flag = 0;

            rps.num_negative_pics = task.m_numRefActive[0];
            rps.num_positive_pics = task.m_numRefActive[1];

            for (i = 0; i < mfxU32(task.m_numRefActive[0] + task.m_numRefActive[1]); i ++)
            {
                mfxU32 lx = i >= task.m_numRefActive[0];
                mfxU32 idx = lx ? i - task.m_numRefActive[0] : i;
                mfxI16 prev = (!i || i == task.m_numRefActive[0]) ? 0 : rps.pic[i-1].DeltaPocSX;

                rps.pic[i].used_by_curr_pic_sx_flag = 1;
                rps.pic[i].DeltaPocSX               = mfxI16(task.m_dpb[0][task.m_refPicList[lx][idx]].m_poc - task.m_poc);
                rps.pic[i].delta_poc_sx_minus1      = Abs(rps.pic[i].DeltaPocSX - prev) - 1;
            }
        }

        assert(0 == m_sps.long_term_ref_pics_present_flag);

        if (m_sps.temporal_mvp_enabled_flag)
            s.temporal_mvp_enabled_flag = 1;
    }

    if (m_sps.sample_adaptive_offset_enabled_flag)
    {
        s.sao_luma_flag   = 0;
        s.sao_chroma_flag = 0;
    }

    if (isP || isB)
    {
        s.num_ref_idx_active_override_flag = 
           (          m_pps.num_ref_idx_l0_default_active_minus1 + 1 != task.m_numRefActive[0]
            || isB && m_pps.num_ref_idx_l1_default_active_minus1 + 1 != task.m_numRefActive[1]);

        if (s.num_ref_idx_active_override_flag)
        {
            s.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

            if (isB)
                s.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;
        }

        assert(0 == m_pps.lists_modification_present_flag);

        if (isB)
            s.mvd_l1_zero_flag = 0;

        if (m_pps.cabac_init_present_flag)
            s.cabac_init_flag = 0;

        if (s.temporal_mvp_enabled_flag)
        {
            if (isB)
                s.collocated_from_l0_flag = 1;

            s.collocated_from_l0_flag = 0;
        }

        assert(0 == m_pps.weighted_pred_flag);
        assert(0 == m_pps.weighted_bipred_flag);

        s.five_minus_max_num_merge_cand = 0;
    }

    if (mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        s.slice_qp_delta = task.m_qpY - (m_pps.init_qp_minus26 + 26);

    if (m_pps.slice_chroma_qp_offsets_present_flag)
    {
        s.slice_cb_qp_offset = 0;
        s.slice_cr_qp_offset = 0;
    }

    if (m_pps.deblocking_filter_override_enabled_flag)
        s.deblocking_filter_override_flag = 0;

    assert(0 == s.deblocking_filter_override_flag);

    s.loop_filter_across_slices_enabled_flag = 0;

    if (m_pps.tiles_enabled_flag || m_pps.entropy_coding_sync_enabled_flag)
    {
        s.num_entry_point_offsets = 0;

        assert(0 == s.num_entry_point_offsets);
    }
}

void MfxVideoParam::SyncHeadersToMfxParam()
{
    assert(!"not implemented");
}

MfxVideoParam& MfxVideoParam::operator=(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
    return *this;
}

void TaskManager::Reset(mfxU32 numTask)
{
    m_free.resize(numTask);
    m_reordering.resize(0);
    m_encoding.resize(0);
}

Task* TaskManager::New()
{
    UMC::AutomaticUMCMutex guard(m_listMutex);
    Task* pTask = 0;

    if (!m_free.empty())
    {
        pTask = &m_free.front();
        m_reordering.splice(m_reordering.end(), m_free, m_free.begin());
    }

    return pTask;
}

mfxU32 CountL1(DpbArray const & dpb, mfxI32 poc)
{
    mfxU32 c = 0;
    for (mfxU32 i = 0; !isDpbEnd(dpb, i); i ++)
        c += dpb[i].m_poc > poc;
    return c;
}

Task* TaskManager::Reorder(
    MfxVideoParam const & par,
    DpbArray const & dpb,
    bool flush)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    TaskList::iterator begin = m_reordering.begin();
    TaskList::iterator end = m_reordering.end();
    TaskList::iterator top = begin;

    //TODO: add B-ref
    while ( top != end 
        && (top->m_frameType & MFX_FRAMETYPE_B)
        && !CountL1(dpb, top->m_poc))
        top ++;

    if (flush && top == end && begin != end)
    {
        if (par.mfx.GopOptFlag & MFX_GOP_STRICT)
            top = begin;
        else
        {
            top --;
            top->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }
    }

    if (top == end)
        return 0;

    m_encoding.splice(m_encoding.end(), m_reordering, top);

    return &m_encoding.back();
}

void TaskManager::Ready(Task* pTask)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    for (TaskList::iterator it = m_encoding.begin(); it != m_encoding.end(); it ++)
    {
        if (pTask == &*it)
        {
            m_free.splice(m_free.end(), m_encoding, it);
            break;
        }
    }
}



mfxU8 GetFrameType(
    MfxVideoParam const & video,
    mfxU32                frameOrder)
{
    mfxU32 gopOptFlag = MFX_GOP_CLOSED;//video.mfx.GopOptFlag;
    mfxU32 gopPicSize = video.mfx.GopPicSize;
    mfxU32 gopRefDist = video.mfx.GopRefDist;
    mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval + 1);

    //TODO: add B-ref

    if (gopPicSize == 0xffff) //infinite GOP
        idrPicDist = gopPicSize = 0xffffffff;

    if (frameOrder % idrPicDist == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (frameOrder % gopPicSize == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

    if (frameOrder % gopPicSize % gopRefDist == 0)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

    if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        if ((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED) ||
            (frameOrder + 1) % idrPicDist == 0)
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    return (MFX_FRAMETYPE_B);
}

void ConfigureTask(
    Task &                task,
    Task const &          prevTask,
    MfxVideoParam const & par)
{
    const bool isI    = !!(task.m_frameType & MFX_FRAMETYPE_I);
    const bool isP    = !!(task.m_frameType & MFX_FRAMETYPE_P);
    const bool isB    = !!(task.m_frameType & MFX_FRAMETYPE_B);
    const bool isRef  = !!(task.m_frameType & MFX_FRAMETYPE_REF);
    const bool isIDR  = !!(task.m_frameType & MFX_FRAMETYPE_IDR);

    // set coding type and QP
    if (isB)
    {
        task.m_codingType = 3; //TODO: add B1, B2
        task.m_qpY = (mfxU8)par.mfx.QPB;
    }
    else if (isP)
    {
        task.m_codingType = 2;
        task.m_qpY = (mfxU8)par.mfx.QPP;
    }
    else
    {
        assert(task.m_frameType & MFX_FRAMETYPE_I);
        task.m_codingType = 1;
        task.m_qpY = (mfxU8)par.mfx.QPI;
    }

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        task.m_qpY = 0;
    else if (task.m_ctrl.QP)
        task.m_qpY = (mfxU8)task.m_ctrl.QP;

    Copy(task.m_dpb[0], prevTask.m_dpb[1]);

    //construct ref lists
    task.m_numRefActive[0] = task.m_numRefActive[1] = 0;
    Fill(task.m_refPicList, IDX_INVALID);

    if (!isI)
    {
        mfxU8 l0 = 0, l1 = 0;

        for (mfxU8 i = 0; !isDpbEnd(task.m_dpb[0], i); i ++)
        {
            if (task.m_poc > task.m_dpb[0][i].m_poc)
                task.m_refPicList[0][l0++] = i;
            else if (isB)
                task.m_refPicList[1][l1++] = i;
        }

        assert(l0 > 0);

        if (isB && !l1)
        {
            task.m_refPicList[1][l1++] = task.m_refPicList[0][l0-1];
        }


        if (l0 > par.NumRefLX[0])
        {
            memmove(task.m_refPicList[0], task.m_refPicList[0] + l0 - par.NumRefLX[0], par.NumRefLX[0]);
            l0 = (mfxU8)par.NumRefLX[0];
            memset(task.m_refPicList[0] + l0, IDX_INVALID, sizeof(task.m_refPicList[0]) - l0);
        }
        if (l1 > par.NumRefLX[1])
        {
            memmove(task.m_refPicList[1], task.m_refPicList[1] + l1 - par.NumRefLX[1], par.NumRefLX[1]);
            l1 = (mfxU8)par.NumRefLX[1];
            memset(task.m_refPicList[1] + l1, IDX_INVALID, sizeof(task.m_refPicList[1]) - l1);
        }

        task.m_numRefActive[0] = l0;
        task.m_numRefActive[1] = l1;

        for (mfxU8 lx = 0; lx < 2; lx ++)
            for (mfxU8 i = 0; i < task.m_numRefActive[lx]; i ++)
                for (mfxU8 j = i; j < task.m_numRefActive[lx]; j ++)
                    if (  task.m_dpb[0][task.m_refPicList[lx][i]].m_poc
                        < task.m_dpb[0][task.m_refPicList[lx][j]].m_poc)
                        std::swap(task.m_refPicList[lx][i], task.m_refPicList[lx][j]);
    }


    // update dpb
    if (isI)
        Fill(task.m_dpb[1], IDX_INVALID);
    else
        Copy(task.m_dpb[1], task.m_dpb[0]);

    if (isRef)
    {
        for (mfxU16 i = 0; i < MAX_DPB_SIZE; i ++)
        {
            if (task.m_dpb[1][i].m_idxRec == IDX_INVALID)
            {
                if (i && i == par.mfx.NumRefFrame)
                    memmove(task.m_dpb[1], task.m_dpb[1] + 1, (--i) * sizeof(DpbFrame));

                task.m_dpb[1][i] = task;
                break;
            }
        }
    }

    task.m_shNUT = mfxU8(isIDR ? IDR_W_RADL 
        : task.m_poc >= 0 ? (isRef ? TRAIL_R : TRAIL_N)
        : (isRef ? RADL_R : RADL_N));

    par.GetSliceHeader(task, task.m_sh);
}

}; //namespace MfxHwH265Encode
