/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.cpp

\* ****************************************************************************** */

#include "ptir_vpp_plugin.h"
#include "mfxstructures.h"
//#include "mfx_session.h"
#include "vm_sys_info.h"

#include "mfx_plugin_module.h"

#include "plugin_version_linux.h"
#include "mfxvideo++int.h"
#include "hw_utils.h"

#ifndef ALIGN16
#define ALIGN16(SZ) (((SZ + 15) >> 4) << 4)
#endif
#ifndef ALIGN32
#define ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#endif

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
    memset(&m_mfxInitPar, 0, sizeof(mfxVideoParam));
    memset(&m_mfxCurrentPar, 0, sizeof(mfxVideoParam));
    memset(&m_OpaqSurfAlloc, 0, sizeof(mfxExtOpaqueSurfaceAlloc));
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
    if(!task)
        return MFX_ERR_NULL_PTR;
    if(vTasks.size() != 0)
        return MFX_WRN_DEVICE_BUSY;
    if(!ptir)
        return MFX_ERR_NOT_INITIALIZED;
    if(UMC::UMC_OK != m_guard.TryLock())
        return MFX_WRN_DEVICE_BUSY;

    mfxStatus mfxSts = MFX_ERR_NONE;
    PTIR_Task *ptir_task = 0;

    if(surface_in && in_expected)
    {
        mfxSts = CheckInFrameSurface1(surface_in, surface_work);
        if(mfxSts)
        {
            m_guard.Unlock();
            in_expected = false;
            return mfxSts;
        }
    }
    mfxSts = CheckOutFrameSurface1(surface_work);
    if(mfxSts)
    {
        m_guard.Unlock();
        in_expected = false;
        return mfxSts;
    }

    if((!surface_in && in_expected) || bEOS)
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
            m_guard.Unlock();
            if(mfxSts)
                return mfxSts;
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
        if(ptir->bFullFrameRate && inSurfs.size() && workSurfs.size() < (2 * inSurfs.size()))
        {
            addWorkSurf(surface_work);
            m_guard.Unlock();
            in_expected = false;
            return MFX_ERR_MORE_SURFACE;
        }
        else if(inSurfs.size() && (inSurfs.size() != workSurfs.size() ))
        {
            addWorkSurf(surface_work);

            mfxSts = PrepareTask(ptir_task, task, surface_out);
            m_guard.Unlock();
            in_expected = false;
            return MFX_ERR_NONE;
        }
        else if(0 != ptir->Env.control.uiCur || ptir->Env.fqIn.iCount || outSurfs.size())
        {
            addWorkSurf(surface_work);
            mfxSts = PrepareTask(ptir_task, task, surface_out);
            m_guard.Unlock();
            if(mfxSts)
                return mfxSts;
            in_expected = false;
            return MFX_ERR_NONE;
        }
        else if(0 == ptir->Env.fqIn.iCount && 0 == ptir->Env.control.uiCur)
        {
            m_guard.Unlock();
            in_expected = true;
            if(bEOS)
                bEOS = false;
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
    //if(inSurfs.size() == 0 && 0 == ptir->fqIn.iCount && 0 == ptir->uiCur && 0 == outSurfs.size())
    //    return MFX_ERR_UNDEFINED_BEHAVIOR;

    bool beof = false;
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxFrameSurface1 *surface_out = 0;
    mfxFrameSurface1 *surface_outtt = 0;
    mfxFrameSurface1 *exp_surf = 0;
    if(outSurfs.size() == 0 && b_work)
        exp_surf = ptir_task->surface_out;

    if(inSurfs.size() != 0 && outSurfs.size() < 2)
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = inSurfs.begin(); it != inSurfs.end(); ++it) {
            if(0 == workSurfs.size())
                break;
            if(10 < outSurfs.size())
                break;
            if(!surface_out)
                surface_out = GetFreeSurf(workSurfs);
            if(exp_surf == surface_out && ptir->isHW)
                surface_out = GetFreeSurf(workSurfs);
            if((*it == inSurfs.back()) && bEOS)
                beof = true;
            try
            {
                mfxSts = ptir->Process(*it, &surface_out, m_pmfxCore, &surface_outtt, beof, exp_surf);
            }
            catch(...)
            {
                mfxSts = MFX_ERR_DEVICE_FAILED;
                return mfxSts;
            }

            prevSurf = *it;
            *it = 0;
            if(MFX_ERR_NONE == mfxSts)
            {
                surface_out = 0;
                //lets try to minimize latency
                //break;
                if(0 == workSurfs.size())
                    break;
            }
            else if(MFX_ERR_MORE_SURFACE == mfxSts)
            {
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
            if(outSurfs.size())
                exp_surf = 0;
        }
        while(0 == *inSurfs.begin())
        {
            //remove the processed head of the queue
            inSurfs.erase(inSurfs.begin());
            if(0 == inSurfs.size())
                break;
        }
    }
    else if(bEOS && !inSurfs.size() && ptir->Env.control.uiCur && outSurfs.size() < 2)
    {
        if(!surface_out)
            surface_out = GetFreeSurf(workSurfs);
        beof = true;
        try
        {
            mfxSts = ptir->Process(0, &surface_out, m_pmfxCore, &surface_outtt, beof, exp_surf);
        }
        catch(...)
        {
            mfxSts = MFX_ERR_DEVICE_FAILED;
            return mfxSts;
        }
    }
    if(!ptir->isHW && surface_out)
    {
        mfxSts = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_out->Data);
        if(mfxSts)
            return mfxSts;
    }

#if defined(_DEBUG)
    for(std::vector<mfxFrameSurface1*>::iterator it = outSurfs.begin(); it != outSurfs.end(); ++it)
    {
        assert((*it)->Data.Locked != 0);
    }
#endif

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    UMC::AutomaticUMCMutex guard(m_guard_free);
    assert(0 != vTasks.size());
    //assert(0 != outSurfs.size());
    if(0 == vTasks.size() && bInited/*|| 0 == outSurfs.size()*/)
        return MFX_ERR_UNKNOWN;

    for(mfxU32 i=0; i < vTasks.size(); i++){
        if (vTasks[i] == (PTIR_Task*) task)
        {
            m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &vTasks[i]->surface_out->Data);
            //m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &outSurfs.front()->Data);
            if(b_work)
            {
                outSurfs.erase(std::remove(outSurfs.begin(), outSurfs.end(), vTasks[i]->surface_out), outSurfs.end());
            }
            delete vTasks[i];
            vTasks.erase(vTasks.begin() + i);
        }
    }
#if defined(_DEBUG)
    for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs.begin(); it != workSurfs.end(); ++it)
    {
        assert((*it)->Data.Locked != 0);
    }
    for(std::vector<mfxFrameSurface1*>::iterator it = outSurfs.begin(); it != outSurfs.end(); ++it)
    {
        assert((*it)->Data.Locked != 0);
    }
#endif

    return MFX_ERR_NONE;
}
mfxStatus MFX_PTIR_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    bool warning     = false;
    bool error       = false;
    bool p_accel     = false;
    bool HWSupported = false;
    bool opaque      = false;
    bool big_resol   = false;
    bool buffer_mode = false;
    if(!in && !out)
        return MFX_ERR_NULL_PTR;

    mfxVideoParam tmp_param;
    memset(&tmp_param, 0, sizeof(tmp_param));
    if(!out)
        out = &tmp_param;

    if(in)
    {
        const mfxVideoParam& const_in = *in;
        ptir_memcpy_s(&out->vpp, sizeof(out->vpp), &const_in.vpp, sizeof(const_in.vpp));
        if(const_in.AsyncDepth == 0 || const_in.AsyncDepth == 1)
        {
            out->AsyncDepth = const_in.AsyncDepth;
        }
        else
        {
            out->AsyncDepth = 0;
            error = true;
        }
        out->IOPattern  = const_in.IOPattern;

        //if opaque
        if(const_in.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
           const_in.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
           const_in.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY ) ||
           const_in.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
           const_in.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_OPAQUE_MEMORY)   )
        {
            //check that opaque buffer is present -- probably check is needed only at Init()?
            opaque = true;
        }
        //if not opaque check for any other applicable IOPattern
        else if(!(const_in.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY )||
                  const_in.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)||
                  const_in.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_SYSTEM_MEMORY)||
                  const_in.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY )  ))
        {
            out->IOPattern = 0;
            error = true;
        }

        if(const_in.ExtParam || const_in.NumExtParam)
        {
            if(!const_in.ExtParam || !const_in.NumExtParam)
            {
                error = true;
            }
            else if(out != &m_mfxInitPar &&
               ((const_in.NumExtParam != out->NumExtParam) || !out->ExtParam))
            {
                //input and output ext buffers must match
                error = true;
            }
            else
            {
                mfxExtVPPDeinterlacing* extVPPDeintIn = 0;
                mfxExtVPPDeinterlacing* extVPPDeintOut = 0;
                mfxExtOpaqueSurfaceAlloc* in_opaq_buf = 0;
                mfxExtOpaqueSurfaceAlloc* out_opaq_buf = 0;
                for(mfxU32 i = 0; i < const_in.NumExtParam; ++i)
                {
                    if(const_in.ExtParam[i])
                    {
                        if( const_in.ExtParam[i]->BufferId == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION &&
                            const_in.ExtParam[i]->BufferSz == sizeof(mfxExtOpaqueSurfaceAlloc))
                        {
                            if(opaque)
                            {
                                in_opaq_buf = (mfxExtOpaqueSurfaceAlloc*) const_in.ExtParam[i];
                            }
                            else
                            {
                                error = true;
                                out->IOPattern = 0;
                            }
                        }
                        else if( const_in.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING &&
                                 const_in.ExtParam[i]->BufferSz == sizeof(mfxExtVPPDeinterlacing))
                        {
                            extVPPDeintIn = (mfxExtVPPDeinterlacing*) const_in.ExtParam[i];
                        }
                        else
                        {
                            error = true; //unknown ext buffer
                        }
                    }
                    if(out != &m_mfxInitPar && out->ExtParam[i])
                    {
                        if( out->ExtParam[i]->BufferId == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION &&
                            out->ExtParam[i]->BufferSz == sizeof(mfxExtOpaqueSurfaceAlloc))
                        {
                            if(opaque)
                            {
                                out_opaq_buf = (mfxExtOpaqueSurfaceAlloc*) out->ExtParam[i];
                            }
                            else
                            {
                                out->IOPattern = 0;
                                error = true;
                            }
                        }
                        else if( out->ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING &&
                                 out->ExtParam[i]->BufferSz == sizeof(mfxExtVPPDeinterlacing))
                        {
                            extVPPDeintOut = (mfxExtVPPDeinterlacing*) out->ExtParam[i];
                        }
                        else
                        {
                            error = true; //unknown ext buffer
                        }
                    }
                }
                if(extVPPDeintIn)
                {
                    if(extVPPDeintOut)
                        *extVPPDeintOut = *extVPPDeintIn;
                    buffer_mode = true;
                    if( extVPPDeintIn->Mode == MFX_DEINTERLACING_AUTO_DOUBLE            ||
                        extVPPDeintIn->Mode == MFX_DEINTERLACING_AUTO_SINGLE            ||
                        extVPPDeintIn->Mode == MFX_DEINTERLACING_FULL_FR_OUT            ||
                        extVPPDeintIn->Mode == MFX_DEINTERLACING_HALF_FR_OUT            ||
                        extVPPDeintIn->Mode == MFX_DEINTERLACING_24FPS_OUT              ||
                        extVPPDeintIn->Mode == MFX_DEINTERLACING_30FPS_OUT              ||
                        extVPPDeintIn->Mode == MFX_DEINTERLACING_DETECT_INTERLACE        )
                    {
                        if(extVPPDeintIn->TelecineLocation || extVPPDeintIn->TelecinePattern)
                        {
                            error = true;
                            if(extVPPDeintOut)
                            {
                                extVPPDeintOut->Mode = extVPPDeintOut->TelecineLocation = extVPPDeintOut->TelecinePattern = 0;
                            }
                        }
                    }
                    else if(extVPPDeintIn->Mode == MFX_DEINTERLACING_FIXED_TELECINE_PATTERN)
                    {
                        if(extVPPDeintIn->TelecinePattern == MFX_TELECINE_PATTERN_32           ||
                           extVPPDeintIn->TelecinePattern == MFX_TELECINE_PATTERN_2332         ||
                           extVPPDeintIn->TelecinePattern == MFX_TELECINE_PATTERN_FRAME_REPEAT ||
                           extVPPDeintIn->TelecinePattern == MFX_TELECINE_PATTERN_41           )
                        {
                            ;//ok
                        }
                        else if(extVPPDeintIn->TelecinePattern == MFX_TELECINE_POSITION_PROVIDED)
                        {
                            if(extVPPDeintIn->TelecineLocation > 4)
                            {
                                error = true;
                                if(extVPPDeintOut)
                                {
                                    extVPPDeintOut->TelecineLocation = 0;
                                }
                            }
                        }
                        else
                        {
                            error = true;
                            if(extVPPDeintOut)
                            {
                                extVPPDeintOut->TelecineLocation = extVPPDeintOut->TelecinePattern = 0;
                            }
                        }
                    }
                    else
                    {
                        error = true;
                        if(extVPPDeintOut)
                        {
                            extVPPDeintOut->Mode = extVPPDeintOut->TelecineLocation = extVPPDeintOut->TelecinePattern = 0;
                        }
                    }
                }
                if(in_opaq_buf)
                {
                    bool bOpaqMode[2];
                    mfxSts = CheckOpaqMode(const_in, out, *in_opaq_buf, out_opaq_buf, bOpaqMode);
                    if(mfxSts)
                        error = true;
                }
            }
        }
        else if (opaque)
        {
            error = true;
        }

        //if(opaque && (const_in.NumExtParam > 1 || out->NumExtParam > 1))
        //{
        //    error = true;
        //}
        if(const_in.Protected)
        {
            out->Protected = 0;
            error = true;
        }

        if(!buffer_mode)
        {
            if(const_in.vpp.Out.PicStruct == const_in.vpp.In.PicStruct)
            {
                out->vpp.Out.PicStruct = 0;
                out->vpp.In.PicStruct = 0;
                error = true;
            }
            if(MFX_PICSTRUCT_UNKNOWN == const_in.vpp.In.PicStruct)
            {
               if (!((0 == const_in.vpp.In.FrameRateExtN &&
                   0 != const_in.vpp.Out.FrameRateExtD &&
                   30 == (const_in.vpp.Out.FrameRateExtN / const_in.vpp.Out.FrameRateExtD)
                   ) ||
                   (0 == const_in.vpp.In.FrameRateExtN &&
                   0 != const_in.vpp.Out.FrameRateExtD &&
                   60 == (const_in.vpp.Out.FrameRateExtN / const_in.vpp.Out.FrameRateExtD)
                   )))
                {
                    out->vpp.In.PicStruct = 0; // MFX_PICSTRUCT_UNKNOWN == 0, so that 0 is meaningless here
                    out->vpp.In.FrameRateExtN = 0;
                    out->vpp.In.FrameRateExtD = 0;
                    out->vpp.Out.FrameRateExtN = 0;
                    out->vpp.Out.FrameRateExtD = 0;
                    error = true;
                }
            }
            if(const_in.vpp.Out.PicStruct == MFX_PICSTRUCT_UNKNOWN)
            {
                out->vpp.Out.PicStruct = 0; // MFX_PICSTRUCT_UNKNOWN == 0, so that 0 is meaningless here
                error = true;
            }
        }
        if(const_in.vpp.Out.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        {
            out->vpp.Out.PicStruct = 0;
            error = true;
        }

        if(const_in.vpp.In.Width  != const_in.vpp.Out.Width)
        {
            out->vpp.Out.Width = 0;
            error = true;
        }
        if(const_in.vpp.In.Height != const_in.vpp.Out.Height)
        {
            out->vpp.Out.Height = 0;
            error = true;
        }
        if(const_in.vpp.In.CropX  != const_in.vpp.Out.CropX)
        {
            out->vpp.Out.CropX = 0;
            error = true;
        }
        if(const_in.vpp.In.CropY  != const_in.vpp.Out.CropY)
        {
            out->vpp.Out.CropY = 0;
            error = true;
        }
        if(const_in.vpp.In.CropW  != const_in.vpp.Out.CropW)
        {
            out->vpp.Out.CropW = 0;
            error = true;
        }
        if(const_in.vpp.In.CropH  != const_in.vpp.Out.CropH)
        {
            out->vpp.Out.CropH = 0;
            error = true;
        }
        if(const_in.vpp.In.CropH + const_in.vpp.In.CropY > const_in.vpp.In.Height)
        {
            out->vpp.In.CropH = 0;
            out->vpp.In.CropY = 0;
            error = true;
        }
        if(const_in.vpp.In.CropW + const_in.vpp.In.CropX > const_in.vpp.In.Width)
        {
            out->vpp.In.CropW = 0;
            out->vpp.In.CropX = 0;
            error = true;
        }

        if(const_in.vpp.In.Width  % 16 != 0)
        {
            out->vpp.In.Width = 0;
            error = true;
        }
        if(const_in.vpp.In.Width > 3840)
        {
            big_resol = true;
        }
        if(const_in.vpp.In.Height % 32 != 0)
        {
            out->vpp.In.Height = 0;
            error = true;
        }
        if(const_in.vpp.In.Height > 3840)
        {
            big_resol = true;
        }
        if(const_in.vpp.Out.Height % 16 != 0 || const_in.vpp.Out.Height > 3840)
        {
            out->vpp.Out.Height = 0;
            error = true;
        }

        if(const_in.vpp.In.FourCC != MFX_FOURCC_NV12)
        {
            out->vpp.In.FourCC = 0;
            error = true;
        }
        if(const_in.vpp.Out.FourCC != MFX_FOURCC_NV12)
        {
            out->vpp.Out.FourCC = 0;
            error = true;
        }
        if(const_in.vpp.In.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        {
            out->vpp.In.ChromaFormat = 0;
            error = true;
        }
        if(const_in.vpp.In.ChromaFormat != const_in.vpp.Out.ChromaFormat)
        {
            out->vpp.Out.ChromaFormat = 0;
            error = true;
        }

        //Bit depth is not supported
        {
            if(!(const_in.vpp.In.BitDepthChroma == 0 || const_in.vpp.In.BitDepthChroma == 8))
            {
                out->vpp.In.BitDepthChroma = 0;
                error = true;
            }
            if(!(const_in.vpp.In.BitDepthLuma   == 0 || const_in.vpp.In.BitDepthLuma == 8))
            {
                out->vpp.In.BitDepthLuma = 0;
                error = true;
            }
            if(const_in.vpp.In.Shift != 0)
            {
                out->vpp.In.Shift = 0;
                error = true;
            }

            if(!(const_in.vpp.Out.BitDepthChroma == 0 || const_in.vpp.Out.BitDepthChroma == 8))
            {
                out->vpp.Out.BitDepthChroma = 0;
                error = true;
            }
            if(!(const_in.vpp.Out.BitDepthLuma   == 0 || const_in.vpp.Out.BitDepthLuma == 8))
            {
                out->vpp.Out.BitDepthLuma = 0;
                error = true;
            }
            if(const_in.vpp.Out.Shift != 0)
            {
                out->vpp.Out.Shift = 0;
                error = true;
            }
        }

    
        //find suitable mode

        if(!buffer_mode)
        {
            if(MFX_PICSTRUCT_UNKNOWN == const_in.vpp.In.PicStruct &&
               0 == const_in.vpp.In.FrameRateExtN &&
               0 != const_in.vpp.Out.FrameRateExtD &&
               30 == (const_in.vpp.Out.FrameRateExtN / const_in.vpp.Out.FrameRateExtD)
               )
            {
                ;//auto-detection mode
            }
            else if (MFX_PICSTRUCT_UNKNOWN == const_in.vpp.In.PicStruct &&
               0 == const_in.vpp.In.FrameRateExtN &&
               0 != const_in.vpp.Out.FrameRateExtD &&
               60 == (const_in.vpp.Out.FrameRateExtN / const_in.vpp.Out.FrameRateExtD)
               )
            {
                ;//auto-detection mode
            }
            else if((MFX_PICSTRUCT_FIELD_TFF == const_in.vpp.In.PicStruct ||
                     MFX_PICSTRUCT_FIELD_BFF == const_in.vpp.In.PicStruct) &&
              ((4 * const_in.vpp.In.FrameRateExtN * const_in.vpp.In.FrameRateExtD ==
                5 * const_in.vpp.Out.FrameRateExtN * const_in.vpp.Out.FrameRateExtD) ||
               (30000 == const_in.vpp.In.FrameRateExtN && 1001 == const_in.vpp.In.FrameRateExtD &&
                24000 == const_in.vpp.Out.FrameRateExtN && 1001 == const_in.vpp.Out.FrameRateExtD)
                ))
            {
                ;//reverse telecine mode
            }
            else if(MFX_PICSTRUCT_FIELD_TFF == const_in.vpp.In.PicStruct ||
                   MFX_PICSTRUCT_FIELD_BFF == const_in.vpp.In.PicStruct)
            {
                //Deinterlace only mode

                if(2 * const_in.vpp.In.FrameRateExtN * const_in.vpp.Out.FrameRateExtD ==
                    const_in.vpp.In.FrameRateExtD * const_in.vpp.Out.FrameRateExtN)
                {
                    ;//30i -> 60p mode
                }
                else if(const_in.vpp.In.FrameRateExtN * const_in.vpp.Out.FrameRateExtD ==
                    const_in.vpp.In.FrameRateExtD * const_in.vpp.Out.FrameRateExtN)
                {
                    ;//30i -> 30p mode
                }
                else
                {
                    if(const_in.vpp.In.FrameRateExtD &&
                       (30 != const_in.vpp.In.FrameRateExtN / const_in.vpp.In.FrameRateExtD))
                    {
                        out->vpp.In.FrameRateExtN = 0;
                        out->vpp.In.FrameRateExtD = 0;
                    }
                    if(const_in.vpp.Out.FrameRateExtD &&
                       ((30 != const_in.vpp.Out.FrameRateExtN / const_in.vpp.Out.FrameRateExtD) && (60 != const_in.vpp.Out.FrameRateExtN / const_in.vpp.Out.FrameRateExtD)))
                    {
                        out->vpp.Out.FrameRateExtN = 0;
                        out->vpp.Out.FrameRateExtD = 0;
                    }
                    ;//any additional frame rate conversions are unsupported
                    error = true;
                }
            }
            else
            {
                //reverse telecine + additional frame rate conversion are unsupported
                out->vpp.In.FrameRateExtN = 0;
                out->vpp.In.FrameRateExtD = 0;
                out->vpp.Out.FrameRateExtN = 0;
                out->vpp.Out.FrameRateExtD = 0;

                error = true;
            }
        }

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
            if(MFX_ERR_NOT_FOUND == mfxSts)
            {
                HWSupported = false;
                p_accel = true;
            }
            else if(MFX_ERR_NONE > mfxSts)
            {
                return mfxSts;
            }
            else
            {
                eMFXHWType HWType = MFX_HW_UNKNOWN;
                mfxSts = GetHWTypeAndCheckSupport(mfxCorePar.Impl, mfxDeviceHdl, HWType, HWSupported, p_accel);
                if(MFX_WRN_PARTIAL_ACCELERATION == mfxSts || MFX_ERR_NOT_FOUND == mfxSts || big_resol)
                {
                    HWSupported = false;
                    p_accel = true;
                    mfxSts = MFX_ERR_NONE;
                }
                if(MFX_ERR_NONE > mfxSts)
                    return mfxSts;
            }
        }
    }
    else //if(!const_in) fill configurable params
    {
        memset(&out->mfx, 0, sizeof(mfxInfoMFX));
        memset(&out->vpp, 0, sizeof(mfxInfoVPP));

        /* vppIn */
        //out->vpp.In.FourCC       = MFX_FOURCC_NV12;
        out->vpp.In.Height       = 1;
        out->vpp.In.Width        = 1;
        out->vpp.In.CropW        = 1;
        out->vpp.In.CropH        = 1;
        //out->vpp.In.CropX        = 1;
        //out->vpp.In.CropY        = 1;
        out->vpp.In.PicStruct    = 1;
        out->vpp.In.FrameRateExtN = 1;
        out->vpp.In.FrameRateExtD = 1;

        /* vppOut */
        //out->vpp.Out.FourCC       = 1;
        out->vpp.Out.Height       = 1;
        out->vpp.Out.Width        = 1;
        //out->vpp.Out.ChromaFormat = 1;
        //out->vpp.In.ChromaFormat = 1;

        //out->vpp.Out.CropW        = 1;
        //out->vpp.Out.CropH        = 1;
        //out->vpp.Out.CropX        = 1;
        //out->vpp.Out.CropY        = 1;

        //out->vpp.Out.PicStruct    = 1;
        out->vpp.Out.FrameRateExtN = 1;
        out->vpp.Out.FrameRateExtD = 1;

        out->Protected           = 0;
        out->IOPattern           = 1;
        //out->AsyncDepth          = 1;
    }

    if(error)
        return MFX_ERR_UNSUPPORTED;
    else if(p_accel)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else if(warning)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    else
        return MFX_ERR_NONE;
}

mfxStatus MFX_PTIR_Plugin::QueryReset(const mfxVideoParam& old_vp, const mfxVideoParam& new_vp)
{
    if((old_vp.vpp.In.Width  < new_vp.vpp.In.Width ) ||
       (old_vp.vpp.In.Height < new_vp.vpp.In.Height))
       return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(new_vp.AsyncDepth && new_vp.AsyncDepth != old_vp.AsyncDepth)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(new_vp.IOPattern != old_vp.IOPattern)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if((2 * old_vp.vpp.In.FrameRateExtN * old_vp.vpp.Out.FrameRateExtD ==
         old_vp.vpp.In.FrameRateExtD * old_vp.vpp.Out.FrameRateExtN) &&
        ((2 * new_vp.vpp.In.FrameRateExtN * new_vp.vpp.Out.FrameRateExtD ==
          new_vp.vpp.In.FrameRateExtD * new_vp.vpp.Out.FrameRateExtN) ||
         (new_vp.vpp.In.FrameRateExtN * new_vp.vpp.Out.FrameRateExtD ==
          new_vp.vpp.In.FrameRateExtD * new_vp.vpp.Out.FrameRateExtN)))
    {
        //old: 30i -> 60p mode
        //new: 30i -> 60p or 30i -> 30p mode
        ;//allowed
    }
    else if(((old_vp.vpp.In.FrameRateExtN * old_vp.vpp.Out.FrameRateExtD ==
             old_vp.vpp.In.FrameRateExtD * old_vp.vpp.Out.FrameRateExtN) &&
              (4 * new_vp.vpp.In.FrameRateExtN * new_vp.vpp.In.FrameRateExtD ==
               5 * new_vp.vpp.Out.FrameRateExtN * new_vp.vpp.Out.FrameRateExtD)) ||
              (new_vp.vpp.In.FrameRateExtN * new_vp.vpp.Out.FrameRateExtD ==
               new_vp.vpp.In.FrameRateExtD * new_vp.vpp.Out.FrameRateExtN))
    {
        //old 30i -> 30p mode
        //new 30i -> 30p or 24p mode
        ;//allowed
    }
    else if( (4 * old_vp.vpp.In.FrameRateExtN * old_vp.vpp.In.FrameRateExtD ==
              5 * old_vp.vpp.Out.FrameRateExtN * old_vp.vpp.Out.FrameRateExtD) &&
             (4 * new_vp.vpp.In.FrameRateExtN * new_vp.vpp.In.FrameRateExtD ==
              5 * new_vp.vpp.Out.FrameRateExtN * new_vp.vpp.Out.FrameRateExtD) )
    {
        //old 30i -> 24p mode
        //new 30i -> 24p mode
        ;//allowed
    }
    else
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFX_PTIR_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    if(!par || !in || !out)
        return MFX_ERR_NULL_PTR;
    const mfxU16& IOPattern = par->IOPattern;
    if((IOPattern != (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY )) &&
       (IOPattern != (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) &&
       (IOPattern != (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_OPAQUE_MEMORY)) &&
       (IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY )) &&
       (IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) &&
       (IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY)) &&
       (IOPattern != (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY )) &&
       (IOPattern != (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) &&
       (IOPattern != (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY)))
       return MFX_ERR_INVALID_VIDEO_PARAM;

    bool bSWLib = true;
    mfxStatus mfxSts = MFX_ERR_NONE;
    //Check partial acceleration
    bool p_accel = false;
    bool error = false;
    bool HWSupported = false;
    mfxHandleType mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
    mfxHDL mfxDeviceHdl;
    mfxCoreParam mfxCorePar;
    mfxSts = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;
    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(mfxCorePar.Impl))
    {
        mfxSts = GetHandle(mfxDeviceHdl, mfxDeviceType);
        if(MFX_ERR_NOT_FOUND == mfxSts)
        {
            HWSupported = false;
            p_accel = true;
        }
        else if(MFX_ERR_NONE > mfxSts)
        {
            return mfxSts;
        }
        else
        {
            eMFXHWType HWType = MFX_HW_UNKNOWN;
            mfxSts = GetHWTypeAndCheckSupport(mfxCorePar.Impl, mfxDeviceHdl, HWType, HWSupported, p_accel);
            if(MFX_WRN_PARTIAL_ACCELERATION == mfxSts || MFX_ERR_NOT_FOUND == mfxSts)
            {
                HWSupported = false;
                p_accel = true;
                mfxSts = MFX_ERR_NONE;
            }
            if(MFX_ERR_NONE > mfxSts)
                return mfxSts;
            if(!p_accel)
                bSWLib = false;
        }
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

    mfxU16 *pInMemType = &in->Type;
    mfxU16 *pOutMemType = &out->Type;



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
        error = true;
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
        error = true;
    }

    if(error)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if(p_accel)
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
    bool warn = false;
    bool HWSupported = false;
    par_accel = false;

    mfxCoreParam mfxCorePar;
    mfxSts = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;

    memset(&m_mfxInitPar, 0, sizeof(mfxVideoParam));
    mfxSts = Query(par, &m_mfxInitPar);
    if( MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxSts)
    {
        warn = true;
        mfxSts = MFX_ERR_NONE;
    }
    if( !(MFX_ERR_NONE == mfxSts || MFX_WRN_PARTIAL_ACCELERATION == mfxSts) )
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if(MFX_WRN_PARTIAL_ACCELERATION == mfxSts &&
       MFX_IMPL_SOFTWARE != MFX_IMPL_BASETYPE(mfxCorePar.Impl))
    {
        HWSupported = false;
        par_accel = true;
    }
    if(!m_mfxInitPar.AsyncDepth)
        m_mfxInitPar.AsyncDepth = 1;

    mfxExtVPPDeinterlacing*   extVPPDeintIn = 0;
    mfxExtOpaqueSurfaceAlloc* in_opaq_buf   = 0;
    if(par->ExtParam || par->NumExtParam)
    {
        if(!par->ExtParam || !par->NumExtParam)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
        else
        {
            for(mfxU32 i = 0; i < par->NumExtParam; ++i)
            {
                if(par->ExtParam[i])
                {
                    if( par->ExtParam[i]->BufferId == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION &&
                        par->ExtParam[i]->BufferSz == sizeof(mfxExtOpaqueSurfaceAlloc))
                    {
                        in_opaq_buf = (mfxExtOpaqueSurfaceAlloc*) par->ExtParam[i];
                    }
                    else if( par->ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DEINTERLACING &&
                                par->ExtParam[i]->BufferSz == sizeof(mfxExtVPPDeinterlacing))
                    {
                        extVPPDeintIn = (mfxExtVPPDeinterlacing*) par->ExtParam[i];
                    }
                }
            }
        }
    }

    mfxHandleType mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
    mfxHDL mfxDeviceHdl = 0;

    eMFXHWType HWType = MFX_HW_UNKNOWN;
    mfxSts = GetHandle(mfxDeviceHdl, mfxDeviceType);
    if(MFX_ERR_NONE != mfxSts &&
       MFX_IMPL_SOFTWARE != MFX_IMPL_BASETYPE(mfxCorePar.Impl))
    {
        HWSupported = false;
        par_accel = true;
    }

    if (!par_accel)
    {
        mfxSts = GetHWTypeAndCheckSupport(mfxCorePar.Impl, mfxDeviceHdl, HWType, HWSupported, par_accel);
        if (MFX_ERR_NONE > mfxSts)
            return mfxSts;
    }

                    ////force fallback hack
                    //HWSupported = false;
                    //par_accel = true;

    bool isD3D11 = false;
    if(MFX_IMPL_VIA_D3D11 == ((mfxCorePar.Impl) & 0xF00))
        isD3D11 = true;
    try
    {
        frmSupply = new frameSupplier(&inSurfs, &workSurfs, &outSurfs, 0, 0, m_pmfxCore, par->IOPattern, isD3D11, m_guard);
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

    try
    {
        mfxSts = ptir->Init(par, extVPPDeintIn);
    }
    catch(std::bad_alloc&)
    {
        mfxSts = MFX_ERR_MEMORY_ALLOC;
    }
    catch(...)
    {
        mfxSts = MFX_ERR_UNKNOWN;
    }

    if(MFX_ERR_NONE > mfxSts && MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(mfxCorePar.Impl) && HWSupported)
    {
        //failed to initialize CM ptir, try to fallback to SW
        delete ptir;

        par_accel = true;
        try
        {
            ptir = new PTIR_ProcessorCPU(m_pmfxCore, frmSupply);
        }
        catch(std::bad_alloc&)
        {
            delete frmSupply;
            frmSupply = 0;
            mfxSts = MFX_ERR_MEMORY_ALLOC;
        }
        catch(...)
        {
            delete frmSupply;
            frmSupply = 0;
            return MFX_ERR_UNKNOWN;
        }

        try
        {
            mfxSts = ptir->Init(par, extVPPDeintIn);
        }
        catch(std::bad_alloc&)
        {
            delete ptir;
            ptir = 0;
            delete frmSupply;
            frmSupply = 0;
            mfxSts = MFX_ERR_MEMORY_ALLOC;
        }
        catch(...)
        {
            delete ptir;
            ptir = 0;
            delete frmSupply;
            frmSupply = 0;
            mfxSts = MFX_ERR_UNKNOWN;
        }
    }
    bInited = true;

    if(MFX_ERR_NONE == mfxSts)
    {
        m_mfxCurrentPar = m_mfxInitPar;
        b_work = false;

        //if opaque
        if(par->IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
           par->IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
           par->IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY ) ||
           par->IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
           par->IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_OPAQUE_MEMORY)   )
        {
            if (in_opaq_buf)
            {
                mfxFrameAllocRequest req_in ;
                mfxFrameAllocRequest req_out;

                memset(&req_in , 0, sizeof(req_in) );
                memset(&req_out, 0, sizeof(req_out));

                bool bOpaqMode[2];

                mfxSts = CheckOpaqMode(*par, 0, *in_opaq_buf, 0, bOpaqMode);
                if(mfxSts)
                {
                    Close();
                    return MFX_ERR_MEMORY_ALLOC;
                }

                //QueryIOSurf(par, &req_in, &req_out);
                if((par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) && in_opaq_buf->In.Surfaces && in_opaq_buf->In.NumSurface)
                {
                    mfxSts = m_pmfxCore->MapOpaqueSurface(m_pmfxCore->pthis, in_opaq_buf->In.NumSurface, in_opaq_buf->In.Type, in_opaq_buf->In.Surfaces);
                    //mfxSts = m_pmfxCore->MapOpaqueSurface(m_pmfxCore->pthis, in_opaq_buf->In.NumSurface, req_in.Type, in_opaq_buf->In.Surfaces);
                    if(mfxSts)
                    {
                        Close();
                        return MFX_ERR_MEMORY_ALLOC;
                    }
                }
                if((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && in_opaq_buf->Out.Surfaces && in_opaq_buf->Out.NumSurface)
                {
                    mfxSts = m_pmfxCore->MapOpaqueSurface(m_pmfxCore->pthis, in_opaq_buf->Out.NumSurface, in_opaq_buf->Out.Type, in_opaq_buf->Out.Surfaces);
                    //mfxSts = m_pmfxCore->MapOpaqueSurface(m_pmfxCore->pthis, in_opaq_buf->Out.NumSurface, req_out.Type, in_opaq_buf->Out.Surfaces);
                    if(mfxSts)
                    {
                        Close();
                        return MFX_ERR_MEMORY_ALLOC;
                    }
                }
                m_OpaqSurfAlloc = *in_opaq_buf;
                //frmSupply->SetFrmType(req_in.Type, req_out.Type);
                if(frmSupply)
                    frmSupply->SetFrmType(in_opaq_buf->In.Type, in_opaq_buf->Out.Type);
            }
            else
            {
                Close();
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }
            if(extVPPDeintIn)
            {
                m_extVPPDeint = *extVPPDeintIn;
            }
        }
    }
    else
    {
        Close();
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if(par_accel)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else if(warn)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
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
    mfxVideoParam tmpPar;
    memset(&tmpPar, 0, sizeof(tmpPar));
    mfxSts = Query(par, &tmpPar);
    if((MFX_WRN_PARTIAL_ACCELERATION == mfxSts) && par_accel)
        mfxSts = MFX_ERR_NONE;
    else if(MFX_ERR_UNSUPPORTED == mfxSts)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if(MFX_ERR_NONE > mfxSts)
        return mfxSts;
    else if(MFX_ERR_NONE < mfxSts)
        mfxWrn = mfxSts;

    mfxSts = QueryReset(m_mfxInitPar, tmpPar);
    if(mfxSts)
        return mfxSts;

    mfxSts = Close();
    if(MFX_ERR_NONE >= mfxSts)
    {
        mfxSts = Init(par);
        if(mfxSts)
            mfxWrn = mfxSts;

        m_mfxCurrentPar = tmpPar;
    }

    return mfxWrn;
}
mfxStatus MFX_PTIR_Plugin::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard);
    if(!bInited)
        return MFX_ERR_NOT_INITIALIZED;
    bInited = false;

    mfxStatus mfxSts = MFX_ERR_NONE;
    if(vTasks.size())
    {
        UMC::AutomaticUMCMutex guard(m_guard_free);
        while(0 != vTasks.size())
        {
            FreeResources((mfxThreadTask) vTasks.front(), mfxSts);
        }
    }
    if(ptir)
    {
        if(frmSupply)
            frmSupply->FreeFrames();
        mfxSts = ptir->Close();
        delete ptir;
        ptir = 0;
    }
    if(frmSupply)
    {
        delete frmSupply;
        frmSupply = 0;
    }

    if(m_mfxInitPar.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
       m_mfxInitPar.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
       m_mfxInitPar.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY ) ||
       m_mfxInitPar.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
       m_mfxInitPar.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_OPAQUE_MEMORY)   )
    {
        if((m_mfxInitPar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) && m_OpaqSurfAlloc.In.Surfaces && m_OpaqSurfAlloc.In.NumSurface)
        {
            mfxSts = m_pmfxCore->UnmapOpaqueSurface(m_pmfxCore->pthis, m_OpaqSurfAlloc.In.NumSurface, m_OpaqSurfAlloc.In.Type, m_OpaqSurfAlloc.In.Surfaces);
        }
        if((m_mfxInitPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && m_OpaqSurfAlloc.Out.Surfaces && !m_OpaqSurfAlloc.Out.NumSurface)
        {
            mfxSts = m_pmfxCore->UnmapOpaqueSurface(m_pmfxCore->pthis, m_OpaqSurfAlloc.Out.NumSurface, m_OpaqSurfAlloc.Out.Type, m_OpaqSurfAlloc.Out.Surfaces);
        }
    }

    memset(&m_mfxInitPar, 0, sizeof(mfxVideoParam));
    memset(&m_mfxCurrentPar, 0, sizeof(mfxVideoParam));
    memset(&m_OpaqSurfAlloc, 0, sizeof(mfxExtOpaqueSurfaceAlloc));

    if(mfxSts)
        return mfxSts;
    return MFX_ERR_NONE;
}

mfxStatus MFX_PTIR_Plugin::GetVideoParam(mfxVideoParam *par)
{
    if(!par)     return MFX_ERR_NULL_PTR;
    if(!bInited) return MFX_ERR_NOT_INITIALIZED;

    par->AsyncDepth = m_mfxCurrentPar.AsyncDepth;
    par->vpp        = m_mfxCurrentPar.vpp       ;
    par->Protected  = m_mfxCurrentPar.Protected ;
    par->IOPattern  = m_mfxCurrentPar.IOPattern ;

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
        {
            b_work = false;
            *surface_out = outSurfs.front();
            outSurfs.erase(outSurfs.begin());
        }
        else
        {
            b_work = true;
            *surface_out = workSurfs.front();
            //workSurfs.erase(workSurfs.begin());
        }
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
        for(std::vector<mfxFrameSurface1*>::iterator it = vSurfs.begin(); it != vSurfs.end(); ++it)
        {
            s_out = *it;
            vSurfs.erase(it);
            return s_out;
        }
    }
    return 0;
}

inline mfxStatus MFX_PTIR_Plugin::addWorkSurf(mfxFrameSurface1* pSurface)
{
    if(!pSurface)
        return MFX_ERR_NONE;
#if defined(_DEBUG)
    for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs.begin(); it != workSurfs.end(); ++it)
    {
        assert((*it)->Data.Locked != 0);
    }
#endif
    if(workSurfs.end() == std::find (workSurfs.begin(), workSurfs.end(), pSurface) && (workSurfs.size() + ptir->Env.fqIn.iCount + ptir->Env.control.uiCur) < 16)
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
    if(pSurface && ((inSurfs.size() == 0) || (inSurfs.size() != 0 && inSurfs.back() != pSurface)) /*&& prevSurf != pSurface*/ && (inSurfs.size() + ptir->Env.fqIn.iCount + ptir->Env.control.uiCur) < 18)
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
    if(MFX_IMPL_VIA_D3D9 == (mfxCorePar.Impl & 0xF00))
        mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
    else if(MFX_IMPL_VIA_D3D11 == (mfxCorePar.Impl & 0xF00))
        mfxDeviceType = MFX_HANDLE_D3D11_DEVICE;
    else if(MFX_IMPL_VIA_VAAPI == (mfxCorePar.Impl & 0xF00))
        mfxDeviceType = MFX_HANDLE_VA_DISPLAY;
    mfxSts = m_pmfxCore->GetHandle(m_pmfxCore->pthis, mfxDeviceType, &mfxDeviceHdl);

    if(MFX_ERR_NONE != mfxSts &&
       MFX_IMPL_SOFTWARE != MFX_IMPL_BASETYPE(mfxCorePar.Impl))
    {
        mfxSts = m_pmfxCore->CreateAccelerationDevice(m_pmfxCore->pthis, mfxDeviceType, &mfxDeviceHdl);
        if(mfxSts)
            return mfxSts;
        mfxSts = m_pmfxCore->GetHandle(m_pmfxCore->pthis, mfxDeviceType, &mfxDeviceHdl);

        if(mfxSts || !mfxDeviceHdl)
            return MFX_ERR_NOT_FOUND;
        else
            return MFX_ERR_NONE;
    }

    return MFX_ERR_NONE;
}

inline mfxStatus MFX_PTIR_Plugin::GetHWTypeAndCheckSupport(mfxIMPL& impl, mfxHDL& mfxDeviceHdl, eMFXHWType& HWType, bool& HWSupported, bool& par_accel)
{
    if((MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl)) && mfxDeviceHdl)
    {
        HWType = GetHWType(1,impl,mfxDeviceHdl);
        switch (HWType)
        {
        case MFX_HW_BDW:
        case MFX_HW_HSW_ULT:
        case MFX_HW_HSW:
        //case MFX_HW_IVB:
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

inline mfxStatus MFX_PTIR_Plugin::CheckInFrameSurface1(mfxFrameSurface1*& mfxSurf, mfxFrameSurface1*& mfxSurfOut)
{
    if(!mfxSurf) return MFX_ERR_NULL_PTR;

    if((m_mfxCurrentPar.vpp.In.Width  > mfxSurf->Info.Width)  ||
        (m_mfxCurrentPar.vpp.In.Height > mfxSurf->Info.Height))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(!(m_mfxCurrentPar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) && (!mfxSurf->Data.MemId && !(mfxSurf->Data.Y && mfxSurf->Data.UV)))
        return MFX_ERR_NULL_PTR;

    if(mfxSurfOut)
    {
        if((mfxSurf->Info.Width  != mfxSurfOut->Info.Width)  ||
           (mfxSurf->Info.Height != mfxSurfOut->Info.Height) ||
           (mfxSurf->Info.CropX  != mfxSurfOut->Info.CropX)  ||
           (mfxSurf->Info.CropY  != mfxSurfOut->Info.CropY)  ||
           (mfxSurf->Info.CropW  != mfxSurfOut->Info.CropW)  ||
           (mfxSurf->Info.CropH  != mfxSurfOut->Info.CropH))
           return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    //if(!m_mfxCurrentPar.vpp.In.PicStruct)  {
    //    if( !((mfxSurf->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF) || (mfxSurf->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF)))
    //        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    //}  else if (m_mfxCurrentPar.vpp.In.PicStruct != mfxSurf->Info.PicStruct)  {
    //    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    //}

    return CheckFrameSurface1(mfxSurf);
}

inline mfxStatus MFX_PTIR_Plugin::CheckOutFrameSurface1(mfxFrameSurface1*& mfxSurf)
{
    if(!mfxSurf) return MFX_ERR_NULL_PTR;

    if((m_mfxCurrentPar.vpp.Out.Width  > mfxSurf->Info.Width)  ||
       (m_mfxCurrentPar.vpp.Out.Height > mfxSurf->Info.Height))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(mfxSurf->Info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(!(m_mfxCurrentPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (!mfxSurf->Data.MemId && !(mfxSurf->Data.Y && mfxSurf->Data.UV)))
        return MFX_ERR_NULL_PTR;

    return CheckFrameSurface1(mfxSurf);
}

inline mfxStatus MFX_PTIR_Plugin::CheckFrameSurface1(mfxFrameSurface1*& mfxSurf)
{
    if(!mfxSurf) return MFX_ERR_NULL_PTR;

    if((mfxSurf->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420) ||
       (mfxSurf->Info.FourCC      != MFX_FOURCC_NV12))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if((mfxSurf->Info.CropW > mfxSurf->Info.Width)  ||
        (mfxSurf->Info.CropH > mfxSurf->Info.Height) ||
        (mfxSurf->Info.CropX > mfxSurf->Info.CropW)  ||
        (mfxSurf->Info.CropY > mfxSurf->Info.CropH))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if((mfxSurf->Info.CropH + mfxSurf->Info.CropY > mfxSurf->Info.Height) ||
        (mfxSurf->Info.CropW + mfxSurf->Info.CropX > mfxSurf->Info.Width))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(((mfxSurf->Info.Height % 32) != 0) ||
       ((mfxSurf->Info.Width % 16) != 0))
       return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(mfxSurf->Data.Y)
    {
        mfxU32 pitch = mfxSurf->Data.PitchLow + ((mfxU32)mfxSurf->Data.PitchHigh << 16);
        if(pitch == 0 || pitch < mfxSurf->Info.Width)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFX_PTIR_Plugin::CheckOpaqMode(const mfxVideoParam& par, mfxVideoParam* pParOut, const mfxExtOpaqueSurfaceAlloc& opaqAlloc, mfxExtOpaqueSurfaceAlloc* pOpaqAllocOut, bool bOpaqMode[2] )
{
    bool error = false;
    if ( (par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) || (par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) )
    {
        if( par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY )
        {
            if (!(opaqAlloc.In.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) && !(opaqAlloc.In.Type  & MFX_MEMTYPE_SYSTEM_MEMORY))
            {
                if(pParOut)
                    pParOut->IOPattern = 0;
                if(pOpaqAllocOut)
                    pOpaqAllocOut->In.Type = 0;
                error = true;
            }
            if ((opaqAlloc.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (opaqAlloc.In.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)))
            {
                if(pParOut)
                    pParOut->IOPattern = 0;
                if(pOpaqAllocOut)
                    pOpaqAllocOut->In.Type = 0;
                error = true;
            }
            bOpaqMode[0] = true;
        }

        if( par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY )
        {
            if (!(opaqAlloc.Out.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) && !(opaqAlloc.Out.Type  & MFX_MEMTYPE_SYSTEM_MEMORY))
            {
                if(pParOut)
                    pParOut->IOPattern = 0;
                if(pOpaqAllocOut)
                    pOpaqAllocOut->Out.Type = 0;
                error = true;
            }
            if ((opaqAlloc.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (opaqAlloc.Out.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)))
            {
                if(pParOut)
                    pParOut->IOPattern = 0;
                if(pOpaqAllocOut)
                    pOpaqAllocOut->Out.Type = 0;
                error = true;
            }
            bOpaqMode[1] = true;
        }
    }

    if(error)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else
        return MFX_ERR_NONE;
} // mfxStatus CheckOpaqMode( mfxVideoParam* par, bool bOpaqMode[2] )
