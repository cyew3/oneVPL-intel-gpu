/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin.cpp

\* ****************************************************************************** */

#include "mfx_camera_plugin.h"
#include "mfx_session.h"
#include "mfx_task.h"

#ifdef CAMP_PIPE_ITT
#include "ittnotify.h"
__itt_domain* CamPipe = __itt_domain_create(L"CamPipe");
__itt_string_handle* task1 = __itt_string_handle_create(L"CreateEnqueueTasks");
__itt_string_handle* taske = __itt_string_handle_create(L"WaitEvent");
__itt_string_handle* tasks = __itt_string_handle_create(L"SetExtSurf");
#endif

#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
    NULL,
    NULL,
    NULL,
    &MFXCamera_Plugin::Create,
    &MFXCamera_Plugin::CreateByDispatcher
};

MSDK_PLUGIN_API(MFXVPPPlugin*) mfxCreateVPPPlugin()
{
    if (!g_PluginModule.CreateVPPPlugin)
    {
        return 0;
    }
    return g_PluginModule.CreateVPPPlugin();
}

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin)
{
    if (!g_PluginModule.CreatePlugin)
    {
        return 0;
    }
    return (MFXPlugin*) g_PluginModule.CreatePlugin(uid, plugin);
}

const mfxPluginUID MFXCamera_Plugin::g_Camera_PluginGuid = MFX_PLUGINID_CAMERA_HW;

MFXCamera_Plugin::MFXCamera_Plugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    m_useSW = false;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_PARALLEL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_Camera_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_VPP;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;

    Zero(m_Caps);
    m_Caps.InputMemoryOperationMode  = MEM_GPU;
    m_Caps.OutputMemoryOperationMode = MEM_GPU;
    m_Caps.BayerPatternType = MFX_CAM_BAYER_RGGB;

    Zero(m_GammaParams);
    m_memIn            = 0;
    m_cmSurfIn         = 0;
    m_gammaPointSurf   = 0;
    m_gammaCorrectSurf = 0;
    m_gammaOutSurf     = 0;
    m_paddedSurf       = 0;
    m_vignetteMaskSurf = 0;
    m_avgFlagSurf      = 0;
    m_LUTSurf          = 0;

    m_isInitialized = false;

    m_activeThreadCount = 0;

    m_FrameSizeExtra.tileOffsets = 0;
    m_nTiles = 1; // Single tile by default
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
    CAMERA_DEBUG_LOG("PluginInit core = %p \n", core);

    MFX_CHECK_NULL_PTR1(core);
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);

    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

#if !defined (MFX_VA) && defined (AS_VPP_PLUGIN)
    par.Impl = MFX_IMPL_SOFTWARE;
#endif

    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);

    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    CAMERA_DEBUG_LOG("PluginInit JoinSession parent %p child %p \n", (mfxSession) m_pmfxCore->pthis, m_session);
    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pmfxCore->pthis, m_session);
    CAMERA_DEBUG_LOG("PluginInit sts = %d \n", mfxRes);

    return mfxRes;
}

mfxStatus MFXCamera_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;

    CAMERA_DEBUG_LOG("PluginClose \n");

    if (m_session)
    {
        //The application must ensure there is no active task running in the session before calling this (MFXDisjoinSession) function.
        mfxRes = MFXVideoVPP_Close(m_session);
        //Return the first met wrn or error
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED)
            mfxRes2 = mfxRes;

        CAMERA_DEBUG_LOG("PluginClose DisjoinSession = %p \n", m_session);

        mfxRes = MFXInternalPseudoDisjoinSession(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
           mfxRes2 = mfxRes;
        mfxRes = MFXClose(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
            mfxRes2 = mfxRes;
        m_session = 0;
    }

    if ( m_FrameSizeExtra.tileOffsets )
        delete [] m_FrameSizeExtra.tileOffsets;

    if (m_createdByDispatcher)
    {
        delete this;
    }

    CAMERA_DEBUG_LOG("PluginClose sts = %d \n", mfxRes2);
    return mfxRes2;
}

mfxStatus MFXCamera_Plugin::GetPluginParam(mfxPluginParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus mfxSts = MFX_ERR_NONE;

    CAMERA_DEBUG_LOG("Query in=%p out=%p inited %d core  %p device  %p\n", in, out, m_isInitialized, m_core, m_cmDevice);
    if (NULL == in)
    {
        memset(&out->mfx, 0, sizeof(mfxInfoMFX));
        memset(&out->vpp, 0, sizeof(mfxInfoVPP));

        /* vppIn */
        out->vpp.In.FourCC        = 1;
        out->vpp.In.Height        = 1;
        out->vpp.In.Width         = 1;
        out->vpp.In.CropW         = 1;
        out->vpp.In.CropH         = 1;
        out->vpp.In.CropX         = 1;
        out->vpp.In.CropY         = 1;
        out->vpp.In.FrameRateExtN = 1;
        out->vpp.In.FrameRateExtD = 1;
        out->vpp.In.ChromaFormat  = 1;
        out->vpp.In.BitDepthLuma  = 1;

        /* vppOut */
        out->vpp.Out.FourCC       = 1;
        out->vpp.Out.Height       = 1;
        out->vpp.Out.Width        = 1;
        out->vpp.Out.ChromaFormat = 1;

        out->Protected           = 0; //???
        out->IOPattern           = 1; //???
        out->AsyncDepth          = 1;

        if (0 == out->NumExtParam)
            out->NumExtParam     = 1;
        else
            mfxSts = CheckExtBuffers(out, NULL, MFX_CAM_QUERY_SIGNAL);
        CAMERA_DEBUG_LOG("Query end sts =  %d \n", mfxSts);
        return mfxSts;
    }
    else
    {
        //let it be true for now, later need to consider if App passed padded surface or not.
        bool padding = false;

        out->vpp     = in->vpp;

        /* [asyncDepth] section */
        out->AsyncDepth = in->AsyncDepth;
        if(out->AsyncDepth > 10)
        {
            out->AsyncDepth = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        /* [Protected] section */
        out->Protected = in->Protected;
        if (out->Protected)
        {
            out->Protected = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        /* [IOPattern] section */
        out->IOPattern = in->IOPattern;
        mfxSts = CheckIOPattern(in,out,0);

        /* [ExtParam] section */
        if (in->NumExtParam == 0)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        if ((in->ExtParam == 0 && out->ExtParam != 0) ||
            (in->ExtParam != 0 && out->ExtParam == 0) ||
            (in->NumExtParam != out->NumExtParam))
        {
            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if (0 != in->ExtParam)
        {
            for (int i = 0; i < in->NumExtParam; i++)
            {
                if(in->ExtParam[i] == NULL)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        if (0 != out->ExtParam)
        {
            for (int i = 0; i < out->NumExtParam; i++)
            {
                if(out->ExtParam[i] == NULL)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        if (0 == out->NumExtParam && in->ExtParam)
        {
            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if (in->ExtParam && out->ExtParam && (in->NumExtParam == out->NumExtParam))
        {
            mfxU16 i;

            // to prevent multiple initialization
            std::vector<mfxU32> filterList(1);
            bool bMultipleInitDOUSE  = false;

            for (i = 0; i < out->NumExtParam; i++)
            {
                if ((in->ExtParam[i] == 0 && out->ExtParam[i] != 0) ||
                    (in->ExtParam[i] != 0 && out->ExtParam[i] == 0))
                {
                    mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                    break;
                }

                if (in->ExtParam[i] && out->ExtParam[i])
                {
                    if (in->ExtParam[i]->BufferId != out->ExtParam[i]->BufferId)
                    {
                        mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                        break;
                    }

                    if (in->ExtParam[i]->BufferSz != out->ExtParam[i]->BufferSz)
                    {
                        mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                        break;
                    }

                    // --------------------------------
                    // analysis of DOUSE structure
                    // --------------------------------
                    if (MFX_EXTBUFF_VPP_DOUSE == in->ExtParam[i]->BufferId)
                    {
                        if (bMultipleInitDOUSE)
                        {
                            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                            break;
                        }

                        bMultipleInitDOUSE = true;

                        // deep analysis
                        //--------------------------------------
                        {
                            mfxExtVPPDoUse*   extDoUseIn  = (mfxExtVPPDoUse*)in->ExtParam[i];
                            mfxExtVPPDoUse*   extDoUseOut = (mfxExtVPPDoUse*)out->ExtParam[i];

                            if(extDoUseIn->NumAlg != extDoUseOut->NumAlg)
                            {
                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                break;
                            }

                            if (0 == extDoUseIn->NumAlg)
                            {
                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                break;
                            }

                            if (NULL == extDoUseOut->AlgList || NULL == extDoUseIn->AlgList)
                            {
                                mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                                break;
                            }
                        }
                        //--------------------------------------
                    }

                } // if(in->ExtParam[i] && out->ExtParam[i])

            } //  for (i = 0; i < out->NumExtParam; i++)

            MFX_CHECK_STS(mfxSts);
            mfxSts = CheckExtBuffers(in, out, MFX_CAM_QUERY_CHECK_RANGE);

        } // if (in->ExtParam && out->ExtParam && (in->NumExtParam == out->NumExtParam) )
        else
        {
            mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        //assume we are setting the same params in init.
        if(mfxSts == MFX_WRN_FILTER_SKIPPED && in != out)
            mfxSts = MFX_ERR_UNSUPPORTED;

        mfxStatus sts = MFX_ERR_NONE;

        if (out->vpp.In.BitDepthLuma < 8 || out->vpp.In.BitDepthLuma > 16 || (out->vpp.In.BitDepthLuma & 1))
        {
            out->vpp.In.BitDepthLuma = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (out->vpp.In.FourCC != MFX_FOURCC_R16)
        {
            {
                out->vpp.In.FourCC = 0;
                out->vpp.Out.ChromaFormat = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if (out->vpp.Out.FourCC != MFX_FOURCC_RGB4 && out->vpp.Out.FourCC != MFX_FOURCC_ARGB16)
        {
            {
                out->vpp.Out.FourCC = 0;
                out->vpp.Out.ChromaFormat = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if(out->vpp.Out.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
        {
            {
                out->vpp.Out.ChromaFormat = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if(out->vpp.In.ChromaFormat)
        {
            {
                out->vpp.In.ChromaFormat = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if (!(out->vpp.In.FrameRateExtN * out->vpp.In.FrameRateExtD) &&
            (out->vpp.In.FrameRateExtN + out->vpp.In.FrameRateExtD))
        {
            out->vpp.In.FrameRateExtN = 0;
            out->vpp.In.FrameRateExtD = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (out->vpp.In.Width)
        {
            if ((out->vpp.In.Width & 15) != 0)
            {
                out->vpp.In.Width = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (out->vpp.In.CropW > out->vpp.In.Width)
            {
                out->vpp.In.CropW = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (out->vpp.In.CropW + out->vpp.In.CropX > out->vpp.In.Width)
            {
                out->vpp.In.CropX = out->vpp.In.CropW = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }
        else
        {
            out->vpp.In.CropW = 0;
            out->vpp.In.CropX = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (out->vpp.In.Height)
        {
            if ((out->vpp.In.Height & 7) != 0)
            {
                out->vpp.In.Height = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (out->vpp.In.CropH > out->vpp.In.Height)
            {
                out->vpp.In.CropH = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (out->vpp.In.CropH + out->vpp.In.CropY > out->vpp.In.Height)
            {
                out->vpp.In.CropY = out->vpp.In.CropH = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }
        else
        {
            out->vpp.In.CropY = 0;
            out->vpp.In.CropH = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!(out->vpp.Out.FrameRateExtN * out->vpp.Out.FrameRateExtD) &&
            (out->vpp.Out.FrameRateExtN + out->vpp.Out.FrameRateExtD))
        {
            out->vpp.Out.FrameRateExtN = 0;
            out->vpp.Out.FrameRateExtD = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (out->vpp.Out.Width)
        {
            if ((out->vpp.Out.Width & 15) != 0)
            {
                out->vpp.Out.Width = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
            if (out->vpp.Out.CropW > out->vpp.Out.Width)
            {
                out->vpp.Out.CropW = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (out->vpp.Out.CropW + out->vpp.Out.CropX > out->vpp.Out.Width)
            {
                out->vpp.Out.CropX = out->vpp.Out.CropW = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            //need to check carefully, it is not right for cases when app gives padded surface.
            if(!padding)
            {
                if(out->vpp.Out.Width != out->vpp.In.Width &&
                   out->vpp.Out.Width != out->vpp.In.CropW &&
                   out->vpp.Out.CropW != out->vpp.In.Width &&
                   out->vpp.Out.CropW != out->vpp.In.CropW)
                {
                    out->vpp.Out.Width = 0;
                    out->vpp.In.Width  = 0;
                    out->vpp.Out.CropW = 0;
                    out->vpp.In.CropW  = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }
            else
            {
                //only 8 pixel padding supported
                if(out->vpp.Out.Width != (out->vpp.In.Width-16) &&
                   out->vpp.Out.Width != (out->vpp.In.CropW-16) &&
                   out->vpp.Out.CropW != (out->vpp.In.Width-16) &&
                   out->vpp.Out.CropW != (out->vpp.In.CropW-16))
                {
                    out->vpp.Out.Width = 0;
                    out->vpp.In.Width  = 0;
                    out->vpp.Out.CropW = 0;
                    out->vpp.In.CropW  = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }
        }

        if (out->vpp.Out.Height )
        {
            if ((out->vpp.Out.Height  & 7) !=0)
            {
                out->vpp.Out.Height = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (out->vpp.Out.CropH > out->vpp.Out.Height)
            {
                out->vpp.Out.CropH = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (out->vpp.Out.CropH + out->vpp.Out.CropY > out->vpp.Out.Height)
            {
                out->vpp.Out.CropY = out->vpp.Out.CropH = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            //need to check carefully, it is not right for cases when app gives padded surface.
            if(!padding)
            {
                if(out->vpp.Out.Height != (out->vpp.In.Height) &&
                   out->vpp.Out.Height != (out->vpp.In.CropH)  &&
                   out->vpp.Out.CropH  != (out->vpp.In.Height) &&
                   out->vpp.Out.CropH  != (out->vpp.In.CropH))
                {
                    out->vpp.Out.Height = 0;
                    out->vpp.In.Height  = 0;
                    out->vpp.Out.CropH  = 0;
                    out->vpp.In.CropH   = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }
            else
            {
                if(out->vpp.Out.Height != (out->vpp.In.Height-16) &&
                   out->vpp.Out.Height != (out->vpp.In.CropH-16)  &&
                   out->vpp.Out.CropH  != (out->vpp.In.Height-16) &&
                   out->vpp.Out.CropH  != (out->vpp.In.CropH-16))
                {
                    out->vpp.Out.Height = 0;
                    out->vpp.In.Height  = 0;
                    out->vpp.Out.CropH  = 0;
                    out->vpp.In.CropH   = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }
        }

        CAMERA_DEBUG_LOG("Query end sts =  %d  mfxSts = %d \n", sts, mfxSts);

        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;
        else if (sts < MFX_ERR_NONE)
            return sts;

        return mfxSts;
    }
}

mfxStatus MFXCamera_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK_NULL_PTR1(in);
    MFX_CHECK_NULL_PTR1(out);

    mfxU16 asyncDepth = par->AsyncDepth;
    if (asyncDepth == 0)
        asyncDepth = MFX_CAMERA_DEFAULT_ASYNCDEPTH;

    mfxStatus sts = CheckIOPattern(par,par,1);
    in->Info = par->vpp.In;
    in->NumFrameMin = 1;
    in->NumFrameSuggested = in->NumFrameMin + asyncDepth - 1;
    in->Type = MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_EXTERNAL_FRAME;
    in->Type |= par->IOPattern&MFX_IOPATTERN_IN_SYSTEM_MEMORY? MFX_MEMTYPE_SYSTEM_MEMORY:0;

    out->Info = par->vpp.Out;
    out->NumFrameMin = 1;
    out->NumFrameSuggested = out->NumFrameMin + asyncDepth - 1;
    out->Type = MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_EXTERNAL_FRAME;
    out->Type |= par->IOPattern&MFX_IOPATTERN_OUT_SYSTEM_MEMORY?
        MFX_MEMTYPE_SYSTEM_MEMORY:
        MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;

    return sts;
}

mfxStatus MFXCamera_Plugin::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par);
    par->AsyncDepth = m_mfxVideoParam.AsyncDepth;
    par->IOPattern = m_mfxVideoParam.IOPattern;
    memcpy_s(&(par->vpp),sizeof(mfxInfoVPP), &(m_mfxVideoParam.vpp),sizeof(mfxInfoVPP));

    if(par->NumExtParam != 0){
        //ToDo: report supported extparam.
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard1);

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    m_isInitialized = false;

    CAMERA_DEBUG_LOG("MFXVideoVPP_Close device %p m_cmSurfIn %p, %d active threads\n", m_cmDevice, m_cmSurfIn, m_activeThreadCount);

    mfxStatus sts = WaitForActiveThreads();
    sts;
    CAMERA_DEBUG_LOG("MFXVideoVPP_Close after WaitForActiveThreads device %p  %d active threads, sts = %d \n", m_cmDevice, m_activeThreadCount, sts);

    if (m_useSW)
    {
        FreeCamera_CPU(&m_cmi);
    }
    else
    {
        m_raw16padded.Free();
        m_raw16aligned.Free();
        m_aux8.Free();

        if (m_cmSurfIn)
            m_cmDevice->DestroySurface(m_cmSurfIn);

        if (m_avgFlagSurf)
            m_cmDevice->DestroySurface(m_avgFlagSurf);
        if (m_gammaCorrectSurf)
            m_cmDevice->DestroySurface(m_gammaCorrectSurf);

        if (m_gammaPointSurf)
            m_cmDevice->DestroySurface(m_gammaPointSurf);

        if (m_gammaOutSurf)
            m_cmDevice->DestroySurface(m_gammaOutSurf);

        if (m_paddedSurf)
            m_cmDevice->DestroySurface(m_paddedSurf);

        if (m_vignetteMaskSurf)
            m_cmDevice->DestroySurface(m_vignetteMaskSurf);

        // TODO: need to delete LUT buffer in case CCM was used.
        m_cmCtx->Close();

        m_cmDevice.Reset(0);
    }
    CAMERA_DEBUG_LOG("MFXVideoVPP_Close end \n");
    m_isInitialized = false;

    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::ProcessExtendedBuffers(mfxVideoParam *par)
{
    mfxU32 i, j;
    mfxStatus sts = MFX_ERR_NONE;
    bool gammaset        = false;
    bool paddingset      = false;
    bool blacklevelset   = false;
    bool whitebalanceset = false;
    bool ccmset          = false;
    bool vignetteset     = false;

    for (i = 0; i < par->NumExtParam; i++)
    {
        if (m_mfxVideoParam.ExtParam[i])
        {
            if (MFX_EXTBUFF_VPP_DOUSE == par->ExtParam[i]->BufferId)
            {
                mfxExtVPPDoUse* pDoUse = (mfxExtVPPDoUse*)(par->ExtParam[i]);
                for (j = 0; j < pDoUse->NumAlg; j++)
                {
                    if (MFX_EXTBUF_CAM_GAMMA_CORRECTION == pDoUse->AlgList[j])
                        m_Caps.bForwardGammaCorrection = 1;
                    else if (MFX_EXTBUF_CAM_PADDING == pDoUse->AlgList[j])
                        m_Caps.bNoPadding = 1;
                    else if (MFX_EXTBUF_CAM_PIPECONTROL == pDoUse->AlgList[j])
                        m_Caps.BayerPatternType = MFX_CAM_BAYER_BGGR;
                }
            }
            else if (MFX_EXTBUF_CAM_GAMMA_CORRECTION == par->ExtParam[i]->BufferId)
            {
                m_Caps.bForwardGammaCorrection = 1;
                mfxExtCamGammaCorrection* gammaExtBufParams = (mfxExtCamGammaCorrection*)par->ExtParam[i];

                if (gammaExtBufParams && (gammaExtBufParams->Mode == MFX_CAM_GAMMA_LUT || gammaExtBufParams->Mode == MFX_CAM_GAMMA_VALUE))
                {
                    if (gammaExtBufParams->Mode == MFX_CAM_GAMMA_LUT)
                    {
                        if (gammaExtBufParams->GammaPoint     &&
                            gammaExtBufParams->GammaCorrected &&
                            gammaExtBufParams->NumPoints == MFX_CAM_GAMMA_NUM_POINTS_SKL) // only 64-point gamma is currently supported
                        {
                            gammaset = true;
                            m_GammaParams.gamma_lut.gammaPoints = gammaExtBufParams->GammaPoint;
                            m_GammaParams.gamma_lut.gammaCorrect = gammaExtBufParams->GammaCorrected;
                            m_GammaParams.gamma_lut.numPoints = gammaExtBufParams->NumPoints;
                        }
                    }
                    else
                    {
                        m_GammaParams.gamma_value = gammaExtBufParams->GammaValue;
                        m_GammaParams.gamma_lut.gammaPoints = m_GammaParams.gamma_lut.gammaCorrect = 0;
                        if (m_GammaParams.gamma_value > 0)
                            gammaset = true;
                    }
                }
            }
            else if (MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION == par->ExtParam[i]->BufferId)
            {
                m_Caps.bBlackLevelCorrection = 1;
                mfxExtCamBlackLevelCorrection* blackLevelExtBufParams = (mfxExtCamBlackLevelCorrection*)par->ExtParam[i];
                if ( blackLevelExtBufParams )
                {
                    blacklevelset = true;
                    m_BlackLevelParams.bActive = true;
                    m_BlackLevelParams.BlueLevel        = blackLevelExtBufParams->B;
                    m_BlackLevelParams.GreenBottomLevel = blackLevelExtBufParams->G0;
                    m_BlackLevelParams.GreenTopLevel    = blackLevelExtBufParams->G1;
                    m_BlackLevelParams.RedLevel         = blackLevelExtBufParams->R;
                }
            }
            else if (MFX_EXTBUF_CAM_WHITE_BALANCE == par->ExtParam[i]->BufferId)
            {
                m_Caps.bWhiteBalance = 1;
                mfxExtCamWhiteBalance* whiteBalanceExtBufParams = (mfxExtCamWhiteBalance*)par->ExtParam[i];
                if ( whiteBalanceExtBufParams )
                {
                    whitebalanceset = true;
                    m_WBparams.bActive = true;
                    m_WBparams.Mode                  = whiteBalanceExtBufParams->Mode;
                    m_WBparams.BlueCorrection        = whiteBalanceExtBufParams->B;
                    m_WBparams.GreenTopCorrection    = whiteBalanceExtBufParams->G0;
                    m_WBparams.GreenBottomCorrection = whiteBalanceExtBufParams->G1;
                    m_WBparams.RedCorrection         = whiteBalanceExtBufParams->R;
                }
            }
            else if (MFX_EXTBUF_CAM_VIGNETTE_CORRECTION == par->ExtParam[i]->BufferId)
            {
                m_Caps.bVignetteCorrection = 1;
                mfxExtCamVignetteCorrection* VignetteExtBufParams = (mfxExtCamVignetteCorrection*)par->ExtParam[i];
                if ( VignetteExtBufParams )
                {
                    vignetteset = true;
                    m_VignetteParams.bActive = true;
                    // TODO: copy vignette mask data to internal structure
                }
            }
            else if (MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3 == par->ExtParam[i]->BufferId)
            {
                m_Caps.bColorConversionMatrix = 1;
                mfxExtCamColorCorrection3x3* CCMExtBufParams = (mfxExtCamColorCorrection3x3*)par->ExtParam[i];
                if ( CCMExtBufParams )
                {
                    ccmset = true;
                    m_CCMParams.bActive = true;
                    m_CCMParams.CCM[0][0]  = CCMExtBufParams->CCM[0][0];
                    m_CCMParams.CCM[0][1]  = CCMExtBufParams->CCM[0][1];
                    m_CCMParams.CCM[0][2]  = CCMExtBufParams->CCM[0][2];
                    m_CCMParams.CCM[1][0]  = CCMExtBufParams->CCM[1][0];
                    m_CCMParams.CCM[1][1]  = CCMExtBufParams->CCM[1][1];
                    m_CCMParams.CCM[1][2]  = CCMExtBufParams->CCM[1][2];
                    m_CCMParams.CCM[2][0]  = CCMExtBufParams->CCM[2][0];
                    m_CCMParams.CCM[2][1]  = CCMExtBufParams->CCM[2][1];
                    m_CCMParams.CCM[2][2]  = CCMExtBufParams->CCM[2][2];
                }
            }
            else if (MFX_EXTBUF_CAM_PADDING == par->ExtParam[i]->BufferId)
            {
                //????
                m_Caps.bNoPadding = 1;
                paddingset = true;
                mfxExtCamPadding* paddingExtBufParams = (mfxExtCamPadding*)par->ExtParam[i];
                m_PaddingParams.top = paddingExtBufParams->Top;
                m_PaddingParams.bottom = paddingExtBufParams->Bottom;
                m_PaddingParams.left = paddingExtBufParams->Left;
                m_PaddingParams.right = paddingExtBufParams->Right;
            }
            else if (MFX_EXTBUF_CAM_PIPECONTROL == par->ExtParam[i]->BufferId)
            {
                mfxExtCamPipeControl* pipeExtBufParams = (mfxExtCamPipeControl*)par->ExtParam[i];
                m_Caps.BayerPatternType = pipeExtBufParams->RawFormat;
            }
        }
    }

    if (m_Caps.bForwardGammaCorrection)
        if (!gammaset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bNoPadding)
        if (!paddingset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bBlackLevelCorrection)
        if (!blacklevelset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bColorConversionMatrix)
        if (!ccmset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bWhiteBalance)
        if (!whitebalanceset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bVignetteCorrection)
        if (!vignetteset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    return sts;
}

mfxStatus MFXCamera_Plugin::Init(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_guard1);

    MFX_CHECK(!m_isInitialized, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(par);
    mfxStatus mfxSts;

    CAMERA_DEBUG_LOG("MFXVideoVPP_Init\n");

    m_mfxVideoParam = *par;
    m_core = m_session->m_pCORE.get();

    CAMERA_DEBUG_LOG("Init ExternalFrameAllocator NOT SET\n");

    m_platform = m_core->GetHWType();
    if(m_platform < MFX_HW_HSW || m_platform == MFX_HW_VLV || m_platform > MFX_HW_BDW)
    {
        m_useSW = true;
    }

    mfxSts = Query(&m_mfxVideoParam, &m_mfxVideoParam);
    if(mfxSts == MFX_ERR_UNSUPPORTED)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (mfxSts < MFX_ERR_NONE)
    {
        return mfxSts;
    }

    if (m_mfxVideoParam.AsyncDepth == 0)
        m_mfxVideoParam.AsyncDepth = MFX_CAMERA_DEFAULT_ASYNCDEPTH;

    m_Caps.ModuleConfiguration = 0; // zero all filters
    m_Caps.bDemosaic = 1;           // demosaic is always on
    ProcessExtendedBuffers(&m_mfxVideoParam);

    if (m_mfxVideoParam.vpp.Out.FourCC == MFX_FOURCC_RGB4)
    {
        m_Caps.bOutToARGB16 = 0;
    }
    else
    {
        m_Caps.bOutToARGB16 = 1;
    }

    if (m_Caps.bNoPadding)
    {
        if (m_PaddingParams.top  < MFX_CAM_MIN_REQUIRED_PADDING_TOP  || m_PaddingParams.bottom < MFX_CAM_MIN_REQUIRED_PADDING_BOTTOM ||
            m_PaddingParams.left < MFX_CAM_MIN_REQUIRED_PADDING_LEFT || m_PaddingParams.right  < MFX_CAM_MIN_REQUIRED_PADDING_RIGHT)
        {
            m_Caps.bNoPadding = 0; // padding provided by application is not sufficient - have to do it ourselves
            CAMERA_DEBUG_LOG("MFXVideoVPP_Init wrong padding top %d bot %d left %d right %d \n", m_PaddingParams.top, m_PaddingParams.bottom, m_PaddingParams.left, m_PaddingParams.right);
        }
    }

    if ( m_mfxVideoParam.vpp.In.CropW * m_mfxVideoParam.vpp.In.CropH > 0x16E3600 )
    {
        // TODO: make number of tiles dependent on frame size.
        // For existing 7Kx4K 2 tiles seems to be enough.
        m_nTiles = 2;
    }

    m_PaddingParams.top    = MFX_CAM_DEFAULT_PADDING_TOP;
    m_PaddingParams.bottom = MFX_CAM_DEFAULT_PADDING_BOTTOM;
    m_PaddingParams.left   = MFX_CAM_DEFAULT_PADDING_LEFT;
    m_PaddingParams.right  = MFX_CAM_DEFAULT_PADDING_RIGHT;

    if(!m_Caps.bNoPadding)
    {
        m_FrameSizeExtra.frameWidth64      = ((m_mfxVideoParam.vpp.In.CropW  + 31) &~ 0x1F); // 2 bytes each for In, 4 bytes for Out, so 32 is good enough for 64 ???
        m_FrameSizeExtra.paddedFrameWidth  = m_mfxVideoParam.vpp.In.CropW  + m_PaddingParams.left + m_PaddingParams.right;
        m_FrameSizeExtra.paddedFrameHeight = m_mfxVideoParam.vpp.In.CropH  + m_PaddingParams.top + m_PaddingParams.bottom;
        m_FrameSizeExtra.vSliceWidth       = (((m_mfxVideoParam.vpp.In.CropW / CAM_PIPE_KERNEL_SPLIT)  + 15) &~ 0xF) + 16;
        m_FrameSizeExtra.tileNum           = m_nTiles;
        m_FrameSizeExtra.tileOffsets       = new CameraTileOffset[m_nTiles];
        m_FrameSizeExtra.TileWidth         = m_mfxVideoParam.vpp.In.CropW;
        m_FrameSizeExtra.TileHeight        = ( ( m_mfxVideoParam.vpp.In.CropH / m_nTiles ) + 31 ) &~ 0x1F;
        m_FrameSizeExtra.TileHeightPadded  =  m_FrameSizeExtra.TileHeight + m_PaddingParams.top + m_PaddingParams.bottom;
        m_FrameSizeExtra.BitDepth          = m_mfxVideoParam.vpp.In.BitDepthLuma;
        m_FrameSizeExtra.TileInfo          = m_mfxVideoParam.vpp.In;
        for (int i = 0; i < m_nTiles; i++)
        {
            if ( m_nTiles - 1 == i && i > 0)
            {
                // In case of several tiles, last tile must be aligned to the original frame bottom
                m_FrameSizeExtra.tileOffsets[i].TileOffset = m_mfxVideoParam.vpp.In.CropH - m_FrameSizeExtra.TileHeight;
            }
            else
            {
                m_FrameSizeExtra.tileOffsets[i].TileOffset = ( m_mfxVideoParam.vpp.In.CropH / m_nTiles ) * i;
            }
        }
    }
    else
    {
        // TODO: need to set up data for tiles.
        m_FrameSizeExtra.frameWidth64      = ((m_mfxVideoParam.vpp.In.Width + 31) &~ 0x1F); // 2 bytes each for In, 4 bytes for Out, so 32 is good enough for 64 ???
        m_FrameSizeExtra.paddedFrameWidth  = m_mfxVideoParam.vpp.In.Width;
        m_FrameSizeExtra.paddedFrameHeight = m_mfxVideoParam.vpp.In.Height;
        m_FrameSizeExtra.vSliceWidth       = (((m_mfxVideoParam.vpp.In.Width / CAM_PIPE_KERNEL_SPLIT)  + 15) &~ 0xF) + 16;
    }

    m_InputBitDepth = m_mfxVideoParam.vpp.In.BitDepthLuma;
    m_InitWidth     = m_mfxVideoParam.vpp.In.Width;
    m_InitHeight    = m_mfxVideoParam.vpp.In.Height;

    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        m_Caps.OutputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.OutputMemoryOperationMode = MEM_GPUSHARED;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
        m_Caps.InputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.InputMemoryOperationMode = MEM_GPUSHARED;

    /* TODO - automatically select SW/HW based on machine capabilities */
    if (m_useSW)
    {
        //m_CameraProcessor = new CPUCameraProcessor();
        InitCamera_CPU(&m_cmi, m_mfxVideoParam.vpp.In.Width, m_mfxVideoParam.vpp.In.Height, m_InputBitDepth, !m_Caps.bNoPadding, m_Caps.BayerPatternType);
    }
    else
    {
        m_cmDevice.Reset(CreateCmDevicePtr(m_core));
        m_cmCtx.reset(new CmContext(m_FrameSizeExtra, m_cmDevice, &m_Caps, m_platform));
        m_cmCtx->CreateThreadSpaces(&m_FrameSizeExtra);
        mfxSts = AllocateInternalSurfaces();
    }

    m_isInitialized = true;

    CAMERA_DEBUG_LOG("MFXVideoVPP_Init bNoPadding %d  bForwardGammaCorrection %d \n", m_Caps.bNoPadding, m_Caps.bForwardGammaCorrection);
    CAMERA_DEBUG_LOG("MFXVideoVPP_Init end sts =  %d \n", mfxSts);
    CAMERA_DEBUG_LOG("MFXVideoVPP_Init device %p m_cmSurfIn %p  sts =  %d \n", m_cmDevice, m_cmSurfIn, mfxSts);

    if(m_useSW)
        mfxSts = MFX_WRN_PARTIAL_ACCELERATION;

    return mfxSts;
}

mfxStatus MFXCamera_Plugin::Reset(mfxVideoParam *par)
{

    UMC::AutomaticUMCMutex guard(m_guard1);

    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    mfxStatus mfxSts;

    WaitForActiveThreads();

    CAMERA_DEBUG_LOG("MFXVideoVPP_Reset \n");

    mfxVideoParam newParam;
    memcpy_s(&newParam,sizeof(mfxVideoParam),par,sizeof(mfxVideoParam));
    newParam.ExtParam = par->ExtParam;
    mfxSts = Query(&newParam, &newParam);

    if(mfxSts == MFX_ERR_UNSUPPORTED)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    if (m_mfxVideoParam.AsyncDepth != newParam.AsyncDepth)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (newParam.vpp.In.Width > m_InitWidth || newParam.vpp.In.Height > m_InitHeight)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (m_mfxVideoParam.IOPattern != newParam.IOPattern)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    CameraPipeForwardGammaParams oldGammaParams = m_GammaParams;

    m_Caps.ModuleConfiguration = 0; // zero all filters
    m_Caps.bDemosaic = 1;           // demosaic is always on
    ProcessExtendedBuffers(&newParam);

    if (newParam.vpp.Out.FourCC == MFX_FOURCC_RGB4)
    {
        m_Caps.bOutToARGB16 = 0;
    }
    else
    {
        m_Caps.bOutToARGB16 = 1;
    }

    if (m_Caps.bNoPadding)
    {
        if (m_PaddingParams.top  < MFX_CAM_MIN_REQUIRED_PADDING_TOP  || m_PaddingParams.bottom < MFX_CAM_MIN_REQUIRED_PADDING_BOTTOM ||
            m_PaddingParams.left < MFX_CAM_MIN_REQUIRED_PADDING_LEFT || m_PaddingParams.right < MFX_CAM_MIN_REQUIRED_PADDING_RIGHT)
            m_Caps.bNoPadding = 0; // padding provided by application is not sufficient - have to do it ourselves
    }

    m_PaddingParams.top    = MFX_CAM_DEFAULT_PADDING_TOP;
    m_PaddingParams.bottom = MFX_CAM_DEFAULT_PADDING_BOTTOM;
    m_PaddingParams.left   = MFX_CAM_DEFAULT_PADDING_LEFT;
    m_PaddingParams.right  = MFX_CAM_DEFAULT_PADDING_RIGHT;

    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        m_Caps.OutputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.OutputMemoryOperationMode = MEM_GPUSHARED;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
        m_Caps.InputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.InputMemoryOperationMode = MEM_GPUSHARED;

    if ( newParam.vpp.In.CropW * newParam.vpp.In.CropH > 0x16E3600 )
    {
        // TODO: make number of tiles dependent on frame size.
        // For existing 7Kx4K 2 tiles seems to be enough.
        m_nTiles = 2;
    }
    CameraFrameSizeExtra frameSizeExtra;
    frameSizeExtra.frameWidth64      = ((newParam.vpp.In.CropW  + 31) &~ 0x1F); // 2 bytes each for In, 4 bytes for Out, so 32 is good enough for 64 ???
    frameSizeExtra.paddedFrameWidth  = newParam.vpp.In.CropW  + m_PaddingParams.left + m_PaddingParams.right;
    frameSizeExtra.paddedFrameHeight = newParam.vpp.In.CropH  + m_PaddingParams.top + m_PaddingParams.bottom;
    frameSizeExtra.vSliceWidth       = (((newParam.vpp.In.CropW / CAM_PIPE_KERNEL_SPLIT)  + 15) &~ 0xF) + 16;
    frameSizeExtra.tileNum           = m_nTiles;
    frameSizeExtra.tileOffsets       = new CameraTileOffset[m_nTiles];
    frameSizeExtra.TileWidth         = newParam.vpp.In.CropW;
    frameSizeExtra.TileHeight        = ( ( newParam.vpp.In.CropH / m_nTiles ) + 31 ) &~ 0x1F;
    frameSizeExtra.TileHeightPadded  = frameSizeExtra.TileHeight + m_PaddingParams.top + m_PaddingParams.bottom;
    frameSizeExtra.BitDepth          = newParam.vpp.In.BitDepthLuma;
    frameSizeExtra.TileInfo          = newParam.vpp.In;
    for (int i = 0; i < m_nTiles; i++)
    {
        if ( m_nTiles - 1 == i && i > 0)
        {
            // In case of several tiles, last tile must be aligned to the original frame bottom
            frameSizeExtra.tileOffsets[i].TileOffset = newParam.vpp.In.CropH - frameSizeExtra.TileHeight;
        }
        else
        {
            frameSizeExtra.tileOffsets[i].TileOffset = ( newParam.vpp.In.CropH / m_nTiles ) * i;
        }
    }

    m_InputBitDepth = newParam.vpp.In.BitDepthLuma;

    m_cmCtx->Reset(newParam, &m_Caps);
    m_cmCtx->CreateThreadSpaces(&frameSizeExtra);

    mfxSts = ReallocateInternalSurfaces(newParam, frameSizeExtra);
    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    // will need to check numPoints as well when different LUT sizes are supported
    if (m_Caps.bForwardGammaCorrection || m_Caps.bColorConversionMatrix)
    {
        if (!m_gammaCorrectSurf)
            m_gammaCorrectSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        if (!m_gammaPointSurf)
            m_gammaPointSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);

        mfxU8 *pGammaPts = (mfxU8 *)m_GammaParams.gamma_lut.gammaPoints;
        mfxU8 *pGammaCor = (mfxU8 *)m_GammaParams.gamma_lut.gammaCorrect;

        if (pGammaPts != (mfxU8 *)oldGammaParams.gamma_lut.gammaPoints || pGammaCor != (mfxU8 *)oldGammaParams.gamma_lut.gammaCorrect)
        {
            if (!pGammaPts || !pGammaCor)
            {
                // gamma value is used - implement !!! kta
            }
            m_gammaCorrectSurf->WriteSurface(pGammaCor, NULL);
            m_gammaPointSurf->WriteSurface((unsigned char *)pGammaPts, NULL);
        }
    }

    m_mfxVideoParam = newParam;
    m_FrameSizeExtra = frameSizeExtra;
    CAMERA_DEBUG_LOG("MFXVideoVPP_Reset end sts =  %d \n", mfxSts);

    return mfxSts;
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
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameSurface1 *surfIn  = pParam->surf_in;
    mfxFrameSurface1 *surfOut = pParam->surf_out;

    if (m_useSW)
    {
        /* input data  in surfIn->Data.Y16 (16-bit)
         * output data in surfOut->Data.V16 (16-bit) or Data.B (8-bit)
         * params in surfIn->Info
         */
        UMC::AutomaticUMCMutex guard(m_guard);

        if (surfOut->Data.MemId)
            m_core->LockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

        // Step 1. Padding
        // TODO: Add flipping for GB, GR bayers
        if(m_cmi.enable_padd)
        {
            for (int i = 0; i < m_cmi.InHeight; i++)
                for (int j = 0; j < m_cmi.InWidth; j++)
                    m_cmi.cpu_Input[i*m_cmi.InWidth + j] = (short)surfIn->Data.Y16[i*(surfIn->Data.Pitch>>1) + j];
            m_cmi.cpu_status = CPU_Padding_16bpp(m_cmi.cpu_Input, m_cmi.cpu_PaddedBayerImg, m_cmi.InWidth, m_cmi.InHeight, m_cmi.bitDepth);
        }
        else
        {
            for (int i = 0; i < m_cmi.FrameHeight; i++)
                for (int j = 0; j < m_cmi.FrameWidth; j++)
                    m_cmi.cpu_PaddedBayerImg[i*m_cmi.FrameWidth + j] = (short)surfIn->Data.Y16[i*(surfIn->Data.Pitch>>1)];
        }

        // Step 2. Black level correction
        if(m_Caps.bBlackLevelCorrection)
        {
            m_cmi.cpu_status = CPU_BLC(m_cmi.cpu_PaddedBayerImg, m_cmi.FrameWidth, m_cmi.FrameHeight, 
                             m_cmi.bitDepth, m_cmi.BayerType,
                             (short)m_BlackLevelParams.BlueLevel, (short)m_BlackLevelParams.GreenTopLevel, 
                             (short)m_BlackLevelParams.GreenBottomLevel, (short)m_BlackLevelParams.RedLevel,
                             false);
        }

        // Step 3. White balance
        if(m_Caps.bWhiteBalance)
        {
            m_cmi.cpu_status = CPU_WB (m_cmi.cpu_PaddedBayerImg, m_cmi.FrameWidth, m_cmi.FrameHeight, 
                                       m_cmi.bitDepth, m_cmi.BayerType,
                                       (float)m_WBparams.BlueCorrection, (float)m_WBparams.GreenTopCorrection, 
                                       (float)m_WBparams.GreenBottomCorrection, (float)m_WBparams.RedCorrection,
                                       false);
        }

        // Step 4. Demosaic
        m_cmi.cpu_status = Demosaic_CPU(&m_cmi, surfIn->Data.Y16, surfIn->Data.Pitch);

        if (m_Caps.BayerPatternType == MFX_CAM_BAYER_RGGB || m_Caps.BayerPatternType == MFX_CAM_BAYER_GBRG)
        {
            /* swap R and B buffers */
            m_cmi.cpu_R_fgc_in = m_cmi.cpu_B_o;
            m_cmi.cpu_G_fgc_in = m_cmi.cpu_G_o;
            m_cmi.cpu_B_fgc_in = m_cmi.cpu_R_o;
        }
        else
        {
            m_cmi.cpu_R_fgc_in = m_cmi.cpu_R_o;
            m_cmi.cpu_G_fgc_in = m_cmi.cpu_G_o;
            m_cmi.cpu_B_fgc_in = m_cmi.cpu_B_o;
        }

        // Step 5. CMM
        if (m_Caps.bColorConversionMatrix)
        {
            m_cmi.cpu_status = CPU_CCM( (unsigned short*)m_cmi.cpu_R_fgc_in, (unsigned short*)m_cmi.cpu_G_fgc_in, (unsigned short*)m_cmi.cpu_B_fgc_in, 
                                         m_FrameSizeExtra.TileInfo.Width, m_FrameSizeExtra.TileInfo.Height, m_FrameSizeExtra.BitDepth, m_CCMParams.CCM);
        }

        // Step 6. Gamma correction
        if (m_Caps.bForwardGammaCorrection)
        {
            m_cmi.cpu_status = CPU_Gamma_SKL( (unsigned short*)m_cmi.cpu_R_fgc_in, (unsigned short*)m_cmi.cpu_G_fgc_in, (unsigned short*)m_cmi.cpu_B_fgc_in, 
                            m_FrameSizeExtra.TileInfo.Width, m_FrameSizeExtra.TileInfo.Height, m_FrameSizeExtra.BitDepth, 
                            m_GammaParams.gamma_lut.gammaCorrect, m_GammaParams.gamma_lut.gammaPoints);

        }
        if (m_mfxVideoParam.vpp.Out.FourCC == MFX_FOURCC_ARGB16)
            m_cmi.cpu_status = CPU_ARGB16Interleave(surfOut->Data.V16, surfOut->Info.Width, surfOut->Info.Height, surfOut->Data.Pitch, m_FrameSizeExtra.BitDepth, m_Caps.BayerPatternType,
                                         m_cmi.cpu_R_fgc_in, m_cmi.cpu_G_fgc_in, m_cmi.cpu_B_fgc_in);
        else
            m_cmi.cpu_status = CPU_ARGB8Interleave(surfOut->Data.B, surfOut->Info.Width, surfOut->Info.Height, surfOut->Data.Pitch, m_FrameSizeExtra.BitDepth, m_Caps.BayerPatternType,
                                         m_cmi.cpu_R_fgc_in, m_cmi.cpu_G_fgc_in, m_cmi.cpu_B_fgc_in);

        if (surfOut->Data.MemId)
            m_core->UnlockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

        return (mfxStatus)m_cmi.cpu_status;
    }

    m_core->IncreaseReference(&surfIn->Data);
    m_core->IncreaseReference(&surfOut->Data);
    m_core->IncreasePureReference(m_activeThreadCount);

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, tasks);
#endif

    sts = SetExternalSurfaces(pParam);
    if (sts != MFX_ERR_NONE)
        return sts;

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif


#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, task1);
#endif

    sts  = CreateEnqueueTasks(pParam);
    if (sts != MFX_ERR_NONE)
        return sts;

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif

//    m_core->DecreaseReference(&surfIn->Data);
//    m_core->DecreaseReference(&surfOut->Data);

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

    m_raw16aligned.Unlock();
    m_raw16padded.Unlock();
    m_aux8.Unlock();

    if (m_useSW)
        return MFX_ERR_NONE;

    if (pParam)
    {

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, taske);
#endif
        CmEvent *e = (CmEvent *)pParam->pEvent;
        CAMERA_DEBUG_LOG("CompleteCameraAsyncRoutine e=%p device %p \n", e, m_cmDevice);

       if (m_isInitialized)
            m_cmCtx->DestroyEvent(e);
        else
            m_cmCtx->DestroyEventWithoutWait(e);

        CAMERA_DEBUG_LOG("CompleteCameraAsyncRoutine Destroyed event e=%p \n", e);

        if (pParam->inSurf2DUP)
        {
            CmSurface2DUP *surf = (CmSurface2DUP *)pParam->inSurf2DUP;
            m_cmDevice->DestroySurface2DUP(surf);
            pParam->inSurf2DUP = 0;
        }

        m_core->DecreaseReference(&pParam->surf_in->Data);

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif

        if (pParam->outSurf2DUP)
        {
            CmSurface2DUP *surf = (CmSurface2DUP *)pParam->outSurf2DUP;
            m_cmDevice->DestroySurface2DUP(surf);
            pParam->outSurf2DUP = 0;
        }

        m_core->DecreaseReference(&pParam->surf_out->Data);
    }

    m_core->DecreasePureReference(m_activeThreadCount);

    CAMERA_DEBUG_LOG(f, "CompleteCameraAsyncRoutine end: MemId=%p Y16=%p surfOut: MemId=%p B=%p \n", pParam->surf_in->Data.MemId, pParam->surf_in->Data.Y16, pParam->surf_out->Data.MemId, pParam->surf_out->Data.B);
    CAMERA_DEBUG_LOG(f, "CompleteCameraAsyncRoutine end pParam->surf_in->Data.Locked=%d pParam->surf_out->Data.Locked=%d \n", pParam->surf_in->Data.Locked, pParam->surf_out->Data.Locked);
    return sts;
}

mfxStatus MFXCamera_Plugin::VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData* /*aux*/, mfxThreadTask *mfxthreadtask)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    if (!surface_in)
        return MFX_ERR_MORE_DATA;
    if (!surface_out)
        return MFX_ERR_NULL_PTR;

    if(m_mfxVideoParam.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        if(!surface_in->Data.Y16)
            return MFX_ERR_NULL_PTR;
        if(surface_in->Data.Pitch & 15)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if(m_mfxVideoParam.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        if(!surface_out->Data.B)
            return MFX_ERR_NULL_PTR;

        if (surface_out->Info.FourCC == MFX_FOURCC_RGB4)
        {
            surface_out->Data.G = surface_out->Data.B + 1;
            surface_out->Data.R = surface_out->Data.B + 2;
            surface_out->Data.A = surface_out->Data.B + 3;
        }
        else
        { // ARGB16
            surface_out->Data.U16 = surface_out->Data.V16 + 1;
            surface_out->Data.Y16 = surface_out->Data.V16 + 2;
            surface_out->Data.A = (mfxU8 *)(surface_out->Data.V16 + 3);
        }

        if(surface_out->Data.Pitch & 15)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if(!surface_in->Info.Width || !surface_out->Info.Width || !surface_in->Info.Height || !surface_out->Info.Height)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if(surface_in->Info.Width != m_mfxVideoParam.vpp.In.Width || surface_in->Info.Height != m_mfxVideoParam.vpp.In.Height || surface_in->Info.Width != m_mfxVideoParam.vpp.In.Width || surface_in->Info.Height != m_mfxVideoParam.vpp.In.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(!surface_in->Info.CropW || !surface_in->Info.CropH || !surface_out->Info.CropW || !surface_out->Info.CropH )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(surface_in->Info.CropW > surface_in->Info.Width || surface_in->Info.CropH > surface_in->Info.Height || surface_out->Info.CropW > surface_out->Info.Width || surface_out->Info.CropH >surface_out->Info.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (surface_in->Info.CropH + surface_in->Info.CropY > surface_in->Info.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (surface_out->Info.CropH + surface_out->Info.CropY > surface_out->Info.Height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(surface_in->Info.FourCC != MFX_FOURCC_R16)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(surface_out->Info.FourCC != MFX_FOURCC_RGB4 && surface_out->Info.FourCC != MFX_FOURCC_ARGB16)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    AsyncParams *pParams = new AsyncParams;
    pParams->surf_in = surface_in;
    pParams->surf_out = surface_out;

    *mfxthreadtask = (mfxThreadTask*) pParams;

    return mfxRes;
}

mfxStatus MFXCamera_Plugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(task, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    sts = CameraRoutine(this, task, uid_p, uid_a);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus MFXCamera_Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    mfxStatus sts = MFX_ERR_NONE;
    sts = CompleteCameraRoutine(this, task, MFX_ERR_NONE);
    MFX_CHECK_STS(sts);

    return sts;
}