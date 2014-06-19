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
#include "mfxvideo++int.h"
#include "hw_utils.h"

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
    :m_adapter(0)
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

    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_mfxpar, 0, sizeof(mfxVideoParam));
    ptir = 0;
    bEOS = false;
    bInited = false;
    par_accel = false;
    frmSupply = 0;
    prevSurf = 0;
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

//    b_firstFrameProceed = false;
    bInited     = false;
    in_expected = false;
    ptir = 0;

    return mfxRes;
}

mfxStatus MFX_PTIR_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;

    Close();
    if(ptir)
        delete ptir;

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

mfxStatus MFX_PTIR_Plugin::VPPFrameSubmitEx(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task)
{
    if(vTasks.size() != 0)
        return MFX_WRN_DEVICE_BUSY;
    if(UMC::UMC_OK != m_guard.TryLock())
        return MFX_WRN_DEVICE_BUSY;

    mfxStatus mfxSts = MFX_ERR_NONE;
    PTIR_Task *ptir_task = 0;
    if(NULL == surface_work)
    {
        m_guard.Unlock();
        in_expected = false;
        return MFX_ERR_NULL_PTR;
    }
    if(!surface_in)
        bEOS = true;
    else
        bEOS = false;

    if(!bEOS)
    {
        if(outSurfs.size())
        {
            //*surface_out = outSurfs.front();
            if(in_expected)
                addInSurf(surface_in);
            addWorkSurf(surface_work);

            if(inSurfs.size() < 6 && outSurfs.size() < 3)
            {
                m_guard.Unlock();
                in_expected = true;
                return MFX_ERR_MORE_DATA;
            }
            if(workSurfs.size() < 6 && outSurfs.size() < 3)
            {
                m_guard.Unlock();
                in_expected = false;
                return MFX_ERR_MORE_SURFACE;
            }

            mfxSts = PrepareTask(ptir_task, task, surface_out);
            if(mfxSts)
                return mfxSts;
            m_guard.Unlock();
            in_expected = false;
            return MFX_ERR_NONE;
        }
        else if(inSurfs.size() < 7)
        {
            if(in_expected)
                addInSurf(surface_in);
            addWorkSurf(surface_work);
            m_guard.Unlock();
            in_expected = true;
            return MFX_ERR_MORE_DATA;
        }
        else if((workSurfs.size() < 8 && !ptir->bFullFrameRate) || (workSurfs.size() < 14 && ptir->bFullFrameRate) )
        {
            if(in_expected)
                addInSurf(surface_in);
            addWorkSurf(surface_work);
            m_guard.Unlock();
            in_expected = false;
            return MFX_ERR_MORE_SURFACE;
        }
        else
        {
            if(in_expected)
                addInSurf(surface_in);
            addWorkSurf(surface_work);
            mfxSts = PrepareTask(ptir_task, task, surface_out);
            m_guard.Unlock();
            in_expected = false;
            return MFX_ERR_NONE;
        }
    } 
    else
    {
        if(inSurfs.size() && (inSurfs.size() != workSurfs.size() ))
        {
            addWorkSurf(surface_work);

            mfxSts = PrepareTask(ptir_task, task, surface_out);
            m_guard.Unlock();
            in_expected = false;
            return MFX_ERR_NONE;
        }
        else if(0 != ptir->uiCur || ptir->fqIn.iCount || outSurfs.size())
        {
            addWorkSurf(surface_work);
            mfxSts = PrepareTask(ptir_task, task, surface_out);
            if(mfxSts)
                return mfxSts;

            m_guard.Unlock();
            in_expected = false;
            return MFX_ERR_NONE;
        }
        else if(0 == ptir->fqIn.iCount && 0 == ptir->uiCur)
        {
            m_guard.Unlock();
            in_expected = true;
            return MFX_ERR_MORE_DATA;
        }
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
    if(inSurfs.size() == 0 && 0 == ptir->fqIn.iCount && 0 == ptir->uiCur && 0 == outSurfs.size())
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    bool beof = false;
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxFrameSurface1 *surface_out = 0;
    mfxFrameSurface1 *surface_outtt = 0;
    //surface_out = ptir_task->surface_out;
    //surface_out = GetFreeSurf(workSurfs);

    if(inSurfs.size() != 0 && outSurfs.size() < 2)
    {
        //process input frames until 1 output frame is generated
        for(std::vector<mfxFrameSurface1*>::iterator it = inSurfs.begin(); it != inSurfs.end(); ++it) {
            if(0 == workSurfs.size())
                break;
            if(!surface_out)
                surface_out = GetFreeSurf(workSurfs);
            if((*it == inSurfs.back()) && bEOS)
                beof = true;
            try
            {
                mfxSts = ptir->Process(*it, &surface_out, m_pmfxCore, &surface_outtt, beof);
            }
            catch(...)
            {
                mfxSts = MFX_ERR_DEVICE_FAILED;
                return mfxSts;
            }

            //m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &(*it)->Data);
            prevSurf = *it;
            *it = 0;
            if(MFX_ERR_NONE == mfxSts)
            {
                //outSurfs.push_back(surface_outtt);
                //m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
                surface_out = 0;
                if(0 == workSurfs.size())
                    break;
            }
            else if(MFX_ERR_MORE_SURFACE == mfxSts)
            {
                //m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
                surface_out = 0;
                mfxSts = MFX_ERR_NONE;
                if(/*IsSW* &&*/ workSurfs.size() && inSurfs.size())
                    mfxSts = MFX_ERR_NONE;
                else
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
    else if(outSurfs.size() != 0)
    {
        ;//do fucking nothing
    }
    ////else
    //if((!(inSurfs.size() != 0 && outSurfs.size() < 2) && !(outSurfs.size() != 0)) ||
    //    (bEOS && outSurfs.size() == 1 && workSurfs.size() > 0))
    //{
    //    //get cached frame
    //    surface_out = GetFreeSurf(workSurfs);
    //    try
    //    {
    //        mfxSts = ptir->Process(0, &surface_out, m_pmfxCore, &surface_outtt, bEOS);
    //    }
    //    catch(...)
    //    {
    //        mfxSts = MFX_ERR_UNKNOWN;
    //        return mfxSts;
    //    }
    //    if(MFX_ERR_NONE == mfxSts)
    //    {
    //        //outSurfs.push_back(surface_outtt);
    //        //m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
    //        surface_out = 0;
    //    }
    //    else if(MFX_ERR_MORE_SURFACE == mfxSts)
    //    {
    //        m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
    //        surface_out = 0;
    //        mfxSts = MFX_ERR_NONE;
    //    }
    //    else if(MFX_ERR_MORE_DATA == mfxSts)
    //    {
    //        ;
    //    }
    //    else
    //    {
    //        assert(0);
    //        return mfxSts;
    //    }
    //}

    //if(surface_out)
    //{
    //    //smth wrong, execute should always generate output!
    //    assert(0);
    //    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
    //    surface_out = 0;
    //    return MFX_ERR_UNDEFINED_BEHAVIOR;
    //}

    //m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &outSurfs.front()->Data);
    //outSurfs.erase(outSurfs.begin());
    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    assert(0 != vTasks.size());
    assert(0 != outSurfs.size());
    if(0 == vTasks.size() || 0 == outSurfs.size())
        return MFX_ERR_UNKNOWN;

    for(mfxU32 i=0; i < vTasks.size(); i++){
        if (vTasks[i] == (PTIR_Task*) task)
        {
            m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &outSurfs.front()->Data);
            outSurfs.erase(outSurfs.begin());
            delete vTasks[i];
            vTasks.erase(vTasks.begin() + i);
        }
    }

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

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

    if(!(in->IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY )||
         in->IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)))
    {
        //other IOPatterns temporary unsupported
        in->IOPattern = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    if(in->ExtParam || in->NumExtParam)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->Protected)
    {
        out->Protected = 0;
        return MFX_ERR_UNSUPPORTED;
    }

    if(in->vpp.Out.PicStruct == in->vpp.In.PicStruct)
    {
        out->vpp.Out.PicStruct = 0;
        out->vpp.In.PicStruct = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    // auto-detection mode is allowed only for unknown picstruct and unknown input frame rate
    if(in->vpp.In.PicStruct == MFX_PICSTRUCT_UNKNOWN && 
       (in->vpp.In.FrameRateExtN || in->vpp.In.FrameRateExtD || in->vpp.Out.FrameRateExtN || in->vpp.Out.FrameRateExtD))
    {
        out->vpp.In.PicStruct = 0; // MFX_PICSTRUCT_UNKNOWN == 0, so that 0 is meaningless here
        in->vpp.In.FrameRateExtN = 0;
        in->vpp.In.FrameRateExtD = 0;
        in->vpp.Out.FrameRateExtN = 0;
        in->vpp.Out.FrameRateExtD = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->vpp.Out.PicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        out->vpp.Out.PicStruct = 0; // MFX_PICSTRUCT_UNKNOWN == 0, so that 0 is meaningless here
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
    if(in->vpp.In.FourCC != MFX_FOURCC_NV12)
    {
        out->vpp.In.FourCC = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->vpp.In.FourCC != in->vpp.Out.FourCC)
    {
        out->vpp.Out.FourCC = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->vpp.In.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
    {
        out->vpp.In.ChromaFormat = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    if(in->vpp.In.ChromaFormat != in->vpp.Out.ChromaFormat)
    {
        out->vpp.Out.ChromaFormat = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    
    //find suitable mode
    if(MFX_PICSTRUCT_UNKNOWN == in->vpp.In.PicStruct &&
       0 == in->vpp.In.FrameRateExtN && 0 == in->vpp.Out.FrameRateExtN)
    {
        ;//auto-detection mode
    }
    else if((MFX_PICSTRUCT_FIELD_TFF == in->vpp.In.PicStruct ||
             MFX_PICSTRUCT_FIELD_BFF == in->vpp.In.PicStruct) &&
       (4 * in->vpp.In.FrameRateExtN * in->vpp.In.FrameRateExtD ==
        5 * in->vpp.Out.FrameRateExtN * in->vpp.Out.FrameRateExtD))
    {
        ;//reverse telecine mode
    }
    else if(MFX_PICSTRUCT_FIELD_TFF == in->vpp.In.PicStruct ||
           MFX_PICSTRUCT_FIELD_BFF == in->vpp.In.PicStruct)
    {
        ;//Deinterlace only mode
        if(2 * in->vpp.In.FrameRateExtN * in->vpp.Out.FrameRateExtD ==
            in->vpp.In.FrameRateExtD * in->vpp.Out.FrameRateExtN)
        {
            ;//30i -> 60p mode
        }
        else if(in->vpp.In.FrameRateExtN * in->vpp.Out.FrameRateExtD ==
            in->vpp.In.FrameRateExtD * in->vpp.Out.FrameRateExtN)
        {
            ;//30i -> 30p mode
        }
        else
        {
            if(in->vpp.In.FrameRateExtD &&
               (30 != in->vpp.In.FrameRateExtN / in->vpp.In.FrameRateExtD))
            {
                in->vpp.In.FrameRateExtN = 0;
                in->vpp.In.FrameRateExtD = 0;
            }
            if(in->vpp.Out.FrameRateExtD &&
               ((30 != in->vpp.Out.FrameRateExtN / in->vpp.Out.FrameRateExtD) && (60 != in->vpp.Out.FrameRateExtN / in->vpp.Out.FrameRateExtD)))
            {
                out->vpp.Out.FrameRateExtN = 0;
                out->vpp.Out.FrameRateExtD = 0;
            }
            ;//any additional frame rate conversions are unsupported
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
        //reverse telecine + additional frame rate conversion are unsupported
        out->vpp.In.FrameRateExtN = 0;
        out->vpp.In.FrameRateExtD = 0;
        out->vpp.Out.FrameRateExtN = 0;
        out->vpp.Out.FrameRateExtD = 0;

        return MFX_ERR_UNSUPPORTED;
    }

    //Check partial acceleration
    bool p_accel = false;
    mfxHandleType mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
    mfxHDL mfxDeviceHdl;
    mfxCoreParam mfxCorePar;
    mfxSts = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;
    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(mfxCorePar.Impl))
    {
        mfxSts = GetHandle(mfxDeviceHdl, mfxDeviceType);
        if(MFX_ERR_NONE > mfxSts)
            return mfxSts;
        bool HWSupported = false;
        eMFXHWType HWType = MFX_HW_UNKNOWN;
        mfxSts = GetHWTypeAndCheckSupport(mfxCorePar.Impl, mfxDeviceHdl, HWType, HWSupported, p_accel);
        if(MFX_ERR_NONE > mfxSts)
            return mfxSts;
    }


    if(bWarnIsNeeded)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    else if(p_accel)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else
        return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    bool bSWLib = true;

    mfxStatus mfxSts = MFX_ERR_NONE;
    //Check partial acceleration
    mfxHandleType mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
    mfxHDL mfxDeviceHdl;
    mfxCoreParam mfxCorePar;
    mfxSts = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;
    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(mfxCorePar.Impl))
    {
        mfxSts = GetHandle(mfxDeviceHdl, mfxDeviceType);
        if(MFX_ERR_NONE > mfxSts)
            return mfxSts;
        bool HWSupported = false;
        eMFXHWType HWType = MFX_HW_UNKNOWN;
        mfxSts = GetHWTypeAndCheckSupport(mfxCorePar.Impl, mfxDeviceHdl, HWType, HWSupported, par_accel);
        if(MFX_ERR_NONE > mfxSts)
            return mfxSts;
        if(!par_accel)
            bSWLib = false;
    }

    in->Info = par->vpp.In;
    out->Info = par->vpp.Out;
    mfxU16 AsyncDepth = 1;
    if(par->AsyncDepth)
        AsyncDepth = par->AsyncDepth;

    in->NumFrameMin = 24 * AsyncDepth;
    in->NumFrameSuggested = 26 * AsyncDepth;
    //in->Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;

    out->NumFrameMin = 24 * AsyncDepth;
    out->NumFrameSuggested = 26 * AsyncDepth;
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

    if(par_accel)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else
        return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Init(mfxVideoParam *par)
{
    if(!par)
        return MFX_ERR_NULL_PTR;
    if(bInited || ptir)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxStatus mfxSts;

    mfxSts = Query(par, 0);
    if( !(MFX_ERR_NONE == mfxSts || MFX_WRN_PARTIAL_ACCELERATION == mfxSts) )
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxHandleType mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
    mfxHDL mfxDeviceHdl;

    mfxCoreParam mfxCorePar;
    mfxSts = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;
    mfxSts = GetHandle(mfxDeviceHdl, mfxDeviceType);
    if(MFX_ERR_NONE != mfxSts &&
       MFX_IMPL_SOFTWARE != MFX_IMPL_BASETYPE(mfxCorePar.Impl))
        return mfxSts;

    bool HWSupported = false;
    eMFXHWType HWType = MFX_HW_UNKNOWN;

    mfxSts = GetHWTypeAndCheckSupport(mfxCorePar.Impl, mfxDeviceHdl, HWType, HWSupported, par_accel);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;

    try
    {
        frmSupply = new frameSupplier(&inSurfs, &workSurfs, &outSurfs, 0, 0, m_pmfxCore);
    }
    catch(std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    try
    {
        if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(mfxCorePar.Impl) && HWSupported )
        {
            ptir = new PTIR_ProcessorCM(m_pmfxCore, frmSupply, HWType);
        } 
        else if(MFX_IMPL_SOFTWARE == MFX_IMPL_BASETYPE(mfxCorePar.Impl) || par_accel )
        {
            ptir = new PTIR_ProcessorCPU(m_pmfxCore, frmSupply);
        }
        else
        {
            delete frmSupply;
            frmSupply = 0;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }
    catch(std::bad_alloc&)
    {
        delete frmSupply;
        frmSupply = 0;
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...)
    {
        delete frmSupply;
        frmSupply = 0;
        return MFX_ERR_UNKNOWN;
    }

    mfxSts = ptir->Init(par);
    if(mfxSts)
        return mfxSts;

    bInited = true;

    m_mfxpar = *par;

    if(par_accel)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else
        return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Reset(mfxVideoParam *par)
{
    if(!bInited)
        return MFX_ERR_NOT_INITIALIZED;
    if(!par)
        return MFX_ERR_NULL_PTR;

    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus mfxWrn = MFX_ERR_NONE;
    mfxSts = Query(par, 0);
    if((MFX_WRN_PARTIAL_ACCELERATION == mfxSts) && par_accel)
        mfxSts = MFX_ERR_NONE;
    else if(MFX_ERR_NONE > mfxSts)
        return mfxSts;
    else if(MFX_ERR_NONE < mfxSts)
        mfxWrn = mfxSts;

    mfxSts = Close();
    if(MFX_ERR_NONE >= mfxSts)
    {
        mfxSts = Init(par);
        if(mfxSts)
            mfxWrn = mfxSts;
    }

    return mfxWrn;
}
mfxStatus MFX_PTIR_Plugin::Close()
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    if(ptir)
    {
        mfxSts = ptir->Close();
        delete ptir;
        ptir = 0;
    }
    if(frmSupply)
    {
        delete frmSupply;
        frmSupply = 0;
    }
    bInited = false;
    if(mfxSts)
        return mfxSts;
    return MFX_ERR_NONE;
}

mfxStatus MFX_PTIR_Plugin::GetVideoParam(mfxVideoParam *par)
{
    return MFX_ERR_NONE;
}

inline mfxStatus MFX_PTIR_Plugin::PrepareTask(PTIR_Task *ptir_task, mfxThreadTask *task, mfxFrameSurface1 **surface_out)
{
    try
    {
        ptir_task = new PTIR_Task;
        memset(ptir_task, 0, sizeof(PTIR_Task));
        vTasks.push_back(ptir_task);
        *task = ptir_task;
        ptir_task->filled = true;
        if(outSurfs.size())
            *surface_out = outSurfs.front();
        else
            *surface_out = workSurfs.front();
        ptir_task->surface_out = *surface_out;
        //m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &surface_out->Data);
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

inline mfxFrameSurface1* MFX_PTIR_Plugin::GetFreeSurf(std::vector<mfxFrameSurface1*>& vSurfs)
{
    mfxFrameSurface1* s_out;
    if(0 == vSurfs.size())
        return 0;
    else
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = vSurfs.begin(); it != vSurfs.end(); ++it) {
            if((*it)->Data.Locked < 4)
            {
                s_out = *it;
                vSurfs.erase(it);
                return s_out;
            }
        }
    }
    return 0;
}

inline mfxStatus MFX_PTIR_Plugin::addWorkSurf(mfxFrameSurface1* pSurface)
{
    if(!pSurface)
        return MFX_ERR_NONE;
    if(workSurfs.end() == std::find (workSurfs.begin(), workSurfs.end(), pSurface) && (workSurfs.size() + ptir->fqIn.iCount + ptir->uiCur) < 18)
    {
        workSurfs.push_back(pSurface);
        m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &pSurface->Data);
        return MFX_ERR_NONE;
    }
    return MFX_ERR_NONE;
}
inline mfxStatus MFX_PTIR_Plugin::addInSurf(mfxFrameSurface1* pSurface)
{
    if(!pSurface)
        return MFX_ERR_NONE;
    if(pSurface && ((inSurfs.size() == 0) || (inSurfs.size() != 0 && inSurfs.back() != pSurface)) /*&& prevSurf != pSurface*/ && (inSurfs.size() + ptir->fqIn.iCount + ptir->uiCur) < 18)
    {
        if(inSurfs.end() == std::find (inSurfs.begin(), inSurfs.end(), pSurface) || inSurfs.size() == 0)
        {
            inSurfs.push_back(pSurface);
            m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &pSurface->Data);
        }
        else
            assert(false == true);
    }
    return MFX_ERR_NONE;
}

inline mfxStatus MFX_PTIR_Plugin::GetHandle(mfxHDL& mfxDeviceHdl, mfxHandleType& mfxDeviceType)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxCoreParam mfxCorePar;
    mfxSts = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
    if(mfxSts) return mfxSts;
    if(mfxCorePar.Impl & MFX_IMPL_VIA_D3D9)
        mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
    else if(mfxCorePar.Impl & MFX_IMPL_VIA_D3D11)
        mfxDeviceType = MFX_HANDLE_D3D11_DEVICE;
    else if(mfxCorePar.Impl & MFX_IMPL_VIA_VAAPI)
        mfxDeviceType = MFX_HANDLE_VA_DISPLAY;
    mfxSts = m_pmfxCore->GetHandle(m_pmfxCore->pthis, mfxDeviceType, &mfxDeviceHdl);
    if(MFX_ERR_NONE != mfxSts &&
       MFX_IMPL_SOFTWARE != MFX_IMPL_BASETYPE(mfxCorePar.Impl))
        return mfxSts;

    return MFX_ERR_NONE;
}

inline mfxStatus MFX_PTIR_Plugin::GetHWTypeAndCheckSupport(mfxIMPL& impl, mfxHDL& mfxDeviceHdl, eMFXHWType& HWType, bool& HWSupported, bool& par_accel)
{
    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl))
    {
        HWType = GetHWType(1,impl,&mfxDeviceHdl);
        switch (HWType)
        {
        //case MFX_HW_BDW:
        case MFX_HW_HSW_ULT:
        case MFX_HW_HSW:
        case MFX_HW_IVB:
        //case MFX_HW_VLV:
            HWSupported = true;
            par_accel = false;
            break;
        default:
            HWSupported = false;
            par_accel = true;
            break;
        }

        return MFX_ERR_NONE;
    }
    par_accel = false;
    HWSupported = false;

    return MFX_ERR_NONE;
}