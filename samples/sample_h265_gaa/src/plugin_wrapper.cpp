/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* public files */
#include "sample_fei_h265.h"
#include "plugin_wrapper.h"

CH265FEI::CH265FEI(void) :
    m_mfxParams(),
    m_mfxExtParams(),
    m_mfxExtFEIInput(),
    m_mfxExtFEIOutput(),
    m_pmfxSession(),
    m_pmfxFEI(),
    m_pUserEncModule(),
    m_pUserEncPlugin(),
    m_mfxExtParamsBuf(),
    m_mfxExtFEIInputBuf(),
    m_mfxExtFEIOutputBuf()
{
}

CH265FEI::~CH265FEI(void)
{
}

mfxStatus CH265FEI::Init(SampleParams *sp)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxIMPL libType = MFX_IMPL_HARDWARE_ANY;

    // init session
    m_pmfxSession.reset(new MFXVideoSession);

    mfxVersion ver = {10, 1};

    if (libType & MFX_IMPL_HARDWARE_ANY)
    {
        // try search for MSDK on all display adapters
        sts = m_pmfxSession->Init(libType, &ver);

        // MSDK API version may have no support for multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
            sts = m_pmfxSession->Init((libType & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, &ver);
    }
    else
        sts = m_pmfxSession->Init(libType, &ver);

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // check the API version of actually loaded library
    mfxVersion version;
    sts = m_pmfxSession->QueryVersion(&version);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxIMPL impl = 0;
    m_pmfxSession->QueryIMPL(&impl);

    /* external handle required for Linux, optional for Windows */
    mfxHandleType eDevType;
    mfxHDL hdl = NULL;

#if defined(_WIN32) || defined(_WIN64)
    eDevType = MFX_HANDLE_D3D9_DEVICE_MANAGER;
#elif defined(LIBVA_SUPPORT)
    eDevType = MFX_HANDLE_VA_DISPLAY;
#endif

#if defined(_WIN32) || defined(_WIN64)
    if (eDevType == MFX_HANDLE_D3D9_DEVICE_MANAGER)
    {
        m_hwdev.reset(new CD3D9Device());
        sts = m_hwdev->Init(NULL, 1, MSDKAdapter::GetNumber() );
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        sts = m_hwdev->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
#elif defined(LIBVA_X11_SUPPORT) || defined(LIBVA_DRM_SUPPORT)
    if (eDevType == MFX_HANDLE_VA_DISPLAY)
    {
        m_hwdev.reset(CreateVAAPIDevice());
        sts = m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber() );
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL*)&hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
#endif

    mfxHandleType handleType = (mfxHandleType)0;
    bool bIsMustSetExternalHandle = 0;

    if (MFX_IMPL_VIA_D3D9 == MFX_IMPL_VIA_MASK(impl))
    {
        handleType = MFX_HANDLE_D3D9_DEVICE_MANAGER;
        bIsMustSetExternalHandle = false;
    }
#ifdef LIBVA_SUPPORT
    else if (MFX_IMPL_VIA_VAAPI == MFX_IMPL_VIA_MASK(impl))
    {
        handleType = MFX_HANDLE_VA_DISPLAY;
        bIsMustSetExternalHandle = true;
    }
#endif

    if (hdl && bIsMustSetExternalHandle)
    {
        sts = m_pmfxSession->SetHandle(handleType, hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    /* InitFEI() */
    mfxPluginUID uid = {0};

    if (CheckVersion(&version, MSDK_FEATURE_PLUGIN_API))
    {
        if (sp->PluginPath[0])
        {
            /* load from path specified on command line */
            uid = msdkGetPluginUID(MFX_IMPL_HARDWARE, MSDK_VENC, MFX_CODEC_HEVC);
            sts = MFXVideoUSER_LoadByPath(*m_pmfxSession.get(), &uid, 1, sp->PluginPath, -1);
        }
        else
        {
            /* load using GUID */
            if (AreGuidsEqual(uid, MSDK_PLUGINGUID_NULL))
            {
                uid = msdkGetPluginUID(MFX_IMPL_HARDWARE, MSDK_VENC, MFX_CODEC_HEVC);
            }
            if (!AreGuidsEqual(uid, MSDK_PLUGINGUID_NULL))
            {
                m_pUserEncPlugin.reset(LoadPlugin(MFX_PLUGINTYPE_VIDEO_ENCODE, *m_pmfxSession.get(), uid, 1));
                if (m_pUserEncPlugin.get() == NULL) sts = MFX_ERR_UNSUPPORTED;
            }
            if(sts==MFX_ERR_UNSUPPORTED)
            {
                msdk_printf(MSDK_STRING("Default plugin cannot be loaded (possibly you have to define plugin explicitly)\n"));
            }
        }
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }


    // create FEI
    m_pmfxFEI.reset(new MFXVideoENC(*m_pmfxSession.get()));

    mfxVideoParam & param = m_mfxParams;

    MSDK_ZERO_MEMORY(param.mfx);
    param.mfx.CodecId= MFX_CODEC_HEVC;
    param.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    MSDK_ZERO_MEMORY(m_mfxExtParams);
    m_mfxExtParams.Header.BufferId = MFX_EXTBUFF_FEI_H265_PARAM;
    m_mfxExtParams.Header.BufferSz = sizeof(m_mfxExtParams);

    m_mfxParams.mfx.FrameInfo.Width  = sp->PaddedWidth;
    m_mfxParams.mfx.FrameInfo.Height = sp->PaddedHeight;
    m_mfxParams.mfx.NumRefFrame      = sp->NumRefFrames;

    m_mfxExtParams.MaxCUSize     = sp->MaxCUSize;
    m_mfxExtParams.MPMode        = sp->MPMode;
    m_mfxExtParams.NumIntraModes = sp->NumIntraModes;

    /* push ext buffer into package */
    m_mfxExtParamsBuf.push_back((mfxExtBuffer *)&m_mfxExtParams);
    if (!m_mfxExtParamsBuf.empty())
    {
        m_mfxParams.ExtParam = &m_mfxExtParamsBuf[0];
        m_mfxParams.NumExtParam = (mfxU16)m_mfxExtParamsBuf.size();
    }

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pmfxFEI->Init(&m_mfxParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        //msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CH265FEI::Close(void)
{
    /* other resources freed automatically */
    if (m_pmfxFEI.get())
        m_pmfxFEI->Close();

    m_mfxExtParamsBuf.clear();
    m_mfxExtFEIInputBuf.clear();
    m_mfxExtFEIOutputBuf.clear();

    return MFX_ERR_NONE;
}

mfxStatus CH265FEI::ProcessFrameAsync(ProcessInfo *pi, mfxFEIH265Output *out, mfxSyncPoint *syncp)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1 InSurface;
    mfxFrameSurface1 RefSurface[2];
    mfxFrameSurface1 *RefSurfaceList[2];

    mfxENCInput encIn = {0};
    mfxENCOutput encOut = {0};

    InSurface.Data.Y     = pi->YPlaneSrc;
    InSurface.Data.Pitch = pi->YPitchSrc;
    InSurface.Data.FrameOrder = pi->EncOrderSrc;
    encIn.InSurface = &InSurface;

    RefSurface[0].Data.Y     = pi->YPlaneRef;
    RefSurface[0].Data.Pitch = pi->YPitchRef;
    RefSurface[0].Data.FrameOrder = pi->EncOrderRef;
    encIn.NumFrameL0 = (pi->YPlaneRef ? 1 : 0);
    if (encIn.NumFrameL0) {
        RefSurfaceList[0] = &(RefSurface[0]);
        encIn.L0Surface = &(RefSurfaceList[0]);
    }

    RefSurface[1].Data.Y     = 0;
    encIn.NumFrameL1 = 0;

    /* other parameters go here */
    m_mfxExtFEIInput.Header.BufferId = MFX_EXTBUFF_FEI_H265_INPUT;
    m_mfxExtFEIInput.Header.BufferSz = sizeof(m_mfxExtFEIInput);

    m_mfxExtFEIInput.FEIOp     = pi->FEIOp;
    m_mfxExtFEIInput.FrameType = pi->FrameType;
    m_mfxExtFEIInput.RefIdx    = pi->RefIdx;

    m_mfxExtFEIInputBuf.push_back((mfxExtBuffer *)&m_mfxExtFEIInput);
    if (!m_mfxExtFEIInputBuf.empty())
    {
        encIn.ExtParam = &m_mfxExtFEIInputBuf[0];
        encIn.NumExtParam = (mfxU16)m_mfxExtFEIInputBuf.size();
    }

    /* setup output buffers */
    m_mfxExtFEIOutput.Header.BufferId = MFX_EXTBUFF_FEI_H265_OUTPUT;
    m_mfxExtFEIOutput.Header.BufferSz = sizeof(m_mfxExtFEIOutput);

    m_mfxExtFEIOutput.feiOut = out; // pass pointer

    m_mfxExtFEIOutputBuf.push_back((mfxExtBuffer *)&m_mfxExtFEIOutput);
    if (!m_mfxExtFEIOutputBuf.empty())
    {
        encOut.ExtParam = &m_mfxExtFEIOutputBuf[0];
        encOut.NumExtParam = (mfxU16)m_mfxExtFEIOutputBuf.size();
    }

    sts = m_pmfxFEI->ProcessFrameAsync(&encIn, &encOut, syncp);

    return sts;
}

mfxStatus CH265FEI::SyncOperation(mfxSyncPoint syncp)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pmfxSession->SyncOperation(syncp, 0xFFFFFFFF);

    return sts;
}
