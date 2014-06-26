/**             
***
*** Copyright  (C) 1985-2014 Intel Corporation. All rights reserved.
***
*** The information and source code contained herein is the exclusive
*** property of Intel Corporation. and may not be disclosed, examined
*** or reproduced in whole or in part without explicit written authorization
*** from the company.
***
*** ----------------------------------------------------------------------------
**/ 
#pragma once

#include "cmrt_cross_platform.h"
//#include "cm_rt.h"
#include "cmdeviceex.h"
#include "cmkernelex.h"
#include "vector"
using namespace std;

class CmTaskEx
{
public:
  CmTaskEx (CmDeviceEx & cmDeviceEx)
    : cmDeviceEx(cmDeviceEx)
  {
    int result = cmDeviceEx->CreateTask(pCmTask);
    CM_FAIL_IF(result != CM_SUCCESS, result);
  }

  CmTaskEx (CmDeviceEx & cmDeviceEx, CmKernelEx & kernel)
    : cmDeviceEx(cmDeviceEx)
  {
    int result = cmDeviceEx->CreateTask(pCmTask);
    if (result != CM_SUCCESS) {
      throw CMUT_EXCEPTION("fail");
    }

    AddKernel(kernel);
  }

  ~CmTaskEx()
  {
    if (pCmTask != NULL) {
      cmDeviceEx->DestroyTask(pCmTask);
    }
  }

  void AddKernel(CmKernelEx & kernelEx)
  {
    int result = pCmTask->AddKernel(kernelEx);
    CM_FAIL_IF(result != CM_SUCCESS, result);

    kernelExs.push_back(&kernelEx);
  }

  CmTask * operator -> () { return pCmTask; }
  operator CmTask * () { return pCmTask; }

protected:
  CmTask * pCmTask;
  CmDeviceEx & cmDeviceEx;
  std::vector<CmKernelEx *> kernelExs;
};
  
