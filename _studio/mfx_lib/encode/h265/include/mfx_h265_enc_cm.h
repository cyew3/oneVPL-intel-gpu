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

#if defined (MFX_VA)

#pragma once

#ifndef __MFX_H265_ENC_CM_H__
#define __MFX_H265_ENC_CM_H__

#include "mfx_h265_enc.h"
#include "mfx_h265_enc_cm_defs.h"

#include "libmfx_core_interface.h"
#include "cmrt_cross_platform.h"
#include "libmfx_core_interface.h"

#include <vector>
#include <assert.h>

namespace H265Enc {

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
    void Clean(H265Frame **dpb, Ipp32s dpbSize);
};

class CmContext
{
public:

    CmContext(mfxU32 w, mfxU32 h, mfxU8 nRefs, VideoCORE *core);
    ~CmContext();

    void RunVmeCurr(H265VideoParam const &param, H265Frame *pFrameCur, H265Slice *pSliceCur, H265Frame **dpb, Ipp32s dpbSize);
    void RunVmeNext(H265VideoParam const & param, H265Frame * pFrameNext, H265Slice *pSliceNext);

    Ipp32s cmCurIdx;
    Ipp32s cmNextIdx;
    Ipp32u pitchDist[PU_MAX];
    Ipp32u pitchMv[PU_MAX];
    mfxU32 ** pDistCpu[2][2][CM_REF_BUF_LEN];
    mfxI16Pair ** pMvCpu[2][2][CM_REF_BUF_LEN];

    CmMbIntraDist * cmMbIntraDist[2];
    Ipp32u intraPitch[2];

    mfxU32 width;
    mfxU32 height;

    CmMbIntraGrad * cmMbIntraGrad4x4[2];
    CmMbIntraGrad * cmMbIntraGrad8x8[2];

protected:

    CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);
    CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);
    CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);
    CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);
    CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);
    CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem);
    SurfaceIndex * CreateVmeSurfaceG75(CmDevice *      device,
                                       CmSurface2D *   source,
                                       CmSurface2D **  fwdRefs,
                                       CmSurface2D **  bwdRefs,
                                       mfxU32          numFwdRefs,
                                       mfxU32          numBwdRefs);
    template <class T>
    mfxStatus CreateCmSurface2DUp(CmDevice *device, Ipp32u numElemInRow, Ipp32u numRows, CM_SURFACE_FORMAT format,
                                  T *&surfaceCpu, CmSurface2DUP *& surfaceGpu, Ipp32u &pitch);
    template <class T>
    mfxStatus CreateCmBufferUp(CmDevice *device, Ipp32u numElems, T *&surfaceCpu, CmBufferUP *& surfaceGpu);
    void DestroyCmSurface2DUp(CmDevice *device, void *surfaceCpu, CmSurface2DUP *surfaceGpu);
    void DestroyCmBufferUp(CmDevice *device, void *bufferCpu, CmBufferUP *bufferGpu);

    mfxStatus AllocateCmResources(mfxU32 w, mfxU32 h, mfxU8 nRefs, VideoCORE *core);
    void FreeCmResources();

    void SetCurbeData(H265EncCURBEData & curbeData, mfxU32 picType, mfxU32 qp);
    template <class T> SurfaceIndex & GetIndex(const T * cmResource);
    template <class T0> void SetKernelArgLast(CmKernel * kernel, T0 const & arg, unsigned int index);
    template <> inline void SetKernelArgLast<CmSurface2D *>(CmKernel * kernel, CmSurface2D * const & arg, unsigned int index);
    template <> inline void SetKernelArgLast<CmSurface2DUP *>(CmKernel * kernel, CmSurface2DUP * const & arg, unsigned int index);
    template <> inline void SetKernelArgLast<CmBuffer *>(CmKernel * kernel, CmBuffer * const & arg, unsigned int index);
    template <> inline void SetKernelArgLast<CmBufferUP *>(CmKernel * kernel, CmBufferUP * const & arg, unsigned int index);
    template <class T0>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0);
    template <class T0, class T1>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1);
    template <class T0, class T1, class T2>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2);
    template <class T0, class T1, class T2, class T3>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3);
    template <class T0, class T1, class T2, class T3, class T4>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4);
    template <class T0, class T1, class T2, class T3, class T4, class T5>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                      T8 const & arg8);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                      T8 const & arg8, T9 const & arg9);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                      T8 const & arg8, T9 const & arg9, T10 const & arg10);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                      T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                      T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                      T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12, T13 const & arg13);
    template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10, class T11, class T12, class T13, class T14>
    void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7,
                      T8 const & arg8, T9 const & arg9, T10 const & arg10, T11 const & arg11, T12 const & arg12, T13 const & arg13, T14 const & arg14);

private:

    bool cmResurcesAllocated;
    CmDevice * device;
    CmQueue * queue;
    CmSurface2DUP * mbIntraDist[2];
    CmBufferUP * mbIntraGrad4x4[2];
    CmBufferUP * mbIntraGrad8x8[2];

    CmSurface2DUP * distGpu[2][2][CM_REF_BUF_LEN][PU_MAX];
    CmSurface2DUP * mvGpu[2][2][CM_REF_BUF_LEN][PU_MAX];
    mfxU32 * distCpu[2][2][CM_REF_BUF_LEN][PU_MAX];
    mfxI16Pair * mvCpu[2][2][CM_REF_BUF_LEN][PU_MAX];

    CmSurface2D * raw[2];
    CmSurface2D * raw2x[2];
    CmSurface2D * raw4x[2];
    CmSurface2D * raw8x[2];
    CmSurface2D * raw16x[2];
    CmSurface2D ** fwdRef;
    CmSurface2D ** fwdRef2x;
    CmSurface2D ** fwdRef4x;
    CmSurface2D ** fwdRef8x;
    CmSurface2D ** fwdRef16x;
    CmSurface2D * IntraDist;

    CmProgram * program;
    CmKernel * kernelMeIntra;
    CmKernel * kernelGradient;
    CmKernel * kernelIme;
    CmKernel * kernelImeWithPred;
    CmKernel * kernelMe16;
    CmKernel * kernelMe32;
    CmKernel * kernelRefine32x32;
    CmKernel * kernelRefine32x16;
    CmKernel * kernelRefine16x32;
    CmKernel * kernelRefine;
    CmKernel * kernelDownSample2tiers;
    CmKernel * kernelDownSample4tiers;
    CmBuffer * curbe;
    CmBuffer * me1xControl;
    CmBuffer * me2xControl;
    CmBuffer * me4xControl;
    CmBuffer * me8xControl;
    CmBuffer * me16xControl;
    mfxU32 width2x;
    mfxU32 height2x;
    mfxU32 width4x;
    mfxU32 height4x;
    mfxU32 width8x;
    mfxU32 height8x;
    mfxU32 width16x;
    mfxU32 height16x;
    mfxU32 frameOrder;
    Ipp32s numBufferedRefs;
    mfxU32 highRes;

    CmEvent * lastEvent[2];

    Ipp32s cmMvW[PU_MAX];
    Ipp32s cmMvH[PU_MAX];

    H265RefMatchData * recBufData;
};


} // namespace


#endif // __MFX_H265_ENC_CM_H__

#endif // MFX_VA

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
