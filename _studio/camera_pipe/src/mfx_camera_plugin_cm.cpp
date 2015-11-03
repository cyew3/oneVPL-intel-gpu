/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/
//#include "ipps.h"

#ifndef MFX_VA
#define MFX_VA
#endif

#include "mfx_common.h"

#include <fstream>
#include <algorithm>
#include <string>
#include <stdexcept> /* for std exceptions on Linux/Android */

#include "libmfx_core_interface.h"
#include "mfx_camera_plugin.h"
#include "genx_hsw_camerapipe_isa.h"
#include "genx_bdw_camerapipe_isa.h"
#include "genx_skl_camerapipe_isa.h"

namespace
{

using MfxCameraPlugin::CmRuntimeError;


CmProgram * ReadProgram(CmDevice * device, const mfxU8 * buffer, size_t len)
{
    int result = CM_SUCCESS;
    CmProgram * program = 0;

    if ((result = ::ReadProgram(device, program, buffer, (mfxU32)len)) != CM_SUCCESS)
        throw CmRuntimeError();

    return program;
}

CmKernel * CreateKernel(CmDevice * device, CmProgram * program, char const * name, void * funcptr)
{
    int result = CM_SUCCESS;
    CmKernel * kernel = 0;

    if ((result = ::CreateKernel(device, program, name, funcptr, kernel)) != CM_SUCCESS)
        throw CmRuntimeError();

    return kernel;
}

SurfaceIndex & GetIndex(CmSurface2D * surface)
{
    SurfaceIndex * index = 0;
    int result = surface->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

SurfaceIndex & GetIndex(CmSurface2DUP * surface)
{
    SurfaceIndex * index = 0;
    int result = surface->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}


SurfaceIndex & GetIndex(CmBuffer * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

SurfaceIndex & GetIndex(CmBufferUP * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

void Read(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->ReadSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Write(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->WriteSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

};

mfxStatus CMCameraProcessor::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    if (in)
    {
        //ToDo: we need to carry about width/height in case of system memory passthrough without copy,
        //please think about it, so actual formula should be calculated from width and height, and magic number below doesn't look like applicable
        if ( in->vpp.In.CropW * in->vpp.In.CropH > 0x16E3600 )
        {
            if ( in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            {
                // In case of tiling, only system memory is supported as output
                // Must be documented
                return MFX_ERR_UNSUPPORTED;
            }
        }

        for (int i = 0; i < in->NumExtParam; i++)
        {
            if (in->ExtParam[i])
            {
                if (MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION == in->ExtParam[i]->BufferId ||
                    MFX_EXTBUF_CAM_3DLUT                     == in->ExtParam[i]->BufferId)
                {
                    return MFX_ERR_UNSUPPORTED;
                }
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CMCameraProcessor::AsyncRoutine(AsyncParams *pParam)
{
    mfxStatus sts;
    mfxFrameSurface1 *surfIn  = pParam->surf_in;
    mfxFrameSurface1 *surfOut = pParam->surf_out;

    sts = m_core->IncreasePureReference(m_activeThreadCount);
#if 0
    {
        // In case of CM, we need to do a hard reset every N frames to free up the memory.
        UMC::AutomaticUMCMutex guard(m_guard_hard_reset);
        m_FramesTillHardReset--;

        if ( 0 == m_FramesTillHardReset)
        {
            m_FramesTillHardReset = CAMERA_FRAMES_TILL_HARD_RESET;

            //sts = WaitForActiveThreads();

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

            if (m_correctedSurf)
                m_cmDevice->DestroySurface(m_correctedSurf);

            if (m_dnrSurf)
                m_cmDevice->DestroySurface(m_dnrSurf);

            if (m_denoiseSurf)
                m_cmDevice->DestroySurface(m_denoiseSurf);

            if (m_vignetteMaskSurf)
                m_cmDevice->DestroySurface(m_vignetteMaskSurf);

            if( m_LUTSurf)
                m_cmDevice->DestroySurface(m_LUTSurf);

            m_cmCtx->Close();
            m_cmDevice.Reset(0);

            // Init again
            m_cmDevice.Reset(CreateCmDevicePtr(m_core));
            m_cmCtx.reset(new CmContext(m_Params, m_cmDevice, &m_Params.Caps, m_platform));
            m_cmCtx->CreateThreadSpaces(&m_Params);
            sts = AllocateInternalSurfaces();
        }
    }
#endif

    m_core->IncreaseReference(&surfIn->Data);
    m_core->IncreaseReference(&surfOut->Data);

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, task1);
#endif

    sts  = CreateEnqueueTasks(pParam);
    if (sts != MFX_ERR_NONE)
        return sts;

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif

    return MFX_ERR_NONE;
}

mfxStatus CMCameraProcessor::SetExternalSurfaces(AsyncParams *pParam)
{
    MFX_CHECK_NULL_PTR1(pParam);

    bool    b2DUPAllowed      = true;
    int     tileIDHorizontal  = pParam->tileIDHorizontal;
    int       tileIDVertical  = pParam->tileIDVertical;
    mfxFrameSurface1 *surfIn  = pParam->surf_in;
    mfxFrameSurface1 *surfOut = pParam->surf_out;

    if (pParam->Caps.InputMemoryOperationMode == MEM_GPUSHARED && surfIn->Data.MemId)
    {
        m_core->LockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);
    }

    if (pParam->Caps.OutputMemoryOperationMode == MEM_GPUSHARED && surfOut->Data.MemId)
    {
        m_core->LockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);
    }

    mfxU32 inPitch = ((mfxU32)surfIn->Data.PitchHigh << 16) | ((mfxU32)surfIn->Data.PitchLow);

    if (pParam->Caps.bNoPadding)
    {
        mfxU32 inWidth  = (mfxU32)surfIn->Info.Width;
        mfxU32 inHeight = (mfxU32)m_Params.TileHeightPadded;
        mfxU16 *ptr     = (mfxU16*)((mfxU8*)surfIn->Data.Y16 + m_Params.tileOffsets[tileIDHorizontal].TileOffset *inPitch) + tileIDVertical*pParam->FrameSizeExtra.frameWidth * 2;

        if ( m_platform == MFX_HW_BDW && (inWidth & 15) != 0 )
        {
            // CM specific beh: on BDW width must be aligned to 16, otherwise 2DUP fails
            b2DUPAllowed = false;
        }

        if (m_Params.Caps.InputMemoryOperationMode == MEM_GPUSHARED)
        {
            mfxU32 allocSize, allocPitch;
            CAMERA_DEBUG_LOG("SetExternalSurfaces Padded: m_cmDevice =%p \n", m_cmDevice);
            m_cmDevice->GetSurface2DInfo(sizeof(mfxU16)*inWidth, inHeight, CM_SURFACE_FORMAT_A8, allocPitch, allocSize);
            CAMERA_DEBUG_LOG("SetExternalSurfaces Padded: inPitch=%d allocPitch=%d inWidth=%d inHeight=%d \n", inPitch, allocPitch,inWidth, inHeight);

            if (b2DUPAllowed && inPitch == allocPitch && !((mfxU64)ptr & 0xFFF))
            {
                pParam->inSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8, (void *)ptr);
            }
            else
            {
                pParam->inSurf2DUP = 0;
            }
            pParam->inSurf2D = 0;
        }
        else if (m_Params.Caps.InputMemoryOperationMode == MEM_GPU)
        {
            mfxHDLPair outHdl;
            mfxStatus sts = m_core->GetExternalFrameHDL(surfIn->Data.MemId, (mfxHDL*)&outHdl);
            if (sts != MFX_ERR_NONE)
                return sts;

            CmSurface2D *pSurf2D = m_cmCtx->CreateCmSurface2D(outHdl.first);
            if (!pSurf2D)
                return MFX_ERR_MEMORY_ALLOC;

            pParam->inSurf2D   = (mfxMemId)pSurf2D;
            pParam->inSurf2DUP = 0;
        }
    }
    else
    {    // need to do padding
        if (pParam->Caps.InputMemoryOperationMode == MEM_GPUSHARED)
        {
            mfxU32 inWidth  = (mfxU32)surfIn->Info.Width;
            mfxU32 inHeight = (mfxU32)m_Params.TileHeight;
            mfxU32 inHeightAligned = ((mfxU32)(inHeight + 1)/2)*2;
            mfxU16 *ptr     = (mfxU16*)((mfxU8*)surfIn->Data.Y16 + m_Params.tileOffsets[tileIDHorizontal].TileOffset*inPitch) + tileIDVertical*pParam->FrameSizeExtra.frameWidth * 2;
            mfxU32 allocSize, allocPitch;

            if ( m_platform == MFX_HW_BDW && (inWidth & 15) != 0 )
            {
                // CM specific beh: on BDW width must be aligned to 16, otherwise 2DUP fails
                b2DUPAllowed = false;
            }
            m_cmDevice->GetSurface2DInfo(sizeof(mfxU16)*inWidth, inHeight, CM_SURFACE_FORMAT_A8, allocPitch, allocSize);
            if (b2DUPAllowed && !surfIn->Info.CropX && !surfIn->Info.CropY && inHeight == inHeightAligned && inPitch == allocPitch && !((mfxU64)ptr & 0xFFF))
            {
                pParam->inSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, allocPitch,  inHeight, CM_SURFACE_FORMAT_A8, (void *)ptr);
            }
            else
            {
                pParam->inSurf2DUP = 0;
            }
            pParam->inSurf2D = 0;
        }
        else if (pParam->Caps.InputMemoryOperationMode == MEM_GPU)
        {
            mfxHDLPair outHdl;
            mfxStatus sts = m_core->GetExternalFrameHDL(surfIn->Data.MemId, (mfxHDL*)&outHdl);
            if (sts != MFX_ERR_NONE)
                return sts;
            CmSurface2D *pSurf2D = m_cmCtx->CreateCmSurface2D(outHdl.first);
            if (!pSurf2D)
                return MFX_ERR_MEMORY_ALLOC;
            pParam->inSurf2D = (mfxMemId)pSurf2D;
            pParam->inSurf2DUP = 0;
        }
    }

    if (pParam->Caps.OutputMemoryOperationMode == MEM_GPUSHARED) {

        mfxU32 outPitch  = ((mfxU32)surfOut->Data.PitchHigh << 16) | ((mfxU32)surfOut->Data.PitchLow);
        mfxU32 outWidth  = (mfxU32)pParam->FrameSizeExtra.frameWidth;
        mfxU32 outHeight = (mfxU32)m_Params.TileHeight;
        mfxU8 *ptr       = (mfxU8*)surfOut->Data.B + m_Params.tileOffsets[tileIDHorizontal].TileOffset*outPitch + tileIDVertical*pParam->FrameSizeExtra.frameWidth * 4;
        mfxU32 allocSize, allocPitch;

        if ( m_platform == MFX_HW_BDW && (outWidth & 15) != 0 )
        {
            // CM specific beh: on BDW width must be aligned to 16, otherwise 2DUP fails
            b2DUPAllowed = false;
        }

        if (pParam->Caps.bOutToARGB16)
        {
            m_cmDevice->GetSurface2DInfo(sizeof(mfxU16)*outWidth, outHeight, CM_SURFACE_FORMAT_A8R8G8B8, allocPitch, allocSize);
        }
        else
        {
            m_cmDevice->GetSurface2DInfo(outWidth, outHeight, CM_SURFACE_FORMAT_A8R8G8B8, allocPitch, allocSize);
        }

        if (b2DUPAllowed && outPitch == allocPitch && !((mfxU64)ptr & 0xFFF))
        {
            if (m_Params.Caps.bOutToARGB16)
                pParam->outSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, allocPitch>>2, outHeight, CM_SURFACE_FORMAT_A8R8G8B8, (void *)ptr);
            else
                pParam->outSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, allocPitch>>2, outHeight, CM_SURFACE_FORMAT_A8R8G8B8, (void *)ptr);
        }
        else
        {
            pParam->outSurf2DUP = 0;
        }
        pParam->outSurf2D = 0;

    }
    else if (pParam->Caps.OutputMemoryOperationMode == MEM_GPU)
    {
        mfxHDLPair outHdl;

        if (!m_core->IsExternalFrameAllocator())
        {
            CAMERA_DEBUG_LOG("SetExternalSurfaces ExternalFrameAllocator NOT SET ! \n");
        }

        mfxStatus sts = m_core->GetExternalFrameHDL(surfOut->Data.MemId, (mfxHDL*)&outHdl);
        CAMERA_DEBUG_LOG("SetExternalSurfaces GetExternalFrameHDL sts = %d \n", sts);

        if (sts != MFX_ERR_NONE)
            return sts;
        CmSurface2D *pSurf2D = m_cmCtx->CreateCmSurface2D(outHdl.first);
        CAMERA_DEBUG_LOG("SetExternalSurfaces CreateCmSurface2D pSurf2D = %p \n", pSurf2D);

        if (!pSurf2D)
            return MFX_ERR_MEMORY_ALLOC;
        pParam->outSurf2D = (mfxMemId)pSurf2D;
        pParam->outSurf2DUP = 0;
    }

    if (pParam->Caps.InputMemoryOperationMode == MEM_GPUSHARED)
    {
        if (surfIn->Data.MemId)
        {
            m_core->UnlockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);
        }
    }

    if (pParam->Caps.OutputMemoryOperationMode == MEM_GPUSHARED)
    {
        if (surfOut->Data.MemId)
        {
            m_core->UnlockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);
        }
    }
    CAMERA_DEBUG_LOG("SetExternalSurfaces end device %p inSurf2DUP=%p inSurf2D=%p outSurf2DUP=%p outSurf2D=%p \n", m_cmDevice, pParam->inSurf2DUP, pParam->inSurf2D, pParam->outSurf2DUP, pParam->outSurf2D);
    return MFX_ERR_NONE;
}

mfxStatus CMCameraProcessor::CreateEnqueueTasks(AsyncParams *pParam)
{
    { // Guarded section start

    UMC::AutomaticUMCMutex guard(m_guard);
    int doShiftAndSwap = 1;
    CmEvent  *e = NULL;
    SurfaceIndex *pInputSurfaceIndex;
    //bool  firstTile = false;
    mfxU32 out_pitch = pParam->surf_out->Data.PitchLow + ((mfxU32)pParam->surf_out->Data.PitchHigh << 16); 
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task21);
#endif
    for ( mfxU16 tileIDVert  = pParam->FrameSizeExtra.tileNumVer; tileIDVert  > 0 ; tileIDVert--) // Going over vertical tiles
    {
    for ( mfxU16 tileIDHor = 0; tileIDHor < pParam->FrameSizeExtra.tileNumHor;  tileIDHor++) // Goint over horizontal tiles
    {
        // Calculate offsets for tiles
        int in_shift  =  (tileIDVert-1) * pParam->FrameSizeExtra.frameWidth * 2 + pParam->FrameSizeExtra.tileOffsets[tileIDHor].TileOffset * pParam->surf_in->Data.Pitch;
        int out_shift;
        if (pParam->surf_out->Info.FourCC == MFX_FOURCC_RGB4) 
            out_shift = (tileIDVert-1) * pParam->FrameSizeExtra.frameWidth * 4 + pParam->FrameSizeExtra.tileOffsets[tileIDHor].TileOffset * out_pitch;
        else
            out_shift = (tileIDVert-1) * pParam->FrameSizeExtra.frameWidth * 8 + pParam->FrameSizeExtra.tileOffsets[tileIDHor].TileOffset * out_pitch;

        if ( tileIDHor > 0 && pParam->Caps.bNoPadding)
        {
            out_shift-= MFX_CAM_DEFAULT_PADDING_TOP*pParam->surf_out->Data.Pitch;
        }

        SurfaceIndex *pPaddedSurfaceIndex = 0;
        SurfaceIndex *DNRIndex            = 0;

        CmSurface2D *goodPixCntSurf= 0;
        CmSurface2D *bigPixCntSurf = 0;
        CmSurface2D *greenHorSurf  = 0;
        CmSurface2D *greenVerSurf  = 0;
        CmSurface2D *greenAvgSurf  = 0;
        CmSurface2D *avgFlagSurf   = 0;
        CmSurface2D *blueHorSurf   = 0;
        CmSurface2D *blueVerSurf   = 0;
        CmSurface2D *blueAvgSurf   = 0;
        CmSurface2D *redHorSurf    = 0;
        CmSurface2D *redVerSurf    = 0;
        CmSurface2D *redAvgSurf    = 0;
        CmSurface2D *redOutSurf    = 0;
        CmSurface2D *greenOutSurf  = 0;
        CmSurface2D *blueOutSurf   = 0;

        SurfaceIndex *pOutputSurfaceIndex;
        pParam->tileIDHorizontal = tileIDHor;
        pParam->tileIDVertical   = (tileIDVert-1);
        SetExternalSurfaces(pParam);

        if (pParam->inSurf2D)
        {
            CmSurface2D *inSurf = (CmSurface2D *)pParam->inSurf2D;
            inSurf->GetIndex(pInputSurfaceIndex);
        }
        else if (pParam->inSurf2DUP)
        {
            CmSurface2DUP *inSurf = (CmSurface2DUP *)pParam->inSurf2DUP;
            inSurf->GetIndex(pInputSurfaceIndex);
        }
        else if (pParam->Caps.InputMemoryOperationMode == MEM_GPUSHARED)
        {
            m_cmSurfIn->GetIndex(pInputSurfaceIndex);
        }
        else
            return MFX_ERR_NULL_PTR;

        m_paddedSurf->GetIndex(pPaddedSurfaceIndex);
        if (!pParam->Caps.bNoPadding)
        {
            m_cmCtx->CreateTask_Padding(*pInputSurfaceIndex,
                                    *pPaddedSurfaceIndex,
                                    pParam->FrameSizeExtra.paddedFrameWidth,
                                    pParam->FrameSizeExtra.TileHeightPadded,
                                    (int)pParam->InputBitDepth,
                                    (int)pParam->Caps.BayerPatternType);
            m_paddedSurf->GetIndex(pInputSurfaceIndex);
            doShiftAndSwap = 0;
        }

        if ( pParam->Caps.bVignetteCorrection )
        {
            SurfaceIndex *Mask4x4Index;
            SurfaceIndex *vignetteMaskIndex;
            if ( m_vignetteMaskSurf )
            {
                m_vignetteMaskSurf->GetIndex(vignetteMaskIndex);
            }
            else
            {
                return MFX_ERR_ABORTED;
            }

            if ( m_vignette_4x4 ) 
            {
                m_vignette_4x4->GetIndex(Mask4x4Index);
            }
            else
            {
                return MFX_ERR_ABORTED;
            }
            m_cmCtx->CreateTask_VignetteMaskUpSample(*Mask4x4Index, *vignetteMaskIndex);
        }
        if ( pParam->Caps.bBlackLevelCorrection || pParam->Caps.bWhiteBalance || pParam->Caps.bVignetteCorrection )
        {
            SurfaceIndex *vignetteMaskIndex;
            if ( m_vignetteMaskSurf )
            {
                m_vignetteMaskSurf->GetIndex(vignetteMaskIndex);
            }
            else
            {
                vignetteMaskIndex = pInputSurfaceIndex;
            }

            m_cmCtx->CreateTask_BayerCorrection(
                            doShiftAndSwap,
                            *pInputSurfaceIndex,
                            *pPaddedSurfaceIndex,
                            *vignetteMaskIndex,
                            pParam->Caps.bBlackLevelCorrection,
                            pParam->Caps.bVignetteCorrection,
                            pParam->Caps.bWhiteBalance,
                            (short)pParam->BlackLevelParams.BlueLevel, (short)pParam->BlackLevelParams.GreenBottomLevel, (short)pParam->BlackLevelParams.GreenTopLevel, (short)pParam->BlackLevelParams.RedLevel,
                            (float)pParam->WBparams.RedCorrection,    (float)pParam->WBparams.GreenBottomCorrection,    (float)pParam->WBparams.GreenTopCorrection,    (float)pParam->WBparams.BlueCorrection,
                            pParam->InputBitDepth,
                            pParam->Caps.BayerPatternType);
            doShiftAndSwap = 0;
        }

        if (pParam->Caps.bHotPixel || pParam->Caps.bBayerDenoise)
        {
            // Bayer denoise need DNR surface

            m_dnrSurf->GetIndex(DNRIndex);

            if ( pParam->Caps.bHotPixel || doShiftAndSwap)
            {
                // In case of hot pixel removal corrected surface will be an output
                m_correctedSurf->GetIndex(pInputSurfaceIndex);
            }
            m_cmCtx->CreateTask_HotPixel(
                            doShiftAndSwap,
                            *pPaddedSurfaceIndex,
                            *pInputSurfaceIndex,
                            *DNRIndex,
                            pParam->HPParams.PixelThresholdDifference,
                            pParam->HPParams.PixelCountThreshold,
                            pParam->InputBitDepth,
                            pParam->Caps.BayerPatternType);
            doShiftAndSwap = 0;
        }

        if (pParam->Caps.bBayerDenoise)
        {
            SurfaceIndex *DenoiseIndex = 0;
            m_denoiseSurf->GetIndex(DenoiseIndex);
            m_cmCtx->CreateTask_Denoise(
                            doShiftAndSwap,
                            *pInputSurfaceIndex,
                            *DenoiseIndex,
                            *DNRIndex,
                            pParam->DenoiseParams.Threshold,
                            pParam->InputBitDepth,
                            pParam->Caps.BayerPatternType);
            doShiftAndSwap = 0;
            m_denoiseSurf->GetIndex(pInputSurfaceIndex);

        }

        goodPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8, 0);
        bigPixCntSurf  = (CmSurface2D *)AcquireResource(m_aux8, 1);
        m_cmCtx->CreateTask_GoodPixelCheck(*pInputSurfaceIndex,
                                            *pPaddedSurfaceIndex,
                                            goodPixCntSurf,
                                            bigPixCntSurf,
                                            (int)pParam->InputBitDepth,
                                            doShiftAndSwap,
                                            (int)pParam->Caps.BayerPatternType);
        pInputSurfaceIndex = pPaddedSurfaceIndex;

        greenHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 0);
        greenVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 1);
        greenAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 2);

        avgFlagSurf  = m_avgFlagSurf;

        m_cmCtx->CreateTask_RestoreGreen(*pInputSurfaceIndex,
                                            goodPixCntSurf,
                                            bigPixCntSurf,
                                            greenHorSurf,
                                            greenVerSurf,
                                            greenAvgSurf,
                                            avgFlagSurf,
                                            pParam->InputBitDepth, (int)pParam->Caps.BayerPatternType);

        blueHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 3);
        blueVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 4);
        blueAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 5);
        redHorSurf  = (CmSurface2D *)AcquireResource(m_raw16padded, 6);
        redVerSurf  = (CmSurface2D *)AcquireResource(m_raw16padded, 7);
        redAvgSurf  = (CmSurface2D *)AcquireResource(m_raw16padded, 8);

        m_cmCtx->CreateTask_RestoreBlueRed(*pInputSurfaceIndex,
                                            greenHorSurf, greenVerSurf, greenAvgSurf,
                                            blueHorSurf, blueVerSurf, blueAvgSurf,
                                            redHorSurf, redVerSurf, redAvgSurf,
                                            avgFlagSurf, pParam->InputBitDepth, (int)pParam->Caps.BayerPatternType);


        redOutSurf   = (CmSurface2D *)AcquireResource(m_raw16aligned, 0);
        greenOutSurf = (CmSurface2D *)AcquireResource(m_raw16aligned, 1);
        blueOutSurf  = (CmSurface2D *)AcquireResource(m_raw16aligned, 2);

        m_cmCtx->CreateTask_SAD(redHorSurf,
                                greenHorSurf,
                                blueHorSurf,
                                redVerSurf,
                                greenVerSurf,
                                blueVerSurf,
                                redOutSurf,
                                greenOutSurf,
                                blueOutSurf,
                                (int)pParam->Caps.BayerPatternType);

        m_cmCtx->CreateTask_DecideAverage(redAvgSurf,
                                          greenAvgSurf,
                                          blueAvgSurf,
                                          avgFlagSurf,
                                          redOutSurf,
                                          greenOutSurf,
                                          blueOutSurf,
                                          (int)pParam->Caps.BayerPatternType,
                                          pParam->InputBitDepth);

        if (pParam->Caps.OutputMemoryOperationMode == MEM_GPUSHARED && !pParam->outSurf2DUP)
        {
            pOutputSurfaceIndex = NULL;
        }
        else if (pParam->outSurf2DUP)
        {
            CmSurface2DUP *outSurfUP = (CmSurface2DUP *)pParam->outSurf2DUP;
            outSurfUP->GetIndex(pOutputSurfaceIndex);
        }
        else if (pParam->outSurf2D)
        {
            CmSurface2D *outSurf2D = (CmSurface2D *)pParam->outSurf2D;
            outSurf2D->GetIndex(pOutputSurfaceIndex);
        }
        else
            return MFX_ERR_NULL_PTR;

        if (pParam->Caps.bForwardGammaCorrection)
        {
            SurfaceIndex *LUTIndex_R = NULL;
            m_LUTSurf_R->GetIndex(LUTIndex_R);
            SurfaceIndex *LUTIndex_G = NULL;
            m_LUTSurf_G->GetIndex(LUTIndex_G);
            SurfaceIndex *LUTIndex_B = NULL;
            m_LUTSurf_B->GetIndex(LUTIndex_B);

            if (pOutputSurfaceIndex)
                m_cmCtx->CreateTask_GammaAndCCM(m_gammaCorrectSurfR,
                                                m_gammaCorrectSurfG,
                                                m_gammaCorrectSurfB,
                                                m_gammaPointSurf,
                                                redOutSurf,
                                                greenOutSurf,
                                                blueOutSurf,
                                                (pParam->Caps.bColorConversionMatrix ? &pParam->CCMParams : NULL),
                                                *pOutputSurfaceIndex,
                                                pParam->InputBitDepth,
                                                LUTIndex_R, LUTIndex_B, LUTIndex_G,
                                                (int)pParam->Caps.BayerPatternType,
                                                pParam->Caps.bOutToARGB16 ? 0 : 1);
            else
                m_cmCtx->CreateTask_GammaAndCCM(m_gammaCorrectSurfR,
                                                m_gammaCorrectSurfG,
                                                m_gammaCorrectSurfB,
                                                m_gammaPointSurf,
                                                redOutSurf,
                                                greenOutSurf,
                                                blueOutSurf,
                                                (pParam->Caps.bColorConversionMatrix ? &pParam->CCMParams : NULL),
                                                m_gammaOutSurf,
                                                pParam->InputBitDepth,
                                                LUTIndex_R, LUTIndex_B, LUTIndex_G,
                                                (int)pParam->Caps.BayerPatternType,
                                                pParam->Caps.bOutToARGB16 ? 0 : 1);


        }
        else
        {
            if (!pOutputSurfaceIndex)
            {
                    m_gammaOutSurf->GetIndex(pOutputSurfaceIndex);
            }

            m_cmCtx->CreateTask_ARGB(redOutSurf,
                                        greenOutSurf,
                                        blueOutSurf,
                                        *pOutputSurfaceIndex,
                                        (pParam->Caps.bColorConversionMatrix ? &pParam->CCMParams : NULL),
                                        pParam->InputBitDepth,
                                        (int)pParam->Caps.BayerPatternType);

        }


        if (pParam->Caps.InputMemoryOperationMode == MEM_GPUSHARED && !pParam->inSurf2DUP)
        {
            if (! pParam->FrameSizeExtra.TileInfo.CropX && ! pParam->FrameSizeExtra.TileInfo.CropY &&  1 == pParam->FrameSizeExtra.tileNumHor && pParam->surf_in->Info.CropW*2 == pParam->surf_in->Data.Pitch)
            {
                m_cmCtx->EnqueueCopyCPUToGPU(m_cmSurfIn, pParam->surf_in->Data.Y16);
            }
            else
            {
                m_cmCtx->EnqueueCopyCPUToGPU(m_cmSurfIn,
                                             (mfxU8 *)(pParam->surf_in->Data.Y16 + pParam->FrameSizeExtra.TileInfo.CropX ) + in_shift + pParam->FrameSizeExtra.TileInfo.CropY * pParam->surf_in->Data.Pitch,
                                             pParam->surf_in->Data.Pitch);
            }
        }

        if ( ! pParam->Caps.bNoPadding )
            m_cmCtx->EnqueueTask_Padding();

        if ( pParam->Caps.bVignetteCorrection )
        {
             mfxU8 *ptr = (mfxU8 *)pParam->VignetteParams.pCmCorrectionMap + 
                           pParam->VignetteParams.CmStride * tileIDHor * ( pParam->VignetteParams.Height/pParam->FrameSizeExtra.tileNumHor);
             m_cmCtx->EnqueueCopyCPUToGPU(m_vignette_4x4,
                                          ptr,
                                          pParam->VignetteParams.CmStride);
             m_cmCtx->EnqueueTask_VignetteMaskUpSample();
        }

        if ( pParam->Caps.bBlackLevelCorrection || pParam->Caps.bWhiteBalance || pParam->Caps.bVignetteCorrection )
        {
            m_cmCtx->EnqueueTask_BayerCorrection();
        }

        if (pParam->Caps.bHotPixel || pParam->Caps.bBayerDenoise)
        {
            m_cmCtx->EnqueueTask_HP();
        }

        if (pParam->Caps.bBayerDenoise)
        {
            m_cmCtx->EnqueueTask_Denoise();
        }


        m_cmCtx->EnqueueTask_GoodPixelCheck();

        for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
        {
            m_cmCtx->EnqueueSliceTasks(i);
        }

        if ( pParam->Caps.bForwardGammaCorrection)
        {
            e = m_cmCtx->EnqueueTask_ForwardGamma();
        }
        else
        {
            if (e != NULL)
                m_cmCtx->DestroyEventWithoutWait(e);

            e = m_cmCtx->EnqueueTask_ARGB();
        }

#ifdef CAMP_PIPE_ITT
        __itt_task_end(CamPipeAccel);
#endif

        if (pParam->Caps.OutputMemoryOperationMode == MEM_GPUSHARED && !pParam->outSurf2DUP)
        {
            // cropW instead of width ???
            m_cmCtx->DestroyEventWithoutWait(e);
            if (pParam->Caps.bOutToARGB16)
            {
                if (1 == pParam->FrameSizeExtra.tileNumHor && pParam->surf_out->Info.CropW*8 == out_pitch)
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B);
                else
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B  + out_shift,out_pitch);
            }
            else
            {
                if (1 == pParam->FrameSizeExtra.tileNumHor && pParam->surf_out->Info.CropW*4 == out_pitch)
                {
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B);
                }
                else
                {
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B + out_shift, out_pitch);
                }
            }
        }
        pParam->pEvent = e;
    } // for each horizontal tile
    } // for each vertical tile

    } // Guarded section end
    return MFX_ERR_NONE;
}

mfxStatus CMCameraProcessor::Reset(mfxVideoParam *par, CameraParams *pipeParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(pipeParams);
    bool bFrameSizeChanged = *pipeParams != m_Params;
    bool bNewFilters        = pipeParams->Caps != m_Params.Caps;

    if ( bFrameSizeChanged || bNewFilters )
    {
        bool CCM_Gamma_change = false;
        if ( (pipeParams->Caps.bColorConversionMatrix != m_Params.Caps.bColorConversionMatrix           && pipeParams->Caps.bForwardGammaCorrection == 1 &&  m_Params.Caps.bForwardGammaCorrection == 1 ) ||
             (pipeParams->Caps.bColorConversionMatrix == 1 && m_Params.Caps.bColorConversionMatrix == 1 && pipeParams->Caps.bForwardGammaCorrection != m_Params.Caps.bForwardGammaCorrection ))
        {
            // CMM and Gamma share the same kernel. If any changes happens in CCM/Gamma need to re-create kernels
            CCM_Gamma_change = true;
        }

        if ( bFrameSizeChanged || CCM_Gamma_change )
        {
            WaitForActiveThreads();
        }

        m_cmCtx->Reset(*par, &(pipeParams->Caps), &(m_Params.Caps));

        if ( bFrameSizeChanged)
        {
            m_cmCtx->CreateThreadSpaces(pipeParams);
        }

        if ( bFrameSizeChanged  ||
            ( bNewFilters && pipeParams->Caps.bColorConversionMatrix == 1 && m_Params.Caps.bColorConversionMatrix == 0 ) ||
            ( bNewFilters && pipeParams->Caps.bHotPixel == 1 && m_Params.Caps.bHotPixel == 0 ) ||
            ( bNewFilters && pipeParams->Caps.bVignetteCorrection == 1 && m_Params.Caps.bVignetteCorrection == 0 ))
        {
            // Re-allocate surfaces in case frame resolution is changed or add CCM/HP/Vignette that requires additional surf.
            sts = ReallocateInternalSurfaces(*par, *pipeParams);
            if (sts < MFX_ERR_NONE)
                return sts;
        }
    }
    // will need to check numPoints as well when different LUT sizes are supported
    if (pipeParams->Caps.bForwardGammaCorrection || pipeParams->Caps.bColorConversionMatrix)
    {
        if (!m_gammaCorrectSurfR)
            m_gammaCorrectSurfR = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        if (!m_gammaCorrectSurfG)
            m_gammaCorrectSurfG = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        if (!m_gammaCorrectSurfB)
            m_gammaCorrectSurfB = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        
        if (!m_gammaPointSurf)
            m_gammaPointSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);

        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfR);
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfG);
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfB);
        MFX_CHECK_NULL_PTR1(m_gammaPointSurf)

        mfxU16 GammaPts[MFX_CAM_GAMMA_NUM_POINTS_SKL];
        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaPts[i] = pipeParams->GammaParams.Segment[i].Pixel;
        m_gammaPointSurf->WriteSurface((unsigned char *)GammaPts, NULL);

        mfxU16 GammaCor[MFX_CAM_GAMMA_NUM_POINTS_SKL];
        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaCor[i] = pipeParams->GammaParams.Segment[i].Red;
        m_gammaCorrectSurfR->WriteSurface((unsigned char *)GammaCor, NULL);

        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaCor[i] = pipeParams->GammaParams.Segment[i].Green;
        m_gammaCorrectSurfG->WriteSurface((unsigned char *)GammaCor, NULL);

        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaCor[i] = pipeParams->GammaParams.Segment[i].Blue;
        m_gammaCorrectSurfB->WriteSurface((unsigned char *)GammaCor, NULL);

    }

    m_Params = *pipeParams;
    return sts;
}

mfxStatus CMCameraProcessor::CompleteRoutine(AsyncParams *pParams)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    m_raw16aligned.Unlock();
    m_raw16padded.Unlock();
    m_aux8.Unlock();

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, taske);
#endif
    CmEvent *e = (CmEvent *)pParams->pEvent;
    CAMERA_DEBUG_LOG("CompleteCameraAsyncRoutine e=%p device %p \n", e, m_cmDevice);

    if (m_isInitialized)
    {
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, destroyevent);
#endif
        m_cmCtx->DestroyEvent(e);
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe, __itt_null, __itt_null, destroyevent);
#endif
    }
    else
    {
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, destroyeventwowait);
#endif
        m_cmCtx->DestroyEventWithoutWait(e);
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, destroyeventwowait);
#endif
    }
    CAMERA_DEBUG_LOG("CompleteCameraAsyncRoutine Destroyed event e=%p \n", e);

    if (pParams->inSurf2DUP)
    {
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipe, __itt_null, __itt_null, task_destroy_surfup);
#endif
        CmSurface2DUP *surf = (CmSurface2DUP *)pParams->inSurf2DUP;
        m_cmDevice->DestroySurface2DUP(surf);
        pParams->inSurf2DUP = 0;
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe, __itt_null, __itt_null, task_destroy_surfup);
#endif
    }

    m_core->DecreaseReference(&pParams->surf_in->Data);

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipe);
#endif

    if (pParams->outSurf2DUP)
    {
        CmSurface2DUP *surf = (CmSurface2DUP *)pParams->outSurf2DUP;
        m_cmDevice->DestroySurface2DUP(surf);
        pParams->outSurf2DUP = 0;
    }

    m_core->DecreaseReference(&pParams->surf_out->Data);
    m_core->DecreasePureReference(m_activeThreadCount);
    return MFX_ERR_NONE;
}

mfxStatus CMCameraProcessor::WaitForActiveThreads()
{
    mfxStatus sts = MFX_WRN_IN_EXECUTION;
    for (mfxU32 i = 0; i < 200; i++)
    {
        if (m_activeThreadCount == 0)
        {
            sts = MFX_ERR_NONE;
            break;
        }
        vm_time_sleep(10);
    }
    return sts;
};

mfxStatus CMCameraProcessor::AllocateInternalSurfaces()
{
    mfxFrameAllocRequest request = {};
    mfxU32 frNum = 0;
    mfxStatus sts;

    request.Info        = m_Params.TileInfo;
    request.Info.Width  = (mfxU16)m_Params.vSliceWidth;
    request.Info.Height = (mfxU16)m_Params.TileHeightPadded;

    if (m_Params.Caps.bWhiteBalance)
        frNum++;

    if (m_Params.Caps.bDemosaic)
        frNum += 9;

    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);

    sts = m_raw16padded.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width  = (mfxU16)m_Params.frameWidth64;
    request.Info.Height = (mfxU16)m_Params.TileHeight;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);
    frNum = 3;
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_raw16aligned.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width  = (mfxU16)m_Params.paddedFrameWidth;
    request.Info.Height = (mfxU16)m_Params.TileHeightPadded;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    frNum = 2;

    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_aux8.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    m_avgFlagSurf = CreateSurface(m_cmDevice,
                                  m_Params.vSliceWidth,
                                  m_Params.TileHeightPadded,
                                  CM_SURFACE_FORMAT_A8);

    if (m_Params.Caps.bColorConversionMatrix || m_Params.Caps.bForwardGammaCorrection )
    {
        m_LUTSurf_R = CreateBuffer(m_cmDevice, 65536 * 4);
        m_LUTSurf_G = CreateBuffer(m_cmDevice, 65536 * 4);
        m_LUTSurf_B = CreateBuffer(m_cmDevice, 65536 * 4);
        MFX_CHECK_NULL_PTR1(m_LUTSurf_R);
        MFX_CHECK_NULL_PTR1(m_LUTSurf_G);
        MFX_CHECK_NULL_PTR1(m_LUTSurf_B);
    }

    if (m_Params.Caps.bVignetteCorrection || m_Params.Caps.bBlackLevelCorrection || m_Params.Caps.bWhiteBalance )
    {
        m_vignetteMaskSurf = CreateSurface(m_cmDevice,
                                           m_Params.paddedFrameWidth,
                                           m_Params.TileHeightPadded,
                                           CM_SURFACE_FORMAT_YUY2);
        MFX_CHECK_NULL_PTR1(m_vignetteMaskSurf);


        if ( m_Params.Caps.bVignetteCorrection  && m_Params.VignetteParams.pCorrectionMap )
        {
            mfxU32 width_4x4 = ( m_Params.paddedFrameWidth >> 2 );
            width_4x4 = width_4x4 % 2 ? width_4x4 + 1 : width_4x4;
            mfxU32  height_4x4 = m_Params.VignetteParams.Height >> (m_Params.tileNumHor - 1) ;
            m_vignette_4x4  = CreateSurface(m_cmDevice,
                                        width_4x4,
                                        height_4x4,
                                        CM_SURFACE_FORMAT_YUY2);

            MFX_CHECK_NULL_PTR1(m_vignette_4x4);
        }
    }

    if (m_Params.Caps.bForwardGammaCorrection || m_Params.Caps.bColorConversionMatrix)
    {
        m_gammaCorrectSurfR = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        m_gammaCorrectSurfG = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        m_gammaCorrectSurfB = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        m_gammaPointSurf    = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfR);
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfG);
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfB);
        MFX_CHECK_NULL_PTR1(m_gammaPointSurf)
    }

    if (m_Params.Caps.bForwardGammaCorrection)
    {
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfR);
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfG);
        MFX_CHECK_NULL_PTR1(m_gammaCorrectSurfB);
        MFX_CHECK_NULL_PTR1(m_gammaPointSurf)
        mfxU16 GammaPts[MFX_CAM_GAMMA_NUM_POINTS_SKL];
        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaPts[i] = m_Params.GammaParams.Segment[i].Pixel;
        m_gammaPointSurf->WriteSurface((unsigned char *)GammaPts, NULL);

        mfxU16 GammaCor[MFX_CAM_GAMMA_NUM_POINTS_SKL];
        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaCor[i] = m_Params.GammaParams.Segment[i].Red;
        m_gammaCorrectSurfR->WriteSurface((unsigned char *)GammaCor, NULL);

        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaCor[i] = m_Params.GammaParams.Segment[i].Green;
        m_gammaCorrectSurfG->WriteSurface((unsigned char *)GammaCor, NULL);

        for(int i = 0; i < MFX_CAM_GAMMA_NUM_POINTS_SKL; i++)
            GammaCor[i] = m_Params.GammaParams.Segment[i].Blue;
        m_gammaCorrectSurfB->WriteSurface((unsigned char *)GammaCor, NULL);
    }

    if (m_Params.Caps.InputMemoryOperationMode == MEM_GPUSHARED) // need to allocate inputSurf in case the output memory is not 4k aligned
    {
        int width  = m_Params.Caps.bNoPadding ? m_Params.paddedFrameWidth : m_Params.frameWidth;
        int height = m_Params.Caps.bNoPadding ? m_Params.TileHeightPadded : m_Params.TileHeight;

        CAMERA_DEBUG_LOG("AllocateInternalSurfaces:  m_cmSurfIn width %d height %d \n", width, height);
        m_cmSurfIn = CreateSurface(m_cmDevice, width * sizeof(mfxU16),  height, CM_SURFACE_FORMAT_A8);
    }

    if (m_Params.Caps.OutputMemoryOperationMode == MEM_GPUSHARED) // need to allocate gammaSurf in case the output memory is not 4k aligned
    {
        if (!m_Params.Caps.bOutToARGB16)
            m_gammaOutSurf = CreateSurface(m_cmDevice, m_Params.frameWidth,  m_Params.TileHeight , CM_SURFACE_FORMAT_A8R8G8B8);
        else
            m_gammaOutSurf = CreateSurface(m_cmDevice, m_Params.frameWidth *sizeof(mfxU16),  m_Params.TileHeight, CM_SURFACE_FORMAT_A8R8G8B8);
    }

    if( m_Params.Caps.bHotPixel )
    {
        m_correctedSurf = CreateSurface(m_cmDevice,  m_Params.paddedFrameWidth*sizeof(mfxU16), m_Params.TileHeightPadded, CM_SURFACE_FORMAT_A8);
    }

    if ( m_Params.Caps.bBayerDenoise )
    {
        m_denoiseSurf = CreateSurface(m_cmDevice,  m_Params.paddedFrameWidth*sizeof(mfxU16), m_Params.TileHeightPadded, CM_SURFACE_FORMAT_A8);
    }

    if ( m_Params.Caps.bHotPixel || m_Params.Caps.bBayerDenoise )
    {
        m_dnrSurf = CreateSurface(m_cmDevice,  m_Params.paddedFrameWidth*sizeof(mfxU16), m_Params.TileHeightPadded, CM_SURFACE_FORMAT_A8);
    }

    m_paddedSurf = CreateSurface(m_cmDevice,  m_Params.paddedFrameWidth*sizeof(mfxU16), m_Params.TileHeightPadded, CM_SURFACE_FORMAT_A8);
    CAMERA_DEBUG_LOG("AllocateInternalSurfaces:  m_cmSurfIn %p sts %d \n", m_cmSurfIn, sts);
    return sts;
}

mfxStatus CMCameraProcessor::ReallocateInternalSurfaces(mfxVideoParam &newParam, CameraParams &frameSizeExtra)
{
    //m_Caps should be updated earlier
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 frNum = 0;

    if (m_Params.Caps.bDemosaic)
        frNum += 9;

    mfxU16 vSliceWidth       = (mfxU16)frameSizeExtra.vSliceWidth;
    mfxU16 frameWidth64      = (mfxU16)frameSizeExtra.frameWidth64;
    mfxU16 paddedFrameHeight = (mfxU16)frameSizeExtra.TileHeightPadded;
    mfxU16 paddedFrameWidth  = (mfxU16)frameSizeExtra.paddedFrameWidth;

    mfxFrameAllocRequest request = { };

    request.Info        = frameSizeExtra.TileInfo;
    request.Info.Width  = (mfxU16)vSliceWidth;
    request.Info.Height = (mfxU16)frameSizeExtra.TileHeightPadded;

    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);

    sts = m_raw16padded.ReallocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width  = (mfxU16)frameWidth64;
    request.Info.Height = (mfxU16)frameSizeExtra.TileHeight;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);
    frNum = 3;
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_raw16aligned.ReallocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width  = (mfxU16)paddedFrameWidth;
    request.Info.Height = (mfxU16)frameSizeExtra.TileHeightPadded;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    frNum = 2;
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_aux8.ReallocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    if (m_Params.Caps.bForwardGammaCorrection || m_Params.Caps.bColorConversionMatrix)
    {
        if ( ! m_LUTSurf_R )
            m_LUTSurf_R = CreateBuffer(m_cmDevice, 65536 * 4);
        MFX_CHECK_NULL_PTR1(m_LUTSurf_R);
        if ( ! m_LUTSurf_G )
            m_LUTSurf_G = CreateBuffer(m_cmDevice, 65536 * 4);
        MFX_CHECK_NULL_PTR1(m_LUTSurf_G);
        if ( ! m_LUTSurf_B )
            m_LUTSurf_B = CreateBuffer(m_cmDevice, 65536 * 4);
        MFX_CHECK_NULL_PTR1(m_LUTSurf_B);
    }

    if (vSliceWidth > m_Params.vSliceWidth || paddedFrameHeight > m_Params.TileHeightPadded)
    {
        if (m_avgFlagSurf)
            m_cmDevice->DestroySurface(m_avgFlagSurf);
        m_avgFlagSurf = CreateSurface(m_cmDevice, vSliceWidth, paddedFrameHeight, CM_SURFACE_FORMAT_A8);
    }

    if (m_Params.Caps.OutputMemoryOperationMode == MEM_GPUSHARED)
    {
        if (m_gammaOutSurf)
            m_cmDevice->DestroySurface(m_gammaOutSurf);

        if (! m_Params.Caps.bOutToARGB16)
            m_gammaOutSurf = CreateSurface(m_cmDevice, frameSizeExtra.frameWidth,  frameSizeExtra.TileHeight, CM_SURFACE_FORMAT_A8R8G8B8);
        else
            m_gammaOutSurf = CreateSurface(m_cmDevice, frameSizeExtra.frameWidth * sizeof(mfxU16),  frameSizeExtra.TileHeight, CM_SURFACE_FORMAT_A8R8G8B8);
    }
    else if (m_gammaOutSurf)
        m_cmDevice->DestroySurface(m_gammaOutSurf);


    if (paddedFrameWidth > m_Params.paddedFrameWidth || paddedFrameHeight > m_Params.TileHeightPadded)
    {
        if (m_paddedSurf)
            m_cmDevice->DestroySurface(m_paddedSurf);
        m_paddedSurf = CreateSurface(m_cmDevice,  paddedFrameWidth*sizeof(mfxU16), paddedFrameHeight, CM_SURFACE_FORMAT_A8);
    }

    if (frameSizeExtra.Caps.bVignetteCorrection && frameSizeExtra.VignetteParams.pCorrectionMap )
    {

        if ( m_vignetteMaskSurf )
            m_cmDevice->DestroySurface(m_vignetteMaskSurf);
        m_vignetteMaskSurf = CreateSurface(m_cmDevice,
                                           frameSizeExtra.paddedFrameWidth,
                                           frameSizeExtra.TileHeightPadded,
                                           CM_SURFACE_FORMAT_YUY2);
        MFX_CHECK_NULL_PTR1(m_vignetteMaskSurf);
        if ( m_vignette_4x4 )
            m_cmDevice->DestroySurface(m_vignette_4x4);

        mfxU32 width_4x4 = ( frameSizeExtra.paddedFrameWidth >> 2 );
        width_4x4 = width_4x4 % 2 ? width_4x4 + 1 : width_4x4;
        mfxU32  height_4x4 = frameSizeExtra.VignetteParams.Height >> (frameSizeExtra.tileNumHor - 1) ;
        m_vignette_4x4  = CreateSurface(m_cmDevice,
                                    width_4x4,
                                    height_4x4,
                                    CM_SURFACE_FORMAT_YUY2);

        MFX_CHECK_NULL_PTR1(m_vignette_4x4);
    }

    if (frameSizeExtra.Caps.bHotPixel )
    {
        if (m_dnrSurf)
            m_cmDevice->DestroySurface(m_dnrSurf);
        m_dnrSurf = CreateSurface(m_cmDevice,  paddedFrameWidth*sizeof(mfxU16), paddedFrameHeight, CM_SURFACE_FORMAT_A8);

        if (m_correctedSurf)
            m_cmDevice->DestroySurface(m_correctedSurf);
        m_correctedSurf = CreateSurface(m_cmDevice,  paddedFrameWidth*sizeof(mfxU16), paddedFrameHeight, CM_SURFACE_FORMAT_A8);
    }

    if (m_Params.Caps.InputMemoryOperationMode == MEM_GPUSHARED)
    {
        if (m_cmSurfIn)
            m_cmDevice->DestroySurface(m_cmSurfIn);
        int width  = m_Params.Caps.bNoPadding ? paddedFrameWidth  : frameSizeExtra.frameWidth;
        int height = frameSizeExtra.TileHeight;

        CAMERA_DEBUG_LOG("ReallocateInternalSurfaces:  m_cmSurfIn width %d height %d \n", width, height);
        m_cmSurfIn = CreateSurface(m_cmDevice, width * sizeof(mfxU16),  height, CM_SURFACE_FORMAT_A8);
    }
    else if (m_cmSurfIn)
    {
        m_cmDevice->DestroySurface(m_cmSurfIn);
    }

    return sts;
}

namespace MfxCameraPlugin
{

CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    mfxU32 versionPlaceholder = 0;
    if (version == 0)
        version = &versionPlaceholder;

    CmDevice * device = 0;

    int result = CM_SUCCESS;
    if (core->GetVAType() == MFX_HW_D3D9)
    {
#if defined(_WIN32) || defined(_WIN64)
        D3D9Interface * d3dIface = QueryCoreInterface<D3D9Interface>(core, MFXICORED3D_GUID);
        if (d3dIface == 0)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface->GetD3D9DeviceManager(),CM_DEVICE_CREATE_OPTION_SCRATCH_SPACE_DISABLE)) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (core->GetVAType() == MFX_HW_D3D11)
    {
#if defined(_WIN32) || defined(_WIN64)
        D3D11Interface * d3dIface = QueryCoreInterface<D3D11Interface>(core, MFXICORED3D11_GUID);
        if (d3dIface == 0)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface->GetD3D11Device(),CM_DEVICE_CREATE_OPTION_SCRATCH_SPACE_DISABLE)) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (core->GetVAType() == MFX_HW_VAAPI)
    {
        //throw std::logic_error("GetDeviceManager not implemented on Linux for Look Ahead");
#if defined(MFX_VA_LINUX)
        VADisplay display;
        mfxStatus res = core->GetHandle(MFX_HANDLE_VA_DISPLAY, &display); // == MFX_HANDLE_RESERVED2
        if (res != MFX_ERR_NONE || !display)
            return 0;

        if ((result = ::CreateCmDevice(device, *version, display)) != CM_SUCCESS)
            return 0;
#endif
    }

    return device;
}

CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    CmDevice * device = TryCreateCmDevicePtr(core, version);
    if (device == 0)
        throw CmRuntimeError();
    return device;
}

CmDevicePtr::CmDevicePtr(CmDevice * device)
    : m_device(device)
{
}

CmDevicePtr::~CmDevicePtr()
{
    Reset(0);
}

void CmDevicePtr::Reset(CmDevice * device)
{
    if (m_device)
    {
        int result = ::DestroyCmDevice(m_device);
        result; assert(result == CM_SUCCESS);
    }
    m_device = device;
}

CmDevice * CmDevicePtr::operator -> ()
{
    return m_device;
}

CmDevicePtr & CmDevicePtr::operator = (CmDevice * device)
{
    Reset(device);
    return *this;
}

CmDevicePtr::operator CmDevice * ()
{
    return m_device;
}

CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size)
{
    CmBuffer * buffer;
    int result = device->CreateBuffer(size, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem)
{
    CmBufferUP * buffer;
    int result = device->CreateBufferUP(size, mem, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}


CmSurface2D * CreateSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && d3dSurface && (result = device->CreateSurface2D(d3dSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, ID3D11Texture2D * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && d3dSurface && (result = device->CreateSurface2D(d3dSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface2DSubresource(CmDevice * device, ID3D11Texture2D * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    mfxU32 cmSurfaceCount = 1;
    if (device && d3dSurface && (result = device->CreateSurface2DSubresource(d3dSurface, 1, &cmSurface, cmSurfaceCount)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, AbstractSurfaceHandle vaSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (vaSurface) && (result = device->CreateSurface2D(vaSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, mfxHDL nativeSurface, eMFXVAType vatype)
{
    switch (vatype)
    {
    case MFX_HW_D3D9:
        return CreateSurface(device, (IDirect3DSurface9 *)nativeSurface);
    case MFX_HW_D3D11:
        return CreateSurface2DSubresource(device, (ID3D11Texture2D *)nativeSurface);
    case MFX_HW_VAAPI:
        return CreateSurface(device, nativeSurface);
    default:
        throw CmRuntimeError();
    }
}


CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (result = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT(fourcc), cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem)
{
    int result = CM_SUCCESS;
    CmSurface2DUP * cmSurface = 0;
    if (device && (result = device->CreateSurface2DUP(width, height, CM_SURFACE_FORMAT(fourcc), mem, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmContext::CmContext()
: m_device(0)
, m_program(0)
, m_queue(0)
{
}

CmContext::CmContext(
    CameraParams const & video,
    CmDevice *          cmDevice,
    mfxCameraCaps *     pCaps,
    eMFXHWType platform)
{
    m_nTilesHor = video.tileNumHor;
    m_nTilesVer = video.tileNumVer;
    Zero(task_VignetteUpSample);
    Zero(task_BayerCorrection);
    Zero(task_Padding);
    Zero(task_GoodPixelCheck);
    Zero(task_RestoreGreen);
    Zero(task_RestoreBlueRed);
    Zero(task_SAD);
    Zero(task_DecideAvg);
//  Zero(task_CheckConfidence);
//  Zero(task_BadPixelCheck);
    Zero(task_HP);
    Zero(task_Denoise);
    Zero(task_3x3CCM);
    Zero(task_FwGamma);
    Zero(task_ARGB);
    kernel_ARGB     = 0;
    kernel_FwGamma  = 0;
    kernel_FwGamma1 = 0;
    kernel_BayerCorrection = 0;
    kernel_VignetteUpSample = 0;

    m_DenoiseDWM = new unsigned short[25];
    m_DenoisePW  = new unsigned short[6];
    m_DenoiseRT  = new unsigned short[6];
    m_platform = platform;
    Setup(video, cmDevice, pCaps);
}

void CmContext::CreateCameraKernels()
{
    int i;
    bool bNeedConversionToRGB = true;

    if (! m_caps.bNoPadding)
    {
        if ( BAYER_GRBG == m_caps.BayerPatternType || BAYER_GBRG == m_caps.BayerPatternType )
        {
            kernel_padding16bpp = CreateKernel(m_device, m_program, (CM_KERNEL_FUNCTION(PaddingandFlipping_16bpp)), NULL);
        }
        else
        {
            kernel_padding16bpp = CreateKernel(m_device, m_program, (CM_KERNEL_FUNCTION(Padding_16bpp)), NULL); // padding to be done
        }
    }

    if (m_caps.bHotPixel && m_caps.bBayerDenoise)
    {
        kernel_hotpixel = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(HotPixel_DYN_SKL), NULL);
    }
    else if (! m_caps.bHotPixel && m_caps.bBayerDenoise)
    {
        kernel_hotpixel = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(DYNRANGE_CALC_SKL), NULL);
    }
    else if (m_caps.bHotPixel && ! m_caps.bBayerDenoise)
    {
        kernel_hotpixel = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(HotPixel_SKL), NULL);
    }


    if (m_caps.bBayerDenoise )
    {
        kernel_denoise = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(BayerDenoise), NULL);
    }

    if (m_caps.bBlackLevelCorrection || m_caps.bWhiteBalance || m_caps.bVignetteCorrection)
    {
        // BlackLevel correction, White balance and vignette filter share the same kernel.
        kernel_BayerCorrection = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(BAYER_CORRECTION), NULL);
    }

    if ( m_caps.bVignetteCorrection )
    {
         kernel_VignetteUpSample = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GenUpSampledMask), NULL);
    }

    kernel_good_pixel_check = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GOOD_PIXEL_CHECK), NULL);

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(RESTOREG), NULL);
    }

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(RESTOREBandR), NULL);
    }

    if (m_video.BitDepth != 16)
    {
        for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
        {
            CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD), NULL);
        }
    }
    else
    {
        for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
        {
            CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD_16), NULL);
        }
    }

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(DECIDE_AVG), NULL);
    }


    if (m_caps.bForwardGammaCorrection)
    {
        if ( m_caps.bColorConversionMatrix )
        {
            if ( m_caps.bGamma3DLUT )
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_3D), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_3D), NULL);
            }
            else
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
            }
        }
        else
        {
            if ( m_caps.bGamma3DLUT )
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_3D_ONLY_ARGB_OUT), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_3D_ONLY_ARGB_OUT), NULL);
            }
            else
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_ARGB_OUT), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_ARGB_OUT), NULL);
            }
        }

        // Gamma related kernels may create output.
        bNeedConversionToRGB = false;
    }

#if 0
    if (m_caps.bForwardGammaCorrection && !m_caps.bColorConversionMatrix)
    {
        // Only gamma correction needed w/o color corection

        if (m_video.BitDepth == 16)
        {
            kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_16bits), NULL);
            // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
            kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_16bits), NULL);
        }
        else
        {
            if (m_caps.bOutToARGB16)
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY), NULL);
            }
            else
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);

                // This kernel does a conversion, so separate conversion is not needed.
                bNeedConversionToRGB = false;
            }
        }
    }
    else if (m_caps.bForwardGammaCorrection && m_caps.bColorConversionMatrix)
    {
        // Both gamma correction and color corection needed

        if (m_video.BitDepth == 16)
        {
            kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_16bits), NULL);
            // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
            kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_16bits), NULL);
        }
        else
        {
            kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
            // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
            kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
        }
    }
    else if (! m_caps.bForwardGammaCorrection && m_caps.bColorConversionMatrix)
    {
        // color corection needed w/o gamma correction
        kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_ONLY), NULL);
        // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
        kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_ONLY), NULL);

    }
#endif

    if ( bNeedConversionToRGB )
    {
        if (m_caps.bOutToARGB16)
        {
            kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB16), NULL);
        }
        else
        {
            kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB8), NULL);
        }
    }
}

void CmContext::CreateThreadSpaces(CameraParams *pFrameSize)
{
    mfxU32 sliceWidth = pFrameSize->vSliceWidth;
    int    result = CM_SUCCESS;

    heightIn16 = (pFrameSize->TileHeightPadded + 15) >> 4;
    widthIn16  = (pFrameSize->paddedFrameWidth  + 15) >> 4;

    sliceHeightIn16_np = (pFrameSize->TileHeight + 15) >> 4;
    sliceWidthIn16_np  = (sliceWidth - 16) >> 4;

    sliceHeightIn8 = (pFrameSize->TileHeightPadded + 7) >> 3;
    sliceWidthIn8  = sliceWidth >> 3;   // slicewidth is a multiple of 16

    sliceHeightIn8_np = (pFrameSize->TileHeight + 7) >> 3;
    sliceWidthIn8_np  = (sliceWidth - 16) >> 3;

    int vignetteMaskWidth = pFrameSize->paddedFrameWidth / (8/2 * 4);

    if ((result = m_device->CreateThreadSpace(vignetteMaskWidth, 1, TS_VignetteUpSample)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(widthIn16, heightIn16, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(sliceWidthIn16_np, sliceHeightIn16_np, TS_Slice_16x16_np)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(sliceWidthIn8, sliceHeightIn8, TS_Slice_8x8)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(sliceWidthIn8_np, sliceHeightIn8_np, TS_Slice_8x8_np)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::CopyMfxSurfToCmSurf(CmSurface2D *cmSurf, mfxFrameSurface1* mfxSurf) //, gpucopy/cpucopy ??? only for gpucopy so far)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;
    if ((result = m_queue->EnqueueCopyCPUToGPU(cmSurf, (const unsigned char *)mfxSurf->Data.Y16, event_transfer)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::CopyMemToCmSurf(CmSurface2D *cmSurf, void *mem) //, gpucopy/cpucopy ??? only for gpucopy so far)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;
    if ((result = m_queue->EnqueueCopyCPUToGPU(cmSurf, (const unsigned char *)mem, event_transfer)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueCopyCPUToGPU(CmSurface2D *cmSurf, void *mem, mfxU32 stride)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;

    if (stride > 0)
        result = m_queue->EnqueueCopyCPUToGPUStride(cmSurf, (unsigned char *)mem, (unsigned int)stride, event_transfer);
    else
        result = m_queue->EnqueueCopyCPUToGPU(cmSurf, (unsigned char *)mem, event_transfer);

    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    return event_transfer;
}

CmEvent *CmContext::EnqueueCopyGPUToCPU(CmSurface2D *cmSurf, void *mem, mfxU32 stride)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;

    if (stride > 0)
        result = m_queue->EnqueueCopyGPUToCPUStride(cmSurf, (unsigned char *)mem, (unsigned int)stride, event_transfer);
    else
        result = m_queue->EnqueueCopyGPUToCPU(cmSurf, (unsigned char *)mem, event_transfer);

    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    return event_transfer;
}

void CmContext::CreateTask(CmTask *&task)
{
    int result;
    if ((result = m_device->CreateTask(task)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::DestroyTask(CmTask *&task)
{
    int result;
    if ((result = m_device->DestroyTask(task)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::CreateCmTasks()
{
    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++)
    {
        if (!m_caps.bNoPadding)
            CreateTask(CAM_PIPE_TASK_ARRAY(task_Padding, i));

        if ( m_caps.bBlackLevelCorrection || m_caps.bWhiteBalance || m_caps.bVignetteCorrection )
            CreateTask(CAM_PIPE_TASK_ARRAY(task_BayerCorrection, i));

        if ( m_caps.bVignetteCorrection )
            CreateTask(CAM_PIPE_TASK_ARRAY(task_VignetteUpSample, i));

        if ( m_caps.bHotPixel ||  m_caps.bBayerDenoise)
            CreateTask(CAM_PIPE_TASK_ARRAY(task_HP, i));

        if ( m_caps.bBayerDenoise )
            CreateTask(CAM_PIPE_TASK_ARRAY(task_Denoise, i));

        CreateTask(CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i));

        for (int j = 0; j < CAM_PIPE_KERNEL_SPLIT; j++)
        {
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i), j));
        }

        if (m_caps.bForwardGammaCorrection)
        {
            CreateTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));
        }
        else
        {
            CreateTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));
        }
    }

    CAMERA_DEBUG_LOG("CreateCmTasks end device %p task_ARGB=%p \n", m_device, task_ARGB);
}

void CmContext::DestroyCmTasks()
{
    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++)
    {
        if (CAM_PIPE_TASK_ARRAY(task_Padding, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_Padding, i));

        if (CAM_PIPE_TASK_ARRAY(task_BayerCorrection, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_BayerCorrection, i));

        if (CAM_PIPE_TASK_ARRAY(task_HP, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_HP, i));

        if (CAM_PIPE_TASK_ARRAY(task_Denoise, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_Denoise, i));

        if (CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i));

        for (int j = 0; j < CAM_PIPE_KERNEL_SPLIT; j++)
        {
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j));
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j));
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j));
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i), j));
        }

        if (CAM_PIPE_TASK_ARRAY(task_FwGamma, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));

        if (CAM_PIPE_TASK_ARRAY(task_ARGB, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));
    }


    Zero(task_Padding);
    Zero(task_BayerCorrection);
    Zero(task_GoodPixelCheck);
    Zero(task_RestoreGreen);
    Zero(task_RestoreBlueRed);
    Zero(task_SAD);
    Zero(task_DecideAvg);
    Zero(task_FwGamma);
    Zero(task_ARGB);
    Zero(task_HP);
    Zero(task_Denoise);
}

void CmContext::CreateTask_Padding(SurfaceIndex inSurfIndex,
                                   SurfaceIndex paddedSurfIndex,
                                   mfxU32 paddedWidth,
                                   mfxU32 paddedHeight,
                                   int bitDepth,
                                   int bayerPattern,
                                   mfxU32)
{
    int result;
    int threadswidth  = paddedWidth  >> 3;
    int threadsheight = paddedHeight >> 3;
    // Use crops for non-tiled cases
    int width  = m_video.TileInfo.CropW < m_video.frameWidth ? m_video.TileInfo.CropW : m_video.frameWidth;
    int height = m_video.TileInfo.CropH < m_video.TileHeight ? m_video.TileInfo.CropH : m_video.TileHeight;

    kernel_padding16bpp->SetThreadCount(widthIn16 * heightIn16);
    SetKernelArg(kernel_padding16bpp, inSurfIndex, paddedSurfIndex, threadswidth, threadsheight, width, height, bitDepth, bayerPattern);

    task_Padding->Reset();

    if ((result = task_Padding->AddKernel(kernel_padding16bpp)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_Padding()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_Padding, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}


#define BLOCK_WIDTH 8
void CmContext::CreateTask_VignetteMaskUpSample(SurfaceIndex mask_4x4_Index,
                                                SurfaceIndex vignetteMaskIndex)
{
    int result;
    int i = 0;

    unsigned int num_threads_w = m_video.paddedFrameWidth / (BLOCK_WIDTH / 2 * 4);
    kernel_VignetteUpSample->SetThreadCount(num_threads_w * 1);

    kernel_VignetteUpSample->SetKernelArg(i++, sizeof(SurfaceIndex), &mask_4x4_Index);
    kernel_VignetteUpSample->SetKernelArg(i++, sizeof(SurfaceIndex), &vignetteMaskIndex);
    kernel_VignetteUpSample->SetKernelArg(i++, sizeof(UINT32), &m_video.paddedFrameWidth);
    kernel_VignetteUpSample->SetKernelArg(i++, sizeof(UINT32), &m_video.TileHeightPadded);

    result = task_VignetteUpSample->Reset();
    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    result = task_VignetteUpSample->AddKernel(kernel_VignetteUpSample);
    if (result != CM_SUCCESS )
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_VignetteMaskUpSample()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_VignetteUpSample, e, TS_VignetteUpSample)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::CreateTask_BayerCorrection(int first,
                                           SurfaceIndex PaddedSurfIndex,
                                           SurfaceIndex inoutSurfIndex,
                                           SurfaceIndex vignetteMaskIndex,
                                           mfxU16 Enable_BLC,
                                           mfxU16 Enable_VIG,
                                           mfxU16 ENABLE_WB,
                                           short B_shift,
                                           short Gtop_shift,
                                           short Gbot_shift,
                                           short R_shift,
                                           mfxF32 R_scale,
                                           mfxF32 Gtop_scale,
                                           mfxF32 Gbot_scale,
                                           mfxF32 B_scale,
                                           int bitDepth,
                                           int BayerType,
                                           mfxU32)
{
    int result;
    mfxU16 MaxInputLevel = (1<<bitDepth)-1;
    kernel_BayerCorrection->SetThreadCount(widthIn16 * heightIn16);
    int i=0;

    signed short  _B_shift;
    signed short  _Gtop_shift;
    signed short  _Gbot_shift;
    signed short  _R_shift;

    float  _B_scale;
    float  _Gtop_scale;
    float  _Gbot_scale;
    float  _R_scale;

    switch( BayerType)
    {
    case BAYER_BGGR:
        _B_shift    = B_shift;
        _Gtop_shift = Gtop_shift;
        _Gbot_shift = Gbot_shift;
        _R_shift    = R_shift;
        _B_scale    = B_scale;
        _Gtop_scale = Gtop_scale;
        _Gbot_scale = Gbot_scale;
        _R_scale    = R_scale;
        break;
    case BAYER_RGGB:
        _B_shift    = R_shift;
        _Gtop_shift = Gtop_shift;
        _Gbot_shift = Gbot_shift;
        _R_shift    = B_shift;
        _B_scale    = R_scale;
        _Gtop_scale = Gtop_scale;
        _Gbot_scale = Gbot_scale;
        _R_scale    = B_scale;
        break;
    case BAYER_GRBG:
        _B_shift    = B_shift;
        _Gtop_shift = Gtop_shift;
        _Gbot_shift = Gbot_shift;
        _R_shift    = R_shift;
        _B_scale    = B_scale;
        _Gtop_scale = Gtop_scale;
        _Gbot_scale = Gbot_scale;
        _R_scale    = R_scale;
        break;
    case BAYER_GBRG:
        _B_shift    = R_shift;
        _Gtop_shift = Gtop_shift;
        _Gbot_shift = Gbot_shift;
        _R_shift    = B_shift;
        _B_scale    = R_scale;
        _Gtop_scale = Gtop_scale;
        _Gbot_scale = Gbot_scale;
        _R_scale    = B_scale;
        break;
    }

    kernel_BayerCorrection->SetKernelArg( i++, sizeof(SurfaceIndex), &PaddedSurfIndex   );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(SurfaceIndex), &inoutSurfIndex   );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(SurfaceIndex), &vignetteMaskIndex);
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &Enable_BLC       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &Enable_VIG       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &ENABLE_WB        );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &_B_shift          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &_Gtop_shift       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &_Gbot_shift       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &_R_shift          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &_B_scale          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &_Gtop_scale       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &_Gbot_scale       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &_R_scale          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &MaxInputLevel    );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(int),          &first            );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(int),          &bitDepth         );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(int),          &BayerType        );

    result = task_BayerCorrection->Reset();
    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    result = task_BayerCorrection->AddKernel(kernel_BayerCorrection);
    if (result != CM_SUCCESS )
        throw CmRuntimeError();
}

// Bayer denosie definitions (delivered from SKL driver internal)
#define INTERP(x0, x1, x, y0, y1)   ((unsigned short) floor(y0+(x-x0)*(y1-y0)/(double)(x1-x0)))
#define NOISEFACTOR_MIN 0
#define NOISEFACTOR_MID 32
#define NOISEFACTOR_MAX 64
// Pixel Range Threshold Array Denoise Definitions for SKL+ 5x5 Bilateral Filter
#define NOISE_BLF_RANGE_THRESHOLD_S0_MIN            32
#define NOISE_BLF_RANGE_THRESHOLD_S0_MID            192
#define NOISE_BLF_RANGE_THRESHOLD_S0_MAX            384
#define NOISE_BLF_RANGE_THRESHOLD_S1_MIN            64
#define NOISE_BLF_RANGE_THRESHOLD_S1_MID            256
#define NOISE_BLF_RANGE_THRESHOLD_S1_MAX            576
#define NOISE_BLF_RANGE_THRESHOLD_S2_MIN            128
#define NOISE_BLF_RANGE_THRESHOLD_S2_MID            512
#define NOISE_BLF_RANGE_THRESHOLD_S2_MAX            896
#define NOISE_BLF_RANGE_THRESHOLD_S3_MIN            128
#define NOISE_BLF_RANGE_THRESHOLD_S3_MID            640
#define NOISE_BLF_RANGE_THRESHOLD_S3_MAX            1280
#define NOISE_BLF_RANGE_THRESHOLD_S4_MIN            128
#define NOISE_BLF_RANGE_THRESHOLD_S4_MID            896
#define NOISE_BLF_RANGE_THRESHOLD_S4_MAX            1920
#define NOISE_BLF_RANGE_THRESHOLD_S5_MIN            128
#define NOISE_BLF_RANGE_THRESHOLD_S5_MID            1280
#define NOISE_BLF_RANGE_THRESHOLD_S5_MAX            2560

// Pixel Range Weight Array Denoise Definitions for SKL+ 5x5 Bilateral Filter
#define NOISE_BLF_RANGE_WGTS0_MIN                   16
#define NOISE_BLF_RANGE_WGTS0_MID                   16
#define NOISE_BLF_RANGE_WGTS0_MAX                   16
#define NOISE_BLF_RANGE_WGTS1_MIN                   9
#define NOISE_BLF_RANGE_WGTS1_MID                   14
#define NOISE_BLF_RANGE_WGTS1_MAX                   15
#define NOISE_BLF_RANGE_WGTS2_MIN                   2
#define NOISE_BLF_RANGE_WGTS2_MID                   10
#define NOISE_BLF_RANGE_WGTS2_MAX                   13
#define NOISE_BLF_RANGE_WGTS3_MIN                   0
#define NOISE_BLF_RANGE_WGTS3_MID                   5
#define NOISE_BLF_RANGE_WGTS3_MAX                   10
#define NOISE_BLF_RANGE_WGTS4_MIN                   0
#define NOISE_BLF_RANGE_WGTS4_MID                   2
#define NOISE_BLF_RANGE_WGTS4_MAX                   7
#define NOISE_BLF_RANGE_WGTS5_MIN                   0
#define NOISE_BLF_RANGE_WGTS5_MID                   1
#define NOISE_BLF_RANGE_WGTS5_MAX                   4

// Distance Weight Matrix Denoise Definitions for SKL+ 5x5 Bilateral Filter
#define NOISE_BLF_DISTANCE_WGTS00_DEFAULT           12
#define NOISE_BLF_DISTANCE_WGTS01_DEFAULT           12
#define NOISE_BLF_DISTANCE_WGTS02_DEFAULT           10
#define NOISE_BLF_DISTANCE_WGTS10_DEFAULT           12
#define NOISE_BLF_DISTANCE_WGTS11_DEFAULT           11
#define NOISE_BLF_DISTANCE_WGTS12_DEFAULT           10
#define NOISE_BLF_DISTANCE_WGTS20_DEFAULT           10
#define NOISE_BLF_DISTANCE_WGTS21_DEFAULT           10
#define NOISE_BLF_DISTANCE_WGTS22_DEFAULT           8
void CmContext::CreateTask_Denoise(int first,
                            SurfaceIndex InPaddedIndex,
                            SurfaceIndex OutPaddedIndex,
                            SurfaceIndex DNRIndex,
                            mfxU16 Threshold,
                            int bitDepth,
                            int BayerType,
                            mfxU32)
{
    int result;
    mfxU16 MaxInputLevel = (1<<bitDepth)-1;
    kernel_denoise->SetThreadCount(widthIn16 * heightIn16);
    int i=0;
    bool cui_range = (Threshold < NOISEFACTOR_MID);

    unsigned short  dn_thmax = 2048;
    unsigned short  dn_thmin = 512 ;

    unsigned short m_5x5BLF_DistWgts00 = NOISE_BLF_DISTANCE_WGTS00_DEFAULT;
    unsigned short m_5x5BLF_DistWgts01 = NOISE_BLF_DISTANCE_WGTS01_DEFAULT;
    unsigned short m_5x5BLF_DistWgts02 = NOISE_BLF_DISTANCE_WGTS02_DEFAULT;
    unsigned short m_5x5BLF_DistWgts10 = NOISE_BLF_DISTANCE_WGTS10_DEFAULT;
    unsigned short m_5x5BLF_DistWgts11 = NOISE_BLF_DISTANCE_WGTS11_DEFAULT;
    unsigned short m_5x5BLF_DistWgts12 = NOISE_BLF_DISTANCE_WGTS12_DEFAULT;
    unsigned short m_5x5BLF_DistWgts20 = NOISE_BLF_DISTANCE_WGTS20_DEFAULT;
    unsigned short m_5x5BLF_DistWgts21 = NOISE_BLF_DISTANCE_WGTS21_DEFAULT;
    unsigned short m_5x5BLF_DistWgts22 = NOISE_BLF_DISTANCE_WGTS22_DEFAULT;

    m_DenoiseRT[0] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_THRESHOLD_S0_MIN, NOISE_BLF_RANGE_THRESHOLD_S0_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_THRESHOLD_S0_MID, NOISE_BLF_RANGE_THRESHOLD_S0_MAX);
    m_DenoiseRT[1] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_THRESHOLD_S1_MIN, NOISE_BLF_RANGE_THRESHOLD_S1_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_THRESHOLD_S1_MID, NOISE_BLF_RANGE_THRESHOLD_S1_MAX);;
    m_DenoiseRT[2] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_THRESHOLD_S2_MIN, NOISE_BLF_RANGE_THRESHOLD_S2_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_THRESHOLD_S2_MID, NOISE_BLF_RANGE_THRESHOLD_S2_MAX);;
    m_DenoiseRT[3] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_THRESHOLD_S3_MIN, NOISE_BLF_RANGE_THRESHOLD_S3_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_THRESHOLD_S3_MID, NOISE_BLF_RANGE_THRESHOLD_S3_MAX);;
    m_DenoiseRT[4] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_THRESHOLD_S4_MIN, NOISE_BLF_RANGE_THRESHOLD_S4_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_THRESHOLD_S4_MID, NOISE_BLF_RANGE_THRESHOLD_S4_MAX);;
    m_DenoiseRT[5] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_THRESHOLD_S5_MIN, NOISE_BLF_RANGE_THRESHOLD_S5_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_THRESHOLD_S5_MID, NOISE_BLF_RANGE_THRESHOLD_S5_MAX);;

    m_DenoisePW[0] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_WGTS0_MIN, NOISE_BLF_RANGE_WGTS0_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_WGTS0_MID, NOISE_BLF_RANGE_WGTS0_MAX);
    m_DenoisePW[1] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_WGTS1_MIN, NOISE_BLF_RANGE_WGTS1_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_WGTS1_MID, NOISE_BLF_RANGE_WGTS1_MAX);;
    m_DenoisePW[2] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_WGTS2_MIN, NOISE_BLF_RANGE_WGTS2_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_WGTS2_MID, NOISE_BLF_RANGE_WGTS2_MAX);;
    m_DenoisePW[3] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_WGTS3_MIN, NOISE_BLF_RANGE_WGTS3_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_WGTS3_MID, NOISE_BLF_RANGE_WGTS3_MAX);;
    m_DenoisePW[4] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_WGTS4_MIN, NOISE_BLF_RANGE_WGTS4_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_WGTS4_MID, NOISE_BLF_RANGE_WGTS4_MAX);;
    m_DenoisePW[5] = cui_range ? INTERP(NOISEFACTOR_MIN, NOISEFACTOR_MID, Threshold, NOISE_BLF_RANGE_WGTS5_MIN, NOISE_BLF_RANGE_WGTS5_MID) :
                                 INTERP(NOISEFACTOR_MID, NOISEFACTOR_MAX, Threshold, NOISE_BLF_RANGE_WGTS5_MID, NOISE_BLF_RANGE_WGTS5_MAX);;

    m_DenoiseDWM[0]  = m_5x5BLF_DistWgts00; m_DenoiseDWM[1]  = m_5x5BLF_DistWgts01; m_DenoiseDWM[2]  = m_5x5BLF_DistWgts02; m_DenoiseDWM[3]  = m_5x5BLF_DistWgts01; m_DenoiseDWM[4]  = m_5x5BLF_DistWgts00;
    m_DenoiseDWM[5]  = m_5x5BLF_DistWgts10; m_DenoiseDWM[6]  = m_5x5BLF_DistWgts11; m_DenoiseDWM[7]  = m_5x5BLF_DistWgts12; m_DenoiseDWM[8]  = m_5x5BLF_DistWgts11; m_DenoiseDWM[9]  = m_5x5BLF_DistWgts10;
    m_DenoiseDWM[10] = m_5x5BLF_DistWgts20; m_DenoiseDWM[11] = m_5x5BLF_DistWgts21; m_DenoiseDWM[12] = m_5x5BLF_DistWgts22; m_DenoiseDWM[13] = m_5x5BLF_DistWgts21; m_DenoiseDWM[14] = m_5x5BLF_DistWgts20;
    m_DenoiseDWM[15] = m_5x5BLF_DistWgts10; m_DenoiseDWM[16] = m_5x5BLF_DistWgts11; m_DenoiseDWM[17] = m_5x5BLF_DistWgts12; m_DenoiseDWM[18] = m_5x5BLF_DistWgts11; m_DenoiseDWM[19] = m_5x5BLF_DistWgts10;
    m_DenoiseDWM[20] = m_5x5BLF_DistWgts00; m_DenoiseDWM[21] = m_5x5BLF_DistWgts01; m_DenoiseDWM[22] = m_5x5BLF_DistWgts02; m_DenoiseDWM[23] = m_5x5BLF_DistWgts01; m_DenoiseDWM[24] = m_5x5BLF_DistWgts00;

    kernel_denoise->SetKernelArg( i++, sizeof(SurfaceIndex), &InPaddedIndex    );
    kernel_denoise->SetKernelArg( i++, sizeof(SurfaceIndex), &OutPaddedIndex   );
    kernel_denoise->SetKernelArg( i++, sizeof(SurfaceIndex), &DNRIndex         );
    kernel_denoise->SetKernelArg( i++, 25 * sizeof(unsigned short), m_DenoiseDWM);
    kernel_denoise->SetKernelArg( i++, 6 * sizeof(unsigned short), m_DenoiseRT);
    kernel_denoise->SetKernelArg( i++, 6 * sizeof(unsigned short), m_DenoisePW);
    kernel_denoise->SetKernelArg( i++, sizeof(unsigned short), &dn_thmax);
    kernel_denoise->SetKernelArg( i++, sizeof(unsigned short), &dn_thmin);
    kernel_denoise->SetKernelArg( i++, sizeof(int),          &MaxInputLevel    );
    kernel_denoise->SetKernelArg( i++, sizeof(int),          &first            );
    kernel_denoise->SetKernelArg( i++, sizeof(int),          &bitDepth         );
    kernel_denoise->SetKernelArg( i++, sizeof(int),          &BayerType        );

    result = task_Denoise->Reset();
    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    result = task_Denoise->AddKernel(kernel_denoise);
    if (result != CM_SUCCESS )
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_Denoise()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_Denoise, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::CreateTask_HotPixel(int first,
                                    SurfaceIndex InPaddedIndex,
                                    SurfaceIndex OutPaddedIndex,
                                    SurfaceIndex DNRIndex,
                                    mfxU16 ThreshPixelDiff,
                                    mfxU16 ThreshNumPix,
                                    int bitDepth,
                                    int BayerType,
                                    mfxU32)
{
    int result;
    mfxU16 MaxInputLevel = (1<<bitDepth)-1;
    kernel_hotpixel->SetThreadCount(widthIn16 * heightIn16);
    int i=0;

    kernel_hotpixel->SetKernelArg( i++, sizeof(SurfaceIndex), &InPaddedIndex    );
    kernel_hotpixel->SetKernelArg( i++, sizeof(SurfaceIndex), &OutPaddedIndex   );
    kernel_hotpixel->SetKernelArg( i++, sizeof(SurfaceIndex), &DNRIndex         );
    kernel_hotpixel->SetKernelArg( i++, sizeof(int),          &ThreshPixelDiff  );
    kernel_hotpixel->SetKernelArg( i++, sizeof(int),          &ThreshNumPix     );
    kernel_hotpixel->SetKernelArg( i++, sizeof(int),          &MaxInputLevel    );
    kernel_hotpixel->SetKernelArg( i++, sizeof(int),          &first            );
    kernel_hotpixel->SetKernelArg( i++, sizeof(int),          &bitDepth         );
    kernel_hotpixel->SetKernelArg( i++, sizeof(int),          &BayerType        );

    result = task_HP->Reset();
    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    result = task_HP->AddKernel(kernel_hotpixel);
    if (result != CM_SUCCESS )
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_HP()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_HP, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

CmEvent *CmContext::EnqueueTask_BayerCorrection()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_BayerCorrection, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::CreateTask_GoodPixelCheck(SurfaceIndex inSurfIndex,
                                          SurfaceIndex paddedSurfIndex,
                                          CmSurface2D *goodPixCntSurf,
                                          CmSurface2D *bigPixCntSurf,
                                          int bitDepth,
                                          int doShiftSwap,
                                          int bayerPattern,
                                          mfxU32)
{
    int result;
    mfxI32 shift_amount = (bitDepth - 8);
    kernel_good_pixel_check->SetThreadCount(widthIn16 * heightIn16);

    int height = (int)m_video.TileHeight;

    SetKernelArg(kernel_good_pixel_check,
                 inSurfIndex,
                 paddedSurfIndex,
                 GetIndex(goodPixCntSurf),
                 GetIndex(bigPixCntSurf),
                 shift_amount,
                 height,
                 doShiftSwap,
                 bitDepth,
                 bayerPattern);

    task_GoodPixelCheck->Reset();

    if ((result = task_GoodPixelCheck->AddKernel(kernel_good_pixel_check)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_GoodPixelCheck()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_GoodPixelCheck, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::CreateTask_RestoreGreen(SurfaceIndex inSurfIndex,
                                        CmSurface2D *goodPixCntSurf,
                                        CmSurface2D *bigPixCntSurf,
                                        CmSurface2D *greenHorSurf,
                                        CmSurface2D *greenVerSurf,
                                        CmSurface2D *greenAvgSurf,
                                        CmSurface2D *avgFlagSurf,
                                        mfxU32 bitDepth,
                                        int bayerPattern,
                                        mfxU32)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i * sliceWidthIn8) - ((i == 0) ? 0 : i * 2);
        int ybase = 0;

        int wr_x_base = 0;
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i)->SetThreadCount(sliceWidthIn8 * sliceHeightIn8);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i),
                     inSurfIndex,
                     GetIndex(goodPixCntSurf),
                     GetIndex(bigPixCntSurf),
                     GetIndex(greenHorSurf),
                     GetIndex(greenVerSurf),
                     GetIndex(greenAvgSurf),
                     GetIndex(avgFlagSurf),
                     xbase,
                     ybase,
                     wr_x_base);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i),wr_y_base, 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i), MaxIntensity, 11);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i), bayerPattern, 12);
        CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i))) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}

void CmContext::CreateTask_RestoreBlueRed(SurfaceIndex inSurfIndex,
                                            CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf,
                                            CmSurface2D *blueHorSurf, CmSurface2D *blueVerSurf, CmSurface2D *blueAvgSurf,
                                            CmSurface2D *redHorSurf, CmSurface2D *redVerSurf, CmSurface2D *redAvgSurf,
                                            CmSurface2D *avgFlagSurf, mfxU32 bitDepth, int bayerPattern, mfxU32)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = 0;
        int ybase = 0;

        int wr_x_base = (i * sliceWidthIn8) - ((i == 0) ? 0 : i * 2);
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i)->SetThreadCount(sliceWidthIn8 * sliceHeightIn8);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i),
                     inSurfIndex,
                     GetIndex(greenHorSurf), GetIndex(greenVerSurf), GetIndex(greenAvgSurf),
                     GetIndex(blueHorSurf), GetIndex(blueVerSurf), GetIndex(blueAvgSurf),
                     GetIndex(redHorSurf),  GetIndex(redVerSurf), GetIndex(redAvgSurf));
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), GetIndex(avgFlagSurf), 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), xbase, 11);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), ybase, 12);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), wr_x_base, 13);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), wr_y_base, 14);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), MaxIntensity, 15);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), bayerPattern, 16);

        CAM_PIPE_KERNEL_ARRAY(task_RestoreBlueRed, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_RestoreBlueRed, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i))) != CM_SUCCESS)
            throw CmRuntimeError();

    }
}

void CmContext::CreateTask_SAD(CmSurface2D *redHorSurf,
                               CmSurface2D *greenHorSurf,
                               CmSurface2D *blueHorSurf,
                               CmSurface2D *redVerSurf,
                               CmSurface2D *greenVerSurf,
                               CmSurface2D *blueVerSurf,
                               CmSurface2D *redOutSurf,
                               CmSurface2D *greenOutSurf,
                               CmSurface2D *blueOutSurf,
                               int bayerPattern,
                               mfxU32)
{
    int result;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = 0;
        int ybase = 0;

        int wr_x_base = (i * sliceWidthIn8_np);// - ((i == 0)? 0 : i*2);
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_sad, i)->SetThreadCount(sliceWidthIn8_np * sliceHeightIn8_np);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i),
                     GetIndex(redHorSurf), GetIndex(greenHorSurf),  GetIndex(blueHorSurf),
                     GetIndex(redVerSurf), GetIndex(greenVerSurf),  GetIndex(blueVerSurf),
                     xbase, ybase, wr_x_base, wr_y_base);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), bayerPattern, 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(redOutSurf), 11);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(greenOutSurf), 12);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(blueOutSurf), 13);

        CAM_PIPE_KERNEL_ARRAY(task_SAD, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_SAD, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i))) != CM_SUCCESS)
            throw CmRuntimeError();

    }
}

void CmContext::CreateTask_DecideAverage(CmSurface2D *redAvgSurf,
                                         CmSurface2D *greenAvgSurf,
                                         CmSurface2D *blueAvgSurf,
                                         CmSurface2D *avgFlagSurf,
                                         CmSurface2D *redOutSurf,
                                         CmSurface2D *greenOutSurf,
                                         CmSurface2D *blueOutSurf,
                                         int bayerPattern,
                                         int bitdepth,
                                         mfxU32)
{
    int result;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {

        int wr_x_base = (i * sliceWidthIn16_np);// - ((i == 0)? 0 : i*2);;
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i)->SetThreadCount(sliceWidthIn16_np * sliceHeightIn16_np);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i),
                                           GetIndex(redAvgSurf), GetIndex(greenAvgSurf), GetIndex(blueAvgSurf),
                                           GetIndex(avgFlagSurf),
                                           GetIndex(redOutSurf), GetIndex(greenOutSurf), GetIndex(blueOutSurf),
                                           wr_x_base, wr_y_base, bayerPattern);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i), bitdepth, 10);

        CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i))) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}


#define NEW_GAMMA_THREADS

void CmContext::CreateTask_GammaAndCCM(CmSurface2D *correctSurfR,
                                       CmSurface2D *correctSurfG,
                                       CmSurface2D *correctSurfB,
                                       CmSurface2D  *pointSurf,
                                       CmSurface2D  *redSurf,
                                       CmSurface2D  *greenSurf,
                                       CmSurface2D  *blueSurf,
                                       CameraPipe3x3ColorConversionParams  *ccm,
                                       CmSurface2D  *outSurf,
                                       mfxU32       bitDepth,
                                       SurfaceIndex *LUTIndex_R,
                                       SurfaceIndex *LUTIndex_G,
                                       SurfaceIndex *LUTIndex_B,
                                       int BayerType,
                                       int argb8out,
                                       mfxU32)
{
    int result;
    int threadsheight = (( m_video.TileWidth+15)  & 0xFFFFFFF0 / 16);
    int framewidth_in_bytes = m_video.TileWidth * sizeof(int);

    kernel_FwGamma1->SetThreadCount(m_NumEuPerSubSlice * (m_MaxNumOfThreadsPerGroup / m_NumEuPerSubSlice) * 4);

    kernel_FwGamma1->SetKernelArg(0, sizeof(SurfaceIndex), &GetIndex(correctSurfR));
    kernel_FwGamma1->SetKernelArg(1, sizeof(SurfaceIndex), &GetIndex(correctSurfG));
    kernel_FwGamma1->SetKernelArg(2, sizeof(SurfaceIndex), &GetIndex(correctSurfB));
    kernel_FwGamma1->SetKernelArg(3, sizeof(SurfaceIndex), &GetIndex(pointSurf)  );
    mfxU32 height = (mfxU32)m_video.TileHeight;

    if( BayerType == BAYER_BGGR || BayerType == BAYER_GRBG )
    {
        kernel_FwGamma1->SetKernelArg(4, sizeof(SurfaceIndex), &GetIndex(redSurf)  );
        kernel_FwGamma1->SetKernelArg(5, sizeof(SurfaceIndex), &GetIndex(greenSurf));
        kernel_FwGamma1->SetKernelArg(6, sizeof(SurfaceIndex), &GetIndex(blueSurf) );
    }
    else
    {
        kernel_FwGamma1->SetKernelArg(4, sizeof(SurfaceIndex), &GetIndex(blueSurf) );
        kernel_FwGamma1->SetKernelArg(5, sizeof(SurfaceIndex), &GetIndex(greenSurf));
        kernel_FwGamma1->SetKernelArg(6, sizeof(SurfaceIndex), &GetIndex(redSurf)  );
    }

    kernel_FwGamma1->SetKernelArg(8, sizeof(int), &threadsheight      );
    kernel_FwGamma1->SetKernelArg(9, sizeof(int), &bitDepth           );
    kernel_FwGamma1->SetKernelArg(10, sizeof(int), &framewidth_in_bytes);
    kernel_FwGamma1->SetKernelArg(11, sizeof(int), &height             );

    if ( ccm )
    {
        for(int k = 0; k < 3; k++)
            for(int z = 0; z < 3; z++)
                m_ccm(k*3 + z) = (float)ccm->CCM[k][z];
        kernel_FwGamma1->SetKernelArg( 7, m_ccm.get_size_data(), m_ccm.get_addr_data());
        kernel_FwGamma1->SetKernelArg(12, sizeof(SurfaceIndex), LUTIndex_R);
        kernel_FwGamma1->SetKernelArg(13, sizeof(SurfaceIndex), LUTIndex_G);
        kernel_FwGamma1->SetKernelArg(14, sizeof(SurfaceIndex), LUTIndex_B);
        kernel_FwGamma1->SetKernelArg(15, sizeof(int),          &argb8out);
        kernel_FwGamma1->SetKernelArg(16, sizeof(SurfaceIndex), &GetIndex(outSurf));
        kernel_FwGamma1->SetKernelArg(17, sizeof(int),         &BayerType);
    }
    else
    {
        kernel_FwGamma1->SetKernelArg( 7, sizeof(SurfaceIndex), &GetIndex(outSurf));
        kernel_FwGamma1->SetKernelArg(12, sizeof(int),          &BayerType);
        kernel_FwGamma1->SetKernelArg(13, sizeof(int),          &argb8out);
        kernel_FwGamma1->SetKernelArg(14, sizeof(SurfaceIndex), LUTIndex_R);
        kernel_FwGamma1->SetKernelArg(15, sizeof(SurfaceIndex), LUTIndex_G);
        kernel_FwGamma1->SetKernelArg(16, sizeof(SurfaceIndex), LUTIndex_B);
    }

    task_FwGamma->Reset();

    if ((result = task_FwGamma->AddKernel(kernel_FwGamma1)) != CM_SUCCESS)
        throw CmRuntimeError();
}


void CmContext::CreateTask_GammaAndCCM(CmSurface2D *correctSurfR,
                                       CmSurface2D *correctSurfG,
                                       CmSurface2D *correctSurfB,
                                       CmSurface2D *pointSurf,
                                       CmSurface2D *redSurf,
                                       CmSurface2D *greenSurf,
                                       CmSurface2D *blueSurf,
                                       CameraPipe3x3ColorConversionParams *ccm,
                                       SurfaceIndex outSurfIndex,
                                       mfxU32 bitDepth,
                                       SurfaceIndex *LUTIndex_R,
                                       SurfaceIndex *LUTIndex_G,
                                       SurfaceIndex *LUTIndex_B,
                                       int BayerType,
                                       int argb8out,
                                       mfxU32)
{
    int result;
    int threadsheight = (( m_video.TileWidth+15)  & 0xFFFFFFF0 / 16); // not used in the kernel (?)
    int framewidth_in_bytes = m_video.TileWidth * sizeof(int);

    kernel_FwGamma->SetThreadCount(m_NumEuPerSubSlice * (m_MaxNumOfThreadsPerGroup / m_NumEuPerSubSlice) * 4);

    mfxU32 height = (mfxU32)m_video.TileHeight;
    kernel_FwGamma->SetKernelArg(0, sizeof(SurfaceIndex), &GetIndex(correctSurfR));
    kernel_FwGamma->SetKernelArg(1, sizeof(SurfaceIndex), &GetIndex(correctSurfG));
    kernel_FwGamma->SetKernelArg(2, sizeof(SurfaceIndex), &GetIndex(correctSurfB));
    kernel_FwGamma->SetKernelArg(3, sizeof(SurfaceIndex), &GetIndex(pointSurf)  );

    if( BayerType == BAYER_BGGR || BayerType == BAYER_GRBG )
    {
        kernel_FwGamma->SetKernelArg(4, sizeof(SurfaceIndex), &GetIndex(redSurf)  );
        kernel_FwGamma->SetKernelArg(5, sizeof(SurfaceIndex), &GetIndex(greenSurf));
        kernel_FwGamma->SetKernelArg(6, sizeof(SurfaceIndex), &GetIndex(blueSurf) );
    }
    else
    {
        kernel_FwGamma->SetKernelArg(4, sizeof(SurfaceIndex), &GetIndex(blueSurf) );
        kernel_FwGamma->SetKernelArg(5, sizeof(SurfaceIndex), &GetIndex(greenSurf));
        kernel_FwGamma->SetKernelArg(6, sizeof(SurfaceIndex), &GetIndex(redSurf)  );
    }

    kernel_FwGamma->SetKernelArg(8,  sizeof(int), &threadsheight      );
    kernel_FwGamma->SetKernelArg(9,  sizeof(int), &bitDepth           );
    kernel_FwGamma->SetKernelArg(10, sizeof(int), &framewidth_in_bytes);
    kernel_FwGamma->SetKernelArg(11, sizeof(int), &height             );

    if ( ccm )
    {
        for(int k = 0; k < 3; k++)
            for(int z = 0; z < 3; z++)
                m_ccm(k*3 + z) = (float)ccm->CCM[k][z];
        kernel_FwGamma->SetKernelArg( 7, m_ccm.get_size_data(), m_ccm.get_addr_data());
        kernel_FwGamma->SetKernelArg(12, sizeof(SurfaceIndex), LUTIndex_R);
        kernel_FwGamma->SetKernelArg(13, sizeof(SurfaceIndex), LUTIndex_G);
        kernel_FwGamma->SetKernelArg(14, sizeof(SurfaceIndex), LUTIndex_B);
        kernel_FwGamma->SetKernelArg(15, sizeof(int),          &argb8out);
        kernel_FwGamma->SetKernelArg(16, sizeof(SurfaceIndex), &outSurfIndex);
        kernel_FwGamma->SetKernelArg(17, sizeof(int),         &BayerType);
    }
    else
    {
        kernel_FwGamma->SetKernelArg( 7, sizeof(SurfaceIndex), &outSurfIndex);
        kernel_FwGamma->SetKernelArg(12, sizeof(int),         &BayerType);
        kernel_FwGamma->SetKernelArg(13, sizeof(int),          &argb8out);
        kernel_FwGamma->SetKernelArg(14, sizeof(SurfaceIndex), LUTIndex_R );
        kernel_FwGamma->SetKernelArg(15, sizeof(SurfaceIndex), LUTIndex_G );
        kernel_FwGamma->SetKernelArg(16, sizeof(SurfaceIndex), LUTIndex_B );
    }

    task_FwGamma->Reset();

    if ((result = task_FwGamma->AddKernel(kernel_FwGamma)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_ForwardGamma()
{
    int result;

    CmEvent *e = NULL;
    CmThreadGroupSpace* pTGS = NULL;
    if ((result = m_device->CreateThreadGroupSpace(m_NumEuPerSubSlice, (m_MaxNumOfThreadsPerGroup / m_NumEuPerSubSlice), 1, 4, pTGS)) != CM_SUCCESS)
    {
        throw CmRuntimeError();
    }

    if ((result = m_queue->EnqueueWithGroup(task_FwGamma, e, pTGS)) !=  CM_SUCCESS)
    {
        throw CmRuntimeError();
    }

    m_device->DestroyThreadGroupSpace(pTGS);
    return e;
}

void CmContext::CreateTask_ARGB(CmSurface2D *redSurf,
                                CmSurface2D *greenSurf,
                                CmSurface2D *blueSurf,
                                SurfaceIndex outSurfIndex,
                                CameraPipe3x3ColorConversionParams *ccm,
                                mfxU32 bitDepth,
                                int BayerType,
                                mfxU32)
{

    int result, i;
    kernel_ARGB->SetThreadCount(widthIn16 * heightIn16);
    int enable_cmm = 0;
    float ccm_values[9];
    i = 0;
    if( BayerType == BAYER_BGGR || BayerType == BAYER_GRBG )
    {
        kernel_ARGB->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(redSurf));
        kernel_ARGB->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(greenSurf));
        kernel_ARGB->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(blueSurf));
    }
    else
    {
        kernel_ARGB->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(blueSurf));
        kernel_ARGB->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(greenSurf));
        kernel_ARGB->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(redSurf));
    }

    if ( ccm )
    {
        enable_cmm = 1;
        for(int k = 0; k < 3; k++)
            for(int z = 0; z < 3; z++)
                ccm_values[k*3 + z] = (float)ccm->CCM[k][z];
    }

    kernel_ARGB->SetKernelArg(i++, sizeof(SurfaceIndex), &outSurfIndex);
    kernel_ARGB->SetKernelArg(i++, sizeof(int),          &BayerType);
    kernel_ARGB->SetKernelArg(i++, sizeof(int),          &m_video.TileHeight);
    kernel_ARGB->SetKernelArg(i++, sizeof(int),          &bitDepth);
    kernel_ARGB->SetKernelArg(i++, sizeof(int),          &enable_cmm);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[0]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[1]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[2]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[3]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[4]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[5]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[6]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[7]);
    kernel_ARGB->SetKernelArg(i++, sizeof(float),        &ccm_values[8]);

    task_ARGB->Reset();

    if ((result = task_ARGB->AddKernel(kernel_ARGB)) != CM_SUCCESS)
        throw CmRuntimeError();

}

CmEvent *CmContext::EnqueueTask_ARGB()
{
    int result;
    CmEvent *e = NULL;
    CAMERA_DEBUG_LOG("EnqueueTask_ARGB:  task_ARGB=%p\n",task_ARGB);

    if ((result = m_queue->Enqueue(task_ARGB, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::Setup(
    CameraParams const & video,
    CmDevice *            cmDevice,
    mfxCameraCaps      *pCaps)
{
    m_video  = video;
    m_device = cmDevice;
    m_caps   = *pCaps;

    if (m_device->CreateQueue(m_queue) != CM_SUCCESS)
        throw CmRuntimeError();

    if(m_platform == MFX_HW_HSW || m_platform == MFX_HW_HSW_ULT)
        m_program = ReadProgram(m_device, genx_hsw_camerapipe, SizeOf(genx_hsw_camerapipe));
    else if(m_platform == MFX_HW_BDW || m_platform == MFX_HW_CHV)
        m_program = ReadProgram(m_device, genx_bdw_camerapipe, SizeOf(genx_bdw_camerapipe));
    else if(m_platform == MFX_HW_SCL)
        m_program = ReadProgram(m_device, genx_skl_camerapipe, SizeOf(genx_skl_camerapipe));

    // Valid for HSW/BDW
    // Need to add caps checking
    m_NumEuPerSubSlice = 7;

    size_t SizeCap = 4;
    m_device->GetCaps(CAP_USER_DEFINED_THREAD_COUNT_PER_THREAD_GROUP, SizeCap, &m_MaxNumOfThreadsPerGroup);

    CreateCameraKernels();
    CreateCmTasks();
    m_tableCmRelations.clear();
    m_tableCmIndex.clear();
}

void CmContext::Reset(
    mfxVideoParam const & video,
    mfxCameraCaps      *pCaps,
    mfxCameraCaps      *pOldCaps)
{
    bool bNeedConversionToRGB = true;

    if (! task_FwGamma && ( pCaps->bForwardGammaCorrection || pCaps->bColorConversionMatrix) )
        CreateTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));

    if (!task_BayerCorrection && ( pCaps->bBlackLevelCorrection || pCaps->bWhiteBalance || pCaps->bVignetteCorrection) )
    {
        kernel_BayerCorrection = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(BAYER_CORRECTION), NULL);
        CreateTask(CAM_PIPE_TASK_ARRAY(task_BayerCorrection, i));
    }

    if ( ! kernel_FwGamma || !kernel_FwGamma ||  pCaps->bColorConversionMatrix != pOldCaps->bColorConversionMatrix || pCaps->bForwardGammaCorrection != pOldCaps->bForwardGammaCorrection)
    {
        // Create gamma and ccm related kernels/tasks if they are not created yet
        if ( pCaps->bColorConversionMatrix )
        {
            if ( pCaps->bGamma3DLUT )
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_3D), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_3D), NULL);
            }
            else
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
            }
        }
        else
        {
            if ( pCaps->bGamma3DLUT )
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_3D_ONLY_ARGB_OUT), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_3D_ONLY_ARGB_OUT), NULL);
            }
            else
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_ARGB_OUT), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_ARGB_OUT), NULL);
            }
        }

        // Gamma related kernels may create output.
        bNeedConversionToRGB = false;
    }

    if (! task_ARGB && pCaps->bOutToARGB16)
    {
        kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB16), NULL);
        CreateTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));
    }
    else if (! task_ARGB && bNeedConversionToRGB)
    {
        kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB8), NULL);
        CreateTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));
    }

    if ((video.vpp.In.BitDepthLuma | m_video.BitDepth) > 16)
    {
        int i;
        for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
        {
            if (CAM_PIPE_KERNEL_ARRAY(kernel_sad, i))
                m_device->DestroyKernel(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i));
        }

        if (video.vpp.In.BitDepthLuma != 16)
        {
            for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
            {
                CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD), NULL);
            }
        }
        else
        {
            for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
            {
                CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD_16), NULL);
            }
        }
    }

    m_caps = *pCaps;
    // Fix me
    m_video.TileInfo = video.vpp.In;

    ReleaseCmSurfaces();
}

CmSurface2D *CmContext::CreateCmSurface2D(void *pSrc)
{
    std::map<void *, CmSurface2D *>::iterator it;
    it = m_tableCmRelations.find(pSrc);
    INT cmSts = 0;

    CmSurface2D *pCmSurface2D;
    SurfaceIndex *pCmSrcIndex;

    if (m_tableCmRelations.end() == it)
    {
        cmSts = m_device->CreateSurface2D((AbstractSurfaceHandle *)pSrc, pCmSurface2D);
        if (cmSts != CM_SUCCESS)
        {
            CAMERA_DEBUG_LOG("CreateCmSurface2D:  CreateSurface2D pSrc=%p sts=%d \n", pSrc, cmSts);
            return NULL;
        }

        m_tableCmRelations.insert(std::pair<void *, CmSurface2D *>(pSrc, pCmSurface2D));
        cmSts = pCmSurface2D->GetIndex(pCmSrcIndex);
        CAMERA_DEBUG_LOG("CreateCmSurface2D:  GetIndex pCmSrcIndex %p pCmSurface2D %p pSrc %p sts=%d \n", pCmSrcIndex, pCmSurface2D, pSrc, cmSts);
        if (cmSts != CM_SUCCESS)
            return NULL;
        m_tableCmIndex.insert(std::pair<CmSurface2D *, SurfaceIndex *>(pCmSurface2D, pCmSrcIndex));
    }
    else
    {
        pCmSurface2D = it->second;
    }
    return pCmSurface2D;
}

mfxStatus CmContext::ReleaseCmSurfaces(void)
{
    std::map<void *, CmSurface2D *>::iterator itSrc;

    for (itSrc = m_tableCmRelations.begin() ; itSrc != m_tableCmRelations.end(); itSrc++)
    {
        CmSurface2D *temp = itSrc->second;
        CAMERA_DEBUG_LOG(f, "ReleaseCmSurfaces:   %p \n", temp);
        m_device->DestroySurface(temp);
    }
    m_tableCmRelations.clear();
    return MFX_ERR_NONE;
}
}


