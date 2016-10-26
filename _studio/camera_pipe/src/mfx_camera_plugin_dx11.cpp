//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#if defined (_WIN32) || defined (_WIN64)
#include "mfx_camera_plugin_dx11.h"
//#define CAMP_PIPE_ITT
#ifdef CAMP_PIPE_ITT
#include "ittnotify.h"
__itt_domain* CamPipeDX11     = __itt_domain_create(L"CamPipeDX11");
__itt_string_handle* GPUCOPY  = __itt_string_handle_create(L"GPUCOPY");
__itt_string_handle* DDIEXEC  = __itt_string_handle_create(L"DDIEXEC");
__itt_string_handle* STALL    = __itt_string_handle_create(L"STALL");
__itt_string_handle* ASYNC    = __itt_string_handle_create(L"ASYNC");
__itt_string_handle* COMPLETE = __itt_string_handle_create(L"COMPLETE");
__itt_string_handle* QUERY = __itt_string_handle_create(L"QueryStatus");
#endif

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

    mfxVppCaps caps;
    sts = m_ddi->QueryCapabilities( caps );
    MFX_CHECK_STS( sts );

    m_AsyncDepth = ( CameraParams->par.AsyncDepth ? CameraParams->par.AsyncDepth : MFX_CAMERA_DEFAULT_ASYNCDEPTH );

    if ( m_executeParams )
        delete [] m_executeParams; 
    if ( m_executeSurf )
        delete [] m_executeSurf;

    m_executeParams = new MfxHwVideoProcessing::mfxExecuteParams[m_AsyncDepth];
    ZeroMemory(m_executeParams, sizeof(MfxHwVideoProcessing::mfxExecuteParams)*m_AsyncDepth);
    m_executeSurf   = new MfxHwVideoProcessing::mfxDrvSurface[m_AsyncDepth];
    ZeroMemory(m_executeParams, sizeof(MfxHwVideoProcessing::mfxDrvSurface)*m_AsyncDepth);

    if ( CameraParams->Caps.bNoPadding )
    {
        // Padding was done by app. DDI doesn't support such mode, so need
        // just ignore padded borders.
        m_paddedInput = true;
    }

    if (m_params.vpp.In.CropW != m_params.vpp.Out.CropW || 
        m_params.vpp.In.CropH != m_params.vpp.Out.CropH)
    {
        // Resize is not supported
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    m_width  = m_params.vpp.In.CropW;
    m_height = (m_params.vpp.In.CropH/2)*2;  // WA for driver bug: crash in case of odd height

    mfxFrameAllocRequest request;
    request.Info        = m_params.vpp.In;
    request.Info.CropX  = request.Info.CropY = 0;
    request.Info.Width  = m_width;
    request.Info.Height = m_height;

    // Initial frameinfo contains just R16 that should be updated to the
    // internal FourCC representing
    request.Info.FourCC = m_params.vpp.In.FourCC == MFX_FOURCC_R16 ? BayerToFourCC(CameraParams->Caps.BayerPatternType) : m_params.vpp.In.FourCC;
    request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_INTERNAL_FRAME;
    request.NumFrameMin = request.NumFrameSuggested = m_AsyncDepth;

    sts = m_InSurfacePool->AllocateSurfaces(m_core, request);
    MFX_CHECK_STS( sts );

    m_systemMemIn = false;
    if ( CameraParams->par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY )
    {
        m_systemMemIn = true;
    }

    m_systemMemOut = false;
    if ( CameraParams->par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
    {
        m_systemMemOut = true;
        // Output is in system memory. Need to allocate temporary video surf
        mfxFrameAllocRequest request;
        request.Info        = m_params.vpp.Out;
        request.Info.CropX  = request.Info.CropY = 0;
        request.Info.Width  = m_width;
        request.Info.Height = m_height;
        request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_INTERNAL_FRAME;
        request.NumFrameMin = request.NumFrameSuggested = m_AsyncDepth; // Fixme
        sts = m_OutSurfacePool->AllocateSurfaces(m_core, request);
        MFX_CHECK_STS( sts );
    }
    m_counter = 0;
    return sts;
}

mfxStatus D3D11CameraProcessor::Reset(mfxVideoParam *par, CameraParams * FrameParams)
{
    MFX_CHECK_NULL_PTR1(FrameParams);
    UNREFERENCED_PARAMETER(par);

    mfxStatus sts = MFX_ERR_NONE;
    if ( ! m_core )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if ( MFX_HW_D3D11 != m_core->GetVAType() )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_CameraParams = *FrameParams;
    m_params       = FrameParams->par;

    if ( FrameParams->Caps.bNoPadding )
    {
        // Padding was done by app. DDI doesn't support such mode, so need
        // just ignore padded borders.
        m_paddedInput = true;
    }

    if (m_params.vpp.In.CropW != m_params.vpp.Out.CropW ||
        m_params.vpp.In.CropH != m_params.vpp.Out.CropH)
    {
        // Resize is not supported
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxU16 width  = m_params.vpp.In.CropW;
    mfxU16 height = (m_params.vpp.In.CropH/2)*2;  // WA for driver bug: crash in case of odd height

    if ( width > m_width || height > m_height )
    {
        // Resolution up is not supported
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    m_width  = width;
    m_height = height;

    return sts;
}

mfxStatus D3D11CameraProcessor::AsyncRoutine(AsyncParams *pParam)
{
    mfxStatus sts;
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, ASYNC);
#endif    
    m_core->IncreaseReference(&(pParam->surf_out->Data));
    m_core->IncreaseReference(&(pParam->surf_in->Data));

    MfxHwVideoProcessing::mfxDrvSurface    tmpSurf   = {0};
    mfxU32 surfInIndex;
    sts = PreWorkInSurface(pParam->surf_in, &surfInIndex, &tmpSurf);
    MFX_CHECK_STS(sts);

    mfxU32 surfOutIndex = 0;
    sts = PreWorkOutSurface(pParam->surf_out, &surfOutIndex, &m_executeParams[surfInIndex]);
    MFX_CHECK_STS(sts);

    m_executeParams[surfInIndex].bCameraPipeEnabled  = true;

    mfxU8 shift = (mfxU8)(16 - pParam->FrameSizeExtra.BitDepth);
    shift = shift > 16 ? 0 : shift;

    if ( pParam->Caps.bBlackLevelCorrection )
    {
        m_executeParams[surfInIndex].bCameraBlackLevelCorrection = true;
        m_executeParams[surfInIndex].CameraBlackLevel.uB  = (mfxU32)pParam->BlackLevelParams.BlueLevel        << shift;
        m_executeParams[surfInIndex].CameraBlackLevel.uG0 = (mfxU32)pParam->BlackLevelParams.GreenBottomLevel << shift;
        m_executeParams[surfInIndex].CameraBlackLevel.uG1 = (mfxU32)pParam->BlackLevelParams.GreenTopLevel    << shift;
        m_executeParams[surfInIndex].CameraBlackLevel.uR  = (mfxU32)pParam->BlackLevelParams.RedLevel         << shift;
    }

    if ( pParam->Caps.bWhiteBalance )
    {
        m_executeParams[surfInIndex].bCameraWhiteBalaceCorrection = true;
        m_executeParams[surfInIndex].CameraWhiteBalance.fB  = (mfxF32)pParam->WBparams.BlueCorrection;
        m_executeParams[surfInIndex].CameraWhiteBalance.fG0 = (mfxF32)pParam->WBparams.GreenBottomCorrection;
        m_executeParams[surfInIndex].CameraWhiteBalance.fG1 = (mfxF32)pParam->WBparams.GreenTopCorrection;
        m_executeParams[surfInIndex].CameraWhiteBalance.fR  = (mfxF32)pParam->WBparams.RedCorrection;
    }

    if ( pParam->Caps.bHotPixel )
    {
        m_executeParams[surfInIndex].bCameraHotPixelRemoval = true;
        m_executeParams[surfInIndex].CameraHotPixel.uPixelCountThreshold = pParam->HPParams.PixelCountThreshold;
        m_executeParams[surfInIndex].CameraHotPixel.uPixelThresholdDifference = pParam->HPParams.PixelThresholdDifference;
    }

    if ( pParam->Caps.b3DLUT)
    {
        m_executeParams[surfInIndex].bCamera3DLUT = true;
        m_executeParams[surfInIndex].Camera3DLUT.LUTSize = pParam->LUTParams.size;
        m_executeParams[surfInIndex].Camera3DLUT.lut     = (LUT_ENTRY *)pParam->LUTParams.lut;
    }

    if ( pParam->Caps.bColorConversionMatrix )
    {
        m_executeParams[surfInIndex].bCCM = true;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                m_executeParams[surfInIndex].CCMParams.CCM[i][j] = (mfxF32)pParam->CCMParams.CCM[i][j];
            }
        }
    }

    if ( pParam->Caps.bLensCorrection )
    {
        m_executeParams[surfInIndex].bCameraLensCorrection = true;
        for(int i = 0; i < 3; i++)
        {
            m_executeParams[surfInIndex].CameraLensCorrection.a[i] = pParam->LensParams.a[i];
            m_executeParams[surfInIndex].CameraLensCorrection.b[i] = pParam->LensParams.b[i];
            m_executeParams[surfInIndex].CameraLensCorrection.c[i] = pParam->LensParams.c[i];
            m_executeParams[surfInIndex].CameraLensCorrection.d[i] = pParam->LensParams.d[i];
        }
    }

    if ( pParam->Caps.bForwardGammaCorrection )
    {
        m_executeParams[surfInIndex].bCameraGammaCorrection = true;
       /* FCG params for first point must be 0 and for the last point must be 65535 
        * If they are not, driver returns INVALIDARGS error
        */
        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[0].PixelValue                 = 0;
        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[0].BlueChannelCorrectedValue  = 0;
        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[0].GreenChannelCorrectedValue = 0;
        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[0].RedChannelCorrectedValue   = 0;

        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[63].PixelValue                 = 65535;
        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[63].BlueChannelCorrectedValue  = 65535;
        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[63].GreenChannelCorrectedValue = 65535;
        m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[63].RedChannelCorrectedValue   = 65535;

        for(int i = 1; i < 63; i++)
        {
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].PixelValue = pParam->GammaParams.Segment[i].Pixel                 << shift;
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].BlueChannelCorrectedValue  = pParam->GammaParams.Segment[i].Blue  << shift;
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].GreenChannelCorrectedValue = pParam->GammaParams.Segment[i].Green << shift;
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].RedChannelCorrectedValue   = pParam->GammaParams.Segment[i].Red   << shift;
        }
    }

    if ( pParam->Caps.bVignetteCorrection )
    {
        MFX_CHECK_NULL_PTR1(pParam->VignetteParams.pCorrectionMap);
        m_executeParams[surfInIndex].bCameraVignetteCorrection = 1;
        m_executeParams[surfInIndex].CameraVignetteCorrection.Height = pParam->VignetteParams.Height;
        m_executeParams[surfInIndex].CameraVignetteCorrection.Width  = pParam->VignetteParams.Width;
        m_executeParams[surfInIndex].CameraVignetteCorrection.Stride = pParam->VignetteParams.Stride;
        m_executeParams[surfInIndex].CameraVignetteCorrection.pCorrectionMap = (CameraVignetteCorrectionElem *)pParam->VignetteParams.pCorrectionMap;
    }

    m_executeParams[surfInIndex].refCount       = 1;
    m_executeParams[surfInIndex].bkwdRefCount   = m_executeParams[surfInIndex].fwdRefCount = 0;
    m_executeParams[surfInIndex].statusReportID = m_counter++;
    m_executeSurf[surfInIndex].bExternal = tmpSurf.bExternal;
    m_executeSurf[surfInIndex].endTimeStamp = tmpSurf.endTimeStamp;
    memcpy_s(&m_executeSurf[surfInIndex].frameInfo, sizeof(mfxFrameInfo), &tmpSurf.frameInfo, sizeof(mfxFrameInfo));
    m_executeSurf[surfInIndex].hdl = tmpSurf.hdl;
    m_executeSurf[surfInIndex].memId = tmpSurf.memId;
    m_executeSurf[surfInIndex].startTimeStamp = tmpSurf.startTimeStamp;
    m_executeParams[surfInIndex].pRefSurfaces = &m_executeSurf[surfInIndex];

    pParam->surfInIndex  = surfInIndex;
    pParam->surfOutIndex = surfOutIndex;
    {
        #ifdef CAMP_PIPE_ITT
            __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, DDIEXEC);
        #endif
        // DDI execs
        sts = m_ddi->Execute(&m_executeParams[surfInIndex]);
        #ifdef CAMP_PIPE_ITT
            __itt_task_end(CamPipeDX11);
        #endif
    }

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
    return sts;
}

mfxStatus D3D11CameraProcessor::CompleteRoutine(AsyncParams * pParam)
{
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, COMPLETE);
#endif
    MFX_CHECK_NULL_PTR1(pParam);
    mfxStatus sts   = MFX_TASK_DONE;
    
    mfxU32 ddiIndex = pParam->surfInIndex;
    
    mfxU16 queryAttempt = 5;
    while ( queryAttempt )
    {
        #ifdef CAMP_PIPE_ITT
        __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, QUERY);
        #endif
        sts = m_ddi->QueryTaskStatus(m_executeParams[ddiIndex].statusReportID);
        #ifdef CAMP_PIPE_ITT
            __itt_task_end(CamPipeDX11);
        #endif

        if ( MFX_TASK_DONE == sts )
        {
            break;
        }
        #ifdef CAMP_PIPE_ITT
            __itt_task_end(CamPipeDX11);
        #endif
        queryAttempt--;
        vm_time_sleep(10);
    }

    if ( m_systemMemOut )
    {
        mfxFrameSurface1 OutSurf = {0};
        OutSurf.Data.MemId = m_OutSurfacePool->mids[pParam->surfOutIndex];
        OutSurf.Info       = pParam->surf_out->Info;
        if ( MFX_FOURCC_ARGB16 == OutSurf.Info.FourCC && ! pParam->Caps.b3DLUT)
        {
            // For ARGB16 out need to do R<->B swapping.
            // 3D LUT does swapping as well.
            OutSurf.Info.FourCC = MFX_FOURCC_ABGR16;
        }
        else if ( MFX_FOURCC_RGB4 == OutSurf.Info.FourCC && pParam->Caps.b3DLUT)
        {
            // 3D LUT does R<->B swapping. Need to get R and B back.
            OutSurf.Info.FourCC = MFX_FOURCC_BGR4;
        }

        OutSurf.Info.Width  = m_width;
        OutSurf.Info.Height = m_height;
        #ifdef CAMP_PIPE_ITT
            __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, GPUCOPY);
        #endif
        // [1] Copy from system mem to the internal video frame
        sts = m_core->DoFastCopyWrapper(pParam->surf_out,
                                        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                        &OutSurf,
                                        MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                                        );
        #ifdef CAMP_PIPE_ITT
            __itt_task_end(CamPipeDX11);
        #endif
        MFX_CHECK_STS(sts);
    }

    {
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, STALL);
#endif
        UMC::AutomaticUMCMutex guard(m_guard_exec);
        m_InSurfacePool->Unlock(pParam->surfInIndex );
        m_OutSurfacePool->Unlock(pParam->surfOutIndex);
        m_core->DecreaseReference(&(pParam->surf_out->Data));
        m_core->DecreaseReference(&(pParam->surf_in->Data));
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
    }
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
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

mfxStatus D3D11CameraProcessor::PreWorkOutSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, MfxHwVideoProcessing::mfxExecuteParams *params)
{
    mfxHDLPair hdl;
    mfxStatus sts;
    mfxMemId  memIdOut;
    bool      bExternal = true;

    if ( m_systemMemOut )
    {
        // Request surface from internal pool
        memIdOut = AcquireResource(*m_OutSurfacePool, poolIndex);
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
    params->targetSurface.memId     = memIdOut;
    params->targetSurface.bExternal = bExternal;
    params->targetSurface.hdl       = static_cast<mfxHDLPair>(hdl);
    params->targetSurface.frameInfo = surf->Info;
    params->targetSurface.frameInfo.Width  = m_width;
    params->targetSurface.frameInfo.Height = m_height;

    return sts;
}

mfxStatus D3D11CameraProcessor::PreWorkInSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, mfxDrvSurface *DrvSurf)
{
    mfxHDLPair hdl;
    mfxStatus sts;
    mfxMemId memIdIn;
    bool     bExternal = false;

    {
        // Request surface from internal pool
        UMC::AutomaticUMCMutex guard(m_guard);
        while( (memIdIn = AcquireResource(*m_InSurfacePool, poolIndex)) == 0 )
        {
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, STALL);
#endif
            vm_time_sleep(10);
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
        }
    }

    mfxFrameSurface1 InSurf = {0};
    mfxFrameSurface1 appInputSurface = *surf;

    InSurf.Data.MemId  = memIdIn;
    InSurf.Info        = appInputSurface.Info;
    InSurf.Info.Width  = m_width;
    InSurf.Info.Height = m_height;
    InSurf.Info.FourCC =  m_params.vpp.In.FourCC == MFX_FOURCC_R16 ? BayerToFourCC(m_CameraParams.Caps.BayerPatternType) : m_params.vpp.In.FourCC;

    if (m_systemMemIn)
    {
        if ( m_paddedInput && appInputSurface.Info.CropX == 0 && appInputSurface.Info.CropY == 0 )
        {
            // Special case: input is marked as padded, but crops do not reflect that
            mfxU32 shift = (8*appInputSurface.Data.Pitch + 8*2);
            appInputSurface.Data.Y += shift;
        }

        appInputSurface.Data.Y += appInputSurface.Data.Pitch*appInputSurface.Info.CropY + (appInputSurface.Info.CropX<<1);
        #ifdef CAMP_PIPE_ITT
            __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, GPUCOPY);
        #endif
        // [1] Copy from system mem to the internal video frame
        sts = m_core->DoFastCopyWrapper(&InSurf,
                                        MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET,
                                        &appInputSurface,
                                        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY);
        MFX_CHECK_STS(sts);
        #ifdef CAMP_PIPE_ITT
            __itt_task_end(CamPipeDX11);
        #endif
        // [2] Get surface handle
        sts = m_core->GetFrameHDL(memIdIn, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
    }
    else
    {
        memIdIn = surf->Data.MemId; 
        // [1] Get external surface handle
        sts = m_core->GetExternalFrameHDL(memIdIn, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
        bExternal = true;
    }

    // [3] Register surfaece. Not needed really for DX11
    sts = m_ddi->Register(&hdl, 1, TRUE);
    MFX_CHECK_STS(sts);

    // [4] Fill in Drv surfaces
    DrvSurf->hdl       = static_cast<mfxHDLPair>(hdl);
    DrvSurf->bExternal = bExternal;
    DrvSurf->frameInfo = InSurf.Info;
    DrvSurf->frameInfo.Width  = m_width;
    DrvSurf->frameInfo.Height = m_height;
    DrvSurf->memId     = memIdIn;

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