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
