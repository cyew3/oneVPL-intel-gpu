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
#include "../include/genx_hevce_prepare_src_bdw_isa.h"
#include "../include/genx_hevce_prepare_src_hsw_isa.h"
#include "../include/genx_hevce_down_sample_mb_4tiers_bdw_isa.h"
#include "../include/genx_hevce_down_sample_mb_4tiers_hsw_isa.h"

enum {
    MFX_CHROMAFORMAT_YUV420 = 1,
    MFX_CHROMAFORMAT_YUV422 = 2,
};

#ifdef CMRT_EMU
extern "C" {
    void PrepareSrcNv12(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, Ipp32u srcPaddingLu, Ipp32u srcPaddingCh,
                        SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X);
    void PrepareSrcP010(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, Ipp32u srcPaddingLu, Ipp32u srcPaddingCh, SurfaceIndex DST_LU, SurfaceIndex DST_CH,
                        SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X);
    void PrepareSrcNv16(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, Ipp32u srcPaddingLu, Ipp32u srcPaddingCh, SurfaceIndex DST_CH,
                        SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X);
    void PrepareSrcP210(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, Ipp32u srcPaddingLu, Ipp32u srcPaddingCh, SurfaceIndex DST_LU, SurfaceIndex DST_CH,
                        SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X);
};
#endif //CMRT_EMU

namespace {
    template <mfxU32 chromaFormat, class T>
    int RunGpu(const T *inLumaData, const T *inChromaData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData,
               mfxU8 *out1xData, mfxU8 *out2xData, mfxU8 *out4xData, mfxU8 *out8xData, mfxU8 *out16xData);

    template <mfxU32 chromaFormat, class T>
    int RunRef(const T *inLumaData, const T *inChromaData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData,
               mfxU8 *out1xData, mfxU8 *out2xData, mfxU8 *out4xData, mfxU8 *out8xData, mfxU8 *out16xData);

    typedef std::vector<mfxU8> vec8;
    typedef std::vector<mfxU16> vec16;
    int RunGpuNv12(const vec8 &inLuma, const vec8 &inChroma, Ipp32s width, Ipp32s height, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV420>(inLuma.data(), inChroma.data(), width, height, (mfxU8*)nullptr, (mfxU8*)nullptr, out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }
    int RunGpuNv16(const vec8 &inLuma, const vec8 &inChroma, Ipp32s width, Ipp32s height, vec8 &outChroma, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV422>(inLuma.data(), inChroma.data(), width, height, (mfxU8*)nullptr, outChroma.data(), out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }
    int RunGpuP010(const vec16 &inLuma, const vec16 &inChroma, Ipp32s width, Ipp32s height, vec16 &outLuma, vec16 &outChroma, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV420>(inLuma.data(), inChroma.data(), width, height, outLuma.data(), outChroma.data(), out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }
    int RunGpuP210(const vec16 &inLuma, const vec16 &inChroma, Ipp32s width, Ipp32s height, vec16 &outLuma, vec16 &outChroma, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV422>(inLuma.data(), inChroma.data(), width, height, outLuma.data(), outChroma.data(), out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }

    int RunRefNv12(const vec8 &inLuma, const vec8 &inChroma, Ipp32s width, Ipp32s height, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV420>(inLuma.data(), inChroma.data(), width, height, (mfxU8*)nullptr, (mfxU8*)nullptr, out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }
    int RunRefNv16(const vec8 &inLuma, const vec8 &inChroma, Ipp32s width, Ipp32s height, vec8 &outChroma, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV422>(inLuma.data(), inChroma.data(), width, height, (mfxU8*)nullptr, outChroma.data(), out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }
    int RunRefP010(const vec16 &inLuma, const vec16 &inChroma, Ipp32s width, Ipp32s height, vec16 &outLuma, vec16 &outChroma, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV420>(inLuma.data(), inChroma.data(), width, height, outLuma.data(), outChroma.data(), out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }
    int RunRefP210(const vec16 &inLuma, const vec16 &inChroma, Ipp32s width, Ipp32s height, vec16 &outLuma, vec16 &outChroma, vec8 &out1x, vec8 &out2x, vec8 &out4x, vec8 &out8x, vec8 &out16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV422>(inLuma.data(), inChroma.data(), width, height, outLuma.data(), outChroma.data(), out1x.data(), out2x.data(), out4x.data(), out8x.data(), out16x.data());
    }

    template <class T> int Compare(const T *outGpu, const T *outRef, Ipp32s width, Ipp32s height, const char *plane);
    template <class T> int Compare(const std::vector<T> &outGpu, const std::vector<T> &outRef, Ipp32s width, Ipp32s height, const char *plane) { return Compare(outGpu.data(), outRef.data(), width, height, plane); }
};


int main()
{
    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;

    // const per frame params
    Ipp32s inW = 1920;
    Ipp32s inH = 1080;
    Ipp32s outW1x = inW;
    Ipp32s outW2x = ROUNDUP(inW/2, 16);
    Ipp32s outW4x = ROUNDUP(inW/4, 16);
    Ipp32s outW8x = ROUNDUP(inW/8, 16);
    Ipp32s outW16x = ROUNDUP(inW/16, 16);
    Ipp32s outH1x = inH;
    Ipp32s outH2x = ROUNDUP(inH/2, 16);
    Ipp32s outH4x = ROUNDUP(inH/4, 16);
    Ipp32s outH8x = ROUNDUP(inH/8, 16);
    Ipp32s outH16x = ROUNDUP(inH/16, 16);

    std::vector<mfxU8>  inLuma8(inW * inH);
    std::vector<mfxU16> inLuma10(inW * inH);
    std::vector<mfxU8>  inChromaNv12(inW * inH / 2);
    std::vector<mfxU8>  inChromaNv16(inW * inH);
    std::vector<mfxU16> inChromaP010(inW * inH / 2);
    std::vector<mfxU16> inChromaP210(inW * inH);

    std::vector<mfxU16> outLuma10Gpu(outW1x * outH1x);
    std::vector<mfxU16> outLuma10Ref(outW1x * outH1x);
    std::vector<mfxU8>  outChromaNv16Gpu(outW1x * outH1x);
    std::vector<mfxU8>  outChromaNv16Ref(outW1x * outH1x);
    std::vector<mfxU16> outChromaP010Gpu(outW1x * outH1x / 2);
    std::vector<mfxU16> outChromaP010Ref(outW1x * outH1x / 2);
    std::vector<mfxU16> outChromaP210Gpu(outW1x * outH1x);
    std::vector<mfxU16> outChromaP210Ref(outW1x * outH1x);

    std::vector<mfxU8>  out1xGpu(outW1x * outH1x * 3 / 2); // in case of nv12 input out1x contains both luma and chroma
    std::vector<mfxU8>  out1xRef(outW1x * outH1x * 3 / 2);
    std::vector<mfxU8>  out2xGpu(outW2x * outH2x * 3 / 2); // for others chroma is zero
    std::vector<mfxU8>  out2xRef(outW2x * outH2x * 3 / 2);
    std::vector<mfxU8>  out4xGpu(outW4x * outH4x * 3 / 2);
    std::vector<mfxU8>  out4xRef(outW4x * outH4x * 3 / 2);
    std::vector<mfxU8>  out8xGpu(outW8x * outH8x * 3 / 2);
    std::vector<mfxU8>  out8xRef(outW8x * outH8x * 3 / 2);
    std::vector<mfxU8>  out16xGpu(outW16x * outH16x * 3 / 2);
    std::vector<mfxU8>  out16xRef(outW16x * outH16x * 3 / 2);

    srand(0x1234578/*time(0)*/);
    std::generate(inLuma8.begin(),  inLuma8.end(),  [](){ return mfxU8(rand()); });
    std::generate(inLuma10.begin(), inLuma10.end(), [](){ return mfxU16(rand() & 0x3ff); });
    std::generate(inChromaNv12.begin(), inChromaNv12.end(), [](){ return mfxU8(rand()); });
    std::generate(inChromaNv16.begin(), inChromaNv16.end(), [](){ return mfxU8(rand()); });
    std::generate(inChromaP010.begin(), inChromaP010.end(), [](){ return mfxU16(rand() & 0x3ff); });
    std::generate(inChromaP210.begin(), inChromaP210.end(), [](){ return mfxU16(rand() & 0x3ff); });

    Ipp32s res;

#define DISABLE_GPU_POSTPROC_FOR_REXT

    printf("NV12: ");
    res = RunGpuNv12(inLuma8, inChromaNv12, inW, inH, out1xGpu, out2xGpu, out4xGpu, out8xGpu, out16xGpu);
    CHECK_ERR(res);
    res = RunRefNv12(inLuma8, inChromaNv12, inW, inH, out1xRef, out2xRef, out4xRef, out8xRef, out16xRef);
    CHECK_ERR(res);
    if (Compare(out1xGpu, out1xRef, outW1x, outH1x*3/2, "luma&chroma 1x") != PASSED ||
        Compare(out2xGpu, out2xRef, outW2x, outH2x, "2x") != PASSED || Compare(out4xGpu,  out4xRef,  outW4x,  outH4x,  "4x")  != PASSED ||
        Compare(out8xGpu, out8xRef, outW8x, outH8x, "8x") != PASSED || Compare(out16xGpu, out16xRef, outW16x, outH16x, "16x") != PASSED)
        return 1;
    printf("passed\n");

    printf("P010: ");
    res = RunGpuP010(inLuma10, inChromaP010, inW, inH, outLuma10Gpu, outChromaP010Gpu, out1xGpu, out2xGpu, out4xGpu, out8xGpu, out16xGpu);
    CHECK_ERR(res);
    res = RunRefP010(inLuma10, inChromaP010, inW, inH, outLuma10Ref, outChromaP010Ref, out1xRef, out2xRef, out4xRef, out8xRef, out16xRef);
    CHECK_ERR(res);
    if (
#ifndef DISABLE_GPU_POSTPROC_FOR_REXT
        Compare(outLuma10Gpu,     outLuma10Ref,     outW1x, outH1x,   "p010 luma")   != PASSED ||
        Compare(outChromaP010Gpu, outChromaP010Ref, outW1x, outH1x/2, "p010 chroma") != PASSED ||
#endif
        Compare(out1xGpu,  out1xRef,  outW1x,  outH1x,  "luma 1x") != PASSED || Compare(out2xGpu, out2xRef, outW2x, outH2x, "2x") != PASSED ||
        Compare(out4xGpu,  out4xRef,  outW4x,  outH4x,  "4x")      != PASSED || Compare(out8xGpu, out8xRef, outW8x, outH8x, "8x") != PASSED ||
        Compare(out16xGpu, out16xRef, outW16x, outH16x, "16x")     != PASSED)
        return 1;
    printf("passed\n");

    printf("NV16: ");
    res = RunGpuNv16(inLuma8, inChromaNv16, inW, inH, outChromaNv16Gpu, out1xGpu, out2xGpu, out4xGpu, out8xGpu, out16xGpu);
    CHECK_ERR(res);
    res = RunRefNv16(inLuma8, inChromaNv16, inW, inH, outChromaNv16Ref, out1xRef, out2xRef, out4xRef, out8xRef, out16xRef);
    CHECK_ERR(res);
    if (
#ifndef DISABLE_GPU_POSTPROC_FOR_REXT
        Compare(outChromaNv16Gpu,  outChromaNv16Ref, outW1x, outH1x, "nv16 chroma") != PASSED ||
#endif
        Compare(out1xGpu,  out1xRef,  outW1x,  outH1x,  "luma 1x") != PASSED || Compare(out2xGpu, out2xRef, outW2x, outH2x, "2x") != PASSED ||
        Compare(out4xGpu,  out4xRef,  outW4x,  outH4x,  "4x")      != PASSED || Compare(out8xGpu, out8xRef, outW8x, outH8x, "8x") != PASSED ||
        Compare(out16xGpu, out16xRef, outW16x, outH16x, "16x")     != PASSED)
        return 1;
    printf("passed\n");

    printf("P210: ");
    res = RunGpuP210(inLuma10, inChromaP210, inW, inH, outLuma10Gpu, outChromaP210Gpu, out1xGpu, out2xGpu, out4xGpu, out8xGpu, out16xGpu);
    CHECK_ERR(res);
    res = RunRefP210(inLuma10, inChromaP210, inW, inH, outLuma10Ref, outChromaP210Ref, out1xRef, out2xRef, out4xRef, out8xRef, out16xRef);
    CHECK_ERR(res);
    if (
#ifndef DISABLE_GPU_POSTPROC_FOR_REXT
        Compare(outLuma10Gpu,     outLuma10Ref,     outW1x, outH1x, "p210 luma")   != PASSED ||
        Compare(outChromaP210Gpu, outChromaP210Ref, outW1x, outH1x, "p210 chroma") != PASSED ||
#endif
        Compare(out1xGpu,  out1xRef,  outW1x,  outH1x,  "luma 1x") != PASSED || Compare(out2xGpu, out2xRef, outW2x, outH2x, "2x") != PASSED ||
        Compare(out4xGpu,  out4xRef,  outW4x,  outH4x,  "4x")      != PASSED || Compare(out8xGpu, out8xRef, outW8x, outH8x, "8x") != PASSED ||
        Compare(out16xGpu, out16xRef, outW16x, outH16x, "16x")     != PASSED)
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
    int RunGpu(const T *inLumaData, const T *inChromaData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData,
               mfxU8 *out1xData, mfxU8 *out2xData, mfxU8 *out4xData, mfxU8 *out8xData, mfxU8 *out16xData)
    {
        const Ipp32s bpp = sizeof(T);
        const Ipp32u PADDING_LU = 80;
        const Ipp32u PADDING_CH = 80;
        const Ipp32u heightChroma = height >> (chromaFormat == MFX_CHROMAFORMAT_YUV420 ? 1 : 0);
        const CM_SURFACE_FORMAT infmt = (bpp==2) ? CM_SURFACE_FORMAT_V8U8 : CM_SURFACE_FORMAT_P8;

        Ipp32s outW1x = width;
        Ipp32s outW2x = ROUNDUP(width/2, 16);
        Ipp32s outW4x = ROUNDUP(width/4, 16);
        Ipp32s outW8x = ROUNDUP(width/8, 16);
        Ipp32s outW16x = ROUNDUP(width/16, 16);
        Ipp32s outH1x = height;
        Ipp32s outH2x = ROUNDUP(height/2, 16);
        Ipp32s outH4x = ROUNDUP(height/4, 16);
        Ipp32s outH8x = ROUNDUP(height/8, 16);
        Ipp32s outH16x = ROUNDUP(height/16, 16);

        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_prepare_src_hsw, sizeof(genx_hevce_prepare_src_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        if      (chromaFormat == MFX_CHROMAFORMAT_YUV420 && bpp == 1) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcNv12), kernel);
        else if (chromaFormat == MFX_CHROMAFORMAT_YUV422 && bpp == 1) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcNv16), kernel);
        else if (chromaFormat == MFX_CHROMAFORMAT_YUV420 && bpp == 2) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcP010), kernel);
        else if (chromaFormat == MFX_CHROMAFORMAT_YUV422 && bpp == 2) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcP210), kernel);
        CHECK_CM_ERR(res);

        mfxU32 inLumaPitch = 0;
        mfxU32 inLumaSize = 0;
        res = device->GetSurface2DInfo(width + 2*PADDING_LU, height, infmt, inLumaPitch, inLumaSize);
        CHECK_CM_ERR(res);
        mfxU8 *inputLumaSys = (mfxU8 *)CM_ALIGNED_MALLOC(inLumaSize, 0x1000);
        CmSurface2DUP *inputLuma = 0;
        res = device->CreateSurface2DUP(width + 2*PADDING_LU, height, infmt, inputLumaSys, inputLuma);
        CHECK_CM_ERR(res);

        mfxU32 inChromaPitch = 0;
        mfxU32 inChromaSize = 0;
        res = device->GetSurface2DInfo(width + 2*PADDING_CH, heightChroma, infmt, inChromaPitch, inChromaSize);
        CHECK_CM_ERR(res);
        mfxU8 *inputChromaSys = (mfxU8 *)CM_ALIGNED_MALLOC(inChromaSize, 0x1000);
        CmSurface2DUP *inputChroma = 0;
        res = device->CreateSurface2DUP(width + 2*PADDING_CH, heightChroma, infmt, inputChromaSys, inputChroma);
        CHECK_CM_ERR(res);

        CopyAndPad(inLumaData,   width*bpp, (T*)inputLumaSys,   inLumaPitch,   width, height,       PADDING_LU);
        CopyAndPad(inChromaData, width*bpp, (T*)inputChromaSys, inChromaPitch, width, heightChroma, PADDING_CH);

        CmSurface2D *outputLuma10 = 0;
        res = device->CreateSurface2D(width*bpp, height, CM_SURFACE_FORMAT_P8, outputLuma10);
        CHECK_CM_ERR(res);

        CmSurface2D *outputChromaRext = 0;
        res = device->CreateSurface2D(width*bpp, heightChroma, CM_SURFACE_FORMAT_P8, outputChromaRext);
        CHECK_CM_ERR(res);

        CmSurface2D *output1x = 0;
        res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, output1x);
        CHECK_CM_ERR(res);

        CmSurface2D *output2x = 0;
        res = device->CreateSurface2D(outW2x, outH2x, CM_SURFACE_FORMAT_NV12, output2x);
        CHECK_CM_ERR(res);

        CmSurface2D *output4x = 0;
        res = device->CreateSurface2D(outW4x, outH4x, CM_SURFACE_FORMAT_NV12, output4x);
        CHECK_CM_ERR(res);

        CmSurface2D *output8x = 0;
        res = device->CreateSurface2D(outW8x, outH8x, CM_SURFACE_FORMAT_NV12, output8x);
        CHECK_CM_ERR(res);

        CmSurface2D *output16x = 0;
        res = device->CreateSurface2D(outW16x, outH16x, CM_SURFACE_FORMAT_NV12, output16x);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxInputLuma = 0;
        res = inputLuma->GetIndex(idxInputLuma);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxInputChroma = 0;
        res = inputChroma->GetIndex(idxInputChroma);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutputLuma10 = 0;
        res = outputLuma10->GetIndex(idxOutputLuma10);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutputChromaRext = 0;
        res = outputChromaRext->GetIndex(idxOutputChromaRext);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutput1x = 0;
        res = output1x->GetIndex(idxOutput1x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutput2x = 0;
        res = output2x->GetIndex(idxOutput2x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutput4x = 0;
        res = output4x->GetIndex(idxOutput4x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutput8x = 0;
        res = output8x->GetIndex(idxOutput8x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxOutput16x = 0;
        res = output16x->GetIndex(idxOutput16x);
        CHECK_CM_ERR(res);

        Ipp32u argIdx = 0;
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxInputLuma), idxInputLuma);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxInputChroma), idxInputChroma);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(PADDING_LU), &PADDING_LU);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(PADDING_CH), &PADDING_CH);
        CHECK_CM_ERR(res);
        if (bpp == 2) {
            res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutputLuma10), idxOutputLuma10);
            CHECK_CM_ERR(res);
        }
        if (bpp == 2 || chromaFormat == MFX_CHROMAFORMAT_YUV422) {
            res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutputChromaRext), idxOutputChromaRext);
            CHECK_CM_ERR(res);
        }
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutput1x), idxOutput1x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutput2x), idxOutput2x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutput4x), idxOutput4x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutput8x), idxOutput8x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxOutput16x), idxOutput16x);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = outW16x/4;
        mfxU32 tsHeight = outH16x;
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

        output1x->ReadSurfaceStride(out1xData, NULL, width);
        output2x->ReadSurfaceStride(out2xData, NULL, outW2x);
        output4x->ReadSurfaceStride(out4xData, NULL, outW4x);
        output8x->ReadSurfaceStride(out8xData, NULL, outW8x);
        output16x->ReadSurfaceStride(out16xData, NULL, outW16x);
        if (outLumaData)
            outputLuma10->ReadSurfaceStride((mfxU8 *)outLumaData, NULL, width*bpp);
        if (outChromaData)
            outputChromaRext->ReadSurfaceStride((mfxU8 *)outChromaData, NULL, width*bpp);

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace)/1000000.);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroySurface2DUP(inputLuma);
        device->DestroySurface2DUP(inputChroma);
        CM_ALIGNED_FREE(inputLumaSys);
        CM_ALIGNED_FREE(inputChromaSys);
        device->DestroySurface(output1x);
        device->DestroySurface(output2x);
        device->DestroySurface(output4x);
        device->DestroySurface(output8x);
        device->DestroySurface(output16x);
        device->DestroySurface(outputLuma10);
        device->DestroySurface(outputChromaRext);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }

    template <mfxU32 chromaFormat, class T>
    int RunRef(const T *inLumaData, const T *inChromaData, Ipp32s width, Ipp32s height, T *outLumaData, T *outChromaData,
               mfxU8 *out1xData, mfxU8 *out2xData, mfxU8 *out4xData, mfxU8 *out8xData, mfxU8 *out16xData)
    {
        const Ipp32s bpp = sizeof(T);
        const Ipp32u heightChroma = height >> (chromaFormat == MFX_CHROMAFORMAT_YUV420 ? 1 : 0);

        if (outLumaData) {
            memcpy(outLumaData, inLumaData, width * height * bpp); // 10bit copy
            std::transform(inLumaData, inLumaData + width * height, out1xData, [](T in) -> mfxU8 { return (mfxU8)MIN(0xff, (in + 2) >> 2); }); // 8bit copy
        } else {
            memcpy(out1xData, inLumaData, width * height);
        }

        if (outChromaData) {
            memcpy(outChromaData, inChromaData, width * heightChroma * bpp);
            memset(out1xData + width * height, 0, width * height/2);
        } else {
            memcpy(out1xData + width * height, inChromaData, width * height/2);
        }

        Ipp32s outW1x = width;
        Ipp32s outW2x = ROUNDUP(width/2, 16);
        Ipp32s outW4x = ROUNDUP(width/4, 16);
        Ipp32s outW8x = ROUNDUP(width/8, 16);
        Ipp32s outW16x = ROUNDUP(width/16, 16);
        Ipp32s outH1x = height;
        Ipp32s outH2x = ROUNDUP(height/2, 16);
        Ipp32s outH4x = ROUNDUP(height/4, 16);
        Ipp32s outH8x = ROUNDUP(height/8, 16);
        Ipp32s outH16x = ROUNDUP(height/16, 16);

        memset(out2xData  +  outW2x *  outH2x, 0,  outW2x *  outH2x / 2);
        memset(out4xData  +  outW4x *  outH4x, 0,  outW4x *  outH4x / 2);
        memset(out8xData  +  outW8x *  outH8x, 0,  outW8x *  outH8x / 2);
        memset(out16xData + outW16x * outH16x, 0, outW16x * outH16x / 2);

        auto Average2x2 = [](const mfxU8 *p, Ipp32s pitch) { return mfxU8((p[0] + p[1] + p[pitch] + p[pitch + 1] + 2) >> 2); };

        Ipp8u in[16*16];
        Ipp8u out2x[8*8];
        Ipp8u out4x[4*4];
        Ipp8u out8x[2*2];
        for (Ipp32s y = 0; y < outH16x; y++) {
            for (Ipp32s x = 0; x < outW16x; x++) {
                for (Ipp32s i = 0; i < 16; i++)
                    for (Ipp32s j = 0; j < 16; j++)
                        in[j+16*i] = out1xData[MIN(width-1, x*16+j) + width * MIN(height-1, y*16+i)];
                for (Ipp32s i = 0; i < 8; i++)
                    for (Ipp32s j = 0; j < 8; j++)
                        out2x[j+8*i] = Average2x2(in + j*2+i*2*16, 16);
                for (Ipp32s i = 0; i < 4; i++)
                    for (Ipp32s j = 0; j < 4; j++)
                        out4x[j+4*i] = Average2x2(out2x + j*2+i*2*8, 8);
                for (Ipp32s i = 0; i < 2; i++)
                    for (Ipp32s j = 0; j < 2; j++)
                        out8x[j+2*i] = Average2x2(out4x + j*2+i*2*4, 4);

                out16xData[x+outW16x*y] = Average2x2(out8x, 2);

                for (Ipp32s i = 0; i < 2; i++)
                    for (Ipp32s j = 0; j < 2; j++)
                        if (x*2+j < outW8x && y*2+i < outH8x)
                            out8xData[x*2+j+outW8x*(y*2+i)] = out8x[j+2*i];
                for (Ipp32s i = 0; i < 4; i++)
                    for (Ipp32s j = 0; j < 4; j++)
                        if (x*4+j < outW4x && y*4+i < outH4x)
                            out4xData[x*4+j+outW4x*(y*4+i)] = out4x[j+4*i];
                for (Ipp32s i = 0; i < 8; i++)
                    for (Ipp32s j = 0; j < 8; j++)
                        if (x*8+j < outW2x && y*8+i < outH2x)
                            out2xData[x*8+j+outW2x*(y*8+i)] = out2x[j+8*i];
            }
        }

        return PASSED;
    }
}
