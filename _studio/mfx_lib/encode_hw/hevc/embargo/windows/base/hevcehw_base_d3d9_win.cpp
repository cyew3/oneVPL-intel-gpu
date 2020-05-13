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

#include "hevcehw_base_d3d9_win.h"
#include "libmfx_core_interface.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Base;

class DDIDeviceDX9
    : public MfxEncodeHW::DeviceDX9
    , protected DDITracer
{
public:
    DDIDeviceDX9(std::function<mfxStatus(const DDIExecParam&)>& execute)
        : DDITracer(MFX_HW_D3D9)
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
        return MfxEncodeHW::DeviceDX9::Execute(par);
    }
    mfxStatus DefaultExecute(const DDIExecParam& par)
    {
        return MfxEncodeHW::DeviceDX9::Execute(par);
    }
protected:
    std::function<mfxStatus(const DDIExecParam&)>& m_execute;
};

DDI_D3D9::DDI_D3D9(mfxU32 FeatureId)
    : HEVCEHW::Base::IDDI(FeatureId)
{
    SetTraceName("Base_DDI_D3D9");
    m_pDevice.reset(new DDIDeviceDX9(m_execute));
}

mfxStatus DDI_D3D9::DefaultExecute(const DDIExecParam& par)
{
    return dynamic_cast<DDIDeviceDX9&>(*m_pDevice).DefaultExecute(par);
}

mfxStatus DDI_D3D9::Register(StorageRW& strg)
{
    auto& resources = Glob::DDI_Resources::Get(strg);

    for (auto& res : resources)
    {
        EmulSurfaceRegister surfaceReg = {};
        surfaceReg.type = (D3DFORMAT)res.Function;
        surfaceReg.num_surf = res.Resource.Size;

        auto GetSurfD3D9 = [](mfxHDLPair hdl) { return (IDirect3DSurface9*)hdl.first; };

        mfxHDLPair* pHP = (mfxHDLPair*)res.Resource.pData;
        std::transform(pHP, pHP + res.Resource.Size, surfaceReg.surface, GetSurfD3D9);

        mfxHDLPair hpair = { surfaceReg.surface[0], &surfaceReg };

        auto sts = m_pDevice->BeginPicture(&hpair);
        MFX_CHECK_STS(sts);

        sts = m_pDevice->EndPicture();
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

void DDI_D3D9::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
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

void DDI_D3D9::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_QueryCaps
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
    {
        MFX_CHECK(strg.Contains(Glob::GUID::Key), MFX_ERR_UNSUPPORTED);

        mfxStatus sts;
        auto& core = Glob::VideoCore::Get(strg);
        const GUID& guid = Glob::GUID::Get(strg);
        auto& caps = Glob::EncodeCaps::GetOrConstruct(strg);

        SetStorage(strg);

        auto& hwCaps = dynamic_cast<ENCODE_CAPS_HEVC&>(caps);

        bool bCreateDevice = m_guid != guid
            || MFX_ERR_NONE != m_pDevice->QueryCaps(&hwCaps, sizeof(hwCaps));

        if (bCreateDevice)
        {
            EncodeCapsHevc fakeCaps;
            Defaults::Param dpar(par, fakeCaps, core.GetHWType(), Glob::Defaults::Get(strg));
            sts = m_pDevice->Create(core, guid, dpar.base.GetCodedPicWidth(dpar), dpar.base.GetCodedPicHeight(dpar), true);
            MFX_CHECK_STS(sts);

            sts = m_pDevice->QueryCaps(&hwCaps, sizeof(hwCaps));
            MFX_CHECK_STS(sts);

            m_guid = guid;
        }

        m_bMbQpDataSupport = caps.MbQpDataSupport;

        return MFX_ERR_NONE;
    });
}

void DDI_D3D9::InitExternal(const FeatureBlocks& /*blocks*/, TPushIE Push)
{
    Push(BLK_CreateDevice
        , [this](const mfxVideoParam& par, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(strg);
        auto& guid = Glob::GUID::Get(strg);

        SetStorage(strg);

        MFX_CHECK(!m_pDevice->IsValid() || m_guid != guid, MFX_ERR_NONE);

        EncodeCapsHevc fakeCaps;
        Defaults::Param dpar(par, fakeCaps, core.GetHWType(), Glob::Defaults::Get(strg));

        m_guid = guid;

        return m_pDevice->Create(core, guid, dpar.base.GetCodedPicWidth(dpar), dpar.base.GetCodedPicHeight(dpar), false);
    });
}

void DDI_D3D9::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
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
        auto& hwCaps = dynamic_cast<ENCODE_CAPS_HEVC&>(caps);
        sts = m_pDevice->QueryCaps(&hwCaps, sizeof(hwCaps));
        MFX_CHECK_STS(sts);

        {
            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});

            sts = m_pDevice->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, pInfo->Info);
            MFX_CHECK_STS(sts);

            local.Insert(Tmp::BSAllocInfo::Key, std::move(pInfo));
        }

        if (m_bMbQpDataSupport)
        {
            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});

            sts = m_pDevice->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBQPDATA, pInfo->Info);
            MFX_CHECK_STS(sts);

            local.Insert(Tmp::MBQPAllocInfo::Key, std::move(pInfo));
        }

        return sts;
    });

    Push(BLK_Register
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        return Register(strg);
    });
}

void DDI_D3D9::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
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

void DDI_D3D9::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
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

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)