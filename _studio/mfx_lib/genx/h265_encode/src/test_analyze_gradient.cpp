/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "stdio.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_h265_cmcode_isa.h"

#ifdef CMRT_EMU
extern "C"
void AnalyzeGradient(SurfaceIndex SURF_SRC, SurfaceIndex SURF_GRADIENT);
#endif //CMRT_EMU

const mfxI32 WIDTH  = 416;
const mfxI32 HEIGHT = 240;
const mfxI8 YUV_NAME[] = "./test_data/basketball_pass_416x240p_2.yuv";

namespace {
int RunGpu(const mfxU8 *inData, mfxU16 *outData);
int RunCpu(const mfxU8 *inData, mfxU16 *outData);
};

int TestAnalyzeGradient()
{
    mfxI32 outDataSize = WIDTH * HEIGHT * 40 / 16;
    mfxU16 *outputGpu = new mfxU16[outDataSize];
    mfxU16 *outputCpu = new mfxU16[outDataSize];
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return FAILED;
    mfxU8 *input = new mfxU8[WIDTH * HEIGHT];
    mfxI32 bytesRead = fread(input, 1, WIDTH * HEIGHT, f);
    if (bytesRead != WIDTH * HEIGHT)
        return FAILED;
    fclose(f);

    res = RunGpu(input, outputGpu);
    CHECK_ERR(res);

    res = RunCpu(input, outputCpu);
    CHECK_ERR(res);

    for (mfxI32 y = 0; y < HEIGHT / 4; y++) {
        for (mfxI32 x = 0; x < WIDTH / 4; x++) {
            mfxU16 *histogramGpu = outputGpu + (y * (WIDTH / 4) + x) * 40;
            mfxU16 *histogramCpu = outputCpu + (y * (WIDTH / 4) + x) * 40;
            for (mfxI32 mode = 0; mode < 35; mode++) {
                if (histogramGpu[mode] != histogramCpu[mode]) {
                    printf("bad histogram value (%d != %d) for mode %d for block (x,y)=(%d,%d)\n",
                           histogramGpu[mode], histogramCpu[mode], mode, x, y);
                    return FAILED;
                }
            }
        }
    }

    delete [] input;
    delete [] outputGpu;
    delete [] outputCpu;

    return PASSED;
}

namespace {

int RunGpu(const mfxU8 *inData, mfxU16 *outData)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    mfxI32 res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_h265_cmcode, sizeof(genx_h265_cmcode), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(AnalyzeGradient), kernel);
    CHECK_CM_ERR(res);

    CmSurface2D *input = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, input);
    CHECK_CM_ERR(res);

    res = input->WriteSurface(inData, NULL);
    CHECK_CM_ERR(res);

    size_t outputSize = WIDTH * HEIGHT * 40 / 16 * sizeof(mfxU16);
    mfxU16 *outputSys = (mfxU16 *)CM_ALIGNED_MALLOC(outputSize, 0x1000);
    CmBufferUP *outputCm = 0;
    res = device->CreateBufferUP(outputSize, outputSys, outputCm);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInput = 0;
    SurfaceIndex *idxOutputCm = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);
    res = outputCm->GetIndex(idxOutputCm);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxOutputCm), idxOutputCm);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(WIDTH), &WIDTH);
    CHECK_CM_ERR(res);

    res = kernel->SetThreadCount(WIDTH * HEIGHT / 16);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(WIDTH / 4, HEIGHT / 4, threadSpace);
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

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    //mfxU64 time;
    //e->GetExecutionTime(time);
    //printf("TIME=%.3f ms\n", time / 1000000.0);

    memcpy(outData, outputSys, outputSize);

    queue->DestroyEvent(e);
    device->DestroyBufferUP(outputCm);
    CM_ALIGNED_FREE(outputSys);
    device->DestroySurface(input);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}

mfxU8 FindNormalAngle(mfxI32 dx, mfxI32 dy)
{
    static const mfxU8 MODE[257] = {
         2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
         3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
         4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
         5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
         6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
         7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
         8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
         9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
        10, 10, 10, 10, 10, 10, 10, 10, 10,
        11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
    };

    if (dx == 0 && dy == 0)
        return 0;

    if (abs(dx) >= abs(dy)) {
        mfxI32 tabIdx = (dy * 128 + dx / 2) / dx;
        return MODE[tabIdx + 128];
    }
    else {
        mfxI32 tabIdx = (dx * 128 + dy / 2) / dy;
        return 36 - MODE[tabIdx + 128];
    }
}

mfxU8 GetPel(const mfxU8 *frame, mfxI32 x, mfxI32 y)
{
    x = CLIPVAL(x, 0, WIDTH - 1);
    y = CLIPVAL(y, 0, HEIGHT - 1);
    return frame[y * WIDTH + x];
}

void BuildHistogram(const mfxU8 *frame, mfxI32 blockX, mfxI32 blockY, mfxI32 blockSize,
                    mfxU16 *histogram)
{
    for (mfxI32 y = blockY; y < blockY + blockSize; y++) {
        for (mfxI32 x = blockX; x < blockX + blockSize; x++) {
            mfxI16 dy = GetPel(frame, x-1, y-1) + 2 * GetPel(frame, x-1, y+0) + GetPel(frame, x-1, y+1)
                       -GetPel(frame, x+1, y-1) - 2 * GetPel(frame, x+1, y+0) - GetPel(frame, x+1, y+1);
            mfxI16 dx = GetPel(frame, x-1, y+1) + 2 * GetPel(frame, x+0, y+1) + GetPel(frame, x+1, y+1)
                       -GetPel(frame, x-1, y-1) - 2 * GetPel(frame, x+0, y-1) - GetPel(frame, x+1, y-1);
            mfxU8  ang = FindNormalAngle(dx, dy);
            mfxU16 amp = (mfxI16)(abs(dx) + abs(dy));
            histogram[ang] += amp;
        }
    }
}

int RunCpu(const mfxU8 *inData, mfxU16 *outData)
{
    for (mfxI32 y = 0; y < HEIGHT / 4; y++, outData += (WIDTH / 4) * 40) {
        for (mfxI32 x = 0; x < WIDTH / 4; x++) {
            memset(outData + x * 40, 0, sizeof(mfxU16) * 40);
            BuildHistogram(inData, x * 4, y * 4, 4, outData + x * 40);
        }
    }

    return PASSED;
}

};
