//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_debug.h"
#include <cstdarg>

#if defined(_MSC_VER)
#include <windows.h>
#endif

#ifdef ENABLE_TRACE
#include "umc_h265_frame.h"
#endif

namespace UMC_HEVC_DECODER
{

#ifdef ENABLE_TRACE
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

const vm_char* GetFrameInfoString(H265DecoderFrame * frame)
{
    static vm_char str[256];
    vm_string_sprintf(str, VM_STRING("POC - %d, uid - %d"), frame->m_PicOrderCnt, frame->m_UID);
    return str;
}

#endif // ENABLE_TRACE

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER

