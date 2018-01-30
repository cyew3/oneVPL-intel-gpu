//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//
#ifndef _CPUDETECT_H_
#define _CPUDETECT_H_

#include "asc_structures.h"
#if !defined(_WIN32)
    #include <cpuid.h>
#endif
//
// CPU Dispatcher
// 1) global CPU flags are initialized at startup
// 2) each stub configures a function pointer on first call
//

#pragma warning(disable:4505)

static mfxI32 CpuFeature_SSE41() {
#if defined(_WIN32) || defined(_WIN64)
    mfxI32 info[4], mask = (1 << 19);    // SSE41
    __cpuidex(info, 0x1, 0);
    return ((info[2] & mask) == mask);
#else
    return((__builtin_cpu_supports("sse4.1")));
#endif
}
static mfxI32 CpuFeature_AVX() {
#if defined(_WIN32)
    mfxI32 info[4], mask = (1 << 27) | (1 << 28);    // OSXSAVE,AVX
    __cpuidex(info, 0x1, 0);
    if ((info[2] & mask) == mask)
        return ((_xgetbv(_XCR_XFEATURE_ENABLED_MASK) & 0x6) == 0x6);
    return 0;
#else
    return((__builtin_cpu_supports("avx2")));
#endif
}

static mfxI32 CpuFeature_AVX2() {
#if defined(__AVX2__)
    #if defined(_WIN32) || defined(_WIN64)
        mfxI32 info[4], mask = (1 << 5); // AVX2
        if (CpuFeature_AVX()) {
            __cpuidex(info, 0x7, 0);
            return ((info[1] & mask) == mask);
        }
        return 0;
    #else
        return((__builtin_cpu_supports("avx2")));
    #endif
#else
    return 0;
#endif //defined(__AVX2__)
}

#pragma warning(default:4505)

//
// end Dispatcher
//


#endif