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
#include "../include/genx_hevce_me_p16_4mv_bdw_isa.h"
#include "../include/genx_hevce_me_p16_4mv_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void MeP16_4MV(SurfaceIndex CTRL, SurfaceIndex SRCREF,
    SurfaceIndex DIST16x16, SurfaceIndex DIST16x8, SurfaceIndex DIST8x16, SurfaceIndex DIST8x8,
    SurfaceIndex MV16x16, SurfaceIndex MV16x8, SurfaceIndex MV8x16, SurfaceIndex MV8x8, int rectParts);

#endif //CMRT_EMU

namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxU32 *dist16x16Data, mfxU32 *dist16x8Data, mfxU32 *dist8x16Data, mfxU32 *dist8x8Data,
        mfxI16Pair *mv16x16Data, mfxI16Pair *mv16x8Data, mfxI16Pair *mv8x16Data, mfxI16Pair *mv8x8Data, int rectParts);
    int RunCpu(const Me2xControl &ctrl, const mfxU8 *src, const mfxU8 *ref, const mfxI16Pair *mvPred,
        mfxU32 *dist16x16, mfxU32 *dist16x8, mfxU32 *dist8x16, mfxU32 *dist8x8,
        mfxI16Pair *mv16x16, mfxI16Pair *mv16x8, mfxI16Pair *mv8x16, mfxI16Pair *mv8x8);
    int Compare(const Me2xControl &ctrl, const mfxI16Pair *mv1, const mfxI16Pair *mv2);
};


int main()
{
    int width = ROUNDUP(1920, 16);
    int height = ROUNDUP(1080, 16);
    int widthIn8 = DIVUP(width, 8);
    int heightIn8 = DIVUP(height, 8);
    int widthIn16 = DIVUP(width, 16);
    int heightIn16 = DIVUP(height, 16);
    int frameSize = width * height * 3 / 2;

    std::vector<mfxU8> src(frameSize);
    std::vector<mfxU8> ref(frameSize);
    std::vector<mfxI16Pair> mvPred(widthIn16 * heightIn16);
    std::vector<mfxI16Pair> mv8x8(widthIn8 * heightIn8);
    std::vector<mfxI16Pair> mv16x8(widthIn16 * heightIn8);
    std::vector<mfxI16Pair> mv8x16(widthIn8 * heightIn16);
    std::vector<mfxI16Pair> mv16x16(widthIn16 * heightIn16);
    std::vector<mfxU32> dist8x8(widthIn8 * heightIn8);
    std::vector<mfxU32> dist16x8(widthIn16 * heightIn8);
    std::vector<mfxU32> dist8x16(widthIn8 * heightIn16);
    std::vector<mfxU32> dist16x16(widthIn16 * heightIn16);

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src.data(), 1, src.size(), f) != src.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref.data(), 1, ref.size(), f) != ref.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    srand(0x12345678);
    for (mfxI32 y = 0; y < heightIn16; y++) {
        for (mfxI32 x = 0; x < widthIn16; x++) {
            mvPred[y * widthIn16 + x].x = (rand() & 0x1e) - 16; // -16,-14..,12,14
            mvPred[y * widthIn16 + x].y = (rand() & 0x1e) - 16; // -16,-14..,12,14
        }
    }

    Me2xControl ctrl = {};
    SetupMeControlShortPath(ctrl, width, height);
    //SetupMeControl(ctrl, width, height);

    Ipp32s res;
    printf("rectParts=0: ");
    res = RunGpu(ctrl, src.data(), ref.data(), mvPred.data(),
        dist16x16.data(), dist16x8.data(), dist8x16.data(), dist8x8.data(),
        mv16x16.data(), mv16x8.data(), mv8x16.data(), mv8x8.data(), 0);
    CHECK_ERR(res);

    printf("  rectParts=1: ");
    res = RunGpu(ctrl, src.data(), ref.data(), mvPred.data(),
        dist16x16.data(), dist16x8.data(), dist8x16.data(), dist8x8.data(),
        mv16x16.data(), mv16x8.data(), mv8x16.data(), mv8x8.data(), 1);
    CHECK_ERR(res);

    //res = RunCpu(input.data(), outputCpu.data(), inWidth, inHeight);
    //CHECK_ERR(res);

    //res = Compare(outputCpu.data(), outputGpu.data(), outWidth, outHeight);
    //CHECK_ERR(res);

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(
        const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxU32 *dist16x16Data, mfxU32 *dist16x8Data, mfxU32 *dist8x16Data, mfxU32 *dist8x8Data,
        mfxI16Pair *mv16x16Data, mfxI16Pair *mv16x8Data, mfxI16Pair *mv8x16Data, mfxI16Pair *mv8x8Data, int rectParts)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_me_p16_4mv_hsw, sizeof(genx_hevce_me_p16_4mv_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MeP16_4MV), kernel);
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

        Ipp32u mv16x16Pitch = 0;
        Ipp32u mv16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x16Pitch, mv16x16Size);
        CHECK_CM_ERR(res);
        void *mv16x16Sys = CM_ALIGNED_MALLOC(mv16x16Size, 0x1000);
        CmSurface2DUP *mv16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x16Sys, mv16x16);
        CHECK_CM_ERR(res);

        Ipp32u mv16x8Pitch = 0;
        Ipp32u mv16x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x8Pitch, mv16x8Size);
        CHECK_CM_ERR(res);
        void *mv16x8Sys = CM_ALIGNED_MALLOC(mv16x8Size, 0x1000);
        CmSurface2DUP *mv16x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x8Sys, mv16x8);
        CHECK_CM_ERR(res);

        Ipp32u mv8x16Pitch = 0;
        Ipp32u mv8x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv8x16Pitch, mv8x16Size);
        CHECK_CM_ERR(res);
        void *mv8x16Sys = CM_ALIGNED_MALLOC(mv8x16Size, 0x1000);
        CmSurface2DUP *mv8x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv8x16Sys, mv8x16);
        CHECK_CM_ERR(res);

        Ipp32u mv8x8Pitch = 0;
        Ipp32u mv8x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv8x8Pitch, mv8x8Size);
        CHECK_CM_ERR(res);
        void *mv8x8Sys = CM_ALIGNED_MALLOC(mv8x8Size, 0x1000);
        CmSurface2DUP *mv8x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv8x8Sys, mv8x8);
        CHECK_CM_ERR(res);

        Ipp32u dist16x16Pitch = 0;
        Ipp32u dist16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist16x16Pitch, dist16x16Size);
        CHECK_CM_ERR(res);
        void *dist16x16Sys = CM_ALIGNED_MALLOC(dist16x16Size, 0x1000);
        CmSurface2DUP *dist16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist16x16Sys, dist16x16);
        CHECK_CM_ERR(res);

        Ipp32u dist16x8Pitch = 0;
        Ipp32u dist16x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist16x8Pitch, dist16x8Size);
        CHECK_CM_ERR(res);
        void *dist16x8Sys = CM_ALIGNED_MALLOC(dist16x8Size, 0x1000);
        CmSurface2DUP *dist16x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist16x8Sys, dist16x8);
        CHECK_CM_ERR(res);

        Ipp32u dist8x16Pitch = 0;
        Ipp32u dist8x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist8x16Pitch, dist8x16Size);
        CHECK_CM_ERR(res);
        void *dist8x16Sys = CM_ALIGNED_MALLOC(dist8x16Size, 0x1000);
        CmSurface2DUP *dist8x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist8x16Sys, dist8x16);
        CHECK_CM_ERR(res);

        Ipp32u dist8x8Pitch = 0;
        Ipp32u dist8x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist8x8Pitch, dist8x8Size);
        CHECK_CM_ERR(res);
        void *dist8x8Sys = CM_ALIGNED_MALLOC(dist8x8Size, 0x1000);
        CmSurface2DUP *dist8x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist8x8Sys, dist8x8);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxCtrl = 0;
        res = ctrlBuf->GetIndex(idxCtrl);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxDist16x16 = 0;
        res = dist16x16->GetIndex(idxDist16x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist16x8 = 0;
        res = dist16x8->GetIndex(idxDist16x8);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist8x16 = 0;
        res = dist8x16->GetIndex(idxDist8x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist8x8 = 0;
        res = dist8x8->GetIndex(idxDist8x8);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxMv16x16 = 0;
        res = mv16x16->GetIndex(idxMv16x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv16x8 = 0;
        res = mv16x8->GetIndex(idxMv16x8);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv8x16 = 0;
        res = mv8x16->GetIndex(idxMv8x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv8x8 = 0;
        res = mv8x8->GetIndex(idxMv8x8);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxCtrl), idxCtrl);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*genxRefs), genxRefs);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxDist16x16), idxDist16x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idxDist16x8), idxDist16x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(4, sizeof(*idxDist8x16), idxDist8x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(*idxDist8x8), idxDist8x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(6, sizeof(*idxMv16x16), idxMv16x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(7, sizeof(*idxMv16x8), idxMv16x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(8, sizeof(*idxMv8x16), idxMv8x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(9, sizeof(*idxMv8x8), idxMv8x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(10, sizeof(rectParts), &rectParts);
        CHECK_CM_ERR(res);

        // MV16x16 also serves as MV predictor
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy((char *)mv16x16Sys + y * mv16x16Pitch, mvPredData + y * DIVUP(ctrl.width, 16), sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));

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
            memcpy(mv16x16Data + y * DIVUP(ctrl.width, 16), (char *)mv16x16Sys + y * mv16x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv16x8Data + y * DIVUP(ctrl.width, 16), (char *)mv16x8Sys + y * mv16x8Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(mv8x16Data + y * DIVUP(ctrl.width, 8), (char *)mv8x16Sys + y * mv8x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv8x8Data + y * DIVUP(ctrl.width, 8), (char *)mv8x8Sys + y * mv8x8Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));

        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(dist16x16Data + y * DIVUP(ctrl.width, 16), (char *)dist16x16Sys + y * dist16x16Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(dist16x8Data + y * DIVUP(ctrl.width, 16), (char *)dist16x8Sys + y * dist16x8Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(dist8x16Data + y * DIVUP(ctrl.width, 8), (char *)dist8x16Sys + y * dist8x16Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 8));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(dist8x8Data + y * DIVUP(ctrl.width, 8), (char *)dist8x8Sys + y * dist8x8Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 8));

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs);
        device->DestroySurface(ctrlBuf);
        device->DestroySurface(src);
        device->DestroySurface(ref);
        device->DestroySurface2DUP(mv16x16);
        device->DestroySurface2DUP(mv16x8);
        device->DestroySurface2DUP(mv8x16);
        device->DestroySurface2DUP(mv8x8);
        device->DestroySurface2DUP(dist16x16);
        device->DestroySurface2DUP(dist16x8);
        device->DestroySurface2DUP(dist8x16);
        device->DestroySurface2DUP(dist8x8);
        CM_ALIGNED_FREE(mv16x16Sys);
        CM_ALIGNED_FREE(mv16x8Sys);
        CM_ALIGNED_FREE(mv8x16Sys);
        CM_ALIGNED_FREE(mv8x8Sys);
        CM_ALIGNED_FREE(dist16x16Sys);
        CM_ALIGNED_FREE(dist16x8Sys);
        CM_ALIGNED_FREE(dist8x16Sys);
        CM_ALIGNED_FREE(dist8x8Sys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }
}
