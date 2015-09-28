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
#include "../include/genx_hevce_me_p32_4mv_bdw_isa.h"
#include "../include/genx_hevce_me_p32_4mv_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void MeP32_4MV(SurfaceIndex CTRL, SurfaceIndex SRCREF,
    SurfaceIndex MV32x32, SurfaceIndex MV16x16, SurfaceIndex MV32x16, SurfaceIndex MV16x32, int rectParts);
#endif //CMRT_EMU

namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxI16Pair *mv32x32Data, mfxI16Pair *mv32x16Data, mfxI16Pair *mv16x32Data, mfxI16Pair *mv16x16Data);
    int RunCpu(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxI16Pair *mv32x32Data, mfxI16Pair *mv32x16Data, mfxI16Pair *mv16x32Data, mfxI16Pair *mv16x16Data);
    int Compare(const Me2xControl &ctrl, const mfxI16Pair *mv1, const mfxI16Pair *mv2);
};


int main()
{
    int width = ROUNDUP(1920/2, 16);
    int height = ROUNDUP(1080/2, 16);
    int frameSize = width * height * 3 / 2;

    std::vector<mfxU8> src(frameSize);
    std::vector<mfxU8> ref(frameSize);
    std::vector<mfxI16Pair> mvPred(DIVUP(width,16) * DIVUP(height,16));
    std::vector<mfxI16Pair> mv32x32(DIVUP(width,16) * DIVUP(height,16));
    std::vector<mfxI16Pair> mv32x16(DIVUP(width,16) * DIVUP(height,8));
    std::vector<mfxI16Pair> mv16x32(DIVUP(width,8) * DIVUP(height,16));
    std::vector<mfxI16Pair> mv16x16(DIVUP(width,8) * DIVUP(height,8));

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src.data(), 1, src.size(), f) != src.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref.data(), 1, ref.size(), f) != ref.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    srand(0x12345678);
    for (mfxI32 y = 0; y < DIVUP(height,16); y++) {
        for (mfxI32 x = 0; x < DIVUP(width,16); x++) {
            mvPred[y * DIVUP(width,16) + x].x = (rand() & 0x1e) - 16; // -16,-14..,12,14
            mvPred[y * DIVUP(width,16) + x].y = (rand() & 0x1e) - 16; // -16,-14..,12,14
        }
    }

    Me2xControl ctrl = {};
    SetupMeControlShortPath(ctrl, width, height);
    //SetupMeControl(ctrl, width, height);

    Ipp32s res;
    res = RunGpu(ctrl, src.data(), ref.data(), mvPred.data(), mv32x32.data(), mv32x16.data(), mv16x32.data(), mv16x16.data());
    CHECK_ERR(res);

    //res = RunCpu(input.data(), outputCpu.data(), inWidth, inHeight);
    //CHECK_ERR(res);

    //res = Compare(outputCpu.data(), outputGpu.data(), outWidth, outHeight);
    //CHECK_ERR(res);

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxI16Pair *mv32x32Data, mfxI16Pair *mv32x16Data, mfxI16Pair *mv16x32Data, mfxI16Pair *mv16x16Data)
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
        res = kernel->SetKernelArg(3, sizeof(*idxMv32x16), idxMv32x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(4, sizeof(*idxMv16x32), idxMv16x32);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(*idxMv16x16), idxMv16x16);
        CHECK_CM_ERR(res);
        int rectParts = 0;
        res = kernel->SetKernelArg(6, sizeof(rectParts), &rectParts);
        CHECK_CM_ERR(res);

        // MV16x16 also serves as MV predictor
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
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
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
}
