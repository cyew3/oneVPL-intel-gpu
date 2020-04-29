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
#include "av1ehw_g12_ddi_packer_win.h"
#include <algorithm>

using namespace AV1EHW;
using namespace AV1EHW::Gen12;
using namespace AV1EHW::Windows;
using namespace AV1EHW::Windows::Gen12;

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

mfxStatus DDIPacker::Register(
    VideoCORE& core
    , const mfxFrameAllocResponse& response
    , mfxU32 type)
{
    auto& res = m_resources[type];

    res.resize(response.NumFrameActual, {});

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = core.GetFrameHDL(response.mids[i], (mfxHDL*)&res[i]);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

void DDIPacker::InitAlloc(const FeatureBlocks&, TPushIA Push)
{
    Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(strg);

        m_vaType = core.GetVAType();

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

        ResetPackHeaderBuffer();
        m_cbd.resize(MAX_DDI_BUFFERS + m_buf.size());

        for (mfxU32 i = 0; i < m_resId.size(); i++)
            m_resId[i] = i;

        // Reserve space for feedback reports.
        ENCODE_QUERY_STATUS_PARAM_TYPE fbType = QUERY_STATUS_PARAM_FRAME;
        m_feedback.Reset(Glob::AllocBS::Get(strg).GetResponse().NumFrameActual, fbType);

        mfxStatus sts = Register(core, Glob::AllocRec::Get(strg).GetResponse(), MFX_FOURCC_NV12);
        MFX_CHECK_STS(sts);

        sts = Register(core, Glob::AllocBS::Get(strg).GetResponse(), ID_BITSTREAMDATA);
        MFX_CHECK_STS(sts);

        m_taskRes.resize(m_resources.at(MFX_FOURCC_NV12).size() + NUM_RES);

        if (!strg.Contains(Glob::DDI_Resources::Key))
            strg.Insert(Glob::DDI_Resources::Key, new Glob::DDI_Resources::TRef);

        auto& resources = Glob::DDI_Resources::Get(strg);

        for (auto& res : m_resources)
        {
            DDIExecParam rpar = {};
            rpar.Function = res.first;
            rpar.Resource.pData = res.second.data();
            rpar.Resource.Size = (mfxU32)res.second.size();

            resources.emplace_back(std::move(rpar));
        }

        auto pFB = make_storable<DDIFeedbackParam>(DDIFeedbackParam{});
        DDIExecParam submit;

        submit.Function = ENCODE_ENC_PAK_ID;
        submit.In.pData = &m_execPar;
        submit.In.Size = sizeof(m_execPar);

        m_query = {};
        m_query.StatusParamType = fbType;
        m_query.SizeOfStatusParamStruct = m_feedback.feedback_size();
        pFB->Function = ENCODE_QUERY_STATUS_ID;
        pFB->In.pData = &m_query;
        pFB->In.Size = sizeof(m_query);
        pFB->Out.pData = &m_feedback[0];
        pFB->Out.Size = (mfxU32)m_feedback.size() * m_feedback.feedback_size();
        pFB->Get = [this](mfxU32 id) { return m_feedback.Get(id); };
        pFB->Update = [this](mfxU32) { return m_feedback.Update(); };

        strg.Insert(Glob::DDI_Feedback::Key, std::move(pFB));

        if (!strg.Contains(Glob::DDI_SubmitParam::Key))
            strg.Insert(Glob::DDI_SubmitParam::Key, new Glob::DDI_SubmitParam::TRef);

        Glob::DDI_SubmitParam::Get(strg).emplace_back(std::move(submit));

        return MFX_ERR_NONE;
    });
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
    std::copy_n(seg.SegmentId, seg.NumSegmentIdAlloc, segment_map.begin());
}

static void FillSegmentationMap(
    const mfxExtAV1Segmentation& seg
    , std::vector<UCHAR>& segment_map)
{
    segment_map.resize(seg.NumSegmentIdAlloc);
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

        ResetPackHeaderBuffer();
        m_cbd.resize(MAX_DDI_BUFFERS + m_buf.size());

        return MFX_ERR_NONE;
    });
}

template<class T>
bool PackCBD(ENCODE_COMPBUFFERDESC& dst, D3DFORMAT id, std::vector<T>& src)
{
    dst = {};
    dst.CompressedBufferType = (id);
    dst.DataSize = UINT(sizeof(T) * src.size());
    dst.pCompBuffer = src.data();
    return true;
}

template<class T>
bool PackCBD(ENCODE_COMPBUFFERDESC& dst, D3DFORMAT id, T& src)
{
    dst = {};
    dst.CompressedBufferType = (id);
    dst.DataSize = (UINT)sizeof(T);
    dst.pCompBuffer = &src;
    return true;
}

mfxStatus DDIPacker::SetTaskResources(
    Task::Common::TRef& task
    , Glob::DDI_SubmitParam::TRef& submit)
{
    m_resId[RES_BS]   = task.BS.Idx;

    MFX_CHECK(m_vaType == MFX_HW_D3D11, MFX_ERR_NONE);

    auto& ppsCBD   = m_execPar.pCompressedBuffers[m_execPar.NumCompBuffers - 1];
    auto  IsEncPak = [](DDIExecParam& p) { return p.Function == ENCODE_ENC_PAK_ID; };
    auto  itExec   = std::find_if(submit.begin(), submit.end(), IsEncPak);

    ThrowAssert(itExec == submit.end()
        , "ENCODE_ENC_PAK_ID parameters not found");

    ThrowAssert(!m_execPar.NumCompBuffers || ppsCBD.CompressedBufferType != ID_PPSDATA
        , "prev. CBD must be PPSDATA");

    m_taskRes[m_resId[RES_BS]  = 0] = m_resources.at(ID_BITSTREAMDATA).at(task.BS.Idx).first;
    m_taskRes[m_resId[RES_RAW] = 1] = task.HDLRaw.first;

    mfxU32 resId = (m_resId[RES_REF] = 2);
    auto&  ref   = m_resources.at(MFX_FOURCC_NV12);

    std::transform(ref.begin(), ref.end(), &m_taskRes[resId], [](mfxHDLPair pair) { return pair.first; });

    resId += mfxU32(ref.size());

    itExec->Resource.pData = m_taskRes.data();
    itExec->Resource.Size  = resId;

    m_inputDesc                   = {};
    m_inputDesc.IndexOriginal     = m_resId[RES_RAW];
    m_inputDesc.ArraSliceOriginal = (UINT)(UINT_PTR)(task.HDLRaw.second);
    m_inputDesc.IndexRecon        = m_resId[RES_REF] + task.Rec.Idx;
    m_inputDesc.ArraySliceRecon   = (UINT)(UINT_PTR)(ref.at(task.Rec.Idx).second);

    ppsCBD.pReserved = &m_inputDesc;

    return MFX_ERR_NONE;
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
        FillSegmentationMap(seg, m_segment_map);

        m_execPar = {};
        m_execPar.pCompressedBuffers = m_cbd.data();

        auto& nCBD    = m_execPar.NumCompBuffers;
        auto  CbdBack = [&]() -> ENCODE_COMPBUFFERDESC& { return m_cbd.at(nCBD); };

        nCBD += PackCBD(CbdBack(), ID_SPSDATA, m_sps);
        nCBD += PackCBD(CbdBack(), ID_PPSDATA, m_pps);

        auto sts = SetTaskResources(task, Glob::DDI_SubmitParam::Get(global));
        MFX_CHECK_STS(sts);

        if (!m_tile_groups_task.empty())
            nCBD += PackCBD(CbdBack(), ID_TILEGROUPDATA, m_tile_groups_task);
        else if (!m_tile_groups_global.empty())
            nCBD += PackCBD(CbdBack(), ID_TILEGROUPDATA, m_tile_groups_global);

        if (!m_segment_map.empty())
            nCBD += PackCBD(CbdBack(), ID_SEGMENTMAP, m_segment_map);

        nCBD += PackCBD(CbdBack(), ID_BITSTREAMDATA, m_resId[RES_BS]);

        const bool insertIVF = (task.InsertHeaders & INSERT_IVF_SEQ) || (task.InsertHeaders & INSERT_IVF_FRM);
        nCBD += (insertIVF)
            && PackCBD(CbdBack(), ID_PACKEDHEADERDATA, PackHeader(ph.IVF));

        nCBD += (task.InsertHeaders & INSERT_SPS)
            && PackCBD(CbdBack(), ID_PACKEDHEADERDATA, PackHeader(ph.SPS));

        nCBD += (task.InsertHeaders & INSERT_PPS)
            && PackCBD(CbdBack(), ID_PACKEDHEADERDATA, PackHeader(ph.PPS));

        return MFX_ERR_NONE;
    });
}

void DDIPacker::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(!Glob::DDI_Feedback::Get(global).bNotReady, MFX_TASK_BUSY);

        auto& task = Task::Common::Get(s_task);

        mfxStatus   sts      = MFX_ERR_NONE;
        auto        bsInfo   = Glob::AllocBS::Get(global).GetInfo();
        auto        pFeedback = m_feedback.Get(task.StatusReportId);

        MFX_CHECK(pFeedback != 0, MFX_ERR_DEVICE_FAILED);

        bool bFeedbackValid =
            (pFeedback->bStatus == ENCODE_OK)
            && (mfxU32(bsInfo.Width * bsInfo.Height) >= pFeedback->bitstreamSize)
            && (pFeedback->bitstreamSize > 0);

        MFX_CHECK(pFeedback->bStatus != ENCODE_NOTREADY, MFX_TASK_BUSY);
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

        task.BsDataLength = pFeedback->bitstreamSize;

        sts = mfxStatus(!bFeedbackValid * MFX_ERR_DEVICE_FAILED);

        if (sts < MFX_ERR_NONE)
        {
            Glob::RTErr::Get(global) = sts;
        }

        m_feedback.Remove(task.StatusReportId);

        return sts;
    });
}

void DDIPacker::NewHeader()
{
    assert(m_buf.size());

    if (++m_cur == m_buf.end())
        m_cur = m_buf.begin();

    *m_cur = {};
}

void DDIPacker::ResetPackHeaderBuffer()
{
    m_buf.resize(MAX_HEADER_BUFFERS);
    m_cur = m_buf.begin();
}

ENCODE_PACKEDHEADER_DATA& DDIPacker::PackHeader(const PackedData& d)
{
    NewHeader();

    m_cur->pData = d.pData;
    m_cur->DataLength = CeilDiv(d.BitLen, 8u);
    m_cur->BufferSize = m_cur->DataLength;
    m_cur->SkipEmulationByteCount = 3 + d.bLongSC;

    return *m_cur;
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
}

inline void FillRefCtrlL1(
    const TaskCommonPar& task
    , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps)
{
    if (!IsB(task.FrameType))
        return;

    mfxU8 idx = 0;
    for (mfxU8 refFrame = BWDREF_FRAME; refFrame < MAX_REF_FRAMES; refFrame++)
    {
        if (IsValidRefFrame(task.RefList, refFrame)) // Assume search order is same as ref_list order. Todo: to align the search order with HW capability.
            FillSearchIdx(pps.ref_frame_ctrl_l1, idx++, refFrame);
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

void FeedbackStorage::Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType)
{
    if (fbType != QUERY_STATUS_PARAM_FRAME) {
        assert(!"unknown query function");
        fbType = QUERY_STATUS_PARAM_FRAME;
    }

    m_type = fbType;
    m_pool_size = cacheSize;

    m_buf.resize(m_fb_size * m_pool_size);
    m_buf_cache.resize(m_fb_size * m_pool_size);

    for (size_t i = 0; i < m_pool_size; i++)
    {
        Feedback *c = (Feedback *)&m_buf_cache[i * m_fb_size];
        c->bStatus = ENCODE_NOTAVAILABLE;
    }
}

const FeedbackStorage::Feedback* FeedbackStorage::Get(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_pool_size; i++)
    {
        Feedback *pFb = (Feedback *)&m_buf_cache[i * m_fb_size];

        if (pFb->StatusReportFeedbackNumber == feedbackNumber)
            return pFb;
    }
    return 0;
}

mfxStatus FeedbackStorage::Update()
{
    for (size_t i = 0; i < m_pool_size; i++)
    {
        Feedback *u = (Feedback *)&m_buf[i * m_fb_size];

        if (u->bStatus != ENCODE_NOTAVAILABLE)
        {
            Feedback *hit = 0;

            for (size_t j = 0; j < m_pool_size; j++)
            {
                Feedback *c = (Feedback *)&m_buf_cache[j * m_fb_size];

                if (c->StatusReportFeedbackNumber == u->StatusReportFeedbackNumber)
                {
                    hit = c;  // hit
                    break;
                }
                else if (hit == 0 && c->bStatus == ENCODE_NOTAVAILABLE)
                {
                    hit = c;  // first free
                }
            }

            MFX_CHECK(hit != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            CacheFeedback(hit, u);
        }
    }
    return MFX_ERR_NONE;
}

// copy fb into cache
inline void FeedbackStorage::CacheFeedback(Feedback *fb_dst, Feedback *fb_src)
{
    *fb_dst = *fb_src;
}

mfxStatus FeedbackStorage::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_pool_size; i++)
    {
        Feedback *c = (Feedback *)&m_buf_cache[i * m_fb_size];

        if (c->StatusReportFeedbackNumber == feedbackNumber)
        {
            c->bStatus = ENCODE_NOTAVAILABLE;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
