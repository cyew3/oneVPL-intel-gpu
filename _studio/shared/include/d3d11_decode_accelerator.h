// Copyright (c) 2011-2019 Intel Corporation
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

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#ifndef _D3D11_DECODE_ACCELERATOR_H_
#define _D3D11_DECODE_ACCELERATOR_H_

#define UMC_VA_DXVA

#include <atlbase.h>
#include <d3d11.h>


#include "umc_va_dxva2.h"
#include "mfxvideo.h"

class  D3D11VideoCORE;
class mfx_UMC_FrameAllocator;

struct MFXD3D11AcceleratorParams
    : public UMC::VideoAcceleratorParams
{

    DYNAMIC_CAST_DECL(MFXD3D11AcceleratorParams, UMC::VideoAcceleratorParams)

    MFXD3D11AcceleratorParams()
        : m_video_device(nullptr)
        , m_video_context(nullptr)
        , m_video_processing(nullptr)
    {}

    ID3D11VideoDevice         *m_video_device;
    ID3D11VideoContext        *m_video_context;
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing  *m_video_processing;
#endif
};

class MFXD3D11Accelerator : public UMC::DXAccelerator
{

public:

    MFXD3D11Accelerator();

    ~MFXD3D11Accelerator()
    {
        Close();
    };

    // I/F between core and accelerator
    UMC::Status Init(UMC::VideoAcceleratorParams*) override;
    UMC::Status Close() override;

    // I/F between UMC decoders and accelerator
    UMC::Status BeginFrame(Ipp32s index) override;
    UMC::Status Execute() override;
    UMC::Status ExecuteExtensionBuffer(void * buffer) override;
    UMC::Status QueryTaskStatus(Ipp32s , void *, void *) override
    { return UMC::UMC_ERR_UNSUPPORTED;}
    UMC::Status EndFrame(void * handle = 0) override;

    bool IsIntelCustomGUID() const override;

    UMC::Status GetVideoDecoderDriverHandle(HANDLE*);

    void GetVideoDecoder(void **handle)
    {
        *handle = m_pDecoder;
    };

    UMC::Status ExecuteExtension(int, ExtensionData const&) override;

private:

    D3D11_VIDEO_DECODER_BUFFER_TYPE MapDXVAToD3D11BufType(const Ipp32s DXVABufType) const;

    UMC::Status GetSuitVideoDecoderConfig(D3D11_VIDEO_DECODER_DESC*, D3D11_VIDEO_DECODER_CONFIG*);
    UMC::Status GetCompBufferInternal(UMC::UMCVACompBuffer*) override;
    UMC::Status ReleaseBufferInternal(UMC::UMCVACompBuffer*) override;

private:

    ID3D11VideoDevice               *m_pVideoDevice;
    ID3D11VideoContext              *m_pVideoContext;

    // we own video decoder, let using com pointer
    CComPtr<ID3D11VideoDecoder>      m_pDecoder;

    // current decoder
    GUID                             m_DecoderGuid;

    CComPtr<ID3D11VideoDecoderOutputView> m_pVDOView;

    mfxU32                           m_numberSurfaces;
};


#endif
#endif
#endif
