/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2012 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_dec_debug.h"
#include <cstdarg>

// #define __EXCEPTION_HANDLER_

#ifdef __EXCEPTION_HANDLER_

#include <eh.h>
class ExceptionHandlerInitializer
{
public:

    ExceptionHandlerInitializer()
    {
        _set_se_translator(ExceptionHandlerInitializer::trans_func);
    }

    static void trans_func(unsigned int , EXCEPTION_POINTERS* )
    {
        __asm int 3;
        throw UMC::h264_exception(UMC::UMC_ERR_INVALID_STREAM);
    }
};

static ExceptionHandlerInitializer exceptionHandler;
#endif // __EXCEPTION_HANDLER_

namespace UMC
{

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

} // namespace UMC

#endif // UMC_ENABLE_H264_VIDEO_DECODER

