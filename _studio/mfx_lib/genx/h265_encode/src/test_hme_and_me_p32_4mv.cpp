/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015 Intel Corporation. All Rights Reserved.
//
*/

#include "stdio.h"
#include "time.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "vector"
#include "../include/test_common.h"
#include "../include/genx_hevce_hme_and_me_p32_4mv_bdw_isa.h"
#include "../include/genx_hevce_hme_and_me_p32_4mv_hsw_isa.h"
#include "../include/genx_hevce_me_p32_4mv_bdw_isa.h"
#include "../include/genx_hevce_me_p32_4mv_hsw_isa.h"
#include "../include/genx_hevce_ime_3tiers_4mv_bdw_isa.h"
#include "../include/genx_hevce_ime_3tiers_4mv_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void Ime3Tiers4Mv(SurfaceIndex CTRL, SurfaceIndex SRCREF_16X, SurfaceIndex SRCREF_8X, SurfaceIndex SRCREF_4X, SurfaceIndex MV_OUT);
extern "C" void MeP32_4MV(SurfaceIndex CTRL, SurfaceIndex SRCREF,
    SurfaceIndex MV32x32, SurfaceIndex MV16x16, SurfaceIndex MV32x16, SurfaceIndex MV16x32, int rectParts);
extern "C" void HmeMe32(SurfaceIndex CTRL16x, SurfaceIndex CTRL2x, SurfaceIndex SRCREF,
    SurfaceIndex MV32x32, SurfaceIndex MV16x16, SurfaceIndex MV32x16, SurfaceIndex MV16x32, int rectParts);
#endif //CMRT_EMU

namespace {
    int RunGpu(const Me2xControl &ctrl16x, const Me2xControl &ctrl2x, const mfxU8 *src16xData, const mfxU8 *ref16xData, const mfxU8 *src8xData, const mfxU8 *ref8xData,
            const mfxU8 *src4xData, const mfxU8 *ref4xData, const mfxU8 *src2xData, const mfxU8 *ref2xData, mfxI16Pair *mv64x64, mfxI16Pair *mv32x32, mfxI16Pair *mv16x16);
    int RunGpuHme(const Me2xControl &ctrl, const mfxU8 *src16xData, const mfxU8 *ref16xData,
        const mfxU8 *src8xData, const mfxU8 *ref8xData, const mfxU8 *src4xData, const mfxU8 *ref4xData, mfxI16Pair *mvData, mfxU16 width, mfxU16 height);
    int RunGpuMe32(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxI16Pair *mv32x32Data, mfxI16Pair *mv16x16Data, mfxI16Pair *mv32x16Data, mfxI16Pair *mv16x32Data);
    int CompareMV(const mfxI16Pair *data1, const mfxI16Pair *data2, mfxU32 width, mfxU32 height);
    int CompareMV64(const mfxI16Pair *data1, const mfxI16Pair *data2, mfxU32 width, mfxU32 height);
};

int DownSize2X(mfxU8 *src1x, int w1x, int h1x, mfxU8 *dst2x, int w2x, int h2x)
{
    if (w1x/2 > w2x) return 1;
    if (h1x/2 > h2x) return 1;

    for (int i = 0; i < h1x; i+=2)
        for (int j = 0; j < w1x; j+=2)
            dst2x[(i / 2) * w2x + (j / 2)] = src1x[i * w1x + j];

    return 0;
}

int main()
{
    mfxU16 width1x = ROUNDUP(WIDTH, 16);
    mfxU16 width2x = ROUNDUP(WIDTH / 2, 16);
    mfxU16 width4x = ROUNDUP(WIDTH / 4, 16);
    mfxU16 width8x = ROUNDUP(WIDTH / 8, 16);
    mfxU16 width16x = ROUNDUP(WIDTH / 16, 16);
    mfxU16 height1x = ROUNDUP(HEIGHT, 16);
    mfxU16 height2x = ROUNDUP(HEIGHT / 2, 16);
    mfxU16 height4x = ROUNDUP(HEIGHT / 4, 16);
    mfxU16 height8x = ROUNDUP(HEIGHT / 8, 16);
    mfxU16 height16x = ROUNDUP(HEIGHT / 16, 16);
    int frameSize1x = width1x * height1x * 3 / 2;
    int frameSize2x = width2x * height2x * 3 / 2;
    int frameSize4x = width4x * height4x * 3 / 2;
    int frameSize8x = width8x * height8x * 3 / 2;
    int frameSize16x = width16x * height16x * 3 / 2;

    std::vector<mfxU8> src1x(frameSize1x);
    std::vector<mfxU8> ref1x(frameSize1x);
    std::vector<mfxU8> src2x(frameSize2x);
    std::vector<mfxU8> ref2x(frameSize2x);
    std::vector<mfxU8> src4x(frameSize4x);
    std::vector<mfxU8> ref4x(frameSize4x);
    std::vector<mfxU8> src8x(frameSize8x);
    std::vector<mfxU8> ref8x(frameSize8x);
    std::vector<mfxU8> src16x(frameSize16x);
    std::vector<mfxU8> ref16x(frameSize16x);
    std::vector<mfxI16Pair> mvGpu64x64(DIVUP(width2x, 32) * DIVUP(height2x, 32)); // 1 mv per 64x64 block
    std::vector<mfxI16Pair> mvGpu32x32(DIVUP(width2x, 16) * DIVUP(height2x, 16)); // 1 mv per 32x32 block
    std::vector<mfxI16Pair> mvGpu16x16(DIVUP(width2x, 8) * DIVUP(height2x, 8)); // 1 mv per 16x16 block
    std::vector<mfxI16Pair> mv(DIVUP(width2x, 16) * DIVUP(height2x, 16)); // 1 mv per 32x32 block

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src1x.data(), 1, src1x.size(), f) != src1x.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref1x.data(), 1, ref1x.size(), f) != ref1x.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    int res = 0;
    if (DownSize2X(src1x.data(), width1x, height1x, src2x.data(), width2x, height2x))
        return printf("FAILED to downscale src 2X\n"), 1;
    if (DownSize2X(src2x.data(), width2x, height2x, src4x.data(), width4x, height4x))
        return printf("FAILED to downscale src 4X\n"), 1;
    if (DownSize2X(src4x.data(), width4x, height4x, src8x.data(), width8x, height8x))
        return printf("FAILED to downscale src 8X\n"), 1;
    if (DownSize2X(src8x.data(), width8x, height8x, src16x.data(), width16x, height16x))
        return printf("FAILED to downscale src 16X\n"), 1;
    if (DownSize2X(ref1x.data(), width1x, height1x, ref2x.data(), width2x, height2x))
        return printf("FAILED to downscale ref 2X\n"), 1;
    if (DownSize2X(ref2x.data(), width2x, height2x, ref4x.data(), width4x, height4x))
        return printf("FAILED to downscale ref 4X\n"), 1;
    if (DownSize2X(ref4x.data(), width4x, height4x, ref8x.data(), width8x, height8x))
        return printf("FAILED to downscale ref 8X\n"), 1;
    if (DownSize2X(ref8x.data(), width8x, height8x, ref16x.data(), width16x, height16x))
        return printf("FAILED to downscale ref 16X\n"), 1;

    Me2xControl ctrl16x = {};
    SetupMeControl(ctrl16x, width16x, height16x);
    Me2xControl ctrl2x = {};
    SetupMeControlShortPath(ctrl2x, width2x, height2x);

    res = RunGpu(ctrl16x, ctrl2x, src16x.data(), ref16x.data(), src8x.data(), ref8x.data(), src4x.data(), ref4x.data(), src2x.data(), ref2x.data(), mvGpu64x64.data(), mvGpu32x32.data(), mvGpu16x16.data());
    CHECK_ERR(res);

    res = RunGpuHme(ctrl16x, src16x.data(), ref16x.data(), src8x.data(), ref8x.data(), src4x.data(), ref4x.data(), mv.data(), width2x, height2x);
    CHECK_ERR(res);

    std::vector<mfxI16Pair> mv32x32(DIVUP(width2x,16) * DIVUP(height2x,16));
    std::vector<mfxI16Pair> mv32x16(DIVUP(width2x,16) * DIVUP(height2x,8));
    std::vector<mfxI16Pair> mv16x32(DIVUP(width2x,8) * DIVUP(height2x,16));
    std::vector<mfxI16Pair> mv16x16(DIVUP(width2x,8) * DIVUP(height2x,8));

    res = RunGpuMe32(ctrl2x, src2x.data(), ref2x.data(), mv.data(), mv32x32.data(), mv16x16.data(), mv32x16.data(), mv16x32.data());
    CHECK_ERR(res);

    //res = CompareMV64(mvGpu64x64.data(), mvGpu32x32.data(), ctrl2x.width, ctrl2x.height);
    //CHECK_ERR(res);

    res = CompareMV(mv32x32.data(), mvGpu32x32.data(), ctrl2x.width / 16, ctrl2x.height /16);
    CHECK_ERR(res);

    res = CompareMV(mv16x16.data(), mvGpu16x16.data(), ctrl2x.width / 8, ctrl2x.height /8);
    CHECK_ERR(res);

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(const Me2xControl &ctrl16x, const Me2xControl &ctrl2x, const mfxU8 *src16xData, const mfxU8 *ref16xData, const mfxU8 *src8xData, const mfxU8 *ref8xData,
        const mfxU8 *src4xData, const mfxU8 *ref4xData, const mfxU8 *src2xData, const mfxU8 *ref2xData, mfxI16Pair *mv64x64Data, mfxI16Pair *mv32x32Data, mfxI16Pair *mv16x16Data)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_hme_and_me_p32_4mv_hsw, sizeof(genx_hevce_hme_and_me_p32_4mv_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(HmeMe32), kernel);
        CHECK_CM_ERR(res);

        CmSurface2D *src16x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl2x.width/8, 16), ROUNDUP(ctrl2x.height/8, 16), CM_SURFACE_FORMAT_NV12, src16x);
        CHECK_CM_ERR(res);
        res = src16x->WriteSurfaceStride(src16xData, NULL, ROUNDUP(ctrl2x.width/8, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref16x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl2x.width/8, 16), ROUNDUP(ctrl2x.height/8, 16), CM_SURFACE_FORMAT_NV12, ref16x);
        CHECK_CM_ERR(res);
        res = ref16x->WriteSurfaceStride(ref16xData, NULL, ROUNDUP(ctrl2x.width/8, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs16x = 0;
        res = device->CreateVmeSurfaceG7_5(src16x, &ref16x, NULL, 1, 0, genxRefs16x);
        CHECK_CM_ERR(res);

        CmSurface2D *src8x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl2x.width/4, 16), ROUNDUP(ctrl2x.height/4, 16), CM_SURFACE_FORMAT_NV12, src8x);
        CHECK_CM_ERR(res);
        res = src8x->WriteSurfaceStride(src8xData, NULL, ROUNDUP(ctrl2x.width/4, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref8x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl2x.width/4, 16), ROUNDUP(ctrl2x.height/4, 16), CM_SURFACE_FORMAT_NV12, ref8x);
        CHECK_CM_ERR(res);
        res = ref8x->WriteSurfaceStride(ref8xData, NULL, ROUNDUP(ctrl2x.width/4, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs8x = 0;
        res = device->CreateVmeSurfaceG7_5(src8x, &ref8x, NULL, 1, 0, genxRefs8x);
        CHECK_CM_ERR(res);

        CmSurface2D *src4x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl2x.width/2, 16), ROUNDUP(ctrl2x.height/2, 16), CM_SURFACE_FORMAT_NV12, src4x);
        CHECK_CM_ERR(res);
        res = src4x->WriteSurfaceStride(src4xData, NULL, ROUNDUP(ctrl2x.width/2, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref4x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl2x.width/2, 16), ROUNDUP(ctrl2x.height/2, 16), CM_SURFACE_FORMAT_NV12, ref4x);
        CHECK_CM_ERR(res);
        res = ref4x->WriteSurfaceStride(ref4xData, NULL, ROUNDUP(ctrl2x.width/2, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs4x = 0;
        res = device->CreateVmeSurfaceG7_5(src4x, &ref4x, NULL, 1, 0, genxRefs4x);
        CHECK_CM_ERR(res);

        CmSurface2D *src2x = 0;
        res = device->CreateSurface2D(ctrl2x.width, ctrl2x.height, CM_SURFACE_FORMAT_NV12, src2x);
        CHECK_CM_ERR(res);
        res = src2x->WriteSurfaceStride(src2xData, NULL, ctrl2x.width);
        CHECK_CM_ERR(res);
        CmSurface2D *ref2x = 0;
        res = device->CreateSurface2D(ctrl2x.width, ctrl2x.height, CM_SURFACE_FORMAT_NV12, ref2x);
        CHECK_CM_ERR(res);
        res = ref2x->WriteSurfaceStride(ref2xData, NULL, ctrl2x.width);
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs2x = 0;
        res = device->CreateVmeSurfaceG7_5(src2x, &ref2x, NULL, 1, 0, genxRefs2x);
        CHECK_CM_ERR(res);

        Ipp32u mv64x64Pitch = 0;
        Ipp32u mv64x64Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl2x.width, 32) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 32), CM_SURFACE_FORMAT_P8, mv64x64Pitch, mv64x64Size);
        CHECK_CM_ERR(res);
        void *mv64x64Sys = CM_ALIGNED_MALLOC(mv64x64Size, 0x1000);
        CmSurface2DUP *mv64x64 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl2x.width, 32) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 32), CM_SURFACE_FORMAT_P8, mv64x64Sys, mv64x64);
        CHECK_CM_ERR(res);

        Ipp32u mv32x32Pitch = 0;
        Ipp32u mv32x32Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl2x.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 16), CM_SURFACE_FORMAT_P8, mv32x32Pitch, mv32x32Size);
        CHECK_CM_ERR(res);
        void *mv32x32Sys = CM_ALIGNED_MALLOC(mv32x32Size, 0x1000);
        CmSurface2DUP *mv32x32 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl2x.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 16), CM_SURFACE_FORMAT_P8, mv32x32Sys, mv32x32);
        CHECK_CM_ERR(res);

        Ipp32u mv32x16Pitch = 0;
        Ipp32u mv32x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl2x.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 8), CM_SURFACE_FORMAT_P8, mv32x16Pitch, mv32x16Size);
        CHECK_CM_ERR(res);
        void *mv32x16Sys = CM_ALIGNED_MALLOC(mv32x16Size, 0x1000);
        CmSurface2DUP *mv32x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl2x.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 8), CM_SURFACE_FORMAT_P8, mv32x16Sys, mv32x16);
        CHECK_CM_ERR(res);

        Ipp32u mv16x32Pitch = 0;
        Ipp32u mv16x32Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl2x.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 16), CM_SURFACE_FORMAT_P8, mv16x32Pitch, mv16x32Size);
        CHECK_CM_ERR(res);
        void *mv16x32Sys = CM_ALIGNED_MALLOC(mv16x32Size, 0x1000);
        CmSurface2DUP *mv16x32 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl2x.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 16), CM_SURFACE_FORMAT_P8, mv16x32Sys, mv16x32);
        CHECK_CM_ERR(res);

        Ipp32u mv16x16Pitch = 0;
        Ipp32u mv16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl2x.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 8), CM_SURFACE_FORMAT_P8, mv16x16Pitch, mv16x16Size);
        CHECK_CM_ERR(res);
        void *mv16x16Sys = CM_ALIGNED_MALLOC(mv16x16Size, 0x1000);
        CmSurface2DUP *mv16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl2x.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl2x.height, 8), CM_SURFACE_FORMAT_P8, mv16x16Sys, mv16x16);
        CHECK_CM_ERR(res);

        CmBuffer *ctrl16xBuf = 0;
        res = device->CreateBuffer(sizeof(Me2xControl), ctrl16xBuf);
        CHECK_CM_ERR(res);
        res = ctrl16xBuf->WriteSurface((const Ipp8u *)&ctrl16x, NULL, sizeof(Me2xControl));
        CHECK_CM_ERR(res);

        SurfaceIndex *idxctrl16x = 0;
        res = ctrl16xBuf->GetIndex(idxctrl16x);
        CHECK_CM_ERR(res);

        CmBuffer *ctrl2xBuf = 0;
        res = device->CreateBuffer(sizeof(Me2xControl), ctrl2xBuf);
        CHECK_CM_ERR(res);
        res = ctrl2xBuf->WriteSurface((const Ipp8u *)&ctrl2x, NULL, sizeof(Me2xControl));
        CHECK_CM_ERR(res);

        SurfaceIndex *idxctrl2x = 0;
        res = ctrl2xBuf->GetIndex(idxctrl2x);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxMv64x64 = 0;
        res = mv64x64->GetIndex(idxMv64x64);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv32x32 = 0;
        res = mv32x32->GetIndex(idxMv32x32);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv32x16 = 0;
        res = mv32x16->GetIndex(idxMv32x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv16x32 = 0;
        res = mv16x32->GetIndex(idxMv16x32);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv16x16 = 0;
        res = mv16x16->GetIndex(idxMv16x16);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxctrl16x), idxctrl16x);   // 16x size
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*idxctrl2x), idxctrl2x);   // 2x size
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*genxRefs16x), genxRefs16x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*genxRefs8x), genxRefs8x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(4, sizeof(*genxRefs4x), genxRefs4x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(*genxRefs2x), genxRefs2x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(6, sizeof(*idxMv64x64), idxMv64x64);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(7, sizeof(*idxMv32x32), idxMv32x32);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(8, sizeof(*idxMv16x16), idxMv16x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(9, sizeof(*idxMv32x16), idxMv32x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(10, sizeof(*idxMv16x32), idxMv16x32);
        CHECK_CM_ERR(res);
        int rectParts = 0;
        res = kernel->SetKernelArg(11, sizeof(rectParts), &rectParts);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = DIVUP(ctrl16x.width, 16);    // 16x blocks
        mfxU32 tsHeight = DIVUP(ctrl16x.height, 16);
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        CmThreadSpace * threadSpace = 0;
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);

        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);

        CmTask * task = 0;
        res = device->CreateTask(task);
        CHECK_CM_ERR(res);

        res = task->AddKernel(kernel);
        CHECK_CM_ERR(res);

        CmQueue *queue = 0;
        res = device->CreateQueue(queue);
        CHECK_CM_ERR(res);

        CmEvent * e = 0;
        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);

        res = e->WaitForTaskFinished();
        CHECK_CM_ERR(res);

        for (int y = 0; y < DIVUP(ctrl2x.height, 32); y++)
            memcpy(mv64x64Data + y * DIVUP(ctrl2x.width, 32), (char *)mv64x64Sys + y * mv64x64Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl2x.width, 32));
        for (int y = 0; y < DIVUP(ctrl2x.height, 16); y++)
            memcpy(mv32x32Data + y * DIVUP(ctrl2x.width, 16), (char *)mv32x32Sys + y * mv32x32Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl2x.width, 16));
        for (int y = 0; y < DIVUP(ctrl2x.height, 8); y++)
            memcpy(mv16x16Data + y * DIVUP(ctrl2x.width, 8), (char *)mv16x16Sys + y * mv16x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl2x.width, 8));
        //for (int y = 0; y < DIVUP(ctrl2x.height, 8); y++)
        //    memcpy(mv32x16Data + y * DIVUP(ctrl2x.width, 16), (char *)mv32x16Sys + y * mv32x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl2x.width, 16));
        //for (int y = 0; y < DIVUP(ctrl2x.height, 16); y++)
        //    memcpy(mv16x32Data + y * DIVUP(ctrl2x.width, 8), (char *)mv16x32Sys + y * mv16x32Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl2x.width, 8));

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs16x);
        device->DestroyVmeSurfaceG7_5(genxRefs8x);
        device->DestroyVmeSurfaceG7_5(genxRefs4x);
        device->DestroyVmeSurfaceG7_5(genxRefs2x);
        device->DestroySurface(ctrl16xBuf);
        device->DestroySurface(ctrl2xBuf);
        device->DestroySurface(src16x);
        device->DestroySurface(src8x);
        device->DestroySurface(src4x);
        device->DestroySurface(src2x);
        device->DestroySurface(ref16x);
        device->DestroySurface(ref8x);
        device->DestroySurface(ref4x);
        device->DestroySurface(ref2x);
        device->DestroySurface2DUP(mv64x64);
        device->DestroySurface2DUP(mv32x32);
        device->DestroySurface2DUP(mv32x16);
        device->DestroySurface2DUP(mv16x32);
        device->DestroySurface2DUP(mv16x16);
        CM_ALIGNED_FREE(mv64x64Sys);
        CM_ALIGNED_FREE(mv32x32Sys);
        CM_ALIGNED_FREE(mv32x16Sys);
        CM_ALIGNED_FREE(mv16x32Sys);
        CM_ALIGNED_FREE(mv16x16Sys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }

    int RunGpuHme(const Me2xControl &ctrl, const mfxU8 *src16xData, const mfxU8 *ref16xData,
        const mfxU8 *src8xData, const mfxU8 *ref8xData, const mfxU8 *src4xData, const mfxU8 *ref4xData, mfxI16Pair *mvData, mfxU16 width, mfxU16 height)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_ime_3tiers_4mv_hsw, sizeof(genx_hevce_ime_3tiers_4mv_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(Ime3Tiers4Mv), kernel);
        CHECK_CM_ERR(res);

        // ctrl: input is 16x
        CmBuffer *ctrlBuf = 0;
        res = device->CreateBuffer(sizeof(Me2xControl), ctrlBuf);
        CHECK_CM_ERR(res);
        res = ctrlBuf->WriteSurface((const Ipp8u *)&ctrl, NULL, sizeof(Me2xControl));
        CHECK_CM_ERR(res);

        CmSurface2D *src16x = 0;
        res = device->CreateSurface2D(ROUNDUP(width/8, 16), ROUNDUP(height/8, 16), CM_SURFACE_FORMAT_NV12, src16x);
        CHECK_CM_ERR(res);
        res = src16x->WriteSurfaceStride(src16xData, NULL, ROUNDUP(width/8, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref16x = 0;
        res = device->CreateSurface2D(ROUNDUP(width/8, 16), ROUNDUP(height/8, 16), CM_SURFACE_FORMAT_NV12, ref16x);
        CHECK_CM_ERR(res);
        res = ref16x->WriteSurfaceStride(ref16xData, NULL, ROUNDUP(width/8, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs16x = 0;
        res = device->CreateVmeSurfaceG7_5(src16x, &ref16x, NULL, 1, 0, genxRefs16x);
        CHECK_CM_ERR(res);

        CmSurface2D *src8x = 0;
        res = device->CreateSurface2D(ROUNDUP(width/4, 16), ROUNDUP(height/4, 16), CM_SURFACE_FORMAT_NV12, src8x);
        CHECK_CM_ERR(res);
        res = src8x->WriteSurfaceStride(src8xData, NULL, ROUNDUP(width/4, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref8x = 0;
        res = device->CreateSurface2D(ROUNDUP(width/4, 16), ROUNDUP(height/4, 16), CM_SURFACE_FORMAT_NV12, ref8x);
        CHECK_CM_ERR(res);
        res = ref8x->WriteSurfaceStride(ref8xData, NULL, ROUNDUP(width/4, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs8x = 0;
        res = device->CreateVmeSurfaceG7_5(src8x, &ref8x, NULL, 1, 0, genxRefs8x);
        CHECK_CM_ERR(res);

        CmSurface2D *src4x = 0;
        res = device->CreateSurface2D(ROUNDUP(width/2, 16), ROUNDUP(height/2, 16), CM_SURFACE_FORMAT_NV12, src4x);
        CHECK_CM_ERR(res);
        res = src4x->WriteSurfaceStride(src4xData, NULL, ROUNDUP(width/2, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref4x = 0;
        res = device->CreateSurface2D(ROUNDUP(width/2, 16), ROUNDUP(height/2, 16), CM_SURFACE_FORMAT_NV12, ref4x);
        CHECK_CM_ERR(res);
        res = ref4x->WriteSurfaceStride(ref4xData, NULL, ROUNDUP(width/2, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs4x = 0;
        res = device->CreateVmeSurfaceG7_5(src4x, &ref4x, NULL, 1, 0, genxRefs4x);
        CHECK_CM_ERR(res);

        int mvSurfWidth = DIVUP(width, 16);    // 32x32 blocks
        int mvSurfHeight = DIVUP(height, 16);
        Ipp32u mvSurfPitch = 0;
        Ipp32u mvSurfSize = 0;
        res = device->GetSurface2DInfo(mvSurfWidth * sizeof(mfxI16Pair), mvSurfHeight, CM_SURFACE_FORMAT_P8, mvSurfPitch, mvSurfSize);
        CHECK_CM_ERR(res);
        void *mvSys = CM_ALIGNED_MALLOC(mvSurfSize, 0x1000);
        CmSurface2DUP *mv = 0;
        res = device->CreateSurface2DUP(mvSurfWidth * sizeof(mfxI16Pair), mvSurfHeight, CM_SURFACE_FORMAT_P8, mvSys, mv);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxCtrl = 0;
        res = ctrlBuf->GetIndex(idxCtrl);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxMv = 0;
        res = mv->GetIndex(idxMv);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxCtrl), idxCtrl);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*genxRefs16x), genxRefs16x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*genxRefs8x), genxRefs8x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*genxRefs4x), genxRefs4x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(4, sizeof(*idxMv), idxMv);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = DIVUP(ctrl.width, 16);
        mfxU32 tsHeight = DIVUP(ctrl.height, 16);
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        CmThreadSpace * threadSpace = 0;
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);

        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);

        CmTask * task = 0;
        res = device->CreateTask(task);
        CHECK_CM_ERR(res);

        res = task->AddKernel(kernel);
        CHECK_CM_ERR(res);

        CmQueue *queue = 0;
        res = device->CreateQueue(queue);
        CHECK_CM_ERR(res);

        CmEvent * e = 0;
        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);

        res = e->WaitForTaskFinished();
        CHECK_CM_ERR(res);

        for (int y = 0; y < mvSurfHeight; y++)
            for (int x = 0; x < mvSurfWidth; x++)
                mvData[y * mvSurfWidth + x] = *((mfxI16Pair *)((char *)mvSys + y * mvSurfPitch) + x);

#ifndef CMRT_EMU
        printf("TIME(Ime_3tiers)=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs16x);
        device->DestroyVmeSurfaceG7_5(genxRefs8x);
        device->DestroyVmeSurfaceG7_5(genxRefs4x);
        device->DestroySurface(ctrlBuf);
        device->DestroySurface(src16x);
        device->DestroySurface(src8x);
        device->DestroySurface(src4x);
        device->DestroySurface(ref16x);
        device->DestroySurface(ref8x);
        device->DestroySurface(ref4x);
        device->DestroySurface2DUP(mv);
        CM_ALIGNED_FREE(mvSys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }

    int RunGpuMe32(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
                   mfxI16Pair *mv32x32Data, mfxI16Pair *mv16x16Data, mfxI16Pair *mv32x16Data, mfxI16Pair *mv16x32Data)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_me_p32_4mv_hsw, sizeof(genx_hevce_me_p32_4mv_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MeP32_4MV), kernel);
        CHECK_CM_ERR(res);

        CmBuffer *ctrlBuf = 0;
        res = device->CreateBuffer(sizeof(Me2xControl), ctrlBuf);
        CHECK_CM_ERR(res);
        res = ctrlBuf->WriteSurface((const Ipp8u *)&ctrl, NULL, sizeof(Me2xControl));
        CHECK_CM_ERR(res);

        CmSurface2D *src = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, src);
        CHECK_CM_ERR(res);
        res = src->WriteSurfaceStride(srcData, NULL, ctrl.width);
        CHECK_CM_ERR(res);

        CmSurface2D *ref = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, ref);
        CHECK_CM_ERR(res);
        res = ref->WriteSurfaceStride(refData, NULL, ctrl.width);
        CHECK_CM_ERR(res);

        SurfaceIndex *genxRefs = 0;
        res = device->CreateVmeSurfaceG7_5(src, &ref, NULL, 1, 0, genxRefs);
        CHECK_CM_ERR(res);

        Ipp32u mv32x32Pitch = 0;
        Ipp32u mv32x32Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv32x32Pitch, mv32x32Size);
        CHECK_CM_ERR(res);
        void *mv32x32Sys = CM_ALIGNED_MALLOC(mv32x32Size, 0x1000);
        CmSurface2DUP *mv32x32 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv32x32Sys, mv32x32);
        CHECK_CM_ERR(res);

        Ipp32u mv32x16Pitch = 0;
        Ipp32u mv32x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv32x16Pitch, mv32x16Size);
        CHECK_CM_ERR(res);
        void *mv32x16Sys = CM_ALIGNED_MALLOC(mv32x16Size, 0x1000);
        CmSurface2DUP *mv32x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv32x16Sys, mv32x16);
        CHECK_CM_ERR(res);

        Ipp32u mv16x32Pitch = 0;
        Ipp32u mv16x32Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x32Pitch, mv16x32Size);
        CHECK_CM_ERR(res);
        void *mv16x32Sys = CM_ALIGNED_MALLOC(mv16x32Size, 0x1000);
        CmSurface2DUP *mv16x32 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x32Sys, mv16x32);
        CHECK_CM_ERR(res);

        Ipp32u mv16x16Pitch = 0;
        Ipp32u mv16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x16Pitch, mv16x16Size);
        CHECK_CM_ERR(res);
        void *mv16x16Sys = CM_ALIGNED_MALLOC(mv16x16Size, 0x1000);
        CmSurface2DUP *mv16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x16Sys, mv16x16);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxCtrl = 0;
        res = ctrlBuf->GetIndex(idxCtrl);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxMv32x32 = 0;
        res = mv32x32->GetIndex(idxMv32x32);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv32x16 = 0;
        res = mv32x16->GetIndex(idxMv32x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv16x32 = 0;
        res = mv16x32->GetIndex(idxMv16x32);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv16x16 = 0;
        res = mv16x16->GetIndex(idxMv16x16);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxCtrl), idxCtrl);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*genxRefs), genxRefs);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxMv32x32), idxMv32x32);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idxMv16x16), idxMv16x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(4, sizeof(*idxMv32x16), idxMv32x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(*idxMv16x32), idxMv16x32);
        CHECK_CM_ERR(res);
        int rectParts = 0;
        res = kernel->SetKernelArg(6, sizeof(rectParts), &rectParts);
        CHECK_CM_ERR(res);

        // MV32x32 also serves as MV predictor
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy((char *)mv32x32Sys + y * mv32x32Pitch, mvPredData + y * DIVUP(ctrl.width, 16), sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));

        mfxU32 tsWidth  = DIVUP(ctrl.width, 16);
        mfxU32 tsHeight = DIVUP(ctrl.height, 16);
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        CmThreadSpace * threadSpace = 0;
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);

        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);

        CmTask * task = 0;
        res = device->CreateTask(task);
        CHECK_CM_ERR(res);

        res = task->AddKernel(kernel);
        CHECK_CM_ERR(res);

        CmQueue *queue = 0;
        res = device->CreateQueue(queue);
        CHECK_CM_ERR(res);

        CmEvent * e = 0;
        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);

        res = e->WaitForTaskFinished();
        CHECK_CM_ERR(res);

        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(mv32x32Data + y * DIVUP(ctrl.width, 16), (char *)mv32x32Sys + y * mv32x32Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv32x16Data + y * DIVUP(ctrl.width, 16), (char *)mv32x16Sys + y * mv32x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(mv16x32Data + y * DIVUP(ctrl.width, 8), (char *)mv16x32Sys + y * mv16x32Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv16x16Data + y * DIVUP(ctrl.width, 8), (char *)mv16x16Sys + y * mv16x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));

#ifndef CMRT_EMU
        printf("TIME(MeP32_4MV)=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs);
        device->DestroySurface(ctrlBuf);
        device->DestroySurface(src);
        device->DestroySurface(ref);
        device->DestroySurface2DUP(mv32x32);
        device->DestroySurface2DUP(mv32x16);
        device->DestroySurface2DUP(mv16x32);
        device->DestroySurface2DUP(mv16x16);
        CM_ALIGNED_FREE(mv32x32Sys);
        CM_ALIGNED_FREE(mv32x16Sys);
        CM_ALIGNED_FREE(mv16x32Sys);
        CM_ALIGNED_FREE(mv16x16Sys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }

    int CompareMV(const mfxI16Pair *data1, const mfxI16Pair *data2, mfxU32 width, mfxU32 height)
    {
        for (mfxU32 i = 0; i < height; i++)
            for (mfxU32 j = 0; j < width; j++)
                if ((data1[i * width + j].x != data2[i * width + j].x) ||
                    (data1[i * width + j].y != data2[i * width + j].y))
                    return 1;
        return 0;
    }

    int CompareMV64(const mfxI16Pair *data64, const mfxI16Pair *data32, mfxU32 width2x, mfxU32 height2x)
    {
        printf("\n");
        int maxdx = 0, maxdy = 0;
        int maxdxx = 0, maxdxy = 0;
        int maxdyx = 0, maxdyy = 0;
        int dx = 0, dy = 0;
        for (mfxU32 i = 0; i < DIVUP(height2x, 16); i++) {
            for (mfxU32 j = 0; j < DIVUP(width2x, 16); j++) {
                printf("(%6i,%6i:%6i,%6i)",
                    data64[(i/2) * DIVUP(width2x, 32) + (j/2)].x, data64[(i/2) * DIVUP(width2x, 32) + (j/2)].y,
                    data32[i * DIVUP(width2x, 16) + j].x, data32[i * DIVUP(width2x, 16) + j].y);
                dx = data64[(i/2) * DIVUP(width2x, 32) + (j/2)].x - data32[i * DIVUP(width2x, 16) + j].x;
                dy = data64[(i/2) * DIVUP(width2x, 32) + (j/2)].y - data32[i * DIVUP(width2x, 16) + j].y;
                //printf("(%3i,%3i)", dx, dy);
                if (abs(dx) > abs(maxdx)) {
                    maxdx = dx;
                    maxdxx = j;
                    maxdxy = i;
                }
                if (abs(dy) > abs(maxdy)) {
                    maxdy = dy;
                    maxdyx = j;
                    maxdyy = i;
                }
                //if ((data1[i * width + j].x != data2[i * width + j].x) ||
                //    (data1[i * width + j].y != data2[i * width + j].y))
                    //return 1;
            }
            printf("\n");
        }
        printf("maxdx = %i at (%i;%i)\n", maxdx, maxdxx, maxdxy);
        printf("maxdy = %i at (%i;%i)\n", maxdy, maxdyx, maxdyy);
        return 0;
    }

}
