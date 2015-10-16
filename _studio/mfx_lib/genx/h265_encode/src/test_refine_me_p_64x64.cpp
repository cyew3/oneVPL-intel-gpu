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
#include "vector"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_hevce_refine_me_p_64x64_bdw_isa.h"
#include "../include/genx_hevce_refine_me_p_64x64_hsw_isa.h"

#ifdef CMRT_EMU
extern "C"
void RefineMeP64x64(SurfaceIndex DIST, SurfaceIndex MV, SurfaceIndex SRC, SurfaceIndex REF);
#endif //CMRT_EMU

namespace {
    int RunGpu(const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvData, mfxI16Pair *outMvData, Ipp32u *distData, Ipp32s width, Ipp32s height);
    int RunRef(const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvData, mfxI16Pair *outMvData, Ipp32u *distData, Ipp32s width, Ipp32s height);
    template <class T> int Compare(const T *outGpu, const T *outRef, Ipp32s width, Ipp32s height, const char *name);
}

static const Ipp32s BLOCKSIZE = 64;

int main()
{
    Ipp32s width = 1920;
    Ipp32s height = 1080;
    mfxI32 width64x = DIVUP(width, BLOCKSIZE);
    mfxI32 height64x = DIVUP(height, BLOCKSIZE);
    std::vector<Ipp32u> distGpu(width64x * height64x * 16);
    std::vector<Ipp32u> distRef(width64x * height64x * 16);
    std::vector<Ipp8u> src(width * height * 3 / 2);
    std::vector<Ipp8u> ref(width * height * 3 / 2);
    std::vector<mfxI16Pair> mv(width64x * height64x);
    std::vector<mfxI16Pair> mvGpu(width64x * height64x);
    std::vector<mfxI16Pair> mvRef(width64x * height64x);
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src.data(), 1, src.size(), f) != src.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref.data(), 1, ref.size(), f) != ref.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    // fill motion vector field
    srand(0x12345678);
    for (Ipp32s y = 0; y < height64x; y++) {
        for (Ipp32s x = 0; x < width64x; x++) {
            mv[y * width64x + x].x = (rand() & 0xfc) - 128; // -128,-124..,120,124
            mv[y * width64x + x].y = (rand() & 0xfc) - 128; // -128,-124..,120,124
        }
    }

    res = RunGpu(src.data(), ref.data(), mv.data(), mvGpu.data(), distGpu.data(), width, height);
    CHECK_ERR(res);

    res = RunRef(src.data(), ref.data(), mv.data(), mvRef.data(), distRef.data(), width, height);
    CHECK_ERR(res);

    if (Compare(distGpu.data(), distRef.data(), width64x*16, height64x, "dist") != PASSED)
        return FAILED;

    if (Compare(mvGpu.data(), mvRef.data(), width64x, height64x, "mv") != PASSED)
        return FAILED;

    return printf("passed\n"), 0;
}

bool operator==(const mfxI16Pair &l, const mfxI16Pair &r) { return l.x == r.x && l.y == r.y; }

namespace {
    template <class T> int Compare(const T *outGpu, const T *outRef, Ipp32s width, Ipp32s height, const char *name)
    {
        auto diff = std::mismatch(outGpu, outGpu + width * height, outRef);
        if (diff.first != outGpu + width * height) {
            Ipp32s pos = Ipp32s(diff.first - outGpu);
            printf("FAILED: \"%s\" surfaces differ at x=%d, y=%d tst=%d, ref=%d\n", name, pos%width, pos/width, *diff.first, *diff.second);
            return FAILED;
        }
        return PASSED;
    }

    int RunGpu(const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvData, mfxI16Pair *outMvData, Ipp32u *distData, Ipp32s width, Ipp32s height)
    {
        mfxI32 width64x = DIVUP(width, BLOCKSIZE);
        mfxI32 height64x = DIVUP(height, BLOCKSIZE);

        mfxU32 version = 0;
        CmDevice *device = 0;
        mfxI32 res = CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_refine_me_p_64x64_hsw, sizeof(genx_hevce_refine_me_p_64x64_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(RefineMeP64x64), kernel);
        CHECK_CM_ERR(res);

        CmSurface2D *src = 0;
        res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, src);
        CHECK_CM_ERR(res);
        res = src->WriteSurfaceStride(srcData, NULL, width);
        CHECK_CM_ERR(res);

        CmSurface2D *ref = 0;
        res = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_NV12, ref);
        CHECK_CM_ERR(res);
        res = ref->WriteSurfaceStride(refData, NULL, width);
        CHECK_CM_ERR(res);

        mfxU32 mvPitch = 0;
        mfxU32 mvSize = 0;
        res = device->GetSurface2DInfo(width64x * sizeof(mfxI16Pair), height64x, CM_SURFACE_FORMAT_P8, mvPitch, mvSize);
        CHECK_CM_ERR(res);
        mfxU8 *mvSys = (mfxU8 *)CM_ALIGNED_MALLOC(mvSize, 0x1000);
        CmSurface2DUP *mv = 0;
        res = device->CreateSurface2DUP(width64x * sizeof(mfxI16Pair), height64x, CM_SURFACE_FORMAT_P8, mvSys, mv);
        CHECK_CM_ERR(res);
        for (mfxU32 y = 0; y < height64x; y++)
            memcpy(mvSys + y * mvPitch, mvData + y * width64x, width64x * sizeof(mfxI16Pair));

        mfxU32 distPitch = 0;
        mfxU32 distSize = 0;
        res = device->GetSurface2DInfo(width64x * 16 * sizeof(Ipp32u), height64x, CM_SURFACE_FORMAT_P8, distPitch, distSize);
        CHECK_CM_ERR(res);
        mfxU8 *distSys = (mfxU8 *)CM_ALIGNED_MALLOC(distSize, 0x1000);
        memset(distSys, 0, distSize);
        CmSurface2DUP *dist = 0;
        res = device->CreateSurface2DUP(width64x * 16 * sizeof(Ipp32u), height64x, CM_SURFACE_FORMAT_P8, distSys, dist);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxSrc = 0;
        res = src->GetIndex(idxSrc);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxRef = 0;
        res = ref->GetIndex(idxRef);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxMv = 0;
        res = mv->GetIndex(idxMv);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxDist = 0;
        res = dist->GetIndex(idxDist);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxDist), idxDist);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*idxMv), idxMv);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxSrc), idxSrc);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idxRef), idxRef);
        CHECK_CM_ERR(res);

        res = kernel->SetThreadCount(width64x * height64x);
        CHECK_CM_ERR(res);

        CmThreadSpace * threadSpace = 0;
        res = device->CreateThreadSpace(width64x, height64x, threadSpace);
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

        for (Ipp32s y = 0; y < height64x; y++)
            memcpy(distData + y * width64x * 16, distSys + y * distPitch, width64x * 16 * sizeof(*distData));
        for (mfxU32 y = 0; y < height64x; y++)
            memcpy(outMvData + y * width64x, mvSys + y * mvPitch, width64x * sizeof(mfxI16Pair));

#ifndef CMRT_EMU
        printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
    
        queue->DestroyEvent(e);
        device->DestroySurface2DUP(dist);
        CM_ALIGNED_FREE(distSys);
        device->DestroySurface2DUP(mv);
        CM_ALIGNED_FREE(mvSys);
        device->DestroySurface(ref);
        device->DestroySurface(src);
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
    mfxI16 InterpolateVer(T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty)
    {
        return -GetPel(p, pitch, w, h, intx, inty - 1) + 5 * GetPel(p, pitch, w, h, intx, inty + 0)
               -GetPel(p, pitch, w, h, intx, inty + 2) + 5 * GetPel(p, pitch, w, h, intx, inty + 1);
    }

    mfxU8 ShiftAndSat(mfxI16 val, int shift)
    {
        short offset = 1 << (shift - 1);
        return (mfxU8)CLIPVAL((val + offset) >> shift, 0, 255);
    }

    mfxU8 InterpolatePel(mfxU8 const * p, Ipp32s pitch, Ipp32s width, Ipp32s height, mfxI32 x, mfxI32 y)
    {
        int intx = x >> 2;
        int inty = y >> 2;
        int dx = x & 3;
        int dy = y & 3;
        const int W = width;
        const int H = height;
        short val = 0;

        if (dy == 0 && dx == 0) {
            val = GetPel(p, pitch, W, H, intx, inty);

        } else if (dy == 0) {
            val = InterpolateHor(p, pitch, W, H, intx, inty);
            val = ShiftAndSat(val, 3);
            if (dx == 1)
                val = ShiftAndSat(GetPel(p, pitch, W, H, intx + 0, inty) + val, 1);
            else if (dx == 3)
                val = ShiftAndSat(GetPel(p, pitch, W, H, intx + 1, inty) + val, 1);

        } else if (dx == 0) {
            val = InterpolateVer(p, pitch , W, H, intx, inty);
            val = ShiftAndSat(val, 3);
            if (dy == 1)
                val = ShiftAndSat(GetPel(p, pitch , W, H, intx, inty + 0) + val, 1);
            else if (dy == 3)
                val = ShiftAndSat(GetPel(p, pitch , W, H, intx, inty + 1) + val, 1);

        } else if (dx == 2) {
            short tmp[4] = {
                InterpolateHor(p, pitch , W, H, intx, inty - 1),
                InterpolateHor(p, pitch , W, H, intx, inty + 0),
                InterpolateHor(p, pitch , W, H, intx, inty + 1),
                InterpolateHor(p, pitch , W, H, intx, inty + 2)
            };
            val = InterpolateVer(tmp, 1, 1, 4, 0, 1);
            val = ShiftAndSat(val, 6);

            if (dy == 1)
                val = ShiftAndSat(ShiftAndSat(tmp[1], 3) + val, 1);
            else if (dy == 3)
                val = ShiftAndSat(ShiftAndSat(tmp[2], 3) + val, 1);

        } else if (dy == 2) {
            short tmp[4] = {
                InterpolateVer(p, pitch , W, H, intx - 1, inty),
                InterpolateVer(p, pitch , W, H, intx + 0, inty),
                InterpolateVer(p, pitch , W, H, intx + 1, inty),
                InterpolateVer(p, pitch , W, H, intx + 2, inty)
            };
            val = InterpolateHor(tmp, 0, 4, 1, 1, 0);
            val = ShiftAndSat(val, 6);
            if (dx == 1)
                val = ShiftAndSat(ShiftAndSat(tmp[1], 3) + val, 1);
            else if (dx == 3)
                val = ShiftAndSat(ShiftAndSat(tmp[2], 3) + val, 1);
        }
        else {
            short hor = InterpolateHor(p, pitch , W, H, intx, inty + (dy >> 1));
            short ver = InterpolateVer(p, pitch , W, H, intx + (dx >> 1), inty);
            val = ShiftAndSat(ShiftAndSat(hor, 3) + ShiftAndSat(ver, 3), 1);
        }

        return (mfxU8)val;
    }


    void InterpolateBlock(const mfxU8 *ref, Ipp32s pitch, Ipp32s width, Ipp32s height, mfxI16Pair point, mfxI32 blockW, mfxI32 blockH, mfxU8 *block)
    {
        for (mfxI32 y = 0; y < blockH; y++, block += blockW)
            for (mfxI32 x = 0; x < blockW; x++)
                block[x] = InterpolatePel(ref, pitch, width, height, point.x + x * 4, point.y + y * 4);
    }

    mfxU32 CalcSad(const mfxU8 *blockSrc, const mfxU8 *blockRef, mfxI32 blockW, mfxI32 blockH)
    {
        mfxU32 sad = 0;
        for (mfxI32 y = 0; y < blockH; y++, blockSrc += blockW, blockRef += blockW)
            for (mfxI32 x = 0; x < blockW; x++)
                sad += abs(blockSrc[x] - blockRef[x]);
        return sad;
    }

    void CopyBlock(const mfxU8 *src, Ipp32s pitch, Ipp32s width, Ipp32s height,
        Ipp32s x, Ipp32s y, Ipp32s blockW, Ipp32s blockH, Ipp8u *block)
    {
        for (mfxI32 yy = 0; yy < blockH; yy++, block += blockW)
            for (mfxI32 xx = 0; xx < blockW; xx++)
                block[xx] = GetPel(src, pitch, width, height, x + xx, y + yy); 
    }

    int RunRef(const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvData, mfxI16Pair *outMvData, Ipp32u *distData, Ipp32s width, Ipp32s height)
    {
        mfxI32 width64x = DIVUP(width, BLOCKSIZE);
        mfxI32 height64x = DIVUP(height, BLOCKSIZE);
        mfxU8 blockSrc[BLOCKSIZE * BLOCKSIZE];
        mfxU8 blockRef[BLOCKSIZE * BLOCKSIZE];

        for (mfxI32 yBlk = 0; yBlk < height64x; yBlk++, distData += width64x * 16) {
            for (mfxI32 xBlk = 0; xBlk < width64x; xBlk++) {
                CopyBlock(srcData, width, width, height, xBlk * BLOCKSIZE, yBlk * BLOCKSIZE, BLOCKSIZE, BLOCKSIZE, blockSrc);

                mfxI16 originMvx = mvData[yBlk * width64x + xBlk].x;
                mfxI16 originMvy = mvData[yBlk * width64x + xBlk].y;
                mfxI16 cx = xBlk * BLOCKSIZE + originMvx / 4;
                mfxI16 cy = yBlk * BLOCKSIZE + originMvy / 4;
                mfxU32 bestSad = mfxU32(-1);
                for (mfxI16 dy = -1; dy <= 1; dy++) {
                    for (mfxI16 dx = -1; dx <= 1; dx++) {
                //for (mfxI16 dy = 0; dy <= 0; dy++) {
                //    for (mfxI16 dx = 0; dx <= 0; dx++) {
                        CopyBlock(refData, width, width, height, cx + dx, cy + dy, BLOCKSIZE, BLOCKSIZE, blockRef);
                        mfxU32 sad = CalcSad(blockSrc, blockRef, BLOCKSIZE, BLOCKSIZE);
                        if (bestSad > sad) {
                            bestSad = sad;
                            outMvData[yBlk * width64x + xBlk].x = originMvx + dx * 4;
                            outMvData[yBlk * width64x + xBlk].y = originMvy + dy * 4;
                        }
                    }
                }

                mfxI16Pair hpelPoint = outMvData[yBlk * width64x + xBlk];
                hpelPoint.x = (mfxI16)(hpelPoint.x + xBlk * BLOCKSIZE * 4);
                hpelPoint.y = (mfxI16)(hpelPoint.y + yBlk * BLOCKSIZE * 4);

                for (mfxU32 sadIdx = 0; sadIdx < 9; sadIdx++) {
                    mfxI16 dx = (mfxI16)(sadIdx % 3 - 1);
                    mfxI16 dy = (mfxI16)(sadIdx / 3 - 1);
                    mfxI16Pair qpelPoint = { hpelPoint.x + 2 * dx, hpelPoint.y + 2 * dy };

                    InterpolateBlock(refData, width, width, height, qpelPoint, BLOCKSIZE, BLOCKSIZE, blockRef);
                    distData[xBlk * 16 + sadIdx] = CalcSad(blockSrc, blockRef, BLOCKSIZE, BLOCKSIZE);
                }
            }
        }

        return PASSED;
    }
};