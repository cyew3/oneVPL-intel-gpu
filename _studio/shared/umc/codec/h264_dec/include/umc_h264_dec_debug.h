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

#ifndef __UMC_H264_DEC_DEBUG_H
#define __UMC_H264_DEC_DEBUG_H

#include "umc_h264_dec_defs_dec.h"

namespace UMC
{

void Trace(vm_char * format, ...);

#if 0
#if defined _DEBUG
#define DEBUG_PRINT(x) Trace x
static int pppp = 0;
#else
#define DEBUG_PRINT(x)
#endif

#else
#define DEBUG_PRINT(x)
#endif

} // namespace UMC

#endif // __UMC_H264_DEC_DEBUG_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
