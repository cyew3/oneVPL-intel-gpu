//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

#include "mfx_h265_defs.h"
#include "mfx_h265_enc_cm_utils.h"

namespace H265Enc {

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
///    double totTimeMs = timer.total / double(timer.freq) * 1000.0;
///    double minTimeMs = timer.min   / double(timer.freq) * 1000.0;
///    double avgTimeMs = totTimeMs / timer.callCount;
    //printf("%s (called %3u times) time min=%.3f avg=%.3f total=%.3f ms\n", name, timer.callCount, minTimeMs, avgTimeMs, totTimeMs);
}

typedef std::map<std::pair<int, int>, double> Map;
Map interpolationTime;

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

    //double sum = 0.0;
    //for (Map::iterator i = interpolationTime.begin(); i != interpolationTime.end(); ++i)
    //    printf("Interpolate_%02dx%02d time %.3f\n", i->first.first, i->first.second, i->second), sum += i->second;
    //printf("Interpolate_AVERAGE time %.3f\n", sum/interpolationTime.size());
}

#endif // #if defined(_WIN32) || defined(_WIN64)

template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
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
    //fprintf(stderr, "Success creating kernel %s\n", name);

    return kernel;
}

void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel, mfxU32 tsWidth,
                   mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "EnqueueKernel");
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

//void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel0, CmKernel *kernel1, mfxU32 tsWidth,
//                   mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern)
//{
//    int result = CM_SUCCESS;
//
//    if ((result = kernel0->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
//        throw CmRuntimeError();
//    if ((result = kernel1->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    CmThreadSpace * cmThreadSpace = 0;
//    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    cmThreadSpace->SelectThreadDependencyPattern(tsPattern);
//
//    CmTask * cmTask = 0;
//    if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if ((result = cmTask->AddKernel(kernel0)) != CM_SUCCESS)
//        throw CmRuntimeError();
//    if ((result = cmTask->AddKernel(kernel1)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if (event != NULL && event != CM_NO_EVENT)
//        queue->DestroyEvent(event);
//
//    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    device->DestroyThreadSpace(cmThreadSpace);
//    device->DestroyTask(cmTask);
//}

//void EnqueueKernel(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth,
//                   mfxU32 tsHeight, CmEvent *&event)
//{
//    int result = CM_SUCCESS;
//
//    if ((result = kernel->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    CmThreadSpace * cmThreadSpace = 0;
//    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    cmThreadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
//
//    //CmTask * cmTask = 0;
//    //if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
//    //    throw CmRuntimeError();\
//
//    if (cmTask)
//        cmTask->Reset();
//    else
//        throw CmRuntimeError();
//
//    if ((result = cmTask->AddKernel(kernel)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if (event != NULL && event != CM_NO_EVENT)
//        queue->DestroyEvent(event);
//
//    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    device->DestroyThreadSpace(cmThreadSpace);
//    //device->DestroyTask(cmTask);
//}

//void EnqueueKernelLight(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth,
//                        mfxU32 tsHeight, CmEvent *&event)
//{
//    // for kernel reusing; ThreadCount should be set in caller before SetArgs (Cm restriction!!!)
//
//    int result = CM_SUCCESS;
//
//    CmThreadSpace * cmThreadSpace = 0;
//    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    cmThreadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
//
//    //CmTask * cmTask = 0;
//    //if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
//    //    throw CmRuntimeError();\
//
//    if (cmTask)
//        cmTask->Reset();
//    else
//        throw CmRuntimeError();
//
//    if ((result = cmTask->AddKernel(kernel)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if (event != NULL && event != CM_NO_EVENT)
//        queue->DestroyEvent(event);
//
//    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    device->DestroyThreadSpace(cmThreadSpace);
//    //device->DestroyTask(cmTask);
//}

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

void Read(CmBuffer * buffer, void * buf, CmEvent * e)
{
    int result = CM_SUCCESS;
    if ((result = buffer->ReadSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Write(CmBuffer * buffer, void * buf, CmEvent * e)
{
    int result = CM_SUCCESS;
    if ((result = buffer->WriteSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}
#ifdef CM_4_0
    #define TASKNUMALLOC 3
#else
    #define TASKNUMALLOC 0
#endif
CmDevice * TryCreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version)
{
    mfxU32 versionPlaceholder = 0;
    if (version == 0)
        version = &versionPlaceholder;

    mfxCoreParam par;
    mfxStatus sts = core->GetCoreParam(&par);

    if (sts != MFX_ERR_NONE)
        return 0;

    CmDevice * device = 0;

    int result = CM_SUCCESS;
	mfxU32 impl = par.Impl & 0x0700;
    if (impl == MFX_IMPL_VIA_D3D9)
    {
#if defined(_WIN32) || defined(_WIN64)
        IDirect3DDeviceManager9 * d3dIface;
        sts = core->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&d3dIface);
        if (sts != MFX_ERR_NONE || !d3dIface)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface,(TASKNUMALLOC<<4)+1)) != CM_SUCCESS)
            return 0;
#endif  // #if defined(_WIN32) || defined(_WIN64)
    }
    else if (impl & MFX_IMPL_VIA_D3D11)
    {
#if defined(_WIN32) || defined(_WIN64)
        ID3D11Device * d3dIface;
        sts = core->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&d3dIface);
        if (sts != MFX_ERR_NONE || !d3dIface)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface,(TASKNUMALLOC<<4)+1)) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (impl & MFX_IMPL_VIA_VAAPI)
    {
        //throw std::logic_error("GetDeviceManager not implemented on Linux");
#if defined(MFX_VA_LINUX)
        VADisplay display;
        mfxStatus res = core->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL*)&display); // == MFX_HANDLE_RESERVED2
        if (res != MFX_ERR_NONE || !display)
            return 0;

        if ((result = ::CreateCmDevice(device, *version, display)) != CM_SUCCESS)
            return 0;
#endif
    }

    return device;
}

CmDevice * CreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version)
{
    /* return 0 instead of throwing exception to allow failing gracefully */
    CmDevice * device = TryCreateCmDevicePtr(core, version);
#if 0
    if (device == 0)
        throw CmRuntimeError();
#endif
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

void SetSearchPath(VmeSearchPath *spath)
{
    small_memcpy(spath->sp, Diamond, sizeof(spath->sp));
    spath->lenSp = 16;
    spath->maxNumSu = 57;
}

void SetSearchPathSmall(VmeSearchPath *spath)
{
    memset(spath->sp, 0, sizeof(spath->sp));
    small_memcpy(spath->sp, SmallPath, 3);
    spath->lenSp = 4;
    spath->maxNumSu = 9;
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
            small_memcpy(&spath.IMESearchPath0to31[0], &SingleSU[0], 32*sizeof(mfxU8));
            maxNumSU = 1;
            break;
        case 3:
            small_memcpy(&spath.IMESearchPath0to31[0], &RasterScan_48x40[0], 32*sizeof(mfxU8));
            small_memcpy(&spath.IMESearchPath32to55[0], &RasterScan_48x40[32], 24*sizeof(mfxU8));
            break;
        case 4:
            small_memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
            small_memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
            break;
        case 5:
            small_memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
            small_memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
            maxNumSU = 16;
            break;
        case 6:
        default:
            small_memcpy(&spath.IMESearchPath0to31[0], &Diamond[0], 32*sizeof(mfxU8));
            small_memcpy(&spath.IMESearchPath32to55[0], &Diamond[32], 24*sizeof(mfxU8));
            break;
        }
    }
    else
    {
        if (meMethod == 6)
        {
            small_memcpy(&spath.IMESearchPath0to31[0], &Diamond[0], 32*sizeof(mfxU8));
            small_memcpy(&spath.IMESearchPath32to55[0], &Diamond[32], 24*sizeof(mfxU8));
        }
        else
        {
            small_memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
            small_memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
        }
    }

    return maxNumSU;
}

void SetCurbeData(
    H265EncCURBEData & curbeData,
    mfxU32             PicType,
    mfxU32             qp,
    mfxU32             width,
    mfxU32             height)
{
    mfxU32 interSad       = 0; // 0-sad,2-haar
    mfxU32 intraSad       = 0; // 0-sad,2-haar
    //mfxU32 ftqBasedSkip   = 0; //3;
    mfxU32 blockBasedSkip = 0;//1;
///    mfxU32 qpIdx          = (qp + 1) >> 1;
///    mfxU32 transformFlag  = 0;//(extOpt->IntraPredBlockSize > 1);
    mfxU32 meMethod       = 6;

    mfxVMEIMEIn spath;
    SetSearchPath(spath, PicType, meMethod);

    //init CURBE data based on Image State and Slice State. this CURBE data will be
    //written to the surface and sent to all kernels.
    Zero(curbeData);

    //DW0
    curbeData.SkipModeEn            = 0;//!(PicType & MFX_FRAMETYPE_I);
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
    curbeData.SearchCtrl            = (PicType & MFX_FRAMETYPE_B) ? 7 : 0;
    curbeData.DualSearchPathOption  = 0;
    curbeData.SubPelMode            = 0;//3; // all modes
    curbeData.SkipType              = 0; //!!(task.m_frameType & MFX_FRAMETYPE_B); //for B 0-16x16, 1-8x8
    curbeData.DisableFieldCacheAllocation = 0;
    curbeData.InterChromaMode       = 0;
    curbeData.FTQSkipEnable         = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.BMEDisableFBR         = !!(PicType & MFX_FRAMETYPE_P);
    curbeData.BlockBasedSkipEnabled = blockBasedSkip;
    curbeData.InterSAD              = interSad;
    curbeData.IntraSAD              = intraSad;
    curbeData.SubMbPartMask         = (PicType & MFX_FRAMETYPE_I) ? 0 : 0x7e; // only 16x16 for Inter
    //DW4
    curbeData.SliceHeightMinusOne   = height / 16 - 1;
    curbeData.PictureHeightMinusOne = height / 16 - 1;
    curbeData.PictureWidth          = width / 16;
    curbeData.Log2MvScaleFactor     = 0;
    curbeData.Log2MbScaleFactor     = 0;
    //DW5
    curbeData.RefWidth              = (PicType & MFX_FRAMETYPE_B) ? 32 : 48;
    curbeData.RefHeight             = (PicType & MFX_FRAMETYPE_B) ? 32 : 40;
    //DW6
    curbeData.BatchBufferEndCommand = BATCHBUFFER_END;
    //DW7
    curbeData.IntraPartMask         = 0; // all partitions enabled
    curbeData.NonSkipZMvAdded       = 0;//!!(PicType & MFX_FRAMETYPE_P);
    curbeData.NonSkipModeAdded      = 0;//!!(PicType & MFX_FRAMETYPE_P);
    curbeData.IntraCornerSwap       = 0;
    curbeData.MVCostScaleFactor     = 0;
    curbeData.BilinearEnable        = 0;
    curbeData.SrcFieldPolarity      = 0;
    curbeData.WeightedSADHAAR       = 0;
    curbeData.AConlyHAAR            = 0;
    curbeData.RefIDCostMode         = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.SkipCenterMask        = !!(PicType & MFX_FRAMETYPE_P);
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
    curbeData.HMERefWindowsCombiningThreshold = (PicType & MFX_FRAMETYPE_B) ? 8 : 16; //  0;  !sergo (should be =8 for B frames)
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

Kernel::Kernel() : kernel(), task(), ts() {}

void Kernel::Configure(CmDevice *device, CmProgram *program, const char *name,
                       mfxU32 tsWidth, mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern)
{
    kernel = CreateKernel(device, program, name, NULL);
    if (kernel->SetThreadCount(tsWidth * tsHeight) != CM_SUCCESS)
        throw CmRuntimeError();
    if (device->CreateThreadSpace(tsWidth, tsHeight, ts) != CM_SUCCESS)
        throw CmRuntimeError();
    if (ts->SelectThreadDependencyPattern(tsPattern) != CM_SUCCESS)
        throw CmRuntimeError();
    if (device->CreateTask(task) != CM_SUCCESS)
        throw CmRuntimeError();
    if (task->AddKernel(kernel) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Kernel::Destroy(CmDevice *device)
{
    device->DestroyThreadSpace(ts);
    ts = NULL;
    device->DestroyTask(task);
    task = NULL;
    device->DestroyKernel(kernel);
    kernel = NULL;
}

void Kernel::Enqueue(CmQueue *queue, CmEvent *&e)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Enqueue");
    if (e != NULL && e != CM_NO_EVENT)
        queue->DestroyEvent(e);
    if (queue->Enqueue(task, e, ts) != CM_SUCCESS)
        throw CmRuntimeError();
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
