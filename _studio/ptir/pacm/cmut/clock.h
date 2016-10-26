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
#if defined(_WIN32) || defined(_WIN64)
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
    return !!QueryPerformanceCounter(&begin);
  }

  bool End()
  {
    if (!(!!QueryPerformanceCounter(&end))) {
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
#endif

#if defined(LINUX32) || defined (LINUX64)
class Clock
{
public:
  Clock()
    : duration(0), count(0)
  {
  }

  bool Begin()
  {
    return true;
  }

  bool End()
  {
    return true;
  }

  double Duration() const { return duration; }
  int Count() const { return count; }

protected:
  int freq;
  int begin;
  int end;

  double duration; // ms
  int count;
};
#endif
