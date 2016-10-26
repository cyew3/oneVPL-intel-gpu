//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "assert.h"
#include "stdio.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_h265_cmcode_isa.h"

#ifdef CMRT_EMU
extern "C" {
void Me1xAndInterpolateFrame(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_DIST16x16, // me1x params
                             SurfaceIndex SURF_DIST16x8, SurfaceIndex SURF_DIST8x16, SurfaceIndex SURF_DIST8x8,
                             SurfaceIndex SURF_DIST8x4, SurfaceIndex SURF_DIST4x8, SurfaceIndex SURF_MV16x16,
                             SurfaceIndex SURF_MV16x8, SurfaceIndex SURF_MV8x16, SurfaceIndex SURF_MV8x8,
                             SurfaceIndex SURF_MV8x4, SurfaceIndex SURF_MV4x8,
                             SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ, // interpolation params
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG);
void InterpolateFrame(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                      SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG);
void MeP16(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_DIST16x16,
           SurfaceIndex SURF_DIST16x8, SurfaceIndex SURF_DIST8x16, SurfaceIndex SURF_DIST8x8/*,
           SurfaceIndex SURF_DIST8x4, SurfaceIndex SURF_DIST4x8*/, SurfaceIndex SURF_MV16x16,
           SurfaceIndex SURF_MV16x8, SurfaceIndex SURF_MV8x16, SurfaceIndex SURF_MV8x8/*,
           SurfaceIndex SURF_MV8x4, SurfaceIndex SURF_MV4x8*/);
};
#endif //CMRT_EMU

struct VmeSearchPath // sizeof=58
{
    mfxU8 sp[56];
    mfxU8 maxNumSu;
    mfxU8 lenSp;
};

struct MeControl // sizeof=64
{
    VmeSearchPath searchPath;
    mfxU8  reserved[2];
    mfxU16 width;
    mfxU16 height;
};

const unsigned char DIAMOND[56] =
{
    0x0F,0xF1,0x0F,0x12,//5
    0x0D,0xE2,0x22,0x1E,//9
    0x10,0xFF,0xE2,0x20,//13
    0xFC,0x06,0xDD,//16
    0x2E,0xF1,0x3F,0xD3,0x11,0x3D,0xF3,0x1F,//24
    0xEB,0xF1,0xF1,0xF1,//28
    0x4E,0x11,0x12,0xF2,0xF1,//33
    0xE0,0xFF,0xFF,0x0D,0x1F,0x1F,//39
    0x20,0x11,0xCF,0xF1,0x05,0x11,//45
    0x00,0x00,0x00,0x00,0x00,0x00,//51
};


class CmRuntimeError : public std::exception
{
public:
    CmRuntimeError() : std::exception() { assert(!"CmRuntimeError"); }
};


template <class T>
struct Surface2DUp
{
    T *cpuMem;
    CmSurface2DUP *gpuMem;
    unsigned int pitch;
    int width;
    int height;
};


template <class T>
Surface2DUp<T> CreateCmSurface2DUp(CmDevice *device, int numElemInRow, int numRows)
{
    Surface2DUp<T> surface = {};
    unsigned size = 0;
    unsigned numBytesInRow = numElemInRow * sizeof(T);
    int res = CM_SUCCESS;
    if ((res = device->GetSurface2DInfo(numBytesInRow, numRows, CM_SURFACE_FORMAT_P8, surface.pitch, size)) != CM_SUCCESS)
        throw CmRuntimeError();
    surface.cpuMem = static_cast<T *>(CM_ALIGNED_MALLOC(size, 0x1000)); // 4K aligned
    if ((res = device->CreateSurface2DUP(numBytesInRow, numRows, CM_SURFACE_FORMAT_P8, surface.cpuMem, surface.gpuMem)) != CM_SUCCESS)
        throw CmRuntimeError();
    surface.width = numElemInRow;
    surface.height = numRows;
    return surface;
}

template <class T>
void DestroyCmSurface2DUp(CmDevice *device, Surface2DUp<T> &surface)
{
    CM_ALIGNED_FREE(surface.cpuMem);
    device->DestroySurface2DUP(surface.gpuMem);
    surface = Surface2DUp<T>();
}

template <class T>
SurfaceIndex *GetIndex(T *cmresource)
{
    SurfaceIndex *index = 0;
    int res = cmresource->GetIndex(index);
    if (res != CM_SUCCESS)
        throw CmRuntimeError();
    return index;
}


typedef Surface2DUp<unsigned char> SurfacePic;
typedef Surface2DUp<mfxI16Pair> SurfaceMv;
typedef Surface2DUp<unsigned int> SurfaceDist;

namespace {
int RunTst(CmDevice *device, unsigned char *src, unsigned char *ref, MeControl &control,
           SurfaceDist &dist16x16, SurfaceDist &dist16x8, SurfaceDist &dist8x16, SurfaceDist &dist8x8,
           SurfaceDist &dist8x4, SurfaceDist &dist4x8, SurfaceMv &mv16x16, SurfaceMv &mv16x8,
           SurfaceMv &mv8x16, SurfaceMv &mv8x8, SurfaceMv &mv8x4, SurfaceMv &mv4x8,
           SurfacePic &refH, SurfacePic &refV, SurfacePic &refD);

int RunRef(CmDevice *device, unsigned char *src, unsigned char *ref, MeControl &control,
           SurfaceDist &dist16x16, SurfaceDist &dist16x8, SurfaceDist &dist8x16, SurfaceDist &dist8x8,
           SurfaceDist &dist8x4, SurfaceDist &dist4x8, SurfaceMv &mv16x16, SurfaceMv &mv16x8,
           SurfaceMv &mv8x16, SurfaceMv &mv8x8, SurfaceMv &mv8x4, SurfaceMv &mv4x8,
           SurfacePic &refH, SurfacePic &refV, SurfacePic &refD);
};


bool operator !=(mfxI16Pair const l, mfxI16Pair const r) { return l.x != r.x || l.y != r.y; }

template <class T>
int Compare(const Surface2DUp<T> &tst, const Surface2DUp<T> &ref, const char *dataName)
{
    if (tst.height != ref.height || tst.width != ref.width)
        printf("different form factor (%d, %d) != (%d, %d)... ", tst.height, tst.width, ref.height, ref.width);
    const T *dataTst = tst.cpuMem;
    const T *dataRef = ref.cpuMem;
    for (int y = 0; y < tst.height; y++) {
        for (int x = 0; x < tst.width; x++) {
            if (dataTst[x] != dataRef[x]) {
                printf("different value (%d != %d) at surface %s (%.1f, %.1f)... ", dataTst[x], dataRef[x], dataName, x, y);
                return FAILED;
            }
        }
        dataTst = (T*)((char *)dataTst + tst.pitch);
        dataRef = (T*)((char *)dataRef + ref.pitch);
    }

    return PASSED;
}

int TestMe1xAndInterpolateFrame()
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    mfxI32 cmerr = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(cmerr);

    auto mv16x16Tst = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH / 16, HEIGHT / 16);
    auto mv16x16Ref = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH / 16, HEIGHT / 16);
    auto mv16x8Tst  = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH / 16, HEIGHT /  8);
    auto mv16x8Ref  = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH / 16, HEIGHT /  8);
    auto mv8x16Tst  = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  8, HEIGHT / 16);
    auto mv8x16Ref  = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  8, HEIGHT / 16);
    auto mv8x8Tst   = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  8, HEIGHT /  8);
    auto mv8x8Ref   = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  8, HEIGHT /  8);
    auto mv8x4Tst   = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  8, HEIGHT /  4);
    auto mv8x4Ref   = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  8, HEIGHT /  4);
    auto mv4x8Tst   = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  4, HEIGHT /  8);
    auto mv4x8Ref   = CreateCmSurface2DUp<mfxI16Pair>(device, WIDTH /  4, HEIGHT /  8);

    auto dist16x16Tst = CreateCmSurface2DUp<unsigned int>(device, WIDTH / 16, HEIGHT / 16);
    auto dist16x16Ref = CreateCmSurface2DUp<unsigned int>(device, WIDTH / 16, HEIGHT / 16);
    auto dist16x8Tst  = CreateCmSurface2DUp<unsigned int>(device, WIDTH / 16, HEIGHT /  8);
    auto dist16x8Ref  = CreateCmSurface2DUp<unsigned int>(device, WIDTH / 16, HEIGHT /  8);
    auto dist8x16Tst  = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  8, HEIGHT / 16);
    auto dist8x16Ref  = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  8, HEIGHT / 16);
    auto dist8x8Tst   = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  8, HEIGHT /  8);
    auto dist8x8Ref   = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  8, HEIGHT /  8);
    auto dist8x4Tst   = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  8, HEIGHT /  4);
    auto dist8x4Ref   = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  8, HEIGHT /  4);
    auto dist4x8Tst   = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  4, HEIGHT /  8);
    auto dist4x8Ref   = CreateCmSurface2DUp<unsigned int>(device, WIDTH /  4, HEIGHT /  8);

    auto refHTst = CreateCmSurface2DUp<unsigned char>(device, WIDTHB, HEIGHTB);
    auto refHRef = CreateCmSurface2DUp<unsigned char>(device, WIDTHB, HEIGHTB);
    auto refVTst = CreateCmSurface2DUp<unsigned char>(device, WIDTHB, HEIGHTB);
    auto refVRef = CreateCmSurface2DUp<unsigned char>(device, WIDTHB, HEIGHTB);
    auto refDTst = CreateCmSurface2DUp<unsigned char>(device, WIDTHB, HEIGHTB);
    auto refDRef = CreateCmSurface2DUp<unsigned char>(device, WIDTHB, HEIGHTB);

    auto src = new unsigned char[WIDTH * HEIGHT];
    auto ref = new unsigned char[WIDTH * HEIGHT];

    MeControl control = {};
    memcpy(control.searchPath.sp, DIAMOND, sizeof(control.searchPath.sp));
    control.searchPath.lenSp = 16;
    control.searchPath.maxNumSu = 57;
    control.width = WIDTH;
    control.height = HEIGHT;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return FAILED;
    if (fread(ref, 1, WIDTH * HEIGHT, f) != WIDTH * HEIGHT) // read luma 1 frame
        return FAILED;
    if (fseek(f, WIDTH * HEIGHT / 2, SEEK_CUR) != 0) // skip chroma
        return FAILED;
    if (fread(src, 1, WIDTH * HEIGHT, f) != WIDTH * HEIGHT) // read luma 1 frame
        return FAILED;
    fclose(f);

    mfxI32 res = PASSED;
    res = RunTst(device, src, ref, control,
                 dist16x16Tst, dist16x8Tst, dist8x16Tst, dist8x8Tst, dist8x4Tst, dist4x8Tst,
                 mv16x16Tst, mv16x8Tst, mv8x16Tst, mv8x8Tst, mv8x4Tst, mv4x8Tst,
                 refHTst, refVTst, refDTst);
    CHECK_ERR(res);

    res = RunRef(device, src, ref, control,
                 dist16x16Ref, dist16x8Ref, dist8x16Ref, dist8x8Ref, dist8x4Ref, dist4x8Ref,
                 mv16x16Ref, mv16x8Ref, mv8x16Ref, mv8x8Ref, mv8x4Ref, mv4x8Ref,
                 refHRef, refVRef, refDRef);
    CHECK_ERR(res);

    #define COMPARE(DATATYPE) Compare(DATATYPE##Tst, DATATYPE##Ref, #DATATYPE);
    res = COMPARE(dist16x16);
    CHECK_ERR(res);
    res = COMPARE(dist16x8);
    CHECK_ERR(res);
    res = COMPARE(dist8x16);
    CHECK_ERR(res);
    res = COMPARE(dist8x8);
    CHECK_ERR(res);
    res = COMPARE(dist8x4);
    CHECK_ERR(res);
    res = COMPARE(dist4x8);
    CHECK_ERR(res);
    res = COMPARE(mv16x16);
    CHECK_ERR(res);
    res = COMPARE(mv16x8);
    CHECK_ERR(res);
    res = COMPARE(mv8x16);
    CHECK_ERR(res);
    res = COMPARE(mv8x8);
    CHECK_ERR(res);
    res = COMPARE(mv8x4);
    CHECK_ERR(res);
    res = COMPARE(mv4x8);
    CHECK_ERR(res);
    res = COMPARE(refH);
    CHECK_ERR(res);
    res = COMPARE(refV);
    CHECK_ERR(res);
    res = COMPARE(refD);
    CHECK_ERR(res);

    DestroyCmSurface2DUp(device, mv16x16Tst);
    DestroyCmSurface2DUp(device, mv16x16Ref);
    DestroyCmSurface2DUp(device, mv16x8Tst);
    DestroyCmSurface2DUp(device, mv16x8Ref);
    DestroyCmSurface2DUp(device, mv8x16Tst);
    DestroyCmSurface2DUp(device, mv8x16Ref);
    DestroyCmSurface2DUp(device, mv8x8Tst);
    DestroyCmSurface2DUp(device, mv8x8Ref);
    DestroyCmSurface2DUp(device, mv8x4Tst);
    DestroyCmSurface2DUp(device, mv8x4Ref);
    DestroyCmSurface2DUp(device, mv4x8Tst);
    DestroyCmSurface2DUp(device, mv4x8Ref);
    DestroyCmSurface2DUp(device, dist16x16Tst);
    DestroyCmSurface2DUp(device, dist16x16Ref);
    DestroyCmSurface2DUp(device, dist16x8Tst);
    DestroyCmSurface2DUp(device, dist16x8Ref);
    DestroyCmSurface2DUp(device, dist8x16Tst);
    DestroyCmSurface2DUp(device, dist8x16Ref);
    DestroyCmSurface2DUp(device, dist8x8Tst);
    DestroyCmSurface2DUp(device, dist8x8Ref);
    DestroyCmSurface2DUp(device, dist8x4Tst);
    DestroyCmSurface2DUp(device, dist8x4Ref);
    DestroyCmSurface2DUp(device, dist4x8Tst);
    DestroyCmSurface2DUp(device, dist4x8Ref);
    DestroyCmSurface2DUp(device, refHTst);
    DestroyCmSurface2DUp(device, refHRef);
    DestroyCmSurface2DUp(device, refVTst);
    DestroyCmSurface2DUp(device, refVRef);
    DestroyCmSurface2DUp(device, refDTst);
    DestroyCmSurface2DUp(device, refDRef);
    delete [] src;
    delete [] ref;
    ::DestroyCmDevice(device);

    return PASSED;
}

namespace {
int RunTst(CmDevice *device, unsigned char *src, unsigned char *ref, MeControl &control,
           SurfaceDist &dist16x16, SurfaceDist &dist16x8, SurfaceDist &dist8x16, SurfaceDist &dist8x8,
           SurfaceDist &dist8x4, SurfaceDist &dist4x8, SurfaceMv &mv16x16, SurfaceMv &mv16x8,
           SurfaceMv &mv8x16, SurfaceMv &mv8x8, SurfaceMv &mv8x4, SurfaceMv &mv4x8,
           SurfacePic &refH, SurfacePic &refV, SurfacePic &refD)
{
    CmProgram *program = 0;
    mfxI32 res = device->LoadProgram((void *)genx_h265_cmcode, sizeof(genx_h265_cmcode), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(Me1xAndInterpolateFrame), kernel);
    CHECK_CM_ERR(res);

    CmSurface2D *inSrc = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, inSrc);
    CHECK_CM_ERR(res);
    res = inSrc->WriteSurface(src, NULL);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, inRef);
    CHECK_CM_ERR(res);
    res = inRef->WriteSurface(ref, NULL);
    CHECK_CM_ERR(res);

    CmBuffer *inControl = 0;
    res = device->CreateBuffer(sizeof(MeControl), inControl);
    CHECK_CM_ERR(res);
    res = inControl->WriteSurface((unsigned char *)&control, NULL);
    CHECK_CM_ERR(res);

    SurfaceIndex *vme75Refs = 0;
    res = device->CreateVmeSurfaceG7_5(inSrc, &inRef, 0, 1, 0, vme75Refs);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), GetIndex(inControl));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), vme75Refs);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(SurfaceIndex), GetIndex(dist16x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(SurfaceIndex), GetIndex(dist16x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(4, sizeof(SurfaceIndex), GetIndex(dist8x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(5, sizeof(SurfaceIndex), GetIndex(dist8x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(6, sizeof(SurfaceIndex), GetIndex(dist8x4.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(7, sizeof(SurfaceIndex), GetIndex(dist4x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(8, sizeof(SurfaceIndex), GetIndex(mv16x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(9, sizeof(SurfaceIndex), GetIndex(mv16x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(10, sizeof(SurfaceIndex), GetIndex(mv8x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(11, sizeof(SurfaceIndex), GetIndex(mv8x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(12, sizeof(SurfaceIndex), GetIndex(mv8x4.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(13, sizeof(SurfaceIndex), GetIndex(mv4x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(14, sizeof(SurfaceIndex), GetIndex(inRef));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(15, sizeof(SurfaceIndex), GetIndex(refH.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(16, sizeof(SurfaceIndex), GetIndex(refV.gpuMem));
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(17, sizeof(SurfaceIndex), GetIndex(refD.gpuMem));
    CHECK_CM_ERR(res);

    mfxI32 numBlocksHor = (WIDTH + 15) / 16;
    mfxI32 numBlocksVer = (HEIGHT + 15) / 16;
    res = kernel->SetThreadCount(numBlocksHor * numBlocksVer);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(numBlocksHor, numBlocksVer, threadSpace);
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

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    mfxU64 time;
    e->GetExecutionTime(time);
    printf("TIME=%.3f ms ", time / 1000000.0);

    queue->DestroyEvent(e);

    device->DestroySurface(inRef);
    device->DestroySurface(inSrc);
    device->DestroySurface(inControl);
    device->DestroyVmeSurfaceG7_5(vme75Refs);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);

    return PASSED;
}


int RunRef(CmDevice *device, unsigned char *src, unsigned char *ref, MeControl &control,
           SurfaceDist &dist16x16, SurfaceDist &dist16x8, SurfaceDist &dist8x16, SurfaceDist &dist8x8,
           SurfaceDist &dist8x4, SurfaceDist &dist4x8, SurfaceMv &mv16x16, SurfaceMv &mv16x8,
           SurfaceMv &mv8x16, SurfaceMv &mv8x8, SurfaceMv &mv8x4, SurfaceMv &mv4x8,
           SurfacePic &refH, SurfacePic &refV, SurfacePic &refD)
{
    CmProgram *program = 0;
    mfxI32 res = device->LoadProgram((void *)genx_h265_cmcode, sizeof(genx_h265_cmcode), program);
    CHECK_CM_ERR(res);

    CmKernel *kernelMe = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(MeP16), kernelMe);
    CHECK_CM_ERR(res);

    CmKernel *kernelInterp = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(InterpolateFrame), kernelInterp);
    CHECK_CM_ERR(res);

    CmSurface2D *inSrc = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, inSrc);
    CHECK_CM_ERR(res);
    res = inSrc->WriteSurface(src, NULL);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, inRef);
    CHECK_CM_ERR(res);
    res = inRef->WriteSurface(ref, NULL);
    CHECK_CM_ERR(res);

    CmBuffer *inControl = 0;
    res = device->CreateBuffer(sizeof(MeControl), inControl);
    CHECK_CM_ERR(res);
    res = inControl->WriteSurface((unsigned char *)&control, NULL);
    CHECK_CM_ERR(res);

    SurfaceIndex *vme75Refs = 0;
    res = device->CreateVmeSurfaceG7_5(inSrc, &inRef, 0, 1, 0, vme75Refs);
    CHECK_CM_ERR(res);

    res = kernelMe->SetKernelArg(0, sizeof(SurfaceIndex), GetIndex(inControl));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(1, sizeof(SurfaceIndex), vme75Refs);
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(2, sizeof(SurfaceIndex), GetIndex(dist16x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(3, sizeof(SurfaceIndex), GetIndex(dist16x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(4, sizeof(SurfaceIndex), GetIndex(dist8x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(5, sizeof(SurfaceIndex), GetIndex(dist8x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(6, sizeof(SurfaceIndex), GetIndex(dist8x4.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(7, sizeof(SurfaceIndex), GetIndex(dist4x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(8, sizeof(SurfaceIndex), GetIndex(mv16x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(9, sizeof(SurfaceIndex), GetIndex(mv16x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(10, sizeof(SurfaceIndex), GetIndex(mv8x16.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(11, sizeof(SurfaceIndex), GetIndex(mv8x8.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(12, sizeof(SurfaceIndex), GetIndex(mv8x4.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelMe->SetKernelArg(13, sizeof(SurfaceIndex), GetIndex(mv4x8.gpuMem));
    CHECK_CM_ERR(res);
    
    res = kernelInterp->SetKernelArg(0, sizeof(SurfaceIndex), GetIndex(inRef));
    CHECK_CM_ERR(res);
    res = kernelInterp->SetKernelArg(1, sizeof(SurfaceIndex), GetIndex(refH.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelInterp->SetKernelArg(2, sizeof(SurfaceIndex), GetIndex(refV.gpuMem));
    CHECK_CM_ERR(res);
    res = kernelInterp->SetKernelArg(3, sizeof(SurfaceIndex), GetIndex(refD.gpuMem));
    CHECK_CM_ERR(res);

    mfxI32 numBlocksHor = (WIDTH + 15) / 16;
    mfxI32 numBlocksVer = (HEIGHT + 15) / 16;
    res = kernelMe->SetThreadCount(numBlocksHor * numBlocksVer);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(numBlocksHor, numBlocksVer, threadSpace);
    CHECK_CM_ERR(res);
    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);
    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernelMe);
    CHECK_CM_ERR(res);
    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);
    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);
    mfxU64 time;
    e->GetExecutionTime(time);
    printf("TIME=%.3f ms ", time / 1000000.0);
    queue->DestroyEvent(e);

    const mfxU16 BlockW = 8;
    const mfxU16 BlockH = 8;
    mfxU32 tsWidth = WIDTH / BlockW;
    mfxU32 tsHeight = HEIGHT / BlockH * 2;

    res = kernelInterp->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);
    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);
    task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernelInterp);
    CHECK_CM_ERR(res);
    queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);
    e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);
    time = 0;
    e->GetExecutionTime(time);
    printf("TIME=%.3f ms ", time / 1000000.0);
    queue->DestroyEvent(e);

    device->DestroySurface(inRef);
    device->DestroySurface(inSrc);
    device->DestroySurface(inControl);
    device->DestroyVmeSurfaceG7_5(vme75Refs);
    device->DestroyKernel(kernelMe);
    device->DestroyKernel(kernelInterp);
    device->DestroyProgram(program);

    return PASSED;
}

};