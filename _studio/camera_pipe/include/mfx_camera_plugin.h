/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_CAMERA_PLUGIN_INCLUDED__)
#define __MFX_CAMERA_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"
#include "mfx_session.h"
#include "mfxcamera.h"

#include "mfx_camera_plugin_utils.h"
#include "mfx_camera_plugin_cpu.h"

using namespace MfxCameraPlugin;

#define MFX_CAMERA_DEFAULT_ASYNCDEPTH 3

#if defined( AS_VPP_PLUGIN )
class MFXCamera_Plugin : public MFXVPPPlugin
{
public:
    static const mfxPluginUID g_Camera_PluginGuid;

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task);
    virtual mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *, mfxFrameSurface1 *, mfxFrameSurface1 **, mfxThreadTask *)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus );
    virtual mfxStatus Query(mfxVideoParam *, mfxVideoParam *);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual void Release()
    {
        delete this;
    }

    static MFXVPPPlugin* Create() {
        return new MFXCamera_Plugin(false);
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
    {
        if (memcmp(&guid, &g_Camera_PluginGuid, sizeof(mfxPluginUID)))
        {
            return MFX_ERR_NOT_FOUND;
        }

        MFXCamera_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFXCamera_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(MFX_CORE_CATCH_TYPE)
        {
            return MFX_ERR_UNKNOWN;
        }

        try
        {
            tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXVPPPlugin> (tmp_pplg));
        }
        catch(std::bad_alloc&)
        {
            delete tmp_pplg;
            return MFX_ERR_MEMORY_ALLOC;
        }

        *mfxPlg = (mfxPlugin)*tmp_pplg->m_adapter;
        tmp_pplg->m_createdByDispatcher = true;

        return MFX_ERR_NONE;
    }

    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_VPP;
    }

    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:
    MFXCamera_Plugin(bool CreateByDispatcher);
    virtual ~MFXCamera_Plugin();
    std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > m_adapter;

    struct AsyncParams
    {
        mfxFrameSurface1 *surf_in;
        mfxFrameSurface1 *surf_out;

        mfxMemId inSurf2D;
        mfxMemId inSurf2DUP;

        mfxMemId outSurf2D;
        mfxMemId outSurf2DUP;

        void      *pEvent;
    };

    UMC::Mutex m_guard;
    UMC::Mutex m_guard1;

#ifdef WRITE_CAMERA_LOG
    UMC::Mutex m_log_guard;
#endif

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;

    mfxStatus           AllocateInternalSurfaces();
    mfxStatus           ReallocateInternalSurfaces(mfxVideoParam &newParam, CameraFrameSizeExtra &frameSizeExtra);
    mfxStatus           SetExternalSurfaces(AsyncParams *pParam);

    mfxStatus           CameraAsyncRoutine(AsyncParams *pParam);
    mfxStatus           CompleteCameraAsyncRoutine(AsyncParams *pParam);

    static mfxStatus    CameraRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus    CompleteCameraRoutine(void *pState, void *pParam, mfxStatus taskRes);

    mfxStatus           ProcessExtendedBuffers(mfxVideoParam *par);

    mfxStatus           CreateEnqueueTasks(AsyncParams *pParam);

    mfxStatus           WaitForActiveThreads();

    mfxVideoParam       m_mfxVideoParam;
    CmDevicePtr         m_cmDevice;
    VideoCORE*          m_core;
    eMFXHWType          m_platform;

    bool                m_isInitialized;
    bool                m_useSW;

    mfxU16              m_activeThreadCount;

    std::auto_ptr<CmContext>    m_cmCtx;

    mfxCameraCaps       m_Caps;

    //Filter specific parameters
    CameraPipeWhiteBalanceParams       m_WBparams;
    CameraPipeForwardGammaParams       m_GammaParams;
    CameraPipeVignetteParams           m_VignetteParams;
    CameraPipePaddingParams            m_PaddingParams;
    CameraPipeBlackLevelParams         m_BlackLevelParams;
    CameraPipe3x3ColorConversionParams m_CCMParams;
    CameraFrameSizeExtra               m_FrameSizeExtra;

    mfxU32                       m_InputBitDepth;
    int                          m_nTiles;

    void                *m_memIn;

    CmSurface2D         *m_cmSurfIn;
    CmSurface2D         *m_paddedSurf;
    CmSurface2D         *m_gammaCorrectSurf;
    CmSurface2D         *m_gammaPointSurf;
    CmSurface2D         *m_gammaOutSurf;
    CmSurface2D         *m_avgFlagSurf;
    CmSurface2D         *m_vignetteMaskSurf;
    CmBuffer            *m_LUTSurf;

private:

    MfxFrameAllocResponse   m_raw16padded;
    MfxFrameAllocResponse   m_raw16aligned;
    MfxFrameAllocResponse   m_aux8;

    CamInfo m_cmi;
};
#endif //#if defined( AS_VPP_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif  // __MFX_CAMERA_PLUGIN_INCLUDED__
