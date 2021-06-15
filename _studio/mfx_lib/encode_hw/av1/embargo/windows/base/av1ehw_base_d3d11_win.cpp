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

#include "av1ehw_base_data.h"
#include "av1ehw_base_d3d11_win.h"
#include "libmfx_core_interface.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;
using namespace AV1EHW::Windows;
using namespace AV1EHW::Windows::Base;

namespace AV1EHW
{
namespace Windows
{
namespace Base
{
class DDIDeviceDX11
    : public MfxEncodeHW::DeviceDX11
    , protected DDITracer
{
public:
    DDIDeviceDX11(std::function<mfxStatus(const DDIExecParam&)>& execute)
        : DDITracer()
        , m_execute(execute)
    {}

    virtual void Trace(const DDIExecParam& ep, bool bAfterExec, mfxU32 res) override
    {
        if (bAfterExec)
        {
            DDITracer::Trace("==HRESULT", res);
            TraceFunc(true, ep.Function, ep.Out.pData, ep.Out.Size);
        }
        else
        {
            TraceFunc(false, ep.Function, ep.In.pData, ep.In.Size);
        }
    }

    virtual mfxStatus Execute(const DDIExecParam& par) override
    {
        if (m_execute)
        {
            return m_execute(par);
        }
        return MfxEncodeHW::DeviceDX11::Execute(par);
    }

    mfxStatus DefaultExecute(const DDIExecParam& par)
    {
        return MfxEncodeHW::DeviceDX11::Execute(par);
    }

    virtual mfxStatus QueryCompBufferInfo(mfxU32 type, mfxFrameInfo& info) override
    {
        auto sts = MfxEncodeHW::DeviceDX11::QueryCompBufferInfo(type, info);
        MFX_CHECK_STS(sts);

        SetIf(info.FourCC
            , m_DX9TypeToDX11.at(type) == D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA
            , mfxU32(MFX_FOURCC_P8_TEXTURE));

        return sts;
    }

protected:
    std::function<mfxStatus(const DDIExecParam&)>& m_execute;
};
} //namespace Base
} //namespace Windows
} //namespace AV1EHW

DDI_D3D11::DDI_D3D11(mfxU32 FeatureId)
    : AV1EHW::Base::IDDI(FeatureId)
{
    SetTraceName("Base_DDI_D3D11");
    m_pDevice.reset(new DDIDeviceDX11(m_execute));
}

mfxStatus DDI_D3D11::DefaultExecute(const DDIExecParam& par)
{
    return dynamic_cast<DDIDeviceDX11&>(*m_pDevice).DefaultExecute(par);
}

mfxStatus DDI_D3D11::QueryEncodeCaps(mfxVideoParam& par, StorageW& strg, EncodeCapsAv1& caps)
{
    mfxStatus sts;
    auto& core = Glob::VideoCore::Get(strg);
    const GUID& guid = Glob::GUID::Get(strg);

    SetStorage(strg);

    auto& hwCaps = dynamic_cast<ENCODE_CAPS_AV1&>(caps);

    bool bCreateDevice = m_guid != guid
        || MFX_ERR_NONE != m_pDevice->QueryCaps(&hwCaps, sizeof(hwCaps));

    if (bCreateDevice)
    {
        EncodeCapsAv1 fakeCaps;
        Defaults::Param dpar(par, fakeCaps, core.GetHWType(), Glob::Defaults::Get(strg));
        sts = m_pDevice->Create(core, guid, dpar.base.GetCodedPicWidth(dpar), dpar.base.GetCodedPicHeight(dpar), true);
        MFX_CHECK_STS(sts);

        sts = m_pDevice->QueryCaps(&hwCaps, sizeof(hwCaps));
        MFX_CHECK_STS(sts);

        m_guid = guid;
    }
    return MFX_ERR_NONE;
}

void DDI_D3D11::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetCallChains
        , [this](const mfxVideoParam&, mfxVideoParam& /*par*/, StorageRW& strg) -> mfxStatus
    {
        auto& ddiExecute = Glob::DDI_Execute::GetOrConstruct(strg);

        MFX_CHECK(!ddiExecute, MFX_ERR_NONE);

        ddiExecute.Push(
            [this](Glob::DDI_Execute::TRef::TExt, const DDIExecParam& ep)
        {
            return DefaultExecute(ep);
        });

        return MFX_ERR_NONE;
    });
}

void DDI_D3D11::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_QueryCaps
        , [this](mfxVideoParam& par, StorageW& strg, StorageRW&)
    {
        auto& caps = Glob::EncodeCaps::Get(strg);
        return QueryEncodeCaps(par, strg, caps);
    });
}

inline mfxU16 GetTuIdx(mfxVideoParam& par)
{
    mfxU16 tuIdx = 0;
    if (par.mfx.TargetUsage == 0)
    {
        tuIdx = DEFAULT_TARGET_USAGE - 1;
    }
    else
    {
        tuIdx = par.mfx.TargetUsage - 1;
    }
    return tuIdx;
}

void DDI_D3D11::Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    Push(BLK_QueryCaps
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& global) -> mfxStatus
    {
        MFX_CHECK(global.Contains(Glob::GUID::Key), MFX_ERR_UNDEFINED_BEHAVIOR);

        auto& caps = Glob::EncodeCaps::GetOrConstruct(global);
        return QueryEncodeCaps(par, global, caps);
    });

    Push(BLK_QueryCORE
        , [&](const mfxVideoParam& /* in */, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        mfxExtEncoderCapability* pCaps = ExtBuffer::Get(par);
        MFX_CHECK(pCaps, MFX_ERR_NONE);
        MFX_CHECK(!global.Contains(Glob::RealState::Key), MFX_ERR_NONE); // ignore Reset

        auto& core = Glob::VideoCore::Get(global);
        MFX_CHECK(core.GetVAType() == MFX_HW_D3D11, MFX_ERR_UNSUPPORTED);

        EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(&core, MFXIHWMBPROCRATE_GUID);
        MFX_CHECK(pEncodeCaps, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxU32 mbPerSec[16] = {};
        auto&  guid = Glob::GUID::Get(global);
        mfxU16 tuIdx = GetTuIdx(par);

        //check if MbPerSec for particular TU was already queried or need to save
        mfxStatus sts = pEncodeCaps->GetHWCaps<mfxU32>(guid, mbPerSec, mfxU32(Size(mbPerSec)));
        bool bValid = sts == MFX_ERR_NONE && mbPerSec[tuIdx] != 0;

        pCaps->MBPerSec = mbPerSec[tuIdx];

        bool bQueryMode = !(blocks.m_initialized.at(GetID()) & (eFeatureMode::INIT | eFeatureMode::QUERY_IO_SURF));
        ThrowIf(bQueryMode && bValid, MFX_ERR_NONE); //don't run more blocks

        return MFX_ERR_NONE;
     });

    Push(BLK_QueryDDI
        , [&](const mfxVideoParam& /* in */, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        mfxExtEncoderCapability* pCaps = ExtBuffer::Get(par);
        MFX_CHECK(pCaps && !pCaps->MBPerSec, MFX_ERR_NONE);
        MFX_CHECK(!global.Contains(Glob::RealState::Key), MFX_ERR_NONE); // ignore Reset

        ENCODE_QUERY_PROCESSING_RATE_INPUT inPar = {};

        // Some encoding parameters affect MB processing rate. Pass them to driver
        inPar.GopPicSize = par.mfx.GopPicSize;
        inPar.GopRefDist = mfxU8(par.mfx.GopRefDist);
        inPar.Level = mfxU8(par.mfx.CodecLevel);
        inPar.Profile = mfxU8(par.mfx.CodecProfile);
        inPar.TargetUsage = mfxU8(par.mfx.TargetUsage);

        // DDI driver programming notes require to send 0x(ff) for undefined parameters
        SetDefault(inPar.GopPicSize, 0xffff);
        SetDefault(inPar.GopRefDist, 0xff);
        SetDefault(inPar.Level, 0xff);
        SetDefault(inPar.Profile, 0xff);
        SetDefault(inPar.TargetUsage, 0xff);

        mfxU32 mbPerSec[16] = {};
        DDIExecParam ddiPar;

        ddiPar.Function = ENCODE_QUERY_MAX_MB_PER_SEC_ID;
        ddiPar.In.pData = &inPar;
        ddiPar.In.Size = sizeof(inPar);
        ddiPar.Out.pData = mbPerSec;
        ddiPar.Out.Size = sizeof(mbPerSec);

        auto sts = Glob::DDI_Execute::Get(global)(ddiPar);
        MFX_CHECK_STS(sts);

        pCaps->MBPerSec = mbPerSec[0];

        auto& core = Glob::VideoCore::Get(global);
        auto& guid = Glob::GUID::Get(global);
        mfxU16 tuIdx = GetTuIdx(par);

        auto  pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(&core, MFXIHWMBPROCRATE_GUID);
        MFX_CHECK(pEncodeCaps, MFX_ERR_UNDEFINED_BEHAVIOR);

        std::fill(std::begin(mbPerSec), std::end(mbPerSec), 0);
        pEncodeCaps->GetHWCaps<mfxU32>(guid, mbPerSec, mfxU32(Size(mbPerSec)));

        mbPerSec[tuIdx] = pCaps->MBPerSec;

        sts = pEncodeCaps->SetHWCaps<mfxU32>(guid, mbPerSec, mfxU32(Size(mbPerSec)));
        MFX_CHECK_STS(sts);

        bool bQueryMode = !(blocks.m_initialized.at(GetID()) & (eFeatureMode::INIT | eFeatureMode::QUERY_IO_SURF));
        ThrowIf(bQueryMode, MFX_ERR_NONE); //don't run more blocks

        return MFX_ERR_NONE;
    });
}

void DDI_D3D11::InitExternal(const FeatureBlocks& /*blocks*/, TPushIE Push)
{
    Push(BLK_CreateDevice
        , [this](const mfxVideoParam& par, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(strg);
        auto& guid = Glob::GUID::Get(strg);

        SetStorage(strg);

        MFX_CHECK(!m_pDevice->IsValid() || m_guid != guid, MFX_ERR_NONE);

        EncodeCapsAv1 fakeCaps;
        Defaults::Param dpar(par, fakeCaps, core.GetHWType(), Glob::Defaults::Get(strg));

        m_guid = guid;

        return m_pDevice->Create(core, guid, dpar.base.GetCodedPicWidth(dpar), dpar.base.GetCodedPicHeight(dpar), false);
    });
}

void DDI_D3D11::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_CreateService
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        SetStorage(strg);

        Tmp::DDI_InitParam::TRef* pPar = nullptr;

        if (local.Contains(Tmp::DDI_InitParam::Key))
            pPar = &Tmp::DDI_InitParam::Get(local);

        mfxStatus sts = m_pDevice->Init(pPar);
        MFX_CHECK_STS(sts);

        auto& caps = Glob::EncodeCaps::GetOrConstruct(strg);
        auto& hwCaps = dynamic_cast<ENCODE_CAPS_AV1&>(caps);
        sts = m_pDevice->QueryCaps(&hwCaps, sizeof(hwCaps));
        MFX_CHECK_STS(sts);

        {
            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});

            sts = m_pDevice->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, pInfo->Info);
            MFX_CHECK_STS(sts);

            local.Insert(Tmp::BSAllocInfo::Key, std::move(pInfo));
        }

        return sts;
    });

    Push(BLK_Register
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        return Register(strg);
    });
}

void DDI_D3D11::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(m_pDevice->IsValid(), MFX_ERR_NOT_INITIALIZED);

        auto& task = Task::Common::Get(s_task);

        if (!(task.SkipCMD & SKIPCMD_NeedDriverCall))
            return MFX_ERR_NONE;

        auto& par = Glob::DDI_SubmitParam::Get(global);

        SetStorage(global);

        mfxStatus sts;
        sts = m_pDevice->BeginPicture(&task.HDLRaw);
        MFX_CHECK_STS(sts);

        for (auto& ep : par)
        {
            sts = m_pDevice->Execute(ep);
            MFX_CHECK_STS(sts);
        }

        sts = m_pDevice->EndPicture();
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    });
}

void DDI_D3D11::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        if (!(task.SkipCMD & SKIPCMD_NeedDriverCall))
            return MFX_ERR_NONE;

        SetStorage(global);

        return m_pDevice->QueryStatus(Glob::DDI_Feedback::Get(global), task.StatusReportId);
    });
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
