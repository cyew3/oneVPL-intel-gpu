//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_PTIR_MEMORY_INCLUDED__)
#define __MFX_PTIR_MEMORY_INCLUDED__

#include <memory.h>

#if defined(LINUX32) || defined(__APPLE__)
#ifdef __cplusplus
    extern "C"
    {
#endif /* __cplusplus */
        #include <safe_lib.h>
#ifdef __cplusplus
    }
#endif /* __cplusplus */
#endif


#if defined(_WIN32) || defined(_WIN64)
    #define ptir_memcpy_s(dst, dst_size, src, size_to_copy)  memcpy_s(dst, dst_size, src, size_to_copy)
#elif defined(LINUX32) || defined(LINUX64)
    #define ptir_memcpy_s(dst, dst_size, src, size_to_copy)  memcpy_s(dst, dst_size, src, size_to_copy)
#else
    #define ptir_memcpy_s(dst, dst_size, src, size_to_copy)  memcpy(dst, src, size_to_copy)
#endif
#define ptir_memcpy(dst, src, size)  ptir_memcpy_s(dst, size, src, size)

#endif  // __MFX_PTIR_MEMORY_INCLUDED__