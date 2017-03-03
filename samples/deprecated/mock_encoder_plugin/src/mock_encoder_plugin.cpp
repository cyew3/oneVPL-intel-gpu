/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include <stdio.h>
#include "mock_encoder_plugin.h"
#include <iostream>


// disable "unreferenced formal parameter" warning -
// not all formal parameters of interface functions will be used by sample plugin
#pragma warning(disable : 4100)

namespace {
    const mfxU32 CODEC_H265 = MFX_MAKEFOURCC('H','E','V','C');
}

//defining module template for decoder plugin
#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
    NULL,
    MockEncoderPlugin::Create,
    NULL,
    MockEncoderPlugin::CreateByDispatcher
};

const mfxPluginUID MockEncoderPlugin::g_mockEncoderGuid = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
std::auto_ptr<MockEncoderPlugin> MockEncoderPlugin::g_singlePlg;
std::auto_ptr<MFXPluginAdapter<MFXEncoderPlugin> > MockEncoderPlugin::g_adapter;

MockEncoderPlugin::MockEncoderPlugin(bool createdByDispatcher)
    : m_MaxNumTasks(6)
    , m_PluginParam(CODEC_H265, MFX_PLUGINTYPE_VIDEO_ENCODE, g_mockEncoderGuid)
    , m_bIsOpaq()
    , m_bInited()
    , m_createdByDispatcher(createdByDispatcher)
{
}

MockEncoderPlugin::~MockEncoderPlugin()
{
    Close();
}

/* Methods required for integration with Media SDK */
mfxStatus MockEncoderPlugin::PluginInit(mfxCoreInterface *core)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::PluginInit");
    MSDK_CHECK_POINTER(core, MFX_ERR_NULL_PTR);
    m_mfxCore = MFXCoreInterface(*core);
    return MFX_ERR_NONE;
}

mfxStatus MockEncoderPlugin::PluginClose()
{
    MSDK_TRACE_INFO("MockEncoderPlugin::PluginClose");
    Close();

    if (m_createdByDispatcher) {
        g_singlePlg.reset(0);
        g_adapter.reset(0);
    }

    return MFX_ERR_NONE;
}

mfxStatus MockEncoderPlugin::GetPluginParam(mfxPluginParam *par)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::GetPluginParam");
    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);

    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus MockEncoderPlugin::EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task)
{
    MSDK_CHECK_POINTER(surface, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(bs, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    if (m_Tasks.size() == m_MaxNumTasks)
    {
        MSDK_TRACE_INFO("MockEncoderPlugin::device busy");
        return MFX_WRN_DEVICE_BUSY; // currently there are no free tasks available according to async depth
    }

    m_Tasks.push_back(MockEncoderPluginTask());

    *task = (mfxThreadTask)&m_Tasks.back();
    MSDK_TRACE_INFO("MockEncoderPlugin::EncodeFrameSubmit task=" << *task);

    return sts;
}

mfxStatus MockEncoderPlugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    MockEncoderPluginTask &current_task = *(MockEncoderPluginTask *)task;

    return current_task();
}

mfxStatus MockEncoderPlugin::FreeResources(mfxThreadTask task, mfxStatus sts)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::FreeResources");

    if (m_Tasks.empty()) {
        return MFX_ERR_UNKNOWN;
    }

    MockEncoderPluginTask *pEncTask = (MockEncoderPluginTask *)task;

    for (std::list<MockEncoderPluginTask>::iterator i = m_Tasks.begin(); i != m_Tasks.end(); i++)
    {
        //pointer to task
        if (&*i == pEncTask) {
            m_Tasks.erase(i);
            return MFX_ERR_NONE;
        }
    }

    MSDK_TRACE_ERROR("[MockEncoderPlugin::FreeResources] Task ptr=0x"<<std::hex<<pEncTask<<", not found");
    return MFX_ERR_UNKNOWN;
}


mfxStatus MockEncoderPlugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::Query");
    return MFX_ERR_NONE;
}

mfxStatus MockEncoderPlugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request, mfxFrameAllocRequest *out)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::QueryIOSurf");

    //ffmpeg use its internal surfaces allocation, so we need at least one surface
    request->NumFrameMin = 3;
    request->NumFrameSuggested = request->NumFrameMin;
    request->Info = par->mfx.FrameInfo;


    //request->Type =
    mfxCoreParam core_par;
    mfxStatus sts = m_mfxCore.GetCoreParam(&core_par);
    if (MFX_ERR_NONE != sts)
    {
        MSDK_TRACE_ERROR(MSDK_STRING("MockEncoderPlugin::QueryIOSurf(), m_pmfxCore->GetCoreParam() returned sts=") << sts);
        return sts;
    }

    mfxI32 isInternalManaging = (core_par.Impl & MFX_IMPL_SOFTWARE) ?
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging) {
        if (core_par.Impl & MFX_IMPL_SOFTWARE)
            request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_ENCODE;
        else
            request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_ENCODE;

    } else {
        if (core_par.Impl & MFX_IMPL_HARDWARE)
            request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_ENCODE;
        else
            request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_ENCODE;
    }


    if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        request->Type |= MFX_MEMTYPE_OPAQUE_FRAME;
    }
    else
    {
        request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    return MFX_ERR_NONE;
}
mfxStatus MockEncoderPlugin::Init(mfxVideoParam *mfxParam)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::Init");

    MSDK_CHECK_POINTER(mfxParam, MFX_ERR_NULL_PTR);
    if (!m_mfxCore.IsCoreSet()) {
        MSDK_TRACE_ERROR("MockEncoderPlugin::Init - IsCoreSet() = false");
        return MFX_ERR_NULL_PTR;
    }

    mfxStatus sts ;

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
        MSDK_CHECK_STATUS(sts, "m_mfxCore.MapOpaqueSurface failed");
    }

    m_MaxNumTasks = m_VideoParam.AsyncDepth;
    if (m_MaxNumTasks < 1) m_MaxNumTasks = 1;


    m_bInited = true;

    return MFX_ERR_NONE;
}

mfxStatus MockEncoderPlugin::Reset(mfxVideoParam *par)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::Reset");
    return MFX_ERR_NONE;
}

mfxStatus MockEncoderPlugin::Close()
{
    MSDK_TRACE_INFO("MockEncoderPlugin::Close");
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
        MSDK_TRACE_ERROR("MockEncoderPlugin::Close - IsCoreSet() = false");
        return MFX_ERR_NULL_PTR;
    }

    if (m_bIsOpaq)
    {
        sts = m_mfxCore.UnmapOpaqueSurface(pluginOpaqueAlloc->Out.NumSurface,
            pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
        MSDK_CHECK_STATUS(sts, "m_mfxCore.UnmapOpaqueSurface failed");
    }

    m_Tasks.clear();

    m_bInited = false;

    return MFX_ERR_NONE;
}

mfxStatus MockEncoderPlugin::GetVideoParam(mfxVideoParam *par)
{
    MSDK_TRACE_INFO("MockEncoderPlugin::GetVideoParam");
    return MFX_ERR_NONE;
}


