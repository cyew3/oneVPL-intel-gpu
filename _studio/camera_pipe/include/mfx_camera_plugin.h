/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_CAMERA_PLUGIN_INCLUDED__)
#define __MFX_CAMERA_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include <ippi.h>

#include "mfx_camera_plugin_utils.h"
#include "mfx_camera_plugin_cpu.h"
#include "mfx_camera_plugin_cm.h"
#if defined (_WIN32) || defined (_WIN64)
#include "mfx_camera_plugin_dx11.h"
#include "mfx_camera_plugin_dx9.h"
#endif

#define MFX_CAMERA_DEFAULT_ASYNCDEPTH 3
#define MAX_CAMERA_SUPPORTED_WIDTH  16280
#define MAX_CAMERA_SUPPORTED_HEIGHT 15952

enum CameraFallback{
    FALLBACK_NONE = 0,
    FALLBACK_CPU  = 1,
    FALLBACK_CM   = 2,
};

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

    UMC::Mutex m_guard1;

#ifdef WRITE_CAMERA_LOG
    UMC::Mutex m_log_guard;
#endif

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    CameraProcessor *   m_CameraProcessor;

    mfxStatus           CameraAsyncRoutine(AsyncParams *pParam);
    mfxStatus           CompleteCameraAsyncRoutine(AsyncParams *pParam);

    static mfxStatus    CameraRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus    CompleteCameraRoutine(void *pState, void *pParam, mfxStatus taskRes);

    mfxStatus           ProcessExtendedBuffers(mfxVideoParam *par);

    mfxStatus           WaitForActiveThreads();

    mfxVideoParam       m_mfxVideoParam;

    // Width and height provided at Init
    mfxU16              m_InitWidth;
    mfxU16              m_InitHeight;

    VideoCORE*          m_core;
    eMFXHWType          m_platform;

    bool                m_isInitialized;
    bool                m_useSW;

    mfxU16              m_activeThreadCount;

    mfxCameraCaps       m_Caps;

    //Filter specific parameters
    CameraPipeDenoiseParams            m_DenoiseParams;
    CameraPipeHotPixelParams           m_HPParams;
    CameraPipeWhiteBalanceParams       m_WBparams;
    CameraPipeForwardGammaParams       m_GammaParams;
    CameraPipeVignetteParams           m_VignetteParams;
    CameraPipePaddingParams            m_PaddingParams;
    CameraPipe3DLUTParams              m_3DLUTParams;
    CameraPipeBlackLevelParams         m_BlackLevelParams;
    CameraPipe3x3ColorConversionParams m_CCMParams;
    CameraPipeLensCorrectionParams     m_LensParams;
    CameraParams                       m_PipeParams;
    CameraFallback                     m_fallback;

    mfxU32                       m_InputBitDepth;
    mfxU16                       m_nTilesVer;                       
    mfxU16                       m_nTilesHor;

    mfxU16  BayerPattern_API2CM(mfxU16 api_bayer_type)
    {
        switch(api_bayer_type)
        {
          case MFX_CAM_BAYER_BGGR:
            return BGGR;
          case MFX_CAM_BAYER_GBRG:
            return GBRG;
          case MFX_CAM_BAYER_GRBG:
            return GRBG;
          case MFX_CAM_BAYER_RGGB:
            return RGGB;
        }

        return 0;
    };

private:


};
#endif //#if defined( AS_VPP_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif  // __MFX_CAMERA_PLUGIN_INCLUDED__
