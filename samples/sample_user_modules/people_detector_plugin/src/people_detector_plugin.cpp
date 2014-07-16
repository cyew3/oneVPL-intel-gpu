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
    m_bIsOutOpaque(false)
{
    m_MaxNumTasks = 0;

    memset(&m_VideoParam, 0, sizeof(m_VideoParam));
    memset(&m_Param, 0, sizeof(m_Param));
    m_Param.Treshold = 200.0f;
    m_Param.RenderFlag = MFX_RENDER_ALWAYS;

    memset(&m_PluginParam, 0, sizeof(m_PluginParam));

    m_session = 0;
    //m_pmfxCore = 0;

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
    Close();
}

mfxStatus PeopleDetectionPlugin::PluginInit(mfxCoreInterface *core)
{
    MSDK_CHECK_POINTER(core, MFX_ERR_NULL_PTR);
    m_mfxCore = MFXCoreInterface(*core);
    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::PluginClose()
{
    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::GetPluginParam(mfxPluginParam *par)
{
    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);

    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::Submit(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task)
{
    MSDK_CHECK_POINTER(in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);
    //MSDK_CHECK_POINTER(*in, MFX_ERR_NULL_PTR);
    //MSDK_CHECK_POINTER(*out, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(task, MFX_ERR_NULL_PTR);
    //MSDK_CHECK_NOT_EQUAL(in_num, 1, MFX_ERR_UNSUPPORTED);
    //MSDK_CHECK_NOT_EQUAL(out_num, 1, MFX_ERR_UNSUPPORTED);
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

    for(int bufferIndex = 0; bufferIndex < mfxParam->NumExtParam; bufferIndex++ )
    {
        if (mfxParam->ExtParam[bufferIndex]->BufferId == (mfxU32)MFX_EXTBUFF_VPP_PEOPLE_DETECTOR)
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
    memset(m_pTasks, 0, sizeof(PeopleDetectorTask) * m_MaxNumTasks);

    m_VideoParam.vpp.In.CropW = mfxParam->vpp.In.Width;
    m_VideoParam.vpp.In.CropH = mfxParam->vpp.In.Height;

    m_VideoParam.vpp.Out.CropW = mfxParam->vpp.Out.Width;
    m_VideoParam.vpp.Out.CropH = mfxParam->vpp.Out.Height;
    m_VideoParam.vpp.Out.FourCC = mfxParam->vpp.Out.FourCC;

    m_bInited = true;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::Close()
{
    if (!m_bInited)
        return MFX_ERR_NONE;

    memset(&m_Param, 0, sizeof(vaExtVPPDetectPeople));

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

    m_bInited = false;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);

    out->vpp = in->vpp;

    out->AsyncDepth = in->AsyncDepth;

    out->IOPattern = in->IOPattern;

    mfxU32 temp = out->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    if (temp==0 || temp==MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);

    in->Info = par->vpp.In;
    in->NumFrameMin = 1;
    in->NumFrameSuggested = in->NumFrameMin + par->AsyncDepth;
    in->Type = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_SYSTEM_MEMORY;

    out->Info = par->vpp.Out;
    out->NumFrameMin = 1;
    out->NumFrameSuggested = out->NumFrameMin + par->AsyncDepth;
    out->Type = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_SYSTEM_MEMORY;

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectionPlugin::VPPFrameSubmit(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxThreadTask *task)
{
    return Submit((mfxHDL*) in, 1, (mfxHDL*) out, 1, task);
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

    if (pIn->CropW != m_VideoParam.vpp.In.CropW || pIn->CropH != m_VideoParam.vpp.In.CropH ||
        pIn->FourCC != m_VideoParam.vpp.In.FourCC ||
        pOut->CropW != m_VideoParam.vpp.Out.CropW || pOut->CropH != m_VideoParam.vpp.Out.CropH ||
        pOut->FourCC != m_VideoParam.vpp.Out.FourCC)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}
