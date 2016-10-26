//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 1985-2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "cmrt_cross_platform.h"
//#include "cm_rt.h"
#include "cmdeviceex.h"
#include "cmassert.h"

class CmBufferEx;
class CmBufferUPEx;
class CmSurface2DEx;
class CmSurface2DUPEx;

class CmKernelEx
{
public:
#ifdef CMRT_EMU
  template<typename kernelFunctionTy>
  CmKernelEx (CmDeviceEx & cmDeviceEx, const char * pKernelName, kernelFunctionTy kernelFunction)
    : cmDeviceEx(cmDeviceEx), kernelName(pKernelName)
  {
#ifdef _DEBUG
    cout << "CmKernelEx (\"" << pKernelName << "\")" << endl;
#endif
    pCmKernel = cmDeviceEx.CreateKernel (pKernelName, (const void *)kernelFunction);
  }
#else
  CmKernelEx (CmDeviceEx & cmDeviceEx, const char * pKernelName)
    : cmDeviceEx(cmDeviceEx), kernelName(pKernelName)
  {
#ifdef _DEBUG
    cout << "CmKernelEx (\"" << pKernelName << "\")" << endl;
#endif
    pCmKernel = cmDeviceEx.CreateKernel(pKernelName);
  }
#endif

  ~CmKernelEx ()
  {
    cmDeviceEx.DestroyKernel(pCmKernel);
  }

public:
  CmKernel * operator -> () { return pCmKernel; }
  operator CmKernel * () { return pCmKernel; }

  void SetThreadCount (unsigned int threadCount)
  {
#ifdef _DEBUG
    //cout << "SetThreadCount (" << threadCount << ")" << endl;
#endif

    int result = pCmKernel->SetThreadCount(threadCount);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  template<typename ArgTy>
  void SetKernelArg(UINT index, ArgTy arg)
  {  
    int result = pCmKernel->SetKernelArg(index, sizeof(ArgTy), &arg);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }
  template<typename ArgTy>
  void SetThreadArg(UINT threadId, UINT index, ArgTy arg)
  {  
    int result = pCmKernel->SetThreadArg(threadId, index, sizeof(ArgTy), &arg);
    CM_FAIL_IF(result != CM_SUCCESS, result);
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

  std::string KernelName() const { return kernelName; }

protected:
  template<typename SurfaceTy>
  void SetKernelArgBySurface(UINT index, SurfaceTy & surface)
  {
    SurfaceIndex * pSurfaceIndex = NULL;
    int result = surface.GetIndex(pSurfaceIndex);
    CM_FAIL_IF(result != CM_SUCCESS, result);

    result = pCmKernel->SetKernelArg(index, sizeof(SurfaceIndex), pSurfaceIndex);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  template<typename SurfaceTy>
  void SetThreadArgBySurface(UINT threadId, UINT index, SurfaceTy & surface)
  {
    SurfaceIndex * pSurfaceIndex = NULL;
    int result = surface.GetIndex(pSurfaceIndex);
    CM_FAIL_IF(result != CM_SUCCESS, result);
    
    result = pCmKernel->SetThreadArg(threadId, index, sizeof(SurfaceIndex), pSurfaceIndex);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

protected:
  CmKernel * pCmKernel;
  CmDeviceEx & cmDeviceEx;
  std::string kernelName;
};
