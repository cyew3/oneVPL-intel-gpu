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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)
#include "av1ehw_base_ddi_packer_win.h"
#include <algorithm>

using namespace AV1EHW;
using namespace AV1EHW::Base;
using namespace AV1EHW::Windows;
using namespace AV1EHW::Windows::Base;

void DDIPacker::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_HardcodeCaps
        , [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto  hw   = Glob::VideoCore::Get(strg).GetHWType();
        auto& caps = Glob::EncodeCaps::Get(strg);

        caps.msdk.CQPSupport = true;
        caps.msdk.CBRSupport = false;
        caps.msdk.VBRSupport = false;
        caps.msdk.ICQSupport = false;

        m_hwType = hw;

        return MFX_ERR_NONE;
    });
}

void DDIPacker::InitAlloc(const FeatureBlocks&, TPushIA Push)
{
    Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(strg);

        SetVaType(core.GetVAType());

        if (m_vaType == MFX_HW_D3D11)
        {
            ID_SPSDATA          = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA;
            ID_PPSDATA          = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA;
            ID_TILEGROUPDATA    = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA;
            ID_SEGMENTMAP       = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBSEGMENTMAP;
            ID_BITSTREAMDATA    = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA;
            ID_PACKEDHEADERDATA = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
        }

        const auto& pInfos = Glob::TileGroups::Get(strg);
        FillTileGroupBuffer(pInfos, m_tile_groups_global);

        const auto& par     = Glob::VideoParam::Get(strg);
        const auto& bs_sh  = Glob::SH::Get(strg);
        const auto& bs_fh  = Glob::FH::Get(strg);
        FillSpsBuffer(par, bs_sh, m_sps);
        FillPpsBuffer(par, bs_fh, m_pps);

        auto& bsInfo = Glob::AllocBS::Get(strg);
        SetMaxBs(bsInfo.GetInfo().Width * bsInfo.GetInfo().Height);
        InitFeedback(bsInfo.GetResponse().NumFrameActual, QUERY_STATUS_PARAM_FRAME, 0);

        mfxStatus sts = Register(core, Glob::AllocRec::Get(strg).GetResponse(), RES_REF);
        MFX_CHECK_STS(sts);

        sts = Register(core, bsInfo.GetResponse(), RES_BS);
        MFX_CHECK_STS(sts);

        if (strg.Contains(Glob::AllocMBQP::Key))
        {
            sts = Register(core, Glob::AllocMBQP::Get(strg).GetResponse(), RES_CUQP);
            MFX_CHECK_STS(sts);
        }

        const std::map<mfxU32, mfxU32> mapResId =
        {
              { mfxU32(RES_REF),  mfxU32(MFX_FOURCC_NV12) }
            , { mfxU32(RES_BS),   mfxU32(ID_BITSTREAMDATA) }
        };
        auto& resources = Glob::DDI_Resources::GetOrConstruct(strg);

        for (auto& res : m_resources)
        {
            DDIExecParam rpar = {};
            rpar.Function       = mapResId.at(res.first);
            rpar.Resource.pData = res.second.data();
            rpar.Resource.Size  = (mfxU32)res.second.size();

            resources.emplace_back(std::move(rpar));
        }

        GetFeedbackInterface(Glob::DDI_Feedback::GetOrConstruct(strg));

        DDIExecParam submit;

        submit.Function = ENCODE_ENC_PAK_ID;
        submit.In.pData = EndPicture();
        submit.In.Size  = sizeof(ENCODE_EXECUTE_PARAMS);

        Glob::DDI_SubmitParam::GetOrConstruct(strg).emplace_back(std::move(submit));

        return MFX_ERR_NONE;

    });
}

static mfxU32 MapSegIdBlockSizeToDDI(mfxU16 size)
{
    switch (size)
    {
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_8x8:
        return BLOCK_8x8;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32:
        return BLOCK_32x32;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_64x64:
        return BLOCK_64x64;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_16x16:
    default:
        return BLOCK_16x16;
    }
}

static void FillSegmentationParam(
    const mfxExtAV1Segmentation& seg
    , const FH& bs_fh
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    pps.stAV1Segments = DXVA_Intel_Seg_AV1{};

    if (!bs_fh.segmentation_params.segmentation_enabled)
        return;

    auto& segFlags = pps.stAV1Segments.SegmentFlags.fields;
    segFlags.segmentation_enabled = bs_fh.segmentation_params.segmentation_enabled;

    segFlags.SegmentNumber = seg.NumSegments;
    pps.PicFlags.fields.SegIdBlockSize = MapSegIdBlockSizeToDDI(seg.SegmentIdBlockSize);

    segFlags.update_map = bs_fh.segmentation_params.segmentation_update_map;
    segFlags.temporal_update = bs_fh.segmentation_params.segmentation_temporal_update;

    std::transform(bs_fh.segmentation_params.FeatureMask, bs_fh.segmentation_params.FeatureMask + AV1_MAX_NUM_OF_SEGMENTS,
        pps.stAV1Segments.feature_mask,
        [](uint32_t x) -> UCHAR {
        return static_cast<UCHAR>(x);
    });

    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; ++i) {
        std::transform(bs_fh.segmentation_params.FeatureData[i], bs_fh.segmentation_params.FeatureData[i] + SEG_LVL_MAX,
            pps.stAV1Segments.feature_data[i],
            [](uint32_t x) -> SHORT {
            return static_cast<SHORT>(x);
        });
    }
}

static void FillSegmentationMap(
    const ExtBuffer::Param<mfxVideoParam>& par
    , std::vector<UCHAR>& segment_map)
{
    const mfxExtAV1Param& av1Par = ExtBuffer::Get(par);
    if (av1Par.SegmentationMode != MFX_AV1_SEGMENT_MANUAL)
        return;

    const mfxExtAV1Segmentation& seg = ExtBuffer::Get(par);
    segment_map.resize(seg.NumSegmentIdAlloc);
    if (seg.SegmentId)
        std::copy_n(seg.SegmentId, seg.NumSegmentIdAlloc, segment_map.begin());
}

static void FillSegmentationMap(
    const mfxExtAV1Segmentation& seg
    , std::vector<UCHAR>& segment_map)
{
    segment_map.resize(seg.NumSegmentIdAlloc);
    if (seg.SegmentId)
        std::copy_n(seg.SegmentId, seg.NumSegmentIdAlloc, segment_map.begin());
}

void DDIPacker::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        const auto& par = Glob::VideoParam::Get(strg);
        const auto& bs_sh = Glob::SH::Get(strg);
        const auto& bs_fh = Glob::FH::Get(strg);

        const auto& pInfos = Glob::TileGroups::Get(strg);
        FillTileGroupBuffer(pInfos, m_tile_groups_global);

        FillSpsBuffer(par, bs_sh, m_sps);
        FillPpsBuffer(par, bs_fh, m_pps);
        FillSegmentationMap(par, m_segment_map);

        return MFX_ERR_NONE;
    });
}


void DDIPacker::SubmitTask(const FeatureBlocks& blocks, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this, &blocks](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task  = Task::Common::Get(s_task);
        auto& ph    = Glob::PackedHeaders::Get(global);
        auto& bs_sh = Glob::SH::Get(global);
        auto& bs_fh = Task::FH::Get(s_task);

        AV1E_LOG("*DEBUG* task[%d] Submitting: %dx%d, frame_type=%d\n", task.StatusReportId, bs_fh.FrameWidth, bs_fh.FrameHeight, bs_fh.frame_type);

        const auto& tileGroupInfos = Task::TileGroups::Get(s_task);
        if (tileGroupInfos.empty())
            m_tile_groups_task.clear();
        else
            FillTileGroupBuffer(tileGroupInfos, m_tile_groups_task);

        FillPpsBuffer(task, bs_sh, bs_fh, m_pps);
        const auto& seg = Task::Segment::Get(s_task);
        FillSegmentationParam(seg, bs_fh, m_pps);
        FillSegmentationMap(seg, m_segment_map);

        mfxU32 nCBD = 0;
        BeginPicture();

        nCBD += PackCBD(ID_SPSDATA, m_sps);
        nCBD += PackCBD(ID_PPSDATA, m_pps);

        auto& submitPar = Glob::DDI_SubmitParam::Get(global);

        auto itEncPak = std::find_if(submitPar.begin(), submitPar.end(), DDIExecParam::IsFunction<ENCODE_ENC_PAK_ID>);
        ThrowAssert(itEncPak == submitPar.end(), "ENCODE_ENC_PAK_ID parameters not found");

        auto  itPPS = std::find_if(m_cbd.begin(), m_cbd.end()
            , [&](ENCODE_COMPBUFFERDESC& cbd) { return cbd.CompressedBufferType == ID_PPSDATA; });
        ThrowAssert(itPPS == m_cbd.end(), "PPSDATA not found");

        itPPS->pReserved = SetTaskResources(itEncPak->Resource, task.HDLRaw, task.BS.Idx, task.Rec.Idx, task.CUQP.Idx);

        if (!m_tile_groups_task.empty())
            nCBD += PackCBD(ID_TILEGROUPDATA, m_tile_groups_task);
        else if (!m_tile_groups_global.empty())
            nCBD += PackCBD(ID_TILEGROUPDATA, m_tile_groups_global);

        if (!m_segment_map.empty())
            nCBD += PackCBD(ID_SEGMENTMAP, m_segment_map);

        nCBD += PackCBD(ID_BITSTREAMDATA, m_resId[RES_BS]);

        const bool insertIVF = (task.InsertHeaders & INSERT_IVF_SEQ) || (task.InsertHeaders & INSERT_IVF_FRM);
        nCBD += (insertIVF)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.IVF, true));

        nCBD += (task.InsertHeaders & INSERT_SPS)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.SPS, true));

        nCBD += (task.InsertHeaders & INSERT_PPS)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.PPS, true));

        itEncPak->In.pData = EndPicture();
        itEncPak->In.Size  = sizeof(ENCODE_EXECUTE_PARAMS);

        return MFX_ERR_NONE;
    });
}

void DDIPacker::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& fb = Glob::DDI_Feedback::Get(global);
        MFX_CHECK(!fb.bNotReady, MFX_TASK_BUSY);

        auto& task = Task::Common::Get(s_task);

        MFX_CHECK((task.SkipCMD & SKIPCMD_NeedDriverCall), MFX_ERR_NONE);

        auto pFeedback = static_cast<const ENCODE_QUERY_STATUS_PARAMS_AV1*>(fb.Get(task.StatusReportId));
        auto sts = ReadFeedback(pFeedback, m_feedback.GetFeedBackSize(), task.BsDataLength);

        if (sts < MFX_ERR_NONE)
        {
            Glob::RTErr::Get(global) = sts;
        }

        switch (pFeedback->bStatus)
        {
        case ENCODE_OK:
            AV1E_LOG("  *DEBUG* task[%d] STATUS: ENCODE_OK (encoded size: %d bytes)!\n", task.StatusReportId, pFeedback->bitstreamSize);
            break;
        case ENCODE_ERROR:
            AV1E_LOG("**FATAL** task[%d]: STATUS: ENCODE_ERROR (See QUERY_STATUS_PARAM_FRAME request)!\n", task.StatusReportId);
            break;
        case ENCODE_NOTAVAILABLE:
        default:
            AV1E_LOG("**FATAL** task[%d]: STATUS: BAD STATUS %d (See QUERY_STATUS_PARAM_FRAME request)\n", task.StatusReportId, pFeedback->bStatus);
        }

        fb.Remove(task.StatusReportId);

        return sts;
    });
}

inline mfxU8 MapRateControlMethodToDDI(mfxU16 brcMethod)
{
    switch (brcMethod)
    {
    case MFX_RATECONTROL_CBR:
        return 1;
    case MFX_RATECONTROL_VBR:
        return 2;
    default:
        return 0;
    }
}

void DDIPacker::FillSpsBuffer(
    const ExtBuffer::Param<mfxVideoParam>& par
    , const SH& bs_sh
    , ENCODE_SET_SEQUENCE_PARAMETERS_AV1 & sps)
{
    sps = {};

    sps.seq_profile = static_cast<UCHAR>(bs_sh.seq_profile);
    sps.seq_level_idx = 0;

    sps.order_hint_bits_minus_1 = static_cast<UCHAR>(bs_sh.order_hint_bits_minus1);
    sps.CodingToolFlags.fields.enable_order_hint = bs_sh.enable_order_hint;
    sps.CodingToolFlags.fields.enable_superres = bs_sh.enable_superres;
    sps.CodingToolFlags.fields.enable_cdef = bs_sh.enable_cdef;
    sps.CodingToolFlags.fields.enable_restoration = bs_sh.enable_restoration;
    sps.CodingToolFlags.fields.enable_warped_motion = bs_sh.enable_warped_motion;

    sps.GopPicSize = par.mfx.GopPicSize;
    sps.TargetUsage = static_cast<UCHAR>(par.mfx.TargetUsage);
    sps.RateControlMethod = MapRateControlMethodToDDI(par.mfx.RateControlMethod);

    mfxU32 nom = par.mfx.FrameInfo.FrameRateExtN;
    mfxU32 denom = par.mfx.FrameInfo.FrameRateExtD;

    sps.FrameRate[0].Numerator = nom;
    sps.FrameRate[0].Denominator = denom;

    //TODO: implement BRC and temporal layer related parameters in future
    sps.SeqFlags.fields.bResetBRC = 0;
    sps.NumTemporalLayersMinus1 = 0;
}

void DDIPacker::FillPpsBuffer(
    const ExtBuffer::Param<mfxVideoParam>& par
    , const FH& bs_fh
    , ENCODE_SET_PICTURE_PARAMETERS_AV1 & pps)
{
    par;
    pps = {};

    //frame size
    pps.frame_height_minus1 = static_cast<USHORT>(bs_fh.FrameHeight - 1);
    pps.frame_width_minus1 = static_cast<USHORT>(bs_fh.FrameWidth - 1);

    //quantizer
    pps.y_dc_delta_q = static_cast<CHAR>(bs_fh.quantization_params.DeltaQYDc);
    pps.u_dc_delta_q =  static_cast<CHAR>(bs_fh.quantization_params.DeltaQUDc);
    pps.u_ac_delta_q =  static_cast<CHAR>(bs_fh.quantization_params.DeltaQUAc);
    pps.v_dc_delta_q =  static_cast<CHAR>(bs_fh.quantization_params.DeltaQVDc);
    pps.v_ac_delta_q =  static_cast<CHAR>(bs_fh.quantization_params.DeltaQVAc);

    //other params
    pps.PicFlags.fields.error_resilient_mode = bs_fh.error_resilient_mode;
    pps.interp_filter = static_cast<UCHAR>(bs_fh.interpolation_filter);
    pps.PicFlags.fields.allow_high_precision_mv = bs_fh.allow_high_precision_mv;
    pps.PicFlags.fields.reduced_tx_set_used = bs_fh.reduced_tx_set;
    //description for tx_mode in DDI 0.04 is outdated (lists TX modes not supported by 1.0.0.errata1)
    pps.dwModeControlFlags.fields.tx_mode = bs_fh.TxMode;
    pps.temporal_id = 0;
}

inline void FillSearchIdx(
    RefFrameCtrl& refFrameCtrl, mfxU8 idx, mfxU8 refFrame)
{
    switch (idx)
    {
    case 0:
        refFrameCtrl.fields.search_idx0 = refFrame;
        break;
    case 1:
        refFrameCtrl.fields.search_idx1 = refFrame;
        break;
    case 2:
        refFrameCtrl.fields.search_idx2 = refFrame;
        break;
    case 3:
        refFrameCtrl.fields.search_idx3 = refFrame;
        break;
    case 4:
        refFrameCtrl.fields.search_idx4 = refFrame;
        break;
    case 5:
        refFrameCtrl.fields.search_idx5 = refFrame;
        break;
    case 6:
        refFrameCtrl.fields.search_idx6 = refFrame;
        break;
    default:
        assert(!"Invalid index");
    }
}

inline void FillRefCtrlL0(
    const TaskCommonPar& task
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    mfxU8 idx = 0;
    for (mfxU8 refFrame = LAST_FRAME; refFrame < BWDREF_FRAME; refFrame++)
    {
        if (IsValidRefFrame(task.RefList, refFrame)) // Assume search order is same as ref_list order. Todo: to align the search order with HW capability.
            FillSearchIdx(pps.ref_frame_ctrl_l0, idx++, refFrame);
    }

    if (IsP(task.FrameType) && !task.isLDB)  // not RAB frame
    {
        mfxU8 refFrame = ALTREF_FRAME;
        if (IsValidRefFrame(task.RefList, refFrame))
            FillSearchIdx(pps.ref_frame_ctrl_l0, idx, refFrame);
     }
}

inline void FillRefCtrlL1(
    const TaskCommonPar& task
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    if (IsB(task.FrameType) || task.isLDB)
    {
        mfxU8 idx = 0;
        for (mfxU8 refFrame = BWDREF_FRAME; refFrame < MAX_REF_FRAMES; refFrame++)
        {
            if (IsValidRefFrame(task.RefList, refFrame)) // Assume search order is same as ref_list order. Todo: to align the search order with HW capability.
                FillSearchIdx(pps.ref_frame_ctrl_l1, idx++, refFrame);
        }
    }
}

inline void FillRefParams(
    const TaskCommonPar& task
    , const FH& bs_fh
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    std::fill(pps.RefFrameList, pps.RefFrameList + NUM_REF_FRAMES, IDX_INVALID);

    if (pps.PicFlags.fields.frame_type == KEY_FRAME)
        return;

    std::transform(task.DPB.begin(), task.DPB.end(), pps.RefFrameList,
        [](DpbType::const_reference dpbFrm) -> UCHAR
        {
            return static_cast<UCHAR>(dpbFrm->Rec.Idx);
        });

    std::transform(bs_fh.ref_frame_idx, bs_fh.ref_frame_idx + REFS_PER_FRAME, pps.ref_frame_idx,
        [](const int32_t idx) -> UCHAR
        {
            return static_cast<UCHAR>(idx);
        });

    FillRefCtrlL0(task, pps);
    FillRefCtrlL1(task, pps);
}

inline void FillTile(
    const FH& bs_fh
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    auto& ti = bs_fh.tile_info;
    pps.tile_cols = static_cast<USHORT>(ti.TileCols);
    for (mfxU16 i = 0; i < pps.tile_cols; i++)
        pps.width_in_sbs_minus_1[i] = static_cast<USHORT>(ti.TileWidthInSB[i] - 1);

    pps.tile_rows = static_cast<USHORT>(ti.TileRows);
    for (mfxU16 i = 0; i < pps.tile_rows; i++)
        pps.height_in_sbs_minus_1[i] = static_cast<USHORT>(ti.TileHeightInSB[i] - 1);

    pps.context_update_tile_id = static_cast<UCHAR>(ti.context_update_tile_id);
}

inline void FillCDEF(
    const SH& bs_sh
    , const FH& bs_fh
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    if (!bs_sh.enable_cdef)
        return;

    auto& cdef = bs_fh.cdef_params;
    pps.cdef_damping_minus_3 = static_cast<UCHAR>(cdef.cdef_damping - 3);
    pps.cdef_bits = static_cast<UCHAR>(cdef.cdef_bits);

    for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        pps.cdef_y_strengths[i] = static_cast<UCHAR>(cdef.cdef_y_pri_strength[i] * CDEF_STRENGTH_DIVISOR + cdef.cdef_y_sec_strength[i]);
        pps.cdef_uv_strengths[i] = static_cast<UCHAR>(cdef.cdef_uv_pri_strength[i] * CDEF_STRENGTH_DIVISOR + cdef.cdef_uv_sec_strength[i]);
    }
}

void DDIPacker::FillPpsBuffer(
    const TaskCommonPar& task
    , const SH& bs_sh
    , const FH& bs_fh
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    pps.PicFlags.fields.frame_type = bs_fh.frame_type;
    pps.base_qindex = static_cast<USHORT>(bs_fh.quantization_params.base_q_idx);
    pps.MinBaseQIndex = task.MinBaseQIndex;
    pps.MaxBaseQIndex = task.MaxBaseQIndex;

    pps.order_hint = static_cast<UCHAR>(bs_fh.order_hint);

    //DPB and refs management
    pps.primary_ref_frame = static_cast<UCHAR>(bs_fh.primary_ref_frame);
    pps.ref_frame_ctrl_l0.value = 0;
    pps.ref_frame_ctrl_l1.value = 0;
    FillRefParams(task, bs_fh, pps);

    //loop filter
    auto& lf = bs_fh.loop_filter_params;
    pps.filter_level[0] = static_cast<UCHAR>(lf.loop_filter_level[0]);
    pps.filter_level[1] = static_cast<UCHAR>(lf.loop_filter_level[1]);
    pps.filter_level_u = static_cast<UCHAR>(lf.loop_filter_level[2]);
    pps.filter_level_v = static_cast<UCHAR>(lf.loop_filter_level[3]);
    pps.cLoopFilterInfoFlags.fields.sharpness_level = lf.loop_filter_sharpness;
    pps.cLoopFilterInfoFlags.fields.mode_ref_delta_enabled = lf.loop_filter_delta_enabled;
    pps.cLoopFilterInfoFlags.fields.mode_ref_delta_update = lf.loop_filter_delta_update;

    std::copy(lf.loop_filter_ref_deltas, lf.loop_filter_ref_deltas + TOTAL_REFS_PER_FRAME, pps.ref_deltas);
    std::copy(lf.loop_filter_mode_deltas, lf.loop_filter_mode_deltas + MAX_MODE_LF_DELTAS, pps.mode_deltas);

    // block-level deltas
    pps.dwModeControlFlags.fields.delta_q_present_flag = bs_fh.delta_q_present;
    pps.dwModeControlFlags.fields.delta_lf_present_flag = bs_fh.delta_lf_present;
    pps.dwModeControlFlags.fields.delta_lf_multi = bs_fh.delta_lf_multi;

    pps.dwModeControlFlags.fields.reference_mode = bs_fh.reference_select ?
        REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
    pps.dwModeControlFlags.fields.skip_mode_present = bs_fh.skip_mode_present;

    FillTile(bs_fh, pps);
    FillCDEF(bs_sh, bs_fh, pps);

    //context
    pps.PicFlags.fields.disable_cdf_update = bs_fh.disable_cdf_update;
    pps.PicFlags.fields.disable_frame_end_update_cdf = bs_fh.disable_frame_end_update_cdf;

    // Tile Groups
    if (!m_tile_groups_task.empty())
        pps.NumTileGroupsMinus1 = static_cast<UCHAR>(m_tile_groups_task.size() - 1);
    else
        pps.NumTileGroupsMinus1 = static_cast<UCHAR>(m_tile_groups_global.size() - 1);

    pps.TileGroupOBUHdrInfo.fields.obu_extension_flag = 0;
    pps.TileGroupOBUHdrInfo.fields.obu_has_size_field = 1;
    pps.TileGroupOBUHdrInfo.fields.temporal_id = 0;
    pps.TileGroupOBUHdrInfo.fields.spatial_id = 0;

    //other params
    pps.PicFlags.fields.error_resilient_mode = bs_fh.error_resilient_mode;
    pps.StatusReportFeedbackNumber = task.StatusReportId;
    pps.CurrOriginalPic = task.Rec.Idx;
    pps.CurrReconstructedPic = task.Rec.Idx;
    pps.PicFlags.fields.EnableFrameOBU = (task.InsertHeaders & INSERT_FRM_OBU) ? 1 : 0;

    //offsets
    auto& offsets = task.Offsets;
    pps.FrameHdrOBUSizeByteOffset = offsets.FrameHdrOBUSizeByteOffset;
    pps.LoopFilterParamsBitOffset = offsets.LoopFilterParamsBitOffset;
    pps.QIndexBitOffset = offsets.QIndexBitOffset;
    pps.SegmentationBitOffset = offsets.SegmentationBitOffset;
    pps.CDEFParamsBitOffset = offsets.CDEFParamsBitOffset;
    pps.CDEFParamsSizeInBits = static_cast<UCHAR>(offsets.CDEFParamsSizeInBits);
}

void DDIPacker::FillTileGroupBuffer(
    const TileGroupInfos& infos
    , std::vector<ENCODE_SET_TILE_GROUP_HEADER_AV1>& tile_groups)
{
    tile_groups.resize(infos.size());

    for (mfxU16 i = 0; i < infos.size(); i++)
    {
        ENCODE_SET_TILE_GROUP_HEADER_AV1& tg = tile_groups[i];
        tg = {};

        tg.tg_start = static_cast<UCHAR>(infos[i].TgStart);
        tg.tg_end = static_cast<UCHAR>(infos[i].TgEnd);
    }
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
