/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_debug.h"
#include "umc_h265_timing.h"
#include <cstdarg>

namespace UMC_HEVC_DECODER
{

#ifdef __EXCEPTION_HANDLER_
ExceptionHandlerInitializer exceptionHandler;
#endif // __EXCEPTION_HANDLER_

#ifdef USE_DETAILED_H265_TIMING
    TimingInfo* clsTimingInfo;
#endif

// Debug output
void Trace(vm_char * format, ...)
{
    va_list(arglist);
    va_start(arglist, format);

    vm_char cStr[256];
    vm_string_vsprintf(cStr, format, arglist);

    //OutputDebugString(cStr);
    vm_string_printf(VM_STRING("%s"), cStr);
    //fflush(stdout);
}

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER

