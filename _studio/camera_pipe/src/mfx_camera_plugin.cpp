//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_camera_plugin.h"
#include "mfx_session.h"
#include "mfx_task.h"

#ifdef CAMP_PIPE_ITT
#include "ittnotify.h"
__itt_domain* CamPipe = __itt_domain_create(L"CamPipe");
__itt_string_handle* task1 = __itt_string_handle_create(L"CreateEnqueueTasks");
__itt_string_handle* taske = __itt_string_handle_create(L"WaitEvent");
__itt_string_handle* tasks = __itt_string_handle_create(L"SetExtSurf");
__itt_string_handle* task_destroy_surfup = __itt_string_handle_create(L"task_destroy_surfup");
__itt_string_handle* destroyevent = __itt_string_handle_create(L"destroyevent");
__itt_string_handle* destroyeventwowait = __itt_string_handle_create(L"destroyevenwowait");
__itt_string_handle* VPPFrameSubmitmark = __itt_string_handle_create(L"VPPFrameSubmit");
__itt_string_handle* VPPExecuteMark = __itt_string_handle_create(L"VPPExecute");
__itt_string_handle* VPPAsyncRoutineMark = __itt_string_handle_create(L"VPPAsyncRoutine");
__itt_string_handle* VPPCompleteRoutineMark = __itt_string_handle_create(L"VPPCompleteRoutine");
#endif

#include "mfx_plugin_module.h"
#if defined(LINUX32) || defined(LINUX64)
#include "plugin_version_linux.h"
#endif
PluginModuleTemplate g_PluginModule = {
    NULL,
    NULL,
    NULL,
    &MFXCamera_Plugin::Create,
    &MFXCamera_Plugin::CreateByDispatcher
};

#ifndef UNIFIED_PLUGIN
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

#endif

const mfxPluginUID MFXCamera_Plugin::g_Camera_PluginGuid = MFX_PLUGINID_CAMERA_HW;

MFXCamera_Plugin::MFXCamera_Plugin(bool CreateByDispatcher)
{
    m_session  = 0;
    m_pmfxCore = 0;
    m_useSW    = false;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_PARALLEL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_Camera_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_VPP;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;
    m_pmfxCore = NULL;
    m_core = NULL;
    Zero(m_Caps);
    m_adapter.reset(0);
    m_Caps.InputMemoryOperationMode  = MEM_GPU;
    m_Caps.OutputMemoryOperationMode = MEM_GPU;
    m_Caps.BayerPatternType          = MFX_CAM_BAYER_RGGB;

    Zero(m_session);
    Zero(m_GammaParams);
    Zero(m_VignetteParams);
    Zero(m_BlackLevelParams);
    Zero(m_3DLUTParams);
    Zero(m_CCMParams);
    Zero(m_WBparams);
    Zero(m_HPParams);
    Zero(m_DenoiseParams);
    Zero(m_LensParams);
    Zero(m_PaddingParams);
    Zero(m_mfxVideoParam);
    Zero(m_PipeParams.Caps);
    Zero(m_PipeParams.GammaParams);
    Zero(m_PipeParams.par);
    Zero(m_PipeParams.TileInfo);
    Zero(m_PipeParams.VignetteParams);
    
    m_PipeParams.tileNumHor = 0;
    m_PipeParams.tileNumVer = 0;
    m_PipeParams.TileWidth = 0;
    m_PipeParams.TileHeight = 0;
    m_PipeParams.TileHeightPadded = 0;
    m_PipeParams.frameWidth = 0;
    m_PipeParams.frameWidth64 = 0;
    m_PipeParams.paddedFrameHeight = 0;
    m_PipeParams.paddedFrameWidth = 0;
    m_PipeParams.vSliceWidth = 0;
    m_PipeParams.BitDepth = 0;

    m_InputBitDepth = 0;
    m_CameraProcessor   = NULL;
    m_isInitialized     = false;
    m_useSW = false;
    //by default we don't expect falback on SKL+ platforms
    m_fallback = FALLBACK_NONE;
    m_activeThreadCount = 0;
    m_InitHeight = 0;
    m_InitWidth = 0;
    m_platform = MFX_HW_UNKNOWN;
    m_PipeParams.tileOffsets = 0;
    m_nTilesHor = 1; // Single tile by default
    m_nTilesVer = 1;
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

    if ( m_PipeParams.tileOffsets )
        delete [] m_PipeParams.tileOffsets;

    if ( m_CameraProcessor )
    {
        delete m_CameraProcessor;
    }

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

        if (out->vpp.In.FourCC != MFX_FOURCC_R16 && 
            out->vpp.In.FourCC != MFX_FOURCC_ARGB16 &&
            out->vpp.In.FourCC != MFX_FOURCC_ABGR16)
        {
            {
                out->vpp.In.FourCC = 0;
                out->vpp.Out.ChromaFormat = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if (out->vpp.Out.FourCC != MFX_FOURCC_RGB4 &&
            out->vpp.Out.FourCC != MFX_FOURCC_ARGB16 &&
            out->vpp.Out.FourCC != MFX_FOURCC_ABGR16 &&
            out->vpp.Out.FourCC != MFX_FOURCC_NV12)
        {
            {
                out->vpp.Out.FourCC = 0;
                out->vpp.Out.ChromaFormat = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if(out->vpp.Out.ChromaFormat != MFX_CHROMAFORMAT_YUV444 &&
            out->vpp.Out.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        {
            {
                out->vpp.Out.ChromaFormat = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if(out->vpp.In.FourCC == MFX_FOURCC_R16 && out->vpp.In.ChromaFormat != MFX_CHROMAFORMAT_YUV400)
        {
            // R16 must be monochrome
            out->vpp.In.ChromaFormat = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if(out->vpp.In.FourCC != MFX_FOURCC_R16 && out->vpp.In.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
        {
            // Non-R16 input must be RGB-like
            out->vpp.In.ChromaFormat = 0;
            sts = MFX_ERR_UNSUPPORTED;
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
            if (out->vpp.In.Width > MAX_CAMERA_SUPPORTED_WIDTH)
            {
                out->vpp.In.Width = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

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
            if (out->vpp.In.Height > MAX_CAMERA_SUPPORTED_HEIGHT)
            {
                out->vpp.In.Height = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

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
            if (out->vpp.Out.Width > MAX_CAMERA_SUPPORTED_WIDTH)
            {
                out->vpp.Out.Width = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

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
                mfxU32 InWidth  = out->vpp.In.CropW;
                mfxU32 InHeight = out->vpp.In.CropH;

                mfxU32 OutWidth  = out->vpp.Out.CropW;
                mfxU32 OutHeight = out->vpp.Out.CropH;

                if(InWidth  != OutWidth)
                {
                    out->vpp.Out.Width = 0;
                    out->vpp.In.Width  = 0;
                    out->vpp.Out.CropW = 0;
                    out->vpp.In.CropW  = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                }

                if(InHeight != OutHeight)
                {
                    out->vpp.Out.Height = 0;
                    out->vpp.In.Height  = 0;
                    out->vpp.Out.CropH = 0;
                    out->vpp.In.CropH  = 0;
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

            if (out->vpp.Out.Height > MAX_CAMERA_SUPPORTED_HEIGHT)
            {
                out->vpp.Out.Height = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

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
                mfxU32 InWidth  = out->vpp.In.CropW;
                mfxU32 InHeight = out->vpp.In.CropH;

                mfxU32 OutWidth  = out->vpp.Out.CropW;
                mfxU32 OutHeight = out->vpp.Out.CropH;

            if(InWidth  != OutWidth)
            {
                out->vpp.Out.Width = 0;
                out->vpp.In.Width  = 0;
                out->vpp.Out.CropW  = 0;
                out->vpp.In.CropW   = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if(InHeight != OutHeight)
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

        CAMERA_DEBUG_LOG("Query end sts =  %d  mfxSts = %d \n", sts, mfxSts);

        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;
        else if (sts < MFX_ERR_NONE)
            return sts;

        m_core = m_session->m_pCORE.get();
        m_platform = m_core->GetHWType();

#if defined (_WIN32) || defined (_WIN64)
        if (MFX_HW_SCL<= m_platform)
        {
            if (in->vpp.In.CropW > 8160 && MFX_FOURCC_ARGB16 == in->vpp.Out.FourCC)
            {
                // Some issues observed on >8K resolutions with argb16 out on SKL. Fallback to CM at the moment;
                m_fallback = FALLBACK_CM;
            }
        }
#endif

        if (FALLBACK_CM == m_fallback || MFX_HW_HSW == m_platform || MFX_HW_HSW_ULT == m_platform || MFX_HW_BDW == m_platform || MFX_HW_CHV == m_platform)
        {
            sts = CMCameraProcessor::Query(in, out);
        }
#if defined (_WIN32) || defined (_WIN64)
        else if (MFX_HW_SCL<= m_platform && MFX_HW_D3D11 == m_core->GetVAType())
        {
            sts = D3D11CameraProcessor::Query(in, out);
        }
        else if (MFX_HW_SCL<= m_platform && MFX_HW_D3D9 == m_core->GetVAType())
        {
            sts = D3D9CameraProcessor::Query(in, out);
        }
#endif
        else
        {
            sts = CPUCameraProcessor::Query(in, out);
        }

        if (sts < MFX_ERR_NONE)
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

    mfxVideoParam tmpPar = *par;
    mfxStatus sts = CheckIOPattern(par,&tmpPar,1);
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

    par->Protected  = 0;
    if( NULL == par->ExtParam || 0 == par->NumExtParam)
    {
        return MFX_ERR_NONE;
    }
    for( mfxU32 i = 0; i < par->NumExtParam; i++ )
    {
        if( MFX_EXTBUFF_VPP_DOUSE == par->ExtParam[i]->BufferId )
        {
            mfxExtVPPDoUse* pVPPHint = (mfxExtVPPDoUse*)(par->ExtParam[i]);
            mfxU32 numUsedFilters = m_Caps.GetFiltersNum();
            mfxU32 i = 0;

            if(numUsedFilters > pVPPHint->NumAlg)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            if ( m_Caps.bDemosaic )
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_PIPECONTROL;
            }

            if (m_Caps.bRGBToYUV)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_CSC_YUV_RGB;
            }

            if ( m_Caps.bNoPadding)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_PADDING;
            }

            if (m_Caps.bTotalColorControl)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_TOTAL_COLOR_CONTROL;
            }

            if ( m_Caps.b3DLUT)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_3DLUT;
            }

            if ( m_Caps.bBayerDenoise)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_BAYER_DENOISE;
            }

            if ( m_Caps.bBlackLevelCorrection)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION;
            }

            if ( m_Caps.bColorConversionMatrix)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3;
            }

            if ( m_Caps.bForwardGammaCorrection && m_Caps.bGamma3DLUT)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_FORWARD_GAMMA_CORRECTION;
            }

            if ( m_Caps.bForwardGammaCorrection && ! m_Caps.bGamma3DLUT)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_GAMMA_CORRECTION;
            }

            if ( m_Caps.bHotPixel)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL;
            }

            if ( m_Caps.bLensCorrection)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION;
            }

            if ( m_Caps.bVignetteCorrection)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_VIGNETTE_CORRECTION;
            }

            if ( m_Caps.bWhiteBalance)
            {
                pVPPHint->AlgList[i++] = MFX_EXTBUF_CAM_WHITE_BALANCE;
            }
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard1);

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(m_CameraProcessor, MFX_ERR_UNDEFINED_BEHAVIOR);
    m_isInitialized = false;

    mfxStatus sts = WaitForActiveThreads();

    sts = m_CameraProcessor->Close();

    if ( m_VignetteParams.pCmCorrectionMap )
        delete [] m_VignetteParams.pCmCorrectionMap;
    return sts;
}

mfxStatus MFXCamera_Plugin::ProcessExtendedBuffers(mfxVideoParam *par)
{
    mfxU32 i, j;
    mfxStatus sts = MFX_ERR_NONE;
    bool gammaset        = false;
    bool paddingset      = false;
    bool blacklevelset   = false;
    bool whitebalanceset = false;
    bool tccset          = false;
    bool ccmset          = false;
    bool vignetteset     = false;
    bool hotpixelset     = false;
    bool denoiseset      = false;
    bool lensset         = false;
    bool threeDlutset    = false;
    bool rgbtoyuvset     = false;

    for (i = 0; i < par->NumExtParam; i++)
    {
        if (par->ExtParam[i])
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
            else if (MFX_EXTBUF_CAM_FORWARD_GAMMA_CORRECTION == par->ExtParam[i]->BufferId)
            {
                m_Caps.bForwardGammaCorrection = 1;
                m_Caps.bGamma3DLUT             = true;
                mfxExtCamFwdGamma* gammaExtBufParams = (mfxExtCamFwdGamma*)par->ExtParam[i];

                if (gammaExtBufParams && gammaExtBufParams->Segment)
                {
                    if (gammaExtBufParams->NumSegments == MFX_CAM_GAMMA_NUM_POINTS_SKL) // only 64-point gamma is currently supported
                    {
                        gammaset = true;
                        m_GammaParams.NumSegments = MFX_CAM_GAMMA_NUM_POINTS_SKL;
                        for (int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
                        {
                            m_GammaParams.Segment[i].Pixel = gammaExtBufParams->Segment[i].Pixel;
                            m_GammaParams.Segment[i].Green = gammaExtBufParams->Segment[i].Green;
                            m_GammaParams.Segment[i].Red   = gammaExtBufParams->Segment[i].Red;
                            m_GammaParams.Segment[i].Blue  = gammaExtBufParams->Segment[i].Blue;
                        }
                    }
                }
            }
            else if (MFX_EXTBUF_CAM_GAMMA_CORRECTION == par->ExtParam[i]->BufferId)
            {
                m_Caps.bForwardGammaCorrection = 1;
                m_Caps.bGamma3DLUT             = false;
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
                            m_GammaParams.NumSegments = MFX_CAM_GAMMA_NUM_POINTS_SKL;
                            for (int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
                            {
                                m_GammaParams.Segment[i].Pixel = gammaExtBufParams->GammaPoint[i];
                                m_GammaParams.Segment[i].Green = gammaExtBufParams->GammaCorrected[i];
                                m_GammaParams.Segment[i].Red   = gammaExtBufParams->GammaCorrected[i];
                                m_GammaParams.Segment[i].Blue  = gammaExtBufParams->GammaCorrected[i];
                            }
                        }
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
            else if (MFX_EXTBUF_CAM_TOTAL_COLOR_CONTROL == par->ExtParam[i]->BufferId)
            {
                m_Caps.bTotalColorControl = 1;
                mfxExtCamTotalColorControl* tccExtBufParams = (mfxExtCamTotalColorControl*)par->ExtParam[i];
                if (tccExtBufParams)
                {
                    tccset = true;
                    m_TCCParams.bActive = true;
                    m_TCCParams.R = tccExtBufParams->R;
                    m_TCCParams.G = tccExtBufParams->G;
                    m_TCCParams.B = tccExtBufParams->B;
                    m_TCCParams.C = tccExtBufParams->C;
                    m_TCCParams.M = tccExtBufParams->M;
                    m_TCCParams.Y = tccExtBufParams->Y;
                }
            }
            else if (MFX_EXTBUF_CAM_CSC_YUV_RGB == par->ExtParam[i]->BufferId)
            {
                m_Caps.bRGBToYUV = 1;
                mfxExtCamCscYuvRgb* rgbToYuvExtBufParams = (mfxExtCamCscYuvRgb*)par->ExtParam[i];
                if (rgbToYuvExtBufParams)
                {
                    rgbtoyuvset = true;
                    m_RGBToYUVParams.bActive = true;
                    for (int i = 0; i < 3; i++) {
                        m_RGBToYUVParams.PreOffset[i] = rgbToYuvExtBufParams->PreOffset[i];
                        m_RGBToYUVParams.PostOffset[i] = rgbToYuvExtBufParams->PostOffset[i];
                    }
                    for (int i = 0; i < 3; i++)
                    {
                        for (int j = 0; j < 3; j++)
                        {
                            m_RGBToYUVParams.Matrix[i][j] = rgbToYuvExtBufParams->Matrix[i][j];
                        }
                    }
                }
            }
            else if (MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL == par->ExtParam[i]->BufferId)
            {
                m_Caps.bHotPixel = 1;
                mfxExtCamHotPixelRemoval* hotpixelExtBufParams = (mfxExtCamHotPixelRemoval*)par->ExtParam[i];
                if ( hotpixelExtBufParams )
                {
                    hotpixelset = true;
                    m_HPParams.bActive = true;
                    m_HPParams.PixelCountThreshold      = hotpixelExtBufParams->PixelCountThreshold;
                    m_HPParams.PixelThresholdDifference = hotpixelExtBufParams->PixelThresholdDifference;
                }
            }
            else if (MFX_EXTBUFF_VPP_DENOISE == par->ExtParam[i]->BufferId || MFX_EXTBUF_CAM_BAYER_DENOISE == par->ExtParam[i]->BufferId)
            {
                m_Caps.bBayerDenoise = 1;
                mfxExtCamBayerDenoise* denoiseExtBufParams = (mfxExtCamBayerDenoise*)par->ExtParam[i];
                if ( denoiseExtBufParams )
                {
                    denoiseset = true;
                    m_DenoiseParams.bActive = true;
                    m_DenoiseParams.Threshold = denoiseExtBufParams->Threshold;
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
                    m_VignetteParams.Height  = (mfxU16)VignetteExtBufParams->Height;
                    m_VignetteParams.Width   = (mfxU16)VignetteExtBufParams->Width;
                    m_VignetteParams.Stride  = (mfxU16)VignetteExtBufParams->Pitch;
                    m_VignetteParams.CmWidth  = m_VignetteParams.Width >> 2;
                    m_VignetteParams.CmStride = m_VignetteParams.Stride >> 2;
                    m_VignetteParams.pCorrectionMap  = (CameraPipeVignetteCorrectionElem *)VignetteExtBufParams->CorrectionMap;
                    if ( m_VignetteParams.pCmCorrectionMap )
                        delete [] m_VignetteParams.pCmCorrectionMap;
                    m_VignetteParams.pCmCorrectionMap = (CameraPipeVignetteCorrectionElem *)new mfxU8[m_VignetteParams.Height *  m_VignetteParams.CmStride];
                    MFX_CHECK_NULL_PTR1(m_VignetteParams.pCorrectionMap);
                    MFX_CHECK_NULL_PTR1(m_VignetteParams.pCmCorrectionMap)
                    IppiSize size = {2, m_VignetteParams.CmWidth*m_VignetteParams.Height / 2  };
                    IppStatus ippSts;
                    ippSts = ippiCopy_8u_C1R((mfxU8*)m_VignetteParams.pCorrectionMap, 8, (mfxU8*)m_VignetteParams.pCmCorrectionMap, 2, size);
                    MFX_CHECK_STS((mfxStatus)ippSts);
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
            else if (MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION == par->ExtParam[i]->BufferId)
            {
                m_Caps.bLensCorrection = 1;
                mfxExtCamLensGeomDistCorrection* LensExtBufParams = (mfxExtCamLensGeomDistCorrection*)par->ExtParam[i];
                if ( LensExtBufParams )
                {
                    lensset = true;
                    m_LensParams.bActive = true;
                    m_LensParams.a[0] = LensExtBufParams->a[0];
                    m_LensParams.a[1] = LensExtBufParams->a[1];
                    m_LensParams.a[2] = LensExtBufParams->a[2];
                    m_LensParams.b[0] = LensExtBufParams->b[0];
                    m_LensParams.b[1] = LensExtBufParams->b[1];
                    m_LensParams.b[2] = LensExtBufParams->b[2];
                    m_LensParams.c[0] = LensExtBufParams->c[0];
                    m_LensParams.c[1] = LensExtBufParams->c[1];
                    m_LensParams.c[2] = LensExtBufParams->c[2];
                    m_LensParams.d[0] = LensExtBufParams->d[0];
                    m_LensParams.d[1] = LensExtBufParams->d[1];
                    m_LensParams.d[2] = LensExtBufParams->d[2];
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
                m_Caps.BayerPatternType = BayerPattern_API2CM(pipeExtBufParams->RawFormat);
            }
            else if (MFX_EXTBUF_CAM_3DLUT == par->ExtParam[i]->BufferId)
            {
                mfxExtCam3DLut* pipeExtBufParams = (mfxExtCam3DLut*)par->ExtParam[i];
                m_Caps.b3DLUT = 1;
                if ( pipeExtBufParams && pipeExtBufParams->Table )
                {
                    threeDlutset = true;
                    m_3DLUTParams.size = pipeExtBufParams->Size;
                    m_3DLUTParams.lut  = pipeExtBufParams->Table;
                }
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
    if (m_Caps.bTotalColorControl)
        if (!tccset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;
    
    if (m_Caps.bRGBToYUV)
        if (!rgbtoyuvset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bWhiteBalance)
        if (!whitebalanceset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bVignetteCorrection)
        if (!vignetteset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bHotPixel)
        if (!hotpixelset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bBayerDenoise)
        if (!denoiseset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.bLensCorrection)
        if (!lensset)
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (m_Caps.b3DLUT)
        if (!threeDlutset)
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

    m_platform = m_core->GetHWType();
    m_fallback = FALLBACK_NONE;
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

    if (m_mfxVideoParam.vpp.Out.FourCC == MFX_FOURCC_RGB4 || m_mfxVideoParam.vpp.Out.FourCC == MFX_FOURCC_NV12)
    {
        m_Caps.bOutToARGB16 = 0;
    }
    else if (m_mfxVideoParam.vpp.Out.FourCC == MFX_FOURCC_ARGB16)
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
        m_nTilesHor = (m_mfxVideoParam.vpp.In.CropH > 5000 ) ? 4 : 2;
        m_nTilesVer = (m_mfxVideoParam.vpp.In.CropW > 8000 ) ? 2 : 1;
    }
    else
    {
         m_nTilesVer = 1;
         m_nTilesHor = 1;
    }

    m_PaddingParams.top    = MFX_CAM_DEFAULT_PADDING_TOP;
    m_PaddingParams.bottom = MFX_CAM_DEFAULT_PADDING_BOTTOM;
    m_PaddingParams.left   = MFX_CAM_DEFAULT_PADDING_LEFT;
    m_PaddingParams.right  = MFX_CAM_DEFAULT_PADDING_RIGHT;


    mfxU32 aligned_cut = 0;

    m_PipeParams.frameWidth        = m_nTilesVer > 1 ? ((m_mfxVideoParam.vpp.In.CropW / m_nTilesVer + 15) &~ 0xF) + 16 : m_mfxVideoParam.vpp.In.CropW;
    m_PipeParams.frameWidth64      = ((m_PipeParams.frameWidth  + 31) &~ 0x1F); // 2 bytes each for In, 4 bytes for Out, so 32 is good enough for 64 ???
    m_PipeParams.paddedFrameWidth  = m_PipeParams.frameWidth + m_PaddingParams.left + m_PaddingParams.right;
    m_PipeParams.paddedFrameHeight = m_mfxVideoParam.vpp.In.CropH  + m_PaddingParams.top + m_PaddingParams.bottom;
    m_PipeParams.vSliceWidth       = (((m_PipeParams.frameWidth / CAM_PIPE_KERNEL_SPLIT)  + 15) &~ 0xF) + 16;
    m_PipeParams.tileNumHor        = m_nTilesHor;
    m_PipeParams.tileNumVer        = m_nTilesVer;
    m_PipeParams.tileOffsets       = new CameraTileOffset[m_nTilesHor];
    m_PipeParams.TileWidth         = m_mfxVideoParam.vpp.In.CropW;
    m_PipeParams.BitDepth          = m_mfxVideoParam.vpp.In.BitDepthLuma;
    m_PipeParams.TileInfo          = m_mfxVideoParam.vpp.In;

    if(!m_Caps.bNoPadding)
    {
        m_PipeParams.TileHeight        = m_mfxVideoParam.vpp.In.CropH;
        if ( m_nTilesHor > 1 )
        {
            // in case frame data pointer is 4K aligned, adding aligned_cut gives another 4K aligned pointer
            // below of the tiles border
            aligned_cut = ( m_mfxVideoParam.vpp.In.CropH / m_nTilesHor - 0x3F ) &~0x3F;
            m_PipeParams.TileHeight = m_mfxVideoParam.vpp.In.CropH / (m_nTilesHor>>1) - aligned_cut;
        }
        m_PipeParams.TileHeightPadded  = m_PipeParams.TileHeight + m_PaddingParams.top + m_PaddingParams.bottom;
    }
    else
    {
        m_PipeParams.TileHeightPadded  = m_mfxVideoParam.vpp.In.CropH + m_PaddingParams.top + m_PaddingParams.bottom;
        if ( m_nTilesHor > 1 )
        {
            aligned_cut = ( m_PipeParams.paddedFrameHeight / m_nTilesHor - 0x3F ) &~0x3F;
            m_PipeParams.TileHeightPadded = m_mfxVideoParam.vpp.In.CropH / (m_nTilesHor>>1) - aligned_cut;
        }
        m_PipeParams.TileHeight  = m_PipeParams.TileHeightPadded - m_PaddingParams.top - m_PaddingParams.bottom;
        m_PipeParams.TileInfo.CropX -= MFX_CAM_DEFAULT_PADDING_LEFT;
        m_PipeParams.TileInfo.CropY -= MFX_CAM_DEFAULT_PADDING_TOP;
    }

    for (int i = 0; i < m_nTilesHor; i++)
    {
        if ( m_nTilesHor - 1 == i && i > 0)
        {
            // In case of several tiles, last tile must be aligned to the original frame bottom
            if (!m_Caps.bNoPadding)
            {
                m_PipeParams.tileOffsets[i].TileOffset = m_mfxVideoParam.vpp.In.CropH - m_PipeParams.TileHeight;
            }
            else
            {
                m_PipeParams.tileOffsets[i].TileOffset = m_PipeParams.paddedFrameHeight - m_PipeParams.TileHeightPadded;
            }
        }
        else
        {
            m_PipeParams.tileOffsets[i].TileOffset = aligned_cut * i;
        }
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

    m_PipeParams.Caps        = m_Caps;
    m_PipeParams.GammaParams = m_GammaParams;
    m_PipeParams.VignetteParams = m_VignetteParams;
    m_PipeParams.par         = *par;

    if (FALLBACK_CM == m_fallback || MFX_HW_HSW == m_platform || MFX_HW_HSW_ULT == m_platform || MFX_HW_BDW == m_platform || MFX_HW_CHV == m_platform)
    {
        m_CameraProcessor = new CMCameraProcessor();
    }
#if defined (_WIN32) || defined (_WIN64)
    else if (MFX_HW_SCL<= m_platform && MFX_HW_D3D11 == m_core->GetVAType())
    {
        m_CameraProcessor = new D3D11CameraProcessor();
    }
    else if (MFX_HW_SCL<= m_platform && MFX_HW_D3D9 == m_core->GetVAType())
    {
        m_CameraProcessor = new D3D9CameraProcessor();
    }
#endif
    else
    {
        // Fallback to SW in case there is no appripriate HW-accel version found
        m_CameraProcessor = new CPUCameraProcessor();
        m_useSW = true;
    }

    m_CameraProcessor->SetCore(m_core);
    mfxSts = m_CameraProcessor->Init(&m_PipeParams);
    MFX_CHECK_STS(mfxSts);

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
        m_nTilesHor = (newParam.vpp.In.CropH > 5000 ) ? 4 : 2;
        m_nTilesVer = (newParam.vpp.In.CropW > 8000 ) ? 2 : 1;
        if ( newParam.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY )
        {
            // In case of tiling, only system memory is supported as output.
            // Must be documented
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
         m_nTilesHor = 1;
         m_nTilesVer = 1;
    }

    CameraParams pipeParams;
    pipeParams.frameWidth        = m_nTilesVer > 1 ? ((newParam.vpp.In.CropW / m_nTilesVer + 15) &~ 0xF) + 16 : newParam.vpp.In.CropW;
    pipeParams.frameWidth64      = ((pipeParams.frameWidth + 31) &~ 0x1F); // 2 bytes each for In, 4 bytes for Out, so 32 is good enough for 64 ???
    pipeParams.paddedFrameWidth  = pipeParams.frameWidth + m_PaddingParams.left + m_PaddingParams.right;
    pipeParams.paddedFrameHeight = newParam.vpp.In.CropH  + m_PaddingParams.top + m_PaddingParams.bottom;
    pipeParams.vSliceWidth       = (((pipeParams.frameWidth / CAM_PIPE_KERNEL_SPLIT)  + 15) &~ 0xF) + 16;
    pipeParams.tileNumHor        = m_nTilesHor;
    pipeParams.tileNumVer        = m_nTilesVer;
    pipeParams.tileOffsets       = new CameraTileOffset[m_nTilesHor];
    pipeParams.TileWidth         = newParam.vpp.In.CropW;
    pipeParams.TileHeight        = newParam.vpp.In.CropH;
    if ( m_nTilesHor > 1 )
    {
        // in case frame data pointer is 4K aligned, aligned_cut gives another 4K aligned pointer
        // below of the tiles border
        mfxU32 aligned_cut = ( newParam.vpp.In.CropH / m_nTilesHor - 0x3F ) &~0x3F;
        pipeParams.TileHeight = newParam.vpp.In.CropH / (m_nTilesHor>>1) - aligned_cut;
    }
    pipeParams.TileHeightPadded  = pipeParams.TileHeight + m_PaddingParams.top + m_PaddingParams.bottom;
    pipeParams.BitDepth          = newParam.vpp.In.BitDepthLuma;
    pipeParams.TileInfo          = newParam.vpp.In;
    for (int i = 0; i < m_nTilesHor; i++)
    {
        if ( m_nTilesHor - 1 == i && i > 0)
        {
            // In case of several tiles, last tile must be aligned to the original frame bottom
            pipeParams.tileOffsets[i].TileOffset = newParam.vpp.In.CropH - pipeParams.TileHeight;
            pipeParams.tileOffsets[i].TileOffset = ((pipeParams.tileOffsets[i].TileOffset + 1)/2)*2;
        }
        else
        {
            pipeParams.tileOffsets[i].TileOffset = ( pipeParams.TileHeight ) * i;
        }
    }

    pipeParams.Caps        = m_Caps;
    pipeParams.GammaParams = m_GammaParams;
    pipeParams.par         = *par;
    m_CameraProcessor->Reset(&newParam, &pipeParams);

    m_mfxVideoParam = newParam;
    m_PipeParams    = pipeParams;
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
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, VPPAsyncRoutineMark);
#endif
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK(m_isInitialized,   MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_CameraProcessor, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(pParam, MFX_ERR_UNDEFINED_BEHAVIOR);

    sts = m_core->IncreasePureReference(m_activeThreadCount);
    MFX_CHECK_STS(sts);

    sts = m_CameraProcessor->AsyncRoutine(pParam);
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif
    return sts;
}

mfxStatus MFXCamera_Plugin::CompleteCameraRoutine(void *pState, void *pParam, mfxStatus taskRes)
{
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, VPPCompleteRoutineMark);
#endif
    mfxStatus sts;
    taskRes;
    pState;
    MFXCamera_Plugin & impl = *(MFXCamera_Plugin *)pState;
    AsyncParams *asyncParams = (AsyncParams *)pParam;

    sts = impl.CompleteCameraAsyncRoutine(asyncParams);

    if (pParam)
        delete (AsyncParams *)pParam; // not safe !!! ???
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif
     return sts;
}

mfxStatus MFXCamera_Plugin::CompleteCameraAsyncRoutine(AsyncParams *pParam)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK(m_isInitialized,   MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_CameraProcessor, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(pParam, MFX_ERR_UNDEFINED_BEHAVIOR);

    sts = m_CameraProcessor->CompleteRoutine(pParam);
    MFX_CHECK_STS(sts);

    sts = m_core->DecreasePureReference(m_activeThreadCount);

    return sts;
}

mfxStatus MFXCamera_Plugin::VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData* /*aux*/, mfxThreadTask *mfxthreadtask)
{
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, VPPFrameSubmitmark);
#endif
    UMC::AutomaticUMCMutex guard(m_guard1);
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    if (!surface_in)
        return MFX_ERR_MORE_DATA;
    if (!surface_out)
        return MFX_ERR_NULL_PTR;

    if(m_mfxVideoParam.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        if(MFX_FOURCC_R16 == surface_in->Info.FourCC)
        {
            if(!surface_in->Data.Y16)
                return MFX_ERR_NULL_PTR;
        }
        else
        {
            if(!surface_in->Data.R || !surface_in->Data.G || !surface_in->Data.B )
                return MFX_ERR_NULL_PTR;
        }

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
        else if (surface_out->Info.FourCC == MFX_FOURCC_ARGB16)
        { // ARGB16. Why do we update pointers here? W/a for some canon problem?
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
    if(surface_in->Info.FourCC != MFX_FOURCC_R16 && surface_in->Info.FourCC != MFX_FOURCC_ARGB16 && surface_in->Info.FourCC != MFX_FOURCC_ABGR16)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(surface_out->Info.FourCC != MFX_FOURCC_RGB4 &&
       surface_out->Info.FourCC != MFX_FOURCC_ARGB16 &&
       surface_out->Info.FourCC != MFX_FOURCC_ABGR16 &&
       surface_out->Info.FourCC != MFX_FOURCC_NV12)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    AsyncParams *pParams = new AsyncParams;
    pParams->surf_in  = surface_in;
    pParams->surf_out = surface_out;
    pParams->Caps     = m_Caps;
    pParams->FrameSizeExtra   = m_PipeParams;
    pParams->DenoiseParams    = m_DenoiseParams;
    pParams->HPParams         = m_HPParams;
    pParams->BlackLevelParams = m_BlackLevelParams;
    pParams->CCMParams        = m_CCMParams;
    pParams->GammaParams      = m_GammaParams;
    pParams->InputBitDepth    = m_InputBitDepth;
    pParams->PaddingParams    = m_PaddingParams;
    pParams->VignetteParams   = m_VignetteParams;
    pParams->WBparams         = m_WBparams;
    pParams->LensParams       = m_LensParams;
    pParams->LUTParams        = m_3DLUTParams;
    pParams->TCCParams        = m_TCCParams;
    pParams->RGBToYUVParams   = m_RGBToYUVParams;
    *mfxthreadtask = (mfxThreadTask*) pParams;
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif
    return mfxRes;
}

mfxStatus MFXCamera_Plugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, VPPExecuteMark);
#endif
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(task, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    sts = CameraRoutine(this, task, uid_p, uid_a);
    MFX_CHECK_STS(sts);
    
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif
    return sts;
}

mfxStatus MFXCamera_Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    mfxStatus sts = MFX_ERR_NONE;
    sts = CompleteCameraRoutine(this, task, MFX_ERR_NONE);
    MFX_CHECK_STS(sts);

    return sts;
}
