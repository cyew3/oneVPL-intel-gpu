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
#include "algorithm"
#include "../include/test_common.h"
#include "../include/genx_hevce_down_sample_mb_4tiers_bdw_isa.h"
#include "../include/genx_hevce_down_sample_mb_4tiers_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" void DownSampleMB4t(SurfaceIndex SurfIndex, SurfaceIndex Surf2XIndex, SurfaceIndex Surf4XIndex, SurfaceIndex Surf8XIndex, SurfaceIndex Surf16XIndex);
#endif //CMRT_EMU

namespace {
    int RunGpu(const mfxU8 *inData, mfxU8 *outData1, mfxU8 *outData2, mfxU8 *outData3, mfxU8 *outData4, mfxU32 width, mfxU32 height);
    int RunCpu(const mfxU8 *inData, mfxU8 *outData1, mfxU8 *outData2, mfxU8 *outData3, mfxU8 *outData4, mfxU32 width, mfxU32 height);
};


int main()
{
    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;

    // const per frame params
    Ipp32s inWidth = 1920;
    Ipp32s inHeight = 1080;
    Ipp32s outWidth1 = ROUNDUP(inWidth/2, 16);
    Ipp32s outHeight1 = ROUNDUP(inHeight/2, 16);
    Ipp32s outWidth2 = ROUNDUP(inWidth/4, 16);
    Ipp32s outHeight2 = ROUNDUP(inHeight/4, 16);
    Ipp32s outWidth3 = ROUNDUP(inWidth/8, 16);
    Ipp32s outHeight3 = ROUNDUP(inHeight/8, 16);
    Ipp32s outWidth4 = ROUNDUP(inWidth/16, 16);
    Ipp32s outHeight4 = ROUNDUP(inHeight/16, 16);
    Ipp32s inFrameSize = inWidth * inHeight * 3 / 2;
    Ipp32s outFrameSize1 = outWidth1 * outHeight1 * 3 / 2;
    Ipp32s outFrameSize2 = outWidth2 * outHeight2 * 3 / 2;
    Ipp32s outFrameSize3 = outWidth3 * outHeight3 * 3 / 2;
    Ipp32s outFrameSize4 = outWidth4 * outHeight4 * 3 / 2;

    std::vector<mfxU8> input(inFrameSize);
    std::vector<mfxU8> output1Gpu(outFrameSize1);
    std::vector<mfxU8> output1Cpu(outFrameSize1);
    std::vector<mfxU8> output2Gpu(outFrameSize2);
    std::vector<mfxU8> output2Cpu(outFrameSize2);
    std::vector<mfxU8> output3Gpu(outFrameSize3);
    std::vector<mfxU8> output3Cpu(outFrameSize3);
    std::vector<mfxU8> output4Gpu(outFrameSize4);
    std::vector<mfxU8> output4Cpu(outFrameSize4);

    srand(0x1234578/*time(0)*/);
    for (Ipp32s i = 0; i < inFrameSize; i++)
        input[i] = mfxU8(rand());

    Ipp32s res;
    res = RunGpu(input.data(), output1Gpu.data(), output2Gpu.data(), output3Gpu.data(), output4Gpu.data(), inWidth, inHeight);
    CHECK_ERR(res);

    res = RunCpu(input.data(), output1Cpu.data(), output2Cpu.data(), output3Cpu.data(), output4Cpu.data(), inWidth, inHeight);
    CHECK_ERR(res);

    auto diff = std::mismatch(output1Cpu.begin(), output1Cpu.end(), output1Gpu.begin());
    if (diff.first != output1Cpu.end()) {
        int pos = int(diff.first - output1Cpu.begin());
        return printf("FAILED: 2x surfaces differ at x=%d, y=%d cpu=%d, gpu=%d\n", pos%outWidth1, pos/outWidth1, *diff.first, *diff.second), 1;
    }
    diff = std::mismatch(output2Cpu.begin(), output2Cpu.end(), output2Gpu.begin());
    if (diff.first != output2Cpu.end()) {
        int pos = int(diff.first - output2Cpu.begin());
        return printf("FAILED: 4x surfaces differ at x=%d, y=%d cpu=%d, gpu=%d\n", pos%outWidth2, pos/outWidth2, *diff.first, *diff.second), 1;
    }
    diff = std::mismatch(output3Cpu.begin(), output3Cpu.end(), output3Gpu.begin());
    if (diff.first != output3Cpu.end()) {
        int pos = int(diff.first - output3Cpu.begin());
        return printf("FAILED: 8x surfaces differ at x=%d, y=%d cpu=%d, gpu=%d\n", pos%outWidth3, pos/outWidth3, *diff.first, *diff.second), 1;
    }
    diff = std::mismatch(output4Cpu.begin(), output4Cpu.end(), output4Gpu.begin());
    if (diff.first != output4Cpu.end()) {
        int pos = int(diff.first - output4Cpu.begin());
        return printf("FAILED: 16x surfaces differ at x=%d, y=%d cpu=%d, gpu=%d\n", pos%outWidth4, pos/outWidth4, *diff.first, *diff.second), 1;
    }

    return printf("passed\n"), 0;
}


namespace {
    int RunGpu(const mfxU8 *inData, mfxU8 *outData1, mfxU8 *outData2, mfxU8 *outData3, mfxU8 *outData4, mfxU32 width, mfxU32 height)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_down_sample_mb_4tiers_hsw, sizeof(genx_hevce_down_sample_mb_4tiers_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(DownSampleMB4t), kernel);
        CHECK_CM_ERR(res);

        CmSurface2D *input = 0;
        res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, input);
        CHECK_CM_ERR(res);
        res = input->WriteSurfaceStride(inData, NULL, width);
        CHECK_CM_ERR(res);

        CmSurface2D *output1 = 0;
        res = device->CreateSurface2D(ROUNDUP(width/2, 16), ROUNDUP(height/2, 16), CM_SURFACE_FORMAT_NV12, output1);
        CHECK_CM_ERR(res);

        CmSurface2D *output2 = 0;
        res = device->CreateSurface2D(ROUNDUP(width/4, 16), ROUNDUP(height/4, 16), CM_SURFACE_FORMAT_NV12, output2);
        CHECK_CM_ERR(res);

        CmSurface2D *output3 = 0;
        res = device->CreateSurface2D(ROUNDUP(width/8, 16), ROUNDUP(height/8, 16), CM_SURFACE_FORMAT_NV12, output3);
        CHECK_CM_ERR(res);

        CmSurface2D *output4 = 0;
        res = device->CreateSurface2D(ROUNDUP(width/16, 16), ROUNDUP(height/16, 16), CM_SURFACE_FORMAT_NV12, output4);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxInput = 0;
        res = input->GetIndex(idxInput);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxOutput1 = 0;
        res = output1->GetIndex(idxOutput1);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxOutput2 = 0;
        res = output2->GetIndex(idxOutput2);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxOutput3 = 0;
        res = output3->GetIndex(idxOutput3);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxOutput4 = 0;
        res = output4->GetIndex(idxOutput4);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*idxOutput1), idxOutput1);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxOutput2), idxOutput2);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idxOutput3), idxOutput3);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(4, sizeof(*idxOutput4), idxOutput4);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = ROUNDUP(width/16, 16)/4;
        mfxU32 tsHeight = ROUNDUP(height/16, 16);
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

        output1->ReadSurfaceStride(outData1, NULL, ROUNDUP(width/2, 16));
        output2->ReadSurfaceStride(outData2, NULL, ROUNDUP(width/4, 16));
        output3->ReadSurfaceStride(outData3, NULL, ROUNDUP(width/8, 16));
        output4->ReadSurfaceStride(outData4, NULL, ROUNDUP(width/16, 16));

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroySurface(input);
        device->DestroySurface(output1);
        device->DestroySurface(output2);
        device->DestroySurface(output3);
        device->DestroySurface(output4);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }


    int RunCpu(const mfxU8 *inData, mfxU8 *outData1, mfxU8 *outData2, mfxU8 *outData3, mfxU8 *outData4, mfxU32 width, mfxU32 height)
    {
        Ipp32s outWidth1 = ROUNDUP(width/2, 16);
        Ipp32s outWidth2 = ROUNDUP(width/4, 16);
        Ipp32s outWidth3 = ROUNDUP(width/8, 16);
        Ipp32s outWidth4 = ROUNDUP(width/16, 16);
        Ipp32s outHeight1 = ROUNDUP(height/2, 16);
        Ipp32s outHeight2 = ROUNDUP(height/4, 16);
        Ipp32s outHeight3 = ROUNDUP(height/8, 16);
        Ipp32s outHeight4 = ROUNDUP(height/16, 16);

        Ipp8u in[16*16];
        Ipp8u out2x[8*8];
        Ipp8u out4x[4*4];
        Ipp8u out8x[2*2];
        for (Ipp32s y = 0; y < outHeight4; y++) {
            for (Ipp32s x = 0; x < outWidth4; x++) {
                for (Ipp32s i = 0; i < 16; i++)
                    for (Ipp32s j = 0; j < 16; j++)
                        in[j+16*i] = inData[MIN(width-1, x*16+j) + width * MIN(height-1, y*16+i)];
                for (Ipp32s i = 0; i < 8; i++)
                    for (Ipp32s j = 0; j < 8; j++)
                        out2x[j+8*i] = in[j*2+i*2*16] + in[j*2+1+i*2*16] + in[j*2+(i*2+1)*16] + in[j*2+1+(i*2+1)*16] + 2 >> 2;
                for (Ipp32s i = 0; i < 4; i++)
                    for (Ipp32s j = 0; j < 4; j++)
                        out4x[j+4*i] = out2x[j*2+i*2*8] + out2x[j*2+1+i*2*8] + out2x[j*2+(i*2+1)*8] + out2x[j*2+1+(i*2+1)*8] + 2 >> 2;
                for (Ipp32s i = 0; i < 2; i++)
                    for (Ipp32s j = 0; j < 2; j++)
                        out8x[j+2*i] = out4x[j*2+i*2*4] + out4x[j*2+1+i*2*4] + out4x[j*2+(i*2+1)*4] + out4x[j*2+1+(i*2+1)*4] + 2 >> 2;

                outData4[x+outWidth4*y] = out8x[0] + out8x[1] + out8x[2] + out8x[3] + 2 >> 2;

                for (Ipp32s i = 0; i < 2; i++)
                    for (Ipp32s j = 0; j < 2; j++)
                        if (x*2+j < outWidth3 && y*2+i < outHeight3)
                            outData3[x*2+j+outWidth3*(y*2+i)] = out8x[j+2*i];
                for (Ipp32s i = 0; i < 4; i++)
                    for (Ipp32s j = 0; j < 4; j++)
                        if (x*4+j < outWidth2 && y*4+i < outHeight2)
                            outData2[x*4+j+outWidth2*(y*4+i)] = out4x[j+4*i];
                for (Ipp32s i = 0; i < 8; i++)
                    for (Ipp32s j = 0; j < 8; j++)
                        if (x*8+j < outWidth1 && y*8+i < outHeight1)
                            outData1[x*8+j+outWidth1*(y*8+i)] = out2x[j+8*i];
            }
        }

        return PASSED;
    }
}
