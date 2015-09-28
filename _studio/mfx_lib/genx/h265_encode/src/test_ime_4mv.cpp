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
#include "../include/genx_hevce_ime_4mv_bdw_isa.h"
#include "../include/genx_hevce_ime_4mv_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void Ime_4mv(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV_OUT);
#endif //CMRT_EMU

namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *src, const mfxU8 *ref, mfxI16Pair *mv);
    int RunCpu(const Me2xControl &ctrl, const mfxU8 *src, const mfxU8 *ref, mfxI16Pair *mv);
    int Compare(const Me2xControl &ctrl, const mfxI16Pair *mv1, const mfxI16Pair *mv2);
};


int main()
{
    int width = ROUNDUP(1920 / 4, 16);
    int height = ROUNDUP(1080 / 4, 16);
    int frameSize = width * height * 3 / 2;

    std::vector<mfxU8> src(frameSize);
    std::vector<mfxU8> ref(frameSize);
    std::vector<mfxI16Pair> mv(DIVUP(width, 16) * DIVUP(height, 16) * 4); // 4 mvs per 16x16 block

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src.data(), 1, src.size(), f) != src.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref.data(), 1, ref.size(), f) != ref.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    Me2xControl ctrl = {};
    SetupMeControl(ctrl, width, height);

    Ipp32s res;
    res = RunGpu(ctrl, src.data(), ref.data(), mv.data());
    CHECK_ERR(res);

    //res = RunCpu(input.data(), outputCpu.data(), inWidth, inHeight);
    //CHECK_ERR(res);

    //res = Compare(outputCpu.data(), outputGpu.data(), outWidth, outHeight);
    //CHECK_ERR(res);

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, mfxI16Pair *mvData)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_ime_4mv_hsw, sizeof(genx_hevce_ime_4mv_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(Ime_4mv), kernel);
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
        res = kernel->SetKernelArg(1, sizeof(*genxRefs), genxRefs);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxMv), idxMv);
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
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs);
        device->DestroySurface(ctrlBuf);
        device->DestroySurface(src);
        device->DestroySurface(ref);
        device->DestroySurface2DUP(mv);
        CM_ALIGNED_FREE(mvSys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }
}
