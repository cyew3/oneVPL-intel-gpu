/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_plugin.cpp

\* ****************************************************************************** */

#include <windows.h>
#include <initguid.h>
#include "mfx_screen_capture_plugin.h"
#include "mfx_utils.h"
#include "mfxstructures.h"


// {BF44DACD-217F-4370-A383-D573BC56707E}
DEFINE_GUID(DXVADDI_Intel_GetDesktopScreen, 
            0xbf44dacd, 0x217f, 0x4370, 0xa3, 0x83, 0xd5, 0x73, 0xbc, 0x56, 0x70, 0x7e);

typedef struct tagDESKTOP_EXECUTE_PARAMS
{
    DXGI_FORMAT  DesktopFormat;
    UINT         StatusReportFeedbackNumber;
    USHORT       Width;
    USHORT      Height;
    UINT      Reserved;
} DESKTOP_EXECUTE_PARAMS;

typedef struct tagDESKTOP_FORMAT
{
    DXGI_FORMAT      DesktopFormat;
    USHORT           MaxWidth;
    USHORT           MaxHeight;
    UINT             Reserved[2];
} DESKTOP_FORMAT;

typedef struct tagDESKTOP_PARAM_STRUCT_SIZE
{
    UINT    SizeOfParamStruct;
    UINT    reserved;
} DESKTOP_PARAM_STRUCT_SIZE;





enum
{
    QUERY_SIZE = 32
};

enum
{
    DESKTOP_FORMAT_COUNT_ID = 0x100,
    DESKTOP_FORMATS_ID      = 0x101,
    DESKTOP_GETDESKTOP_ID   = 0x104,
    DESKTOP_QUERY_STATUS_ID = 0x105
};

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    return MFXScreenCapture_Plugin::Create();
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    return MFXScreenCapture_Plugin::CreateByDispatcher(uid, plugin);
}


MFXScreenCapture_Plugin::MFXScreenCapture_Plugin(bool CreateByDispatcher)
{
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));
    memset(&m_par, 0, sizeof(mfxVideoParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = MFX_PLUGINID_CAPTURE_HW;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_DECODE;
    m_PluginParam.PluginVersion = 1;
    m_PluginParam.CodecId = MFX_CODEC_CAPTURE;
    m_createdByDispatcher = CreateByDispatcher;
    m_StatusReportFeedbackNumber = 0;

}

MFXScreenCapture_Plugin::~MFXScreenCapture_Plugin()
{
}

mfxStatus MFXScreenCapture_Plugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;

    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);

    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    if (m_createdByDispatcher) {
        delete this;
    }
    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *, mfxFrameAllocRequest *out)
{
    MFX_CHECK_NULL_PTR2(par, out);
    out->Info.FourCC = MFX_FOURCC_NV12;

    if (par->AsyncDepth)
    {
        out->NumFrameMin = out->NumFrameSuggested = par->AsyncDepth;
    }
    else
    {
        out->NumFrameMin = out->NumFrameSuggested = 1; //default value 
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) 
    {
        out->Type =  MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) 
    {
        out->Type =  MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else //all other types including opaq are unsupported (for now)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXScreenCapture_Plugin::Init(mfxVideoParam *par)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    mfxHDL hdl;
    m_par = *par;
    mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&hdl);
    m_pD11Device = (ID3D11Device*)hdl;
    CComPtr<ID3D11DeviceContext> pImmediateContext;
    m_pD11Device->GetImmediateContext(&pImmediateContext);
    m_pD11Context = pImmediateContext;

    m_pD11VideoDevice = m_pD11Device;
    m_pD11VideoContext = m_pD11Context;

    mfxRes = CreateVideoAccelerator();
    MFX_CHECK_STS(mfxRes);

    mfxRes = CheckCapabilities();
    MFX_CHECK_STS(mfxRes);

    return mfxRes;


}
mfxStatus MFXScreenCapture_Plugin::CreateVideoAccelerator()
{
    HRESULT hres;
    D3D11_VIDEO_DECODER_DESC video_desc;

    video_desc.SampleWidth = m_par.mfx.FrameInfo.Width;
    video_desc.SampleHeight = m_par.mfx.FrameInfo.Height;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;
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
    
    hres  = m_pD11VideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;
    //w/a for debug. Waiting for HSD5595359. This w/a will cause decoder resource leak.
#if defined(_DEBUG)
    m_pDecoder.p->AddRef();
#endif

    return MFX_ERR_NONE;
}
mfxStatus MFXScreenCapture_Plugin::DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task)
{
    mfxStatus mfxRes = DecodeFrameSubmit(bs, surface_work, surface_out);
    if (MFX_ERR_NONE == mfxRes)
    {
        AsyncParams *pAsyncParam = new AsyncParams;
        pAsyncParam->surface_work = surface_work;
        pAsyncParam->surface_out = *surface_out;
        pAsyncParam->StatusReportFeedbackNumber = m_StatusReportFeedbackNumber;
        *task = (mfxThreadTask*)pAsyncParam;
    }
    return mfxRes;

}

mfxStatus MFXScreenCapture_Plugin::DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)
{
    mfxStatus mfxRes;
    CComPtr<ID3D11VideoDecoderOutputView> pOutputView;
    
    if (bs) // bs should be zero
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (!surface_work || !surface_out)
        return MFX_ERR_NULL_PTR;
    
    //only external surfaces are supported for now
    if (!surface_work->Data.MemId)
        return MFX_ERR_UNSUPPORTED;

    if (surface_work->Data.Locked)
        return MFX_ERR_LOCK_MEMORY;

    mfxRes = m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &surface_work->Data);
    MFX_CHECK_STS(mfxRes);

    mfxRes = CheckFrameInfo(&surface_work->Info);
    MFX_CHECK_STS(mfxRes);

    mfxRes = BeginFrame(surface_work->Data.MemId, pOutputView);
    MFX_CHECK_STS(mfxRes);

    mfxRes = GetDesktopScreenOperation(surface_work);
    MFX_CHECK_STS(mfxRes);

    HRESULT hr = m_pD11VideoContext->DecoderEndFrame(m_pDecoder);
    if FAILED(hr)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    surface_work->Info.CropH = m_par.mfx.FrameInfo.Height;
    surface_work->Info.CropW = m_par.mfx.FrameInfo.Width;

    surface_work->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    *surface_out = surface_work;
    return MFX_ERR_NONE;
    
}

mfxStatus MFXScreenCapture_Plugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    // should be query here 
    uid_p; uid_a;
    mfxStatus mfxRes = MFX_ERR_NONE;
    AsyncParams *pAsyncParam  = (AsyncParams *)task;
    mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &pAsyncParam->surface_work->Data);

    //will be enabled after check 
    //if (!m_StatusList.size())
    //{
    //    mfxRes = QueryStatus();
    //    MFX_CHECK_STS(mfxRes);
    //}

    //if (m_StatusList.front().StatusReportFeedbackNumber == pAsyncParam->StatusReportFeedbackNumber)
    //{
    //    if (m_StatusList.front().uStatus == 3)
    //        pAsyncParam->surface_work->Data.Corrupted = MFX_CORRUPTION_MAJOR;
    //    m_StatusList.pop_front();
    //    mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &pAsyncParam->surface_work->Data);
    //}
    //else if (m_StatusList.front().StatusReportFeedbackNumber > pAsyncParam->StatusReportFeedbackNumber) // frames should go one by one 
    //{
    //    return MFX_ERR_DEVICE_FAILED;
    //}
    //else 
    //{
    //    mfxRes = QueryStatus();
    //    return mfxRes;
    //}
    
    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::CheckFrameInfo(mfxFrameInfo *info)
{
    if (info->Width  > m_par.mfx.FrameInfo.Width)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (info->Height > m_par.mfx.FrameInfo.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return MFX_ERR_NONE;
}

mfxStatus  MFXScreenCapture_Plugin::BeginFrame(mfxMemId MemId, ID3D11VideoDecoderOutputView *pOutputView)
{
    HRESULT hr = S_OK;
    mfxHDLPair Pair;
    
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

mfxStatus  MFXScreenCapture_Plugin::GetDesktopScreenOperation(mfxFrameSurface1 *surface_work)
{
    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION dec_ext =  {0};
    DESKTOP_EXECUTE_PARAMS desktop_execute;

    memset( &desktop_execute, 0, sizeof(desktop_execute));
    desktop_execute.DesktopFormat = DXGI_FORMAT_NV12;
    desktop_execute.Height = surface_work->Info.Height;
    desktop_execute.Width = surface_work->Info.Width;
    desktop_execute.StatusReportFeedbackNumber = ++m_StatusReportFeedbackNumber;

    dec_ext.Function = DESKTOP_GETDESKTOP_ID;
    dec_ext.pPrivateInputData =  &desktop_execute;
    dec_ext.PrivateInputDataSize = sizeof(desktop_execute);


    hr = m_pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);

    if (FAILED(hr))
        return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;

}

mfxStatus  MFXScreenCapture_Plugin::CheckCapabilities()
{
    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION dec_ext =  {0};
    DESKTOP_FORMAT *desktop_format;
    DESKTOP_PARAM_STRUCT_SIZE param_size = {0};
    mfxU32 count = 0;

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
    
    count = dec_ext.PrivateOutputDataSize;

    desktop_format = new DESKTOP_FORMAT[count];
    param_size.SizeOfParamStruct = count;
    
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

    for (mfxU32 i= 0; i < count; i++)
    {
        if(desktop_format[i].DesktopFormat == DXGI_FORMAT_NV12)
        {
            if (m_par.mfx.FrameInfo.Height <= desktop_format[i].MaxHeight &&
                m_par.mfx.FrameInfo.Width <= desktop_format[i].MaxWidth)
            {
                delete []desktop_format;
                return MFX_ERR_NONE;
            }

        }
    }
    
    delete []desktop_format;
    return MFX_ERR_UNSUPPORTED;

}

mfxStatus  MFXScreenCapture_Plugin::QueryStatus()
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
            m_StatusList.push_back(pQuaryParams[i]);
    }
    return MFX_ERR_NONE;

}



