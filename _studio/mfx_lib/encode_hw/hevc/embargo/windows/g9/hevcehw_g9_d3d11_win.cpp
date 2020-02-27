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

#include "hevcehw_g9_d3d11_win.h"
#include "libmfx_core_interface.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen9;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Gen9;

class DDIDeviceDX11
    : public MfxEncodeHW::DeviceDX11
    , protected DDITracer
{
public:
    DDIDeviceDX11(std::function<mfxStatus(const DDIExecParam&)>& execute)
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
        return MfxEncodeHW::DeviceDX11::Execute(par);
    }
    mfxStatus DefaultExecute(const DDIExecParam& par)
    {
        return MfxEncodeHW::DeviceDX11::Execute(par);
    }
    virtual mfxStatus QueryCompBufferInfo(mfxU32 type, mfxFrameInfo& info) override
    {
        auto sts = MfxEncodeHW::DeviceDX11::QueryCompBufferInfo(type, info);
        MFX_CHECK_STS(sts);

        SetIf(info.FourCC
            , m_DX9TypeToDX11.at(type) == D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA
            , mfxU32(MFX_FOURCC_P8_TEXTURE));

        return sts;
    }
protected:
    std::function<mfxStatus(const DDIExecParam&)>& m_execute;
};

DDI_D3D11::DDI_D3D11(mfxU32 FeatureId)
    : DDI_D3D9(FeatureId)
{
    SetTraceName("G9_DDI_D3D11");
    m_pDevice.reset(new DDIDeviceDX11(m_execute));
}

mfxStatus DDI_D3D11::DefaultExecute(const DDIExecParam& par)
{
    return dynamic_cast<DDIDeviceDX11&>(*m_pDevice).DefaultExecute(par);
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)