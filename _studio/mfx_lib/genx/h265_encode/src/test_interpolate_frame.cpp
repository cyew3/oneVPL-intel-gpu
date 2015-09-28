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
#include "../include/genx_hevce_interpolate_frame_bdw_isa.h"
#include "../include/genx_hevce_interpolate_frame_hsw_isa.h"

#ifdef CMRT_EMU
extern "C"
void InterpolateFrameWithBorder(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                      SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG);
#endif //CMRT_EMU

#define BORDER 4
#define WIDTHB (WIDTH + BORDER*2)
#define HEIGHTB (HEIGHT + BORDER*2)

namespace {
int RunGpu(const mfxU8 *in, mfxU8 *outH, mfxU8 *outV, mfxU8 *outD);
int RunCpu(const mfxU8 *in, mfxU8 *outH, mfxU8 *outV, mfxU8 *outD);
};

int main()
{
    mfxU8 *in   = new mfxU8[WIDTH * HEIGHT];
    mfxU8 *outHorzGpu = new mfxU8[WIDTHB * HEIGHTB];
    mfxU8 *outVertGpu = new mfxU8[WIDTHB * HEIGHTB];
    mfxU8 *outDiagGpu = new mfxU8[WIDTHB * HEIGHTB];
    mfxU8 *outHorzCpu = new mfxU8[WIDTHB * HEIGHTB];
    mfxU8 *outVertCpu = new mfxU8[WIDTHB * HEIGHTB];
    mfxU8 *outDiagCpu = new mfxU8[WIDTHB * HEIGHTB];
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(in, 1, WIDTH * HEIGHT, f) != WIDTH * HEIGHT) // read luma 1 frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    fclose(f);

    res = RunGpu(in, outHorzGpu, outVertGpu, outDiagGpu);
    CHECK_ERR(res);

    res = RunCpu(in, outHorzCpu, outVertCpu, outDiagCpu);
    CHECK_ERR(res);

    // compare output
    for (mfxI32 y = 0; y < HEIGHTB; y++) {
        for (mfxI32 x = 0; x < WIDTHB; x++) {
            if (outHorzGpu[y * WIDTHB + x] != outHorzCpu[y * WIDTHB + x]) {
                printf("bad sad value (%d != %d) for horizontal half-pel at (%.1f, %.1f)...\n",
                        outHorzGpu[y * WIDTHB + x], outHorzCpu[y * WIDTHB + x], x + 0.5, y);
                return 1;
            }
            if (outVertGpu[y * WIDTHB + x] != outVertCpu[y * WIDTHB + x]) {
                printf("bad sad value (%d != %d) for horizontal half-pel at (%.1f, %.1f)...\n",
                        outVertGpu[y * WIDTHB + x], outVertCpu[y * WIDTHB + x], x, y + 0.5);
                return 1;
            }
            if (outDiagGpu[y * WIDTHB + x] != outDiagCpu[y * WIDTHB + x]) {
                printf("bad sad value (%d != %d) for horizontal half-pel at (%.1f, %.1f)...\n",
                        outDiagGpu[y * WIDTHB + x], outDiagCpu[y * WIDTHB + x], x + 0.5, y + 0.5);
                return 1;
            }
        }
    }

    delete [] in;
    delete [] outHorzGpu;
    delete [] outVertGpu;
    delete [] outDiagGpu;
    delete [] outHorzCpu;
    delete [] outVertCpu;
    delete [] outDiagCpu;

    return printf("passed\n"), 0;
}

namespace {
int RunGpu(const mfxU8 *in, mfxU8 *outH, mfxU8 *outV, mfxU8 *outD)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    mfxI32 res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_interpolate_frame_hsw, sizeof(genx_hevce_interpolate_frame_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(InterpolateFrameWithBorder), kernel);
    CHECK_CM_ERR(res);

    CmSurface2D *inFpel = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, inFpel);
    CHECK_CM_ERR(res);
    res = inFpel->WriteSurface(in, NULL);
    CHECK_CM_ERR(res);

    mfxU32 outHorzPitch = 0;
    mfxU32 outHorzSize = 0;
    res = device->GetSurface2DInfo(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_P8, outHorzPitch, outHorzSize);
    CHECK_CM_ERR(res);
    mfxU8 *outHorzSys = (mfxU8 *)CM_ALIGNED_MALLOC(outHorzSize, 0x1000);
    CmSurface2DUP *outHorzCm = 0;
    res = device->CreateSurface2DUP(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_P8, outHorzSys, outHorzCm);
    CHECK_CM_ERR(res);

    mfxU32 outVertPitch = 0;
    mfxU32 outVertSize = 0;
    res = device->GetSurface2DInfo(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_P8, outVertPitch, outVertSize);
    CHECK_CM_ERR(res);
    mfxU8 *outVertSys = (mfxU8 *)CM_ALIGNED_MALLOC(outVertSize, 0x1000);
    CmSurface2DUP *outVertCm = 0;
    res = device->CreateSurface2DUP(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_P8, outVertSys, outVertCm);
    CHECK_CM_ERR(res);

    mfxU32 outDiagPitch = 0;
    mfxU32 outDiagSize = 0;
    res = device->GetSurface2DInfo(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_P8, outDiagPitch, outDiagSize);
    CHECK_CM_ERR(res);
    mfxU8 *outDiagSys = (mfxU8 *)CM_ALIGNED_MALLOC(outDiagSize, 0x1000);
    CmSurface2DUP *outDiagCm = 0;
    res = device->CreateSurface2DUP(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_P8, outDiagSys, outDiagCm);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInFpel = 0;
    res = inFpel->GetIndex(idxInFpel);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutHorz = 0;
    res = outHorzCm->GetIndex(idxOutHorz);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutVert = 0;
    res = outVertCm->GetIndex(idxOutVert);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutDiag = 0;
    res = outDiagCm->GetIndex(idxOutDiag);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInFpel), idxInFpel);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxOutHorz), idxOutHorz);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(*idxOutVert), idxOutVert);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(*idxOutDiag), idxOutDiag);
    CHECK_CM_ERR(res);

    const mfxU16 BlockW = 8;
    const mfxU16 BlockH = 8;

    mfxU32 tsWIDTHB = (WIDTHB + BlockW - 1) / BlockW;
    mfxU32 tsHEIGHTB = (HEIGHTB + BlockH - 1) / BlockH;
    res = kernel->SetThreadCount(tsWIDTHB * tsHEIGHTB);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWIDTHB, tsHEIGHTB, threadSpace);
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

    for (mfxI32 y = 0; y < HEIGHTB; y++, outH += WIDTHB, outV += WIDTHB, outD += WIDTHB) {
        memcpy(outH, outHorzSys + y * outHorzPitch, WIDTHB);
        memcpy(outV, outVertSys + y * outVertPitch, WIDTHB);
        memcpy(outD, outDiagSys + y * outDiagPitch, WIDTHB);
    }

#ifndef CMRT_EMU
    printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);

    queue->DestroyEvent(e);
    device->DestroySurface2DUP(outHorzCm);
    device->DestroySurface2DUP(outVertCm);
    device->DestroySurface2DUP(outDiagCm);
    CM_ALIGNED_FREE(outHorzSys);
    CM_ALIGNED_FREE(outVertSys);
    CM_ALIGNED_FREE(outDiagSys);
    device->DestroySurface(inFpel);
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
mfxI16 InterpolateHor(const T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty)
{
    return -GetPel(p, pitch, w, h, intx - 1, inty) + 5 * GetPel(p, pitch, w, h, intx + 0, inty)
           -GetPel(p, pitch, w, h, intx + 2, inty) + 5 * GetPel(p, pitch, w, h, intx + 1, inty);
}

template <class T>
mfxI16 InterpolateVer(const T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty)
{
    return -GetPel(p, pitch, w, h, intx, inty - 1) + 5 * GetPel(p, pitch, w, h, intx, inty + 0)
           -GetPel(p, pitch, w, h, intx, inty + 2) + 5 * GetPel(p, pitch, w, h, intx, inty + 1);
}

mfxU8 ShiftAndSat(mfxI16 val, int shift)
{
    short offset = 1 << (shift - 1);
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
        short hor = InterpolateHor(p, W, W, H, intx, inty + (dy >> 1));
        short ver = InterpolateVer(p, W, W, H, intx + (dx >> 1), inty);
        val = ShiftAndSat(ShiftAndSat(hor, 3) + ShiftAndSat(ver, 3), 1);
    }

    return (mfxU8)val;
}

int RunCpu(const mfxU8 *in, mfxU8 *outH, mfxU8 *outV, mfxU8 *outD)
{
    for (mfxI32 y = 0; y < HEIGHTB; y++, outH += WIDTHB, outV += WIDTHB, outD += WIDTHB) {
        for (mfxI32 x = 0; x < WIDTHB; x++) {
            outH[x] = InterpolatePel(in, 4 * (x - BORDER) + 2, 4 * (y - BORDER) + 0);
            outV[x] = InterpolatePel(in, 4 * (x - BORDER) + 0, 4 * (y - BORDER) + 2);
            outD[x] = InterpolatePel(in, 4 * (x - BORDER) + 2, 4 * (y - BORDER) + 2);
        }
    }

    return PASSED;
}

};