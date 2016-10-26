//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#include "stdio.h"
#include "time.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "vector"
#include "../include/test_common.h"
#include "../include/genx_hevce_ime_3tiers_4mv_bdw_isa.h"
#include "../include/genx_hevce_ime_3tiers_4mv_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void Ime3Tiers4Mv(SurfaceIndex CTRL, SurfaceIndex SRCREF_16X, SurfaceIndex SRCREF_8X, SurfaceIndex SRCREF_4X, SurfaceIndex MV_OUT);
#endif //CMRT_EMU


namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *src16xData, const mfxU8 *ref16xData, const mfxU8 *src8xData, const mfxU8 *ref8xData, const mfxU8 *src4xData, const mfxU8 *ref4xData, mfxI16Pair *mv);
    int RunCpu(const Me2xControl &ctrl, const mfxU8 *src16xData, const mfxU8 *ref16xData, const mfxU8 *src8xData, const mfxU8 *ref8xData, const mfxU8 *src4xData, const mfxU8 *ref4xData, mfxI16Pair *mv);
    int Compare(const mfxU8 *data1, const mfxU8 *data2, mfxU32 width, mfxU32 height);
};


int main()
{
    int width4x = ROUNDUP(1920 / 4, 16);
    int width8x = ROUNDUP(1920 / 8, 16);
    int width16x = ROUNDUP(1920 / 16, 16);
    int height4x = ROUNDUP(1080 / 4, 16);
    int height8x = ROUNDUP(1080 / 8, 16);
    int height16x = ROUNDUP(1080 / 16, 16);
    int frameSize4x = width4x * height4x * 3 / 2;
    int frameSize8x = width8x * height8x * 3 / 2;
    int frameSize16x = width16x * height16x * 3 / 2;

    std::vector<mfxU8> src4x(frameSize4x);
    std::vector<mfxU8> ref4x(frameSize4x);
    std::vector<mfxU8> src8x(frameSize8x);
    std::vector<mfxU8> ref8x(frameSize8x);
    std::vector<mfxU8> src16x(frameSize16x);
    std::vector<mfxU8> ref16x(frameSize16x);
    std::vector<mfxI16Pair> mv(DIVUP(width4x, 16) * DIVUP(height4x, 16) * 4); // 4 mvs per 16x16 block

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src4x.data(), 1, src4x.size(), f) != src4x.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref4x.data(), 1, ref4x.size(), f) != ref4x.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    if (fread(src8x.data(), 1, src8x.size(), f) != src8x.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref8x.data(), 1, ref8x.size(), f) != ref8x.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    if (fread(src16x.data(), 1, src16x.size(), f) != src16x.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref16x.data(), 1, ref16x.size(), f) != ref16x.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    Me2xControl ctrl = {};
    SetupMeControl(ctrl, width4x, height4x);

    Ipp32s res;
    res = RunGpu(ctrl, src16x.data(), ref16x.data(), src8x.data(), ref8x.data(), src4x.data(), ref4x.data(), mv.data());
    CHECK_ERR(res);

    //res = RunCpu(input.data(), outputCpu.data(), inWidth, inHeight);
    //CHECK_ERR(res);

    //res = Compare(outputCpu.data(), outputGpu.data(), outWidth, outHeight);
    //CHECK_ERR(res);

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *src16xData, const mfxU8 *ref16xData, const mfxU8 *src8xData, const mfxU8 *ref8xData, const mfxU8 *src4xData, const mfxU8 *ref4xData, mfxI16Pair *mvData)
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

        CmBuffer *ctrlBuf = 0;
        res = device->CreateBuffer(sizeof(Me2xControl), ctrlBuf);
        CHECK_CM_ERR(res);
        res = ctrlBuf->WriteSurface((const Ipp8u *)&ctrl, NULL, sizeof(Me2xControl));
        CHECK_CM_ERR(res);

        CmSurface2D *src16x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl.width/4, 16), ROUNDUP(ctrl.height/4, 16), CM_SURFACE_FORMAT_NV12, src16x);
        CHECK_CM_ERR(res);
        res = src16x->WriteSurfaceStride(src16xData, NULL, ROUNDUP(ctrl.width/4, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref16x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl.width/4, 16), ROUNDUP(ctrl.height/4, 16), CM_SURFACE_FORMAT_NV12, ref16x);
        CHECK_CM_ERR(res);
        res = ref16x->WriteSurfaceStride(ref16xData, NULL, ROUNDUP(ctrl.width/4, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs16x = 0;
        res = device->CreateVmeSurfaceG7_5(src16x, &ref16x, NULL, 1, 0, genxRefs16x);
        CHECK_CM_ERR(res);

        CmSurface2D *src8x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl.width/2, 16), ROUNDUP(ctrl.height/2, 16), CM_SURFACE_FORMAT_NV12, src8x);
        CHECK_CM_ERR(res);
        res = src8x->WriteSurfaceStride(src8xData, NULL, ROUNDUP(ctrl.width/2, 16));
        CHECK_CM_ERR(res);
        CmSurface2D *ref8x = 0;
        res = device->CreateSurface2D(ROUNDUP(ctrl.width/2, 16), ROUNDUP(ctrl.height/2, 16), CM_SURFACE_FORMAT_NV12, ref8x);
        CHECK_CM_ERR(res);
        res = ref8x->WriteSurfaceStride(ref8xData, NULL, ROUNDUP(ctrl.width/2, 16));
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs8x = 0;
        res = device->CreateVmeSurfaceG7_5(src8x, &ref8x, NULL, 1, 0, genxRefs8x);
        CHECK_CM_ERR(res);

        CmSurface2D *src4x = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, src4x);
        CHECK_CM_ERR(res);
        res = src4x->WriteSurfaceStride(src4xData, NULL, ctrl.width);
        CHECK_CM_ERR(res);
        CmSurface2D *ref4x = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, ref4x);
        CHECK_CM_ERR(res);
        res = ref4x->WriteSurfaceStride(ref4xData, NULL, ctrl.width);
        CHECK_CM_ERR(res);
        SurfaceIndex *genxRefs4x = 0;
        res = device->CreateVmeSurfaceG7_5(src4x, &ref4x, NULL, 1, 0, genxRefs4x);
        CHECK_CM_ERR(res);

        int mvSurfWidth = DIVUP(ctrl.width, 16) * 2;
        int mvSurfHeight = DIVUP(ctrl.height, 16) * 2;
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

        mfxU32 tsWidth  = DIVUP(ctrl.width/4, 16);
        mfxU32 tsHeight = DIVUP(ctrl.height/4, 16);
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
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
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
}
