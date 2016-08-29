/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d11.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_d3d11.h"
#include "mfx_utils.h"

#include "Windows.h"

namespace MfxCapture
{

D3D11_Capturer::D3D11_Capturer(mfxCoreInterface* _core)
    :Capturer(HW_D3D11),
     m_pmfxCore(_core)
{
    m_TargetId = 0;
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    m_bImplicitResizeWA = true;
    m_CropW  = 0;
    m_CropH  = 0;
    m_AsyncDepth = 0;
#endif
}

D3D11_Capturer::~D3D11_Capturer()
{
    Destroy();
}

mfxStatus D3D11_Capturer::CreateVideoAccelerator( mfxVideoParam const & par, const mfxU32 targetId)
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
    D3D11_VIDEO_DECODER_DESC video_desc;
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxHDL hdl;

    mfxU16 width = 0;
    mfxU16 height = 0;
    DXGI_FORMAT format = MfxFourccToDxgiFormat(par.mfx.FrameInfo.FourCC);
    width  = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
    height = par.mfx.FrameInfo.CropH ? par.mfx.FrameInfo.CropH : par.mfx.FrameInfo.Height;

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    bool alloc_surfs = false;
    width  = (width % 2 ) ? (width + 1 ) : width;
    height = (height % 2) ? (height + 1) : height;
    alloc_surfs = true;
#endif

    mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&hdl);
    m_pD11Device = (ID3D11Device*)hdl;
    CComPtr<ID3D11DeviceContext> pImmediateContext;
    m_pD11Device->GetImmediateContext(&pImmediateContext);
    m_pD11Context = pImmediateContext;

    m_pD11VideoDevice = m_pD11Device;
    m_pD11VideoContext = m_pD11Context;

    video_desc.SampleWidth = width;
    video_desc.SampleHeight = height;
    video_desc.OutputFormat = format;
    video_desc.Guid = DXVADDI_Intel_GetDesktopScreen;

    D3D11_VIDEO_DECODER_CONFIG video_config = {0}; 

    mfxU32 cDecoderProfiles = m_pD11VideoDevice->GetVideoDecoderProfileCount();
    bool isRequestedGuidPresent = false;


    for (mfxU32 i = 0; i < cDecoderProfiles; i++)
    {
        GUID decoderGuid;
        HRESULT hr = m_pD11VideoDevice->GetVideoDecoderProfile(i, &decoderGuid);
        if (FAILED(hr))
        {
            continue;
        }

        if (DXVADDI_Intel_GetDesktopScreen == decoderGuid)
            isRequestedGuidPresent = true;

    }

    //HRESULT hr = m_pD11VideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
    //if (FAILED(hr)) 
    //    return MFX_ERR_DEVICE_FAILED;

    //for (mfxU32 i = 0; i < count; i++)
    //{
    //    hr = m_pD11VideoDevice->GetVideoDecoderConfig(&video_desc, i, &video_config);
    //    if (FAILED(hr))
    //        continue;


    //    return MFX_ERR_NONE;

    //} 

    /*Note that the format indicated here is the format driver reports for creating decode device (call CheckVideoDecoderFormat() with NV12), 
    not the format that GetDesktopScreen will output although they may be the same. 
    The application should query for the supported GetSesktopScreen format after creating the device as described in the flow. */
    video_desc.OutputFormat = DXGI_FORMAT_NV12;

    hres  = m_pD11VideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    if(m_TargetId)
    {
        D3D11_VIDEO_DECODER_EXTENSION dec_ext =  {0};
        DESKTOP_DISPLAY_SELECT display_select = {0};

        dec_ext.Function = DESKTOP_DISPLAY_SELECT_ID;
        dec_ext.pPrivateInputData =  &display_select;
        dec_ext.PrivateInputDataSize = sizeof(display_select);
        display_select.DisplayID = m_TargetId;

        hres = m_pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);
        if (FAILED(hres))
        {
            Destroy();
            return MFX_ERR_DEVICE_FAILED;
        }
    }

    //WA for implicit driver resize issue:
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    {
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

mfxStatus D3D11_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out, const mfxU32 targetId)
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

    m_pDecoder.Release();

    return MFX_ERR_NONE;
}

mfxStatus D3D11_Capturer::CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out)
{
    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION dec_ext =  {0};
    DESKTOP_FORMAT *desktop_format;
    DESKTOP_PARAM_STRUCT_SIZE param_size = {0};
    mfxU32 count = 0;

    mfxU16 width = 0;
    mfxU16 height = 0;
    width  = in.mfx.FrameInfo.CropW ? in.mfx.FrameInfo.CropW : in.mfx.FrameInfo.Width;
    height = in.mfx.FrameInfo.CropH ? in.mfx.FrameInfo.CropH : in.mfx.FrameInfo.Height;

    memset( &dec_ext, 0, sizeof(dec_ext));

    dec_ext.Function = DESKTOP_FORMAT_COUNT_ID;
    dec_ext.pPrivateOutputData =  &count;
    dec_ext.PrivateOutputDataSize = sizeof(count);


    hr = m_pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);
    if (FAILED(hr))
        return MFX_ERR_DEVICE_FAILED;

    if (count == 0)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    //count = dec_ext.PrivateOutputDataSize;

    desktop_format = new(std::nothrow) DESKTOP_FORMAT[count];
    if(!desktop_format) return MFX_ERR_MEMORY_ALLOC;
    param_size.SizeOfParamStruct = sizeof(DESKTOP_FORMAT);
    
    dec_ext.Function = DESKTOP_FORMATS_ID;
    dec_ext.pPrivateInputData =  &param_size;
    dec_ext.PrivateInputDataSize = sizeof(DESKTOP_PARAM_STRUCT_SIZE);

    dec_ext.pPrivateOutputData = desktop_format;
    dec_ext.PrivateOutputDataSize = sizeof(DESKTOP_FORMAT) * count; //Not sure about multiplying by count

    hr = m_pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);
    if (FAILED(hr))
    {
        delete []desktop_format;
        return MFX_ERR_DEVICE_FAILED;
    }

    for (mfxU32 i = 0; i < count; i++)
    {
        if(desktop_format[i].DesktopFormat == DXGI_FORMAT_NV12 && MFX_FOURCC_NV12 == in.mfx.FrameInfo.FourCC)
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
        else if((desktop_format[i].DesktopFormat == DXGI_FORMAT_B8G8R8A8_UNORM && 
                (MFX_FOURCC_RGB4 == in.mfx.FrameInfo.FourCC || DXGI_FORMAT_AYUV == in.mfx.FrameInfo.FourCC || MFX_FOURCC_AYUV_RGB4 == in.mfx.FrameInfo.FourCC)))
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

mfxStatus D3D11_Capturer::Destroy()
{
    bool error = false;
    //m_pD11Device.Release(); //controlled by the application

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    ResetInternalSurfaces(0,0);
#endif

    try{
        m_pD11Context.Release();
    } catch (...) {
        error = true;
    }
    try{
        m_pD11VideoDevice.Release();
    } catch (...) {
        error = true;
    }
    try{
        m_pD11VideoContext.Release();
    } catch (...) {
        error = true;
    }
    try{
        m_pDecoder.Release();
    } catch (...) {
        error = true;
    }

    if(error)
        return MFX_ERR_DEVICE_FAILED;
    else
        return MFX_ERR_NONE;
}

mfxStatus D3D11_Capturer::BeginFrame( mfxFrameSurface1* pSurf )
{
    if(!pSurf) return MFX_ERR_UNKNOWN;
    const mfxMemId MemId = pSurf->Data.MemId;
    HRESULT hr = S_OK;
    mfxHDLPair Pair;
    CComPtr<ID3D11VideoDecoderOutputView> pOutputView;

    mfxStatus mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, MemId, (mfxHDL*)&Pair);
    MFX_CHECK_STS(mfxRes);

    D3D11_RESOURCE_DIMENSION resType = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    ((ID3D11Resource *)Pair.first)->GetType(&resType);
    if(D3D11_RESOURCE_DIMENSION_TEXTURE2D != resType)
        return MFX_ERR_INVALID_HANDLE;

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    {
        ID3D11Texture2D* userTexture = (ID3D11Texture2D *)Pair.first;

        D3D11_TEXTURE2D_DESC desc;
        userTexture->GetDesc(&desc);

        mfxU32 CropW  = (pSurf->Info.CropW % 2) ? (pSurf->Info.CropW + 1) : pSurf->Info.CropW;
        mfxU32 CropH  = (pSurf->Info.CropH % 2) ? (pSurf->Info.CropH + 1) : pSurf->Info.CropH;

        if(m_CropW && m_CropH)
        {
            if(m_CropH != (mfxU32) desc.Height || m_CropW != (mfxU32) desc.Width)
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
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC OutputDesc;
        OutputDesc.DecodeProfile = DXVADDI_Intel_GetDesktopScreen;
        OutputDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
        OutputDesc.Texture2D.ArraySlice = (UINT)(size_t)Pair.second;


        hr = m_pD11VideoDevice->CreateVideoDecoderOutputView((ID3D11Resource *)Pair.first, 
                                                          &OutputDesc,
                                                          &pOutputView);


        if( SUCCEEDED( hr ) )
        {
            hr = m_pD11VideoContext->DecoderBeginFrame(m_pDecoder, pOutputView, 0, NULL);
            if SUCCEEDED( hr )
                return MFX_ERR_NONE;
        }
    }
    
    return MFX_ERR_DEVICE_FAILED;
}


mfxStatus D3D11_Capturer::EndFrame( )
{
    HRESULT hr = m_pD11VideoContext->DecoderEndFrame(m_pDecoder);
    if FAILED(hr)
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    return MFX_ERR_NONE;
}


mfxStatus D3D11_Capturer::GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber)
{
    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION dec_ext =  {0};
    DESKTOP_EXECUTE_PARAMS desktop_execute;

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    ID3D11Texture2D* pOwnTexture = 0;
    mfxFrameSurface1* pOwnMfxSurf = 0;
    CComPtr<ID3D11VideoDecoderOutputView> pOutputView;
    mfxStatus mfxRes = MFX_ERR_NONE;
    if(m_bImplicitResizeWA)
    {
        pOwnMfxSurf = GetFreeInternalSurface();
        if(!pOwnMfxSurf)
            return MFX_ERR_DEVICE_FAILED;
        mfxRes = m_pmfxCore->IncreaseReference(m_pmfxCore->pthis,&pOwnMfxSurf->Data);
        if(mfxRes)
            return mfxRes;

        pOwnTexture = (ID3D11Texture2D*) pOwnMfxSurf->Data.MemId;
        if(!pOwnTexture)
            return MFX_ERR_DEVICE_FAILED;

        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC OutputDesc;
        OutputDesc.DecodeProfile = DXVADDI_Intel_GetDesktopScreen;
        OutputDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
        OutputDesc.Texture2D.ArraySlice = (UINT)(size_t) 0;

        hr = m_pD11VideoDevice->CreateVideoDecoderOutputView(pOwnTexture, 
                                                          &OutputDesc,
                                                          &pOutputView);

        if( SUCCEEDED( hr ) )
        {
            hr = m_pD11VideoContext->DecoderBeginFrame(m_pDecoder, pOutputView, 0, NULL);
            if FAILED ( hr )
                return MFX_ERR_DEVICE_FAILED;
        }
    }
#endif

    mfxU16 width = 0;
    mfxU16 height = 0;
    mfxU16 croph = surface_work->Info.CropH;
    mfxU16 cropw = surface_work->Info.CropW;
    width  = cropw ? cropw : surface_work->Info.Width;
    height = croph ? croph : surface_work->Info.Height;
#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
    if(m_bImplicitResizeWA)
    {
        height = (height % 2) ? (height+1) : height;
        width  = (width % 2)  ? (width+1)  : width;
    }
#endif
    memset( &desktop_execute, 0, sizeof(desktop_execute));
    desktop_execute.DesktopFormat = MfxFourccToDxgiFormat(surface_work->Info.FourCC);

    desktop_execute.Height = height;
    desktop_execute.Width  = width;
    desktop_execute.StatusReportFeedbackNumber = ++StatusReportFeedbackNumber;

    dec_ext.Function = DESKTOP_GETDESKTOP_ID;
    dec_ext.pPrivateInputData =  &desktop_execute;
    dec_ext.PrivateInputDataSize = sizeof(desktop_execute);

    hr = m_pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);
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
        if(!pOwnTexture)
            return MFX_ERR_DEVICE_FAILED;

        mfxHDLPair Pair;
        mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, (mfxHDL*)&Pair);
        MFX_CHECK_STS(mfxRes);

        //RECT srcSize = {0,0,width,height};

        //std::list<DESKTOP_QUERY_STATUS_PARAMS> List;
        //QueryStatus(List);

        //m_pD11Context->CopyResource((ID3D11Resource*) Pair.first, pOwnTexture);
        //if (FAILED(hr))    return MFX_ERR_DEVICE_FAILED;
        m_pD11Context->CopySubresourceRegion((ID3D11Resource*) Pair.first, (UINT)(size_t)Pair.second, 0,0,0,pOwnTexture,0,0);

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


mfxStatus D3D11_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{
    if(StatusList.size())
        StatusList.clear();

    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION dec_ext =  {0};
    DESKTOP_PARAM_STRUCT_SIZE param_size = {0};
    DESKTOP_QUERY_STATUS_PARAMS pQuaryParams[QUERY_SIZE];

    param_size.SizeOfParamStruct = QUERY_SIZE;

    dec_ext.Function = DESKTOP_QUERY_STATUS_ID;
    dec_ext.pPrivateInputData =  &param_size;
    dec_ext.PrivateInputDataSize = sizeof(DESKTOP_PARAM_STRUCT_SIZE);

    dec_ext.pPrivateOutputData =  pQuaryParams;
    dec_ext.PrivateOutputDataSize = sizeof(DESKTOP_QUERY_STATUS_PARAMS)*QUERY_SIZE;

    hr = m_pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);
    if (FAILED(hr))
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    for (mfxU32 i=0; i < QUERY_SIZE; i++)
    {
        if (pQuaryParams[i].uStatus == 0 || pQuaryParams[i].uStatus == 3)
            StatusList.push_back(pQuaryParams[i]);
    }
    return MFX_ERR_NONE;
}

#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)
mfxStatus D3D11_Capturer::ResetInternalSurfaces(const mfxU32& n, mfxFrameInfo* frameInfo)
{
    if(n && !frameInfo)
        return MFX_ERR_UNKNOWN;

    if(0 == n)
    {
        if(m_InternalSurfPool.size())
        {
            for(std::list<mfxFrameSurface1>::iterator it = m_InternalSurfPool.begin(); it != m_InternalSurfPool.end(); ++it)
            {
                if((ID3D11Texture2D*) it->Data.MemId)
                {
                    ((ID3D11Texture2D*) it->Data.MemId)->Release();
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

        if(!m_pD11Device)
            return MFX_ERR_MEMORY_ALLOC;

        mfxU32 width   = (frameInfo->Width % 2) ?  (frameInfo->Width + 1) :  frameInfo->Width;
        mfxU32 height  = (frameInfo->Height % 2) ? (frameInfo->Height + 1) : frameInfo->Height;
        //mfxU32 width   = frameInfo->Width;
        //mfxU32 height  = frameInfo->Height;

        DXGI_FORMAT format = MfxFourccToDxgiFormat(frameInfo->FourCC);
        if(DXGI_FORMAT_B8G8R8A8_UNORM == format)
            format = DXGI_FORMAT_AYUV;

        m_CropW = width;
        m_CropH = height;

        D3D11_TEXTURE2D_DESC desc = {0};

        desc.Width = width;
        desc.Height =  height;

        desc.MipLevels = 1;
        //number of subresources is 1 in case of not single texture
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.MiscFlags = 0;

        desc.BindFlags = D3D11_BIND_DECODER;

        for(mfxU16 i = 0; i < n; ++i)
        {
            ID3D11Texture2D* pTexture = 0;
            mfxFrameSurface1 surf = {0,0,0,0,*frameInfo,0};

            hr = m_pD11Device->CreateTexture2D(&desc, NULL, &pTexture);

            if(FAILED(hr) || !pTexture)
            {
                if(pTexture) pTexture->Release();
                ResetInternalSurfaces(0,0);
                return MFX_ERR_DEVICE_FAILED;
            }
            surf.Data.MemId = pTexture;
            m_InternalSurfPool.push_back(surf);
        }
    }

    return MFX_ERR_NONE;
}

mfxFrameSurface1* D3D11_Capturer::GetFreeInternalSurface()
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
#endif //#if defined(ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE)

} //namespace MfxCapture