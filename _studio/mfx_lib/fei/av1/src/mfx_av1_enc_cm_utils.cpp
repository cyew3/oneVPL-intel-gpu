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


#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>
#include <tuple>

#include "mfx_av1_enc_cm_utils.h"

namespace AV1Enc {

template<class T> inline void Zero(T &obj) { memset(&obj, 0, sizeof(obj)); }

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
//    //    throw CmRuntimeError();
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
//    //    throw CmRuntimeError();
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
    if ((result = queue->EnqueueCopyCPUToGPUFullStride(surface, (const mfxU8 *)sysMem, pitch, 0, CM_FASTCOPY_OPTION_NONBLOCKING, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void EnqueueCopyGPUToCPUStride(CmQueue *queue, CmSurface2D *surface, void *sysMem,
                               mfxU32 pitch, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyGPUToCPUFullStride(surface, (mfxU8 *)sysMem, pitch, 0, CM_FASTCOPY_OPTION_NONBLOCKING, event)) != CM_SUCCESS)
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
        // Remove CSR Allocation, Other low mem options, (1<<22) | (1<< 29) |( 1<< 28)
        if ((result = ::CreateCmDevice(device, *version, d3dIface,((TASKNUMALLOC<<4)+1) | (1 << 22) | (1 << 29) )) != CM_SUCCESS)
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
        // Remove CSR Allocation, Other low mem options, (1<<22) | (1<< 29) |( 1<< 28)
        if ((result = ::CreateCmDevice(device, *version, d3dIface,((TASKNUMALLOC<<4)+1) | (1 << 22) | (1 << 29) )) != CM_SUCCESS)
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

#define TRACE_CM_ALLOC 0
#if TRACE_CM_ALLOC
std::map<unsigned,int> buffers;
std::map<unsigned,int> buffersUp;
std::map<std::tuple<unsigned,unsigned,unsigned>,int> surfaces;
std::map<std::tuple<unsigned,unsigned,unsigned>,int> surfacesUp;

const char *getfmt(unsigned fourcc) {
    if (fourcc == CM_SURFACE_FORMAT_P8)   return "  P8";
    if (fourcc == CM_SURFACE_FORMAT_A8)   return "  A8";
    if (fourcc == CM_SURFACE_FORMAT_NV12) return "NV12";
    if (fourcc == CM_SURFACE_FORMAT_V8U8) return "V8U8";
    return "UNKN";
}

int getsize(unsigned width, unsigned height, unsigned fourcc) {
    if (fourcc == CM_SURFACE_FORMAT_P8)   return width * height;
    if (fourcc == CM_SURFACE_FORMAT_A8)   return width * height;
    if (fourcc == CM_SURFACE_FORMAT_NV12) return width * height * 3 / 2;
    if (fourcc == CM_SURFACE_FORMAT_V8U8) return width * height * 2;
    return width * height;
}

struct PRINT {
    ~PRINT() {
        fprintf(stderr, "Buffers:\n");
        int total_total_count = 0;
        int total_total_size = 0;
        int total_count = 0;
        int total_size = 0;
        for (auto kv: buffers) {
            fprintf(stderr, "  %d bytes - %d\n", kv.first, kv.second);
            total_count += kv.second;
            total_size  += kv.first * kv.second;
        }
        total_total_count += total_count;
        total_total_size  += total_size;
        fprintf(stderr, "  TOTAL - %d, %d bytes\n", total_count, total_size);
        fprintf(stderr, "\n");

        total_count = 0;
        total_size = 0;
        fprintf(stderr, "BuffersUp:\n");
        for (auto kv: buffersUp) {
            fprintf(stderr, "  %d bytes - %d\n", kv.first, kv.second);
            total_count += kv.second;
            total_size  += kv.first * kv.second;
        }
        total_total_count += total_count;
        total_total_size  += total_size;
        fprintf(stderr, "  TOTAL - %d, %d bytes\n", total_count, total_size);
        fprintf(stderr, "\n");

        total_count = 0;
        total_size = 0;
        fprintf(stderr, "Surfaces:\n");
        for (auto kv: surfaces) {
            fprintf(stderr, "  %4dx%-4d (%s) - %d\n", std::get<0>(kv.first), std::get<1>(kv.first), getfmt(std::get<2>(kv.first)), kv.second);
            total_count += kv.second;
            total_size  += getsize(std::get<0>(kv.first), std::get<1>(kv.first), std::get<2>(kv.first)) * kv.second;
        }
        total_total_count += total_count;
        total_total_size  += total_size;
        fprintf(stderr, "  TOTAL - %d, %d bytes\n", total_count, total_size);
        fprintf(stderr, "\n");

        total_count = 0;
        total_size = 0;
        fprintf(stderr, "SurfacesUp:\n");
        for (auto kv: surfacesUp) {
            fprintf(stderr, "  %4dx%-4d (%s) - %d\n", std::get<0>(kv.first), std::get<1>(kv.first), getfmt(std::get<2>(kv.first)), kv.second);
            total_count += kv.second;
            total_size  += getsize(std::get<0>(kv.first), std::get<1>(kv.first), std::get<2>(kv.first)) * kv.second;
        }
        total_total_count += total_count;
        total_total_size  += total_size;
        fprintf(stderr, "  TOTAL - %d, %d bytes\n", total_count, total_size);
        fprintf(stderr, "\n");

        fprintf(stderr, "TOTAL - %d, %d bytes\n", total_total_count, total_total_size);
        fprintf(stderr, "\n");
    }
} DUMMY;
#endif //TRACE_CM_ALLOC

CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size)
{
    CmBuffer * buffer;
    int result = device->CreateBuffer(size, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
#if TRACE_CM_ALLOC
    buffers[size]++;
#endif //TRACE_CM_ALLOC
    return buffer;
}

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem)
{
    CmBufferUP * buffer;
    int result = device->CreateBufferUP(size, mem, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
#if TRACE_CM_ALLOC
    buffersUp[size]++;
#endif //TRACE_CM_ALLOC
    return buffer;
}

CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (result = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT(fourcc), cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
#if TRACE_CM_ALLOC
    surfaces[std::make_tuple(width,height,fourcc)]++;
#endif //TRACE_CM_ALLOC
    return cmSurface;
}

CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem)
{
    int result = CM_SUCCESS;
    CmSurface2DUP * cmSurface = 0;
    if (device && (result = device->CreateSurface2DUP(width, height, CM_SURFACE_FORMAT(fourcc), mem, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
#if TRACE_CM_ALLOC
    surfacesUp[std::make_tuple(width,height,fourcc)]++;
#endif //TRACE_CM_ALLOC
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

mfxU8 ToU4U4(mfxU16 val) {
    if (val > 4095)
        val = 4095;
    mfxU16 shift = 0;
    mfxU16 base = val;
    mfxU16 rem = 0;
    while (base > 15) {
        rem += (base & 1) << shift;
        base = (base >> 1);
        shift++;
    }
    base += (rem << 1 >> shift);
    return mfxU8(base | (shift << 4));
}

void SetupMeControl(MeControl &ctrl, int width, int height, double lambda)
{
    const mfxU8 myDiamond[56] = {
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

    memcpy(ctrl.longSp, myDiamond, (int)std::min(sizeof(myDiamond), sizeof(ctrl.longSp)));
    ctrl.longSpLenSp = 16;
    ctrl.longSpMaxNumSu = 57;

    const mfxU8 ShortPath[4] = {
        0x0F, 0xF0, 0x01, 0x00
    };

    memcpy(ctrl.shortSp, myDiamond, (int)std::min(sizeof(myDiamond), sizeof(ctrl.shortSp)));
    ctrl.shortSpLenSp = 16;
    ctrl.shortSpMaxNumSu = 57;

    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;

    const mfxU8 MvBits[4][8] = { // mvCostScale = qpel, hpel, ipel 2pel
        { 1, 4, 5, 6, 8, 10, 12, 14 },
        { 1, 5, 6, 8, 10, 12, 14, 16 },
        { 1, 6, 8, 10, 12, 14, 16, 18 },
        { 1, 8, 10, 12, 14, 16, 18, 20 }
    };

    ctrl.mvCostScaleFactor[0] = 2; // int-pel cost table precision
    ctrl.mvCostScaleFactor[1] = 2;
    ctrl.mvCostScaleFactor[2] = 2;
    ctrl.mvCostScaleFactor[3] = 2;
    ctrl.mvCostScaleFactor[4] = 2;

    const mfxU8 *mvBits = MvBits[ctrl.mvCostScaleFactor[0]];
    for (int32_t i = 0; i < 8; i++)
        ctrl.mvCost[0][i] = ToU4U4(mfxU16(0.5 + lambda/2 * mvBits[i]));

    mvBits = MvBits[ctrl.mvCostScaleFactor[1]];
    for (int32_t i = 0; i < 8; i++)
        ctrl.mvCost[1][i] = ToU4U4(mfxU16(0.5 + lambda/2 /  4 * (1 * (i > 0) + mvBits[i])));

    mvBits = MvBits[ctrl.mvCostScaleFactor[2]];
    for (int32_t i = 0; i < 8; i++)
        ctrl.mvCost[2][i] = ToU4U4(mfxU16(0.5 + lambda/2 / 16 * (2 * (i > 0) + mvBits[i])));

    mvBits = MvBits[ctrl.mvCostScaleFactor[3]];
    for (int32_t i = 0; i < 8; i++)
        ctrl.mvCost[3][i] = ToU4U4(mfxU16(0.5 + lambda/2 / 64 * (3 * (i > 0) + mvBits[i])));

    mvBits = MvBits[ctrl.mvCostScaleFactor[4]];
    for (int32_t i = 0; i < 8; i++)
        ctrl.mvCost[4][i] = ToU4U4(mfxU16(0.5 + lambda/2 / 64 * (4 * (i > 0) + mvBits[i])));

    //memset(ctrl.mvCost, 0, sizeof(ctrl.mvCost));

    mfxU8 MvCostHumanFriendly[5][8];
    for (int32_t i = 0; i < 5; i++)
        for (int32_t j = 0; j < 8; j++)
            MvCostHumanFriendly[i][j] = (ctrl.mvCost[i][j] & 0xf) << ((ctrl.mvCost[i][j] >> 4) & 0xf);
}


Kernel::Kernel() : kernel(m_kernel[0]), m_device(), m_task(), m_numKernels(0) { Zero(m_kernel); Zero(m_ts); }

void Kernel::AddKernel(CmDevice *device, CmProgram *program, const char *name,
                       mfxU32 tsWidth, mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern, bool addSync)
{
    assert(m_device == NULL || m_device == device);
    assert(m_numKernels < 16);

    mfxU32 idx = m_numKernels++;

    m_kernel[idx] = CreateKernel(device, program, name, NULL);
    if (m_kernel[idx]->SetThreadCount(tsWidth * tsHeight) != CM_SUCCESS)
        throw CmRuntimeError();
    if (device->CreateThreadSpace(tsWidth, tsHeight, m_ts[idx]) != CM_SUCCESS)
        throw CmRuntimeError();
    if (m_ts[idx]->SelectThreadDependencyPattern(tsPattern) != CM_SUCCESS)
        throw CmRuntimeError();
    if (m_task == NULL)
        if (device->CreateTask(m_task) != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_numKernels > 1 && addSync)
        if (m_task->AddSync() != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_task->AddKernel(m_kernel[idx]) != CM_SUCCESS)
        throw CmRuntimeError();
    if (m_numKernels == 2)
        if (m_kernel[0]->AssociateThreadSpace(m_ts[0]) != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_numKernels > 1) {
        if (m_kernel[idx]->AssociateThreadSpace(m_ts[idx]) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}

void Kernel::AddKernelWithGroup(CmDevice *device, CmProgram *program, const char *name,
                       mfxU32 grWidth, mfxU32 grHeight, mfxU32 tsWidth, mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern, bool addSync)
{
    assert(m_device == NULL || m_device == device);
    assert(m_numKernels < 16);

    mfxU32 idx = m_numKernels++;

    m_kernel[idx] = CreateKernel(device, program, name, NULL);

    int threadCount = (grWidth * grHeight) * (tsWidth * tsHeight);

    if (m_kernel[idx]->SetThreadCount(threadCount) != CM_SUCCESS)
        throw CmRuntimeError();

    if (device->CreateThreadGroupSpace(tsWidth, tsHeight, grWidth, grHeight, m_tgs[idx]))
        throw CmRuntimeError();

    //if (m_tgs[idx]->SelectThreadDependencyPattern(tsPattern) != CM_SUCCESS)
    //    throw CmRuntimeError();
    if (m_task == NULL)
        if (device->CreateTask(m_task) != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_numKernels > 1 && addSync)
        if (m_task->AddSync() != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_task->AddKernel(m_kernel[idx]) != CM_SUCCESS)
        throw CmRuntimeError();
    //if (m_numKernels == 2)
    //    if (m_kernel[0]->AssociateThreadSpace(m_ts[0]) != CM_SUCCESS)
    //        throw CmRuntimeError();
    //if (m_numKernels > 1)
    {
        if (m_kernel[idx]->AssociateThreadGroupSpace(m_tgs[idx]) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}
void Kernel::Destroy()
{
    if (m_device) {
        for (mfxU32 i = 0; i < m_numKernels; i++) {
            m_device->DestroyThreadSpace(m_ts[i]);
            m_ts[i] = NULL;
            m_device->DestroyKernel(m_kernel[i]);
            m_kernel[i] = NULL;
        }
        m_device->DestroyTask(m_task);
        m_task = NULL;
        m_device = NULL;
    }
}

void Kernel::Enqueue(CmQueue *queue, CmEvent *&e)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Enqueue");
    if (e != NULL && e != CM_NO_EVENT)
        queue->DestroyEvent(e);
    if (m_numKernels == 1) {
        int result = queue->Enqueue(m_task, e, m_ts[0]);
        if (result != CM_SUCCESS)
            throw CmRuntimeError();
    } else {
        int result = queue->Enqueue(m_task, e);
        if (result != CM_SUCCESS)
            throw CmRuntimeError();
    }
}

void Kernel::EnqueueWithGroup(CmQueue *queue, CmEvent *&e)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "EnqueueWithGroup");
    if (e != NULL && e != CM_NO_EVENT)
        queue->DestroyEvent(e);
    /*if (m_numKernels == 1) {
        if (queue->Enqueue(m_task, e, m_ts[0]) != CM_SUCCESS)
            throw CmRuntimeError();
    } else */
    {
        if (queue->EnqueueWithGroup(m_task, e) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
