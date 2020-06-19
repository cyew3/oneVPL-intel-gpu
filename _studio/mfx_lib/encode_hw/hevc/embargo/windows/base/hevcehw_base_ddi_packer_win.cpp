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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)
#include "hevcehw_base_ddi_packer_win.h"
#include <numeric>

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Base;

void DDIPacker::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_HardcodeCaps
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
    {
        auto  hw   = Glob::VideoCore::Get(strg).GetHWType();
        auto& caps = Glob::EncodeCaps::Get(strg);

        HardcodeCapsCommon(caps, hw, par);

        caps.msdk.CBRSupport = true;
        caps.msdk.VBRSupport = true;
        caps.msdk.CQPSupport = true;
        caps.msdk.ICQSupport = true;

        caps.MaxEncodedBitDepth |= ((hw >= MFX_HW_KBL) && (hw < MFX_HW_CNL));

        m_hwType = hw;

        return MFX_ERR_NONE;
    });
}

void DDIPacker::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetCallChains
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(!strg.Contains(CC::Key), MFX_ERR_NONE);

        auto& cc = CC::GetOrConstruct(strg);

        cc.InitSPS.Push([this](
            CallChains::TInitSPS::TExt
            , const StorageR& glob
            , ENCODE_SET_SEQUENCE_PARAMETERS_HEVC& sps)
        {
            FillSpsBuffer(Glob::VideoParam::Get(glob), Glob::SPS::Get(glob), sps);
        });
        cc.InitPPS.Push([this](
            CallChains::TInitPPS::TExt
            , const StorageR& glob
            , ENCODE_SET_PICTURE_PARAMETERS_HEVC& pps)
        {
            FillPpsBuffer(Glob::VideoParam::Get(glob), Glob::PPS::Get(glob), pps);
        });
        cc.UpdatePPS.Push([this](
            CallChains::TUpdatePPS::TExt
            , const StorageR& global
            , const StorageR& s_task
            , const ENCODE_SET_SEQUENCE_PARAMETERS_HEVC& /*sps*/
            , ENCODE_SET_PICTURE_PARAMETERS_HEVC& pps)
        {
            FillPpsBuffer(Task::Common::Get(s_task), Task::SSH::Get(s_task), Glob::EncodeCaps::Get(global), pps);
        });
        cc.InitFeedback.Push([this](
            CallChains::TInitFeedback::TExt
            , const StorageR&
            , mfxU16 cacheSize
            , ENCODE_QUERY_STATUS_PARAM_TYPE fbType
            , mfxU32 maxSlices)
        {
            InitFeedback(cacheSize, fbType, maxSlices);
        });
        cc.ReadFeedback.Push([this](
            CallChains::TReadFeedback::TExt
            , const StorageR& /*global*/
            , StorageW& s_task
            , const void* pFB
            , mfxU32 size)
        {
            return ReadFeedback(pFB, size, Task::Common::Get(s_task).BsDataLength);
        });
        cc.UpdateSPS.Push([this](
            CallChains::TUpdateSPS::TExt
            , const StorageR& /*global*/
            , ENCODE_SET_SEQUENCE_PARAMETERS_HEVC& /*sps*/)
        {});
        cc.UpdateCqmHint.Push([this](
            CallChains::TUpdateCqmHint::TExt
            , TaskCommonPar& /*task*/
            , const LOOKAHEAD_INFO& /*pLPLA*/)
        {});

        return MFX_ERR_NONE;
    });
}

void DDIPacker::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
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
            ID_SLICEDATA        = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA;
            ID_BITSTREAMDATA    = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA;
            ID_MBQPDATA         = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA;
            ID_PACKEDHEADERDATA = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
            ID_PACKEDSLICEDATA  = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA;
            ID_QUANTDATA        = (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA;
        }

        const auto& par = Glob::VideoParam::Get(strg);
        const auto& cc  = CC::Get(strg);

        cc.InitSPS(strg, m_sps);
        cc.InitPPS(strg, m_pps);
        cc.UpdateSPS(strg, m_sps);
        FillSliceBuffer(Glob::SliceInfo::Get(strg), m_slices);

        // Reserve space for feedback reports.
        const mfxExtHEVCParam& hpar = ExtBuffer::Get(par);
        ENCODE_QUERY_STATUS_PARAM_TYPE fbType =
              m_pps.bEnableSliceLevelReport
            ? QUERY_STATUS_PARAM_SLICE
            : QUERY_STATUS_PARAM_FRAME;
        mfxU32 maxSlices = CeilDiv(hpar.PicHeightInLumaSamples, hpar.LCUSize) * CeilDiv(hpar.PicWidthInLumaSamples, hpar.LCUSize);
        maxSlices = std::min<mfxU32>(maxSlices, MAX_SLICES);

        cc.InitFeedback(strg, Glob::AllocBS::Get(strg).GetResponse().NumFrameActual, fbType, maxSlices);
        auto& bsInfo = Glob::AllocBS::Get(strg);

        SetMaxBs(bsInfo.GetInfo().Width * bsInfo.GetInfo().Height);

        cc.InitFeedback(strg, bsInfo.GetResponse().NumFrameActual, fbType, maxSlices);

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
            , { mfxU32(RES_CUQP), mfxU32(ID_MBQPDATA) }
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

void DDIPacker::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        const auto& cc = CC::Get(strg);

        cc.InitSPS(strg, m_sps);
        cc.InitPPS(strg, m_pps);
        FillSliceBuffer(Glob::SliceInfo::Get(strg), m_slices);

        m_bResetBRC = !!(Glob::ResetHint::Get(strg).Flags & RF_BRC_RESET);

        return MFX_ERR_NONE;
    });
}

void DDIPacker::SubmitTask(const FeatureBlocks& blocks, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this, &blocks](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        m_bResetBRC |= task.bResetBRC;

        m_numSkipFrames += task.ctrl.SkipFrame * !!(task.SkipCMD & SKIPCMD_NeedNumSkipAdding);

        bool bIncSkipCount = !(task.SkipCMD & SKIPCMD_NeedDriverCall) && (task.SkipCMD & SKIPCMD_NeedCurrentFrameSkipping);
        m_numSkipFrames  += bIncSkipCount;
        m_sizeSkipFrames += task.BsDataLength * bIncSkipCount;

        MFX_CHECK(task.SkipCMD & SKIPCMD_NeedDriverCall, MFX_ERR_NONE);

        auto&  sh        = Task::SSH::Get(s_task);
        auto&  ph        = Glob::PackedHeaders::Get(global);
        auto&  cc        = CC::Get(global);
        bool   bSkipCurr = !!(task.SkipCMD & SKIPCMD_NeedCurrentFrameSkipping);
        mfxU32 nCBD      = 0;

        m_sps.bResetBRC = m_bResetBRC;
        m_bResetBRC     = false;

        cc.UpdatePPS(global, s_task, m_sps, m_pps);
        FillSliceBuffer(sh, task.RefPicList, m_slices);

        m_pps.SkipFrameFlag  = bSkipCurr * 2 + (!bSkipCurr && !!m_numSkipFrames);
        m_pps.NumSlices      = (USHORT)m_slices.size();
        m_pps.NumSkipFrames  = (UCHAR)m_numSkipFrames;
        m_pps.SizeSkipFrames = m_sizeSkipFrames;
        m_numSkipFrames      = 0;
        m_sizeSkipFrames     = 0;

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

        nCBD += PackCBD(ID_SLICEDATA, m_slices);
        nCBD += PackCBD(ID_BITSTREAMDATA, GetResId(RES_BS));

        nCBD += m_sps.scaling_list_enable_flag
            && PackCBD(ID_QUANTDATA, m_qMatrix);

        nCBD += task.bCUQPMap
            && PackCBD(ID_MBQPDATA, GetResId(RES_CUQP));

        nCBD += (task.InsertHeaders & INSERT_AUD)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.AUD[mfx::clamp<mfxU8>(task.CodingType, 1, 3) - 1], true));

        nCBD += (task.InsertHeaders & INSERT_VPS)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.VPS, true));

        nCBD += (task.InsertHeaders & INSERT_SPS)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.SPS, true));

        nCBD += (task.InsertHeaders & INSERT_PPS)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.PPS, true));

#if defined(MFX_ENABLE_LP_LOOKAHEAD)
        if (global.Contains(Glob::LpLookAhead::Key))
        {
            auto& lpla = Glob::LpLookAhead::Get(global);
            nCBD += ((task.InsertHeaders & INSERT_PPS) && !lpla.bAnalysis && lpla.bIsLpLookaheadEnabled)
                && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.CqmPPS, true));
        }
#endif

        nCBD += (((task.InsertHeaders & INSERT_SEI) || (task.ctrl.NumPayload > 0)) && ph.PrefixSEI.BitLen)
            && PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.PrefixSEI, true));

        mfxU32 nSliceSkip = !!(task.SkipCMD & SKIPCMD_NeedSkipSliceGen) * m_pps.NumSlices;
        mfxU32 nSliceReal = !(task.SkipCMD & SKIPCMD_NeedSkipSliceGen) * m_pps.NumSlices;

        for (mfxU32 i = 0; i < nSliceSkip; ++i)
        {
            nCBD += PackCBD(ID_PACKEDHEADERDATA, PackHeader(ph.SSH.at(i), false));
        }

        for (mfxU32 i = 0; i < nSliceReal; ++i)
        {
            auto& ps = ph.SSH.at(i);
            auto& ds = m_slices.at(i);

            ds.SliceQpDeltaBitOffset = ps.PackInfo.at(PACK_QPDOffset);
            if (m_sps.SAO_enabled_flag)
            {
                ds.SliceSAOFlagBitOffset = ps.PackInfo.at(PACK_SAOOffset);
            }

            ds.BitLengthSliceHeaderStartingPortion = 40 + 8 * ps.bLongSC;
            ds.SliceHeaderByteOffset               = 0;

            nCBD += PackCBD(ID_PACKEDSLICEDATA, PackHeader(ps, false));
        }

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

        auto pFeedback = fb.Get(task.StatusReportId);
        auto sts       = CC::Get(global).ReadFeedback(global, s_task, pFeedback, m_feedback.GetFeedBackSize());

        if (sts < MFX_ERR_NONE)
        {
            Glob::RTErr::Get(global) = sts;
        }

        auto& cc = CC::Get(global);
        cc.UpdateCqmHint(task, ((ENCODE_QUERY_STATUS_PARAMS_HEVC *)pFeedback)->lookaheadInfo);

        fb.Remove(task.StatusReportId);

        return sts;
    });
}

void DDIPacker::FillSpsBuffer(
    const ExtBuffer::Param<mfxVideoParam>& par
    , const SPS& bs_sps
    , ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps)
{
    auto& CO2        = (const mfxExtCodingOption2&)ExtBuffer::Get(par);
    auto& CO3        = (const mfxExtCodingOption3&)ExtBuffer::Get(par);
    auto& extVsi     = (const mfxExtVideoSignalInfo&)ExtBuffer::Get(par);
    auto& fi         = par.mfx.FrameInfo;
    bool  isSWBRC    = IsOn(CO2.ExtBRC) || (par.mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT);
    bool  isBPyramid = (CO2.BRefType == MFX_B_REF_PYRAMID);

    sps = {};

    sps.log2_min_coding_block_size_minus3 = (mfxU8)bs_sps.log2_min_luma_coding_block_size_minus3;

    sps.wFrameWidthInMinCbMinus1 = (mfxU16)(bs_sps.pic_width_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.wFrameHeightInMinCbMinus1 = (mfxU16)(bs_sps.pic_height_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.general_profile_idc = bs_sps.general.profile_idc;
    sps.general_level_idc   = bs_sps.general.level_idc;
    sps.general_tier_flag   = bs_sps.general.tier_flag;

    sps.GopPicSize          = par.mfx.GopPicSize;
    sps.GopRefDist          = mfxU8(par.mfx.GopRefDist);
    sps.GopOptFlag          = mfxU8(par.mfx.GopOptFlag);

    sps.TargetUsage         = mfxU8(par.mfx.TargetUsage);
    sps.RateControlMethod   = mfxU8(isSWBRC ? MFX_RATECONTROL_CQP : par.mfx.RateControlMethod);

    sps.ContentInfo = ENCODE_CONTENT(
        eContent_NonVideoScreen
        * (    fi.Height <= 576
            && fi.Width <= 736
            && par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
            && par.mfx.QPP < 32));

    bool bVBR =
        par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VCM;

    if (bVBR)
    {
        sps.MinBitRate      = 0;
        sps.TargetBitRate   = TargetKbps(par.mfx);
        sps.MaxBitRate      = MaxKbps(par.mfx);
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        sps.MinBitRate      = TargetKbps(par.mfx);
        sps.TargetBitRate   = TargetKbps(par.mfx);
        sps.MaxBitRate      = TargetKbps(par.mfx);
    }

    sps.FrameRate.Numerator        = fi.FrameRateExtN;
    sps.FrameRate.Denominator      = fi.FrameRateExtD;
    sps.InitVBVBufferFullnessInBit = 8000 * InitialDelayInKB(par.mfx);
    sps.VBVBufferSizeInBit         = 8000 * BufferSizeInKB(par.mfx);
    sps.LookaheadDepth             = (UCHAR) CO2.LookAheadDepth;

    sps.bResetBRC        = 0;
    sps.GlobalSearch     = 0;
    sps.LocalSearch      = 0;
    sps.EarlySkip        = 0;
    // it should be zero by default (MBBRC == UNKNOWN), but to save previous behavior MBBRC is on by default
    sps.MBBRC            = 2 - !IsOff(CO2.MBBRC);

    //WA:  Parallel BRC is switched on in sync & async mode (quality drop in noParallelBRC in driver)
    sps.ParallelBRC = (sps.GopRefDist > 1) && (sps.GopRefDist <= 8) && isBPyramid && !IsOn(par.mfx.LowPower);

    sps.SliceSizeControl = !!CO2.MaxSliceSize;

    ENCODE_FRAMESIZE_TOLERANCE FSTByLDBRC[2] = { eFrameSizeTolerance_Normal, eFrameSizeTolerance_ExtremelyLow };
    sps.FrameSizeTolerance = FSTByLDBRC[IsOn(CO3.LowDelayBRC)];

    sps.ICQQualityFactor = mfxU8(
        par.mfx.ICQQuality * (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        + CO3.QVBRQuality * (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR));

    if (sps.ParallelBRC)
    {
        if (!isBPyramid)
        {
            sps.NumOfBInGop[0]  = (par.mfx.GopPicSize - 1) - (par.mfx.GopPicSize - 1) / par.mfx.GopRefDist;
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
    }

    assert(fi.BitDepthLuma == 8
        || fi.BitDepthLuma == 10
        || fi.BitDepthLuma == 12
        || fi.BitDepthLuma == 16
    );

    sps.SourceBitDepth =
          0 * (fi.BitDepthLuma == 8 )
        + 1 * (fi.BitDepthLuma == 10)
        + 2 * (fi.BitDepthLuma == 12)
        + 3 * (fi.BitDepthLuma == 16);

    sps.SourceFormat = 3 * (fi.FourCC == MFX_FOURCC_RGB4 || fi.FourCC == MFX_FOURCC_A2RGB10);

    assert(fi.ChromaFormat > MFX_CHROMAFORMAT_YUV400 && fi.ChromaFormat <= MFX_CHROMAFORMAT_YUV444);
    sps.SourceFormat = std::max<UINT>(fi.ChromaFormat - 1, sps.SourceFormat);

    sps.scaling_list_enable_flag           = bs_sps.scaling_list_enabled_flag;
    sps.sps_temporal_mvp_enable_flag       = bs_sps.temporal_mvp_enabled_flag;
    sps.strong_intra_smoothing_enable_flag = bs_sps.strong_intra_smoothing_enabled_flag;
    sps.amp_enabled_flag                   = bs_sps.amp_enabled_flag;    // SKL: 0, CNL+: 1
    sps.SAO_enabled_flag                   = bs_sps.sample_adaptive_offset_enabled_flag; // 0, 1
    sps.pcm_enabled_flag                   = bs_sps.pcm_enabled_flag;
    sps.pcm_loop_filter_disable_flag       = 1;//bs_sps.pcm_loop_filter_disabled_flag;
    sps.chroma_format_idc                  = bs_sps.chroma_format_idc;
    sps.separate_colour_plane_flag         = bs_sps.separate_colour_plane_flag;

    sps.log2_max_coding_block_size_minus3       = (mfxU8)(bs_sps.log2_min_luma_coding_block_size_minus3
        + bs_sps.log2_diff_max_min_luma_coding_block_size);
    sps.log2_min_coding_block_size_minus3       = (mfxU8)bs_sps.log2_min_luma_coding_block_size_minus3;
    sps.log2_max_transform_block_size_minus2    = (mfxU8)(bs_sps.log2_min_transform_block_size_minus2
        + bs_sps.log2_diff_max_min_transform_block_size);
    sps.log2_min_transform_block_size_minus2    = (mfxU8)bs_sps.log2_min_transform_block_size_minus2;
    sps.max_transform_hierarchy_depth_intra     = (mfxU8)bs_sps.max_transform_hierarchy_depth_intra;
    sps.max_transform_hierarchy_depth_inter     = (mfxU8)bs_sps.max_transform_hierarchy_depth_inter;
    sps.log2_min_PCM_cb_size_minus3             = (mfxU8)bs_sps.log2_min_pcm_luma_coding_block_size_minus3;
    sps.log2_max_PCM_cb_size_minus3             = (mfxU8)(bs_sps.log2_min_pcm_luma_coding_block_size_minus3
        + bs_sps.log2_diff_max_min_pcm_luma_coding_block_size);
    sps.bit_depth_luma_minus8                   = (mfxU8)bs_sps.bit_depth_luma_minus8;
    sps.bit_depth_chroma_minus8                 = (mfxU8)bs_sps.bit_depth_chroma_minus8;
    sps.pcm_sample_bit_depth_luma_minus1        = (mfxU8)bs_sps.pcm_sample_bit_depth_luma_minus1;
    sps.pcm_sample_bit_depth_chroma_minus1      = (mfxU8)bs_sps.pcm_sample_bit_depth_chroma_minus1;
    sps.DisableHRDConformance                   = IsOff(CO3.BRCPanicMode);
    sps.ScenarioInfo = ENCODE_SCENARIO(
        (CO3.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING) * eScenario_GameStreaming
        + (CO3.ScenarioInfo == MFX_SCENARIO_REMOTE_GAMING) * eScenario_RemoteGaming);

    sps.LowDelayMode     = bs_sps.low_delay_mode;
    sps.HierarchicalFlag = bs_sps.hierarchical_flag;

    if (extVsi.ColourDescriptionPresent)
    {
    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4)
        switch (extVsi.MatrixCoefficients)
        {
        case MFX_TRANSFERMATRIX_BT601:
        {
            sps.InputColorSpace = eColorSpace_P601;
            break;
        }
        case MFX_TRANSFERMATRIX_BT709:
        {
            sps.InputColorSpace = eColorSpace_P709;
            break;
        }
        default:
            break;
            //need define in API value for eColorSpace_P2020
        }
    }

    else if (fi.Width * fi.Height >= 1920 * 1080) // From BT.709-6
    {
        sps.InputColorSpace = eColorSpace_P709;
    }
    else
    {
        sps.InputColorSpace = eColorSpace_P601;
    }
}

void DDIPacker::FillPpsBuffer(
    const ExtBuffer::Param<mfxVideoParam>& par
    , const PPS& bs_pps
    , ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    const mfxExtCodingOption2& CO2 = ExtBuffer::Get(par);

    pps = {};

    pps.tiles_enabled_flag               = bs_pps.tiles_enabled_flag;
    pps.entropy_coding_sync_enabled_flag = bs_pps.entropy_coding_sync_enabled_flag;
    pps.sign_data_hiding_flag            = bs_pps.sign_data_hiding_enabled_flag;
    pps.constrained_intra_pred_flag      = bs_pps.constrained_intra_pred_flag;
    pps.transform_skip_enabled_flag      = bs_pps.transform_skip_enabled_flag;
    pps.transquant_bypass_enabled_flag   = bs_pps.transquant_bypass_enabled_flag;
    pps.cu_qp_delta_enabled_flag         = bs_pps.cu_qp_delta_enabled_flag;
    pps.weighted_pred_flag               = bs_pps.weighted_pred_flag;
    pps.weighted_bipred_flag             = bs_pps.weighted_bipred_flag;

    pps.bEnableGPUWeightedPrediction     = 0;

    pps.loop_filter_across_slices_flag        = bs_pps.loop_filter_across_slices_enabled_flag;
    pps.loop_filter_across_tiles_flag         = bs_pps.loop_filter_across_tiles_enabled_flag;
    pps.scaling_list_data_present_flag        = bs_pps.scaling_list_data_present_flag;
    pps.dependent_slice_segments_enabled_flag = bs_pps.dependent_slice_segments_enabled_flag;
    pps.bLastPicInSeq                         = 0;
    pps.bLastPicInStream                      = 0;
    pps.bUseRawPicForRef                      = 0;
    pps.bEmulationByteInsertion               = 0; //!!!
    pps.bEnableRollingIntraRefresh            = 0;
    pps.BRCPrecision                          = 0;

    pps.QpY = mfxI8(bs_pps.init_qp_minus26 + 26);

    pps.diff_cu_qp_delta_depth  = (mfxU8)bs_pps.diff_cu_qp_delta_depth;
    pps.pps_cb_qp_offset        = (mfxU8)bs_pps.cb_qp_offset;
    pps.pps_cr_qp_offset        = (mfxU8)bs_pps.cr_qp_offset;
    pps.num_tile_columns_minus1 = (mfxU8)bs_pps.num_tile_columns_minus1;
    pps.num_tile_rows_minus1    = (mfxU8)bs_pps.num_tile_rows_minus1;

    std::copy(bs_pps.column_width, bs_pps.column_width + pps.num_tile_columns_minus1, pps.tile_column_width);
    std::copy(bs_pps.row_height, bs_pps.row_height + pps.num_tile_rows_minus1, pps.tile_row_height);
    std::fill(pps.tile_column_width + pps.num_tile_columns_minus1, std::end(pps.tile_column_width), mfxU16(0));
    std::fill(pps.tile_row_height + pps.num_tile_rows_minus1, std::end(pps.tile_row_height), mfxU16(0));

    pps.log2_parallel_merge_level_minus2 = (mfxU8)bs_pps.log2_parallel_merge_level_minus2;

    pps.num_ref_idx_l0_default_active_minus1 = (mfxU8)bs_pps.num_ref_idx_l0_default_active_minus1;
    pps.num_ref_idx_l1_default_active_minus1 = (mfxU8)bs_pps.num_ref_idx_l1_default_active_minus1;

    pps.LcuMaxBitsizeAllowed       = 0;
    pps.bUseRawPicForRef           = 0;
    pps.NumSlices                  = par.mfx.NumSlice;

    pps.bEnableSliceLevelReport = 0;

    if (CO2.MaxSliceSize)
        pps.MaxSliceSizeInBytes = CO2.MaxSliceSize;

    pps.DisplayFormatSwizzle = (par.mfx.FrameInfo.FourCC == MFX_FOURCC_A2RGB10) ||
                               (par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4);
}

void DDIPacker::FillSliceBuffer(
    const std::vector<SliceInfo>& sinfo
    , std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    slice.resize(sinfo.size());

    for (mfxU16 i = 0; i < slice.size(); i++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];
        cs = {};

        cs.slice_id = i;
        cs.slice_segment_address = sinfo[i].SegmentAddress;
        cs.NumLCUsInSlice = sinfo[i].NumLCU;
        cs.bLastSliceOfPic = (i == slice.size() - 1);
    }
}

void DDIPacker:: FillSliceBuffer(
    const Slice & sh
    , const mfxU8 (&rpl)[2][15]
    , bool bLast
    , ENCODE_SET_SLICE_HEADER_HEVC & cs)
{
    cs.slice_type = sh.type;

    memset(cs.RefPicList, 0xff, sizeof(cs.RefPicList));
    cs.num_ref_idx_l0_active_minus1 = sh.num_ref_idx_l0_active_minus1;
    cs.num_ref_idx_l1_active_minus1 = sh.num_ref_idx_l1_active_minus1;

    if (cs.slice_type != 2)
    {
        for (mfxU32 i = 0; i <= cs.num_ref_idx_l0_active_minus1; i++)
            cs.RefPicList[0][i].bPicEntry = rpl[0][i];

        if (cs.slice_type == 0)
        {
            for (mfxU32 i = 0; i <= cs.num_ref_idx_l1_active_minus1; i++)
                cs.RefPicList[1][i].bPicEntry = rpl[1][i];
        }
    }

    cs.bLastSliceOfPic                      = bLast;
    cs.dependent_slice_segment_flag         = sh.dependent_slice_segment_flag;
    cs.slice_temporal_mvp_enable_flag       = sh.temporal_mvp_enabled_flag;
    cs.slice_sao_luma_flag                  = sh.sao_luma_flag;
    cs.slice_sao_chroma_flag                = sh.sao_chroma_flag;
    cs.mvd_l1_zero_flag                     = sh.mvd_l1_zero_flag;
    cs.cabac_init_flag                      = sh.cabac_init_flag;
    cs.slice_deblocking_filter_disable_flag = sh.deblocking_filter_disabled_flag;
    cs.collocated_from_l0_flag              = sh.collocated_from_l0_flag;
    cs.slice_qp_delta                       = sh.slice_qp_delta;
    cs.slice_cb_qp_offset                   = sh.slice_cb_qp_offset;
    cs.slice_cr_qp_offset                   = sh.slice_cr_qp_offset;
    cs.beta_offset_div2                     = sh.beta_offset_div2;
    cs.tc_offset_div2                       = sh.tc_offset_div2;
    cs.MaxNumMergeCand                      = 5 - sh.five_minus_max_num_merge_cand;
}

void DDIPacker::FillPpsBuffer(
    const TaskCommonPar & task
    , const Slice & sh
    , const ENCODE_CAPS_HEVC & /*caps*/
    , ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    auto GetPicEntry = [](const DpbFrame& ref)
    {
        ENCODE_PICENTRY pe  = {};
        pe.bPicEntry        = ref.Rec.Idx;
        pe.AssociatedFlag   = (ref.Rec.Idx == IDX_INVALID) || ref.isLTR;
        return pe;
    };
    auto GetPOC = [](const DpbFrame& ref)
    {
        return ref.POC;
    };

    pps.CurrOriginalPic.Index7Bits      = task.Rec.Idx;
    pps.CurrOriginalPic.AssociatedFlag  = !!(task.FrameType & MFX_FRAMETYPE_REF);
    pps.CurrReconstructedPic            = GetPicEntry(task);

    if (sh.temporal_mvp_enabled_flag)
        pps.CollocatedRefPicIndex = task.RefPicList[!sh.collocated_from_l0_flag][sh.collocated_ref_idx];
    else
        pps.CollocatedRefPicIndex = IDX_INVALID;

    std::transform(std::begin(task.DPB.Active), std::end(task.DPB.Active), pps.RefFrameList, GetPicEntry);
    std::transform(std::begin(task.DPB.Active), std::end(task.DPB.Active), pps.RefFramePOCList, GetPOC);

    pps.NumDirtyRects   = 0;
    pps.pDirtyRect      = 0;
    pps.CodingType      = task.CodingType;
    pps.CurrPicOrderCnt = task.POC;

    pps.bEnableRollingIntraRefresh = task.IRState.refrType;

    if (pps.bEnableRollingIntraRefresh)
    {
        pps.IntraInsertionLocation  = task.IRState.IntraLocation;
        pps.IntraInsertionSize      = task.IRState.IntraSize;
        pps.QpDeltaForInsertedIntra = mfxU8(task.IRState.IntRefQPDelta);
    }

    pps.StatusReportFeedbackNumber  = task.StatusReportId;
    pps.nal_unit_type               = task.SliceNUT;
#if defined(MFX_ENABLE_LP_LOOKAHEAD)
    if (task.LplaStatus.TargetFrameSize > 0)
        pps.TargetFrameSize = task.LplaStatus.TargetFrameSize;
#endif
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)