/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"

#if MFX_D3D11_SUPPORT

#include "dxgi.h"
#include "mfx_d3d11_device.h"

MFXD3D11Device::MFXD3D11Device()
{

}

MFXD3D11Device::~MFXD3D11Device()
{

}

mfxStatus MFXD3D11Device::Init( mfxU32 nAdapter
    , WindowHandle  hDeviceWindow
    , bool          bIsWindowed
    , mfxU32        renderTargetFmt
    , int           /*backBufferCount*/
    , const vm_char     * /*pDvxva2LibName*/
    , bool          isD3D9FeatureLevels)
{
    HRESULT hres = S_OK;
    static D3D_FEATURE_LEVEL FeatureLevels11[] = { 
        D3D_FEATURE_LEVEL_11_1, 
        D3D_FEATURE_LEVEL_11_0, 
        D3D_FEATURE_LEVEL_10_1, 
        D3D_FEATURE_LEVEL_10_0,
    };
    static D3D_FEATURE_LEVEL FeatureLevels9[] = { 
        D3D_FEATURE_LEVEL_9_1,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_3
    };
    D3D_FEATURE_LEVEL *pFeatureLevelsIn = isD3D9FeatureLevels?FeatureLevels9:FeatureLevels11;

    D3D_FEATURE_LEVEL pFeatureLevelsOut;
    UINT array_size = isD3D9FeatureLevels?MFX_ARRAY_SIZE(FeatureLevels9):MFX_ARRAY_SIZE(FeatureLevels11);

    CComPtr<IDXGIAdapter1> dxgiAdapter;
    MFX_CHECK(S_OK == CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**) (&m_pDXGIFactory)));
    MFX_CHECK(S_OK == m_pDXGIFactory->EnumAdapters1(nAdapter, &dxgiAdapter));

    hres =  D3D11CreateDeviceWrapper(dxgiAdapter,    // provide real adapter
                              D3D_DRIVER_TYPE_UNKNOWN, // !!!!!! See MSDN, how to create device using not default device
                              NULL,
                              0,
                              pFeatureLevelsIn,
                              array_size,
                              D3D11_SDK_VERSION,
                              &m_pD3D11Device,
                              &pFeatureLevelsOut,
                              &m_pD3D11Ctx);

    MFX_CHECK_WITH_ERR(!FAILED(hres), MFX_ERR_DEVICE_FAILED);
    m_pDX11VideoDevice = m_pD3D11Device;
    m_pVideoContext = m_pD3D11Ctx;

    MFX_CHECK_POINTER(m_pDX11VideoDevice.p);
    MFX_CHECK_POINTER(m_pVideoContext.p);

    CComQIPtr<ID3D10Multithread> p_mt(m_pVideoContext);

    MFX_CHECK(p_mt, MFX_ERR_DEVICE_FAILED);
    p_mt->SetMultithreadProtected(true);

    m_format = (DXGI_FORMAT)renderTargetFmt;

    if (NULL != hDeviceWindow)
    {
        RECT dummy = {0};
        MFX_CHECK_STS(Reset(hDeviceWindow, dummy, bIsWindowed));
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXD3D11Device::CreateVideoProcessor(mfxFrameSurface1 * pSrf)
{
    if (m_VideoProcessorEnum.p || NULL == pSrf)
        return MFX_ERR_NONE;

    //create video processor
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
    MFX_ZERO_MEM( ContentDesc );

    ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    ContentDesc.InputFrameRate.Numerator = 60000;
    ContentDesc.InputFrameRate.Denominator = 1000;
    ContentDesc.InputWidth  = pSrf->Info.CropW;
    ContentDesc.InputHeight = pSrf->Info.CropH;
    ContentDesc.OutputWidth = pSrf->Info.CropW;
    ContentDesc.OutputHeight = pSrf->Info.CropH;
    ContentDesc.OutputFrameRate.Numerator = 60000;
    ContentDesc.OutputFrameRate.Denominator = 1000;

    ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    MFX_CHECK_WITH_ERR(!FAILED(m_pDX11VideoDevice->CreateVideoProcessorEnumerator( &ContentDesc, &m_VideoProcessorEnum )), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ERR(!FAILED(m_pDX11VideoDevice->CreateVideoProcessor( m_VideoProcessorEnum, 0, &m_pVideoProcessor )), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus MFXD3D11Device::Reset(WindowHandle hDeviceWindow, RECT, bool bIsWindowed)
{
    MFX_CHECK_WITH_ERR(!FAILED(m_pDXGIFactory->MakeWindowAssociation( (HWND)hDeviceWindow, 0 )), MFX_ERR_DEVICE_FAILED);

    // Get the DXGISwapChain
    DXGI_SWAP_CHAIN_DESC scd;
    MFX_ZERO_MEM(scd);

    scd.Windowed = bIsWindowed ? TRUE : FALSE;
    scd.OutputWindow = (HWND)hDeviceWindow;
    scd.SampleDesc.Count = 1;
    scd.BufferDesc.Format = m_format;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 4;
    scd.SwapEffect = DXGI_FORMAT_R10G10B10A2_UNORM == m_format ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    MFX_CHECK_WITH_ERR(!FAILED(m_pDXGIFactory->CreateSwapChain(m_pD3D11Device, &scd, &m_pSwapChain)), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus MFXD3D11Device::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    MFX_CHECK(type == MFX_HANDLE_D3D11_DEVICE, MFX_ERR_UNSUPPORTED);
    *pHdl  = (mfxHDL )m_pD3D11Device.p;
    return MFX_ERR_NONE;
}

mfxStatus MFXD3D11Device::RenderFrame(mfxFrameSurface1 * pSrf, mfxFrameAllocator *pAlloc)
{
    MFX_CHECK_STS(CreateVideoProcessor(pSrf));

    // Get Backbuffer
    ID3D11VideoProcessorInputView* pInputView = NULL;
    ID3D11VideoProcessorOutputView* pOutputView = NULL;
    ID3D11Texture2D* pDXGIBackBuffer = NULL;

    MFX_CHECK_WITH_ERR(!FAILED(m_pSwapChain->GetBuffer(0, __uuidof( ID3D11Texture2D ), (void**)&pDXGIBackBuffer)), MFX_ERR_DEVICE_FAILED);

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
    OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    OutputViewDesc.Texture2D.MipSlice = 0;
    MFX_CHECK_WITH_ERR(!FAILED(m_pDX11VideoDevice->CreateVideoProcessorOutputView(
        pDXGIBackBuffer,
        m_VideoProcessorEnum,
        &OutputViewDesc,
        &pOutputView )), MFX_ERR_DEVICE_FAILED);

    mfxHDLPair pair = {NULL};
    MFX_CHECK_STS(pAlloc->GetHDL(pAlloc->pthis, pSrf->Data.MemId, (mfxHDL*)&pair));

    ID3D11Texture2D  *pRTTexture2D = reinterpret_cast<ID3D11Texture2D*>(pair.first);

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputViewDesc;
    InputViewDesc.FourCC = 0;
    InputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    InputViewDesc.Texture2D.MipSlice = (UINT)pair.second;
    InputViewDesc.Texture2D.ArraySlice = (UINT)pair.second;//dwFlipToIndex;

    MFX_CHECK_WITH_ERR(!FAILED(m_pDX11VideoDevice->CreateVideoProcessorInputView( 
        pRTTexture2D,
        m_VideoProcessorEnum,
        &InputViewDesc,
        &pInputView )), MFX_ERR_DEVICE_FAILED);

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

    //  NV12 surface to RGB backbuffer ...
    RECT rect = {0};            
    
    rect.right  = pSrf->Info.CropW;
    rect.bottom = pSrf->Info.CropH;
    m_pVideoContext->VideoProcessorSetStreamSourceRect(m_pVideoProcessor, 0, true, &rect);
    m_pVideoContext->VideoProcessorSetStreamFrameFormat( m_pVideoProcessor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    m_pVideoContext->VideoProcessorBlt( m_pVideoProcessor, pOutputView, 0, 1, &StreamData );                         

    MFX_CHECK_WITH_ERR(!FAILED(m_pSwapChain->Present( 0, 0 )), MFX_ERR_DEVICE_FAILED);

    pDXGIBackBuffer->Release();
    pInputView->Release();
    pOutputView->Release();
    
    return MFX_ERR_NONE;
}

void      MFXD3D11Device::Close()
{
}

#endif