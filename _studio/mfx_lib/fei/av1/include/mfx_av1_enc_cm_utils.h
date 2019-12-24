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


#ifndef _MFX_AV1_ENC_CM_UTILS_H
#define _MFX_AV1_ENC_CM_UTILS_H

#pragma warning(disable: 4100; disable: 4505)

#include "cmrt_cross_platform.h"
#include "cmvm.h"

///#include "mfx_h265_enc.h"
#include "libmfx_core_interface.h"
#include "mfxplugin++.h"

#include <vector>
///#include <assert.h>

namespace AV1Enc {

class CmRuntimeError : public std::exception
{
public:
    CmRuntimeError() : std::exception() {
        assert(!"CmRuntimeError");
    }
};

struct MeControl // sizeof=120
{
    mfxU8  longSp[32];          // 0 B
    mfxU8  shortSp[32];         // 32 B
    mfxU8  longSpLenSp;         // 64 B
    mfxU8  longSpMaxNumSu;      // 65 B
    mfxU8  shortSpLenSp;        // 66 B
    mfxU8  shortSpMaxNumSu;     // 67 B
    mfxU16 width;               // 34 W
    mfxU16 height;              // 35 W
    mfxU8  mvCost[5][8];         // 72 B (1x), 80 B (2x), 88 B (4x), 96 B (8x) 104 B (16x)
    mfxU8  mvCostScaleFactor[5]; // 112 B (1x), 113 B (2x), 114 B (4x), 115 B (8x) 116 B (16x)
    mfxU8  reserved[3];          // 117 B
};


const mfxU8 SingleSU[56] =
{
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


const mfxU8 L1 = 0x0F;
const mfxU8 R1 = 0x01;
const mfxU8 U1 = 0xF0;
const mfxU8 D1 = 0x10;


const mfxU8 RasterScan_48x40[56] =
{// Starting location does not matter so long as wrapping enabled.
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1
};


//      37 1E 1F 20 21 22 23 24
//      36 1D 0C 0D 0E 0F 10 25
//      35 1C 0B 02 03 04 11 26
//      34 1B 0A 01>00<05 12 27
//      33 1A 09 08 07 06 13 28
//      32 19 18 17 16 15 14 29
//      31 30 2F 2E 2D 2C 2B 2A
const mfxU8 FullSpiral_48x40[56] =
{ // L -> U -> R -> D
    L1,
    U1,
    R1,R1,
    D1,D1,
    L1,L1,L1,
    U1,U1,U1,
    R1,R1,R1,R1,
    D1,D1,D1,D1,
    L1,L1,L1,L1,L1,
    U1,U1,U1,U1,U1,
    R1,R1,R1,R1,R1,R1,
    D1,D1,D1,D1,D1,D1,      // The last D1 steps outside the search window.
    L1,L1,L1,L1,L1,L1,L1,   // These are outside the search window.
    U1,U1,U1,U1,U1,U1,U1
};

const mfxU8 Diamond[56] =
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

const mfxU8 SmallPath[3] =
{
    0x0F, 0xF0, 0x01
};

CmProgram * ReadProgram(CmDevice * device, std::istream & is);
CmProgram * ReadProgram(CmDevice * device, char const * filename);
CmProgram * ReadProgram(CmDevice * device, const unsigned char* buffer, int len);
CmKernel * CreateKernel(CmDevice * device, CmProgram * program, char const * name, void * funcptr);
void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern = CM_NONE_DEPENDENCY);
void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel0, CmKernel *kernel1, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern = CM_NONE_DEPENDENCY);
void EnqueueKernel(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event);
void EnqueueKernelLight(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth, mfxU32 tsHeight, CmEvent *&event);
void EnqueueCopyCPUToGPU(CmQueue *queue, CmSurface2D *surface, const void *sysMem, CmEvent *&event);
void EnqueueCopyGPUToCPU(CmQueue *queue, CmSurface2D *surface, void *sysMem, CmEvent *&event);
void EnqueueCopyCPUToGPUStride(CmQueue *queue, CmSurface2D *surface, const void *sysMem, mfxU32 pitch, CmEvent *&event);
void EnqueueCopyGPUToCPUStride(CmQueue *queue, CmSurface2D *surface, void *sysMem, mfxU32 pitch, CmEvent *&event);
void Read(CmBuffer * buffer, void * buf, CmEvent * e = 0);
void Write(CmBuffer * buffer, void * buf, CmEvent * e = 0);
CmDevice * TryCreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version);
CmDevice * CreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version);
CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);
CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);
SurfaceIndex * CreateVmeSurfaceG75(CmDevice * device, CmSurface2D * source, CmSurface2D ** fwdRefs, CmSurface2D ** bwdRefs, mfxU32 numFwdRefs, mfxU32 numBwdRefs);

void SetupMeControl(MeControl &ctrl, int width, int height, double lambda);

CmDevice * TryCreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version = 0);
CmDevice * CreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version = 0);
CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);
CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);
CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);
CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem);

template <class T>
SurfaceIndex & GetIndex(const T * cmResource)
{
    SurfaceIndex * index = 0;
    int result = const_cast<T *>(cmResource)->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

template <class T0>
void SetKernelArgLast(CmKernel * kernel, T0 const & arg, unsigned int index)
{
    int result = kernel->SetKernelArg(index, sizeof(arg), &arg);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}
template <> inline
    void SetKernelArgLast<CmSurface2D *>(CmKernel * kernel, CmSurface2D * const & arg, unsigned int index)
{
    int result = kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}
template <> inline
    void SetKernelArgLast<CmSurface2DUP *>(CmKernel * kernel, CmSurface2DUP * const & arg, unsigned int index)
{
    int result = kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}
template <> inline
    void SetKernelArgLast<CmBuffer *>(CmKernel * kernel, CmBuffer * const & arg, unsigned int index)
{
    int result = kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}
template <> inline
    void SetKernelArgLast<CmBufferUP *>(CmKernel * kernel, CmBufferUP * const & arg, unsigned int index)
{
    int result = kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}

template <uint32_t N> inline
    void SetKernelArgLast(CmKernel * kernel, const vector<SurfaceIndex, N> & arg, unsigned int index)
{
    typedef vector<SurfaceIndex, N> VSI;
    kernel->SetKernelArg(index, arg.get_size_data(), const_cast<VSI &>(arg).get_addr_data());
}

template <class T0>
void SetKernelArg(CmKernel * kernel, T0 const & arg0)
{
    SetKernelArgLast(kernel, arg0, 0);
}

template <class T0, class T1>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1)
{
    SetKernelArgLast(kernel, arg0, 0);
    SetKernelArgLast(kernel, arg1, 1);
}

template <class T0, class T1, class T2>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2)
{
    SetKernelArg(kernel, arg0, arg1);
    SetKernelArgLast(kernel, arg2, 2);
}

template <class T0, class T1, class T2, class T3>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3)
{
    SetKernelArg(kernel, arg0, arg1, arg2);
    SetKernelArgLast(kernel, arg3, 3);
}

template <class T0, class T1, class T2, class T3, class T4>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3);
    SetKernelArgLast(kernel, arg4, 4);
}

template <class T0, class T1, class T2, class T3, class T4, class T5>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4);
    SetKernelArgLast(kernel, arg5, 5);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5);
    SetKernelArgLast(kernel, arg6, 6);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    SetKernelArgLast(kernel, arg7, 7);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    SetKernelArgLast(kernel, arg8, 8);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8, T9 const & arg9)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    SetKernelArgLast(kernel, arg9, 9);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8, T9 const & arg9, T10 const & arg10)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
    SetKernelArgLast(kernel, arg10, 10);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    SetKernelArgLast(kernel, arg11, 11);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
    SetKernelArgLast(kernel, arg12, 12);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12, T13 const & arg13)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12);
    SetKernelArgLast(kernel, arg13, 13);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                  T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12, T13 const & arg13, T14 const & arg14)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13);
    SetKernelArgLast(kernel, arg14, 14);
}

struct Kernel
{
    Kernel();
    ~Kernel() { Destroy(); }
    void AddKernel(CmDevice *device, CmProgram *program, const char *name,
                   mfxU32 tsWidth, mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern, bool addSync = false);

    void AddKernelWithGroup(
        CmDevice *device, CmProgram *program, const char *name, mfxU32 grWidth, mfxU32 grHeight,
        mfxU32 tsWidth, mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern, bool addSync = false);

    void Destroy();
    void Enqueue(CmQueue *queue, CmEvent *&e);
    void EnqueueWithGroup(CmQueue *queue, CmEvent *&e);
    CmKernel     *&kernel;
    CmDevice      *m_device;
    CmKernel      *m_kernel[16];
    CmTask        *m_task;
    CmThreadSpace *m_ts[16];
    CmThreadGroupSpace* m_tgs[16];
    mfxU32         m_numKernels;

private:
    void operator=(const Kernel&) = delete;
};

}

#endif /* _MFX_H265_ENC_CM_UTILS_H */
