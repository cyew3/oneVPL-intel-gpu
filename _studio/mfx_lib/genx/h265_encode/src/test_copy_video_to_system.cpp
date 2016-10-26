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
#include "algorithm"
#include "../include/test_common.h"
#include "../include/genx_hevce_copy_video_to_system_bdw_isa.h"
#include "../include/genx_hevce_copy_video_to_system_hsw_isa.h"

enum {
    MFX_CHROMAFORMAT_YUV420 = 1,
    MFX_CHROMAFORMAT_YUV422 = 2,
};

#ifdef CMRT_EMU
extern "C" {
    void CopyVideoToSystem420(SurfaceIndex SRC, SurfaceIndex DST_LU, SurfaceIndex DST_CH, Ipp32u dstPaddingLu, Ipp32u dstPaddingCh);
};
#endif //CMRT_EMU

namespace {
    template <mfxU32 chromaFormat, class T>
    int RunGpu(const T *inData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData);

    template <mfxU32 chromaFormat, class T>
    int RunRef(const T *inData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData);

    typedef std::vector<mfxU8> vec8;
    typedef std::vector<mfxU16> vec16;

    int RunGpuNv12(const vec8 &in, Ipp32s width, Ipp32s height, vec8 &outLuma, vec8 &outChroma) {
        return RunGpu<MFX_CHROMAFORMAT_YUV420>(in.data(), width, height, outLuma.data(), outChroma.data());
    }
    int RunGpuP010(const vec16 &in, Ipp32s width, Ipp32s height, vec16 &outLuma, vec16 &outChroma) {
        return RunGpu<MFX_CHROMAFORMAT_YUV420>(in.data(), width, height, outLuma.data(), outChroma.data());
    }

    int RunRefNv12(const vec8 &in, Ipp32s width, Ipp32s height, vec8 &outLuma, vec8 &outChroma) {
        return RunRef<MFX_CHROMAFORMAT_YUV420>(in.data(), width, height, outLuma.data(), outChroma.data());
    }
    int RunRefP010(const vec16 &in, Ipp32s width, Ipp32s height, vec16 &outLuma, vec16 &outChroma) {
        return RunRef<MFX_CHROMAFORMAT_YUV420>(in.data(), width, height, outLuma.data(), outChroma.data());
    }

    template <class T> int Compare(const T *outGpu, const T *outRef, Ipp32s width, Ipp32s height, const char *plane);
    template <class T> int Compare(const std::vector<T> &outGpu, const std::vector<T> &outRef, Ipp32s width, Ipp32s height, const char *plane) { return Compare(outGpu.data(), outRef.data(), width, height, plane); }
};


const Ipp32s BLOCKW = 32;
const Ipp32s BLOCKH = 8;

int main()
{
    Ipp32s width = 1920;
    Ipp32s height = 1080;

    std::vector<mfxU8>  in8(width * height * 3 / 2);
    std::vector<mfxU16> in10(width * height * 3 / 2);

    std::vector<mfxU8>  outLuma8Gpu(width * height);
    std::vector<mfxU8>  outLuma8Ref(width * height);
    std::vector<mfxU16> outLuma10Gpu(width * height);
    std::vector<mfxU16> outLuma10Ref(width * height);
    std::vector<mfxU8>  outChroma8Gpu(width * height / 2);
    std::vector<mfxU8>  outChroma8Ref(width * height / 2);
    std::vector<mfxU16> outChroma10Gpu(width * height / 2);
    std::vector<mfxU16> outChroma10Ref(width * height / 2);

    srand(0x1234578/*time(0)*/);
    std::generate(in8.begin(),  in8.end(),  [](){ return mfxU8(rand()); });
    std::generate(in10.begin(), in10.end(), [](){ return mfxU16(rand() & 0x3ff); });

    Ipp32s res;

    printf("NV12: ");
    res = RunGpuNv12(in8, width, height, outLuma8Gpu, outChroma8Gpu);
    CHECK_ERR(res);
    res = RunRefNv12(in8, width, height, outLuma8Ref, outChroma8Ref);
    CHECK_ERR(res);
    if (Compare(outLuma8Gpu, outLuma8Ref, width, height, "luma") != PASSED ||
        Compare(outChroma8Gpu, outChroma8Ref, width, height/2, "chroma") != PASSED)
        return 1;
    printf("passed\n");

    printf("P010: ");
    res = RunGpuP010(in10, width, height, outLuma10Gpu, outChroma10Gpu);
    CHECK_ERR(res);
    res = RunRefP010(in10, width, height, outLuma10Ref, outChroma10Ref);
    CHECK_ERR(res);
    if (Compare(outLuma10Gpu, outLuma10Ref, width, height, "luma") != PASSED ||
        Compare(outChroma10Gpu, outChroma10Ref, width, height/2, "chroma") != PASSED)
        return 1;
    printf("passed\n");

    return 0;
}


namespace {
    template <class T>
    int Compare(const T *outGpu, const T *outRef, Ipp32s width, Ipp32s height, const char *plane)
    {
        auto diff = std::mismatch(outGpu, outGpu + width * height, outRef);
        if (diff.first != outGpu + width * height) {
            Ipp32s pos = Ipp32s(diff.first - outGpu);
            printf("FAILED: \"%s\" surfaces differ at x=%d, y=%d tst=%d, ref=%d\n", plane, pos%width, pos/width, *diff.first, *diff.second);
            return FAILED;
        }
        return PASSED;
    }


    template <class T> void CopyAndPad(const T *in, Ipp32s inPitch, T *out, Ipp32s outPitch, Ipp32s width, Ipp32s height, Ipp32s padding)
    {
        for (Ipp32s y = 0; y < height; y++) {
            memcpy(out + padding, in, width * sizeof(T));
            for (Ipp32s x = 0; x < padding; x++) {
                out[x] = out[padding];
                out[padding + width + x] = out[padding + width - 1];
            }
            out = (T *)((Ipp8u *)out + outPitch);
            in  = (T *)((Ipp8u *)in + inPitch);
        }
    }

    template <mfxU32 chromaFormat, class T>
    int RunGpu(const T *inData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData)
    {
        const Ipp32s bpp = sizeof(T);
        const Ipp32s PADDING_LU = 80;
        const Ipp32s PADDING_CH = 80;
        const Ipp32s heightChroma = height >> (chromaFormat == MFX_CHROMAFORMAT_YUV420 ? 1 : 0);
        const CM_SURFACE_FORMAT infmt = (bpp==2) ? CM_SURFACE_FORMAT_P010 : CM_SURFACE_FORMAT_NV12;

        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_copy_video_to_system_hsw, sizeof(genx_hevce_copy_video_to_system_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(CopyVideoToSystem420), kernel);
        CHECK_CM_ERR(res);

        CmSurface2D *in = 0;
        res = device->CreateSurface2D(width, height, infmt, in);
        CHECK_CM_ERR(res);
        res = in->WriteSurfaceStride((const Ipp8u*)inData, NULL, width*bpp);
        CHECK_CM_ERR(res);

        mfxU32 outLumaPitch = 0;
        mfxU32 outLumaSize = 0;
        res = device->GetSurface2DInfo((width + 2*PADDING_LU)*bpp, height, CM_SURFACE_FORMAT_P8, outLumaPitch, outLumaSize);
        CHECK_CM_ERR(res);
        mfxU8 *outLumaSys = (mfxU8 *)CM_ALIGNED_MALLOC(outLumaSize, 0x1000);
        CmSurface2DUP *outLuma = 0;
        res = device->CreateSurface2DUP((width + 2*PADDING_LU)*bpp, height, CM_SURFACE_FORMAT_P8, outLumaSys, outLuma);
        CHECK_CM_ERR(res);

        mfxU32 outChromaPitch = 0;
        mfxU32 outChromaSize = 0;
        res = device->GetSurface2DInfo((width + 2*PADDING_CH)*bpp, heightChroma, CM_SURFACE_FORMAT_P8, outChromaPitch, outChromaSize);
        CHECK_CM_ERR(res);
        mfxU8 *outChromaSys = (mfxU8 *)CM_ALIGNED_MALLOC(outChromaSize, 0x1000);
        CmSurface2DUP *outChroma = 0;
        res = device->CreateSurface2DUP((width + 2*PADDING_CH)*bpp, heightChroma, CM_SURFACE_FORMAT_P8, outChromaSys, outChroma);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxIn = 0;
        res = in->GetIndex(idxIn);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutLuma = 0;
        res = outLuma->GetIndex(idxOutLuma);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutChroma = 0;
        res = outChroma->GetIndex(idxOutChroma);
        CHECK_CM_ERR(res);

        Ipp32u argIdx = 0;
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxIn), idxIn);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutLuma), idxOutLuma);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutChroma), idxOutChroma);
        CHECK_CM_ERR(res);
        Ipp32s paddingLuInBytes = PADDING_LU * bpp;
        res = kernel->SetKernelArg(argIdx++, sizeof(paddingLuInBytes), &paddingLuInBytes);
        CHECK_CM_ERR(res);
        Ipp32s paddingChInBytes = PADDING_CH * bpp;
        res = kernel->SetKernelArg(argIdx++, sizeof(paddingChInBytes), &paddingChInBytes);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = DIVUP(width, BLOCKW)*bpp;
        mfxU32 tsHeight = DIVUP(height, BLOCKH);
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

        CmEvent *e = 0;
        CHECK_CM_ERR(res);
        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);
        res = e->WaitForTaskFinished();
        CHECK_CM_ERR(res);

        for (Ipp32s y = 0; y < height; y++)
            memcpy(outLumaData + y * width, outLumaSys + PADDING_LU*bpp + y * outLumaPitch, width * bpp);
        for (Ipp32s y = 0; y < heightChroma; y++)
            memcpy(outChromaData + y * width, outChromaSys + PADDING_CH*bpp + y * outChromaPitch, width * bpp);

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace)/1000000.);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroySurface(in);
        device->DestroySurface2DUP(outLuma);
        device->DestroySurface2DUP(outChroma);
        CM_ALIGNED_FREE(outLumaSys);
        CM_ALIGNED_FREE(outChromaSys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }

    template <mfxU32 chromaFormat, class T>
    int RunRef(const T *inData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData)
    {
        const Ipp32s bpp = sizeof(T);
        const Ipp32u heightChroma = height >> (chromaFormat == MFX_CHROMAFORMAT_YUV420 ? 1 : 0);

        memcpy(outLumaData, inData, width * height * bpp);
        memcpy(outChromaData, inData + width * height, width * heightChroma * bpp);

        return PASSED;
    }
}
