/**********************************************************************************

Copyright (C) 2014 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: h263e_plugin.cpp

***********************************************************************************/

#include "h263e_plugin.h"
#include "mfx_plugin_module.h"
#include "mfxvideo++int.h"

#include "umc_media_data.h"
#include "umc_video_data.h"

#if 1
    #define DBG_ENTER
    #define DBG_LEAVE
    #define DBG_LEAVE_STS(sts)
    #define DBG_STS(sts)
    #define DBG_VAL_I(val)
#else
    #define DBG_ENTER \
        { printf("h263: %s:%d: %s: +\n", __FILE__, __LINE__, __FUNCTION__); fflush(NULL); }
    #define DBG_LEAVE \
        { printf("h263: %s:%d: %s: -\n", __FILE__, __LINE__, __FUNCTION__); fflush(NULL); }
    #define DBG_LEAVE_STS(sts) \
        { printf("h263: %s:%d: %s: sts=%d: -\n", __FILE__, __LINE__, __FUNCTION__, sts); fflush(NULL); }
    #define DBG_STS(sts) \
        { printf("h263: %s:%d: %s: sts=%d\n", __FILE__, __LINE__, __FUNCTION__, sts); fflush(NULL); }
    #define DBG_VAL_I(val) \
        { printf("h263: %s:%d: %s: %s=%d\n", __FILE__, __LINE__, __FUNCTION__, #val, val); fflush(NULL); }
#endif

PluginModuleTemplate g_PluginModule = {
    NULL,
    &MFX_H263E_Plugin::Create,
    NULL,
    NULL,
    &MFX_H263E_Plugin::CreateByDispatcher
};

MSDK_PLUGIN_API(MFXEncoderPlugin*) mfxCreateEncoderPlugin() {
    if (!g_PluginModule.CreateEncoderPlugin) {
        return 0;
    }
    return g_PluginModule.CreateEncoderPlugin();
}

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    if (!g_PluginModule.CreatePlugin) {
        return 0;
    }
    return (MFXPlugin*) g_PluginModule.CreatePlugin(uid, plugin);
}

const mfxPluginUID MFX_H263E_Plugin::g_PluginGuid = MFX_PLUGINID_H263E_HW;

MFX_H263E_Plugin::MFX_H263E_Plugin(bool CreateByDispatcher)
    :m_adapter(0)
{
    m_session = 0;
    m_pmfxCore = NULL;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = 1;//MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = 8;//MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
    m_PluginParam.CodecId = MFX_CODEC_H263;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;

    memset(&m_mfxpar, 0, sizeof(mfxVideoParam));
}

mfxStatus MFX_H263E_Plugin::PluginInit(mfxCoreInterface * pCore)
{
    DBG_ENTER;
    if (!pCore)
        return MFX_ERR_NULL_PTR;

    m_pmfxCore = pCore;
    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::PluginClose()
{
    DBG_ENTER;
    if (m_createdByDispatcher) {
        delete this;
    }

    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::GetPluginParam(mfxPluginParam *par)
{
    DBG_ENTER;
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task)
{
    DBG_ENTER;
    MFX_H263ENC::Task* pTask = NULL;
    mfxStatus sts  = MFX_ERR_NONE;

    DBG_VAL_I(bs->MaxLength);
    mfxStatus checkSts = MFX_ERR_NONE/*MFX_H263ENC::CheckEncodeFrameParam(
        m_video,
        ctrl,
        surface,
        bs,
        true)*/;

    MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

    {
        UMC::AutomaticUMCMutex guard(m_taskMutex);
        sts = m_TaskManager.InitTask(surface, bs, pTask);
        MFX_CHECK_STS(sts);
        /*if (ctrl)
            pTask->m_ctrl = *ctrl;*/

    }

    *task = (mfxThreadTask*)pTask;

    DBG_LEAVE_STS(MFX_ERR_NONE);
    return checkSts;
}

mfxStatus MFX_H263E_Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
{
    DBG_ENTER;
    mfxStatus sts = MFX_ERR_NONE;
    MFX_H263ENC::Task *pTask = (MFX_H263ENC::Task*)task;

    MFX_CHECK(pTask->m_status == MFX_H263ENC::TASK_OCCUPIED, MFX_ERR_UNDEFINED_BEHAVIOR);

    UMC::VideoData in;
    UMC::MediaData out;

    if (pTask->m_surface)
    {
        in.Init(m_umc_params.m_Param.Width, m_umc_params.m_Param.Height, UMC::YUV420, 8);
        in.SetPlanePointer(pTask->m_surface->Data.Y, 0);
        in.SetPlanePointer(pTask->m_surface->Data.U, 1);
        in.SetPlanePointer(pTask->m_surface->Data.V, 2);

        in.SetPlanePitch(pTask->m_surface->Data.Pitch, 0);
        in.SetPlanePitch(pTask->m_surface->Data.Pitch/2, 1);
        in.SetPlanePitch(pTask->m_surface->Data.Pitch/2, 2);
        in.SetTime(pTask->m_frame_order/m_umc_params.info.framerate);
    }

    out.SetBufferPointer(pTask->m_bitstream->Data, pTask->m_bitstream->MaxLength);
    DBG_VAL_I(pTask->m_bitstream->MaxLength);

    UMC::Status umc_sts = m_umc_encoder.GetFrame((pTask->m_surface)? &in: NULL, &out);
    DBG_STS(sts);
    sts = (umc_sts == UMC::UMC_OK)? MFX_ERR_NONE: MFX_ERR_UNKNOWN;

    pTask->m_bitstream->DataOffset = 0;
    pTask->m_bitstream->DataLength = out.GetDataSize();
    DBG_VAL_I(pTask->m_bitstream->DataLength)

    {
        UMC::AutomaticUMCMutex guard(m_taskMutex);

        sts = pTask->FreeTask();
        MFX_CHECK_STS(sts);
    }

    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_TASK_DONE;
}

mfxStatus MFX_H263E_Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    task;
    DBG_ENTER;
    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    DBG_ENTER;
    MFX_CHECK_NULL_PTR1(out);

    memcpy(&(out->mfx), &(in->mfx), sizeof(mfxInfoMFX));
    DBG_VAL_I(in->mfx.FrameInfo.Width);
    DBG_VAL_I(in->mfx.FrameInfo.Height);
    out->mfx.BufferSizeInKB = (3 * in->mfx.FrameInfo.Width * in->mfx.FrameInfo.Height / 2 + 999) / 1000;
    out->AsyncDepth = in->AsyncDepth;
    out->IOPattern = in->IOPattern;

    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    DBG_ENTER;
    MFX_CHECK_NULL_PTR2(par,in);

    in->Type = mfxU16((par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)? MFX_H263ENC::MFX_MEMTYPE_SYS_EXT: MFX_H263ENC::MFX_MEMTYPE_D3D_EXT) ;

    in->NumFrameMin =  (par->AsyncDepth ? par->AsyncDepth: 1)  + 1; // default AsyncDepth is 1
    in->NumFrameSuggested = in->NumFrameMin + 1;

    in->Info = par->mfx.FrameInfo;
    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::Init(mfxVideoParam *par)
{
    DBG_ENTER;
    mfxStatus sts  = MFX_ERR_NONE;

    m_video = *par;

    {
        UMC::AutomaticUMCMutex guard(m_taskMutex);

        sts = m_TaskManager.Init(m_pmfxCore, &m_video);
        MFX_CHECK_STS(sts);
    }

    m_umc_params.m_Param.Width = par->mfx.FrameInfo.Width;
    m_umc_params.m_Param.Height = par->mfx.FrameInfo.Height;
    m_umc_params.info.clip_info.width = par->mfx.FrameInfo.Width;
    m_umc_params.info.clip_info.height = par->mfx.FrameInfo.Height;
    m_umc_params.info.framerate = par->mfx.FrameInfo.FrameRateExtN/par->mfx.FrameInfo.FrameRateExtD;

    UMC::Status umc_sts = m_umc_encoder.Init(&m_umc_params);
    DBG_STS(umc_sts);
    sts = (umc_sts == UMC::UMC_OK)? MFX_ERR_NONE: MFX_ERR_UNKNOWN;

    DBG_LEAVE_STS(sts);
    return sts;
}

mfxStatus MFX_H263E_Plugin::Reset(mfxVideoParam *par)
{
    DBG_ENTER;
    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::Close()
{
    DBG_ENTER;
    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::GetVideoParam(mfxVideoParam *par)
{
    DBG_ENTER;
    MFX_CHECK_NULL_PTR1(par);

    memcpy(&(par->mfx), &(m_video.mfx), sizeof(mfxInfoMFX));
    par->AsyncDepth = m_video.AsyncDepth;
    par->IOPattern = m_video.IOPattern;
    DBG_LEAVE_STS(MFX_ERR_NONE);
    return MFX_ERR_NONE;
}
