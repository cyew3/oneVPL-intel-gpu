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
#include "../include/test_common.h"
#include "../include/genx_hevce_me_p16_4mv_and_refine_32x32_bdw_isa.h"
#include "../include/genx_hevce_me_p16_4mv_and_refine_32x32_hsw_isa.h"
#include "../include/genx_hevce_me_p16_4mv_bdw_isa.h"
#include "../include/genx_hevce_me_p16_4mv_hsw_isa.h"


#ifdef CMRT_EMU
extern "C" void Me16AndRefine32x32(
    SurfaceIndex CONTROL, SurfaceIndex SRC_AND_REF, SurfaceIndex DIST16x16,
    SurfaceIndex DIST8x8, SurfaceIndex MV16x16, SurfaceIndex MV8x8,
    SurfaceIndex SRC, SurfaceIndex REF, SurfaceIndex MV32x32, SurfaceIndex DIST32x32);
extern "C" void Me16RectPartsAndRefine32x32(
    SurfaceIndex CONTROL, SurfaceIndex SRC_AND_REF,
    SurfaceIndex DIST16x16, SurfaceIndex DIST16x8, SurfaceIndex DIST8x16, SurfaceIndex DIST8x8,
    SurfaceIndex MV16x16, SurfaceIndex MV16x8, SurfaceIndex MV8x16, SurfaceIndex MV8x8,
    SurfaceIndex SRC, SurfaceIndex REF, SurfaceIndex MV32x32, SurfaceIndex DIST32x32);
#endif //CMRT_EMU

namespace {
    int RunGpu(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxU32 *dist16x16Data, mfxU32 *dist16x8Data, mfxU32 *dist8x16Data, mfxU32 *dist8x8Data,
        mfxI16Pair *mv16x16Data, mfxI16Pair *mv16x8Data, mfxI16Pair *mv8x16Data, mfxI16Pair *mv8x8Data,
        const mfxI16Pair *mv32x32Data, mfxU32 *dist32x32, int rectParts);
    int RunRef(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxU32 *dist16x16Data, mfxU32 *dist16x8Data, mfxU32 *dist8x16Data, mfxU32 *dist8x8Data,
        mfxI16Pair *mv16x16Data, mfxI16Pair *mv16x8Data, mfxI16Pair *mv8x16Data, mfxI16Pair *mv8x8Data,
        const mfxI16Pair *mv32x32Data, mfxU32 *dist32x32, int rectParts);
    template <class T> int Compare(const T *outGpu, const T *outRef, Ipp32s width, Ipp32s height, const char *name);

};

int main()
{
    int width = ROUNDUP(1920, 16);
    int height = ROUNDUP(1080, 16);
    int widthIn8 = DIVUP(width, 8);
    int heightIn8 = DIVUP(height, 8);
    int widthIn16 = DIVUP(width, 16);
    int heightIn16 = DIVUP(height, 16);
    int widthIn32 = DIVUP(width, 32);
    int heightIn32 = DIVUP(height, 32);
    int frameSize = width * height * 3 / 2;

    std::vector<mfxU8> src(frameSize);
    std::vector<mfxU8> ref(frameSize);
    std::vector<mfxI16Pair> mvPred(widthIn16 * heightIn16);
    std::vector<mfxI16Pair> mv32x32(widthIn32 * heightIn32);

    std::vector<mfxI16Pair> mv8x8Tst(widthIn8 * heightIn8);
    std::vector<mfxI16Pair> mv8x8Ref(widthIn8 * heightIn8);
    std::vector<mfxI16Pair> mv16x8Tst(widthIn16 * heightIn8);
    std::vector<mfxI16Pair> mv16x8Ref(widthIn16 * heightIn8);
    std::vector<mfxI16Pair> mv8x16Tst(widthIn8 * heightIn16);
    std::vector<mfxI16Pair> mv8x16Ref(widthIn8 * heightIn16);
    std::vector<mfxI16Pair> mv16x16Tst(widthIn16 * heightIn16);
    std::vector<mfxI16Pair> mv16x16Ref(widthIn16 * heightIn16);
    std::vector<mfxU32> dist8x8Tst(widthIn8 * heightIn8);
    std::vector<mfxU32> dist8x8Ref(widthIn8 * heightIn8);
    std::vector<mfxU32> dist16x8Tst(widthIn16 * heightIn8);
    std::vector<mfxU32> dist16x8Ref(widthIn16 * heightIn8);
    std::vector<mfxU32> dist8x16Tst(widthIn8 * heightIn16);
    std::vector<mfxU32> dist8x16Ref(widthIn8 * heightIn16);
    std::vector<mfxU32> dist16x16Tst(widthIn16 * heightIn16);
    std::vector<mfxU32> dist16x16Ref(widthIn16 * heightIn16);
    std::vector<mfxU32> dist32x32Tst(widthIn32 * 16 * heightIn32);
    std::vector<mfxU32> dist32x32Ref(widthIn32 * 16 * heightIn32);

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src.data(), 1, src.size(), f) != src.size()) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fread(ref.data(), 1, ref.size(), f) != ref.size()) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    srand(0x12345678);
    for (mfxI32 y = 0; y < heightIn16; y++) {
        for (mfxI32 x = 0; x < widthIn16; x++) {
            mvPred[y * widthIn16 + x].x = (rand() & 0x1e) - 16; // -16,-14..,12,14
            mvPred[y * widthIn16 + x].y = (rand() & 0x1e) - 16; // -16,-14..,12,14
        }
    }

    for (mfxI32 y = 0; y < heightIn32; y++) {
        for (mfxI32 x = 0; x < widthIn32; x++) {
            mv32x32[y * widthIn32 + x].x = (rand() & 0x1e) - 16; // -16,-14..,12,14
            mv32x32[y * widthIn32 + x].y = (rand() & 0x1e) - 16; // -16,-14..,12,14
        }
    }

    Me2xControl ctrl = {};
    SetupMeControlShortPath(ctrl, width, height);
    //SetupMeControl(ctrl, width, height);

    Ipp32s res;
    printf("rectParts=0: ");
    res = RunGpu(ctrl, src.data(), ref.data(), mvPred.data(), dist16x16Tst.data(), dist16x8Tst.data(),
        dist8x16Tst.data(), dist8x8Tst.data(), mv16x16Tst.data(), mv16x8Tst.data(), mv8x16Tst.data(),
        mv8x8Tst.data(), mv32x32.data(), dist32x32Tst.data(), 0);
    CHECK_ERR(res);
    res = RunRef(ctrl, src.data(), ref.data(), mvPred.data(), dist16x16Ref.data(), dist16x8Ref.data(),
        dist8x16Ref.data(), dist8x8Ref.data(), mv16x16Ref.data(), mv16x8Ref.data(), mv8x16Ref.data(),
        mv8x8Ref.data(), mv32x32.data(), dist32x32Ref.data(), 0);
    CHECK_ERR(res);

    if (Compare(dist32x32Tst.data(), dist32x32Ref.data(), widthIn32*16, heightIn32, "dist32x32") != PASSED ||
        Compare(dist32x32Tst.data(), dist32x32Ref.data(), widthIn32*16, heightIn32, "dist32x32") != PASSED ||
        Compare(dist16x16Tst.data(), dist16x16Ref.data(), widthIn16, heightIn16, "dist16x16") != PASSED ||
        Compare(dist8x8Tst.data(), dist8x8Ref.data(), widthIn8, heightIn8, "dist8x8") != PASSED ||
        Compare(mv16x16Tst.data(), mv16x16Ref.data(), widthIn16, heightIn16, "mv16x16") != PASSED ||
        Compare(mv8x8Tst.data(), mv8x8Ref.data(), widthIn8, heightIn8, "mv8x8") != PASSED)
        return FAILED;

    printf("  rectParts=1: ");
    res = RunGpu(ctrl, src.data(), ref.data(), mvPred.data(), dist16x16Tst.data(), dist16x8Tst.data(),
        dist8x16Tst.data(), dist8x8Tst.data(), mv16x16Tst.data(), mv16x8Tst.data(), mv8x16Tst.data(),
        mv8x8Tst.data(), mv32x32.data(), dist32x32Tst.data(), 1);
    CHECK_ERR(res);
    res = RunRef(ctrl, src.data(), ref.data(), mvPred.data(), dist16x16Ref.data(), dist16x8Ref.data(),
        dist8x16Ref.data(), dist8x8Ref.data(), mv16x16Ref.data(), mv16x8Ref.data(), mv8x16Ref.data(),
        mv8x8Ref.data(), mv32x32.data(), dist32x32Ref.data(), 1);
    CHECK_ERR(res);
    if (Compare(dist32x32Tst.data(), dist32x32Ref.data(), widthIn32*16, heightIn32, "dist32x32") != PASSED ||
        Compare(dist16x16Tst.data(), dist16x16Ref.data(), widthIn16, heightIn16, "dist16x16") != PASSED ||
        Compare(dist16x8Tst.data(), dist16x8Ref.data(), widthIn16, heightIn8, "dist16x8") != PASSED ||
        Compare(dist8x16Tst.data(), dist8x16Ref.data(), widthIn8, heightIn16, "dist8x16") != PASSED ||
        Compare(dist8x8Tst.data(), dist8x8Ref.data(), widthIn8, heightIn8, "dist8x8") != PASSED ||
        Compare(mv16x16Tst.data(), mv16x16Ref.data(), widthIn16, heightIn16, "mv16x16") != PASSED ||
        Compare(mv16x8Tst.data(), mv16x8Ref.data(), widthIn16, heightIn8, "mv16x8") != PASSED ||
        Compare(mv8x16Tst.data(), mv8x16Ref.data(), widthIn8, heightIn16, "mv8x16") != PASSED ||
        Compare(mv8x8Tst.data(), mv8x8Ref.data(), widthIn8, heightIn8, "mv8x8") != PASSED)
        return FAILED;

    return printf("passed\n"), 0;
}


bool operator==(const mfxI16Pair l, const mfxI16Pair r) { return l.x == r.x && l.y == r.y; };

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

    int RunGpu(
        const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxU32 *dist16x16Data, mfxU32 *dist16x8Data, mfxU32 *dist8x16Data, mfxU32 *dist8x8Data,
        mfxI16Pair *mv16x16Data, mfxI16Pair *mv16x8Data, mfxI16Pair *mv8x16Data, mfxI16Pair *mv8x8Data,
        const mfxI16Pair *mv32x32Data, mfxU32 *dist32x32Data, int rectParts)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_me_p16_4mv_and_refine_32x32_hsw, sizeof(genx_hevce_me_p16_4mv_and_refine_32x32_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = (rectParts==0) ? device->CreateKernel(program, CM_KERNEL_FUNCTION(Me16AndRefine32x32), kernel)
                             : device->CreateKernel(program, CM_KERNEL_FUNCTION(Me16RectPartsAndRefine32x32), kernel);
        CHECK_CM_ERR(res);

        CmBuffer *ctrlBuf = 0;
        res = device->CreateBuffer(sizeof(Me2xControl), ctrlBuf);
        CHECK_CM_ERR(res);
        res = ctrlBuf->WriteSurface((const Ipp8u *)&ctrl, NULL, sizeof(Me2xControl));
        CHECK_CM_ERR(res);

        CmSurface2D *src = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, src);
        CHECK_CM_ERR(res);
        res = src->WriteSurfaceStride(srcData, NULL, ctrl.width);
        CHECK_CM_ERR(res);

        CmSurface2D *ref = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, ref);
        CHECK_CM_ERR(res);
        res = ref->WriteSurfaceStride(refData, NULL, ctrl.width);
        CHECK_CM_ERR(res);

        SurfaceIndex *genxRefs = 0;
        res = device->CreateVmeSurfaceG7_5(src, &ref, NULL, 1, 0, genxRefs);
        CHECK_CM_ERR(res);

        Ipp32u mv32x32Pitch = 0;
        Ipp32u mv32x32Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 32) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 32), CM_SURFACE_FORMAT_P8, mv32x32Pitch, mv32x32Size);
        CHECK_CM_ERR(res);
        void *mv32x32Sys = CM_ALIGNED_MALLOC(mv32x32Size, 0x1000);
        CmSurface2DUP *mv32x32 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 32) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 32), CM_SURFACE_FORMAT_P8, mv32x32Sys, mv32x32);
        CHECK_CM_ERR(res);

        Ipp32u mv16x16Pitch = 0;
        Ipp32u mv16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x16Pitch, mv16x16Size);
        CHECK_CM_ERR(res);
        void *mv16x16Sys = CM_ALIGNED_MALLOC(mv16x16Size, 0x1000);
        CmSurface2DUP *mv16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x16Sys, mv16x16);
        CHECK_CM_ERR(res);

        Ipp32u mv16x8Pitch = 0;
        Ipp32u mv16x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x8Pitch, mv16x8Size);
        CHECK_CM_ERR(res);
        void *mv16x8Sys = CM_ALIGNED_MALLOC(mv16x8Size, 0x1000);
        CmSurface2DUP *mv16x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x8Sys, mv16x8);
        CHECK_CM_ERR(res);

        Ipp32u mv8x16Pitch = 0;
        Ipp32u mv8x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv8x16Pitch, mv8x16Size);
        CHECK_CM_ERR(res);
        void *mv8x16Sys = CM_ALIGNED_MALLOC(mv8x16Size, 0x1000);
        CmSurface2DUP *mv8x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv8x16Sys, mv8x16);
        CHECK_CM_ERR(res);

        Ipp32u mv8x8Pitch = 0;
        Ipp32u mv8x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv8x8Pitch, mv8x8Size);
        CHECK_CM_ERR(res);
        void *mv8x8Sys = CM_ALIGNED_MALLOC(mv8x8Size, 0x1000);
        CmSurface2DUP *mv8x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv8x8Sys, mv8x8);
        CHECK_CM_ERR(res);

        Ipp32u dist32x32Pitch = 0;
        Ipp32u dist32x32Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * 16 * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist32x32Pitch, dist32x32Size);
        CHECK_CM_ERR(res);
        void *dist32x32Sys = CM_ALIGNED_MALLOC(dist32x32Size, 0x1000);
        CmSurface2DUP *dist32x32 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * 16 * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist32x32Sys, dist32x32);
        CHECK_CM_ERR(res);

        Ipp32u dist16x16Pitch = 0;
        Ipp32u dist16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist16x16Pitch, dist16x16Size);
        CHECK_CM_ERR(res);
        void *dist16x16Sys = CM_ALIGNED_MALLOC(dist16x16Size, 0x1000);
        CmSurface2DUP *dist16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist16x16Sys, dist16x16);
        CHECK_CM_ERR(res);

        Ipp32u dist16x8Pitch = 0;
        Ipp32u dist16x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist16x8Pitch, dist16x8Size);
        CHECK_CM_ERR(res);
        void *dist16x8Sys = CM_ALIGNED_MALLOC(dist16x8Size, 0x1000);
        CmSurface2DUP *dist16x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist16x8Sys, dist16x8);
        CHECK_CM_ERR(res);

        Ipp32u dist8x16Pitch = 0;
        Ipp32u dist8x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist8x16Pitch, dist8x16Size);
        CHECK_CM_ERR(res);
        void *dist8x16Sys = CM_ALIGNED_MALLOC(dist8x16Size, 0x1000);
        CmSurface2DUP *dist8x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist8x16Sys, dist8x16);
        CHECK_CM_ERR(res);

        Ipp32u dist8x8Pitch = 0;
        Ipp32u dist8x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist8x8Pitch, dist8x8Size);
        CHECK_CM_ERR(res);
        void *dist8x8Sys = CM_ALIGNED_MALLOC(dist8x8Size, 0x1000);
        CmSurface2DUP *dist8x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist8x8Sys, dist8x8);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxCtrl = 0;
        res = ctrlBuf->GetIndex(idxCtrl);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxDist32x32 = 0;
        res = dist32x32->GetIndex(idxDist32x32);
        SurfaceIndex *idxDist16x16 = 0;
        res = dist16x16->GetIndex(idxDist16x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist16x8 = 0;
        res = dist16x8->GetIndex(idxDist16x8);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist8x16 = 0;
        res = dist8x16->GetIndex(idxDist8x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist8x8 = 0;
        res = dist8x8->GetIndex(idxDist8x8);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxMv32x32 = 0;
        res = mv32x32->GetIndex(idxMv32x32);
        SurfaceIndex *idxMv16x16 = 0;
        res = mv16x16->GetIndex(idxMv16x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv16x8 = 0;
        res = mv16x8->GetIndex(idxMv16x8);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv8x16 = 0;
        res = mv8x16->GetIndex(idxMv8x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv8x8 = 0;
        res = mv8x8->GetIndex(idxMv8x8);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxSrc = 0;
        res = src->GetIndex(idxSrc);
        SurfaceIndex *idxRef = 0;
        res = ref->GetIndex(idxRef);

        int argIdx = 0;
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxCtrl);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), genxRefs);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxDist16x16);
        CHECK_CM_ERR(res);
        if (rectParts) {
            res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxDist16x8);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxDist8x16);
            CHECK_CM_ERR(res);
        }
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxDist8x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxMv16x16);
        CHECK_CM_ERR(res);
        if (rectParts) {
            res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxMv16x8);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxMv8x16);
            CHECK_CM_ERR(res);
        }
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxMv8x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxSrc);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxRef);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxMv32x32);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxDist32x32);
        CHECK_CM_ERR(res);

        // MV16x16 also serves as MV predictor
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy((char *)mv16x16Sys + y * mv16x16Pitch, mvPredData + y * DIVUP(ctrl.width, 16), sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));

        for (int y = 0; y < DIVUP(ctrl.height, 32); y++)
            memcpy((char *)mv32x32Sys + y * mv32x32Pitch, mv32x32Data + y * DIVUP(ctrl.width, 32), sizeof(mfxI16Pair) * DIVUP(ctrl.width, 32));

        mfxU32 tsWidth  = DIVUP(ctrl.width, 16);
        mfxU32 tsHeight = DIVUP(ctrl.height, 16);
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

        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(mv16x16Data + y * DIVUP(ctrl.width, 16), (char *)mv16x16Sys + y * mv16x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv16x8Data + y * DIVUP(ctrl.width, 16), (char *)mv16x8Sys + y * mv16x8Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(mv8x16Data + y * DIVUP(ctrl.width, 8), (char *)mv8x16Sys + y * mv8x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv8x8Data + y * DIVUP(ctrl.width, 8), (char *)mv8x8Sys + y * mv8x8Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));

        for (int y = 0; y < DIVUP(ctrl.height, 32); y++)
            memcpy(dist32x32Data + y * DIVUP(ctrl.width, 32) * 16, (char *)dist32x32Sys + y * dist32x32Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 32) * 16);
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(dist16x16Data + y * DIVUP(ctrl.width, 16), (char *)dist16x16Sys + y * dist16x16Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(dist16x8Data + y * DIVUP(ctrl.width, 16), (char *)dist16x8Sys + y * dist16x8Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(dist8x16Data + y * DIVUP(ctrl.width, 8), (char *)dist8x16Sys + y * dist8x16Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 8));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(dist8x8Data + y * DIVUP(ctrl.width, 8), (char *)dist8x8Sys + y * dist8x8Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 8));

        // zeroing unused 7 sads
        for (int y = 0; y < DIVUP(ctrl.height, 32); y++)
            for (int x = 0; x < DIVUP(ctrl.width, 32); x++)
                memset(dist32x32Data + (y * DIVUP(ctrl.width, 32) + x) * 16 + 9, 0, sizeof(mfxU32) * 7);

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs);
        device->DestroySurface(ctrlBuf);
        device->DestroySurface(src);
        device->DestroySurface(ref);
        device->DestroySurface2DUP(mv32x32);
        device->DestroySurface2DUP(mv16x16);
        device->DestroySurface2DUP(mv16x8);
        device->DestroySurface2DUP(mv8x16);
        device->DestroySurface2DUP(mv8x8);
        device->DestroySurface2DUP(dist32x32);
        device->DestroySurface2DUP(dist16x16);
        device->DestroySurface2DUP(dist16x8);
        device->DestroySurface2DUP(dist8x16);
        device->DestroySurface2DUP(dist8x8);
        CM_ALIGNED_FREE(mv32x32Sys);
        CM_ALIGNED_FREE(mv16x16Sys);
        CM_ALIGNED_FREE(mv16x8Sys);
        CM_ALIGNED_FREE(mv8x16Sys);
        CM_ALIGNED_FREE(mv8x8Sys);
        CM_ALIGNED_FREE(dist32x32Sys);
        CM_ALIGNED_FREE(dist16x16Sys);
        CM_ALIGNED_FREE(dist16x8Sys);
        CM_ALIGNED_FREE(dist8x16Sys);
        CM_ALIGNED_FREE(dist8x8Sys);
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

    int RunRef(const Me2xControl &ctrl, const mfxU8 *srcData, const mfxU8 *refData, const mfxI16Pair *mvPredData,
        mfxU32 *dist16x16Data, mfxU32 *dist16x8Data, mfxU32 *dist8x16Data, mfxU32 *dist8x8Data,
        mfxI16Pair *mv16x16Data, mfxI16Pair *mv16x8Data, mfxI16Pair *mv8x16Data, mfxI16Pair *mv8x8Data,
        const mfxI16Pair *mv32x32Data, mfxU32 *dist32x32, int rectParts)
    {
        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_me_p16_4mv_hsw, sizeof(genx_hevce_me_p16_4mv_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MeP16_4MV), kernel);
        CHECK_CM_ERR(res);

        CmBuffer *ctrlBuf = 0;
        res = device->CreateBuffer(sizeof(Me2xControl), ctrlBuf);
        CHECK_CM_ERR(res);
        res = ctrlBuf->WriteSurface((const Ipp8u *)&ctrl, NULL, sizeof(Me2xControl));
        CHECK_CM_ERR(res);

        CmSurface2D *src = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, src);
        CHECK_CM_ERR(res);
        res = src->WriteSurfaceStride(srcData, NULL, ctrl.width);
        CHECK_CM_ERR(res);

        CmSurface2D *ref = 0;
        res = device->CreateSurface2D(ctrl.width, ctrl.height, CM_SURFACE_FORMAT_NV12, ref);
        CHECK_CM_ERR(res);
        res = ref->WriteSurfaceStride(refData, NULL, ctrl.width);
        CHECK_CM_ERR(res);

        SurfaceIndex *genxRefs = 0;
        res = device->CreateVmeSurfaceG7_5(src, &ref, NULL, 1, 0, genxRefs);
        CHECK_CM_ERR(res);

        Ipp32u mv16x16Pitch = 0;
        Ipp32u mv16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x16Pitch, mv16x16Size);
        CHECK_CM_ERR(res);
        void *mv16x16Sys = CM_ALIGNED_MALLOC(mv16x16Size, 0x1000);
        CmSurface2DUP *mv16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv16x16Sys, mv16x16);
        CHECK_CM_ERR(res);

        Ipp32u mv16x8Pitch = 0;
        Ipp32u mv16x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x8Pitch, mv16x8Size);
        CHECK_CM_ERR(res);
        void *mv16x8Sys = CM_ALIGNED_MALLOC(mv16x8Size, 0x1000);
        CmSurface2DUP *mv16x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv16x8Sys, mv16x8);
        CHECK_CM_ERR(res);

        Ipp32u mv8x16Pitch = 0;
        Ipp32u mv8x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv8x16Pitch, mv8x16Size);
        CHECK_CM_ERR(res);
        void *mv8x16Sys = CM_ALIGNED_MALLOC(mv8x16Size, 0x1000);
        CmSurface2DUP *mv8x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, mv8x16Sys, mv8x16);
        CHECK_CM_ERR(res);

        Ipp32u mv8x8Pitch = 0;
        Ipp32u mv8x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv8x8Pitch, mv8x8Size);
        CHECK_CM_ERR(res);
        void *mv8x8Sys = CM_ALIGNED_MALLOC(mv8x8Size, 0x1000);
        CmSurface2DUP *mv8x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, mv8x8Sys, mv8x8);
        CHECK_CM_ERR(res);

        Ipp32u dist16x16Pitch = 0;
        Ipp32u dist16x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist16x16Pitch, dist16x16Size);
        CHECK_CM_ERR(res);
        void *dist16x16Sys = CM_ALIGNED_MALLOC(dist16x16Size, 0x1000);
        CmSurface2DUP *dist16x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist16x16Sys, dist16x16);
        CHECK_CM_ERR(res);

        Ipp32u dist16x8Pitch = 0;
        Ipp32u dist16x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist16x8Pitch, dist16x8Size);
        CHECK_CM_ERR(res);
        void *dist16x8Sys = CM_ALIGNED_MALLOC(dist16x8Size, 0x1000);
        CmSurface2DUP *dist16x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 16) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist16x8Sys, dist16x8);
        CHECK_CM_ERR(res);

        Ipp32u dist8x16Pitch = 0;
        Ipp32u dist8x16Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist8x16Pitch, dist8x16Size);
        CHECK_CM_ERR(res);
        void *dist8x16Sys = CM_ALIGNED_MALLOC(dist8x16Size, 0x1000);
        CmSurface2DUP *dist8x16 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 16), CM_SURFACE_FORMAT_P8, dist8x16Sys, dist8x16);
        CHECK_CM_ERR(res);

        Ipp32u dist8x8Pitch = 0;
        Ipp32u dist8x8Size = 0;
        res = device->GetSurface2DInfo(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist8x8Pitch, dist8x8Size);
        CHECK_CM_ERR(res);
        void *dist8x8Sys = CM_ALIGNED_MALLOC(dist8x8Size, 0x1000);
        CmSurface2DUP *dist8x8 = 0;
        res = device->CreateSurface2DUP(DIVUP(ctrl.width, 8) * sizeof(mfxI16Pair), DIVUP(ctrl.height, 8), CM_SURFACE_FORMAT_P8, dist8x8Sys, dist8x8);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxCtrl = 0;
        res = ctrlBuf->GetIndex(idxCtrl);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxDist16x16 = 0;
        res = dist16x16->GetIndex(idxDist16x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist16x8 = 0;
        res = dist16x8->GetIndex(idxDist16x8);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist8x16 = 0;
        res = dist8x16->GetIndex(idxDist8x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxDist8x8 = 0;
        res = dist8x8->GetIndex(idxDist8x8);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxMv16x16 = 0;
        res = mv16x16->GetIndex(idxMv16x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv16x8 = 0;
        res = mv16x8->GetIndex(idxMv16x8);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv8x16 = 0;
        res = mv8x16->GetIndex(idxMv8x16);
        CHECK_CM_ERR(res);
        SurfaceIndex *idxMv8x8 = 0;
        res = mv8x8->GetIndex(idxMv8x8);
        CHECK_CM_ERR(res);

        int argIdx = 0;
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxCtrl), idxCtrl);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*genxRefs), genxRefs);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxDist16x16), idxDist16x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxDist16x8), idxDist16x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxDist8x16), idxDist8x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxDist8x8), idxDist8x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxMv16x16), idxMv16x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxMv16x8), idxMv16x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxMv8x16), idxMv8x16);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(*idxMv8x8), idxMv8x8);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(argIdx++, sizeof(rectParts), &rectParts);
        CHECK_CM_ERR(res);

        // MV16x16 also serves as MV predictor
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy((char *)mv16x16Sys + y * mv16x16Pitch, mvPredData + y * DIVUP(ctrl.width, 16), sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));

        mfxU32 tsWidth  = DIVUP(ctrl.width, 16);
        mfxU32 tsHeight = DIVUP(ctrl.height, 16);
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

        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(mv16x16Data + y * DIVUP(ctrl.width, 16), (char *)mv16x16Sys + y * mv16x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv16x8Data + y * DIVUP(ctrl.width, 16), (char *)mv16x8Sys + y * mv16x8Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(mv8x16Data + y * DIVUP(ctrl.width, 8), (char *)mv8x16Sys + y * mv8x16Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(mv8x8Data + y * DIVUP(ctrl.width, 8), (char *)mv8x8Sys + y * mv8x8Pitch, sizeof(mfxI16Pair) * DIVUP(ctrl.width, 8));

        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(dist16x16Data + y * DIVUP(ctrl.width, 16), (char *)dist16x16Sys + y * dist16x16Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(dist16x8Data + y * DIVUP(ctrl.width, 16), (char *)dist16x8Sys + y * dist16x8Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 16));
        for (int y = 0; y < DIVUP(ctrl.height, 16); y++)
            memcpy(dist8x16Data + y * DIVUP(ctrl.width, 8), (char *)dist8x16Sys + y * dist8x16Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 8));
        for (int y = 0; y < DIVUP(ctrl.height, 8); y++)
            memcpy(dist8x8Data + y * DIVUP(ctrl.width, 8), (char *)dist8x8Sys + y * dist8x8Pitch, sizeof(mfxU32) * DIVUP(ctrl.width, 8));

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        device->DestroyVmeSurfaceG7_5(genxRefs);
        device->DestroySurface(ctrlBuf);
        device->DestroySurface(src);
        device->DestroySurface(ref);
        device->DestroySurface2DUP(mv16x16);
        device->DestroySurface2DUP(mv16x8);
        device->DestroySurface2DUP(mv8x16);
        device->DestroySurface2DUP(mv8x8);
        device->DestroySurface2DUP(dist16x16);
        device->DestroySurface2DUP(dist16x8);
        device->DestroySurface2DUP(dist8x16);
        device->DestroySurface2DUP(dist8x8);
        CM_ALIGNED_FREE(mv16x16Sys);
        CM_ALIGNED_FREE(mv16x8Sys);
        CM_ALIGNED_FREE(mv8x16Sys);
        CM_ALIGNED_FREE(mv8x8Sys);
        CM_ALIGNED_FREE(dist16x16Sys);
        CM_ALIGNED_FREE(dist16x8Sys);
        CM_ALIGNED_FREE(dist8x16Sys);
        CM_ALIGNED_FREE(dist8x8Sys);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);


        mfxI32 numBlocksHor = DIVUP(ctrl.width, 32) ;
        mfxI32 numBlocksVer = DIVUP(ctrl.height, 32);
        mfxU8 blockSrc[32 * 32];
        mfxU8 blockRef[32 * 32];

        for (mfxI32 yBlk = 0; yBlk < numBlocksVer; yBlk++, dist32x32 += 16 * numBlocksHor) {
            for (mfxI32 xBlk = 0; xBlk < numBlocksHor; xBlk++) {
                CopyBlock(srcData, xBlk * 32, yBlk * 32, 32, 32, blockSrc);

                mfxI16Pair hpelPoint = mv32x32Data[yBlk * numBlocksHor + xBlk];
                hpelPoint.x = (mfxI16)(hpelPoint.x + xBlk * 32 * 4);
                hpelPoint.y = (mfxI16)(hpelPoint.y + yBlk * 32 * 4);

                for (mfxU32 sadIdx = 0; sadIdx < 9; sadIdx++) {
                    mfxI16 dx = (mfxI16)(sadIdx % 3 - 1);
                    mfxI16 dy = (mfxI16)(sadIdx / 3 - 1);
                    mfxI16Pair qpelPoint = { hpelPoint.x + dx, hpelPoint.y + dy };

                    InterpolateBlock(refData, qpelPoint, 32, 32, blockRef);
                    dist32x32[16 * xBlk + sadIdx] = CalcSad(blockSrc, blockRef, 32, 32);
                }
                for (mfxU32 sadIdx = 10; sadIdx < 16; sadIdx++)
                    dist32x32[16 * xBlk + sadIdx] = 0;
            }
        }

        return PASSED;
    }
}
