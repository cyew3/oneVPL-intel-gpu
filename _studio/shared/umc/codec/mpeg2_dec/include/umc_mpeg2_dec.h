//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

//  MPEG-2 is a international standard promoted by ISO/IEC and
//  other organizations. Implementations of this standard, or the standard
//  enabled platforms may require licenses from various entities, including
//  Intel Corporation.

#pragma once

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_video_decoder.h"

//#define OTLAD

namespace UMC
{

// profile and level definitions for mpeg2 video
// values for profile and level fields in BaseCodecParams
#define MPEG2_PROFILE_SIMPLE  5
#define MPEG2_PROFILE_MAIN    4
#define MPEG2_PROFILE_SNR     3
#define MPEG2_PROFILE_SPATIAL 2
#define MPEG2_PROFILE_HIGH    1

#define MPEG2_LEVEL_LOW   10
#define MPEG2_LEVEL_MAIN   8
#define MPEG2_LEVEL_H14    6
#define MPEG2_LEVEL_HIGH   4

    class MPEG2VideoDecoderBase;
    
}

#endif // ifdef UMC_ENABLE_MPEG2_VIDEO_DECODER
