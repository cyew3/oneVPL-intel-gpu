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
    memset(&m_CurrentPar, 0, sizeof(mfxVideoParam));
    memset(&m_InitPar, 0, sizeof(mfxVideoParam));
    m_inited = false;

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

    //only MFX_IMPL_VIA_D3D11 is supported
    if(MFX_IMPL_VIA_D3D11 != (par.Impl & 0x0F00))
        return MFX_ERR_UNSUPPORTED;

    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    mfxRes = Close();

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
    if(m_inited)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxRes = QueryMode2(*par, m_InitPar, true);
    if(MFX_ERR_UNSUPPORTED ==  mfxRes)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if(MFX_ERR_NONE != mfxRes)
        return mfxRes;

    if(!m_InitPar.AsyncDepth)
        m_InitPar.AsyncDepth = 1;
    if(!m_InitPar.mfx.FrameInfo.CropH)
        m_InitPar.mfx.FrameInfo.CropH = m_InitPar.mfx.FrameInfo.Height;
    if(!m_InitPar.mfx.FrameInfo.CropW)
        m_InitPar.mfx.FrameInfo.CropW = m_InitPar.mfx.FrameInfo.Width;
    if(!m_InitPar.mfx.FrameInfo.PicStruct)
        m_InitPar.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    m_CurrentPar = m_InitPar;
    m_inited = true;

    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::Close()
{
    if(!m_inited)
        return MFX_ERR_NOT_INITIALIZED;

    m_pD11Device.Release();
    m_pD11Context.Release();
    m_pD11VideoDevice.Release();
    m_pD11VideoContext.Release();
    m_pDecoder.Release();

    m_inited = false;

    return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    if(in)
        return QueryMode2(*in,*out);
    else
        return QueryMode1(*out);
}

mfxStatus MFXScreenCapture_Plugin::QueryMode1(mfxVideoParam& out)
{
    /*
    If the in pointer is zero, the function returns the class configurability in the output mfxVideoParam structure.
    A non-zero value in each field of the output structure indicates that the SDK implementation can configure the field with Init.
    */
    out.AsyncDepth                  = 1;
    out.IOPattern                   = 1;
    out.mfx.FrameInfo.FourCC        = 1;
    out.mfx.FrameInfo.ChromaFormat  = 1;
    out.mfx.FrameInfo.Width         = 1;
    out.mfx.FrameInfo.Height        = 1;
    out.mfx.FrameInfo.CropW         = 1;
    out.mfx.FrameInfo.CropH         = 1;
    //CropX and CropY are not supported by driver

    //Todo: should it be there?
    //out.mfx.FrameInfo.FrameRateExtN = 1;
    //out.mfx.FrameInfo.FrameRateExtD = 1;
    //out.mfx.FrameInfo.AspectRatioW = 1;
    //out.mfx.FrameInfo.AspectRatioH = 1;
    //out.mfx.FrameInfo.PicStruct    = 1;

    return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::QueryMode2(const mfxVideoParam& in, mfxVideoParam& out, bool onInit)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxU16 width  = 0;
    mfxU16 height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_NV12;

    //bool warning     = false;
    bool error       = false;

    out.IOPattern  = in.IOPattern;
    out.AsyncDepth = in.AsyncDepth;
    memcpy_s(&out.mfx,sizeof(out.mfx),&in.mfx,sizeof(in.mfx));

    if(in.AsyncDepth > 5 )
    {
        error = true;
        out.AsyncDepth = 0;
    }
    if(in.IOPattern & 0xF)
    {
        //encoder's or VPP's IOPatterns are unsupported
        out.IOPattern = 0;
        error = true;
    }
    if(in.ExtParam || in.NumExtParam || out.ExtParam || out.NumExtParam)
    {
        error = true;
    }
    if(in.Protected)
    {
        out.Protected = 0;
        error = true;
    }
    if(in.mfx.LowPower)
    {
        out.mfx.LowPower = 0;
        error = true;
    }
    if(in.mfx.BRCParamMultiplier)
    {
        out.mfx.BRCParamMultiplier = 0;
        error = true;
    }
    if(MFX_CODEC_CAPTURE != in.mfx.CodecId)
    {
        out.mfx.CodecId = 0;
        error = true;
    }
    if(in.mfx.CodecProfile)
    {
        out.mfx.CodecProfile = 0;
        error = true;
    }
    if(in.mfx.CodecLevel)
    {
        out.mfx.CodecLevel = 0;
        error = true;
    }
    if(in.mfx.NumThread)
    {
        out.mfx.NumThread = 0;
        error = true;
    }

    if(in.mfx.DecodedOrder       ||    /*in.mfx.ExtendedPicStruct  || */   in.mfx.TimeStampCalc      ||    in.mfx.SliceGroupsPresent ||
       in.mfx.JPEGChromaFormat   ||    /*in.mfx.Rotation           || */   in.mfx.JPEGColorFormat    ||    in.mfx.InterleavedDec        )
    {
        out.mfx.DecodedOrder       = 0;
        out.mfx.ExtendedPicStruct  = 0;
        out.mfx.TimeStampCalc      = 0;
        out.mfx.SliceGroupsPresent = 0;
        out.mfx.JPEGChromaFormat   = 0;
        out.mfx.Rotation           = 0;
        out.mfx.JPEGColorFormat    = 0;
        out.mfx.InterleavedDec     = 0;
    }

    if(in.mfx.FrameInfo.PicStruct && (MFX_PICSTRUCT_PROGRESSIVE != in.mfx.FrameInfo.PicStruct))
    {
        out.mfx.FrameInfo.PicStruct = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.CropH > in.mfx.FrameInfo.Height)
    {
        out.mfx.FrameInfo.CropH = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.CropW > in.mfx.FrameInfo.Width)
    {
        out.mfx.FrameInfo.CropW = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.CropY)
    {
        out.mfx.FrameInfo.CropY = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.CropX)
    {
        out.mfx.FrameInfo.CropX = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.Width  % 16 != 0)
    {
        out.mfx.FrameInfo.Width = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.Height % 16 != 0)
    {
        out.mfx.FrameInfo.Height = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.Width > 4096)
    {
        out.mfx.FrameInfo.Width = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.Height > 4096)
    {
        out.mfx.FrameInfo.Height = 0;
        error = true;
    }
    if(!(in.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 /*|| in.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4*/) )
    {
        out.mfx.FrameInfo.FourCC = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
    {
        out.mfx.FrameInfo.ChromaFormat = 0;
        error = true;
    }

    {
        if(!(in.mfx.FrameInfo.BitDepthChroma == 0 || in.mfx.FrameInfo.BitDepthChroma == 8))
        {
            out.mfx.FrameInfo.BitDepthChroma = 0;
            error = true;
        }
        if(!(in.mfx.FrameInfo.BitDepthLuma   == 0 || in.mfx.FrameInfo.BitDepthLuma == 8))
        {
            out.mfx.FrameInfo.BitDepthLuma = 0;
            error = true;
        }
        if(in.mfx.FrameInfo.Shift != 0)
        {
            out.mfx.FrameInfo.Shift = 0;
            error = true;
        }
    }

    if (error)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    width  = in.mfx.FrameInfo.CropW ? in.mfx.FrameInfo.CropW : in.mfx.FrameInfo.Width;
    height = in.mfx.FrameInfo.CropH ? in.mfx.FrameInfo.CropH : in.mfx.FrameInfo.Height;
    if(MFX_FOURCC_NV12 == in.mfx.FrameInfo.FourCC)
        format = DXGI_FORMAT_NV12;

    //mfxRes = CreateVideoAccelerator(width, height, format);
    //if(mfxRes)
    //{
    //    out.mfx.FrameInfo.Width  = out.mfx.FrameInfo.CropW = width;
    //    out.mfx.FrameInfo.Height = out.mfx.FrameInfo.CropH = height;
    //    return MFX_ERR_UNSUPPORTED;
    //}

    if(onInit)
    {
        mfxRes = CreateVideoAccelerator(width, height, format);
        MFX_CHECK_STS(mfxRes);

        mfxRes = CheckCapabilities(m_pD11VideoContext, width, height);
        MFX_CHECK_STS(mfxRes);
    }
    else
    {
        HRESULT hres;
        D3D11_VIDEO_DECODER_DESC video_desc;
        mfxStatus mfxRes = MFX_ERR_NONE;
        mfxHDL hdl;
        CComPtr<ID3D11Device>            pD11Device;
        CComPtr<ID3D11DeviceContext>     pD11Context;

        CComQIPtr<ID3D11VideoDevice>     pD11VideoDevice;
        CComQIPtr<ID3D11VideoContext>    pD11VideoContext;
        CComPtr<ID3D11VideoDecoder>      pDecoder;

        mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&hdl);
        if(mfxRes || !hdl)
            return MFX_ERR_UNSUPPORTED;
        pD11Device = (ID3D11Device*)hdl;
        CComPtr<ID3D11DeviceContext> pImmediateContext;
        pD11Device->GetImmediateContext(&pImmediateContext);
        pD11Context = pImmediateContext;

        pD11VideoDevice  = pD11Device;
        pD11VideoContext = pD11Context;

        video_desc.SampleWidth = width;
        video_desc.SampleHeight = height;
        video_desc.OutputFormat = format;
        video_desc.Guid = DXVADDI_Intel_GetDesktopScreen;

        D3D11_VIDEO_DECODER_CONFIG video_config = {0}; 

        mfxU32 cDecoderProfiles = pD11VideoDevice->GetVideoDecoderProfileCount();
        bool isRequestedGuidPresent = false;


        for (mfxU32 i = 0; i < cDecoderProfiles; i++)
        {
            GUID decoderGuid;
            HRESULT hr = pD11VideoDevice->GetVideoDecoderProfile(i, &decoderGuid);
            if (FAILED(hr))
            {
                continue;
            }

            if (DXVADDI_Intel_GetDesktopScreen == decoderGuid)
                isRequestedGuidPresent = true;
        }

        hres  = pD11VideoDevice->CreateVideoDecoder(&video_desc, &video_config, &pDecoder);
        if (FAILED(hres))
        {
            error = true;
        }
        //w/a for debug. Waiting for HSD5595359. This w/a will cause decoder resource leak.
        #if defined(_DEBUG)
            pDecoder.p->AddRef();
        #endif

        mfxRes = CheckCapabilities(pD11VideoContext, width, height);
        if(mfxRes)
        {
            out.mfx.FrameInfo.Width  = out.mfx.FrameInfo.CropW = 0;
            out.mfx.FrameInfo.Height = out.mfx.FrameInfo.CropH = 0;
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::CreateVideoAccelerator(mfxU16& SampleWidth, mfxU16& SampleHeight, DXGI_FORMAT& OutputFormat)
{
    HRESULT hres;
    D3D11_VIDEO_DECODER_DESC video_desc;
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxHDL hdl;

    mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&hdl);
    m_pD11Device = (ID3D11Device*)hdl;
    CComPtr<ID3D11DeviceContext> pImmediateContext;
    m_pD11Device->GetImmediateContext(&pImmediateContext);
    m_pD11Context = pImmediateContext;

    m_pD11VideoDevice = m_pD11Device;
    m_pD11VideoContext = m_pD11Context;

    video_desc.SampleWidth = SampleWidth;
    video_desc.SampleHeight = SampleHeight;
    video_desc.OutputFormat = OutputFormat;
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
    if(!m_inited)
        return MFX_ERR_NOT_INITIALIZED;

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

    surface_work->Info.CropH = m_CurrentPar.mfx.FrameInfo.Height;
    surface_work->Info.CropW = m_CurrentPar.mfx.FrameInfo.Width;

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
    if (info->Width  > m_CurrentPar.mfx.FrameInfo.Width)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (info->Height > m_CurrentPar.mfx.FrameInfo.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::BeginFrame(mfxMemId MemId, ID3D11VideoDecoderOutputView *pOutputView)
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

mfxStatus MFXScreenCapture_Plugin::GetDesktopScreenOperation(mfxFrameSurface1 *surface_work)
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

mfxStatus MFXScreenCapture_Plugin::CheckCapabilities(CComQIPtr<ID3D11VideoContext>& pD11VideoContext, mfxU16& w, mfxU16& h)
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


    hr = pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);
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

    hr = pD11VideoContext->DecoderExtension(m_pDecoder, &dec_ext);
    if (FAILED(hr))
    {
        delete []desktop_format;
        return MFX_ERR_DEVICE_FAILED;
    }

    for (mfxU32 i= 0; i < count; i++)
    {
        if(desktop_format[i].DesktopFormat == DXGI_FORMAT_NV12)
        {
            if (h <= desktop_format[i].MaxHeight &&
                w <= desktop_format[i].MaxWidth)
            {
                delete []desktop_format;
                return MFX_ERR_NONE;
            }

        }
    }
    
    delete []desktop_format;
    return MFX_ERR_UNSUPPORTED;

}

mfxStatus MFXScreenCapture_Plugin::QueryStatus()
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

mfxStatus MFXScreenCapture_Plugin::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    if(!m_inited)
        return MFX_ERR_NOT_INITIALIZED;

    par->AsyncDepth = m_CurrentPar.AsyncDepth;
    par->IOPattern = m_CurrentPar.IOPattern;
    par->Protected = m_CurrentPar.Protected;
    par->mfx = m_CurrentPar.mfx;

    return MFX_ERR_NONE;

}