// Copyright (c) 2016-2020 Intel Corporation
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

#if defined (MFX_VA_WIN)

#include "auxiliary_device.h"
#include "mfx_vp9_encode_hw_d3d_common.h"
#include <atlbase.h>

namespace MfxHwVP9Encode
{

class D3D11Encoder : public D3DXCommonEncoder
{
public:
    D3D11Encoder();

    virtual
    ~D3D11Encoder();

    virtual
    mfxStatus CreateAuxilliaryDevice(
        VideoCORE* core,
        GUID       guid,
        VP9MfxVideoParam const & par) override;

    virtual
    mfxStatus CreateAccelerationService(
        VP9MfxVideoParam const & par) override;

    virtual
    mfxStatus Reset(
        VP9MfxVideoParam const & par) override;

    virtual
    mfxStatus Register(
        mfxFrameAllocResponse& response,
        D3DDDIFORMAT type) override;

    // (mfxExecuteBuffers& data)
    virtual
    mfxStatus Execute(
        Task const &task,
        mfxHDLPair pair) override;

    // recomendation from HW
    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT type,
        mfxFrameAllocRequest& request,
        mfxU32 frameWidth,
        mfxU32 frameHeight) override;

    virtual
    mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_VP9& caps) override;

    virtual
    mfxStatus QueryStatusAsync(
        Task & task) override;

    virtual
        mfxU32 GetReconSurfFourCC() override;

    virtual
    mfxStatus Destroy() override;

    D3D11Encoder(const D3D11Encoder&) = delete;
    D3D11Encoder& operator=(const D3D11Encoder&) = delete;

private:
    GUID m_guid;

    CComPtr<ID3D11VideoDecoder>                 m_pVDecoder;
    CComQIPtr<ID3D11VideoDevice>                m_pVDevice;
    CComQIPtr<ID3D11VideoContext>               m_pVContext;


    VideoCORE*  m_pmfxCore;
    ENCODE_CAPS_VP9    m_caps;
    mfxPlatform        m_platform;

    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 m_sps;
    ENCODE_SET_PICTURE_PARAMETERS_VP9  m_pps;
    ENCODE_SEGMENT_PARAMETERS          m_seg;
    std::vector<ENCODE_QUERY_STATUS_PARAMS>     m_feedbackUpdate;
    CachedFeedback                              m_feedbackCached;

    std::vector<mfxHDLPair>                     m_reconQueue;
    std::vector<mfxHDLPair>                     m_bsQueue;
    std::vector<mfxHDLPair>                     m_segmapQueue;

    bool                                        m_infoQueried;
    std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
    std::vector<D3DDDIFORMAT>                   m_uncompBufInfo; // isn't used. Required just to query buffer info from driver.

    std::vector<mfxU8> m_frameHeaderBuf;
    ENCODE_PACKEDHEADER_DATA m_descForFrameHeader;

    VP9SeqLevelParam m_seqParam;

    mfxU32 m_width;
    mfxU32 m_height;

    UMC::Mutex m_guard;
};
} // MfxHwVP9Encode

#endif // (MFX_VA_WIN) 