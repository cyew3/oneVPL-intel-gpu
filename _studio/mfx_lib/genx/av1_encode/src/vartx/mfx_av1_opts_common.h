// Copyright (c) 2014-2019 Intel Corporation
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

#include "assert.h"
#include "ippdefs.h"

#if defined(_WIN32) || defined(_WIN64)
  #define H265_FORCEINLINE __forceinline
  #define H265_NONLINE __declspec(noinline)
  #define H265_FASTCALL __fastcall
  #define ALIGN_DECL(X) __declspec(align(X))
#else
  #define H265_FORCEINLINE __attribute__((always_inline))
  #define H265_NONLINE __attribute__((noinline))
  #define H265_FASTCALL
  #define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

#ifdef __INTEL_COMPILER
# define H265_RESTRICT __restrict
#elif defined _MSC_VER
# if _MSC_VER >= 1400
#  define H265_RESTRICT __restrict
# else
#  define H265_RESTRICT
# endif
#else
# define H265_RESTRICT
#endif
