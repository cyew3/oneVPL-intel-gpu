// Copyright (c) 2014-2019 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
#pragma once
#if defined(_WIN32) || defined(_WIN64)

#include "auxiliary_device.h"
#include "mfx_h265_encode_hw_ddi.h"
#include "mfx_h265_encode_hw_ddi_trace.h"
#include "mfx_h265_encode_hw_d3d_common.h"

namespace MfxHwH265Encode
{

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
class D3D9Encoder : public D3DXCommonEncoder, DDIHeaderPacker, DDITracer
{
public:
    D3D9Encoder();

    virtual
    ~D3D9Encoder();

    virtual
    mfxStatus CreateAuxilliaryDevice(
        VideoCORE * core,
        GUID        guid,
        MfxVideoParam const & par);

    virtual
    mfxStatus CreateAccelerationService(
        MfxVideoParam const & par);

    virtual
    mfxStatus Reset(
        MfxVideoParam const & par, bool bResetBRC);

    virtual
    mfxStatus Register(
        mfxFrameAllocResponse & response,
        D3DDDIFORMAT            type);

    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT           type,
        mfxFrameAllocRequest & request);

    virtual
    mfxStatus QueryEncodeCaps(
        MFX_ENCODE_CAPS_HEVC & caps);

    virtual
    mfxStatus QueryMbPerSec(
        mfxVideoParam const & par,
        mfxU32(&mbPerSec)[16]);

    virtual
    mfxStatus Destroy();

    virtual
    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut)
    {
        return DDIHeaderPacker::PackHeader(task, nut);
    }

protected:
    // async call
    virtual
    mfxStatus QueryStatusAsync(
        Task & task);

    virtual
    mfxStatus ExecuteImpl(
        Task const &task,
        mfxHDLPair surface);

private:
    VideoCORE*              m_core;
    std::unique_ptr<AuxiliaryDevice> m_auxDevice;

    GUID                 m_guid;
    mfxU32               m_width;
    mfxU32               m_height;
    MFX_ENCODE_CAPS_HEVC m_caps;
    ENCODE_ENC_CTRL_CAPS m_capsQuery;
    ENCODE_ENC_CTRL_CAPS m_capsGet;
    bool                 m_infoQueried;
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    bool                 m_widi;
#endif
    mfxU32               m_maxSlices;

    DDI_SPS                                     m_sps;
    DDI_PPS                                     m_pps;
#ifdef MFX_ENABLE_HEVC_CUSTOM_QMATRIX
    DXVA_Qmatrix_HEVC                           m_qMatrix; //buffer for quantization matrix
#endif
    std::vector<DDI_SLICE>                      m_slice;
    std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
    std::vector<D3DDDIFORMAT>                   m_uncompBufInfo;
    std::vector<ENCODE_COMPBUFFERDESC>          m_cbd;
    FeedbackStorage                             m_feedbackPool;

    std::vector<ENCODE_RECT> m_dirtyRects;

    template <class T> struct SizeOf { enum { value = sizeof(T) }; };
    template<> struct SizeOf <void> { enum { value = 0 }; };

    template <typename T, typename U>
    HRESULT Execute(mfxU32 func, T* in, mfxU32 inSizeInBytes, U* out, mfxU32 outSizeInBytes)
    {
        HRESULT hr;
        Trace(">>Function", func);
        TraceArray(in, inSizeInBytes / std::max<mfxU32>(SizeOf<T>::value, 1));
        hr = m_auxDevice->Execute(func, in, inSizeInBytes, out, outSizeInBytes);
        TraceArray(out, outSizeInBytes / std::max<mfxU32>(SizeOf<U>::value, 1));
        Trace("<<HRESULT", hr);
        return hr;
    }

    template <typename T, typename U>
    HRESULT Execute(mfxU32 func, T& in, U& out)
    {
        return Execute(func, &in, sizeof(in), &out, sizeof(out));
    }
    
    template <typename T>
    HRESULT Execute(mfxU32 func, T& in, void*)
    {
        return Execute(func, &in, sizeof(in), (void*)0, (mfxU32)0);
    }
    
    template <typename U>
    HRESULT Execute(mfxU32 func, void*, U& out)
    {
        return Execute(func, (void*)0, (mfxU32)0, &out, sizeof(out));
    }
};

typedef D3D9Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT, ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT, ENCODE_SET_SLICE_HEADER_HEVC_REXT> D3D9EncoderREXT;
typedef D3D9Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC, ENCODE_SET_PICTURE_PARAMETERS_HEVC, ENCODE_SET_SLICE_HEADER_HEVC> D3D9EncoderDefault;

}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)
#endif
