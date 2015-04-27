/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2015 Intel Corporation. All Rights Reserved.
//
//
*/

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
        delete g_dispatcher;
    }

    H264_Dispatcher *g_dispatcher;
};

H264_Dispatcher * GetH264Dispatcher()
{
    static Initializer initializer;

    if (!initializer.g_dispatcher)
    {
        mfxU32 cpuFeature = GetCPUFeature();

        switch(cpuFeature)
        {
        case CPU_FEAT_SSSE3:
        case CPU_FEAT_SSE4:
        case CPU_FEAT_SSE4_ATOM:
        case CPU_FEAT_AVX2:
            initializer.g_dispatcher = new H264_Dispatcher_sse();
            break;
        case CPU_FEAT_PX:
        default:
            initializer.g_dispatcher = new H264_Dispatcher();
            break;
        }
    }

    return initializer.g_dispatcher;
}

}