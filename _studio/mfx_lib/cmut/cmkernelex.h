//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "cmrt_cross_platform.h"
#include "cmdeviceex.h"
#include "cmassert.h"

namespace mdfut {
  class CmBufferEx;
  class CmBufferUPEx;
  class CmSurface2DEx;
  class CmSurface2DUPEx;

  class CmKernelEx
  {
  public:

  #ifdef CMRT_EMU
    template<typename kernelFunctionTy>
    CmKernelEx(CmDeviceEx & deviceEx, const char * pKernelName, kernelFunctionTy kernelFunction)
      : deviceEx(deviceEx), kernelName(pKernelName)
    {
  #ifdef _DEBUG
      cout << "CmKernelEx(\"" << pKernelName << "\")" << endl;
  #endif
      pKernel = deviceEx.CreateKernel(pKernelName, (const void *)kernelFunction);
    }
  #else
    CmKernelEx(CmDeviceEx & deviceEx, const char * pKernelName)
      : deviceEx(deviceEx), kernelName(pKernelName)
    {
  #ifdef _DEBUG
      cout << "CmKernelEx(\"" << pKernelName << "\")" << endl;
  #endif
      pKernel = deviceEx.CreateKernel(pKernelName);
    }
  #endif

    ~CmKernelEx()
    {
      deviceEx.DestroyKernel(pKernel);
    }

  public:
    CmKernel * operator ->() { return pKernel; }
    operator CmKernel *() { return pKernel; }

    void SetThreadCount (unsigned int threadCount)
    {
      int result = pKernel->SetThreadCount(threadCount);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }

#if __INTEL_MDF > CM_3_0
    void SetThreadSpace(CmThreadSpace * pThreadSpace = NULL)
    {
      int result = pKernel->AssociateThreadSpace(pThreadSpace);
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }
#endif

    template<typename ArgTy>
    void SetKernelArg(UINT index, ArgTy arg)
    {
      int result = pKernel->SetKernelArg(index, sizeof(ArgTy), &arg);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }

    template<typename ArgTy>
    void SetThreadArg(UINT threadId, UINT index, ArgTy arg)
    {
      int result = pKernel->SetThreadArg(threadId, index, sizeof(ArgTy), &arg);
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }

  #define SPECIALIZE_SET_ARG(SurfaceType) \
    void SetKernelArg(UINT index, SurfaceType##Ex & surface)  \
    { \
      SetKernelArg(index, (SurfaceType &)surface); \
    } \
    \
    void SetKernelArg(UINT index, SurfaceType & surface)  \
    { \
      SetKernelArgBySurface(index, surface); \
    } \
    \
    void SetThreadArg(UINT threadId, UINT index, SurfaceType##Ex & surface)  \
    { \
      SetThreadArg(threadId, index, (SurfaceType &)surface); \
    } \
    \
    void SetThreadArg(UINT threadId, UINT index, SurfaceType & surface)  \
    { \
      SetThreadArgBySurface(threadId, index, surface); \
    }

    SPECIALIZE_SET_ARG(CmBuffer)
    SPECIALIZE_SET_ARG(CmBufferUP)
    SPECIALIZE_SET_ARG(CmSurface2D)
    SPECIALIZE_SET_ARG(CmSurface2DUP)

    void SetKernelArg(UINT index, CmSurface3D & surface3D)
    {
      SetKernelArgBySurface (index, surface3D);
    }

    const std::string & KernelName() const { return kernelName; }

  protected:
    template<typename SurfaceTy>
    void SetKernelArgBySurface(UINT index, SurfaceTy & surface)
    {
      SurfaceIndex * pSurfaceIndex = NULL;
      int result = surface.GetIndex(pSurfaceIndex);
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);

      result = pKernel->SetKernelArg(index, sizeof(SurfaceIndex), pSurfaceIndex);
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }

    template<typename SurfaceTy>
    void SetThreadArgBySurface(UINT threadId, UINT index, SurfaceTy & surface)
    {
      SurfaceIndex * pSurfaceIndex = NULL;
      int result = surface.GetIndex(pSurfaceIndex);
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);

      result = pKernel->SetThreadArg(threadId, index, sizeof(SurfaceIndex), pSurfaceIndex);
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }

  protected:
    CmKernel * pKernel;
    CmDeviceEx & deviceEx;
    std::string kernelName;
  CmKernelEx(const CmKernelEx& that);
  //prohibit assignment operator
  CmKernelEx& operator=(const CmKernelEx&);
  };
};
