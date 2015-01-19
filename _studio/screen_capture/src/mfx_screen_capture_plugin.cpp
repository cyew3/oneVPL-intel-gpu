/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_plugin.cpp

\* ****************************************************************************** */

#include <windows.h>
#include <initguid.h>
#include "mfx_screen_capture_plugin.h"
#include "mfx_utils.h"
#include "mfxstructures.h"

namespace MfxCapture
{

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
    //if(MFX_IMPL_VIA_D3D11 != (par.Impl & 0x0F00))
    //    return MFX_ERR_UNSUPPORTED;

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
    if(!m_inited && !m_pCapturer.get())
        return MFX_ERR_NOT_INITIALIZED;

    m_pCapturer.reset(0);
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
    //if(in.mfx.FrameInfo.Width  % 16 != 0)
    //{
    //    out.mfx.FrameInfo.Width = 0;
    //    error = true;
    //}
    //if(in.mfx.FrameInfo.Height % 16 != 0)
    //{
    //    out.mfx.FrameInfo.Height = 0;
    //    error = true;
    //}
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

    if(onInit)
    {
        m_pCapturer.reset( CreatePlatformCapturer(m_pmfxCore) );

        mfxRes = m_pCapturer.get()->CreateVideoAccelerator(in);
        MFX_CHECK_STS(mfxRes);

        mfxRes = m_pCapturer.get()->CheckCapabilities(in, &out);
        MFX_CHECK_STS(mfxRes);
    }
    else
    {
        m_pCapturer.reset( CreatePlatformCapturer(m_pmfxCore) );

        mfxRes = m_pCapturer.get()->QueryVideoAccelerator(in, &out);
        MFX_CHECK_STS(mfxRes);

        m_pCapturer.reset( 0 );
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task)
{
    if(!m_inited || !m_pCapturer.get())
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

    mfxRes = m_pCapturer.get()->BeginFrame(surface_work->Data.MemId);
    MFX_CHECK_STS(mfxRes);

    mfxRes = m_pCapturer.get()->GetDesktopScreenOperation(surface_work, m_StatusReportFeedbackNumber);
    MFX_CHECK_STS(mfxRes);

    mfxRes = m_pCapturer.get()->EndFrame();
    MFX_CHECK_STS(mfxRes);

    surface_work->Info.CropH = m_CurrentPar.mfx.FrameInfo.Height;
    surface_work->Info.CropW = m_CurrentPar.mfx.FrameInfo.Width;

    surface_work->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    *surface_out = surface_work;
    return MFX_ERR_NONE;
    
}

mfxStatus MFXScreenCapture_Plugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    if(!m_inited || !m_pCapturer.get())
        return MFX_ERR_NOT_INITIALIZED;
    uid_p; uid_a;
    mfxStatus mfxRes = MFX_ERR_NONE;

    AsyncParams *pAsyncParam  = (AsyncParams *)task;

    mfxRes = m_pCapturer.get()->QueryStatus(m_StatusList);
    MFX_CHECK_STS(mfxRes);

    //will be enabled after check 
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
    //    mfxRes = m_pCapturer.get()->QueryStatus(m_StatusList);
    //    return mfxRes;
    //}

    mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &pAsyncParam->surface_work->Data);

    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::CheckFrameInfo(mfxFrameInfo *info)
{
    if (info->Width  < m_CurrentPar.mfx.FrameInfo.Width)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (info->Height < m_CurrentPar.mfx.FrameInfo.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

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

}