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

#ifndef __UMC_H265_DEC_DEBUG_H
#define __UMC_H265_DEC_DEBUG_H

#include "vm_time.h" /* for vm_time_get_tick on Linux */
#include "umc_h265_dec_defs.h"

//#define ENABLE_TRACE

namespace UMC_HEVC_DECODER
{

#ifdef ENABLE_TRACE
    class H265DecoderFrame;
    void Trace(vm_char * format, ...);
    const vm_char* GetFrameInfoString(H265DecoderFrame * frame);

#if defined _DEBUG
#define DEBUG_PRINT(x) Trace x
static int pppp = 0;
#define DEBUG_PRINT1(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT1(x)
#endif

#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT1(x)
#endif

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_DEC_DEBUG_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
