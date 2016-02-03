/**             
***
*** Copyright  (C) 1985-2016 Intel Corporation. All rights reserved.
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
#include "vector"
#include "cmkernelex.h"
#include "cmdeviceex.h"
#include "cmassert.h"

class CmThreadSpaceEx
{
public:
  CmThreadSpaceEx(CmDeviceEx & deviceEx, unsigned int width, unsigned int height)
    : deviceEx(deviceEx), pThreadSpace(NULL), width(width), height(height)
  {
    int result = deviceEx->CreateThreadSpace(width, height, pThreadSpace);
    CM_FAIL_IF(result != CM_SUCCESS || pThreadSpace == NULL, result);
  }

  ~CmThreadSpaceEx()
  {
    deviceEx->DestroyThreadSpace(pThreadSpace);
  }

  operator CmThreadSpace *() { return pThreadSpace; }
  CmThreadSpace * operator ->() { return pThreadSpace; }
  unsigned int ThreadCount() const { return width * height; }

protected:
  CmDeviceEx & deviceEx;
  CmThreadSpace * pThreadSpace;
  unsigned int width;
  unsigned int height;
};
