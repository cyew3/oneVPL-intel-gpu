//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

#include "mfx_h265_defs.h"
#include "genx_h265_cmcode_proto.h"
#include "genx_h265_cmcode_isa.h"
#include "genx_h265_cmcode_bdw_isa.h"

#include "mfx_h265_enc_cm_fei.h"
#include "mfx_h265_enc_cm_utils.h"

/* enable interpolation for FEI plugin */
#if defined AS_H265FEI_PLUGIN || defined SAVE_FEI_STATE
#define ENABLE_INTERP_KERNELS
#endif

namespace H265Enc {

template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
}

/* leave in same file to avoid difficulties with templates (T is arbitrary type) */
template <class T>
void CreateCmSurface2DUp(CmDevice *device, Ipp32u numElemInRow, Ipp32u numRows, CM_SURFACE_FORMAT format,
                         T *&surfaceCpu, CmSurface2DUP *& surfaceGpu, Ipp32u &pitch)
{
    Ipp32u size = 0;
    Ipp32u numBytesInRow = numElemInRow * sizeof(T);
    int res = CM_SUCCESS;
    if ((res = device->GetSurface2DInfo(numBytesInRow, numRows, format, pitch, size)) != CM_SUCCESS) {
        throw CmRuntimeError();
    }

    surfaceCpu = static_cast<T *>(CM_ALIGNED_MALLOC(size, 0x1000)); // 4K aligned
    surfaceGpu = CreateSurface(device, numBytesInRow, numRows, format, surfaceCpu);
}

template <class T>
void CreateCmBufferUp(CmDevice *device, Ipp32u numElems, T *&surfaceCpu, CmBufferUP *& surfaceGpu)
{
    Ipp32u numBytes = numElems * sizeof(T);
    surfaceCpu = static_cast<T *>(CM_ALIGNED_MALLOC(numBytes, 0x1000)); // 4K aligned
    surfaceGpu = CreateBuffer(device, numBytes, surfaceCpu);
}

void DestroyCmSurface2DUp(CmDevice *device, void *surfaceCpu, CmSurface2DUP *surfaceGpu)
{
    device->DestroySurface2DUP(surfaceGpu);
    CM_ALIGNED_FREE(surfaceCpu);
}

void DestroyCmBufferUp(CmDevice *device, void *bufferCpu, CmBufferUP *bufferGpu)
{
    device->DestroyBufferUP(bufferGpu);
    CM_ALIGNED_FREE(bufferCpu);
}

///* must correspond with public API! */
//enum UnsupportedBlockSizes {
//    FEI_16x8_US  = 7,
//    FEI_8x16_US  = 8,
//    FEI_8x4_US   = 10,
//    FEI_4x8_US   = 11,
//};

mfxStatus H265CmCtx::AllocateCmResources(mfxFEIH265Param *param, void *core)
{
    Ipp32u i, j, k;
    Ipp32s cmMvW[MFX_FEI_H265_BLK_MAX], cmMvH[MFX_FEI_H265_BLK_MAX];

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "AllocateCmResources");

    /* init member variables */
    width         = param->Width;
    height        = param->Height;
    numRefFrames  = param->NumRefFrames;
    numIntraModes = param->NumIntraModes;

    /* basic parameter checking */
    if (param->Height == 0 || param->Width == 0 || param->NumRefFrames > MFX_FEI_H265_MAX_NUM_REF_FRAMES || param->NumIntraModes != 1) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    /* 1 = square, 2 = square + symmetric motion partitions, 3 = all modes (incl. asym) */
    if (param->MaxCUSize >= 32)   {
        vmeMode = (param->MPMode > 1 ? VME_MODE_REFINE : VME_MODE_LARGE);
    } else {
        vmeMode = VME_MODE_SMALL;
    }

    /* pad and align to 16 pixels */
    width32  = AlignValue(width, 32);
    height32 = AlignValue(height, 32);
    width    = AlignValue(width, 16);
    height   = AlignValue(height, 16);
    width2x  = AlignValue(width / 2, 16);
    height2x = AlignValue(height / 2, 16);
    width4x  = AlignValue(width / 4, 16);
    height4x = AlignValue(height / 4, 16);
    width8x  = AlignValue(width / 8, 16);
    height8x = AlignValue(height / 8, 16);
    width16x = AlignValue(width / 16, 16);
    height16x = AlignValue(height / 16, 16);

    if ((width >= 720) || (height >=576))   {
        hmeLevel = HME_LEVEL_HIGH;
    } else {
        hmeLevel = HME_LEVEL_LOW;
    }

    /* internal buffer: add one for lookahead frame (assume that ref buf list gets at most 1 new frame per pass, unless others are removed) */
    numRefBufs = numRefFrames + 1;
    if (numRefBufs > MFX_FEI_H265_MAX_NUM_REF_FRAMES + 1) {
        //fprintf(stderr, "Error - numRefFrames too large\n");
        return MFX_ERR_UNSUPPORTED;
    }


    /* set up Cm operations */
    device = CreateCmDevicePtr((VideoCORE *) core);
    if (!device) {
        //fprintf(stderr, "Error - unsupported GPU\n");
        return MFX_ERR_UNSUPPORTED;
    }

    device->CreateQueue(queue);

/*
    size_t sz = 4;
    int val;
    device->GetCaps(CAP_BUFFER_COUNT, sz, &val);
    device->GetCaps(CAP_SURFACE2D_COUNT, sz, &val);
*/

    GPU_PLATFORM hwType;
    size_t size = sizeof(mfxU32);
    device->GetCaps(CAP_GPU_PLATFORM, size, &hwType);
//    eMFXHWType hwType = core->GetHWType();

    switch (hwType)
    {
#if defined (CMRT_EMU)
    case PLATFORM_INTEL_SNB:
#endif
    case PLATFORM_INTEL_HSW:
///    case MFX_HW_HSW_ULT:
        program = ReadProgram(device, genx_h265_cmcode, sizeof(genx_h265_cmcode)/sizeof(genx_h265_cmcode[0]));
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
///    case MFX_HW_CHV:
        program = ReadProgram(device, genx_h265_cmcode_bdw, sizeof(genx_h265_cmcode_bdw)/sizeof(genx_h265_cmcode_bdw[0]));
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    device->CreateTask(task);

    /* create surfaces for input (source) data, double buffer for max of one frame lookahead */
    for (i = 0; i < FEI_DEPTH; i++) {
        picBufInput[i].encOrder = -1;
        picBufInput[i].bufOrig   = CreateSurface(device, param->Width, param->Height, CM_SURFACE_FORMAT_NV12);
        picBufInput[i].bufDown2x = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
        picBufInput[i].bufDown4x = CreateSurface(device, width4x, height4x, CM_SURFACE_FORMAT_NV12);
        if (hmeLevel == HME_LEVEL_HIGH) {
            picBufInput[i].bufDown8x = CreateSurface(device, width8x, height8x, CM_SURFACE_FORMAT_NV12);
            picBufInput[i].bufDown16x = CreateSurface(device, width16x, height16x, CM_SURFACE_FORMAT_NV12);
        }

        refBufMap[i] = -1;
    }

    /* see test_interpolate_frame.cpp */
    interpWidth  = width  + 2*MFX_FEI_H265_INTERP_BORDER;
    interpHeight = height + 2*MFX_FEI_H265_INTERP_BORDER;
    interpBlocksW = (interpWidth + 8 - 1) / 8;
    interpBlocksH = (interpHeight + 8 - 1) / 8;

    /* create surfaces for reference data
     * allocate numRefFrames + 1 surfaces for max of one frame lookahead
     * (assume that ref frame list for frame N+1 has at most one new ref frame compared to list for frame N)
     * also allocate 3 surfaces for each ref frame for the half-pel interpolated frames
     */
    for (i = 0; i < numRefBufs; i++) {
        picBufRef[i].encOrder = -1;
        picBufRef[i].bufOrig = CreateSurface(device, param->Width, param->Height, CM_SURFACE_FORMAT_NV12);
        picBufRef[i].bufDown2x = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
        picBufRef[i].bufDown4x = CreateSurface(device, width4x, height4x, CM_SURFACE_FORMAT_NV12);
        if (hmeLevel == HME_LEVEL_HIGH) {
            picBufRef[i].bufDown8x = CreateSurface(device, width8x, height8x, CM_SURFACE_FORMAT_NV12);
            picBufRef[i].bufDown16x = CreateSurface(device, width16x, height16x, CM_SURFACE_FORMAT_NV12);
        }

#ifdef ENABLE_INTERP_KERNELS
        CreateCmSurface2DUp(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, um_interpolateData[i][0], picBufRef[i].bufHPelH, um_interpolatePitch);
        CreateCmSurface2DUp(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, um_interpolateData[i][1], picBufRef[i].bufHPelV, um_interpolatePitch);
        CreateCmSurface2DUp(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, um_interpolateData[i][2], picBufRef[i].bufHPelD, um_interpolatePitch);
#endif
    }

    /* dimensions of output MV grid for each block size (width/height were aligned to multiple of 16)
     * currently Cm kernels do not process non-square blocks < 16x16, but allocate surfaces anyway to avoid changing API (may be supported in future)
     */
    cmMvW[MFX_FEI_H265_BLK_16x16   ] = width / 16;      cmMvH[MFX_FEI_H265_BLK_16x16   ] = height / 16;
    cmMvW[MFX_FEI_H265_BLK_16x8    ] = width / 16;      cmMvH[MFX_FEI_H265_BLK_16x8    ] = height /  8;
    cmMvW[MFX_FEI_H265_BLK_8x16    ] = width /  8;      cmMvH[MFX_FEI_H265_BLK_8x16    ] = height / 16;
    cmMvW[MFX_FEI_H265_BLK_8x8     ] = width /  8;      cmMvH[MFX_FEI_H265_BLK_8x8     ] = height /  8;
    cmMvW[MFX_FEI_H265_BLK_8x4_US  ] = width /  8;      cmMvH[MFX_FEI_H265_BLK_8x4_US  ] = height /  4;
    cmMvW[MFX_FEI_H265_BLK_4x8_US  ] = width /  4;      cmMvH[MFX_FEI_H265_BLK_4x8_US  ] = height /  8;
    cmMvW[MFX_FEI_H265_BLK_32x32   ] = width2x / 16;    cmMvH[MFX_FEI_H265_BLK_32x32   ] = height2x / 16;
    cmMvW[MFX_FEI_H265_BLK_32x16   ] = width2x / 16;    cmMvH[MFX_FEI_H265_BLK_32x16   ] = height2x /  8;
    cmMvW[MFX_FEI_H265_BLK_16x32   ] = width2x /  8;    cmMvH[MFX_FEI_H265_BLK_16x32   ] = height2x / 16;
    cmMvW[MFX_FEI_H265_BLK_64x64   ] = width4x / 16;    cmMvH[MFX_FEI_H265_BLK_64x64   ] = height4x / 16;
    cmMvW[MFX_FEI_H265_BLK_128x128 ] = width8x / 16;    cmMvH[MFX_FEI_H265_BLK_128x128 ] = height8x / 16;
    cmMvW[MFX_FEI_H265_BLK_256x256 ] = width16x / 16;   cmMvH[MFX_FEI_H265_BLK_256x256 ] = height16x / 16;

    for (i = 0; i < FEI_DEPTH; i++) {
        CreateCmSurface2DUp(device, width / 16, height / 16, CM_SURFACE_FORMAT_P8, um_mbIntraDist[i], mbIntraDist[i], um_intraPitch);

        /* create surfaces for intra histograms (4x4 and 8x8) and distortion (16x16) */
        // note: GradientAnalisys32x32Best treats blocks 32x32
        CreateCmSurface2DUp(device, width32 /  4, height32 /  4, CM_SURFACE_FORMAT_P8,   um_mbIntraGrad4x4[i],   mbIntraGrad4x4[i],   um_pitchGrad4x4[i]);
        CreateCmSurface2DUp(device, width32 /  8, height32 /  8, CM_SURFACE_FORMAT_P8,   um_mbIntraGrad8x8[i],   mbIntraGrad8x8[i],   um_pitchGrad8x8[i]);
        CreateCmSurface2DUp(device, width32 / 16, height32 / 16, CM_SURFACE_FORMAT_P8, um_mbIntraGrad16x16[i], mbIntraGrad16x16[i], um_pitchGrad16x16[i]);
        CreateCmSurface2DUp(device, width32 / 32, height32 / 32, CM_SURFACE_FORMAT_P8, um_mbIntraGrad32x32[i], mbIntraGrad32x32[i], um_pitchGrad32x32[i]);

        /* create surfaces for inter motion vectors and distortion estimates */
        for (j = 0; j < numRefFrames; j++) {
            /* distortion estimates */
            for (k = MFX_FEI_H265_BLK_32x32; k <=  MFX_FEI_H265_BLK_16x32; k++)
                CreateCmSurface2DUp(device, cmMvW[k] * 16, cmMvH[k], CM_SURFACE_FORMAT_P8, um_distCpu[i][j][k], distGpu[i][j][k], um_pitchDist[k]);
            for (k = MFX_FEI_H265_BLK_16x16; k < MFX_FEI_H265_BLK_MAX; k++) {
                if (/*(k == MFX_FEI_H265_BLK_16x8) || (k == MFX_FEI_H265_BLK_8x16) || */(k == MFX_FEI_H265_BLK_8x4_US) || (k == MFX_FEI_H265_BLK_4x8_US))
                    continue; // others are not used in tu7 now
                CreateCmSurface2DUp(device, cmMvW[k] * 1, cmMvH[k], CM_SURFACE_FORMAT_P8, um_distCpu[i][j][k], distGpu[i][j][k], um_pitchDist[k]);
            }

            /* motion vectors */
            for (k = 0; k < MFX_FEI_H265_BLK_MAX; k++) {
                if (/*(k == MFX_FEI_H265_BLK_16x8) || (k == MFX_FEI_H265_BLK_8x16) || */(k == MFX_FEI_H265_BLK_8x4_US) || (k == MFX_FEI_H265_BLK_4x8_US))
                    continue; // others are not used in tu7 now
                if (HME_LEVEL_LOW) {
                    if ((k == MFX_FEI_H265_BLK_256x256) || (k == MFX_FEI_H265_BLK_128x128) || (k == MFX_FEI_H265_BLK_64x64))
                        continue;
                } else
                {   /* for HME_LEVEL_HIGH 16x, 8x and 4x MVs are not saved in Ime3tiers kernel now*/
                    if ((k == MFX_FEI_H265_BLK_256x256) || (k == MFX_FEI_H265_BLK_128x128) || (k == MFX_FEI_H265_BLK_64x64))
                        continue;
                }
                { // surfaces 256x256, 128x128 and 64x64 should be aligned to 256 to use Ime3tiers kernel
                    mfxU32 w0 = cmMvW[k];
                    mfxU32 h0 = cmMvH[k];
/*
                    if (k == MFX_FEI_H265_BLK_128x128) {
                        w0 = width16x * 2;
                        h0 = height16x * 2;
                    } else if (k == MFX_FEI_H265_BLK_64x64) {
                        w0 = width16x * 4;
                        h0 = height16x * 4;
                    }
*/
                    CreateCmSurface2DUp(device, w0, h0, CM_SURFACE_FORMAT_P8, um_mvCpu[i][j][k], mvGpu[i][j][k], um_pitchMv[k]);
                }
            }
        }

        /* allocate buffers to store top N angle candidates for 4x4, 8x8 intra */
        um_mbIntraModeTop4[i]  = new mfxI16[numIntraModes * (width/4)  * (height/4)];
        um_mbIntraModeTop8[i]  = new mfxI16[numIntraModes * (width/8)  * (height/8)];
        um_mbIntraModeTop16[i] = new mfxI16[numIntraModes * (width/16) * (height/16)];
        um_mbIntraModeTop32[i] = new mfxI16[numIntraModes * (width/32) * (height/32)];

        if (!um_mbIntraModeTop4[i] || !um_mbIntraModeTop8[i] || !um_mbIntraModeTop16[i] || !um_mbIntraModeTop32[i])
            return MFX_ERR_MEMORY_ALLOC;
    }

    /* create Cm kernels */
    kernelDownSample2tiers = CreateKernel(device, program, "DownSampleMB2t", (void *)DownSampleMB2t);
    kernelDownSample4tiers = CreateKernel(device, program, "DownSampleMB4t", (void *)DownSampleMB4t);
    kernelMeIntra          = CreateKernel(device, program, "MeP16_Intra", (void *)MeP16_Intra);
    kernelGradient         = CreateKernel(device, program, "AnalyzeGradient32x32Best", (void *)AnalyzeGradient32x32Best);
    kernelMe16             = CreateKernel(device, program, "MeP16_4MV", (void *)MeP16_4MV);
    kernelMe32             = CreateKernel(device, program, "MeP32_4MV", (void *)MeP32_4MV);
    kernelRefine32x32      = CreateKernel(device, program, "RefineMeP32x32", (void *)RefineMeP32x32);
    kernelRefine32x16      = CreateKernel(device, program, "RefineMeP32x16", (void *)RefineMeP32x16);
    kernelRefine16x32      = CreateKernel(device, program, "RefineMeP16x32", (void *)RefineMeP16x32);
    kernelInterpolateFrame = CreateKernel(device, program, "InterpolateFrameWithBorder", (void *)InterpolateFrame);
    kernelIme              = CreateKernel(device, program, "Ime_4MV", (void *)Ime_4MV);
    kernelIme3tiers        = CreateKernel(device, program, "Ime3tiers4MV", (void *)Ime3tiers4MV);

    /* set up VME */
    curbe       = CreateBuffer(device, sizeof(H265EncCURBEData));
    me1xControl = CreateBuffer(device, sizeof(Me2xControl));
    me2xControl = CreateBuffer(device, sizeof(Me2xControl));
    me4xControl = CreateBuffer(device, sizeof(Me2xControl));
    H265EncCURBEData curbeData = {};
    SetCurbeData(curbeData, MFX_FRAMETYPE_P, 26, width, height);

    curbeData.PictureHeightMinusOne = height / 16 - 1;
    curbeData.SliceHeightMinusOne   = height / 16 - 1;
    curbeData.PictureWidth          = width  / 16;
    curbeData.Log2MvScaleFactor     = 0;
    curbeData.Log2MbScaleFactor     = 1;
    curbeData.SubMbPartMask         = 0x7e;
    curbeData.InterSAD              = 2;
    curbeData.IntraSAD              = 2;
    curbe->WriteSurface((mfxU8 *)&curbeData, NULL);

    Me2xControl control;
    SetSearchPathSmall(&control.searchPath);    // short SP for all kernels except for kernelMe16
    control.width = (mfxU16)width;
    control.height = (mfxU16)height;
    me1xControl->WriteSurface((mfxU8 *)&control, NULL);
    control.width = (mfxU16)width2x;
    control.height = (mfxU16)height2x;
    me2xControl->WriteSurface((mfxU8 *)&control, NULL);
    control.width = (mfxU16)width4x;
    control.height = (mfxU16)height4x;
    me4xControl->WriteSurface((mfxU8 *)&control, NULL);
    if (hmeLevel == HME_LEVEL_HIGH) {
        me8xControl = CreateBuffer(device, sizeof(Me2xControl));
        me16xControl = CreateBuffer(device, sizeof(Me2xControl));
        control.width = (mfxU16)width8x;
        control.height = (mfxU16)height8x;
        me8xControl->WriteSurface((mfxU8 *)&control, NULL);
        SetSearchPath(&control.searchPath);
        control.width = (mfxU16)width16x;
        control.height = (mfxU16)height16x;
        me16xControl->WriteSurface((mfxU8 *)&control, NULL);
    }

    return MFX_ERR_NONE;
}

void H265CmCtx::FreeCmResources()
{
    Ipp32u i, j, k;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "FreeCmResources");

    device->DestroySurface(curbe);
    device->DestroySurface(me1xControl);
    device->DestroySurface(me2xControl);
    device->DestroySurface(me4xControl);
    if (hmeLevel == HME_LEVEL_HIGH) {
        device->DestroySurface(me8xControl);
        device->DestroySurface(me16xControl);
    }

    device->DestroyKernel(kernelDownSample2tiers);
    device->DestroyKernel(kernelDownSample4tiers);
    device->DestroyKernel(kernelMeIntra);
    device->DestroyKernel(kernelGradient);
    device->DestroyKernel(kernelMe16);
    device->DestroyKernel(kernelMe32);
    device->DestroyKernel(kernelRefine32x32);
    device->DestroyKernel(kernelRefine32x16);
    device->DestroyKernel(kernelRefine16x32);
    device->DestroyKernel(kernelInterpolateFrame);
    device->DestroyKernel(kernelIme);
    device->DestroyKernel(kernelIme3tiers);

    for (i = 0; i < FEI_DEPTH; i++) {
        DestroyCmSurface2DUp(device, um_mbIntraDist[i], mbIntraDist[i]);
        DestroyCmSurface2DUp(device,   um_mbIntraGrad4x4[i],   mbIntraGrad4x4[i]);
        DestroyCmSurface2DUp(device,   um_mbIntraGrad8x8[i],   mbIntraGrad8x8[i]);
        DestroyCmSurface2DUp(device, um_mbIntraGrad16x16[i], mbIntraGrad16x16[i]);
        DestroyCmSurface2DUp(device, um_mbIntraGrad32x32[i], mbIntraGrad32x32[i]);

        for (j = 0; j < numRefFrames; j++) {
            for (k = 0; k < MFX_FEI_H265_BLK_MAX; k++) {
                DestroyCmSurface2DUp(device, um_distCpu[i][j][k], distGpu[i][j][k]);
                DestroyCmSurface2DUp(device, um_mvCpu[i][j][k], mvGpu[i][j][k]);
            }
        }

        delete[] um_mbIntraModeTop4[i];
        delete[] um_mbIntraModeTop8[i];
        delete[] um_mbIntraModeTop16[i];
        delete[] um_mbIntraModeTop32[i];
    }

    for (i = 0; i < FEI_DEPTH; i++) {
        device->DestroySurface(picBufInput[i].bufOrig);
        device->DestroySurface(picBufInput[i].bufDown2x);
        device->DestroySurface(picBufInput[i].bufDown4x);
        if (hmeLevel == HME_LEVEL_HIGH) {
            device->DestroySurface(picBufInput[i].bufDown8x);
            device->DestroySurface(picBufInput[i].bufDown16x);
        }
    }

    for (i = 0; i < numRefBufs; i++) {
        device->DestroySurface(picBufRef[i].bufOrig);
        device->DestroySurface(picBufRef[i].bufDown2x);
        device->DestroySurface(picBufRef[i].bufDown4x);
        if (hmeLevel == HME_LEVEL_HIGH) {
            device->DestroySurface(picBufRef[i].bufDown8x);
            device->DestroySurface(picBufRef[i].bufDown16x);
        }

#ifdef ENABLE_INTERP_KERNELS
        DestroyCmSurface2DUp(device, um_interpolateData[i][0], picBufRef[i].bufHPelH);
        DestroyCmSurface2DUp(device, um_interpolateData[i][1], picBufRef[i].bufHPelV);
        DestroyCmSurface2DUp(device, um_interpolateData[i][2], picBufRef[i].bufHPelD);
#endif
    }

    device->DestroyTask(task);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);
}

Ipp32s H265CmCtx::GetCurIdx(int encOrder)
{
    int idx, minIdx;

    //fprintf(stderr, "Frame POC = % 3d\n", frameIn->EncOrder);
    //fprintf(stderr, "CURR: refBufMap = {% 3d, % 3d}, idx = % 2d --> ", refBufMap[0], refBufMap[1], curIdx);

    /* check if frame is already in buffer, or if buffer has empty slots (startup) */
    for (idx = 0; idx < FEI_DEPTH; idx++) {
        if (refBufMap[idx] == encOrder || refBufMap[idx] == -1) {
            refBufMap[idx] = encOrder;
            return idx;
        }
    }

    /* overwrite oldest frame */
    minIdx = 0;
    for (idx = 1; idx < FEI_DEPTH; idx++) {
        if (refBufMap[idx] < refBufMap[minIdx])
            minIdx = idx;
    }

    refBufMap[minIdx] = encOrder;
    return minIdx;
}

/* return index of input or ref buffer, if not in buffer return -1 */
Ipp32s H265CmCtx::GetGPUBuf(mfxFEIH265Input *feiIn, Ipp32s poc, Ipp32s getRef)
{
    Ipp32u i;

    if (getRef) {
        /* return reference buffer index */
        for (i = 0; i < numRefBufs; i++) {
            if (picBufRef[i].encOrder == poc)
                return i;
        }
    } else {
        /* return input buffer index */
        for (i = 0; i < 2; i++) {
///            if (picBufInput[i].picOrder == poc)
                if (picBufInput[i].encOrder == poc)
                return i;
        }
    }

    return -1;
}

/* add new input frame to buffer (original and downsampled if needed) */
Ipp32s H265CmCtx::AddGPUBufInput(mfxFEIH265Input *feiIn, mfxFEIH265Frame *frameIn, int idx)
{
///    picBufInput[idx].picOrder = frameIn->PicOrder;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "InputCpy/DS");

    picBufInput[idx].encOrder = frameIn->EncOrder;
    EnqueueCopyCPUToGPUStride(queue, picBufInput[idx].bufOrig, frameIn->YPlane, frameIn->YPitch, lastEvent[idx]);

    if (hmeLevel == HME_LEVEL_HIGH) {
        SetKernelArg(kernelDownSample4tiers, picBufInput[idx].bufOrig, picBufInput[idx].bufDown2x, picBufInput[idx].bufDown4x,
            picBufInput[idx].bufDown8x, picBufInput[idx].bufDown16x);
        // output is by 16x16
        EnqueueKernel(device, queue, kernelDownSample4tiers, width16x / 16, height16x / 16, lastEvent[idx]);
    } else {  // HME_LEVEL_LOW
        SetKernelArg(kernelDownSample2tiers, picBufInput[idx].bufOrig, picBufInput[idx].bufDown2x, picBufInput[idx].bufDown4x);
        // output is by 8x8
        EnqueueKernel(device, queue, task, kernelDownSample2tiers, width2x / 16, height2x / 16, lastEvent[idx]);
    }

/*
    if (vmeMode >= VME_MODE_LARGE) {
        SetKernelArg(kernelDownSampleSrc, picBufInput[idx].bufOrig, picBufInput[idx].bufDown2x);
        EnqueueKernel(device, queue, task, kernelDownSampleSrc, width / 16, height / 16, lastEvent[idx]);
    }
*/

    return idx;
}

/* add new reference frame to buffer (original and downsampled if needed) */
Ipp32s H265CmCtx::AddGPUBufRef(mfxFEIH265Input *feiIn, mfxFEIH265Frame *frameRef, int idx)
{
    Ipp32u i, minIdx;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "RefCpy/DS");

    /* at startup -1 means empty slot */
    for (i = 0; i < numRefBufs; i++) {
        if (picBufRef[i].encOrder == -1)
            break;
    }

    /* if no empty slots in ref buffer, replace the oldest  */
    if (i == numRefBufs) {
        minIdx = 0;
        for (i = 1; i < numRefBufs; i++) {
            if (picBufRef[i].encOrder < picBufRef[minIdx].encOrder)
                minIdx = i;
        }
        i = minIdx;
    }
    VM_ASSERT(i < numRefBufs);

    picBufRef[i].encOrder = frameRef->EncOrder;
    EnqueueCopyCPUToGPUStride(queue, picBufRef[i].bufOrig, frameRef->YPlane, frameRef->YPitch, lastEvent[idx]);

    /* DownSample reference frame for HME */
    if (hmeLevel == HME_LEVEL_HIGH) {
        SetKernelArg(kernelDownSample4tiers, picBufRef[i].bufOrig, picBufRef[i].bufDown2x, picBufRef[i].bufDown4x,
            picBufRef[i].bufDown8x, picBufRef[i].bufDown16x);
        // output is by 16x16
        EnqueueKernel(device, queue, kernelDownSample4tiers, width16x / 16, height16x / 16, lastEvent[idx]);
    } else {  // HME_LEVEL_LOW
        SetKernelArg(kernelDownSample2tiers, picBufRef[i].bufOrig, picBufRef[i].bufDown2x, picBufRef[i].bufDown4x);
        // output is by 8x8
        EnqueueKernel(device, queue, task, kernelDownSample2tiers, width2x / 16, height2x / 16, lastEvent[idx]);
    }

/*
    if (vmeMode >= VME_MODE_LARGE) {
        SetKernelArg(kernelDownSampleSrc, picBufRef[i].bufOrig, picBufRef[i].bufDown2x);
        EnqueueKernel(device, queue, task, kernelDownSampleSrc, width / 16, height / 16, lastEvent[idx]);
    }
*/

    return i;
}

/* common ME kernel */
void H265CmCtx::RunVmeKernel(CmEvent **lastEvent, CmSurface2DUP **dist, CmSurface2DUP **mv, PicBufGpu *picBufInput, PicBufGpu *picBufRef)
{
    SurfaceIndex *refs = NULL, *refs2x = NULL, *refs4x = NULL, *refs8x = NULL, *refs16x = NULL;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "RunVme/Refine");

    refs = CreateVmeSurfaceG75(device, picBufInput->bufOrig, &(picBufRef->bufOrig), 0, 1, 0);
    refs2x = CreateVmeSurfaceG75(device, picBufInput->bufDown2x, &(picBufRef->bufDown2x), 0, 1, 0);
    refs4x = CreateVmeSurfaceG75(device, picBufInput->bufDown4x, &(picBufRef->bufDown4x), 0, 1, 0);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "HME");
        if (hmeLevel == HME_LEVEL_HIGH) {
            //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Ime3tiers");
            refs8x = CreateVmeSurfaceG75(device, picBufInput->bufDown8x, &(picBufRef->bufDown8x), 0, 1, 0);
            refs16x = CreateVmeSurfaceG75(device, picBufInput->bufDown16x, &(picBufRef->bufDown16x), 0, 1, 0);
            {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Ime3tiers");
                kernelIme3tiers->SetThreadCount((width16x / 16) * (height16x / 16));
                SetKernelArg(kernelIme3tiers, me16xControl, *refs16x, *refs8x, *refs4x, mv[MFX_FEI_H265_BLK_32x32]);
                EnqueueKernelLight(device, queue, task, kernelIme3tiers, width16x / 16, height16x / 16, *lastEvent);
            }
        } else {  // HME_LEVEL_LOW
            //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Ime4x");
            SetKernelArg(kernelIme, me4xControl, *refs4x, mv[MFX_FEI_H265_BLK_32x32]);
            EnqueueKernel(device, queue, task, kernelIme, width4x / 16, height4x / 16, *lastEvent);
        }
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Me32");
        mfxI32 rectParts = (vmeMode == VME_MODE_REFINE);
#if defined (AS_H265FEI_PLUGIN) || defined (SAVE_FEI_STATE)
        rectParts = 1;  // to enable 16x32 and 32x16
#endif  // (AS_H265FEI_PLUGIN)
        SetKernelArg(kernelMe32, me2xControl, *refs2x, mv[MFX_FEI_H265_BLK_32x32], mv[MFX_FEI_H265_BLK_16x16],
            mv[MFX_FEI_H265_BLK_32x16], mv[MFX_FEI_H265_BLK_16x32], rectParts);
        EnqueueKernel(device, queue, task, kernelMe32, width2x / 16, height2x / 16, *lastEvent);
    }

    /* add 16x32, 32x16, 32x32 */
    if (vmeMode >= VME_MODE_LARGE) {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Refine32x32");
        SetKernelArg(kernelRefine32x32, dist[MFX_FEI_H265_BLK_32x32], mv[MFX_FEI_H265_BLK_32x32], picBufInput->bufOrig, picBufRef->bufOrig);
        EnqueueKernel(device, queue, task, kernelRefine32x32, width2x / 16, height2x / 16, *lastEvent);
    }

    /* refine 16x32 and 32x16 partitions (calculate distortion estimates) */
    if (vmeMode == VME_MODE_REFINE) {  /* is not used now in tu7; combi kernel (32x32+SubParts should be used for this) */
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Refine32x16");
            SetKernelArg(kernelRefine32x16, dist[MFX_FEI_H265_BLK_32x16], mv[MFX_FEI_H265_BLK_32x16], picBufInput->bufOrig, picBufRef->bufOrig);
            EnqueueKernel(device, queue, task, kernelRefine32x16, width2x / 16, height2x / 8, *lastEvent);
        }

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Refine16x32");
            SetKernelArg(kernelRefine16x32, dist[MFX_FEI_H265_BLK_16x32], mv[MFX_FEI_H265_BLK_16x32], picBufInput->bufOrig, picBufRef->bufOrig);
            EnqueueKernel(device, queue, task, kernelRefine16x32, width2x / 8, height2x / 16, *lastEvent);
        }
    }

    {
        /* always estimate 4x8, 8x4, ... 16x16 */
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Me16");
        mfxI32 rectParts = (vmeMode == VME_MODE_REFINE);
#if defined (AS_H265FEI_PLUGIN) || defined (SAVE_FEI_STATE)
        rectParts = 1;  // to enable 16x8 and 8x16
#endif  // (AS_H265FEI_PLUGIN)
        SetKernelArg(kernelMe16, me1xControl, *refs, dist[MFX_FEI_H265_BLK_16x16], dist[MFX_FEI_H265_BLK_16x8], dist[MFX_FEI_H265_BLK_8x16],
            dist[MFX_FEI_H265_BLK_8x8]/*, dist[MFX_FEI_H265_BLK_8x4_US], dist[MFX_FEI_H265_BLK_4x8_US]*/, mv[MFX_FEI_H265_BLK_16x16],
            mv[MFX_FEI_H265_BLK_16x8], mv[MFX_FEI_H265_BLK_8x16], mv[MFX_FEI_H265_BLK_8x8]/*, mv[MFX_FEI_H265_BLK_8x4_US], mv[MFX_FEI_H265_BLK_4x8_US]*/, rectParts);
        EnqueueKernel(device, queue, task, kernelMe16, width / 16, height / 16, *lastEvent);
    }

    if (hmeLevel == HME_LEVEL_HIGH) {
        device->DestroyVmeSurfaceG7_5(refs16x);
        device->DestroyVmeSurfaceG7_5(refs8x);
    }
    device->DestroyVmeSurfaceG7_5(refs4x);
    device->DestroyVmeSurfaceG7_5(refs2x);
    device->DestroyVmeSurfaceG7_5(refs);
}

mfxFEISyncPoint H265CmCtx::RunVme(mfxFEIH265Input *feiIn, mfxFEIH265Output *feiOut)
{
    Ipp32s refIdx, bufIn, bufRef;
    mfxFEIH265Frame *frameIn;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "RunVme");

    /* load current frame */
    frameIn = &(feiIn->FEIFrameIn);
    if (!frameIn->YPlane)
        return NULL;

    /* get index for set of GPU buffers */
    curIdx = GetCurIdx(frameIn->EncOrder);

    lastEvent[curIdx] = 0;

    if (feiIn->FrameType == MFX_FRAMETYPE_I)        { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  frame = I"); }
    else if (feiIn->FrameType == MFX_FRAMETYPE_P)   { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  frame = P"); }
    else if (feiIn->FrameType == MFX_FRAMETYPE_B)   { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  frame = B"); }

    /* get index of input frame, add to buffer if it's not present (first time used) */
    bufIn = 0;  // warning removal
    if (feiIn->FEIOp) {
///        bufIn = GetGPUBuf(feiIn, frameIn->PicOrder, 0);
        bufIn = GetGPUBuf(feiIn, frameIn->EncOrder, 0);
        //fprintf(stderr, "(I) poc = % 3d bufIn = % 2d  ", frameIn->PicOrder, bufIn);
        if (bufIn < 0) {
            //fprintf(stderr, " adding... ");
            bufIn = AddGPUBufInput(feiIn, frameIn, curIdx);
        }
        //fprintf(stderr, " bufIn = % 2d\n", bufIn);
    }

    if (feiIn->FEIOp & MFX_FEI_H265_OP_INTRA_MODE) {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Gradient");
        SetKernelArg(kernelGradient, picBufInput[bufIn].bufOrig, mbIntraGrad4x4[curIdx], mbIntraGrad8x8[curIdx],
            mbIntraGrad16x16[curIdx], mbIntraGrad32x32[curIdx], width);
        EnqueueKernel(device, queue, task, kernelGradient, width32 / 32, height32 / 32, lastEvent[curIdx]);
    }

    if ((feiIn->FEIOp & MFX_FEI_H265_OP_INTRA_DIST) && (feiIn->FrameType == MFX_FRAMETYPE_P || feiIn->FrameType == MFX_FRAMETYPE_B)) {
        SurfaceIndex *refsIntra = CreateVmeSurfaceG75(device, picBufInput[bufIn].bufOrig, 0, 0, 0, 0);
        SetKernelArg(kernelMeIntra, curbe, *refsIntra, picBufInput[bufIn].bufOrig, mbIntraDist[curIdx]);
        EnqueueKernel(device, queue, task, kernelMeIntra, width / 16, height / 16, lastEvent[curIdx]);
        device->DestroyVmeSurfaceG7_5(refsIntra);
    }

    /* process 1 ref frame */
    if ((feiIn->FEIOp & (MFX_FEI_H265_OP_INTER_ME | MFX_FEI_H265_OP_INTERPOLATE)) && (feiIn->FrameType == MFX_FRAMETYPE_P || feiIn->FrameType == MFX_FRAMETYPE_B)) {
        if ( feiIn->FEIFrameRef.YPlane ) {
            //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "RunVmeKernel - P/B");
            refIdx = feiIn->RefIdx;
            bufRef = GetGPUBuf(feiIn, feiIn->FEIFrameRef.EncOrder, 1);
            if (bufRef < 0)
                bufRef = AddGPUBufRef(feiIn, &feiIn->FEIFrameRef, curIdx);

            if (feiIn->FEIOp & (MFX_FEI_H265_OP_INTER_ME)) {
                //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  InterME - P/B");
                RunVmeKernel(&lastEvent[curIdx], distGpu[curIdx][refIdx], mvGpu[curIdx][refIdx], &picBufInput[bufIn], &picBufRef[bufRef]);
            }

#ifdef ENABLE_INTERP_KERNELS
            if (feiIn->FEIOp & (MFX_FEI_H265_OP_INTERPOLATE)) {
                //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  Interpolate - P/B");

                um_pInterpolateData[curIdx][refIdx][0] = um_interpolateData[bufRef][0];
                um_pInterpolateData[curIdx][refIdx][1] = um_interpolateData[bufRef][1];
                um_pInterpolateData[curIdx][refIdx][2] = um_interpolateData[bufRef][2];

                SetKernelArg(kernelInterpolateFrame, picBufRef[bufRef].bufOrig, picBufRef[bufRef].bufHPelH, picBufRef[bufRef].bufHPelV, picBufRef[bufRef].bufHPelD);
                EnqueueKernel(device, queue, task, kernelInterpolateFrame, interpBlocksW, interpBlocksH, lastEvent[curIdx]);
            }
#endif
        }
    }

    /* allocate new sync object for current frame - will be freed in SyncCurrent() */
    mfxSyncPoint syncPoint = new (_mfxSyncPoint);

    if (syncPoint) {
        /* return NULL if memory alloc fails */
        syncPoint->lastEvent = lastEvent[curIdx];
        syncPoint->feiOut = feiOut;
        syncPoint->curIdx = curIdx;
        //fprintf(stderr, "Assigning sync point - ptr = 0x%p  lastEvent = 0x%p  feiOut = 0x%p  curIdx = %d)\n", syncPoint, syncPoint->lastEvent, syncPoint->feiOut, syncPoint->curIdx);
    }

    return syncPoint;
}

/* assume wait time = -1 means wait forever (docs not clear) 
 * plugin uses const 0xFFFFFFFF (-1 signed) since CompleteFrameH265FEIRoutine() is blocking (scheduler waits for user specified time)
 */
mfxStatus H265CmCtx::SyncCurrent(mfxSyncPoint syncp, mfxU32 wait)
{
    mfxU32 j, k;

    if (!syncp)
        return MFX_ERR_NULL_PTR;

    //fprintf(stderr, "SyncCurrent              - ptr = 0x%p  lastEvent = 0x%p  feiOut = 0x%p  curIdx = %d)\n", syncp, syncp->lastEvent, syncp->feiOut, syncp->curIdx);

    /* wait for current frame to finish */
    if (syncp->lastEvent) {
        if (syncp->lastEvent->WaitForTaskFinished(wait) != CM_SUCCESS) {
            return MFX_WRN_IN_EXECUTION;
        }
        queue->DestroyEvent(syncp->lastEvent);
    }

    syncp->lastEvent = NULL;
    mfxFEIH265Output *feiOut = (mfxFEIH265Output *)(syncp->feiOut);

    /* copy output parameters from current frame for SW encoder to use */
    feiOut->PaddedWidth  = width;
    feiOut->PaddedHeight = height;

    /* scale pitch to units of mfxFEIH265IntraDist */
    feiOut->IntraPitch     = um_intraPitch / sizeof(mfxFEIH265IntraDist);
    feiOut->IntraDist      = um_mbIntraDist[syncp->curIdx];

    /* scale pitch to relevant units */
    for (k = 0; k < MFX_FEI_H265_BLK_MAX; k++) {
        feiOut->PitchDist[k] = um_pitchDist[k] / sizeof(mfxU32);
        feiOut->PitchMV[k]   = um_pitchMv[k] / sizeof(mfxI16Pair);
    }

    for (j = 0; j < numRefFrames; j++) {
        for (k = 0; k < MFX_FEI_H265_BLK_MAX; k++) {
            feiOut->Dist[j][k] = um_distCpu[syncp->curIdx][j][k];
            feiOut->MV[j][k]   = um_mvCpu[syncp->curIdx][j][k];
        }
    }

#ifdef ENABLE_INTERP_KERNELS
    feiOut->InterpolateWidth  = interpWidth;
    feiOut->InterpolateHeight = interpHeight;
    feiOut->InterpolatePitch  = um_interpolatePitch;

    for (mfxU32 i = 0; i < MFX_FEI_H265_MAX_NUM_REF_FRAMES; i++) {
        for (j = 0; j < 3; j++) {
            feiOut->Interp[i][j] = um_pInterpolateData[syncp->curIdx][i][j];
        }
    }
#endif

    feiOut->IntraMaxModes = numIntraModes;

    feiOut->IntraModes4x4   = (mfxU32 *)um_mbIntraGrad4x4[syncp->curIdx];
    feiOut->IntraModes8x8   = (mfxU32 *)um_mbIntraGrad8x8[syncp->curIdx];
    feiOut->IntraModes16x16 = (mfxU32 *)um_mbIntraGrad16x16[syncp->curIdx];
    feiOut->IntraModes32x32 = (mfxU32 *)um_mbIntraGrad32x32[syncp->curIdx];
    feiOut->IntraPitch4x4   = um_pitchGrad4x4[syncp->curIdx] / sizeof(mfxU32);
    feiOut->IntraPitch8x8   = um_pitchGrad8x8[syncp->curIdx] / sizeof(mfxU32);
    feiOut->IntraPitch16x16 = um_pitchGrad16x16[syncp->curIdx] / sizeof(mfxU32);
    feiOut->IntraPitch32x32 = um_pitchGrad32x32[syncp->curIdx] / sizeof(mfxU32);

    /* free the sync object allocated at end of RunVME() */
    delete syncp;

    return MFX_ERR_NONE;
}

/* from mfx_hevc_enc_plugin.h (only enable if DLL exports are needed) */
#if 0 && (defined(_WIN32) || defined(_WIN64))
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif

/* use C wrapper to support codecs which are C only */
MSDK_PLUGIN_API(mfxStatus) H265FEI_Init(mfxFEIH265 *feih265, mfxFEIH265Param *param, void *core)
{
    mfxStatus err;
    H265CmCtx *hcm = new H265CmCtx;

    /* set up Cm state */
    err = hcm->AllocateCmResources(param, core);
    if (err) {
        delete hcm;
        return err;
    }

    /* return handle and in/out pointers */
    *feih265 = (mfxFEIH265)hcm;

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_Close(mfxFEIH265 feih265)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    hcm->FreeCmResources();

    delete hcm;

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_ProcessFrameAsync(mfxFEIH265 feih265, mfxFEIH265Input *in, mfxFEIH265Output *out, mfxFEISyncPoint *syncp)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *syncp = (mfxFEISyncPoint)hcm->RunVme(in, out);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_SyncOperation(mfxFEIH265 feih265, mfxFEISyncPoint syncp, mfxU32 wait)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_SyncOperation");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    return hcm->SyncCurrent((mfxSyncPoint)syncp, wait);
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
