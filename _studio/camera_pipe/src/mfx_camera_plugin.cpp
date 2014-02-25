/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin.cpp

\* ****************************************************************************** */

#define VPP_IN       (0)
#define VPP_OUT      (1)

#include "mfx_camera_plugin.h"
//#include "mfx_session.h"
#include "mfx_task.h"

#ifdef CAMP_PIPE_ITT
#include "ittnotify.h"

__itt_domain* CamPipe = __itt_domain_create(L"CamPipe");

//__itt_string_handle* CPU_file_fread;
//__itt_string_handle* CPU_raw_unpack_;
__itt_string_handle* task1 = __itt_string_handle_create(L"CreateEnqueueTasks");;
#endif

#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
    NULL,
    NULL,
    NULL,
    &MFXCamera_Plugin::Create,
    &MFXCamera_Plugin::CreateByDispatcher
};

MSDK_PLUGIN_API(MFXVPPPlugin*) mfxCreateVPPPlugin() {
    if (!g_PluginModule.CreateVPPPlugin) {
        return 0;
    }
    return g_PluginModule.CreateVPPPlugin();
}

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
    if (!g_PluginModule.CreatePlugin) {
        return 0;
    }
    return (MFXPlugin*) g_PluginModule.CreatePlugin(uid, plugin);
}

const mfxPluginUID MFXCamera_Plugin::g_Camera_PluginGuid = {0x54, 0x54, 0x26, 0x16, 0x24, 0x33, 0x41, 0xe6, 0x93, 0xae, 0x89, 0x99, 0x42, 0xce, 0x73, 0x55};
std::auto_ptr<MFXCamera_Plugin> MFXCamera_Plugin::g_singlePlg;
std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > MFXCamera_Plugin::g_adapter;


MFXCamera_Plugin::MFXCamera_Plugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_Camera_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_VPP;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;

    Zero(m_Caps);
    m_Caps.InputMemoryOperationMode = MEM_FASTGPUCPY;
    m_Caps.OutputMemoryOperationMode = MEM_FASTGPUCPY;

    m_cmSurfIn = 0;
    m_cmSurfUPIn = 0;
    m_memIn = 0;
    m_cmBufferOut = 0;
    m_cmBufferUPOut = 0;
    m_memOut = 0;
    m_memOutAllocPtr = 0;
}

MFXCamera_Plugin::~MFXCamera_Plugin()
{
    if (m_session)
    {
        PluginClose();
    }
}

mfxStatus MFXCamera_Plugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);

    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    //par.Impl = MFX_IMPL_RT;

#if !defined (MFX_VA) && defined (AS_VPP_PLUGIN)
    par.Impl = MFX_IMPL_SOFTWARE;
#endif

    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);

    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pmfxCore->pthis, m_session);

    return mfxRes;
}

mfxStatus MFXCamera_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;
    if (m_session)
    {
        //The application must ensure there is no active task running in the session before calling this (MFXDisjoinSession) function.
        mfxRes = MFXVideoVPP_Close(m_session);
        //Return the first met wrn or error
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED)
            mfxRes2 = mfxRes;
        //uncomment when the light core is ready (?) kta
        mfxRes = MFXInternalPseudoDisjoinSession(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
           mfxRes2 = mfxRes;
        mfxRes = MFXClose(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
            mfxRes2 = mfxRes;
        m_session = 0;
    }
    if (m_createdByDispatcher) {
        g_singlePlg.reset(0);
        g_adapter.reset(0);
    }
    return mfxRes2;
}

mfxStatus MFXCamera_Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    if (!in)
        return MFX_ERR_NULL_PTR;
    if (!out)
        return MFX_ERR_NULL_PTR;

    mfxU16 asyncDepth = par->AsyncDepth;
    if (asyncDepth == 0)
        asyncDepth = m_core->GetAutoAsyncDepth();

    in->Info = par->vpp.In;
    in->NumFrameMin = 1;
    in->NumFrameSuggested = in->NumFrameMin + asyncDepth - 1;
    in->Type = MFX_MEMTYPE_FROM_VPPIN;

    out->Info = par->vpp.Out;
    out->NumFrameMin = 1;
    out->NumFrameSuggested = out->NumFrameMin + asyncDepth - 1;
    out->Type = MFX_MEMTYPE_FROM_VPPOUT;

    return MFX_ERR_NONE;
}

mfxStatus  MFXCamera_Plugin::GetVideoParam(mfxVideoParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_mfxVideoParam;
    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::Init(mfxVideoParam *par)
{
    mfxStatus mfxRes;

    m_mfxVideoParam = *par;
    m_core = m_session->m_pCORE.get();

    if (m_mfxVideoParam.AsyncDepth == 0)
        m_mfxVideoParam.AsyncDepth = m_core->GetAutoAsyncDepth();

    m_FrameWidth64   = ((m_mfxVideoParam.vpp.In.CropW + 31) & 0xFFFFFFE0); // 2 bytes each for In, 4 bytes for Out, so 32 is good enough for 64 ???
    m_PaddedFrameWidth  = m_mfxVideoParam.vpp.In.CropW + 16;
    m_PaddedFrameHeight = m_mfxVideoParam.vpp.In.CropH + 16;
    m_InputBitDepth = m_mfxVideoParam.vpp.In.BitDepthLuma;

        // get Caps and algo params from the ExtBuffers (?) or QueryCaps???
    m_Caps.bDemosaic = 1;
    m_Caps.bForwardGammaCorrection = 1;
    m_Caps.bColorConversionMaxtrix = 1;
    m_Caps.bWhiteBalance = 1;
    m_Caps.InputMemoryOperationMode = MEM_FASTGPUCPY; //MEM_GPUSHARED; //
    m_Caps.OutputMemoryOperationMode = MEM_GPUSHARED; //MEM_FASTGPUCPY;

    m_WBparams.bActive = false; // no WB
    m_WBparams.RedCorrection         = 2.156250f;
    m_WBparams.GreenTopCorrection    = 1.000000f;
    m_WBparams.GreenBottomCorrection = 1.000000f;
    m_WBparams.BlueCorrection        = 1.417969f;

    m_GammaParams.bActive = true;
    for (int i = 0 ; i < 64; i++)
    {
        m_GammaParams.gamma_hw_params.Points[i]  = (mfxU16)gamma_point[i];
        m_GammaParams.gamma_hw_params.Correct[i] = (mfxU16)gamma_correct[i];
    }

    pipeline_config cam_pipeline;
    cam_pipeline.wbFlag = m_Caps.bWhiteBalance &&  m_WBparams.bActive;
    cam_pipeline.gammaFlag = m_Caps.bForwardGammaCorrection && m_GammaParams.bActive;

    m_cmDevice.Reset(CreateCmDevicePtr(m_core));
    m_cmCtx.reset(new CmContext(m_mfxVideoParam, m_cmDevice, &cam_pipeline));

    m_cmCtx->CreateThreadSpace(m_PaddedFrameWidth, m_PaddedFrameHeight);

    mfxRes = AllocateInternalSurfaces();

    return mfxRes;
}

mfxStatus MFXCamera_Plugin::CameraRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    mfxStatus sts;

    threadNumber; callNumber;

    MFXCamera_Plugin & impl = *(MFXCamera_Plugin *)pState;
    AsyncParams *asyncParams = (AsyncParams *)pParam;

    sts = impl.CameraAsyncRoutine(asyncParams);


    return sts;
}

mfxStatus MFXCamera_Plugin::CameraAsyncRoutine(AsyncParams *pParam)
{
    pParam;
    mfxStatus sts;
    mfxFrameSurface1 *surfIn = pParam->surf_in;
    mfxFrameSurface1 *surfOut = pParam->surf_out;

    m_core->IncreaseReference(&surfIn->Data);
    m_core->IncreaseReference(&surfOut->Data);

    //sts = SetExternalSurfaces(surfIn, surfOut);
    sts = SetExternalSurfaces(pParam);

//    sts = SetTasks(); //(surfIn, surfOut);
//    sts = EnqueueTasks();

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, task1);
#endif

    sts  = CreateEnqueueTasks(pParam);

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif

    m_core->DecreaseReference(&surfIn->Data);
    m_core->DecreaseReference(&surfOut->Data);

    return sts;
}


mfxStatus MFXCamera_Plugin::CompleteCameraRoutine(void *pState, void *pParam, mfxStatus taskRes)
{
    mfxStatus sts;
    taskRes;
    pState;
    MFXCamera_Plugin & impl = *(MFXCamera_Plugin *)pState;
    AsyncParams *asyncParams = (AsyncParams *)pParam;

    sts = impl.CompleteCameraAsyncRoutine(asyncParams);

    if (pParam)
        delete (AsyncParams *)pParam; // not safe !!! ???

    return sts;
}


mfxStatus MFXCamera_Plugin::CompleteCameraAsyncRoutine(AsyncParams *pParam)
{
    
    mfxStatus sts = MFX_ERR_NONE;
    if (pParam) {

        ((CmEvent *)pParam->pEvent)->WaitForTaskFinished((DWORD)-1);  //???

        if (pParam->inSurf2D) {
            ReleaseResource(m_rawIn, pParam->inSurf2D);
        } else if (pParam->inSurf2DUP) {
            if (pParam->pMemIn)
                CM_ALIGNED_FREE(pParam->pMemIn); // remove/change when pool implemented !!!
            pParam->pMemIn = 0;
            CmSurface2DUP *surf = (CmSurface2DUP *)pParam->inSurf2DUP;
            m_cmDevice->DestroySurface2DUP(surf);
            pParam->inSurf2DUP = 0;
        }
    }

    m_raw16aligned.Unlock();
    m_raw16padded.Unlock();
    m_aux8.Unlock();

    return sts;
}

//mfxStatus MFXCamera_Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
//{
//}

mfxStatus MFXCamera_Plugin::VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData* /*aux*/, mfxThreadTask *mfxthreadtask)
{
    mfxStatus mfxRes;

    if (!surface_in)
        return MFX_ERR_NULL_PTR;
    if (!surface_out)
        return MFX_ERR_NULL_PTR;

    mfxSyncPoint syncp;
    MFX_TASK task;
    MFX_ENTRY_POINT entryPoint;
    memset(&entryPoint, 0, sizeof(entryPoint));
    memset(&task, 0, sizeof(task));

    AsyncParams *pParams = new AsyncParams;
    pParams->surf_in = surface_in;
    pParams->surf_out = surface_out;

    entryPoint.pRoutine = CameraRoutine;
    entryPoint.pRoutineName = "CameraPipeline";
    entryPoint.pCompleteProc = &CompleteCameraRoutine;
    entryPoint.pState = this; //???
    entryPoint.requiredNumThreads = 1;
    entryPoint.pParam = pParams;

    task.pOwner = m_session->m_plgVPP.get();
    task.entryPoint = entryPoint;
    task.priority = m_session->m_priority;
    task.threadingPolicy = (m_PluginParam.ThreadPolicy == MFX_THREADPOLICY_SERIAL ? MFX_TASK_THREADING_INTRA : MFX_TASK_THREADING_INTER); // ???
    //task.threadingPolicy = m_session->m_plgVPP->GetThreadingPolicy();
    // fill dependencies
    task.pSrc[0] = surface_in;
    task.pDst[0] = surface_out;

//#ifdef MFX_TRACE_ENABLE
//    task.nParentId = MFX_AUTO_TRACE_GETID();
//    task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;
//#endif
    // register input and call the task
    mfxRes = m_session->m_pScheduler->AddTask(task, &syncp);

    *mfxthreadtask = (mfxThreadTask)syncp;

    return mfxRes;
}