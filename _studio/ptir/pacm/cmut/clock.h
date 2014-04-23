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

#include "windows.h"
#include "winbase.h"

class Clock
{
public:
  Clock()
    : duration(0), count(0)
  {
    QueryPerformanceFrequency(&freq);
  }

  bool Begin()
  {
    return QueryPerformanceCounter(&begin);
  }

  bool End()
  {
    if (!QueryPerformanceCounter(&end)) {
      return false;
    }

    double time = (double)(end.QuadPart - begin.QuadPart) / (double)freq.QuadPart * 1000;
    duration += time;

    ++count;

    return true;
  }

  double Duration() const { return duration; }
  int Count() const { return count; }

protected:
  LARGE_INTEGER freq;
  LARGE_INTEGER begin;
  LARGE_INTEGER end;

  double duration; // ms
  int count;
};
