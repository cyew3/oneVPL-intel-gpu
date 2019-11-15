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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "hevcehw_g11_d3d9_win.h"
#include <d3d11.h>
#include <atlbase.h>

namespace HEVCEHW
{
namespace Windows
{
namespace Gen11
{

#pragma warning(push)
#pragma warning(disable:4250) //inherits via dominance
class DDI_D3D11
    : public virtual FeatureBase
    , protected virtual DDITracer
    , protected DDI_D3D9
{
public:
    DDI_D3D11(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
        , DDITracer(MFX_HW_D3D11)
        , DDI_D3D9(FeatureId)
    {
        SetTraceName("G11_DDI_D3D11");
    }

    struct CreateDeviceIn
    {
        bool                        bTemporal   = false;
        D3D11_VIDEO_DECODER_DESC    desc        = {};
        D3D11_VIDEO_DECODER_CONFIG  config      = {};
    };

    struct CreateDeviceOut
    {
        CComPtr<ID3D11VideoDecoder>   vdecoder;
        CComQIPtr<ID3D11VideoDevice>  vdevice;
        CComQIPtr<ID3D11VideoContext> vcontext;
    };

protected:
    CComPtr<ID3D11DeviceContext>  m_context;
    CComPtr<ID3D11VideoDecoder>   m_vdecoder;
    CComQIPtr<ID3D11VideoDevice>  m_vdevice;
    CComQIPtr<ID3D11VideoContext> m_vcontext;
    USHORT m_configDecoderSpecific         = ENCODE_ENC_PAK;
    GUID   m_guidConfigBitstreamEncryption = DXVA_NoEncrypt;

    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override
    {
        DDI_D3D9::Query1NoCaps(blocks, Push);
    };
    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override
    {
        DDI_D3D9::Query1WithCaps(blocks, Push);
    };
    virtual void InitExternal(const FeatureBlocks& blocks, TPushIE Push) override
    {
        DDI_D3D9::InitExternal(blocks, Push);
    };
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& /*blocks*/, TPushRS /*Push*/) override {};

    virtual bool IsInitialized() override { return !!m_vcontext; }

    virtual mfxStatus CreateAuxilliaryDevice(
        VideoCORE&  core
        , GUID        guid
        , mfxU32      width
        , mfxU32      height
        , bool        isTemporal = false) override;

    mfxStatus CreateAuxilliaryDevice(const DDIExecParam& ep);

    virtual mfxStatus CreateAccelerationService(StorageRW& local) override;
    virtual mfxStatus QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameInfo& info) override;

    HRESULT DecoderExtension(D3D11_VIDEO_DECODER_EXTENSION const & ext);

    mfxStatus BeginFrame();
    mfxStatus EndFrame();

    virtual mfxStatus DefaultExecute(const DDIExecParam& par) override;

    using DDI_D3D9::Execute;
};
#pragma warning(pop)

} //Gen11
} //Windows
} //namespace HEVCEHW

#endif
