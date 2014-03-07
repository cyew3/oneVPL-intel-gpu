//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#include <stdio.h>
#include "ffmpeg_decoder_plugin.h"
#include <iostream>
#include "ffmpeg_port.h"
#include "mfx_plugin_module.h"
#include <algorithm>


// disable "unreferenced formal parameter" warning -
// not all formal parameters of interface functions will be used by sample plugin
#pragma warning(disable : 4100)

namespace {
    const mfxU32 FF_CODEC_H263 = MFX_MAKEFOURCC('h','2','6','3');
    const mfxU32 FF_CODEC_VP8 = MFX_MAKEFOURCC('V','P','8',' ');
    FFPortable g_ffPort;
}

//defining module template for decoder plugin
#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
    FFDecPlugin::Create,
    NULL,
    NULL
};


FFDecPlugin::FFDecPlugin()
    : m_bInited(false)
    , m_bIsOpaq(false)
    , m_VideoParam()
    , m_ffDecoder(false)
    , m_MaxNumTasks()
{
    mfxPluginUID uid = {0xA3, 0xD3, 0x8D, 0x30, 0xCE, 0xB0, 0x42, 0xCB, 0x8B, 0x01, 0x87, 0xB4, 0x9A, 0x83, 0xBF, 0x3A};
    m_PluginParam.reset(new MFXPluginParam(FF_CODEC_VP8, 0, uid));
}

FFDecPlugin::~FFDecPlugin()
{
    Close();
}

/* Methods required for integration with Media SDK */
mfxStatus FFDecPlugin::PluginInit(mfxCoreInterface *core)
{
    MSDK_CHECK_POINTER(core, MFX_ERR_NULL_PTR);

    m_mfxCore = MFXCoreInterface(*core);

    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::PluginClose()
{
    Close();
    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::GetPluginParam(mfxPluginParam *par)
{
    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);

    *par = *m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task)
{
    MSDK_CHECK_POINTER(surface_work, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(surface_out, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(task, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    mfxFrameSurface1 *real_surface_work = surface_work;
    *surface_out = surface_work;

    mfxStatus sts = MFX_ERR_NONE;

    if (m_Tasks.size() == m_MaxNumTasks)
    {
        return MFX_WRN_DEVICE_BUSY; // currently there are no free tasks available according to async depth
    }

    if (m_bIsOpaq)
    {
        sts = m_mfxCore.GetRealSurface(surface_work, &real_surface_work);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    m_Tasks.push_back(FFDecWorkload(g_ffPort, &m_ffDecoder, &m_ffParser, &m_scl, m_mfxCore, bs, real_surface_work));

    *task = (mfxThreadTask)&m_Tasks.back();

#if !IS_PLUGIN_ASYNC()
    sts = m_Tasks.back()();
    //TODO: who is responsible to unqueue of task if err_more_data
    if (sts < MFX_ERR_NONE) {
        FreeResources(*task, sts);
    }
#endif

    return sts;
}

mfxStatus FFDecPlugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    FFDecWorkload &current_task = *(FFDecWorkload *)task;

    return current_task();
}

mfxStatus FFDecPlugin::FreeResources(mfxThreadTask task, mfxStatus sts)
{
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    if (m_Tasks.empty()) {
        return MFX_ERR_UNKNOWN;
    }
    FFDecWorkload *pDecTask = (FFDecWorkload *)task;

    for (std::list<FFDecWorkload>::iterator i = m_Tasks.begin(); i != m_Tasks.end(); i++)
    {
        //pointer to task
        if (&*i == pDecTask) {
            m_Tasks.erase(i);
            return MFX_ERR_NONE;
        }
    }

    MSDK_TRACE_ERROR("[FFDecPlugin::FreeResources] Task ptr=0x"<<std::hex<<pDecTask<<", not found");
    return MFX_ERR_UNKNOWN;
}


mfxStatus FFDecPlugin::CheckParam(mfxVideoParam *mfxParam)
{
    MSDK_CHECK_POINTER(mfxParam, MFX_ERR_NULL_PTR);

    mfxFrameInfo &param = mfxParam->mfx.FrameInfo;

    // only NV12 color format is supported
    if (MFX_FOURCC_NV12 != param.FourCC &&
        MFX_FOURCC_YV12 != param.FourCC )
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::CheckInOutFrameInfo(mfxFrameInfo *pIn, mfxFrameInfo *pOut)
{
    MSDK_CHECK_POINTER(pIn, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pOut, MFX_ERR_NULL_PTR);

    if (pIn->CropW != m_VideoParam.vpp.In.CropW || pIn->CropH != m_VideoParam.vpp.In.CropH ||
        pIn->FourCC != m_VideoParam.vpp.In.FourCC ||
        pOut->CropW != m_VideoParam.vpp.Out.CropW || pOut->CropH != m_VideoParam.vpp.Out.CropH ||
        pOut->FourCC != m_VideoParam.vpp.Out.FourCC)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}


mfxStatus FFDecPlugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MSDK_TRACE_INFO("FFDecPlugin::Query");
    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    MSDK_TRACE_INFO("FFDecPlugin::QueryIOSurf");

    //ffmpeg use its internal surfaces allocation, so we need at least one surface
    out->NumFrameMin = (std::max)((mfxU16)1, (std::min)(m_VideoParam.AsyncDepth, (mfxU16)6));
    out->NumFrameSuggested = out->NumFrameMin;
    out->Info = par->mfx.FrameInfo;


    //request->Type =
    mfxCoreParam core_par;
    mfxStatus sts = m_mfxCore.GetCoreParam(&core_par);
    if (MFX_ERR_NONE != sts)
    {
        MSDK_TRACE_ERROR(MSDK_STRING("FFDecPlugin::QueryIOSurf(), m_pmfxCore->GetCoreParam() returned sts=") << sts);
        return sts;
    }

    mfxI32 isInternalManaging = (core_par.Impl & MFX_IMPL_SOFTWARE) ?
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging) {
        if (core_par.Impl & MFX_IMPL_SOFTWARE)
            out->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        else
            out->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;

    } else {
        if (core_par.Impl & MFX_IMPL_HARDWARE)
            out->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        else
            out->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }


    if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        out->Type |= MFX_MEMTYPE_OPAQUE_FRAME;
    }
    else
    {
        out->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    return MFX_ERR_NONE;
}
mfxStatus FFDecPlugin::Init(mfxVideoParam *mfxParam)
{
    MSDK_TRACE_INFO("FFDecPlugin::Init");

    MSDK_CHECK_POINTER(mfxParam, MFX_ERR_NULL_PTR);
    if (!m_mfxCore.IsCoreSet()) {
        MSDK_TRACE_ERROR("FFDecPlugin::Init - IsCoreSet() = false");
        return MFX_ERR_NULL_PTR;
    }

    // check validity of parameters
    mfxStatus sts = CheckParam(mfxParam);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    m_VideoParam = *mfxParam;

    // map opaque surfaces array in case of opaque surfaces
    m_bIsOpaq = (m_VideoParam.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ? true : false;
    mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;

    if (m_bIsOpaq)
    {
        pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer(m_VideoParam.ExtParam,
            m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // check existence of corresponding allocs
    if (m_bIsOpaq && !pluginOpaqueAlloc->Out.Surfaces)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_bIsOpaq)
    {
        sts = m_mfxCore.MapOpaqueSurface(pluginOpaqueAlloc->Out.NumSurface,
            pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    m_MaxNumTasks = m_VideoParam.AsyncDepth;
    if (m_MaxNumTasks < 1) m_MaxNumTasks = 1;


    sts = g_ffPort.InitCodec(m_ffDecoder, mfxParam->mfx.CodecId);
    if (MFX_ERR_NONE != sts) {
        MSDK_TRACE_ERROR("FFPortable::InitCodec failure = " << sts);
        return sts;
    }
    sts = g_ffPort.OpenCodec(m_ffDecoder);
    if (MFX_ERR_NONE != sts) {
        MSDK_TRACE_ERROR("FFPortable::OpenCodec failure = " << sts);
        return sts;
    }

    sts = g_ffPort.OpenParser(m_ffParser, mfxParam->mfx.CodecId);
    if (MFX_ERR_NONE != sts) {
        MSDK_TRACE_ERROR("FFPortable::OpenParser failure = " << sts);
        return sts;
    }

    m_bInited = true;

    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::Reset(mfxVideoParam *par)
{
    MSDK_TRACE_INFO("FFDecPlugin::Reset");
    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::Close()
{
    MSDK_TRACE_INFO("FFDecPlugin::Close");
    if (!m_bInited)
        return MFX_ERR_NONE;

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;

    if (m_bIsOpaq)
    {
        pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)
            GetExtBuffer(m_VideoParam.ExtParam, m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // check existence of corresponding allocs
    if ((m_bIsOpaq && !pluginOpaqueAlloc->Out.Surfaces))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_mfxCore.IsCoreSet()) {
        MSDK_TRACE_ERROR("FFDecPlugin::Close - IsCoreSet() = false");
        return MFX_ERR_NULL_PTR;
    }

    if (m_bIsOpaq)
    {
        sts = m_mfxCore.UnmapOpaqueSurface(pluginOpaqueAlloc->Out.NumSurface,
            pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    g_ffPort.ReleaseCodec(m_ffDecoder);
    g_ffPort.ReleaseParser(m_ffParser);

    m_Tasks.clear();

    m_bInited = false;

    return MFX_ERR_NONE;
}
mfxStatus FFDecPlugin::GetVideoParam(mfxVideoParam *par)
{
    MSDK_TRACE_INFO("FFDecPlugin::GetVideoParam");
    return MFX_ERR_NONE;
}

mfxStatus FFDecPlugin::DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
{
    MSDK_TRACE_INFO("FFDecPlugin::DecodeHeader");

    FFCodec codec(false);
    mfxStatus sts = g_ffPort.InitCodec(codec, par->mfx.CodecId);
    if (MFX_ERR_NONE != sts) {
        MSDK_TRACE_ERROR("FFPortable::InitCodec failure = " << sts);
        return sts;
    }
    sts = g_ffPort.OpenCodec(codec);
    if (MFX_ERR_NONE != sts) {
        MSDK_TRACE_ERROR("FFPortable::OpenCodec failure = " << sts);
        return sts;
    }

    mfxBitstream bs_orig = *bs;
    mfxFrameSurface1 srf = {0};
    sts = g_ffPort.DecodeFrame(codec, *bs, srf);

    g_ffPort.ReleaseCodec(codec);

    //decode frame obviously will adjust offset at least
    *bs = bs_orig;

    if (sts == MFX_ERR_NONE) {
        par->mfx.FrameInfo = srf.Info;
    }
    return sts;
}


