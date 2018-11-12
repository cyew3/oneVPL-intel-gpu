// Copyright (c) 1985-2018 Intel Corporation
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
  
