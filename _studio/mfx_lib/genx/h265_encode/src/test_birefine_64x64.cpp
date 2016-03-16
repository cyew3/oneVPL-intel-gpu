/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#pragma warning(disable : 4505)
#include "stdio.h"
#pragma warning(push)
#include "cm_rt.h"
#include "cm/cm_def.h"
#include "cm/cm_vm.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_hevce_birefine_hsw_isa.h"
#include "../include/genx_hevce_interpolate_frame_hsw_isa.h"

#ifdef CMRT_EMU
extern "C" {
void InterpolateFrameWithBorderMerge(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPELS);
void BiRefine64x64(SurfaceIndex SURF_DST, SurfaceIndex SURF_SRC,
                   vector<SurfaceIndex, 1> SURF_REF0, vector<SurfaceIndex, 1> SURF_REF1,
                   SurfaceIndex SURF_DATA0, SurfaceIndex SURF_DATA1);
}
#endif //CMRT_EMU

#define REFKERNEL_NAME BiRefine64x64
#define INTERPOLATE_KERNEL_NAME InterpolateFrameWithBorderMerge

struct OutputMv
{
    mfxI16Pair mv[2];
};

struct MvSad32Data  // sizeof(MvSad32Data)= 16
{
    mfxI16Pair mv;
    mfxU32 sad[9];
    mfxU32 mbz[6];
};

namespace {
int RunGpu(const mfxU8 *src, const mfxU8 *ref0, const mfxU8 *ref1,
           const MvSad32Data *data0, const MvSad32Data *data1, OutputMv *dst, mfxU32 blkW, mfxU32 blkH);
int RunCpu(const mfxU8 *src, const mfxU8 *ref0, const mfxU8 *ref1,
           const MvSad32Data *data0, const MvSad32Data *data1, OutputMv *dst, mfxU32 blkW, mfxU32 blkH);
}

int TestBiRefine64x64()
{
    enum
    {
        BLOCK_W = 64,
        BLOCK_H = 64
    };

    mfxI32 numBlocksHor = (WIDTH + BLOCK_W - 1) / BLOCK_W;
    mfxI32 numBlocksVer = (HEIGHT + BLOCK_H - 1) / BLOCK_H;
    OutputMv *outputGpu = new OutputMv[numBlocksHor * numBlocksVer];
    OutputMv *outputCpu = new OutputMv[numBlocksHor * numBlocksVer];
    mfxU8  *src = new mfxU8[numBlocksHor * numBlocksVer * BLOCK_W * BLOCK_H * 3 / 2]; // format is NV12!!!
    mfxU8  *ref0 = new mfxU8[numBlocksHor * numBlocksVer * BLOCK_W * BLOCK_H * 3 / 2];
    mfxU8  *ref1 = new mfxU8[numBlocksHor * numBlocksVer * BLOCK_W * BLOCK_H * 3 / 2];
    MvSad32Data *data0 = new MvSad32Data[numBlocksHor * numBlocksVer];  // 9 distortions per 64x64 block (mem for 16 ones like in kernel)
    MvSad32Data *data1 = new MvSad32Data[numBlocksHor * numBlocksVer];
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return FAILED;
    if (fread(ref0, 1, WIDTH * HEIGHT * 3 / 2, f) != WIDTH * HEIGHT * 3 / 2)
        return FAILED;
    if (fread(ref1, 1, WIDTH * HEIGHT * 3 / 2, f) != WIDTH * HEIGHT * 3 / 2)
        return FAILED;
    if (fread(src, 1, WIDTH * HEIGHT * 3 / 2, f) != WIDTH * HEIGHT * 3 / 2)
        return FAILED;
    fclose(f);

    // fill motion vector field
    for (mfxI32 y = 0; y < numBlocksVer; y++) {
        for (mfxI32 x = 0; x < numBlocksHor; x++) {
            mfxI16Pair mv0, mv1;
            mv0.x = (mfxI16)(rand() % ((WIDTH  + 20) * 4)) & ~3;  // ipel MVs for 64x64 blocks
            mv0.y = (mfxI16)(rand() % ((HEIGHT + 20) * 4)) & ~3;
            mv1.x = (mfxI16)(rand() % ((WIDTH  + 20) * 4)) & ~3;
            mv1.y = (mfxI16)(rand() % ((HEIGHT + 20) * 4)) & ~3;

            mv0.x &= 0x1FFF;
            mv0.y &= 0x1FFF;
            mv1.x &= 0x1FFF;
            mv1.y &= 0x1FFF;

            data0[y * numBlocksHor + x].mv = mv0;
            data1[y * numBlocksHor + x].mv = mv1;
            for (int i = 0; i < 9; i++) {
                data0[y * numBlocksHor + x].sad[i] = rand();
                data1[y * numBlocksHor + x].sad[i] = rand();
            }
        }
    }

    // check all MVs to be supported by HW [-2048; 2047]
    for (mfxI32 y = 0; y < numBlocksVer; y++) {
        for (mfxI32 x = 0; x < numBlocksHor; x++) {
            if ((data0[y * numBlocksHor + x].mv.x < -8192) || (data0[y * numBlocksHor + x].mv.x > 8191) ||
                (data1[y * numBlocksHor + x].mv.x < -8192) || (data1[y * numBlocksHor + x].mv.x > 8191)) {
                printf("\ngenerated MV is out of [-8192; 8192] supported by HW!!!\n");
                printf("mv0 (%i, %i) = %i , %i\n", x, y, data0[y * numBlocksHor + x].mv.x, data0[y * numBlocksHor + x].mv.y);
                printf("mv1 (%i, %i) = %i , %i\n", x, y, data1[y * numBlocksHor + x].mv.x, data1[y * numBlocksHor + x].mv.y);
                return FAILED;
            }
        }
    }
    
    res = RunGpu(src, ref0, ref1, data0, data1, outputGpu, BLOCK_W, BLOCK_H);
    CHECK_ERR(res);

    res = RunCpu(src, ref0, ref1, data0, data1, outputCpu, BLOCK_W, BLOCK_H);
    CHECK_ERR(res);

    for (mfxI32 y = 0; y < numBlocksVer; y++) {
        for (mfxI32 x = 0; x < numBlocksHor; x++) {
            mfxI32 ind = y * numBlocksHor + x;
            mfxI16Pair mvG0 =  outputGpu[ind].mv[0];
            mfxI16Pair mvC0 =  outputCpu[ind].mv[0];
            mfxI16Pair mvG1 =  outputGpu[ind].mv[1];
            mfxI16Pair mvC1 =  outputCpu[ind].mv[1];

            if ((mvG0.x != mvC0.x) || (mvG0.y != mvC0.y) ||
                (mvG1.x != mvC1.x) || (mvG1.y != mvC1.y)){
                printf("\nmv differ for block (%d,%d) SURF\nGPU0 %d, %d\nCPU0 %d, %d\nGPU1 %d, %d\nCPU1 %d, %d\n",
                    x, y, mvG0.x, mvG0.y, mvC0.x, mvC0.y, mvG1.x, mvG1.y, mvC1.x, mvC1.y);
                printf("mv0 (%i, %i) = %i , %i\n", x, y, data0[y * numBlocksHor + x].mv.x, data0[y * numBlocksHor + x].mv.y);
                printf("mv1 (%i, %i) = %i , %i\n", x, y, data1[y * numBlocksHor + x].mv.x, data1[y * numBlocksHor + x].mv.y);
                return FAILED;
            }
        }
    }

    delete [] outputCpu;
    delete [] src;
    delete [] ref0;
    delete [] ref1;
    delete [] data0;
    delete [] data1;

    return PASSED;
}

namespace {

int RunGpu(const mfxU8 *src, const mfxU8 *ref0, const mfxU8 *ref1,
           const MvSad32Data *data0, const MvSad32Data *data1, OutputMv *outData,
           mfxU32 blkW, mfxU32 blkH)
{
    mfxI32 numBlocksHor = (WIDTH + blkW - 1) / blkW;
    mfxI32 numBlocksVer = (HEIGHT + blkH - 1) / blkH;

    mfxU32 version = 0;
    CmDevice *device = 0;
    mfxI32 res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program_interp = 0;
    res = device->LoadProgram((void *)genx_hevce_interpolate_frame_hsw, sizeof(genx_hevce_interpolate_frame_hsw), program_interp);
    CHECK_CM_ERR(res);

    CmKernel *kernelMergeHpel = 0;
    res = device->CreateKernel(program_interp, CM_KERNEL_FUNCTION(INTERPOLATE_KERNEL_NAME), kernelMergeHpel);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_birefine_hsw, sizeof(genx_hevce_birefine_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(REFKERNEL_NAME), kernel);
    CHECK_CM_ERR(res);

    CmSurface2D *inSrc = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_NV12, inSrc);
    CHECK_CM_ERR(res);
    res = inSrc->WriteSurface(src, NULL);
    CHECK_CM_ERR(res);

    // refined MVs
    mfxU32 outputWidth = numBlocksHor * sizeof(OutputMv);
    mfxU32 outputPitch = 0;
    mfxU32 outputSize = 0;
    res = device->GetSurface2DInfo(outputWidth, numBlocksVer, CM_SURFACE_FORMAT_P8, outputPitch, outputSize);
    CHECK_CM_ERR(res);
    mfxU8 *outputSys = (mfxU8 *)CM_ALIGNED_MALLOC(outputSize, 0x1000);
    CmSurface2DUP *outputCm = 0;
    res = device->CreateSurface2DUP(outputWidth, numBlocksVer, CM_SURFACE_FORMAT_P8, outputSys, outputCm);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef0 = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_NV12, inRef0);
    CHECK_CM_ERR(res);
    res = inRef0->WriteSurface(ref0, NULL);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef1 = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_NV12, inRef1);
    CHECK_CM_ERR(res);
    res = inRef1->WriteSurface(ref1, NULL);
    CHECK_CM_ERR(res);
    
    CmSurface2D *refHalf0 = 0;
    res = device->CreateSurface2D(WIDTHB * 2, HEIGHTB * 2, CM_SURFACE_FORMAT_P8, refHalf0);
    CHECK_CM_ERR(res);
    
    CmSurface2D *refHalf1 = 0;
    res = device->CreateSurface2D(WIDTHB * 2, HEIGHTB * 2, CM_SURFACE_FORMAT_P8, refHalf1);
    CHECK_CM_ERR(res);

    CmSurface2D *inData0 = 0;  // 9 distortions per block (mem for 16 ones)
    res = device->CreateSurface2D(numBlocksHor * sizeof(MvSad32Data), numBlocksVer, CM_SURFACE_FORMAT_P8, inData0);
    CHECK_CM_ERR(res);
    res = inData0->WriteSurface((const mfxU8 *)data0, NULL);
    CHECK_CM_ERR(res);

    CmSurface2D *inData1 = 0;  // 9 distortions per block (mem for 16 ones)
    res = device->CreateSurface2D(numBlocksHor * sizeof(MvSad32Data), numBlocksVer, CM_SURFACE_FORMAT_P8, inData1);
    CHECK_CM_ERR(res);
    res = inData1->WriteSurface((const mfxU8 *)data1, NULL);
    CHECK_CM_ERR(res);
    
    SurfaceIndex *idxInSrc = 0;
    res = inSrc->GetIndex(idxInSrc);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInDst = 0;
    res = outputCm->GetIndex(idxInDst);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInRef0 = 0;
    res = inRef0->GetIndex(idxInRef0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInRef1 = 0;
    res = inRef1->GetIndex(idxInRef1);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxRefHalf0 = 0;
    res = refHalf0->GetIndex(idxRefHalf0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxRefHalf1 = 0;
    res = refHalf1->GetIndex(idxRefHalf1);
    CHECK_CM_ERR(res);
    
    SurfaceIndex *idxInData0 = 0;
    res = inData0->GetIndex(idxInData0);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInData1 = 0;
    res = inData1->GetIndex(idxInData1);
    CHECK_CM_ERR(res);
    
    // interpolation is done by 8x8
    const mfxU16 BlockW = 8;
    const mfxU16 BlockH = 8;
    mfxU32 tsWidth = (WIDTHB + BlockW - 1) / BlockW;
    mfxU32 tsHeight = (HEIGHTB + BlockH - 1) / BlockH;

    // merge Halfs0  Halfs1  

    res = kernelMergeHpel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);
    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);

    res = kernelMergeHpel->SetKernelArg(0, sizeof(SurfaceIndex), idxInRef0);
    CHECK_CM_ERR(res);
    res = kernelMergeHpel->SetKernelArg(1, sizeof(SurfaceIndex), idxRefHalf0);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernelMergeHpel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    CmEvent * e = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);
#if !defined CMRT_EMU
    for (int i=0; i<500; i++)
#endif
        res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    device->DestroyTask(task);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    mfxU64 time;
    e->GetExecutionTime(time);
    printf("TIME Interp Merge Ref0 = %.3f ms ", time / 1000000.0);
    queue->DestroyEvent(e);

    res = kernelMergeHpel->SetKernelArg(0, sizeof(SurfaceIndex), idxInRef1);
    CHECK_CM_ERR(res);
    res = kernelMergeHpel->SetKernelArg(1, sizeof(SurfaceIndex), idxRefHalf1);
    CHECK_CM_ERR(res);

    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernelMergeHpel);
    CHECK_CM_ERR(res);
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);
#if !defined CMRT_EMU
    for (int i=0; i<500; i++)
#endif
        res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);
    e->GetExecutionTime(time);
    printf("TIME Interp Merge Ref1 = %.3f ms ", time / 1000000.0);
    queue->DestroyEvent(e);

    res = kernel->SetThreadCount(numBlocksHor * numBlocksVer);    // processes one 64x64 block
    CHECK_CM_ERR(res);

    res = device->CreateThreadSpace(numBlocksHor, numBlocksVer, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);

    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInDst), idxInDst);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxInSrc), idxInSrc);
    CHECK_CM_ERR(res);
    vector<SurfaceIndex, 1> surfsRef0;
    surfsRef0[0] = *idxRefHalf0;
    res = kernel->SetKernelArg(2, surfsRef0.get_size_data(), surfsRef0.get_addr_data());
    CHECK_CM_ERR(res);
    vector<SurfaceIndex, 1> surfsRef1;
    surfsRef1[0] = *idxRefHalf1;
    res = kernel->SetKernelArg(3, surfsRef1.get_size_data(), surfsRef1.get_addr_data());
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(4, sizeof(*idxInData0), idxInData0);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(5, sizeof(*idxInData1), idxInData1);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

#if !defined CMRT_EMU
    for (int i=0; i<500; i++)
#endif
        res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    e->GetExecutionTime(time);
    printf("TIME=%.3f ms ", time / 1000000.0);

    for (mfxI32 y = 0; y < numBlocksVer; y++)
        memcpy(outData + y * numBlocksHor, outputSys + y * outputPitch, outputWidth);

    queue->DestroyEvent(e);
    device->DestroySurface2DUP(outputCm);
    CM_ALIGNED_FREE(outputSys);
    device->DestroySurface(inData0);
    device->DestroySurface(inData1);
    device->DestroySurface(inRef0);
    device->DestroySurface(inRef1);
    device->DestroySurface(refHalf0);
    device->DestroySurface(refHalf1);
    device->DestroySurface(inSrc);
    device->DestroyKernel(kernelMergeHpel);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}

template <class T>
T GetPel(const T *frame, mfxU32 pitch, mfxI32 w, mfxI32 h, mfxI32 x, mfxI32 y)
{
    x = CLIPVAL(x, 0, w - 1);
    y = CLIPVAL(y, 0, h - 1);
    return frame[y * pitch + x];
}

template <class T>
mfxI16 InterpolateHorQPel(const T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty, mfxI16 m1, mfxI16 m2)
{
    return -GetPel(p, pitch, w, h, intx - 1, inty) + m1 * GetPel(p, pitch, w, h, intx + 0, inty)
           -GetPel(p, pitch, w, h, intx + 2, inty) + m2 * GetPel(p, pitch, w, h, intx + 1, inty);
}

template <class T>
mfxI16 InterpolateVerQPel(T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty, mfxI16 m1, mfxI16 m2)
{
    return -GetPel(p, pitch, w, h, intx, inty - 1) + m1 * GetPel(p, pitch, w, h, intx, inty + 0)
           -GetPel(p, pitch, w, h, intx, inty + 2) + m2 * GetPel(p, pitch, w, h, intx, inty + 1);
}

template <class T>
mfxI16 InterpolateHor(const T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty)
{
    return -GetPel(p, pitch, w, h, intx - 1, inty) + 5 * GetPel(p, pitch, w, h, intx + 0, inty)
           -GetPel(p, pitch, w, h, intx + 2, inty) + 5 * GetPel(p, pitch, w, h, intx + 1, inty);
}

template <class T>
mfxI16 InterpolateVer(T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty)
{
    return -GetPel(p, pitch, w, h, intx, inty - 1) + 5 * GetPel(p, pitch, w, h, intx, inty + 0)
           -GetPel(p, pitch, w, h, intx, inty + 2) + 5 * GetPel(p, pitch, w, h, intx, inty + 1);
}

mfxU8 ShiftAndSat(mfxI16 val, int shift)
{
    short offset = 1 << (shift - 1);
//    short offset = 0;
    return (mfxU8)CLIPVAL((val + offset) >> shift, 0, 255);
}

mfxU8 InterpolatePel(mfxU8 const * p, mfxI32 x, mfxI32 y)
{
    int intx = x >> 2;
    int inty = y >> 2;
    int dx = x & 3;
    int dy = y & 3;
    const int W = WIDTH;
    const int H = HEIGHT;
    short val = 0;

    if (dy == 0 && dx == 0) {
        val = GetPel(p, W, W, H, intx, inty);

    } else if (dy == 0) {
        val = InterpolateHor(p, W, W, H, intx, inty);
        val = ShiftAndSat(val, 3);
        if (dx == 1)
            val = ShiftAndSat(GetPel(p, W, W, H, intx + 0, inty) + val, 1);
        else if (dx == 3)
            val = ShiftAndSat(GetPel(p, W, W, H, intx + 1, inty) + val, 1);

    } else if (dx == 0) {
        val = InterpolateVer(p, W, W, H, intx, inty);
        val = ShiftAndSat(val, 3);
        if (dy == 1)
            val = ShiftAndSat(GetPel(p, W, W, H, intx, inty + 0) + val, 1);
        else if (dy == 3)
            val = ShiftAndSat(GetPel(p, W, W, H, intx, inty + 1) + val, 1);

    } else if (dx == 2) {
        short tmp[4] = {
            InterpolateHor(p, W, W, H, intx, inty - 1),
            InterpolateHor(p, W, W, H, intx, inty + 0),
            InterpolateHor(p, W, W, H, intx, inty + 1),
            InterpolateHor(p, W, W, H, intx, inty + 2)
        };
        val = InterpolateVer(tmp, 1, 1, 4, 0, 1);
        val = ShiftAndSat(val, 6);

        if (dy == 1)
            val = ShiftAndSat(ShiftAndSat(tmp[1], 3) + val, 1);
        else if (dy == 3)
            val = ShiftAndSat(ShiftAndSat(tmp[2], 3) + val, 1);

    } else if (dy == 2) {
        short tmp[4] = {
            InterpolateVer(p, W, W, H, intx - 1, inty),
            InterpolateVer(p, W, W, H, intx + 0, inty),
            InterpolateVer(p, W, W, H, intx + 1, inty),
            InterpolateVer(p, W, W, H, intx + 2, inty)
        };
        val = InterpolateHor(tmp, 0, 4, 1, 1, 0);
        val = ShiftAndSat(val, 6);
        if (dx == 1)
            val = ShiftAndSat(ShiftAndSat(tmp[1], 3) + val, 1);
        else if (dx == 3)
            val = ShiftAndSat(ShiftAndSat(tmp[2], 3) + val, 1);
    }
    else {
/*  //filter [-1,13,5,-1]
        short hor, ver;

        if (dx == 1)
            hor = InterpolateHorQPel(p, W, W, H, intx, inty + (dy >> 1), 13, 5);
        else if (dx == 3)
            hor = InterpolateHorQPel(p, W, W, H, intx, inty + (dy >> 1), 5, 13);

        if (dy == 1)
            ver = InterpolateVerQPel(p, W, W, H, intx + (dx >> 1), inty, 13, 5);
        else if (dy == 3)
            ver = InterpolateVerQPel(p, W, W, H, intx + (dx >> 1), inty, 5, 13);

        val = ShiftAndSat(ShiftAndSat(hor, 4) + ShiftAndSat(ver, 4), 1);
*/
#if 0
        short hor = InterpolateHor(p, W, W, H, intx, inty + (dy >> 1));
        short ver = InterpolateVer(p, W, W, H, intx + (dx >> 1), inty);
        val = ShiftAndSat(ShiftAndSat(hor, 3) + ShiftAndSat(ver, 3), 1);
#else
        short tl = InterpolatePel(p, x - 1, y - 1);
        short tr = InterpolatePel(p, x + 1, y - 1);
        short bl = InterpolatePel(p, x - 1, y + 1);
        short br = InterpolatePel(p, x + 1, y + 1);
        val = ShiftAndSat((tl + tr + bl + br), 2);
#endif
    }

    return (mfxU8)val;
}


void InterpolateBlock(const mfxU8 *ref, mfxI16Pair point, mfxI32 blockW, mfxI32 blockH, mfxU8 *block)
{
    for (mfxI32 y = 0; y < blockH; y++, block += blockW)
        for (mfxI32 x = 0; x < blockW; x++)
            block[x] = InterpolatePel(ref, point.x + x * 4, point.y + y * 4);
}

mfxU32 CalcSad(const mfxU8 *blockSrc, const mfxU8 *blockRef, mfxI32 blockW, mfxI32 blockH)
{
    mfxU32 sad = 0;
    for (mfxI32 y = 0; y < blockH; y++, blockSrc += blockW, blockRef += blockW)
        for (mfxI32 x = 0; x < blockW; x++)
            sad += abs(blockSrc[x] - blockRef[x]);
    return sad;
}

void CopyBlock(const mfxU8 *src, mfxI32 x, mfxI32 y, mfxI32 blockW, mfxI32 blockH, mfxU8 *block)
{
    for (mfxI32 yy = 0; yy < blockH; yy++, block += blockW)
        for (mfxI32 xx = 0; xx < blockW; xx++)
            block[xx] = GetPel(src, WIDTH, WIDTH, HEIGHT, x + xx, y + yy); 
}

void SaveBlock(const mfxU8 *block, mfxI32 x, mfxI32 y, mfxI32 blockW, mfxI32 blockH, mfxU8 *dst)
{
    for (mfxI32 yy = 0; yy < blockH; yy++, block += blockW)
        for (mfxI32 xx = 0; xx < blockW; xx++)
            dst[(y + yy) * WIDTH + (x + xx)] = block[xx];
}

void UpdateSrcBlock(mfxU8 *dst, mfxU8 *src, mfxU8 *ref, mfxI32 blockW, mfxI32 blockH)
{
    for (mfxI32 i = 0; i < blockW * blockH; i++) {
        mfxI32 val = (mfxI32)src[i] + src[i] - ref[i]; 
        dst[i] = (mfxU8)CLIPVAL(val, 0, 255);
    }
}

//void print_matrix(mfxU8 *ptr, mfxI32 w, mfxI32 h, mfxI32 pitch)
//{
//    printf("\n");
//    for (int y = 0; y < h; y++) {
//        mfxU8 *pline = ptr + y * pitch;
//        for (int x = 0; x < w; x++) {
//            printf("%4i", pline[x]);
//        }
//        printf("\n");
//    }
//    fflush(stdout);
//}

int RunCpu(const mfxU8 *src, const mfxU8 *ref0, const mfxU8 *ref1,
           const MvSad32Data *data0, const MvSad32Data *data1,
           OutputMv *outData, mfxU32 blkW, mfxU32 blkH)
{
    mfxI32 numBlocksHor = (WIDTH + blkW - 1) / blkW;
    mfxI32 numBlocksVer = (HEIGHT + blkH - 1) / blkH;
    mfxU8 *blockSrc = new mfxU8[blkW * blkH];
    mfxU8 *blockSrc1 = new mfxU8[blkW * blkH];
    mfxU8 *blockRefFix = new mfxU8[blkW * blkH];
    mfxU8 *blockRefTry = new mfxU8[blkW * blkH];
    mfxI16Pair mvRef;
    mfxI32 lfix, ltry;
    
    for (mfxI32 yBlk = 0; yBlk < numBlocksVer; yBlk++, outData += numBlocksHor) {
        for (mfxI32 xBlk = 0; xBlk < numBlocksHor; xBlk++) {
            mfxI32 ind = yBlk * numBlocksHor + xBlk;
            const mfxU8 *reffix, *reftry;
            mfxI16Pair mvfix, mvtry;
            mfxU32 min0 = data0[ind].sad[4], min1 = data1[ind].sad[4];
            mfxI16Pair mv0 = data0[ind].mv, mv1 = data1[ind].mv; // ipels for 64x64

            for (mfxI16 y = -1; y <= 1; y++) {
                for (mfxI16 x = -1; x <= 1; x++) {
                    if ((y == 0) && (x == 0))
                        continue;
                    int n = (y * 3 + 3) + (x + 1);
                    if (data0[ind].sad[n] < min0) {
                        min0 = data0[ind].sad[n];
                        mv0.x = data0[ind].mv.x + x * 2;
                        mv0.y = data0[ind].mv.y + y * 2;
                    }
                    if (data1[ind].sad[n] < min1) {
                        min1 = data1[ind].sad[n];
                        mv1.x = data1[ind].mv.x + x * 2;
                        mv1.y = data1[ind].mv.y + y * 2;
                    }
                }
            }

            if (min0 <= min1) {
                reffix = ref0;
                mvfix = mv0;
                lfix = 0;
                reftry = ref1;
                mvtry = mv1;
                ltry = 1;
            } else {
                reffix = ref1;
                mvfix = mv1;
                lfix = 1;
                reftry = ref0;
                mvtry = mv0;
                ltry = 0;
            }

            CopyBlock(src, xBlk * blkW, yBlk * blkH, blkW, blkH, blockSrc);

            mvRef.x = mvfix.x + (mfxI16)(xBlk * blkW) * 4;
            mvRef.y = mvfix.y + (mfxI16)(yBlk * blkH) * 4;
            InterpolateBlock(reffix, mvRef, blkW, blkH, blockRefFix);

            UpdateSrcBlock(blockSrc1, blockSrc, blockRefFix, blkW, blkH);

            // move to the nearest hpel
            mvtry.x &= ~1;
            mvtry.y &= ~1;

            mfxI16Pair refMv = {mvtry.x + (mfxI16)(xBlk * blkW * 4), mvtry.y + (mfxI16)(yBlk * blkH * 4)};
            InterpolateBlock(reftry, refMv, blkW, blkH, blockRefTry);   // central hpel first

            mfxU32 sadbest = CalcSad(blockSrc1, blockRefTry, blkW, blkH);
            sadbest >>= 1;

            // choose best qpel
            mfxI16Pair qmvbest = mvtry;
            for (mfxI16 yQPel = -2; yQPel <= 3; yQPel++) {
                for (mfxI16 xQPel = -2; xQPel <= 3; xQPel++) {
                    if ((yQPel == 0) && (xQPel == 0))
                        continue;
                    if (((yQPel == -2) && (xQPel == -2)) ||
                        ((yQPel == -2) && (xQPel == -1)) ||
                        ((yQPel == -2) && (xQPel ==  2)) ||
                        ((yQPel == -2) && (xQPel ==  3)) ||
                        ((yQPel == -1) && (xQPel == -2)) ||
                        ((yQPel == -1) && (xQPel ==  3)) ||
                        ((yQPel ==  2) && (xQPel == -2)) ||
                        ((yQPel ==  2) && (xQPel ==  3)) ||
                        ((yQPel ==  3) && (xQPel == -2)) ||
                        ((yQPel ==  3) && (xQPel == -1)) ||
                        ((yQPel ==  3) && (xQPel == 2)) ||
                        ((yQPel ==  3) && (xQPel == 3)))
                        continue;

                    mfxI16Pair qpeltry = {mvtry.x + xQPel, mvtry.y + yQPel};
                    refMv.x = qpeltry.x + (mfxI16)(xBlk * blkW * 4);
                    refMv.y = qpeltry.y + (mfxI16)(yBlk * blkH * 4);
                    InterpolateBlock(reftry, refMv, blkW, blkH, blockRefTry);

                    mfxU32 sad = CalcSad(blockSrc1, blockRefTry, blkW, blkH);
                    sad >>= 1;
                    if (sad < sadbest) {
                        qmvbest = qpeltry;
                        sadbest = sad;
                    }
                }
            }

            outData[xBlk].mv[ltry] = qmvbest;
            
            // refine 2nd MV
            mvtry = mvfix;
            mvfix = qmvbest;

            mvRef.x = mvfix.x + (mfxI16)(xBlk * blkW) * 4;
            mvRef.y = mvfix.y + (mfxI16)(yBlk * blkH) * 4;
            InterpolateBlock(reftry, mvRef, blkW, blkH, blockRefFix);
            UpdateSrcBlock(blockSrc1, blockSrc, blockRefFix, blkW, blkH);

            // move to the nearest hpel
            mvtry.x &= ~1;
            mvtry.y &= ~1;

            refMv.x = mvtry.x + (mfxI16)(xBlk * blkW * 4);
            refMv.y = mvtry.y + (mfxI16)(yBlk * blkH * 4);
            InterpolateBlock(reffix, refMv, blkW, blkH, blockRefTry);   // central pel first
            sadbest = CalcSad(blockSrc1, blockRefTry, blkW, blkH);
            sadbest >>= 1;

            qmvbest = mvtry;
            for (mfxI16 yQPel = -2; yQPel <= 3; yQPel++) {
                for (mfxI16 xQPel = -2; xQPel <= 3; xQPel++) {
                    if ((yQPel == 0) && (xQPel == 0))
                        continue;
                    if (((yQPel == -2) && (xQPel == -2)) ||
                        ((yQPel == -2) && (xQPel == -1)) ||
                        ((yQPel == -2) && (xQPel ==  2)) ||
                        ((yQPel == -2) && (xQPel ==  3)) ||
                        ((yQPel == -1) && (xQPel == -2)) ||
                        ((yQPel == -1) && (xQPel ==  3)) ||
                        ((yQPel ==  2) && (xQPel == -2)) ||
                        ((yQPel ==  2) && (xQPel ==  3)) ||
                        ((yQPel ==  3) && (xQPel == -2)) ||
                        ((yQPel ==  3) && (xQPel == -1)) ||
                        ((yQPel ==  3) && (xQPel == 2)) ||
                        ((yQPel ==  3) && (xQPel == 3)))
                        continue;

                    mfxI16Pair qpeltry = {mvtry.x + xQPel, mvtry.y + yQPel};
                    refMv.x = qpeltry.x + (mfxI16)(xBlk * blkW * 4);
                    refMv.y = qpeltry.y + (mfxI16)(yBlk * blkH * 4);
                    InterpolateBlock(reffix, refMv, blkW, blkH, blockRefTry);
                    mfxU32 sad = CalcSad(blockSrc1, blockRefTry, blkW, blkH);
                    sad >>= 1;
                    if (sad < sadbest) {
                        qmvbest = qpeltry;
                        sadbest = sad;
                    }
                }
            }

            outData[xBlk].mv[lfix] = qmvbest;
        }
    }

    delete[] blockSrc;
    delete[] blockSrc1;
    delete[] blockRefFix;
    delete[] blockRefTry;

    return PASSED;
}

};