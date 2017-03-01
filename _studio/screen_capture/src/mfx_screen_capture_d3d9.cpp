//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_screen_capture_d3d9.h"
#include "mfx_utils.h"

#include "Windows.h"

namespace MfxCapture
{

D3D9_Capturer::D3D9_Capturer(mfxCoreInterface* _core)
    :Capturer(HW_D3D9),
     m_pmfxCore(_core)
{
    m_TargetId = 0;
    m_hDirectXHandle = 0;
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    m_bImplicitResizeWA = true;
    m_CropW  = 0;
    m_CropH  = 0;
    m_AsyncDepth = 0;
#endif
}

D3D9_Capturer::~D3D9_Capturer()
{
    Destroy();
}

mfxStatus D3D9_Capturer::CreateVideoAccelerator( mfxVideoParam const & par, const mfxU32 targetId)
{
    if(IsWin10orLater())
        return MFX_ERR_UNSUPPORTED;

    if(targetId)
    {
        //dispDescr disp = GetTargetId(dispIndex);
        //m_TargetId = disp.TargetID;
        m_TargetId = targetId;
    }

    HRESULT hres;

    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxHDL hdl;

    mfxU16 width = 0;
    mfxU16 height = 0;
    D3DFORMAT format = MfxFourccToD3dFormat(par.mfx.FrameInfo.FourCC);
    width  = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
    height = par.mfx.FrameInfo.CropH ? par.mfx.FrameInfo.CropH : par.mfx.FrameInfo.Height;


#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    bool alloc_surfs = false;
    {
        width  = (width % 2 ) ? (width + 1 ) : width;
        height = (height % 2) ? (height + 1) : height;
        alloc_surfs = true;
    }
#endif

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
    {
        DXVA2_DecodeExecuteParams dec_exec = { 0 };
        DXVA2_DecodeExtensionData dec_ext = { 0 };
        dec_exec.pExtensionData = &dec_ext;

        mfxU32 count = 0;
        dec_ext.Function = AUXDEV_GET_ACCEL_GUID_COUNT;
        dec_ext.pPrivateOutputData = &count;
        dec_ext.PrivateOutputDataSize = sizeof(count);
        hres = m_pDecoder->Execute(&dec_exec);
        MFX_CHECK(SUCCEEDED(hres), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(count, MFX_ERR_DEVICE_FAILED);

        dec_ext.Function = AUXDEV_GET_ACCEL_GUIDS;
        pGuids = new(std::nothrow) GUID[count];
        if (!pGuids) return MFX_ERR_MEMORY_ALLOC;
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
    }

    dummy_surf->Release();

    if(m_TargetId)
    {
        DXVA2_DecodeExecuteParams dec_exec = {0};
        DXVA2_DecodeExtensionData dec_ext =  {0};
        dec_exec.pExtensionData = &dec_ext;
        DESKTOP_DISPLAY_SELECT display_select = {0};

        dec_ext.Function = DESKTOP_DISPLAY_SELECT_ID;
        dec_ext.pPrivateInputData =  &display_select;
        dec_ext.PrivateInputDataSize = sizeof(display_select);

        display_select.DisplayID = m_TargetId;

        hres = m_pDecoder->Execute(&dec_exec);
        if (FAILED(hres))
        {
            return MFX_ERR_DEVICE_FAILED;
        }
    }

    //WA for implicit driver resize issue:
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    {
        //always get Direct3DDevice for possible resize WA
        hres = m_pDirect3DDeviceManager->LockDevice(m_hDirectXHandle, &m_pDirect3DDevice, FALSE);
        if(FAILED(hres))
        {
            m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
            return MFX_ERR_DEVICE_FAILED;
        }

        hres = m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, FALSE);
        if(FAILED(hres))
        {
            m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
            return MFX_ERR_DEVICE_FAILED;
        }
        if(m_pDirect3DDevice)
            m_pDirect3DDevice.Release();

        mfxU32 n = par.AsyncDepth ? par.AsyncDepth : 1;

        m_CropW = width;
        m_CropH = height;
        m_AsyncDepth = n;

        mfxFrameInfo int_info = par.mfx.FrameInfo;
        int_info.Width  = int_info.CropW = width;
        int_info.Height = int_info.CropH = height;


        if(width  != par.mfx.FrameInfo.Width ||
           height != par.mfx.FrameInfo.Height ||
           alloc_surfs                         )
        {
            m_bImplicitResizeWA = true;

            mfxRes = ResetInternalSurfaces(n, &int_info);
            if(MFX_ERR_NONE != mfxRes)
            {
                Destroy();
                return MFX_ERR_MEMORY_ALLOC;
            }
        }
        else
        {
            m_bImplicitResizeWA = false;
        }
    }
#endif

    return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out, const mfxU32 targetId)
{
    //temporary creates decoder instance to check capabilities
    mfxStatus mfxRes = MFX_ERR_NONE;

    mfxRes = CreateVideoAccelerator(in, targetId);
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
    desktop_format = new(std::nothrow) DESKTOP_FORMAT[count];
    if(!desktop_format) return MFX_ERR_MEMORY_ALLOC;

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
    bool error = false;

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    ResetInternalSurfaces(0,0);
#endif

    if(m_pDecoder)
    {
        try{
            m_pDecoder.Release();
        } catch (...) {
            error = true;
        }
    }

    try{
        if(m_pDirectXVideoService)
            m_pDirectXVideoService.Release();
    } catch (...) {
        error = true;
    }
    try{
        if(m_pDirect3DDevice)
            m_pDirect3DDevice.Release();
    } catch (...) {
        error = true;
    }
    try{
        if(m_pD3D)
            m_pD3D.Release();
    } catch (...) {
        error = true;
    }
    try{
        if (m_pDirect3DDeviceManager && m_hDirectXHandle != INVALID_HANDLE_VALUE)
        {
            m_pDirect3DDeviceManager->CloseDeviceHandle(m_hDirectXHandle);
            m_hDirectXHandle = INVALID_HANDLE_VALUE;
        }
    } catch (...) {
        error = true;
    }

    //if(m_pDirect3DDeviceManager)
    //    m_pDirect3DDeviceManager.Release();

    if(error)
        return MFX_ERR_DEVICE_FAILED;
    else
        return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::BeginFrame( mfxFrameSurface1* pSurf)
{
    if(!pSurf) return MFX_ERR_UNKNOWN;
    const mfxMemId MemId = pSurf->Data.MemId;
    HRESULT hr = S_OK;
    mfxHDLPair Pair;
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    {
        mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, MemId, (mfxHDL*)&Pair);
        MFX_CHECK_STS(mfxRes);
        IDirect3DSurface9* d3dSurf = (IDirect3DSurface9*) Pair.first;
        if(!d3dSurf)
            return MFX_ERR_DEVICE_FAILED;

        D3DSURFACE_DESC desc;

        hr = d3dSurf->GetDesc(&desc);
        if(FAILED(hr))
            return MFX_ERR_DEVICE_FAILED;

        mfxU32 CropW  = (pSurf->Info.CropW % 2) ? (pSurf->Info.CropW + 1) : pSurf->Info.CropW;
        mfxU32 CropH  = (pSurf->Info.CropH % 2) ? (pSurf->Info.CropH + 1) : pSurf->Info.CropH;

        if(m_CropW && m_CropH)
        {
            if(m_CropH != (mfxU32) desc.Height || (mfxU32) m_CropW != (mfxU32) desc.Width)
            {
                //user provided surface was allocated with a different resolution
                if(0 == m_InternalSurfPool.size() ||
                    (m_InternalSurfPool.front().Info.Width != CropW || m_InternalSurfPool.front().Info.Height != CropH))
                {
                    mfxFrameInfo info = pSurf->Info;
                    info.Width  = (mfxU16) CropW;
                    info.Height = (mfxU16) CropH;
                    mfxRes = ResetInternalSurfaces(m_AsyncDepth, &info);
                    if(mfxRes)
                        return MFX_ERR_DEVICE_FAILED;

                    m_bImplicitResizeWA = true;
                }
            }
            else
            {
                m_bImplicitResizeWA = false;
            }
        }

    }

    if(m_bImplicitResizeWA)
    {
        return MFX_ERR_NONE;
    }
    else
#endif
    {
        mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, MemId, (mfxHDL*)&Pair);
        MFX_CHECK_STS(mfxRes);

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
    HRESULT hr;

    if(!surface_work)
        return MFX_ERR_NULL_PTR;

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    mfxStatus mfxRes;
    IDirect3DSurface9* pOwnSurface = 0;
    mfxFrameSurface1* pOwnMfxSurf = 0;

    if(m_bImplicitResizeWA)
    {
        pOwnMfxSurf = GetFreeInternalSurface();
        if(!pOwnMfxSurf)
            return MFX_ERR_DEVICE_FAILED;
        mfxRes = m_pmfxCore->IncreaseReference(m_pmfxCore->pthis,&pOwnMfxSurf->Data);
        if(mfxRes)
            return mfxRes;

        pOwnSurface = (IDirect3DSurface9*) pOwnMfxSurf->Data.MemId;
        if(!pOwnSurface)
            return MFX_ERR_DEVICE_FAILED;

        hr = m_pDecoder->BeginFrame(pOwnSurface, 0);
        if FAILED( hr )
            return MFX_ERR_DEVICE_FAILED;
    }
#endif

    DXVA2_DecodeExecuteParams dec_exec = {0};
    DXVA2_DecodeExtensionData dec_ext =  {0};
    dec_exec.pExtensionData = &dec_ext;

    DESKTOP_EXECUTE_PARAMS desktop_execute;

    mfxU16 width = 0;
    mfxU16 height = 0;
    mfxU16 croph = surface_work->Info.CropH;
    mfxU16 cropw = surface_work->Info.CropW;
    height = croph ? croph : surface_work->Info.Height;
    width  = cropw ? cropw : surface_work->Info.Width;
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    if(m_bImplicitResizeWA)
    {
        height = (height % 2) ? (height+1) : height;
        width  = (width % 2)  ? (width+1)  : width;
    }
#endif

    dec_ext.Function = DESKTOP_GETDESKTOP_ID;
    dec_ext.pPrivateInputData =  &desktop_execute;
    dec_ext.PrivateInputDataSize = sizeof(desktop_execute);
    
    desktop_execute.DesktopFormat = (DXGI_FORMAT) MfxFourccToD3dFormat(surface_work->Info.FourCC);
    desktop_execute.StatusReportFeedbackNumber = ++StatusReportFeedbackNumber;
    desktop_execute.Width = width;
    desktop_execute.Height = height;

    hr = m_pDecoder->Execute(&dec_exec);
    if (FAILED(hr))
    {
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
        if(pOwnMfxSurf)
        {
            m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&pOwnMfxSurf->Data);
        }
#endif
        return MFX_ERR_DEVICE_FAILED;
    }

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    if(m_bImplicitResizeWA)
    {
        if(!pOwnSurface)
            return MFX_ERR_DEVICE_FAILED;

        mfxHDLPair Pair;
        mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, (mfxHDL*)&Pair);
        MFX_CHECK_STS(mfxRes);

        RECT srcSize = {0,0,width,height};

        //std::list<DESKTOP_QUERY_STATUS_PARAMS> List;
        //QueryStatus(List);

        hr = m_pDirect3DDeviceManager->LockDevice(m_hDirectXHandle, &m_pDirect3DDevice, true);
        if (FAILED(hr))    return MFX_ERR_DEVICE_FAILED;

        hr = m_pDirect3DDevice->StretchRect(pOwnSurface, &srcSize, (IDirect3DSurface9*) Pair.first, &srcSize, D3DTEXF_NONE);
        if (FAILED(hr))    return MFX_ERR_DEVICE_FAILED;

        hr = m_pDirect3DDeviceManager->UnlockDevice(m_hDirectXHandle, false);
        if (FAILED(hr))    return MFX_ERR_DEVICE_FAILED;

        if(m_pDirect3DDevice)
            m_pDirect3DDevice.Release();

        if(pOwnMfxSurf)
        {
            mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&pOwnMfxSurf->Data);
            if(mfxRes)
                return mfxRes;
        }
    }
#endif

    return MFX_ERR_NONE;
}

mfxStatus D3D9_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{
    if(StatusList.size())
        StatusList.clear();

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

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
mfxStatus D3D9_Capturer::ResetInternalSurfaces(const mfxU32& n, mfxFrameInfo* frameInfo)
{
    if(n && !frameInfo)
        return MFX_ERR_UNKNOWN;

    if(0 == n)
    {
        if(m_InternalSurfPool.size())
        {
            for(std::list<mfxFrameSurface1>::iterator it = m_InternalSurfPool.begin(); it != m_InternalSurfPool.end(); ++it)
            {
                if((IDirect3DSurface9*) it->Data.MemId)
                {
                    ((IDirect3DSurface9*) it->Data.MemId)->Release();
                    it->Data.MemId = 0;
                }
            }
            m_InternalSurfPool.clear();
        }

        return MFX_ERR_NONE;
    }

    //Create own surface pool
    if(frameInfo && n)
    {
        if(m_InternalSurfPool.size())
        {
            ResetInternalSurfaces(0,0);
        }

        HRESULT hr;

        if(!m_pDirectXVideoService)
            return MFX_ERR_MEMORY_ALLOC;

        mfxU32 width   = /*(frameInfo->Width % 2) ?  (frameInfo->Width + 1) :  */frameInfo->Width;
        mfxU32 height  = (frameInfo->Height % 2) ? (frameInfo->Height + 1) : frameInfo->Height;
        //mfxU32 width   = frameInfo->Width;
        //mfxU32 height  = frameInfo->Height;

        D3DFORMAT format = MfxFourccToD3dFormat(frameInfo->FourCC);

        m_CropW = width;
        m_CropH = height;

        for(mfxU16 i = 0; i < n; ++i)
        {
            IDirect3DSurface9* pSurface = 0;
            mfxFrameSurface1 surf = {0,0,0,0,*frameInfo,0};

            hr = m_pDirectXVideoService->CreateSurface(width, height, 0, format, D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &pSurface, NULL);
            if(FAILED(hr) || !pSurface)
            {
                if(pSurface) pSurface->Release();
                ResetInternalSurfaces(0,0);
                return MFX_ERR_DEVICE_FAILED;
            }
            surf.Data.MemId = pSurface;
            m_InternalSurfPool.push_back(surf);
        }
    }

    return MFX_ERR_NONE;
}

mfxFrameSurface1* D3D9_Capturer::GetFreeInternalSurface()
{
    if(!m_InternalSurfPool.size())
        return 0;
    mfxFrameSurface1* pSurf = 0; 

    for(std::list<mfxFrameSurface1>::iterator it = m_InternalSurfPool.begin(); it != m_InternalSurfPool.end(); ++it)
    {
        if( !(*it).Data.Locked)
        {
            pSurf = &(*it);
            break;
        }
    }
    return pSurf;
}
#endif

} //namespace MfxCapture