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

#include "hevcehw_g11_d3d11_win.h"
#include "libmfx_core_interface.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Gen11;

void DDI_D3D11::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    FeatureBlocks blocks;

    blocks.m_trace[GetID()] = GetTrace();

    DDI_D3D9::InitAlloc(blocks
        , [this, &blocks](mfxU32 blkId, TCallIA&& call)
    {
        blocks.Push<IA>({ GetID(), blkId }, std::move(call));
    });

    auto& q = FeatureBlocks::BQ<IA>::Get(blocks);
    q.remove_if([](FeatureBlocks::ID& id) { return id.BlockID == BLK_Register; });

    for (auto& itBlk : q)
    {
        Push(itBlk.BlockID, std::move(itBlk.m_call));
    }

    Push(BLK_Register
        , [this](StorageRW& /*strg*/, StorageRW&) -> mfxStatus
    {
        return MFX_ERR_NONE;
    });
}

mfxStatus DDI_D3D11::DefaultExecute(const DDIExecParam& ep)
{
    try
    {
        if (ep.Function == AUXDEV_CREATE_ACCEL_SERVICE)
        {
            return CreateAuxilliaryDevice(ep);
        }

        D3D11_VIDEO_DECODER_EXTENSION ext = {};
                
        ext.Function              = ep.Function;
        ext.pPrivateInputData     = ep.In.pData;
        ext.PrivateInputDataSize  = ep.In.Size;
        ext.pPrivateOutputData    = ep.Out.pData;
        ext.PrivateOutputDataSize = ep.Out.Size;
        ext.ResourceCount         = ep.Resource.Size;
        ext.ppResourceList        = (ID3D11Resource**)ep.Resource.pData;

        HRESULT hr = DecoderExtension(ext);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

void DDI_D3D11::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        MFX_CHECK(task.SkipCMD & SKIPCMD_NeedDriverCall, MFX_ERR_NONE);

        auto& par = Glob::DDI_SubmitParam::Get(global);

        m_pCurrStorage = &global;

        auto Execute1 = [&](const DDIExecParam& ep) { return Execute(ep); };
        MFX_CHECK(!std::any_of(par.begin(), par.end(), Execute1), MFX_ERR_DEVICE_FAILED);

        mfxStatus sts = EndFrame();
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
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    return MFX_ERR_NONE;
}

HRESULT DDI_D3D11::DecoderExtension(D3D11_VIDEO_DECODER_EXTENSION const & ext)
{
    TraceFunc(false, ext.Function, ext.pPrivateInputData, ext.PrivateInputDataSize);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
        m_lastRes = m_vcontext->DecoderExtension(m_vdecoder, &ext);
    }
    Trace("==HRESULT", m_lastRes);
    TraceFunc(true, ext.Function, ext.pPrivateOutputData, ext.PrivateOutputDataSize);

    return m_lastRes;
}

mfxStatus DDI_D3D11::CreateAuxilliaryDevice(const DDIExecParam& ep)
{
    ThrowAssert(ep.Function != AUXDEV_CREATE_ACCEL_SERVICE, "Wrong Function for CreateAuxilliaryDevice");
    ThrowAssert(!ep.In.pData || ep.In.Size < sizeof(CreateDeviceIn), "Invalid DDIExecParam::In");
    ThrowAssert(!ep.Out.pData || ep.Out.Size < sizeof(CreateDeviceOut), "Invalid DDIExecParam::Out");
    auto& in = *(const CreateDeviceIn*)ep.In.pData;
    auto& out = *(CreateDeviceOut*)ep.Out.pData;

    bool bUseCurrDevice =
        m_vdevice
        && m_vcontext
        && m_vdecoder
        && in.config.ConfigDecoderSpecific == m_configDecoderSpecific
        && in.config.guidConfigBitstreamEncryption == m_guidConfigBitstreamEncryption
        && m_guid == in.desc.Guid;

    MFX_CHECK(!bUseCurrDevice, MFX_ERR_NONE);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "DDI_D3D11::CreateAuxilliaryDevice");
    HRESULT hr;

    D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(m_pCore);
    MFX_CHECK(pD3d11, MFX_ERR_DEVICE_FAILED)

    out.vdevice = pD3d11->GetD3D11VideoDevice(in.bTemporal);
    MFX_CHECK(out.vdevice, MFX_ERR_DEVICE_FAILED);

    out.vcontext = pD3d11->GetD3D11VideoContext(in.bTemporal);
    MFX_CHECK(out.vcontext, MFX_ERR_DEVICE_FAILED);

    // [1] Query supported decode profiles
    {
        bool isFound = false;

        UINT profileCount;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfileCount");
            profileCount = out.vdevice->GetVideoDecoderProfileCount();
        }
        assert(profileCount > 0);

        for (UINT i = 0; i < profileCount; ++i)
        {
            GUID profileGuid;

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfile");
                hr = out.vdevice->GetVideoDecoderProfile(i, &profileGuid);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

            if (in.desc.Guid == profileGuid)
            {
                isFound = true;
                break;
            }
        }
        MFX_CHECK(isFound, MFX_ERR_UNSUPPORTED);
    }

    // [2] Query the supported encode functions
    {
        mfxU32 count = 0;

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfigCount");
        hr = out.vdevice->GetVideoDecoderConfigCount(&in.desc, &count);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        // GetVideoDecoderConfigCount may return OK but the number of decoder configurations
        // that the driver supports for a specified video description = 0.
        // If we ignore this fact CreateVideoDecoder call will crash with an exception.
        MFX_CHECK(count, MFX_ERR_UNSUPPORTED);
    }

    // [3] CreateVideoDecoder
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoder");
        hr = out.vdevice->CreateVideoDecoder(&in.desc, &in.config, &out.vdecoder);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;
}

mfxStatus DDI_D3D11::CreateAuxilliaryDevice(
    VideoCORE&  core
    , GUID        guid
    , mfxU32      width
    , mfxU32      height
    , bool        isTemporal)
{
    CreateDeviceIn  in;
    CreateDeviceOut out;

    m_pCore = &core;

    in.bTemporal                            = isTemporal;
    in.desc                                 = { guid, width, height, DXGI_FORMAT_NV12 };
    in.config.ConfigDecoderSpecific         = m_configDecoderSpecific;
    in.config.guidConfigBitstreamEncryption = m_guidConfigBitstreamEncryption;

    auto sts = Execute(AUXDEV_CREATE_ACCEL_SERVICE, in, out);
    MFX_CHECK_STS(sts);

    m_vdevice   = out.vdevice;
    m_vcontext  = out.vcontext;
    m_vdecoder  = out.vdecoder;
    m_guid      = in.desc.Guid;
    m_width     = in.desc.SampleWidth;
    m_height    = in.desc.SampleHeight;

    // [4] Query the encoding device capabilities
    sts = Execute(ENCODE_QUERY_ACCEL_CAPS_ID, (void*)0, m_caps);
    MFX_CHECK_STS(sts);

    Trace(m_guid, 0);
    Trace(m_caps, 0);

    return MFX_ERR_NONE;
}

mfxStatus DDI_D3D11::CreateAccelerationService(StorageRW& local)
{
    MFX_CHECK(m_vdecoder, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(local.Contains(Tmp::DDI_InitParam::Key), MFX_ERR_NONE);

    auto params = Tmp::DDI_InitParam::Get(local);

    for (auto& par : params)
    {
        auto sts = Execute(par);
        MFX_CHECK_STS(sts);
    }

    auto itDevicePar = std::find_if(params.begin(), params.end()
        , [](DDIExecParam& x) { return x.Function == AUXDEV_CREATE_ACCEL_SERVICE; });

    if (itDevicePar != params.end())
    {
        auto& ep = *itDevicePar;
        ThrowAssert(!ep.In.pData || ep.In.Size < sizeof(CreateDeviceIn), "Invalid DDIExecParam::In");

        auto& in = *(const CreateDeviceIn*)ep.In.pData;

        m_configDecoderSpecific         = in.config.ConfigDecoderSpecific;
        m_guidConfigBitstreamEncryption = in.config.guidConfigBitstreamEncryption;
    }

    return MFX_ERR_NONE;
}

const std::map<DXGI_FORMAT, mfxU32> DXGIFormatToMFX =
{
      {DXGI_FORMAT_NV12, MFX_FOURCC_NV12}
    , {DXGI_FORMAT_P010, MFX_FOURCC_P010}
    , {DXGI_FORMAT_YUY2, MFX_FOURCC_YUY2}
    , {DXGI_FORMAT_Y210, MFX_FOURCC_P210}
    , {DXGI_FORMAT_P8  , MFX_FOURCC_P8  }
};

const std::map<D3DDDIFORMAT, D3DDDIFORMAT> DX9TypeToDX11 =
{
      {D3DDDIFMT_INTELENCODE_SPSDATA,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA}
    , {D3DDDIFMT_INTELENCODE_PPSDATA,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA}
    , {D3DDDIFMT_INTELENCODE_SLICEDATA,        (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA}
    , {D3DDDIFMT_INTELENCODE_QUANTDATA,        (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA}
    , {D3DDDIFMT_INTELENCODE_BITSTREAMDATA,    (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA}
    , {D3DDDIFMT_INTELENCODE_MBDATA,           (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA}
    , {D3DDDIFMT_INTELENCODE_SEIDATA,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SEIDATA}
    , {D3DDDIFMT_INTELENCODE_VUIDATA,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA}
    , {D3DDDIFMT_INTELENCODE_VMESTATE,         (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMESTATE}
    , {D3DDDIFMT_INTELENCODE_VMEINPUT,         (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEINPUT}
    , {D3DDDIFMT_INTELENCODE_VMEOUTPUT,        (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEOUTPUT}
    , {D3DDDIFMT_INTELENCODE_EFEDATA,          (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_EFEDATA}
    , {D3DDDIFMT_INTELENCODE_STATDATA,         (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_STATDATA}
    , {D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA}
    , {D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA,  (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA}
    , {D3DDDIFMT_INTELENCODE_MBQPDATA,         (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA}
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
            info.FourCC = (type != D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA)
                ? DXGIFormatToMFX.at((DXGI_FORMAT)cbinf.CompressedFormats)
                : MFX_FOURCC_P8_TEXTURE;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_DEVICE_FAILED;
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)