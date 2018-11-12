// Copyright (c) 2015-2018 Intel Corporation
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