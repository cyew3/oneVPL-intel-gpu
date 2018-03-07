/******************************************************************************\
Copyright (c) 2005-2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "pch.h"
#include "UWPRenderer.h"

CUWPRenderer::CUWPRenderer()
{
    pVideoProcessorEnum = NULL;
    pVideoProcessor = NULL;
    prevFrameTimestamp = 0;
    SetFrameRate(100);
    isPlaying = true;
}

mfxStatus CUWPRenderer::CreateSwapChain(mfxHDL pDevHdl, UINT width, UINT height, bool is10Bit)
{

    ID3D11Device* pDev = reinterpret_cast<ID3D11Device*>(pDevHdl);

    CComPtr<IDXGIFactory2> pDXGIFactory;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)(&pDXGIFactory))))
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    MSDK_CHECK_POINTER(pDXGIFactory.p, MFX_ERR_NULL_PTR);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

    swapChainDesc.Format = (is10Bit) ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;                          // Don't use multi-sampling.
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;                               // Use double buffering to minimize latency.
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    try
    {
        HRESULT hres = pDXGIFactory->CreateSwapChainForComposition(pDev,
            &swapChainDesc,
            NULL,
            &pSwapChain);

        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    // Creating Video Processor
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
    MSDK_ZERO_MEMORY(ContentDesc);

    ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    ContentDesc.InputFrameRate.Numerator = 30000;
    ContentDesc.InputFrameRate.Denominator = 1000;
    ContentDesc.InputWidth = width;
    ContentDesc.InputHeight = height;
    ContentDesc.OutputWidth = width;
    ContentDesc.OutputHeight = height;
    ContentDesc.OutputFrameRate.Numerator = 30000;
    ContentDesc.OutputFrameRate.Denominator = 1000;

    ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    //Get video device interfaces
    pVideoDevice = pDev;
    MSDK_CHECK_POINTER(pVideoDevice.p, MFX_ERR_NULL_PTR);

    ID3D11DeviceContext* ctx;
    pDev->GetImmediateContext(&ctx);
    pVideoContext = ctx;
    ctx->Release();
    MSDK_CHECK_POINTER(pVideoContext.p, MFX_ERR_NULL_PTR);

    // Turn on multithreading for the Context
    CComQIPtr<ID3D10Multithread> p_mt(pVideoContext);

    if (p_mt)
    {
        p_mt->SetMultithreadProtected(true);
    }
    else
    {
        return MFX_ERR_DEVICE_FAILED;
    }


    if (FAILED(pVideoDevice->CreateVideoProcessorEnumerator(&ContentDesc, &pVideoProcessorEnum)))
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    if (FAILED(pVideoDevice->CreateVideoProcessor(pVideoProcessorEnum, 0, &pVideoProcessor)))
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    Start();

    return MFX_ERR_NONE;
}

mfxStatus CUWPRenderer::RenderFrame(CMfxFrameSurfaceExt * pSrf)
{
    HRESULT hres = S_OK;
    mfxStatus sts;

    CComPtr<ID3D11Texture2D>     pDXGIBackBuffer;
    CComPtr<ID3D11VideoProcessorOutputView> pOutputView;
    CComPtr<ID3D11VideoProcessorInputView>  pInputView;


    //--- Creating Input View (basing on provided surface)
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputViewDesc;
    InputViewDesc.FourCC = 0;
    InputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    InputViewDesc.Texture2D.MipSlice = 0;
    InputViewDesc.Texture2D.ArraySlice = 0;

    mfxHDLPair pair = { NULL };
    sts = pSrf->pAlloc->GetHDL(pSrf->pAlloc->pthis, pSrf->Data.MemId, (mfxHDL*)&pair);
    MSDK_CHECK_STATUS(sts, "pAlloc->GetHDL failed");

    ID3D11Texture2D  *pRTTexture2D = reinterpret_cast<ID3D11Texture2D*>(pair.first);

    hres = pVideoDevice->CreateVideoProcessorInputView(
        pRTTexture2D,
        pVideoProcessorEnum,
        &InputViewDesc,
        &pInputView.p);

    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    //--- Creating output view (basing on swapchain backbuffer)
    hres = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pDXGIBackBuffer.p);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
    OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    OutputViewDesc.Texture2D.MipSlice = 0;

    hres = pVideoDevice->CreateVideoProcessorOutputView(
        pDXGIBackBuffer,
        pVideoProcessorEnum,
        &OutputViewDesc,
        &pOutputView.p);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    //--- Creating StreamData for copying and resizing frame (NV12 surface to RGB backbuffer)
    RECT rect = { 0 };
    rect.right = pSrf->Info.CropW;
    rect.bottom = pSrf->Info.CropH;

    D3D11_VIDEO_PROCESSOR_STREAM StreamData;

    StreamData.Enable = TRUE;
    StreamData.OutputIndex = 0;
    StreamData.InputFrameOrField = 0;
    StreamData.PastFrames = 0;
    StreamData.FutureFrames = 0;
    StreamData.ppPastSurfaces = NULL;
    StreamData.ppFutureSurfaces = NULL;
    StreamData.pInputSurface = pInputView;
    StreamData.ppPastSurfacesRight = NULL;
    StreamData.ppFutureSurfacesRight = NULL;
    StreamData.pInputSurfaceRight = NULL;

    pVideoContext->VideoProcessorSetStreamSourceRect(pVideoProcessor, 0, true, &rect);
    pVideoContext->VideoProcessorSetStreamFrameFormat(pVideoProcessor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    hres = pVideoContext->VideoProcessorBlt(pVideoProcessor, pOutputView, 0, 1, &StreamData);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    DXGI_PRESENT_PARAMETERS parameters = { 0 };
    hres = pSwapChain->Present1(0, 0, &parameters);
    if (FAILED(hres))
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

mfxStatus CUWPRenderer::Resize(UINT width, UINT height)
{
    if (pSwapChain)
    {
        HRESULT hr = pSwapChain->ResizeBuffers(2,width,height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
        return FAILED(hr) ? MFX_ERR_DEVICE_FAILED : MFX_ERR_NONE;
    }
    return MFX_ERR_NONE;
}

void CUWPRenderer::EnqueueSurface(CMfxFrameSurfaceExt* surface)
{
    {
        std::unique_lock <std::mutex> lock(cs);
        outputSurfaces.push_back(surface);
    }
}

bool CUWPRenderer::RunOnce()
{
    if (!isPlaying)
    {
        return true;
    }
    CMfxFrameSurfaceExt* pSurf = NULL;
    msdk_tick now;
    while ((now = CMSDKTime::GetTick()) - prevFrameTimestamp < timeoutTicks); // wait for exact time for the frame
    prevFrameTimestamp = now;
    pSurf = PopSurface();
    if (pSurf)
    {
        RenderFrame(pSurf);
        pSurf->UserLock = false;
    }

    return true;
}

CMfxFrameSurfaceExt * CUWPRenderer::PopSurface()
{
    std::unique_lock <std::mutex> lock(cs);
    CMfxFrameSurfaceExt* retVal = NULL;
    if (outputSurfaces.size())
    {
        retVal = outputSurfaces.front();
        outputSurfaces.pop_front();
    }
    return retVal;
}

void CUWPRenderer::SetPlay(bool isPlaying)
{
    this->isPlaying = isPlaying;
}

void CUWPRenderer::ClearQueue()
{
    std::unique_lock <std::mutex> lock(cs);
    CMfxFrameSurfaceExt* retVal = NULL;
    for(auto surf : outputSurfaces)
    {
        surf->UserLock = false;
    }

    outputSurfaces.clear();
}
