//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#if defined(LINUX32) || defined(__APPLE__)

#include "vm_strings.h"

Ipp32s vm_string_vprintf(const vm_char *format, va_list argptr)
{
    Ipp32s sts = 0;
    va_list copy;
    va_copy(copy, argptr);
    sts = vprintf(format, copy);
    va_end(argptr);
    return sts;
}

#else
#pragma warning( disable: 4206 )
#endif /* #if defined(LINUX32) || (__APPLE__) */
