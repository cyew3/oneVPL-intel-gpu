// Copyright (c) 2021 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include "av1ehw_base_va_lin.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;
using namespace AV1EHW::Linux::Base;

static const Glob::GuidToVa::TRef DefaultGuidToVa =
{
    { DXVA2_Intel_LowpowerEncode_AV1_420_8b,    VAGUID{VAProfileAV1Profile0, VAEntrypointEncSliceLP}}
    , { DXVA2_Intel_LowpowerEncode_AV1_420_10b, VAGUID{VAProfileAV1Profile0, VAEntrypointEncSliceLP}}
};

VAGUID DDI_VA::MapGUID(StorageR& strg, const GUID& guid)
{
    if (strg.Contains(Glob::GuidToVa::Key))
    {
        auto& g2va = Glob::GuidToVa::Get(strg);
        if (g2va.find(guid) != g2va.end())
            return g2va.at(guid);
    }

    return DefaultGuidToVa.at(guid);
}

void DDI_VA::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetCallChains
        , [this](const mfxVideoParam&, mfxVideoParam& /*par*/, StorageRW& strg) -> mfxStatus
    {
        auto& ddiExec = Glob::DDI_Execute::GetOrConstruct(strg);

        MFX_CHECK(!ddiExec, MFX_ERR_NONE);

        ddiExec.Push([&](Glob::DDI_Execute::TRef::TExt, const DDIExecParam& ep)
        {
            return Execute(ep);
        });

        m_callVa = ddiExec;

        return MFX_ERR_NONE;
    });
}

mfxStatus DDI_VA::CreateAndQueryCaps(StorageW& strg)
{
    auto& guid      = Glob::GUID::Get(strg);
    auto  vaGuid    = MapGUID(strg, guid);
    auto  vap       = VAProfile(vaGuid.Profile);
    auto  vaep      = VAEntrypoint(vaGuid.Entrypoint);
    bool  bNeedNewDevice = !IsValid() || vap != m_profile || vaep != m_entrypoint;

    m_callVa = Glob::DDI_Execute::Get(strg);

    if (bNeedNewDevice)
    {
        auto& core = Glob::VideoCore::Get(strg);
        mfxStatus sts = Create(core, vap, vaep);
        MFX_CHECK_STS(sts);

        sts = QueryCaps();
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

void DDI_VA::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_QueryCaps
        , [this](const mfxVideoParam&, StorageW& strg, StorageRW&)
    {
        CreateAndQueryCaps(strg);
        auto& caps = Glob::EncodeCaps::Get(strg);
        caps = m_caps;
    });
}

void DDI_VA::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_QueryCaps
        , [this](const mfxVideoParam&, mfxVideoParam& /*par*/, StorageRW& strg) -> mfxStatus
    {
        MFX_CHECK(strg.Contains(Glob::GUID::Key), MFX_ERR_UNSUPPORTED);
        auto sts = CreateAndQueryCaps(strg);
        MFX_CHECK_STS(sts);

        auto& caps = Glob::EncodeCaps::GetOrConstruct(strg);
        caps = m_caps;

        return sts;
    });
}

void DDI_VA::InitExternal(const FeatureBlocks& /*blocks*/, TPushIE Push)
{
    Push(BLK_CreateDevice
        , [this](const mfxVideoParam& /*par*/, StorageRW& strg, StorageRW&) -> mfxStatus
        {
            return CreateAndQueryCaps(strg);
        });
}

mfxStatus DDI_VA::CreateVABuffers(
    const std::list<DDIExecParam>& par
    , std::vector<VABufferID>& pool)
{
    pool.resize(par.size(), VA_INVALID_ID);

    std::transform(par.begin(), par.end(), pool.begin()
        , [this](const DDIExecParam& p){ return CreateVABuffer(p); });

    bool bFailed = pool.end() != std::find(pool.begin(), pool.end(), VA_INVALID_ID);
    MFX_CHECK(!bFailed, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus DDI_VA::DestroyVABuffers(std::vector<VABufferID>& pool)
{
    bool bFailed = std::any_of(pool.begin(), pool.end()
        , [this](VABufferID id) { return !!DestroyVABuffer(id); });

    pool.clear();

    MFX_CHECK(!bFailed, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

void DDI_VA::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_CreateService
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
        {
            const auto&                     par   = Glob::VideoParam::Get(strg);
            const mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(par);
            mfxStatus sts;

            m_callVa = Glob::DDI_Execute::Get(strg);

            std::vector<VAConfigAttrib> attrib(2);

            attrib[0].type = VAConfigAttribRTFormat;
            attrib[1].type = VAConfigAttribRateControl;

            sts = MfxEncodeHW::DeviceVAAPI::QueryCaps(attrib.data(), attrib.size() * sizeof(VAConfigAttrib));
            MFX_CHECK_STS(sts);

            MFX_CHECK(attrib[0].value & (VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV420_10), MFX_ERR_DEVICE_FAILED);

            uint32_t vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

            MFX_CHECK((attrib[1].value & vaRCType), MFX_ERR_DEVICE_FAILED);

            attrib[1].value = vaRCType;

            sts = MfxEncodeHW::DeviceVAAPI::Init(
                rsPar.FrameWidth
                , rsPar.FrameHeight
                , VA_PROGRESSIVE
                , Glob::AllocRec::Get(strg).GetResponse()
                , attrib.data()
                , int(attrib.size()));
            MFX_CHECK_STS(sts);

            auto& info = Tmp::BSAllocInfo::GetOrConstruct(local);

            // request linear buffer
            info.Info.FourCC = MFX_FOURCC_P8;

            // context_id required for allocation video memory (temp  solution)
            info.AllocId      = m_vaContextEncode;
            info.Info.Width   = rsPar.FrameWidth * 2;
            info.Info.Height  = rsPar.FrameHeight;

            return MFX_ERR_NONE;
        });

        Push(BLK_Register
            , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
        {
            auto& res = Glob::DDI_Resources::Get(strg);

            auto itBs = std::find_if(res.begin(), res.end()
                , [](decltype(*res.begin()) r)
            {
                return r.Function == MFX_FOURCC_P8;
            });
            MFX_CHECK(itBs != res.end(), MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(itBs->Resource.Size == sizeof(VABufferID), MFX_ERR_UNDEFINED_BEHAVIOR);

            VABufferID* pBsBegin = (VABufferID*)itBs->Resource.pData;
            m_bs.assign(pBsBegin, pBsBegin + itBs->Resource.Num);

            mfxStatus sts = CreateVABuffers(
                Tmp::DDI_InitParam::Get(local)
                , m_perSeqPar);
            MFX_CHECK_STS(sts);

            return MFX_ERR_NONE;
        });
}

void DDI_VA::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
        {
            mfxStatus sts;

            m_callVa = Glob::DDI_Execute::Get(strg);

            sts = DestroyVABuffers(m_perSeqPar);
            MFX_CHECK_STS(sts);

            sts = CreateVABuffers(Tmp::DDI_InitParam::Get(local), m_perSeqPar);
            MFX_CHECK_STS(sts);

            return MFX_ERR_NONE;
        });
}

void DDI_VA::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        mfxStatus sts;
        auto& task = Task::Common::Get(s_task);

        m_callVa = Glob::DDI_Execute::Get(global);

        MFX_CHECK((task.SkipCMD & SKIPCMD_NeedDriverCall), MFX_ERR_NONE);

        sts = DestroyVABuffers(m_perPicPar);
        MFX_CHECK_STS(sts);

        sts = CreateVABuffers(Glob::DDI_SubmitParam::Get(global), m_perPicPar);
        MFX_CHECK_STS(sts);

        sts = BeginPicture(task.HDLRaw.first);
        MFX_CHECK_STS(sts);

        sts = RenderPicture(m_perPicPar.data(), (int)m_perPicPar.size());
        MFX_CHECK_STS(sts);

        sts = RenderPicture(m_perSeqPar.data(), (int)m_perSeqPar.size());
        MFX_CHECK_STS(sts);

        sts = EndPicture();
        MFX_CHECK_STS(sts);

        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS
            , "A|ENCODE|AV1|PACKET_END|", "%d|%d"
            , m_vaContextEncode, task.StatusReportId);

        return MFX_ERR_NONE;
    });
}

void DDI_VA::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        m_callVa = Glob::DDI_Execute::Get(global);

        MFX_CHECK((task.SkipCMD & SKIPCMD_NeedDriverCall), MFX_ERR_NONE);

        return QueryStatus(Glob::DDI_Feedback::Get(global), task.StatusReportId);
    });
}

mfxStatus DDI_VA::QueryCaps()
{
    m_caps = {};

    std::map<VAConfigAttribType, int> idx_map;
    VAConfigAttribType attr_types[] =
    {
        VAConfigAttribRTFormat
        , VAConfigAttribRateControl
        , VAConfigAttribMaxPictureHeight
        , VAConfigAttribMaxPictureWidth
        , VAConfigAttribEncMaxRefFrames
        , VAConfigAttribEncTileSupport
        , VAConfigAttribEncDynamicScaling
        , (VAConfigAttribType)VAConfigAttribEncAV1
        , (VAConfigAttribType)VAConfigAttribEncAV1Ext1
        , (VAConfigAttribType)VAConfigAttribEncAV1Ext2
    };
    std::vector<VAConfigAttrib> attrs;
    auto AV = [&](VAConfigAttribType t) {return attrs[idx_map[t]].value; };

    mfxI32 i = 0;
    std::for_each(std::begin(attr_types), std::end(attr_types)
        , [&](decltype(*std::begin(attr_types)) type)
    {
        attrs.push_back({ type, 0 });
        idx_map[type] = i;
        i++;
    });

    auto sts = MfxEncodeHW::DeviceVAAPI::QueryCaps(attrs.data(), attrs.size() * sizeof(VAConfigAttrib));
    MFX_CHECK_STS(sts);

    m_caps.ChromeSupportFlags.fields.i420 = !!(AV(VAConfigAttribRTFormat) & VA_RT_FORMAT_YUV420);

    m_caps.BitDepthSupportFlags.fields.eight_bits = !!(AV(VAConfigAttribRTFormat) & VA_RT_FORMAT_YUV420);
    m_caps.BitDepthSupportFlags.fields.ten_bits   = !!(AV(VAConfigAttribRTFormat) & VA_RT_FORMAT_YUV420_10BPP);

    m_caps.msdk.CBRSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_CBR);
    m_caps.msdk.VBRSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_VBR);
    m_caps.msdk.CQPSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_CQP);
    m_caps.msdk.ICQSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_ICQ);

    MFX_CHECK(AV(VAConfigAttribMaxPictureWidth) != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(AV(VAConfigAttribMaxPictureHeight) != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK_COND(AV(VAConfigAttribMaxPictureWidth) && AV(VAConfigAttribMaxPictureHeight));
    m_caps.MaxPicWidth = AV(VAConfigAttribMaxPictureWidth);
    m_caps.MaxPicHeight = AV(VAConfigAttribMaxPictureHeight);

    if (AV(VAConfigAttribEncMaxRefFrames) != VA_ATTRIB_NOT_SUPPORTED)
    {
        m_caps.MaxNum_ReferenceL0_P = mfxU8(AV(VAConfigAttribEncMaxRefFrames) & 0xFF);
        m_caps.MaxNum_ReferenceL0_B = mfxU8((AV(VAConfigAttribEncMaxRefFrames) >> 8) & 0xFF);
        m_caps.MaxNum_ReferenceL1_B = mfxU8((AV(VAConfigAttribEncMaxRefFrames) >> 16) & 0xFF);
    }
    else
    {
        m_caps.MaxNum_ReferenceL0_P = 2;
        m_caps.MaxNum_ReferenceL0_B = 2;
        m_caps.MaxNum_ReferenceL1_B = 1;
    }

    m_caps.FrameOBUSupport           = 1;
    m_caps.ForcedSegmentationSupport = 1;
    m_caps.AV1ToolSupportFlags.fields.enable_order_hint = 1;
    m_caps.AV1ToolSupportFlags.fields.enable_cdef       = 1;

    auto attribValEncAV1Ext1 = *(VAConfigAttribValEncAV1Ext1 *)(&attrs[idx_map[(VAConfigAttribType)VAConfigAttribEncAV1Ext1]].value);
    m_caps.SegmentFeatureSupport               = attribValEncAV1Ext1.bits.segment_feature_support;
    m_caps.MinSegIdBlockSizeAccepted           = attribValEncAV1Ext1.bits.min_segid_block_size_accepted;
    m_caps.SupportedInterpolationFilters.value = static_cast<mfxU8>(attribValEncAV1Ext1.bits.interp_filter_type);

    auto attribValEncAV1Ext2 = *(VAConfigAttribValEncAV1Ext2 *)(&attrs[idx_map[(VAConfigAttribType)VAConfigAttribEncAV1Ext2]].value);
    m_caps.TileSizeBytesMinus1 = attribValEncAV1Ext2.bits.tile_size_bytes_minus1;

    return MFX_ERR_NONE;
}

uint32_t DDI_VA::ConvertRateControlMFX2VAAPI(mfxU16 rateControl)
{
    static const std::map<mfxU16, uint32_t> RCMFX2VAAPI =
    {
        { mfxU16(MFX_RATECONTROL_CQP)   , uint32_t(VA_RC_CQP) },
        { mfxU16(MFX_RATECONTROL_CBR)   , uint32_t(VA_RC_CBR) },
        { mfxU16(MFX_RATECONTROL_VBR)   , uint32_t(VA_RC_VBR) },
    };

    auto itRC = RCMFX2VAAPI.find(rateControl);
    if (itRC != RCMFX2VAAPI.end())
    {
        return itRC->second;
    }

    return uint32_t(VA_RC_NONE);
}

#endif // defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
