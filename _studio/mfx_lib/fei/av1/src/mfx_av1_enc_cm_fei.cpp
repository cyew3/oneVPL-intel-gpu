// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_AV1E_VIDEO_ENCODE)

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

#include "genx_av1_prepare_src_hsw_isa.h"
#include "genx_av1_prepare_src_bdw_isa.h"
#include "genx_av1_prepare_src_skl_isa.h"
#include "genx_av1_mepu_hsw_isa.h"
#include "genx_av1_mepu_bdw_isa.h"
#include "genx_av1_mepu_skl_isa.h"
#include "genx_av1_mode_decision_hsw_isa.h"
#include "genx_av1_mode_decision_bdw_isa.h"
#include "genx_av1_mode_decision_skl_isa.h"
#include "genx_av1_mode_decision_pass2_hsw_isa.h"
#include "genx_av1_mode_decision_pass2_bdw_isa.h"
#include "genx_av1_mode_decision_pass2_skl_isa.h"

#include "genx_av1_interpolate_decision_hsw_isa.h"
#include "genx_av1_interpolate_decision_bdw_isa.h"
#include "genx_av1_interpolate_decision_skl_isa.h"
#include "genx_av1_interpolate_decision_single_hsw_isa.h"
#include "genx_av1_interpolate_decision_single_bdw_isa.h"
#include "genx_av1_interpolate_decision_single_skl_isa.h"

#include "genx_av1_intra_hsw_isa.h"
#include "genx_av1_intra_bdw_isa.h"
#include "genx_av1_intra_skl_isa.h"
#include "genx_av1_vartx_decision_hsw_isa.h"
#include "genx_av1_vartx_decision_bdw_isa.h"
#include "genx_av1_vartx_decision_skl_isa.h"

#include "genx_hevce_me_p16_4mv_and_refine_32x32_hsw_isa.h"
#include "genx_hevce_me_p16_4mv_and_refine_32x32_bdw_isa.h"
#include "genx_hevce_me_p16_4mv_and_refine_32x32_skl_isa.h"
#include "genx_hevce_refine_me_p_64x64_hsw_isa.h"
#include "genx_hevce_refine_me_p_64x64_bdw_isa.h"
#include "genx_hevce_refine_me_p_64x64_skl_isa.h"
#include "genx_hevce_hme_and_me_p32_4mv_hsw_isa.h"
#include "genx_hevce_hme_and_me_p32_4mv_bdw_isa.h"
#include "genx_hevce_hme_and_me_p32_4mv_skl_isa.h"

#include "mfx_av1_enc_cm_fei.h"
#include "mfx_av1_enc_cm_utils.h"

#undef MFX_TRACE_ENABLE

namespace AV1Enc {

#define GET_INSURF(s)  (&((mfxFEIH265Surface *)(s))->sIn )
#define GET_RECSURF(s) (&((mfxFEIH265Surface *)(s))->sRec )
#define GET_UPSURF(s)  (&((mfxFEIH265Surface *)(s))->sUp )
#define GET_OUTSURF(s) ( ((mfxFEIH265Surface *)(s))->sOut.bufOut )
#define GET_OUTBUF(s)  ( ((mfxFEIH265Surface *)(s))->sBuf.bufOut )

/* leave in same file to avoid difficulties with templates (T is arbitrary type) */
template <class T>
void GetDimensionsCmSurface2DUp(CmDevice *device, uint32_t numElemInRow, uint32_t numRows, CM_SURFACE_FORMAT format, mfxSurfInfoENC *surfInfo)
{
    uint32_t numBytesInRow = numElemInRow * sizeof(T);

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
void CreateCmSurface2DUpPreAlloc(CmDevice *device, uint32_t numBytesInRow, uint32_t numRows, CM_SURFACE_FORMAT format, T *&surfaceCpu, CmSurface2DUP *& surfaceGpu)
{
    if (surfaceCpu) {
        surfaceGpu = CreateSurface(device, numBytesInRow, numRows, format, surfaceCpu);
    }
}

template <class T>
CmSurface2DUP *CreateCmSurface2DUpPreAlloc(CmDevice *device, uint32_t numBytesInRow, uint32_t numRows, CM_SURFACE_FORMAT format, T *&surfaceCpu)
{
    return (surfaceCpu) ? CreateSurface(device, numBytesInRow, numRows, format, surfaceCpu) : NULL;
}

template <class T>
void CreateCmSurface2DUp(CmDevice *device, uint32_t numElemInRow, uint32_t numRows, CM_SURFACE_FORMAT format,
                         T *&surfaceCpu, CmSurface2DUP *& surfaceGpu, uint32_t &pitch)
{
    uint32_t size = 0;
    uint32_t numBytesInRow = numElemInRow * sizeof(T);
    int res = CM_SUCCESS;
    if ((res = device->GetSurface2DInfo(numBytesInRow, numRows, format, pitch, size)) != CM_SUCCESS) {
        throw CmRuntimeError();
    }

    surfaceCpu = static_cast<T *>(CM_ALIGNED_MALLOC(size, 0x1000)); // 4K aligned
    surfaceGpu = CreateSurface(device, numBytesInRow, numRows, format, surfaceCpu);
}

template <class T>
void CreateCmBufferUp(CmDevice *device, uint32_t numElems, T *&surfaceCpu, CmBufferUP *& surfaceGpu)
{
    uint32_t numBytes = numElems * sizeof(T);
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
    int32_t cmMvW[MFX_FEI_H265_BLK_MAX], cmMvH[MFX_FEI_H265_BLK_MAX];

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
    width32  = UMC::align_value<uint32_t>(width, 32);
    height32 = UMC::align_value<uint32_t>(height, 32);
    width     = UMC::align_value<uint32_t>(width,  4);
    height    = UMC::align_value<uint32_t>(height, 4);
    width2x   = UMC::align_value<uint32_t>((width  + 1) / 2, 4);
    height2x  = UMC::align_value<uint32_t>((height + 1) / 2, 4);
    width4x   = UMC::align_value<uint32_t>((width  + 3) / 4, 4);
    height4x  = UMC::align_value<uint32_t>((height + 3) / 4, 4);

    /* dimensions of output MV grid for each block size (width/height were aligned to multiple of 16) */
    cmMvW[MFX_FEI_H265_BLK_16x16] = (width   + 15) / 16; cmMvH[MFX_FEI_H265_BLK_16x16] = (height   + 15) / 16;
    cmMvW[MFX_FEI_H265_BLK_16x8 ] = (width   + 15) / 16; cmMvH[MFX_FEI_H265_BLK_16x8 ] = (height   +  7) /  8;
    cmMvW[MFX_FEI_H265_BLK_8x16 ] = (width   +  7) /  8; cmMvH[MFX_FEI_H265_BLK_8x16 ] = (height   + 15) / 16;
    cmMvW[MFX_FEI_H265_BLK_8x8  ] = (width   +  7) /  8; cmMvH[MFX_FEI_H265_BLK_8x8  ] = (height   +  7) /  8;
    cmMvW[MFX_FEI_H265_BLK_32x32] = (width2x + 15) / 16; cmMvH[MFX_FEI_H265_BLK_32x32] = (height2x + 15) / 16;
    cmMvW[MFX_FEI_H265_BLK_32x16] = (width2x + 15) / 16; cmMvH[MFX_FEI_H265_BLK_32x16] = (height2x +  7) /  8;
    cmMvW[MFX_FEI_H265_BLK_16x32] = (width2x +  7) /  8; cmMvH[MFX_FEI_H265_BLK_16x32] = (height2x + 15) / 16;
    cmMvW[MFX_FEI_H265_BLK_64x64] = (width4x + 15) / 16; cmMvH[MFX_FEI_H265_BLK_64x64] = (height4x + 15) / 16;

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
    //GetDimensionsCmSurface2DUp<uint8_t>(device, interpWidth * 2, interpHeight * 2, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[0]);
    //GetDimensionsCmSurface2DUp<uint8_t>(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[1]);
    //GetDimensionsCmSurface2DUp<uint8_t>(device, interpWidth, interpHeight, CM_SURFACE_FORMAT_P8, &extAlloc->Interpolate[2]);

    /* bi-refine */
    GetDimensionsCmSurface2DUp<mfxFEIH265BirefData>(device, cmMvW[MFX_FEI_H265_BLK_32x32], cmMvH[MFX_FEI_H265_BLK_32x32],
        CM_SURFACE_FORMAT_P8, &extAlloc->BirefData[MFX_FEI_H265_BLK_32x32]);
    GetDimensionsCmSurface2DUp<mfxFEIH265BirefData>(device, cmMvW[MFX_FEI_H265_BLK_64x64], cmMvH[MFX_FEI_H265_BLK_64x64],
        CM_SURFACE_FORMAT_P8, &extAlloc->BirefData[MFX_FEI_H265_BLK_64x64]);

    /* source and reconstructed reference frames */
    int32_t bppShift = (param->FourCC == MFX_FOURCC_P010 || param->FourCC == MFX_FOURCC_P210) ? 1 : 0;
    CM_SURFACE_FORMAT format = (param->FourCC == MFX_FOURCC_P010 || param->FourCC == MFX_FOURCC_P210) ? CM_SURFACE_FORMAT_V8U8 : CM_SURFACE_FORMAT_P8;
    int32_t minPitchInBytes = UMC::align_value<int32_t>(UMC::align_value<int32_t>(param->Padding<<bppShift, 64) + (param->Width<<bppShift) + (param->Padding<<bppShift), 64);
    if ((minPitchInBytes & (minPitchInBytes - 1)) == 0)
        minPitchInBytes += 64;
    GetDimensionsCmSurface2DUp<uint8_t>(device, minPitchInBytes>>bppShift, param->Height, format, &extAlloc->SrcRefLuma);

    minPitchInBytes = UMC::align_value<int32_t>(UMC::align_value<int32_t>(param->PaddingChroma<<bppShift, 64) + (param->WidthChroma<<bppShift) + (param->PaddingChroma<<bppShift), 64);
    if ((minPitchInBytes & (minPitchInBytes - 1)) == 0)
        minPitchInBytes += 64;
    GetDimensionsCmSurface2DUp<uint8_t>(device, minPitchInBytes>>bppShift, param->HeightChroma, format, &extAlloc->SrcRefChroma);

    const int32_t sizeofModeInfo = 32;
    const int32_t sbCols = (param->Width  + 63) >> 6;
    const int32_t sbRows = (param->Height + 63) >> 6;
    GetDimensionsCmSurface2DUp<uint8_t>(device, sbCols * 8 * sizeofModeInfo, sbRows * 8, CM_SURFACE_FORMAT_P8, &extAlloc->ModeInfo);

    GetDimensionsCmSurface2DUp<uint8_t>(device, sbCols * 4, sbRows, CM_SURFACE_FORMAT_P8, &extAlloc->RsCs64Info);
    GetDimensionsCmSurface2DUp<uint8_t>(device, 2*sbCols * 4, 2*sbRows, CM_SURFACE_FORMAT_P8, &extAlloc->RsCs32Info);
    GetDimensionsCmSurface2DUp<uint8_t>(device, 4*sbCols * 4, 4*sbRows, CM_SURFACE_FORMAT_P8, &extAlloc->RsCs16Info);
    GetDimensionsCmSurface2DUp<uint8_t>(device, 8*sbCols * 4, 8*sbRows, CM_SURFACE_FORMAT_P8, &extAlloc->RsCs8Info);

    return MFX_ERR_NONE;
}

/* allocate a single input surface (source or recon) or output surface */
void * AV1CmCtx::AllocateSurface(mfxFEIH265SurfaceType surfaceType, void *sysMem1, void *sysMem2, mfxSurfInfoENC *surfInfo)
{
    mfxFEIH265Surface *s;
    int32_t minPitchInBytes;

    CM_SURFACE_FORMAT format = (fourcc == MFX_FOURCC_P010 || fourcc == MFX_FOURCC_P210) ? CM_SURFACE_FORMAT_V8U8 : CM_SURFACE_FORMAT_P8;
    mfxU8 *sysMemLu = (mfxU8 *)sysMem1 - UMC::align_value<int32_t>(padding<<bppShift, 64);
    mfxU8 *sysMemCh = (mfxU8 *)sysMem2 - UMC::align_value<int32_t>(paddingChroma<<bppShift, 64);

    switch (surfaceType) {
    case MFX_FEI_H265_SURFTYPE_INPUT:
        s = new mfxFEIH265Surface();    // zero-init
        s->surfaceType = MFX_FEI_H265_SURFTYPE_INPUT;
        s->sIn.bufLuma10bit = NULL; // FIXME: allocate when GPU postprocessing for REXT is ready
        s->sIn.bufChromaRext = NULL; // FIXME: allocate when GPU postprocessing for REXT is ready
        s->sIn.bufOrigNv12  = CreateSurface(device, sourceWidth, sourceHeight, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown2x    = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown4x    = CreateSurface(device, width4x, height4x, CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown8x    = CreateSurface(device,  width8x,  height8x,  CM_SURFACE_FORMAT_NV12);
        s->sIn.bufDown16x   = CreateSurface(device, width16x, height16x, CM_SURFACE_FORMAT_NV12);
        s->sIn.meControl   = CreateBuffer(device, sizeof(MeControl));
        s->sIn.mdControl   = CreateBuffer(device, sizeof(MdControl));
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
        s->sRec.bufDown8x   = CreateSurface(device,  width8x,  height8x,  CM_SURFACE_FORMAT_NV12);
        s->sRec.bufDown16x  = CreateSurface(device, width16x, height16x, CM_SURFACE_FORMAT_NV12);
        if (enableInterp)
            s->sRec.bufInterpMerged = CreateSurface(device, interpWidth * 2, interpHeight * 2, CM_SURFACE_FORMAT_A8);
        return s;

    case MFX_FEI_H265_SURFTYPE_UP:
        s = new mfxFEIH265Surface();    // zero-init
        s->surfaceType = MFX_FEI_H265_SURFTYPE_UP;
        minPitchInBytes = UMC::align_value<int32_t>(UMC::align_value<int32_t>(padding<<bppShift, 64) + (width<<bppShift) + (padding<<bppShift), 64);
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

mfxStatus AV1CmCtx::FreeSurface(mfxFEIH265Surface *s)
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
        device->DestroySurface(s->sIn.mdControl);
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

mfxStatus AV1CmCtx::AllocateCmResources(mfxFEIH265Param *param, void *core)
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
    width32   = UMC::align_value<uint32_t>(width,  32);
    height32  = UMC::align_value<uint32_t>(height, 32);
    width     = UMC::align_value<uint32_t>(width,  4);
    height    = UMC::align_value<uint32_t>(height, 4);
    width2x   = UMC::align_value<uint32_t>((width  + 1) / 2, 4);
    height2x  = UMC::align_value<uint32_t>((height + 1) / 2, 4);
    width4x   = UMC::align_value<uint32_t>((width  + 3) / 4, 4);
    height4x  = UMC::align_value<uint32_t>((height + 3) / 4, 4);
    width8x   = UMC::align_value<uint32_t>((width  + 7) / 8, 4);
    height8x  = UMC::align_value<uint32_t>((height + 7) / 8, 4);
    width16x  = UMC::align_value<uint32_t>((width  + 15) / 16, 4);
    height16x = UMC::align_value<uint32_t>((height + 15) / 16, 4);

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
    device->InitPrintBuffer(10*1024*1024);

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
            //case MFX_HW_HSW_ULT:
            programPrepareSrc       = ReadProgram(device, genx_av1_prepare_src_hsw, sizeof(genx_av1_prepare_src_hsw));
            programHmeMe32          = ReadProgram(device, genx_hevce_hme_and_me_p32_4mv_hsw, sizeof(genx_hevce_hme_and_me_p32_4mv_hsw));
            programMe16Refine32x32  = ReadProgram(device, genx_hevce_me_p16_4mv_and_refine_32x32_hsw, sizeof(genx_hevce_me_p16_4mv_and_refine_32x32_hsw));
            programMePu             = ReadProgram(device, genx_av1_mepu_hsw, sizeof(genx_av1_mepu_hsw));
            programMd               = ReadProgram(device, genx_av1_mode_decision_hsw, sizeof(genx_av1_mode_decision_hsw));
            programMdPass2          = ReadProgram(device, genx_av1_mode_decision_pass2_hsw, sizeof(genx_av1_mode_decision_pass2_hsw));
            programInterpDecision   = ReadProgram(device, genx_av1_interpolate_decision_hsw, sizeof(genx_av1_interpolate_decision_hsw));
            programInterpDecisionSingle = ReadProgram(device, genx_av1_interpolate_decision_single_hsw, sizeof(genx_av1_interpolate_decision_single_hsw));
            programVarTxDecision    = ReadProgram(device, genx_av1_vartx_decision_hsw, sizeof(genx_av1_vartx_decision_hsw));
            programAv1Intra         = ReadProgram(device, genx_av1_intra_hsw, sizeof(genx_av1_intra_hsw));
            programRefine64x64      = ReadProgram(device, genx_hevce_refine_me_p_64x64_hsw, sizeof(genx_hevce_refine_me_p_64x64_hsw));
            break;
        case PLATFORM_INTEL_BDW:
        case PLATFORM_INTEL_CHV:
            //case MFX_HW_CHV:
            programPrepareSrc       = ReadProgram(device, genx_av1_prepare_src_bdw, sizeof(genx_av1_prepare_src_bdw));
            programHmeMe32          = ReadProgram(device, genx_hevce_hme_and_me_p32_4mv_bdw, sizeof(genx_hevce_hme_and_me_p32_4mv_bdw));
            programMe16Refine32x32  = ReadProgram(device, genx_hevce_me_p16_4mv_and_refine_32x32_bdw, sizeof(genx_hevce_me_p16_4mv_and_refine_32x32_bdw));
            programMePu             = ReadProgram(device, genx_av1_mepu_bdw, sizeof(genx_av1_mepu_bdw));
            programMd               = ReadProgram(device, genx_av1_mode_decision_bdw, sizeof(genx_av1_mode_decision_bdw));
            programMdPass2          = ReadProgram(device, genx_av1_mode_decision_pass2_bdw, sizeof(genx_av1_mode_decision_pass2_bdw));
            programInterpDecision   = ReadProgram(device, genx_av1_interpolate_decision_bdw, sizeof(genx_av1_interpolate_decision_bdw));
            programInterpDecisionSingle = ReadProgram(device, genx_av1_interpolate_decision_single_bdw, sizeof(genx_av1_interpolate_decision_single_bdw));
            programVarTxDecision    = ReadProgram(device, genx_av1_vartx_decision_bdw, sizeof(genx_av1_vartx_decision_bdw));
            programAv1Intra         = ReadProgram(device, genx_av1_intra_bdw, sizeof(genx_av1_intra_bdw));
            programRefine64x64      = ReadProgram(device, genx_hevce_refine_me_p_64x64_bdw, sizeof(genx_hevce_refine_me_p_64x64_bdw));
            break;
        case PLATFORM_INTEL_SKL:
        case PLATFORM_INTEL_KBL:
        case PLATFORM_INTEL_GLK:
        case PLATFORM_INTEL_CFL:
        case PLATFORM_INTEL_BXT:
            programPrepareSrc       = ReadProgram(device, genx_av1_prepare_src_skl, sizeof(genx_av1_prepare_src_skl));
            programHmeMe32          = ReadProgram(device, genx_hevce_hme_and_me_p32_4mv_skl, sizeof(genx_hevce_hme_and_me_p32_4mv_skl));
            programMe16Refine32x32  = ReadProgram(device, genx_hevce_me_p16_4mv_and_refine_32x32_skl, sizeof(genx_hevce_me_p16_4mv_and_refine_32x32_skl));
            programMePu             = ReadProgram(device, genx_av1_mepu_skl, sizeof(genx_av1_mepu_skl));
            programMd               = ReadProgram(device, genx_av1_mode_decision_skl, sizeof(genx_av1_mode_decision_skl));
            programMdPass2          = ReadProgram(device, genx_av1_mode_decision_pass2_skl, sizeof(genx_av1_mode_decision_pass2_skl));
            programInterpDecision   = ReadProgram(device, genx_av1_interpolate_decision_skl, sizeof(genx_av1_interpolate_decision_skl));
            programInterpDecisionSingle = ReadProgram(device, genx_av1_interpolate_decision_single_skl, sizeof(genx_av1_interpolate_decision_single_skl));
            programVarTxDecision    = ReadProgram(device, genx_av1_vartx_decision_skl, sizeof(genx_av1_vartx_decision_skl));
            programAv1Intra         = ReadProgram(device, genx_av1_intra_skl, sizeof(genx_av1_intra_skl));
            programRefine64x64      = ReadProgram(device, genx_hevce_refine_me_p_64x64_skl, sizeof(genx_hevce_refine_me_p_64x64_skl));
            break;
        default:
            return MFX_ERR_UNSUPPORTED;
        }


    /* create Cm kernels */
    kernelMe.AddKernel(device, programHmeMe32, "HmeMe32",
        (width16x + 15) / 16, (height16x + 15) / 16, CM_NONE_DEPENDENCY);
    kernelMe.AddKernel(device, programRefine64x64, "RefineMeP64x64",
        (width4x + 15) / 16, (height4x + 15) / 16, CM_NONE_DEPENDENCY, true);

    if ((width + 15) / 16 > CM_MAX_THREADSPACE_WIDTH_FOR_MW) {  // split into 2 parts
        mfxU32 width0 = ((width / 16) >> 1) & ~1;  // must be even for 32x32 blocks
        kernelMe.AddKernel(device, programMe16Refine32x32, "Me16AndRefine32x32",
            width0, height / 16, CM_NONE_DEPENDENCY, false);
        kernelMe.AddKernel(device, programMe16Refine32x32, "Me16AndRefine32x32",
            (width / 16) - width0, height / 16, CM_NONE_DEPENDENCY, false);
    } else {
        kernelMe.AddKernel(device, programMe16Refine32x32, "Me16AndRefine32x32",
            (width + 15) / 16, (height + 15) / 16, CM_NONE_DEPENDENCY, false);
    }


    const int32_t tileWidth = 64;
    const int32_t tileHeight = 64;
    const int32_t mdTileCols = (sourceWidth + tileWidth - 1) / tileWidth;
    const int32_t mdTileCols1x2 = (sourceWidth + (tileWidth * 2) - 1) / (tileWidth * 2);
    const int32_t mdTileRows = (sourceHeight + tileHeight - 1) / tileHeight;
    const int32_t mdTileRows2x1 = (sourceHeight + (tileHeight * 2) - 1) / (tileHeight * 2);
    const int32_t ifTileCols = (sourceWidth + 31) / 32;
    const int32_t ifTileRows = (sourceHeight + 31) / 32;
    const int32_t vartxTileCols = (sourceWidth + 63) / 64;
    const int32_t vartxTileRows = (sourceHeight + 63) / 64;
    kernelMePu.AddKernel(device, programMePu, "MePuGacc8x8And16x16", (width + 15) / 16, (height + 15) / 16, CM_NONE_DEPENDENCY, false);
    kernelMePu.AddKernel(device, programMePu, "MePuGacc32x32", (width + 31) / 32, (height + 31) / 32, CM_NONE_DEPENDENCY, false);
    kernelMePu.AddKernel(device, programMePu, "MePuGacc64x64", (width + 63) / 64, (height + 63) / 64, CM_NONE_DEPENDENCY, false);
    kernelMd.AddKernel(device, programMd, "ModeDecision", mdTileCols1x2, mdTileRows, CM_NONE_DEPENDENCY, true);

    kernelMd.AddKernel(device, programMdPass2, "ModeDecisionPass2", mdTileCols, mdTileRows2x1, CM_NONE_DEPENDENCY, true);
#if USE_HWPAK_RESTRICT
    kernelMd.AddKernel(device, programInterpDecisionSingle, "InterpolateDecisionSingle", ifTileCols, ifTileRows, CM_NONE_DEPENDENCY, true);
#else
    kernelMd.AddKernel(device, programInterpDecision, "InterpolateDecision", ifTileCols, ifTileRows, CM_NONE_DEPENDENCY, true);
#endif


//#if GPU_INTRA_DECISION
    const int32_t intraTileCols = (sourceWidth + 31) >> 5;
    const int32_t intraTileRows = (sourceHeight + 31) >> 5;
    kernelMd.AddKernel(device, programAv1Intra, "CheckIntra", intraTileCols, intraTileRows, CM_NONE_DEPENDENCY, false);
//#endif
    kernelVarTx.AddKernel(device, programVarTxDecision, "VarTxDecision", vartxTileCols, vartxTileRows, CM_NONE_DEPENDENCY, false);

    const char *prepareSrcName = fourcc == MFX_FOURCC_NV12 ? "PrepareSrcNv12" :
                                 fourcc == MFX_FOURCC_NV16 ? "PrepareSrcNv16" :
                                 fourcc == MFX_FOURCC_P010 ? "PrepareSrcP010" :
                                 fourcc == MFX_FOURCC_P210 ? "PrepareSrcP210" : NULL;
    kernelPrepareSrc.AddKernel(device, programPrepareSrc,
        prepareSrcName, width16x / 4, height16x, CM_NONE_DEPENDENCY);

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

    // allocate single surface to store intermedia data in MePuGaccc64x64
    int32_t widthInCtbs  = (width + param->MaxCUSize - 1) / param->MaxCUSize;
    int32_t heightInCtbs = (height+ param->MaxCUSize - 1) / param->MaxCUSize;
    mePuScratchPad = CreateSurface(device, widthInCtbs * 64, heightInCtbs * 64 * 2, CM_SURFACE_FORMAT_A8);

    newMvPred[0] = CreateSurface(device, widthInCtbs * 64, heightInCtbs * 64, CM_SURFACE_FORMAT_A8);
    newMvPred[1] = CreateSurface(device, widthInCtbs * 64, heightInCtbs * 64, CM_SURFACE_FORMAT_A8);
    newMvPred[2] = CreateSurface(device, widthInCtbs * 64, heightInCtbs * 64, CM_SURFACE_FORMAT_A8);
    newMvPred[3] = CreateSurface(device, widthInCtbs * 64, heightInCtbs * 64, CM_SURFACE_FORMAT_A8);

    return MFX_ERR_NONE;
}

// TODO - update
void AV1CmCtx::FreeCmResources()
{
    //device->FlushPrintBuffer();

    device->DestroySurface(curbe);
    device->DestroySurface(mePuScratchPad);
    device->DestroySurface(newMvPred[0]);
    device->DestroySurface(newMvPred[1]);
    device->DestroySurface(newMvPred[2]);
    device->DestroySurface(newMvPred[3]);

    if (lastEventSaved != NULL && lastEventSaved != CM_NO_EVENT)
        queue->DestroyEvent(lastEventSaved);

    kernelPrepareSrc.Destroy();
    kernelPrepareRef.Destroy();
    kernelGradient.Destroy();
    kernelRefine64x64.Destroy();
    kernelRefine32x32sad.Destroy();
    kernelInterpolateFrame.Destroy();

    kernelMe.Destroy();
    kernelMd.Destroy();
    kernelVarTx.Destroy();
    kernelMePu.Destroy();
    kernelDeblock.Destroy();
    kernelFullPostProc.Destroy();

    device->DestroyProgram(programPrepareSrc);
    device->DestroyProgram(programHmeMe32);
    device->DestroyProgram(programMe16Refine32x32);
    device->DestroyProgram(programMePu);
    device->DestroyProgram(programMd);
    device->DestroyProgram(programMdPass2);
    device->DestroyProgram(programInterpDecision);
    device->DestroyProgram(programInterpDecisionSingle);
    device->DestroyProgram(programVarTxDecision);
    device->DestroyProgram(programAv1Intra);
    device->DestroyProgram(programRefine64x64);

    ::DestroyCmDevice(device);
}


/* copy new input frame to GPU (original and downsampled if needed) */
mfxStatus AV1CmCtx::CopyInputFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "CopyInput");
    CmSurface2DUP *inLu = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.luma;
    CmSurface2DUP *inCh = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.chroma;
    mfxFEIH265InputSurface *surf = &((mfxFEIH265Surface *)in->copyArgs.surfVid)->sIn;
    CmSurface2D *x1  = surf->bufOrigNv12;
    CmSurface2D *x2  = surf->bufDown2x;
    CmSurface2D *x4  = surf->bufDown4x;
    CmSurface2D *x8  = surf->bufDown8x;
    CmSurface2D *x16 = surf->bufDown16x;
    mfxU32 copyChroma = in->copyArgs.copyChroma;

    mfxU32 paddingLu = UMC::align_value<uint32_t>(padding<<bppShift,64)>>bppShift;
    mfxU32 paddingCh = UMC::align_value<uint32_t>(paddingChroma<<bppShift,64)>>bppShift;

    SetKernelArg(kernelPrepareSrc.kernel, inLu, inCh, x1, x2, x4, x8, x16, paddingLu, paddingCh, copyChroma);
    kernelPrepareSrc.Enqueue(queue, *lastEvent);
    return MFX_ERR_NONE;
}

/* copy new recon frame to GPU (original and downsampled if needed) */
mfxStatus AV1CmCtx::CopyReconFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "CopyReconAndInterp");
    CmSurface2DUP *inLu = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.luma;
    CmSurface2DUP *inCh = ((mfxFEIH265Surface *)in->copyArgs.surfSys)->sUp.chroma;
    mfxFEIH265ReconSurface *surf = &((mfxFEIH265Surface *)in->copyArgs.surfVid)->sRec;
    CmSurface2D *dummy = surf->bufOrigNv12;
    if (fourcc == MFX_FOURCC_NV12)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, UMC::align_value<int32_t>(padding<<bppShift,64)>>bppShift, UMC::align_value<int32_t>(paddingChroma<<bppShift,64)>>bppShift, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_NV16)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, UMC::align_value<int32_t>(padding<<bppShift,64)>>bppShift, UMC::align_value<int32_t>(paddingChroma<<bppShift,64)>>bppShift, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_P010)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, UMC::align_value<int32_t>(padding<<bppShift,64)>>bppShift, UMC::align_value<int32_t>(paddingChroma<<bppShift,64)>>bppShift, dummy, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);
    else if (fourcc == MFX_FOURCC_P210)
        SetKernelArg(kernelPrepareRef.m_kernel[0], inLu, inCh, UMC::align_value<int32_t>(padding<<bppShift,64)>>bppShift, UMC::align_value<int32_t>(paddingChroma<<bppShift,64)>>bppShift, dummy, dummy, surf->bufOrigNv12, surf->bufDown2x, surf->bufDown4x, surf->bufDown8x, surf->bufDown16x);

    if (enableInterp) {
        // coupling of interp
        if (interpBlocksW > CM_MAX_THREADSPACE_WIDTH_FOR_MW) {
            mfxU32 nW = (interpBlocksW / 2 > CM_MAX_THREADSPACE_WIDTH_FOR_MW) ? 3 : 2;
            mfxU32 nH = (interpBlocksH > CM_MAX_THREADSPACE_HEIGHT_FOR_MW) ? 2 : 1;
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
void * AV1CmCtx::RunVme(mfxFEIH265Input *feiIn, mfxExtFEIH265Output *feiOut)
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
            SetupMeControl(control, width, height, feiIn->meArgs.lambda*512);
            meControl->WriteSurface((mfxU8 *)&control, NULL);
            surfIn->meControlInited = true;
            surfIn->lambda = feiIn->meArgs.lambda;
        }

        SetKernelArg(kernelMe.m_kernel[0], meControl, *refs16x, *refs8x, *refs4x, *refs2x,
            data[MFX_FEI_H265_BLK_64x64], data[MFX_FEI_H265_BLK_32x32], data[MFX_FEI_H265_BLK_16x16]);
        SetKernelArg(kernelMe.m_kernel[1], data[MFX_FEI_H265_BLK_64x64], surfIn->bufOrigNv12, surfRef->bufOrigNv12);
        if (width / 16 > CM_MAX_THREADSPACE_WIDTH_FOR_MW) {  // split into 2 parts
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

    if (feiIn->FEIOp & MFX_FEI_H265_OP_INTER_MD) {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MD");
        CmSurface2D *src  = ((mfxFEIH265Surface *)feiIn->mdArgs.surfSrc)->sIn.bufOrigNv12;
        CmSurface2D *ref0 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfRef0)->sRec.bufOrigNv12;
        CmSurface2D *ref1 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfRef1)->sRec.bufOrigNv12;
        CmBuffer *mdControl = ((mfxFEIH265Surface *)feiIn->mdArgs.surfSrc)->sIn.mdControl;
        CmSurface2DUP *mi1 = ((mfxFEIH265Surface *)feiIn->mdArgs.modeInfo1)->sOut.bufOut;
        CmSurface2DUP *mi2 = ((mfxFEIH265Surface *)feiIn->mdArgs.modeInfo2)->sOut.bufOut;
        CmSurface2DUP *Rs8 = ((mfxFEIH265Surface *)feiIn->mdArgs.Rs8)->sOut.bufOut;
        CmSurface2DUP *Cs8 = ((mfxFEIH265Surface *)feiIn->mdArgs.Cs8)->sOut.bufOut;
        CmSurface2DUP *Rs16 = ((mfxFEIH265Surface *)feiIn->mdArgs.Rs16)->sOut.bufOut;
        CmSurface2DUP *Cs16 = ((mfxFEIH265Surface *)feiIn->mdArgs.Cs16)->sOut.bufOut;
        CmSurface2DUP *Rs32 = ((mfxFEIH265Surface *)feiIn->mdArgs.Rs32)->sOut.bufOut;
        CmSurface2DUP *Cs32 = ((mfxFEIH265Surface *)feiIn->mdArgs.Cs32)->sOut.bufOut;
        CmSurface2DUP *Rs64 = ((mfxFEIH265Surface *)feiIn->mdArgs.Rs64)->sOut.bufOut;
        CmSurface2DUP *Cs64 = ((mfxFEIH265Surface *)feiIn->mdArgs.Cs64)->sOut.bufOut;
        CmSurface2DUP *recLu = ((mfxFEIH265Surface *)feiIn->mdArgs.surfReconSys)->sUp.luma;
        CmSurface2DUP *recCh = ((mfxFEIH265Surface *)feiIn->mdArgs.surfReconSys)->sUp.chroma;
        CmBufferUP *varTxInfo = ((mfxFEIH265Surface *)feiIn->mdArgs.varTxInfo)->sBuf.bufOut;

        CmSurface2D *interp8x8   = newMvPred[0];
        CmSurface2D *interp16x16 = newMvPred[1];
        CmSurface2D *interp32x32 = newMvPred[2];
        CmSurface2D *interp64x64 = newMvPred[3];

        CmSurface2DUP *ref0MeData8x8   = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData0[0])->sOut.bufOut;
        CmSurface2DUP *ref0MeData16x16 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData0[1])->sOut.bufOut;
        CmSurface2DUP *ref0MeData32x32 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData0[2])->sOut.bufOut;
        CmSurface2DUP *ref0MeData64x64 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData0[3])->sOut.bufOut;

        CmSurface2DUP *ref1MeData8x8   = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData1[0])->sOut.bufOut;
        CmSurface2DUP *ref1MeData16x16 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData1[1])->sOut.bufOut;
        CmSurface2DUP *ref1MeData32x32 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData1[2])->sOut.bufOut;
        CmSurface2DUP *ref1MeData64x64 = ((mfxFEIH265Surface *)feiIn->mdArgs.surfInterData1[3])->sOut.bufOut;

        const mfxU32 compoundAllowed = feiIn->mdArgs.compoundAllowed;

        vector<SurfaceIndex,7> refsAndPreds;
        refsAndPreds[0] = GetIndex(ref0);
        refsAndPreds[1] = GetIndex(ref1);
        refsAndPreds[2] = GetIndex(interp8x8);
        refsAndPreds[3] = GetIndex(interp16x16);
        refsAndPreds[4] = GetIndex(interp32x32);
        refsAndPreds[5] = GetIndex(interp64x64);
        refsAndPreds[6] = GetIndex(mePuScratchPad);

        vector<SurfaceIndex,8> meData;
        meData[0] = GetIndex(ref0MeData8x8);
        meData[1] = GetIndex(ref0MeData16x16);
        meData[2] = GetIndex(ref0MeData32x32);
        meData[3] = GetIndex(ref0MeData64x64);
        meData[4] = GetIndex(ref1MeData8x8);
        meData[5] = GetIndex(ref1MeData16x16);
        meData[6] = GetIndex(ref1MeData32x32);
        meData[7] = GetIndex(ref1MeData64x64);

        vector<SurfaceIndex,2> mi;
        mi[0] = GetIndex(mi1);
        mi[1] = GetIndex(mi2);

        vector<SurfaceIndex, 8> RsCs;
        RsCs[0] = GetIndex(Rs8);
        RsCs[1] = GetIndex(Cs8);
        RsCs[2] = GetIndex(Rs16);
        RsCs[3] = GetIndex(Cs16);
        RsCs[4] = GetIndex(Rs32);
        RsCs[5] = GetIndex(Cs32);
        RsCs[6] = GetIndex(Rs64);
        RsCs[7] = GetIndex(Cs64);
        vector<SurfaceIndex, 4> RsCs3264;
        RsCs3264[0] = GetIndex(Rs32);
        RsCs3264[1] = GetIndex(Cs32);
        RsCs3264[2] = GetIndex(Rs64);
        RsCs3264[3] = GetIndex(Cs64);

        mdControl->WriteSurface(feiIn->mdArgs.param, nullptr);

        mfxU32 value;
        size_t sizeOfValue = sizeof(value);
        device->GetCaps(CAP_KERNEL_COUNT_PER_TASK, sizeOfValue, &value);

        mfxU32 SliceQpY = feiIn->SliceQpY;
        mfxU32 qpLayer = (feiIn->SliceQpY<<2) + feiIn->PyramidLayer;
        mfxU32 PyramidLayer = feiIn->PyramidLayer;
        mfxU32 TemporalSync = feiIn->TemporalSync;

        SetKernelArg(kernelMePu.m_kernel[0], src, ref0, ref1, ref0MeData8x8, ref1MeData8x8, ref0MeData16x16, ref1MeData16x16, interp8x8, interp16x16, compoundAllowed);
        SetKernelArg(kernelMePu.m_kernel[1], src, ref0, ref1, ref0MeData32x32, ref1MeData32x32, interp32x32, compoundAllowed);
        SetKernelArg(kernelMePu.m_kernel[2], src, ref0, ref1, ref0MeData64x64, ref1MeData64x64, interp64x64, compoundAllowed, mePuScratchPad);
        kernelMePu.Enqueue(queue, CM_NO_EVENT);
        SetKernelArg(kernelMd.m_kernel[0], mdControl, src, refsAndPreds, meData, mi1, RsCs3264, qpLayer);
#if 1
        //TRYINTRA_ORIG
        SetKernelArg(kernelMd.m_kernel[1], mdControl, src, refsAndPreds, mi, RsCs, SliceQpY, PyramidLayer, TemporalSync, meData);
#else
        SetKernelArg(kernelMd.m_kernel[1], mdControl, src, refsAndPreds, mi, RsCs, SliceQpY, PyramidLayer, TemporalSync);
#endif
        SetKernelArg(kernelMd.m_kernel[2], mdControl, src, refsAndPreds, mi2, recLu, recCh, (mfxI32)UMC::align_value<int32_t>(padding, 64));

//#if GPU_INTRA_DECISION
        const mfxU32 miCols = ((MdControl*)feiIn->mdArgs.param)->miCols;
        const mfxU32 miRows = ((MdControl*)feiIn->mdArgs.param)->miRows;
        const mfxU32 lambda = (((MdControl*)feiIn->mdArgs.param)->lambdaInt + 2) >> 2;
        const mfxU32 earlyExit = (feiIn->IsRef) ? 0 : 1;
        SetKernelArg(kernelMd.m_kernel[3], src, mi2, miCols, miRows, lambda, earlyExit);
//#endif
        kernelMd.Enqueue(queue, CM_NO_EVENT);

        SetKernelArg(kernelVarTx.m_kernel[0], mdControl, src, recLu, mi2, mePuScratchPad, varTxInfo, (mfxI32)UMC::align_value<int32_t>(padding, 64));
        kernelVarTx.Enqueue(queue, lastEvent);

        //if (lastEvent->WaitForTaskFinished() != CM_SUCCESS)
        //    printf("WaitForTaskFinished failed\n");
        //if (device->FlushPrintBuffer() != CM_SUCCESS)
        //    printf("FlushPrintBuffer failed\n");
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

    saveSyncPoint = feiIn->SaveSyncPoint;
    if (lastEvent)
        lastEventSaved = lastEvent;

    //fprintf(stderr, "Finish saved = %d, addr = 0x%p\n", saveSyncPoint, lastEventSaved);
    return lastEventSaved;
}

/* assume wait time = -1 means wait forever (docs not clear)
 * plugin uses const 0xFFFFFFFF (-1 signed) since CompleteFrameH265FEIRoutine() is blocking (scheduler waits for user specified time)
 */
mfxStatus AV1CmCtx::SyncCurrent(void *syncp, mfxU32 wait)
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
mfxStatus AV1CmCtx::DestroySavedSyncPoint(void *syncp)
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
    AV1CmCtx *hcm = new AV1CmCtx;

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
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    hcm->FreeCmResources();

    delete hcm;

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_ProcessFrameAsync(mfxFEIH265 feih265, mfxFEIH265Input *in, mfxExtFEIH265Output *out, mfxFEISyncPoint *syncp)
{
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *syncp = (mfxFEISyncPoint)hcm->RunVme(in, out);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_SyncOperation(mfxFEIH265 feih265, mfxFEISyncPoint syncp, mfxU32 wait)
{
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_SyncOperation");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    return hcm->SyncCurrent((mfxSyncPoint)syncp, wait);
}

/* internal only - allow sync objects to be saved and synced on/destroyed out of order */
MSDK_PLUGIN_API(mfxStatus) H265FEI_DestroySavedSyncPoint(mfxFEIH265 feih265, mfxFEISyncPoint syncp)
{
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

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
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateInputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pInSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_UP, sysMemLu, sysMemCh, NULL);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateInputSurface(mfxFEIH265 feih265, mfxHDL *pInSurf)
{
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateInputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pInSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_INPUT, NULL, NULL, NULL);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateReconSurface(mfxFEIH265 feih265, mfxHDL *pRecSurf)
{
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateReconSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pRecSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_RECON, NULL, NULL, NULL);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateOutputSurface(mfxFEIH265 feih265, mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *pOutSurf)
{
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_AllocateOutputSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    *pOutSurf = hcm->AllocateSurface(MFX_FEI_H265_SURFTYPE_OUTPUT, sysMem, NULL, surfInfo);

    return MFX_ERR_NONE;
}

MSDK_PLUGIN_API(mfxStatus) H265FEI_AllocateOutputBuffer(mfxFEIH265 feih265, mfxU8 *sysMem, mfxU32 size, mfxHDL *pOutBuf)
{
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

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
    AV1CmCtx *hcm = (AV1CmCtx *)feih265;

    //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "H265FEI_FreeSurface");

    if (!hcm)
        return MFX_ERR_INVALID_HANDLE;

    return hcm->FreeSurface((mfxFEIH265Surface *)pSurf);
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
