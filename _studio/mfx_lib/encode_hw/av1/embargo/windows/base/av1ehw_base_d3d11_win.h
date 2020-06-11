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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include <d3d11.h>
#include <atlbase.h>
#include "av1ehw_base.h"
#include "av1ehw_ddi.h"
#include "av1ehw_ddi_trace.h"
#include "av1ehw_base_data.h"
#include "auxiliary_device.h"
#include "av1ehw_base_iddi.h"

namespace AV1EHW
{
namespace Windows
{
namespace Base
{
using namespace AV1EHW::Base;

#pragma warning(push)
#pragma warning(disable:4250) //inherits via dominance
class DDI_D3D11
    : public virtual FeatureBase
    , protected DDITracer
    , protected AV1EHW::Base::IDDI
{
public:
    DDI_D3D11(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
        , AV1EHW::Base::IDDI(FeatureId)
    {
        SetTraceName("G12_DDI_D3D11");
    }

protected:
    VideoCORE*                           m_pCore                         = nullptr;
    std::unique_ptr<AuxiliaryDevice>     m_auxDevice;
    CComPtr<ID3D11DeviceContext>         m_context;
    CComPtr<ID3D11VideoDecoder>          m_vdecoder;
    CComQIPtr<ID3D11VideoDevice>         m_vdevice;
    CComQIPtr<ID3D11VideoContext>        m_vcontext;
    GUID                                 m_guid                          = {};
    mfxU32                               m_width                         = 0;
    mfxU32                               m_height                        = 0;
    ENCODE_CAPS_AV1                      m_caps                          = {};
    std::vector<ENCODE_COMP_BUFFER_INFO> m_compBufInfo;
    std::vector<D3DDDIFORMAT>            m_uncompBufInfo;
    USHORT                               m_configDecoderSpecific         = ENCODE_ENC_PAK;
    GUID                                 m_guidConfigBitstreamEncryption = DXVA_NoEncrypt;

    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;
    virtual void InitExternal(const FeatureBlocks& blocks, TPushIE Push) override;
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& /*blocks*/, TPushRS /*Push*/) override {};

    virtual bool IsInitialized() { return !!m_vcontext; }

    virtual mfxStatus CreateAuxilliaryDevice(
        VideoCORE&    core
        , GUID        guid
        , mfxU32      width
        , mfxU32      height
        , bool        isTemporal = false);

    virtual mfxStatus QueryEncodeCaps(ENCODE_CAPS_AV1& caps);
    virtual mfxStatus QueryEncodeCaps(mfxVideoParam& par, StorageW& strg, EncodeCapsAv1& caps);

    virtual mfxStatus CreateAccelerationService(StorageRW& local);
    virtual mfxStatus QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameInfo& info);

    HRESULT DecoderExtension(D3D11_VIDEO_DECODER_EXTENSION const & ext);

    mfxStatus BeginFrame();
    mfxStatus EndFrame();

    mfxStatus Execute(const DDIExecParam& par);

    template <typename T, typename U>
    HRESULT Execute(
        mfxU32 func
        , T* in
        , mfxU32 inSizeInBytes
        , U* out
        , mfxU32 outSizeInBytes)
    {
        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function                = func;
        ext.pPrivateInputData       = in;
        ext.PrivateInputDataSize    = inSizeInBytes;
        ext.pPrivateOutputData      = out;
        ext.PrivateOutputDataSize   = outSizeInBytes;
        return DecoderExtension(ext);
    }

    template <typename T, typename U>
    HRESULT Execute(mfxU32 func, T& in, U& out)
    {
        return Execute(func, &in, sizeof(in), &out, sizeof(out));
    }

    template <typename T>
    HRESULT Execute(mfxU32 func, T& in, void*)
    {
        return Execute(func, &in, sizeof(in), (void*)0, 0);
    }

    template <typename U>
    HRESULT Execute(mfxU32 func, void*, U& out)
    {
        return Execute(func, (void*)0, 0, &out, sizeof(out));
    }
};
#pragma warning(pop)

} //Base
} //Windows
} //namespace AV1EHW

#endif
