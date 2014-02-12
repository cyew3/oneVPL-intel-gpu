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

#include "mfx_camera_plugin_utils.h"

using namespace MfxCameraPlugin;

#if defined( AS_VPP_PLUGIN )
class MFXCamera_Plugin : public MFXVPPPlugin
{
public:
    static const mfxPluginUID g_Camera_PluginGuid;
    static std::auto_ptr<MFXCamera_Plugin> g_singlePlg;
    static std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > g_adapter;
    MFXCamera_Plugin(bool CreateByDispatcher);
    virtual ~MFXCamera_Plugin();

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task);
    //{
    //    return MFXVideoVPP_RunFrameVPPAsync(m_session, surface_in, surface_out, aux, (mfxSyncPoint*) task);
    //}
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 )
    {
        return MFXVideoCORE_SyncOperation(m_session, (mfxSyncPoint) task, MFX_INFINITE);
    }
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus )
    {
        return MFX_ERR_NONE;
    }
    //virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    virtual mfxStatus Query(mfxVideoParam *, mfxVideoParam *)
    {
        return MFX_ERR_NONE;
        //return MFXVideoVPP_Query(m_session, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);

    virtual mfxStatus Init(mfxVideoParam *par);
//    {
//        return MFXVideoVPP_Init(m_session, par);
//    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return MFXVideoVPP_Reset(m_session, par);
    }
    virtual mfxStatus Close()
    {
        return MFXVideoVPP_Close(m_session);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return MFXVideoVPP_GetVideoParam(m_session, par);
    }
    virtual void Release()
    {
        delete this;
    }
    static MFXVPPPlugin* Create() {
        return new MFXCamera_Plugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {

        if (g_singlePlg.get()) {
            return MFX_ERR_NOT_FOUND;
        }
        //check that guid match
        g_singlePlg.reset(new MFXCamera_Plugin(true));

        if (memcmp(& guid , &g_Camera_PluginGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }

        g_adapter.reset(new MFXPluginAdapter<MFXVPPPlugin> (g_singlePlg.get()));
        *mfxPlg = g_adapter->operator mfxPlugin();

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

    struct AsyncParams
    {
        mfxFrameSurface1 *surf_in;
        mfxFrameSurface1 *surf_out;
        //mfxExtVppAuxData *aux;
        //mfxU64  inputTimeStamp;
    };

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;

    mfxStatus           AllocateInternalSurfaces();
    mfxStatus           SetExternalSurfaces(mfxFrameSurface1 *surfIn, mfxFrameSurface1 *surfOut);

    mfxStatus           CameraAsyncRoutine(AsyncParams *pParam);

    static mfxStatus    CameraRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);

    mfxStatus           SetTasks(); //mfxFrameSurface1 *surfIn, mfxFrameSurface1 *surfOut);
    mfxStatus           EnqueueTasks();

    mfxVideoParam       m_mfxVideoParam;
    CmDevicePtr         m_cmDevice;
    VideoCORE*          m_core;

    std::auto_ptr<CmContext>    m_cmCtx;

    mfxCameraCaps       m_Caps;
    CameraPipeWhiteBalanceParams m_WBparams;
    CameraPipeForwardGammaParams m_GammaParams;

    bool                m_bInMemGPUShared;
    bool                m_bInMemGPUCopy;
    bool                m_bInMemCPUCopy;

    mfxU32              m_FrameWidth64;
    mfxU32              m_PaddedFrameWidth;
    mfxU32              m_PaddedFrameHeight;
    mfxU32              m_InputBitDepth;

    CmSurface2D         *m_cmSurfIn; // CmSurface2DUP or CmSurface2D
    CmSurface2DUP       *m_cmSurfUPIn; // CmSurface2DUP or CmSurface2D
    void                *m_memIn;
    void                *m_memOut;
    void                *m_memOutAllocPtr;


    CmBuffer            *m_cmBufferOut;
    CmBufferUP          *m_cmBufferUPOut; // ???

    CmSurface2D         *m_gammaCorrectSurf;
    CmSurface2D         *m_gammaPointSurf;

private:

    //MfxFrameAllocResponse   m_internalVidSurf[2];
    MfxFrameAllocResponse   m_raw16;
    MfxFrameAllocResponse   m_raw16padded;
    MfxFrameAllocResponse   m_raw16aligned;
    MfxFrameAllocResponse   m_aux8;


};
#endif //#if defined( AS_VPP_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif  // __MFX_HEVC_VPP_PLUGIN_INCLUDED__
