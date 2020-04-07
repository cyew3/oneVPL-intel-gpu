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

#include "av1ehw_g12_d3d11_win.h"
#include "libmfx_core_interface.h"

using namespace AV1EHW;
using namespace AV1EHW::Gen12;
using namespace AV1EHW::Windows;
using namespace AV1EHW::Windows::Gen12;

void DDI_D3D11::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
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

        auto ddiExecute = Glob::DDI_Execute::GetOrConstruct(strg);
        MFX_CHECK(!ddiExecute, MFX_ERR_NONE);

        ddiExecute.Push(
            [&](Glob::DDI_Execute::TRef::TExt, const DDIExecParam& ep)
        {
            return Execute(ep);
        });

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

        if (IsInitialized() && m_guid == guid)
            return MFX_ERR_NONE;

        return CreateAuxilliaryDevice(core, guid, par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height, false);
    });
}

mfxStatus DDI_D3D11::QueryEncodeCaps(
    ENCODE_CAPS_AV1& caps)
{
    MFX_CHECK(IsInitialized(), MFX_ERR_DEVICE_FAILED);

    caps = m_caps;

    return MFX_ERR_NONE;
}

void DDI_D3D11::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    FeatureBlocks blocks;

    blocks.m_trace[GetID()] = GetTrace();

    TPushIA push_ = [this, &blocks](mfxU32 blkId, TCallIA&& call)
    {
        blocks.Push<IA>({ GetID(), blkId }, std::move(call));
    };

    push_(BLK_CreateService
        , [this](StorageRW&, StorageRW& local) -> mfxStatus
    {
        mfxStatus sts = CreateAccelerationService(local);
        MFX_CHECK_STS(sts);

        {
            auto pInfo = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});

            sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, pInfo->Info);
            MFX_CHECK_STS(sts);

            local.Insert(Tmp::BSAllocInfo::Key, std::move(pInfo));
        }

        return sts;
    });

    push_(BLK_Register
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

    auto& q = FeatureBlocks::BQ<IA>::Get(blocks);
    q.remove_if([](FeatureBlocks::ID& id) { return id.BlockID == BLK_Register; });

    for (auto& itBlk : q)
    {
        Push(itBlk.BlockID, std::move(itBlk.m_call));
    }

    Push(BLK_Register
        , [this](StorageRW& , StorageRW&) -> mfxStatus
    {
        return MFX_ERR_NONE;
    });
}

void DDI_D3D11::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(IsInitialized(), MFX_ERR_NOT_INITIALIZED);
        auto& task = Task::Common::Get(s_task);

        MFX_CHECK(task.SkipCMD & SKIPCMD_NeedDriverCall, MFX_ERR_NONE);

        auto& par = Glob::DDI_SubmitParam::Get(global);

        auto Execute1 = [&](const DDIExecParam& ep) { return Execute(ep); };
        MFX_CHECK(!std::any_of(par.begin(), par.end(), Execute1), MFX_ERR_DEVICE_FAILED);

        mfxStatus sts = EndFrame();
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    });
}

mfxStatus DDI_D3D11::Execute(const DDIExecParam& ep)
{
    try
    {
        D3D11_VIDEO_DECODER_EXTENSION ext = {};

        ext.Function = ep.Function;
        ext.pPrivateInputData = ep.In.pData;
        ext.PrivateInputDataSize = ep.In.Size;
        ext.pPrivateOutputData = ep.Out.pData;
        ext.PrivateOutputDataSize = ep.Out.Size;
        ext.ResourceCount = ep.Resource.Size;
        ext.ppResourceList = (ID3D11Resource**)ep.Resource.pData;

        HRESULT hr = DecoderExtension(ext);
        MFX_CHECK_WITH_LOG(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED, "**FATAL** Got ERROR from the driver on DecoderExtension()\n");
    }
    catch (...)
    {
        AV1E_LOG("**FATAL** Got EXCEPTION from the driver on DecoderExtension()\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

void DDI_D3D11::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        if (!(task.SkipCMD & SKIPCMD_NeedDriverCall))
            return MFX_ERR_NONE;

        auto& ddiFB = Glob::DDI_Feedback::Get(global);
        auto pFB = (const ENCODE_QUERY_STATUS_PARAMS_AV1*)ddiFB.Get(task.StatusReportId);

        if (pFB && pFB->bStatus == ENCODE_OK)
            return MFX_ERR_NONE;

        HRESULT hr;

        try
        {
            hr = Execute(
                ddiFB.Function
                , (ENCODE_QUERY_STATUS_PARAMS_DESCR*)ddiFB.In.pData
                , ddiFB.In.Size
                , (ENCODE_QUERY_STATUS_PARAMS_AV1*)ddiFB.Out.pData
                , ddiFB.Out.Size);
        }
        catch (...)
        {
            AV1E_LOG("**FATAL** Got EXCEPTION from the driver on QueryTaskStatus()\n");
            return MFX_ERR_DEVICE_FAILED;
        }

        ddiFB.bNotReady = (hr == D3DERR_WASSTILLDRAWING);
        MFX_CHECK(!ddiFB.bNotReady, MFX_ERR_NONE);
        MFX_CHECK_WITH_LOG(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED, "**FATAL** Got ERROR from the driver on QueryTaskStatus()\n");

        return ddiFB.Update(task.StatusReportId);
    });
}

mfxStatus DDI_D3D11::BeginFrame()
{
    return MFX_ERR_NONE;
}

mfxStatus DDI_D3D11::EndFrame()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
    try
    {
        auto hr = m_vcontext->DecoderEndFrame(m_vdecoder);
        MFX_CHECK_WITH_LOG(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED, "**FATAL** Got ERROR from the driver on DecoderEndFrame()\n");
    }
    catch (...)
    {
        AV1E_LOG("**FATAL** Got EXCEPTION from the driver on DecoderEndFrame()\n");
        return MFX_ERR_DEVICE_FAILED;
    }
    return MFX_ERR_NONE;
}

HRESULT DDI_D3D11::DecoderExtension(D3D11_VIDEO_DECODER_EXTENSION const & ext)
{
    HRESULT hr;

    Trace(">>Function", ext.Function);

    if (!ext.pPrivateOutputData || ext.pPrivateInputData)
        Trace(ext, 0); //input

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
        hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
    }

    if (ext.pPrivateOutputData)
        Trace(ext, 1); //output

    Trace("<<HRESULT", hr);

    return hr;
}

mfxStatus DDI_D3D11::CreateAuxilliaryDevice(
    VideoCORE&    core
    , GUID        guid
    , mfxU32      width
    , mfxU32      height
    , bool        isTemporal)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "DDI_D3D11::CreateAuxilliaryDevice");
    HRESULT hr;
    m_pCore = &core;

    if (!m_vcontext)
    {
        D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(&core);
        MFX_CHECK_WITH_LOG(pD3d11, MFX_ERR_DEVICE_FAILED, "**FATAL** QueryCoreInterface() FAILED\n");

        m_vdevice = pD3d11->GetD3D11VideoDevice(isTemporal);
        MFX_CHECK_WITH_LOG(m_vdevice, MFX_ERR_DEVICE_FAILED, "**FATAL** GetD3D11VideoDevice() FAILED\n");

        m_vcontext = pD3d11->GetD3D11VideoContext(isTemporal);
        MFX_CHECK_WITH_LOG(m_vcontext, MFX_ERR_DEVICE_FAILED, "**FATAL** GetD3D11VideoContext() FAILED\n");
    }

    // [1] Query supported decode profiles
    {
        bool isFound = false;

        UINT profileCount;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfileCount");
            profileCount = m_vdevice->GetVideoDecoderProfileCount();
        }
        assert(profileCount > 0);

        for (UINT i = 0; i < profileCount; i++)
        {
            GUID profileGuid;

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfile");
                hr = m_vdevice->GetVideoDecoderProfile(i, &profileGuid);
                MFX_CHECK_WITH_LOG(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED, "**FATAL** Got ERROR from the driver on GetVideoDecoderProfile()\n");
            }

            if (guid == profileGuid)
            {
                isFound = true;
                break;
            }
        }
        MFX_CHECK_WITH_LOG(isFound, MFX_ERR_UNSUPPORTED, "**FATAL** Required GUID NOT FOUND in the supported profiles of the driver\n");
    }


    D3D11_VIDEO_DECODER_DESC    desc = { guid, width, height, DXGI_FORMAT_NV12 };

    // [2] Query the supported encode functions
    {
        mfxU32 count = 0;

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfigCount");
        hr = m_vdevice->GetVideoDecoderConfigCount(&desc, &count);
        MFX_CHECK_WITH_LOG(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED, "**FATAL** Got ERROR from the driver on GetVideoDecoderConfigCount()\n");

        // GetVideoDecoderConfigCount may return OK but the number of decoder configurations
        // that the driver supports for a specified video description = 0.
        // If we ignore this fact CreateVideoDecoder call will crash with an exception.
        MFX_CHECK_WITH_LOG(count, MFX_ERR_UNSUPPORTED, "**FATAL** Encode configurations NOT FOUND\n");
    }

    // [3] CreateVideoDecoder
    {
        D3D11_VIDEO_DECODER_CONFIG  config = {};
        config.ConfigDecoderSpecific         = m_configDecoderSpecific;
        config.guidConfigBitstreamEncryption = m_guidConfigBitstreamEncryption;

        if (!!m_vdecoder)
            m_vdecoder.Release();

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoder");
            hr = m_vdevice->CreateVideoDecoder(&desc, &config, &m_vdecoder);
            MFX_CHECK_WITH_LOG(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED, "**FATAL** Got ERROR from the driver on CreateVideoDecoder()\n");
        }
    }

    // [4] Query the encoding device capabilities
    {
        hr = Execute(ENCODE_QUERY_ACCEL_CAPS_ID, (void*)0, m_caps);
        MFX_CHECK_WITH_LOG(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED, "**FATAL** Got ERROR from the driver on Execute(ENCODE_QUERY_ACCEL_CAPS_ID)\n");
    }

    m_guid = guid;
    m_width = width;
    m_height = height;

    Trace(m_guid, 0);
    Trace(m_caps, 0);

    return MFX_ERR_NONE;
}

mfxStatus DDI_D3D11::CreateAccelerationService(StorageRW& local)
{
    MFX_CHECK(m_vdecoder, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(local.Contains(Tmp::DDI_InitParam::Key), MFX_ERR_NONE);

    auto params = Tmp::DDI_InitParam::Get(local);
    auto itDevicePar = std::find_if(params.begin(), params.end()
        , [](DDIExecParam& x) { return x.Function == AUXDEV_CREATE_ACCEL_SERVICE; });

    if (itDevicePar != params.end())
    {
        auto& ecd = *(ENCODE_CREATEDEVICE*)itDevicePar->Out.pData;
        bool bNeedNewDevice =
            ecd.CodingFunction != m_configDecoderSpecific
            || ecd.EncryptionMode != m_guidConfigBitstreamEncryption;

        if (bNeedNewDevice)
        {
            m_configDecoderSpecific = ecd.CodingFunction;
            m_guidConfigBitstreamEncryption = ecd.EncryptionMode;

            mfxStatus sts = CreateAuxilliaryDevice(*m_pCore, m_guid, m_width, m_height);
            MFX_CHECK_STS(sts);
        }

        params.erase(itDevicePar);
    }

    for (auto& par : params)
    {
        HRESULT hr = Execute(par.Function, par.In.pData, par.In.Size, par.Out.pData, par.Out.Size);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;
}

const std::map<DXGI_FORMAT, mfxU32> DXGIFormatToMFX =
{
      {DXGI_FORMAT_NV12, MFX_FOURCC_NV12}
    , {DXGI_FORMAT_P010, MFX_FOURCC_P010}
    , {DXGI_FORMAT_YUY2, MFX_FOURCC_YUY2}
    , {DXGI_FORMAT_Y210, MFX_FOURCC_P210}
    , {DXGI_FORMAT_P8  , MFX_FOURCC_P8 }
};

const std::map<D3DDDIFORMAT, D3DDDIFORMAT> DX9TypeToDX11 =
{
      {D3DDDIFMT_INTELENCODE_SPSDATA,            (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA}
    , {D3DDDIFMT_INTELENCODE_PPSDATA,            (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA}
    , {D3DDDIFMT_INTELENCODE_SLICEDATA,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA}
    , {D3DDDIFMT_INTELENCODE_QUANTDATA,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA}
    , {D3DDDIFMT_INTELENCODE_BITSTREAMDATA,      (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA}
    , {D3DDDIFMT_INTELENCODE_MBDATA,             (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA}
    , {D3DDDIFMT_INTELENCODE_SEIDATA,            (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SEIDATA}
    , {D3DDDIFMT_INTELENCODE_VUIDATA,            (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA}
    , {D3DDDIFMT_INTELENCODE_VMESTATE,           (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMESTATE}
    , {D3DDDIFMT_INTELENCODE_VMEINPUT,           (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEINPUT}
    , {D3DDDIFMT_INTELENCODE_VMEOUTPUT,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEOUTPUT}
    , {D3DDDIFMT_INTELENCODE_EFEDATA,            (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_EFEDATA}
    , {D3DDDIFMT_INTELENCODE_STATDATA,           (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_STATDATA}
    , {D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA,   (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA}
    , {D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA,    (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA}
    , {D3DDDIFMT_INTELENCODE_MBQPDATA,           (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA}
    , {D3DDDIFMT_INTELENCODE_MBSEGMENTMAP,       (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBSEGMENTMAP}
    , {D3DDDIFMT_INTELENCODE_SYNCOBJECT,         (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SYNCOBJECT}
};

mfxStatus DDI_D3D11::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameInfo& info)
{
    MFX_CHECK(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    type = DX9TypeToDX11.at(type);

    if (m_compBufInfo.empty())
    {
        ENCODE_FORMAT_COUNT cnt = {};

        auto hr = Execute(ENCODE_FORMAT_COUNT_ID, (void*)0, cnt);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(cnt.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(cnt.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats = {};
        encodeFormats.CompressedBufferInfoSize  = int(m_compBufInfo.size() * sizeof(ENCODE_COMP_BUFFER_INFO));
        encodeFormats.UncompressedFormatSize    = int(m_uncompBufInfo.size() * sizeof(D3DDDIFORMAT));
        encodeFormats.pCompressedBufferInfo     = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats      = &m_uncompBufInfo[0];

        hr = Execute(ENCODE_FORMATS_ID, (void*)0, encodeFormats);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);
    }

    for (auto& cbinf : m_compBufInfo)
    {
        if (cbinf.Type == type)
        {
            info.Width  = cbinf.CreationWidth;
            info.Height = cbinf.CreationHeight;
            info.FourCC = DXGIFormatToMFX.at((DXGI_FORMAT)cbinf.CompressedFormats);
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_DEVICE_FAILED;
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
