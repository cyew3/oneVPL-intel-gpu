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

#pragma once
#include "ehw_device.h"
#include "encoding_ddi.h"
#include <vector>
#include <map>
#include <d3d11.h>
#include <atlbase.h>

namespace MfxEncodeHW
{

class DeviceDX11
    : public Device
{
public:
    virtual mfxStatus Create(
        VideoCORE&    core
        , GUID        guid
        , mfxU32      width
        , mfxU32      height
        , bool        isTemporal) override;

    virtual bool      IsValid() const override { return m_pCore && m_vcontext; }
    virtual mfxStatus QueryCaps(void* pCaps, mfxU32 size) override;
    virtual mfxStatus QueryCompBufferInfo(mfxU32, mfxFrameInfo&) override;
    virtual mfxStatus Init(const std::list<DDIExecParam>*) override;
    virtual mfxStatus Execute(const DDIExecParam&) override;
    virtual mfxStatus QueryStatus(DDIFeedback& fb, mfxU32 id) override;
    virtual mfxStatus BeginPicture(mfxHDL) override;
    virtual mfxStatus EndPicture() override;
    virtual mfxU32    GetLastErr() const override { return m_lastErr; }

    struct CreateDeviceIn
    {
        bool                        bTemporal = false;
        D3D11_VIDEO_DECODER_DESC    desc      = {};
        D3D11_VIDEO_DECODER_CONFIG  config    = {};
    };

    struct CreateDeviceOut
    {
        CComPtr<ID3D11VideoDecoder>   vdecoder;
        CComQIPtr<ID3D11VideoDevice>  vdevice;
        CComQIPtr<ID3D11VideoContext> vcontext;
    };

protected:
    CComPtr<ID3D11DeviceContext>         m_context;
    CComPtr<ID3D11VideoDecoder>          m_vdecoder;
    CComQIPtr<ID3D11VideoDevice>         m_vdevice;
    CComQIPtr<ID3D11VideoContext>        m_vcontext;
    std::vector<mfxU8>                   m_caps;
    std::vector<ENCODE_COMP_BUFFER_INFO> m_compBufInfo;
    std::vector<D3DDDIFORMAT>            m_uncompBufInfo;
    VideoCORE*                           m_pCore        = nullptr;
    GUID                                 m_guid         = {};
    mfxU32                               m_width        = 0;
    mfxU32                               m_height       = 0;
    mfxU32                               m_lastErr      = 0;
    USHORT                               m_configDecoderSpecific         = ENCODE_ENC_PAK;
    GUID                                 m_guidConfigBitstreamEncryption = DXVA_NoEncrypt;
    std::map<mfxU32, mfxU32> m_DX9TypeToDX11 =
    {
          {(mfxU32)D3DDDIFMT_INTELENCODE_SPSDATA,          (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_PPSDATA,          (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_SLICEDATA,        (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_QUANTDATA,        (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_BITSTREAMDATA,    (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_MBDATA,           (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_SEIDATA,          (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_SEIDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_VUIDATA,          (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_VMESTATE,         (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMESTATE}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_VMEINPUT,         (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEINPUT}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_VMEOUTPUT,        (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEOUTPUT}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_EFEDATA,          (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_EFEDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_STATDATA,         (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_STATDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA,  (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA}
        , {(mfxU32)D3DDDIFMT_INTELENCODE_MBQPDATA,         (mfxU32)D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA}
    };
    std::map<DXGI_FORMAT, mfxU32> m_DXGIFormatToMFX =
    {
          {DXGI_FORMAT_NV12, MFX_FOURCC_NV12}
        , {DXGI_FORMAT_P010, MFX_FOURCC_P010}
        , {DXGI_FORMAT_YUY2, MFX_FOURCC_YUY2}
        , {DXGI_FORMAT_Y210, MFX_FOURCC_P210}
        , {DXGI_FORMAT_P8  , MFX_FOURCC_P8  }
    };


    mfxStatus CreateVideoDecoder(const DDIExecParam&);
    void SetDevice(const DDIExecParam&);
};

} //namespace MfxEncodeHW