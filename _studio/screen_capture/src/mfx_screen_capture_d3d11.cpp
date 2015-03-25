/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d11.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_d3d11.h"
#include "mfx_utils.h"

namespace MfxCapture
{

D3D11_Capturer::D3D11_Capturer(mfxCoreInterface* _core)
    :m_pmfxCore(_core)
{
    Mode = HW_D3D11;
}

D3D11_Capturer::~D3D11_Capturer()
{
    Destroy();
}

mfxStatus D3D11_Capturer::CreateVideoAccelerator( mfxVideoParam const & par)
{
    HRESULT hres;
    D3D11_VIDEO_DECODER_DESC video_desc;
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxHDL hdl;

    mfxU16 width = 0;
    mfxU16 height = 0;
    DXGI_FORMAT format = MfxFourccToDxgiFormat(par.mfx.FrameInfo.FourCC);
    width  = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
    height = par.mfx.FrameInfo.CropH ? par.mfx.FrameInfo.CropH : par.mfx.FrameInfo.Height;

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

    return MFX_ERR_NONE;
}

mfxStatus D3D11_Capturer::QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out)
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

    desktop_format = new DESKTOP_FORMAT[count];
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
                (MFX_FOURCC_RGB4 == in.mfx.FrameInfo.FourCC || DXGI_FORMAT_AYUV == in.mfx.FrameInfo.FourCC)))
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
    //m_pD11Device.Release(); //controlled by the application
    m_pD11Context.Release();
    m_pD11VideoDevice.Release();
    m_pD11VideoContext.Release();
    m_pDecoder.Release();

    return MFX_ERR_NONE;
}

mfxStatus D3D11_Capturer::BeginFrame( mfxMemId MemId)
{
    HRESULT hr = S_OK;
    mfxHDLPair Pair;
    CComPtr<ID3D11VideoDecoderOutputView> pOutputView;

    mfxStatus mfxRes = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, MemId, (mfxHDL*)&Pair);
    MFX_CHECK_STS(mfxRes);

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

    memset( &desktop_execute, 0, sizeof(desktop_execute));
    desktop_execute.DesktopFormat = MfxFourccToDxgiFormat(surface_work->Info.FourCC);
    mfxU16 croph = surface_work->Info.CropY + surface_work->Info.CropH;
    mfxU16 cropw = surface_work->Info.CropX + surface_work->Info.CropW;
    desktop_execute.Height = croph ? croph : surface_work->Info.Height;
    desktop_execute.Width  = cropw ? cropw : surface_work->Info.Width;
    desktop_execute.StatusReportFeedbackNumber = ++StatusReportFeedbackNumber;

    dec_ext.Function = DESKTOP_GETDESKTOP_ID;
    dec_ext.pPrivateInputData =  &desktop_execute;
    dec_ext.PrivateInputDataSize = sizeof(desktop_execute);


    hr = m_pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);

    if (FAILED(hr))
        return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;
}


mfxStatus D3D11_Capturer::QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList)
{
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

} //namespace MfxCapture