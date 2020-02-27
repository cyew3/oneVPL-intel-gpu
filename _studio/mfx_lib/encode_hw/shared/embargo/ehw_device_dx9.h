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
#include "auxiliary_device.h"
#include <vector>

namespace MfxEncodeHW
{

class DeviceDX9
    : public Device
{
public:
    virtual mfxStatus Create(
        VideoCORE&    core
        , GUID        guid
        , mfxU32      width
        , mfxU32      height
        , bool        isTemporal) override;

    virtual bool      IsValid() const override { return m_pCore && m_auxDevice; }
    virtual mfxStatus QueryCaps(void* pCaps, mfxU32 size) override;
    virtual mfxStatus QueryCompBufferInfo(mfxU32, mfxFrameInfo&) override;
    virtual mfxStatus Init(const std::list<DDIExecParam>*) override;
    virtual mfxStatus QueryStatus(DDIFeedback& fb, mfxU32 id) override;
    virtual mfxStatus Execute(const DDIExecParam&) override;
    virtual mfxStatus BeginPicture(mfxHDL) override;
    virtual mfxStatus EndPicture() override;
    virtual mfxU32    GetLastErr() const override { return mfxU32(m_lastErr); }

protected:
    std::unique_ptr<AuxiliaryDevice>     m_auxDevice;
    std::vector<mfxU8>                   m_caps;
    std::vector<ENCODE_COMP_BUFFER_INFO> m_compBufInfo;
    std::vector<D3DDDIFORMAT>            m_uncompBufInfo;
    VideoCORE*                           m_pCore   = nullptr;
    GUID                                 m_guid    = {};
    mfxU32                               m_width   = 0;
    mfxU32                               m_height  = 0;
    HRESULT                              m_lastErr = S_OK;
};

} //namespace MfxEncodeHW