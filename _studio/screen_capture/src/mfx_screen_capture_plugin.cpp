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

#include "mfx_screen_capture_sw_d3d9.h"
#include "mfx_screen_capture_sw_d3d11.h"

namespace MfxCapture
{

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    return MFXScreenCapture_Plugin::Create();
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    return MFXScreenCapture_Plugin::CreateByDispatcher(uid, plugin);
}

bool inline isSW(const Capturer* capt)
{
    if(!capt) 
        return false;
    if(SW_D3D11 == capt->Mode || SW_D3D9 == capt->Mode)
        return true;
    else
        return false;
}

MFXScreenCapture_Plugin::MFXScreenCapture_Plugin(bool CreateByDispatcher)
{
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));
    memset(&m_CurrentPar,  0, sizeof(mfxVideoParam));
    memset(&m_CurExtPar,   0, sizeof(mfxExtScreenCaptureParam));
    memset(&m_InitPar,     0, sizeof(mfxVideoParam));
    memset(&m_InitExtPar,  0, sizeof(mfxExtScreenCaptureParam));
    memset(&m_OpaqAlloc,   0, sizeof(mfxExtOpaqueSurfaceAlloc));
    memset(&m_response,    0, sizeof(mfxFrameAllocResponse));
    memset(&m_libmfxVer,   0, sizeof(mfxVersion));

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

    m_bDirtyRect = false;
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

    Close();

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
    mfxU32  FourCC;

    const mfxU16& IOPattern = par->IOPattern;
    if(((IOPattern & 0xF0) != (MFX_IOPATTERN_OUT_VIDEO_MEMORY )) &&
       ((IOPattern & 0xF0) != (MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) &&
       ((IOPattern & 0xF0) != (MFX_IOPATTERN_OUT_OPAQUE_MEMORY)))
       return MFX_ERR_INVALID_VIDEO_PARAM;

    if(MFX_FOURCC_NV12 == par->mfx.FrameInfo.FourCC)
        FourCC = MFX_FOURCC_NV12;
    else if(MFX_FOURCC_RGB4 == par->mfx.FrameInfo.FourCC)
    {
        mfxCoreParam param = {0};
        mfxStatus mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &param);
        if (MFX_ERR_NONE != mfxRes)
            return mfxRes;
        if((MFX_IMPL_VIA_D3D11 == (param.Impl & 0x0F00)) && !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
            FourCC = (m_libmfxVer.Minor > 15 ) ? MFX_FOURCC_AYUV_RGB4 : DXGI_FORMAT_AYUV;
        else
            FourCC = MFX_FOURCC_RGB4;
    }
    else if(DXGI_FORMAT_AYUV == par->mfx.FrameInfo.FourCC)
        FourCC = DXGI_FORMAT_AYUV;
    else if(MFX_FOURCC_AYUV_RGB4 == par->mfx.FrameInfo.FourCC)
        FourCC = MFX_FOURCC_AYUV_RGB4;
    else
        return MFX_ERR_UNSUPPORTED;

    out->Info = par->mfx.FrameInfo;
    out->Info.FourCC = FourCC;

    const mfxExtScreenCaptureParam* extPar = GetExtendedBuffer<mfxExtScreenCaptureParam>(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, par);
    const mfxU16 EnableDirtyRect = extPar ? extPar->EnableDirtyRect : 0;

    if (par->AsyncDepth)
    {
        out->NumFrameSuggested = EnableDirtyRect ? (2 * par->AsyncDepth) : par->AsyncDepth;
        out->NumFrameMin = out->NumFrameSuggested;
    }
    else
    {
        out->NumFrameSuggested = EnableDirtyRect ? 2 : 1; //default value
        out->NumFrameMin = out->NumFrameSuggested;
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) 
    {
        out->Type =  MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) 
    {
        out->Type =  MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) 
    {
        out->Type =  MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else //all other types are unsupported
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::Init(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_Guard);

    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    if(m_inited)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    bool fallback = false;

    mfxCoreParam param = {0};
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &param);
    MFX_CHECK_STS(mfxRes);

    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(param.Impl))
    {
        mfxHandleType type = (MFX_IMPL_VIA_D3D11 == (param.Impl & 0x0F00)) ? MFX_HANDLE_D3D11_DEVICE : MFX_HANDLE_D3D9_DEVICE_MANAGER;
        mfxHDL handle = 0;
        mfxRes = m_pmfxCore->CreateAccelerationDevice(m_pmfxCore->pthis, type, &handle);
        switch (mfxRes)
        {
            case MFX_ERR_NONE:
                break;
            case MFX_ERR_NOT_FOUND:
                mfxRes = m_pmfxCore->CreateAccelerationDevice(m_pmfxCore->pthis, type, &handle);
                break;
            default:
                return mfxRes;
        }
        MFX_CHECK_STS(mfxRes);
    }

    mfxRes = QueryMode2(*par, m_InitPar, true);
    if(MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
    {
        mfxRes = MFX_ERR_NONE;
        fallback = true;
    }
    if(MFX_ERR_UNSUPPORTED == mfxRes)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if(MFX_ERR_NONE != mfxRes)
        return mfxRes;

    mfxExtScreenCaptureParam* ExtPar = GetExtendedBuffer<mfxExtScreenCaptureParam>(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, par);
    if(ExtPar)
    {
        m_CurExtPar = m_InitExtPar = *ExtPar;
        if(m_CurExtPar.EnableDirtyRect)
            m_bDirtyRect = true;
        if(m_CurExtPar.DisplayIndex)
        {
            //display selection thrlough getDesktop.ddi is unsupported now
            fallback = true;
        }
    }

    m_CurrentPar = m_InitPar;
    m_inited = true;

    mfxExtOpaqueSurfaceAlloc* in_opaq_buf   = 0;
    if(par->ExtParam || par->NumExtParam)
    {
        if(!par->ExtParam || !par->NumExtParam)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
        else
        {
            for(mfxU32 i = 0; i < par->NumExtParam; ++i)
            {
                if(par->ExtParam[i])
                {
                    if( par->ExtParam[i]->BufferId == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION &&
                        par->ExtParam[i]->BufferSz == sizeof(mfxExtOpaqueSurfaceAlloc))
                    {
                        in_opaq_buf = (mfxExtOpaqueSurfaceAlloc*) par->ExtParam[i];
                        m_OpaqAlloc = *in_opaq_buf;
                    }
                }
            }
        }
    }
    if((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY && !in_opaq_buf) || (par->IOPattern ^ MFX_IOPATTERN_OUT_OPAQUE_MEMORY && in_opaq_buf))
    {
        Close();
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && in_opaq_buf)
    {
        if(!in_opaq_buf->Out.NumSurface || !in_opaq_buf->Out.Surfaces)
        {
            Close();
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        mfxRes = m_pmfxCore->MapOpaqueSurface(m_pmfxCore->pthis, in_opaq_buf->Out.NumSurface, in_opaq_buf->Out.Type, in_opaq_buf->Out.Surfaces);
    }

    if((par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY || m_OpaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && !isSW(m_pCapturer.get()))
    {
        mfxVideoParam tmp_par = *par;
        mfxFrameAllocRequest request = {0};
        memset(&m_response, 0, sizeof(m_response));
        tmp_par.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        mfxRes = QueryIOSurf(&tmp_par, 0, &request);
        if(mfxRes < MFX_ERR_NONE)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
        request.Type = request.Type ^ MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_INTERNAL_FRAME;

        mfxRes = m_pmfxCore->FrameAllocator.Alloc(m_pmfxCore->FrameAllocator.pthis, &request, &m_response);
        if(mfxRes < MFX_ERR_NONE)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
        for(mfxU32 i = 0; i < m_response.NumFrameActual; ++i)
        {
            mfxFrameSurface1 surf = {0};
            surf.Info = tmp_par.mfx.FrameInfo;
            surf.Data.MemId = m_response.mids[i];
            m_SurfPool.push_back(surf);
        }
    }

    if(!m_InitPar.AsyncDepth)
        m_InitPar.AsyncDepth = 1;
    if(!m_InitPar.mfx.FrameInfo.CropH)
        m_InitPar.mfx.FrameInfo.CropH = m_InitPar.mfx.FrameInfo.Height;
    if(!m_InitPar.mfx.FrameInfo.CropW)
        m_InitPar.mfx.FrameInfo.CropW = m_InitPar.mfx.FrameInfo.Width;
    if(!m_InitPar.mfx.FrameInfo.PicStruct)
        m_InitPar.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    if(m_bDirtyRect)
    {
        bool isSystem = false;
        if((par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY || m_OpaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && !isSW(m_pCapturer.get()))
        {
            isSystem = true;
        }
        try
        {
            m_pDirtyRectAnalyzer.reset(new CpuDirtyRectFilter(m_pmfxCore));
        }
        catch(...)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }

        mfxRes = m_pDirtyRectAnalyzer->Init(par, isSystem);
        if(MFX_ERR_NONE != mfxRes)
        {
            Close();
            return mfxRes;
        }

        fallback = true;
    }

    if(fallback && mfxRes >= MFX_ERR_NONE) //fallback status is more important than warning
        mfxRes = MFX_WRN_PARTIAL_ACCELERATION;
    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::Close()
{
    UMC::AutomaticUMCMutex guard(m_Guard);

    if(!m_inited && !m_pCapturer.get())
        return MFX_ERR_NOT_INITIALIZED;
    mfxStatus mfxRes = MFX_ERR_NONE;

    memset(&m_CurrentPar,0,sizeof(m_CurrentPar));
    memset(&m_InitPar,0,sizeof(m_InitPar));

    if(m_OpaqAlloc.Out.NumSurface)
    {
        mfxRes = m_pmfxCore->UnmapOpaqueSurface(m_pmfxCore->pthis, m_OpaqAlloc.Out.NumSurface, m_OpaqAlloc.Out.Type, m_OpaqAlloc.Out.Surfaces);
        memset(&m_OpaqAlloc,0,sizeof(m_OpaqAlloc));
    }

    if(m_response.NumFrameActual)
    {
        mfxRes = m_pmfxCore->FrameAllocator.Free(m_pmfxCore->FrameAllocator.pthis, &m_response);
        memset(&m_response,0,sizeof(m_response));
    }

    if(m_SurfPool.size())
        m_SurfPool.clear();

    if(m_StatusList.size())
        m_StatusList.clear();

    m_StatusReportFeedbackNumber = 0;

    m_pCapturer.reset(0);
    m_pFallbackDXGICapturer.reset(0);
    m_pFallbackD3D9Capturer.reset(0);
    m_inited = false;

    return mfxRes;
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

    mfxCoreParam param = { 0 };
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &param);
    MFX_CHECK_STS(mfxRes);

    if (MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(param.Impl))
    {
        mfxHandleType type = (MFX_IMPL_VIA_D3D11 == (param.Impl & 0x0F00)) ? MFX_HANDLE_D3D11_DEVICE : MFX_HANDLE_D3D9_DEVICE_MANAGER;
        mfxHDL handle = 0;
        mfxRes = m_pmfxCore->GetHandle(m_pmfxCore->pthis, type, &handle);
        switch (mfxRes)
        {
        case MFX_ERR_NONE:
            break;
        case MFX_ERR_NOT_FOUND:
            mfxRes = m_pmfxCore->CreateAccelerationDevice(m_pmfxCore->pthis, type, &handle);
            break;
        default:
            return mfxRes;
        }
        MFX_CHECK_STS(mfxRes);
    }

    //bool warning     = false;
    bool error       = false;
    bool opaque      = false;
    bool fallback    = false;

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
    if(in.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        opaque = true;

    if(in.ExtParam || in.NumExtParam)
    {
        if(!in.ExtParam || !in.NumExtParam)
        {
            error = true;
        }
        else if(&out != &m_InitPar &&
            ((in.NumExtParam != out.NumExtParam) || !out.ExtParam))
        {
            //input and output ext buffers must match
            error = true;
        }
        else
        {
            mfxExtOpaqueSurfaceAlloc* in_opaq_buf = 0;
            mfxExtOpaqueSurfaceAlloc* out_opaq_buf = 0;
            for(mfxU32 i = 0; i < in.NumExtParam; ++i)
            {
                if(in.ExtParam[i])
                {
                    if( in.ExtParam[i]->BufferId == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION &&
                        in.ExtParam[i]->BufferSz == sizeof(mfxExtOpaqueSurfaceAlloc))
                    {
                        if(opaque && !in_opaq_buf)
                        {
                            in_opaq_buf = (mfxExtOpaqueSurfaceAlloc*) in.ExtParam[i];
                        }
                        else
                        {
                            error = true;
                            out.IOPattern = 0;
                        }
                    }
                    else if( in.ExtParam[i]->BufferId == MFX_EXTBUFF_SCREEN_CAPTURE_PARAM &&
                        in.ExtParam[i]->BufferSz == sizeof(mfxExtScreenCaptureParam))
                    {
                        ;//
                    }
                    else
                    {
                        error = true; //unknown ext buffer
                    }
                }
                if(&out != &m_InitPar && out.ExtParam[i])
                {
                    if( out.ExtParam[i]->BufferId == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION &&
                        out.ExtParam[i]->BufferSz == sizeof(mfxExtOpaqueSurfaceAlloc))
                    {
                        if(opaque && !out_opaq_buf)
                        {
                            out_opaq_buf = (mfxExtOpaqueSurfaceAlloc*) out.ExtParam[i];
                        }
                        else
                        {
                            out.IOPattern = 0;
                            error = true;
                        }
                    }
                    else if( out.ExtParam[i]->BufferId == MFX_EXTBUFF_SCREEN_CAPTURE_PARAM &&
                        out.ExtParam[i]->BufferSz == sizeof(mfxExtScreenCaptureParam))
                    {
                        ;//
                    }
                    else
                    {
                        error = true; //unknown ext buffer
                    }
                }
            }
            if(in_opaq_buf)
            {
                mfxStatus mfxSts = CheckOpaqBuffer(in, &out, *in_opaq_buf, out_opaq_buf);
                if(mfxSts)
                    error = true;
            }
        }
    }
    else if (opaque)
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
    if((in.mfx.FrameInfo.CropY + in.mfx.FrameInfo.CropH) > in.mfx.FrameInfo.Height)
    {
        out.mfx.FrameInfo.CropH = 0;
        error = true;
    }
    if((in.mfx.FrameInfo.CropX + in.mfx.FrameInfo.CropW) > in.mfx.FrameInfo.Width)
    {
        out.mfx.FrameInfo.CropW = 0;
        error = true;
    }

    //Crops should be ignored on init
    if(in.mfx.FrameInfo.CropY)
    {
        out.mfx.FrameInfo.CropY = 0;
        //error = true;
    }
    if(in.mfx.FrameInfo.CropX)
    {
        out.mfx.FrameInfo.CropX = 0;
        //error = true;
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
    if(!(in.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 || in.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4 
        || in.mfx.FrameInfo.FourCC == DXGI_FORMAT_AYUV || in.mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV_RGB4) )
    {
        out.mfx.FrameInfo.FourCC = 0;
        error = true;
    }
    if(!(in.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420 || in.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444) )
    {
        out.mfx.FrameInfo.ChromaFormat = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 && in.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
    {
        out.mfx.FrameInfo.ChromaFormat = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4 && in.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        out.mfx.FrameInfo.ChromaFormat = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV_RGB4 && in.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        out.mfx.FrameInfo.ChromaFormat = 0;
        error = true;
    }
    if(in.mfx.FrameInfo.FourCC == DXGI_FORMAT_AYUV && in.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
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

    if(onInit)
    {
        m_DisplayIndex = 0;
        mfxExtScreenCaptureParam* ExtPar = GetExtendedBuffer<mfxExtScreenCaptureParam>(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, &in);
        if(ExtPar)
        {
            m_CurExtPar = m_InitExtPar = *ExtPar;
            if(m_CurExtPar.EnableDirtyRect)
            {
                fallback = true;
                m_bDirtyRect = true;
            }
            if(m_CurExtPar.DisplayIndex)
            {
                //display selection thrlough getDesktop.ddi is unsupported now
                //fallback = true;
                m_DisplayIndex = m_CurExtPar.DisplayIndex;
            }
        }

        m_pCapturer.reset( CreatePlatformCapturer(m_pmfxCore) );
        if(m_pCapturer.get())
            mfxRes = m_pCapturer->CreateVideoAccelerator(in, m_DisplayIndex);
        else
            mfxRes = MFX_ERR_MEMORY_ALLOC;
        if(mfxRes)
        {
            fallback = true;
            m_pCapturer.reset( new(std::nothrow) SW_D3D11_Capturer(m_pmfxCore) );
            if(!m_pCapturer.get())
                return MFX_ERR_MEMORY_ALLOC;
            mfxRes = m_pCapturer->CreateVideoAccelerator(in, m_DisplayIndex);
            if(mfxRes)
            {
                m_pCapturer.reset( new(std::nothrow) SW_D3D9_Capturer(m_pmfxCore) );
                if(!m_pCapturer.get())
                    return MFX_ERR_MEMORY_ALLOC;
                mfxRes = m_pCapturer->CreateVideoAccelerator(in, m_DisplayIndex);
            }
            MFX_CHECK_STS(mfxRes);

            mfxRes = m_pCapturer->CheckCapabilities(in, &out);
        }
        MFX_CHECK_STS(mfxRes);

        if(m_pCapturer.get())
            mfxRes = m_pCapturer->CheckCapabilities(in, &out);
        if(mfxRes)
        {
            fallback = true;
            m_pCapturer.reset( new(std::nothrow) SW_D3D11_Capturer(m_pmfxCore) );
            if(!m_pCapturer.get())
                return MFX_ERR_MEMORY_ALLOC;
            mfxRes = m_pCapturer->CreateVideoAccelerator(in, m_DisplayIndex);
            if(mfxRes)
            {
                m_pCapturer.reset( new(std::nothrow) SW_D3D9_Capturer(m_pmfxCore) );
                if(!m_pCapturer.get())
                    return MFX_ERR_MEMORY_ALLOC;
                mfxRes = m_pCapturer->CreateVideoAccelerator(in, m_DisplayIndex);
            }
            MFX_CHECK_STS(mfxRes);

            mfxRes = m_pCapturer->CheckCapabilities(in, &out);
        }
        MFX_CHECK_STS(mfxRes);

#if defined(ENABLE_RUNTIME_SW_FALLBACK)
        if(SW_D3D11 != m_pCapturer->Mode)
        {
            m_pFallbackDXGICapturer.reset( new(std::nothrow) SW_D3D11_Capturer(m_pmfxCore) );
            if(m_pFallbackDXGICapturer.get())
            {
                mfxRes = m_pFallbackDXGICapturer->CreateVideoAccelerator(in, m_DisplayIndex);
                if(mfxRes)
                    m_pFallbackDXGICapturer.reset( 0 );
                //m_pCapturer->CheckCapabilities(in, &out);
            }
        }

        m_pFallbackD3D9Capturer.reset( new(std::nothrow) SW_D3D9_Capturer(m_pmfxCore) );
        if(m_pFallbackD3D9Capturer.get())
        {
            m_pFallbackD3D9Capturer->CreateVideoAccelerator(in, m_DisplayIndex);
            //m_pCapturer->CheckCapabilities(in, &out);
        }
#endif
    }
    else
    {
        if(!m_inited)
        {
            std::auto_ptr<Capturer> pTmpCapturer;

            pTmpCapturer.reset( CreatePlatformCapturer(m_pmfxCore) );
            if(pTmpCapturer.get())
                mfxRes = pTmpCapturer->QueryVideoAccelerator(in, &out);
            else
                mfxRes = MFX_ERR_DEVICE_FAILED;
            if(mfxRes)
            {
                fallback = true;
            }
            pTmpCapturer.reset( 0 );
        }
        else
        {
            if(m_pCapturer.get())
            {
                mfxRes = m_pCapturer->CheckCapabilities(in, &out);
                if(mfxRes)
                {
                    fallback = true;
                }
            }
        }
    }

    if(fallback)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else
        return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task)
{
    UMC::AutomaticUMCMutex guard(m_Guard);
    mfxStatus mfxRes;

    if(!m_inited || !m_pCapturer.get())
        return MFX_ERR_NOT_INITIALIZED;

    if (bs) // bs should be zero
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (!surface_work || !surface_out)
        return MFX_ERR_NULL_PTR;

    if (surface_work->Data.Locked)
        return MFX_ERR_LOCK_MEMORY;

    mfxRes = m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &surface_work->Data);
    MFX_CHECK_STS(mfxRes);

    mfxRes = CheckFrameInfo(surface_work->Info);
    MFX_CHECK_STS(mfxRes);

    mfxFrameSurface1* real_surface = surface_work;
    if(m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxRes = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, surface_work, &real_surface);
        MFX_CHECK_STS(mfxRes);
        if(!real_surface)    return MFX_ERR_NULL_PTR;
    }
    mfxFrameSurface1* ext_surface = real_surface; //for runtime SW fallback

    if(!isSW(m_pCapturer.get()) &&
       ((m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY && m_OpaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        real_surface = GetFreeInternalSurface();
        if(!real_surface)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        mfxRes = m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &real_surface->Data);
        MFX_CHECK_STS(mfxRes);
    }

    mfxFrameSurface1* pPrevSurf = 0;

    bool rt_fallback_d3d = false;
    bool rt_fallback_dxgi = false;
    mfxRes = DecodeFrameSubmit(real_surface, rt_fallback_d3d, rt_fallback_dxgi, ext_surface);
    if(m_bDirtyRect)
    {
        UMC::AutomaticUMCMutex guard(m_DirtyRectGuard);
        if(!m_pPrevSurface)
        {
            m_pPrevSurface = surface_work;
            return MFX_ERR_MORE_SURFACE;
        }
        else
        {
            pPrevSurf = m_pPrevSurface;
            m_pPrevSurface = surface_work;
        }
    }

    if (MFX_ERR_NONE <= mfxRes)
    {
        //surface_work->Info.CropH = m_CurrentPar.mfx.FrameInfo.Height;
        //surface_work->Info.CropW = m_CurrentPar.mfx.FrameInfo.Width;
        surface_work->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        AsyncParams *pAsyncParam = new(std::nothrow) AsyncParams;
        if(!pAsyncParam) return MFX_ERR_MEMORY_ALLOC;
        //pAsyncParam->surface_work = real_surface;
        pAsyncParam->surface_out =  surface_work;
        pAsyncParam->real_surface = real_surface;
        pAsyncParam->dirty_rect_surface = pPrevSurf;
        pAsyncParam->StatusReportFeedbackNumber = m_StatusReportFeedbackNumber;
        pAsyncParam->rt_fallback_dxgi = rt_fallback_dxgi;
        pAsyncParam->rt_fallback_d3d = rt_fallback_d3d;
        *task = (mfxThreadTask*)pAsyncParam;
        surface_work->Data.TimeStamp = (mfxU64) MFX_TIMESTAMP_UNKNOWN;
        surface_work->Data.Corrupted = 0;
        surface_work->Data.DataFlag = 0;
        *surface_out = surface_work;
    }

    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::DecodeFrameSubmit(mfxFrameSurface1 *surface, bool& rt_fallback_d3d, bool& rt_fallback_dxgi, mfxFrameSurface1 *ext_surface)
{
    mfxStatus mfxRes;
    CComPtr<ID3D11VideoDecoderOutputView> pOutputView;
    //bool wrn = false;

    //HW accelerated
    try
    {
        mfxRes = m_pCapturer->BeginFrame(surface);
        if(mfxRes)
            rt_fallback_dxgi = true;

        if(!rt_fallback_dxgi)
        {
            mfxRes = m_pCapturer->GetDesktopScreenOperation(surface, m_StatusReportFeedbackNumber);
            if(mfxRes)
                rt_fallback_dxgi = true;
        }

        mfxRes = m_pCapturer->EndFrame();
        if(mfxRes)
            rt_fallback_dxgi = true;
    }
    catch(...)
    {
        rt_fallback_dxgi = true;
    }

    if(rt_fallback_dxgi)
    {
#if defined(ENABLE_RUNTIME_SW_FALLBACK)
        if(!m_pFallbackDXGICapturer.get())
        {
            rt_fallback_d3d = true;
        }
        else
        {
            try
            {
                mfxRes = m_pFallbackDXGICapturer->BeginFrame(ext_surface);
                if(mfxRes)
                    rt_fallback_d3d = true;

                if(!rt_fallback_d3d)
                {
                    mfxRes = m_pFallbackDXGICapturer->GetDesktopScreenOperation(ext_surface, m_StatusReportFeedbackNumber);
                    if(mfxRes)
                        rt_fallback_d3d = true;
                }

                mfxRes = m_pFallbackDXGICapturer->EndFrame();
                if(mfxRes)
                    rt_fallback_d3d = true;
            }
            catch(...)
            {
                rt_fallback_d3d = true;
            }
        }
#else
        ext_surface;
        return MFX_ERR_DEVICE_FAILED;
#endif
    }

    if(rt_fallback_d3d)
    {
#if defined(ENABLE_RUNTIME_SW_FALLBACK)
        if(!m_pFallbackD3D9Capturer.get())
            return MFX_ERR_DEVICE_FAILED;

        rt_fallback_dxgi = false;

        mfxRes = m_pFallbackD3D9Capturer->BeginFrame(ext_surface);
        MFX_CHECK_STS(mfxRes);

        mfxRes = m_pFallbackD3D9Capturer->GetDesktopScreenOperation(ext_surface, m_StatusReportFeedbackNumber);
        if(MFX_ERR_NONE < mfxRes)
        {
            //wrn = true;
            mfxRes = MFX_ERR_NONE;
        }
        MFX_CHECK_STS(mfxRes);

        mfxRes = m_pFallbackD3D9Capturer->EndFrame();
        MFX_CHECK_STS(mfxRes);
#else
        ext_surface;
        return MFX_ERR_DEVICE_FAILED;
#endif
    }

    //if(wrn)
    //    return MFX_WRN_VIDEO_PARAM_CHANGED;
    //else
        return MFX_ERR_NONE;
}

mfxStatus MFXScreenCapture_Plugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    if(!m_inited || !m_pCapturer.get())
        return MFX_ERR_NOT_INITIALIZED;
    uid_p; uid_a;
    mfxStatus mfxRes = MFX_ERR_NONE;

    AsyncParams *pAsyncParam  = (AsyncParams *)task;

    if(pAsyncParam->rt_fallback_dxgi)
    {
        mfxRes = m_pFallbackDXGICapturer->QueryStatus(m_StatusList);
        MFX_CHECK_STS(mfxRes);
    }
    else if(pAsyncParam->rt_fallback_d3d)
    {
        mfxRes = m_pFallbackD3D9Capturer->QueryStatus(m_StatusList);
        MFX_CHECK_STS(mfxRes);
    }
    else
    {
        mfxRes = m_pCapturer->QueryStatus(m_StatusList);
        MFX_CHECK_STS(mfxRes);
    }

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
    //    mfxRes = m_pCapturer->QueryStatus(m_StatusList);
    //    return mfxRes;
    //}
    mfxFrameSurface1* real_output = pAsyncParam->surface_out;

    if( (!isSW(m_pCapturer.get()) && !(pAsyncParam->rt_fallback_dxgi || pAsyncParam->rt_fallback_d3d)) &&
        ((m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY && m_OpaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        if(m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        {
            mfxRes = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, pAsyncParam->surface_out, &real_output);
            MFX_CHECK_STS(mfxRes);
            if(!real_output)    return MFX_ERR_NULL_PTR;
        }

        mfxRes = m_pmfxCore->CopyFrame(m_pmfxCore->pthis, real_output, pAsyncParam->real_surface);
        MFX_CHECK_STS(mfxRes);
        mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &pAsyncParam->real_surface->Data);
        MFX_CHECK_STS(mfxRes);
    }

    if( (!isSW(m_pCapturer.get()) && (pAsyncParam->rt_fallback_dxgi || pAsyncParam->rt_fallback_d3d)) &&
        ((m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (m_CurrentPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY && m_OpaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &pAsyncParam->real_surface->Data);
        MFX_CHECK_STS(mfxRes);
    }

    if(m_bDirtyRect)
    {
        mfxFrameSurface1* surface_work = pAsyncParam->surface_out;

        mfxExtDirtyRect* extDirtyRect = 0;
        if(m_bDirtyRect)
            extDirtyRect = GetExtendedBuffer<mfxExtDirtyRect>(MFX_EXTBUFF_DIRTY_RECTANGLES, surface_work);
        if(!pAsyncParam->dirty_rect_surface)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        else
        {
            if(extDirtyRect)
            {
                m_pDirtyRectAnalyzer->RunFrameVPP(*pAsyncParam->dirty_rect_surface, *surface_work);
            }
            //Decoder-like behavior. Application must not change the previous surface.
            mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &pAsyncParam->dirty_rect_surface->Data);
            MFX_CHECK_STS(mfxRes);
        }
    }
    else
    {
        mfxRes = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &pAsyncParam->surface_out->Data);
    }

    return mfxRes;
}

mfxStatus MFXScreenCapture_Plugin::CheckFrameInfo(const mfxFrameInfo& info)
{
    if (info.Width  < m_CurrentPar.mfx.FrameInfo.Width)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (info.Height < m_CurrentPar.mfx.FrameInfo.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if ((MFX_FOURCC_NV12 == m_CurrentPar.mfx.FrameInfo.FourCC) && (MFX_FOURCC_NV12 != info.FourCC))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if ( (MFX_FOURCC_RGB4 == m_CurrentPar.mfx.FrameInfo.FourCC) && 
        !(MFX_FOURCC_RGB4 == info.FourCC || DXGI_FORMAT_AYUV == info.FourCC || MFX_FOURCC_AYUV_RGB4 == info.FourCC)
       )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (info.ChromaFormat < m_CurrentPar.mfx.FrameInfo.ChromaFormat)
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

    mfxExtScreenCaptureParam* extPar = GetExtendedBuffer<mfxExtScreenCaptureParam>(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, par);
    if(extPar)
    {
        extPar->DisplayIndex        = m_CurExtPar.DisplayIndex       ;
        extPar->EnableDirtyRect     = m_CurExtPar.EnableDirtyRect    ;
        extPar->EnableCursorCapture = m_CurExtPar.EnableCursorCapture;
        memcpy_s(&extPar->reserved,sizeof(m_CurExtPar.reserved),&m_CurExtPar.reserved,sizeof(m_CurExtPar.reserved));
    }

    return MFX_ERR_NONE;
}

mfxFrameSurface1* MFXScreenCapture_Plugin::GetFreeInternalSurface()
{
    if(!m_SurfPool.size())
        return 0;
    mfxFrameSurface1* pSurf = 0; 

    for(std::list<mfxFrameSurface1>::iterator it = m_SurfPool.begin(); it != m_SurfPool.end(); ++it)
    {
        if( !(*it).Data.Locked)
        {
            pSurf = &(*it);
            break;
        }
    }
    return pSurf;
}

mfxStatus MFXScreenCapture_Plugin::CheckOpaqBuffer(const mfxVideoParam& par, mfxVideoParam* pParOut, const mfxExtOpaqueSurfaceAlloc& opaqAlloc, mfxExtOpaqueSurfaceAlloc* pOpaqAllocOut )
{
    bool error = false;
    if(par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        error = true;
        if(pParOut)
            pParOut->IOPattern = 0;
    }

    if( par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY )
    {
        if ( (!(opaqAlloc.Out.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) && !(opaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY)) ||
             (opaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (opaqAlloc.Out.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET))     ||
             !(opaqAlloc.Out.Type & MFX_MEMTYPE_OPAQUE_FRAME)
           )
        {
            if(pParOut)
                pParOut->IOPattern = 0;
            if(pOpaqAllocOut)
                pOpaqAllocOut->Out.Type = 0;
            error = true;
        }
    }

    if(error)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else
        return MFX_ERR_NONE;
} // mfxStatus CheckOpaqMode

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxVideoParam* par)
{
    if(!par || !BufferId)
        return 0;
    if(!par->NumExtParam || !par->ExtParam)
        return 0;
    for(mfxU32 i = 0; i < par->NumExtParam; ++i)
    {
        if(!par->ExtParam[i])
            continue;
        if(BufferId == par->ExtParam[i]->BufferId && sizeof(T) == par->ExtParam[i]->BufferSz)
            return (T*) par->ExtParam[i];
    }

    return 0;
}

}

