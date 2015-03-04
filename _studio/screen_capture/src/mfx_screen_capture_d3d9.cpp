/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d9.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_d3d9.h"
#include "mfx_utils.h"

namespace MfxCapture
{

D3D9_Capturer::D3D9_Capturer(mfxCoreInterface* _core)
    :m_pmfxCore(_core)
{
    Mode = HW_D3D9;
}

D3D9_Capturer::~D3D9_Capturer()
{
    Destroy();
}

mfxStatus D3D9_Capturer::CreateVideoAccelerator( mfxVideoParam const & par)
{
    HRESULT hres;

    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxHDL hdl;

    mfxU16 width = 0;
    mfxU16 height = 0;
    D3DFORMAT format = MfxFourccToD3dFormat(par.mfx.FrameInfo.FourCC);
    width  = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
    height = par.mfx.FrameInfo.CropH ? par.mfx.FrameInfo.CropH : par.mfx.FrameInfo.Height;

    mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&hdl);
    m_pDirect3DDeviceManager = (IDirect3DDeviceManager9*)hdl;

    hres = m_pDirect3DDeviceManager->OpenDeviceHandle(&m_hDirectXHandle);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    hres = m_pDirect3DDeviceManager->GetVideoService(m_hDirectXHandle,
                                                   __uuidof(IDirectXVideoDecoderService),
                                                   (void**)&m_pDirectXVideoService);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    mfxU32 guidCount = 0;
    GUID* pGuids = 0;

    hres = m_pDirectXVideoService->GetDecoderDeviceGuids(&guidCount, &pGuids);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK( !(guidCount == 0 || pGuids == 0), MFX_ERR_DEVICE_FAILED);

    bool isIntelAuxGuidPresent = false;

    for (mfxU32 i = 0; i < guidCount; i++)
    {
        if (DXVA2_Intel_Auxiliary_Device == pGuids[i])
            isIntelAuxGuidPresent = true;
        
    }
    CoTaskMemFree(pGuids);
    MFX_CHECK(isIntelAuxGuidPresent, MFX_ERR_UNSUPPORTED);

    DXVA2_VideoDesc video_desc = {0};
    DXVA2_ConfigPictureDecode config = {0}; //The parameters in pConfig is not currently used

    video_desc.SampleWidth = width;
    video_desc.SampleHeight = height;
    video_desc.Format = format;
    //The rest of the parameters is not currently used

    IDirect3DSurface9* dummy_surf;
    hres = m_pDirectXVideoService->CreateSurface(1920, 1080, 0, format, D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &dummy_surf, 0);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    mfxU32 NumSurfaces = 1; //Size of the ppDecoderRenderTargets array. It shall set to 1.

    hres = m_pDirectXVideoService->CreateVideoDecoder(DXVA2_Intel_Auxiliary_Device, &video_desc, &config, &dummy_surf, NumSurfaces, &m_pDecoder);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    // obtain number of supported guid
    DXVA2_DecodeExecuteParams dec_exec = {0};
    DXVA2_DecodeExtensionData dec_ext =  {0};
    dec_exec.pExtensionData = &dec_ext;


    mfxU32 count = 0;
    dec_ext.Function = AUXDEV_GET_ACCEL_GUID_COUNT;
    dec_ext.pPrivateOutputData =  &count;
    dec_ext.PrivateOutputDataSize = sizeof(count);
    hres = m_pDecoder->Execute(&dec_exec);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(count, MFX_ERR_DEVICE_FAILED);

    dec_ext.Function = AUXDEV_GET_ACCEL_GUIDS;
    pGuids =  new GUID[count];
    dec_ext.pPrivateOutputData = pGuids;
    dec_ext.PrivateOutputDataSize = sizeof(GUID)*count;
    hres = m_pDecoder->Execute(&dec_exec);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(count, MFX_ERR_DEVICE_FAILED);

    bool isRequestedGuidPresent = false;
    for (mfxU32 i = 0; i < count; i++)
    {
        if (DXVADDI_Intel_GetDesktopScreen == pGuids[i])
            isRequestedGuidPresent = true;
    }
    delete[] pGuids;
    MFX_CHECK(isRequestedGuidPresent, MFX_ERR_UNSUPPORTED);

    dec_ext.Function = AUXDEV_CREATE_ACCEL_SERVICE;
    GUID GetDesktopScreenGUID = DXVADDI_Intel_GetDesktopScreen;
    dec_ext.pPrivateInputData = &GetDesktopScreenGUID;
    dec_ext.PrivateInputDataSize = sizeof(GetDesktopScreenGUID);

    video_desc.SampleWidth = width;
    video_desc.SampleHeight = height;
    video_desc.Format = format;
    DXVA2_VideoDesc* pDesk = &video_desc;
    dec_ext.pPrivateOutputData = &pDesk;
    dec_ext.PrivateOutputDataSize = sizeof(video_desc);
    hres = m_pDecoder->Execute(&dec_exec);
    MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);

    dummy_surf->Release();

    return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out)
{
    //temporary creates decoder instance to check capabilities
    mfxStatus mfxRes = MFX_ERR_NONE;

    mfxRes = CreateVideoAccelerator(in);
    if(MFX_ERR_NONE != mfxRes)
    {
        Destroy();
        return mfxRes;
    }

    mfxRes = CheckCapabilities(in, out);
    if(MFX_ERR_NONE != mfxRes)
    {
        Destroy();
        return mfxRes;
    }

    Destroy();

    return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out)
{
    HRESULT hr;
    DXVA2_DecodeExecuteParams dec_exec = {0};
    DXVA2_DecodeExtensionData dec_ext =  {0};
    dec_exec.pExtensionData = &dec_ext;

    DESKTOP_FORMAT *desktop_format = 0;
    DESKTOP_PARAM_STRUCT_SIZE param_size = {0};
    mfxU32 count = 0;

    mfxU16 width = 0;
    mfxU16 height = 0;
    width  = in.mfx.FrameInfo.CropW ? in.mfx.FrameInfo.CropW : in.mfx.FrameInfo.Width;
    height = in.mfx.FrameInfo.CropH ? in.mfx.FrameInfo.CropH : in.mfx.FrameInfo.Height;

    //memset( &dec_ext, 0, sizeof(dec_ext));

    //Query DESKTOP_FORMAT_COUNT_ID
    dec_ext.Function = DESKTOP_FORMAT_COUNT_ID;
    dec_ext.pPrivateOutputData =  &count;
    dec_ext.PrivateOutputDataSize = sizeof(count);


    hr = m_pDecoder->Execute(&dec_exec);
    if (FAILED(hr))
        return MFX_ERR_DEVICE_FAILED;

    if (count == 0)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    //Query DESKTOP_FORMATS_ID
    param_size.SizeOfParamStruct = sizeof(DESKTOP_FORMAT);
    desktop_format = new DESKTOP_FORMAT[count];

    dec_ext.Function = DESKTOP_FORMATS_ID;
    dec_ext.pPrivateInputData =  &param_size;
    dec_ext.PrivateInputDataSize = sizeof(DESKTOP_PARAM_STRUCT_SIZE);

    dec_ext.pPrivateOutputData = desktop_format;
    dec_ext.PrivateOutputDataSize = sizeof(DESKTOP_FORMAT) * count; //Not sure about multiplying by count

    hr = m_pDecoder->Execute(&dec_exec);
    if (FAILED(hr))
    {
        delete []desktop_format;
        return MFX_ERR_DEVICE_FAILED;
    }

    for (mfxU32 i = 0; i < count; i++)
    {
        if((D3DFORMAT) desktop_format[i].DesktopFormat == D3DFMT_NV12 && MFX_FOURCC_NV12 == in.mfx.FrameInfo.FourCC)
        {
            if (height <= desktop_format[i].MaxHeight &&
                width <= desktop_format[i].MaxWidth)
            {
                delete []desktop_format;
                return MFX_ERR_NONE;
            }
            else
            {
                if(out)
                {
                    out->mfx.FrameInfo.Width  = 0;
                    out->mfx.FrameInfo.Height = 0;
                    out->mfx.FrameInfo.CropW  = 0;
                    out->mfx.FrameInfo.CropH  = 0;
                }
                delete []desktop_format;
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(((D3DFORMAT) desktop_format[i].DesktopFormat == D3DFMT_A8R8G8B8  && MFX_FOURCC_RGB4 == in.mfx.FrameInfo.FourCC))
        {
            if (height <= desktop_format[i].MaxHeight &&
                width <= desktop_format[i].MaxWidth)
            {
                delete []desktop_format;
                return MFX_ERR_NONE;
            }
            else
            {
                if(out)
                {
                    out->mfx.FrameInfo.Width  = 0;
                    out->mfx.FrameInfo.Height = 0;
                    out->mfx.FrameInfo.CropW  = 0;
                    out->mfx.FrameInfo.CropH  = 0;
                }
                delete []desktop_format;
                return MFX_ERR_UNSUPPORTED;
            }
        }
    }

    if(out)
    {
        out->mfx.FrameInfo.FourCC  = 0;
    }
    delete []desktop_format;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3D9_Capturer::Destroy()
{
    HRESULT hr;
    bool error = false;

    DXVA2_DecodeExecuteParams dec_exec = {0};
    DXVA2_DecodeExtensionData dec_ext =  {0};
    dec_exec.pExtensionData = &dec_ext;

    dec_ext.Function = AUXDEV_DESTROY_ACCEL_SERVICE;
    GUID GetDesktopScreenGUID = GUID_NULL;
    dec_ext.pPrivateInputData = &GetDesktopScreenGUID;
    dec_ext.PrivateInputDataSize = sizeof(GetDesktopScreenGUID);

    if(m_pDecoder)
    {
        hr = m_pDecoder->Execute(&dec_exec);
        if(FAILED(hr))
            error = true;
        //not sure about correct closing the device, let's ignore error
        error = false;
        m_pDecoder.Release();
    }

    if(m_pDirectXVideoService)
        m_pDirectXVideoService.Release();
    if(m_pDirect3DDevice)
        m_pDirect3DDevice.Release();
    if(m_pD3D)
        m_pD3D.Release();

    if (m_pDirect3DDeviceManager && m_hDirectXHandle != INVALID_HANDLE_VALUE)
    {
        m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
        m_hDirectXHandle = INVALID_HANDLE_VALUE;
    }

    //if(m_pDirect3DDeviceManager)
    //    m_pDirect3DDeviceManager.Release();

    if(error)
        return MFX_ERR_DEVICE_FAILED;
    else
        return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::BeginFrame( mfxMemId MemId)
{
    HRESULT hr = S_OK;
    mfxHDLPair Pair;

    mfxStatus mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, MemId, (mfxHDL*)&Pair);
    MFX_CHECK_STS(mfxRes);

    if( SUCCEEDED( hr ) )
    {
        hr = m_pDecoder->BeginFrame((IDirect3DSurface9*) Pair.first, 0);
        if SUCCEEDED( hr )
            return MFX_ERR_NONE;
    }

    return MFX_ERR_DEVICE_FAILED;
}


mfxStatus D3D9_Capturer::EndFrame( )
{
    HANDLE reserved = 0;
    HRESULT hr = m_pDecoder->EndFrame(&reserved);
    if FAILED(hr)
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber)
{
    if(!surface_work)
        return MFX_ERR_NULL_PTR;
    HRESULT hr;
    DXVA2_DecodeExecuteParams dec_exec = {0};
    DXVA2_DecodeExtensionData dec_ext =  {0};
    dec_exec.pExtensionData = &dec_ext;

    DESKTOP_EXECUTE_PARAMS desktop_execute;

    mfxU16 width = 0;
    mfxU16 height = 0;
    width  = surface_work->Info.CropW ? surface_work->Info.CropW : surface_work->Info.Width;
    height = surface_work->Info.CropH ? surface_work->Info.CropH : surface_work->Info.Height;

    dec_ext.Function = DESKTOP_GETDESKTOP_ID;
    dec_ext.pPrivateInputData =  &desktop_execute;
    dec_ext.PrivateInputDataSize = sizeof(desktop_execute);
    
    desktop_execute.DesktopFormat = (DXGI_FORMAT) MfxFourccToD3dFormat(surface_work->Info.FourCC);
    desktop_execute.StatusReportFeedbackNumber = ++StatusReportFeedbackNumber;
    desktop_execute.Width = width;
    desktop_execute.Height = height;

    hr = m_pDecoder->Execute(&dec_exec);
    if (FAILED(hr))
        return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{
    HRESULT hr;
    DXVA2_DecodeExecuteParams dec_exec = {0};
    DXVA2_DecodeExtensionData dec_ext =  {0};
    dec_exec.pExtensionData = &dec_ext;

    DESKTOP_PARAM_STRUCT_SIZE param_size = {0};
    DESKTOP_QUERY_STATUS_PARAMS pQuaryParams[QUERY_SIZE];

    dec_ext.Function = DESKTOP_QUERY_STATUS_ID;
    dec_ext.pPrivateInputData =  &param_size;
    dec_ext.PrivateInputDataSize = sizeof(param_size);

    param_size.SizeOfParamStruct = QUERY_SIZE;

    dec_ext.pPrivateOutputData =  pQuaryParams;
    dec_ext.PrivateOutputDataSize = sizeof(DESKTOP_QUERY_STATUS_PARAMS)*QUERY_SIZE;

    hr = m_pDecoder->Execute(&dec_exec);
    if (FAILED(hr))
        return MFX_ERR_DEVICE_FAILED;

    for (mfxU32 i=0; i < QUERY_SIZE; i++)
    {
        if (pQuaryParams[i].uStatus == 0 || pQuaryParams[i].uStatus == 3)
            StatusList.push_back(pQuaryParams[i]);
    }
    return MFX_ERR_NONE;
}

} //namespace MfxCapture