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
    void PrepareRefNv12(SurfaceIndex SRC_1X, SurfaceIndex DST_LU, SurfaceIndex DST_CH, Ipp32u dstPaddingLu, Ipp32u dstPaddingCh,
                        SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X);
};
#endif //CMRT_EMU

namespace {
    template <mfxU32 chromaFormat, class T>
    int RunGpu(T *sysLumaData, T *sysChromaData, Ipp32s width, Ipp32s height, T *vidLumaRextData, T *vidChromaRextData,
               mfxU8 *vid1xData, mfxU8 *vid2xData, mfxU8 *vid4xData, mfxU8 *vid8xData, mfxU8 *vid16xData, bool fromSys);

    template <mfxU32 chromaFormat, class T>
    int RunRef(T *sysLumaData, T *sysChromaData, Ipp32s width, Ipp32s height, T *vidLumaRextData, T *vidChromaRextData,
               mfxU8 *vid1xData, mfxU8 *vid2xData, mfxU8 *vid4xData, mfxU8 *vid8xData, mfxU8 *vid16xData, bool fromSys);

    typedef std::vector<mfxU8> vec8;
    typedef std::vector<mfxU16> vec16;
    int RunGpuNv12(const vec8 &sysLuma, const vec8 &sysChroma, Ipp32s width, Ipp32s height, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV420>((mfxU8*)sysLuma.data(), (mfxU8*)sysChroma.data(), width, height, (mfxU8*)nullptr, (mfxU8*)nullptr, vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }
    int RunGpuNv16(const vec8 &sysLuma, const vec8 &sysChroma, Ipp32s width, Ipp32s height, vec8 &vidChroma, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV422>((mfxU8*)sysLuma.data(), (mfxU8*)sysChroma.data(), width, height, (mfxU8*)nullptr, vidChroma.data(), vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }
    int RunGpuP010(const vec16 &sysLuma, const vec16 &sysChroma, Ipp32s width, Ipp32s height, vec16 &vidLuma, vec16 &vidChroma, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV420>((mfxU16*)sysLuma.data(), (mfxU16*)sysChroma.data(), width, height, vidLuma.data(), vidChroma.data(), vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }
    int RunGpuP210(const vec16 &sysLuma, const vec16 &sysChroma, Ipp32s width, Ipp32s height, vec16 &vidLuma, vec16 &vidChroma, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV422>((mfxU16*)sysLuma.data(), (mfxU16*)sysChroma.data(), width, height, vidLuma.data(), vidChroma.data(), vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }
    int RunFromSysGpuNv12(vec8 &sysLuma, vec8 &sysChroma, Ipp32s width, Ipp32s height, const vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunGpu<MFX_CHROMAFORMAT_YUV420>(sysLuma.data(), sysChroma.data(), width, height, (mfxU8*)nullptr, (mfxU8*)nullptr, (mfxU8*)vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), false);
    }

    int RunRefNv12(const vec8 &sysLuma, const vec8 &sysChroma, Ipp32s width, Ipp32s height, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV420>((mfxU8*)sysLuma.data(), (mfxU8*)sysChroma.data(), width, height, (mfxU8*)nullptr, (mfxU8*)nullptr, vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }
    int RunRefNv16(const vec8 &sysLuma, const vec8 &sysChroma, Ipp32s width, Ipp32s height, vec8 &vidChroma, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV422>((mfxU8*)sysLuma.data(), (mfxU8*)sysChroma.data(), width, height, (mfxU8*)nullptr, vidChroma.data(), vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }
    int RunRefP010(const vec16 &sysLuma, const vec16 &sysChroma, Ipp32s width, Ipp32s height, vec16 &vidLuma, vec16 &vidChroma, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV420>((mfxU16*)sysLuma.data(), (mfxU16*)sysChroma.data(), width, height, vidLuma.data(), vidChroma.data(), vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }
    int RunRefP210(const vec16 &sysLuma, const vec16 &sysChroma, Ipp32s width, Ipp32s height, vec16 &vidLuma, vec16 &vidChroma, vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV422>((mfxU16*)sysLuma.data(), (mfxU16*)sysChroma.data(), width, height, vidLuma.data(), vidChroma.data(), vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), true);
    }

    int RunFromSysRefNv12(vec8 &sysLuma, vec8 &sysChroma, Ipp32s width, Ipp32s height, const vec8 &vid1x, vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x) {
        return RunRef<MFX_CHROMAFORMAT_YUV420>(sysLuma.data(), sysChroma.data(), width, height, (mfxU8*)nullptr, (mfxU8*)nullptr, (mfxU8*)vid1x.data(), vid2x.data(), vid4x.data(), vid8x.data(), vid16x.data(), false);
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
    Ipp32s width = 1920;
    Ipp32s height = 1080;
    Ipp32s outW2x = ROUNDUP(width/2, 16);
    Ipp32s outW4x = ROUNDUP(width/4, 16);
    Ipp32s outW8x = ROUNDUP(width/8, 16);
    Ipp32s outW16x = ROUNDUP(width/16, 16);
    Ipp32s outH2x = ROUNDUP(height/2, 16);
    Ipp32s outH4x = ROUNDUP(height/4, 16);
    Ipp32s outH8x = ROUNDUP(height/8, 16);
    Ipp32s outH16x = ROUNDUP(height/16, 16);

    // used as input
    std::vector<mfxU8>  sysLuma8(width * height);
    std::vector<mfxU16> sysLuma10(width * height);
    std::vector<mfxU8>  sysChromaNv12(width * height / 2);
    std::vector<mfxU8>  sysChromaNv16(width * height);
    std::vector<mfxU16> sysChromaP010(width * height / 2);
    std::vector<mfxU16> sysChromaP210(width * height);
    std::vector<mfxU8>  vid1x(width * height * 3 / 2);

    // used as output
    std::vector<mfxU16> vidLuma10Gpu(width * height);
    std::vector<mfxU16> vidLuma10Ref(width * height);
    std::vector<mfxU8>  vidChromaNv16Gpu(width * height);
    std::vector<mfxU8>  vidChromaNv16Ref(width * height);
    std::vector<mfxU16> vidChromaP010Gpu(width * height / 2);
    std::vector<mfxU16> vidChromaP010Ref(width * height / 2);
    std::vector<mfxU16> vidChromaP210Gpu(width * height);
    std::vector<mfxU16> vidChromaP210Ref(width * height);
    std::vector<mfxU8>  sysLuma8Gpu(width * height);
    std::vector<mfxU8>  sysLuma8Ref(width * height);
    std::vector<mfxU8>  sysChromaNv12Gpu(width * height / 2);
    std::vector<mfxU8>  sysChromaNv12Ref(width * height / 2);
    std::vector<mfxU8>  vid1xGpu(width * height * 3 / 2);
    std::vector<mfxU8>  vid1xRef(width * height * 3 / 2);
    std::vector<mfxU8>  vid2xGpu(outW2x * outH2x * 3 / 2);
    std::vector<mfxU8>  vid2xRef(outW2x * outH2x * 3 / 2);
    std::vector<mfxU8>  vid4xGpu(outW4x * outH4x * 3 / 2);
    std::vector<mfxU8>  vid4xRef(outW4x * outH4x * 3 / 2);
    std::vector<mfxU8>  vid8xGpu(outW8x * outH8x * 3 / 2);
    std::vector<mfxU8>  vid8xRef(outW8x * outH8x * 3 / 2);
    std::vector<mfxU8>  vid16xGpu(outW16x * outH16x * 3 / 2);
    std::vector<mfxU8>  vid16xRef(outW16x * outH16x * 3 / 2);

    srand(0x1234578/*time(0)*/);
    std::generate(sysLuma8.begin(),  sysLuma8.end(),  [](){ return mfxU8(rand()); });
    std::generate(sysLuma10.begin(), sysLuma10.end(), [](){ return mfxU16(rand() & 0x3ff); });
    std::generate(sysChromaNv12.begin(), sysChromaNv12.end(), [](){ return mfxU8(rand()); });
    std::generate(sysChromaNv16.begin(), sysChromaNv16.end(), [](){ return mfxU8(rand()); });
    std::generate(sysChromaP010.begin(), sysChromaP010.end(), [](){ return mfxU16(rand() & 0x3ff); });
    std::generate(sysChromaP210.begin(), sysChromaP210.end(), [](){ return mfxU16(rand() & 0x3ff); });

    Ipp32s res;

#define NO_GPU_POSTPROC_FOR_REXT

    printf("PrepareSrcNV12: ");
    res = RunGpuNv12(sysLuma8, sysChromaNv12, width, height, vid1xGpu, vid2xGpu, vid4xGpu, vid8xGpu, vid16xGpu);
    CHECK_ERR(res);
    res = RunRefNv12(sysLuma8, sysChromaNv12, width, height, vid1xRef, vid2xRef, vid4xRef, vid8xRef, vid16xRef);
    CHECK_ERR(res);
    if (Compare(vid1xGpu, vid1xRef, width,  height*3/2, "luma&chroma 1x") != PASSED ||
        Compare(vid2xGpu, vid2xRef, outW2x, outH2x, "2x") != PASSED || Compare(vid4xGpu,  vid4xRef,  outW4x,  outH4x,  "4x")  != PASSED ||
        Compare(vid8xGpu, vid8xRef, outW8x, outH8x, "8x") != PASSED || Compare(vid16xGpu, vid16xRef, outW16x, outH16x, "16x") != PASSED)
        return 1;
    printf("passed\n");

    printf("PrepareSrcP010: ");
    res = RunGpuP010(sysLuma10, sysChromaP010, width, height, vidLuma10Gpu, vidChromaP010Gpu, vid1xGpu, vid2xGpu, vid4xGpu, vid8xGpu, vid16xGpu);
    CHECK_ERR(res);
    res = RunRefP010(sysLuma10, sysChromaP010, width, height, vidLuma10Ref, vidChromaP010Ref, vid1xRef, vid2xRef, vid4xRef, vid8xRef, vid16xRef);
    CHECK_ERR(res);
    if (
#ifndef NO_GPU_POSTPROC_FOR_REXT
        Compare(vidLuma10Gpu,     vidLuma10Ref,     width, height,   "p010 luma")   != PASSED ||
        Compare(vidChromaP010Gpu, vidChromaP010Ref, width, height/2, "p010 chroma") != PASSED ||
#endif
        Compare(vid1xGpu,  vid1xRef,  width,   height,  "luma 1x") != PASSED || Compare(vid2xGpu, vid2xRef, outW2x, outH2x, "2x") != PASSED ||
        Compare(vid4xGpu,  vid4xRef,  outW4x,  outH4x,  "4x")      != PASSED || Compare(vid8xGpu, vid8xRef, outW8x, outH8x, "8x") != PASSED ||
        Compare(vid16xGpu, vid16xRef, outW16x, outH16x, "16x")     != PASSED)
        return 1;
    printf("passed\n");

    printf("PrepareSrcNV16: ");
    res = RunGpuNv16(sysLuma8, sysChromaNv16, width, height, vidChromaNv16Gpu, vid1xGpu, vid2xGpu, vid4xGpu, vid8xGpu, vid16xGpu);
    CHECK_ERR(res);
    res = RunRefNv16(sysLuma8, sysChromaNv16, width, height, vidChromaNv16Ref, vid1xRef, vid2xRef, vid4xRef, vid8xRef, vid16xRef);
    CHECK_ERR(res);
    if (
#ifndef NO_GPU_POSTPROC_FOR_REXT
        Compare(vidChromaNv16Gpu,  vidChromaNv16Ref, width, height, "nv16 chroma") != PASSED ||
#endif
        Compare(vid1xGpu,  vid1xRef,  width,   height,  "luma 1x") != PASSED || Compare(vid2xGpu, vid2xRef, outW2x, outH2x, "2x") != PASSED ||
        Compare(vid4xGpu,  vid4xRef,  outW4x,  outH4x,  "4x")      != PASSED || Compare(vid8xGpu, vid8xRef, outW8x, outH8x, "8x") != PASSED ||
        Compare(vid16xGpu, vid16xRef, outW16x, outH16x, "16x")     != PASSED)
        return 1;
    printf("passed\n");

    printf("PrepareSrcP210: ");
    res = RunGpuP210(sysLuma10, sysChromaP210, width, height, vidLuma10Gpu, vidChromaP210Gpu, vid1xGpu, vid2xGpu, vid4xGpu, vid8xGpu, vid16xGpu);
    CHECK_ERR(res);
    res = RunRefP210(sysLuma10, sysChromaP210, width, height, vidLuma10Ref, vidChromaP210Ref, vid1xRef, vid2xRef, vid4xRef, vid8xRef, vid16xRef);
    CHECK_ERR(res);
    if (
#ifndef NO_GPU_POSTPROC_FOR_REXT
        Compare(vidLuma10Gpu,     vidLuma10Ref,     width, height, "p210 luma")   != PASSED ||
        Compare(vidChromaP210Gpu, vidChromaP210Ref, width, height, "p210 chroma") != PASSED ||
#endif
        Compare(vid1xGpu,  vid1xRef,  width,   height,  "luma 1x") != PASSED || Compare(vid2xGpu, vid2xRef, outW2x, outH2x, "2x") != PASSED ||
        Compare(vid4xGpu,  vid4xRef,  outW4x,  outH4x,  "4x")      != PASSED || Compare(vid8xGpu, vid8xRef, outW8x, outH8x, "8x") != PASSED ||
        Compare(vid16xGpu, vid16xRef, outW16x, outH16x, "16x")     != PASSED)
        return 1;
    printf("passed\n");

    std::generate(vid1x.begin(), vid1x.end(), [](){ return mfxU8(rand()); });

    printf("PrepareRefNV12: ");
    res = RunFromSysGpuNv12(sysLuma8Gpu, sysChromaNv12Gpu, width, height, vid1x, vid2xGpu, vid4xGpu, vid8xGpu, vid16xGpu);
    CHECK_ERR(res);
    res = RunFromSysRefNv12(sysLuma8Ref, sysChromaNv12Ref, width, height, vid1x, vid2xRef, vid4xRef, vid8xRef, vid16xRef);
    CHECK_ERR(res);
    if (Compare(sysLuma8Gpu,      sysLuma8Ref,      width, height,   "luma sys")   != PASSED ||
        Compare(sysChromaNv12Gpu, sysChromaNv12Ref, width, height/2, "chroma sys") != PASSED ||
        Compare(vid2xGpu, vid2xRef, outW2x, outH2x, "2x") != PASSED || Compare(vid4xGpu,  vid4xRef,  outW4x,  outH4x,  "4x")  != PASSED ||
        Compare(vid8xGpu, vid8xRef, outW8x, outH8x, "8x") != PASSED || Compare(vid16xGpu, vid16xRef, outW16x, outH16x, "16x") != PASSED)
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
    int RunGpu(T *sysLumaData, T *sysChromaData, Ipp32s width, Ipp32s height, T *vidLumaData, T *vidChromaData,
               mfxU8 *vid1xData, mfxU8 *vid2xData, mfxU8 *vid4xData, mfxU8 *vid8xData, mfxU8 *vid16xData, bool fromSys)
    {
        const Ipp32s bpp = sizeof(T);
        const Ipp32u PADDING_LU = 80;
        const Ipp32u PADDING_CH = 80;
        const Ipp32s heightChroma = height >> (chromaFormat == MFX_CHROMAFORMAT_YUV420 ? 1 : 0);
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
        if (fromSys) {
            if      (chromaFormat == MFX_CHROMAFORMAT_YUV420 && bpp == 1) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcNv12), kernel);
            else if (chromaFormat == MFX_CHROMAFORMAT_YUV422 && bpp == 1) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcNv16), kernel);
            else if (chromaFormat == MFX_CHROMAFORMAT_YUV420 && bpp == 2) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcP010), kernel);
            else if (chromaFormat == MFX_CHROMAFORMAT_YUV422 && bpp == 2) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcP210), kernel);
        } else {
            if      (chromaFormat == MFX_CHROMAFORMAT_YUV420 && bpp == 1) res = device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareRefNv12), kernel);
        }
        CHECK_CM_ERR(res);

        mfxU32 sysLumaPitch = 0;
        mfxU32 sysLumaSize = 0;
        res = device->GetSurface2DInfo(width + 2*PADDING_LU, height, infmt, sysLumaPitch, sysLumaSize);
        CHECK_CM_ERR(res);
        mfxU8 *sysLuma = (mfxU8 *)CM_ALIGNED_MALLOC(sysLumaSize, 0x1000);
        CmSurface2DUP *sysLumaUp = 0;
        res = device->CreateSurface2DUP(width + 2*PADDING_LU, height, infmt, sysLuma, sysLumaUp);
        CHECK_CM_ERR(res);

        mfxU32 sysChromaPitch = 0;
        mfxU32 sysChromaSize = 0;
        res = device->GetSurface2DInfo(width + 2*PADDING_CH, heightChroma, infmt, sysChromaPitch, sysChromaSize);
        CHECK_CM_ERR(res);
        mfxU8 *sysChroma = (mfxU8 *)CM_ALIGNED_MALLOC(sysChromaSize, 0x1000);
        CmSurface2DUP *sysChromaUp = 0;
        res = device->CreateSurface2DUP(width + 2*PADDING_CH, heightChroma, infmt, sysChroma, sysChromaUp);
        CHECK_CM_ERR(res);

        CmSurface2D *vidLuma10 = 0;
        res = device->CreateSurface2D(width*bpp, height, CM_SURFACE_FORMAT_P8, vidLuma10);
        CHECK_CM_ERR(res);

        CmSurface2D *vidChromaRext = 0;
        res = device->CreateSurface2D(width*bpp, heightChroma, CM_SURFACE_FORMAT_P8, vidChromaRext);
        CHECK_CM_ERR(res);

        CmSurface2D *vid1x = 0;
        res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, vid1x);
        CHECK_CM_ERR(res);

        CmSurface2D *vid2x = 0;
        res = device->CreateSurface2D(outW2x, outH2x, CM_SURFACE_FORMAT_NV12, vid2x);
        CHECK_CM_ERR(res);

        CmSurface2D *vid4x = 0;
        res = device->CreateSurface2D(outW4x, outH4x, CM_SURFACE_FORMAT_NV12, vid4x);
        CHECK_CM_ERR(res);

        CmSurface2D *vid8x = 0;
        res = device->CreateSurface2D(outW8x, outH8x, CM_SURFACE_FORMAT_NV12, vid8x);
        CHECK_CM_ERR(res);

        CmSurface2D *vid16x = 0;
        res = device->CreateSurface2D(outW16x, outH16x, CM_SURFACE_FORMAT_NV12, vid16x);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxSysLuma = 0;
        res = sysLumaUp->GetIndex(idxSysLuma);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxSysChroma = 0;
        res = sysChromaUp->GetIndex(idxSysChroma);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxVidLuma10 = 0;
        res = vidLuma10->GetIndex(idxVidLuma10);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxVidChromaRext = 0;
        res = vidChromaRext->GetIndex(idxVidChromaRext);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxVid1x = 0;
        res = vid1x->GetIndex(idxVid1x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxVid2x = 0;
        res = vid2x->GetIndex(idxVid2x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxVid4x = 0;
        res = vid4x->GetIndex(idxVid4x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxVid8x = 0;
        res = vid8x->GetIndex(idxVid8x);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxVid16x = 0;
        res = vid16x->GetIndex(idxVid16x);
        CHECK_CM_ERR(res);

        Ipp32u argIdx = 0;
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxSysLuma);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxSysChroma);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(Ipp32u), &PADDING_LU);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(Ipp32u), &PADDING_CH);
        CHECK_CM_ERR(res);
        if (bpp == 2) {
            res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxVidLuma10);
            CHECK_CM_ERR(res);
        }
        if (bpp == 2 || chromaFormat == MFX_CHROMAFORMAT_YUV422) {
            res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxVidChromaRext);
            CHECK_CM_ERR(res);
        }
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxVid1x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxVid2x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxVid4x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxVid8x);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxVid16x);
        CHECK_CM_ERR(res);

        mfxU32 tsWidth  = outW16x/4;
        mfxU32 tsHeight = outH16x;
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        if (fromSys) {
            CopyAndPad(sysLumaData,  width*bpp, (T*)sysLuma,   sysLumaPitch,   width, height,       PADDING_LU);
            CopyAndPad(sysChromaData, width*bpp, (T*)sysChroma, sysChromaPitch, width, heightChroma, PADDING_CH);
        }
        else {
            vid1x->WriteSurfaceStride(vid1xData, NULL, width);
        }

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

        if (fromSys) {
            vid1x->ReadSurfaceStride(vid1xData, NULL, width);
            if (vidLumaData) vidLuma10->ReadSurfaceStride((mfxU8 *)vidLumaData, NULL, width*bpp);
            if (vidChromaData) vidChromaRext->ReadSurfaceStride((mfxU8 *)vidChromaData, NULL, width*bpp);
        } else {
            for (int y = 0; y < height; y++)
                memcpy(sysLumaData + y*width, sysLuma + PADDING_LU + y * sysLumaPitch, width);
            for (int y = 0; y < heightChroma; y++)
                memcpy(sysChromaData + y*width, sysChroma + PADDING_CH + y * sysChromaPitch, width);
        }
        vid2x->ReadSurfaceStride(vid2xData, NULL, outW2x);
        vid4x->ReadSurfaceStride(vid4xData, NULL, outW4x);
        vid8x->ReadSurfaceStride(vid8xData, NULL, outW8x);
        vid16x->ReadSurfaceStride(vid16xData, NULL, outW16x);

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace)/1000000.);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroySurface2DUP(sysLumaUp);
        device->DestroySurface2DUP(sysChromaUp);
        CM_ALIGNED_FREE(sysLuma);
        CM_ALIGNED_FREE(sysChroma);
        device->DestroySurface(vid1x);
        device->DestroySurface(vid2x);
        device->DestroySurface(vid4x);
        device->DestroySurface(vid8x);
        device->DestroySurface(vid16x);
        device->DestroySurface(vidLuma10);
        device->DestroySurface(vidChromaRext);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }

    template <mfxU32 chromaFormat, class T>
    int RunRef(T *sysLumaData, T *sysChromaData, Ipp32s width, Ipp32s height, T *vidLumaRextData, T *vidChromaRextData,
               mfxU8 *vid1xData, mfxU8 *vid2xData, mfxU8 *vid4xData, mfxU8 *vid8xData, mfxU8 *vid16xData, bool fromSys)
    {
        const Ipp32s bpp = sizeof(T);
        const Ipp32u heightChroma = height >> (chromaFormat == MFX_CHROMAFORMAT_YUV420 ? 1 : 0);

        if (fromSys) {
            if (vidLumaRextData) {
                memcpy(vidLumaRextData, sysLumaData, width * height * bpp); // 10bit copy
                std::transform(sysLumaData, sysLumaData + width * height, vid1xData, [](T in) -> mfxU8 { return (mfxU8)MIN(0xff, (in + 2) >> 2); }); // 8bit copy
            } else {
                memcpy(vid1xData, sysLumaData, width * height);
            }

            if (vidChromaRextData) {
                memcpy(vidChromaRextData, sysChromaData, width * heightChroma * bpp);
                memset(vid1xData + width * height, 0, width * height/2);
            } else {
                memcpy(vid1xData + width * height, sysChromaData, width * height/2);
            }
        } else {
            if (vidLumaRextData) {
                memcpy(sysLumaData, vidLumaRextData, width * height * bpp); // 10bit copy
                std::transform(sysLumaData, sysLumaData + width * height, vid1xData, [](T in) -> mfxU8 { return (mfxU8)MIN(0xff, (in + 2) >> 2); }); // 8bit copy
            } else {
                memcpy(sysLumaData, vid1xData, width * height);
            }

            if (vidChromaRextData) {
                memcpy(sysChromaData, vidChromaRextData, width * heightChroma * bpp);
                memset(vid1xData + width * height, 0, width * height/2);
            } else {
                memcpy(sysChromaData, vid1xData + width * height, width * height/2);
            }
        }

        Ipp32s vidW2x = ROUNDUP(width/2, 16);
        Ipp32s vidW4x = ROUNDUP(width/4, 16);
        Ipp32s vidW8x = ROUNDUP(width/8, 16);
        Ipp32s vidW16x = ROUNDUP(width/16, 16);
        Ipp32s vidH2x = ROUNDUP(height/2, 16);
        Ipp32s vidH4x = ROUNDUP(height/4, 16);
        Ipp32s vidH8x = ROUNDUP(height/8, 16);
        Ipp32s vidH16x = ROUNDUP(height/16, 16);

        memset(vid2xData  +  vidW2x *  vidH2x, 0,  vidW2x *  vidH2x / 2);
        memset(vid4xData  +  vidW4x *  vidH4x, 0,  vidW4x *  vidH4x / 2);
        memset(vid8xData  +  vidW8x *  vidH8x, 0,  vidW8x *  vidH8x / 2);
        memset(vid16xData + vidW16x * vidH16x, 0, vidW16x * vidH16x / 2);

        auto Average2x2 = [](const mfxU8 *p, Ipp32s pitch) { return mfxU8((p[0] + p[1] + p[pitch] + p[pitch + 1] + 2) >> 2); };

        Ipp8u blk16x16[16*16];
        Ipp8u blk8x8[8*8];
        Ipp8u blk4x4[4*4];
        Ipp8u blk2x2[2*2];
        for (Ipp32s y = 0; y < vidH16x; y++) {
            for (Ipp32s x = 0; x < vidW16x; x++) {
                for (Ipp32s i = 0; i < 16; i++)
                    for (Ipp32s j = 0; j < 16; j++)
                        blk16x16[j+16*i] = vid1xData[MIN(width-1, x*16+j) + width * MIN(height-1, y*16+i)];
                for (Ipp32s i = 0; i < 8; i++)
                    for (Ipp32s j = 0; j < 8; j++)
                        blk8x8[j+8*i] = Average2x2(blk16x16 + j*2+i*2*16, 16);
                for (Ipp32s i = 0; i < 4; i++)
                    for (Ipp32s j = 0; j < 4; j++)
                        blk4x4[j+4*i] = Average2x2(blk8x8 + j*2+i*2*8, 8);
                for (Ipp32s i = 0; i < 2; i++)
                    for (Ipp32s j = 0; j < 2; j++)
                        blk2x2[j+2*i] = Average2x2(blk4x4 + j*2+i*2*4, 4);

                vid16xData[x+vidW16x*y] = Average2x2(blk2x2, 2);

                for (Ipp32s i = 0; i < 2; i++)
                    for (Ipp32s j = 0; j < 2; j++)
                        if (x*2+j < vidW8x && y*2+i < vidH8x)
                            vid8xData[x*2+j+vidW8x*(y*2+i)] = blk2x2[j+2*i];
                for (Ipp32s i = 0; i < 4; i++)
                    for (Ipp32s j = 0; j < 4; j++)
                        if (x*4+j < vidW4x && y*4+i < vidH4x)
                            vid4xData[x*4+j+vidW4x*(y*4+i)] = blk4x4[j+4*i];
                for (Ipp32s i = 0; i < 8; i++)
                    for (Ipp32s j = 0; j < 8; j++)
                        if (x*8+j < vidW2x && y*8+i < vidH2x)
                            vid2xData[x*8+j+vidW2x*(y*8+i)] = blk8x8[j+8*i];
            }
        }

        return PASSED;
    }
}
