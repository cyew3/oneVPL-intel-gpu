//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

#include "genx_hevce_prepare_src_hsw_isa.h"
#include "genx_hevce_prepare_src_bdw_isa.h"
#include "genx_hevce_prepare_src_skl_isa.h"
#include "genx_hevce_hme_and_me_p32_4mv_hsw_isa.h"
#include "genx_hevce_hme_and_me_p32_4mv_bdw_isa.h"
#include "genx_hevce_hme_and_me_p32_4mv_skl_isa.h"
#include "genx_hevce_intra_avc_hsw_isa.h"
#include "genx_hevce_intra_avc_bdw_isa.h"
#include "genx_hevce_intra_avc_skl_isa.h"
#include "genx_hevce_interpolate_frame_hsw_isa.h"
#include "genx_hevce_interpolate_frame_bdw_isa.h"
#include "genx_hevce_interpolate_frame_skl_isa.h"
#include "genx_hevce_me_p16_4mv_and_refine_32x32_hsw_isa.h"
#include "genx_hevce_me_p16_4mv_and_refine_32x32_bdw_isa.h"
#include "genx_hevce_me_p16_4mv_and_refine_32x32_skl_isa.h"
#include "genx_hevce_birefine_bdw_isa.h"
#include "genx_hevce_birefine_hsw_isa.h"
#include "genx_hevce_birefine_skl_isa.h"
#include "genx_hevce_refine_me_p_64x64_hsw_isa.h"
#include "genx_hevce_refine_me_p_64x64_bdw_isa.h"
#include "genx_hevce_refine_me_p_64x64_skl_isa.h"
#include "genx_hevce_refine_me_p_16x32_hsw_isa.h"
#include "genx_hevce_refine_me_p_16x32_bdw_isa.h"
#include "genx_hevce_refine_me_p_16x32_skl_isa.h"
#include "genx_hevce_refine_me_p_32x16_hsw_isa.h"
#include "genx_hevce_refine_me_p_32x16_bdw_isa.h"
#include "genx_hevce_refine_me_p_32x16_skl_isa.h"
#include "genx_hevce_refine_me_p_32x32_sad_hsw_isa.h"
#include "genx_hevce_refine_me_p_32x32_sad_bdw_isa.h"
#include "genx_hevce_refine_me_p_32x32_sad_skl_isa.h"
#include "genx_hevce_analyze_gradient_32x32_best_hsw_isa.h"
#include "genx_hevce_analyze_gradient_32x32_best_bdw_isa.h"
#include "genx_hevce_analyze_gradient_32x32_best_skl_isa.h"
#include "genx_hevce_deblock_hsw_isa.h"
#include "genx_hevce_deblock_bdw_isa.h"
#include "genx_hevce_deblock_skl_isa.h"
#include "genx_hevce_sao_hsw_isa.h"
#include "genx_hevce_sao_bdw_isa.h"
#include "genx_hevce_sao_skl_isa.h"

#include "mfx_h265_enc_cm_fei.h"
#include "mfx_h265_enc_cm_utils.h"

#undef MFX_TRACE_ENABLE

namespace H265Enc {

#define GET_INSURF(s)  (&((mfxFEIH265Surface *)(s))->sIn )
#define GET_RECSURF(s) (&((mfxFEIH265Surface *)(s))->sRec )
#define GET_UPSURF(s)  (&((mfxFEIH265Surface *)(s))->sUp )
#define GET_OUTSURF(s) ( ((mfxFEIH265Surface *)(s))->sOut.bufOut )
#define GET_OUTBUF(s)  ( ((mfxFEIH265Surface *)(s))->sBuf.bufOut )

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
CmSurface2DUP *CreateCmSurface2DUpPreAlloc(CmDevice *device, Ipp32u numBytesInRow, Ipp32u numRows, CM_SURFACE_FORMAT format, T *&surfaceCpu)
{
    return (surfaceCpu) ? CreateSurface(device, numBytesInRow, numRows, format, surfaceCpu) : NULL;
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

static mfxStatus GetSurfaceDimensions(CmDevice *device, mfxFEIH265Param *param, mfxExtFEIH265Alloc *extAlloc)
{
    Ipp32s cmMvW[MFX_FEI_H265_BLK_MAX], cmMvH[MFX_FEI_H265_BLK_MAX];

    mfxU32 k;
    mfxU32 width32;
    mfxU32 height32;
    mfxU32 width2x;
    mfxU32 height2x;
    mfxU32 width4x;
    mfxU32 height4x;
    mfxU32 interpWidth;
    mfxU32 interpHeight;

    mfxU32 width = param->Width;
    mfxU32 height = param->Height;

    /* pad and align to 16 pixels */
    width32  = AlignValue(width, 32);
    height32 = AlignValue(height, 32);
    width    = AlignValue(width, 16);
    height   = AlignValue(height, 16);

    width2x  = AlignValue(width / 2, 16);
    height2x = AlignValue(height / 2, 16);

    width4x  = AlignValue(width / 4, 16);
    height4x = AlignValue(height / 4, 16);

    /* dimensions of output MV grid for each block size (width/height were aligned to multiple of 16) */
    cmMvW[MFX_FEI_H265_BLK_16x16   ] = width / 16;      cmMvH[MFX_FEI_H265_BLK_16x16   ] = height / 16;
    cmMvW[MFX_FEI_H265_BLK_16x8    ] = width / 16;      cmMvH[MFX_FEI_H265_BLK_16x8    ] = height /  8;
    cmMvW[MFX_FEI_H265_BLK_8x16    ] = width /  8;      cmMvH[MFX_FEI_H265_BLK_8x16    ] = height / 16;
    cmMvW[MFX_FEI_H265_BLK_8x8     ] = width /  8;      cmMvH[MFX_FEI_H265_BLK_8x8     ] = height /  8;
    cmMvW[MFX_FEI_H265_BLK_32x32   ] = width2x / 16;    cmMvH[MFX_FEI_H265_BLK_32x32   ] = height2x / 16;
    cmMvW[MFX_FEI_H265_BLK_32x16   ] = width2x / 16;    cmMvH[MFX_FEI_H265_BLK_32x16   ] = height2x /  8;
    cmMvW[MFX_FEI_H265_BLK_16x32   ] = width2x /  8;    cmMvH[MFX_FEI_H265_BLK_16x32   ] = height2x / 16;
    cmMvW[MFX_FEI_H265_BLK_64x64   ] = width4x / 16;    cmMvH[MFX_FEI_H265_BLK_64x64   ] = height4x / 16;

    /* see test_interpolate_frame.cpp */
    interpWidth  = width/*  + 2*MFX_FEI_H265_INTERP_BORDER*/;
    interpHeight = height/* + 2*MFX_FEI_H265_INTERP_BORDER*/;

    /* intra distortion */
    GetDimensionsCmSurface2DUp<mfxFEIH265IntraDist>(device, width / 16, height / 16, CM_SURFACE_FORMAT_P8, &extAlloc->IntraDist);

    /* intra modes */
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 /  4, height32 /  4, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[0]);
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 /  8, height32 /  8, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[1]);
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 / 16, height32 / 16, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[2]);
    GetDimensionsCmSurface2DUp<mfxI32>(device, width32 / 32, height32 / 32, CM_SURFACE_FORMAT_P8, &extAlloc->IntraMode[3]);

    /* inter MV and distortion */
    for (k = MFX_FEI_H265_BLK_64x64; k <=  MFX_FEI_H265_BLK_32x32/*MFX_FEI_H265_BLK_16x32*/; k++) // 1 MV and 9 SADs per block
        GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 16, cmMvH[k], CM_SURFACE_FORMAT_P8, &extAlloc->InterData[k]);
    for (k = MFX_FEI_H265_BLK_16x16; k <= MFX_FEI_H265_BLK_8x8; k++) // 1 MV and 1 SAD per block
        GetDimensionsCmSurface2DUp<mfxU32>(device, cmMvW[k] * 2,  cmMvH[k], CM_SURFACE_FORMAT_P8, &extAlloc->InterData[k]);

    /* interpolate suefaces if output is needed */
    //GetDimensionsCmSurface2DUp<Ipp8u>(device, interpWidth * 2, interpHeight * 2, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[0]);
    //GetDimensionsCmSurface2DUp<Ipp8u>(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[1]);
    //GetDimensionsCmSurface2DUp<Ipp8u>(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[2]);

    /* bi-refine */
    GetDimensionsCmSurface2DUp<mfxFEIH265BirefData>(device, cmMvW[MFX_FEI_H265_BLK_32x32], cmMvH[MFX_FEI_H265_BLK_32x32],
        CM_SURFACE_FORMAT_P8, &extAlloc->BirefData[MFX_FEI_H265_BLK_32x32]);
    GetDimensionsCmSurface2DUp<mfxFEIH265BirefData>(device, cmMvW[MFX_FEI_H265_BLK_64x64], cmMvH[MFX_FEI_H265_BLK_64x64],
        CM_SURFACE_FORMAT_P8, &extAlloc->BirefData[MFX_FEI_H265_BLK_64x64]);

    /* source and reconstructed reference frames */
    Ipp32s bppShift = (param->FourCC == MFX_FOURCC_P010 || param->FourCC == MFX_FOURCC_P210) ? 1 : 0;
    CM_SURFACE_FORMAT format = (param->FourCC == MFX_FOURCC_P010 || param->FourCC == MFX_FOURCC_P210) ? CM_SURFACE_FORMAT_V8U8 : CM_SURFACE_FORMAT_P8;
    Ipp32s minPitchInBytes = AlignValue(AlignValue(param->Padding<<bppShift, 64) + (param->Width<<bppShift) + (param->Padding<<bppShift), 64);
    if ((minPitchInBytes & (minPitchInBytes - 1)) == 0)
        minPitchInBytes += 64;
    GetDimensionsCmSurface2DUp<Ipp8u>(device, minPitchInBytes>>bppShift, param->Height, format, &extAlloc->SrcRefLuma);

    minPitchInBytes = AlignValue(AlignValue(param->PaddingChroma<<bppShift, 64) + (param->WidthChroma<<bppShift) + (param->PaddingChroma<<bppShift), 64);
    if ((minPitchInBytes & (minPitchInBytes - 1)) == 0)
        minPitchInBytes += 64;
    GetDimensionsCmSurface2DUp<Ipp8u>(device, minPitchInBytes>>bppShift, param->HeightChroma, format, &extAlloc->SrcRefChroma);

    return MFX_ERR_NONE;
}

/* allocate a single input surface (source or recon) or output surface */
void * H265CmCtx::AllocateSurface(mfxFEIH265SurfaceType surfaceType, void *sysMem1, void *sysMem2, mfxSurfInfoENC *surfInfo)
{
    mfxFEIH265Surface *s;
    Ipp32s minPitchInBytes;

    CM_SURFACE_FORMAT format = (fourcc == MFX_FOURCC_P010 || fourcc == MFX_FOURCC_P210) ? CM_SURFACE_FORMAT_V8U8 : CM_SURFACE_FORMAT_P8;
    mfxU8 *sysMemLu = (mfxU8 *)sysMem1 - AlignValue(padding<<bppShift, 64);
    mfxU8 *sysMemCh = (mfxU8 *)sysMem2 - AlignValue(paddingChroma<<bppShift, 64);

    switch (surfaceType) {
    case MFX_FEI_H265_SURFTYPE_INPUT:
        s = new mfxFEIH265Surface();    // zero-init
        s->surfaceType = MFX_FEI_H265_SURFTYPE_INPUT;
        s->sIn.bufLuma10bit = NULL; // FIXME: allocate when GPU postprocessing for REXT is ready
        s->sIn.bufChromaRext = NULL; // FIXME: allocate when GPU postprocessing for REXT is ready
        s->sIn.bufOrigNv12 = CreateSurface(device, sourceWidth, sourceHeight, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown2x   = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown4x   = CreateSurface(device, width4x, height4x, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown8x   = CreateSurface(device,  width8x,  height8x,  CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown16x  = CreateSurface(device, width16x, height16x, CM_SURFACE_FORMAT_NV12);
        s->sIn.meControl   = CreateBuffer(device, sizeof(MeControl));
        s->sIn.postprocParam = CreateBuffer(device, sizeof(PostProcParam));
        s->sIn.meControlInited = false;
        s->sIn.lambda      = 0.0;
        return s;

    case MFX_FEI_H265_SURFTYPE_RECON:
        s = new mfxFEIH265Surface();    // zero-init
        s->surfaceType = MFX_FEI_H265_SURFTYPE_RECON;
        s->sRec.bufLuma10bit = NULL;  // FIXME: allocate when GPU postprocessing for REXT is ready
        s->sRec.bufChromaRext = NULL; // FIXME: allocate when GPU postprocessing for REXT is ready
        s->sRec.bufOrigNv12 = CreateSurface(device, sourceWidth, sourceHeight, CM_SURFACE_FORMAT_NV12);
        s->sRec.bufDown2x   = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
        s->sRec.bufDown4x   = CreateSurface(device, width4x, height4x, CM_SURFACE_FORMAT_NV12);
        s->sRec.bufDown8x = CreateSurface(device,  width8x,  height8x,  CM_SURFACE_FORMAT_NV12);
        s->sRec.bufDown16x = CreateSurface(device, width16x, height16x, CM_SURFACE_FORMAT_NV12);
        if (enableInterp)
            s->sRec.bufInterpMerged = CreateSurface(device, interpWidth * 2, interpHeight * 2, CM_SURFACE_FORMAT_A8);
        return s;

    case MFX_FEI_H265_SURFTYPE_UP:
        s = new mfxFEIH265Surface();    // zero-init
        s->surfaceType = MFX_FEI_H265_SURFTYPE_UP;
        minPitchInBytes = AlignValue(AlignValue(padding<<bppShift, 64) + (width<<bppShift) + (padding<<bppShift), 64);
        if ((minPitchInBytes & (minPitchInBytes - 1)) == 0)
            minPitchInBytes += 64;
        s->sUp.luma = CreateCmSurface2DUpPreAlloc(device, minPitchInBytes>>bppShift, height, format, sysMemLu);
        s->sUp.chroma = CreateCmSurface2DUpPreAlloc(device, minPitchInBytes>>bppShift, heightChroma, format, sysMemCh);
        return s;

    case MFX_FEI_H265_SURFTYPE_OUTPUT:
        s = new mfxFEIH265Surface();    // zero-init
        s->surfaceType = MFX_FEI_H265_SURFTYPE_OUTPUT;
        s->sOut.sysMem = (unsigned char *)sysMem1;
        CreateCmSurface2DUpPreAlloc(device, surfInfo->numBytesInRow, surfInfo->numRows, CM_SURFACE_FORMAT_P8, s->sOut.sysMem, s->sOut.bufOut);
        return s;

    case MFX_FEI_H265_SURFTYPE_OUTPUT_BUFFER:
        s = new mfxFEIH265Surface();    // zero-init
        s->surfaceType = MFX_FEI_H265_SURFTYPE_OUTPUT_BUFFER;
        s->sBuf.sysMem = (unsigned char *)sysMem1;
        s->sBuf.bufOut = CreateBuffer(device, surfInfo->size, sysMem1);
        return s;

    default:
        return NULL;
    }
}

mfxStatus H265CmCtx::FreeSurface(mfxFEIH265Surface *s)
{
    switch (s->surfaceType) {
    case MFX_FEI_H265_SURFTYPE_INPUT:
        if (s->sIn.bufLuma10bit)  device->DestroySurface(s->sIn.bufLuma10bit);
        if (s->sIn.bufChromaRext) device->DestroySurface(s->sIn.bufChromaRext);
        device->DestroySurface(s->sIn.bufOrigNv12);
        device->DestroySurface(s->sIn.bufDown2x);
        device->DestroySurface(s->sIn.bufDown4x);
        if (s->sIn.bufDown8x)   device->DestroySurface(s->sIn.bufDown8x);
        if (s->sIn.bufDown16x)  device->DestroySurface(s->sIn.bufDown16x);
        device->DestroySurface(s->sIn.meControl);
        device->DestroySurface(s->sIn.postprocParam);
        break;
    case MFX_FEI_H265_SURFTYPE_RECON:
        if (s->sIn.bufLuma10bit)  device->DestroySurface(s->sIn.bufLuma10bit);
        if (s->sIn.bufChromaRext) device->DestroySurface(s->sIn.bufChromaRext);
        device->DestroySurface(s->sIn.bufOrigNv12);
        device->DestroySurface(s->sRec.bufDown2x);
        device->DestroySurface(s->sRec.bufDown4x);
        if (s->sRec.bufDown8x)   device->DestroySurface(s->sRec.bufDown8x);
        if (s->sRec.bufDown16x)  device->DestroySurface(s->sRec.bufDown16x);
        device->DestroySurface(s->sRec.bufInterpMerged);
        break;
    case MFX_FEI_H265_SURFTYPE_UP:
        device->DestroySurface2DUP(s->sUp.luma);
        device->DestroySurface2DUP(s->sUp.chroma);
        break;
    case MFX_FEI_H265_SURFTYPE_OUTPUT:
        DestroyCmSurface2DUpPreAlloc(device, s->sOut.bufOut);
        break;
    case MFX_FEI_H265_SURFTYPE_OUTPUT_BUFFER:
        device->DestroyBufferUP(s->sBuf.bufOut);
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
    width                  = param->Width;
    height                 = param->Height;
    padding                = param->Padding;   
    widthChroma            = param->WidthChroma;
    heightChroma           = param->HeightChroma;
    paddingChroma          = param->PaddingChroma;
    numRefFrames           = param->NumRefFrames;
    numIntraModes          = param->NumIntraModes;
    fourcc                 = param->FourCC;
    bppShift               = (param->FourCC == MFX_FOURCC_P010 || param->FourCC == MFX_FOURCC_P210) ? 1 : 0;
    targetUsage            = param->TargetUsage;
    enableChromaSao        = param->EnableChromaSao;
    enableInterp           = param->InterpFlag;

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

    interpWidth  = width/*  + 2*MFX_FEI_H265_INTERP_BORDER*/;
    interpHeight = height/* + 2*MFX_FEI_H265_INTERP_BORDER*/;
    interpBlocksW = (interpWidth + 8 - 1) / 8;
    interpBlocksH = (interpHeight + 8 - 1) / 8;

    /* internal buffer: add one for lookahead frame (assume that ref buf list gets at most 1 new frame per pass, unless others are removed) */
    numRefBufs = numRefFrames + 1;
    if (numRefBufs > MFX_FEI_H265_MAX_NUM_REF_FRAMES + 1) {
        //fprintf(stderr, "Error - numRefFrames too large\n");
        return MFX_ERR_UNSUPPORTED;
    }

    /* set up Cm operations */
    device = CreateCmDevicePtr((MFXCoreInterface *) core);
    if (!device) {
        //fprintf(stderr, "Error - unsupported GPU\n");
        return MFX_ERR_UNSUPPORTED;
    }

    device->CreateQueue(queue);

    GPU_PLATFORM hwType;
    size_t size = sizeof(mfxU32);
    device->GetCaps(CAP_GPU_PLATFORM, size, &hwType);
    switch (hwType)
    {
#if defined (CMRT_EMU)
    case PLATFORM_INTEL_SNB:
#endif
    case PLATFORM_INTEL_HSW:
        programGradient         = ReadProgram(device, genx_hevce_analyze_gradient_32x32_best_hsw, sizeof(genx_hevce_analyze_gradient_32x32_best_hsw));
        programPrepareSrc       = ReadProgram(device, genx_hevce_prepare_src_hsw, sizeof(genx_hevce_prepare_src_hsw));
        programHmeMe32          = ReadProgram(device, genx_hevce_hme_and_me_p32_4mv_hsw, sizeof(genx_hevce_hme_and_me_p32_4mv_hsw));
        programInterpolateFrame = ReadProgram(device, genx_hevce_interpolate_frame_hsw, sizeof(genx_hevce_interpolate_frame_hsw));
        programMeIntra          = ReadProgram(device, genx_hevce_intra_avc_hsw, sizeof(genx_hevce_intra_avc_hsw));
        programMe16Refine32x32  = ReadProgram(device, genx_hevce_me_p16_4mv_and_refine_32x32_hsw, sizeof(genx_hevce_me_p16_4mv_and_refine_32x32_hsw));
        programBiRefine         = ReadProgram(device, genx_hevce_birefine_hsw, sizeof(genx_hevce_birefine_hsw));
        programRefine64x64      = ReadProgram(device, genx_hevce_refine_me_p_64x64_hsw, sizeof(genx_hevce_refine_me_p_64x64_hsw));
        programRefine16x32      = ReadProgram(device, genx_hevce_refine_me_p_16x32_hsw, sizeof(genx_hevce_refine_me_p_16x32_hsw));
        programRefine32x16      = ReadProgram(device, genx_hevce_refine_me_p_32x16_hsw, sizeof(genx_hevce_refine_me_p_32x16_hsw));
        programRefine32x32sad   = ReadProgram(device, genx_hevce_refine_me_p_32x32_sad_hsw, sizeof(genx_hevce_refine_me_p_32x32_sad_hsw));
        programDeblock          = ReadProgram(device, genx_hevce_deblock_hsw, sizeof(genx_hevce_deblock_hsw));
        programSao              = ReadProgram(device, genx_hevce_sao_hsw, sizeof(genx_hevce_sao_hsw));
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
        programGradient         = ReadProgram(device, genx_hevce_analyze_gradient_32x32_best_bdw, sizeof(genx_hevce_analyze_gradient_32x32_best_bdw));
        programPrepareSrc       = ReadProgram(device, genx_hevce_prepare_src_bdw, sizeof(genx_hevce_prepare_src_bdw));
        programHmeMe32          = ReadProgram(device, genx_hevce_hme_and_me_p32_4mv_bdw, sizeof(genx_hevce_hme_and_me_p32_4mv_bdw));
        programInterpolateFrame = ReadProgram(device, genx_hevce_interpolate_frame_bdw, sizeof(genx_hevce_interpolate_frame_bdw));
        programMeIntra          = ReadProgram(device, genx_hevce_intra_avc_bdw, sizeof(genx_hevce_intra_avc_bdw));
        programMe16Refine32x32  = ReadProgram(device, genx_hevce_me_p16_4mv_and_refine_32x32_bdw, sizeof(genx_hevce_me_p16_4mv_and_refine_32x32_bdw));
        programBiRefine         = ReadProgram(device, genx_hevce_birefine_bdw, sizeof(genx_hevce_birefine_bdw));
        programRefine64x64      = ReadProgram(device, genx_hevce_refine_me_p_64x64_bdw, sizeof(genx_hevce_refine_me_p_64x64_bdw));
        programRefine16x32      = ReadProgram(device, genx_hevce_refine_me_p_16x32_bdw, sizeof(genx_hevce_refine_me_p_16x32_bdw));
        programRefine32x16      = ReadProgram(device, genx_hevce_refine_me_p_32x16_bdw, sizeof(genx_hevce_refine_me_p_32x16_bdw));
        programRefine32x32sad   = ReadProgram(device, genx_hevce_refine_me_p_32x32_sad_bdw, sizeof(genx_hevce_refine_me_p_32x32_sad_bdw));
        programDeblock          = ReadProgram(device, genx_hevce_deblock_bdw, sizeof(genx_hevce_deblock_bdw));
        programSao              = ReadProgram(device, genx_hevce_sao_bdw, sizeof(genx_hevce_sao_bdw));
        break;
    case PLATFORM_INTEL_SKL:
    case PLATFORM_INTEL_KBL:
        programGradient         = ReadProgram(device, genx_hevce_analyze_gradient_32x32_best_skl, sizeof(genx_hevce_analyze_gradient_32x32_best_skl));
        programPrepareSrc       = ReadProgram(device, genx_hevce_prepare_src_skl, sizeof(genx_hevce_prepare_src_skl));
        programHmeMe32          = ReadProgram(device, genx_hevce_hme_and_me_p32_4mv_skl, sizeof(genx_hevce_hme_and_me_p32_4mv_skl));
        programInterpolateFrame = ReadProgram(device, genx_hevce_interpolate_frame_skl, sizeof(genx_hevce_interpolate_frame_skl));
        programMeIntra          = ReadProgram(device, genx_hevce_intra_avc_skl, sizeof(genx_hevce_intra_avc_skl));
        programMe16Refine32x32  = ReadProgram(device, genx_hevce_me_p16_4mv_and_refine_32x32_skl, sizeof(genx_hevce_me_p16_4mv_and_refine_32x32_skl));
        programBiRefine         = ReadProgram(device, genx_hevce_birefine_skl, sizeof(genx_hevce_birefine_skl));
        programRefine64x64      = ReadProgram(device, genx_hevce_refine_me_p_64x64_skl, sizeof(genx_hevce_refine_me_p_64x64_skl));
        programRefine16x32      = ReadProgram(device, genx_hevce_refine_me_p_16x32_skl, sizeof(genx_hevce_refine_me_p_16x32_skl));
        programRefine32x16      = ReadProgram(device, genx_hevce_refine_me_p_32x16_skl, sizeof(genx_hevce_refine_me_p_32x16_skl));
        programRefine32x32sad   = ReadProgram(device, genx_hevce_refine_me_p_32x32_sad_skl, sizeof(genx_hevce_refine_me_p_32x32_sad_skl));
        programDeblock          = ReadProgram(device, genx_hevce_deblock_skl, sizeof(genx_hevce_deblock_skl));
        programSao              = ReadProgram(device, genx_hevce_sao_skl, sizeof(genx_hevce_sao_skl));
        break;
    default:
        assert(!"unsupported GPU_PLATFORM");
        return MFX_ERR_UNSUPPORTED;
    }

    /* create Cm kernels */
    kernelGradient.AddKernel(device, programGradient, "AnalyzeGradient32x32Best",
        width32 / 32, height32 / 32, CM_NONE_DEPENDENCY);

    kernelMe.AddKernel(device, programHmeMe32, "HmeMe32",
        width16x / 16, height16x / 16, CM_NONE_DEPENDENCY);
    kernelMe.AddKernel(device, programRefine64x64, "RefineMeP64x64",
        width4x / 16, height4x / 16, CM_NONE_DEPENDENCY, true);

    if (width / 16 > CM_MAX_THREADSPACE_WIDTH) {  // split into 2 parts
        mfxU32 width0 = ((width / 16) >> 1) & ~1;  // must be even for 32x32 blocks
        kernelMe.AddKernel(device, programMe16Refine32x32, "Me16AndRefine32x32",
            width0, height / 16, CM_NONE_DEPENDENCY, false);
        kernelMe.AddKernel(device, programMe16Refine32x32, "Me16AndRefine32x32",
            (width / 16) - width0, height / 16, CM_NONE_DEPENDENCY, false);
    } else {
        kernelMe.AddKernel(device, programMe16Refine32x32, "Me16AndRefine32x32",
            width / 16, height / 16, CM_NONE_DEPENDENCY, false);
    }

    // Birefine kernels (32x32 and 64x64 only)
    kernelBiRefine.AddKernel(device, programBiRefine, "BiRefine32x32",
        width32 / 32, height32 / 32, CM_NONE_DEPENDENCY);
    kernelBiRefine.AddKernel(device, programBiRefine, "BiRefine64x64",
        width4x / 16, height4x / 16, CM_NONE_DEPENDENCY);

    const char *prepareSrcName = fourcc == MFX_FOURCC_NV12 ? "PrepareSrcNv12" :
                                 fourcc == MFX_FOURCC_NV16 ? "PrepareSrcNv16" :
                                 fourcc == MFX_FOURCC_P010 ? "PrepareSrcP010" :
                                 fourcc == MFX_FOURCC_P210 ? "PrepareSrcP210" : NULL;
    kernelPrepareSrc.AddKernel(device, programPrepareSrc, prepareSrcName,
        width16x / 4, height16x, CM_NONE_DEPENDENCY);

    if (fourcc == MFX_FOURCC_NV12)
        kernelPrepareRef.AddKernel(device, programPrepareSrc, "PrepareRefNv12",
            width16x / 4, height16x, CM_NONE_DEPENDENCY);
    else
        kernelPrepareRef.AddKernel(device, programPrepareSrc, prepareSrcName,
            width16x / 4, height16x, CM_NONE_DEPENDENCY);

    if (enableInterp) {
        if (interpBlocksW > CM_MAX_THREADSPACE_WIDTH) {
            mfxU32 nW = (interpBlocksW / 2 > CM_MAX_THREADSPACE_WIDTH) ? 3 : 2;
            mfxU32 nH = (interpBlocksH > CM_MAX_THREADSPACE_HEIGHT) ? 2 : 1;
            mfxU32 wStep = interpBlocksW / nW;
            mfxU32 hStep = interpBlocksH / nH;
            for (mfxU32 j = 1; j < nH; j++) {
                for (mfxU32 i = 1; i < nW; i++)
                    kernelPrepareRef.AddKernel(device, programInterpolateFrame, "InterpolateFrameMerge",
                        wStep, hStep, CM_NONE_DEPENDENCY, true);
                kernelPrepareRef.AddKernel(device, programInterpolateFrame, "InterpolateFrameMerge",
                    (interpBlocksW - wStep * (nW - 1)), hStep, CM_NONE_DEPENDENCY);
            }
            for (mfxU32 i = 1; i < nW; i++)
                kernelPrepareRef.AddKernel(device, programInterpolateFrame, "InterpolateFrameMerge",
                    wStep, (interpBlocksH - hStep * (nH - 1)), CM_NONE_DEPENDENCY);
            kernelPrepareRef.AddKernel(device, programInterpolateFrame, "InterpolateFrameMerge",
                (interpBlocksW - wStep * (nW - 1)), (interpBlocksH - hStep * (nH - 1)), CM_NONE_DEPENDENCY);
        } else {
            kernelPrepareRef.AddKernel(device, programInterpolateFrame, "InterpolateFrameMerge",
                interpBlocksW, interpBlocksH, CM_NONE_DEPENDENCY, true);
        }
    }

    mfxU32 dblTsWidth = (width + 12 + 15) / 16;
    if (dblTsWidth > CM_MAX_THREADSPACE_WIDTH) {  // split into 2 parts
        mfxU32 dblTsWidth0 = dblTsWidth / 2;
        kernelDeblock.AddKernel(device, programDeblock, "Deblock",
            dblTsWidth0, (height + 12 + 15) / 16, CM_NONE_DEPENDENCY);
        kernelDeblock.AddKernel(device, programDeblock, "Deblock",
            dblTsWidth - dblTsWidth0, (height + 12 + 15) / 16, CM_NONE_DEPENDENCY);

        kernelFullPostProc.AddKernel(device, programDeblock, "Deblock",
            dblTsWidth0, (height + 12 + 15) / 16, CM_NONE_DEPENDENCY);
        kernelFullPostProc.AddKernel(device, programDeblock, "Deblock",
            dblTsWidth - dblTsWidth0, (height + 12 + 15) / 16, CM_NONE_DEPENDENCY);

        mfxU32 saoTsWidth = (width + 63) / 64 * 4;
        mfxU32 saoTsWidth0 = saoTsWidth / 2;
        kernelFullPostProc.AddKernel(device, programSao, "SaoStat",
            saoTsWidth0, (height + 63) / 64 * 4, CM_NONE_DEPENDENCY, true);
        kernelFullPostProc.AddKernel(device, programSao, "SaoStat",
            saoTsWidth - saoTsWidth0, (height + 63) / 64 * 4, CM_NONE_DEPENDENCY, true);

        if (param->EnableChromaSao) {
            mfxU32 saoChromaTsWidth = ((width>>1) + 31) / 32 * 4;
            mfxU32 saoChromaTsWidth0 = saoChromaTsWidth / 2;
            kernelFullPostProc.AddKernel(device, programSao, "SaoStatChroma",
                saoChromaTsWidth0, ((height>>1) + 31) / 32 * 2, CM_NONE_DEPENDENCY, false);
            kernelFullPostProc.AddKernel(device, programSao, "SaoStatChroma",
                saoChromaTsWidth - saoChromaTsWidth0, ((height>>1) + 31) / 32 * 2, CM_NONE_DEPENDENCY, false);
        }

    } else {
        kernelDeblock.AddKernel(device, programDeblock, "Deblock",
            dblTsWidth, (height + 12 + 15) / 16, CM_NONE_DEPENDENCY);
        kernelFullPostProc.AddKernel(device, programDeblock, "Deblock",
            dblTsWidth, (height + 12 + 15) / 16, CM_NONE_DEPENDENCY);
        kernelFullPostProc.AddKernel(device, programSao, "SaoStat",
            (width + 63) / 64 * 4, (height + 63) / 64 * 4, CM_NONE_DEPENDENCY, true);
        if (param->EnableChromaSao) {
            kernelFullPostProc.AddKernel(device, programSao, "SaoStatChroma",
                ((width>>1) + 31) / 32 * 4, ((height>>1) + 31) / 32 * 2, CM_NONE_DEPENDENCY, false);
        }
    }

    kernelFullPostProc.AddKernel(device, programSao, "SaoEstimate",
        (width + 63) / 64, (height + 63) / 64, CM_WAVEFRONT, true);
    kernelFullPostProc.AddKernel(device, programSao, "SaoApply",
        (width + 63) / 64, (height + 63) / 64, CM_NONE_DEPENDENCY, true);

    curbe = CreateBuffer(device, sizeof(H265EncCURBEData));
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

    // allocate single intermediate surface to store deblocked pixels
    deblocked = CreateSurface(device, sourceWidth, sourceHeight, CM_SURFACE_FORMAT_NV12); // so far only nv12 is supported

    // luma sao
    Ipp32s blockW = 16;
    Ipp32s blockH = 16;
    mfxU32 tsWidth   = (sourceWidth  + param->MaxCUSize - 1) / param->MaxCUSize * (param->MaxCUSize / blockW);
    mfxU32 tsHeight  = (sourceHeight + param->MaxCUSize - 1) / param->MaxCUSize * (param->MaxCUSize / blockH);;
    mfxU32 numThreads = tsWidth * tsHeight;
    mfxU32 bufsize = numThreads * (16+16+32+32) * sizeof(Ipp16s);
    Ipp32s MaxCUSize = param->MaxCUSize >> 1;
    // chroma sao
    blockW = 8;
    blockH = 16;
    tsWidth   = ((sourceWidth >> 1)  + MaxCUSize - 1) / MaxCUSize * (MaxCUSize / blockW);
    tsHeight  = ((sourceHeight >> 1) + MaxCUSize - 1) / MaxCUSize * (MaxCUSize / blockH);
    numThreads = tsWidth * tsHeight;
    mfxU32 bufsizeChroma = numThreads * (16+16+32+32) * sizeof(Ipp16s);
    saoStat = CreateBuffer(device, bufsize + (bufsizeChroma << 1));

    return MFX_ERR_NONE;
}

// TODO - update
void H265CmCtx::FreeCmResources()
{
    //device->FlushPrintBuffer();

    device->DestroySurface(deblocked);
    device->DestroySurface(saoStat);
    device->DestroySurface(curbe);

    if (lastEventSaved != NULL && lastEventSaved != CM_NO_EVENT)
        queue->DestroyEvent(lastEventSaved);
 
    kernelPrepareSrc.Destroy();
    kernelPrepareRef.Destroy();
    kernelGradient.Destroy();
    kernelRefine64x64.Destroy();
    kernelRefine32x32sad.Destroy();
    kernelInterpolateFrame.Destroy();

    kernelMe.Destroy();
    kernelDeblock.Destroy();
    kernelFullPostProc.Destroy();

    device->DestroyProgram(programPrepareSrc);
    device->DestroyProgram(programMeIntra);
    device->DestroyProgram(programGradient);
    device->DestroyProgram(programHmeMe32);
    device->DestroyProgram(programMe16Refine32x32);
    device->DestroyProgram(programBiRefine);
    device->DestroyProgram(programRefine64x64);
    device->DestroyProgram(programRefine32x32sad);
    device->DestroyProgram(programRefine32x16);
    device->DestroyProgram(programRefine16x32);
    device->DestroyProgram(programInterpolateFrame);
    device->DestroyProgram(programDeblock);
    device->DestroyProgram(programSao);

    ::DestroyCmDevice(device);
}


/* copy new input frame to GPU (original and downsampled if needed) */
mfxStatus H265CmCtx::CopyInputFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "CopyInput");
    CmSurface2DUP *inLu = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.luma;
    CmSurface2DUP *inCh = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.chroma;
    mfxFEIH265InputSurface *surf = &((mfxFEIH265Surface *)in->copyArgs.surfVid)->sIn;
    CmSurface2D *dummy = surf->bufOrigNv12;
    if (fourcc == MFX_FOURCC_NV12)
        SetKernelArg(kernelPrepareSrc.kernel, inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_NV16)
        SetKernelArg(kernelPrepareSrc.kernel, inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_P010)
        SetKernelArg(kernelPrepareSrc.kernel, inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, dummy, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_P210)
        SetKernelArg(kernelPrepareSrc.kernel, inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, dummy, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    kernelPrepareSrc.Enqueue(queue, *lastEvent);
    return MFX_ERR_NONE;
}

/* copy new recon frame to GPU (original and downsampled if needed) */
mfxStatus H265CmCtx::CopyReconFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "CopyReconAndInterp");
    CmSurface2DUP *inLu = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.luma;
    CmSurface2DUP *inCh = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.chroma;
    mfxFEIH265ReconSurface *surf = &((mfxFEIH265Surface *)in->copyArgs.surfVid)->sRec;
    CmSurface2D *dummy = surf->bufOrigNv12;
    if (fourcc == MFX_FOURCC_NV12)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_NV16)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_P010)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, dummy, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_P210)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, dummy, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);

    if (enableInterp) {
        // coupling of interp
        if (interpBlocksW > CM_MAX_THREADSPACE_WIDTH) {
            mfxU32 nW = (interpBlocksW / 2 > CM_MAX_THREADSPACE_WIDTH) ? 3 : 2;
            mfxU32 nH = (interpBlocksH > CM_MAX_THREADSPACE_HEIGHT) ? 2 : 1;
            mfxU32 wStep = interpBlocksW / nW;
            mfxU32 hStep = interpBlocksH / nH;
            mfxU32 iker = 1;
            for (mfxU32 j = 0; j < nH - 1; j++) {
                for (mfxU32 i = 0; i < nW - 1; i++) {
                    SetKernelArg(kernelPrepareRef.m_kernel[iker++], surf->bufOrigNv12, surf->bufInterpMerged, i * wStep, j * hStep);
                }
                SetKernelArg(kernelPrepareRef.m_kernel[iker++], surf->bufOrigNv12, surf->bufInterpMerged, (nW - 1) * wStep, j * hStep);
            }
            for (mfxU32 i = 0; i < nW - 1; i++) {
                    SetKernelArg(kernelPrepareRef.m_kernel[iker++], surf->bufOrigNv12, surf->bufInterpMerged, i * wStep, (nH - 1) * hStep);
            }
            SetKernelArg(kernelPrepareRef.m_kernel[iker++], surf->bufOrigNv12, surf->bufInterpMerged, (nW - 1) * wStep, (nH - 1) * hStep);
        } else {
            SetKernelArg(kernelPrepareRef.m_kernel[1], surf->bufOrigNv12, surf->bufInterpMerged, 0, 0);
        }
    }

    kernelPrepareRef.Enqueue(queue, *lastEvent);
    return MFX_ERR_NONE;
}

// KERNEL DISPATCHER 
void * H265CmCtx::RunVme(mfxFEIH265Input *feiIn, mfxExtFEIH265Output *feiOut)
{
    CmSurface2DUP *data[MFX_FEI_H265_BLK_MAX];
    CmEvent * lastEvent;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "RunVme");

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
        CopyInputFrameToGPU(&lastEvent, feiIn);

    if (feiIn->FEIOp & MFX_FEI_H265_OP_GPU_COPY_REF) { // along with Interp
        CopyReconFrameToGPU(&lastEvent, feiIn);
    }
        
    if (feiIn->FEIOp & MFX_FEI_H265_OP_INTRA_MODE) {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Gradient");
        mfxFEIH265InputSurface *surfIn = &((mfxFEIH265Surface *)feiIn->meArgs.surfSrc)->sIn;
        SetKernelArg(kernelGradient.kernel, surfIn->bufOrigNv12, GET_OUTSURF(feiOut->SurfIntraMode[0]), GET_OUTSURF(feiOut->SurfIntraMode[1]), GET_OUTSURF(feiOut->SurfIntraMode[2]), GET_OUTSURF(feiOut->SurfIntraMode[3]), width);
        kernelGradient.Enqueue(queue, lastEvent);
    }

    //if ((feiIn->FEIOp & MFX_FEI_H265_OP_INTRA_DIST) && (feiIn->FrameType == MFX_FRAMETYPE_P || feiIn->FrameType == MFX_FRAMETYPE_B)) {
    //    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "IntraDist");
    //    mfxFEIH265InputSurface *surfIn = &((mfxFEIH265Surface *)feiIn->meArgs.surfSrc)->sIn;
    //    SurfaceIndex *refsIntra = CreateVmeSurfaceG75(device, surfIn->bufOrigNv12, 0, 0, 0, 0);
    //    SetKernelArg(kernelMeIntra.kernel, curbe, *refsIntra, surfIn->bufOrigNv12, GET_OUTSURF(feiOut->SurfIntraDist));
    //    kernelMeIntra.Enqueue(queue, lastEvent);
    //    device->DestroyVmeSurfaceG7_5(refsIntra);
    //}

    if (feiIn->FEIOp & (MFX_FEI_H265_OP_INTER_ME)) {
        data[MFX_FEI_H265_BLK_64x64] = GET_OUTSURF(feiOut->SurfInterData[MFX_FEI_H265_BLK_64x64]);
        data[MFX_FEI_H265_BLK_32x16] = GET_OUTSURF(feiOut->SurfInterData[rectParts ? MFX_FEI_H265_BLK_32x16 : MFX_FEI_H265_BLK_32x32]);
        data[MFX_FEI_H265_BLK_16x32] = GET_OUTSURF(feiOut->SurfInterData[rectParts ? MFX_FEI_H265_BLK_16x32 : MFX_FEI_H265_BLK_32x32]);
        data[MFX_FEI_H265_BLK_32x32] = GET_OUTSURF(feiOut->SurfInterData[MFX_FEI_H265_BLK_32x32]);
        data[MFX_FEI_H265_BLK_16x16] = GET_OUTSURF(feiOut->SurfInterData[MFX_FEI_H265_BLK_16x16]);
        data[MFX_FEI_H265_BLK_16x8]  = GET_OUTSURF(feiOut->SurfInterData[rectParts ? MFX_FEI_H265_BLK_16x8 : MFX_FEI_H265_BLK_16x16]);
        data[MFX_FEI_H265_BLK_8x16]  = GET_OUTSURF(feiOut->SurfInterData[rectParts ? MFX_FEI_H265_BLK_8x16 : MFX_FEI_H265_BLK_16x16]);
        data[MFX_FEI_H265_BLK_8x8]   = GET_OUTSURF(feiOut->SurfInterData[MFX_FEI_H265_BLK_8x8]);

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "ME");
        mfxFEIH265InputSurface *surfIn = &((mfxFEIH265Surface *)feiIn->meArgs.surfSrc)->sIn;
        mfxFEIH265ReconSurface *surfRef = &((mfxFEIH265Surface *)feiIn->meArgs.surfRef)->sRec;
        SurfaceIndex *refs   = CreateVmeSurfaceG75(device, surfIn->bufOrigNv12, &(surfRef->bufOrigNv12), 0, 1, 0);
        SurfaceIndex *refs2x = CreateVmeSurfaceG75(device, surfIn->bufDown2x, &(surfRef->bufDown2x), 0, 1, 0);
        SurfaceIndex *refs4x = CreateVmeSurfaceG75(device, surfIn->bufDown4x, &(surfRef->bufDown4x), 0, 1, 0);
        SurfaceIndex *refs8x = CreateVmeSurfaceG75(device, surfIn->bufDown8x, &(surfRef->bufDown8x), 0, 1, 0);
        SurfaceIndex *refs16x = CreateVmeSurfaceG75(device, surfIn->bufDown16x, &(surfRef->bufDown16x), 0, 1, 0);
        CmBuffer *meControl = surfIn->meControl;

        if (!surfIn->meControlInited || surfIn->lambda != feiIn->meArgs.lambda) {
            MeControl control = {};
            SetupMeControl(control, width, height, feiIn->meArgs.lambda);
            meControl->WriteSurface((mfxU8 *)&control, NULL);
            surfIn->meControlInited = true;
            surfIn->lambda = feiIn->meArgs.lambda;
        }

        SetKernelArg(kernelMe.m_kernel[0], meControl, *refs16x, *refs8x, *refs4x, *refs2x,
            data[MFX_FEI_H265_BLK_64x64], data[MFX_FEI_H265_BLK_32x32], data[MFX_FEI_H265_BLK_16x16]);
        SetKernelArg(kernelMe.m_kernel[1], data[MFX_FEI_H265_BLK_64x64], surfIn->bufOrigNv12, surfRef->bufOrigNv12);
        if (width / 16 > CM_MAX_THREADSPACE_WIDTH) {  // split into 2 parts
            SetKernelArg(kernelMe.m_kernel[2], meControl, *refs, data[MFX_FEI_H265_BLK_32x32], data[MFX_FEI_H265_BLK_16x16],
                data[MFX_FEI_H265_BLK_8x8], surfIn->bufOrigNv12, surfRef->bufOrigNv12, 0, 0);
            mfxU32 startMbX = ((width / 16) >> 1) & ~1;  // must be even for 32x32 blocks
            SetKernelArg(kernelMe.m_kernel[3], meControl, *refs, data[MFX_FEI_H265_BLK_32x32], data[MFX_FEI_H265_BLK_16x16],
                data[MFX_FEI_H265_BLK_8x8], surfIn->bufOrigNv12, surfRef->bufOrigNv12, startMbX, 0);
        } else {
            SetKernelArg(kernelMe.m_kernel[2], meControl, *refs, data[MFX_FEI_H265_BLK_32x32], data[MFX_FEI_H265_BLK_16x16],
                data[MFX_FEI_H265_BLK_8x8], surfIn->bufOrigNv12, surfRef->bufOrigNv12, 0, 0);
        }
        kernelMe.Enqueue(queue, lastEvent);

        device->DestroyVmeSurfaceG7_5(refs);
        device->DestroyVmeSurfaceG7_5(refs16x);
        device->DestroyVmeSurfaceG7_5(refs8x);
        device->DestroyVmeSurfaceG7_5(refs4x);
        device->DestroyVmeSurfaceG7_5(refs2x);
    }

    /* bi-refinement */
    if (feiIn->FEIOp & MFX_FEI_H265_OP_BIREFINE) {

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "  Biref");
        mfxFEIH265InputSurface *surfIn = &((mfxFEIH265Surface *)feiIn->birefArgs.surfSrc)->sIn;
        mfxFEIH265ReconSurface *surfRef0 = &((mfxFEIH265Surface *)feiIn->birefArgs.surfRef0)->sRec;
        mfxFEIH265ReconSurface *surfRef1 = &((mfxFEIH265Surface *)feiIn->birefArgs.surfRef1)->sRec;

        CmSurface2DUP *birefdata[MFX_FEI_H265_BLK_MAX];
        birefdata[MFX_FEI_H265_BLK_64x64] = GET_OUTSURF(feiOut->SurfBirefData[MFX_FEI_H265_BLK_64x64]);
        birefdata[MFX_FEI_H265_BLK_32x32] = GET_OUTSURF(feiOut->SurfBirefData[MFX_FEI_H265_BLK_32x32]);

        mfxFEIOptParamsBiref *optParams = (mfxFEIOptParamsBiref *)(feiIn->birefArgs.params);
        CmSurface2DUP *dataRef0[MFX_FEI_H265_BLK_MAX], *dataRef1[MFX_FEI_H265_BLK_MAX];
        dataRef0[MFX_FEI_H265_BLK_32x32] = GET_OUTSURF(optParams->InterDataRef0[MFX_FEI_H265_BLK_32x32]);
        dataRef0[MFX_FEI_H265_BLK_64x64] = GET_OUTSURF(optParams->InterDataRef0[MFX_FEI_H265_BLK_64x64]);
        dataRef1[MFX_FEI_H265_BLK_32x32] = GET_OUTSURF(optParams->InterDataRef1[MFX_FEI_H265_BLK_32x32]);
        dataRef1[MFX_FEI_H265_BLK_64x64] = GET_OUTSURF(optParams->InterDataRef1[MFX_FEI_H265_BLK_64x64]);

        vector<SurfaceIndex, 1> surfsRef0;
        surfsRef0[0] = GetIndex(surfRef0->bufInterpMerged);

        vector<SurfaceIndex, 1> surfsRef1;
        surfsRef1[0] = GetIndex(surfRef1->bufInterpMerged);

        SetKernelArg(kernelBiRefine.m_kernel[0], birefdata[MFX_FEI_H265_BLK_32x32], surfIn->bufOrigNv12,
            surfsRef0, surfsRef1, dataRef0[MFX_FEI_H265_BLK_32x32], dataRef1[MFX_FEI_H265_BLK_32x32]);

        SetKernelArg(kernelBiRefine.m_kernel[1], birefdata[MFX_FEI_H265_BLK_64x64], surfIn->bufOrigNv12,
            surfsRef0, surfsRef1, dataRef0[MFX_FEI_H265_BLK_64x64], dataRef1[MFX_FEI_H265_BLK_64x64]);

        kernelBiRefine.Enqueue(queue, lastEvent);
    }


    /* GPU deblock, SAO estimate and SAO apply */
    if (feiIn->FEIOp & (MFX_FEI_H265_OP_POSTPROC | MFX_FEI_H265_OP_DEBLOCK)) {
        // Prepare arguments
        CmSurface2DUP *recUpLu = ((mfxFEIH265Surface *)feiIn->postprocArgs.reconSurfSys)->sUp.luma;
        CmSurface2DUP *recUpCh = ((mfxFEIH265Surface *)feiIn->postprocArgs.reconSurfSys)->sUp.chroma;
        CmSurface2D *origin = ((mfxFEIH265Surface *)feiIn->postprocArgs.inputSurf)->sIn.bufOrigNv12;  // postproc supports only NV12 for now
        CmSurface2D *recon = ((mfxFEIH265Surface *)feiIn->postprocArgs.reconSurfVid)->sRec.bufOrigNv12; // postproc supports only NV12 for now
        CmBufferUP *cuData = ((mfxFEIH265Surface *)feiIn->postprocArgs.cuData)->sBuf.bufOut;
        CmBufferUP *saoModes = ((mfxFEIH265Surface *)feiIn->postprocArgs.saoModes)->sBuf.bufOut;
        CmBuffer *postprocParam = ((mfxFEIH265Surface *)feiIn->postprocArgs.inputSurf)->sIn.postprocParam;
        postprocParam->WriteSurface((const Ipp8u*)feiIn->postprocArgs.param, NULL);

        int doSao = feiIn->FEIOp & (MFX_FEI_H265_OP_POSTPROC);

        if (doSao) {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Deblock+Sao");
            mfxU32 dblTsWidth = (width + 12 + 15) / 16;

            if (dblTsWidth > CM_MAX_THREADSPACE_WIDTH) {  // split into 2 parts
                int kernelIdx = 0;
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], recUpLu, recUpCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, doSao ? deblocked : recon, cuData, postprocParam, 0, 0); // Deblocking1
                mfxU32 startMbX = dblTsWidth / 2;
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], recUpLu, recUpCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, doSao ? deblocked : recon, cuData, postprocParam, startMbX, 0); // Deblocking2

                mfxU32 saoTsWidth = (width + 63) / 64 * 4;
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], origin, deblocked, postprocParam, saoStat, 0, 0); // SaoStat1
                startMbX =  saoTsWidth / 2;
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], origin, deblocked, postprocParam, saoStat, startMbX, 0); // SaoStat2

                if (enableChromaSao) {
                    mfxU32 saoChromaTsWidth = ((width>>1) + 31) / 32 * 4;
                    SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], origin, deblocked, postprocParam, saoStat, 0, 0); // SaoStatChroma1
                    startMbX = saoChromaTsWidth / 2;
                    SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], origin, deblocked, postprocParam, saoStat, startMbX, 0); // // SaoStatChroma2
                }

                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], postprocParam, saoStat, saoModes); // SaoEst
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], deblocked, recon, postprocParam, saoModes);// SaoApply
            
                kernelFullPostProc.Enqueue(queue, lastEvent);
            } else {
                // could be (1) Deblock + SaoStat + SaoEst + SaoApply or (2) Deblock + SaoStat + SaoStatChroma + SaoEst + SaoApply
                int kernelIdx = 0;
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], recUpLu, recUpCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, doSao ? deblocked : recon, cuData, postprocParam, 0, 0); // Deblocking
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], origin, deblocked, postprocParam, saoStat, 0, 0); // SaoStat
                if (enableChromaSao)
                    SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], origin, deblocked, postprocParam, saoStat, 0, 0); // SaoStatChroma
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], postprocParam, saoStat, saoModes); // SaoEst
                SetKernelArg(kernelFullPostProc.m_kernel[kernelIdx++], deblocked, recon, postprocParam, saoModes); // SaoApply
                kernelFullPostProc.Enqueue(queue, lastEvent);
            }

        } else {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Deblock");
            mfxU32 dblTsWidth = (width + 12 + 15) / 16;
            if (dblTsWidth > CM_MAX_THREADSPACE_WIDTH) {  // split into 2 parts
                SetKernelArg(kernelDeblock.m_kernel[0], recUpLu, recUpCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, doSao ? deblocked : recon, cuData, postprocParam, 0, 0); // Deblocking1
                mfxU32 startMbX = dblTsWidth / 2;
                SetKernelArg(kernelDeblock.m_kernel[1], recUpLu, recUpCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, doSao ? deblocked : recon, cuData, postprocParam, startMbX, 0); // Deblocking2
                kernelDeblock.Enqueue(queue, lastEvent);
            } else {
                SetKernelArg(kernelDeblock.kernel, recUpLu, recUpCh, AlignValue(padding<<bppShift,64)>>bppShift, AlignValue(paddingChroma<<bppShift,64)>>bppShift, doSao ? deblocked : recon, cuData, postprocParam, 0, 0); // Deblocking
                kernelDeblock.Enqueue(queue, lastEvent);
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

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_SyncOperation");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    return hcm->SyncCurrent((mfxSyncPoint)syncp, wait);
}

/* internal only - allow sync objects to be saved and synced on/destroyed out of order */
MSDK_PLUGIN_API(mfxStatus) H265FEI_DestroySavedSyncPoint(mfxFEIH265 feih265, mfxFEISyncPoint syncp)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_DestroySavedSyncPoint");

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

    CmDevice *device = CreateCmDevicePtr((MFXCoreInterface *) core);
    if (!device)
        return MFX_ERR_UNSUPPORTED;

    mfxFEIH265Param param;
    param.Width = width;
    param.Height = height;
    GetSurfaceDimensions(device, &param, extAlloc);

    ::DestroyCmDevice(device);

    return MFX_ERR_NONE;
}

/* calculate dimensions for Cm output buffers (2DUp surfaces) 
 * NOTE - this assumes the device does NOT yet exist (i.e. Init() hasn't been called)
 */
MSDK_PLUGIN_API(mfxStatus) H265FEI_GetSurfaceDimensions_new(void *core, mfxFEIH265Param *param, mfxExtFEIH265Alloc *extAlloc)
{
    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_GetSurfaceDimensions");

    if (!param->Width || !param->Height || !extAlloc)
        return MFX_ERR_NULL_PTR;

    CmDevice *device = CreateCmDevicePtr((MFXCoreInterface *) core);
    if (!device)
        return MFX_ERR_UNSUPPORTED;

    GetSurfaceDimensions(device, param, extAlloc);

    ::DestroyCmDevice(device);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateSurfaceUp(mfxFEIH265 feih265, mfxU8 *sysMemLu, mfxU8 *sysMemCh, mfxHDL *pInSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateInputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pInSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_UP, sysMemLu, sysMemCh, NULL);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateInputSurface(mfxFEIH265 feih265, mfxHDL *pInSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateInputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pInSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_INPUT, NULL, NULL, NULL);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateReconSurface(mfxFEIH265 feih265, mfxHDL *pRecSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateReconSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pRecSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_RECON, NULL, NULL, NULL);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateOutputSurface(mfxFEIH265 feih265, mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *pOutSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateOutputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pOutSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_OUTPUT, sysMem, NULL, surfInfo);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateOutputBuffer(mfxFEIH265 feih265, mfxU8 *sysMem, mfxU32 size, mfxHDL *pOutBuf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateOutputBuffer");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    mfxSurfInfoENC surfInfo;
    surfInfo.size = size;
    *pOutBuf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_OUTPUT_BUFFER, sysMem, NULL, &surfInfo);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_FreeSurface(mfxFEIH265 feih265, mfxHDL pSurf)
{
    H265CmCtx *hcm = (H265CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_FreeSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    return hcm->FreeSurface((mfxFEIH265Surface *)pSurf);
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
