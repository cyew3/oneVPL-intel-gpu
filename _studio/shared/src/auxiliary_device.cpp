//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_VA_WIN)

#include "auxiliary_device.h"
#include "mfx_utils.h"
#include <vector>

#define D3DFMT_NV12 (D3DFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'))

static const DXVA2_AYUVSample16 Background_Default =
{   // Black BG
    0x8000,         // Cr
    0x8000,         // Cb
    0x0000,         // Y
    0xffff          // Alpha
};

static const DXVA2_ExtendedFormat DXVA2_ExtendedFormat_Default = 
{
    DXVA2_SampleProgressiveFrame,           // SampleFormat
    DXVA2_VideoChromaSubsampling_MPEG2,     // VideoChromaSubsampling
    DXVA2_NominalRange_Normal,              // NominalRange
    DXVA2_VideoTransferMatrix_BT709,        // VideoTransferMatrix
    DXVA2_VideoLighting_dim,                // VideoLighting
    DXVA2_VideoPrimaries_BT709,             // VideoPrimaries
    DXVA2_VideoTransFunc_709                // VideoTransferFunction
};

AuxiliaryDevice::AuxiliaryDevice()
{
    // zeroise objects
    memset(&D3DAuxObjects, 0, sizeof(D3DAuxObjects));
    m_Guid = GUID_NULL;

} // AuxiliaryDevice::AuxiliaryDevice(VideoCORE *pCore, mfxStatus sts)

AuxiliaryDevice::~AuxiliaryDevice(void)
{
    // release object
    Release();

} // AuxiliaryDevice::~AuxiliaryDevice(void)

mfxStatus AuxiliaryDevice::Initialize(IDirect3DDeviceManager9       *pD3DDeviceManager,
                                      IDirectXVideoDecoderService   *pVideoDecoderService)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    HRESULT hr;

    if (pD3DDeviceManager)
    {
        D3DAuxObjects.pD3DDeviceManager = pD3DDeviceManager;
    }
    if (pVideoDecoderService)
    {
        SAFE_RELEASE(D3DAuxObjects.m_pDXVideoDecoderService);
        D3DAuxObjects.m_pDXVideoDecoderService = pVideoDecoderService;
        pVideoDecoderService->AddRef();
    }

    if (!D3DAuxObjects.m_pDXVideoDecoderService)
    {
        MFX_CHECK(D3DAuxObjects.pD3DDeviceManager, MFX_ERR_NOT_INITIALIZED);
        
        hr = D3DAuxObjects.pD3DDeviceManager->OpenDeviceHandle(&D3DAuxObjects.m_hDXVideoDecoderService);

        hr = D3DAuxObjects.pD3DDeviceManager->GetVideoService(D3DAuxObjects.m_hDXVideoDecoderService, 
                                                              IID_IDirectXVideoDecoderService, 
                                                              (void**)&D3DAuxObjects.m_pDXVideoDecoderService);

        D3DAuxObjects.pD3DDeviceManager->CloseDeviceHandle(D3DAuxObjects.m_hDXVideoDecoderService);
        D3DAuxObjects.m_hDXVideoDecoderService = INVALID_HANDLE_VALUE;
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    MFX_CHECK(D3DAuxObjects.m_pDXVideoDecoderService, MFX_ERR_NOT_INITIALIZED);

    UINT    cDecoderGuids = 0;
    GUID    *pDecoderGuids = NULL;

    // retrieves an array of GUIDs that identifies the decoder devices supported by the graphics hardware 
    hr = D3DAuxObjects.m_pDXVideoDecoderService->GetDecoderDeviceGuids(&cDecoderGuids, &pDecoderGuids);

    MFX_CHECK(cDecoderGuids, MFX_ERR_DEVICE_FAILED);

    // check that intel auxiliary device supported
    while(cDecoderGuids)
    {
        cDecoderGuids--;

        if (DXVA2_Intel_Auxiliary_Device == pDecoderGuids[cDecoderGuids])
        {
            // auxiliary device was found
            break;
        }

        MFX_CHECK(cDecoderGuids, MFX_ERR_UNSUPPORTED);
    }

    if (pDecoderGuids)
    {
        CoTaskMemFree(pDecoderGuids);
    }

    UINT cFormats = 0;
    D3DFORMAT *pFormats = NULL;
    D3DFORMAT format = (D3DFORMAT)MAKEFOURCC('I', 'M', 'C', '3');

    // retrieves the supported render targets for a specified decoder device
    // this function call is necessary only for compliant with MS DXVA2 decoder interface
    // the actual render is obtained through another call
    hr = D3DAuxObjects.m_pDXVideoDecoderService->GetDecoderRenderTargets(DXVA2_Intel_Auxiliary_Device, 
                                                                         &cFormats, &pFormats);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    // delete temporal memory
    if (pFormats)
    {
        format = pFormats[0];
        CoTaskMemFree(pFormats);
    }

    DXVA2_VideoDesc             DXVA2VideoDesc = {0};
    DXVA2_ConfigPictureDecode   *pConfig = NULL;
    UINT                        cConfigurations = 0;
    
    // set surface size as small as possible
    DXVA2VideoDesc.SampleWidth  = 64;
    DXVA2VideoDesc.SampleHeight = 64;

    DXVA2VideoDesc.Format                              = D3DFMT_NV12;

    // creates DXVA decoder render target
    // the surfaces are required by DXVA2 in order to create decoder device
    // and are not used for any others purpose by auxiliary device

    if (D3DAuxObjects.m_pD3DSurface)
    {
        D3DAuxObjects.m_pD3DSurface->Release();
        D3DAuxObjects.m_pD3DSurface = NULL;
    }

    // try to create surface of IMC3 color format
    hr = D3DAuxObjects.m_pDXVideoDecoderService->CreateSurface(DXVA2VideoDesc.SampleWidth, 
                                                               DXVA2VideoDesc.SampleHeight, 
                                                               0, 
                                                               (D3DFORMAT)MAKEFOURCC('I', 'M', 'C', '3'), 
                                                               D3DPOOL_DEFAULT, 
                                                               0, 
                                                               DXVA2_VideoDecoderRenderTarget, 
                                                               &D3DAuxObjects.m_pD3DSurface, 
                                                               NULL);
    if (FAILED(hr))
    {
        // try color format reported by GetDecoderRenderTargets()
        hr = D3DAuxObjects.m_pDXVideoDecoderService->CreateSurface(DXVA2VideoDesc.SampleWidth, 
                                                                   DXVA2VideoDesc.SampleHeight, 
                                                                   0, 
                                                                   format, 
                                                                   D3DPOOL_DEFAULT, 
                                                                   0, 
                                                                   DXVA2_VideoDecoderRenderTarget, 
                                                                   &D3DAuxObjects.m_pD3DSurface, 
                                                                   NULL);
    }

    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    // retrieves the configurations that are available for a decoder device
    // this function call is necessary only for compliant with MS DXVA2 decoder interface
    hr = D3DAuxObjects.m_pDXVideoDecoderService->GetDecoderConfigurations(DXVA2_Intel_Auxiliary_Device,
                                                                          &DXVA2VideoDesc, 
                                                                          NULL, 
                                                                          &cConfigurations, 
                                                                          &pConfig);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(pConfig, MFX_ERR_DEVICE_FAILED);

    DXVA2_ConfigPictureDecode m_Config = pConfig[0];

    // creates a video decoder device
    hr = D3DAuxObjects.m_pDXVideoDecoderService->CreateVideoDecoder(DXVA2_Intel_Auxiliary_Device, 
                                                                    &DXVA2VideoDesc, 
                                                                    &m_Config, 
                                                                    &D3DAuxObjects.m_pD3DSurface, 
                                                                    1, 
                                                                    &D3DAuxObjects.m_pDXVideoDecoder);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    if (pConfig)
    {
        CoTaskMemFree(pConfig);
    }


    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::Initialize(void)

mfxStatus AuxiliaryDevice::Release(void)
{
    ReleaseAccelerationService();

    SAFE_RELEASE(D3DAuxObjects.m_pD3DSurface);
    SAFE_RELEASE(D3DAuxObjects.m_pDummySurface);
    SAFE_RELEASE(D3DAuxObjects.m_pDXVideoDecoder);
    SAFE_RELEASE(D3DAuxObjects.m_pDXVideoDecoderService);
    SAFE_RELEASE(D3DAuxObjects.m_pDXVideoProcessorService);
    SAFE_RELEASE(D3DAuxObjects.m_pDXRegistrationDevice);


    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::Release(void)


mfxStatus AuxiliaryDevice::IsAccelerationServiceExist(const GUID guid)
{
    HRESULT hr;
    mfxU32 guidsCount = 0;

    // obtain number of supported guid
    hr = Execute(AUXDEV_GET_ACCEL_GUID_COUNT, 0, 0, &guidsCount, sizeof(mfxU32));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(guidsCount, MFX_ERR_DEVICE_FAILED);

    std::vector<GUID> guids(guidsCount);
    hr = Execute(AUXDEV_GET_ACCEL_GUIDS, 0, 0, &guids[0], guidsCount * sizeof(GUID));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_UNKNOWN);

    mfxU32 i;
    for (i = 0; i < guidsCount; i++)
    {
        if (IsEqualGUID(guids[i], guid))
        {
            break;
        }
    }
    MFX_CHECK(i < guidsCount, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::IsAccelerationServiceExist(const GUID guid)

mfxStatus AuxiliaryDevice::CreateAccelerationService(const GUID guid, void *pCreateParams, UINT uCreateParamSize)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    mfxStatus sts;
    HRESULT hr;

    // check this guid are supported by driver
    MFX_SAFE_CALL(IsAccelerationServiceExist(guid));

    // release service before allocation
    ReleaseAccelerationService();

    // create acceleration service
    hr = Execute(AUXDEV_CREATE_ACCEL_SERVICE, (void *)&guid, sizeof(GUID), (void*)pCreateParams, uCreateParamSize);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    // obtain registration handle
    FASTCOMP_QUERYCAPS sQuery;
    UINT uQuerySize;

    sQuery.Type = FASTCOMP_QUERY_REGISTRATION_HANDLE;
    uQuerySize  = sizeof(sQuery);

    sts = QueryAccelCaps(&DXVA2_FastCompositing, &sQuery, &uQuerySize);
    D3DAuxObjects.m_hRegistration = (MFX_ERR_NONE == sts) ? sQuery.hRegistration : NULL;

    // create surface registration device
    sts = CreateSurfaceRegistrationDevice();
    MFX_CHECK_STS(sts);


    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::CreateAccelerationService(const GUID guid)

mfxStatus AuxiliaryDevice::ReleaseAccelerationService()
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    HRESULT hr;

    if (D3DAuxObjects.m_pDXVideoDecoder && m_Guid != GUID_NULL)
    {
        m_Guid = GUID_NULL;
        hr = Execute(AUXDEV_DESTROY_ACCEL_SERVICE, (void*)&m_Guid, sizeof(GUID));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::ReleaseAccelerationService()

mfxStatus AuxiliaryDevice::CreateSurfaceRegistrationDevice(void)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    HRESULT hr;

    // gets a handle to the Direct3D device
    hr = D3DAuxObjects.pD3DDeviceManager->OpenDeviceHandle(&D3DAuxObjects.m_hRegistrationDevice);
    
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_NOT_INITIALIZED);
    
    // create DXVA services object
    hr = D3DAuxObjects.pD3DDeviceManager->GetVideoService(D3DAuxObjects.m_hRegistrationDevice, 
                                            IID_IDirectXVideoProcessorService,
                                            (void**)&D3DAuxObjects.m_pDXVideoProcessorService);
    
    D3DAuxObjects.pD3DDeviceManager->CloseDeviceHandle(D3DAuxObjects.m_hRegistrationDevice);
    D3DAuxObjects.m_hRegistrationDevice = INVALID_HANDLE_VALUE;
    
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_NOT_INITIALIZED);

    // -----------------------------------------------------------------
    // skip checking of GUID by GetVideoProccesorDeviceGuids, because of
    // surface registration device GUID is not reported by driver
    // -----------------------------------------------------------------

    // initialize the video descriptor
    
    DXVA2_VideoDesc DXVA2VideoDesc;

    // size can be as small as possible
    DXVA2VideoDesc.SampleWidth                         = 16;
    DXVA2VideoDesc.SampleHeight                        = 16;
    DXVA2VideoDesc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    DXVA2VideoDesc.SampleFormat.NominalRange           = DXVA2_NominalRange_16_235;
    DXVA2VideoDesc.SampleFormat.VideoTransferMatrix    = DXVA2_VideoTransferMatrix_BT709;
    DXVA2VideoDesc.SampleFormat.VideoLighting          = DXVA2_VideoLighting_dim;
    DXVA2VideoDesc.SampleFormat.VideoPrimaries         = DXVA2_VideoPrimaries_BT709;
    DXVA2VideoDesc.SampleFormat.VideoTransferFunction  = DXVA2_VideoTransFunc_709;
    DXVA2VideoDesc.SampleFormat.SampleFormat           = DXVA2_SampleProgressiveFrame;
    DXVA2VideoDesc.Format                              = D3DFMT_YUY2;
    DXVA2VideoDesc.InputSampleFreq.Numerator           = 60;
    DXVA2VideoDesc.InputSampleFreq.Denominator         = 1;
    DXVA2VideoDesc.OutputFrameFreq.Numerator           = 60;
    DXVA2VideoDesc.OutputFrameFreq.Denominator         = 1;

    // create the video processor surface registration device
    // the target and sample formats must be set to D3DFMT_YUY2 
    // regardless of the format used with pre-processing device
    hr = D3DAuxObjects.m_pDXVideoProcessorService->CreateVideoProcessor(DXVA2_Registration_Device,
                                                                        &DXVA2VideoDesc,
                                                                        D3DFMT_YUY2,
                                                                        1,
                                                                        &D3DAuxObjects.m_pDXRegistrationDevice);

    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_NOT_INITIALIZED);
    
    // create surface
    if (D3DAuxObjects.m_pDummySurface)
    {
        D3DAuxObjects.m_pDummySurface->Release();
        D3DAuxObjects.m_pDummySurface = NULL;
    }

    hr = D3DAuxObjects.m_pDXVideoProcessorService->CreateSurface(64, 
                                                                 64, 
                                                                 0, 
                                                                 (D3DFORMAT)MAKEFOURCC('Y','U','Y','2'), 
                                                                 D3DPOOL_DEFAULT, 
                                                                 0, 
                                                                 DXVA2_VideoProcessorRenderTarget, 
                                                                 &D3DAuxObjects.m_pDummySurface, 
                                                                 0);

    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_NOT_INITIALIZED);

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::CreateSurfaceRegistrationDevice(void)

mfxStatus AuxiliaryDevice::Register(IDirect3DSurface9 **pSources, mfxU32 iCount, BOOL bRegister)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    HRESULT hRes;

    DXVA2_SURFACE_REGISTRATION RegRequest;
    DXVA2_SAMPLE_REG           RegEntry[33];

    DXVA2_VideoProcessBltParams BltParams;
    DXVA2_VideoSample           Samples[32];

    // create a surface registration request 
    // this may be performed only once, when the surface is created, 
    // before the first call to FastCompositingBlt
    RegRequest.RegHandle            = D3DAuxObjects.m_hRegistration;
    RegRequest.pRenderTargetRequest = &RegEntry[0];
    RegRequest.nSamples             = iCount;
    RegRequest.pSamplesRequest      = &RegEntry[1];

    RegEntry[0].pD3DSurface = NULL; // D3DAuxObjects.m_pDummySurface;
    RegEntry[0].Operation   = REG_IGNORE;
    
    for (mfxU32 i = 0; i < iCount; i++)
    {
        RegEntry[i + 1].pD3DSurface = pSources[i];
        RegEntry[i + 1].Operation   = (bRegister) ? REG_REGISTER : REG_UNREGISTER;
    }

    // prepare VideoProcessBlt paramaters to the registration device
    memset(&BltParams, 0, sizeof(BltParams));
    memset(Samples   , 0, sizeof(Samples));
    
    BltParams.TargetFrame = (REFERENCE_TIME) (&RegRequest);
    BltParams.TargetRect.left   = 0;
    BltParams.TargetRect.top    = 0;
    BltParams.TargetRect.right  = 4;
    BltParams.TargetRect.bottom = 4;
    BltParams.BackgroundColor.Alpha = 0xffff;
    
    for (mfxU32 i = 0; i < iCount; i++)
    {
        Samples[i].Start        = (REFERENCE_TIME) (&RegRequest);
        Samples[i].End          = Samples[i].Start + 1;
        Samples[i].SrcSurface   = pSources[i];
        Samples[i].SrcRect      = BltParams.TargetRect;
        Samples[i].DstRect      = BltParams.TargetRect;
    }

    // register surfaces for the FastCompositing device
    hRes = D3DAuxObjects.m_pDXRegistrationDevice->VideoProcessBlt(D3DAuxObjects.m_pDummySurface,
                                                                  &BltParams,
                                                                  Samples,
                                                                  iCount,
                                                                  NULL);
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::Register(IDirect3DSurface9 **pSources, mfxU32 iCount, BOOL bRegister)

mfxStatus AuxiliaryDevice::FastCopyBlt(IDirect3DSurface9 *source, IDirect3DSurface9 *destination)
{
    HRESULT hr;
    
    // check input parameters
    MFX_CHECK_NULL_PTR2(source, destination);

    MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_PRIVATE, NULL);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "^Input^IDirect3DSurface9", "%p", source);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "^Output^IDirect3DSurface9", "%p", destination);

    FASTCOPY_BLT_PARAMS fcBltParams;

    fcBltParams.pSource = source;
    fcBltParams.pDest   = destination;

    // fast copy function
    hr = Execute(0x100, (void*)&fcBltParams, sizeof(FASTCOPY_BLT_PARAMS));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::FastCopyBlt(IDirect3DSurface9 *source, IDirect3DSurface9 *destination)

mfxStatus AuxiliaryDevice::CreateSurface(IDirect3DSurface9 **surface, mfxU32 width, mfxU32 height, 
                                         D3DFORMAT format, D3DPOOL pool)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    HRESULT hRes;

    MFX_CHECK(D3DAuxObjects.m_pDXRegistrationDevice, MFX_ERR_NOT_INITIALIZED);

    // create system memory surface
    hRes = D3DAuxObjects.m_pDXVideoProcessorService->CreateSurface(width,
                                                                   height,
                                                                   0,
                                                                   format,
                                                                   pool,
                                                                   0,
                                                                   DXVA2_VideoSoftwareRenderTarget,
                                                                   surface,
                                                                   NULL);
    // check errors
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
    
    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::CreateSystemMemorySurface(IDirect3DSurface9 **surface, mfxU32 width, mfxU32 height, D3DFORMAT format)

HRESULT AuxiliaryDevice::BeginFrame(IDirect3DSurface9 *pRenderTarget, void *pvPVPData)
{
    MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_PRIVATE, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_INTERNAL, "^Output^IDirect3DSurface9", "%p", pRenderTarget);
    MFX_CHECK(D3DAuxObjects.m_pDXVideoDecoder, E_FAIL);

    return D3DAuxObjects.m_pDXVideoDecoder->BeginFrame(pRenderTarget, pvPVPData);

} // HRESULT AuxiliaryDevice::BeginFrame(HANDLE *pHandleComplete)

HRESULT AuxiliaryDevice::EndFrame(HANDLE *pHandleComplete)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    MFX_CHECK(D3DAuxObjects.m_pDXVideoDecoder, E_FAIL);

    return D3DAuxObjects.m_pDXVideoDecoder->EndFrame(pHandleComplete);

} // HRESULT AuxiliaryDevice::EndFrame(HANDLE *pHandleComplete)

#ifdef MFX_TRACE_ENABLE
static const char* AuxFuncName[] =
{
    "", //                                      = 0,
    "DDI_GET_ACCEL_GUID_COUNT", //              = 1,
    "DDI_GET_ACCEL_GUIDS", //                   = 2,
    "DDI_GET_ACCEL_RT_FORMAT_COUNT", //         = 3,
    "DDI_GET_ACCEL_RT_FORMATS", //              = 4,
    "DDI_GET_ACCEL_FORMAT_COUNT", //            = 5,
    "DDI_GET_ACCEL_FORMATS", //                 = 6,
    "DDI_QUERY_ACCEL_CAPS", //                  = 7,
    "DDI_CREATE_ACCEL_SERVICE", //              = 8,
    "DDI_DESTROY_ACCEL_SERVICE", //             = 9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // [50]
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // [50]
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // [50]
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // [50]
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,             // [46]
    "DDI_ENC", //                           = 0x100,
    "DDI_PAK", //                           = 0x101,
    "DDI_ENCODE", //                        = 0x102,
    "DDI_VPP", //                           = 0x103,
    "DDI_FORMAT_COUNT", //                  = 0x104,
    "DDI_FORMATS", //                       = 0x105,
    "DDI_ENC_CTRL_CAPS", //                 = 0x106,
    "DDI_ENC_CTRL_GET", //                  = 0x107,
    "DDI_ENC_CTRL_SET", //                  = 0x108,
    "DDI_MBDATA_LAYOUT", //                 = 0x109,
    "DDI_10A",
    "DDI_10B",
    "DDI_10C",
    "DDI_10D",
    "DDI_10E",
    "DDI_10F",
    "DDI_110",
    "DDI_111",
    "DDI_112",
    "DDI_113",
    "DDI_114",
    "DDI_115",
    "DDI_116",
    "DDI_117",
    "DDI_118",
    "DDI_119",
    "DDI_11A",
    "DDI_11B",
    "DDI_11C",
    "DDI_11D",
    "DDI_11E",
    "DDI_11F",
    "DDI_INSERT_DATA", //                   = 0x120,
    "DDI_QUERY_ENCODE", //                  = 0x121
    "DDI_122",
    "DDI_123",
    "DDI_124",
    "DDI_125",
    "DDI_126",
    "DDI_127",
    "DDI_128",
    "DDI_129",
    "DDI_12A",
    "DDI_12B",
    "DDI_12C",
    "DDI_12D",
    "DDI_12E",
    "DDI_12F",
    //FASTCOMP_BLT            = 0x100,
    //QUERY_SCENE_DETECTION   = 0x200
    //QUERY_VPREP_STATUS      = 0x300
};
#endif

HRESULT AuxiliaryDevice::Execute(mfxU32 func,
                                 void*  input,
                                 mfxU32 inSize,
                                 void*  output,
                                 mfxU32 outSize)
{
#ifdef MFX_TRACE_ENABLE
    const char *FuncName = (func < sizeof(AuxFuncName)/sizeof(AuxFuncName[0])) ? AuxFuncName[func] : NULL;
    if (!FuncName) FuncName = "DDI_UNKNOWN";
    if (func == FASTCOMP_BLT && m_Guid == DXVA2_FastCompositing) FuncName = "DDI_FASTCOMP";
    if (func == FASTCOMP_QUERY_STATUS_FUNC_ID) FuncName = "DDI_QUERY_VPP";
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, FuncName);
#endif
    MFX_CHECK(D3DAuxObjects.m_pDXVideoDecoder, E_FAIL);

    if (AUXDEV_CREATE_ACCEL_SERVICE == func && NULL == input)
        return E_POINTER;

    DXVA2_DecodeExtensionData extensionData;
    extensionData.Function = func;
    extensionData.pPrivateInputData = input;
    extensionData.PrivateInputDataSize = inSize;
    extensionData.pPrivateOutputData = output;
    extensionData.PrivateOutputDataSize = outSize;

    DXVA2_DecodeExecuteParams executeParams;
    executeParams.NumCompBuffers = 0;
    executeParams.pCompressedBuffers = 0;
    executeParams.pExtensionData = &extensionData;

    HRESULT hr = D3DAuxObjects.m_pDXVideoDecoder->Execute(&executeParams);

    if (AUXDEV_CREATE_ACCEL_SERVICE == func && SUCCEEDED(hr))
        m_Guid = *(GUID*)input; // to call AUXDEV_DESTROY_ACCEL_SERVICE in destructor

    return hr;

} // HRESULT AuxiliaryDevice::Execute(mfxU32 func, void*  input, mfxU32 inSize, void*  output, mfxU32 outSize)

mfxStatus AuxiliaryDevice::QueryAccelFormats(CONST GUID *pAccelGuid, D3DFORMAT **ppFormats, UINT *puCount)
{
    DXVA2_DecodeExecuteParams sExecute;
    DXVA2_DecodeExtensionData sExtension;

    HRESULT    hRes     = E_INVALIDARG;
    UINT       uCount   = 0;
    D3DFORMAT *pFormats = NULL;


    // check parameters
    if (!pAccelGuid || !ppFormats || !puCount)
    {
        return MFX_ERR_NULL_PTR;
    }

    // check the decoder object
    MFX_CHECK(D3DAuxObjects.m_pDXVideoDecoder, MFX_ERR_DEVICE_FAILED);

    // get guid count
    sExtension.Function              = AUXDEV_GET_ACCEL_FORMAT_COUNT;
    sExtension.pPrivateInputData     = (PVOID)pAccelGuid;
    sExtension.PrivateInputDataSize  = sizeof(GUID);
    sExtension.pPrivateOutputData    = &uCount;
    sExtension.PrivateOutputDataSize = sizeof(int);

    sExecute.NumCompBuffers     = 0;
    sExecute.pCompressedBuffers = 0;
    sExecute.pExtensionData     = &sExtension;

    hRes = D3DAuxObjects.m_pDXVideoDecoder->Execute(&sExecute);
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    // allocate array of formats
    pFormats = (D3DFORMAT *) malloc(uCount * sizeof(D3DFORMAT));

    if (!pFormats)
    {
        free(pFormats);
        return MFX_ERR_MEMORY_ALLOC;
    }

    // get supported formats
    sExtension.Function              = AUXDEV_GET_ACCEL_FORMATS;
    sExtension.pPrivateInputData     = (PVOID)pAccelGuid;
    sExtension.PrivateInputDataSize  = sizeof(GUID);
    sExtension.pPrivateOutputData    = pFormats;
    sExtension.PrivateOutputDataSize = uCount * sizeof(D3DFORMAT);

    hRes = D3DAuxObjects.m_pDXVideoDecoder->Execute(&sExecute);
    
    // check errors
    if (FAILED(hRes))
    {
        // clean up
        
        if (pFormats)
        {
            free(pFormats);
            pFormats = NULL;
        }

        return MFX_ERR_DEVICE_FAILED;
    }

    *puCount   = uCount;
    *ppFormats = pFormats;

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::QueryAccelFormats(CONST GUID *pAccelGuid, D3DFORMAT **ppFormats, UINT *puCount)

mfxStatus AuxiliaryDevice::QueryAccelCaps(CONST GUID *pAccelGuid, void *pCaps, UINT *puCapSize)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    DXVA2_DecodeExecuteParams sExecute;
    DXVA2_DecodeExtensionData sExtension;

    HRESULT hRes;

    // check parameters (pCaps may be NULL => puCapSize will receive the size of the device caps)
    if (!pAccelGuid || !puCapSize)
    {
        return MFX_ERR_NULL_PTR;
    }

    // check the decoder object
    MFX_CHECK(D3DAuxObjects.m_pDXVideoDecoder, MFX_ERR_NULL_PTR);

    // query caps
    sExtension.Function              = AUXDEV_QUERY_ACCEL_CAPS;
    sExtension.pPrivateInputData     = (PVOID)pAccelGuid;
    sExtension.PrivateInputDataSize  = sizeof(GUID);
    sExtension.pPrivateOutputData    = pCaps;
    sExtension.PrivateOutputDataSize = *puCapSize;

    sExecute.NumCompBuffers     = 0;
    sExecute.pCompressedBuffers = 0;
    sExecute.pExtensionData     = &sExtension;

    hRes = D3DAuxObjects.m_pDXVideoDecoder->Execute(&sExecute);
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    *puCapSize = sExtension.PrivateOutputDataSize;

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::QueryAccelCaps(CONST GUID *pAccelGuid, void *pCaps, UINT *puCapSize)

mfxStatus AuxiliaryDevice::QueryAccelRTFormats(CONST GUID *pAccelGuid, D3DFORMAT **ppFormats, UINT *puCount)
{
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_PRIVATE);
    DXVA2_DecodeExecuteParams sExecute;
    DXVA2_DecodeExtensionData sExtension;

    HRESULT    hRes     = E_INVALIDARG;
    UINT       uCount   = 0;
    D3DFORMAT *pFormats = NULL;

    // check parameters
    if (!pAccelGuid || !ppFormats || !puCount)
    {
        return MFX_ERR_NULL_PTR;
    }

    // check the decoder object
    MFX_CHECK(D3DAuxObjects.m_pDXVideoDecoder, MFX_ERR_NULL_PTR);

    // get guid count
    sExtension.Function              = AUXDEV_GET_ACCEL_RT_FORMAT_COUNT;
    sExtension.pPrivateInputData     = (PVOID)pAccelGuid;
    sExtension.PrivateInputDataSize  = sizeof(GUID);
    sExtension.pPrivateOutputData    = &uCount;
    sExtension.PrivateOutputDataSize = sizeof(int);

    sExecute.NumCompBuffers     = 0;
    sExecute.pCompressedBuffers = 0;
    sExecute.pExtensionData     = &sExtension;

    hRes = D3DAuxObjects.m_pDXVideoDecoder->Execute(&sExecute);
    MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

    // allocate array of formats
    pFormats = (D3DFORMAT *) malloc(uCount * sizeof(D3DFORMAT));

    if (!pFormats)
    {
        free(pFormats);
        return MFX_ERR_MEMORY_ALLOC;
    }

    // get guids
    sExtension.Function              = AUXDEV_GET_ACCEL_RT_FORMATS;
    sExtension.pPrivateInputData     = (PVOID)pAccelGuid;
    sExtension.PrivateInputDataSize  = sizeof(GUID);
    sExtension.pPrivateOutputData    = pFormats;
    sExtension.PrivateOutputDataSize = uCount * sizeof(D3DFORMAT);

    hRes = D3DAuxObjects.m_pDXVideoDecoder->Execute(&sExecute);

    if (FAILED(hRes))
    {
        if (pFormats)
        {
            free(pFormats);
            pFormats = NULL;
        }

        return MFX_ERR_DEVICE_FAILED;
    }

    *puCount   = uCount;
    *ppFormats = pFormats;

    return MFX_ERR_NONE;

} // mfxStatus AuxiliaryDevice::QueryAccelRTFormats(CONST GUID *pAccelGuid, D3DFORMAT **ppFormats, UINT *puCount)

#endif // #if defined (MFX_VA)
