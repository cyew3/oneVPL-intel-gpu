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
#include "cmkernelex.h"
#include "vector"

namespace mdfut {
  class CmTaskEx
  {
  public:
    CmTaskEx(CmDeviceEx & deviceEx)
      : deviceEx(deviceEx)
    {
      int result = deviceEx->CreateTask(pTask);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }

    CmTaskEx(CmDeviceEx & deviceEx, CmKernelEx & kernelEx)
      : deviceEx(deviceEx)
    {
      int result = deviceEx->CreateTask(pTask);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);

      AddKernel(kernelEx);
    }

#if __INTEL_MDF > CM_3_0
    CmTaskEx(CmDeviceEx & deviceEx, const std::vector<CmKernelEx *> & kernelExs, DWORD syncs)
      : deviceEx(deviceEx)
    {
      int result = deviceEx->CreateTask(pTask);
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);

      for (int i = 0; i < kernelExs.size(); ++i) {
        AddKernel(*kernelExs[i]);

        if (i != kernelExs.size() - 1 && (syncs & (1 << i)) != 0) {
          int result = pTask->AddSync();
          MDFUT_FAIL_IF(result != CM_SUCCESS, result);
        }
      }
    }
#endif

    ~CmTaskEx()
    {
      if (pTask != NULL) {
        deviceEx->DestroyTask(pTask);
      }
    }

    void AddKernel(CmKernelEx & kernelEx)
    {
      int result = pTask->AddKernel(kernelEx);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);

      kernelExs.push_back(&kernelEx);
    }

    CmTask * operator ->() { return pTask; }
    operator CmTask *() { return pTask; }

    const std::vector<CmKernelEx *> & KernelExs() const { return kernelExs; }

  protected:
    CmTask * pTask;
    CmDeviceEx & deviceEx;
    std::vector<CmKernelEx *> kernelExs;

      CmTaskEx(const CmTaskEx& that);
  //prohibit assignment operator
  CmTaskEx& operator=(const CmTaskEx&);
  };
};