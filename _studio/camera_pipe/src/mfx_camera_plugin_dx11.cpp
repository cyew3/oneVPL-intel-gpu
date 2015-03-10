/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_dx11.cpp

\* ****************************************************************************** */
#if defined (_WIN32) || defined (_WIN64)
#include "mfx_camera_plugin_dx11.h"


mfxStatus D3D11CameraProcessor::Init(CameraParams *CameraParams)
{
    MFX_CHECK_NULL_PTR1(CameraParams);
    mfxStatus sts = MFX_ERR_NONE;
    if ( ! m_core )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if ( MFX_HW_D3D11 != m_core->GetVAType() )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_CameraParams = *CameraParams;
    m_ddi.reset( new D3D11VideoProcessor() );
    if (0 == m_ddi.get())
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    m_params = CameraParams->par;
    sts = m_ddi->CreateDevice( m_core, &m_params, false);
    MFX_CHECK_STS( sts );

    mfxVppCaps caps = {0};
    sts = m_ddi->QueryCapabilities( caps );
    MFX_CHECK_STS( sts );

    m_AsyncDepth = CameraParams->par.AsyncDepth;

    // Camera Pipe expects a system memory as input. Need to allocate
    m_executeSurf.resize( m_AsyncDepth );
    mfxFrameAllocRequest request;
    request.Info        = m_params.vpp.In;

    // Initial frameinfo contains just R16 that should be updated to the
    // internal FourCC representing
    request.Info.FourCC = BayerToFourCC(CameraParams->Caps.BayerPatternType);
    request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_INTERNAL_FRAME;
    // Alocate a single surface at the moment. Need to allocate more surfaces depending on async
    request.NumFrameMin = request.NumFrameSuggested = m_AsyncDepth; // Fixme

    m_InSurfacePool->AllocateSurfaces(m_core, request);

    m_systemMemOut = false;
    if ( CameraParams->par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
    {
        m_systemMemOut = true;
        // Output is in system memory. Need to allocate temporary video surf
        mfxFrameAllocRequest request;
        request.Info        = m_params.vpp.Out;
        request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_INTERNAL_FRAME;
        request.NumFrameMin = request.NumFrameSuggested = m_AsyncDepth; // Fixme
        m_OutSurfacePool->AllocateSurfaces(m_core, request);;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11CameraProcessor::AsyncRoutine(AsyncParams *pParam)
{
    mfxStatus sts;
    UMC::AutomaticUMCMutex guard(m_guard);
    m_core->IncreaseReference(&(pParam->surf_out->Data));
    m_core->IncreaseReference(&(pParam->surf_in->Data));

    ZeroMemory(&m_executeParams, sizeof(m_executeParams));

    // [1] Make required actions for the input surface
    mfxU32 surfInIndex;
    sts = PreWorkInSurface(pParam->surf_in, &surfInIndex);
    MFX_CHECK_STS(sts);
    m_executeParams.pRefSurfaces  = &m_executeSurf[surfInIndex];

    // [2] Make required actions for the output surface
    mfxU32 surfOutIndex;
    sts = PreWorkOutSurface(pParam->surf_out, &surfOutIndex);
    MFX_CHECK_STS(sts);

    // Turn on camera
    m_executeParams.bCameraPipeEnabled      = true;

    // [3] Fill in params
    if ( pParam->Caps.bBlackLevelCorrection )
    {
        m_executeParams.bCameraBlackLevelCorrection = true;
        m_executeParams.CameraBlackLevel.uB  = (mfxU32)pParam->BlackLevelParams.BlueLevel;
        m_executeParams.CameraBlackLevel.uG0 = (mfxU32)pParam->BlackLevelParams.GreenBottomLevel;
        m_executeParams.CameraBlackLevel.uG1 = (mfxU32)pParam->BlackLevelParams.GreenTopLevel;
        m_executeParams.CameraBlackLevel.uR  = (mfxU32)pParam->BlackLevelParams.RedLevel;
    }

    if ( pParam->Caps.bWhiteBalance )
    {
        m_executeParams.bCameraWhiteBalaceCorrection = true;
        m_executeParams.CameraWhiteBalance.fB  = (mfxF32)pParam->WBparams.BlueCorrection;
        m_executeParams.CameraWhiteBalance.fG0 = (mfxF32)pParam->WBparams.GreenBottomCorrection;
        m_executeParams.CameraWhiteBalance.fG1 = (mfxF32)pParam->WBparams.GreenTopCorrection;
        m_executeParams.CameraWhiteBalance.fR  = (mfxF32)pParam->WBparams.RedCorrection;
    }

    if ( pParam->Caps.bHotPixel )
    {
        m_executeParams.bCameraHotPixelRemoval = true;
        m_executeParams.CameraHotPixel.uPixelCountThreshold = pParam->HPParams.PixelCountThreshold;
        m_executeParams.CameraHotPixel.uPixelThresholdDifference = pParam->HPParams.PixelThresholdDifference;
    }

    if ( pParam->Caps.bColorConversionMatrix )
    {
        m_executeParams.bCCM = true;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                m_executeParams.CCMParams.CCM[i][j] = (mfxF32)pParam->CCMParams.CCM[i][j];
            }
        }
    }

    if ( pParam->Caps.bForwardGammaCorrection )
    {
        m_executeParams.bCameraGammaCorrection = true;
        for(int i = 0; i < 64; i++)
        {
            m_executeParams.CameraForwardGammaCorrection.Segment[i].PixelValue = pParam->GammaParams.gamma_lut.gammaPoints[i];
            m_executeParams.CameraForwardGammaCorrection.Segment[i].BlueChannelCorrectedValue  = pParam->GammaParams.gamma_lut.gammaCorrect[i];
            m_executeParams.CameraForwardGammaCorrection.Segment[i].GreenChannelCorrectedValue = pParam->GammaParams.gamma_lut.gammaCorrect[i];
            m_executeParams.CameraForwardGammaCorrection.Segment[i].RedChannelCorrectedValue   = pParam->GammaParams.gamma_lut.gammaCorrect[i];
        }
    }

    if ( pParam->Caps.bVignetteCorrection )
    {
        MFX_CHECK_NULL_PTR1(pParam->VignetteParams.pCorrectionMap);
        m_executeParams.bCameraVignetteCorrection = 1;
        m_executeParams.CameraVignetteCorrection.Height = pParam->VignetteParams.Height;
        m_executeParams.CameraVignetteCorrection.Width  = pParam->VignetteParams.Width;
        m_executeParams.CameraVignetteCorrection.Stride = pParam->VignetteParams.Stride;
        m_executeParams.CameraVignetteCorrection.pCorrectionMap = (CameraVignetteCorrectionElem *)pParam->VignetteParams.pCorrectionMap;
    }


    m_executeParams.refCount       = 1;
    m_executeParams.bkwdRefCount   = m_executeParams.fwdRefCount = 0;
    m_executeParams.statusReportID = surfInIndex;

    // [4] Start execution
    sts = m_ddi->Execute(&m_executeParams);

    if ( m_systemMemOut )
    {
        mfxFrameSurface1 OutSurf = {0};
        OutSurf.Data.MemId = m_OutSurfacePool->mids[surfOutIndex];
        OutSurf.Info       = pParam->surf_out->Info;

        // [1] Copy from system mem to the internal video frame
        sts = m_core->DoFastCopyWrapper(pParam->surf_out,
                                        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                        &OutSurf,
                                        MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                                        );
        MFX_CHECK_STS(sts);
    }

    m_InSurfacePool->Unlock(surfInIndex);
    m_OutSurfacePool->Unlock(surfOutIndex);
    m_core->DecreaseReference(&(pParam->surf_out->Data));
    m_core->DecreaseReference(&(pParam->surf_in->Data));

    return sts;
}

mfxStatus D3D11CameraProcessor::CompleteRoutine(AsyncParams * pParam)
{
    mfxStatus sts = MFX_ERR_NONE;
    UNREFERENCED_PARAMETER(pParam);
    return sts;
}

mfxStatus D3D11CameraProcessor::CheckIOPattern(mfxU16  IOPattern)
{
    if (IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY ||
        IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    if (IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY ||
        IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3D11CameraProcessor::PreWorkOutSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex)
{
    mfxHDLPair hdl;
    mfxStatus sts;
    mfxMemId  memIdOut;
    bool      bExternal = true;

    if ( m_systemMemOut )
    {
        // Request surface from internal pool
        memIdOut = AcquireResource(*m_OutSurfacePool, poolIndex);
        // [2] Get surface handle
        sts = m_core->GetFrameHDL(memIdOut, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
        bExternal = false;
    }
    else
    {
        memIdOut = surf->Data.MemId;
        // [1] Get external surface handle
        sts = m_core->GetExternalFrameHDL(memIdOut, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
        bExternal = true;
    }

    // [2] Get surface handle
    sts = m_ddi->Register(&hdl, 1, TRUE);
    MFX_CHECK_STS(sts);

    // [3] Fill in Drv surfaces
    m_executeParams.targetSurface.memId     = memIdOut;//surf->Data.MemId;
    m_executeParams.targetSurface.bExternal = bExternal;
    m_executeParams.targetSurface.hdl       = static_cast<mfxHDLPair>(hdl);
    m_executeParams.targetSurface.frameInfo = surf->Info;

    return sts;
}

mfxStatus D3D11CameraProcessor::PreWorkInSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex)
{
    mfxHDLPair hdl;
    mfxStatus sts;

    // Request surface from internal pool
    mfxMemId memIdIn = AcquireResource(*m_InSurfacePool, poolIndex);
    mfxFrameSurface1 InSurf = {0};

    InSurf.Data.MemId = memIdIn;
    InSurf.Info       = surf->Info;
    InSurf.Info.FourCC =  BayerToFourCC(m_CameraParams.Caps.BayerPatternType);

    // [1] Copy from system mem to the internal video frame
    sts = m_core->DoFastCopyWrapper(&InSurf,
                                    MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET,
                                    surf,
                                    MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY);
    MFX_CHECK_STS(sts);

    // [2] Get surface handle
    sts = m_core->GetFrameHDL(memIdIn, (mfxHDL *)&hdl);
    MFX_CHECK_STS(sts);

    // [3] Register surfaece. Not needed really for DX11
    sts = m_ddi->Register(&hdl, 1, TRUE);
    MFX_CHECK_STS(sts);

    // [4] Fill in Drv surfaces
    m_executeSurf[*poolIndex].hdl       = static_cast<mfxHDLPair>(hdl);
    m_executeSurf[*poolIndex].bExternal = false;
    m_executeSurf[*poolIndex].frameInfo = InSurf.Info;
    m_executeSurf[*poolIndex].memId     = memIdIn;

    return sts;
}

mfxU32 D3D11CameraProcessor::BayerToFourCC(mfxU32 Bayer)
{
    switch(Bayer)
    {
    case BGGR:
        return MFX_FOURCC_R16_BGGR;
    case RGGB:
        return MFX_FOURCC_R16_RGGB;
    case GRBG:
        return MFX_FOURCC_R16_GRBG;
    case GBRG:
        return MFX_FOURCC_R16_GBRG;
    }
    return 0;
}
#endif