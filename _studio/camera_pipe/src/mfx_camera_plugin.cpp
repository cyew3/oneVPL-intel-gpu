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

const mfxPluginUID MFXCamera_Plugin::g_Camera_PluginGuid = {0x54, 0x54, 0x26, 0x16, 0x24, 0x33, 0x41, 0xe6, 0x93, 0xae, 0x89, 0x99, 0x42, 0xce, 0x73, 0x55};
std::auto_ptr<MFXCamera_Plugin> MFXCamera_Plugin::g_singlePlg;
std::auto_ptr<MFXPluginAdapter<MFXVPPPlugin> > MFXCamera_Plugin::g_adapter;


MFXCamera_Plugin::MFXCamera_Plugin(bool CreateByDispatcher)
{
    m_session = 0;
    m_pmfxCore = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_PARALLEL;//MFX_THREADPOLICY_SERIAL;
    //m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 4;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_Camera_PluginGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_VPP;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;

    Zero(m_Caps);
    m_Caps.InputMemoryOperationMode = MEM_GPU;
    m_Caps.OutputMemoryOperationMode = MEM_GPU;
    m_Caps.BayerPatternType = MFX_CAM_BAYER_RGGB;

    Zero(m_GammaParams);
    m_cmSurfIn = 0;
    m_cmSurfUPIn = 0;
    m_memIn = 0;
    m_cmBufferOut = 0;
    m_cmBufferUPOut = 0;
    m_memOut = 0;
    m_memOutAllocPtr = 0;

    m_gammaPointSurf = m_gammaCorrectSurf = 0;
    m_gammaOutSurf = 0;
    m_paddedSurf = 0;
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    m_avgFlagSurf = 0;
#endif

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
    if (!core)
        return MFX_ERR_NULL_PTR;
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &par);

    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    //par.Impl = MFX_IMPL_RUNTIME;

#if !defined (MFX_VA) && defined (AS_VPP_PLUGIN)
    par.Impl = MFX_IMPL_SOFTWARE;
#endif

    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);

    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pmfxCore->pthis, m_session);

    return mfxRes;
}

mfxStatus MFXCamera_Plugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxStatus mfxRes2 = MFX_ERR_NONE;
    if (m_session)
    {
        //The application must ensure there is no active task running in the session before calling this (MFXDisjoinSession) function.
        mfxRes = MFXVideoVPP_Close(m_session);
        //Return the first met wrn or error
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED)
            mfxRes2 = mfxRes;
        //uncomment when the light core is ready (?) kta
        mfxRes = MFXInternalPseudoDisjoinSession(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
           mfxRes2 = mfxRes;
        mfxRes = MFXClose(m_session);
        if(mfxRes != MFX_ERR_NONE && mfxRes != MFX_ERR_NOT_INITIALIZED && mfxRes2 == MFX_ERR_NONE)
            mfxRes2 = mfxRes;
        m_session = 0;
    }
    if (m_createdByDispatcher) {
        g_singlePlg.reset(0);
        g_adapter.reset(0);
    }
    return mfxRes2;
}

mfxStatus MFXCamera_Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}


mfxStatus MFXCamera_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus mfxSts = MFX_ERR_NONE;

    if (NULL == in)
    {
        memset(&out->mfx, 0, sizeof(mfxInfoMFX));
        memset(&out->vpp, 0, sizeof(mfxInfoVPP));

        /* vppIn */
        out->vpp.In.FourCC       = 1;
        out->vpp.In.Height       = 1;
        out->vpp.In.Width        = 1;
        out->vpp.In.CropW        = 1;
        out->vpp.In.CropH        = 1;
        out->vpp.In.CropX        = 1;
        out->vpp.In.CropY        = 1;
        //out->vpp.In.PicStruct    = 1;
        out->vpp.In.FrameRateExtN = 1;
        out->vpp.In.FrameRateExtD = 1;

        /* vppOut */
        out->vpp.Out.FourCC       = 1;
        out->vpp.Out.Height       = 1;
        out->vpp.Out.Width        = 1;

        //out->vpp.Out.CropW        = 1;
        //out->vpp.Out.CropH        = 1;
        //out->vpp.Out.CropX        = 1;
        //out->vpp.Out.CropY        = 1;

        //out->vpp.Out.PicStruct    = 1;
        //out->vpp.Out.FrameRateExtN = 1;
        //out->vpp.Out.FrameRateExtD = 1;

        out->Protected           = 0; //???
        out->IOPattern           = 1; //???
        out->AsyncDepth          = 1;

        if (0 == out->NumExtParam)
            out->NumExtParam     = 1;
        else
            mfxSts = CheckExtBuffers(out, NULL, MFX_CAM_QUERY_SIGNAL);
        return mfxSts;
    }
    else
    {
        out->vpp       = in->vpp;

        /* [asyncDepth] section */
        out->AsyncDepth = in->AsyncDepth;

        /* [Protected] section */
        out->Protected = in->Protected;
        if (out->Protected)
        {
            out->Protected = 0;
            mfxSts = MFX_ERR_UNSUPPORTED;
        }

        /* [IOPattern] section */
        out->IOPattern = in->IOPattern;
        mfxU32 temp = out->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY);
        if (temp==0 || temp==(MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY))
            out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        /* [ExtParam] section */
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
                MFX_CHECK_NULL_PTR1(in->ExtParam[i]);
            }
        }

        if (0 != out->ExtParam)
        {
            for (int i = 0; i < out->NumExtParam; i++)
            {
                MFX_CHECK_NULL_PTR1(out->ExtParam[i]);
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
                    //mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR;
                    mfxSts = MFX_ERR_NULL_PTR;
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

        } // if (in->ExtParam && out->ExtParam && (in->NumExtParam == out->NumExtParam) )

        mfxSts = CheckExtBuffers(in, out, MFX_CAM_QUERY_CHECK_RANGE);

        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;

        mfxStatus sts = MFX_ERR_NONE;
        if (out->vpp.In.FourCC != MFX_FOURCC_R16)
        {
            if (out->vpp.In.FourCC) // ERR_NONE if == 0 ???
            {
                out->vpp.In.FourCC = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }
        }

        if (out->vpp.Out.FourCC != MFX_FOURCC_RGB4) // !!! modify when 16-bit support added !!! 
        {
            if (out->vpp.Out.FourCC) // pass if == 0 ???
            {
                out->vpp.In.FourCC = 0;
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
            //if ((out->vpp.In.Width & 15) != 0)
            //{
            //    out->vpp.In.Width = 0;
            //    sts = MFX_ERR_UNSUPPORTED;
            //}

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

        if (out->vpp.In.Height)
        {
            //if ((out->vpp.In.Height & 15) != 0)
            //{
            //    out->vpp.In.Height = 0;
            //    sts = MFX_ERR_UNSUPPORTED;
            //}

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
        
        // todo: reconsider/correct the usage of CropW and Width ???
        //if (out->vpp.In.CropW)
        //{
        //    if ((out->vpp.In.CropW & 15) != 0) // need this limitation ???
        //    {
        //        out->vpp.In.CropW = 0;
        //        sts = MFX_ERR_UNSUPPORTED;
        //    }
        //}

        //if (out->vpp.In.CropH)
        //{
        //    if ((out->vpp.In.CropH & 15) != 0)
        //    {
        //        out->vpp.In.CropH = 0;
        //        sts = MFX_ERR_UNSUPPORTED;
        //    }
        //}

        if (!(out->vpp.Out.FrameRateExtN * out->vpp.Out.FrameRateExtD) &&
            (out->vpp.Out.FrameRateExtN + out->vpp.Out.FrameRateExtD))
        {
            out->vpp.Out.FrameRateExtN = 0;
            out->vpp.Out.FrameRateExtD = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (out->vpp.Out.Width)
        {
            //if ((out->vpp.Out.Width & 15) != 0)
            //{
            //    out->vpp.Out.Width = 0;
            //    sts = MFX_ERR_UNSUPPORTED;
            //}
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
        }

        if (out->vpp.Out.Height )
        {
            //if ((out->vpp.Out.Height  & 15) !=0)
            //{
            //    out->vpp.Out.Height = 0;
            //    sts = MFX_ERR_UNSUPPORTED;
            //}
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
        }

        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;
        else if (sts < MFX_ERR_NONE)
            return sts;
        else if (mfxSts != MFX_ERR_NONE)
            return mfxSts;
        else if (sts != MFX_ERR_NONE)
            return sts;

        return mfxSts;
    }
}


mfxStatus MFXCamera_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    if (!in)
        return MFX_ERR_NULL_PTR;
    if (!out)
        return MFX_ERR_NULL_PTR;

    mfxU16 asyncDepth = par->AsyncDepth;
    if (asyncDepth == 0)
        asyncDepth = m_core->GetAutoAsyncDepth();

    in->Info = par->vpp.In;
    in->NumFrameMin = 1;
    in->NumFrameSuggested = in->NumFrameMin + asyncDepth - 1;
    in->Type = MFX_MEMTYPE_FROM_VPPIN;

    out->Info = par->vpp.Out;
    out->NumFrameMin = 1;
    out->NumFrameSuggested = out->NumFrameMin + asyncDepth - 1;
    out->Type = MFX_MEMTYPE_FROM_VPPOUT;

    return MFX_ERR_NONE;
}

mfxStatus  MFXCamera_Plugin::GetVideoParam(mfxVideoParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_mfxVideoParam;
    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::Close()
{
    m_raw16padded.Free();
    m_raw16aligned.Free();
    m_aux8.Free();
    m_rawIn.Free();

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    if (m_avgFlagSurf)
        m_cmDevice->DestroySurface(m_avgFlagSurf);
#endif
    if (m_gammaCorrectSurf)
        m_cmDevice->DestroySurface(m_gammaCorrectSurf);

    if (m_gammaPointSurf)
        m_cmDevice->DestroySurface(m_gammaPointSurf);

    if (m_gammaOutSurf)
        m_cmDevice->DestroySurface(m_gammaOutSurf);

    if (m_paddedSurf)
        m_cmDevice->DestroySurface(m_paddedSurf);

    m_cmDevice.Reset(0);

    return MFX_ERR_NONE;
}

/*
mfxExtBuffer* GetExtendedBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    if (extBuf != 0)
    {
        for (mfxU16 i = 0; i < numExtBuf; i++)
        {
            if (extBuf[i] != 0 && extBuf[i]->BufferId == id) // assuming aligned buffers
                return (extBuf[i]);
        }
    }
    return 0;
}
*/

mfxStatus MFXCamera_Plugin::ProcessExtendedBuffers(mfxVideoParam *par)
{
    mfxU32 i, j;
    mfxStatus sts = MFX_ERR_NONE;
    bool gammaset = false;
    bool paddingset = false;

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
                        if (gammaExtBufParams->GammaPoint && gammaExtBufParams->GammaCorrected && gammaExtBufParams->NumPoints == MFX_CAM_DEFAULT_NUM_GAMMA_POINTS) // only 64-point gamma is currently supported  
                        {
                            m_GammaParams.gamma_lut.gammaPoints = gammaExtBufParams->GammaPoint;
                            m_GammaParams.gamma_lut.gammaCorrect = gammaExtBufParams->GammaCorrected;
                            m_GammaParams.gamma_lut.numPoints = gammaExtBufParams->NumPoints;
                            gammaset = true;
                        }
                    } 
                    else 
                    { // if (gammaExtBufParams->Mode == MFX_GAMMA_VALUE) {
                        m_GammaParams.gamma_value = gammaExtBufParams->GammaValue;
                        m_GammaParams.gamma_lut.gammaPoints = m_GammaParams.gamma_lut.gammaCorrect = 0;
                        if (m_GammaParams.gamma_value > 0)
                            gammaset = true;
                    }
                }
            }
            else if (MFX_EXTBUF_CAM_PADDING == par->ExtParam[i]->BufferId) 
            {
                //????
                m_Caps.bNoPadding = 1;
                paddingset = true;
                mfxExtCamPadding* paddingExtBufParams = (mfxExtCamPadding*)par->ExtParam[i];
                //m_PaddingParams.mode = paddingExtBufParams->Mode;
                m_PaddingParams.top = paddingExtBufParams->Top;
                m_PaddingParams.bottom = paddingExtBufParams->Bottom;
                m_PaddingParams.left = paddingExtBufParams->Left;
                m_PaddingParams.right = paddingExtBufParams->Right;
            }
            else if (MFX_EXTBUF_CAM_PIPECONTROL == par->ExtParam[i]->BufferId) 
            {
                mfxExtCamPipeControl* pipeExtBufParams = (mfxExtCamPipeControl*)par->ExtParam[i];
                m_Caps.BayerPatternType = pipeExtBufParams->RawFormat;
                
                /// !!!
                m_Caps.Reserved = pipeExtBufParams->reserved1; // tmp for testing reasons: switching between GPUSHARED and FASTGPUSPY
            }
        }
    }

    if (m_Caps.bForwardGammaCorrection) 
    {
        if (!gammaset) 
        {
            m_GammaParams.gamma_lut.gammaPoints = default_gamma_point;
            m_GammaParams.gamma_lut.gammaCorrect = default_gamma_correct;
            m_GammaParams.gamma_lut.numPoints = MFX_CAM_DEFAULT_NUM_GAMMA_POINTS;
        }
    }

    if (m_Caps.bNoPadding)
    {
        if (!paddingset)
        {
            m_PaddingParams.top = MFX_CAM_DEFAULT_PADDING_TOP;
            m_PaddingParams.bottom = MFX_CAM_DEFAULT_PADDING_BOTTOM;
            m_PaddingParams.left = MFX_CAM_DEFAULT_PADDING_LEFT;
            m_PaddingParams.right = MFX_CAM_DEFAULT_PADDING_RIGHT;
        }
    }
    return sts;
}

mfxStatus MFXCamera_Plugin::Init(mfxVideoParam *par)
{
    mfxStatus mfxSts;

    m_mfxVideoParam = *par;
    m_core = m_session->m_pCORE.get();

    mfxSts = Query(&m_mfxVideoParam, &m_mfxVideoParam);
    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    if (m_mfxVideoParam.AsyncDepth == 0)
        m_mfxVideoParam.AsyncDepth = m_core->GetAutoAsyncDepth();

    m_Caps.ModuleConfiguration = 0; // zero all filters
    m_Caps.bDemosaic = 1;           // demosaic is always on
    ProcessExtendedBuffers(&m_mfxVideoParam);

    m_Caps.bOutToARGB8 = 1;
    // uncomment and midify below when 16-bit output is supported
    //if (m_mfxVideoParam.vpp.Out.FourCC != MFX_FOURCC_RGB4) {
    //    m_Caps.bOutToARGB8 = 0;
    //    m_Caps.bOutToARGB8 = 1;
    //}


    if (m_Caps.bNoPadding) 
    {
        if (m_PaddingParams.top < MFX_CAM_MIN_REQUIRED_PADDING_TOP || m_PaddingParams.bottom < MFX_CAM_MIN_REQUIRED_PADDING_BOTTOM ||
            m_PaddingParams.left < MFX_CAM_MIN_REQUIRED_PADDING_LEFT || m_PaddingParams.right < MFX_CAM_MIN_REQUIRED_PADDING_RIGHT)
            m_Caps.bNoPadding = 0; // padding provided by application is not sufficient - have to do it ourselves
    }
    {
        m_PaddingParams.top = MFX_CAM_DEFAULT_PADDING_TOP;
        m_PaddingParams.bottom = MFX_CAM_DEFAULT_PADDING_BOTTOM;
        m_PaddingParams.left = MFX_CAM_DEFAULT_PADDING_LEFT;
        m_PaddingParams.right = MFX_CAM_DEFAULT_PADDING_RIGHT;
    }

    m_FrameSizeExtra.frameWidth64   = ((m_mfxVideoParam.vpp.In.CropW + 31) &~ 0x1F); // 2 bytes each for In, 4 bytes for Out, so 32 is good enough for 64 ???
    m_FrameSizeExtra.paddedFrameWidth  = m_mfxVideoParam.vpp.In.CropW + m_PaddingParams.left + m_PaddingParams.right;
    m_FrameSizeExtra.paddedFrameHeight = m_mfxVideoParam.vpp.In.CropH + m_PaddingParams.top + m_PaddingParams.bottom;

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    m_FrameSizeExtra.vSliceWidth       = (((m_mfxVideoParam.vpp.In.CropW / CAM_PIPE_KERNEL_SPLIT)  + 15) &~ 0xF) + 16;
#else
    m_FrameSizeExtra.vSliceWidth       = m_FrameSizeExtra.paddedFrameWidth;
#endif
    m_InputBitDepth = m_mfxVideoParam.vpp.In.BitDepthLuma;


    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        m_Caps.OutputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.OutputMemoryOperationMode = (m_Caps.Reserved ? MEM_FASTGPUCPY : MEM_GPUSHARED);
        //m_Caps.OutputMemoryOperationMode = MEM_GPUSHARED;
        //m_Caps.OutputMemoryOperationMode = MEM_FASTGPUCPY;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
        m_Caps.InputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.InputMemoryOperationMode = MEM_GPUSHARED;

    //m_WBparams.bActive = false; // no WB
    //m_WBparams.RedCorrection         = 2.156250f;
    //m_WBparams.GreenTopCorrection    = 1.000000f;
    //m_WBparams.GreenBottomCorrection = 1.000000f;
    //m_WBparams.BlueCorrection        = 1.417969f;

    //m_GammaParams.bActive = true;

    m_cmDevice.Reset(CreateCmDevicePtr(m_core));
    m_cmCtx.reset(new CmContext(m_mfxVideoParam, m_cmDevice, &m_Caps));

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    m_cmCtx->CreateThreadSpaces(m_FrameSizeExtra.vSliceWidth);
#else
    m_cmCtx->CreateThreadSpace(m_FrameSizeExtra.paddedFrameWidth, m_FrameSizeExtra.paddedFrameHeight);
#endif
    mfxSts = AllocateInternalSurfaces();

    return mfxSts;
}


mfxStatus MFXCamera_Plugin::Reset(mfxVideoParam *par)
{
    mfxStatus mfxSts;

    mfxVideoParam newParam = *par;
    mfxSts = Query(&newParam, &newParam);
    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    if (newParam.AsyncDepth > 0)
        m_mfxVideoParam.AsyncDepth = newParam.AsyncDepth;


    //mfxCameraCaps oldCaps = m_Caps;
    CameraPipeForwardGammaParams oldGammaParams = m_GammaParams;

    m_Caps.ModuleConfiguration = 0; // zero all filters
    m_Caps.bDemosaic = 1;           // demosaic is always on
    ProcessExtendedBuffers(&newParam);

    m_Caps.bOutToARGB8 = 1;
    // uncomment and modify below when 16-bit output is supported
    //if (m_mfxVideoParam.vpp.Out.FourCC != MFX_FOURCC_RGB4) {
    //    m_Caps.bOutToARGB8 = 0;
    //    m_Caps.bOutToARGB8 = 1;
    //}

    if (m_Caps.bNoPadding) 
    {
        if (m_PaddingParams.top < MFX_CAM_MIN_REQUIRED_PADDING_TOP || m_PaddingParams.bottom < MFX_CAM_MIN_REQUIRED_PADDING_BOTTOM ||
            m_PaddingParams.left < MFX_CAM_MIN_REQUIRED_PADDING_LEFT || m_PaddingParams.right < MFX_CAM_MIN_REQUIRED_PADDING_RIGHT)
            m_Caps.bNoPadding = 0; // padding provided by application is not sufficient - have to do it ourselves
    }
    {
        m_PaddingParams.top = MFX_CAM_DEFAULT_PADDING_TOP;
        m_PaddingParams.bottom = MFX_CAM_DEFAULT_PADDING_BOTTOM;
        m_PaddingParams.left = MFX_CAM_DEFAULT_PADDING_LEFT;
        m_PaddingParams.right = MFX_CAM_DEFAULT_PADDING_RIGHT;
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        m_Caps.OutputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.OutputMemoryOperationMode = (m_Caps.Reserved ? MEM_FASTGPUCPY : MEM_GPUSHARED);
//        m_Caps.OutputMemoryOperationMode = MEM_GPUSHARED;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
        m_Caps.InputMemoryOperationMode = MEM_GPU;
    else
        m_Caps.InputMemoryOperationMode = MEM_GPUSHARED;

    CameraFrameSizeExtra frameSizeExtra;

    frameSizeExtra.frameWidth64   = ((newParam.vpp.In.CropW + 31) &~ 0x1F);
    frameSizeExtra.paddedFrameWidth  = newParam.vpp.In.CropW + m_PaddingParams.left + m_PaddingParams.right;
    frameSizeExtra.paddedFrameHeight = newParam.vpp.In.CropH + m_PaddingParams.top + m_PaddingParams.bottom;
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    frameSizeExtra.vSliceWidth       = (((newParam.vpp.In.CropW / CAM_PIPE_KERNEL_SPLIT)  + 15) &~ 0xF) + 16;
#else
    frameSizeExtra.vSliceWidth       = frameSizeExtra.paddedFrameWidth;
#endif

//    m_InputBitDepth = m_mfxVideoParam.vpp.In.BitDepthLuma;

    m_cmCtx->Reset(m_mfxVideoParam, &m_Caps);


#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    m_cmCtx->CreateThreadSpaces(frameSizeExtra.vSliceWidth);
#else
    m_cmCtx->CreateThreadSpace(frameSizeExtra.paddedFrameWidth, frameSizeExtra.paddedFrameHeight);
#endif


    mfxSts = ReallocateInternalSurfaces(newParam, frameSizeExtra);

    // will need to check numPoints as well when different LUT sizes are supported
    if (m_Caps.bForwardGammaCorrection) {
        if (!m_gammaCorrectSurf)
            m_gammaCorrectSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        if (!m_gammaPointSurf)
            m_gammaPointSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);

        mfxU8 *pGammaPts = (mfxU8 *)m_GammaParams.gamma_lut.gammaPoints;
        mfxU8 *pGammaCor = (mfxU8 *)m_GammaParams.gamma_lut.gammaCorrect;
        if (pGammaPts != (mfxU8 *)oldGammaParams.gamma_lut.gammaPoints || pGammaCor != (mfxU8 *)oldGammaParams.gamma_lut.gammaCorrect) {
            if (!pGammaPts || !pGammaCor) {
                // gamma value is used - implement !!! kta
            }
            m_gammaCorrectSurf->WriteSurface(pGammaCor, NULL);
            m_gammaPointSurf->WriteSurface((unsigned char *)pGammaPts, NULL);
        }
    }
    m_FrameSizeExtra = frameSizeExtra;

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
    pParam;
    mfxStatus sts;
    mfxFrameSurface1 *surfIn = pParam->surf_in;
    mfxFrameSurface1 *surfOut = pParam->surf_out;

    m_core->IncreaseReference(&surfIn->Data);
    m_core->IncreaseReference(&surfOut->Data);

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, tasks);
#endif

    //sts = SetExternalSurfaces(surfIn, surfOut);
    sts = SetExternalSurfaces(pParam);

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif


#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, task1);
#endif

    sts  = CreateEnqueueTasks(pParam);

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

    if (pParam) {

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, taske);
#endif

        CmEvent *e = (CmEvent *)pParam->pEvent;
        m_cmCtx->DestroyEvent(e);

        if (pParam->inSurf2D) {
            ReleaseResource(m_rawIn, pParam->inSurf2D);
        } else if (pParam->inSurf2DUP) {
            if (pParam->pMemIn)
                CM_ALIGNED_FREE(pParam->pMemIn); // remove/change when pool implemented !!!
            pParam->pMemIn = 0;
            CmSurface2DUP *surf = (CmSurface2DUP *)pParam->inSurf2DUP;
            m_cmDevice->DestroySurface2DUP(surf);
            pParam->inSurf2DUP = 0;
        }

        m_core->DecreaseReference(&pParam->surf_in->Data);


#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif
        
        m_core->DecreaseReference(&pParam->surf_out->Data);

    }

    return sts;
}

//mfxStatus MFXCamera_Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
//{
//}

mfxStatus MFXCamera_Plugin::VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData* /*aux*/, mfxThreadTask *mfxthreadtask)
{
    mfxStatus mfxRes;

    if (!surface_in)
        return MFX_ERR_NULL_PTR;
    if (!surface_out)
        return MFX_ERR_NULL_PTR;

    mfxSyncPoint syncp;
    MFX_TASK task;
    MFX_ENTRY_POINT entryPoint;
    memset(&entryPoint, 0, sizeof(entryPoint));
    memset(&task, 0, sizeof(task));

    AsyncParams *pParams = new AsyncParams;
    pParams->surf_in = surface_in;
    pParams->surf_out = surface_out;

    entryPoint.pRoutine = CameraRoutine;
    entryPoint.pRoutineName = "CameraPipeline";
    entryPoint.pCompleteProc = &CompleteCameraRoutine;
    entryPoint.pState = this; //???
    entryPoint.requiredNumThreads = 1;
    entryPoint.pParam = pParams;

    task.pOwner = m_session->m_plgVPP.get();
    task.entryPoint = entryPoint;
    task.priority = m_session->m_priority;
    task.threadingPolicy = (m_PluginParam.ThreadPolicy == MFX_THREADPOLICY_SERIAL ? MFX_TASK_THREADING_INTRA : MFX_TASK_THREADING_INTER); // ???
    //task.threadingPolicy = m_session->m_plgVPP->GetThreadingPolicy();
    // fill dependencies
    task.pSrc[0] = surface_in;
    task.pDst[0] = surface_out;

//#ifdef MFX_TRACE_ENABLE
//    task.nParentId = MFX_AUTO_TRACE_GETID();
//    task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;
//#endif
    // register input and call the task
    mfxRes = m_session->m_pScheduler->AddTask(task, &syncp);

    *mfxthreadtask = (mfxThreadTask)syncp;

    return mfxRes;
}