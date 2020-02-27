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

#include "ehw_device_dx9.h"
#include "ehw_utils_ddi.h"
#include "encoding_ddi.h"
#include <algorithm>

namespace MfxEncodeHW
{
mfxStatus DeviceDX9::Create(
    VideoCORE&    core
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

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX9::QueryCaps(void* pCaps, mfxU32 size)
{
    MFX_CHECK(IsValid(), MFX_ERR_NOT_INITIALIZED);

    if (m_caps.size() < size)
    {
        m_caps.resize(size);

        DDIExecParam xpar;
        xpar.Function  = AUXDEV_QUERY_ACCEL_CAPS;
        xpar.In.pData  = &m_guid;
        xpar.In.Size   = sizeof(m_guid);
        xpar.Out.pData = m_caps.data();
        xpar.Out.Size  = size;

        auto sts = Execute(xpar);
        MFX_CHECK_STS(sts);
    }

    std::copy_n(m_caps.data(), size, (mfxU8*)pCaps);

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX9::QueryCompBufferInfo(mfxU32 type, mfxFrameInfo& info)
{
    MFX_CHECK(IsValid(), MFX_ERR_NOT_INITIALIZED);

    if (m_compBufInfo.empty())
    {
        ENCODE_FORMAT_COUNT encodeFormatCount = {};
        DDIExecParam xpar;
        xpar.Function  = ENCODE_FORMAT_COUNT_ID;
        xpar.In.pData  = &m_guid;
        xpar.In.Size   = sizeof(m_guid);
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
    info.FourCC = itCB->CompressedFormats;

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX9::Init(const std::list<DDIExecParam>* pPar)
{
    MFX_CHECK(IsValid(), MFX_ERR_NOT_INITIALIZED);

    bool bUseDefault = !pPar
        || (std::none_of(pPar->begin(), pPar->end(), DDIExecParam::IsFunction<AUXDEV_CREATE_ACCEL_SERVICE>));

    mfxStatus sts;

    if (bUseDefault)
    {
        DXVADDI_VIDEODESC desc = {};
        desc.SampleWidth    = m_width;
        desc.SampleHeight   = m_height;
        desc.Format         = (D3DDDIFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'));

        ENCODE_CREATEDEVICE encodeCreateDevice = {};
        encodeCreateDevice.pVideoDesc     = &desc;
        encodeCreateDevice.CodingFunction = ENCODE_ENC_PAK;
        encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

        DDIExecParam xpar;
        xpar.Function  = AUXDEV_CREATE_ACCEL_SERVICE;
        xpar.In.pData  = &m_guid;
        xpar.In.Size   = sizeof(m_guid);
        xpar.Out.pData = &encodeCreateDevice;
        xpar.Out.Size  = sizeof(encodeCreateDevice);

        sts = Execute(xpar);
        MFX_CHECK_STS(sts);
    }

    MFX_CHECK(pPar, MFX_ERR_NONE);

    auto ExecuteWrap = [&sts, this](const DDIExecParam& xpar)
    {
        sts = Execute(xpar);
        return !sts;
    };
    bool bFailed = std::any_of(pPar->begin(), pPar->end(), ExecuteWrap);
    MFX_CHECK(!bFailed, sts);

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX9::Execute(const DDIExecParam& ep)
{
    Trace(ep, false, 0);
    try
    {
        if (ep.Function == ENCODE_ENC_PAK_ID)
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_PAK_ID");
            m_lastErr = m_auxDevice->Execute(
                ep.Function
                , ep.In.pData
                , ep.In.Size
                , ep.Out.pData
                , ep.Out.Size);
        }
        else
        {
            m_lastErr = m_auxDevice->Execute(
                ep.Function
                , ep.In.pData
                , ep.In.Size
                , ep.Out.pData
                , ep.Out.Size);
        }
    }
    catch (...)
    {
        m_lastErr = E_FAIL;
    }
    Trace(ep, true, m_lastErr);
    MFX_CHECK(SUCCEEDED(m_lastErr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus DeviceDX9::BeginPicture(mfxHDL hdl)
{
    mfxHDLPair* pHDL = (mfxHDLPair*)hdl;
    MFX_CHECK_NULL_PTR1(pHDL);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "BeginFrame");
        m_lastErr = m_auxDevice->BeginFrame((IDirect3DSurface9*)pHDL->first, pHDL->second);
    }
    MFX_CHECK(SUCCEEDED(m_lastErr), MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus DeviceDX9::EndPicture()
{
    HANDLE hdl;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "EndFrame");
        m_lastErr = m_auxDevice->EndFrame(&hdl);
    }
    MFX_CHECK(SUCCEEDED(m_lastErr), MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus DeviceDX9::QueryStatus(DDIFeedback& fb, mfxU32 id)
{
    return DDIParPacker::QueryStatus(*this, fb, id);
}

} //namespace MfxEncodeHW