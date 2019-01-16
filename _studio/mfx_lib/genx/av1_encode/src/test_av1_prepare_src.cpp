// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "stdio.h"
#include "time.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)

#include "assert.h"
#include "memory"
#include "vector"
#include "algorithm"
#include "../include/test_common.h"


class cm_error : public std::runtime_error
{
    int m_errcode;
    int m_line;
    const char *m_file;
public:
    cm_error(int errcode, const char *file, int line, const char *message)
        : runtime_error(message), m_errcode(errcode), m_line(line), m_file(file) {}
    int errcode() const { return m_errcode; }
    int line() const { return m_line; }
    const char *file() const { return m_file; }
};

#define THROW_CM_ERR(ERR) if ((ERR) != CM_SUCCESS) { throw cm_error(ERR, __FILE__, __LINE__, nullptr); }
#define THROW_CM_ERR_MSG(ERR, MSG) if ((ERR) != CM_SUCCESS) { throw cm_error(ERR, __FILE__, __LINE__, MSG); }

#ifdef CMRT_EMU
extern "C" {
    void PrepareSrcNv12(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, uint32_t srcPaddingLu, uint32_t srcPaddingCh,
                        SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X);
};
const char genx_av1_prepare_src_hsw[1] = {};
const char genx_av1_prepare_src_bdw[1] = {};
const char genx_av1_prepare_src_skl[1] = {};
#else //CMRT_EMU
#include "../include/genx_av1_prepare_src_hsw_isa.h"
#include "../include/genx_av1_prepare_src_bdw_isa.h"
#include "../include/genx_av1_prepare_src_skl_isa.h"
#endif //CMRT_EMU

namespace {
    void RunGpu(const uint8_t *sysLumaData, const uint8_t *sysChromaData, int32_t width, int32_t height,
                uint8_t *vid1xData, uint8_t *vid2xData, uint8_t *vid4xData, uint8_t *vid8xData, uint8_t *vid16xData,
                int lambdaInt, uint8_t *pred, int32_t copyChroma);

    void RunCpu(const uint8_t *sysLumaData, const uint8_t *sysChromaData, int32_t width, int32_t height,
                uint8_t *vid1xData, uint8_t *vid2xData, uint8_t *vid4xData, uint8_t *vid8xData, uint8_t *vid16xData,
                int lambdaInt, uint8_t *pred);

    typedef std::vector<uint8_t> vec8;
    void RunGpu(const vec8 &sysLuma, const vec8 &sysChroma, int32_t width, int32_t height, vec8 &vid1x,
                vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x, int lambdaInt, vec8 &pred, int32_t copyChroma)
    {
        RunGpu(sysLuma.data(), sysChroma.data(), width, height, vid1x.data(), vid2x.data(),
               vid4x.data(), vid8x.data(), vid16x.data(), lambdaInt, pred.data(), copyChroma);
    }

    void RunCpu(const vec8 &sysLuma, const vec8 &sysChroma, int32_t width, int32_t height, vec8 &vid1x,
                vec8 &vid2x, vec8 &vid4x, vec8 &vid8x, vec8 &vid16x, int lambdaInt, vec8 &pred)
    {
        RunCpu(sysLuma.data(), sysChroma.data(), width, height, vid1x.data(), vid2x.data(),
               vid4x.data(), vid8x.data(), vid16x.data(), lambdaInt, pred.data());
    }

    int Compare(const vec8 &outGpu, const vec8 &outRef, int32_t width, int32_t height, const char *plane);
    int CompareIntraInfo(const uint32_t *outGpu, const uint32_t *outRef, int32_t width, int32_t height);
};

namespace global_test_params {
    char *yuv = "C:/yuv/1080p/park_joy_1920x1080_500_50.yuv";
    int32_t width  = 1920;
    int32_t height = 1080;
};

void parse_cmd(int argc, char **argv) {
    if (argc == 2) {
        if (_stricmp(argv[1], "--help") == 0 ||
            _stricmp(argv[1], "-h") == 0 ||
            _stricmp(argv[1], "-?") == 0)
        {

            char exe_name[256], exe_ext[256];
            _splitpath_s(argv[0], nullptr, 0, nullptr, 0, exe_name, 256, exe_ext, 256);
            printf("Usage examples:\n\n");
            printf("  1) Run with default parameters (yuv=%s width=%d height=%d)\n",
                global_test_params::yuv, global_test_params::width, global_test_params::height);
            printf("    %s%s\n\n", exe_name, exe_ext);
            printf("  2) Run with custom parameters\n");
            printf("    %s%s <yuvname> <width> <height>\n", exe_name, exe_ext);
            exit(0);
        }
    }

    if (argc == 4) {
        global_test_params::yuv    = argv[1];
        global_test_params::width  = atoi(argv[2]);
        global_test_params::height = atoi(argv[3]);
    }
}

void load_frame(FILE *f, uint8_t *luma, uint8_t *chroma, int width, int height)
{
    for (int i = 0; i < height; i++) {
        if (fread(luma + i * width, 1, width, f) != width)
            throw std::exception("failed to read frame from file");
    }

    const int chroma_size = width * height / 4;
    std::vector<uint8_t> u(chroma_size);
    std::vector<uint8_t> v(chroma_size);
    if (fread(u.data(), 1, chroma_size, f) != chroma_size)
        throw std::exception("failed to read frame from file");
    if (fread(v.data(), 1, chroma_size, f) != chroma_size)
        throw std::exception("failed to read frame from file");

    for (int i = 0; i < height / 2; i++) {
        for (int j = 0; j < width / 2; j++) {
            chroma[i * width + 2 * j + 0] = u[i * width / 2 + j];
            chroma[i * width + 2 * j + 1] = v[i * width / 2 + j];
        }
    }
}

int main(int argc, char **argv)
{
    try {
        parse_cmd(argc, argv);
        print_hw_caps();

        printf("Running with %s %dx%d\n", global_test_params::yuv,
            global_test_params::width, global_test_params::height);

        const int32_t width = global_test_params::width;
        const int32_t height = global_test_params::height;

        FILE *f = fopen(global_test_params::yuv, "rb");
        if (!f)
            throw std::runtime_error("Failed to open yuv file");

        // const per frame params
        int32_t sbCols = (width + 63) / 64;
        int32_t sbRows = (height + 63) / 64;
        int32_t outW2x = ROUNDUP(width/2, 16);
        int32_t outW4x = ROUNDUP(width/4, 16);
        int32_t outW8x = ROUNDUP(width/8, 16);
        int32_t outW16x = ROUNDUP(width/16, 16);
        int32_t outH2x = ROUNDUP(height/2, 16);
        int32_t outH4x = ROUNDUP(height/4, 16);
        int32_t outH8x = ROUNDUP(height/8, 16);
        int32_t outH16x = ROUNDUP(height/16, 16);

        // used as input
        std::vector<uint8_t> sysLuma8(width * height);
        std::vector<uint8_t> sysChromaNv12(width * height / 2);
        std::vector<uint8_t> vid1x(width * height * 3 / 2);

        // used as output
        std::vector<uint8_t> sysLuma8Gpu(width * height);
        std::vector<uint8_t> sysLuma8Ref(width * height);
        std::vector<uint8_t> sysChromaNv12Gpu(width * height / 2);
        std::vector<uint8_t> sysChromaNv12Ref(width * height / 2);
        std::vector<uint8_t> vid1xGpu(width * height * 3 / 2);
        std::vector<uint8_t> vid1xRef(width * height * 3 / 2);
        std::vector<uint8_t> vid2xGpu(outW2x * outH2x * 3 / 2);
        std::vector<uint8_t> vid2xRef(outW2x * outH2x * 3 / 2);
        std::vector<uint8_t> vid4xGpu(outW4x * outH4x * 3 / 2);
        std::vector<uint8_t> vid4xRef(outW4x * outH4x * 3 / 2);
        std::vector<uint8_t> vid8xGpu(outW8x * outH8x * 3 / 2);
        std::vector<uint8_t> vid8xRef(outW8x * outH8x * 3 / 2);
        std::vector<uint8_t> vid16xGpu(outW16x * outH16x * 3 / 2);
        std::vector<uint8_t> vid16xRef(outW16x * outH16x * 3 / 2);
        std::vector<uint8_t> predGpu(sbCols*64 * height*64);
        std::vector<uint8_t> predRef(sbCols*64 * height*64);

        srand(0x1234578/*time(0)*/);
        load_frame(f, sysLuma8.data(), sysChromaNv12.data(), width, height);

        const int lambdaInt = 10;

        RunGpu(sysLuma8, sysChromaNv12, width, height, vid1xGpu, vid2xGpu,
               vid4xGpu, vid8xGpu, vid16xGpu, lambdaInt, predGpu, 0);
        RunCpu(sysLuma8, sysChromaNv12, width, height, vid1xRef, vid2xRef,
               vid4xRef, vid8xRef, vid16xRef, lambdaInt, predRef);

        if (Compare(vid1xGpu,  vid1xRef,  width,   height,  "1x" ) != PASSED ||
            Compare(vid2xGpu,  vid2xRef,  outW2x,  outH2x,  "2x" ) != PASSED ||
            Compare(vid4xGpu,  vid4xRef,  outW4x,  outH4x,  "4x" ) != PASSED ||
            Compare(vid8xGpu,  vid8xRef,  outW8x,  outH8x,  "8x" ) != PASSED ||
            Compare(vid16xGpu, vid16xRef, outW16x, outH16x, "16x") != PASSED)
        {
            printf("FAILED\n");
            return 1;
        }
        printf("passed\n");

        RunGpu(sysLuma8, sysChromaNv12, width, height, vid1xGpu, vid2xGpu,
               vid4xGpu, vid8xGpu, vid16xGpu, lambdaInt, predGpu, 1);

        if (Compare(vid1xGpu,  vid1xRef,  width,   height*3/2, "1x" ) != PASSED ||
            Compare(vid2xGpu,  vid2xRef,  outW2x,  outH2x,     "2x" ) != PASSED ||
            Compare(vid4xGpu,  vid4xRef,  outW4x,  outH4x,     "4x" ) != PASSED ||
            Compare(vid8xGpu,  vid8xRef,  outW8x,  outH8x,     "8x" ) != PASSED ||
            Compare(vid16xGpu, vid16xRef, outW16x, outH16x,    "16x") != PASSED)
        {
            printf("FAILED\n");
            return 1;
        }
        printf("passed\n");

        return 0;

    } catch (cm_error &e) {
        printf("CM_ERROR:\n");
        printf("  code: %d\n", e.errcode());
        printf("  file: %s\n", e.file());
        printf("  line: %d\n", e.line());
        if (const char *msg = e.what())
            printf("  message: %s\n", msg);

    } catch (std::exception &e) {
        printf("ERROR: %s\n", e.what());
        return 1;
    }
}


namespace {
    int Compare(const vec8 &outGpu, const vec8 &outRef, int32_t width, int32_t height, const char *plane)
    {
        auto diff = std::mismatch(outGpu.data(), outGpu.data() + width * height, outRef.data());
        if (diff.first != outGpu.data() + width * height) {
            int32_t pos = int32_t(diff.first - outGpu.data());
            printf("\"%s\" surfaces differ at x=%d, y=%d tst=%d, ref=%d\n", plane, pos%width, pos/width, *diff.first, *diff.second);
            return FAILED;
        }
        return PASSED;
    }

    template <class T> void CopyAndPad(const T *in, int32_t inPitch, T *out, int32_t outPitch, int32_t width, int32_t height, int32_t padding)
    {
        for (int32_t y = 0; y < height; y++) {
            memcpy(out + padding, in, width * sizeof(T));
            for (int32_t x = 0; x < padding; x++) {
                out[x] = out[padding];
                out[padding + width + x] = out[padding + width - 1];
            }
            out = (T *)((uint8_t *)out + outPitch);
            in  = (T *)((uint8_t *)in + inPitch);
        }
    }

    template <typename T> SurfaceIndex *get_index(T *cm_resource) {
        SurfaceIndex *index = 0;
        int res = cm_resource->GetIndex(index);
        THROW_CM_ERR(res);
        return index;
    }


    void RunGpu(const uint8_t *sysLumaData, const uint8_t *sysChromaData, int32_t width, int32_t height,
                uint8_t *vid1xData, uint8_t *vid2xData, uint8_t *vid4xData, uint8_t *vid8xData, uint8_t *vid16xData,
                int lambdaInt, uint8_t *pred, int32_t copyChroma)
    {
        int32_t sb_cols = (width  + 63) / 64;
        int32_t sb_rows = (height + 63) / 64;

        const uint32_t PADDING_LU = 80;
        const uint32_t PADDING_CH = 80;

        int32_t outW1x = width;
        int32_t outW2x = ROUNDUP(width/2, 16);
        int32_t outW4x = ROUNDUP(width/4, 16);
        int32_t outW8x = ROUNDUP(width/8, 16);
        int32_t outW16x = ROUNDUP(width/16, 16);
        int32_t outH1x = height;
        int32_t outH2x = ROUNDUP(height/2, 16);
        int32_t outH4x = ROUNDUP(height/4, 16);
        int32_t outH8x = ROUNDUP(height/8, 16);
        int32_t outH16x = ROUNDUP(height/16, 16);

        mfxU32 version = 0;
        CmDevice *device = nullptr;
        CmProgram *program = nullptr;
        CmKernel *kernel = nullptr;
        uint8_t *sysLuma = nullptr;
        uint8_t *sysChroma = nullptr;
        CmSurface2DUP *sysLumaUp = nullptr;
        CmSurface2DUP *sysChromaUp = nullptr;
        CmSurface2D *vid1x = 0;
        CmSurface2D *vid2x = 0;
        CmSurface2D *vid4x = 0;
        CmSurface2D *vid8x = 0;
        CmSurface2D *vid16x = 0;
        CmThreadSpace * threadSpace = 0;
        CmTask * task = 0;
        CmQueue *queue = 0;
        CmEvent *e = 0;
        uint32_t sysLumaPitch = 0;
        uint32_t sysLumaSize = 0;
        uint32_t sysChromaPitch = 0;
        uint32_t sysChromaSize = 0;
        uint32_t tsWidth  = outW16x/4;
        uint32_t tsHeight = outH16x;

        THROW_CM_ERR(::CreateCmDevice(device, version));

        THROW_CM_ERR(device->InitPrintBuffer());

        GPU_PLATFORM hw_type;
        size_t size = sizeof(mfxU32);
        THROW_CM_ERR(device->GetCaps(CAP_GPU_PLATFORM, size, &hw_type));
#ifdef CMRT_EMU
        hw_type = PLATFORM_INTEL_HSW;
#endif // CMRT_EMU
        switch (hw_type) {
        case PLATFORM_INTEL_HSW:
            THROW_CM_ERR(device->LoadProgram((void *)genx_av1_prepare_src_hsw, sizeof(genx_av1_prepare_src_hsw), program, "nojitter"));
            break;
        case PLATFORM_INTEL_BDW:
        case PLATFORM_INTEL_CHV:
            THROW_CM_ERR(device->LoadProgram((void *)genx_av1_prepare_src_bdw, sizeof(genx_av1_prepare_src_bdw), program, "nojitter"));
            break;
        case PLATFORM_INTEL_SKL:
            THROW_CM_ERR(device->LoadProgram((void *)genx_av1_prepare_src_skl, sizeof(genx_av1_prepare_src_skl), program, "nojitter"));
            break;
        default:
            throw cm_error(CM_FAILURE, __FILE__, __LINE__, "Unknown HW type");
        }

        THROW_CM_ERR(device->CreateKernel(program, CM_KERNEL_FUNCTION(PrepareSrcNv12), kernel));

        THROW_CM_ERR(device->GetSurface2DInfo(width + 2*PADDING_LU, height, CM_SURFACE_FORMAT_P8, sysLumaPitch, sysLumaSize));
        sysLuma = (uint8_t *)CM_ALIGNED_MALLOC(sysLumaSize, 0x1000);
        THROW_CM_ERR(device->CreateSurface2DUP(width + 2*PADDING_LU, height, CM_SURFACE_FORMAT_P8, sysLuma, sysLumaUp));

        THROW_CM_ERR(device->GetSurface2DInfo(width + 2*PADDING_CH, height, CM_SURFACE_FORMAT_P8, sysChromaPitch, sysChromaSize));
        sysChroma = (uint8_t *)CM_ALIGNED_MALLOC(sysChromaSize, 0x1000);
        THROW_CM_ERR(device->CreateSurface2DUP(width + 2*PADDING_CH, height/2, CM_SURFACE_FORMAT_P8, sysChroma, sysChromaUp));

        CopyAndPad(sysLumaData,   width, (uint8_t *)sysLuma,   sysLumaPitch,   width, height,   PADDING_LU);
        CopyAndPad(sysChromaData, width, (uint8_t *)sysChroma, sysChromaPitch, width, height/2, PADDING_CH);

        THROW_CM_ERR(device->CreateSurface2D(width,   height,  CM_SURFACE_FORMAT_NV12, vid1x));
        THROW_CM_ERR(device->CreateSurface2D(outW2x,  outH2x,  CM_SURFACE_FORMAT_NV12, vid2x));
        THROW_CM_ERR(device->CreateSurface2D(outW4x,  outH4x,  CM_SURFACE_FORMAT_NV12, vid4x));
        THROW_CM_ERR(device->CreateSurface2D(outW8x,  outH8x,  CM_SURFACE_FORMAT_NV12, vid8x));
        THROW_CM_ERR(device->CreateSurface2D(outW16x, outH16x, CM_SURFACE_FORMAT_NV12, vid16x));

        uint32_t argIdx = 0;
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(sysLumaUp)));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(sysChromaUp)));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(vid1x)));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(vid2x)));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(vid4x)));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(vid8x)));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(vid16x)));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(uint32_t), &PADDING_LU));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(uint32_t), &PADDING_CH));
        THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(uint32_t), &copyChroma));

        THROW_CM_ERR(kernel->SetThreadCount(tsWidth * tsHeight));
        THROW_CM_ERR(device->CreateThreadSpace(tsWidth, tsHeight, threadSpace));
        THROW_CM_ERR(threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY));
        THROW_CM_ERR(device->CreateTask(task));
        THROW_CM_ERR(task->AddKernel(kernel));
        THROW_CM_ERR(device->CreateQueue(queue));
        THROW_CM_ERR(queue->Enqueue(task, e, threadSpace));
        THROW_CM_ERR(e->WaitForTaskFinished());
        THROW_CM_ERR(device->FlushPrintBuffer());

        THROW_CM_ERR(vid1x->ReadSurfaceStride(vid1xData, NULL, width));
        THROW_CM_ERR(vid2x->ReadSurfaceStride(vid2xData, NULL, outW2x));
        THROW_CM_ERR(vid4x->ReadSurfaceStride(vid4xData, NULL, outW4x));
        THROW_CM_ERR(vid8x->ReadSurfaceStride(vid8xData, NULL, outW8x));
        THROW_CM_ERR(vid16x->ReadSurfaceStride(vid16xData, NULL, outW16x));

#ifndef CMRT_EMU
        printf("CopyChroma=%d TIME=%.3fms ", copyChroma, GetAccurateGpuTime(queue, task, threadSpace)/1000000.);
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
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);
    }

    void RunCpu(const uint8_t *sysLumaData, const uint8_t *sysChromaData, int32_t width, int32_t height,
                uint8_t *vid1xData, uint8_t *vid2xData, uint8_t *vid4xData, uint8_t *vid8xData, uint8_t *vid16xData,
                int lambdaInt, uint8_t *out_pred)
    {
        memcpy(vid1xData, sysLumaData, width * height);
        memcpy(vid1xData + width * height, sysChromaData, width * height/2);

        int32_t vidW2x = ROUNDUP(width/2, 16);
        int32_t vidW4x = ROUNDUP(width/4, 16);
        int32_t vidW8x = ROUNDUP(width/8, 16);
        int32_t vidW16x = ROUNDUP(width/16, 16);
        int32_t vidH2x = ROUNDUP(height/2, 16);
        int32_t vidH4x = ROUNDUP(height/4, 16);
        int32_t vidH8x = ROUNDUP(height/8, 16);
        int32_t vidH16x = ROUNDUP(height/16, 16);

        memset(vid2xData  +  vidW2x *  vidH2x, 0,  vidW2x *  vidH2x / 2);
        memset(vid4xData  +  vidW4x *  vidH4x, 0,  vidW4x *  vidH4x / 2);
        memset(vid8xData  +  vidW8x *  vidH8x, 0,  vidW8x *  vidH8x / 2);
        memset(vid16xData + vidW16x * vidH16x, 0, vidW16x * vidH16x / 2);

        auto Average2x2 = [](const mfxU8 *p, int32_t pitch) { return mfxU8((p[0] + p[1] + p[pitch] + p[pitch + 1] + 2) >> 2); };

        uint8_t blk16x16[16*16];
        uint8_t blk8x8[8*8];
        uint8_t blk4x4[4*4];
        uint8_t blk2x2[2*2];
        for (int32_t y = 0; y < vidH16x; y++) {
            for (int32_t x = 0; x < vidW16x; x++) {
                for (int32_t i = 0; i < 16; i++)
                    for (int32_t j = 0; j < 16; j++)
                        blk16x16[j+16*i] = vid1xData[MIN(width-1, x*16+j) + width * MIN(height-1, y*16+i)];
                for (int32_t i = 0; i < 8; i++)
                    for (int32_t j = 0; j < 8; j++)
                        blk8x8[j+8*i] = Average2x2(blk16x16 + j*2+i*2*16, 16);
                for (int32_t i = 0; i < 4; i++)
                    for (int32_t j = 0; j < 4; j++)
                        blk4x4[j+4*i] = Average2x2(blk8x8 + j*2+i*2*8, 8);
                for (int32_t i = 0; i < 2; i++)
                    for (int32_t j = 0; j < 2; j++)
                        blk2x2[j+2*i] = Average2x2(blk4x4 + j*2+i*2*4, 4);

                vid16xData[x+vidW16x*y] = Average2x2(blk2x2, 2);

                for (int32_t i = 0; i < 2; i++)
                    for (int32_t j = 0; j < 2; j++)
                        if (x*2+j < vidW8x && y*2+i < vidH8x)
                            vid8xData[x*2+j+vidW8x*(y*2+i)] = blk2x2[j+2*i];
                for (int32_t i = 0; i < 4; i++)
                    for (int32_t j = 0; j < 4; j++)
                        if (x*4+j < vidW4x && y*4+i < vidH4x)
                            vid4xData[x*4+j+vidW4x*(y*4+i)] = blk4x4[j+4*i];
                for (int32_t i = 0; i < 8; i++)
                    for (int32_t j = 0; j < 8; j++)
                        if (x*8+j < vidW2x && y*8+i < vidH2x)
                            vid2xData[x*8+j+vidW2x*(y*8+i)] = blk8x8[j+8*i];
            }
        }
    }
}
