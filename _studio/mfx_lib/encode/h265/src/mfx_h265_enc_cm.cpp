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

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

#include "mfx_h265_defs.h"
//#include "mfx_h265_prediction.h"
#include "mfx_h265_enc_cm.h"
#include "mfx_h265_enc_cm_defs.h"
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

CmEvent * EnqueueKernel(CmDevice * device, CmQueue * queue, CmKernel * kernel, mfxU32 tsWidth,
                        mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern = CM_NONE_DEPENDENCY)
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

    CmEvent * e = 0;
    if ((result = queue->Enqueue(cmTask, e, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    device->DestroyThreadSpace(cmThreadSpace);
    device->DestroyTask(cmTask);

    return e;
}


SurfaceIndex & GetIndex(CmSurface2D * surface)
{
    SurfaceIndex * index = 0;
    int result = surface->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

SurfaceIndex & GetIndex(CmSurface2DUP * surface)
{
    SurfaceIndex * index = 0;
    int result = surface->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}


SurfaceIndex & GetIndex(CmBuffer * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

SurfaceIndex const & GetIndex(CmBufferUP * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
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


namespace
{
    mfxU32 Map44LutValueBack(mfxU32 val)
    {
        mfxU32 base  = val & 0xf;
        mfxU32 shift = (val >> 4) & 0xf;
        assert((base << shift) < (1 << 12)); // encoded value must fit in 12-bits (Bspec)
        return base << shift;
    }

    const mfxU8 QP_LAMBDA[40] = // pow(2, qp/6)
    {
        1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 4, 4, 4, 5, 6,
        6, 7, 8, 9,10,11,13,14,
        16,18,20,23,25,29,32,36,
        40,45,51,57,64,72,81,91
    };

    mfxU8 GetMaxMvsPer2Mb(mfxU32 level)
    {
        return level < 30 ? 126 : (level == 30 ? 32 : 16);
    }

    mfxU32 GetMaxMvLenY(mfxU32 level)
    {
        return level < 11
            ? 63
            : level < 21
                ? 127
                : level < 31
                    ? 255
                    : 511;
    }

    using std::min;
    using std::max;

    mfxU8 Map44LutValue(mfxU32 v, mfxU8 max)
    {
        if(v == 0)        return 0;

        mfxI16 D = (mfxI16)(log((double)v)/log(2.)) - 3;
        if(D < 0)    D = 0;
        mfxI8 ret = (mfxU8)((D << 4) + (int)((v + (D == 0 ? 0 : (1<<(D-1)))) >> D));

        ret =  (ret & 0xf) == 0 ? (ret | 8) : ret;

        if(((ret&15)<<(ret>>4)) > ((max&15)<<(max>>4)))        ret = max;
        return ret;
    }

    const mfxU16 Dist10[26] =
    {
        0,0,0,0,2,
        4,7,11,17,25,
        35,50,68,91,119,
        153,194,241,296,360,
        432,513,604,706,819,
        944
    };

    const mfxU16 Dist25[26] =
    {
        0,0,0,0,
        12,32,51,69,87,
        107,129,154,184,219,
        260,309,365,431,507,
        594,694,806,933,1074,
        1232,1406
    };

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

CmSurface2DUP ** dist32x32[2] = {0};
CmSurface2DUP ** dist32x16[2] = {0};
CmSurface2DUP ** dist16x32[2] = {0};
CmSurface2DUP ** dist16x16[2] = {0};
CmSurface2DUP ** dist16x8[2] = {0};
CmSurface2DUP ** dist8x16[2] = {0};
CmSurface2DUP ** dist8x8[2] = {0};
CmSurface2DUP ** dist8x4[2] = {0};
CmSurface2DUP ** dist4x8[2] = {0};

CmSurface2DUP ** mv32x32[2] = {0};
CmSurface2DUP ** mv32x16[2] = {0};
CmSurface2DUP ** mv16x32[2] = {0};
CmSurface2DUP ** mv16x16[2] = {0};
CmSurface2DUP ** mv16x8[2] = {0};
CmSurface2DUP ** mv8x16[2] = {0};
CmSurface2DUP ** mv8x8[2] = {0};
CmSurface2DUP ** mv8x4[2] = {0};
CmSurface2DUP ** mv4x8[2] = {0};

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
CmBuffer * curbe2x = 0;
mfxU32 width = 0;
mfxU32 height = 0;
mfxU32 width2x = 0;
mfxU32 height2x = 0;
mfxU32 frameOrder = 0;
mfxU32 maxNumRefs = 0;

Ipp32u dist32x32Pitch = 0;
Ipp32u dist32x16Pitch = 0;
Ipp32u dist16x32Pitch = 0;
Ipp32u dist16x16Pitch = 0;
Ipp32u dist16x8Pitch = 0;
Ipp32u dist8x16Pitch = 0;
Ipp32u dist8x8Pitch = 0;
Ipp32u dist8x4Pitch = 0;
Ipp32u dist4x8Pitch = 0;
CmMbDist32 ** cmDist32x32[2] = {0};
CmMbDist32 ** cmDist32x16[2] = {0};
CmMbDist32 ** cmDist16x32[2] = {0};
CmMbDist16 ** cmDist16x16[2] = {0};
CmMbDist16 ** cmDist16x8[2] = {0};
CmMbDist16 ** cmDist8x16[2] = {0};
CmMbDist16 ** cmDist8x8[2] = {0};
CmMbDist16 ** cmDist8x4[2] = {0};
CmMbDist16 ** cmDist4x8[2] = {0};

Ipp32u mv32x32Pitch = 0;
Ipp32u mv32x16Pitch = 0;
Ipp32u mv16x32Pitch = 0;
Ipp32u mv16x16Pitch = 0;
Ipp32u mv16x8Pitch = 0;
Ipp32u mv8x16Pitch = 0;
Ipp32u mv8x8Pitch = 0;
Ipp32u mv8x4Pitch = 0;
Ipp32u mv4x8Pitch = 0;
mfxI16Pair ** cmMv32x32[2] = {0};
mfxI16Pair ** cmMv32x16[2] = {0};
mfxI16Pair ** cmMv16x32[2] = {0};
mfxI16Pair ** cmMv16x16[2] = {0};
mfxI16Pair ** cmMv16x8[2] = {0};
mfxI16Pair ** cmMv8x16[2] = {0};
mfxI16Pair ** cmMv8x8[2] = {0};
mfxI16Pair ** cmMv8x4[2] = {0};
mfxI16Pair ** cmMv4x8[2] = {0};

CmEvent * me1xIntraEvent = 0;
CmEvent * gradientEvent = 0;
CmEvent * ready16x16Event = 0;
CmEvent * readyMV32x32Event = 0;
CmEvent * readyRefine32x32Event = 0;
CmEvent * readyRefine32x16Event = 0;
CmEvent * readyRefine16x32Event = 0;
CmEvent * me1xEvent[16] = {0};   // max num refs
CmEvent * me2xEvent[16] = {0};
CmEvent * readDist32x32Event[16] = {0};
CmEvent * readDist32x16Event[16] = {0};
CmEvent * readDist16x32Event[16] = {0};
CmEvent * lastEvent[2] = {};
Ipp32s cmCurIdx = 0;
Ipp32s cmNextIdx = 0;

int cmMvW[PU_MAX] = {};
int cmMvH[PU_MAX] = {};

bool operator==(mfxI16Pair const & l, mfxI16Pair const & r) { return l.x == r.x && l.y == r.y; }
template <class T>
mfxI16Pair operator*(mfxI16Pair const & p, T scalar) { mfxI16Pair r = { mfxI16(p.x * scalar), mfxI16(p.y * scalar) }; return r; }
template <class T>
mfxI16Pair & operator*=(mfxI16Pair & p, T scalar) { p.x *= scalar; p.y *= scalar; return p; }

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
    if (cmResurcesAllocated)
    {
        device->DestroySurface(curbe);
        device->DestroySurface(curbe2x);
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

        for (uint i = 0; i < 2; i++)
        {
            device->DestroySurface2DUP(mbIntraDist[i]);
            CM_ALIGNED_FREE(cmMbIntraDist[i]);
            mbIntraDist[i] = 0;
            cmMbIntraDist[i] = 0;

            device->DestroyBufferUP(mbIntraGrad4x4[i]);
            CM_ALIGNED_FREE(cmMbIntraGrad4x4[i]);
            mbIntraGrad4x4[i] = 0;
            cmMbIntraGrad4x4[i] = 0;

            device->DestroyBufferUP(mbIntraGrad8x8[i]);
            CM_ALIGNED_FREE(cmMbIntraGrad8x8[i]);
            mbIntraGrad8x8[i] = 0;
            cmMbIntraGrad8x8[i] = 0;

            for (uint j = 0; j < maxNumRefs; j++)
            {
                device->DestroySurface2DUP(dist32x32[i][j]);
                device->DestroySurface2DUP(dist32x16[i][j]);
                device->DestroySurface2DUP(dist16x32[i][j]);
                device->DestroySurface2DUP(dist16x16[i][j]);
                device->DestroySurface2DUP(dist16x8[i][j]);
                device->DestroySurface2DUP(dist8x16[i][j]);
                device->DestroySurface2DUP(dist8x8[i][j]);
                device->DestroySurface2DUP(dist8x4[i][j]);
                device->DestroySurface2DUP(dist4x8[i][j]);
                CM_ALIGNED_FREE(cmDist32x32[i][j]);
                CM_ALIGNED_FREE(cmDist32x16[i][j]);
                CM_ALIGNED_FREE(cmDist16x32[i][j]);
                CM_ALIGNED_FREE(cmDist16x16[i][j]);
                CM_ALIGNED_FREE(cmDist16x8[i][j]);
                CM_ALIGNED_FREE(cmDist8x16[i][j]);
                CM_ALIGNED_FREE(cmDist8x8[i][j]);
                CM_ALIGNED_FREE(cmDist8x4[i][j]);
                CM_ALIGNED_FREE(cmDist4x8[i][j]);

                device->DestroySurface2DUP(mv32x32[i][j]);
                CM_ALIGNED_FREE(cmMv32x32[i][j]);
                device->DestroySurface2DUP(mv32x16[i][j]);
                CM_ALIGNED_FREE(cmMv32x16[i][j]);
                device->DestroySurface2DUP(mv16x32[i][j]);
                CM_ALIGNED_FREE(cmMv16x32[i][j]);
                device->DestroySurface2DUP(mv16x16[i][j]);
                CM_ALIGNED_FREE(cmMv16x16[i][j]);
                device->DestroySurface2DUP(mv16x8[i][j]);
                CM_ALIGNED_FREE(cmMv16x8[i][j]);
                device->DestroySurface2DUP(mv8x16[i][j]);
                CM_ALIGNED_FREE(cmMv8x16[i][j]);
                device->DestroySurface2DUP(mv8x8[i][j]);
                CM_ALIGNED_FREE(cmMv8x8[i][j]);
                device->DestroySurface2DUP(mv8x4[i][j]);
                CM_ALIGNED_FREE(cmMv8x4[i][j]);
                device->DestroySurface2DUP(mv4x8[i][j]);
                CM_ALIGNED_FREE(cmMv4x8[i][j]);
            }
            delete [] dist32x32[i];
            delete [] dist32x16[i];
            delete [] dist16x32[i];
            delete [] dist16x16[i];
            delete [] dist16x8[i];
            delete [] dist8x16[i];
            delete [] dist8x8[i];
            delete [] dist8x4[i];
            delete [] dist4x8[i];
            delete [] cmDist32x32[i];
            delete [] cmDist32x16[i];
            delete [] cmDist16x32[i];
            delete [] cmDist16x16[i];
            delete [] cmDist16x8[i];
            delete [] cmDist8x16[i];
            delete [] cmDist8x8[i];
            delete [] cmDist8x4[i];
            delete [] cmDist4x8[i];
            dist32x32[i] = 0;
            dist32x16[i] = 0;
            dist16x32[i] = 0;
            dist16x16[i] = 0;
            dist16x8[i] = 0;
            dist8x16[i] = 0;
            dist8x8[i] = 0;
            dist8x4[i] = 0;
            dist4x8[i] = 0;
            cmDist32x32[i] = 0;
            cmDist32x16[i] = 0;
            cmDist16x32[i] = 0;
            cmDist16x16[i] = 0;
            cmDist16x8[i] = 0;
            cmDist8x16[i] = 0;
            cmDist8x8[i] = 0;
            cmDist8x4[i] = 0;
            cmDist4x8[i] = 0;

            delete [] mv32x32[i];
            delete [] cmMv32x32[i];
            mv32x32[i] = 0;
            cmMv32x32[i] = 0;
            delete [] mv32x16[i];
            delete [] cmMv32x16[i];
            mv32x16[i] = 0;
            cmMv32x16[i] = 0;
            delete [] mv16x32[i];
            delete [] cmMv16x32[i];
            mv16x32[i] = 0;
            cmMv16x32[i] = 0;
            delete [] mv16x16[i];
            delete [] cmMv16x16[i];
            mv16x16[i] = 0;
            cmMv16x16[i] = 0;
            delete [] mv16x8[i];
            delete [] cmMv16x8[i];
            mv16x8[i] = 0;
            cmMv16x8[i] = 0;
            delete [] mv8x16[i];
            delete [] cmMv8x16[i];
            mv8x16[i] = 0;
            cmMv8x16[i] = 0;
            delete [] mv8x8[i];
            delete [] cmMv8x8[i];
            mv8x8[i] = 0;
            cmMv8x8[i] = 0;
            delete [] mv8x4[i];
            delete [] cmMv8x4[i];
            mv8x4[i] = 0;
            cmMv8x4[i] = 0;
            delete [] mv4x8[i];
            delete [] cmMv4x8[i];
            mv4x8[i] = 0;
            cmMv4x8[i] = 0;

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
    cmMvW[PU16x4 ] = w / 16; cmMvH[PU16x4 ] = h /  8;
    cmMvW[PU16x12] = w / 16; cmMvH[PU16x12] = h /  8;
    cmMvW[PU4x16 ] = w /  8; cmMvH[PU4x16 ] = h / 16;
    cmMvW[PU12x16] = w /  8; cmMvH[PU12x16] = h / 16;
    cmMvW[PU8x8  ] = w /  8; cmMvH[PU8x8  ] = h /  8;
    cmMvW[PU8x4  ] = w /  8; cmMvH[PU8x4  ] = h /  4;
    cmMvW[PU4x8  ] = w /  4; cmMvH[PU4x8  ] = h /  8;
    cmMvW[PU32x32] = width2x / 16; cmMvH[PU32x32] = height2x / 16;
    cmMvW[PU32x16] = width2x / 16; cmMvH[PU32x16] = height2x /  8;
    cmMvW[PU16x32] = width2x /  8; cmMvH[PU16x32] = height2x / 16;
    cmMvW[PU32x8 ] = width2x / 16; cmMvH[PU32x8 ] = height2x /  8;
    cmMvW[PU32x24] = width2x / 16; cmMvH[PU32x24] = height2x /  8;
    cmMvW[PU8x32 ] = width2x /  8; cmMvH[PU8x32 ] = height2x / 16;
    cmMvW[PU24x32] = width2x /  8; cmMvH[PU24x32] = height2x / 16;

    /* surfaces for mv32x32 */
    Ipp32u mv32x32Size = 0;
    device->GetSurface2DInfo(cmMvW[PU32x32] * sizeof(mfxI16Pair), cmMvH[PU32x32], CM_SURFACE_FORMAT_P8, mv32x32Pitch, mv32x32Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        /* surface for intra costs */
        Ipp32u intraSize = 0;
        device->GetSurface2DInfo(w / 16 * sizeof(CmMbIntraDist), h / 16, CM_SURFACE_FORMAT_P8, intraPitch[i], intraSize);
        cmMbIntraDist[i] = (CmMbIntraDist *)CM_ALIGNED_MALLOC(intraSize, 0x1000);    // 4K aligned 
        mbIntraDist[i] = CreateSurface(device, w * sizeof(CmMbIntraDist) / 16, h / 16, CM_SURFACE_FORMAT_P8, cmMbIntraDist[i]);

        /* surface for intra gradient histogram */
        cmMbIntraGrad4x4[i] = (CmMbIntraGrad *)CM_ALIGNED_MALLOC(w * h / 16 * sizeof(CmMbIntraGrad), 0x1000);
        mbIntraGrad4x4[i] = CreateBuffer(device, w * h / 16 * sizeof(CmMbIntraGrad), cmMbIntraGrad4x4[i]);

        cmMbIntraGrad8x8[i] = (CmMbIntraGrad *)CM_ALIGNED_MALLOC(w * h / 64 * sizeof(CmMbIntraGrad), 0x1000);
        mbIntraGrad8x8[i] = CreateBuffer(device, w * h / 64 * sizeof(CmMbIntraGrad), cmMbIntraGrad8x8[i]);

        cmMv32x32[i] = new mfxI16Pair* [maxNumRefs];
        mv32x32[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv32x32[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv32x32Size, 0x1000);    // 4K aligned 
            mv32x32[i][j] = CreateSurface(device, cmMvW[PU32x32] * sizeof(mfxI16Pair), cmMvH[PU32x32], CM_SURFACE_FORMAT_P8, cmMv32x32[i][j]);
        }
    }

    /* surfaces for mv32x16 */
    Ipp32u mv32x16Size = 0;
    device->GetSurface2DInfo(cmMvW[PU32x16] * sizeof(mfxI16Pair), cmMvH[PU32x16], CM_SURFACE_FORMAT_P8, mv32x16Pitch, mv32x16Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv32x16[i] = new mfxI16Pair* [maxNumRefs];
        mv32x16[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv32x16[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv32x16Size, 0x1000);    // 4K aligned 
            mv32x16[i][j] = CreateSurface(device, cmMvW[PU32x16] * sizeof(mfxI16Pair), cmMvH[PU32x16], CM_SURFACE_FORMAT_P8, cmMv32x16[i][j]);
        }
    }

    /* surfaces for mv16x32 */
    Ipp32u mv16x32Size = 0;
    device->GetSurface2DInfo(cmMvW[PU16x32] * sizeof(mfxI16Pair), cmMvH[PU16x32], CM_SURFACE_FORMAT_P8, mv16x32Pitch, mv16x32Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv16x32[i] = new mfxI16Pair* [maxNumRefs];
        mv16x32[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv16x32[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv16x32Size, 0x1000);    // 4K aligned 
            mv16x32[i][j] = CreateSurface(device, cmMvW[PU16x32] * sizeof(mfxI16Pair), cmMvH[PU16x32], CM_SURFACE_FORMAT_P8, cmMv16x32[i][j]);
        }
    }

    /* surfaces for mv16x16 */
    Ipp32u mv16x16Size = 0;
    device->GetSurface2DInfo(cmMvW[PU16x16] * sizeof(mfxI16Pair), cmMvH[PU16x16], CM_SURFACE_FORMAT_P8, mv16x16Pitch, mv16x16Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv16x16[i] = new mfxI16Pair* [maxNumRefs];
        mv16x16[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv16x16[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv16x16Size, 0x1000);    // 4K aligned 
            mv16x16[i][j] = CreateSurface(device, cmMvW[PU16x16] * sizeof(mfxI16Pair), cmMvH[PU16x16], CM_SURFACE_FORMAT_P8, cmMv16x16[i][j]);
        }
    }

    /* surfaces for mv16x8 */
    Ipp32u mv16x8Size = 0;
    device->GetSurface2DInfo(cmMvW[PU16x8] * sizeof(mfxI16Pair), cmMvH[PU16x8], CM_SURFACE_FORMAT_P8, mv16x8Pitch, mv16x8Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv16x8[i] = new mfxI16Pair* [maxNumRefs];
        mv16x8[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv16x8[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv16x8Size, 0x1000);    // 4K aligned 
            mv16x8[i][j] = CreateSurface(device, cmMvW[PU16x8] * sizeof(mfxI16Pair), cmMvH[PU16x8], CM_SURFACE_FORMAT_P8, cmMv16x8[i][j]);
        }
    }

    /* surfaces for mv8x16 */
    Ipp32u mv8x16Size = 0;
    device->GetSurface2DInfo(cmMvW[PU8x16] * sizeof(mfxI16Pair), cmMvH[PU8x16], CM_SURFACE_FORMAT_P8, mv8x16Pitch, mv8x16Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv8x16[i] = new mfxI16Pair* [maxNumRefs];
        mv8x16[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv8x16[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv8x16Size, 0x1000);    // 4K aligned 
            mv8x16[i][j] = CreateSurface(device, cmMvW[PU8x16] * sizeof(mfxI16Pair), cmMvH[PU8x16], CM_SURFACE_FORMAT_P8, cmMv8x16[i][j]);
        }
    }

    /* surfaces for mv8x8 */
    Ipp32u mv8x8Size = 0;
    device->GetSurface2DInfo(cmMvW[PU8x8] * sizeof(mfxI16Pair), cmMvH[PU8x8], CM_SURFACE_FORMAT_P8, mv8x8Pitch, mv8x8Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv8x8[i] = new mfxI16Pair* [maxNumRefs];
        mv8x8[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv8x8[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv8x8Size, 0x1000);    // 4K aligned 
            mv8x8[i][j] = CreateSurface(device, cmMvW[PU8x8] * sizeof(mfxI16Pair), cmMvH[PU8x8], CM_SURFACE_FORMAT_P8, cmMv8x8[i][j]);
        }
    }

    /* surfaces for mv8x4 */
    Ipp32u mv8x4Size = 0;
    device->GetSurface2DInfo(cmMvW[PU8x4] * sizeof(mfxI16Pair), cmMvH[PU8x4], CM_SURFACE_FORMAT_P8, mv8x4Pitch, mv8x4Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv8x4[i] = new mfxI16Pair* [maxNumRefs];
        mv8x4[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv8x4[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv8x4Size, 0x1000);    // 4K aligned 
            mv8x4[i][j] = CreateSurface(device, cmMvW[PU8x4] * sizeof(mfxI16Pair), cmMvH[PU8x4], CM_SURFACE_FORMAT_P8, cmMv8x4[i][j]);
        }
    }

    /* surfaces for mv4x8 */
    Ipp32u mv4x8Size = 0;
    device->GetSurface2DInfo(cmMvW[PU4x8] * sizeof(mfxI16Pair), cmMvH[PU4x8], CM_SURFACE_FORMAT_P8, mv4x8Pitch, mv4x8Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmMv4x8[i] = new mfxI16Pair* [maxNumRefs];
        mv4x8[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmMv4x8[i][j] = (mfxI16Pair *)CM_ALIGNED_MALLOC(mv4x8Size, 0x1000);    // 4K aligned 
            mv4x8[i][j] = CreateSurface(device, cmMvW[PU4x8] * sizeof(mfxI16Pair), cmMvH[PU4x8], CM_SURFACE_FORMAT_P8, cmMv4x8[i][j]);
        }
    }


    /* surfaces for dist32x32 */
    Ipp32u dist32x32Size = 0;
    device->GetSurface2DInfo(cmMvW[PU32x32] * sizeof(CmMbDist32), cmMvH[PU32x32], CM_SURFACE_FORMAT_P8, dist32x32Pitch, dist32x32Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist32x32[i] = new CmMbDist32* [maxNumRefs];
        dist32x32[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist32x32[i][j] = (CmMbDist32 *)CM_ALIGNED_MALLOC(dist32x32Size, 0x1000);    // 4K aligned 
            dist32x32[i][j] = CreateSurface(device, cmMvW[PU32x32] * sizeof(CmMbDist32), cmMvH[PU32x32], CM_SURFACE_FORMAT_P8, cmDist32x32[i][j]);
        }
    }

    /* surfaces for dist32x16 */
    Ipp32u dist32x16Size = 0;
    device->GetSurface2DInfo(cmMvW[PU32x16] * sizeof(CmMbDist32), cmMvH[PU32x16], CM_SURFACE_FORMAT_P8, dist32x16Pitch, dist32x16Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist32x16[i] = new CmMbDist32* [maxNumRefs];
        dist32x16[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist32x16[i][j] = (CmMbDist32 *)CM_ALIGNED_MALLOC(dist32x16Size, 0x1000);    // 4K aligned 
            dist32x16[i][j] = CreateSurface(device, cmMvW[PU32x16] * sizeof(CmMbDist32), cmMvH[PU32x16], CM_SURFACE_FORMAT_P8, cmDist32x16[i][j]);
        }
    }

    /* surfaces for dist16x32 */
    Ipp32u dist16x32Size = 0;
    device->GetSurface2DInfo(cmMvW[PU16x32] * sizeof(CmMbDist32), cmMvH[PU16x32], CM_SURFACE_FORMAT_P8, dist16x32Pitch, dist16x32Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist16x32[i] = new CmMbDist32* [maxNumRefs];
        dist16x32[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist16x32[i][j] = (CmMbDist32 *)CM_ALIGNED_MALLOC(dist16x32Size, 0x1000);    // 4K aligned 
            dist16x32[i][j] = CreateSurface(device, cmMvW[PU16x32] * sizeof(CmMbDist32), cmMvH[PU16x32], CM_SURFACE_FORMAT_P8, cmDist16x32[i][j]);
        }
    }

    /* surfaces for dist16x16 */
    Ipp32u dist16x16Size = 0;
    device->GetSurface2DInfo(cmMvW[PU16x16] * sizeof(CmMbDist16), cmMvH[PU16x16], CM_SURFACE_FORMAT_P8, dist16x16Pitch, dist16x16Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist16x16[i] = new CmMbDist16* [maxNumRefs];
        dist16x16[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist16x16[i][j] = (CmMbDist16 *)CM_ALIGNED_MALLOC(dist16x16Size, 0x1000);    // 4K aligned 
            dist16x16[i][j] = CreateSurface(device, cmMvW[PU16x16] * sizeof(CmMbDist16), cmMvH[PU16x16], CM_SURFACE_FORMAT_P8, cmDist16x16[i][j]);
        }
    }

    /* surfaces for dist16x8 */
    Ipp32u dist16x8Size = 0;
    device->GetSurface2DInfo(cmMvW[PU16x8] * sizeof(CmMbDist16), cmMvH[PU16x8], CM_SURFACE_FORMAT_P8, dist16x8Pitch, dist16x8Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist16x8[i] = new CmMbDist16* [maxNumRefs];
        dist16x8[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist16x8[i][j] = (CmMbDist16 *)CM_ALIGNED_MALLOC(dist16x8Size, 0x1000);    // 4K aligned 
            dist16x8[i][j] = CreateSurface(device, cmMvW[PU16x8] * sizeof(CmMbDist16), cmMvH[PU16x8], CM_SURFACE_FORMAT_P8, cmDist16x8[i][j]);
        }
    }

    /* surfaces for dist8x16 */
    Ipp32u dist8x16Size = 0;
    device->GetSurface2DInfo(cmMvW[PU8x16] * sizeof(CmMbDist16), cmMvH[PU8x16], CM_SURFACE_FORMAT_P8, dist8x16Pitch, dist8x16Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist8x16[i] = new CmMbDist16* [maxNumRefs];
        dist8x16[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist8x16[i][j] = (CmMbDist16 *)CM_ALIGNED_MALLOC(dist8x16Size, 0x1000);    // 4K aligned 
            dist8x16[i][j] = CreateSurface(device, cmMvW[PU8x16] * sizeof(CmMbDist16), cmMvH[PU8x16], CM_SURFACE_FORMAT_P8, cmDist8x16[i][j]);
        }
    }

    /* surfaces for dist8x8 */
    Ipp32u dist8x8Size = 0;
    device->GetSurface2DInfo(cmMvW[PU8x8] * sizeof(CmMbDist16), cmMvH[PU8x8], CM_SURFACE_FORMAT_P8, dist8x8Pitch, dist8x8Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist8x8[i] = new CmMbDist16* [maxNumRefs];
        dist8x8[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist8x8[i][j] = (CmMbDist16 *)CM_ALIGNED_MALLOC(dist8x8Size, 0x1000);    // 4K aligned 
            dist8x8[i][j] = CreateSurface(device, cmMvW[PU8x8] * sizeof(CmMbDist16), cmMvH[PU8x8], CM_SURFACE_FORMAT_P8, cmDist8x8[i][j]);
        }
    }

    /* surfaces for dist8x4 */
    Ipp32u dist8x4Size = 0;
    device->GetSurface2DInfo(cmMvW[PU8x4] * sizeof(CmMbDist16), cmMvH[PU8x4], CM_SURFACE_FORMAT_P8, dist8x4Pitch, dist8x4Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist8x4[i] = new CmMbDist16* [maxNumRefs];
        dist8x4[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist8x4[i][j] = (CmMbDist16 *)CM_ALIGNED_MALLOC(dist8x4Size, 0x1000);    // 4K aligned 
            dist8x4[i][j] = CreateSurface(device, cmMvW[PU8x4] * sizeof(CmMbDist16), cmMvH[PU8x4], CM_SURFACE_FORMAT_P8, cmDist8x4[i][j]);
        }
    }

    /* surfaces for dist4x8 */
    Ipp32u dist4x8Size = 0;
    device->GetSurface2DInfo(cmMvW[PU4x8] * sizeof(CmMbDist16), cmMvH[PU4x8], CM_SURFACE_FORMAT_P8, dist4x8Pitch, dist4x8Size);

    for (Ipp32u i = 0; i < 2; i++)
    {
        cmDist4x8[i] = new CmMbDist16* [maxNumRefs];
        dist4x8[i] = new CmSurface2DUP* [maxNumRefs];
        for (Ipp32u j = 0; j < maxNumRefs; j++)
        {
            cmDist4x8[i][j] = (CmMbDist16 *)CM_ALIGNED_MALLOC(dist4x8Size, 0x1000);    // 4K aligned 
            dist4x8[i][j] = CreateSurface(device, cmMvW[PU4x8] * sizeof(CmMbDist16), cmMvH[PU4x8], CM_SURFACE_FORMAT_P8, cmDist4x8[i][j]);
        }
    }


    kernelDownSampleSrc = CreateKernel(device, program, "DownSampleMB", (void *)DownSampleMB);
    kernelDownSampleFwd = CreateKernel(device, program, "DownSampleMB", (void *)DownSampleMB);
    kernelMeIntra = CreateKernel(device, program, "RawMeMB_P_Intra", (void *)RawMeMB_P_Intra);
    kernelGradient = CreateKernel(device, program, "AnalyzeGradient", (void *)AnalyzeGradient);
    kernelMe      = CreateKernel(device, program, "RawMeMB_P", (void *)RawMeMB_P);
    kernelMe2x    = CreateKernel(device, program, "MeP32", (void *)MeP32);
    kernelRefine32x32 = CreateKernel(device, program, "RefineMeP32x32", (void *)RefineMeP32x32);
    kernelRefine32x16 = CreateKernel(device, program, "RefineMeP32x16", (void *)RefineMeP32x16);
    kernelRefine16x32 = CreateKernel(device, program, "RefineMeP16x32", (void *)RefineMeP16x32);
    curbe   = CreateBuffer(device, sizeof(H265EncCURBEData));
    curbe2x = CreateBuffer(device, sizeof(H265EncCURBEData));

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "SetCurbeDataAllCur");
        // common curbe data
        H265EncCURBEData curbeData = {};
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "SetCurbeDataCur");
            SetCurbeData(curbeData, MFX_FRAMETYPE_P, 26);
        }

        // original sized
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "curbe->WriteSurface0");
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

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "curbe->WriteSurface1");
            curbeData.PictureHeightMinusOne = height2x / 16 - 1;
            curbeData.SliceHeightMinusOne   = height2x / 16 - 1;
            curbeData.PictureWidth          = width2x  / 16;
            curbeData.InterSAD              = 0;
            curbeData.IntraSAD              = 0;
            curbe2x->WriteSurface((mfxU8 *)&curbeData, NULL);
        }
    }

    cmResurcesAllocated = true;
    cmCurIdx = 1;
    cmNextIdx = 0;
}

void RunVme(
    H265VideoParam const & param,
    H265Frame *            pFrameCur,
    H265Frame *            pFrameNext,
    H265Slice *            pSliceCur,
    H265Slice *            pSliceNext,
    H265Frame **           refsCur,
    H265Frame **           refsNext)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RunVme");

#if defined(_WIN32) || defined(_WIN64)
    mfxU64 runVmeStart = gettick();
#endif // #if defined(_WIN32) || defined(_WIN64)

    CmEvent * writeSrcEvent = 0;
    CmEvent * writeFwdEvent = 0;
    CmEvent * dsSrcEvent = 0;
    CmEvent * dsFwdEvent = 0;
        
    const H265Frame *refFrameCur = refsCur[0];

    if (frameOrder == 0) {
        {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuRawCur");
            queue->EnqueueCopyCPUToGPUStride(raw[cmCurIdx], pFrameCur->y, pFrameCur->pitch_luma,
                                             writeSrcEvent);
            lastEvent[cmCurIdx] = writeSrcEvent;
        }

        if (param.intraAngModes == 3) {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelGradient");
            SetKernelArg(kernelGradient, GetIndex(raw[cmCurIdx]), GetIndex(mbIntraGrad4x4[cmCurIdx]),
                         GetIndex(mbIntraGrad8x8[cmCurIdx]), width);
            gradientEvent = EnqueueKernel(device, queue, kernelGradient, width / 8, height / 8);
            lastEvent[cmCurIdx] = gradientEvent;
        }
    }
    else if (pFrameCur->m_PicCodType & MFX_FRAMETYPE_P) {
        SurfaceIndex * refs2x = CreateVmeSurfaceG75(device, raw2x[cmCurIdx], &fwdRef2xCur, 0, 1, 0);
        SurfaceIndex * refs   = CreateVmeSurfaceG75(device, raw[cmCurIdx], &fwdRefCur, 0, 1, 0);
        {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuCurFwdRef");
            queue->EnqueueCopyCPUToGPUStride(fwdRefCur, refFrameCur->y, refFrameCur->pitch_luma,
                                             writeFwdEvent);
        }
        if (param.Log2MaxCUSize > 4) {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "DSCurFwdRef");
            SetKernelArg(kernelDownSampleFwd, GetIndex(fwdRefCur), GetIndex(fwdRef2xCur));
            dsFwdEvent = EnqueueKernel(device, queue, kernelDownSampleFwd, width / 16, height / 16);
        }
        {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelMe");
            SetKernelArg(kernelMe, GetIndex(curbe), *refs, GetIndex(dist16x16[cmCurIdx][0]),
                         GetIndex(dist16x8[cmCurIdx][0]), GetIndex(dist8x16[cmCurIdx][0]),
                         GetIndex(dist8x8[cmCurIdx][0]), GetIndex(dist8x4[cmCurIdx][0]),
                         GetIndex(dist4x8[cmCurIdx][0]), GetIndex(mv16x16[cmCurIdx][0]),
                         GetIndex(mv16x8[cmCurIdx][0]), GetIndex(mv8x16[cmCurIdx][0]),
                         GetIndex(mv8x8[cmCurIdx][0]), GetIndex(mv8x4[cmCurIdx][0]),
                         GetIndex(mv4x8[cmCurIdx][0]));
            ready16x16Event = EnqueueKernel(device, queue, kernelMe, width / 16, height / 16);
            lastEvent[cmCurIdx] = ready16x16Event;
        }
        if (param.Log2MaxCUSize > 4) {
            {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelMe2x");
                SetKernelArg(kernelMe2x, GetIndex(curbe2x), *refs2x, GetIndex(mv32x32[cmCurIdx][0]),
                             GetIndex(mv32x16[cmCurIdx][0]), GetIndex(mv16x32[cmCurIdx][0]));
                readyMV32x32Event = EnqueueKernel(device, queue, kernelMe2x, width2x / 16, height2x / 16);
            }
            {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelRefine32x32");
                SetKernelArg(kernelRefine32x32, GetIndex(dist32x32[cmCurIdx][0]),
                             GetIndex(mv32x32[cmCurIdx][0]), GetIndex(raw[cmCurIdx]), GetIndex(fwdRefCur));
                readyRefine32x32Event = EnqueueKernel(device, queue, kernelRefine32x32, width2x / 16,
                                                      height2x / 16);
            }
            {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelRefine32x16");
                SetKernelArg(kernelRefine32x16, GetIndex(dist32x16[cmCurIdx][0]),
                             GetIndex(mv32x16[cmCurIdx][0]), GetIndex(raw[cmCurIdx]), GetIndex(fwdRefCur));
                readyRefine32x16Event = EnqueueKernel(device, queue, kernelRefine32x16, width2x / 16,
                                                      height2x / 8);
            }
            {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelRefine16x32");
                SetKernelArg(kernelRefine16x32, GetIndex(dist16x32[cmCurIdx][0]),
                             GetIndex(mv16x32[cmCurIdx][0]), GetIndex(raw[cmCurIdx]), GetIndex(fwdRefCur));
                readyRefine16x32Event = EnqueueKernel(device, queue, kernelRefine16x32, width2x / 8,
                                                      height2x / 16);
                lastEvent[cmCurIdx] = readyRefine16x32Event;
            }
        }
        device->DestroyVmeSurfaceG7_5(refs2x);
        device->DestroyVmeSurfaceG7_5(refs);
    }


    if (pFrameNext)
    { // common next raw
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CommonNextRaw");

        { 
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuRawNext");
            queue->EnqueueCopyCPUToGPUStride(raw[cmNextIdx], pFrameNext->y, pFrameNext->pitch_luma,
                                             writeSrcEvent);
            lastEvent[cmNextIdx] = writeSrcEvent;
        }
        if (param.intraAngModes == 3) {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelGradient");
            SetKernelArg(kernelGradient, GetIndex(raw[cmNextIdx]), GetIndex(mbIntraGrad4x4[cmNextIdx]),
                         GetIndex(mbIntraGrad8x8[cmNextIdx]), width);
            gradientEvent = EnqueueKernel(device, queue, kernelGradient, width / 8, height / 8);
            lastEvent[cmNextIdx] = gradientEvent;
        }

        if (pFrameNext->m_PicCodType & MFX_FRAMETYPE_P) {
            
            if (param.Log2MaxCUSize > 4) {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "DSRaw2xNext");
                SetKernelArg(kernelDownSampleSrc, GetIndex(raw[cmNextIdx]), GetIndex(raw2x[cmNextIdx]));
                dsSrcEvent = EnqueueKernel(device, queue, kernelDownSampleSrc, width / 16, height / 16);
            }

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefLast_kernelMeIntra");
            SurfaceIndex *refsIntra = CreateVmeSurfaceG75(device, raw[cmNextIdx], 0, 0, 0, 0);
            SetKernelArg(kernelMeIntra, GetIndex(curbe), *refsIntra, GetIndex(raw[cmNextIdx]),
                         GetIndex(mbIntraDist[cmNextIdx]));
            me1xIntraEvent = EnqueueKernel(device, queue, kernelMeIntra, width / 16, height / 16);
            device->DestroyVmeSurfaceG7_5(refsIntra);
            lastEvent[cmNextIdx] = me1xIntraEvent;
        }

        { 
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Enqueue Next");
            for (int refIdx = 1; pFrameNext && refIdx < pSliceNext->num_ref_idx[0]; refIdx++)
            {
                H265Frame const *refFrameNext = refsNext[refIdx];
                SurfaceIndex * refs2x = CreateVmeSurfaceG75(device, raw2x[cmNextIdx], &fwdRef2xNext, 0, 1, 0);
                SurfaceIndex * refs   = CreateVmeSurfaceG75(device, raw[cmNextIdx],   &fwdRefNext,   0, 1, 0);
                {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyToGpuRefNext");
                    queue->EnqueueCopyCPUToGPUStride(fwdRefNext, refFrameNext->y,
                                                     refFrameNext->pitch_luma, writeFwdEvent);
                }
                if (param.Log2MaxCUSize > 4) {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "DSNextFwdRef");
                    SetKernelArg(kernelDownSampleFwd, GetIndex(fwdRefNext), GetIndex(fwdRef2xNext));
                    dsFwdEvent = EnqueueKernel(device, queue, kernelDownSampleFwd, width / 16, height / 16);
                }
                {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelMe");
                    SetKernelArg(kernelMe, GetIndex(curbe), *refs,
                        GetIndex(dist16x16[cmNextIdx][refIdx]), GetIndex(dist16x8[cmNextIdx][refIdx]),
                        GetIndex(dist8x16[cmNextIdx][refIdx]), GetIndex(dist8x8[cmNextIdx][refIdx]),
                        GetIndex(dist8x4[cmNextIdx][refIdx]), GetIndex(dist4x8[cmNextIdx][refIdx]),
                        GetIndex(mv16x16[cmNextIdx][refIdx]), GetIndex(mv16x8[cmNextIdx][refIdx]),
                        GetIndex(mv8x16[cmNextIdx][refIdx]), GetIndex(mv8x8[cmNextIdx][refIdx]),
                        GetIndex(mv8x4[cmNextIdx][refIdx]), GetIndex(mv4x8[cmNextIdx][refIdx]));
                    CmEvent *e = EnqueueKernel(device, queue, kernelMe, width / 16, height / 16);
                    if (refIdx + 1 == pSliceNext->num_ref_idx[0])
                        lastEvent[cmNextIdx] = e;
                }
                if (param.Log2MaxCUSize > 4) {
                    {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelMe2x");
                    SetKernelArg(kernelMe2x, GetIndex(curbe2x), *refs2x,
                        GetIndex(mv32x32[cmNextIdx][refIdx]),
                        GetIndex(mv32x16[cmNextIdx][refIdx]),
                        GetIndex(mv16x32[cmNextIdx][refIdx]));
                    EnqueueKernel(device, queue, kernelMe2x, width2x / 16, height2x / 16);
                    }
                    {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelRefine32x32");
                    SetKernelArg(kernelRefine32x32, GetIndex(dist32x32[cmNextIdx][refIdx]),
                        GetIndex(mv32x32[cmNextIdx][refIdx]), GetIndex(raw[cmNextIdx]),
                        GetIndex(fwdRefNext));
                    EnqueueKernel(device, queue, kernelRefine32x32, width2x / 16, height2x / 16);
                    }
                    {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelRefine32x16");
                    SetKernelArg(kernelRefine32x16, GetIndex(dist32x16[cmNextIdx][refIdx]),
                        GetIndex(mv32x16[cmNextIdx][refIdx]), GetIndex(raw[cmNextIdx]),
                        GetIndex(fwdRefNext));
                    EnqueueKernel(device, queue, kernelRefine32x16, width2x / 16, height2x / 8);
                    }
                    {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RefNext_kernelRefine16x32");
                    SetKernelArg(kernelRefine16x32, GetIndex(dist16x32[cmNextIdx][refIdx]),
                        GetIndex(mv16x32[cmNextIdx][refIdx]), GetIndex(raw[cmNextIdx]),
                        GetIndex(fwdRefNext));
                    CmEvent *e = EnqueueKernel(device, queue, kernelRefine16x32, width2x / 8, height2x / 16);
                    if (refIdx + 1 == pSliceNext->num_ref_idx[0])
                        lastEvent[cmNextIdx] = e;
                    }
                }

                //TIMERS.me1x.Add(GetCmTime(me1xEvent));
                //TIMERS.me2x.Add(GetCmTime(me2xEvent));
                //TIMERS.refine32x32.Add(GetCmTime(refineEvent32x32));
                //TIMERS.refine32x16.Add(GetCmTime(refineEvent32x16));
                //TIMERS.refine16x32.Add(GetCmTime(refineEvent16x32));
                //TIMERS.dsSrc.Add(GetCmTime(dsSrcEvent));
                //TIMERS.dsFwd.Add(GetCmTime(dsFwdEvent));
                //TIMERS.writeSrc.Add(GetCmTime(writeSrcEvent));
                //TIMERS.writeFwd.Add(GetCmTime(writeFwdEvent));
                //TIMERS.readDist32x32.Add(GetCmTime(readDist32x32Event));
                //TIMERS.readDist32x16.Add(GetCmTime(readDist32x16Event));
                //TIMERS.readDist16x32.Add(GetCmTime(readDist16x32Event));

                device->DestroyVmeSurfaceG7_5(refs2x);
                device->DestroyVmeSurfaceG7_5(refs);
            }
        }
    }


    {   MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "WaitForLastKernelCur");
        lastEvent[cmCurIdx]->WaitForTaskFinished();
        queue->DestroyEvent(lastEvent[cmCurIdx]);
        lastEvent[cmCurIdx] = NULL;
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

    mfxU32 skipVal = (picType & MFX_FRAMETYPE_P) ? (Dist10[qpIdx]) : (Dist25[qpIdx]);
    if (!blockBasedSkip)
        skipVal *= 3;
    else if (!transformFlag)
        skipVal /= 2;

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
///    curbeData.MaxVmvR                         = GetMaxMvLenY(m_video.mfx.CodecLevel) * 4;
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

void GetAngModesFromHistogram(Ipp32s xPu, Ipp32s yPu, Ipp32s puSize, Ipp8s *modes, Ipp32s numModes)
{
    VM_ASSERT(numModes <= 33);
    VM_ASSERT(puSize >= 4);
    VM_ASSERT(puSize <= 64);
    VM_ASSERT(xPu + puSize <= (Ipp32s)width);
    VM_ASSERT(yPu + puSize <= (Ipp32s)height);

    Ipp32s pitch = width;

    Ipp32s histogram[35] = {};

    // all in units of 4x4 blocks
    if (puSize == 4) {
        pitch >>= 2;
        puSize >>= 2;
        const CmMbIntraGrad *histBlock = cmMbIntraGrad4x4[cmCurIdx] + (xPu >> 2) + (yPu >> 2) * pitch;
        for (Ipp32s i = 0; i < 35; i++)
            histogram[i] = histBlock->histogram[i];
    }
    else {
        pitch >>= 3;
        puSize >>= 3;
        const CmMbIntraGrad *histBlock = cmMbIntraGrad8x8[cmCurIdx] + (xPu >> 3) + (yPu >> 3) * pitch;

        for (Ipp32s y = 0; y < puSize; y++, histBlock += pitch)
            for (Ipp32s x = 0; x < puSize; x++)
                for (Ipp32s i = 0; i < 35; i++)
                    histogram[i] += histBlock[x].histogram[i];
    }

    for (Ipp32s i = 0; i < numModes; i++) {
        Ipp32s mode = (Ipp32s)(std::max_element(histogram + 2, histogram + 35) - histogram);
        modes[i] = mode;
        histogram[mode] = -1;
    }
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
