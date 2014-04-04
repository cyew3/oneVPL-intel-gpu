/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.
//
*/

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined (MFX_ENABLE_CM)

#pragma once

#ifndef __MFX_H265_ENC_CM_H__
#define __MFX_H265_ENC_CM_H__

#include "mfx_h265_enc.h"

#include "libmfx_core_interface.h"
#include "cmrt_cross_platform.h"
#include "libmfx_core_interface.h"

#include <vector>
#include <assert.h>

class CmDevice;
class CmBuffer;
class CmBufferUP;
class CmSurface2D;
class CmEvent;
class CmQueue;
class CmProgram;
class CmKernel;
class SurfaceIndex;
class CmThreadSpace;
//class CmTask;
class H265CmTask;

namespace H265Enc {

struct H265EncCURBEData;

class CmRuntimeError : public std::exception
{
public:
    CmRuntimeError() : std::exception() {
        assert(!"CmRuntimeError");
    }
};

class H265RefMatchData
{

typedef struct {
    Ipp32s poc;
    Ipp8s  globi;
} MatchData;

private:
    Ipp32s refMatchBufLen;
    MatchData *refMatchBuf;

public:
    H265RefMatchData(Ipp32s bufLen);
    virtual ~H265RefMatchData();
    
    Ipp8s GetByPoc(Ipp32s poc);
    Ipp32s Add(Ipp32s poc);
    void Update(H265FrameList *pDpb);
};

CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);

CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);

CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);

CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);

CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem);

SurfaceIndex * CreateVmeSurfaceG75(
    CmDevice *      device,
    CmSurface2D *   source,
    CmSurface2D **  fwdRefs,
    CmSurface2D **  bwdRefs,
    mfxU32          numFwdRefs,
    mfxU32          numBwdRefs);

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
    kernel->SetKernelArg(index, sizeof(arg), &arg);
}
template <> inline
void SetKernelArgLast<CmSurface2D *>(CmKernel * kernel, CmSurface2D * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
}
template <> inline
void SetKernelArgLast<CmSurface2DUP *>(CmKernel * kernel, CmSurface2DUP * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
}
template <> inline
void SetKernelArgLast<CmBuffer *>(CmKernel * kernel, CmBuffer * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
}
template <> inline
void SetKernelArgLast<CmBufferUP *>(CmKernel * kernel, CmBufferUP * const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(SurfaceIndex), &GetIndex(arg));
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

void SetCurbeData(
    H265EncCURBEData & curbeData,
    mfxU32             picType,
    mfxU32             qp);

} // namespace

#endif // __MFX_H265_ENC_CM_H__

#endif // MFX_ENABLE_CM

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
