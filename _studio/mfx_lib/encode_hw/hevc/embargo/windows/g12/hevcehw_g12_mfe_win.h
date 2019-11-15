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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_MFE)

#include "hevcehw_g12_mfe.h"
#include "mfx_mfe_adapter_dxva.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Gen12
{
class MFE
    : public HEVCEHW::Gen12::MFE
{
public:
    MFE(mfxU32 FeatureId)
        : HEVCEHW::Gen12::MFE(FeatureId)
    {}

protected:
    virtual void InitExternal(const FeatureBlocks& blocks, TPushIE Push) override;
    virtual void InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push) override;
    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push) override;

    MFEDXVAEncoder*                    m_pMfeAdapter = nullptr;
    ENCODE_SINGLE_STREAM_INFO          m_streamInfo  = {};
    MFE_CODEC                          m_codec       = CODEC_HEVC;
    ENCODE_EVENT_DESCR                 m_mfeGpuEvent = {};
    bool                               m_bSendEvent  = false;
    std::vector<ENCODE_COMPBUFFERDESC> m_cbd;
};
} //Gen12
} //namespace Windows
} //namespace HEVCEHW

#endif
