// Copyright (c) 2020 Intel Corporation
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

#include "ehw_device_dx11.h"
#include "ehw_utils_ddi.h"
#include <algorithm>
#include "feature_blocks/mfx_feature_blocks_utils.h"

namespace MfxEncodeHW
{
using namespace MfxFeatureBlocks;

mfxStatus DeviceDX11::Create(
    VideoCORE&    core
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

    DDIExecParam xpar;
    xpar.Function  = AUXDEV_CREATE_ACCEL_SERVICE;
    xpar.In.pData  = &in;
    xpar.In.Size   = sizeof(in);
    xpar.Out.pData = &out;
    xpar.Out.Size  = sizeof(out);

    auto sts = Execute(xpar);
    MFX_CHECK_STS(sts);

    SetDevice(xpar);

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX11::CreateVideoDecoder(const DDIExecParam& ep)
{
    ThrowIf(ep.Function != AUXDEV_CREATE_ACCEL_SERVICE, std::logic_error("Wrong Function for CreateAuxilliaryDevice"));
    ThrowIf(!ep.In.pData || ep.In.Size < sizeof(CreateDeviceIn), std::logic_error("Invalid DDIExecParam::In"));
    ThrowIf(!ep.Out.pData || ep.Out.Size < sizeof(CreateDeviceOut), std::logic_error("Invalid DDIExecParam::Out"));
    auto& in  = *(const CreateDeviceIn*)ep.In.pData;
    auto& out = *(CreateDeviceOut*)ep.Out.pData;

    bool bUseCurrDevice =
        m_vdevice
        && m_vcontext
        && m_vdecoder
        && in.config.ConfigDecoderSpecific == m_configDecoderSpecific
        && in.config.guidConfigBitstreamEncryption == m_guidConfigBitstreamEncryption
        && m_guid == in.desc.Guid;

    MFX_CHECK(!bUseCurrDevice, MFX_ERR_NONE);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CreateAuxilliaryDevice");
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

mfxStatus DeviceDX11::QueryCaps(void* pCaps, mfxU32 size)
{
    MFX_CHECK(IsValid(), MFX_ERR_NOT_INITIALIZED);

    if (m_caps.size() < size)
    {
        m_caps.resize(size);

        DDIExecParam xpar;
        xpar.Function  = ENCODE_QUERY_ACCEL_CAPS_ID;
        xpar.Out.pData = m_caps.data();
        xpar.Out.Size  = size;

        auto sts = Execute(xpar);
        MFX_CHECK_STS(sts);
    }

    std::copy_n(m_caps.data(), size, (mfxU8*)pCaps);

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX11::QueryCompBufferInfo(mfxU32 type, mfxFrameInfo& info)
{
    MFX_CHECK(IsValid(), MFX_ERR_NOT_INITIALIZED);

    type = m_DX9TypeToDX11.at(type);

    if (m_compBufInfo.empty())
    {
        ENCODE_FORMAT_COUNT encodeFormatCount = {};
        DDIExecParam xpar;
        xpar.Function  = ENCODE_FORMAT_COUNT_ID;
        xpar.Out.pData = &encodeFormatCount;
        xpar.Out.Size  = sizeof(encodeFormatCount);

        mfxStatus sts = Execute(xpar);
        MFX_CHECK_STS(sts);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize  = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize    = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo     = m_compBufInfo.data();
        encodeFormats.pUncompressedFormats      = m_uncompBufInfo.data();

        xpar = DDIExecParam();
        xpar.Function  = ENCODE_FORMATS_ID;
        xpar.Out.pData = &encodeFormats;
        xpar.Out.Size  = sizeof(encodeFormats);

        sts = Execute(xpar);
        MFX_CHECK_STS(sts);
        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
    }

    auto IsRequestedCB = [type](ENCODE_COMP_BUFFER_INFO& cb)
    {
        return cb.Type == D3DFORMAT(type);
    };

    auto itCB = std::find_if(m_compBufInfo.begin(), m_compBufInfo.end(), IsRequestedCB);
    MFX_CHECK(itCB != m_compBufInfo.end(), MFX_ERR_DEVICE_FAILED);

    info.Width  = itCB->CreationWidth;
    info.Height = itCB->CreationHeight;
    info.FourCC = m_DXGIFormatToMFX.at((DXGI_FORMAT)itCB->CompressedFormats);

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX11::Init(const std::list<DDIExecParam>* pPar)
{
    MFX_CHECK(IsValid(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(pPar, MFX_ERR_NONE);

    for (auto& par : *pPar)
    {
        auto sts = Execute(par);
        MFX_CHECK_STS(sts);

        if (par.Function == AUXDEV_CREATE_ACCEL_SERVICE)
        {
            SetDevice(par);
        }
    }

    return MFX_ERR_NONE;
}

void DeviceDX11::SetDevice(const DDIExecParam& ep)
{
    ThrowIf(ep.Function != AUXDEV_CREATE_ACCEL_SERVICE, std::logic_error("Wrong Function for CreateAuxilliaryDevice"));
    ThrowIf(!ep.In.pData || ep.In.Size < sizeof(CreateDeviceIn), std::logic_error("Invalid DDIExecParam::In"));
    ThrowIf(!ep.Out.pData || ep.Out.Size < sizeof(CreateDeviceOut), std::logic_error("Invalid DDIExecParam::Out"));

    auto& in = *(const CreateDeviceIn*)ep.In.pData;
    auto& out = *(CreateDeviceOut*)ep.Out.pData;

    m_vdevice                       = out.vdevice;
    m_vcontext                      = out.vcontext;
    m_vdecoder                      = out.vdecoder;
    m_guid                          = in.desc.Guid;
    m_width                         = in.desc.SampleWidth;
    m_height                        = in.desc.SampleHeight;
    m_configDecoderSpecific         = in.config.ConfigDecoderSpecific;
    m_guidConfigBitstreamEncryption = in.config.guidConfigBitstreamEncryption;
}

mfxStatus DeviceDX11::Execute(const DDIExecParam& ep)
{
    try
    {
        if (ep.Function == AUXDEV_CREATE_ACCEL_SERVICE)
        {
            return CreateVideoDecoder(ep);
        }

        D3D11_VIDEO_DECODER_EXTENSION ext = {};

        ext.Function              = ep.Function;
        ext.pPrivateInputData     = ep.In.pData;
        ext.PrivateInputDataSize  = ep.In.Size;
        ext.pPrivateOutputData    = ep.Out.pData;
        ext.PrivateOutputDataSize = ep.Out.Size;
        ext.ResourceCount         = ep.Resource.Size;
        ext.ppResourceList        = (ID3D11Resource**)ep.Resource.pData;

        Trace(ep, false, 0);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
            m_lastErr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
        }
        Trace(ep, true, m_lastErr);
        MFX_CHECK(SUCCEEDED(m_lastErr), MFX_ERR_DEVICE_FAILED);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX11::BeginPicture(mfxHDL)
{
    return MFX_ERR_NONE;
}

mfxStatus DeviceDX11::EndPicture()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
    try
    {
        m_lastErr = m_vcontext->DecoderEndFrame(m_vdecoder);
        MFX_CHECK(SUCCEEDED(m_lastErr), MFX_ERR_DEVICE_FAILED);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    return MFX_ERR_NONE;
}

mfxStatus DeviceDX11::QueryStatus(DDIFeedback& fb, mfxU32 id)
{
    return DDIParPacker::QueryStatus(*this, fb, id);
}

} //namespace MfxEncodeHW