/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_PTIR_VPP_PLUGIN_INCLUDED__)
#define __MFX_PTIR_VPP_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include <vector>
#include <algorithm>
#include "mfxvideo.h"
#include "mfxplugin++.h"
#include <umc_mutex.h>
#include "ptir_wrap.h"
#include "ptir_vpp_utils.h"



//#include "mfxvideo++int.h"

struct PTIR_Task
{
    mfxU32           id;
    bool             filled;
    mfxFrameSurface1 *surface_out;
};

enum {
    //MFX_FOURCC_NV12         = MFX_MAKEFOURCC('N','V','1','2'),   /* Native Format */
    //...

    MFX_FOURCC_I420         = MFX_MAKEFOURCC('I','4','2','0'),   /* Non-native Format */
};


class MFX_PTIR_Plugin : public MFXVPPPlugin
{
    static const mfxPluginUID g_VPP_PluginGuid;
public:
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    virtual mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 );
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus );
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual void Release()
    {
        delete this;
    }
    static MFXVPPPlugin* Create() {
        return new MFX_PTIR_Plugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        if (memcmp(& guid , &g_VPP_PluginGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }
        MFX_PTIR_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFX_PTIR_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(...)
        {
            return MFX_ERR_UNKNOWN;;
        }

        try
        {
            tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXVPPPlugin> (tmp_pplg));
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        *mfxPlg = tmp_pplg->m_adapter->operator mfxPlugin();
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
    MFX_PTIR_Plugin(bool CreateByDispatcher);
    virtual ~MFX_PTIR_Plugin();

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    mfxVideoParam       m_mfxInitPar;
    mfxVideoParam       m_mfxCurrentPar;
    bool                m_createdByDispatcher;
    std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > m_adapter;
    PTIR_Processor*  ptir;
    UMC::Mutex m_guard;

    bool bEOS;
    bool bInited;
    bool par_accel;
    bool in_expected;
    frameSupplier* frmSupply;
    mfxFrameSurface1* prevSurf;
    std::vector<PTIR_Task*> vTasks;
    std::vector<mfxFrameSurface1*> inSurfs;
    std::vector<mfxFrameSurface1*> workSurfs;
    std::vector<mfxFrameSurface1*> outSurfs;

    mfxStatus QueryReset(const mfxVideoParam& old_vp, const mfxVideoParam& new_vp);

    inline mfxStatus GetHandle(mfxHDL& mfxDeviceHdl, mfxHandleType& mfxDeviceType);
    inline mfxStatus GetHWTypeAndCheckSupport(mfxIMPL& impl, mfxHDL& mfxDeviceHdl, eMFXHWType& HWType, bool& HWSupported, bool& par_accel);
    inline mfxStatus CheckInFrameSurface1(mfxFrameSurface1*& mfxSurf, mfxFrameSurface1*& mfxSurfOut);
    inline mfxStatus CheckOutFrameSurface1(mfxFrameSurface1*& mfxSurf);
    inline mfxStatus CheckFrameSurface1(mfxFrameSurface1*& mfxSurf);

    inline mfxStatus PrepareTask(PTIR_Task *ptir_task, mfxThreadTask *task, mfxFrameSurface1 **surface_out);
    inline mfxFrameSurface1* GetFreeSurf(std::vector<mfxFrameSurface1*>& vSurfs);
    inline mfxStatus addWorkSurf(mfxFrameSurface1* pSurface);
    inline mfxStatus addInSurf(mfxFrameSurface1* pSurface);
};

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif

#endif  // __MFX_PTIR_VPP_PLUGIN_INCLUDED__
