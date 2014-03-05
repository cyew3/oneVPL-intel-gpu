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

    out_surf = 0;

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
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus mfxCCSts = MFX_ERR_NONE;
    if(NULL == surface_out)
        return MFX_ERR_NULL_PTR;

    if(NULL != surface_in)
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 work420_surface;
        memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
        memcpy(&(work420_surface.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        work420_surface.Info.FourCC = MFX_FOURCC_I420;
        mfxU16& w = work420_surface.Info.Width;
        mfxU16& h = work420_surface.Info.Height;
        //work420_surface.Data.Y = (mfxU8*) malloc (w*h*3/2);
        //work420_surface.Data.Y = frmIO[i].plaY.ucData;
        work420_surface.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        work420_surface.Data.U = work420_surface.Data.Y+w*h;
        work420_surface.Data.V = work420_surface.Data.U+w*h/4;
        work420_surface.Data.Pitch = w;

        mfxFrameSurface1 surf_out;
        memset(&surf_out, 0, sizeof(mfxFrameSurface1));
        memcpy(&(surf_out.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        surf_out.Info.FourCC = MFX_FOURCC_I420;
        //mfxU16& w = surf_out.Info.Width;
        //mfxU16& h = surf_out.Info.Height;
        //surf_out.Data.Y = (mfxU8*) malloc (w*h*3/2);
        surf_out.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        surf_out.Data.U = surf_out.Data.Y+w*h;
        surf_out.Data.V = surf_out.Data.U+w*h/4;
        surf_out.Data.Pitch = w;

        if(surface_in->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        mfxCCSts = ColorSpaceConversionWCopy(surface_in, &work420_surface, work420_surface.Info.FourCC);
        assert(!mfxCCSts);
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;


        mfxSts = PTIR_ProcessFrame( &work420_surface, &surf_out );

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            PTIR_Task *ptir_task = new PTIR_Task;
            *task = ptir_task;
            mfxCCSts = ColorSpaceConversionWCopy(&surf_out, surface_out, surface_out->Info.FourCC);
            assert(!mfxCCSts);

        }
        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }
        return mfxSts;
    }
    else if(0 == fqIn.iCount && 0 == uiCur)
    {
        return MFX_ERR_MORE_DATA;
    }
    else
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 surf_out;
        memset(&surf_out, 0, sizeof(mfxFrameSurface1));
        memcpy(&(surf_out.Info), &(surface_out->Info), sizeof(mfxFrameInfo));
        mfxU16& w = surf_out.Info.Width;
        mfxU16& h = surf_out.Info.Height;
        surf_out.Info.FourCC = MFX_FOURCC_I420;
        //mfxU16& w = surf_out.Info.Width;
        //mfxU16& h = surf_out.Info.Height;
        //surf_out.Data.Y = (mfxU8*) malloc (w*h*3/2);
        surf_out.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        surf_out.Data.U = surf_out.Data.Y+w*h;
        surf_out.Data.V = surf_out.Data.U+w*h/4;
        surf_out.Data.Pitch = w;

        if(surface_out->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            isUnlockReq = true;
        }

        mfxSts = PTIR_ProcessFrame( 0, &surf_out );

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            PTIR_Task *ptir_task = new PTIR_Task;
            *task = ptir_task;
            mfxCCSts = ColorSpaceConversionWCopy(&surf_out, surface_out, surface_out->Info.FourCC);
            assert(!mfxCCSts);

        }
        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            isUnlockReq = false;
        }
        return mfxSts;
    }

    return mfxSts;
}
mfxStatus MFX_PTIR_Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
{
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
    if(!in)
        return MFX_ERR_NULL_PTR;

    mfxVideoParam tmp_param;
    if(!out)
        out = &tmp_param;

    memcpy(&out->vpp, &in->vpp, sizeof(mfxInfoVPP));
    in->AsyncDepth = out->AsyncDepth;
    in->IOPattern  = out->IOPattern;

    if(in->vpp.Out.PicStruct == in->vpp.In.PicStruct)
    {
        out->vpp.Out.PicStruct = 0;
        out->vpp.In.PicStruct = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->vpp.In.PicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        out->vpp.In.PicStruct = 65535; // MFX_PICSTRUCT_UNKNOWN == 0, so that 0 is meaningless here
        return MFX_ERR_UNSUPPORTED;
    }
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

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    bool bSWLib = true;

    in->Info = par->vpp.In;
    out->Info = par->vpp.Out;

    in->NumFrameMin = 6;
    in->NumFrameSuggested = 6;
    //in->Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;

    out->NumFrameMin = 6;
    out->NumFrameSuggested = 6;
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
    if(mfxSts)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    uiInWidth  = uiWidth  = par->vpp.In.Width;
    uiInHeight = uiHeight = par->vpp.In.Height;
    dFrameRate = par->vpp.In.FrameRateExtN / par->vpp.In.FrameRateExtD;

    bisInterlaced = false;
    bFullFrameRate = false;
    if((MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct ||
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
        }
        Frame_Create(&frmIO[LASTFRAME], uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 0);
    }
    catch(std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;;
    }
    catch(...) 
    { 
        return MFX_ERR_UNKNOWN;; 
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