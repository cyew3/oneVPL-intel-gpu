// Copyright (c) 2019 Intel Corporation
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

#include "hevcehw_g9_d3d9_win.h"
#include "libmfx_core_interface.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen9;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Gen9;

void DDI_D3D9::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetCallChains
        , [this](const mfxVideoParam&, mfxVideoParam& /*par*/, StorageRW& strg) -> mfxStatus
    {
        auto& ddiExecute = Glob::DDI_Execute::GetOrConstruct(strg);

        m_pCurrStorage = &strg;

        MFX_CHECK(!ddiExecute, MFX_ERR_NONE);

        ddiExecute.Push(
            [&](Glob::DDI_Execute::TRef::TExt, const DDIExecParam& ep)
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

        bool bCreateDevice =
            m_guid != guid
            || MFX_ERR_NONE != QueryEncodeCaps(caps);

        if (bCreateDevice)
        {
            sts = CreateAuxilliaryDevice(core, guid, par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height, true);
            MFX_CHECK_STS(sts);

            sts = QueryEncodeCaps(caps);
            MFX_CHECK_STS(sts);
        }

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

        m_pCurrStorage = &strg;

        if (IsInitialized() && m_guid == guid)
            return MFX_ERR_NONE;

        return CreateAuxilliaryDevice(core, guid, par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height, false);
    });
}

void DDI_D3D9::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_CreateService
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        m_pCurrStorage = &strg;

        mfxStatus sts = CreateAccelerationService(local);
        MFX_CHECK_STS(sts);

        {
            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});

            sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, pInfo->Info);
            MFX_CHECK_STS(sts);

            local.Insert(Tmp::BSAllocInfo::Key, std::move(pInfo));
        }

        if (m_caps.MbQpDataSupport)
        {
            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});

            sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBQPDATA, pInfo->Info);
            MFX_CHECK_STS(sts);

            local.Insert(Tmp::MBQPAllocInfo::Key, std::move(pInfo));
        }

        return sts;
    });

    Push(BLK_Register
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(IsInitialized(), MFX_ERR_NOT_INITIALIZED);
        auto& resources = Glob::DDI_Resources::Get(strg);

        for (auto& res : resources)
        {
            EmulSurfaceRegister surfaceReg = {};
            surfaceReg.type = (D3DFORMAT)res.Function;
            surfaceReg.num_surf = res.Resource.Size;

            mfxHDLPair* pHP = (mfxHDLPair*)res.Resource.pData;
            for (mfxU32 i = 0; i < res.Resource.Size; i++)
                surfaceReg.surface[i] = (IDirect3DSurface9*)pHP[i].first;

            HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

            hr = m_auxDevice->EndFrame(nullptr);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }

        return MFX_ERR_NONE;
    });
}

mfxStatus DDI_D3D9::DefaultExecute(const DDIExecParam& ep)
{
    TraceFunc(false, ep.Function, ep.In.pData, ep.In.Size);
    try
    {
        m_lastRes = m_auxDevice->Execute(
            ep.Function
            , ep.In.pData
            , ep.In.Size
            , ep.Out.pData
            , ep.Out.Size);
        Trace("==HRESULT", m_lastRes);
        MFX_CHECK(SUCCEEDED(m_lastRes), MFX_ERR_DEVICE_FAILED);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    TraceFunc(true, ep.Function, ep.Out.pData, ep.Out.Size);

    return MFX_ERR_NONE;
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

        m_pCurrStorage = &global;

        try
        {
            HANDLE handle;
            HRESULT hr;
            mfxStatus sts;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "BeginFrame");
                hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)task.HDLRaw.first, 0);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

            for (auto& ep : par)
            {
                if (ep.Function == ENCODE_ENC_PAK_ID)
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_PAK_ID");
                    sts = Execute(
                        ep.Function
                        , ep.In.pData
                        , ep.In.Size
                        , ep.Out.pData
                        , ep.Out.Size);
                }
                else
                {
                    sts = Execute(
                        ep.Function
                        , ep.In.pData
                        , ep.In.Size
                        , ep.Out.pData
                        , ep.Out.Size);

                }
                MFX_CHECK_STS(sts);
            }

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "EndFrame");
                m_auxDevice->EndFrame(&handle);
            }
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

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

        auto& ddiFB = Glob::DDI_Feedback::Get(global);
        auto pFB = (const ENCODE_QUERY_STATUS_PARAMS*)ddiFB.Get(task.StatusReportId);

        if (pFB && pFB->bStatus == ENCODE_OK)
            return MFX_ERR_NONE;

        m_pCurrStorage = &global;

        mfxStatus sts;
        try
        {
            sts = Execute(
                ddiFB.Function
                , (ENCODE_QUERY_STATUS_PARAMS_DESCR*)ddiFB.In.pData
                , ddiFB.In.Size
                , (ENCODE_QUERY_STATUS_PARAMS*)ddiFB.Out.pData
                , ddiFB.Out.Size);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        ddiFB.bNotReady = (m_lastRes == D3DERR_WASSTILLDRAWING);
        MFX_CHECK(!ddiFB.bNotReady, MFX_ERR_NONE);
        MFX_CHECK_STS(sts);

        return ddiFB.Update(task.StatusReportId);
    });
}

mfxStatus DDI_D3D9::CreateAuxilliaryDevice(
    VideoCORE&  core
    , GUID        guid
    , mfxU32      width
    , mfxU32      height
    , bool        isTemporal)
{
    m_pCore = &core;

    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_pCore, MFXICORED3D_GUID);
    MFX_CHECK(pID3D, MFX_ERR_DEVICE_FAILED);

    IDirectXVideoDecoderService *service = 0;
    mfxStatus sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service, isTemporal);
    MFX_CHECK_STS(sts);
    MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    std::unique_ptr<AuxiliaryDevice> auxDevice(new AuxiliaryDevice());
    sts = auxDevice->Initialize(0, service);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    m_guid      = guid;
    m_width     = width;
    m_height    = height;
    m_auxDevice = std::move(auxDevice);

    sts = Execute(AUXDEV_QUERY_ACCEL_CAPS, &guid, sizeof(guid), &m_caps, sizeof(m_caps));
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus DDI_D3D9::QueryEncodeCaps(
    ENCODE_CAPS_HEVC & caps)
{
    MFX_CHECK(IsInitialized(), MFX_ERR_DEVICE_FAILED);

    caps = m_caps;

    return MFX_ERR_NONE;
}

mfxStatus DDI_D3D9::CreateAccelerationService(StorageRW& local)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);
    
    Tmp::DDI_InitParam::TRef* pPar = nullptr;

    if (local.Contains(Tmp::DDI_InitParam::Key))
        pPar = &Tmp::DDI_InitParam::Get(local);

    bool bUseDefault = !pPar
        || (std::none_of(pPar->begin(), pPar->end(), [](DDIExecParam par)
    {
        return par.Function == AUXDEV_CREATE_ACCEL_SERVICE;
    }));

    mfxStatus sts;

    if (bUseDefault)
    {
        DXVADDI_VIDEODESC desc = {};
        desc.SampleWidth = m_width;
        desc.SampleHeight = m_height;
        desc.Format = D3DDDIFMT_NV12;

        ENCODE_CREATEDEVICE encodeCreateDevice = {};
        encodeCreateDevice.pVideoDesc = &desc;
        encodeCreateDevice.CodingFunction = ENCODE_ENC_PAK;
        encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "AUXDEV_CREATE_ACCEL_SERVICE");
            sts = Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
        }
        MFX_CHECK_STS(sts);
    }

    MFX_CHECK(pPar, MFX_ERR_NONE);

    for (auto& par : *pPar)
    {
        sts = Execute(par.Function, par.In.pData, par.In.Size, par.Out.pData, par.Out.Size);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

mfxStatus DDI_D3D9::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameInfo& info)
{
    MFX_CHECK(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    if (m_compBufInfo.empty())
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount = 0;

        GUID guid = m_auxDevice->GetCurrentGuid();
        mfxStatus sts = Execute(ENCODE_FORMAT_COUNT_ID, guid, encodeFormatCount);
        MFX_CHECK_STS(sts);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize  = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize    = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo     = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats      = &m_uncompBufInfo[0];

        sts = Execute(ENCODE_FORMATS_ID, (void*)0, encodeFormats);
        MFX_CHECK_STS(sts);
        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);
    }

    for (auto& cbinf : m_compBufInfo)
    {
        if (cbinf.Type == type)
        {
            info.Width  = cbinf.CreationWidth;
            info.Height = cbinf.CreationHeight;
            info.FourCC = cbinf.CompressedFormats;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_DEVICE_FAILED;
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)