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

#include "hevcehw_base.h"
#include "hevcehw_ddi.h"
#include "hevcehw_ddi_trace.h"
#include "hevcehw_g9_data.h"
#include "auxiliary_device.h"
#include "hevcehw_g9_iddi.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Gen9
{
using namespace HEVCEHW::Gen9;

#pragma warning(push)
#pragma warning(disable:4250) //inherits via dominance
class DDI_D3D9
    : public virtual FeatureBase
    , protected virtual DDITracer
    , protected HEVCEHW::Gen9::IDDI
{
public:
    DDI_D3D9(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
        , DDITracer(MFX_HW_D3D9)
        , HEVCEHW::Gen9::IDDI(FeatureId)
    {
        SetTraceName("G9_DDI_D3D9");
    }

protected:
    static const D3DDDIFORMAT D3DDDIFMT_NV12 = (D3DDDIFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'));
    VideoCORE*                           m_pCore        = nullptr;
    std::unique_ptr<AuxiliaryDevice>     m_auxDevice;
    GUID                                 m_guid         = {};
    mfxU32                               m_width        = 0;
    mfxU32                               m_height       = 0;
    ENCODE_CAPS_HEVC                     m_caps         = {};
    std::vector<ENCODE_COMP_BUFFER_INFO> m_compBufInfo;
    std::vector<D3DDDIFORMAT>            m_uncompBufInfo;
    NotNull<const StorageR*>             m_pCurrStorage;
    HRESULT                              m_lastRes      = S_OK;

    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void InitExternal(const FeatureBlocks& blocks, TPushIE Push) override;
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& /*blocks*/, TPushRS /*Push*/) override {};

    virtual bool IsInitialized() { return !!m_auxDevice; }

    virtual mfxStatus CreateAuxilliaryDevice(
        VideoCORE&  core
        , GUID        guid
        , mfxU32      width
        , mfxU32      height
        , bool        isTemporal = false);

    virtual mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_HEVC & caps);

    virtual mfxStatus CreateAccelerationService(StorageRW& local);
    virtual mfxStatus QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameInfo& info);

    virtual mfxStatus DefaultExecute(const DDIExecParam& ep);
    mfxStatus Execute(const DDIExecParam& ep)
    {
        m_lastRes = S_OK;
        return Glob::DDI_Execute::Get(*m_pCurrStorage)(ep);
    }

    template <class T> struct SizeOf { enum { value = sizeof(T) }; };
    template<> struct SizeOf <void> { enum { value = 0 }; };

    template <typename T, typename U>
    mfxStatus Execute(mfxU32 func, T* in, mfxU32 inSizeInBytes, U* out, mfxU32 outSizeInBytes)
    {
        DDIExecParam xPar;
        xPar.Function  = func;
        xPar.In.pData  = in;
        xPar.In.Size   = inSizeInBytes;
        xPar.Out.pData = out;
        xPar.Out.Size  = outSizeInBytes;
        return Execute(xPar);
    }

    template <typename T, typename U>
    mfxStatus Execute(mfxU32 func, T& in, U& out)
    {
        return Execute(func, &in, sizeof(in), &out, sizeof(out));
    }

    template <typename T>
    mfxStatus Execute(mfxU32 func, T& in, void*)
    {
        return Execute(func, &in, sizeof(in), (void*)0, 0);
    }

    template <typename U>
    mfxStatus Execute(mfxU32 func, void*, U& out)
    {
        return Execute(func, (void*)0, 0, &out, sizeof(out));
    }
};
#pragma warning(pop)

} //Gen9
} //Windows
} //namespace HEVCEHW

#endif
