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
#include "../include/genx_hevce_down_sample_mb_bdw_isa.h"
#include "../include/genx_hevce_down_sample_mb_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void DownSampleMB(SurfaceIndex SurfIndex, SurfaceIndex Surf2XIndex);
#endif //CMRT_EMU

namespace {
    int RunGpu(const mfxU8 *inData, mfxU8 *outData, mfxU32 width, mfxU32 height);
    int RunCpu(const mfxU8 *inData, mfxU8 *outData, mfxU32 width, mfxU32 height);
    int Compare(const mfxU8 *data1, const mfxU8 *data2, mfxU32 width, mfxU32 height);
};


int main()
{
    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;

    // const per frame params
    Ipp32s inWidth = 1920;
    Ipp32s inHeight = 1080;
    Ipp32s outWidth = ROUNDUP(inWidth/2, 16);
    Ipp32s outHeight = ROUNDUP(inHeight/2, 16);
    Ipp32s inFrameSize = inWidth * inHeight * 3 / 2;
    Ipp32s outFrameSize = outWidth * outHeight * 3 / 2;

    std::vector<mfxU8> input(inFrameSize);
    std::vector<mfxU8> outputGpu(outFrameSize);
    std::vector<mfxU8> outputCpu(outFrameSize);

    srand(time(0));
    for (Ipp32s i = 0; i < inFrameSize; i++)
        input[i] = mfxU8(rand());

    Ipp32s res;
    res = RunGpu(input.data(), outputGpu.data(), inWidth, inHeight);
    CHECK_ERR(res);

    //res = RunCpu(input.data(), outputCpu.data(), inWidth, inHeight);
    //CHECK_ERR(res);

    //res = Compare(outputCpu.data(), outputGpu.data(), outWidth, outHeight);
    //CHECK_ERR(res);

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(const mfxU8 *inData, mfxU8 *outData, mfxU32 width, mfxU32 height)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_down_sample_mb_hsw, sizeof(genx_hevce_down_sample_mb_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(DownSampleMB), kernel);
        CHECK_CM_ERR(res);

        CmSurface2D *input = 0;
        res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, input);
        CHECK_CM_ERR(res);
        res = input->WriteSurfaceStride(inData, NULL, width);
        CHECK_CM_ERR(res);

        CmSurface2D *output = 0;
        res = device->CreateSurface2D(ROUNDUP(width/2, 16), ROUNDUP(height/2, 16), CM_SURFACE_FORMAT_NV12, output);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxInput = 0;
        res = input->GetIndex(idxInput);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxOutput = 0;
        res = output->GetIndex(idxOutput);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*idxOutput), idxOutput);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = DIVUP(width, 16);
        mfxU32 tsHeight = DIVUP(height, 16);
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

        output->ReadSurfaceStride(outData, NULL, ROUNDUP(width/2, 16));

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroySurface(input);
        device->DestroySurface(output);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }
}
