// Copyright (c) 2013-2018 Intel Corporation
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

#include "mfx_h264_dispatcher.h"
#include "ipp.h"

namespace MFX_H264_PP
{

enum { CPU_FEAT_AUTO = 0, CPU_FEAT_PX, CPU_FEAT_SSE4, CPU_FEAT_SSE4_ATOM, CPU_FEAT_SSSE3, CPU_FEAT_AVX2, CPU_FEAT_COUNT };

mfxU32 GetCPUFeature()
{
    mfxU32 cpuFeature;
    // GetPlatformType
    Ipp32u cpuIdInfoRegs[4];
    Ipp64u featuresMask;
    IppStatus sts = ippGetCpuFeatures(&featuresMask, cpuIdInfoRegs);
    if (ippStsNoErr != sts)
        return sts;

    if (featuresMask & (Ipp64u)(ippCPUID_AVX2)) // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
        cpuFeature = CPU_FEAT_AVX2;
    else if ((featuresMask & (Ipp64u)(ippCPUID_MOVBE)) && (featuresMask & (Ipp64u)(ippCPUID_SSE42)) && !(featuresMask & (Ipp64u)(ippCPUID_AVX)))
        cpuFeature = CPU_FEAT_SSE4_ATOM;
    else if (featuresMask & (Ipp64u)(ippCPUID_SSE42))
        cpuFeature = CPU_FEAT_SSE4;
    else if (featuresMask & (Ipp64u)(ippCPUID_SSSE3))
        cpuFeature = CPU_FEAT_SSSE3;
    else
        cpuFeature = CPU_FEAT_PX;

    return cpuFeature;
}

class Initializer
{
public:
    Initializer()
        : g_dispatcher(0)
    {
    }

    ~Initializer()
    {
        g_dispatcher = 0;
    }

    H264_Dispatcher *g_dispatcher;
};

// it is not thread safe. Should be called once
H264_Dispatcher * GetH264Dispatcher()
{
    static Initializer initializer;

    if (!initializer.g_dispatcher)
    {
        static H264_Dispatcher_sse g_dispatcher_sse;
        static H264_Dispatcher g_dispatcher_px;

        mfxU32 cpuFeature = GetCPUFeature();

        switch(cpuFeature)
        {
        case CPU_FEAT_SSSE3:
        case CPU_FEAT_SSE4:
        case CPU_FEAT_SSE4_ATOM:
        case CPU_FEAT_AVX2:
            initializer.g_dispatcher = &g_dispatcher_sse;
            break;
        case CPU_FEAT_PX:
        default:
            initializer.g_dispatcher = &g_dispatcher_px;
            break;
        }
    }

    return initializer.g_dispatcher;
}

}

void InitializeH264Optimizations()
{
    MFX_H264_PP::GetH264Dispatcher();
}

class StaticInitializer
{
public:
    StaticInitializer()
    {
        InitializeH264Optimizations();
    }
};

static StaticInitializer g_StaticInitializer;

