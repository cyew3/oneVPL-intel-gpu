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
#include "av1ehw_base_iddi.h"
#include "ehw_device_dx11.h"

namespace AV1EHW
{
namespace Windows
{
namespace Base
{
using namespace AV1EHW::Base;

class DDI_D3D11
    : public AV1EHW::Base::IDDI
{
public:
    DDI_D3D11(mfxU32 FeatureId);

    using CreateDeviceIn = MfxEncodeHW::DeviceDX11::CreateDeviceIn;
    using CreateDeviceOut = MfxEncodeHW::DeviceDX11::CreateDeviceOut;

protected:
    std::unique_ptr<MfxEncodeHW::Device> m_pDevice;
    GUID m_guid = {};
    std::function<mfxStatus(const DDIExecParam&)> m_execute;

    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;
    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void InitExternal(const FeatureBlocks& blocks, TPushIE Push) override;
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& /*blocks*/, TPushRS /*Push*/) override {};

    void SetStorage(const StorageR& s)
    {
        m_execute = Glob::DDI_Execute::Get(s);
    }

    virtual mfxStatus DefaultExecute(const DDIExecParam& par);
    virtual mfxStatus Register(StorageRW&) { return MFX_ERR_NONE; };

    mfxStatus QueryEncodeCaps(mfxVideoParam& par, StorageW& strg, EncodeCapsAv1& caps);

};

} //Base
} //Windows
} //namespace AV1EHW

#endif
