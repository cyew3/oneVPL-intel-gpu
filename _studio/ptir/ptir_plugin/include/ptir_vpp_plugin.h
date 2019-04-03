// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

//0,1,2,3 with reference to the reference PTIR application
typedef enum {
    TELECINE_PATTERN_0_32           = 0, //(0) 3:2 Telecine
    TELECINE_PATTERN_1_2332         = 1, //(1) 2:3:3:2 Telecine
    TELECINE_PATTERN_2_FRAME_REPEAT = 2, //(2) One frame repeat Telecine
    TELECINE_PATTERN_3_41           = 3, //(3) 4:1 Telecine
    TELECINE_POSITION_PROVIDED      = 4  //User must provide position inside a sequence 
                                         //of 5 frames where the artifacts starts - Value [0 - 4]
} ptirTelecinePattern;

typedef enum {
    PTIR_AUTO_DOUBLE                    = 0, //AutoMode with deinterlacing double framerate output
    PTIR_AUTO_SINGLE                    = 1, //AutoMode with deinterlacing single framerate output
    PTIR_DEINTERLACE_FULL               = 2, //Deinterlace only Mode with full framerate output
    PTIR_DEINTERLACE_HALF               = 3, //Deinterlace only Mode with half framerate output
    PTIR_OUT24FPS_FIXED                 = 4, //24fps Fixed output mode
    PTIR_FIXED_TELECINE_PATTERN_REMOVAL = 5, //Fixed Telecine pattern removal mode
    PTIR_OUT30FPS_FIXED                 = 6, //30fps Fixed output mode
    PTIR_ONLY_DETECT_INTERLACE          = 7  //Only interlace detection
} ptirOperationMode;

typedef enum {
    PTIR_TFF  = 0, //Top Field First
    PTIR_BFF  = 1  //Bottom Field First
} ptirTopBotFrameFirst;

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
    mfxExtOpaqueSurfaceAlloc m_OpaqSurfAlloc;
    mfxExtVPPDeinterlacing   m_extVPPDeint;
    bool                m_createdByDispatcher;
    std::unique_ptr<MFXPluginAdapter<MFXVPPPlugin> > m_adapter;
    PTIR_Processor*  ptir;
    UMC::Mutex m_guard;
    UMC::Mutex m_guard_free;

    bool bEOS;
    bool bInited;
    bool par_accel;
    bool in_expected;
    bool b_work;
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
    mfxStatus CheckOpaqMode(const mfxVideoParam& par, mfxVideoParam* pParOut, const mfxExtOpaqueSurfaceAlloc& opaqAlloc, mfxExtOpaqueSurfaceAlloc* pOpaqAllocOut, bool bOpaqMode[2] );

    inline mfxStatus PrepareTask(PTIR_Task *ptir_task, mfxThreadTask *task, mfxFrameSurface1 **surface_out);
    inline mfxFrameSurface1* GetFreeSurf(std::vector<mfxFrameSurface1*>& vSurfs);
    inline mfxStatus addWorkSurf(mfxFrameSurface1* pSurface);
    inline mfxStatus addInSurf(mfxFrameSurface1* pSurface);

private:
    //prohobit copy constructor
    MFX_PTIR_Plugin(const MFX_PTIR_Plugin& that);
    //prohibit assignment operator
    MFX_PTIR_Plugin& operator=(const MFX_PTIR_Plugin&);

};

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif

#endif  // __MFX_PTIR_VPP_PLUGIN_INCLUDED__
