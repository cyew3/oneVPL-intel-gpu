/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.cpp

\* ****************************************************************************** */

#include "ptir_vpp_plugin.h"
//#include "mfx_session.h"
#include "vm_sys_info.h"

#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"

PluginModuleTemplate g_PluginModule = {
    NULL,
    NULL,
    NULL,
    &MFX_PTIR_Plugin::Create,
    &MFX_PTIR_Plugin::CreateByDispatcher
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

const mfxPluginUID MFX_PTIR_Plugin::g_VPP_PluginGuid = MFX_PLUGINID_ITELECINE_HW;

MFX_PTIR_Plugin::MFX_PTIR_Plugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_VPP_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_VPP;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;
}

MFX_PTIR_Plugin::~MFX_PTIR_Plugin()
{
    if (m_session)
    {
        PluginClose();
    }
}

mfxStatus MFX_PTIR_Plugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);

    b_firstFrameProceed = false;
    bInited = false;

    uiDeinterlacingMode = INTL_MODE_SMART_CHECK;
    uiDeinterlacingMeasure = 16;
    dTimeStamp = 0.0;
    dBaseTime = 0.0;
    dOutBaseTime = 0.0;
    dBaseTimeSw = 0.0;
    dDeIntTime = 0.0;
    //liTime[sizeof(cOperations) / sizeof(const char *) + 1] = {0},
    liFreq;
    liFileSize;
    fTCodeOut = NULL;


    FrameQueue_Initialize(&fqIn);
    Pattern_init(&mainPattern);
    uiCount = MFX_INFINITE / 2; // /2 to avoid overflow. TODO: fix
    uiSupBuf = BUFMINSIZE;

    return mfxRes;
}

mfxStatus MFX_PTIR_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;

    Close();

    if (m_createdByDispatcher) {
        delete this;
    }

    return mfxRes2;
}

mfxStatus MFX_PTIR_Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus MFX_PTIR_Plugin::VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task)
{
    if(vTasks.size() != 0)
        return MFX_WRN_DEVICE_BUSY;
    if(UMC::UMC_OK != m_guard.TryLock())
        return MFX_WRN_DEVICE_BUSY;

    mfxStatus mfxSts = MFX_ERR_NONE;
    PTIR_Task *ptir_task = 0;
    if(NULL == surface_out)
    {
        m_guard.Unlock();
        return MFX_ERR_NULL_PTR;
    }
    if(!surface_in)
        bEOS = true;
    else
        bEOS = false;

    if(0 != fqIn.iCount)
    {
        /*if previous submit call returned ERR_MORE_SURFACE, application should not update input frame,
        however, plugin can handle it */
        if(surface_in && prevSurf != surface_in && inSurfs.size() != 0 && inSurfs.back() != surface_in)
        {
            if(inSurfs.end() == std::find (inSurfs.begin(), inSurfs.end(), surface_in))
            {
                inSurfs.push_back(surface_in);
                m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &surface_in->Data);
            }
        }
        mfxSts = PrepareTask(ptir_task, task, surface_out);
        if(mfxSts)
            return mfxSts;

        m_guard.Unlock();
        if(1 == fqIn.iCount)
            return MFX_ERR_NONE;
        else
            return MFX_ERR_MORE_SURFACE;
    }
    else if(surface_in && inSurfs.size() < 8)
    {
        //add in surface to the queue if it is not already here
        if(inSurfs.end() == std::find (inSurfs.begin(), inSurfs.end(), surface_in))
        {
            inSurfs.push_back(surface_in);
            m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &surface_in->Data);
        }

        m_guard.Unlock();
        return MFX_ERR_MORE_DATA;
    }
    else if(surface_in && inSurfs.size() > 7 )
    {
        //add in surface to the queue if it is not already here
        if(inSurfs.end() == std::find (inSurfs.begin(), inSurfs.end(), surface_in))
        {
            inSurfs.push_back(surface_in);
            m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &surface_in->Data);
        }
        //queue is full, ready to create task for execution
        mfxSts = PrepareTask(ptir_task, task, surface_out);
        if(mfxSts)
            return mfxSts;

        m_guard.Unlock();
        return MFX_ERR_NONE;
    }
    else if(0 != uiCur)
    {
        //input surface is 0, end of stream case, and there are smth in PTIR cache
        mfxSts = PrepareTask(ptir_task, task, surface_out);
        if(mfxSts)
            return mfxSts;

        m_guard.Unlock();
        if(uiCur > 1)
            return MFX_ERR_MORE_SURFACE;
        else
            return MFX_ERR_NONE;
    }
    else if(0 == fqIn.iCount && 0 == uiCur)
    {
        m_guard.Unlock();
        return MFX_ERR_MORE_DATA;
    }

    m_guard.Unlock();
    return MFX_ERR_MORE_DATA;
}
mfxStatus MFX_PTIR_Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
{
    UMC::AutomaticUMCMutex guard(m_guard);
    PTIR_Task *ptir_task = (PTIR_Task*) task;
    if(!ptir_task->filled)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    if(inSurfs.size() == 0 && 0 == fqIn.iCount && 0 == uiCur)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxFrameSurface1 *surface_out;
    surface_out = ptir_task->surface_out;

    if(inSurfs.size() != 0)
    {
        //process input frames until 1 output frame is generated
        for(std::vector<mfxFrameSurface1*>::iterator it = inSurfs.begin(); it != inSurfs.end(); ++it) {
            mfxSts = Process(*it, surface_out);
            m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &(*it)->Data);
            prevSurf = *it;
            *it = 0;
            if(MFX_ERR_NONE == mfxSts)
            {
                m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
                surface_out = 0;
                break;
            }
            else if(MFX_ERR_MORE_SURFACE == mfxSts)
            {
                m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
                surface_out = 0;
                mfxSts = MFX_ERR_NONE;
                break;
            }
            else if(MFX_ERR_MORE_DATA != mfxSts)
            {
                assert(0);
                return mfxSts;
            }
        }
        while(0 == *inSurfs.begin())
        {
            //remove the processed head of the queue
            inSurfs.erase(inSurfs.begin());
            if(0 == inSurfs.size())
                break;
        }
    }
    else
    {
        //get cached frame
        mfxSts = Process(0, surface_out);
        if(MFX_ERR_NONE == mfxSts ||
           MFX_ERR_MORE_SURFACE == mfxSts)
        {
            m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
            surface_out = 0;
            if(0 == uiCur && 0 == fqIn.iCount)
                uiCur = 0;
        }
        else if(MFX_ERR_MORE_DATA != mfxSts)
        {
            assert(0);
            return mfxSts;
        }
    }

    if(surface_out)
    {
        //smth wrong, execute should always generate output!
        assert(0);
        m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
        surface_out = 0;
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    for(mfxU32 i=0; i < vTasks.size(); i++){
        if (vTasks[i] == (PTIR_Task*) task)
        {
            delete vTasks[i];
            vTasks.erase(vTasks.begin() + i);
        }
    }

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    bool bWarnIsNeeded = false;
    if(!in)
        return MFX_ERR_NULL_PTR;

    mfxVideoParam tmp_param;
    if(!out)
        out = &tmp_param;

    memcpy(&out->vpp, &in->vpp, sizeof(mfxInfoVPP));
    if(in->AsyncDepth == 0 || in->AsyncDepth == 1)
    {
        out->AsyncDepth = in->AsyncDepth;
    }
    else
    {
        out->AsyncDepth = 1;
        bWarnIsNeeded = true;
    }
    out->IOPattern  = in->IOPattern;

    if(in->vpp.Out.PicStruct == in->vpp.In.PicStruct)
    {
        out->vpp.Out.PicStruct = 0;
        out->vpp.In.PicStruct = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    // auto-detection mode
    //if(in->vpp.In.PicStruct == MFX_PICSTRUCT_UNKNOWN)
    //{
    //    out->vpp.In.PicStruct = 65535; // MFX_PICSTRUCT_UNKNOWN == 0, so that 0 is meaningless here
    //    return MFX_ERR_UNSUPPORTED;
    //}
    if(in->vpp.Out.PicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        out->vpp.Out.PicStruct = 65535; // MFX_PICSTRUCT_UNKNOWN == 0, so that 0 is meaningless here
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->vpp.Out.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        out->vpp.Out.PicStruct = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    if(in->vpp.In.Width  != in->vpp.Out.Width  ||
       in->vpp.In.Height != in->vpp.Out.Height ||
       in->vpp.In.CropX  != in->vpp.Out.CropX  ||
       in->vpp.In.CropY  != in->vpp.Out.CropY  ||
       in->vpp.In.CropW  != in->vpp.Out.CropW  ||
       in->vpp.In.CropH  != in->vpp.Out.CropH )
    {
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->vpp.In.FourCC != in->vpp.Out.FourCC &&
       in->vpp.In.FourCC != MFX_FOURCC_NV12)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return bWarnIsNeeded ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    bool bSWLib = true;

    in->Info = par->vpp.In;
    out->Info = par->vpp.Out;
    mfxU16 AsyncDepth = 1;
    if(par->AsyncDepth)
        AsyncDepth = par->AsyncDepth;

    in->NumFrameMin = 10 * AsyncDepth;
    in->NumFrameSuggested = 12 * AsyncDepth;
    //in->Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;

    out->NumFrameMin = 2 * AsyncDepth;
    out->NumFrameSuggested = 4 * AsyncDepth;
    //out->Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;


    mfxU32 IOPattern = par->IOPattern;
    mfxU16 *pInMemType = &in->Type;
    mfxU16 *pOutMemType = &out->Type;

    if ((IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) &&
        (IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }


    mfxU16 nativeMemType = (bSWLib) ? (mfxU16)MFX_MEMTYPE_SYSTEM_MEMORY : (mfxU16)MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;

    if( IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY )
    {
          *pInMemType = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else if (IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        *pInMemType = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
    }
    else if (IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        *pInMemType = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_OPAQUE_FRAME|nativeMemType;
    }
    else
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if( IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
    {
        *pOutMemType = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else if (IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        *pOutMemType = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
    }
    else if(IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        *pOutMemType = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_OPAQUE_FRAME|nativeMemType;
    }
    else
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Init(mfxVideoParam *par)
{
    if(!par)
        return MFX_ERR_NULL_PTR;

    mfxStatus mfxSts;

    mfxSts = Query(par, 0);
    if(!(MFX_ERR_NONE == mfxSts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxSts))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    //uiInWidth  = uiWidth  = par->vpp.In.Width;
    //uiInHeight = uiHeight = par->vpp.In.Height;
    uiInWidth  = uiWidth  = par->vpp.In.CropW;
    uiInHeight = uiHeight = par->vpp.In.CropH;
    if(par->vpp.In.FrameRateExtN && par->vpp.In.FrameRateExtD)
        dFrameRate = par->vpp.In.FrameRateExtN / par->vpp.In.FrameRateExtD;
    else
        dFrameRate = 30.0;

    bisInterlaced = false;
    bFullFrameRate = false;
    uiLastFrameNumber = 0;
    if(MFX_PICSTRUCT_UNKNOWN == par->vpp.In.PicStruct &&
       0 == par->vpp.In.FrameRateExtN && 0 == par->vpp.Out.FrameRateExtN)
    {
        //auto-detection mode, currently equal to reverse telecine mode
        bisInterlaced = false;
        bFullFrameRate = false;
    }
    else if((MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct ||
             MFX_PICSTRUCT_FIELD_BFF == par->vpp.In.PicStruct) &&
       (par->vpp.In.FrameRateExtN  == 30 && par->vpp.In.FrameRateExtD == 1 &&
        par->vpp.Out.FrameRateExtN == 24 && par->vpp.Out.FrameRateExtD == 1))
    {
        //reverse telecine mode
        bisInterlaced = false;
        bFullFrameRate = false;
    }
    else if(MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct ||
           MFX_PICSTRUCT_FIELD_BFF == par->vpp.In.PicStruct)
    {
        //Deinterlace only mode
        bisInterlaced = true;

        if(2 * par->vpp.In.FrameRateExtN * par->vpp.Out.FrameRateExtD ==
            par->vpp.In.FrameRateExtD * par->vpp.Out.FrameRateExtN)
        {
            //30i -> 60p mode
            bFullFrameRate = true;
        }
        else if(par->vpp.In.FrameRateExtN * par->vpp.Out.FrameRateExtD ==
            par->vpp.In.FrameRateExtD * par->vpp.Out.FrameRateExtN)
        {
            //30i -> 30p mode
            bFullFrameRate = false;
        }
        else
        {
            //any additional frame rate conversions are unsupported
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }
    else
    {
        //reverse telecine + additional frame rate conversion are unsupported
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    //PTIR's frames init
    try //try is useless here since frames allocated by malloc, but probably in future it could be changed to new
    {
        for(i = 0; i < LASTFRAME; i++)
        {
            Frame_Create(&frmIO[i], uiInWidth, uiInHeight, uiInWidth / 2, uiInHeight / 2, 0);
            frmBuffer[i] = frmIO + i;
            if(!frmIO[i].ucMem)
            {
                Close();
                return MFX_ERR_MEMORY_ALLOC;
            }
        }
        Frame_Create(&frmIO[LASTFRAME], uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 0);
        if(!frmIO[LASTFRAME].ucMem)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
    }
    catch(std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...) 
    { 
        return MFX_ERR_UNKNOWN;
    }

    bInited = true;

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Reset(mfxVideoParam *par)
{
    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Close()
{
    if(bInited)
    {
        try
        {
            for (mfxU32 i = 0; i <= LASTFRAME; ++i)
                Frame_Release(&frmIO[i]);
            bInited = false;

            return MFX_ERR_NONE;
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;;
        }
        catch(...) 
        { 
            return MFX_ERR_UNKNOWN;; 
        }
    }
    else
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
}
mfxStatus MFX_PTIR_Plugin::GetVideoParam(mfxVideoParam *par)
{
    return MFX_ERR_NONE;
}

inline mfxStatus MFX_PTIR_Plugin::PrepareTask(PTIR_Task *ptir_task, mfxThreadTask *task, mfxFrameSurface1 *surface_out)
{
    try
    {
        ptir_task = new PTIR_Task;
        memset(ptir_task, 0, sizeof(PTIR_Task));
        vTasks.push_back(ptir_task);
        *task = ptir_task;
        ptir_task->filled = true;
        ptir_task->surface_out = surface_out;
        m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &surface_out->Data);
    }
    catch(std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...) 
    { 
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
