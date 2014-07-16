/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "people_detector_plugin.h"

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin)
{
    return (MFXPlugin*) PeopleDetectionPlugin::CreatePeopleDetector(uid, plugin);
}

PeopleDetectionPlugin::PeopleDetectionPlugin() :
    m_bInited(false),
    m_pTasks(NULL),
    m_bIsInOpaque(false),
    m_bIsOutOpaque(false),
    m_session(0),
    m_MaxNumTasks(0)
{
    MSDK_ZERO_MEMORY(m_VideoParam);
    MSDK_ZERO_MEMORY(m_PluginParam);
    MSDK_ZERO_MEMORY(m_Param);

    m_Param.Treshold = 200.0f;
    m_Param.RenderFlag = MFX_RENDER_ALWAYS;

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_PeopleDetection_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_VPP;
    m_PluginParam.PluginVersion = 1;
}

PeopleDetectionPlugin::~PeopleDetectionPlugin()
{
    PluginClose();
}

mfxStatus PeopleDetectionPlugin::PluginInit(mfxCoreInterface *core)
{
    MSDK_CHECK_POINTER(core, MFX_ERR_NULL_PTR);
    m_mfxCore = MFXCoreInterface(*core);
    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::PluginClose()
{
    Close();
    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::GetPluginParam(mfxPluginParam *par)
{
    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);

    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    PeopleDetectorTask *current_task = (PeopleDetectorTask *)task;

    sts = current_task->pProcessor->Process();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus PeopleDetectionPlugin::FreeResources(mfxThreadTask task, mfxStatus sts)
{
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    PeopleDetectorTask *current_task = (PeopleDetectorTask *)task;

    m_mfxCore.DecreaseReference(&(current_task->In->Data));
    m_mfxCore.DecreaseReference(&(current_task->Out->Data));

    MSDK_SAFE_DELETE(current_task->pProcessor);
    current_task->bBusy = false;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::Init(mfxVideoParam *mfxParam)
{
    MSDK_CHECK_POINTER(mfxParam, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    m_VideoParam = *mfxParam;

    mfxInfoVPP *pParam = &mfxParam->vpp;

//only NV12 color format is supported now
    if (MFX_FOURCC_NV12 != pParam->In.FourCC || MFX_FOURCC_NV12 != pParam->Out.FourCC)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    if (!(mfxParam->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    for(int bufferIndex = 0; bufferIndex < mfxParam->NumExtParam; bufferIndex++ )
    {
        if (mfxParam->ExtParam[bufferIndex]->BufferId == (mfxU32)VA_EXTBUFF_VPP_PEOPLE_DETECTOR)
        {
            vaExtVPPDetectPeople* tmpPar = (vaExtVPPDetectPeople*) mfxParam->ExtParam[bufferIndex];
            m_Param.Treshold = tmpPar->Treshold;
            if (m_Param.Treshold < 0)
            {
                return MFX_ERR_UNSUPPORTED;
            }
        }
    }

    // map opaque surfaces array in case of opaque surfaces
    m_bIsInOpaque = (m_VideoParam.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) ? true : false;
    m_bIsOutOpaque = (m_VideoParam.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ? true : false;
    mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;

    if (m_bIsInOpaque || m_bIsOutOpaque)
    {
        pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer(m_VideoParam.ExtParam,
            m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // check existence of corresponding allocs
    if ((m_bIsInOpaque && ! pluginOpaqueAlloc->In.Surfaces) || (m_bIsOutOpaque && !pluginOpaqueAlloc->Out.Surfaces))
       return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_bIsInOpaque)
    {
        sts = m_mfxCore.MapOpaqueSurface(pluginOpaqueAlloc->In.NumSurface,
            pluginOpaqueAlloc->In.Type, pluginOpaqueAlloc->In.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    if (m_bIsOutOpaque)
    {
        sts = m_mfxCore.MapOpaqueSurface(pluginOpaqueAlloc->Out.NumSurface,
            pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    m_MaxNumTasks = m_VideoParam.AsyncDepth;
    if (m_MaxNumTasks < 2) m_MaxNumTasks = 2;

    m_pTasks = new PeopleDetectorTask [m_MaxNumTasks];
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);
    MSDK_ZERO_MEMORY(*m_pTasks);

    m_bInited = true;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::Close()
{
    if (!m_bInited)
        return MFX_ERR_NONE;

    MSDK_SAFE_DELETE_ARRAY(m_pTasks);

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;

    if (m_bIsInOpaque || m_bIsOutOpaque)
    {
        pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)
            GetExtBuffer(m_VideoParam.ExtParam, m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    if ((m_bIsInOpaque && ! pluginOpaqueAlloc->In.Surfaces) || (m_bIsOutOpaque && !pluginOpaqueAlloc->Out.Surfaces))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_bIsInOpaque)
    {
        sts = m_mfxCore.UnmapOpaqueSurface(pluginOpaqueAlloc->In.NumSurface,
            pluginOpaqueAlloc->In.Type, pluginOpaqueAlloc->In.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    if (m_bIsOutOpaque)
    {
        sts = m_mfxCore.UnmapOpaqueSurface(pluginOpaqueAlloc->Out.NumSurface,
            pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    MSDK_ZERO_MEMORY(m_Param);

    m_bInited = false;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);

    if(NULL == in)
    {
        MSDK_ZERO_MEMORY(out->mfx);
        MSDK_ZERO_MEMORY(out->vpp);

        out->vpp.In.FourCC = out->vpp.Out.FourCC = MFX_FOURCC_NV12;
        out->vpp.In.Height = out->vpp.Out.Height = 1;
        out->vpp.In.Width = out->vpp.Out.Width = 1;
        out->vpp.In.PicStruct = out->vpp.Out.PicStruct = 1;
        out->vpp.In.FrameRateExtN = out->vpp.Out.FrameRateExtN = 30;
        out->vpp.In.FrameRateExtD = out->vpp.Out.FrameRateExtD = 1;
        out->IOPattern = 1;
        out->AsyncDepth = 1;
    }
    else
    {
        if (in->vpp.In.FourCC != MFX_FOURCC_NV12)
        {
            in->vpp.In.FourCC = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }
        if (in->vpp.Out.FourCC != MFX_FOURCC_NV12)
        {
            in->vpp.Out.FourCC = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }
        if (!(in->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY))
        {
            in->IOPattern = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }
        out->vpp = in->vpp;
        out->AsyncDepth = in->AsyncDepth;
        out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    return mfxSts;
}

mfxStatus PeopleDetectionPlugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);

    mfxRes = CheckInOutFrameInfo(&(par->vpp.In), &(par->vpp.Out));

    in->Info = par->vpp.In;
    in->NumFrameMin = 1;
    in->NumFrameSuggested = in->NumFrameMin + par->AsyncDepth;

    out->Info = par->vpp.Out;
    out->NumFrameMin = 1;
    out->NumFrameSuggested = out->NumFrameMin + par->AsyncDepth;

    return mfxRes;
}

mfxStatus PeopleDetectionPlugin::VPPFrameSubmit(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxThreadTask *task)
{
    MSDK_CHECK_POINTER(in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(task, MFX_ERR_NULL_PTR);
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    mfxFrameSurface1 *surface_in = (mfxFrameSurface1 *)in;
    mfxFrameSurface1 *surface_out = (mfxFrameSurface1 *)out;
    mfxFrameSurface1 *real_surface_in = surface_in;
    mfxFrameSurface1 *real_surface_out = surface_out;

    mfxStatus sts = MFX_ERR_NONE;

    if (m_bIsInOpaque)
    {
        sts = m_mfxCore.GetRealSurface(surface_in, &real_surface_in);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    if (m_bIsOutOpaque)
    {
        sts = m_mfxCore.GetRealSurface(surface_out, &real_surface_out);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    sts = CheckInOutFrameInfo(&real_surface_in->Info, &real_surface_out->Info);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxU32 ind = FindFreeTaskIdx();

    if (ind >= m_MaxNumTasks)
    {
        return MFX_WRN_DEVICE_BUSY;
    }

    m_mfxCore.IncreaseReference(&(real_surface_in->Data));
    m_mfxCore.IncreaseReference(&(real_surface_out->Data));

    m_pTasks[ind].In = real_surface_in;
    m_pTasks[ind].Out = real_surface_out;
    m_pTasks[ind].bBusy = true;

    m_pTasks[ind].pProcessor = new PeopleDetectorProcessor(&m_Param);
    MSDK_CHECK_POINTER(m_pTasks[ind].pProcessor, MFX_ERR_MEMORY_ALLOC);

    m_pTasks[ind].pProcessor->SetAllocator(&m_mfxCore.FrameAllocator());
    m_pTasks[ind].pProcessor->Init(real_surface_in, real_surface_out);

    *task = (mfxThreadTask)&m_pTasks[ind];

    return MFX_ERR_NONE;
}

mfxU32 PeopleDetectionPlugin::FindFreeTaskIdx()
{
    mfxU32 i;
    for (i = 0; i < m_MaxNumTasks; i++)
    {
        if (false == m_pTasks[i].bBusy)
        {
            break;
        }
    }

    return i;
}

mfxStatus PeopleDetectionPlugin::CheckInOutFrameInfo(mfxFrameInfo *pIn, mfxFrameInfo *pOut)
{
    MSDK_CHECK_POINTER(pIn, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pOut, MFX_ERR_NULL_PTR);

    if (pIn->FourCC != MFX_FOURCC_NV12 ||
        pOut->FourCC != MFX_FOURCC_NV12)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    if( 0 == pIn->Width || 0 == pIn->Height ||
        0 == pOut->Width || 0 == pOut->Height)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}
