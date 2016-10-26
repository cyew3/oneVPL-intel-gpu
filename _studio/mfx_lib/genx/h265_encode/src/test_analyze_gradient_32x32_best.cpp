//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "stdio.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_hevce_analyze_gradient_32x32_best_bdw_isa.h"
#include "../include/genx_hevce_analyze_gradient_32x32_best_hsw_isa.h"

#ifdef CMRT_EMU
extern "C"
void AnalyzeGradient32x32Best(SurfaceIndex SURF_SRC, SurfaceIndex SURF_GRADIENT);
#endif //CMRT_EMU

namespace {
int RunGpu(const mfxU8 *inData, mfxU8 *outData4x4, mfxU8 *outData8x8, mfxU8 *outData16x16, mfxU8 *outData32x32);
int RunCpu(const mfxU8 *inData, mfxU8 *outData, mfxU32 blockSize); // mfxU32 for CPU to test mfxU16 overflow on GPU
int Compare(mfxU8 *outDataGpu, mfxU8 *outDataCpu, mfxU32 blockSize);
};

int main()
{
    mfxI32 outData4x4Size = DIVUP(WIDTH, 4) * DIVUP(HEIGHT, 4);
    mfxI32 outData8x8Size = DIVUP(WIDTH, 8) * DIVUP(HEIGHT, 8);
    mfxI32 outData16x16Size = DIVUP(WIDTH, 16) * DIVUP(HEIGHT, 16);
    mfxI32 outData32x32Size = DIVUP(WIDTH, 32) * DIVUP(HEIGHT, 32);
    
    mfxU8 *output4x4Gpu = new mfxU8[outData4x4Size];
    mfxU8 *output8x8Gpu = new mfxU8[outData8x8Size];
    mfxU8 *output16x16Gpu = new mfxU8[outData16x16Size];
    mfxU8 *output32x32Gpu = new mfxU8[outData32x32Size];
    mfxU8 *output4x4Cpu = new mfxU8[outData4x4Size];
    mfxU8 *output8x8Cpu = new mfxU8[outData8x8Size];
    mfxU8 *output16x16Cpu = new mfxU8[outData16x16Size];
    mfxU8 *output32x32Cpu = new mfxU8[outData32x32Size];
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    mfxU8 *input = new mfxU8[WIDTH * HEIGHT];
    mfxI32 bytesRead = fread(input, 1, WIDTH * HEIGHT, f);
    if (bytesRead != WIDTH * HEIGHT)
        return printf("FAILED to read frame from yuv file\n"), 1;
    fclose(f);

    res = RunGpu(input, output4x4Gpu, output8x8Gpu, output16x16Gpu, output32x32Gpu);
    CHECK_ERR(res);

    res = RunCpu(input, output4x4Cpu, 4);
    CHECK_ERR(res);
    res = RunCpu(input, output8x8Cpu, 8);
    CHECK_ERR(res);
    res = RunCpu(input, output16x16Cpu, 16);
    CHECK_ERR(res);
    res = RunCpu(input, output32x32Cpu, 32);
    CHECK_ERR(res);
    res = Compare(output4x4Gpu, output4x4Cpu, 4);
    CHECK_ERR(res);
    res = Compare(output8x8Gpu, output8x8Cpu, 8);
    CHECK_ERR(res);
    res = Compare(output16x16Gpu, output16x16Cpu, 16);
    CHECK_ERR(res);
    res = Compare(output32x32Gpu, output32x32Cpu, 32);
    CHECK_ERR(res);

    delete [] input;
    delete [] output4x4Gpu;
    delete [] output8x8Gpu;
    delete [] output16x16Gpu;
    delete [] output32x32Gpu;
    delete [] output4x4Cpu;
    delete [] output8x8Cpu;
    delete [] output16x16Cpu;
    delete [] output32x32Cpu;

    return printf("passed\n"), 0;
}

namespace {

int RunGpu(const mfxU8 *inData, mfxU8 *outData4x4, mfxU8 *outData8x8, mfxU8 *outData16x16, mfxU8 *outData32x32)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    mfxI32 res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_analyze_gradient_32x32_best_hsw, sizeof(genx_hevce_analyze_gradient_32x32_best_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(AnalyzeGradient32x32Best), kernel);
    CHECK_CM_ERR(res);

    CmSurface2D *input = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, input);
    CHECK_CM_ERR(res);

    res = input->WriteSurface(inData, NULL);
    CHECK_CM_ERR(res);


    mfxU32 output4x4W = DIVUP(WIDTH, 4);
    mfxU32 output4x4H = DIVUP(HEIGHT, 4);
    mfxU32 output4x4Pitch=0;
    mfxU32 output4x4Size=0;
    device->GetSurface2DInfo(output4x4W, output4x4H, CM_SURFACE_FORMAT_A8R8G8B8, output4x4Pitch, output4x4Size);
    mfxU8 *output4x4Sys = (mfxU8*)CM_ALIGNED_MALLOC(output4x4Size, 0x1000);
    CmSurface2DUP *output4x4Cm = 0;
    res = device->CreateSurface2DUP(output4x4W, output4x4H, CM_SURFACE_FORMAT_A8R8G8B8, output4x4Sys, output4x4Cm);
    CHECK_CM_ERR(res);

    mfxU32 output8x8W = DIVUP(WIDTH, 8);
    mfxU32 output8x8H = DIVUP(HEIGHT, 8);
    mfxU32 output8x8Pitch=0;
    mfxU32 output8x8Size=0;
    device->GetSurface2DInfo(output8x8W, output8x8H, CM_SURFACE_FORMAT_A8R8G8B8, output8x8Pitch, output8x8Size);
    mfxU8 *output8x8Sys = (mfxU8*)CM_ALIGNED_MALLOC(output8x8Size, 0x1000);
    CmSurface2DUP *output8x8Cm = 0;
    res = device->CreateSurface2DUP(output8x8W, output8x8H, CM_SURFACE_FORMAT_A8R8G8B8, output8x8Sys, output8x8Cm);
    CHECK_CM_ERR(res);

    mfxU32 output16x16W = DIVUP(WIDTH, 16);
    mfxU32 output16x16H = DIVUP(HEIGHT, 16);
    mfxU32 output16x16Pitch = 0;
    mfxU32 output16x16Size = 0;
    device->GetSurface2DInfo(output16x16W, output16x16H, CM_SURFACE_FORMAT_A8R8G8B8, output16x16Pitch, output16x16Size);
    mfxU8 *output16x16Sys = (mfxU8*)CM_ALIGNED_MALLOC(output16x16Size, 0x1000);
    CmSurface2DUP *output16x16Cm = 0;
    res = device->CreateSurface2DUP(output16x16W, output16x16H, CM_SURFACE_FORMAT_A8R8G8B8, output16x16Sys, output16x16Cm);
    CHECK_CM_ERR(res);

    mfxU32 output32x32W = DIVUP(WIDTH, 32);
    mfxU32 output32x32H = DIVUP(HEIGHT, 32);
    mfxU32 output32x32Pitch=0;
    mfxU32 output32x32Size = 0;
    device->GetSurface2DInfo(output32x32W, output32x32H, CM_SURFACE_FORMAT_A8R8G8B8, output32x32Pitch, output32x32Size);
    mfxU8 *output32x32Sys = (mfxU8*)CM_ALIGNED_MALLOC(output32x32Size, 0x1000);
    CmSurface2DUP *output32x32Cm = 0;
    res = device->CreateSurface2DUP(output32x32W, output32x32H, CM_SURFACE_FORMAT_A8R8G8B8, output32x32Sys, output32x32Cm);
    CHECK_CM_ERR(res);


    SurfaceIndex *idxInput = 0;
    SurfaceIndex *idxOutput4x4Cm = 0;
    SurfaceIndex *idxOutput8x8Cm = 0;
    SurfaceIndex *idxOutput16x16Cm = 0;
    SurfaceIndex *idxOutput32x32Cm = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);
    res = output4x4Cm->GetIndex(idxOutput4x4Cm);
    CHECK_CM_ERR(res);
    res = output8x8Cm->GetIndex(idxOutput8x8Cm);
    CHECK_CM_ERR(res);
    res = output16x16Cm->GetIndex(idxOutput16x16Cm);
    CHECK_CM_ERR(res);
    res = output32x32Cm->GetIndex(idxOutput32x32Cm);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxOutput4x4Cm), idxOutput4x4Cm);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(*idxOutput8x8Cm), idxOutput8x8Cm);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(*idxOutput16x16Cm), idxOutput16x16Cm);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(4, sizeof(*idxOutput32x32Cm), idxOutput32x32Cm);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(5, sizeof(WIDTH), &WIDTH);
    CHECK_CM_ERR(res);

    mfxU32 tsWidth = DIVUP(WIDTH, 32);
    mfxU32 tsHeight = DIVUP(HEIGHT, 32);

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

    for (int i = 0; i < output4x4H; i++) {
        for (int j = 0; j < output4x4W; j++)
            outData4x4[i * output4x4W + j] = output4x4Sys[i * output4x4Pitch + j * sizeof(mfxI32)];
    }
    for (int i = 0; i < output8x8H; i++) {
        for (int j = 0; j < output8x8W; j++)
            outData8x8[i * output8x8W + j] = output8x8Sys[i * output8x8Pitch + j * sizeof(mfxI32)];
    }
    for (int i = 0; i < output16x16H; i++) {
        for (int j = 0; j < output16x16W; j++)
            outData16x16[i * output16x16W + j] = output16x16Sys[i * output16x16Pitch + j * sizeof(mfxI32)];
    }
    for (int i = 0; i < output32x32H; i++) {
        for (int j = 0; j < output32x32W; j++)
            outData32x32[i * output32x32W + j] = output32x32Sys[i * output32x32Pitch + j * sizeof(mfxI32)];
    }

#ifndef CMRT_EMU
    printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    queue->DestroyEvent(e);
    device->DestroySurface2DUP(output4x4Cm);
    CM_ALIGNED_FREE(output4x4Sys);
    device->DestroySurface2DUP(output8x8Cm);
    CM_ALIGNED_FREE(output8x8Sys);
    device->DestroySurface2DUP(output16x16Cm);
    CM_ALIGNED_FREE(output16x16Sys);
    device->DestroySurface2DUP(output32x32Cm);
    CM_ALIGNED_FREE(output32x32Sys);
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

float atan2_fast(float y, float x)
{
    double a0;
    double a1;
    float atan2;
    const float CM_CONST_PI = 3.14159f;
    const float M_DBL_EPSILON = 0.00001f;

    if (y >= 0) {
        a0 = CM_CONST_PI * 0.5;
        a1 = 0;
    }
    else
    {
        a0 = CM_CONST_PI * 1.5;
        a1 = CM_CONST_PI * 2;
    }

    if (x < 0) {
        a1 = CM_CONST_PI;
    }

    float xy = x * y;
    float x2 = x * x;
    float y2 = y * y;

    a0 -= (xy / (y2 + 0.28f * x2 + M_DBL_EPSILON));
    a1 += (xy / (x2 + 0.28f * y2 + M_DBL_EPSILON));

    if (y2 <= x2) {
        atan2 = a1;
    }
    else {
        atan2 = a0;
    }

    //atan2 = matrix<float,R,C>(atan2, (flags & SAT));
    return atan2;
}

void BuildHistogram2(const mfxU8 *frame, mfxI32 blockX, mfxI32 blockY, mfxI32 blockSize,
    mfxU32 *histogram)
{
    for (mfxI32 y = blockY; y < blockY + blockSize; y++) {
        for (mfxI32 x = blockX; x < blockX + blockSize; x++) {
            mfxI16 dy = GetPel(frame, x - 1, y - 1) + 2 * GetPel(frame, x - 1, y + 0) + GetPel(frame, x - 1, y + 1)
                - GetPel(frame, x + 1, y - 1) - 2 * GetPel(frame, x + 1, y + 0) - GetPel(frame, x + 1, y + 1);
            mfxI16 dx = GetPel(frame, x - 1, y + 1) + 2 * GetPel(frame, x + 0, y + 1) + GetPel(frame, x + 1, y + 1)
                - GetPel(frame, x - 1, y - 1) - 2 * GetPel(frame, x + 0, y - 1) - GetPel(frame, x + 1, y - 1);
            //mfxU8  ang = FindNormalAngle(dx, dy);

            float fdx = dx;
            float fdy = dy;
            float y2 = fdy * fdy;
            float x2 = fdx * fdx;
            float angf;

            if (dx == 0 && dy == 0) {
                angf = 0;
            }
            else {
                angf = atan2_fast(fdy, fdx);
            }

            const float CM_CONST_PI = 3.14159f;
            if (y2 > x2 && dy > 0) angf += CM_CONST_PI;
            if (y2 <= x2 && dx > 0 && dy >= 0) angf += CM_CONST_PI;
            if (y2 <= x2 && dx > 0 && dy < 0) angf -= CM_CONST_PI;

            angf -= 0.75 * CM_CONST_PI;
            mfxI8 ang2 = angf * 10.504226 + 2;
            if (dx == 0 && dy == 0) ang2 = 0;

            mfxU16 amp = (mfxI16)(abs(dx) + abs(dy));
            histogram[ang2] += amp;
        }
    }
}

void BuildHistogram(const mfxU8 *frame, mfxI32 blockX, mfxI32 blockY, mfxI32 blockSize,
                    mfxU32 *histogram)
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

void BuildModes(const mfxU8 *frame, mfxI32 blockX, mfxI32 blockY, mfxI32 blockSize,
    mfxU8 *modes)
{
    mfxU32 histogram[36] = { 0 };
    BuildHistogram2(frame, blockX, blockY, blockSize, histogram);
    int maxAmp = -1, maxMode = -1, curAmp, curMode;
    for (int i = 0; i < 36; i++)
    {
        curAmp = histogram[i];
        curMode = i;
        if (curAmp >= maxAmp) {
            maxAmp = curAmp;
            maxMode = curMode;
        }
    }
    modes[0] = maxMode;
}


int RunCpu(const mfxU8 *inData, mfxU8 *outData, mfxU32 blockSize)
{
    for (mfxI32 y = 0; y < DIVUP(HEIGHT, blockSize); y++, outData += DIVUP(WIDTH, blockSize)) {
        for (mfxI32 x = 0; x < DIVUP(WIDTH, blockSize); x++) {
            *(outData + x) = 0;
            BuildModes(inData, x * blockSize, y * blockSize, blockSize, outData + x);
        }
    }

    return PASSED;
}


int Compare(mfxU8 *outDataGpu, mfxU8 *outDataCpu, mfxU32 blockSize)
{
    for (mfxI32 y = 0; y < DIVUP(HEIGHT, blockSize); y++) {
        for (mfxI32 x = 0; x < DIVUP(WIDTH, blockSize); x++) {
            mfxU8 *modesGpu = outDataGpu + (y * DIVUP(WIDTH, blockSize) + x);
            mfxU8 *modesCpu = outDataCpu + (y * DIVUP(WIDTH, blockSize) + x);
            if (*modesGpu != *modesCpu)
            {
                printf("bad modes value, CPU: max mode %d, GPU max mode %d, for block%dx%d (x,y)=(%d,%d)\n",
                    modesCpu[0], modesGpu[0], blockSize, blockSize, x, y);
                return FAILED;
            }
        }
    }
    return PASSED;
}
};
