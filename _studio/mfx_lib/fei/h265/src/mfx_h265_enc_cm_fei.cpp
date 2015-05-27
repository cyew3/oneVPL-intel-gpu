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

#undef MFX_TRACE_ENABLE

namespace H265Enc {

#define GET_INSURF(s)  (&((mfxFEIH265Surface *)(s))->sIn )
#define GET_OUTSURF(s) ( ((mfxFEIH265Surface *)(s))->sOut.bufOut )

template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
}

/* leave in same file to avoid difficulties with templates (T is arbitrary type) */
template <class T>
void GetDimensionsCmSurface2DUp(CmDevice *device, Ipp32u numElemInRow, Ipp32u numRows, CM_SURFACE_FORMAT format, mfxSurfInfoENC *surfInfo)
{
    Ipp32u numBytesInRow = numElemInRow * sizeof(T);

    if (surfInfo) {
        memset(surfInfo, 0, sizeof(mfxSurfInfoENC));

        int res = CM_SUCCESS;
        if ((res = device->GetSurface2DInfo(numBytesInRow, numRows, format, surfInfo->pitch, surfInfo->size)) != CM_SUCCESS) {
            throw CmRuntimeError();
        }
        surfInfo->align = 0x1000;     // 4K aligned

        /* save these in opaque data - needed later for call to CreateCmSurface2DUp() */
        surfInfo->numBytesInRow = numBytesInRow;
        surfInfo->numRows = numRows;
    }
}

/* okay to pass NULL surfaceCpu (just skips it) */
template <class T>
void CreateCmSurface2DUpPreAlloc(CmDevice *device, Ipp32u numBytesInRow, Ipp32u numRows, CM_SURFACE_FORMAT format, T *&surfaceCpu, CmSurface2DUP *& surfaceGpu)
{
    if (surfaceCpu) {
        surfaceGpu = CreateSurface(device, numBytesInRow, numRows, format, surfaceCpu);
    }
}

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

/* destroy CM surface but do not free system memory (allocated by calling app) */
void DestroyCmSurface2DUpPreAlloc(CmDevice *device, CmSurface2DUP *surfaceGpu)
{
    if (surfaceGpu)
    {
        device->DestroySurface2DUP(surfaceGpu);
    }
}

void DestroyCmBufferUp(CmDevice *device, void *bufferCpu, CmBufferUP *bufferGpu)
{
    device->DestroyBufferUP(bufferGpu);
    CM_ALIGNED_FREE(bufferCpu);
}

static mfxStatus GetSurfaceDimensions(CmDevice *device, mfxU32 width, mfxU32 height, mfxExtFEIH265Alloc *extAlloc)
{
    Ipp32s cmMvW[MFX_FEI_H265_BLK_MAX], cmMvH[MFX_FEI_H265_BLK_MAX];

    mfxU32 k;
    mfxU32 width32;
    mfxU32 height32;
    mfxU32 width2x;
    mfxU32 height2x;
    mfxU32 interpWidth;
    mfxU32 interpHeight;

    /* pad and align to 16 pixels */
    width32  = AlignValue(width, 32);
    height32 = AlignValue(height, 32);
    width    = AlignValue(width, 16);
    height   = AlignValue(height, 16);

    width2x  = AlignValue(width / 2, 16);
    height2x = AlignValue(height / 2, 16);

    /* dimensions of output MV grid for each block size (width/height were aligned to multiple of 16) */
    cmMvW[MFX_FEI_H265_BLK_16x16   ] = width / 16;      cmMvH[MFX_FEI_H265_BLK_16x16   ] = height / 16;
    cmMvW[MFX_FEI_H265_BLK_16x8    ] = width / 16;      cmMvH[MFX_FEI_H265_BLK_16x8    ] = height /  8;
    cmMvW[MFX_FEI_H265_BLK_8x16    ] = width /  8;      cmMvH[MFX_FEI_H265_BLK_8x16    ] = height / 16;
    cmMvW[MFX_FEI_H265_BLK_8x8     ] = width /  8;      cmMvH[MFX_FEI_H265_BLK_8x8     ] = height /  8;
    cmMvW[MFX_FEI_H265_BLK_32x32   ] = width2x / 16;    cmMvH[MFX_FEI_H265_BLK_32x32   ] = height2x / 16;
    cmMvW[MFX_FEI_H265_BLK_32x16   ] = width2x / 16;    cmMvH[MFX_FEI_H265_BLK_32x16   ] = height2x /  8;
    cmMvW[MFX_FEI_H265_BLK_16x32   ] = width2x /  8;    cmMvH[MFX_FEI_H265_BLK_16x32   ] = height2x / 16;

    /* see test_interpolate_frame.cpp */
    interpWidth  = width  + 2*MFX_FEI_H265_INTERP_BORDER;
    interpHeight = height + 2*MFX_FEI_H265_INTERP_BORDER;

    /* intra distortion */
    GetDimensionsCmSurface2DUp<mfxFEIH265IntraDist>(device, width / 16, height / 16, CM_SURFACE_FORMAT_P8, &extAlloc->IntraDist);

    /* intra modes */
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 /  4, height32 /  4, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[0]);
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 /  8, height32 /  8, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[1]);
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 / 16, height32 / 16, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[2]);
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 / 32, height32 / 32, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[3]);

    /* inter distortion */
    for (k = MFX_FEI_H265_BLK_32x32; k <=  MFX_FEI_H265_BLK_16x32; k++)
        GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 16, cmMvH[k], CM_SURFACE_FORMAT_P8, &extAlloc->InterDist[k]);
    for (k = MFX_FEI_H265_BLK_16x16; k <= MFX_FEI_H265_BLK_8x8; k++)
        GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 1,  cmMvH[k], CM_SURFACE_FORMAT_P8, &extAlloc->InterDist[k]);

    /* inter MV */
    for (k = MFX_FEI_H265_BLK_32x32; k <=  MFX_FEI_H265_BLK_8x8; k++)
        GetDimensionsCmSurface2DUp<mfxI16Pair>(device, cmMvW[k], cmMvH[k], CM_SURFACE_FORMAT_P8, &extAlloc->InterMV[k]);

    /* interpolate */
    GetDimensionsCmSurface2DUp<Ipp8u>(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[0]);
    GetDimensionsCmSurface2DUp<Ipp8u>(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[1]);
    GetDimensionsCmSurface2DUp<Ipp8u>(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[2]);

    return MFX_ERR_NONE;
}

/* allocate a single input surface (source or recon) or output surface */
void * H265CmCtx::AllocateSurface(mfxFEIH265SurfaceType surfaceType, void *sysMem, mfxSurfInfoENC *surfInfo)
{
    mfxFEIH265Surface *s;

    switch (surfaceType) {
    case MFX_FEI_H265_SURFTYPE_INPUT:
        s = new mfxFEIH265Surface();    // zero-init

        s->surfaceType = MFX_FEI_H265_SURFTYPE_INPUT;
        s->sIn.bufOrig     = CreateSurface(device, sourceWidth, sourceHeight, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown2x   = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown4x   = CreateSurface(device, width4x, height4x, CM_SURFACE_FORMAT_NV12);
        if (hmeLevel == HME_LEVEL_HIGH) {
            s->sIn.bufDown8x = CreateSurface(device,  width8x,  height8x,  CM_SURFACE_FORMAT_NV12);
            s->sIn.bufDown16x = CreateSurface(device, width16x, height16x, CM_SURFACE_FORMAT_NV12);
        }
        
        return s;
    case MFX_FEI_H265_SURFTYPE_OUTPUT:
        s = new mfxFEIH265Surface();    // zero-init

        s->surfaceType = MFX_FEI_H265_SURFTYPE_OUTPUT;
        s->sOut.sysMem = (unsigned char *)sysMem;

        CreateCmSurface2DUpPreAlloc(device, surfInfo->numBytesInRow, surfInfo->numRows, CM_SURFACE_FORMAT_P8, s->sOut.sysMem, s->sOut.bufOut);
        
        return s;
    default:
        return NULL;
    }
}

mfxStatus H265CmCtx::FreeSurface(mfxFEIH265Surface *s)
{
    switch (s->surfaceType) {
    case MFX_FEI_H265_SURFTYPE_INPUT:
        device->DestroySurface(s->sIn.bufOrig);
        device->DestroySurface(s->sIn.bufDown2x);
        device->DestroySurface(s->sIn.bufDown4x);
        if (s->sIn.bufDown8x)   device->DestroySurface(s->sIn.bufDown8x);
        if (s->sIn.bufDown16x)  device->DestroySurface(s->sIn.bufDown16x);
        break;
    case MFX_FEI_H265_SURFTYPE_OUTPUT:
        DestroyCmSurface2DUpPreAlloc(device, s->sOut.bufOut);
        break;
    default:
        return MFX_ERR_INVALID_HANDLE;
    }
        
    delete s;
    return MFX_ERR_NONE;
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
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "AllocateCmResources");

    /* init member variables */
    width         = param->Width;
    height        = param->Height;
    numRefFrames  = param->NumRefFrames;
    numIntraModes = param->NumIntraModes;
    bitDepth      = param->BitDepth;
    targetUsage   = param->TargetUsage;

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

    rectParts = (vmeMode == VME_MODE_REFINE);
#if defined (AS_H265FEI_PLUGIN) || defined (SAVE_FEI_STATE)
    rectParts = 1;  // to enable 16x32 and 32x16
#endif  // (AS_H265FEI_PLUGIN)

    sourceWidth = width;
    sourceHeight = height;

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

    interpWidth  = width  + 2*MFX_FEI_H265_INTERP_BORDER;
    interpHeight = height + 2*MFX_FEI_H265_INTERP_BORDER;
    interpBlocksW = (interpWidth + 8 - 1) / 8;
    interpBlocksH = (interpHeight + 8 - 1) / 8;

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

    if (bitDepth > 8)
        bitDepthBuffer = (mfxU8 *)CM_ALIGNED_MALLOC(width*height*2, 16);

    return MFX_ERR_NONE;
}

// TODO - update
void H265CmCtx::FreeCmResources()
{
    if (lastEventSaved != NULL && lastEventSaved != CM_NO_EVENT)
        queue->DestroyEvent(lastEventSaved);

    if (bitDepthBuffer)
        CM_ALIGNED_FREE(bitDepthBuffer);
 
    device->DestroyTask(task);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);
}

void H265CmCtx::ConvertBitDepth(void *inPtr, mfxU32 inBits, mfxU32 inPitch, void *outPtr, mfxU32 outBits)
{
    // TODO - arbitrary bit depths, add AVX2 code, eventually move to GPU
    mfxU16 *in16;
    mfxU8  *out08;
    mfxU32 i, j;
    mfxU32 rnd, shift;

    in16 = (mfxU16 *)inPtr;
    out08 = (mfxU8 *)outPtr;

    shift = (inBits - 8);
    rnd = (1 << (shift - 1));

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            out08[i*width + j] = (mfxU8)((in16[i*inPitch/2 + j] + rnd) >> shift);
        }
    }
}

/* copy new input frame to GPU (original and downsampled if needed) */
mfxStatus H265CmCtx::CopyInputFrameToGPU(CmEvent **lastEvent, mfxHDL pInSurf, void *YPlane, mfxU32 YPitch, mfxU32 YBitDepth)
{
    mfxFEIH265InputSurface *surf = GET_INSURF(pInSurf);

    if (YBitDepth > 8) {
        ConvertBitDepth(YPlane, YBitDepth, YPitch, bitDepthBuffer, 8);
        EnqueueCopyCPUToGPUStride(queue, surf->bufOrig, bitDepthBuffer, width, *lastEvent);
        //(*lastEvent)->WaitForTaskFinished(0xffffffff);  // TODO - avoid multiple threads overwriting same tmp buffer (assign multiple bufs?)
    } else if (YBitDepth == 8) {
        EnqueueCopyCPUToGPUStride(queue, surf->bufOrig, YPlane, YPitch, *lastEvent);
    }

    if (hmeLevel == HME_LEVEL_HIGH) {
        SetKernelArg(kernelDownSample4tiers, surf->bufOrig, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
        // output is by 16x16
        EnqueueKernel(device, queue, kernelDownSample4tiers, width16x / 16, height16x / 16, *lastEvent);
    } else {  // HME_LEVEL_LOW
        SetKernelArg(kernelDownSample2tiers, surf->bufOrig, surf->bufDown2x, surf->bufDown4x);
        // output is by 8x8
        EnqueueKernel(device, queue, task, kernelDownSample2tiers, width2x / 16, height2x / 16, *lastEvent);
    }

    return MFX_ERR_NONE;
}

/* common ME kernel */
void H265CmCtx::RunVmeKernel(CmEvent **lastEvent, CmSurface2DUP **dist, CmSurface2DUP **mv, mfxFEIH265InputSurface *picBufInput, mfxFEIH265InputSurface *picBufRef)
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

void * H265CmCtx::RunVme(mfxFEIH265Input *feiIn, mfxExtFEIH265Output *feiOut)
{
    mfxFEIH265Frame *frameIn;
    mfxFEIH265InputSurface *surfInSrc;
    mfxFEIH265InputSurface *surfInRef;
    CmSurface2DUP *dist[MFX_FEI_H265_BLK_MAX], *mv[MFX_FEI_H265_BLK_MAX];
    CmEvent * lastEvent;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "RunVme");

    /* load current frame */
    frameIn = &(feiIn->FEIFrameIn);
    //if (!frameIn->YPlane)
    //    return NULL;        // TODO - probably this should go away since interpolate is on recon frames only - need to check elsewhere for NULL ptr

    /* if set, don't call DestroyEvent() on the final event from previous call to ProcessFrameAsync() - leave it active for app to sync on then destroy manually */
    if (saveSyncPoint) {
        //fprintf(stderr, "skipping destroy, addr = 0x%p\n", lastEventSaved);
        lastEvent = NULL;
    } else {
        //fprintf(stderr, "running destroy,  addr = 0x%p\n", lastEventSaved);
        lastEvent = lastEventSaved;
    }

    if (feiIn->FrameType == MFX_FRAMETYPE_I)        { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  frame = I"); }
    else if (feiIn->FrameType == MFX_FRAMETYPE_P)   { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  frame = P"); }
    else if (feiIn->FrameType == MFX_FRAMETYPE_B)   { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  frame = B"); }

    /* copy frames to GPU first if needed */
    if (feiIn->FEIOp & MFX_FEI_H265_OP_GPU_COPY_SRC)
        CopyInputFrameToGPU(&lastEvent, feiIn->FEIFrameIn.surfIn, feiIn->FEIFrameIn.YPlane, feiIn->FEIFrameIn.YPitch, feiIn->FEIFrameIn.YBitDepth);

    if (feiIn->FEIOp & MFX_FEI_H265_OP_GPU_COPY_REF)
        CopyInputFrameToGPU(&lastEvent, feiIn->FEIFrameRef.surfIn, feiIn->FEIFrameRef.YPlane, feiIn->FEIFrameRef.YPitch, feiIn->FEIFrameRef.YBitDepth);

    surfInSrc = GET_INSURF(feiIn->FEIFrameIn.surfIn);
    surfInRef = GET_INSURF(feiIn->FEIFrameRef.surfIn);

    if (feiIn->FEIOp & MFX_FEI_H265_OP_INTRA_MODE) {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Gradient");
        SetKernelArg(kernelGradient, surfInSrc->bufOrig, GET_OUTSURF(feiOut->SurfIntraMode[0]), GET_OUTSURF(feiOut->SurfIntraMode[1]), GET_OUTSURF(feiOut->SurfIntraMode[2]), GET_OUTSURF(feiOut->SurfIntraMode[3]), width);
        EnqueueKernel(device, queue, task, kernelGradient, width32 / 32, height32 / 32, lastEvent);
    }

    if ((feiIn->FEIOp & MFX_FEI_H265_OP_INTRA_DIST) && (feiIn->FrameType == MFX_FRAMETYPE_P || feiIn->FrameType == MFX_FRAMETYPE_B)) {
        SurfaceIndex *refsIntra = CreateVmeSurfaceG75(device, surfInSrc->bufOrig, 0, 0, 0, 0);
        SetKernelArg(kernelMeIntra, curbe, *refsIntra, surfInSrc->bufOrig, GET_OUTSURF(feiOut->SurfIntraDist));
        EnqueueKernel(device, queue, task, kernelMeIntra, width / 16, height / 16, lastEvent);
        device->DestroyVmeSurfaceG7_5(refsIntra);
    }

    /* process 1 ref frame */
    if ((feiIn->FEIOp & (MFX_FEI_H265_OP_INTER_ME | MFX_FEI_H265_OP_INTERPOLATE)) && (feiIn->FrameType == MFX_FRAMETYPE_P || feiIn->FrameType == MFX_FRAMETYPE_B)) {
        //if ( feiIn->FEIFrameRef.YPlane )
        {
            //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "RunVmeKernel - P/B");
            if (feiIn->FEIOp & (MFX_FEI_H265_OP_INTER_ME)) {
                //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  InterME - P/B");
                //for (Ipp32s k = MFX_FEI_H265_BLK_32x32; k <= MFX_FEI_H265_BLK_8x8; k++) {
                //    dist[k] = GET_OUTSURF(feiOut->SurfInterDist[k]);
                //    mv[k]   = GET_OUTSURF(feiOut->SurfInterMV[k]);
                //}
                dist[MFX_FEI_H265_BLK_32x16] = GET_OUTSURF(feiOut->SurfInterDist[rectParts ? MFX_FEI_H265_BLK_32x16 : MFX_FEI_H265_BLK_32x32]);
                dist[MFX_FEI_H265_BLK_16x32] = GET_OUTSURF(feiOut->SurfInterDist[rectParts ? MFX_FEI_H265_BLK_16x32 : MFX_FEI_H265_BLK_32x32]);
                dist[MFX_FEI_H265_BLK_32x32] = GET_OUTSURF(feiOut->SurfInterDist[MFX_FEI_H265_BLK_32x32]);
                dist[MFX_FEI_H265_BLK_16x16] = GET_OUTSURF(feiOut->SurfInterDist[MFX_FEI_H265_BLK_16x16]);
                dist[MFX_FEI_H265_BLK_16x8]  = GET_OUTSURF(feiOut->SurfInterDist[rectParts ? MFX_FEI_H265_BLK_16x8 : MFX_FEI_H265_BLK_16x16]);
                dist[MFX_FEI_H265_BLK_8x16]  = GET_OUTSURF(feiOut->SurfInterDist[rectParts ? MFX_FEI_H265_BLK_8x16 : MFX_FEI_H265_BLK_16x16]);
                dist[MFX_FEI_H265_BLK_8x8]   = GET_OUTSURF(feiOut->SurfInterDist[MFX_FEI_H265_BLK_8x8]);
                mv[MFX_FEI_H265_BLK_32x16]   = GET_OUTSURF(feiOut->SurfInterMV[rectParts ? MFX_FEI_H265_BLK_32x16 : MFX_FEI_H265_BLK_32x32]);
                mv[MFX_FEI_H265_BLK_16x32]   = GET_OUTSURF(feiOut->SurfInterMV[rectParts ? MFX_FEI_H265_BLK_16x32 : MFX_FEI_H265_BLK_32x32]);
                mv[MFX_FEI_H265_BLK_32x32]   = GET_OUTSURF(feiOut->SurfInterMV[MFX_FEI_H265_BLK_32x32]);
                mv[MFX_FEI_H265_BLK_16x16]   = GET_OUTSURF(feiOut->SurfInterMV[MFX_FEI_H265_BLK_16x16]);
                mv[MFX_FEI_H265_BLK_16x8]    = GET_OUTSURF(feiOut->SurfInterMV[rectParts ? MFX_FEI_H265_BLK_16x8 : MFX_FEI_H265_BLK_16x16]);
                mv[MFX_FEI_H265_BLK_8x16]    = GET_OUTSURF(feiOut->SurfInterMV[rectParts ? MFX_FEI_H265_BLK_8x16 : MFX_FEI_H265_BLK_16x16]);
                mv[MFX_FEI_H265_BLK_8x8]     = GET_OUTSURF(feiOut->SurfInterMV[MFX_FEI_H265_BLK_8x8]);
                RunVmeKernel(&lastEvent, dist, mv, surfInSrc, surfInRef);
            }

            if (feiIn->FEIOp & (MFX_FEI_H265_OP_INTERPOLATE)) {
                //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  Interpolate - P/B");
                SetKernelArg(kernelInterpolateFrame, surfInRef->bufOrig, GET_OUTSURF(feiOut->SurfInterp[0]), GET_OUTSURF(feiOut->SurfInterp[1]), GET_OUTSURF(feiOut->SurfInterp[2]));
                EnqueueKernel(device, queue, task, kernelInterpolateFrame, interpBlocksW, interpBlocksH, lastEvent);
            }
        }
    }

    saveSyncPoint = feiIn->SaveSyncPoint;
    if (lastEvent)
        lastEventSaved = lastEvent;

    //fprintf(stderr, "Finish saved = %d, addr = 0x%p\n", saveSyncPoint, lastEventSaved);
    return lastEventSaved;
}

/* assume wait time = -1 means wait forever (docs not clear) 
 * plugin uses const 0xFFFFFFFF (-1 signed) since CompleteFrameH265FEIRoutine() is blocking (scheduler waits for user specified time)
 */
mfxStatus H265CmCtx::SyncCurrent(void *syncp, mfxU32 wait)
{
    CmEvent * lastEvent = (CmEvent *)syncp;

    if (!lastEvent)
        return MFX_ERR_NULL_PTR;

    //fprintf(stderr, "SyncCur:                 0x%p\n", lastEvent);

    /* wait for current frame to finish */
    CM_STATUS status = CM_STATUS_QUEUED;
    if (lastEvent->GetStatus(status) != CM_SUCCESS)
        return MFX_ERR_DEVICE_FAILED;
    if (status == CM_STATUS_FINISHED)
        return MFX_ERR_NONE;

    if (wait == 0)
        return MFX_WRN_IN_EXECUTION;

    while (lastEvent->WaitForTaskFinished(1) != CM_SUCCESS) {
        if (lastEvent->GetStatus(status) != CM_SUCCESS)
            return MFX_ERR_DEVICE_FAILED;
        if (status == CM_STATUS_FINISHED)
            break;
    }
    return MFX_ERR_NONE;
}

/* destroy sync point which was not destroyed by subsequent call to ProcessFrameAsync() - call after SyncCurrent() */
mfxStatus H265CmCtx::DestroySavedSyncPoint(void *syncp)
{
    CmEvent * lastEvent = (CmEvent *)syncp;

    if (!lastEvent)
        return MFX_ERR_NULL_PTR;

    if (lastEventSaved == lastEvent)
        lastEventSaved = NULL;

    if (lastEvent != NULL && lastEvent != CM_NO_EVENT)
        queue->DestroyEvent(lastEvent);

    return MFX_ERR_NONE;
}

/* from mfx_hevc_enc_plugin.h (not using DLL exports) */
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type

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

MSDK_PLUGIN_API(mfxStatus) H265FEI_ProcessFrameAsync(mfxFEIH265 feih265, mfxFEIH265Input *in, mfxExtFEIH265Output *out, mfxFEISyncPoint *syncp)
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

/* internal only - allow sync objects to be saved and synced on/destroyed out of order */
MSDK_PLUGIN_API(mfxStatus) H265FEI_DestroySavedSyncPoint(mfxFEIH265 feih265, mfxFEISyncPoint syncp)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_DestroySavedSyncPoint");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    return hcm->DestroySavedSyncPoint((mfxSyncPoint)syncp);
}

/* calculate dimensions for Cm output buffers (2DUp surfaces) 
 * NOTE - this assumes the device does NOT yet exist (i.e. Init() hasn't been called)
 */
MSDK_PLUGIN_API(mfxStatus) H265FEI_GetSurfaceDimensions(void *core, mfxU32 width, mfxU32 height, mfxExtFEIH265Alloc *extAlloc)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_GetSurfaceDimensions");

    if (!width || !height || !extAlloc)
        return MFX_ERR_NULL_PTR;

    CmDevice *device = CreateCmDevicePtr((VideoCORE *) core);
    if (!device)
        return MFX_ERR_UNSUPPORTED;

    GetSurfaceDimensions(device, width, height, extAlloc);

    ::DestroyCmDevice(device);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateInputSurface(mfxFEIH265 feih265, mfxHDL *pInSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateInputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pInSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_INPUT, NULL, NULL);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateOutputSurface(mfxFEIH265 feih265, mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *pOutSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateOutputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pOutSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_OUTPUT, sysMem, surfInfo);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_FreeSurface(mfxFEIH265 feih265, mfxHDL pSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_FreeSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    return hcm->FreeSurface((mfxFEIH265Surface *)pSurf);
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
