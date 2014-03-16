/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined (MFX_ENABLE_CM)

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

#include "mfx_h265_defs.h"
#include "mfx_h265_enc_cm_defs.h"
#include "mfx_h265_enc_cm.h"
#include "genx_h265_cmcode_proto.h"
#include "genx_h265_cmcode_isa.h"

namespace H265Enc {

template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
}

template<class T> inline void Zero(T & obj)
{
    memset(&obj, 0, sizeof(obj));
}

const char   ME_PROGRAM_NAME[] = "genx_h265_cmcode.isa";

const mfxU32 SEARCHPATHSIZE    = 56;
const mfxU32 BATCHBUFFER_END   = 0x5000000;

CmProgram * ReadProgram(CmDevice * device, std::istream & is)
{
    CmProgram * program = 0;
    char * code = 0;
    std::streamsize size = 0;
    device, is;

#ifndef CMRT_EMU
    is.seekg(0, std::ios::end);
    size = is.tellg();
    is.seekg(0, std::ios::beg);
    if (size == 0)
        throw CmRuntimeError();

    std::vector<char> buf((int)size);
    code = &buf[0];
    is.read(code, size);
    if (is.gcount() != size)
        throw CmRuntimeError();
#endif // CMRT_EMU

    if (device->LoadProgram(code, mfxU32(size), program, "nojitter") != CM_SUCCESS)
        throw CmRuntimeError();

    return program;
}

CmProgram * ReadProgram(CmDevice * device, char const * filename)
{
    std::fstream f;
    filename;
#ifndef CMRT_EMU
    f.open(filename, std::ios::binary | std::ios::in);
    if (!f)
        throw CmRuntimeError();
#endif // CMRT_EMU
    return ReadProgram(device, f);
}

CmProgram * ReadProgram(CmDevice * device, const unsigned char* buffer, int len)
{
    int result = CM_SUCCESS;
    CmProgram * program = 0;

    if ((result = device->LoadProgram((void*)buffer, len, program, "nojitter")) != CM_SUCCESS)
        throw CmRuntimeError();

    return program;
}

CmKernel * CreateKernel(CmDevice * device, CmProgram * program, char const * name, void * funcptr)
{
    int result = CM_SUCCESS;
    CmKernel * kernel = 0;

    if ((result = ::CreateKernel(device, program, name, funcptr, kernel)) != CM_SUCCESS)
        throw CmRuntimeError();

    return kernel;
}

void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel, mfxU32 tsWidth,
                   mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern = CM_NONE_DEPENDENCY)
{
    int result = CM_SUCCESS;

    if ((result = kernel->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmThreadSpace * cmThreadSpace = 0;
    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    cmThreadSpace->SelectThreadDependencyPattern(tsPattern);

    CmTask * cmTask = 0;
    if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = cmTask->AddKernel(kernel)) != CM_SUCCESS)
        throw CmRuntimeError();

    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    device->DestroyThreadSpace(cmThreadSpace);
    device->DestroyTask(cmTask);
}

void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel0, CmKernel *kernel1, mfxU32 tsWidth,
                   mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern = CM_NONE_DEPENDENCY)
{
    int result = CM_SUCCESS;

    if ((result = kernel0->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = kernel1->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmThreadSpace * cmThreadSpace = 0;
    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    cmThreadSpace->SelectThreadDependencyPattern(tsPattern);

    CmTask * cmTask = 0;
    if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = cmTask->AddKernel(kernel0)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = cmTask->AddKernel(kernel1)) != CM_SUCCESS)
        throw CmRuntimeError();

    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    device->DestroyThreadSpace(cmThreadSpace);
    device->DestroyTask(cmTask);
}

void EnqueueCopyCPUToGPU(CmQueue *queue, CmSurface2D *surface, const void *sysMem, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyCPUToGPU(surface, (const mfxU8 *)sysMem, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void EnqueueCopyGPUToCPU(CmQueue *queue, CmSurface2D *surface, void *sysMem, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyGPUToCPU(surface, (mfxU8 *)sysMem, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void EnqueueCopyCPUToGPUStride(CmQueue *queue, CmSurface2D *surface, const void *sysMem,
                               mfxU32 pitch, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyCPUToGPUStride(surface, (const mfxU8 *)sysMem, pitch, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void EnqueueCopyGPUToCPUStride(CmQueue *queue, CmSurface2D *surface, void *sysMem,
                               mfxU32 pitch, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyGPUToCPUStride(surface, (mfxU8 *)sysMem, pitch, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Read(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->ReadSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Write(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->WriteSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    mfxU32 versionPlaceholder = 0;
    if (version == 0)
        version = &versionPlaceholder;

    CmDevice * device = 0;

    int result = CM_SUCCESS;
#if defined(_WIN32) || defined(_WIN64)
        if ((result = ::CreateCmDevice(device, *version, (IDirect3DDeviceManager9 *)0)) != CM_SUCCESS)
            return 0;
#endif  // #if defined(_WIN32) || defined(_WIN64)
#if defined(LINUX32) || defined(LINUX64)
        VADisplay display = 0;
        if ((result = ::CreateCmDevice(device, *version, display)) != CM_SUCCESS)
            return 0;
#endif // #if defined(LINUX32) || defined(LINUX64)

    return device;
}

CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    CmDevice * device = TryCreateCmDevicePtr(core, version);
    if (device == 0)
        throw CmRuntimeError();
    return device;
}


CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size)
{
    CmBuffer * buffer;
    int result = device->CreateBuffer(size, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem)
{
    CmBufferUP * buffer;
    int result = device->CreateBufferUP(size, mem, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}

CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (result = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT(fourcc), cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}

CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem)
{
    int result = CM_SUCCESS;
    CmSurface2DUP * cmSurface = 0;
    if (device && (result = device->CreateSurface2DUP(width, height, CM_SURFACE_FORMAT(fourcc), mem, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


SurfaceIndex * CreateVmeSurfaceG75(
    CmDevice *      device,
    CmSurface2D *   source,
    CmSurface2D **  fwdRefs,
    CmSurface2D **  bwdRefs,
    mfxU32          numFwdRefs,
    mfxU32          numBwdRefs)
{
    if (numFwdRefs == 0)
        fwdRefs = 0;
    if (numBwdRefs == 0)
        bwdRefs = 0;

    int result = CM_SUCCESS;
    SurfaceIndex * index;
    if ((result = device->CreateVmeSurfaceG7_5(source, fwdRefs, bwdRefs, numFwdRefs, numBwdRefs, index)) != CM_SUCCESS)
        throw CmRuntimeError();
    return index;
}

template <class T>
void CreateCmSurface2DUp(CmDevice *device, Ipp32u numElemInRow, Ipp32u numRows, CM_SURFACE_FORMAT format,
                         T *&surfaceCpu, CmSurface2DUP *& surfaceGpu, Ipp32u &pitch)
{
    Ipp32u size = 0;
    Ipp32u numBytesInRow = numElemInRow * sizeof(T);
    int res = CM_SUCCESS;
    if ((res = device->GetSurface2DInfo(numBytesInRow, numRows, format, pitch, size)) != CM_SUCCESS)
        throw CmRuntimeError();
    surfaceCpu = static_cast<T *>(CM_ALIGNED_MALLOC(size, 0x1000)); // 4K aligned
    surfaceGpu = CreateSurface(device, numBytesInRow, numRows, format, surfaceCpu);
}

template <class T>
void CreateCmBufferUp(CmDevice *device, Ipp32u numElems, T *&surfaceCpu, CmBufferUP *& surfaceGpu)
{
    Ipp32u numBytes = numElems * sizeof(T);
    surfaceCpu = static_cast<T *>(CM_ALIGNED_MALLOC(numBytes, 0x1000)); // 4K aligned
    surfaceGpu = CreateBuffer(device, numBytes, surfaceCpu);
}

void DestroyCmSurface2DUp(CmDevice *device, void *surfaceCpu, CmSurface2DUP *surfaceGpu)
{
    device->DestroySurface2DUP(surfaceGpu);
    CM_ALIGNED_FREE(surfaceCpu);
}

void DestroyCmBufferUp(CmDevice *device, void *bufferCpu, CmBufferUP *bufferGpu)
{
    device->DestroyBufferUP(bufferGpu);
    CM_ALIGNED_FREE(bufferCpu);
}


namespace
{
    void SetSearchPath(VmeSearchPath *spath)
    {
        memcpy(spath->sp, Diamond, sizeof(spath->sp));
        spath->lenSp = 16;
        spath->maxNumSu = 57;
    }

    mfxU32 SetSearchPath(
        mfxVMEIMEIn & spath,
        mfxU32        frameType,
        mfxU32        meMethod)
    {
        mfxU32 maxNumSU = SEARCHPATHSIZE + 1;

        if (frameType & MFX_FRAMETYPE_P)
        {
            switch (meMethod)
            {
            case 2:
                memcpy(&spath.IMESearchPath0to31[0], &SingleSU[0], 32*sizeof(mfxU8));
                maxNumSU = 1;
                break;
            case 3:
                memcpy(&spath.IMESearchPath0to31[0], &RasterScan_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &RasterScan_48x40[32], 24*sizeof(mfxU8));
                break;
            case 4:
                memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
                break;
            case 5:
                memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
                maxNumSU = 16;
                break;
            case 6:
            default:
                memcpy(&spath.IMESearchPath0to31[0], &Diamond[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &Diamond[32], 24*sizeof(mfxU8));
                break;
            }
        }
        else
        {
            if (meMethod == 6)
            {
                memcpy(&spath.IMESearchPath0to31[0], &Diamond[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &Diamond[32], 24*sizeof(mfxU8));
            }
            else
            {
                memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
            }
        }

        return maxNumSU;
    }

};


bool cmResurcesAllocated = false;
CmDevice * device = 0;
CmQueue * queue = 0;
CmSurface2DUP * mbIntraDist[2] = {};
CmBufferUP * mbIntraGrad4x4[2] = {};
CmBufferUP * mbIntraGrad8x8[2] = {};
Ipp32u intraPitch[2] = {};
CmMbIntraDist * cmMbIntraDist[2] = {};
CmMbIntraGrad * cmMbIntraGrad4x4[2] = {};
CmMbIntraGrad * cmMbIntraGrad8x8[2] = {};

CmSurface2DUP * distGpu[2][MAX_NUM_REF_IDX][PU_MAX] = {};
CmSurface2DUP * mvGpu[2][MAX_NUM_REF_IDX][PU_MAX] = {};
mfxU32 * distCpu[2][MAX_NUM_REF_IDX][PU_MAX] = {0};
mfxI16Pair * mvCpu[2][MAX_NUM_REF_IDX][PU_MAX] = {0};
Ipp32u pitchDist[PU_MAX] = {};
Ipp32u pitchMv[PU_MAX] = {};

CmSurface2D * raw2x[2] = {};
CmSurface2D * raw[2] = {};
CmSurface2D * fwdRef2xCur = 0;
CmSurface2D * fwdRefCur = 0;
CmSurface2D * fwdRef2xNext = 0;
CmSurface2D * fwdRefNext = 0;
CmSurface2D * IntraDist = 0;

CmProgram * program = 0;
CmKernel * kernelMeIntra = 0;
CmKernel * kernelGradient = 0;
CmKernel * kernelMe = 0;
CmKernel * kernelMe2x = 0;
CmKernel * kernelRefine32x32 = 0;
CmKernel * kernelRefine32x16 = 0;
CmKernel * kernelRefine16x32 = 0;
CmKernel * kernelRefine = 0;
CmKernel * kernelDownSampleSrc = 0;
CmKernel * kernelDownSampleFwd = 0;
CmBuffer * curbe = 0;
CmBuffer * me1xControl = 0;
CmBuffer * me2xControl = 0;
mfxU32 width = 0;
mfxU32 height = 0;
mfxU32 width2x = 0;
mfxU32 height2x = 0;
mfxU32 frameOrder = 0;
Ipp32s maxNumRefs = 0;

CmEvent * lastEvent[2] = {};
Ipp32s cmCurIdx = 0;
Ipp32s cmNextIdx = 0;

Ipp32s cmMvW[PU_MAX] = {};
Ipp32s cmMvH[PU_MAX] = {};

#if defined(_WIN32) || defined(_WIN64)

struct Timers
{
    struct Timer {
        void Add(mfxU64 t) { total += t; min = callCount > 0 ? IPP_MIN(min, t) : t; callCount++; }
        mfxU64 total;
        mfxU64 min;
        mfxU32 callCount;
        mfxU64 freq;
    } me1x, me2x, refine32x32, refine32x16, refine16x32, refine, dsSrc, dsFwd,
        writeSrc, writeFwd, readDist32x32, readDist32x16, readDist16x32, readMb1x, readMb2x, runVme, processMv;
} TIMERS = {};

mfxU64 gettick() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
mfxU64 getfreq() {
    LARGE_INTEGER t;
    QueryPerformanceFrequency(&t);
    return t.QuadPart;
}

mfxU64 GetCmTime(CmEvent * e) {
    mfxU64 t = 0;
    e->GetExecutionTime(t);
    return t;
}

void printTime(char * name, Timers::Timer const & timer)
{
    double totTimeMs = timer.total / double(timer.freq) * 1000.0;
    double minTimeMs = timer.min   / double(timer.freq) * 1000.0;
    double avgTimeMs = totTimeMs / timer.callCount;
    printf("%s (called %3u times) time min=%.3f avg=%.3f total=%.3f ms\n", name, timer.callCount, minTimeMs, avgTimeMs, totTimeMs);
}

typedef std::map<std::pair<int, int>, double> Map;
Map interpolationTime;

//struct PostMortemPrinter {~PostMortemPrinter() {
void PrintTimes()
{
    TIMERS.runVme.freq = TIMERS.processMv.freq = TIMERS.readMb1x.freq = TIMERS.readMb2x.freq = getfreq();
    TIMERS.writeSrc.freq = TIMERS.writeFwd.freq = TIMERS.readDist32x32.freq = TIMERS.readDist32x16.freq = TIMERS.readDist16x32.freq = TIMERS.dsSrc.freq =
        TIMERS.dsFwd.freq = TIMERS.me1x.freq = TIMERS.me2x.freq = TIMERS.refine32x32.freq = TIMERS.refine32x16.freq = TIMERS.refine16x32.freq = TIMERS.refine.freq = 1000000000;

    printTime("RunVme           ", TIMERS.runVme);
    printTime("Write src        ", TIMERS.writeSrc);
    printTime("Write fwd        ", TIMERS.writeFwd);
    printTime("read dist32x32   ", TIMERS.readDist32x32);
    printTime("read dist32x16   ", TIMERS.readDist32x16);
    printTime("read dist16x32   ", TIMERS.readDist16x32);
    printTime("Downscale src    ", TIMERS.dsSrc);
    printTime("Downscale fwd    ", TIMERS.dsFwd);
    printTime("ME 1x            ", TIMERS.me1x);
    printTime("ME 2x            ", TIMERS.me2x);
    printTime("Refine32x32      ", TIMERS.refine32x32);
    printTime("Refine32x16      ", TIMERS.refine32x16);
    printTime("Refine16x32      ", TIMERS.refine16x32);
    printTime("Read Mb 1x       ", TIMERS.readMb1x);
    printTime("Read Mb 2x       ", TIMERS.readMb2x);
    printTime("Process Mv       ", TIMERS.processMv);

    double sum = 0.0;
    for (Map::iterator i = interpolationTime.begin(); i != interpolationTime.end(); ++i)
        printf("Interpolate_%02dx%02d time %.3f\n", i->first.first, i->first.second, i->second), sum += i->second;
    printf("Interpolate_AVERAGE time %.3f\n", sum/interpolationTime.size());
}
//}} g_postMortemPrinter;

#endif // #if defined(_WIN32) || defined(_WIN64)

void FreeCmResources()
{
    if (cmResurcesAllocated) {
        device->DestroySurface(curbe);
        device->DestroySurface(me1xControl);
        device->DestroySurface(me2xControl);
        device->DestroyKernel(kernelMeIntra);
        device->DestroyKernel(kernelGradient);
        device->DestroyKernel(kernelMe);
        device->DestroyKernel(kernelMe2x);
        device->DestroyKernel(kernelRefine32x32);
        device->DestroyKernel(kernelRefine32x16);
        device->DestroyKernel(kernelRefine16x32);
        device->DestroyKernel(kernelRefine);
        device->DestroyKernel(kernelDownSampleSrc);
        device->DestroyKernel(kernelDownSampleFwd);

        for (Ipp32s i = 0; i < 2; i++) {
            DestroyCmSurface2DUp(device, cmMbIntraDist[i], mbIntraDist[i]);
            DestroyCmBufferUp(device, cmMbIntraGrad4x4[i], mbIntraGrad4x4[i]);
            DestroyCmBufferUp(device, cmMbIntraGrad8x8[i], mbIntraGrad8x8[i]);

            for (Ipp32s j = 0; j < maxNumRefs; j++) {
                for (Ipp32s k = PU32x32; k < PU_MAX; k++) {
                    DestroyCmSurface2DUp(device, distCpu[i][j][k], distGpu[i][j][k]);
                    DestroyCmSurface2DUp(device, mvCpu[i][j][k], mvGpu[i][j][k]);
                }
            }

            device->DestroySurface(raw[i]);
            device->DestroySurface(raw2x[i]);
        }

        device->DestroySurface(fwdRefCur);
        device->DestroySurface(fwdRef2xCur);
        device->DestroySurface(fwdRefNext);
        device->DestroySurface(fwdRef2xNext);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        cmResurcesAllocated = false;
    }
}

void AllocateCmResources(mfxU32 w, mfxU32 h, mfxU16 nRefs)
{
    if (cmResurcesAllocated)
        return;

    w = (w + 15) & ~15;
    h = (h + 15) & ~15;
    width2x = AlignValue(w / 2, 16);
    height2x = AlignValue(h / 2, 16);
    width = w;
    height = h;
    frameOrder = 0;
    maxNumRefs = nRefs;

    device = CreateCmDevicePtr(0);
    device->CreateQueue(queue);

    program = ReadProgram(device, genx_h265_cmcode, sizeof(genx_h265_cmcode)/sizeof(genx_h265_cmcode[0]));

    fwdRefCur    = CreateSurface(device, w, h, CM_SURFACE_FORMAT_NV12);
    fwdRef2xCur  = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
    fwdRefNext   = CreateSurface(device, w, h, CM_SURFACE_FORMAT_NV12);
    fwdRef2xNext = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);

    for (Ipp32u i = 0; i < 2; i++) {
        raw[i]   = CreateSurface(device, w, h, CM_SURFACE_FORMAT_NV12);
        raw2x[i] = CreateSurface(device, width2x, height2x, CM_SURFACE_FORMAT_NV12);
    }

    cmMvW[PU16x16] = w / 16; cmMvH[PU16x16] = h / 16;
    cmMvW[PU16x8 ] = w / 16; cmMvH[PU16x8 ] = h /  8;
    cmMvW[PU8x16 ] = w /  8; cmMvH[PU8x16 ] = h / 16;
    cmMvW[PU8x8  ] = w /  8; cmMvH[PU8x8  ] = h /  8;
    cmMvW[PU8x4  ] = w /  8; cmMvH[PU8x4  ] = h /  4;
    cmMvW[PU4x8  ] = w /  4; cmMvH[PU4x8  ] = h /  8;
    cmMvW[PU32x32] = width2x / 16; cmMvH[PU32x32] = height2x / 16;
    cmMvW[PU32x16] = width2x / 16; cmMvH[PU32x16] = height2x /  8;
    cmMvW[PU16x32] = width2x /  8; cmMvH[PU16x32] = height2x / 16;

    for (Ipp32u i = 0; i < 2; i++) {
        CreateCmSurface2DUp(device, w / 16, h / 16, CM_SURFACE_FORMAT_P8, cmMbIntraDist[i],
                            mbIntraDist[i], intraPitch[i]);
        CreateCmBufferUp(device, w * h / 16, cmMbIntraGrad4x4[i], mbIntraGrad4x4[i]);
        CreateCmBufferUp(device, w * h / 64, cmMbIntraGrad8x8[i], mbIntraGrad8x8[i]);

        for (Ipp32s j = 0; j < maxNumRefs; j++) {
            for (Ipp32u k = PU32x32; k <= PU16x32; k++)
                CreateCmSurface2DUp(device, cmMvW[k] * 16, cmMvH[k], CM_SURFACE_FORMAT_P8,
                                    distCpu[i][j][k], distGpu[i][j][k], pitchDist[k]);
            for (Ipp32u k = PU16x16; k < PU_MAX; k++)
                CreateCmSurface2DUp(device, cmMvW[k] * 1, cmMvH[k], CM_SURFACE_FORMAT_P8,
                                    distCpu[i][j][k], distGpu[i][j][k], pitchDist[k]);
            for (Ipp32u k = 0; k < PU_MAX; k++)
                CreateCmSurface2DUp(device, cmMvW[k], cmMvH[k], CM_SURFACE_FORMAT_P8,
                                    mvCpu[i][j][k], mvGpu[i][j][k], pitchMv[k]);
        }
    }

    kernelDownSampleSrc = CreateKernel(device, program, "DownSampleMB", (void *)DownSampleMB);
    kernelDownSampleFwd = CreateKernel(device, program, "DownSampleMB", (void *)DownSampleMB);
    kernelMeIntra = CreateKernel(device, program, "RawMeMB_P_Intra", (void *)RawMeMB_P_Intra);
    kernelGradient = CreateKernel(device, program, "AnalyzeGradient", (void *)AnalyzeGradient);
    kernelMe = CreateKernel(device, program, "RawMeMB_P", (void *)RawMeMB_P);
    kernelMe2x = CreateKernel(device, program, "MeP32", (void *)MeP32);
    kernelRefine32x32 = CreateKernel(device, program, "RefineMeP32x32", (void *)RefineMeP32x32);
    kernelRefine32x16 = CreateKernel(device, program, "RefineMeP32x16", (void *)RefineMeP32x16);
    kernelRefine16x32 = CreateKernel(device, program, "RefineMeP16x32", (void *)RefineMeP16x32);
    curbe   = CreateBuffer(device, sizeof(H265EncCURBEData));
    me1xControl = CreateBuffer(device, sizeof(Me2xControl));
    me2xControl = CreateBuffer(device, sizeof(Me2xControl));

    H265EncCURBEData curbeData = {};
    SetCurbeData(curbeData, MFX_FRAMETYPE_P, 26);

    { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "curbe->WriteSurface0");
        curbeData.PictureHeightMinusOne = height / 16 - 1;
        curbeData.SliceHeightMinusOne   = height / 16 - 1;
        curbeData.PictureWidth          = width  / 16;
        curbeData.Log2MvScaleFactor     = 0;
        curbeData.Log2MbScaleFactor     = 1;
        curbeData.SubMbPartMask         = 0x7e;
        curbeData.InterSAD              = 2;
        curbeData.IntraSAD              = 2;
        curbe->WriteSurface((mfxU8 *)&curbeData, NULL);
    }

    { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "me2xControl->WriteSurface");
        Me2xControl control;
        SetSearchPath(&control.searchPath);
        control.width = width;
        control.height = height;
        me1xControl->WriteSurface((mfxU8 *)&control, NULL);
        control.width = width2x;
        control.height = height2x;
        me2xControl->WriteSurface((mfxU8 *)&control, NULL);
    }

    cmResurcesAllocated = true;
    cmCurIdx = 1;
    cmNextIdx = 0;
}

void RunVmeCurr(H265VideoParam const &param, H265Frame *pFrameCur, H265Slice *pSliceCur,
                H265Frame ** refsCur)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RunVmeCurr");

    if (frameOrder == 0) {
        {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuRawCur");
            EnqueueCopyCPUToGPUStride(queue, raw[cmCurIdx], pFrameCur->y, pFrameCur->pitch_luma,
                                      lastEvent[cmCurIdx]);
        }

        if (param.intraAngModes == 3) {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelGradient");
            SetKernelArg(kernelGradient, raw[cmCurIdx], mbIntraGrad4x4[cmCurIdx],
                         mbIntraGrad8x8[cmCurIdx], width);
            EnqueueKernel(device, queue, kernelGradient, width / 8, height / 8, lastEvent[cmCurIdx]);
        }
    }
    else if (pFrameCur->m_PicCodType & MFX_FRAMETYPE_P) {
        CmSurface2DUP ** dist = distGpu[cmCurIdx][0];
        CmSurface2DUP ** mv = mvGpu[cmCurIdx][0];
        SurfaceIndex * refs2x = CreateVmeSurfaceG75(device, raw2x[cmCurIdx], &fwdRef2xCur, 0, 1, 0);
        SurfaceIndex * refs   = CreateVmeSurfaceG75(device, raw[cmCurIdx], &fwdRefCur, 0, 1, 0);
        {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuCurFwdRef");
            EnqueueCopyCPUToGPUStride(queue, fwdRefCur, refsCur[0]->y, refsCur[0]->pitch_luma,
                                      lastEvent[cmCurIdx]);
        }
        if (param.Log2MaxCUSize > 4) {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "DSCurFwdRef");
            SetKernelArg(kernelDownSampleFwd, fwdRefCur, fwdRef2xCur);
            EnqueueKernel(device, queue, kernelDownSampleFwd, width / 16, height / 16, lastEvent[cmCurIdx]);
        }
        { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelMe");
            SetKernelArg(kernelMe, me1xControl, *refs, dist[PU16x16], dist[PU16x8], dist[PU8x16],
                         dist[PU8x8], dist[PU8x4], dist[PU4x8], mv[PU16x16], mv[PU16x8],
                         mv[PU8x16], mv[PU8x8], mv[PU8x4], mv[PU4x8]);
            EnqueueKernel(device, queue, kernelMe, width / 16, height / 16, lastEvent[cmCurIdx]);
        }
        if (param.Log2MaxCUSize > 4) {
            { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelMe2x");
                SetKernelArg(kernelMe2x, me2xControl, *refs2x, mv[PU32x32], mv[PU32x16],
                             mv[PU16x32]);
                EnqueueKernel(device, queue, kernelMe2x, width2x / 16, height2x / 16,
                              lastEvent[cmCurIdx]);
            }
            { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelRefine32x32");
                SetKernelArg(kernelRefine32x32, dist[PU32x32], mv[PU32x32], raw[cmCurIdx],
                             fwdRefCur);
                EnqueueKernel(device, queue, kernelRefine32x32, width2x / 16, height2x / 16,
                              lastEvent[cmCurIdx]);
            }
            { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelRefine32x16");
                SetKernelArg(kernelRefine32x16, dist[PU32x16], mv[PU32x16], raw[cmCurIdx],
                             fwdRefCur);
                EnqueueKernel(device, queue, kernelRefine32x16, width2x / 16, height2x / 8,
                              lastEvent[cmCurIdx]);
            }
            {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelRefine16x32");
                SetKernelArg(kernelRefine16x32, dist[PU16x32], mv[PU16x32], raw[cmCurIdx],
                             fwdRefCur);
                EnqueueKernel(device, queue, kernelRefine16x32, width2x / 8, height2x / 16,
                              lastEvent[cmCurIdx]);
            }
        }
        device->DestroyVmeSurfaceG7_5(refs2x);
        device->DestroyVmeSurfaceG7_5(refs);
    }

    {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "WaitForLastKernelCur");
        lastEvent[cmCurIdx]->WaitForTaskFinished();
        queue->DestroyEvent(lastEvent[cmCurIdx]);
        lastEvent[cmCurIdx] = NULL;
    }
}   


void RunVmeNext(H265VideoParam const & param, H265Frame * pFrameNext, H265Slice *pSliceNext,
                H265Frame **refsNext)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RunVmeNext");

    if (pFrameNext)
    {
        { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuRawNext");
            EnqueueCopyCPUToGPUStride(queue, raw[cmNextIdx], pFrameNext->y, pFrameNext->pitch_luma,
                                      lastEvent[cmNextIdx]);
        }
        if (param.intraAngModes == 3) {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelGradient");
            SetKernelArg(kernelGradient, raw[cmNextIdx], mbIntraGrad4x4[cmNextIdx],
                         mbIntraGrad8x8[cmNextIdx], width);
            EnqueueKernel(device, queue, kernelGradient, width / 8, height / 8,
                          lastEvent[cmNextIdx]);
        }

        if (pFrameNext->m_PicCodType & MFX_FRAMETYPE_P) {
            if (param.Log2MaxCUSize > 4) {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "DSRaw2xNext");
                SetKernelArg(kernelDownSampleSrc, raw[cmNextIdx], raw2x[cmNextIdx]);
                EnqueueKernel(device, queue, kernelDownSampleSrc, width / 16, height / 16,
                              lastEvent[cmNextIdx]);
            }

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelMeIntra");
            SurfaceIndex *refsIntra = CreateVmeSurfaceG75(device, raw[cmNextIdx], 0, 0, 0, 0);
            SetKernelArg(kernelMeIntra, curbe, *refsIntra, raw[cmNextIdx], mbIntraDist[cmNextIdx]);
            EnqueueKernel(device, queue, kernelMeIntra, width / 16, height / 16,
                          lastEvent[cmNextIdx]);
            device->DestroyVmeSurfaceG7_5(refsIntra);
        }

        for (Ipp32s refIdx = 1; pFrameNext && refIdx < pSliceNext->num_ref_idx[0]; refIdx++)
        {
            CmSurface2DUP ** dist = distGpu[cmNextIdx][refIdx];
            CmSurface2DUP ** mv = mvGpu[cmNextIdx][refIdx];
            SurfaceIndex *refs2x = CreateVmeSurfaceG75(device, raw2x[cmNextIdx], &fwdRef2xNext, 0, 1, 0);
            SurfaceIndex *refs   = CreateVmeSurfaceG75(device, raw[cmNextIdx],   &fwdRefNext,   0, 1, 0);
            { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuRefNext");
                EnqueueCopyCPUToGPUStride(queue, fwdRefNext, refsNext[refIdx]->y,
                                          refsNext[refIdx]->pitch_luma, lastEvent[cmNextIdx]);
            }
            if (param.Log2MaxCUSize > 4) {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "DSNextFwdRef");
                SetKernelArg(kernelDownSampleFwd, fwdRefNext, fwdRef2xNext);
                EnqueueKernel(device, queue, kernelDownSampleFwd, width / 16, height / 16,
                              lastEvent[cmNextIdx]);
            }
            { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelMe");
                SetKernelArg(kernelMe, me1xControl, *refs, dist[PU16x16], dist[PU16x8], dist[PU8x16],
                             dist[PU8x8], dist[PU8x4], dist[PU4x8], mv[PU16x16], mv[PU16x8],
                             mv[PU8x16], mv[PU8x8], mv[PU8x4], mv[PU4x8]);
                EnqueueKernel(device, queue, kernelMe, width / 16, height / 16,
                              lastEvent[cmNextIdx]);
            }
            if (param.Log2MaxCUSize > 4) {
                { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelMe2x");
                    SetKernelArg(kernelMe2x, me2xControl, *refs2x, mv[PU32x32], mv[PU32x16],
                                 mv[PU16x32]);
                    EnqueueKernel(device, queue, kernelMe2x, width2x / 16, height2x / 16,
                                  lastEvent[cmNextIdx]);
                }
                { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelRefine32x32");
                    SetKernelArg(kernelRefine32x32, dist[PU32x32], mv[PU32x32], raw[cmNextIdx],
                                 fwdRefNext);
                    EnqueueKernel(device, queue, kernelRefine32x32, width2x / 16, height2x / 16,
                                  lastEvent[cmNextIdx]);
                }
                { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelRefine32x16");
                    SetKernelArg(kernelRefine32x16, dist[PU32x16], mv[PU32x16], raw[cmNextIdx],
                                 fwdRefNext);
                    EnqueueKernel(device, queue, kernelRefine32x16, width2x / 16, height2x / 8,
                                  lastEvent[cmNextIdx]);
                }
                { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelRefine16x32");
                    SetKernelArg(kernelRefine16x32, dist[PU16x32], mv[PU16x32], raw[cmNextIdx],
                                 fwdRefNext);
                    EnqueueKernel(device, queue, kernelRefine16x32, width2x / 8, height2x / 16,
                                  lastEvent[cmNextIdx]);
                }
            }

            device->DestroyVmeSurfaceG7_5(refs2x);
            device->DestroyVmeSurfaceG7_5(refs);
        }
    }

    frameOrder++;
}   


void SetCurbeData(
    H265EncCURBEData & curbeData,
    mfxU32             picType,
    mfxU32             qp)
{
    mfxU32 interSad       = 0; // 0-sad,2-haar
    mfxU32 intraSad       = 0; // 0-sad,2-haar
    //mfxU32 ftqBasedSkip   = 0; //3;
    mfxU32 blockBasedSkip = 0;//1;
    mfxU32 qpIdx          = (qp + 1) >> 1;
    mfxU32 transformFlag  = 0;//(extOpt->IntraPredBlockSize > 1);
    mfxU32 meMethod       = 6;

    mfxVMEIMEIn spath;
    SetSearchPath(spath, picType, meMethod);

    //init CURBE data based on Image State and Slice State. this CURBE data will be
    //written to the surface and sent to all kernels.
    Zero(curbeData);

    //DW0
    curbeData.SkipModeEn            = 0;//!(picType & MFX_FRAMETYPE_I);
    curbeData.AdaptiveEn            = 1;
    curbeData.BiMixDis              = 0;
    curbeData.EarlyImeSuccessEn     = 0;
    curbeData.T8x8FlagForInterEn    = 1;
    curbeData.EarlyImeStop          = 0;
    //DW1
    curbeData.MaxNumMVs             = 0x3F/*(GetMaxMvsPer2Mb(m_video.mfx.CodecLevel) >> 1) & 0x3F*/;
    curbeData.BiWeight              = /*((task.m_frameType & MFX_FRAMETYPE_B) && extDdi->WeightedBiPredIdc == 2) ? CalcBiWeight(task, 0, 0) :*/ 32;
    curbeData.UniMixDisable         = 0;
    //DW2
    curbeData.MaxNumSU              = 57;
    curbeData.LenSP                 = 16;
    //DW3
    curbeData.SrcSize               = 0;
    curbeData.MbTypeRemap           = 0;
    curbeData.SrcAccess             = 0;
    curbeData.RefAccess             = 0;
    curbeData.SearchCtrl            = (picType & MFX_FRAMETYPE_B) ? 7 : 0;
    curbeData.DualSearchPathOption  = 0;
    curbeData.SubPelMode            = 0;//3; // all modes   
    curbeData.SkipType              = 0; //!!(task.m_frameType & MFX_FRAMETYPE_B); //for B 0-16x16, 1-8x8
    curbeData.DisableFieldCacheAllocation = 0;
    curbeData.InterChromaMode       = 0;
    curbeData.FTQSkipEnable         = 0;//!(picType & MFX_FRAMETYPE_I);
    curbeData.BMEDisableFBR         = !!(picType & MFX_FRAMETYPE_P);
    curbeData.BlockBasedSkipEnabled = blockBasedSkip;
    curbeData.InterSAD              = interSad;
    curbeData.IntraSAD              = intraSad;
    curbeData.SubMbPartMask         = (picType & MFX_FRAMETYPE_I) ? 0 : 0x7e; // only 16x16 for Inter
    //DW4
    curbeData.SliceHeightMinusOne   = height / 16 - 1;
    curbeData.PictureHeightMinusOne = height / 16 - 1;
    curbeData.PictureWidth          = width / 16;
    curbeData.Log2MvScaleFactor     = 0;
    curbeData.Log2MbScaleFactor     = 0;
    //DW5
    curbeData.RefWidth              = (picType & MFX_FRAMETYPE_B) ? 32 : 48;
    curbeData.RefHeight             = (picType & MFX_FRAMETYPE_B) ? 32 : 40;
    //DW6
    curbeData.BatchBufferEndCommand = BATCHBUFFER_END;
    //DW7
    curbeData.IntraPartMask         = 0; // all partitions enabled
    curbeData.NonSkipZMvAdded       = 0;//!!(picType & MFX_FRAMETYPE_P);
    curbeData.NonSkipModeAdded      = 0;//!!(picType & MFX_FRAMETYPE_P);
    curbeData.IntraCornerSwap       = 0;
    curbeData.MVCostScaleFactor     = 0;
    curbeData.BilinearEnable        = 0;
    curbeData.SrcFieldPolarity      = 0;
    curbeData.WeightedSADHAAR       = 0;
    curbeData.AConlyHAAR            = 0;
    curbeData.RefIDCostMode         = 0;//!(picType & MFX_FRAMETYPE_I);
    curbeData.SkipCenterMask        = !!(picType & MFX_FRAMETYPE_P);
    //DW8
    curbeData.ModeCost_0            = 0;
    curbeData.ModeCost_1            = 0;
    curbeData.ModeCost_2            = 0;
    curbeData.ModeCost_3            = 0;
    //DW9
    curbeData.ModeCost_4            = 0;
    curbeData.ModeCost_5            = 0;
    curbeData.ModeCost_6            = 0;
    curbeData.ModeCost_7            = 0;
    //DW10
    curbeData.ModeCost_8            = 0;
    curbeData.ModeCost_9            = 0;
    curbeData.RefIDCost             = 0;
    curbeData.ChromaIntraModeCost   = 0;
    //DW11
    curbeData.MvCost_0              = 0;
    curbeData.MvCost_1              = 0;
    curbeData.MvCost_2              = 0;
    curbeData.MvCost_3              = 0;
    //DW12
    curbeData.MvCost_4              = 0;
    curbeData.MvCost_5              = 0;
    curbeData.MvCost_6              = 0;
    curbeData.MvCost_7              = 0;
    //DW13
    curbeData.QpPrimeY              = qp;
    curbeData.QpPrimeCb             = qp;
    curbeData.QpPrimeCr             = qp;
    curbeData.TargetSizeInWord      = 0xff;
    //DW14
    curbeData.FTXCoeffThresh_DC               = 0;
    curbeData.FTXCoeffThresh_1                = 0;
    curbeData.FTXCoeffThresh_2                = 0;
    //DW15
    curbeData.FTXCoeffThresh_3                = 0;
    curbeData.FTXCoeffThresh_4                = 0;
    curbeData.FTXCoeffThresh_5                = 0;
    curbeData.FTXCoeffThresh_6                = 0;
    //DW16
    curbeData.IMESearchPath0                  = spath.IMESearchPath0to31[0];
    curbeData.IMESearchPath1                  = spath.IMESearchPath0to31[1];
    curbeData.IMESearchPath2                  = spath.IMESearchPath0to31[2];
    curbeData.IMESearchPath3                  = spath.IMESearchPath0to31[3];
    //DW17
    curbeData.IMESearchPath4                  = spath.IMESearchPath0to31[4];
    curbeData.IMESearchPath5                  = spath.IMESearchPath0to31[5];
    curbeData.IMESearchPath6                  = spath.IMESearchPath0to31[6];
    curbeData.IMESearchPath7                  = spath.IMESearchPath0to31[7];
    //DW18
    curbeData.IMESearchPath8                  = spath.IMESearchPath0to31[8];
    curbeData.IMESearchPath9                  = spath.IMESearchPath0to31[9];
    curbeData.IMESearchPath10                 = spath.IMESearchPath0to31[10];
    curbeData.IMESearchPath11                 = spath.IMESearchPath0to31[11];
    //DW19
    curbeData.IMESearchPath12                 = spath.IMESearchPath0to31[12];
    curbeData.IMESearchPath13                 = spath.IMESearchPath0to31[13];
    curbeData.IMESearchPath14                 = spath.IMESearchPath0to31[14];
    curbeData.IMESearchPath15                 = spath.IMESearchPath0to31[15];
    //DW20
    curbeData.IMESearchPath16                 = spath.IMESearchPath0to31[16];
    curbeData.IMESearchPath17                 = spath.IMESearchPath0to31[17];
    curbeData.IMESearchPath18                 = spath.IMESearchPath0to31[18];
    curbeData.IMESearchPath19                 = spath.IMESearchPath0to31[19];
    //DW21
    curbeData.IMESearchPath20                 = spath.IMESearchPath0to31[20];
    curbeData.IMESearchPath21                 = spath.IMESearchPath0to31[21];
    curbeData.IMESearchPath22                 = spath.IMESearchPath0to31[22];
    curbeData.IMESearchPath23                 = spath.IMESearchPath0to31[23];
    //DW22
    curbeData.IMESearchPath24                 = spath.IMESearchPath0to31[24];
    curbeData.IMESearchPath25                 = spath.IMESearchPath0to31[25];
    curbeData.IMESearchPath26                 = spath.IMESearchPath0to31[26];
    curbeData.IMESearchPath27                 = spath.IMESearchPath0to31[27];
    //DW23
    curbeData.IMESearchPath28                 = spath.IMESearchPath0to31[28];
    curbeData.IMESearchPath29                 = spath.IMESearchPath0to31[29];
    curbeData.IMESearchPath30                 = spath.IMESearchPath0to31[30];
    curbeData.IMESearchPath31                 = spath.IMESearchPath0to31[31];
    //DW24
    curbeData.IMESearchPath32                 = spath.IMESearchPath32to55[0];
    curbeData.IMESearchPath33                 = spath.IMESearchPath32to55[1];
    curbeData.IMESearchPath34                 = spath.IMESearchPath32to55[2];
    curbeData.IMESearchPath35                 = spath.IMESearchPath32to55[3];
    //DW25
    curbeData.IMESearchPath36                 = spath.IMESearchPath32to55[4];
    curbeData.IMESearchPath37                 = spath.IMESearchPath32to55[5];
    curbeData.IMESearchPath38                 = spath.IMESearchPath32to55[6];
    curbeData.IMESearchPath39                 = spath.IMESearchPath32to55[7];
    //DW26
    curbeData.IMESearchPath40                 = spath.IMESearchPath32to55[8];
    curbeData.IMESearchPath41                 = spath.IMESearchPath32to55[9];
    curbeData.IMESearchPath42                 = spath.IMESearchPath32to55[10];
    curbeData.IMESearchPath43                 = spath.IMESearchPath32to55[11];
    //DW27
    curbeData.IMESearchPath44                 = spath.IMESearchPath32to55[12];
    curbeData.IMESearchPath45                 = spath.IMESearchPath32to55[13];
    curbeData.IMESearchPath46                 = spath.IMESearchPath32to55[14];
    curbeData.IMESearchPath47                 = spath.IMESearchPath32to55[15];
    //DW28
    curbeData.IMESearchPath48                 = spath.IMESearchPath32to55[16];
    curbeData.IMESearchPath49                 = spath.IMESearchPath32to55[17];
    curbeData.IMESearchPath50                 = spath.IMESearchPath32to55[18];
    curbeData.IMESearchPath51                 = spath.IMESearchPath32to55[19];
    //DW29
    curbeData.IMESearchPath52                 = spath.IMESearchPath32to55[20];
    curbeData.IMESearchPath53                 = spath.IMESearchPath32to55[21];
    curbeData.IMESearchPath54                 = spath.IMESearchPath32to55[22];
    curbeData.IMESearchPath55                 = spath.IMESearchPath32to55[23];
    //DW30
    curbeData.Intra4x4ModeMask                = 0;
    curbeData.Intra8x8ModeMask                = 0;
    //DW31
    curbeData.Intra16x16ModeMask              = 0;
    curbeData.IntraChromaModeMask             = 0;
    curbeData.IntraComputeType                = 1; // luma only
    //DW32
    curbeData.SkipVal                         = 0;//skipVal;
    curbeData.MultipredictorL0EnableBit       = 0xEF;
    curbeData.MultipredictorL1EnableBit       = 0xEF;
    //DW33
    curbeData.IntraNonDCPenalty16x16          = 0;//36;
    curbeData.IntraNonDCPenalty8x8            = 0;//12;
    curbeData.IntraNonDCPenalty4x4            = 0;//4;
    //DW34
    //curbeData.MaxVmvR                         = GetMaxMvLenY(m_video.mfx.CodecLevel) * 4;
    curbeData.MaxVmvR                         = 0x7FFF;
    //DW35
    curbeData.PanicModeMBThreshold            = 0xFF;
    curbeData.SmallMbSizeInWord               = 0xFF;
    curbeData.LargeMbSizeInWord               = 0xFF;
    //DW36
    curbeData.HMECombineOverlap               = 1;  //  0;  !sergo
    curbeData.HMERefWindowsCombiningThreshold = (picType & MFX_FRAMETYPE_B) ? 8 : 16; //  0;  !sergo (should be =8 for B frames)
    curbeData.CheckAllFractionalEnable        = 0;
    //DW37
    curbeData.CurLayerDQId                    = 0;
    curbeData.TemporalId                      = 0;
    curbeData.NoInterLayerPredictionFlag      = 1;
    curbeData.AdaptivePredictionFlag          = 0;
    curbeData.DefaultBaseModeFlag             = 0;
    curbeData.AdaptiveResidualPredictionFlag  = 0;
    curbeData.DefaultResidualPredictionFlag   = 0;
    curbeData.AdaptiveMotionPredictionFlag    = 0;
    curbeData.DefaultMotionPredictionFlag     = 0;
    curbeData.TcoeffLevelPredictionFlag       = 0;
    curbeData.UseRawMePredictor               = 1;
    curbeData.SpatialResChangeFlag            = 0;
///    curbeData.isFwdFrameShortTermRef          = task.m_list0[0].Size() > 0 && !task.m_dpb[0][task.m_list0[0][0] & 127].m_longterm;
    //DW38
    curbeData.ScaledRefLayerLeftOffset        = 0;
    curbeData.ScaledRefLayerRightOffset       = 0;
    //DW39
    curbeData.ScaledRefLayerTopOffset         = 0;
    curbeData.ScaledRefLayerBottomOffset      = 0;
}

Ipp32s GetPuSize(Ipp32s puw, Ipp32s puh)
{
    const Ipp32s MAX_PU_WIDTH = 64;
    const Ipp32s MAX_PU_HEIGHT = 64;
    enum { IDX4, IDX8, IDX16, IDX32, IDX_MAX };
    static const Ipp8s tab_wh2Idx[MAX_PU_WIDTH >> 2] = { IDX4, IDX8, -1, IDX16, -1, -1, -1, IDX32, -1, -1, -1, -1, -1, -1, -1, -1 };
    static const Ipp8s tab_lookUpPuSize[IDX_MAX][IDX_MAX] = {
        //H=  4       8       16       32
        {    -1,  PU4x8,      -1,      -1 }, //W=4
        { PU8x4,  PU8x8,  PU8x16,      -1 }, //W=8
        {    -1, PU16x8, PU16x16, PU16x32 }, //W=16
        {    -1,     -1, PU32x16, PU32x32 }, //W=32
    };

    return tab_lookUpPuSize[tab_wh2Idx[puw / 4 - 1]][tab_wh2Idx[puh / 4 - 1]];
}

} // namespace

#endif // MFX_ENABLE_CM

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
